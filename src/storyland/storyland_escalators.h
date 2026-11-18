#pragma once
#include "src/leeds/leeds_native_world.h"
#include "src/leeds/leeds_native_layout.h"
#include "src/storyland/storyland_script_object.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

struct StorylandEscalatorDefinition {
    uint32_t nativeOffset = 0;
    std::array<StorylandNativePoint, 4> path{};
    std::array<float, 16> matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    StorylandNativePoint center;
    float radius = 0, lowerFraction = 0, upperFraction = 1;
    uint32_t stepCount = 0;
    bool forward = false;
};

struct StorylandEscalatorStep {
    size_t worldIndex = SIZE_MAX;
    std::array<float, 16> previousMatrix{};
    StorylandNativePoint velocity;
    float phase = 0;
    bool continuous = false;
};

struct StorylandEscalator {
    StorylandEscalatorDefinition definition;
    std::vector<StorylandEscalatorStep> steps;
    bool active = false;
};

struct StorylandEscalatorState {
    // This is a host-generated identity namespace, not a recovered CPool header.
    static constexpr uint32_t GeneratedPoolHeader = 0x30;
    uint32_t stepModel = UINT32_MAX, milliseconds = 0;
    std::vector<StorylandEscalator> escalators;
    bool attached = false, initialized = false;
};

// Disable step collision along with the steps themselves when leaving the scene.
inline void storylandDisableEscalators(StorylandEscalatorState& state, StorylandNativeWorld& world) {
    bool changed = false;
    for (auto& instance : world.instances) {
        if (instance.poolHeader != StorylandEscalatorState::GeneratedPoolHeader) continue;
        instance.modelId = UINT32_MAX;
        changed = true;
    }
    for (auto& escalator : state.escalators) escalator.active = false;
    state.initialized = false;
    if (changed) storylandPrepareNativeCollisionIndex(world);
}

inline bool storylandDecodeEscalators(const std::vector<uint8_t>& bytes, bool vcs,
    StorylandEscalatorState& state, std::string& error) {
    state = {};
    error.clear();
    // VCS loader 332580 reads ResourceImage+CC/D0; the DTZ prefix is 20 bytes.
    // No matching LCS registry has been established, so never interpret its EC field.
    if (!vcs) return true;
    const auto contains = [&](size_t at, size_t length) {
        return at <= bytes.size() && length <= bytes.size() - at;
    };
    const auto word = [&](size_t at) {
        return uint32_t(bytes[at]) | uint32_t(bytes[at + 1]) << 8 |
            uint32_t(bytes[at + 2]) << 16 | uint32_t(bytes[at + 3]) << 24;
    };
    const auto real = [&](size_t at) {
        const uint32_t bits = word(at);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    };
    if (!contains(0xEC, 8) || !contains(0x5C, 4)) {
        error = "Truncated VCS escalator registry header.";
        return false;
    }
    const uint32_t table = word(0xEC), count = word(0xF0);
    if (!count) return true;
    StorylandVcsPspStreamingLayout streaming;
    const bool psp = storylandVcsPspStreamingLayout(bytes, streaming);
    const size_t stride = psp ? 0x140u : 0x150u;
    if (count > 1024 || table < 0x20 || !contains(table, size_t(count) * stride)) {
        error = "Invalid VCS escalator registry range.";
        return false;
    }
    const uint32_t specialModels = word(0x5C);
    if (specialModels < 0x20 || !contains(specialModels, 0x114)) {
        error = "Invalid VCS escalator step-model registry.";
        return false;
    }
    state.stepModel = uint16_t(bytes[specialModels + 0x112]) |
        uint16_t(bytes[specialModels + 0x113]) << 8;
    for (uint32_t index = 0; index < count; ++index) {
        StorylandEscalator escalator;
        auto& definition = escalator.definition;
        const size_t row = size_t(table) + size_t(index) * stride;
        definition.nativeOffset = uint32_t(row);
        for (size_t point = 0; point < 4; ++point) {
            definition.path[point] = {real(row + point * 16), real(row + point * 16 + 4),
                real(row + point * 16 + 8)};
        }
        for (size_t axis = 0; axis < 3; ++axis)
            for (size_t component = 0; component < 3; ++component)
                definition.matrix[axis * 4 + component] = real(row + 0x40 + axis * 16 + component * 4);
        definition.stepCount = word(row + (psp ? 0x94u : 0x98u));
        definition.forward = psp ? bytes[row + 0x91u] != 0 : word(row + 0x94u) != 0;
        definition.lowerFraction = real(row + (psp ? 0x98u : 0x9Cu));
        definition.upperFraction = real(row + (psp ? 0x9Cu : 0xA0u));
        const size_t center = row + (psp ? 0xA0u : 0xB0u);
        definition.center = {real(center), real(center + 4u), real(center + 8u)};
        definition.radius = real(row + (psp ? 0xB0u : 0xC0u));
        bool finite = std::isfinite(definition.radius);
        for (const auto& point : definition.path)
            finite = finite && std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
        for (float value : definition.matrix) finite = finite && std::isfinite(value);
        finite = finite && std::isfinite(definition.center.x) && std::isfinite(definition.center.y) &&
            std::isfinite(definition.center.z);
        if (!finite || definition.radius < 0 || definition.stepCount == 0 || definition.stepCount > 32 ||
            !(definition.lowerFraction > 0 && definition.lowerFraction < definition.upperFraction &&
                definition.upperFraction < 1)) {
            state = {};
            error = "Invalid VCS escalator path or step count.";
            return false;
        }
        escalator.steps.resize(definition.stepCount);
        state.escalators.push_back(std::move(escalator));
    }
    return true;
}

