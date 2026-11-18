#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

enum class StorylandGraphicsPlatform { Ps2, Psp, Mobile, Desktop };
// Keep the byte colours in the row until the sky blends the hours/weather.
struct StorylandNativeTimecycleRow {
    std::array<uint8_t,3> ambient{}, objectAmbient{}, directional{}, skyTop{}, skyBottom{};
    std::array<uint8_t,3> ambientBlur{}, objectAmbientBlur{};
    std::array<uint8_t,3> sunCore{}, sunCorona{}, lowCloud{}, cloudTop{}, cloudBottom{};
    float sunSize = 1, spriteSize = 1, spriteBrightness = 1, farClip = 800, fogStart = 200;
    uint8_t shadowStrength = 0, lightShadowStrength = 0, poleShadowStrength = 0;
    float radiosityIntensity = 0, radiosityLimit = 0, frameGlowIntensity = 0, frameGlowRadius = 0;
    std::array<uint8_t,3> frameFilter{};
};
class StorylandNativeTimecycle {
public:
    bool load(const std::vector<uint8_t>& dtz, bool vcs, std::string& error);
    bool isLoaded() const { return loaded; }
    bool isVcs() const { return vcsGame; }
    const StorylandNativeTimecycleRow& row(unsigned hour,unsigned weather) const;
private:
    bool loaded = false, vcsGame = false;
    std::array<std::array<StorylandNativeTimecycleRow,24>,8> rows{};
};
std::vector<std::array<uint8_t,3>> storylandNativeClothingPalette(const std::vector<uint8_t>& dtz,bool vcs);
std::vector<std::array<uint8_t,3>> storylandNativeVehiclePalette(const std::vector<uint8_t>& dtz,bool vcs);
int storylandNativeClothingSlot(uint32_t materialRgba);
uint32_t storylandNativePedMaterialColor(uint32_t materialRgba,
    const std::array<uint32_t,6>& clothingColors);
std::array<uint8_t,4> storylandGraphicsDecode5551(uint16_t packed,StorylandGraphicsPlatform platform);
