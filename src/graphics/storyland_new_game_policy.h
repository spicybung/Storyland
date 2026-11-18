#pragma once
#include <functional>

inline bool storylandHasAnyGameSave(const std::function<bool(int)>& slotExists) {
    if (!slotExists) return false;
    for (int slot = 1; slot <= 8; ++slot) {
        if (slotExists(slot)) return true;
    }
    return false;
}

inline bool storylandShouldConfirmNewGame(bool storylineStarted, bool savesFound) {
    return storylineStarted || savesFound;
}
