#pragma once

#include <array>
#include <cmath>
#include <cstdint>

struct StorylandLcsSectorCell {
    int32_t column = 0;
    int32_t row = 0;
    bool valid = false;
};

inline StorylandLcsSectorCell storylandLcsCameraSector(float x, float y) {
    StorylandLcsSectorCell result;
    if (!std::isfinite(x) || !std::isfinite(y)) return result;
    // SLUS_214.23 0x1FDF6C: separate COP1 float32 operations, then truncation.
    // Its row reciprocal is 0x3C3D3113, one ULP above float(1 / 86.6).
    volatile float shiftedY = y + 2000.0f;
    volatile float rowPosition = shiftedY * 0x1.7a6226p-7f;
    if (rowPosition <= -2147483648.0f || rowPosition >= 2147483648.0f) return result;
    result.row = int32_t(rowPosition);
    volatile float shiftedX = x + 2000.0f;
    volatile float scaledX = shiftedX * 0x1.47ae14p-7f;
    volatile float columnPosition = scaledX + float(result.row & 1) * 0.5f;
    if (columnPosition <= -2147483648.0f || columnPosition >= 2147483648.0f) return result;
    result.column = int32_t(columnPosition);
    result.valid = result.row >= 0 && result.row < 46 && result.column >= 0;
    return result;
}

inline bool storylandLcsWorldConditionEnabled(uint8_t selector, uint8_t state,
    uint8_t hour, const std::array<uint32_t, 256>& groups, bool resourceRequest = false) {
    if (selector == 0xFFu) return resourceRequest;
    if (!(selector & 0x80u)) return groups[selector] == state;
    const uint8_t off = selector & 0x7Fu;
    return off >= state ? hour >= state && hour < off : hour >= state || hour < off;
}
