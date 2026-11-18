#pragma once
#include "src/leeds/leeds_dtz.h"
#include "src/leeds/leeds_native_world.h"
#include "src/storyland/storyland_script_corona.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>

struct StorylandC2dLightClock {
    uint32_t milliseconds = 0;
    unsigned hour = 12;
    float wetRoads = 0;
    bool vcs = true;
    bool bridgeWarning = false;
};

inline StorylandC2dLightClock storylandC2dClock(uint32_t milliseconds, float hour,
    float wetRoads, bool vcs, bool bridgeWarning = false) {
    if (!std::isfinite(hour)) hour = 12;
    hour = std::fmod(hour, 24.0f);
    if (hour < 0) hour += 24;
    return {milliseconds, unsigned(hour), wetRoads, vcs, bridgeWarning};
}

struct StorylandC2dLightActivation {
    bool lit = false;
    bool registerDark = false;
    bool controlsEntityVisibility = false;
};

inline StorylandC2dLightActivation storylandC2dLightActivation(
    const StorylandDtz2dfxEffect& effect, uint16_t entityPhase,
    const StorylandC2dLightClock& clock) {
    StorylandC2dLightActivation result;
    const bool night = clock.hour < 7 || clock.hour >= 19;
    const uint32_t time = clock.milliseconds;
    const uint32_t local = effect.modelEffectIndex;
    const auto flicker = [&](bool indexed) {
        const uint32_t phase = indexed ? uint32_t(entityPhase) << 3 : entityPhase;
        result.lit = ((phase ^ time) & 0x60) != 0;
        result.registerDark = !result.lit;
        if (((phase ^ (time >> 11)) & 3) != 0) result.lit = true;
    };
    switch (effect.lightType) {
    case 0: result.lit = true; break;
    case 1: result.lit = night; break;
    case 2: flicker(false); break;
    case 3:
        if (night || clock.wetRoads > .5f) flicker(false);
        break;
    case 4: result.lit = ((time + (local << 7)) & 0x200) != 0; break;
    case 5: result.lit = night && ((time + (local << 7)) & 0x200) != 0; break;
    case 6: result.lit = ((time + (local << 8)) & 0x400) != 0; break;
    case 7: result.lit = night && ((time + (local << 8)) & 0x400) != 0; break;
    case 8: result.lit = ((time + (local << 9)) & 0x800) != 0; break;
    case 9: result.lit = night && ((time + (local << 9)) & 0x800) != 0; break;
    case 10:
    case 11:
        if (effect.lightType == 10 || night) {
            if (entityPhase < 17) flicker(true);
            else result.lit = true;
        }
        break;
    case 12: break;
    case 13: result.lit = !clock.vcs && clock.bridgeWarning && (time & 0x200) != 0; break;
    case 14: result.lit = !clock.vcs && clock.bridgeWarning && (time & 0x1FF) < 60; break;
    default: break;
    }
    result.controlsEntityVisibility = (effect.flags & 8) != 0;
    if (result.controlsEntityVisibility) result.registerDark = false;
    return result;
}

struct StorylandC2dObjectLightState {
    bool authored = false;
    bool lit = false;
    bool controlsVisibility = false;
    bool visible = true;

    bool emissionEnabled() const { return !authored || lit; }
    bool placementVisible(bool lightPass, bool fallbackLightPass) const {
        return visible && (!lightPass || (authored ? lit : fallbackLightPass));
    }
};

// A draw-frame view of the authored switches, shared by colour, shadow and
// reflection submissions. It is rebuilt from the current clock and entity state.
class StorylandC2dWorldLightState {
public:
    StorylandC2dWorldLightState(const std::vector<StorylandDtz2dfxEffect>& effects,
        const std::vector<StorylandDtz2dfxWorldInstance>& instances,
        const StorylandC2dLightClock& clock) {
        models.reserve(effects.size());
        for (const auto& effect : effects) {
            if (!effect.valid || !effect.isLight || effect.modelIndex < 0) continue;
            models.push_back({uint32_t(effect.modelIndex), effect.modelEffectIndex,
                stateFor(effect, 0, clock, true)});
        }
        merge(models);
        entities.reserve(instances.size());
        for (const auto& instance : instances) {
            if (!instance.valid || instance.effectIndex >= effects.size()) continue;
            const auto& effect = effects[instance.effectIndex];
            if (!effect.valid || !effect.isLight) continue;
            const uint64_t identity = uint64_t(instance.poolHeader) << 32 | instance.poolIndex;
            entities.push_back({identity, effect.modelEffectIndex,
                stateFor(effect, instance.lightPhase, clock, instance.nativeProcessLights)});
        }
        merge(entities);
    }