inline float storylandEscalatorPhase(const StorylandEscalatorDefinition& definition,
    uint32_t milliseconds, uint32_t step) {
    float phase = float(milliseconds & 0x3FFF) / 16384.0f + float(step) / float(definition.stepCount);
    if (phase > 1) phase -= 1;
    return definition.forward ? phase : 1 - phase;
}

inline StorylandNativePoint storylandEscalatorPosition(const StorylandEscalatorDefinition& definition,
    float phase) {
    size_t segment;
    float fraction;
    if (phase < definition.lowerFraction) {
        segment = 0;
        fraction = phase / definition.lowerFraction;
    } else if (phase < definition.upperFraction) {
        segment = 1;
        fraction = (phase - definition.lowerFraction) /
            (definition.upperFraction - definition.lowerFraction);
    } else {
        segment = 2;
        fraction = (phase - definition.upperFraction) / (1 - definition.upperFraction);
    }
    const auto& first = definition.path[segment];
    const auto& last = definition.path[segment + 1];
    return {first.x + (last.x - first.x) * fraction, first.y + (last.y - first.y) * fraction,
        first.z + (last.z - first.z) * fraction};
}

inline bool storylandEscalatorShouldActivate(const StorylandEscalatorDefinition& definition,
    const StorylandNativePoint& camera, bool visible, size_t freeObjectSlots) {
    const float x = camera.x - definition.center.x, y = camera.y - definition.center.y,
        z = camera.z - definition.center.z;
    return visible && x * x + y * y + z * z < 625 && freeObjectSlots > definition.stepCount + 10;
}

inline size_t storylandEscalatorFreeObjectSlots(const StorylandEscalatorState& state, size_t otherObjects) {
    // VCS 288178 constructs CPool<CObject> with 0x1DB entries ("Objects").
    size_t used = otherObjects;
    for (const auto& escalator : state.escalators) if (escalator.active) used += escalator.definition.stepCount;
    return used < 475 ? 475 - used : 0;
}

