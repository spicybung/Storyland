#define NOMINMAX

#include "src/storyland/storyland_sky.h"
#include "src/re/reVCS/render/storyland_vcs_psp_lighting.h"
#include "src/re/reVCS/render/storyland_vcs_ps2_sun.h"
#include "src/re/reVCS/render/storyland_vcs_timecycle.h"
#include "src/storyland/storyland_sun_flare.h"
#include "src/storyland/storyland_moon_sprite.h"
#include "src/leeds/leeds_native_texture.h"
#include "src/storyland/storyland_environment.h"

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __ANDROID__
#include "src/platform/android/storyland_android_gl.h"
#else
#include <GL/gl.h>
#endif
#include "src/graphics/storyland_directx.h"
#include "src/graphics/storyland_ps2_texture_upload.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr float Pi = 3.14159265358979323846f;

float clampValue(float value, float minimum, float maximum) {
    return std::max(minimum, std::min(maximum, value));
}

float smoothStep(float edge0, float edge1, float value) {
    if (edge0 == edge1) return value < edge0 ? 0.0f : 1.0f;
    float amount = clampValue((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return amount * amount * (3.0f - 2.0f * amount);
}

StorylandSkyColor mixColor(const StorylandSkyColor& a, const StorylandSkyColor& b, float amount) {
    amount = clampValue(amount, 0.0f, 1.0f);
    if(amount==0.0f)return a;
    if(amount==1.0f)return b;
    return {
        a.r + (b.r - a.r) * amount,
        a.g + (b.g - a.g) * amount,
        a.b + (b.b - a.b) * amount,
        a.a + (b.a - a.a) * amount
    };
}

StorylandSkyColor multiplyColor(const StorylandSkyColor& color, float multiplier) {
    return {
        clampValue(color.r * multiplier, 0.0f, 1.0f),
        clampValue(color.g * multiplier, 0.0f, 1.0f),
        clampValue(color.b * multiplier, 0.0f, 1.0f),
        color.a
    };
}

void setColor(const StorylandSkyColor& color) {
    glColor4f(color.r, color.g, color.b, color.a);
}

void multiplyQuaternion(const StorylandSkyRotation& rotation) {
    float lengthSquared = rotation.w * rotation.w + rotation.x * rotation.x +
        rotation.y * rotation.y + rotation.z * rotation.z;
    float inverseLength = 1.0f / std::sqrt(std::max(0.00000001f, lengthSquared));
    float w = rotation.w * inverseLength;
    float x = rotation.x * inverseLength;
    float y = rotation.y * inverseLength;
    float z = rotation.z * inverseLength;

    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;
    float wx = w * x;
    float wy = w * y;
    float wz = w * z;

    GLfloat matrix[16] = {
        1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),        2.0f * (xz - wy),        0.0f,
        2.0f * (xy - wz),        1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),        0.0f,
        2.0f * (xz + wy),        2.0f * (yz - wx),        1.0f - 2.0f * (xx + yy), 0.0f,
        0.0f,                    0.0f,                    0.0f,                    1.0f
    };
    glMultMatrixf(matrix);
}

struct SkyRing {
    float elevation;
    StorylandSkyColor color;
};

void drawSkyRings(const std::array<SkyRing, 7>& rings, float radius) {
    constexpr int SegmentCount = 64;
    for (size_t ringIndex = 0; ringIndex + 1 < rings.size(); ++ringIndex) {
        const SkyRing& lower = rings[ringIndex];
        const SkyRing& upper = rings[ringIndex + 1];
        float lowerElevation = lower.elevation * Pi / 180.0f;
        float upperElevation = upper.elevation * Pi / 180.0f;
        float lowerRadius = std::cos(lowerElevation) * radius;
        float upperRadius = std::cos(upperElevation) * radius;
        float lowerZ = std::sin(lowerElevation) * radius;
        float upperZ = std::sin(upperElevation) * radius;

        glBegin(GL_QUAD_STRIP);
        for (int segment = 0; segment <= SegmentCount; ++segment) {
            float angle = float(segment) / float(SegmentCount) * Pi * 2.0f;
            float cosine = std::cos(angle);
            float sine = std::sin(angle);
            setColor(lower.color);
            glVertex3f(cosine * lowerRadius, sine * lowerRadius, lowerZ);
            setColor(upper.color);
            glVertex3f(cosine * upperRadius, sine * upperRadius, upperZ);
        }
        glEnd();
    }
}

void buildBillboardBasis(float directionX, float directionY, float directionZ, float size,
                         float& centerX, float& centerY, float& centerZ,
                         float& rightX, float& rightY, float& rightZ,
                         float& upX, float& upY, float& upZ) {
    float length = std::sqrt(std::max(0.000001f,
        directionX * directionX + directionY * directionY + directionZ * directionZ));
    centerX = directionX / length;
    centerY = directionY / length;
    centerZ = directionZ / length;

    // Cross against world-up normally, but switch reference axes close to the
    // zenith/nadir. Without this fallback the sun billboard collapses to a
    // point at noon because its horizontal direction has zero length.
    float referenceX = 0.0f;
    float referenceY = std::fabs(centerZ) > 0.95f ? 1.0f : 0.0f;
    float referenceZ = std::fabs(centerZ) > 0.95f ? 0.0f : 1.0f;
    rightX = centerY * referenceZ - centerZ * referenceY;
    rightY = centerZ * referenceX - centerX * referenceZ;
    rightZ = centerX * referenceY - centerY * referenceX;
    float rightLength = std::sqrt(std::max(0.000001f,
        rightX * rightX + rightY * rightY + rightZ * rightZ));
    rightX = rightX / rightLength;
    rightY = rightY / rightLength;
    rightZ = rightZ / rightLength;

    upX = rightY * centerZ - rightZ * centerY;
    upY = rightZ * centerX - rightX * centerZ;
    upZ = rightX * centerY - rightY * centerX;
    float upLength = std::sqrt(std::max(0.000001f, upX * upX + upY * upY + upZ * upZ));
    rightX *= size;
    rightY *= size;
    rightZ *= size;
    upX = upX / upLength * size;
    upY = upY / upLength * size;
    upZ = upZ / upLength * size;
}

void drawFeatheredDisc(float directionX, float directionY, float directionZ, float radius,
                       float size, const StorylandSkyColor& color,
                       float middleScale, float middleAlphaScale) {
    if (color.a <= 0.001f || directionZ <= -0.10f) return;

    float centerX, centerY, centerZ;
    float rightX, rightY, rightZ;
    float upX, upY, upZ;
    buildBillboardBasis(directionX, directionY, directionZ, size,
                        centerX, centerY, centerZ, rightX, rightY, rightZ, upX, upY, upZ);
    centerX *= radius;
    centerY *= radius;
    centerZ *= radius;
    rightX *= radius;
    rightY *= radius;
    rightZ *= radius;
    upX *= radius;
    upY *= radius;
    upZ *= radius;

    StorylandSkyColor middle = color;
    middle.a = clampValue(color.a * middleAlphaScale, 0.0f, 1.0f);

    StorylandSkyColor edge = color;
    edge.a = 0.0f;

    constexpr int SegmentCount = 48;

    glBegin(GL_TRIANGLE_FAN);
    setColor(color);
    glVertex3f(centerX, centerY, centerZ);
    for (int segment = 0; segment <= SegmentCount; ++segment) {
        float angle = float(segment) / float(SegmentCount) * Pi * 2.0f;
        float cosine = std::cos(angle);
        float sine = std::sin(angle);

        setColor(middle);
        glVertex3f(centerX + (rightX * cosine + upX * sine) * middleScale,
                   centerY + (rightY * cosine + upY * sine) * middleScale,
                   centerZ + (rightZ * cosine + upZ * sine) * middleScale);
    }
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    for (int segment = 0; segment <= SegmentCount; ++segment) {
        float angle = float(segment) / float(SegmentCount) * Pi * 2.0f;
        float cosine = std::cos(angle);
        float sine = std::sin(angle);

        setColor(middle);
        glVertex3f(centerX + (rightX * cosine + upX * sine) * middleScale,
                   centerY + (rightY * cosine + upY * sine) * middleScale,
                   centerZ + (rightZ * cosine + upZ * sine) * middleScale);

        setColor(edge);
        glVertex3f(centerX + rightX * cosine + upX * sine,
                   centerY + rightY * cosine + upY * sine,
                   centerZ + rightZ * cosine + upZ * sine);
    }
    glEnd();
}

uint32_t starHash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    return value ^ (value >> 16);
}

void drawStars(float radius, float intensity) {
    if (intensity <= 0.001f) return;
    glPointSize(1.5f);
    glBegin(GL_POINTS);
    for (uint32_t index = 0; index < 180; ++index) {
        uint32_t first = starHash(index * 3u + 1u);
        uint32_t second = starHash(index * 3u + 2u);
        float azimuth = float(first & 0xFFFFu) / 65535.0f * Pi * 2.0f;
        float elevation = (12.0f + float(second & 0xFFFFu) / 65535.0f * 76.0f) * Pi / 180.0f;
        float brightness = (0.35f + float((second >> 16) & 0xFFu) / 255.0f * 0.65f) * intensity;
        glColor4f(brightness, brightness, std::min(1.0f, brightness * 1.08f), brightness);
        float ringRadius = std::cos(elevation) * radius;
        glVertex3f(std::cos(azimuth) * ringRadius, std::sin(azimuth) * ringRadius,
                   std::sin(elevation) * radius);
    }
    glEnd();
    glPointSize(1.0f);
}

