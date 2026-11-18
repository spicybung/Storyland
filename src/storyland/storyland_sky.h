#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cstddef>
#include <string>
#include "src/graphics/storyland_native_graphics.h"
#include "src/storyland/storyland_script_corona.h"

struct StorylandSkyColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct StorylandSkyRotation {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline float storylandSkyViewElevation(const std::array<float,16>& view,
    float horizontalProjection,float verticalProjection,float x,float y) {
    if(!std::isfinite(horizontalProjection)||!std::isfinite(verticalProjection)||
        std::fabs(horizontalProjection)<.000001f||std::fabs(verticalProjection)<.000001f)return 0;
    const float rayX=x/horizontalProjection,rayY=y/verticalProjection;
    const float elevation=(view[8]*rayX+view[9]*rayY-view[10])/std::hypot(rayX,rayY,1.0f);
    return std::isfinite(elevation)?std::clamp(elevation,-1.0f,1.0f):0;
}

enum class StorylandSkyGame {
    Lcs,
    Vcs,
    Manhunt2
};

enum class StorylandSkyWeather {
    Sunny,
    Cloudy,
    Rainy,
    Foggy,
    Windy,
    Thunderstorm
};

// Blended timecycle state. The sky, fog, water and Aero all read these colours.
struct StorylandSkyState {
    StorylandSkyColor zenith;
    StorylandSkyColor upper;
    StorylandSkyColor horizon;
    StorylandSkyColor lower;
    StorylandSkyColor fog;
    StorylandSkyColor ambient;
    StorylandSkyColor objectAmbient;
    StorylandSkyColor ambientBlur;
    StorylandSkyColor objectAmbientBlur;
    StorylandSkyColor directional;
    StorylandSkyColor sunCore;
    StorylandSkyColor sunCorona;
    StorylandSkyColor moon;
    StorylandSkyColor cloud;

    float previewFogStart = 4.0f;
    float previewFarClip = 12.0f;
    float worldFogStart = 200.0f;
    float worldFarClip = 800.0f;
    float sunDirectionX = 0.0f;
    float sunDirectionY = -1.0f;
    float sunDirectionZ = 0.25f;
    float moonDirectionX = 0.0f;
    float moonDirectionY = 1.0f;
    float moonDirectionZ = 0.25f;
    float sunSize = 0.075f;
    float nativeSunSize = 1.0f;
    float spriteSize = 1, spriteBrightness = 1;
    float shadowStrength = 128;
    float radiosityIntensity = 0, radiosityLimit = 0, frameGlowIntensity = 0, frameGlowRadius = 0;
    StorylandSkyColor frameFilter;
    float moonSize = 0.055f;
    float starIntensity = 0.0f;
    float cloudOpacity = 0.0f;
    float cloudOffset = 0.0f;
    float rainIntensity = 0.0f;
    float lightningIntensity = 0.0f;
    float rainOffset = 0.0f;
    float snowIntensity = 0.0f;
    float windStrength = 0.0f, windX = 0.0f, windY = 0.0f;
};

class StorylandSky {
public:
    StorylandSky();

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setGame(StorylandSkyGame game);
    StorylandSkyGame game() const;

    void setWeather(StorylandSkyWeather weather);
    StorylandSkyWeather weather() const;

    void setTime(float hour);
    float time() const;
    void stepTime(float hours);
    void update(float elapsedSeconds);

    bool loadVcsTimecycle(const std::wstring& filePath, std::string& errorMessage);
    bool loadVcsTimecycleText(const std::string& text, std::string& errorMessage);
    bool hasVcsTimecycle() const;
    bool loadManhunt2Weather(const std::wstring& filePath, std::string& errorMessage);
    bool loadManhunt2WeatherText(const std::string& text, std::string& errorMessage);
    bool hasManhunt2WeatherFile() const;
    bool loadNativeTimecycle(const std::vector<uint8_t>& dtz, bool vcs, std::string& error);
    void setNativeWeather(unsigned index);
    void setNativeWeatherState(unsigned previous, unsigned current, float interpolation, float rain);
    unsigned previousNativeWeather() const { return nativeWeatherPrevious; }
    float nativeWeatherInterpolation() const { return nativeWeatherBlend; }
    float nativeWeatherRain() const { return nativeRainIntensity; }
    void setExtraColours(int index,bool fade);
    float extraColourInterpolation() const { return extraColourWeight; }
    unsigned nativeWeather() const { return nativeWeatherIndex; }
    bool hasNativeTimecycle() const {
        return currentGame != StorylandSkyGame::Manhunt2 &&
            nativeTimecycles[currentGame == StorylandSkyGame::Vcs ? 1u : 0u].isLoaded();
    }
    void setWindDirection(float degrees);
    float windDirection() const { return windDirectionDegrees; }

