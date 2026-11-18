#pragma once

#include "src/re/reVCS/modloader/storyland_vcs_modloader.h"

enum class StorylandModloaderInstallResult { Installed, AlreadyPresent, Unsupported, Failed };

std::filesystem::path storylandFindBundledVcsModloader(const std::filesystem::path& executablePath);

StorylandModloaderInstallResult storylandInstallVcsModloader(
    const std::filesystem::path& bundledModloader,
    const std::filesystem::path& gameRoot,
    StorylandModloaderTarget target,
    std::string& error);
