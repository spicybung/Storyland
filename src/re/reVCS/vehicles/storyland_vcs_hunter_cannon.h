#pragma once

#include <cstdint>

namespace StorylandVcsHunterCannon {

// Original CHeli control at 0x2E86E8 compares (last + 60) to the game clock.
inline bool playerShotReady(bool trigger, uint32_t now, uint32_t lastShot) {
    return trigger && uint32_t(lastShot + 60u) < now;
}

}