inline void storylandUpdateEscalators(StorylandEscalatorState& state, StorylandNativeWorld& world,
    const StorylandNativePoint& camera, uint32_t milliseconds, size_t freeObjectSlots,
    bool stepRenderModelAvailable,
    const std::function<bool(const StorylandNativePoint&, float)>& visible) {
    if (state.escalators.empty()) return;
    const auto collision = world.collisionModels.find(state.stepModel);
    const bool modelAvailable = stepRenderModelAvailable && collision != world.collisionModels.end() &&
        (!collision->second.triangles.empty() || !collision->second.boxes.empty() || !collision->second.spheres.empty());
    if (!state.attached) {
        // Allocate stable host slots after the decoded 24/28/2C native pools.
        for (size_t index = 0; index < state.escalators.size(); ++index) {
            auto& escalator = state.escalators[index];
            for (size_t stepIndex = 0; stepIndex < escalator.steps.size(); ++stepIndex) {
                StorylandNativeCollisionInstance instance;
                instance.poolHeader = StorylandEscalatorState::GeneratedPoolHeader;
                instance.poolIndex = uint32_t(index * 32 + stepIndex);
                instance.originalModelId = state.stepModel;
                instance.modelId = UINT32_MAX;
                instance.nativeSectorLists = 0x800;
                instance.matrix = instance.originalMatrix = escalator.definition.matrix;
                const auto existing = std::find_if(world.instances.begin(), world.instances.end(),
                    [&](const auto& entry) {return entry.poolHeader == instance.poolHeader && entry.poolIndex == instance.poolIndex;});
                if (existing == world.instances.end()) {
                    escalator.steps[stepIndex].worldIndex = world.instances.size();
                    world.instances.push_back(instance);
                } else {
                    escalator.steps[stepIndex].worldIndex = size_t(existing - world.instances.begin());
                    *existing = instance;
                }
            }
        }
        state.attached = true;
        storylandPrepareNativeCollisionIndex(world);
    }
    const bool advancing = state.initialized && milliseconds >= state.milliseconds;
    const uint32_t elapsed = advancing ? milliseconds - state.milliseconds : 0;
    bool topologyChanged = false;
    for (auto& escalator : state.escalators) {
        const auto& definition = escalator.definition;
        const bool inView = !visible || visible(definition.center, definition.radius);
        const float dx = camera.x - definition.center.x, dy = camera.y - definition.center.y,
            dz = camera.z - definition.center.z;
        bool activate = escalator.active;
        if (activate && (!modelAvailable || !inView || dx * dx + dy * dy + dz * dz > 784)) {
            activate = false;
            freeObjectSlots += definition.stepCount;
        }
        if (!activate && modelAvailable && storylandEscalatorShouldActivate(definition, camera, inView, freeObjectSlots)) {
            activate = true;
            freeObjectSlots -= definition.stepCount;
        }
        const bool wasActive = escalator.active;
        escalator.active = activate;
        for (size_t index = 0; index < escalator.steps.size(); ++index) {
            auto& step = escalator.steps[index];
            if (step.worldIndex >= world.instances.size()) continue;
            auto& instance = world.instances[step.worldIndex];
            step.previousMatrix = instance.matrix;
            step.velocity = {};
            step.continuous = false;
            if (!activate) {
                if (instance.modelId != UINT32_MAX) {instance.modelId = UINT32_MAX; topologyChanged = true;}
                continue;
            }
            const float phase = storylandEscalatorPhase(definition, milliseconds, uint32_t(index));
            const auto position = storylandEscalatorPosition(definition, phase);
            auto matrix = definition.matrix;
            matrix[12] = position.x; matrix[13] = position.y; matrix[14] = position.z;
            step.continuous = wasActive && advancing && std::fabs(phase - step.phase) < .5f;
            if (step.continuous && elapsed) {
                const float scale = 1000.0f / float(elapsed);
                step.velocity = {(position.x - step.previousMatrix[12]) * scale,
                    (position.y - step.previousMatrix[13]) * scale, (position.z - step.previousMatrix[14]) * scale};
            }
            step.phase = phase;
            if (instance.modelId != state.stepModel) {
                instance.modelId = state.stepModel;
                instance.matrix = matrix;
                topologyChanged = true;
            } else storylandSetNativeInstanceMatrix(world, step.worldIndex, matrix);
        }
    }
    if (topologyChanged) storylandPrepareNativeCollisionIndex(world);
    state.milliseconds = milliseconds;
    state.initialized = true;
}

inline bool storylandEscalatorCarry(const StorylandEscalatorState& state,
    const StorylandNativeWorld& world, const StorylandNativePoint& feet, bool grounded,
    StorylandNativePoint& displacement, StorylandNativePoint& velocity, float supportTolerance = .12f) {
    displacement = velocity = {};
    if (!grounded || !std::isfinite(supportTolerance) || supportTolerance <= 0) return false;
    const auto collision = world.collisionModels.find(state.stepModel);
    if (collision == world.collisionModels.end()) return false;
    float nearest = std::numeric_limits<float>::max();
    bool supported = false;
    for (const auto& escalator : state.escalators) {
        if (!escalator.active) continue;
        for (const auto& step : escalator.steps) {
            if (!step.continuous || step.worldIndex >= world.instances.size()) continue;
            float fraction = 1;
            StorylandNativePoint normal;
            const StorylandNativePoint start{feet.x, feet.y, feet.z + supportTolerance},
                end{feet.x, feet.y, feet.z - supportTolerance};
            if (!storylandTraceCollisionModel(collision->second, step.previousMatrix, start, end, fraction, normal) ||
                normal.z <= .7f) continue;
            const float separation = std::fabs(supportTolerance * (1 - 2 * fraction));
            if (separation >= nearest) continue;
            nearest = separation;
            const auto& matrix = world.instances[step.worldIndex].matrix;
            displacement = {matrix[12] - step.previousMatrix[12], matrix[13] - step.previousMatrix[13],
                matrix[14] - step.previousMatrix[14]};
            velocity = step.velocity;
            supported = true;
        }
    }
    return supported;
}