void drawCloudBands(float radius, const StorylandSkyState& state) {
    if (state.cloudOpacity <= 0.001f) return;

    constexpr int SegmentCount = 160;
    constexpr int BandCount = 3;
    const float lowerDegrees[BandCount] = {12.0f, 22.0f, 34.0f};
    const float upperDegrees[BandCount] = {22.0f, 34.0f, 48.0f};

    for (int band = 0; band < BandCount; ++band) {
        const float lowerElevation = lowerDegrees[band] * Pi / 180.0f;
        const float upperElevation = upperDegrees[band] * Pi / 180.0f;
        const float lowerRingRadius = std::cos(lowerElevation) * radius;
        const float upperRingRadius = std::cos(upperElevation) * radius;
        const float lowerZ = std::sin(lowerElevation) * radius;
        const float upperZ = std::sin(upperElevation) * radius;
        const float phase = state.cloudOffset * Pi * 2.0f * (0.32f + float(band) * 0.11f) + float(band) * 1.91f;

        glBegin(GL_QUAD_STRIP);
        for (int segment = 0; segment <= SegmentCount; ++segment) {
            const float angle = float(segment) / float(SegmentCount) * Pi * 2.0f;
            const float shape = 0.54f +
                0.20f * std::sin(angle * 2.0f + phase) +
                0.12f * std::sin(angle * 3.0f - phase * 0.63f) +
                0.07f * std::sin(angle * 5.0f + phase * 0.27f);
            const float coverage = smoothStep(0.20f, 0.82f, shape);
            // Never collapse a sunny cloud layer into isolated dots.  The retail
            // Stories sky reads as broad translucent masses, not sparse particles.
            const float body = 0.20f + coverage * 0.80f;
            const float bandStrength = 0.34f + float(band) * 0.07f;

            StorylandSkyColor lowerColor = state.cloud;
            StorylandSkyColor upperColor = state.cloud;
            lowerColor.a = state.cloudOpacity * body * bandStrength * 0.42f;
            upperColor.a = state.cloudOpacity * body * bandStrength * 0.66f;

            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            setColor(lowerColor);
            glVertex3f(cosine * lowerRingRadius, sine * lowerRingRadius, lowerZ);
            setColor(upperColor);
            glVertex3f(cosine * upperRingRadius, sine * upperRingRadius, upperZ);
        }
        glEnd();
    }
}

StorylandSkyState interpolateState(const StorylandSkyState& a, const StorylandSkyState& b, float amount) {
    StorylandSkyState result = a;
    result.zenith = mixColor(a.zenith, b.zenith, amount);
    result.upper = mixColor(a.upper, b.upper, amount);
    result.horizon = mixColor(a.horizon, b.horizon, amount);
    result.lower = mixColor(a.lower, b.lower, amount);
    result.fog = mixColor(a.fog, b.fog, amount);
    result.ambient = mixColor(a.ambient, b.ambient, amount);
    result.objectAmbient = mixColor(a.objectAmbient, b.objectAmbient, amount);
    result.ambientBlur = mixColor(a.ambientBlur, b.ambientBlur, amount);
    result.objectAmbientBlur = mixColor(a.objectAmbientBlur, b.objectAmbientBlur, amount);
    result.directional = mixColor(a.directional, b.directional, amount);
    result.sunCore = mixColor(a.sunCore, b.sunCore, amount);
    result.sunCorona = mixColor(a.sunCorona, b.sunCorona, amount);
    result.moon = mixColor(a.moon, b.moon, amount);
    return result;
}

StorylandSkyColor byteColor(const std::vector<float>& values, size_t index) {
    return {
        clampValue(values[index] / 255.0f, 0.0f, 1.0f),
        clampValue(values[index + 1] / 255.0f, 0.0f, 1.0f),
        clampValue(values[index + 2] / 255.0f, 0.0f, 1.0f),
        1.0f
    };
}

std::string uppercaseString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return char(std::toupper(character));
    });
    return value;
}

int weatherIndexForSection(const std::string& sectionName) {
    if (sectionName == "SUNNY") return 0;
    if (sectionName == "CLOUDY") return 1;
    if (sectionName == "RAINY") return 2;
    if (sectionName == "FOGGY") return 3;
    return -1;
}

int manhunt2WeatherIndex(std::string name) {
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char letter) {
        return char(std::toupper(letter));
    });
    if(name=="CLEAR")return 0;
    if(name=="CLOUDY")return 1;
    if(name=="RAINY")return 2;
    if(name=="FOGGY")return 3;
    if(name=="WINDY")return 4;
    if(name=="THUNDERSTORM")return 5;
    return -1;
}

}

StorylandSky::StorylandSky() {
    const auto color=[](float red,float green,float blue) {
        return StorylandSkyColor{red/255.0f,green/255.0f,blue/255.0f,1.0f};
    };
    // Defaults come from the retail Manhunt 2 GLOBAL/WEATHER.INI. Loading the
    // user's copy replaces the complete table, without bundling game assets.
    manhunt2Weather[0]={300.0f,100.0f,color(2,2,5),color(2,2,5)};
    manhunt2Weather[1]={200.0f,30.0f,color(12,12,12),color(6,6,6)};
    manhunt2Weather[2]={200.0f,30.0f,color(12,18,12),color(6,9,6)};
    manhunt2Weather[3]={30.0f,5.0f,color(20,20,20),color(20,20,20)};
    manhunt2Weather[4]={200.0f,30.0f,color(10,10,10),color(7,7,7)};
    manhunt2Weather[5]={100.0f,30.0f,color(10,11,10),color(5,6,5)};
    evaluateState();
}

void StorylandSky::setEnabled(bool value) { enabled = value; }
void StorylandSky::setSanAndreasStyle(bool value) {
    if (sanAndreasStyle == value) return;
    sanAndreasStyle = value;
    evaluateState();
}

void StorylandSky::setPspLighting(bool value) {
    if(pspLighting==value)return;
    pspLighting=value;
    ++nativeFlareRevision;
    nativeSunInitialized=false;
    evaluateState();
}
bool StorylandSky::isEnabled() const { return enabled; }

void StorylandSky::setGame(StorylandSkyGame value) {
    if(currentGame!=value)nativeSunInitialized=false;
    currentGame = value;
    if(value!=StorylandSkyGame::Manhunt2 && !sanAndreasStyle &&
       (currentWeather==StorylandSkyWeather::Windy || currentWeather==StorylandSkyWeather::Thunderstorm))
        currentWeather=StorylandSkyWeather::Sunny;
    evaluateState();
}

StorylandSkyGame StorylandSky::game() const { return currentGame; }

void StorylandSky::setWeather(StorylandSkyWeather value) {
    currentWeather = value;
    if (currentGame != StorylandSkyGame::Manhunt2) {
        nativeWeatherIndex = std::min(static_cast<unsigned>(value), 3u);
        nativeWeatherPrevious = nativeWeatherIndex;
        nativeWeatherBlend = 0;
        nativeRainIntensity = nativeWeatherIndex == 2u ? 1.0f : 0.0f;
    }
    evaluateState();
}

StorylandSkyWeather StorylandSky::weather() const { return currentWeather; }

void StorylandSky::setTime(float hour) {
    if(!std::isfinite(hour)) return;
    currentHour=std::fmod(hour,24.0f);
    if(currentHour<0) currentHour+=24.0f;
    evaluateState();
}

float StorylandSky::time() const { return currentHour; }

void StorylandSky::stepTime(float hours) { setTime(currentHour + hours); }

void StorylandSky::setExtraColours(int index,bool fade) {
    if(index<-1||index>=24)return;
    if(extraColour==index&&extraColourFade==fade)return;
    extraColour=index;extraColourFade=fade;
    if(index>=0) {extraColourRow=index;extraColourWeight=fade?0.0f:1.0f;}
    else if(!fade)extraColourWeight=0;
    evaluateState();
}

void StorylandSky::update(float elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f) return;
    if(extraColourFade)extraColourWeight=std::clamp(extraColourWeight+
        (extraColour>=0?1.0f:-1.0f)*elapsedSeconds*(50.0f/120.0f),0.0f,1.0f);
    cloudOffset += elapsedSeconds * (0.0005f + currentState.windStrength * 0.0025f);
    cloudOffset -= std::floor(cloudOffset);
    rainOffset += std::max(0.0f,elapsedSeconds)*2.25f;
    rainOffset -= std::floor(rainOffset/16.0f)*16.0f;
    evaluateState();
}

void StorylandSky::setWindDirection(float degrees) {
    if (!std::isfinite(degrees)) return;
    windDirectionDegrees = std::remainder(degrees, 360.0f);
    evaluateState();
}

void StorylandSky::applyNativeWeatherEffects(StorylandSkyState& result) const {
    if(currentGame==StorylandSkyGame::Vcs&&!pspLighting) {
        const float distance=std::hypot(100.0f,15.0f);
        result.moonDirectionX=0;result.moonDirectionY=-100.0f/distance;
        result.moonDirectionZ=15.0f/distance;
        const auto brightness=storylandVcsMoonBrightness(currentHour,nativeWeatherPrevious,
            nativeWeatherIndex,nativeWeatherBlend);
        const float tint=float(brightness.tint)/255.0f;
        result.moon={tint,tint,tint,brightness.raw?1.0f:0.0f};
    }
    const float previousWind = storylandWeatherWind(nativeWeatherPrevious);
    result.windStrength = previousWind +
        (storylandWeatherWind(nativeWeatherIndex) - previousWind) * nativeWeatherBlend;
    const float direction = windDirectionDegrees * Pi / 180.0f;
    result.windX = std::cos(direction) * result.windStrength;
    result.windY = std::sin(direction) * result.windStrength;
    result.rainIntensity = nativeRainIntensity;
    result.snowIntensity = currentGame == StorylandSkyGame::Lcs && nativeWeatherIndex == 7u ? 1.0f : 0.0f;
    result.rainOffset = rainOffset;
    result.cloudOffset = cloudOffset;
    if (nativeWeatherIndex == 5u) {
        const float flash = std::fmod(rainOffset, 5.4f);
        result.lightningIntensity = flash < 0.06f ? 0.78f : flash < 0.13f ? 0.22f : 0.0f;
    }
}

bool StorylandSky::loadVcsTimecycle(const std::wstring& filePath, std::string& errorMessage) {
    errorMessage.clear();
    std::ifstream stream{std::filesystem::path(filePath)};
    if (!stream) {
        errorMessage = "Could not open VCS timecyc.dat.";
        return false;
    }

    std::ostringstream contents;
    contents << stream.rdbuf();
    return loadVcsTimecycleText(contents.str(), errorMessage);
}

