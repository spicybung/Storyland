#pragma once
#include "src/leeds/leeds_model.h"
#include <memory>
// Render-only Loop refinement. Original geometry, edits and collision stay intact.
struct StorylandSubdivWeight { uint32_t source; float weight; };
struct StorylandSubdivMesh {
    std::vector<std::vector<StorylandSubdivWeight>> stencils;
    // UV seams can split output vertices but still share a smooth normal.
    std::vector<uint32_t> normalGroups;
    size_t sourceCount=0;
    std::vector<StorylandModelPoint> points;
    std::vector<StorylandModelTriangle> triangles;
    std::vector<StorylandModelTexcoord> uv;
    std::vector<StorylandModelPrelight> colours;
};
std::shared_ptr<const StorylandSubdivMesh> storylandAeroSubdivide(
    const std::vector<StorylandModelPoint>& points,
    const std::vector<StorylandModelTriangle>& triangles,
    const std::vector<StorylandModelTexcoord>& uv,
    const std::vector<StorylandModelPrelight>& colours);
void storylandClearSubdivisionCache();

// Evaluate cached subdivision on the already skinned control mesh. No per-frame topology rebuild.
bool storylandPoseSubdivision(const StorylandSubdivMesh& mesh,
    const std::vector<StorylandModelPoint>& posed,std::vector<StorylandModelPoint>& output);

// Poll render-thread results; only one bounded world refinement runs at a time.
std::shared_ptr<const StorylandSubdivMesh> storylandAeroSubdivideWorld(
    std::vector<StorylandModelPoint> points,
    std::vector<StorylandModelTriangle> triangles,
    std::vector<StorylandModelTexcoord> uv,
    std::vector<StorylandModelPrelight> colours);
