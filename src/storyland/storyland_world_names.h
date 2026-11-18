#pragma once

#include "src/storyland/storyland_gxt.h"
#include "src/storyland/storyland_play_test_hud.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

inline std::string storylandNativeVehicleNameKey(const std::vector<uint8_t>& data, uint32_t model, bool vcs) {
    const auto word = [&](size_t offset) -> uint32_t {
        if (offset > data.size() || data.size() - offset < 4) return 0;
        return uint32_t(data[offset]) | uint32_t(data[offset + 1]) << 8 |
            uint32_t(data[offset + 2]) << 16 | uint32_t(data[offset + 3]) << 24;
    };
    const uint32_t count = word(0x38), table = word(0x3C);
    if (!table || model >= count || uint64_t(table) + uint64_t(model) * 4 + 4 > data.size()) return {};
    const uint32_t info = word(size_t(table) + size_t(model) * 4);
    const size_t nameOffset = vcs ? 0x158 : 0x2E;
    if (!info || uint64_t(info) + nameOffset + 10 > data.size() || data[size_t(info) + 0x10] != 6) return {};
    std::string key;
    for (size_t index = 0; index < 10; ++index) {
        const uint8_t value = data[size_t(info) + nameOffset + index];
        if (!value) return key;
        if (index >= 8 || !((value >= 'A' && value <= 'Z') ||
                (value >= '0' && value <= '9') || value == '_')) return {};
        key.push_back(char(value));
    }
    return {};
}

inline std::string storylandWorldNameUtf8(const std::u16string& text) {
    std::string result;
    for (size_t index = 0; index < text.size(); ++index) {
        uint32_t value = text[index];
        if (value >= 0xD800 && value <= 0xDBFF && index + 1 < text.size() &&
                text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF) {
            value = 0x10000 + ((value - 0xD800) << 10) + (text[++index] - 0xDC00);
        } else if (value >= 0xD800 && value <= 0xDFFF) value = 0xFFFD;
        if (value < 0x80) result.push_back(char(value));
        else if (value < 0x800) {
            result.push_back(char(0xC0 | (value >> 6)));
            result.push_back(char(0x80 | (value & 63)));
        } else if (value < 0x10000) {
            result.push_back(char(0xE0 | (value >> 12)));
            result.push_back(char(0x80 | ((value >> 6) & 63)));
            result.push_back(char(0x80 | (value & 63)));
        } else {
            result.push_back(char(0xF0 | (value >> 18)));
            result.push_back(char(0x80 | ((value >> 12) & 63)));
            result.push_back(char(0x80 | ((value >> 6) & 63)));
            result.push_back(char(0x80 | (value & 63)));
        }
    }
    return result;
}

class StorylandWorldNameHud {
public:
    void update(const std::vector<uint8_t>& data, const StorylandNativeZones& zones,
        const StorylandGxtFile& gxt, const StorylandPlayTestRuntime& runtime,
        const StorylandMissionEnvironment& world, const StorylandNativePoint& player, bool vcs) {
        const double now = runtime.elapsed;
        if (!std::isfinite(now)) return;
        if (now < lastTime) reset();
        lastTime = now;
        if (!vcs) {
            std::string nextZone;
            std::u16string resolvedZone;
            if (world.areaNameForced) {
                nextZone = world.areaNameKey;
                gxt.lookup(nextZone, "MAIN", resolvedZone);
            } else {
                for (bool information : {true, false}) {
                    const auto* zone = zones.at(player, information);
                    if (zone && gxt.lookup(zone->name, "MAIN", resolvedZone) && !resolvedZone.empty()) {
                        nextZone = zone->name;
                        break;
                    }
                }
            }
            if (nextZone != zoneKey) {
                zoneKey = nextZone;
                zoneText = storylandWorldNameUtf8(resolvedZone);
                zoneStartedAt = now;
            }
            forcedZone = world.areaNameForced;
        }
        const int slot = runtime.occupiedVehicle;
        uint32_t model = UINT32_MAX;
        if (slot >= 0 && size_t(slot) < runtime.vehicles.size())
            model = runtime.vehicles[size_t(slot)].nativeDefinition.modelId;
        if (slot != vehicleSlot || model != vehicleModel) {
            vehicleSlot = slot;
            vehicleModel = model;
            if (model != UINT32_MAX) {
                const auto key = storylandNativeVehicleNameKey(data, model, vcs);
                std::u16string text;
                vehicleText = !key.empty() && gxt.lookup(key, "MAIN", text) ? storylandWorldNameUtf8(text) : std::string{};
                vehicleStartedAt = now;
            }
        }
    }

    void draw(ImDrawList* drawList, const StorylandRetailHudAssets* assets,
        const StorylandPlayTestRuntime& runtime, const StorylandMissionEnvironment& world, bool vcs) const {
        if (!drawList || !runtime.hud || runtime.paused || runtime.pauseMenu || !world.wantsHud ||
            world.widescreen || world.cutscene.playing || world.credits.active) return;
        const auto display = ImGui::GetIO().DisplaySize;
        const float scale = display.y / 272.f;
        const float right = display.x - 18.f * scale;
        const float height = 19.f * scale;
        const auto label = [&](const std::string& text, double started, float y, ImU32 color, bool forced) {
            const double elapsed = double(runtime.elapsed) - started;
            const float opacity = forced ? 1.f : float(std::clamp(std::min(elapsed * 4, 6 - elapsed), 0.0, 1.0));
            if (text.empty() || opacity <= 0) return;
            color = (color & ~IM_COL32_A_MASK) | (ImU32(255.f * opacity) << IM_COL32_A_SHIFT);
            const float width = std::max(1.f, display.x - 36.f * scale);
            float fontHeight = height;
            auto extent = storylandMeasurePlayTestText(assets, text.c_str(), fontHeight, 0, "fancytext");
            if (extent.x > width) {
                fontHeight *= width / extent.x;
                extent = storylandMeasurePlayTestText(assets, text.c_str(), fontHeight, 0, "fancytext");
            }
            storylandDrawPlayTestText(drawList, assets, {right - extent.x + scale, y + scale},
                IM_COL32(0, 0, 0, int(255.f * opacity)), text.c_str(), fontHeight, 0, "fancytext");
            storylandDrawPlayTestText(drawList, assets, {right - extent.x, y}, color,
                text.c_str(), fontHeight, 0, "fancytext");
        };
        if (!vcs) label(zoneText, zoneStartedAt, 224.f * scale,
            IM_COL32(120, 170, 225, 255), forcedZone);
        label(vehicleText, vehicleStartedAt, 245.f * scale, IM_COL32(170, 215, 165, 255), false);
    }

    const std::string& zone() const { return zoneText; }
    const std::string& vehicle() const { return vehicleText; }

private:
    void reset() {
        zoneKey.clear(); zoneText.clear(); vehicleText.clear();
        vehicleSlot = -1; vehicleModel = UINT32_MAX;
        zoneStartedAt = vehicleStartedAt = 0;
    }
    std::string zoneKey, zoneText, vehicleText;
    int vehicleSlot = -1;
    uint32_t vehicleModel = UINT32_MAX;
    bool forcedZone = false;
    double zoneStartedAt = 0, vehicleStartedAt = 0, lastTime = 0;
};
