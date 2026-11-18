#include "src/re/reVCS/modloader/storyland_vcs_modloader_install.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace {

std::string lowerName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char letter) { return char(std::tolower(letter)); });
    return name;
}

std::filesystem::path findChild(const std::filesystem::path& root, const std::string& name) {
    std::error_code error;
    std::vector<std::filesystem::path> found;
    for (std::filesystem::directory_iterator it(root, error), end; !error && it != end; it.increment(error))
        if (lowerName(it->path().filename().string()) == lowerName(name)) found.push_back(it->path());
    std::sort(found.begin(), found.end());
    return found.empty() ? std::filesystem::path{} : found.front();
}

bool containsManifest(const std::filesystem::path& root) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) return false;
    for (std::filesystem::directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        if (!it->is_directory(error) || it->is_symlink(error)) continue;
        const auto manifest = findChild(it->path(), "mod.lua");
        if (!manifest.empty() && std::filesystem::is_regular_file(manifest, error)) return true;
    }
    return false;
}

bool childOf(const std::filesystem::path& parent, const std::filesystem::path& child) {
    auto left = parent.begin();
    auto right = child.begin();
    for (; left != parent.end(); ++left, ++right)
        if (right == child.end() || *left != *right) return false;
    return true;
}

bool copyTree(const std::filesystem::path& source, const std::filesystem::path& target,
    std::string& error) {
    std::error_code operation;
    if (std::filesystem::is_symlink(source, operation)) {
        error = "Bundled Modloader contains a symbolic link: " + source.string();
        return false;
    }
    if (std::filesystem::is_regular_file(source, operation)) {
        if (!std::filesystem::copy_file(source, target, std::filesystem::copy_options::none, operation)) {
            error = "Cannot stage Modloader file " + source.string() + ": " + operation.message();
            return false;
        }
        return true;
    }
    if (!std::filesystem::is_directory(source, operation) ||
        !std::filesystem::create_directory(target, operation)) {
        error = "Cannot stage Modloader directory " + target.string() + ": " + operation.message();
        return false;
    }
    std::vector<std::filesystem::path> children;
    for (std::filesystem::directory_iterator it(source, operation), end; !operation && it != end; it.increment(operation))
        children.push_back(it->path());
    if (operation) { error = "Cannot read bundled Modloader: " + operation.message(); return false; }
    std::sort(children.begin(), children.end());
    for (const auto& child : children)
        if (!copyTree(child, target / child.filename(), error)) return false;
    return true;
}

enum class MoveResult { Moved, Exists, Failed };

MoveResult moveWithoutReplacing(const std::filesystem::path& source,
    const std::filesystem::path& target, std::string& error) {
    std::error_code existingError;
    if (std::filesystem::exists(target, existingError)) return MoveResult::Exists;
#ifdef _WIN32
    if (MoveFileExW(source.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) return MoveResult::Moved;
    const auto code = GetLastError();
    if (code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS) return MoveResult::Exists;
    error = "Cannot install Modloader at " + target.string() + ": " +
        std::error_code(int(code), std::system_category()).message();
    return MoveResult::Failed;
#elif defined(__linux__) && defined(SYS_renameat2)
    constexpr unsigned RenameNoReplace = 1;
    if (syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD, target.c_str(), RenameNoReplace) == 0)
        return MoveResult::Moved;
    const auto code = errno;
    if (code == EEXIST || code == ENOTEMPTY) return MoveResult::Exists;
    error = "Cannot atomically install Modloader at " + target.string() + ": " +
        std::error_code(code, std::generic_category()).message();
    return MoveResult::Failed;
#else
    std::error_code operation;
    if (std::filesystem::exists(target, operation)) return MoveResult::Exists;
    error = "Atomic installation without replacing existing mods is unavailable on this platform.";
    return MoveResult::Failed;
#endif
}

