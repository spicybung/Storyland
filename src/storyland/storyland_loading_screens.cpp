#include "src/storyland/storyland_loading_screens.h"
#include "src/storyland/storyland_game_files.h"

#include <SDL.h>
#include <map>
#include <memory>
#include <set>

namespace {
constexpr size_t loadingFileLimit = 32u * 1024u * 1024u;
constexpr size_t loadingPixelLimit = 64u * 1024u * 1024u;

std::string loadingLowerName(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) { return char(std::tolower(character)); });
    return value;
}

std::filesystem::path loadingNamedFolder(const std::filesystem::path& root,
    const std::string& name) {
    if (loadingLowerName(root.filename().u8string()) == name) return root;
    std::error_code error;
    std::filesystem::directory_iterator cursor(root,
        std::filesystem::directory_options::skip_permission_denied, error), end;
    for (; !error && cursor != end; cursor.increment(error)) {
        if (loadingLowerName(cursor->path().filename().u8string()) != name) continue;
        std::error_code entryError;
        if (cursor->is_directory(entryError) && !entryError) return cursor->path();
    }
    return {};
}

std::filesystem::path loadingArtworkDirectory() {
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
                const auto candidate = root / "assets" / "loading" / "vcs";
                error.clear();
                if (std::filesystem::is_directory(candidate, error) && !error)
                    return candidate;
            }
            const auto parent = root.parent_path();
            if (parent == root || parent == parent.root_path()) break;
            root = parent;
        }
    }
    return {};
}

bool loadingReadImage(const std::filesystem::path& path, RgbaImage& image,
    size_t availableBytes) {
    const auto filename = path.u8string();
    std::unique_ptr<SDL_RWops, decltype(&SDL_RWclose)> source(
        SDL_RWFromFile(filename.c_str(), "rb"), SDL_RWclose);
    if (!source) return false;
    const Sint64 length = SDL_RWsize(source.get());
    if (length <= 0 || uint64_t(length) > loadingFileLimit) return false;
    std::vector<uint8_t> bytes(size_t(length), 0);
    if (SDL_RWread(source.get(), bytes.data(), 1, bytes.size()) != bytes.size())
        return false;

    LeedsTextureArchive textures;
    std::string diagnostic;
    if (!textures.loadFromMemory(bytes, LeedsPlatform::Auto, diagnostic)) return false;
    size_t largest = SIZE_MAX;
    size_t area = 0;
    const auto& entries = textures.textures();
    for (size_t index = 0; index < entries.size(); ++index) {
        const auto& texture = entries[index];
        if (texture.width < 64 || texture.height < 64 ||
            texture.width > 2048 || texture.height > 2048) continue;
        const size_t pixels = size_t(texture.width) * size_t(texture.height);
        if (pixels > area) {
            area = pixels;
            largest = index;
        }
    }
    if (largest == SIZE_MAX || area * 4 > availableBytes) return false;
    if (!textures.decodeTexture(largest, image, diagnostic) || image.rgba.size() != area * 4)
        return false;
    return true;
}

bool loadingReadBundledVcsImage(const std::filesystem::path& directory,
    const std::string& name, bool allowPackageAssets, size_t availableBytes,
    std::filesystem::path& path, RgbaImage& image) {
    path = directory / (name + ".xtx");
    bool decoded = !directory.empty() && loadingReadImage(path, image, availableBytes);
#if defined(__ANDROID__)
    if (!decoded && allowPackageAssets) {
        path = std::filesystem::path("loading") / "vcs" / (name + ".xtx");
        decoded = loadingReadImage(path, image, availableBytes);
    }
#else
    (void)allowPackageAssets;
#endif
    return decoded;
}

std::vector<StorylandLoadingScreen> loadingReadBundledVcsArtwork(
    const std::filesystem::path& artworkDirectory) {
    const auto directory = artworkDirectory.empty() ? loadingArtworkDirectory() : artworkDirectory;
    std::vector<StorylandLoadingScreen> result;
    size_t decodedBytes = 0;
    for (unsigned index = 0; index < 12; ++index) {
        const std::string name = "loadsc" + std::to_string(index);
        std::filesystem::path path;
        RgbaImage image;
        if (!loadingReadBundledVcsImage(directory, name, artworkDirectory.empty(),
            loadingPixelLimit - decodedBytes, path, image)) continue;
        decodedBytes += image.rgba.size();
        result.push_back({path, name, std::move(image), true});
    }
    return result;
}
}

std::vector<StorylandLoadingScreen> storylandReadVcsSplashScreens(
    const std::filesystem::path& artworkDirectory) {
    return loadingReadBundledVcsArtwork(artworkDirectory);
}

std::vector<StorylandLoadingScreen> storylandReadLoadingScreens(
    const std::filesystem::path& gamePath, bool vcs,
    const std::filesystem::path& artworkDirectory) {
    std::vector<StorylandLoadingScreen> result;
    if (vcs) {
        result = loadingReadBundledVcsArtwork(artworkDirectory);
        if (result.size() == 12) return result;
    }
    if (gamePath.empty()) return result;

    std::error_code error;
    const auto root = std::filesystem::absolute(gamePath, error);
    if (error) return result;
    std::set<std::filesystem::path> splashFolders;
    for (const auto& folder : storylandNativeGameFolders(root)) {
        const auto splash = loadingNamedFolder(folder, "splash");
        if (!splash.empty()) splashFolders.insert(splash);
    }

    std::map<std::pair<int, int>, std::filesystem::path> files;
    for (const auto& splash : splashFolders) {
        error.clear();
        std::filesystem::directory_iterator cursor(splash,
            std::filesystem::directory_options::skip_permission_denied, error), end;
        for (; !error && cursor != end; cursor.increment(error)) {
            std::error_code entryError;
            if (!cursor->is_regular_file(entryError) || entryError) continue;
            auto order = storylandLoadingArtworkOrder(cursor->path().stem().u8string());
            if (order.first < 0) continue;
            if (vcs) order.first = 0;
            const auto extension = loadingLowerName(cursor->path().extension().u8string());
            if (extension != ".xtx" && extension != ".chk" && extension != ".txd") continue;
            files.emplace(order, cursor->path());
        }
    }

    size_t decodedBytes = 0;
    std::set<std::pair<int, int>> selected;
    for (const auto& screen : result) {
        selected.insert(storylandLoadingArtworkOrder(screen.name));
        decodedBytes += screen.image.rgba.size();
    }
    for (const auto& file : files) {
        if (result.size() >= 64) break;
        if (selected.count(file.first)) continue;
        RgbaImage image;
        if (!loadingReadImage(file.second, image, loadingPixelLimit - decodedBytes)) continue;
        decodedBytes += image.rgba.size();
        const auto name = vcs ? "loadsc" + std::to_string(file.first.second) :
            loadingLowerName(file.second.stem().u8string());
        result.push_back({file.second, name, std::move(image), vcs});
    }
    std::sort(result.begin(), result.end(),
        [](const StorylandLoadingScreen& first, const StorylandLoadingScreen& second) {
            return storylandLoadingArtworkOrder(first.name) < storylandLoadingArtworkOrder(second.name);
        });
    return result;
}