bool StorylandSky::loadVcsTimecycleText(const std::string& text, std::string& errorMessage) {
    errorMessage.clear();
    std::istringstream stream(text);

    std::array<std::array<TimecycleRow, 24>, 4> parsedRows{};
    std::array<uint32_t, 4> parsedCounts{};
    int activeWeather = -1;
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;

        if (line.compare(first, 2, "//") == 0) {
            size_t marker = line.find_last_of('/');
            if (marker != std::string::npos && marker + 1 < line.size()) {
                std::string section = line.substr(marker + 1);
                size_t sectionStart = section.find_first_not_of(" \t");
                if (sectionStart != std::string::npos) section = section.substr(sectionStart);
                section = uppercaseString(section);
                int candidate = weatherIndexForSection(section);
                if (candidate >= 0 && parsedCounts[size_t(candidate)] == 0) activeWeather = candidate;
                else if (candidate < 0 && section.find("AMBIENT") == std::string::npos) {
                    if (line.find("////") != std::string::npos) activeWeather = -1;
                }
            }
            continue;
        }

        if (activeWeather < 0 || parsedCounts[size_t(activeWeather)] >= 24) continue;
        std::istringstream valuesStream(line);
        std::vector<float> values;
        float value = 0.0f;
        while (valuesStream >> value) values.push_back(value);
        if (values.size() != 56) continue;

        TimecycleRow row;
        row.ambient = byteColor(values, 0);
        row.objectAmbient = byteColor(values, 3);
        row.ambientBlur = byteColor(values, 6);
        row.objectAmbientBlur = byteColor(values, 9);
        row.directional = byteColor(values, 12);
        row.skyTop = byteColor(values, 15);
        row.skyBottom = byteColor(values, 18);
        row.sunCore = byteColor(values, 21);
        row.sunCorona = byteColor(values, 24);
        row.sunSize = values[27];
        row.spriteSize = values[28];
        row.spriteBrightness = values[29];
        row.farClip = values[33];
        row.fogStart = values[34];
        row.lowCloud = byteColor(values, 38);
        row.topCloud = byteColor(values, 41);
        row.bottomCloud = byteColor(values, 44);
        parsedRows[size_t(activeWeather)][parsedCounts[size_t(activeWeather)]++] = row;
    }

    for (size_t weather = 0; weather < parsedCounts.size(); ++weather) {
        if (parsedCounts[weather] != 24) {
            std::ostringstream message;
            message << "VCS timecyc.dat weather table " << weather << " contains "
                    << parsedCounts[weather] << " rows; expected 24.";
            errorMessage = message.str();
            return false;
        }
    }

    vcsTimecycleRows = parsedRows;
    vcsTimecycleRowCounts = parsedCounts;
    vcsTimecycleLoaded = true;
    evaluateState();
    return true;
}

bool StorylandSky::hasVcsTimecycle() const { return vcsTimecycleLoaded; }

bool StorylandSky::loadManhunt2Weather(const std::wstring& filePath,std::string& errorMessage) {
    std::ifstream file{std::filesystem::path(filePath)};
    if(!file) {
        errorMessage="Could not open Manhunt 2 GLOBAL/WEATHER.INI.";
        return false;
    }
    std::ostringstream contents;
    contents<<file.rdbuf();
    if(!file.good()&&!file.eof()) {
        errorMessage="Could not read Manhunt 2 WEATHER.INI completely.";
        return false;
    }
    return loadManhunt2WeatherText(contents.str(),errorMessage);
}

bool StorylandSky::loadManhunt2WeatherText(const std::string& text,std::string& errorMessage) {
    auto parsed=manhunt2Weather;
    std::array<unsigned,6> fields{};
    std::istringstream input(text);
    std::string line;
    int active=-1;
    unsigned lineNumber=0;
    while(std::getline(input,line)) {
        ++lineNumber;
        const size_t comment=line.find('#');
        if(comment!=std::string::npos)line.resize(comment);
        std::istringstream words(line);
        std::string command;
        if(!(words>>command))continue;
        command=uppercaseString(command);
        if(command=="RECORD") {
            std::string name;
            words>>name;
            active=manhunt2WeatherIndex(name);
            if(active>=0&&fields[size_t(active)]!=0) {
                errorMessage="Duplicate Manhunt 2 weather record: "+name;
                return false;
            }
            continue;
        }
        if(command=="END") {active=-1;continue;}
        if(active<0)continue;
        auto& record=parsed[size_t(active)];
        auto& mask=fields[size_t(active)];
        if(command=="FARCLIP"||command=="FOGSTART") {
            std::string token;
            if(!(words>>token)) {
                errorMessage="Missing Manhunt 2 fog distance at line "+std::to_string(lineNumber);
                return false;
            }
            size_t consumed=0;
            float value=0;
            try {value=std::stof(token,&consumed);}
            catch(const std::exception&) {consumed=0;}
            if(consumed==0||(consumed<token.size()&&token.substr(consumed)!="f"&&token.substr(consumed)!="F")||
                !std::isfinite(value)||value<0.0f||value>100000.0f) {
                errorMessage="Invalid Manhunt 2 fog distance at line "+std::to_string(lineNumber);
                return false;
            }
            if(command=="FARCLIP") {record.farClip=value;mask|=1u;}
            else {record.fogStart=value;mask|=2u;}
        } else if(command=="SKY"||command=="CLOUDS") {
            std::string channels;
            std::getline(words,channels);
            std::replace(channels.begin(),channels.end(),',',' ');
            std::istringstream rgb(channels);
            int red=-1,green=-1,blue=-1;
            if(!(rgb>>red>>green>>blue)||red<0||red>255||green<0||green>255||blue<0||blue>255) {
                errorMessage="Invalid Manhunt 2 weather colour at line "+std::to_string(lineNumber);
                return false;
            }
            const StorylandSkyColor color{red/255.0f,green/255.0f,blue/255.0f,1.0f};
            if(command=="SKY") {record.sky=color;mask|=4u;}
            else {record.clouds=color;mask|=8u;}
        }
    }
    for(size_t index=0;index<fields.size();++index) {
        if(fields[index]!=15u||parsed[index].farClip<=parsed[index].fogStart) {
            errorMessage="Manhunt 2 WEATHER.INI is missing or has an invalid record " +
                std::to_string(index)+" (CLEAR, CLOUDY, RAINY, FOGGY, WINDY, THUNDERSTORM).";
            return false;
        }
    }
    manhunt2Weather=parsed;
    manhunt2WeatherFileLoaded=true;
    errorMessage.clear();
    evaluateState();
    return true;
}

bool StorylandSky::hasManhunt2WeatherFile() const {return manhunt2WeatherFileLoaded;}

void StorylandSky::readNativeFlareRasters(const std::vector<uint8_t>& dtz, bool vcs) {
    for(auto& raster:nativeFlareRasters)raster={};
    nativeFlareGame=vcs?1:0;
    ++nativeFlareRevision;
    if(nativeFlareRevision==0u)nativeFlareRevision=1u;
    std::set<std::string> requested;
    for(unsigned type=0;type<nativeFlareRasters.size();++type) {
        const char* name=storylandCoronaTextureName(int(type));
        if(name)requested.insert(name);
    }
    // Decode the original PS2 raster and CLUT bytes out of the resident
    // GAME.DTZ texture dictionaries. Do not draw a replacement if absent.
    const auto textures=storylandFindNativeTextures(dtz,vcs,requested,4u*1024u*1024u);
    for(const auto& texture:textures)for(unsigned type=0;type<nativeFlareRasters.size();++type) {
        const char* name=storylandCoronaTextureName(int(type));
        if(!name||texture.name!=name)continue;
        if(texture.image.width<=0||texture.image.height<=0)continue;
        NativeFlareRaster raster;
        raster.width=unsigned(texture.image.width);
        raster.height=unsigned(texture.image.height);
        raster.rgba=texture.image.rgba;
        raster.ps2Alpha=texture.image.ps2Alpha;
        raster.ps2FiveBitColor=texture.image.ps2FiveBitColor;
        if(raster.valid())nativeFlareRasters[type]=std::move(raster);
    }
}

const StorylandSky::NativeFlareRaster& StorylandSky::nativeFlareRaster(unsigned type) const {
    static const NativeFlareRaster empty;
    return type<nativeFlareRasters.size()?nativeFlareRasters[type]:empty;
}

void StorylandSky::releaseNativeFlareGpu() const {
    for(uint32_t& id:nativeFlareGpu) {
        if(id!=0u&&glIsTexture(GLuint(id))) {
            const GLuint texture=GLuint(id);
            glDeleteTextures(1,&texture);
        }
        id=0u;
    }
    nativeFlareUploadedRevision=0u;
}

unsigned StorylandSky::nativeFlareGpuTexture(unsigned type) const {
    if(type>=nativeFlareRasters.size()||currentGame==StorylandSkyGame::Manhunt2||
       nativeFlareGame!=(currentGame==StorylandSkyGame::Vcs?1:0))return 0u;
    if(nativeFlareUploadedRevision!=nativeFlareRevision) {
        releaseNativeFlareGpu();
        nativeFlareUploadedRevision=nativeFlareRevision;
    }
    const NativeFlareRaster& raster=nativeFlareRasters[type];
    if(!raster.valid())return 0u;
    uint32_t& cached=nativeFlareGpu[type];
    if(cached!=0u&&glIsTexture(GLuint(cached)))return cached;
    GLint previousBinding=0,previousAlignment=4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&previousBinding);
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&previousAlignment);
    GLuint texture=0;
    glGenTextures(1,&texture);
    if(!texture)return 0u;
    glBindTexture(GL_TEXTURE_2D,texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812Fu);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812Fu);
    const auto ps2Pixels=!pspLighting?storylandPs2UploadPixels(raster.rgba.data(),raster.rgba.size(),
        raster.ps2Alpha.data(),raster.ps2Alpha.size(),raster.ps2FiveBitColor):std::vector<uint8_t>{};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,GLsizei(raster.width),GLsizei(raster.height),0,
        GL_RGBA,GL_UNSIGNED_BYTE,ps2Pixels.empty()?raster.rgba.data():ps2Pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT,previousAlignment);
    glBindTexture(GL_TEXTURE_2D,GLuint(previousBinding));
    cached=uint32_t(texture);
    return cached;
}

