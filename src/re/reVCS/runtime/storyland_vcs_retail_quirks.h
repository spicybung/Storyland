#pragma once
#include <cstdint>

namespace StorylandVcsRetailQuirks {

// The names appear in separate SLUS_215.90 string groups. The bit mapping
// below assumes the names are ordered by bit; it needs instruction-level proof.
constexpr uint32_t ModelNoExhaust = 0x00001000u;
constexpr uint32_t ModelDoubleExhaust = 0x00002000u;
constexpr uint32_t HandlingNoHandbrake = 0x00000010u;
constexpr uint32_t HandlingRearWheelSteer = 0x00000020u;
constexpr uint32_t HandlingHandbrakeRearSteer = 0x00000040u;
constexpr uint32_t ExtraNoSkidmarks = 0x00000002u;
constexpr uint32_t SoundStartsRadioOff = 0x00000010u;
constexpr uint32_t SoundStartsRadioRock = 0x00000020u;
constexpr uint32_t KnownSoundFlags = 0x0000003fu;

inline bool exhaustEnabled(bool vcs, uint32_t modelFlags) {
    return !vcs || (modelFlags & ModelNoExhaust) == 0;
}

inline bool doubleExhaust(bool vcs, uint32_t modelFlags) {
    return vcs && (modelFlags & ModelDoubleExhaust) != 0 && exhaustEnabled(vcs, modelFlags);
}

inline bool skidmarksEnabled(bool vcs, bool hasExtraFlags, uint32_t extraFlags) {
    return !vcs || !hasExtraFlags || (extraFlags & ExtraNoSkidmarks) == 0;
}

inline bool automobileTyreParticlesEnabled(bool vcs, bool twoWheels,
    bool hasExtraFlags, uint32_t extraFlags) {
    return !vcs || twoWheels || !hasExtraFlags ||
        (extraFlags & ExtraNoSkidmarks) == 0;
}

inline bool handbrakeEnabled(bool vcs, uint32_t handlingFlags, bool requested) {
    return requested && (!vcs || (handlingFlags & HandlingNoHandbrake) == 0);
}

inline bool playerVehicleHandbrake(bool vcs, uint32_t handlingFlags,
    bool playerRequested, bool scriptRequested) {
    return scriptRequested || handbrakeEnabled(vcs, handlingFlags, playerRequested);
}

inline bool rearSteeringEnabled(bool vcs, uint32_t handlingFlags, bool handbrake) {
    return vcs && ((handlingFlags & HandlingRearWheelSteer) != 0 ||
        (handbrake && (handlingFlags & HandlingHandbrakeRearSteer) != 0));
}

inline float wheelSteerDegrees(bool vcs, uint32_t handlingFlags, bool front,
    float steering, bool handbrake) {
    if (rearSteeringEnabled(vcs, handlingFlags, handbrake))
        return front ? 0.0f : -steering;
    return front ? steering : 0.0f;
}

inline int initialRadioStation(bool vcs, bool hasSoundFlags, uint32_t soundFlags,
    int fallback, int rockStation, int offStation) {
    if (!vcs || !hasSoundFlags) return fallback;
    if ((soundFlags & SoundStartsRadioOff) != 0) return offStation;
    if ((soundFlags & SoundStartsRadioRock) != 0 && rockStation >= 0) return rockStation;
    return fallback;
}

}