    void setSanAndreasStyle(bool enabled);
    void setPspLighting(bool enabled);
    bool isSanAndreasStyle() const { return sanAndreasStyle; }
    const StorylandSkyState& state() const;

    void drawBackground(const StorylandSkyRotation& viewRotation, float radius = 100.0f,
        bool useCurrentView = false, float nativeFieldOfView = 0.0f) const;
    void drawWeatherOverlay() const;
    void drawNativeSunCoronas(bool clearLine,uint32_t milliseconds,float nativeTimeStep,
        bool interior=false) const;

    struct NativeFlareRaster {
        unsigned width = 0;
        unsigned height = 0;
        std::vector<uint8_t> rgba;
        std::vector<uint8_t> ps2Alpha;
        bool ps2FiveBitColor=false;
        bool valid() const {
            return width >= 4u && height >= 4u && width <= 1024u && height <= 1024u &&
                rgba.size() == size_t(width) * size_t(height) * 4u;
        }
    };
    const NativeFlareRaster& nativeFlareRaster(unsigned type) const;
    void releaseNativeFlareGpu() const;

private:
    bool enabled = true;
    bool sanAndreasStyle = false;
    bool pspLighting = false;
    int extraColour=-1,extraColourRow=0;
    bool extraColourFade=false;
    float extraColourWeight=0;
    StorylandSkyGame currentGame = StorylandSkyGame::Vcs;
    StorylandSkyWeather currentWeather = StorylandSkyWeather::Sunny;
    float currentHour = 12.0f;
    float cloudOffset = 0.0f;
    float rainOffset = 0.0f;
    float windDirectionDegrees = 45.0f;
    StorylandSkyState currentState;

    std::array<NativeFlareRaster, 9> nativeFlareRasters{};
    int nativeFlareGame = -1;
    unsigned nativeFlareRevision = 1u;
    mutable unsigned nativeFlareUploadedRevision = 0u;
    mutable std::array<uint32_t, 9> nativeFlareGpu{};
    mutable std::array<uint8_t,2> nativeSunAlpha{};
    mutable std::array<StorylandVcsSunRegistration,2> nativeSunRegistrations{};
    mutable uint32_t nativeSunMilliseconds=0;
    mutable bool nativeSunInitialized=false;
    void readNativeFlareRasters(const std::vector<uint8_t>& dtz, bool vcs);
    unsigned nativeFlareGpuTexture(unsigned type) const;
    bool drawNativeCelestial(float x,float y,float z,float radius,float size,
        const StorylandSkyColor& color,unsigned type) const;

    struct TimecycleRow {
        StorylandSkyColor ambient;
        StorylandSkyColor objectAmbient;
        StorylandSkyColor ambientBlur;
        StorylandSkyColor objectAmbientBlur;
        StorylandSkyColor directional;
        StorylandSkyColor skyTop;
        StorylandSkyColor skyBottom;
        StorylandSkyColor sunCore;
        StorylandSkyColor sunCorona;
        StorylandSkyColor lowCloud;
        StorylandSkyColor topCloud;
        StorylandSkyColor bottomCloud;
        float sunSize = 1.0f;
        float spriteSize = 1, spriteBrightness = 1;
        float shadowStrength = 128;
        float radiosityIntensity = 0, radiosityLimit = 0, frameGlowIntensity = 0, frameGlowRadius = 0;
        StorylandSkyColor frameFilter;
        float farClip = 800.0f;
        float fogStart = 200.0f;
    };

    std::array<std::array<TimecycleRow, 24>, 4> vcsTimecycleRows{};
    std::array<uint32_t, 4> vcsTimecycleRowCounts{};
    bool vcsTimecycleLoaded = false;
    std::array<StorylandNativeTimecycle, 2> nativeTimecycles;
    unsigned nativeWeatherIndex = 0;
    unsigned nativeWeatherPrevious = 0;
    float nativeWeatherBlend = 0;
    float nativeRainIntensity = 0;

    struct Manhunt2WeatherRecord {
        float farClip = 200.0f;
        float fogStart = 30.0f;
        StorylandSkyColor sky;
        StorylandSkyColor clouds;
    };
    std::array<Manhunt2WeatherRecord, 6> manhunt2Weather{};
    bool manhunt2WeatherFileLoaded = false;

    StorylandSkyState evaluateSanAndreasState() const;
    void evaluateState();
    StorylandSkyState evaluateVcsTimecycleState() const;
    StorylandSkyState evaluateManhunt2WeatherState() const;
    void applyNativeWeatherEffects(StorylandSkyState& state) const;
};
