#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

inline std::string storylandNativePathLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return char(std::tolower(character));
    });
    return value;
}

inline bool storylandNativeGameFolderName(const std::filesystem::path& path) {
    static const std::set<std::string> names = {
        "ps2", "psp", "chk", "rundata", "models", "data", "configs",
        "scripts", "anim", "text", "usrdir", "psp_game", "levels", "lvz", "img"
    };
    return names.count(storylandNativePathLower(path.filename().string())) != 0;
}

// Search only known game folders, without recursively inspecting unrelated files.
inline std::vector<std::filesystem::path> storylandNativeGameFolders(std::filesystem::path path) {
    std::error_code error;
    if (!std::filesystem::is_directory(path, error)) path = path.parent_path();
    if (path.empty()) {
        error.clear();
        path = std::filesystem::current_path(error);
        if (error) return {};
    }
    path = path.lexically_normal();
    std::vector<std::filesystem::path> folders;
    std::set<std::filesystem::path> seen;
    const auto append = [&](const std::filesystem::path& candidate) {
        std::error_code candidateError;
        if (!std::filesystem::is_directory(candidate, candidateError) || candidateError) return false;
        const auto normalized = candidate.lexically_normal();
        if (!seen.insert(normalized).second) return false;
        folders.push_back(normalized);
        return true;
    };
    append(path);
    for (unsigned level = 0; level < 6 && storylandNativeGameFolderName(path); ++level) {
        const auto parent = path.parent_path();
        if (parent.empty() || parent == path || parent == parent.root_path()) break;
        path = parent;
        append(path);
    }
    // A game root may contain PSP_GAME/USRDIR/RUNDATA/PSP, four levels deep.
    for (unsigned depth = 0; depth < 4; ++depth) {
        const size_t end = folders.size();
        for (size_t index = 0; index < end; ++index) {
            std::vector<std::filesystem::path> children;
            error.clear();
            std::filesystem::directory_iterator cursor(folders[index],
                std::filesystem::directory_options::skip_permission_denied, error), last;
            for (; !error && cursor != last; cursor.increment(error)) {
                std::error_code childError;
                if (!cursor->is_directory(childError) || childError || cursor->is_symlink(childError) ||
                    childError || !storylandNativeGameFolderName(cursor->path())) continue;
                children.push_back(cursor->path());
            }
            std::sort(children.begin(), children.end());
            for (const auto& child : children) append(child);
        }
        if (folders.size() == end) break;
    }
    return folders;
}

inline std::filesystem::path storylandFindNativeGameFile(const std::filesystem::path& root,
    const std::string& filename) {
    const auto target = storylandNativePathLower(filename);
    for (const auto& folder : storylandNativeGameFolders(root)) {
        std::error_code error;
        std::filesystem::directory_iterator cursor(folder,
            std::filesystem::directory_options::skip_permission_denied, error), last;
        for (; !error && cursor != last; cursor.increment(error)) {
            std::error_code itemError;
            if (cursor->is_regular_file(itemError) && !itemError &&
                storylandNativePathLower(cursor->path().filename().string()) == target)
                return cursor->path();
        }
    }
    return {};
}

inline std::vector<std::filesystem::path> storylandFindNativeWorldAreas(const std::filesystem::path& root) {
    std::set<std::filesystem::path> areas;
    for (const auto& folder : storylandNativeGameFolders(root)) {
        std::error_code error;
        std::filesystem::directory_iterator cursor(folder,
            std::filesystem::directory_options::skip_permission_denied, error), last;
        for (; !error && cursor != last; cursor.increment(error)) {
            std::error_code itemError;
            if (cursor->is_regular_file(itemError) && !itemError &&
                storylandNativePathLower(cursor->path().extension().string()) == ".lvz")
                areas.insert(cursor->path());
        }
    }
    return {areas.begin(), areas.end()};
}
