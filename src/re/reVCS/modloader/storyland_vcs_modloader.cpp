#include "src/re/reVCS/modloader/storyland_vcs_modloader.h"
#include "src/storyland/storyland_game_files.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace {

constexpr size_t LuaMemoryLimit = 16u * 1024u * 1024u;
constexpr int LuaInstructionBudget = 1000000;
constexpr size_t MaxScriptBytes = 1024u * 1024u;
constexpr size_t MaxPatchBytes = 16u * 1024u * 1024u;
constexpr size_t MaxModPatchBytes = 32u * 1024u * 1024u;

struct LuaContext {
    StorylandVcsMod* mod = nullptr;
    size_t allocated = 0;
    int remainingInstructions = LuaInstructionBudget;
    size_t recordedPatchBytes = 0;
};

std::string lower(std::string value) {
    return storylandNativePathLower(std::move(value));
}

std::string normalizedName(const std::string& name) {
    std::string normalized = name;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return lower(std::filesystem::path(normalized).lexically_normal().generic_string());
}

std::string retailFileKey(const std::string& name) {
    auto path = std::filesystem::path(normalizedName(name));
    const auto filename = path.filename().string();
    if (filename == "gtaps2.img") path.replace_filename("gta3ps2.img");
    if (filename == "gtaps2.dir") path.replace_filename("gta3ps2.dir");
    return path.generic_string();
}

bool relativeName(const std::string& value) {
    if (value.empty() || value.find(':') != std::string::npos || value.find('\0') != std::string::npos) return false;
    const auto path = std::filesystem::path(normalizedName(value));
    if (path.is_absolute() || path.has_root_name()) return false;
    for (const auto& part : path) if (part == "..") return false;
    return path != ".";
}

std::string pathArgument(lua_State* state, int index) {
    size_t length = 0;
    const char* text = luaL_checklstring(state, index, &length);
    if (length == 0 || length > 4096 || std::memchr(text, '\0', length))
        luaL_error(state, "replacement paths must contain 1..4096 bytes without NUL");
    return std::string(text, length);
}

bool withinDirectory(const std::filesystem::path& directory, const std::filesystem::path& path) {
    std::error_code ec;
    const auto base = std::filesystem::weakly_canonical(directory, ec);
    if (ec) return false;
    const auto child = std::filesystem::weakly_canonical(path, ec);
    if (ec) return false;
    auto left = base.begin();
    auto right = child.begin();
    for (; left != base.end(); ++left, ++right)
        if (right == child.end() || *left != *right) return false;
    return true;
}

std::filesystem::path matchingPath(const std::filesystem::path& directory, const std::string& relative) {
    std::filesystem::path current = directory;
    for (const auto& part : std::filesystem::path(normalizedName(relative))) {
        std::error_code ec;
        std::vector<std::filesystem::path> matches;
        for (std::filesystem::directory_iterator it(current, ec), end; !ec && it != end; it.increment(ec))
            if (lower(it->path().filename().string()) == part.string()) matches.push_back(it->path());
        if (matches.empty()) return {};
        std::sort(matches.begin(), matches.end());
        current = matches.front();
    }
    return current;
}

std::vector<std::filesystem::path> modFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> result;
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(directory,
        std::filesystem::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        if (it.depth() >= 16) it.disable_recursion_pending();
        if (it->is_symlink(ec)) { it.disable_recursion_pending(); continue; }
        if (it->is_regular_file(ec)) result.push_back(it->path());
    }
    std::sort(result.begin(), result.end(), [&](const auto& left, const auto& right) {
        const auto leftName = normalizedName(left.lexically_relative(directory).generic_string());
        const auto rightName = normalizedName(right.lexically_relative(directory).generic_string());
        return leftName == rightName ? left.generic_string() < right.generic_string() : leftName < rightName;
    });
    return result;
}

LuaContext& context(lua_State* state) {
    return **static_cast<LuaContext**>(lua_getextraspace(state));
}

