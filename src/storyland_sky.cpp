#define NOMINMAX

#include "storyland_sky.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
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

void drawCelestialDisc(float directionX, float directionY, float directionZ, float radius,
                       float size, const StorylandSkyColor& color) {
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

    constexpr int SegmentCount = 32;
    glBegin(GL_TRIANGLE_FAN);
    setColor(color);
    glVertex3f(centerX, centerY, centerZ);
    StorylandSkyColor edge = color;
    edge.a = 0.0f;
    for (int segment = 0; segment <= SegmentCount; ++segment) {
        float angle = float(segment) / float(SegmentCount) * Pi * 2.0f;
        float cosine = std::cos(angle);
        float sine = std::sin(angle);
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
    constexpr int SegmentCount = 96;
    constexpr int BandCount = 3;
    for (int band = 0; band < BandCount; ++band) {
        float lowerElevation = (11.0f + float(band) * 8.0f) * Pi / 180.0f;
        float upperElevation = lowerElevation + 5.0f * Pi / 180.0f;
        float phase = state.cloudOffset * Pi * 2.0f * (0.55f + float(band) * 0.18f) + float(band) * 1.7f;
        glBegin(GL_QUAD_STRIP);
        for (int segment = 0; segment <= SegmentCount; ++segment) {
            float angle = float(segment) / float(SegmentCount) * Pi * 2.0f;
            float noise = std::sin(angle * 3.0f + phase) * 0.45f +
                          std::sin(angle * 7.0f - phase * 1.8f) * 0.30f +
                          std::sin(angle * 13.0f + phase * 0.7f) * 0.25f;
            float opacity = clampValue((noise + 0.55f) * state.cloudOpacity * 0.42f, 0.0f, 0.72f);
            StorylandSkyColor lowerColor = state.cloud;
            StorylandSkyColor upperColor = state.cloud;
            lowerColor.a = opacity * 0.35f;
            upperColor.a = opacity;
            float lowerRingRadius = std::cos(lowerElevation) * radius;
            float upperRingRadius = std::cos(upperElevation) * radius;
            setColor(lowerColor);
            glVertex3f(std::cos(angle) * lowerRingRadius, std::sin(angle) * lowerRingRadius,
                       std::sin(lowerElevation) * radius);
            setColor(upperColor);
            glVertex3f(std::cos(angle) * upperRingRadius, std::sin(angle) * upperRingRadius,
                       std::sin(upperElevation) * radius);
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
    result.directional = mixColor(a.directional, b.directional, amount);
    result.sun = mixColor(a.sun, b.sun, amount);
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

}

StorylandSky::StorylandSky() {
    evaluateState();
}

void StorylandSky::setEnabled(bool value) { enabled = value; }
bool StorylandSky::isEnabled() const { return enabled; }

void StorylandSky::setGame(StorylandSkyGame value) {
    currentGame = value;
    evaluateState();
}

StorylandSkyGame StorylandSky::game() const { return currentGame; }

void StorylandSky::setWeather(StorylandSkyWeather value) {
    currentWeather = value;
    evaluateState();
}

StorylandSkyWeather StorylandSky::weather() const { return currentWeather; }

void StorylandSky::setTime(float hour) {
    while (hour < 0.0f) hour += 24.0f;
    while (hour >= 24.0f) hour -= 24.0f;
    currentHour = hour;
    evaluateState();
}

float StorylandSky::time() const { return currentHour; }

void StorylandSky::stepTime(float hours) { setTime(currentHour + hours); }

void StorylandSky::update(float elapsedSeconds) {
    cloudOffset += elapsedSeconds * 0.0025f;
    cloudOffset -= std::floor(cloudOffset);
    currentState.cloudOffset = cloudOffset;
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
        row.directional = byteColor(values, 12);
        row.skyTop = byteColor(values, 15);
        row.skyBottom = byteColor(values, 18);
        row.sunCore = byteColor(values, 21);
        row.sunCorona = byteColor(values, 24);
        row.sunSize = values[27];
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

const StorylandSkyState& StorylandSky::state() const { return currentState; }

StorylandSkyState StorylandSky::evaluateVcsTimecycleState() const {
    size_t weather = 0;
    if (currentWeather == StorylandSkyWeather::Cloudy) weather = 1;
    else if (currentWeather == StorylandSkyWeather::Rainy) weather = 2;
    else if (currentWeather == StorylandSkyWeather::Foggy) weather = 3;

    int firstHour = int(std::floor(currentHour)) % 24;
    int secondHour = (firstHour + 1) % 24;
    float amount = currentHour - std::floor(currentHour);
    const TimecycleRow& first = vcsTimecycleRows[weather][size_t(firstHour)];
    const TimecycleRow& second = vcsTimecycleRows[weather][size_t(secondHour)];

    StorylandSkyState result;
    result.zenith = mixColor(first.skyTop, second.skyTop, amount);
    result.horizon = mixColor(first.skyBottom, second.skyBottom, amount);
    result.upper = mixColor(result.horizon, result.zenith, 0.58f);
    result.lower = multiplyColor(result.horizon, 0.72f);
    result.fog = result.horizon;
    result.ambient = mixColor(first.ambient, second.ambient, amount);
    result.directional = mixColor(first.directional, second.directional, amount);
    result.sun = mixColor(first.sunCorona, second.sunCorona, amount);
    result.moon = {0.72f, 0.78f, 1.0f, 0.78f};
    StorylandSkyColor lowCloud = mixColor(first.lowCloud, second.lowCloud, amount);
    StorylandSkyColor topCloud = mixColor(first.topCloud, second.topCloud, amount);
    StorylandSkyColor bottomCloud = mixColor(first.bottomCloud, second.bottomCloud, amount);
    result.cloud = mixColor(mixColor(lowCloud, topCloud, 0.55f), bottomCloud, 0.25f);
    result.sunSize = clampValue((first.sunSize + (second.sunSize - first.sunSize) * amount) / 110.0f,
                                0.025f, 0.16f);
    result.worldFarClip = std::max(100.0f, first.farClip + (second.farClip - first.farClip) * amount);
    result.worldFogStart = std::max(0.0f, first.fogStart + (second.fogStart - first.fogStart) * amount);
    result.previewFogStart = 4.0f;
    result.previewFarClip = 12.0f;
    result.cloudOpacity = weather == 0 ? 0.24f : weather == 1 ? 0.72f : weather == 2 ? 0.95f : 0.38f;
    result.starIntensity = std::max(1.0f - smoothStep(5.0f, 8.0f, currentHour),
                                    smoothStep(19.0f, 22.0f, currentHour));
    if (weather == 2) result.starIntensity = 0.0f;
    else if (weather == 3) result.starIntensity *= 0.2f;
    return result;
}

void StorylandSky::evaluateState() {
    if (currentGame == StorylandSkyGame::Vcs && vcsTimecycleLoaded) {
        currentState = evaluateVcsTimecycleState();
        float sunAngle = (currentHour - 6.0f) / 24.0f * Pi * 2.0f;
        currentState.sunDirectionX = std::cos(sunAngle) * 0.25f;
        currentState.sunDirectionY = -std::cos(sunAngle);
        currentState.sunDirectionZ = std::sin(sunAngle);
        currentState.moonDirectionX = -currentState.sunDirectionX;
        currentState.moonDirectionY = -currentState.sunDirectionY;
        currentState.moonDirectionZ = -currentState.sunDirectionZ;
        currentState.cloudOffset = cloudOffset;
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
    night.directional = {0.22f, 0.24f, 0.34f, 1.0f};
    dawn.directional = {0.70f, 0.56f, 0.48f, 1.0f};
    day.directional = {0.95f, 0.92f, 0.84f, 1.0f};
    sunset.directional = {0.90f, 0.50f, 0.35f, 1.0f};
    night.sun = {0.0f, 0.0f, 0.0f, 0.0f};
    dawn.sun = {1.0f, 0.65f, 0.42f, 0.86f};
    day.sun = {1.0f, 0.90f, 0.68f, 0.88f};
    sunset.sun = {1.0f, 0.44f, 0.20f, 0.90f};
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
}

void StorylandSky::drawBackground(const StorylandSkyRotation& viewRotation, float radius) const {
    if (!enabled) return;

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                 GL_CURRENT_BIT | GL_LIGHTING_BIT | GL_POLYGON_BIT |
                 GL_TEXTURE_BIT | GL_POINT_BIT | GL_TRANSFORM_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glShadeModel(GL_SMOOTH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    multiplyQuaternion(viewRotation);

    std::array<SkyRing, 7> rings = {{
        {-89.5f, currentState.lower},
        {-15.0f, currentState.lower},
        {-2.5f, currentState.horizon},
        {10.0f, currentState.horizon},
        {30.0f, currentState.upper},
        {62.0f, currentState.zenith},
        {89.5f, currentState.zenith}
    }};
    glDisable(GL_BLEND);
    drawSkyRings(rings, radius);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawStars(radius * 0.96f, currentState.starIntensity);
    drawCelestialDisc(currentState.moonDirectionX, currentState.moonDirectionY,
                      currentState.moonDirectionZ, radius * 0.92f,
                      currentState.moonSize, currentState.moon);
    drawCloudBands(radius * 0.90f, currentState);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    drawCelestialDisc(currentState.sunDirectionX, currentState.sunDirectionY,
                      currentState.sunDirectionZ, radius * 0.88f,
                      currentState.sunSize, currentState.sun);

    glPopMatrix();
    glDepthMask(GL_TRUE);
    glPopAttrib();
}