    StorylandC2dObjectLightState object(int32_t model, uint32_t poolHeader = 0,
        uint32_t poolIndex = UINT32_MAX) const {
        if (poolHeader && poolIndex != UINT32_MAX) {
            const uint64_t identity = uint64_t(poolHeader) << 32 | poolIndex;
            if (const auto* state = find(entities, identity)) return *state;
        }
        if (model >= 0)
            if (const auto* state = find(models, uint32_t(model))) return *state;
        return {};
    }

private:
    struct Entry {
        uint64_t identity = 0;
        uint32_t effectIndex = 0;
        StorylandC2dObjectLightState state;
    };
    std::vector<Entry> models, entities;

    static StorylandC2dObjectLightState stateFor(const StorylandDtz2dfxEffect& effect,
        uint16_t phase, const StorylandC2dLightClock& clock, bool processLights) {
        StorylandC2dObjectLightState state;
        state.authored = true;
        if (!processLights) return state;
        const auto activation = storylandC2dLightActivation(effect, phase, clock);
        state.lit = activation.lit;
        state.controlsVisibility = activation.controlsEntityVisibility;
        state.visible = !state.controlsVisibility || activation.lit;
        return state;
    }

    static void merge(std::vector<Entry>& entries) {
        std::sort(entries.begin(), entries.end(), [](const Entry& first, const Entry& second) {
            if (first.identity != second.identity) return first.identity < second.identity;
            return first.effectIndex < second.effectIndex;
        });
        size_t output = 0;
        for (size_t index = 0; index < entries.size(); ++index) {
            if (output && entries[output - 1].identity == entries[index].identity) {
                auto& target = entries[output - 1].state;
                const auto& source = entries[index].state;
                target.lit |= source.lit;
                if (!target.controlsVisibility && source.controlsVisibility) {
                    target.controlsVisibility = true;
                    target.visible = source.visible;
                }
            } else {
                if (output != index) entries[output] = entries[index];
                ++output;
            }
        }
        entries.resize(output);
    }

    static const StorylandC2dObjectLightState* find(const std::vector<Entry>& entries, uint64_t identity) {
        const auto found = std::lower_bound(entries.begin(), entries.end(), identity,
            [](const Entry& entry, uint64_t value) { return entry.identity < value; });
        return found != entries.end() && found->identity == identity ? &found->state : nullptr;
    }
};

struct StorylandC2dCorona {
    uint64_t identity = 0;
    StorylandNativePoint position;
    float radius = 0, farClip = 0;
    std::array<uint8_t, 3> color{};
    uint8_t alpha = 0, targetAlpha = 0;
    uint8_t flare = 0, roadReflection = 0;
    bool checkObstacles = false, longDistance = false;
    std::string texture;
};

struct StorylandC2dLightFrame {
    // The sprite pool is separate from the lamps used for lighting geometry.
    std::array<StorylandC2dCorona, 56> coronas{};
    uint32_t milliseconds = 0;
    bool initialized = false;
    bool vcs = true;
    size_t registered = 0, dropped = 0;
};

inline bool storylandC2dResolveLightPosition(const StorylandDtz2dfxEffect& effect,
    const StorylandDtz2dfxWorldInstance& source, const StorylandNativeWorld& world,
    StorylandNativePoint& position) {
    const uint64_t identity = uint64_t(source.poolHeader) << 32 | source.poolIndex;
    const auto instance = std::lower_bound(world.instances.begin(), world.instances.end(), identity,
        [](const StorylandNativeCollisionInstance& entry, uint64_t key) {
            return (uint64_t(entry.poolHeader) << 32 | entry.poolIndex) < key;
        });
    if (instance == world.instances.end() || instance->poolHeader != source.poolHeader ||
        instance->poolIndex != source.poolIndex) {
        position = {source.worldX, source.worldY, source.worldZ};
        return source.atZ >= .96f;
    }
    if (instance->modelId != uint32_t(source.modelIndex) || instance->matrix[10] < .96f) return false;
    const auto& matrix = instance->matrix;
    position = {
        matrix[0] * effect.localX + matrix[4] * effect.localY + matrix[8] * effect.localZ + matrix[12],
        matrix[1] * effect.localX + matrix[5] * effect.localY + matrix[9] * effect.localZ + matrix[13],
        matrix[2] * effect.localX + matrix[6] * effect.localY + matrix[10] * effect.localZ + matrix[14]};
    return true;
}

struct StorylandC2dPointLight {
    StorylandNativePoint position;
    std::array<float,3> color{};
    float radius=0;
    bool wetReflection=false;
};

