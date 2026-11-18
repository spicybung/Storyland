#include "src/storyland/storyland_pc_assets.h"

#include <SDL.h>
#include <memory>
#include <set>
#include <utility>

namespace {
constexpr size_t pcArtworkFileLimit = 32u * 1024u * 1024u;
constexpr size_t pcArtworkDecodedLimit = 64u * 1024u * 1024u;

struct PcArtworkFile {
    StorylandPcTexturePack pack;
    const char* group;
    const char* path;
};

constexpr PcArtworkFile pcArtworkFiles[] = {
    {StorylandPcTexturePack::Hud, "blips", "hud/blips.xtx"},
    {StorylandPcTexturePack::Hud, "overlays", "hud/overlays.xtx"},
    {StorylandPcTexturePack::Hud, "weapons", "hud/weapons.xtx"},
    {StorylandPcTexturePack::Frontend, "fronten_pc", "frontend/fronten_pc.xtx"},
    {StorylandPcTexturePack::Frontend, "fronten1", "frontend/fronten1.xtx"},
    {StorylandPcTexturePack::Frontend, "fronten2", "frontend/fronten2.xtx"},
    {StorylandPcTexturePack::Fonts, "fonts", "fonts.xtx"},
    {StorylandPcTexturePack::Particles, "particle", "particle.xtx"},
    {StorylandPcTexturePack::Buttons, "pcbtns", "pcbtns.xtx"},
};

std::filesystem::path pcArtworkDirectory() {
    std::vector<std::filesystem::path> roots;
    if (char* executable = SDL_GetBasePath()) {
        roots.push_back(std::filesystem::u8path(executable));
        SDL_free(executable);
    }
    std::error_code error;
    const auto workingDirectory = std::filesystem::current_path(error);
    if (!error) roots.push_back(workingDirectory);
    std::set<std::filesystem::path> visited;
    for (auto root : roots) {
        for (unsigned depth = 0; depth < 4 && !root.empty(); ++depth) {
            root = root.lexically_normal();
            if (visited.insert(root).second) {
                const auto candidate = root / "assets" / "pc";
                error.clear();
                if (std::filesystem::is_regular_file(candidate / "manifest.txt", error) && !error)
                    return candidate;
            }
            const auto parent = root.parent_path();
            if (parent == root || parent == parent.root_path()) break;
            root = parent;
        }
    }
    return {};
}

bool pcArtworkBytes(const std::filesystem::path& path, std::vector<uint8_t>& bytes) {
    const auto filename = path.u8string();
    std::unique_ptr<SDL_RWops, decltype(&SDL_RWclose)> source(
        SDL_RWFromFile(filename.c_str(), "rb"), SDL_RWclose);
    if (!source) return false;
    const Sint64 length = SDL_RWsize(source.get());
    if (length <= 0 || uint64_t(length) > pcArtworkFileLimit) return false;
    bytes.resize(size_t(length));
    return SDL_RWread(source.get(), bytes.data(), 1, bytes.size()) == bytes.size();
}
}

bool storylandReadPcTextures(StorylandPcTexturePack pack,
    std::vector<StorylandPcTexture>& textures, std::string& diagnostic,
    const std::filesystem::path& artworkDirectory) {
    textures.clear();
    diagnostic.clear();
    const auto directory = artworkDirectory.empty() ? pcArtworkDirectory() : artworkDirectory;
    bool complete = true;
    size_t decodedBytes = 0;
    std::set<std::pair<std::string, std::string>> identities;
    const auto failed = [&](const char* path, const std::string& detail) {
        complete = false;
        if (diagnostic.empty()) diagnostic = std::string("PC artwork ") + path + ": " + detail;
    };
    for (const auto& file : pcArtworkFiles) {
        if (file.pack != pack) continue;
        std::vector<uint8_t> bytes;
        bool available = !directory.empty() && pcArtworkBytes(directory / file.path, bytes);
#if defined(__ANDROID__)
        if (!available && artworkDirectory.empty())
            available = pcArtworkBytes(std::filesystem::path("pc") / file.path, bytes);
#endif
        if (!available) {
            failed(file.path, "file is missing, unreadable or too large.");
            continue;
        }
        LeedsTextureArchive archive;
        std::string error;
        if (!archive.loadFromMemory(bytes, LeedsPlatform::Auto, error,
                std::filesystem::path(file.path).wstring()) || archive.textures().empty() ||
            archive.textures().size() > 256) {
            failed(file.path, error.empty() ? "invalid texture dictionary." : error);
            continue;
        }
        for (size_t index = 0; index < archive.textures().size(); ++index) {
            const auto& entry = archive.textures()[index];
            const size_t availableBytes = pcArtworkDecodedLimit - decodedBytes;
            if (entry.width <= 0 || entry.height <= 0 || entry.width > 2048 || entry.height > 2048 ||
                uint64_t(entry.width) * uint64_t(entry.height) * 4u > availableBytes || entry.name.empty()) {
                failed(file.path, "texture dimensions or decoded-size budget are invalid.");
                continue;
            }
            RgbaImage image;
            if (!archive.decodeTexture(index, image, error) ||
                image.rgba.size() != size_t(image.width) * size_t(image.height) * 4u ||
                rgbaImageDecodedBytes(image) > availableBytes) {
                failed(file.path, error.empty() ? "invalid decoded texture." : error);
                continue;
            }
            if (!identities.emplace(file.group, entry.name).second) {
                failed(file.path, "duplicate texture identity.");
                continue;
            }
            decodedBytes += rgbaImageDecodedBytes(image);
            textures.push_back({file.group, entry.name, std::move(image)});
        }
    }
    if (textures.empty() && diagnostic.empty()) {
        diagnostic = "No textures are defined for this PC artwork pack.";
        complete = false;
    }
    return complete;
}
