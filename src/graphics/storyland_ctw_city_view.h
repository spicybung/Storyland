#pragma once

#include "src/leeds/leeds_ctw_city.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace storyland_ctw {

struct CityViewCamera {
    std::array<float, 3> center{};
    float yaw = 35.0f;
    float pitch = 55.0f;
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    float radius = 1.0f;
    float depthRadius = 1.0f;

    void fit(const CityBounds& bounds, const CityBounds& sceneBounds) {
        if (!bounds.valid) return;
        float diagonalSquared = 0.0f;
        float sceneDiagonalSquared = 0.0f;
        for (size_t axis = 0; axis < 3; ++axis) {
            center[axis] = (bounds.minimum[axis] + bounds.maximum[axis]) * 0.5f;
            const float span = bounds.maximum[axis] - bounds.minimum[axis];
            diagonalSquared += span * span;
            if (sceneBounds.valid) {
                const float sceneSpan = sceneBounds.maximum[axis] - sceneBounds.minimum[axis];
                sceneDiagonalSquared += sceneSpan * sceneSpan;
            }
        }
        radius = std::max(0.01f, std::sqrt(diagonalSquared) * 0.55f);
        depthRadius = std::max(radius, std::sqrt(sceneDiagonalSquared) * 0.55f);
        zoom = 1.0f;
        panX = 0.0f;
        panY = 0.0f;
    }

    void panPixels(float horizontal, float vertical, int width, int height) {
        const float unitsPerPixel = 2.0f * radius /
            (std::max(0.01f, zoom) * float(std::max(1, std::min(width, height))));
        panX += horizontal * unitsPerPixel;
        panY -= vertical * unitsPerPixel;
    }
};

struct CityViewTransform {
    std::array<float, 16> modelView{};
    std::array<float, 16> projection{};
    float halfWidth = 1.0f;
    float halfHeight = 1.0f;
    float farClip = 8.0f;

    float eyeDepth(const CityBounds& bounds) const {
        const float x = (bounds.minimum[0] + bounds.maximum[0]) * 0.5f;
        const float y = (bounds.minimum[1] + bounds.maximum[1]) * 0.5f;
        const float z = (bounds.minimum[2] + bounds.maximum[2]) * 0.5f;
        return modelView[2] * x + modelView[6] * y + modelView[10] * z + modelView[14];
    }

    bool boundsVisible(const CityBounds& bounds) const {
        if (!bounds.valid) return true;
        std::array<float, 3> center{};
        std::array<float, 3> half{};
        for (size_t axis = 0; axis < 3; ++axis) {
            center[axis] = (bounds.minimum[axis] + bounds.maximum[axis]) * 0.5f;
            half[axis] = (bounds.maximum[axis] - bounds.minimum[axis]) * 0.5f;
        }
        const std::array<float, 3> minimum{-halfWidth, -halfHeight, -farClip};
        const std::array<float, 3> maximum{halfWidth, halfHeight, 0.0f};
        for (size_t axis = 0; axis < 3; ++axis) {
            const float projectedCenter = modelView[axis] * center[0] +
                modelView[axis + 4] * center[1] + modelView[axis + 8] * center[2] +
                modelView[axis + 12];
            const float projectedHalf = std::abs(modelView[axis]) * half[0] +
                std::abs(modelView[axis + 4]) * half[1] +
                std::abs(modelView[axis + 8]) * half[2];
            if (projectedCenter + projectedHalf < minimum[axis] ||
                projectedCenter - projectedHalf > maximum[axis]) return false;
        }
        return true;
    }
};

inline CityViewTransform makeCityView(const CityViewCamera& camera, int width, int height) {
    CityViewTransform result;
    constexpr float radians = 3.14159265358979323846f / 180.0f;
    const float cosineYaw = std::cos(camera.yaw * radians);
    const float sineYaw = std::sin(camera.yaw * radians);
    const float cosinePitch = std::cos((camera.pitch - 90.0f) * radians);
    const float sinePitch = std::sin((camera.pitch - 90.0f) * radians);
    auto& matrix = result.modelView;
    matrix[0] = cosineYaw;
    matrix[1] = cosinePitch * sineYaw;
    matrix[2] = sinePitch * sineYaw;
    matrix[4] = -sineYaw;
    matrix[5] = cosinePitch * cosineYaw;
    matrix[6] = sinePitch * cosineYaw;
    matrix[9] = -sinePitch;
    matrix[10] = cosinePitch;
    matrix[15] = 1.0f;
    for (size_t axis = 0; axis < 3; ++axis)
        matrix[axis + 12] = -(matrix[axis] * camera.center[0] +
            matrix[axis + 4] * camera.center[1] + matrix[axis + 8] * camera.center[2]);
    matrix[12] += camera.panX;
    matrix[13] += camera.panY;
    matrix[14] -= std::max(0.01f, camera.depthRadius) * 3.0f;

    const float aspect = float(std::max(1, width)) / float(std::max(1, height));
    const float extent = std::max(0.001f, camera.radius) / std::max(0.01f, camera.zoom);
    result.halfWidth = extent * std::max(1.0f, aspect);
    result.halfHeight = extent / std::min(1.0f, aspect);
    result.farClip = std::max(0.01f, camera.depthRadius) * 8.0f;
    result.projection[0] = 1.0f / result.halfWidth;
    result.projection[5] = 1.0f / result.halfHeight;
    result.projection[10] = -2.0f / result.farClip;
    result.projection[14] = -1.0f;
    result.projection[15] = 1.0f;
    return result;
}

