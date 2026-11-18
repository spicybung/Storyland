#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

struct StorylandVcsRemoteCameraFrame {
    std::array<float,3> eye{}, forward{}, up{};
    float fieldOfView = 60;
};

inline bool storylandReadVcsRemoteCameraSeat(const std::vector<uint8_t>& bytes,
    int32_t model,std::array<float,3>& seat) {
    const auto word = [&](size_t offset) -> uint32_t {
        if (offset > bytes.size() || bytes.size() - offset < 4) return 0;
        return uint32_t(bytes[offset]) | uint32_t(bytes[offset+1]) << 8 |
            uint32_t(bytes[offset+2]) << 16 | uint32_t(bytes[offset+3]) << 24;
    };
    if (model < 0 || uint32_t(model) >= word(0x38)) return false;
    const uint32_t table = word(0x3C);
    if (!table || uint64_t(table) + uint64_t(model) * 4 + 4 > bytes.size()) return false;
    const uint32_t info = word(size_t(table) + size_t(model) * 4);
    if (!info || uint64_t(info) + 0xAC > bytes.size() || bytes[size_t(info)+0x10] != 6 ||
        word(size_t(info)+0x54) != 0) return false;
    std::array<float,3> position{};
    for (size_t axis = 0; axis < position.size(); ++axis) {
        const uint32_t bits = word(size_t(info) + 0xA0 + axis * 4);
        std::memcpy(&position[axis],&bits,sizeof(bits));
        if (!std::isfinite(position[axis])) return false;
    }
    seat = position;
    return true;
}

inline bool storylandVcsAutomobileRemoteCamera(const std::array<float,16>& matrix,
    const std::array<float,3>& seat,bool reset,float& upsideDownOffset,
    StorylandVcsRemoteCameraFrame& result) {
    for (float value : matrix) if (!std::isfinite(value)) return false;
    for (float value : seat) if (!std::isfinite(value)) return false;
    if (!std::isfinite(upsideDownOffset)) return false;

    StorylandVcsRemoteCameraFrame frame;
    const float forwardOffset = seat[1] + .08f;
    const float verticalOffset = seat[2] + .62f;
    float offset = reset ? 0.0f : upsideDownOffset;
    for (size_t axis = 0; axis < frame.eye.size(); ++axis) {
        frame.eye[axis] = matrix[4+axis] * forwardOffset + matrix[8+axis] * verticalOffset;
        frame.eye[axis] += matrix[12+axis];
        frame.forward[axis] = matrix[4+axis];
        frame.up[axis] = matrix[8+axis];
    }
    // Original295E64 calls IsUpsideDown; both accumulator tests are strict less-than.
    if (matrix[10] <= -.9f) offset = offset < .5f ? offset + .03f : .5f;
    else offset = offset < 0.0f ? offset - .03f : 0.0f;
    frame.eye[2] += offset;

    const auto normalize = [](std::array<float,3>& vector) {
        const float squared = vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2];
        if (!std::isfinite(squared)) return false;
        if (squared <= 0.0f) vector[0] = 1.0f;
        else {
            const float inverse = 1.0f / std::sqrt(squared);
            for (float& value : vector) value *= inverse;
        }
        return true;
    };
    const auto cross = [](const std::array<float,3>& first,const std::array<float,3>& second) {
        return std::array<float,3>{first[1]*second[2] - first[2]*second[1],
            -first[0]*second[2] + first[2]*second[0],
            first[0]*second[1] - first[1]*second[0]};
    };
    if (!normalize(frame.forward) || !normalize(frame.up)) return false;
    auto right = cross(frame.forward,frame.up);
    if (!normalize(right)) return false;
    frame.up = cross(right,frame.forward);
    if (!normalize(frame.up)) return false;
    for (float value : frame.eye) if (!std::isfinite(value)) return false;
    result = frame;
    upsideDownOffset = offset;
    return true;
}