// Corona size is for the sprite. The point-light range belongs to the world.
template<class Visit>
inline void storylandForEachC2dPointLight(const std::vector<StorylandDtz2dfxEffect>& effects,
    const std::vector<StorylandDtz2dfxWorldInstance>& instances,const StorylandNativeWorld& world,
    const StorylandC2dLightClock& clock,const StorylandNativePoint& focus,Visit visit) {
    for(const auto& source:instances) {
        if(!source.valid||!source.nativeProcessLights||source.effectIndex>=effects.size())continue;
        const auto& effect=effects[source.effectIndex];
        if(!effect.valid||!effect.isLight||!std::isfinite(effect.pointLightRange)||effect.pointLightRange<=0||
            !(effect.red||effect.green||effect.blue))continue;
        const float radius=effect.pointLightRange;
        const float limit=180+radius;
        if(std::abs(source.worldX-focus.x)>limit||std::abs(source.worldY-focus.y)>limit)continue;
        const auto activation=storylandC2dLightActivation(effect,source.lightPhase,clock);
        if(!activation.lit||activation.controlsEntityVisibility)continue;
        StorylandNativePoint position;
        if(!storylandC2dResolveLightPosition(effect,source,world,position))continue;
        visit(StorylandC2dPointLight{position,{effect.red/255.f,effect.green/255.f,effect.blue/255.f},
            radius,effect.roadReflection!=0});
    }
}

inline void storylandUpdateC2dLights(StorylandC2dLightFrame& frame,
    const std::vector<StorylandDtz2dfxEffect>& effects,
    const std::vector<StorylandDtz2dfxWorldInstance>& instances,
    const StorylandNativeWorld& world, const StorylandNativePoint& camera,
    const StorylandC2dLightClock& clock, float spriteSize,
    const std::function<bool(const StorylandNativePoint&, float)>& onScreen,
    const std::function<bool(const StorylandNativePoint&, const StorylandNativePoint&)>& visible) {
    if (frame.initialized && clock.milliseconds < frame.milliseconds) frame = {};
    const uint32_t elapsed = frame.initialized ? clock.milliseconds - frame.milliseconds : 0;
    frame.initialized = true;
    frame.vcs = clock.vcs;
    frame.milliseconds = clock.milliseconds;
    frame.registered = frame.dropped = 0;
    for (auto& corona : frame.coronas) corona.targetAlpha = 0;
    if (!std::isfinite(spriteSize) || spriteSize < 0) spriteSize = 1;
    for (const auto& source : instances) {
        if (!source.valid || !source.nativeProcessLights || source.effectIndex >= effects.size()) continue;
        const auto& effect = effects[source.effectIndex];
        if (!effect.valid || !effect.isLight || effect.coronaSize <= 0 || effect.coronaFarClip <= 0) continue;
        const auto activation = storylandC2dLightActivation(effect, source.lightPhase, clock);
        if (activation.controlsEntityVisibility || (!activation.lit && !activation.registerDark)) continue;
        StorylandNativePoint position;
        if (!storylandC2dResolveLightPosition(effect, source, world, position)) continue;
        const float dx = position.x - camera.x, dy = position.y - camera.y;
        if (dx * dx + dy * dy > effect.coronaFarClip * effect.coronaFarClip) continue;
        const float radius = effect.coronaSize * spriteSize;
        if (onScreen && !onScreen(position, radius)) continue;
        const uint64_t identity = (uint64_t(source.entityOffset) << 32) | (uint64_t(effect.modelEffectIndex) + 1);
        auto slot = std::find_if(frame.coronas.begin(), frame.coronas.end(),
            [&](const auto& value) { return value.identity == identity; });
        if (slot == frame.coronas.end()) {
            slot = std::find_if(frame.coronas.begin(), frame.coronas.end(),
                [](const auto& value) { return value.identity == 0; });
            if (slot == frame.coronas.end()) { ++frame.dropped; continue; }
            *slot = {};
            slot->identity = identity;
        }
        slot->position = position;
        slot->radius = radius;
        slot->farClip = effect.coronaFarClip;
        slot->color = activation.lit ? std::array<uint8_t, 3>{effect.red, effect.green, effect.blue} :
            std::array<uint8_t, 3>{0, 0, 0};
        slot->targetAlpha = 255;
        slot->flare = effect.flareType;
        slot->roadReflection = effect.roadReflection;
        slot->checkObstacles = (effect.flags & 1) != 0;
        slot->longDistance = (effect.flags & 16) != 0;
        slot->texture = effect.coronaTextureName.empty() ? "coronastar" : effect.coronaTextureName;
        ++frame.registered;
    }
    const float change = float(elapsed) * .05f * 15;
    for (auto& corona : frame.coronas) {
        if (!corona.identity) continue;
        uint8_t target = corona.targetAlpha;
        if (corona.checkObstacles && visible && !visible(corona.position, camera)) target = 0;
        if (corona.alpha < target) corona.alpha = uint8_t(std::min(float(target), float(corona.alpha) + change));
        else if (corona.alpha > target) corona.alpha = uint8_t(std::max(float(target), float(corona.alpha) - change));
        if (corona.alpha == 0 && corona.targetAlpha == 0) corona = {};
    }
}

