#pragma once

#include <array>
#include <cstdint>
#include <string>

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

enum class StorylandSkyGame {
    Lcs,
    Vcs
};

enum class StorylandSkyWeather {
    Sunny,
    Cloudy,
    Rainy,
    Foggy
};

struct StorylandSkyState {
    StorylandSkyColor zenith;
    StorylandSkyColor upper;
    StorylandSkyColor horizon;
    StorylandSkyColor lower;
    StorylandSkyColor fog;
    StorylandSkyColor ambient;
    StorylandSkyColor directional;
    StorylandSkyColor sun;
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
    float moonSize = 0.055f;
    float starIntensity = 0.0f;
    float cloudOpacity = 0.0f;
    float cloudOffset = 0.0f;
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

    const StorylandSkyState& state() const;

    void drawBackground(const StorylandSkyRotation& viewRotation, float radius = 100.0f) const;

private:
    bool enabled = true;
    StorylandSkyGame currentGame = StorylandSkyGame::Vcs;
    StorylandSkyWeather currentWeather = StorylandSkyWeather::Sunny;
    float currentHour = 12.0f;
    float cloudOffset = 0.0f;
    StorylandSkyState currentState;

    struct TimecycleRow {
        StorylandSkyColor ambient;
        StorylandSkyColor directional;
        StorylandSkyColor skyTop;
        StorylandSkyColor skyBottom;
        StorylandSkyColor sunCore;
        StorylandSkyColor sunCorona;
        StorylandSkyColor lowCloud;
        StorylandSkyColor topCloud;
        StorylandSkyColor bottomCloud;
        float sunSize = 1.0f;
        float farClip = 800.0f;
        float fogStart = 200.0f;
    };

    std::array<std::array<TimecycleRow, 24>, 4> vcsTimecycleRows{};
    std::array<uint32_t, 4> vcsTimecycleRowCounts{};
    bool vcsTimecycleLoaded = false;

    void evaluateState();
    StorylandSkyState evaluateVcsTimecycleState() const;
};