struct InstallationStage {
    std::filesystem::path path;
    ~InstallationStage() {
        if (path.empty()) return;
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

} // namespace

std::filesystem::path storylandFindBundledVcsModloader(const std::filesystem::path& executablePath) {
    std::error_code error;
    auto directory = std::filesystem::absolute(executablePath, error).lexically_normal();
    if (error) return {};
    if (!std::filesystem::is_directory(directory, error)) directory = directory.parent_path();
    for (int depth = 0; depth < 5 && !directory.empty(); ++depth) {
        const auto candidate = directory / "assets" / "modloader";
        if (containsManifest(candidate)) return candidate;
        if (directory.parent_path() == directory) break;
        directory = directory.parent_path();
    }
    return {};
}

StorylandModloaderInstallResult storylandInstallVcsModloader(
    const std::filesystem::path& bundledModloader, const std::filesystem::path& gameRoot,
    StorylandModloaderTarget target, std::string& error) {
    error.clear();
    if (target != StorylandModloaderTarget::VcsPs2) {
        error = "Modloader installation currently supports VCS PS2 only.";
        return StorylandModloaderInstallResult::Unsupported;
    }
    if (bundledModloader.empty() || gameRoot.empty()) {
        error = "Bundled Modloader and detected game directory paths must both be provided.";
        return StorylandModloaderInstallResult::Failed;
    }
    std::error_code operation;
    const auto source = std::filesystem::weakly_canonical(bundledModloader, operation);
    if (operation || !containsManifest(source)) {
        error = "Bundled Modloader folder is missing or contains no mod.lua manifests: " + bundledModloader.string();
        return StorylandModloaderInstallResult::Failed;
    }
    const auto root = std::filesystem::weakly_canonical(gameRoot, operation);
    if (operation || !std::filesystem::is_directory(root, operation)) {
        error = "Detected game directory is unavailable: " + gameRoot.string();
        return StorylandModloaderInstallResult::Failed;
    }
    auto destination = findChild(root, "modloader");
    if (destination.empty()) destination = root / "modloader";
    const auto normalizedDestination = std::filesystem::weakly_canonical(destination, operation);
    if (operation) { error = "Cannot resolve game Modloader directory: " + operation.message(); return StorylandModloaderInstallResult::Failed; }
    if (source == normalizedDestination) return StorylandModloaderInstallResult::AlreadyPresent;
    if (childOf(source, normalizedDestination) || childOf(normalizedDestination, source)) {
        error = "Bundled and game Modloader folders must be separate directories.";
        return StorylandModloaderInstallResult::Failed;
    }
    if (std::filesystem::exists(destination, operation) &&
        (!std::filesystem::is_directory(destination, operation) || std::filesystem::is_symlink(destination, operation))) {
        error = "Game Modloader path already exists as a file or symbolic link: " + destination.string();
        return StorylandModloaderInstallResult::Failed;
    }
    static std::atomic<uint64_t> sequence{0};
    InstallationStage stage;
    for (int attempt = 0; attempt < 16; ++attempt) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto candidate = root / (".storyland-modloader-install-" + std::to_string(stamp) + "-" +
            std::to_string(sequence.fetch_add(1)));
        if (std::filesystem::create_directory(candidate, operation)) { stage.path = candidate; break; }
        if (operation && operation != std::errc::file_exists) break;
        operation.clear();
    }
    if (stage.path.empty()) {
        error = "Cannot create a temporary installation directory in " + root.string() + ": " + operation.message();
        return StorylandModloaderInstallResult::Failed;
    }
    const auto stagedRoot = stage.path / "modloader";
    if (!copyTree(source, stagedRoot, error)) return StorylandModloaderInstallResult::Failed;
    auto moved = moveWithoutReplacing(stagedRoot, destination, error);
    if (moved == MoveResult::Moved) return StorylandModloaderInstallResult::Installed;
    if (moved == MoveResult::Failed) return StorylandModloaderInstallResult::Failed;
    if (!std::filesystem::is_directory(destination, operation) || std::filesystem::is_symlink(destination, operation)) {
        error = "Existing game Modloader path is not an ordinary directory: " + destination.string();
        return StorylandModloaderInstallResult::Failed;
    }
    bool installed = false;
    std::vector<std::filesystem::path> entries;
    for (std::filesystem::directory_iterator it(stagedRoot, operation), end; !operation && it != end; it.increment(operation))
        entries.push_back(it->path());
    if (operation) { error = "Cannot enumerate staged Modloader: " + operation.message(); return StorylandModloaderInstallResult::Failed; }
    std::sort(entries.begin(), entries.end());
    for (const auto& entry : entries) {
        if (!findChild(destination, entry.filename().string()).empty()) continue;
        moved = moveWithoutReplacing(entry, destination / entry.filename(), error);
        if (moved == MoveResult::Failed) return StorylandModloaderInstallResult::Failed;
        installed = installed || moved == MoveResult::Moved;
    }
    return installed ? StorylandModloaderInstallResult::Installed : StorylandModloaderInstallResult::AlreadyPresent;
}
