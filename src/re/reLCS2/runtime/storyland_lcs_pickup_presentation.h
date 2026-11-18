#pragma once
#include "src/storyland/storyland_pickup_visual.h"
#include <limits>

struct StorylandLcsPickupCaption {
    std::string text;
    std::string textKey;
    float x = 0;
    float y = 0;
    float z = 0;
    uint8_t alpha = 0;
    std::array<uint8_t,3> color{255,100,100};

    bool visible() const { return alpha != 0 && (!text.empty() || !textKey.empty()); }
};

inline float storylandLcsPickupHeading(uint32_t milliseconds, int32_t pickupType,
    const std::string& modelName) {
    const std::string name = storylandPickupVisualModelName(modelName);
    const uint32_t period = pickupType == 6 || name == "package1" ? 4096 :
        pickupType == 8 || name == "money" ? 1024 : 2048;
    return float(milliseconds & (period - 1)) * (360.0f / float(period));
}

inline bool storylandLcsPickupVisible(const std::string& modelName, bool onMission, bool frenzyActive) {
    return storylandPickupVisualModelName(modelName) != "killfrenzy" || (!onMission && !frenzyActive);
}

template<class Pickup>
StorylandLcsPickupCaption storylandLcsPickupCaption(const Pickup& pickup,
    const std::string& modelName, float cameraX, float cameraY) {
    StorylandLcsPickupCaption caption;
    if (pickup.removed || pickup.unavailable || pickup.type == 0) return caption;
    const float distance = std::hypot(pickup.x - cameraX, pickup.y - cameraY);
    if (!std::isfinite(distance) || distance >= 14.0f) return caption;
    const std::string name = storylandPickupVisualModelName(modelName);
    if (pickup.type == 16 && std::isfinite(pickup.revenue) && pickup.revenue >= 10.0f) {
        const uint32_t value = uint32_t(std::min(double(pickup.revenue), double(UINT32_MAX)));
        const uint16_t money = uint16_t(value);
        if (money) caption.text = "$" + std::to_string(money);
    } else if (name == "bonus" || name == "clothesp") {
        const uint8_t quantity = uint8_t(pickup.ammunition);
        if (quantity >= 1 && quantity <= 13) caption.textKey = "OUTFT" + std::to_string(quantity);
    }
    if (caption.text.empty() && caption.textKey.empty()) return caption;
    caption.x = pickup.x;
    caption.y = pickup.y;
    caption.z = pickup.z + 0.7f;
    caption.alpha = uint8_t(std::clamp((1.0f - distance / 14.0f) * 128.0f, 0.0f, 128.0f));
    return caption;
}