void* luaAllocate(void* user, void* pointer, size_t oldSize, size_t newSize) {
    auto& state = *static_cast<LuaContext*>(user);
    if (!pointer) oldSize = 0;
    if (newSize == 0) {
        std::free(pointer);
        state.allocated -= std::min(state.allocated, oldSize);
        return nullptr;
    }
    const size_t retained = state.allocated - std::min(state.allocated, oldSize);
    if (newSize > LuaMemoryLimit || retained > LuaMemoryLimit - newSize) return nullptr;
    void* resized = std::realloc(pointer, newSize);
    if (resized) state.allocated = retained + newSize;
    return resized;
}

void instructionHook(lua_State* state, lua_Debug*) {
    auto& host = context(state);
    host.remainingInstructions -= 1000;
    if (host.remainingInstructions <= 0) luaL_error(state, "mod.lua exceeded the 1000000 instruction loading budget");
}

int integerField(lua_State* state, int table, const char* name, int fallback, int minimum, int maximum) {
    lua_getfield(state, table, name);
    int value = fallback;
    if (!lua_isnil(state, -1)) {
        const auto supplied = luaL_checkinteger(state, -1);
        if (supplied < minimum || supplied > maximum)
            luaL_error(state, "%s must be between %d and %d", name, minimum, maximum);
        value = int(supplied);
    }
    lua_pop(state, 1);
    return value;
}

std::string stringField(lua_State* state, int table, const char* name, bool required = true) {
    lua_getfield(state, table, name);
    if (!required && lua_isnil(state, -1)) { lua_pop(state, 1); return {}; }
    size_t size = 0;
    const char* text = luaL_checklstring(state, -1, &size);
    if (size == 0 || size > 256 || std::strlen(text) != size)
        luaL_error(state, "%s must be a nonempty string of at most 256 bytes without NUL", name);
    std::string result(text, size);
    lua_pop(state, 1);
    return result;
}

int setMod(lua_State* state) {
    luaL_checktype(state, 1, LUA_TTABLE);
    auto& mod = *context(state).mod;
    const auto name = stringField(state, 1, "name", false);
    if (!name.empty()) mod.name = name;
    mod.priority = integerField(state, 1, "priority", 0, -1000000, 1000000);
    lua_getfield(state, 1, "enabled");
    if (!lua_isnil(state, -1)) {
        luaL_checktype(state, -1, LUA_TBOOLEAN);
        mod.enabled = lua_toboolean(state, -1) != 0;
    }
    lua_pop(state, 1);
    return 0;
}

int replaceFile(lua_State* state) {
    const std::string retail = pathArgument(state, 1);
    const std::string source = pathArgument(state, 2);
    if (!relativeName(retail) || !relativeName(source))
        return luaL_error(state, "replace paths must be relative paths inside the mod folder");
    auto& mod = *context(state).mod;
    if (mod.replacements.size() >= 4096)
        return luaL_error(state, "one mod may declare at most 4096 file replacements");
    const auto path = matchingPath(mod.directory, source);
    std::error_code ec;
    if (path.empty() || !withinDirectory(mod.directory, path) || !std::filesystem::is_regular_file(path, ec))
        return luaL_error(state, "replacement file does not exist inside this mod: %s", source.c_str());
    mod.replacements.emplace_back(retailFileKey(retail), path);
    return 0;
}

std::vector<uint8_t> byteArgument(lua_State* state, int index) {
    std::vector<uint8_t> result;
    if (lua_type(state, index) == LUA_TSTRING) {
        size_t length = 0;
        const char* text = lua_tolstring(state, index, &length);
        if (length > MaxPatchBytes) luaL_error(state, "DTZ patch exceeds the 16 MiB limit");
        result.assign(text, text + length);
    } else {
        luaL_checktype(state, index, LUA_TTABLE);
        const auto count = lua_rawlen(state, index);
        if (count > MaxPatchBytes) luaL_error(state, "DTZ patch exceeds the 16 MiB limit");
        result.reserve(count);
        for (size_t i = 1; i <= count; ++i) {
            lua_rawgeti(state, index, lua_Integer(i));
            const auto byte = luaL_checkinteger(state, -1);
            if (byte < 0 || byte > 255) luaL_error(state, "patch byte must be in 0..255");
            result.push_back(uint8_t(byte));
            lua_pop(state, 1);
        }
    }
    return result;
}

