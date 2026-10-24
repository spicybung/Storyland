#pragma once
#include <algorithm>
#include <array>
#include <cmath>

struct StorylandMeshBounds {
    std::array<double, 3> minimum{};
    std::array<double, 3> maximum{};
    bool valid = false;
};

template<class Vertices>
StorylandMeshBounds storylandMeshBounds(const Vertices& vertices) {
    StorylandMeshBounds result;
    for (const auto& vertex : vertices) {
        const std::array<double, 3> point{vertex.x, vertex.y, vertex.z};
        for (double value : point) if (!std::isfinite(value)) return {};
        if (!result.valid) {
            result.minimum = result.maximum = point;
            result.valid = true;
        } else for (size_t axis = 0; axis < 3; ++axis) {
            result.minimum[axis] = std::min(result.minimum[axis], point[axis]);
            result.maximum[axis] = std::max(result.maximum[axis], point[axis]);
        }
    }
    return result;
}

class StorylandViewFrustum {
public:
    void set(const double* projection, const double* modelView) {
        valid_ = false;
        std::array<double, 16> clip{};
        for (size_t column = 0; column < 4; ++column) for (size_t row = 0; row < 4; ++row)
            for (size_t inner = 0; inner < 4; ++inner) clip[column * 4 + row] += projection[inner * 4 + row] * modelView[column * 4 + inner];
        for (double value : clip) if (!std::isfinite(value)) return;
        for (size_t axis = 0; axis < 3; ++axis) for (size_t side = 0; side < 2; ++side) {
            auto& plane = planes_[axis * 2 + side];
            const double sign = side == 0 ? 1.0 : -1.0;
            for (size_t column = 0; column < 4; ++column) plane[column] = clip[column * 4 + 3] + sign * clip[column * 4 + axis];
            const double length = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
            if (!(length > 1.0e-20) || !std::isfinite(length)) return;
            for (double& value : plane) value /= length;
        }
        valid_ = true;
    }
    bool visible(const StorylandMeshBounds& bounds, const float* transform) const {
        if (!valid_ || !bounds.valid || !transform) return true;
        for (size_t i = 0; i < 16; ++i) if (!std::isfinite(transform[i])) return true;
        if (transform[3] != 0 || transform[7] != 0 || transform[11] != 0 || transform[15] != 1) return true;
        std::array<double, 3> center{}, extent{};
        for (size_t row = 0; row < 3; ++row) {
            center[row] = transform[12 + row];
            for (size_t column = 0; column < 3; ++column) {
                const double coefficient = transform[column * 4 + row];
                center[row] += coefficient * (bounds.minimum[column] + bounds.maximum[column]) * 0.5;
                extent[row] += std::abs(coefficient) * (bounds.maximum[column] - bounds.minimum[column]) * 0.5;
            }
            if (!std::isfinite(center[row]) || !std::isfinite(extent[row])) return true;
        }
        for (const auto& plane : planes_) {
            double distance = plane[3], radius = 0;
            for (size_t axis = 0; axis < 3; ++axis) {
                distance += plane[axis] * center[axis];
                radius += std::abs(plane[axis]) * extent[axis];
            }
            const double tolerance = 1.0e-6 * (1.0 + std::abs(distance) + radius);
            if (distance + radius < -tolerance) return false;
        }
        return true;
    }
private:
    std::array<std::array<double, 4>, 6> planes_{};
    bool valid_ = false;
};
