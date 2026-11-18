#pragma once

#include "src/leeds/leeds_archive.h"
#include <array>
#include <cstdint>
#include <vector>

struct StorylandCompiledScriptBatch {
    uint32_t materialId = UINT32_MAX;
    std::vector<std::array<float,9>> vertices;
};

struct StorylandCompiledScriptPart {
    bool transparent = false;
    StorylandMeshBounds bounds;
    std::vector<StorylandCompiledScriptBatch> batches;
};

inline std::vector<StorylandCompiledScriptPart> storylandBuildCompiledScriptBatches(
    const StorylandLcsDynamicGeometry& geometry) {
    std::vector<StorylandCompiledScriptPart> result;
    result.reserve(geometry.parts.size());
    for (const auto& part : geometry.parts) {
        StorylandCompiledScriptPart prepared;
        prepared.transparent = part.transparent;
        prepared.bounds = part.mesh.bounds;
        const auto& mesh = part.mesh;
        for (const auto& triangle : mesh.triangles) {
            if (triangle.a >= mesh.vertices.size() || triangle.b >= mesh.vertices.size() ||
                triangle.c >= mesh.vertices.size()) continue;
            if (prepared.batches.empty() ||
                prepared.batches.back().materialId != triangle.textureId ||
                prepared.batches.back().vertices.size() >= 12288) {
                prepared.batches.push_back({});
                prepared.batches.back().materialId = triangle.textureId;
                prepared.batches.back().vertices.reserve(1536);
            }
            auto& batch = prepared.batches.back();
            for (uint32_t index : {triangle.a,triangle.b,triangle.c}) {
                const auto& vertex = mesh.vertices[index];
                const bool prelight = vertex.hasPrelight;
                batch.vertices.push_back({vertex.u,vertex.v,
                    prelight ? vertex.red / 255.0f : 1.0f,
                    prelight ? vertex.green / 255.0f : 1.0f,
                    prelight ? vertex.blue / 255.0f : 1.0f,
                    1.0f,vertex.x,vertex.y,vertex.z});
            }
        }
        result.push_back(std::move(prepared));
    }
    return result;
}