inline float storylandC2dCoronaDistanceAlpha(float depth, float farClip) {
    if (!std::isfinite(depth) || !std::isfinite(farClip) || depth <= 0 || farClip <= 0 || depth >= farClip) return 0;
    return depth < farClip * .5f ? 1.0f : 2.0f * (1 - depth / farClip);
}

struct StorylandC2dCoronaSprite {
    StorylandNativePoint position;
    float radius = 0, rotation = 0, alpha = 0;
    std::array<float, 3> color{};
    std::string texture;
    bool depthTest = true, flareGhost = false;
};

template<class DrawSprite>
inline void storylandForEachC2dCoronaSprite(const StorylandC2dLightFrame& frame,
    const float* view, const float* projection, int viewportHeight, DrawSprite draw,float fogginess=0) {
    for (const auto& corona : frame.coronas) {
        if (!corona.identity || !corona.alpha) continue;
        StorylandCoronaProjection projected;
        if (!storylandProjectCorona({corona.position.x, corona.position.y, corona.position.z},
            view, projection, projected, corona.farClip)) continue;
        const float alpha = corona.alpha / 255.0f * storylandCoronaDistanceFade(projected.depth, corona.farClip);
        if (alpha <= 0) continue;
        const float nearFade = std::clamp(projected.biasedDepth - 1.3f, 0.0f, 1.0f);
        StorylandC2dCoronaSprite sprite;
        sprite.position = {projected.center[0], projected.center[1], projected.center[2]};
        const float nativeProjectionScale=frame.vcs&&std::fabs(projection[5])>1.0e-6f?
            2/std::fabs(projection[5]):1;
        const float fogScale=frame.vcs?storylandCoronaFogScale(projected.depth,fogginess):1;
        sprite.radius = (frame.vcs?corona.radius:-corona.radius)*projected.depthScale*nativeProjectionScale*fogScale;
        sprite.rotation = 20 / projected.depth;
        sprite.alpha = alpha * nearFade * nearFade;
        sprite.color = {corona.color[0] / 255.0f, corona.color[1] / 255.0f, corona.color[2] / 255.0f};
        const uint8_t nativeAlpha=storylandCoronaFadedAlpha(corona.alpha,projected.depth,corona.farClip);
        if(frame.vcs) {
            sprite.color=storylandCoronaSpriteColor(storylandCoronaFogColor(corona.color,fogScale),
                nativeAlpha,projected.biasedDepth);
            sprite.alpha=1;
        }
        sprite.texture = corona.texture;
        sprite.depthTest = !corona.checkObstacles;
        draw(sprite);
        if (viewportHeight <= 0 || std::fabs(projection[5]) < 1.0e-6f) continue;
        size_t count = 0;
        const auto& flares = storylandCoronaFlares(corona.flare, count,frame.vcs);
        for (size_t index = 0; index < count; ++index) {
            const auto& flare = flares[index];
            const float x = projected.cameraX * (flare.position - 1) * projected.depthScale;
            const float y = projected.cameraY * (flare.position - 1) * projected.depthScale;
            StorylandC2dCoronaSprite ghost = sprite;
            ghost.position = {sprite.position.x + view[0] * x + view[1] * y,
                sprite.position.y + view[4] * x + view[5] * y,
                sprite.position.z + view[8] * x + view[9] * y};
            ghost.radius = 8*flare.size*projected.biasedDepth/
                ((frame.vcs?448:viewportHeight)*std::fabs(projection[5]));
            ghost.rotation = 0;
            ghost.alpha = alpha * 200 / 256;
            ghost.texture = storylandCoronaTextureName(flare.texture);
            ghost.flareGhost = true;
            for (size_t channel = 0; channel < 3; ++channel)
                ghost.color[channel] *= flare.color[channel] / 256.0f;
            if(frame.vcs) {
                ghost.color=storylandCoronaFlareColor(corona.color,nativeAlpha,flare);
                ghost.alpha=1;
            }
            draw(ghost);
        }
    }
}
