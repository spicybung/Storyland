#pragma once

#include <array>
#include <cmath>
#include <cstddef>

struct StorylandScriptShadowVertex {
    std::array<float, 3> position{};
    std::array<float, 2> uv{};
    bool supported = false;
};

template <typename GroundHeight, typename DrawTile>
void storylandProjectScriptShadow(const std::array<float, 3>& center, float radius,
    GroundHeight groundHeight, DrawTile drawTile) {
    if (!std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(center[0]) || !std::isfinite(center[1]) || !std::isfinite(center[2])) return;

    std::array<StorylandScriptShadowVertex, 25> vertices{};
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            auto& vertex = vertices[size_t(y * 5 + x)];
            vertex.position = {center[0] + (float(x) * 0.5f - 1.0f) * radius,
                center[1] + (float(y) * 0.5f - 1.0f) * radius, center[2] + 2.0f};
            vertex.uv = {float(x) * 0.25f, float(y) * 0.25f};
            float height = 0.0f;
            vertex.supported = groundHeight(vertex.position, height) && std::isfinite(height) &&
                height >= center[2] - 8.0f;
            if (vertex.supported) vertex.position[2] = height + 0.025f;
        }
    }
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const std::array<StorylandScriptShadowVertex, 4> tile{
                vertices[size_t(y * 5 + x)], vertices[size_t(y * 5 + x + 1)],
                vertices[size_t((y + 1) * 5 + x + 1)], vertices[size_t((y + 1) * 5 + x)]};
            if (tile[0].supported && tile[1].supported && tile[2].supported && tile[3].supported)
                drawTile(tile);
        }
    }
}