enum class CityDrawPass { Opaque, Transparent };

struct CityDrawStats {
    size_t blocks = 0;
    size_t instances = 0;
    size_t drawCalls = 0;
    size_t vertices = 0;
};

// The host supplies OpenGL declarations and installs the view matrices. The
// material callback binds a part and rejects parts belonging to the other pass.
template<class BindMaterial>
CityDrawStats drawCityScene(const CityScene& scene, const CityViewTransform& view,
    BindMaterial bindMaterial) {
    CityDrawStats stats;
    std::vector<size_t> visible;
    visible.reserve(scene.instances.size());
    for (const auto& block : scene.blocks) {
        if (!view.boundsVisible(block.bounds)) continue;
        ++stats.blocks;
        const size_t first = std::min(size_t(block.firstInstance), scene.instances.size());
        const size_t end = first + std::min(size_t(block.instanceCount), scene.instances.size() - first);
        for (size_t index = first; index < end; ++index) {
            const auto& instance = scene.instances[index];
            if (instance.modelIndex < scene.models.size() && view.boundsVisible(instance.bounds))
                visible.push_back(index);
        }
    }
    stats.instances = visible.size();
    std::sort(visible.begin(), visible.end(), [&](size_t left, size_t right) {
        return view.eyeDepth(scene.instances[left].bounds) > view.eyeDepth(scene.instances[right].bounds);
    });

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
        GL_CURRENT_BIT | GL_TEXTURE_BIT);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    GLint previousMatrixMode = GL_MODELVIEW;
    glGetIntegerv(GL_MATRIX_MODE, &previousMatrixMode);
    glMatrixMode(GL_MODELVIEW);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    for (unsigned passIndex = 0; passIndex < 2; ++passIndex) {
        const CityDrawPass pass = passIndex == 0 ? CityDrawPass::Opaque : CityDrawPass::Transparent;
        if (pass == CityDrawPass::Opaque) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        } else {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        }
        for (size_t order = 0; order < visible.size(); ++order) {
            const size_t visibleIndex = pass == CityDrawPass::Opaque ? order : visible.size() - order - 1;
            const auto& instance = scene.instances[visible[visibleIndex]];
            const auto& geometry = scene.models[instance.modelIndex].geometry;
            if (geometry.vertices.empty()) continue;
            glPushMatrix();
            glMultMatrixf(instance.transform.data());
            glVertexPointer(3, GL_FLOAT, GLsizei(sizeof(PakVertex)), &geometry.vertices.front().x);
            glTexCoordPointer(2, GL_FLOAT, GLsizei(sizeof(PakVertex)), &geometry.vertices.front().u);
            glNormalPointer(GL_FLOAT, GLsizei(sizeof(PakVertex)), &geometry.vertices.front().nx);
            for (const auto& part : geometry.parts) {
                if (part.firstVertex > geometry.vertices.size() ||
                    part.vertexCount > geometry.vertices.size() - part.firstVertex ||
                    part.vertexCount < 3 ||
                    part.vertexCount > uint32_t(std::numeric_limits<GLsizei>::max()) ||
                    part.firstVertex > uint32_t(std::numeric_limits<GLint>::max())) continue;
                if (!bindMaterial(part, pass)) continue;
                glDrawArrays(GL_TRIANGLE_STRIP, GLint(part.firstVertex), GLsizei(part.vertexCount));
                ++stats.drawCalls;
                stats.vertices += part.vertexCount;
            }
            glPopMatrix();
        }
    }
    glMatrixMode(GLenum(previousMatrixMode));
    glPopClientAttrib();
    glPopAttrib();
    return stats;
}

}
