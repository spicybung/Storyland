#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "src/leeds/leeds_model.h"

enum class StorylandDffTarget {
    SanAndreas,
    MobileLcs
};

struct StorylandDffExportFrame {
    std::string name;
    int32_t parent = -1;
    uint32_t boneId = 0xFFFFFFFFu;
    float matrix[12] = {1,0,0, 0,1,0, 0,0,1, 0,0,0};
};

struct StorylandDffExportFace {
    uint32_t indices[3] = {};
    uint32_t material = 0;
};

struct StorylandDffExportMesh {
    std::string name;
    uint32_t frameIndex = 0;
    std::vector<StorylandModelPoint> vertices;
    std::vector<StorylandModelTexcoord> texcoords;
    std::vector<StorylandModelPrelight> colors;
    std::vector<StorylandModelSkinWeights> weights;
    std::vector<StorylandDffExportFace> faces;
    std::vector<std::string> textureNames;
    std::vector<uint32_t> materialColors;
    std::vector<std::string> environmentNames;
    std::vector<float> environmentStrengths;
    std::vector<StorylandUVMaterialAnimation> rwUVAnimations;
    std::vector<StorylandRslUV::Material> rslUVAnimations;
    bool skinned = false;
};

struct StorylandDffExportScene {
    std::vector<StorylandDffExportFrame> frames;
    std::vector<StorylandDffExportMesh> meshes;
    bool hasSkeleton = false;
    uint32_t skeletonRoot = 0;
    std::vector<uint32_t> skinToFrame;
};

bool storylandBuildDffExportScene(
    const StorylandModelFile& source,
    StorylandDffExportScene& scene,
    std::string& error);

bool storylandWriteDff(
    const StorylandDffExportScene& scene,
    StorylandDffTarget target,
    std::vector<uint8_t>& output,
    std::string& error);

bool storylandConvertModelToDff(
    const StorylandModelFile& source,
    StorylandDffTarget target,
    std::vector<uint8_t>& output,
    std::string& error);
