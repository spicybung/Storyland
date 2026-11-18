#pragma once

#include "src/storyland/storyland_native_random.h"
#include "src/storyland/storyland_service_roles.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

inline size_t storylandChooseVcsAmbientCivilian(
    const std::array<uint32_t, 16>& group,
    const std::vector<StorylandPedStats>& models,
    const std::vector<StorylandServiceRole>& roles,
    const std::vector<uint32_t>& nearbyModels,
    uint32_t randomValue) {
    std::vector<size_t> eligible;
    size_t fewestNearby = SIZE_MAX;
    for (uint32_t modelId : group) {
        size_t nearbyCount = 0;
        for (uint32_t nearby : nearbyModels)
            if (nearby == modelId) ++nearbyCount;
        if (nearbyCount >= 2) continue;

        for (size_t index = 0; index < models.size() && index < roles.size(); ++index) {
            const auto& model = models[index];
            if (!model.valid || model.modelId != modelId ||
                roles[index] != StorylandServiceRole::Civilian) continue;
            if (model.pedType != 4 && model.pedType != 5 &&
                model.pedType != 18 && model.pedType != 20) continue;
            if (nearbyCount < fewestNearby) {
                fewestNearby = nearbyCount;
                eligible.clear();
            }
            if (nearbyCount == fewestNearby) eligible.push_back(index);
            break;
        }
    }
    if (eligible.empty()) return SIZE_MAX;
    return eligible[storylandNativeRandomRangeFromValue(randomValue, uint32_t(eligible.size()))];
}
