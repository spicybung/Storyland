#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

struct StorylandLcsPickupDetails {
    uint16_t revenueRate = 0;
    uint32_t updatedAt = 0;
    std::string textKey;
    std::string missionText;
    bool helpShown = false;

    bool valid() const {
        if (textKey.size() > 8 || missionText.size() > 8)
            return false;
        for (unsigned char value : textKey)
            if (value < 32 || value > 126) return false;
        for (unsigned char value : missionText)
            if (value < 32 || value > 126) return false;
        return true;
    }

    void clear() {
        *this = {};
    }

    template<class Archive> void saveState(Archive& archive) {
        archive(revenueRate, updatedAt, textKey, missionText, helpShown);
    }
};

inline int32_t storylandLcsPickupDefaultAmmo(int32_t weapon, int32_t pickupType) {
    constexpr std::array<uint16_t,37> full{
        0,1,1,1,1,1,1,1,1,1,1,1,8,8,8,8,8,68,24,32,28,20,200,120,120,120,120,120,
        40,28,8,300,200,1000,1,400,36
    };
    constexpr std::array<uint16_t,37> street{
        0,1,1,1,1,1,1,1,1,1,1,1,4,4,4,4,4,34,12,16,14,10,100,60,60,60,60,60,
        20,14,4,150,100,500,1,400,36
    };
    if (weapon < 0 || size_t(weapon) >= full.size()) return 0;
    return (pickupType == 2 || pickupType == 15 ? street : full)[size_t(weapon)];
}
