#pragma once
#include "storyland_model.h"
#include "storyland_anim.h"

bool storylandSampleMobileAnimation(
    const StorylandModelFile& model,
    const StorylandAnimFile& animation,
    float seconds,
    bool lockRootMotion,
    const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,
    std::vector<StorylandModelPoint>& animatedBones,
    std::string& errorMessage);

bool storylandSampleModelAnimation(
    const StorylandModelFile& model,
    const StorylandAnimFile& animation,
    float seconds,
    bool lockRootMotion,
    const std::vector<StorylandModelPoint>& sourcePoints,
    std::vector<StorylandModelPoint>& animatedPoints,
    std::vector<StorylandModelPoint>& animatedBones,
    std::string& errorMessage);