uint32_t patchOffset(lua_State* state) {
    const auto offset = luaL_checkinteger(state, 1);
    if (offset < 0 || uint64_t(offset) > std::numeric_limits<uint32_t>::max())
        luaL_error(state, "DTZ patch offset must be a 32-bit nonnegative byte offset");
    return uint32_t(offset);
}

void appendPatch(lua_State* state, StorylandModDtzPatch patch) {
    if (patch.bytes.empty()) luaL_error(state, "DTZ patch must contain at least one byte");
    if (!patch.expected.empty() && patch.expected.size() != patch.bytes.size())
        luaL_error(state, "expected original DTZ bytes must match the replacement length");
    auto& mod = *context(state).mod;
    auto& host = context(state);
    const size_t added = patch.bytes.size() + patch.expected.size();
    if (mod.patches.size() >= 65536 || added > MaxModPatchBytes - host.recordedPatchBytes)
        luaL_error(state, "one mod may record at most 65536 patches and 32 MiB of patch/expected bytes");
    host.recordedPatchBytes += added;
    lua_Debug call{};
    patch.source = mod.directory.filename().string() + "/mod.lua";
    if (lua_getstack(state, 1, &call) && lua_getinfo(state, "l", &call))
        patch.source += ":" + std::to_string(call.currentline);
    mod.patches.push_back(std::move(patch));
}

int patchBytes(lua_State* state) {
    StorylandModDtzPatch patch;
    patch.offset = patchOffset(state);
    patch.bytes = byteArgument(state, 2);
    if (!lua_isnoneornil(state, 3)) patch.expected = byteArgument(state, 3);
    appendPatch(state, std::move(patch));
    return 0;
}

int patchNumber(lua_State* state) {
    const int width = int(lua_tointeger(state, lua_upvalueindex(1)));
    const bool floating = lua_toboolean(state, lua_upvalueindex(2)) != 0;
    auto encode = [&](int index) {
        uint32_t value = 0;
        if (floating) {
            const double number = luaL_checknumber(state, index);
            if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max())
                luaL_error(state, "patch_f32 requires a finite IEEE float32 value");
            const float number32 = float(number);
            std::memcpy(&value, &number32, sizeof(value));
        } else {
            const auto number = luaL_checkinteger(state, index);
            const uint64_t maximum = width == 4 ? 0xFFFFFFFFull : ((1ull << (8 * width)) - 1);
            if (number < 0 || uint64_t(number) > maximum)
                luaL_error(state, "integer DTZ patch value is outside its unsigned type range");
            value = uint32_t(number);
        }
        std::vector<uint8_t> bytes;
        for (int i = 0; i < width; ++i) bytes.push_back(uint8_t(value >> (i * 8)));
        return bytes;
    };
    StorylandModDtzPatch patch;
    patch.offset = patchOffset(state);
    patch.bytes = encode(2);
    if (!lua_isnoneornil(state, 3)) patch.expected = encode(3);
    appendPatch(state, std::move(patch));
    return 0;
}

