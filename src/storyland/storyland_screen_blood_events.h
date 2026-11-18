#pragma once

#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>

struct StorylandScreenBloodView {
    std::array<float,3> eye{};
    std::array<float,3> forward{};
    bool valid=false;
};

inline StorylandScreenBloodView storylandMakeScreenBloodView(
    const std::array<float,3>& eye,const std::array<float,3>& target) {
    StorylandScreenBloodView view;
    view.eye=eye;
    float lengthSquared=0;
    for(std::size_t axis=0;axis<3;++axis) {
        if(!std::isfinite(eye[axis])||!std::isfinite(target[axis]))return view;
        view.forward[axis]=target[axis]-eye[axis];
        lengthSquared+=view.forward[axis]*view.forward[axis];
    }
    view.valid=lengthSquared>.000001f;
    return view;
}

inline bool storylandCriticalPlayerScreenBlood(bool enabled, bool vcs, uint8_t damageKind,
    bool bleeding, float previousHealth, float health, float maximumHealth, uint8_t hitPiece) {
    if(!enabled||!vcs||damageKind!=1||!bleeding||!std::isfinite(previousHealth)||
        !std::isfinite(health)||!std::isfinite(maximumHealth)||maximumHealth<=0||
        previousHealth<=0||health>=previousHealth)return false;
    return hitPiece==6||health<=0||health<=maximumHealth*.25f;
}

inline bool storylandCloseExecutionScreenBlood(bool enabled, bool vcs, bool localPlayer,
    uint8_t damageKind, bool headHit, const StorylandScreenBloodView& view,
    const std::array<float,3>& player, const std::array<float,3>& victim) {
    if(!enabled||!vcs||!localPlayer||!view.valid||
        (damageKind!=16&&!(damageKind==1&&headHit)))return false;
    float playerDistanceSquared=0,cameraDistanceSquared=0,depth=0,forwardSquared=0;
    for(std::size_t axis=0;axis<3;++axis) {
        if(!std::isfinite(player[axis])||!std::isfinite(victim[axis])||
            !std::isfinite(view.eye[axis])||!std::isfinite(view.forward[axis]))return false;
        const float playerDelta=victim[axis]-player[axis];
        const float cameraDelta=victim[axis]-view.eye[axis];
        playerDistanceSquared+=playerDelta*playerDelta;
        cameraDistanceSquared+=cameraDelta*cameraDelta;
        depth+=cameraDelta*view.forward[axis];
        forwardSquared+=view.forward[axis]*view.forward[axis];
    }
    return playerDistanceSquared<=2.7f*2.7f&&cameraDistanceSquared<=3.0f*3.0f&&
        forwardSquared>0.000001f&&depth>0&&
        depth*depth>=cameraDistanceSquared*forwardSquared*.25f;
}
