#pragma once
#include "src/leeds/leeds_model.h"

#include <cstdint>
#include <array>
#include <vector>

enum class StorylandGlSurfaceMode {
    Textured,
    Solid,
    Wireframe
};

class StorylandGraphicsSynthesizer {
public:
    void BeginFrame(int width, int height) const;
    void ResetSurfaceState() const;
    void ConfigureWorldFog(bool enabled,const std::array<float,3>& color,float start,float end) const;
    void DrawGrid(float extent, float step) const;
    void DrawBounds(
        float minX, float minY, float minZ,
        float maxX, float maxY, float maxZ) const;
    void DrawSimpleSurface(
        const std::vector<StorylandModelPoint>& points,
        const std::vector<StorylandModelTriangle>& triangles,
        const std::vector<StorylandModelTexcoord>& texcoords,
        StorylandGlSurfaceMode mode,
        unsigned int textureId,
        bool flipV) const;
};
