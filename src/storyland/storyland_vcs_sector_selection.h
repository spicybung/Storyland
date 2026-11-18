#pragma once

#include <cmath>
#include <cstdint>

struct StorylandVcsSectorCell {
    int32_t column = 0;
    int32_t row = 0;
    bool valid = false;
};

inline StorylandVcsSectorCell storylandVcsCameraSector(float x, float y) {
    StorylandVcsSectorCell result;
    if (!std::isfinite(x) || !std::isfinite(y)) return result;
    // SLUS_215.90 0x23C188 uses separate float32 operations and truncation.
    // Preserve the authored reciprocals 0x3C175A75 and 0x3C03126F at cell edges.
    volatile float shiftedY = y + 2000.0f;
    volatile float rowPosition = shiftedY * 0x1.2eb4eap-7f;
    if (rowPosition <= -2147483648.0f || rowPosition >= 2147483648.0f) return result;
    result.row = int32_t(rowPosition);
    volatile float shiftedX = x + 2400.0f;
    volatile float scaledX = shiftedX * 0x1.0624dep-7f;
    volatile float columnPosition = scaledX + float(result.row & 1) * 0.5f;
    if (columnPosition <= -2147483648.0f || columnPosition >= 2147483648.0f) return result;
    result.column = int32_t(columnPosition);
    result.valid = result.row >= 0 && result.row < 36 && result.column >= 0;
    return result;
}