int setGangTurfs(lua_State* state) {
    luaL_checktype(state, 1, LUA_TTABLE);
    StorylandModGangTurfConfig config;
    config.enabled = true;
    config.populationKind = integerField(state, 1, "population_kind", 10, 10, 10);
    config.pedestrianType = integerField(state, 1, "pedestrian_type", 32, 32, 32);
    config.health = integerField(state, 1, "health", 100, 1, 10000);
    config.armour = integerField(state, 1, "armour", 0, 0, 10000);
    lua_getfield(state, 1, "models");
    luaL_checktype(state, -1, LUA_TTABLE);
    if (lua_rawlen(state, -1) != 2) return luaL_error(state, "gang models must contain exactly model A and model B");
    for (int i = 0; i < 2; ++i) {
        lua_rawgeti(state, -1, i + 1);
        const std::string name = luaL_checkstring(state, -1);
        if (name.empty() || name.size() > 23 || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
            return luaL_error(state, "gang model must be a native IMG model stem");
        config.haitianModels[size_t(i)] = name;
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    if (lower(config.haitianModels[0]) == lower(config.haitianModels[1]))
        return luaL_error(state, "gang model A and model B must be distinct native models");
    lua_getfield(state, 1, "weapon_slots");
    if (!lua_isnil(state, -1)) {
        luaL_checktype(state, -1, LUA_TTABLE);
        if (lua_rawlen(state, -1) != 2) return luaL_error(state, "weapon_slots must contain exactly two native weapon slots");
        for (int i = 0; i < 2; ++i) {
            lua_rawgeti(state, -1, i + 1);
            const auto slot = luaL_checkinteger(state, -1);
            if (slot < 0 || slot > 15) return luaL_error(state, "native weapon slot must be in 0..15");
            config.weaponSlots[size_t(i)] = int(slot);
            lua_pop(state, 1);
        }
    }
    lua_pop(state, 1);
    lua_getfield(state, 1, "excluded_zones");
    if (!lua_isnil(state, -1)) {
        luaL_checktype(state, -1, LUA_TTABLE);
        const size_t count = lua_rawlen(state, -1);
        if (count > 256) return luaL_error(state, "too many excluded zones");
        for (size_t i = 1; i <= count; ++i) {
            lua_rawgeti(state, -1, lua_Integer(i));
            const std::string zone = luaL_checkstring(state, -1);
            if (zone.empty() || zone.size() > 8) return luaL_error(state, "excluded zone must be a native zone name of 1..8 bytes");
            config.excludedZones.push_back(zone);
            lua_pop(state, 1);
        }
    }
    lua_pop(state, 1);
    lua_getfield(state, 1, "territories");
    luaL_checktype(state, -1, LUA_TTABLE);
    const int territories = lua_gettop(state);
    const size_t count = lua_rawlen(state, territories);
    if (count == 0 || count > 128) return luaL_error(state, "territories must contain 1..128 definitions");
    std::set<std::string> ids;
    for (size_t i = 1; i <= count; ++i) {
        lua_rawgeti(state, territories, lua_Integer(i));
        luaL_checktype(state, -1, LUA_TTABLE);
        const int row = lua_gettop(state);
        StorylandModTurfDefinition territory;
        territory.id = stringField(state, row, "id");
        territory.name = stringField(state, row, "name");
        if (territory.id.size() > 64 || territory.name.size() > 128)
            return luaL_error(state, "territory ID must fit 64 bytes and display name 128 bytes");
        territory.districtZone = stringField(state, row, "district_zone");
        territory.owner = lower(stringField(state, row, "owner"));
        if (territory.owner != "cholos" && territory.owner != "haitians" && territory.owner != "vance")
            return luaL_error(state, "territory owner must be cholos, haitians or vance");
        if (!ids.insert(territory.id).second) return luaL_error(state, "territory IDs must be unique: %s", territory.id.c_str());
        if (territory.districtZone.size() > 8) return luaL_error(state, "district_zone must be a native zone name of 1..8 bytes");
        territory.dailyIncome = integerField(state, row, "daily_income", 0, 0, 1000000);
        lua_getfield(state, row, "bounds");
        luaL_checktype(state, -1, LUA_TTABLE);
        if (lua_rawlen(state, -1) != 4) return luaL_error(state, "territory bounds must contain xmin,ymin,xmax,ymax");
        for (int j = 0; j < 4; ++j) {
            lua_rawgeti(state, -1, j + 1);
            const double value = luaL_checknumber(state, -1);
            if (!std::isfinite(value) || value < 0 || value > 1)
                return luaL_error(state, "territory bounds must be finite fractions inside 0..1 of the native zone");
            territory.bounds[size_t(j)] = float(value);
            lua_pop(state, 1);
        }
        lua_pop(state, 1);
        if (territory.bounds[0] >= territory.bounds[2] || territory.bounds[1] >= territory.bounds[3])
            return luaL_error(state, "territory normalized bounds must have positive width and height");
        config.territories.push_back(std::move(territory));
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    auto& mod = *context(state).mod;
    mod.hasGangTurfs = true;
    mod.gangTurfs = std::move(config);
    return 0;
}

bool executeManifest(StorylandVcsMod& mod, const std::filesystem::path& script, std::string& error) {
    std::ifstream input(script, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 0 || uint64_t(input.tellg()) > MaxScriptBytes) {
        error = script.string() + ": cannot read manifest or manifest exceeds 1 MiB";
        return false;
    }
    std::string source(size_t(input.tellg()), '\0');
    input.seekg(0);
    if (!source.empty() && !input.read(source.data(), std::streamsize(source.size()))) {
        error = script.string() + ": failed reading manifest";
        return false;
    }
    LuaContext host;
    host.mod = &mod;
    lua_State* state = lua_newstate(luaAllocate, &host);
    if (!state) { error = script.string() + ": cannot create Lua 5.4 VM"; return false; }
    *static_cast<LuaContext**>(lua_getextraspace(state)) = &host;
    const luaL_Reg libraries[] = {
        {LUA_GNAME, luaopen_base}, {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string}, {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8}, {nullptr, nullptr}
    };
    for (const luaL_Reg* library = libraries; library->func; ++library) {
        luaL_requiref(state, library->name, library->func, 1);
        lua_pop(state, 1);
    }
    // Lua executes data manifests. Retail file IO remains in the mounted override resolver.
    for (const char* name : {"dofile", "loadfile", "load", "collectgarbage", "print"}) {
        lua_pushnil(state); lua_setglobal(state, name);
    }
    lua_newtable(state);
    const luaL_Reg functions[] = {
        {"mod", setMod}, {"replace", replaceFile}, {"patch_bytes", patchBytes},
        {"gang_turfs", setGangTurfs}, {nullptr, nullptr}
    };
    luaL_setfuncs(state, functions, 0);
    const std::array<std::pair<const char*, int>, 4> numbers{{
        {"patch_u8", 1}, {"patch_u16", 2}, {"patch_u32", 4}, {"patch_f32", 4}
    }};
    for (const auto& number : numbers) {
        lua_pushinteger(state, number.second);
        lua_pushboolean(state, std::strcmp(number.first, "patch_f32") == 0);
        lua_pushcclosure(state, patchNumber, 2);
        lua_setfield(state, -2, number.first);
    }
    lua_pushliteral(state, "vcs-ps2"); lua_setfield(state, -2, "target");
    lua_pushliteral(state, LUA_VERSION); lua_setfield(state, -2, "lua_version");
    lua_setglobal(state, "storyland");
    lua_sethook(state, instructionHook, LUA_MASKCOUNT, 1000);
    const std::string chunkName = "@" + script.string();
    const int loaded = luaL_loadbufferx(state, source.data(), source.size(), chunkName.c_str(), "t");
    const int result = loaded == LUA_OK ? lua_pcall(state, 0, 0, 0) : loaded;
    if (result != LUA_OK) {
        const char* detail = lua_tostring(state, -1);
        error = detail ? detail : script.string() + ": Lua manifest failed";
        if (error.find("mod.lua") == std::string::npos) error = script.string() + ": " + error;
    }
    lua_close(state);
    return result == LUA_OK;
}

bool excludedAutoFile(const std::filesystem::path& file) {
    const auto name = lower(file.filename().string());
    return name == "mod.lua" || name == ".disabled" || name == "readme.md" || name == "readme.txt";
}

std::filesystem::path automaticModFile(const StorylandVcsMod& mod, const std::string& target,
    bool allowAlias = true, bool searchBasename = true) {
    const auto direct = matchingPath(mod.directory, target);
    std::error_code ec;
    if (!direct.empty() && std::filesystem::is_regular_file(direct, ec) && withinDirectory(mod.directory, direct))
        return direct;
    std::filesystem::path result;
    if (searchBasename && target.find('/') == std::string::npos) {
        for (const auto& file : modFiles(mod.directory))
            if (!excludedAutoFile(file) && lower(file.filename().string()) == target) result = file;
    }
    if (!result.empty() || !allowAlias) return result;
    auto alias = std::filesystem::path(target);
    const auto filename = alias.filename().string();
    if (filename != "gta3ps2.img" && filename != "gta3ps2.dir") return {};
    alias.replace_filename(filename == "gta3ps2.img" ? "gtaps2.img" : "gtaps2.dir");
    return automaticModFile(mod, alias.generic_string(), false, searchBasename);
}

template <typename Lookup>
std::filesystem::path scopedModFile(const StorylandVcsMod& mod, const std::string& target,
    const Lookup& automaticFile) {
    const auto basename = std::filesystem::path(target).filename().generic_string();
    auto selected = automaticFile(basename, false);
    const auto scoped = automaticFile(target, false);
    if (!scoped.empty()) selected = scoped;
    if (selected.empty()) {
        selected = automaticFile(basename, true);
        const auto scopedAlias = automaticFile(target, true);
        if (!scopedAlias.empty()) selected = scopedAlias;
    }
    for (const auto& replacement : mod.replacements)
        if (replacement.first == basename) selected = replacement.second;
    for (const auto& replacement : mod.replacements)
        if (replacement.first == target) selected = replacement.second;
    return selected;
}

std::filesystem::path scopedModFile(const StorylandVcsMod& mod, const std::string& target) {
    return scopedModFile(mod, target, [&](const std::string& name, bool allowAlias) {
        return automaticModFile(mod, name, allowAlias, false);
    });
}

} // namespace

bool StorylandVcsModloader::mount(const std::filesystem::path& gameRoot,
    StorylandModloaderTarget target, std::string& error) {
    error.clear();
    if (target != StorylandModloaderTarget::VcsPs2) {
        error = "Lua Modloader currently supports Vice City Stories PS2 only; LCS and PSP mounts are rejected.";
        return false;
    }
    std::error_code ec;
    const auto pendingRoot = std::filesystem::absolute(gameRoot, ec).lexically_normal();
    if (ec || !std::filesystem::is_directory(pendingRoot, ec)) {
        error = "Modloader game root is not a readable directory: " + gameRoot.string();
        return false;
    }
    const auto modRoot = matchingPath(pendingRoot, "modloader");
    if (modRoot.empty()) {
        root = pendingRoot;
        mounted.clear();
        gangTurfs = {};
        return true;
    }
    if (!std::filesystem::is_directory(modRoot, ec)) {
        error = modRoot.string() + ": modloader must be a directory";
        return false;
    }
    std::vector<std::filesystem::path> folders;
    for (std::filesystem::directory_iterator it(modRoot, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec) && !it->is_symlink(ec)) folders.push_back(it->path());
    if (ec) { error = modRoot.string() + ": cannot enumerate mod folders: " + ec.message(); return false; }
    std::sort(folders.begin(), folders.end(), [](const auto& left, const auto& right) {
        const auto a = lower(left.filename().string()), b = lower(right.filename().string());
        return a == b ? left.filename().string() < right.filename().string() : a < b;
    });
    std::vector<StorylandVcsMod> pending;
    for (const auto& folder : folders) {
        if (!matchingPath(folder, ".disabled").empty()) continue;
        StorylandVcsMod mod;
        mod.directory = folder;
        mod.name = folder.filename().string();
        const auto script = matchingPath(folder, "mod.lua");
        if (!script.empty() && !withinDirectory(folder, script)) {
            error = script.string() + ": mod.lua must remain inside its mod directory";
            return false;
        }
        if (!script.empty() && !executeManifest(mod, script, error)) return false;
        if (!mod.enabled) continue;
        std::set<std::string> names;
        for (const auto& file : modFiles(folder)) {
            const std::string key = normalizedName(file.lexically_relative(folder).generic_string());
            if (!names.insert(key).second) {
                error = folder.string() + ": ambiguous case-insensitive filename: " + key;
                return false;
            }
        }
        pending.push_back(std::move(mod));
    }
    std::stable_sort(pending.begin(), pending.end(), [](const auto& left, const auto& right) {
        return left.priority < right.priority;
    });
    StorylandModGangTurfConfig pendingGangTurfs;
    for (const auto& mod : pending) if (mod.hasGangTurfs) pendingGangTurfs = mod.gangTurfs;
    root = pendingRoot;
    gangTurfs = std::move(pendingGangTurfs);
    mounted = std::move(pending);
    return true;
}

std::filesystem::path StorylandVcsModloader::resolveGameFile(const std::string& retailName,
    const std::filesystem::path& fallback) const {
    if (!relativeName(retailName)) return fallback;
    const std::string target = retailFileKey(retailName);
    const bool basenameOnly = target.find('/') == std::string::npos;
    std::filesystem::path result = fallback;
    if (result.empty()) {
        result = matchingPath(root, target);
        if (result.empty() && basenameOnly) result = storylandFindNativeGameFile(root, target);
    }
    for (const auto& mod : mounted) {
        if (!basenameOnly) {
            const auto selected = scopedModFile(mod, target);
            if (!selected.empty()) result = selected;
            continue;
        }
        const auto automatic = automaticModFile(mod, target);
        if (!automatic.empty()) result = automatic;
        for (const auto& replacement : mod.replacements)
            if (replacement.first == target || (basenameOnly && lower(std::filesystem::path(replacement.first).filename().string()) == target))
                result = replacement.second;
    }
    return result;
}

bool StorylandVcsModloader::applyDtzPatches(std::vector<uint8_t>& bytes, std::string& error) const {
    error.clear();
    bool any = false;
    for (const auto& mod : mounted) any = any || !mod.patches.empty();
    if (!any) return true;
    std::vector<uint8_t> patched = bytes;
    for (const auto& mod : mounted) for (const auto& patch : mod.patches) {
        std::ostringstream location;
        location << patch.source << ": DTZ patch at 0x" << std::hex << patch.offset;
        if (patch.offset > patched.size() || patch.bytes.size() > patched.size() - patch.offset) {
            error = location.str() + " exceeds the unpacked GAME.DTZ bounds; check the game region/revision";
            return false;
        }
        if (!patch.expected.empty() && !std::equal(patch.expected.begin(), patch.expected.end(), patched.begin() + patch.offset)) {
            error = location.str() + " expected original bytes do not match; check mod order and game region/revision";
            return false;
        }
        std::copy(patch.bytes.begin(), patch.bytes.end(), patched.begin() + patch.offset);
    }
    bytes.swap(patched);
    return true;
}

std::filesystem::path StorylandVcsModloader::resolveRetailPath(const std::filesystem::path& retailPath) const {
    const auto basename = retailFileKey(retailPath.filename().string());
    std::error_code ec;
    const auto absolute = std::filesystem::absolute(retailPath, ec).lexically_normal();
    if (ec) return resolveGameFile(basename, retailPath);
    const auto relative = absolute.lexically_relative(root);
    if (relative.empty() || !relativeName(relative.generic_string())) return resolveGameFile(basename, retailPath);
    const auto scopedName = retailFileKey(relative.generic_string());
    std::filesystem::path result = retailPath;
    for (const auto& mod : mounted) {
        const auto selected = scopedModFile(mod, scopedName);
        if (!selected.empty()) result = selected;
    }
    return result;
}

std::vector<std::pair<std::string, std::filesystem::path>> StorylandVcsModloader::files() const {
    std::map<std::string, std::filesystem::path> effective;
    for (const auto& mod : mounted) {
        std::map<std::string, std::filesystem::path> automatic;
        for (const auto& file : modFiles(mod.directory)) {
            if (excludedAutoFile(file)) continue;
            const auto name = normalizedName(file.lexically_relative(mod.directory).generic_string());
            automatic.emplace(name, file);
            effective.try_emplace(retailFileKey(name), file);
        }
        for (const auto& replacement : mod.replacements)
            effective.try_emplace(replacement.first, replacement.second);
        const auto lookup = [&](const std::string& name, bool allowAlias) {
            const auto direct = automatic.find(name);
            if (direct != automatic.end()) return direct->second;
            if (!allowAlias) return std::filesystem::path{};
            auto alias = std::filesystem::path(name);
            const auto filename = alias.filename().string();
            if (filename != "gta3ps2.img" && filename != "gta3ps2.dir") return std::filesystem::path{};
            alias.replace_filename(filename == "gta3ps2.img" ? "gtaps2.img" : "gtaps2.dir");
            const auto found = automatic.find(alias.generic_string());
            return found == automatic.end() ? std::filesystem::path{} : found->second;
        };
        for (auto& file : effective) {
            const auto selected = scopedModFile(mod, file.first, lookup);
            if (!selected.empty()) file.second = selected;
        }
    }
    return {effective.begin(), effective.end()};
}

bool StorylandVcsModloader::active() const {
    return !mounted.empty();
}
