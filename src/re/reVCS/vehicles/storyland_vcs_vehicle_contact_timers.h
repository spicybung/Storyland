#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace StorylandVcsVehicleContactTimers {

inline float advance(float previous, float contactFraction, float nativeTimeStep) {
    if (!std::isfinite(contactFraction) || !std::isfinite(nativeTimeStep) || nativeTimeStep < 0.0f)
        return 0.0f;
    if (contactFraction < 1.0f)
        return 4.0f;
    if (!std::isfinite(previous))
        return 0.0f;
    return std::max(0.0f, previous - nativeTimeStep);
}

inline void advanceAll(std::array<float, 4>& timers,
    const std::array<float, 4>& fractions, float nativeTimeStep) {
    for (size_t index = 0; index < timers.size(); ++index)
        timers[index] = advance(timers[index], fractions[index], nativeTimeStep);
}

inline std::array<bool, 4> drivingContacts(const std::array<float, 4>& timers) {
    std::array<bool, 4> contacts{};
    for (size_t index = 0; index < contacts.size(); ++index)
        contacts[index] = timers[index] > 0.0f;
    return contacts;
}

struct ContactCounts {
    uint8_t total = 0;
    uint8_t driven = 0;
};

inline ContactCounts contactCounts(const std::array<float, 4>& timers, char driveType) {
    ContactCounts result;
    for (size_t index = 0; index < timers.size(); ++index) {
        if (!(timers[index] > 0.0f))
            continue;
        ++result.total;
        const bool front = index == 0 || index == 2;
        const bool driven = driveType == '4' ||
            (driveType == 'F' && front) ||
            (driveType == 'R' && !front);
        if (driven)
            ++result.driven;
    }
    return result;
}

inline unsigned tyreForceWheelCount(const std::array<float, 4>& timers,
    unsigned currentContacts, bool vcsAutomobile) {
    // 0x1535BC loads CAutomobile's retained contact count (+0x7FC) into
    // the wheel-count argument used by both ProcessWheel calls below it.
    return vcsAutomobile ? unsigned(contactCounts(timers, 0).total) : currentContacts;
}

inline bool bikeRearWheelContact(const std::array<float, 4>& timers) {
    // CBike sets its rear-wheel flag from the second pair of ray timers.
    return timers[2] > 0.0f || timers[3] > 0.0f;
}

inline size_t bikeSelectedContactLine(const std::array<float, 4>& fractions, size_t wheel) {
    const size_t first = wheel * 2;
    // CBike at 0x33A1AC and 0x33A2xx chooses the lower fraction, with the
    // second ray selected when the fractions are equal.
    return fractions[first] < fractions[first + 1] ? first : first + 1;
}

inline bool bikeTyreContactAvailable(const std::array<float, 4>& timers,
    const std::array<bool, 4>& historyValid, size_t wheel, size_t line) {
    if (wheel >= 2 || line / 2 != wheel) return false;
    const size_t first = wheel * 2;
    return historyValid[line] && (timers[first] > 0.0f || timers[first + 1] > 0.0f);
}

inline bool automobileTyreContactAvailable(const std::array<float, 4>& timers,
    const std::array<bool, 4>& historyValid, size_t line) {
    return line < timers.size() && historyValid[line] && timers[line] > 0.0f;
}

template <typename Point>
inline bool retainedContactNearby(const Point& point, const Point& rayStart,
    const Point& rayEnd, float speed) {
    if (!std::isfinite(speed)) return false;
    const float dx=rayEnd.x-rayStart.x, dy=rayEnd.y-rayStart.y, dz=rayEnd.z-rayStart.z;
    const float lengthSquared=dx*dx+dy*dy+dz*dz;
    if (!std::isfinite(lengthSquared) || lengthSquared<=0.000001f) return false;
    const float fraction=std::clamp(((point.x-rayStart.x)*dx+(point.y-rayStart.y)*dy+
        (point.z-rayStart.z)*dz)/lengthSquared,0.0f,1.0f);
    const float distance=std::hypot(point.x-(rayStart.x+dx*fraction),
        point.y-(rayStart.y+dy*fraction),point.z-(rayStart.z+dz*fraction));
    // Four native time units equal 80 ms. The extra margin permits rotation
    // and suspension travel, while discarding geometry left behind by a warp.
    return std::isfinite(distance) && distance<=2.0f+std::fabs(speed)*0.08f;
}

template <typename Point>
inline bool retainedContactNearVehicle(const Point& point, const Point& vehiclePosition,
    float halfLength, float speed) {
    if (!std::isfinite(halfLength)||!std::isfinite(speed)) return false;
    const float distance=std::hypot(point.x-vehiclePosition.x,point.y-vehiclePosition.y,
        point.z-vehiclePosition.z);
    return std::isfinite(distance)&&distance<=2.0f+std::max(0.0f,halfLength)+std::fabs(speed)*0.08f;
}

inline bool steeringContact(const std::array<float, 4>& timers, bool rearSteering) {
    // At 0x13E270 and 0x13E284, VCS tests timers 0 and 1, even though
    // its 0x13DE18 driving-axle loop groups F=0/2 and R=1/3.
    // The handling flag used for rear-wheel steering does not change that gate.
    (void)rearSteering;
    return timers[0] > 0.0f || timers[1] > 0.0f;
}

}
