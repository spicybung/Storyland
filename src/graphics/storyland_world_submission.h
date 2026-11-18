#pragma once

#include "src/leeds/leeds_archive.h"
#include "src/leeds/leeds_native_uv_animation.h"
#include "src/graphics/storyland_mesh_effects.h"
#include "src/graphics/storyland_world_index_batches.h"
#include <array>
#include <vector>

enum class StorylandWorldColorSource { Constant, VertexRgb, VertexRgba, FloatRgba, Ps2Rgba };

inline std::array<float, 2> storylandWorldUvOffset(
    const StorylandDirectTextureResource* texture, double seconds) {
    if (!texture || texture->pspRaster || texture->legacyRaw4bpp) return {};
    // Only the full VCS raster has this word. Compact LCS rasters deliberately
    // retain formatFlags == 0; their packed size/depth bits are not animation.
    return StorylandNativeTextureUvAnimation::fromVcsRaster(
        uint32_t(texture->formatFlags) << 16u).offset(seconds);
}

// Scratch data for one draw pass. Shared corners retain their authored indices.
struct StorylandWorldDrawVertices {
    const StorylandWorldMeshVertex* vertices = nullptr;
    size_t count = 0;
    bool anyPrelight = false, allPrelit = false;
    bool anyPs2Color = false, allPs2Color = false;
    std::vector<StorylandWorldMeshVertex> displaced;
    std::vector<std::array<float,4>> colors;

    void prepare(const StorylandWorldMesh& mesh, const StorylandMeshEffects* effects = nullptr,
        float seconds = 0, const float* placement = nullptr) {
        count = mesh.vertices.size();
        vertices = count ? mesh.vertices.data() : nullptr;
        anyPrelight = false;
        allPrelit = count != 0;
        anyPs2Color = false;
        allPs2Color = count != 0;
        for (const auto& vertex : mesh.vertices) {
            anyPrelight |= vertex.hasPrelight;
            allPrelit &= vertex.hasPrelight;
            anyPs2Color |= vertex.hasPs2Color;
            allPs2Color &= vertex.hasPs2Color;
        }
        displaced.clear();
        if (effects && effects->spatial(StorylandMeshCategory::World)) {
            const auto bounds = storylandMeshEffectBounds(mesh.bounds);
            displaced.reserve(count);
            for (const auto& vertex : mesh.vertices)
                displaced.push_back(storylandMeshEffectVertex(*effects, StorylandMeshCategory::World,
                    bounds, seconds, mesh.resourceIndex, vertex, placement));
            vertices = displaced.data();
        }
    }

    StorylandWorldColorSource colorSource(const std::array<float,4>& fallback,
        bool prelight, bool nativeAlpha, bool ps2 = false) {
        const bool anyPs2=ps2&&prelight&&anyPs2Color;
        if(anyPs2&&allPs2Color)
            return StorylandWorldColorSource::Ps2Rgba;
        if (prelight && allPrelit && !anyPs2)
            return nativeAlpha ? StorylandWorldColorSource::VertexRgba : StorylandWorldColorSource::VertexRgb;
        if (!nativeAlpha && (!prelight || !anyPrelight) && !anyPs2) return StorylandWorldColorSource::Constant;
        colors.resize(count);
        for (size_t index = 0; index < count; ++index) {
            const auto& vertex = vertices[index];
            auto& color = colors[index];
            color = prelight && vertex.hasPrelight
                ? std::array<float,4>{vertex.red / 255.0f, vertex.green / 255.0f, vertex.blue / 255.0f, 1.0f}
                : fallback;
            if (nativeAlpha) color[3] = vertex.alpha / 255.0f;
            if(ps2&&prelight&&vertex.hasPs2Color)
                for(size_t channel=0;channel<4;++channel)color[channel]=vertex.ps2Color[channel]/255.0f;
        }
        return StorylandWorldColorSource::FloatRgba;
    }
};

inline bool storylandWorldTriangleValid(const StorylandWorldMeshTriangle& triangle, size_t vertices) {
    return triangle.a < vertices && triangle.b < vertices && triangle.c < vertices;
}

inline void storylandAppendWorldTriangle(std::vector<uint32_t>& indices,
    const StorylandWorldMeshTriangle& triangle) {
    indices.push_back(triangle.a);
    indices.push_back(triangle.b);
    indices.push_back(triangle.c);
}

inline float storylandWorldShadowVertexAlpha(const StorylandWorldMeshVertex& vertex) {
    if (vertex.hasPs2Color) return std::min(1.0f, vertex.ps2Color[3] / 128.0f);
    return vertex.hasPrelight ? vertex.alpha / 255.0f : 1.0f;
}

inline bool storylandWorldCastsShadow(const StorylandWorldPlacement& placement) {
    return placement.passName != "LIGHTS" && placement.passName != "NOZWRITE";
}