bool StorylandSky::drawNativeCelestial(float x,float y,float z,float radius,float size,
    const StorylandSkyColor& color,unsigned type) const {
    if(color.a<=0.001f||z<=-0.10f||size<=0.0f)return false;
    const GLuint texture=GLuint(nativeFlareGpuTexture(type));
    if(!texture)return false;
    float cx=0.0f,cy=0.0f,cz=0.0f;
    float rx=0.0f,ry=0.0f,rz=0.0f;
    float ux=0.0f,uy=0.0f,uz=0.0f;
    buildBillboardBasis(x,y,z,size,cx,cy,cz,rx,ry,rz,ux,uy,uz);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,texture);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glColor4f(color.r,color.g,color.b,color.a);
    glBegin(GL_QUADS);
    const float points[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
    for(const auto& point:points) {
        glTexCoord2f((point[0]+1.0f)*0.5f,(1.0f-point[1])*0.5f);
        glVertex3f(radius*(cx+rx*point[0]+ux*point[1]),
                   radius*(cy+ry*point[0]+uy*point[1]),
                   radius*(cz+rz*point[0]+uz*point[1]));
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    return true;
}

bool StorylandSky::loadNativeTimecycle(const std::vector<uint8_t>& dtz,bool vcs,std::string& error) {
    currentGame=vcs?StorylandSkyGame::Vcs:StorylandSkyGame::Lcs;
    const bool loaded=nativeTimecycles[vcs ? 1u : 0u].load(dtz,vcs,error);
    if(loaded) {
        readNativeFlareRasters(dtz,vcs);
        nativeSunInitialized=false;
    }
    evaluateState();return loaded;
}
void StorylandSky::setNativeWeather(unsigned index) {
    const unsigned weather = std::min(index, 7u);
    setNativeWeatherState(weather, weather, 0, weather == 2u || weather == 5u ? 1.0f : 0.0f);
}

void StorylandSky::setNativeWeatherState(unsigned previous, unsigned current, float interpolation, float rain) {
    if (previous > 7 || current > 7 || !std::isfinite(interpolation) || !std::isfinite(rain)) {
        return;
    }
    nativeWeatherPrevious = previous;
    nativeWeatherIndex = current;
    nativeWeatherBlend = std::clamp(interpolation, 0.0f, 1.0f);
    nativeRainIntensity = std::clamp(rain, 0.0f, 1.0f);
    currentWeather = nativeWeatherIndex == 1u ? StorylandSkyWeather::Cloudy :
        (nativeWeatherIndex == 2u || nativeWeatherIndex == 5u) ? StorylandSkyWeather::Rainy :
        nativeWeatherIndex == 3u ? StorylandSkyWeather::Foggy : StorylandSkyWeather::Sunny;
    evaluateState();
}

const StorylandSkyState& StorylandSky::state() const { return currentState; }

StorylandSkyState StorylandSky::evaluateVcsTimecycleState() const {
    const auto& nativeTimecycle = nativeTimecycles[currentGame == StorylandSkyGame::Vcs ? 1u : 0u];
    size_t weather = 0;
    if (currentWeather == StorylandSkyWeather::Cloudy) weather = 1;
    else if (currentWeather == StorylandSkyWeather::Rainy) weather = 2;
    else if (currentWeather == StorylandSkyWeather::Foggy) weather = 3;

    int firstHour = int(std::floor(currentHour)) % 24;
    int secondHour = (firstHour + 1) % 24;
    float amount = currentHour - std::floor(currentHour);
    const bool nativeVcsPs2=currentGame==StorylandSkyGame::Vcs&&!pspLighting&&
        nativeTimecycle.isLoaded()&&nativeTimecycle.isVcs();
    const auto nativeBlend=storylandVcsTimecycleBlend(currentHour,nativeWeatherBlend);
    if(nativeVcsPs2) {
        firstHour=int(nativeBlend.hour);secondHour=int(nativeBlend.nextHour);amount=nativeBlend.time;
    }
    TimecycleRow first = vcsTimecycleRows[weather][size_t(firstHour)];
    TimecycleRow second = vcsTimecycleRows[weather][size_t(secondHour)];
    if(nativeTimecycle.isLoaded() && nativeTimecycle.isVcs()==(currentGame==StorylandSkyGame::Vcs)) {
        weather=nativeWeatherIndex;
        const auto convert=[](const StorylandNativeTimecycleRow& source) {
            TimecycleRow row;
            const auto color=[](const std::array<uint8_t,3>& rgb) { return StorylandSkyColor{rgb[0]/255.0f,rgb[1]/255.0f,rgb[2]/255.0f,1}; };
            row.ambient=color(source.ambient);row.objectAmbient=color(source.objectAmbient);
            row.ambientBlur=color(source.ambientBlur);row.objectAmbientBlur=color(source.objectAmbientBlur);
            row.directional=color(source.directional);row.skyTop=color(source.skyTop);row.skyBottom=color(source.skyBottom);
            row.sunCore=color(source.sunCore);row.sunCorona=color(source.sunCorona);
            row.lowCloud=color(source.lowCloud);row.topCloud=color(source.cloudTop);row.bottomCloud=color(source.cloudBottom);
            row.sunSize=source.sunSize;row.spriteSize=source.spriteSize;row.spriteBrightness=source.spriteBrightness;
            row.shadowStrength=float(source.shadowStrength);
            row.farClip=source.farClip;row.fogStart=source.fogStart;
            row.radiosityIntensity=source.radiosityIntensity;row.radiosityLimit=source.radiosityLimit;
            row.frameGlowIntensity=source.frameGlowIntensity;row.frameGlowRadius=source.frameGlowRadius;
            row.frameFilter=color(source.frameFilter);
            return row;
        };
        first=convert(nativeTimecycle.row(unsigned(firstHour),unsigned(weather)));
        second=convert(nativeTimecycle.row(unsigned(secondHour),unsigned(weather)));
        if (nativeWeatherPrevious != nativeWeatherIndex) {
            const auto blend = [&](TimecycleRow& current, const TimecycleRow& previous) {
                current.ambient = mixColor(previous.ambient, current.ambient, nativeWeatherBlend);
                current.objectAmbient = mixColor(previous.objectAmbient, current.objectAmbient, nativeWeatherBlend);
                current.ambientBlur = mixColor(previous.ambientBlur, current.ambientBlur, nativeWeatherBlend);
                current.objectAmbientBlur = mixColor(previous.objectAmbientBlur, current.objectAmbientBlur, nativeWeatherBlend);
                current.directional = mixColor(previous.directional, current.directional, nativeWeatherBlend);
                current.skyTop = mixColor(previous.skyTop, current.skyTop, nativeWeatherBlend);
                current.skyBottom = mixColor(previous.skyBottom, current.skyBottom, nativeWeatherBlend);
                current.sunCore = mixColor(previous.sunCore, current.sunCore, nativeWeatherBlend);
                current.sunCorona = mixColor(previous.sunCorona, current.sunCorona, nativeWeatherBlend);
                current.lowCloud = mixColor(previous.lowCloud, current.lowCloud, nativeWeatherBlend);
                current.topCloud = mixColor(previous.topCloud, current.topCloud, nativeWeatherBlend);
                current.bottomCloud = mixColor(previous.bottomCloud, current.bottomCloud, nativeWeatherBlend);
                current.sunSize = previous.sunSize + (current.sunSize - previous.sunSize) * nativeWeatherBlend;
                current.spriteSize = previous.spriteSize + (current.spriteSize - previous.spriteSize) * nativeWeatherBlend;
                current.spriteBrightness = previous.spriteBrightness + (current.spriteBrightness - previous.spriteBrightness) * nativeWeatherBlend;
                current.shadowStrength = previous.shadowStrength + (current.shadowStrength - previous.shadowStrength) * nativeWeatherBlend;
                current.farClip = previous.farClip + (current.farClip - previous.farClip) * nativeWeatherBlend;
                current.fogStart = previous.fogStart + (current.fogStart - previous.fogStart) * nativeWeatherBlend;
                current.radiosityIntensity = previous.radiosityIntensity + (current.radiosityIntensity - previous.radiosityIntensity) * nativeWeatherBlend;
                current.radiosityLimit = previous.radiosityLimit + (current.radiosityLimit - previous.radiosityLimit) * nativeWeatherBlend;
                current.frameGlowIntensity = previous.frameGlowIntensity + (current.frameGlowIntensity - previous.frameGlowIntensity) * nativeWeatherBlend;
                current.frameGlowRadius = previous.frameGlowRadius + (current.frameGlowRadius - previous.frameGlowRadius) * nativeWeatherBlend;
                current.frameFilter = mixColor(previous.frameFilter, current.frameFilter, nativeWeatherBlend);
            };
            blend(first, convert(nativeTimecycle.row(unsigned(firstHour), nativeWeatherPrevious)));
            blend(second, convert(nativeTimecycle.row(unsigned(secondHour), nativeWeatherPrevious)));
        }
        if((currentGame==StorylandSkyGame::Lcs||pspLighting)&&extraColourWeight>0) {
            const auto extra=convert(nativeTimecycle.row(unsigned(extraColourRow),6));
            const auto blend=[&](TimecycleRow& row) {
                row.ambient=mixColor(row.ambient,extra.ambient,extraColourWeight);
                row.objectAmbient=mixColor(row.objectAmbient,extra.objectAmbient,extraColourWeight);
                row.ambientBlur=mixColor(row.ambientBlur,extra.ambientBlur,extraColourWeight);
                row.objectAmbientBlur=mixColor(row.objectAmbientBlur,extra.objectAmbientBlur,extraColourWeight);
                row.directional=mixColor(row.directional,extra.directional,extraColourWeight);
                row.farClip+=(extra.farClip-row.farClip)*extraColourWeight;
                row.fogStart+=(extra.fogStart-row.fogStart)*extraColourWeight;
                const bool replaceSky=extra.skyTop.r!=0||extra.skyTop.g!=0||extra.skyTop.b!=0;
                if(pspLighting||replaceSky) {
                    row.spriteSize+=(extra.spriteSize-row.spriteSize)*extraColourWeight;
                    row.spriteBrightness+=(extra.spriteBrightness-row.spriteBrightness)*extraColourWeight;
                }
                if(replaceSky) {
                    row.skyTop=mixColor(row.skyTop,extra.skyTop,extraColourWeight);
                    row.skyBottom=mixColor(row.skyBottom,extra.skyBottom,extraColourWeight);
                    row.sunCore=mixColor(row.sunCore,extra.sunCore,extraColourWeight);
                    row.sunCorona=mixColor(row.sunCorona,extra.sunCorona,extraColourWeight);
                    row.lowCloud=mixColor(row.lowCloud,extra.lowCloud,extraColourWeight);
                    row.topCloud=mixColor(row.topCloud,extra.topCloud,extraColourWeight);
                    row.bottomCloud=mixColor(row.bottomCloud,extra.bottomCloud,extraColourWeight);
                    row.sunSize+=(extra.sunSize-row.sunSize)*extraColourWeight;
                }
            };
            blend(first);blend(second);
        }
    }

    StorylandSkyState result;
    result.zenith = mixColor(first.skyTop, second.skyTop, amount);
    result.horizon = mixColor(first.skyBottom, second.skyBottom, amount);
    if(nativeVcsPs2) {
        const auto& oldHourOldWeather=nativeTimecycle.row(nativeBlend.hour,nativeWeatherPrevious);
        const auto& oldHourNewWeather=nativeTimecycle.row(nativeBlend.hour,nativeWeatherIndex);
        const auto& nextHourOldWeather=nativeTimecycle.row(nativeBlend.nextHour,nativeWeatherPrevious);
        const auto& nextHourNewWeather=nativeTimecycle.row(nativeBlend.nextHour,nativeWeatherIndex);
        const auto nativeSkyColor=[&](std::array<uint8_t,3> StorylandNativeTimecycleRow::* member) {
            std::array<float,3> color{};
            for(size_t channel=0;channel<color.size();++channel) {
                const std::array<uint8_t,4> corners{(oldHourOldWeather.*member)[channel],
                    (oldHourNewWeather.*member)[channel],(nextHourOldWeather.*member)[channel],
                    (nextHourNewWeather.*member)[channel]};
                color[channel]=float(storylandVcsTimecycleByte(corners,nativeBlend))/255.0f;
            }
            return StorylandSkyColor{color[0],color[1],color[2],1};
        };
        result.zenith=nativeSkyColor(&StorylandNativeTimecycleRow::skyTop);
        result.horizon=nativeSkyColor(&StorylandNativeTimecycleRow::skyBottom);
    }
    result.upper = mixColor(result.horizon, result.zenith, 0.58f);
    result.lower = multiplyColor(result.horizon, 0.72f);
    result.fog = result.horizon;
    result.ambient = mixColor(first.ambient, second.ambient, amount);
    result.objectAmbient = mixColor(first.objectAmbient, second.objectAmbient, amount);
    result.ambientBlur = mixColor(first.ambientBlur, second.ambientBlur, amount);
    result.objectAmbientBlur = mixColor(first.objectAmbientBlur, second.objectAmbientBlur, amount);
    result.directional = mixColor(first.directional, second.directional, amount);
    result.sunCore = mixColor(first.sunCore, second.sunCore, amount);
    result.sunCorona = mixColor(first.sunCorona, second.sunCorona, amount);
    float moonVisibility = std::max(1.0f - smoothStep(4.5f, 6.5f, currentHour),
                                    smoothStep(18.5f, 20.5f, currentHour));
    result.moon = {0.72f, 0.78f, 1.0f, 0.78f * moonVisibility};
    StorylandSkyColor lowCloud = mixColor(first.lowCloud, second.lowCloud, amount);
    StorylandSkyColor topCloud = mixColor(first.topCloud, second.topCloud, amount);
    StorylandSkyColor bottomCloud = mixColor(first.bottomCloud, second.bottomCloud, amount);
    StorylandSkyColor timecycleCloud = mixColor(mixColor(lowCloud, topCloud, 0.55f), bottomCloud, 0.25f);
    // Keep procedural preview clouds in the same atmospheric range as the
    // timecycle sky.  Raw timecyc cloud RGB can be extremely bright and looked
    // like opaque white discs/bands in the editor's untextured sky preview.
    result.cloud = mixColor(timecycleCloud, result.upper, 0.38f);
    // timecyc SunSz is a game tuning value, not a fraction of the sky dome.
    // Dividing by 110 made values around 10 produce a billboard almost a third
    // of the viewport wide.  Keep the editor representation compact.
    const float rawSunSize = first.sunSize + (second.sunSize - first.sunSize) * amount;
    result.nativeSunSize=rawSunSize;
    result.sunSize = clampValue(rawSunSize / 250.0f, 0.018f, 0.055f);
    result.spriteSize = first.spriteSize + (second.spriteSize - first.spriteSize) * amount;
    result.spriteBrightness = first.spriteBrightness + (second.spriteBrightness - first.spriteBrightness) * amount;
    result.shadowStrength = first.shadowStrength + (second.shadowStrength - first.shadowStrength) * amount;
    result.radiosityIntensity = first.radiosityIntensity + (second.radiosityIntensity - first.radiosityIntensity) * amount;
    result.radiosityLimit = first.radiosityLimit + (second.radiosityLimit - first.radiosityLimit) * amount;
    result.frameGlowIntensity = first.frameGlowIntensity + (second.frameGlowIntensity - first.frameGlowIntensity) * amount;
    result.frameGlowRadius = first.frameGlowRadius + (second.frameGlowRadius - first.frameGlowRadius) * amount;
    result.frameFilter = mixColor(first.frameFilter, second.frameFilter, amount);
    result.worldFarClip = std::max(100.0f, first.farClip + (second.farClip - first.farClip) * amount);
    result.worldFogStart = first.fogStart + (second.fogStart - first.fogStart) * amount;
    result.previewFogStart = 4.0f;
    result.previewFarClip = 12.0f;
    result.cloudOpacity = weather == 0 ? 0.055f : weather == 1 ? 0.28f : weather == 2 ? 0.46f : 0.16f;
    result.starIntensity = std::max(1.0f - smoothStep(4.5f, 6.25f, currentHour),
                                    smoothStep(19.25f, 21.0f, currentHour));
    if (weather == 2) result.starIntensity = 0.0f;
    else if (weather == 3) result.starIntensity *= 0.2f;
    return result;
}

StorylandSkyState StorylandSky::evaluateManhunt2WeatherState() const {
    const size_t index=std::min(size_t(currentWeather),manhunt2Weather.size()-1);
    const auto& weather=manhunt2Weather[index];
    StorylandSkyState result;
    result.zenith=weather.sky;
    result.upper=mixColor(weather.sky,weather.clouds,0.20f);
    result.horizon=weather.sky;
    result.lower=mixColor(weather.sky,weather.clouds,0.38f);
    result.fog=weather.sky;
    result.cloud=weather.clouds;
    result.cloudOpacity=index==0?0.12f:index==3?0.26f:index==4?0.82f:0.94f;
    result.cloudOffset=cloudOffset;
    result.worldFarClip=weather.farClip;
    result.worldFogStart=weather.fogStart;
    result.previewFarClip=std::clamp(weather.farClip*0.06f,3.0f,24.0f);
    result.previewFogStart=std::clamp(weather.fogStart*0.06f,0.30f,12.0f);
    result.rainIntensity=index==2?0.75f:index==5?1.0f:0.0f;
    result.rainOffset=rainOffset;
    if(index==5) {
        const float stormTime=std::fmod(rainOffset,5.4f);
        result.lightningIntensity=stormTime<0.06f?0.78f:stormTime<0.13f?0.22f:0.0f;
    }
    const float flash=result.lightningIntensity;
    const float baseBrightness=std::clamp(0.18f+0.05f*weather.sky.r,0.12f,0.30f);
    result.ambient={baseBrightness+flash*0.25f,baseBrightness+flash*0.28f,
        baseBrightness+flash*0.32f,1.0f};
    result.objectAmbient=result.ambient;
    result.directional={0.24f+flash*0.70f,0.25f+flash*0.75f,
        0.27f+flash*0.82f,1.0f};
    result.sunCore.a=result.sunCorona.a=result.moon.a=0.0f;
    result.starIntensity=0.0f;
    result.windStrength = index == 4 ? 1.0f : index == 5 ? 2.0f : index == 2 ? 0.7f : 0.15f;
    result.windX = std::cos(windDirectionDegrees * Pi / 180.0f) * result.windStrength;
    result.windY = std::sin(windDirectionDegrees * Pi / 180.0f) * result.windStrength;
    return result;
}

StorylandSkyState StorylandSky::evaluateSanAndreasState() const {
    StorylandSkyState state;
    const float morning=smoothStep(5.0f,9.0f,currentHour);
    const float evening=smoothStep(17.0f,21.0f,currentHour);
    const float day=morning*(1.0f-evening);
    const float twilight=std::max(morning*(1.0f-morning)*4.0f,evening*(1.0f-evening)*4.0f);
    const float night=1.0f-day;
    const auto palette=[&](StorylandSkyColor nightColor,StorylandSkyColor dayColor,
                           StorylandSkyColor duskColor) {
        return mixColor(mixColor(nightColor,dayColor,day),duskColor,twilight*.75f);
    };
    state.zenith=palette({.035f,.055f,.12f,1},{.18f,.48f,.84f,1},{.22f,.23f,.52f,1});
    state.upper=palette({.07f,.09f,.19f,1},{.35f,.62f,.90f,1},{.51f,.31f,.58f,1});
    state.horizon=palette({.15f,.13f,.21f,1},{.76f,.81f,.89f,1},{.98f,.48f,.28f,1});
    state.lower=palette({.12f,.11f,.17f,1},{.71f,.72f,.74f,1},{.55f,.34f,.31f,1});
    state.fog=mixColor(state.lower,state.horizon,.46f);
    state.ambient=palette({.09f,.10f,.17f,1},{.39f,.42f,.49f,1},{.28f,.25f,.29f,1});
    state.objectAmbient=state.ambient;
    state.directional=palette({.10f,.13f,.18f,1},{.95f,.90f,.78f,1},{1.0f,.62f,.40f,1});
    state.sunCore={1.0f,.95f,.78f,day};
    state.sunCorona={1.0f,.73f,.41f,std::min(1.0f,day*.72f+twilight*.42f)};
    state.moon={.73f,.80f,.97f,night};
    state.cloud=palette({.30f,.32f,.46f,1},{.97f,.98f,1.0f,1},{.99f,.70f,.52f,1});
    state.cloudOpacity=.28f;
    state.starIntensity=night*night;
    state.sunSize=.085f;
    state.moonSize=.045f;
    state.worldFarClip=950.0f;
    state.worldFogStart=390.0f;
    state.previewFarClip=16.0f;
    state.previewFogStart=7.0f;
    const float angle=(currentHour-6.0f)*Pi/12.0f;
    state.sunDirectionX=std::cos(angle)*.25f;
    state.sunDirectionY=-std::cos(angle);
    state.sunDirectionZ=std::sin(angle);
    state.moonDirectionX=-state.sunDirectionX;
    state.moonDirectionY=-state.sunDirectionY;
    state.moonDirectionZ=-state.sunDirectionZ;
    state.cloudOffset=cloudOffset;
    state.rainOffset=rainOffset;

    StorylandSkyColor overcast={.37f,.43f,.49f,1};
    if(currentWeather==StorylandSkyWeather::Cloudy){
        state.zenith=mixColor(state.zenith,overcast,.57f);
        state.horizon=mixColor(state.horizon,overcast,.48f);
        state.cloudOpacity=.78f;
        state.worldFarClip=680.0f;
    } else if(currentWeather==StorylandSkyWeather::Rainy ||
              currentWeather==StorylandSkyWeather::Thunderstorm){
        overcast={.23f,.27f,.34f,1};
        state.zenith=mixColor(state.zenith,overcast,.83f);
        state.upper=mixColor(state.upper,overcast,.86f);
        state.horizon=mixColor(state.horizon,overcast,.84f);
        state.fog=mixColor(state.fog,overcast,.79f);
        state.cloudOpacity=.96f;
        state.rainIntensity=currentWeather==StorylandSkyWeather::Thunderstorm?1.0f:.75f;
        state.starIntensity=0.0f;
        state.worldFarClip=390.0f;
        state.worldFogStart=90.0f;
        state.directional=multiplyColor(state.directional,.44f);
        if(currentWeather==StorylandSkyWeather::Thunderstorm){
            const float strike=std::fmod(rainOffset,5.4f);
            state.lightningIntensity=strike<.08f?.9f:strike<.16f?.23f:0.0f;
        }
    } else if(currentWeather==StorylandSkyWeather::Foggy){
        state.zenith=mixColor(state.zenith,state.fog,.84f);
        state.upper=mixColor(state.upper,state.fog,.90f);
        state.horizon=mixColor(state.horizon,state.fog,.95f);
        state.cloudOpacity=.40f;
        state.worldFarClip=200.0f;
        state.worldFogStart=40.0f;
        state.starIntensity*=.12f;
    } else if(currentWeather==StorylandSkyWeather::Windy){
        state.cloudOpacity=.70f;
    }
    state.windStrength=currentWeather==StorylandSkyWeather::Windy?1.65f:
        currentWeather==StorylandSkyWeather::Thunderstorm?2.0f:
        currentWeather==StorylandSkyWeather::Rainy?.85f:.26f;
    state.windX=std::cos(windDirectionDegrees*Pi/180.0f)*state.windStrength;
    state.windY=std::sin(windDirectionDegrees*Pi/180.0f)*state.windStrength;
    return state;
}

void StorylandSky::evaluateState() {
    if (sanAndreasStyle) {
        currentState=evaluateSanAndreasState();
        return;
    }
    const auto& nativeTimecycle = nativeTimecycles[currentGame == StorylandSkyGame::Vcs ? 1u : 0u];
    if(currentGame==StorylandSkyGame::Manhunt2) {
        currentState=evaluateManhunt2WeatherState();
        return;
    }
    if ((nativeTimecycle.isLoaded() && nativeTimecycle.isVcs()==(currentGame==StorylandSkyGame::Vcs)) ||
        (currentGame == StorylandSkyGame::Vcs && vcsTimecycleLoaded)) {
        currentState = evaluateVcsTimecycleState();
        float sunAngle = (currentHour - 6.0f) / 24.0f * Pi * 2.0f;
        currentState.sunDirectionX = std::cos(sunAngle) * 0.25f;
        currentState.sunDirectionY = -std::cos(sunAngle);
        currentState.sunDirectionZ = std::sin(sunAngle);
        if(currentGame==StorylandSkyGame::Vcs) {
            const auto direction=pspLighting?storylandPspSunDirection(currentHour):
                storylandVcsPs2SunDirection(currentHour);
            currentState.sunDirectionX=direction[0];currentState.sunDirectionY=direction[1];
            currentState.sunDirectionZ=direction[2];
        }
        currentState.moonDirectionX = -currentState.sunDirectionX;
        currentState.moonDirectionY = -currentState.sunDirectionY;
        currentState.moonDirectionZ = -currentState.sunDirectionZ;
        currentState.cloudOffset = cloudOffset;
        applyNativeWeatherEffects(currentState);
        return;
    }

    StorylandSkyState night;
    StorylandSkyState dawn;
    StorylandSkyState day;
    StorylandSkyState sunset;

    if (currentGame == StorylandSkyGame::Lcs) {
        night.zenith = {0.015f, 0.025f, 0.070f, 1.0f};
        night.upper = {0.035f, 0.055f, 0.110f, 1.0f};
        night.horizon = {0.090f, 0.105f, 0.145f, 1.0f};
        night.lower = {0.075f, 0.080f, 0.095f, 1.0f};
        night.fog = {0.080f, 0.085f, 0.105f, 1.0f};
        dawn.zenith = {0.130f, 0.220f, 0.360f, 1.0f};
        dawn.upper = {0.340f, 0.380f, 0.490f, 1.0f};
        dawn.horizon = {0.780f, 0.500f, 0.390f, 1.0f};
        dawn.lower = {0.390f, 0.350f, 0.340f, 1.0f};
        dawn.fog = {0.500f, 0.430f, 0.420f, 1.0f};
        day.zenith = {0.180f, 0.390f, 0.650f, 1.0f};
        day.upper = {0.330f, 0.520f, 0.720f, 1.0f};
        day.horizon = {0.620f, 0.680f, 0.720f, 1.0f};
        day.lower = {0.520f, 0.560f, 0.580f, 1.0f};
        day.fog = {0.570f, 0.610f, 0.640f, 1.0f};
        sunset.zenith = {0.150f, 0.220f, 0.390f, 1.0f};
        sunset.upper = {0.420f, 0.330f, 0.430f, 1.0f};
        sunset.horizon = {0.850f, 0.410f, 0.260f, 1.0f};
        sunset.lower = {0.450f, 0.300f, 0.280f, 1.0f};
        sunset.fog = {0.560f, 0.360f, 0.320f, 1.0f};
    } else {
        night.zenith = {0.020f, 0.020f, 0.080f, 1.0f};
        night.upper = {0.055f, 0.045f, 0.130f, 1.0f};
        night.horizon = {0.160f, 0.095f, 0.175f, 1.0f};
        night.lower = {0.105f, 0.070f, 0.125f, 1.0f};
        night.fog = {0.115f, 0.080f, 0.135f, 1.0f};
        dawn.zenith = {0.190f, 0.250f, 0.500f, 1.0f};
        dawn.upper = {0.480f, 0.360f, 0.540f, 1.0f};
        dawn.horizon = {0.930f, 0.500f, 0.350f, 1.0f};
        dawn.lower = {0.520f, 0.300f, 0.340f, 1.0f};
        dawn.fog = {0.620f, 0.400f, 0.410f, 1.0f};
        day.zenith = {0.110f, 0.420f, 0.780f, 1.0f};
        day.upper = {0.230f, 0.580f, 0.820f, 1.0f};
        day.horizon = {0.630f, 0.780f, 0.840f, 1.0f};
        day.lower = {0.480f, 0.650f, 0.700f, 1.0f};
        day.fog = {0.560f, 0.690f, 0.730f, 1.0f};
        sunset.zenith = {0.190f, 0.180f, 0.430f, 1.0f};
        sunset.upper = {0.550f, 0.260f, 0.460f, 1.0f};
        sunset.horizon = {1.000f, 0.420f, 0.170f, 1.0f};
        sunset.lower = {0.560f, 0.220f, 0.270f, 1.0f};
        sunset.fog = {0.690f, 0.320f, 0.300f, 1.0f};
    }

    night.ambient = multiplyColor(night.horizon, 0.30f);
    dawn.ambient = multiplyColor(dawn.horizon, 0.42f);
    day.ambient = multiplyColor(day.horizon, 0.62f);
    sunset.ambient = multiplyColor(sunset.horizon, 0.46f);
    night.objectAmbient = night.ambient;
    dawn.objectAmbient = dawn.ambient;
    day.objectAmbient = day.ambient;
    sunset.objectAmbient = sunset.ambient;
    for(auto* state:{&night,&dawn,&day,&sunset}) {
        state->ambientBlur=state->ambient;
        state->objectAmbientBlur=state->objectAmbient;
    }
    night.directional = {0.22f, 0.24f, 0.34f, 1.0f};
    dawn.directional = {0.70f, 0.56f, 0.48f, 1.0f};
    day.directional = {0.95f, 0.92f, 0.84f, 1.0f};
    sunset.directional = {0.90f, 0.50f, 0.35f, 1.0f};
    night.sunCore = {0.0f, 0.0f, 0.0f, 0.0f};
    dawn.sunCore = {1.0f, 0.82f, 0.56f, 0.82f};
    day.sunCore = {1.0f, 0.96f, 0.82f, 0.86f};
    sunset.sunCore = {1.0f, 0.62f, 0.32f, 0.84f};
    night.sunCorona = {0.0f, 0.0f, 0.0f, 0.0f};
    dawn.sunCorona = {1.0f, 0.65f, 0.42f, 0.48f};
    day.sunCorona = {1.0f, 0.90f, 0.68f, 0.42f};
    sunset.sunCorona = {1.0f, 0.44f, 0.20f, 0.52f};
    night.moon = {0.72f, 0.78f, 1.0f, 0.78f};
    dawn.moon = {0.55f, 0.58f, 0.70f, 0.12f};
    day.moon = {0.0f, 0.0f, 0.0f, 0.0f};
    sunset.moon = {0.45f, 0.48f, 0.62f, 0.10f};

    StorylandSkyState first;
    StorylandSkyState second;
    float amount = 0.0f;
    if (currentHour < 5.0f) {
        first = second = night;
    } else if (currentHour < 7.0f) {
        first = night;
        second = dawn;
        amount = smoothStep(5.0f, 7.0f, currentHour);
    } else if (currentHour < 9.0f) {
        first = dawn;
        second = day;
        amount = smoothStep(7.0f, 9.0f, currentHour);
    } else if (currentHour < 17.0f) {
        first = second = day;
    } else if (currentHour < 20.0f) {
        first = day;
        second = sunset;
        amount = smoothStep(17.0f, 20.0f, currentHour);
    } else if (currentHour < 22.0f) {
        first = sunset;
        second = night;
        amount = smoothStep(20.0f, 22.0f, currentHour);
    } else {
        first = second = night;
    }
    currentState = interpolateState(first, second, amount);

    float nightAmount = std::max(1.0f - smoothStep(5.0f, 8.0f, currentHour),
                                 smoothStep(19.0f, 22.0f, currentHour));
    currentState.starIntensity = nightAmount;
    currentState.cloud = {1.0f, 1.0f, 1.0f, 1.0f};
    currentState.cloudOpacity = 0.22f;
    currentState.previewFogStart = 4.0f;
    currentState.previewFarClip = 12.0f;
    currentState.worldFogStart = 200.0f;
    currentState.worldFarClip = 800.0f;

    float sunAngle = (currentHour - 6.0f) / 24.0f * Pi * 2.0f;
    currentState.sunDirectionX = std::cos(sunAngle) * 0.25f;
    currentState.sunDirectionY = -std::cos(sunAngle);
    currentState.sunDirectionZ = std::sin(sunAngle);
    if(currentGame==StorylandSkyGame::Vcs) {
        const auto direction=pspLighting?storylandPspSunDirection(currentHour):
            storylandVcsPs2SunDirection(currentHour);
        currentState.sunDirectionX=direction[0];currentState.sunDirectionY=direction[1];
        currentState.sunDirectionZ=direction[2];
    }
    currentState.moonDirectionX = -currentState.sunDirectionX;
    currentState.moonDirectionY = -currentState.sunDirectionY;
    currentState.moonDirectionZ = -currentState.sunDirectionZ;

    StorylandSkyColor cloudy = {0.42f, 0.46f, 0.50f, 1.0f};
    if (currentWeather == StorylandSkyWeather::Cloudy) {
        currentState.zenith = mixColor(currentState.zenith, cloudy, 0.42f);
        currentState.upper = mixColor(currentState.upper, cloudy, 0.48f);
        currentState.horizon = mixColor(currentState.horizon, cloudy, 0.52f);
        currentState.fog = mixColor(currentState.fog, cloudy, 0.55f);
        currentState.cloudOpacity = 0.70f;
        currentState.worldFogStart = 145.0f;
        currentState.worldFarClip = 600.0f;
    } else if (currentWeather == StorylandSkyWeather::Rainy) {
        StorylandSkyColor rain = {0.25f, 0.29f, 0.33f, 1.0f};
        currentState.zenith = mixColor(currentState.zenith, rain, 0.72f);
        currentState.upper = mixColor(currentState.upper, rain, 0.76f);
        currentState.horizon = mixColor(currentState.horizon, rain, 0.78f);
        currentState.fog = mixColor(currentState.fog, rain, 0.82f);
        currentState.cloudOpacity = 0.95f;
        currentState.starIntensity = 0.0f;
        currentState.worldFogStart = 80.0f;
        currentState.worldFarClip = 350.0f;
    } else if (currentWeather == StorylandSkyWeather::Foggy) {
        currentState.zenith = mixColor(currentState.zenith, currentState.fog, 0.70f);
        currentState.upper = mixColor(currentState.upper, currentState.fog, 0.76f);
        currentState.horizon = mixColor(currentState.horizon, currentState.fog, 0.85f);
        currentState.cloudOpacity = 0.35f;
        currentState.starIntensity *= 0.20f;
        currentState.worldFogStart = 35.0f;
        currentState.worldFarClip = 180.0f;
    }
    currentState.cloudOffset = cloudOffset;
    applyNativeWeatherEffects(currentState);
}

void StorylandSky::drawBackground(const StorylandSkyRotation& viewRotation, float radius,
    bool useCurrentView, float nativeFieldOfView) const {
    if (!enabled) return;
    if(!std::isfinite(radius)||radius<=0)radius=100;
    std::array<float,16> cameraView{},cameraProjection{};
    glGetFloatv(GL_PROJECTION_MATRIX,cameraProjection.data());
    if(useCurrentView) {
        glGetFloatv(GL_MODELVIEW_MATRIX,cameraView.data());
        cameraView[12]=cameraView[13]=cameraView[14]=0;
        cameraView[3]=cameraView[7]=cameraView[11]=0;cameraView[15]=1;
    }

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                 GL_CURRENT_BIT | GL_LIGHTING_BIT | GL_POLYGON_BIT |
                 GL_TEXTURE_BIT | GL_POINT_BIT | GL_TRANSFORM_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glDepthMask(GL_FALSE);
    glShadeModel(GL_SMOOTH);
    // World UV animation can leave a transform on GL_TEXTURE. Native flares
    // must sample their own unmodified 0..1 raster coordinates.
    glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);

    // Atmosphere stays at background depth. Gameplay colours follow world
    // elevation, including camera pitch and roll, without a visible dome mesh.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    struct ScreenBand {
        float y;
        StorylandSkyColor color;
    };

    const std::array<ScreenBand, 9> bands = {{
        {-1.00f, currentState.lower},
        {-0.56f, currentState.lower},
        {-0.30f, mixColor(currentState.lower, currentState.horizon, 0.70f)},
        {-0.10f, currentState.horizon},
        { 0.18f, currentState.horizon},
        { 0.40f, mixColor(currentState.horizon, currentState.upper, 0.55f)},
        { 0.62f, currentState.upper},
        { 0.82f, currentState.zenith},
        { 1.00f, currentState.zenith}
    }};

    glDisable(GL_BLEND);
    if(useCurrentView) {
        const auto elevationForBand=[](float y) {
            const float height=y*.7002075382f;
            return height/std::hypot(height,1.0f);
        };
        const auto colorAt=[&](float elevation) {
            for(size_t index=1;index<bands.size();++index) {
                const float lower=elevationForBand(bands[index-1].y);
                const float upper=elevationForBand(bands[index].y);
                if(elevation<=upper)return mixColor(bands[index-1].color,bands[index].color,
                    (elevation-lower)/std::max(.000001f,upper-lower));
            }
            return bands.back().color;
        };
        constexpr unsigned Columns=24,Rows=20;
        for(unsigned row=0;row<Rows;++row) {
            glBegin(GL_QUAD_STRIP);
            for(unsigned column=0;column<=Columns;++column)for(unsigned edge=0;edge<2;++edge) {
                const float x=-1+2.0f*column/Columns,y=-1+2.0f*(row+edge)/Rows;
                setColor(colorAt(storylandSkyViewElevation(cameraView,cameraProjection[0],cameraProjection[5],x,y)));
                glVertex2f(x,y);
            }
            glEnd();
        }
    } else {
        glBegin(GL_QUAD_STRIP);
        for(const ScreenBand& band:bands) {
            setColor(band.color);glVertex2f(-1,band.y);glVertex2f(1,band.y);
        }
        glEnd();
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // Celestial directions use the same rotation as the world camera. Their
    // depth range is independent of script-controlled world near/far clipping.
    glMatrixMode(GL_PROJECTION);glPushMatrix();
    auto skyProjection=cameraProjection;
    if(std::fabs(skyProjection[11]+1)<.001f&&std::fabs(skyProjection[15])<.001f) {
        const float nearClip=.01f,farClip=std::max(1.0f,radius*4);
        skyProjection[10]=-(farClip+nearClip)/(farClip-nearClip);
        skyProjection[14]=-2*farClip*nearClip/(farClip-nearClip);
        glLoadMatrixf(skyProjection.data());
    }
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    if(useCurrentView)glMultMatrixf(cameraView.data());else multiplyQuaternion(viewRotation);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawStars(radius * 0.96f, currentState.starIntensity);

    if(currentGame==StorylandSkyGame::Vcs&&!pspLighting&&!sanAndreasStyle) {
        float moonView[16]{};
        glGetFloatv(GL_MODELVIEW_MATRIX,moonView);
        const auto moon=storylandVcsMoonSprite(moonView,cameraProjection.data(),nativeFieldOfView);
        const GLuint texture=currentState.moon.a>0&&moon.visible?
            GLuint(nativeFlareGpuTexture(2u)):0;
        if(texture) {
            glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
            glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
            glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
            glBlendFunc(GL_ONE,GL_ONE);setColor(currentState.moon);
            glBegin(GL_QUADS);
            glTexCoord2f(0,0);glVertex2f(moon.x-moon.halfWidth,moon.y+moon.halfHeight);
            glTexCoord2f(0,1);glVertex2f(moon.x-moon.halfWidth,moon.y-moon.halfHeight);
            glTexCoord2f(1,1);glVertex2f(moon.x+moon.halfWidth,moon.y-moon.halfHeight);
            glTexCoord2f(1,0);glVertex2f(moon.x+moon.halfWidth,moon.y+moon.halfHeight);
            glEnd();glDisable(GL_TEXTURE_2D);
            glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
        }
    } else {
    StorylandSkyColor moonGlow = currentState.moon;
    moonGlow.a = clampValue(currentState.moon.a * 0.34f, 0.0f, 1.0f);
    drawFeatheredDisc(currentState.moonDirectionX, currentState.moonDirectionY,
                      currentState.moonDirectionZ, radius * 0.92f,
                      currentState.moonSize * 2.35f, moonGlow,
                      0.58f, 0.18f);
    if(!drawNativeCelestial(currentState.moonDirectionX,currentState.moonDirectionY,
                            currentState.moonDirectionZ,radius*.92f,
                            currentState.moonSize,currentState.moon,2u)&&!hasNativeTimecycle())
        drawFeatheredDisc(currentState.moonDirectionX,currentState.moonDirectionY,
            currentState.moonDirectionZ,radius*.92f,currentState.moonSize,
            currentState.moon,.82f,.75f);
    }

    if (currentGame == StorylandSkyGame::Lcs || currentGame == StorylandSkyGame::Manhunt2) {
        drawCloudBands(radius * 0.90f, currentState);
    }

    if(!(currentGame==StorylandSkyGame::Vcs&&hasNativeTimecycle()&&!pspLighting)) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    StorylandSkyColor sunCorona = currentState.sunCorona;
    sunCorona.a = clampValue(currentState.sunCorona.a, 0.0f, 1.0f) * 0.62f;
    StorylandSkyColor sunCore = currentState.sunCore;
    sunCore.a = clampValue(currentState.sunCore.a, 0.0f, 1.0f);

    if (currentGame == StorylandSkyGame::Vcs && currentHour >= 4.5f && currentHour <= 7.0f) {
        // Retail VCS' dawn rows intentionally carry dark orange core/corona RGB.
        // With additive blending those values need useful alpha in the preview;
        // do not replace the RGB with a fabricated white/yellow sun.
        sunCorona.a = std::max(sunCorona.a, 0.44f);
        sunCore.a = std::max(sunCore.a, 0.82f);
    }

    // Sun corona and centre use the genuine GAME.DTZ `corona` and
    // `coronastar` sprites. The old procedural disc is only for previews
    // without an installed retail timecycle (not an in-game substitution).
    const bool nativeTimecycleActive=hasNativeTimecycle();
    if(!drawNativeCelestial(currentState.sunDirectionX,currentState.sunDirectionY,
                           currentState.sunDirectionZ,radius*.88f,
                           currentState.sunSize*2.40f,sunCorona,1u)&&!nativeTimecycleActive)
        drawFeatheredDisc(currentState.sunDirectionX,currentState.sunDirectionY,
            currentState.sunDirectionZ,radius*.88f,currentState.sunSize*2.40f,
            sunCorona,.58f,.16f);
    if(!drawNativeCelestial(currentState.sunDirectionX,currentState.sunDirectionY,
                           currentState.sunDirectionZ,radius*.88f,
                           currentState.sunSize,sunCore,0u)&&!nativeTimecycleActive)
        drawFeatheredDisc(currentState.sunDirectionX,currentState.sunDirectionY,
            currentState.sunDirectionZ,radius*.88f,currentState.sunSize,
            sunCore,.82f,.42f);

    // The native SUN flare ghosts sit on the screen-space line through the
    // celestial disc, not at arbitrary positions in the world. Opaque world
    // geometry drawn after the sky replaces the glow at occluded pixels.
    float sunView[16]{};
    glGetFloatv(GL_MODELVIEW_MATRIX,sunView);
    const StorylandSunFlareState sunFlare=storylandSunFlareAt(currentState,
        sunView,cameraProjection.data());
    size_t ghostCount=0;
    const auto ghosts=storylandSunFlareGhosts(currentState,sunFlare,ghostCount);
    if(ghostCount) {
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
        glOrtho(-1,1,-1,1,-1,1);
        glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        for(size_t i=0;i<ghostCount;++i) {
            const auto& ghost=ghosts[i];
            if(ghost.alpha<=0.0f||std::fabs(ghost.x)>1.15f||std::fabs(ghost.y)>1.15f)continue;
            // Original VCS and LCS corona raster, CLUT, and per-texel alpha.
            // If a texture is not in the retail DTZ, skip it; do not draw a circle.
            const GLuint texture=GLuint(nativeFlareGpuTexture(unsigned(ghost.texture)));
            if(!texture)continue;
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D,texture);
            glColor4f(ghost.color[0],ghost.color[1],ghost.color[2],ghost.alpha);
            const float x0=ghost.x-ghost.size,y0=ghost.y-ghost.size;
            const float x1=ghost.x+ghost.size,y1=ghost.y+ghost.size;
            glBegin(GL_QUADS);
            glTexCoord2f(0,1);glVertex2f(x0,y0);
            glTexCoord2f(1,1);glVertex2f(x1,y0);
            glTexCoord2f(1,0);glVertex2f(x1,y1);
            glTexCoord2f(0,0);glVertex2f(x0,y1);
            glEnd();
        }
        glDisable(GL_TEXTURE_2D);
        glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
    }

    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
    glDepthMask(GL_TRUE);
    glMatrixMode(GL_TEXTURE);glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

