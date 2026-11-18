#pragma once

#include "src/leeds/leeds_archive.h"
#include <filesystem>
#include <fstream>
#include <ostream>

inline void storylandWriteSceneryReport(std::ostream& output,
    const StorylandNativeSceneryReport& report,const std::filesystem::path& source) {
    output << "Storyland vegetation and objects\n";
    output << "Source: " << source.u8string() << '\n';
    output << report.summary() << '\n';
    for(const auto& model:report.skippedModels)output << "Skipped model: " << model << '\n';
    for(const auto& texture:report.missingTextures)output << "Missing texture: " << texture << '\n';
}

inline bool storylandSaveSceneryReport(const StorylandNativeSceneryReport& report,
    const std::filesystem::path& source,const std::filesystem::path& destination) {
    std::ofstream output(destination,std::ios::binary|std::ios::trunc);
    if(!output)return false;
    storylandWriteSceneryReport(output,report,source);
    output.flush();
    return bool(output);
}
