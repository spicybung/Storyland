#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

// Restart protection alone does not make player-controlled gameplay cinematic.
inline bool storylandVcsCinematicNativeProps(bool scriptedCamera, bool scriptCutscene,
    bool introProtected, bool playerControl) {
    return scriptedCamera || scriptCutscene || (introProtected && !playerControl);
}

inline bool storylandMayCullStaticScriptProps(bool scriptCutscene, bool scriptedCamera,
    bool worldEffectEnabled) {
    return !scriptCutscene && !scriptedCamera && !worldEffectEnabled;
}

inline bool storylandMayCullScriptAsset(bool cullingAllowed, bool introProtected,
    bool simpleModel, bool directXActive) {
    return cullingAllowed && (!introProtected || (simpleModel && !directXActive));
}

inline uint64_t storylandVcsNativePropOwner(uint32_t pool, uint32_t index) {
    return (uint64_t(pool) << 32) | index;
}

// Per-frame membership for the native placement pools that can be drawn by
// this pass. Large or unrecognized keys retain exact set semantics.
class StorylandVcsNativeSceneryOwners {
public:
    explicit StorylandVcsNativeSceneryOwners(std::size_t instanceCount)
        : pool24(denseCapacity(instanceCount)), pool28(denseCapacity(instanceCount)),
          pool2c(denseCapacity(instanceCount)) {}

    void insert(uint64_t owner) {
        const uint32_t pool = uint32_t(owner >> 32);
        const uint32_t index = uint32_t(owner);
        auto* slots = poolSlots(pool);
        if (slots && std::size_t(index) < slots->size()) {
            (*slots)[index] = 1;
        } else {
            overflow.insert(owner);
        }
    }

    std::size_t count(uint64_t owner) const {
        const uint32_t pool = uint32_t(owner >> 32);
        const uint32_t index = uint32_t(owner);
        const auto* slots = poolSlots(pool);
        if (slots && std::size_t(index) < slots->size()) return (*slots)[index];
        return overflow.count(owner);
    }

private:
    static constexpr std::size_t maximumPoolSlots = 1024u * 1024u;

    static std::size_t denseCapacity(std::size_t instanceCount) {
        return instanceCount < maximumPoolSlots ? instanceCount : maximumPoolSlots;
    }

    std::vector<uint8_t>* poolSlots(uint32_t pool) {
        if (pool == 0x24u) return &pool24;
        if (pool == 0x28u) return &pool28;
        if (pool == 0x2cu) return &pool2c;
        return nullptr;
    }

    const std::vector<uint8_t>* poolSlots(uint32_t pool) const {
        if (pool == 0x24u) return &pool24;
        if (pool == 0x28u) return &pool28;
        if (pool == 0x2cu) return &pool2c;
        return nullptr;
    }

    std::vector<uint8_t> pool24;
    std::vector<uint8_t> pool28;
    std::vector<uint8_t> pool2c;
    std::unordered_set<uint64_t> overflow;
};

inline uint64_t storylandVcsNativeScriptObjectKey(uint32_t index, uint32_t model) {
    return (uint64_t(index) << 32) | model;
}

template<class Aliases, class CompiledModels>
bool storylandVcsUseKnownCompiledNativeProp(int32_t model, bool actorAvailable,
    const Aliases& aliases, const CompiledModels& compiledModels) {
    return model>=0 && !actorAvailable && aliases.count(model)==0 &&
        compiledModels.count(uint32_t(model))!=0;
}

template<class ActorAssets, class CompiledModels>
void storylandInvalidateMissionModelResources(int32_t model, ActorAssets& actors,
    CompiledModels& compiledModels) {
    actors.erase(model);
    if(model>=0)compiledModels.erase(uint32_t(model));
}

template<class CompiledModels, class ResidentModels>
void storylandSynchronizeCompiledModelRevision(CompiledModels& compiledModels,
    ResidentModels& residentModels, uint64_t& observedRevision, uint64_t currentRevision) {
    if(observedRevision==currentRevision)return;
    for(uint32_t model:compiledModels)
        if(model<=uint32_t(INT32_MAX))residentModels.erase(int32_t(model));
    compiledModels.clear();
    observedRevision=currentRevision;
}

template<class NativeOwners, class HiddenOwners, class ScriptOwners>
bool storylandVcsNativePropUnrepresented(uint32_t pool, uint32_t index, uint32_t model,
    const NativeOwners& nativeOwners, const HiddenOwners& hiddenOwners,
    const ScriptOwners& scriptOwners) {
    const uint64_t owner = storylandVcsNativePropOwner(pool,index);
    return nativeOwners.count(owner)==0 && hiddenOwners.count(owner)==0 &&
        scriptOwners.count(storylandVcsNativeScriptObjectKey(index,model))==0;
}

template<class IsOnScreen>
bool storylandVcsNativePropCandidate(uint32_t pool, float x, float y,
    float focusX, float focusY, bool cinematic, IsOnScreen isOnScreen,
    bool protectedIntro = false) {
    if (pool != 0x2Cu && !((cinematic || protectedIntro) && (pool == 0x24u || pool == 0x28u)))
        return false;
    if (std::hypot(x - focusX, y - focusY) <= 140.0f)
        return true;
    return cinematic && isOnScreen();
}

template<class IsVisible, class IsUnrepresented>
bool storylandVcsNativePropAdmitted(bool cinematic, IsVisible isVisible,
    IsUnrepresented isUnrepresented) {
    if (cinematic) return isUnrepresented() && isVisible();
    return isVisible() && isUnrepresented();
}