void StorylandSky::drawNativeSunCoronas(bool clearLine,uint32_t milliseconds,float nativeTimeStep,
    bool interior) const {
    if(!enabled||currentGame!=StorylandSkyGame::Vcs||!hasNativeTimecycle()||pspLighting)return;
    std::array<float,16> view{},projection{};
    glGetFloatv(GL_MODELVIEW_MATRIX,view.data());
    glGetFloatv(GL_PROJECTION_MATRIX,projection.data());
    std::array<float,3> camera{};
    for(size_t axis=0;axis<3;++axis)
        camera[axis]=-(view[axis*4]*view[12]+view[axis*4+1]*view[13]+view[axis*4+2]*view[14]);
    const auto byteColor=[](const StorylandSkyColor& color) {
        return std::array<uint8_t,3>{uint8_t(std::clamp(color.r*255.0f,0.0f,255.0f)),
            uint8_t(std::clamp(color.g*255.0f,0.0f,255.0f)),
            uint8_t(std::clamp(color.b*255.0f,0.0f,255.0f))};
    };
    size_t registrationCount=0;
    const auto registrations=storylandVcsSunRegistrations(
        {currentState.sunDirectionX,currentState.sunDirectionY,currentState.sunDirectionZ},camera,
        byteColor(currentState.sunCore),byteColor(currentState.sunCorona),currentState.nativeSunSize,
        currentState.spriteSize,extraColourWeight!=0,interior,false,registrationCount);
    if(!nativeSunInitialized||milliseconds<nativeSunMilliseconds) {
        nativeSunAlpha={};nativeSunRegistrations={};nativeSunInitialized=false;
    }
    for(size_t index=0;index<registrationCount;++index)nativeSunRegistrations[index]=registrations[index];
    if(!nativeSunInitialized||milliseconds!=nativeSunMilliseconds) {
        const float change=std::isfinite(nativeTimeStep)?std::max(0.0f,nativeTimeStep)*15.0f:0.0f;
        for(size_t index=0;index<nativeSunAlpha.size();++index) {
            StorylandCoronaProjection projected;
            const bool visible=index<registrationCount&&
                storylandProjectCorona(registrations[index].position,view.data(),projection.data(),projected,
                    registrations[index].range,registrations[index].pullAway)&&projected.onScreen&&
                (!registrations[index].checkObstacles||clearLine);
            const float target=visible?255.0f:0.0f;
            if(nativeSunAlpha[index]<target)
                nativeSunAlpha[index]=uint8_t(std::min(target,float(nativeSunAlpha[index])+change));
            else if(nativeSunAlpha[index]>target)
                nativeSunAlpha[index]=uint8_t(std::max(target,float(nativeSunAlpha[index])-change));
        }
        nativeSunMilliseconds=milliseconds;nativeSunInitialized=true;
    }
    size_t spriteCount=0;
    const auto sprites=storylandVcsSunSprites(nativeSunRegistrations,nativeSunRegistrations.size(),nativeSunAlpha,
        view.data(),projection.data(),spriteCount,nativeWeatherPrevious==3u?1.0f:0.0f);
    if(!spriteCount)return;
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_CURRENT_BIT|
        GL_TEXTURE_BIT|GL_TRANSFORM_BIT);
    glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);glDepthMask(GL_FALSE);glDepthFunc(GL_LEQUAL);
    glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    for(size_t index=0;index<spriteCount;++index) {
        const auto& sprite=sprites[index];
        const GLuint texture=GLuint(nativeFlareGpuTexture(sprite.texture));
        if(!texture)continue;
        if(sprite.depthTest)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
        glColor4f(sprite.color[0],sprite.color[1],sprite.color[2],1);
        const float cosine=std::cos(sprite.rotation),sine=std::sin(sprite.rotation);
        glBegin(GL_QUADS);
        for(const auto& corner:std::array<std::array<float,2>,4>{{{-1,-1},{1,-1},{1,1},{-1,1}}}) {
            const float x=sprite.halfWidth*(corner[0]*cosine-corner[1]*sine);
            const float y=sprite.halfHeight*(corner[0]*sine+corner[1]*cosine);
            glTexCoord2f((corner[0]+1)*.5f,(1-corner[1])*.5f);
            glVertex3f(sprite.x+x,sprite.y+y,sprite.depth);
        }
        glEnd();
    }
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();
    glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

