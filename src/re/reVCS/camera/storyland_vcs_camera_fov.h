#pragma once

// 0x294E60: a nonzero CCam +0x80 overrides the camera's current FOV.
inline float storylandVcsCameraFieldOfView(float calculated, float overrideValue) {
    return overrideValue != 0.0f ? overrideValue : calculated;
}