void StorylandSky::drawWeatherOverlay() const {
    if(!enabled || (currentState.rainIntensity<=0.0f && currentState.snowIntensity<=0.0f &&
       currentState.lightningIntensity<=0.0f)) return;
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|
        GL_CURRENT_BIT|GL_LIGHTING_BIT|GL_TEXTURE_BIT|GL_TRANSFORM_BIT);
    glDisable(GL_DEPTH_TEST);glDisable(GL_FOG);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-1.0,1.0,-1.0,1.0,-1.0,1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    if(currentState.rainIntensity>0.0f) {
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        for(unsigned drop=0;drop<240;++drop) {
            const float speed=0.72f+float(drop%7)*0.09f;
            const float px=std::fmod(float(drop)*0.6180339f+0.07f,1.0f);
            const float py=std::fmod(float(drop)*0.4142135f-currentState.rainOffset*speed+32.0f,1.0f);
            const float x=px*2.0f-1.0f,y=py*2.0f-1.0f;
            const float length=0.028f+float(drop%5)*0.008f;
            const float slant=currentState.windX*0.025f;
            glColor4f(0.64f,0.70f,0.77f,currentState.rainIntensity*(0.16f+float(drop%4)*0.045f));
            glVertex2f(x,y);
            glVertex2f(x-slant,y-length);
        }
        glEnd();
    }
    if (currentState.snowIntensity > 0.0f) {
        glPointSize(2.0f);
        glColor4f(0.90f,0.94f,1.0f,0.65f);
        glBegin(GL_POINTS);
        for (unsigned flake=0;flake<160;++flake) {
            const float x=std::fmod(float(flake)*0.6180339f+32.0f+currentState.windX*rainOffset*0.02f,1.0f)*2.0f-1.0f;
            const float y=std::fmod(float(flake)*0.4142135f+32.0f-rainOffset*0.12f,1.0f)*2.0f-1.0f;
            glVertex2f(x,y);
        }
        glEnd();
    }
    if(currentState.lightningIntensity>0.0f) {
        glColor4f(0.73f,0.79f,0.90f,currentState.lightningIntensity*0.45f);
        glBegin(GL_QUADS);
        glVertex2f(-1.0f,-1.0f);glVertex2f(1.0f,-1.0f);
        glVertex2f(1.0f,1.0f);glVertex2f(-1.0f,1.0f);
        glEnd();
    }
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glDepthMask(GL_TRUE);
    glPopAttrib();
}
