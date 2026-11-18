#include "src/storyland/storyland_play_rubbish.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float Pi=3.14159265358979323846f;
constexpr float TwoPi=2.0f*Pi;
constexpr float MaximumDistance=20.0f;
constexpr float FadeDistance=16.5f;
}

const char* storylandRubbishTextureName(StorylandRubbishKind kind) {
    switch(kind) {
    case StorylandRubbishKind::NewspaperA: return "newspaper01_64";
    case StorylandRubbishKind::NewspaperB: return "newspaper02_64";
    case StorylandRubbishKind::LeafA: return "gameleaf01_64";
    case StorylandRubbishKind::LeafB: return "gameleaf02_64";
    case StorylandRubbishKind::Branch: return "";
    }
    return "";
}

float StorylandPlayRubbish::random01() {
    randomState^=randomState<<13;
    randomState^=randomState>>17;
    randomState^=randomState<<5;
    return float((randomState>>8)&0xFFFFFFu)/16777216.0f;
}

void StorylandPlayRubbish::reset() {
    litter.clear();
    spawnAccumulator=0.0f;
    randomState=0xA2F9B413u;
}

void StorylandPlayRubbish::beginHop(StorylandRubbishSheet& sheet,float dx,float dy,
                                    float height,float duration) {
    if(!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(height)||
       !std::isfinite(duration)||duration<=0.0f)return;
    sheet.origin=sheet.position;
    sheet.destination={sheet.position.x+dx,sheet.position.y+dy,sheet.position.z};
    sheet.hopHeight=std::clamp(height,0.05f,1.5f);
    sheet.hopDuration=std::clamp(duration,0.2f,2.8f);
    sheet.hopTime=0.0f;
    sheet.spin=(random01()-.5f)*(3.0f+std::hypot(dx,dy));
    sheet.flying=true;
}

bool StorylandPlayRubbish::spawn(const StorylandNativePoint& focus,const GroundHeight& ground) {
    if(litter.size()>=MaximumSheets||!ground)return false;
    // Do not spawn at the player's feet. An annulus reduces popping while the
    // camera is looking toward the character.
    const float angle=random01()*TwoPi;
    const float radius=4.5f+random01()*(MaximumDistance-6.5f);
    StorylandNativePoint candidate{focus.x+std::cos(angle)*radius,
        focus.y+std::sin(angle)*radius,focus.z};
    float height=0.0f;
    if(!ground(candidate,height)||!std::isfinite(height)||
       std::fabs(height-focus.z)>5.0f)return false;
    candidate.z=height+.035f;
    StorylandRubbishSheet sheet;
    const float kindRoll=random01();
    sheet.kind=kindRoll<.25f?StorylandRubbishKind::NewspaperA:
        kindRoll<.48f?StorylandRubbishKind::NewspaperB:
        kindRoll<.69f?StorylandRubbishKind::LeafA:
        kindRoll<.91f?StorylandRubbishKind::LeafB:
        StorylandRubbishKind::Branch;
    const bool newspaper=sheet.kind==StorylandRubbishKind::NewspaperA||
        sheet.kind==StorylandRubbishKind::NewspaperB;
    const bool branch=sheet.kind==StorylandRubbishKind::Branch;
    sheet.width=newspaper?.35f+random01()*.12f:branch?.26f:.11f+random01()*.08f;
    sheet.height=newspaper?.24f+random01()*.08f:branch?.06f:.13f+random01()*.05f;
    sheet.yaw=random01()*TwoPi;
    sheet.cooldown=.45f+random01()*2.5f;
    sheet.origin=sheet.destination=sheet.position=candidate;
    sheet.opacity=0.0f;
    litter.push_back(sheet);
    return true;
}

size_t StorylandPlayRubbish::movingCount() const {
    size_t count=0;
    for(const auto& sheet:litter)if(sheet.flying)++count;
    return count;
}

void StorylandPlayRubbish::update(const StorylandNativePoint& focus,
    const StorylandNativePoint& wind,const std::vector<StorylandRubbishVehicle>& vehicles,
    float seconds,const GroundHeight& ground) {
    if(!enabled||!ground||!std::isfinite(seconds)||seconds<=0.0f)return;
    const float dt=std::min(seconds,0.10f);
    const float windSpeed=std::clamp(std::hypot(wind.x,wind.y),0.0f,4.0f);
    struct PreparedWake {const StorylandRubbishVehicle* car;float cosine,sine;};
    std::vector<PreparedWake> wakes;
    wakes.reserve(vehicles.size());
    for(const auto& car:vehicles) {
        if(!std::isfinite(car.direction)||!std::isfinite(car.speed)||car.speed<2.0f)continue;
        wakes.push_back({&car,std::cos(car.direction),std::sin(car.direction)});
    }
    litter.erase(std::remove_if(litter.begin(),litter.end(),[&](const auto& sheet) {
        return std::hypot(sheet.position.x-focus.x,sheet.position.y-focus.y)>MaximumDistance+2.5f;
    }),litter.end());
    spawnAccumulator+=dt;
    // Bounded population and amortized terrain probes. Ground queries happen
    // at spawn and landing, not at every animated intermediate hop position.
    unsigned attempts=0;
    while(spawnAccumulator>=.13f&&litter.size()<MaximumSheets&&attempts<2) {
        spawnAccumulator-=.13f;
        spawn(focus,ground);
        ++attempts;
    }
    spawnAccumulator=std::min(spawnAccumulator,.65f);
    for(auto& sheet:litter) {
        const float distance=std::hypot(sheet.position.x-focus.x,sheet.position.y-focus.y);
        const float fade=std::clamp((MaximumDistance-distance)/
            (MaximumDistance-FadeDistance),0.0f,1.0f);
        sheet.opacity=std::min(fade,sheet.opacity+dt*2.5f);
        sheet.cooldown=std::max(0.0f,sheet.cooldown-dt);
        if(sheet.flying) {
            sheet.hopTime+=dt;
            const float t=std::clamp(sheet.hopTime/sheet.hopDuration,0.0f,1.0f);
            const float along=t*t*(3.0f-2.0f*t);
            sheet.position.x=sheet.origin.x+(sheet.destination.x-sheet.origin.x)*along;
            sheet.position.y=sheet.origin.y+(sheet.destination.y-sheet.origin.y)*along;
            sheet.position.z=sheet.origin.z+sheet.hopHeight*std::sin(Pi*t);
            sheet.yaw+=sheet.spin*dt;
            sheet.roll=std::sin(Pi*t)*(.1f+.5f*sheet.hopHeight);
            if(t>=1.0f) {
                float height=0.0f;
                if(ground(sheet.destination,height)&&std::isfinite(height)&&
                   std::fabs(height-focus.z)<=5.0f) {
                    sheet.position.z=height+.035f;
                    sheet.origin=sheet.destination=sheet.position;
                } else {
                    // An invalid landing is removed rather than positioned
                    // through an unloaded or nonexistent ground collision.
                    sheet.opacity=0.0f;
                    sheet.position.x=focus.x+MaximumDistance+10.0f;
                }
                sheet.flying=false;
                sheet.roll=0.0f;
                sheet.cooldown=.3f+random01()*1.3f;
            }
            continue;
        }
        if(sheet.cooldown>0.0f)continue;
        bool stirred=false;
        // Vehicle wake begins behind the vehicle as in the original PS2
        // litter behavior, but is computed in world-space metres per second.
        for(const auto& wake:wakes) {
            const auto& car=*wake.car;
            if(std::fabs(car.position.z-sheet.position.z)>2.0f)continue;
            const float dx=sheet.position.x-car.position.x;
            const float dy=sheet.position.y-car.position.y;
            const float cs=wake.cosine,sn=wake.sine;
            const float behind=dx*cs+dy*sn;
            const float side=std::fabs(-dx*sn+dy*cs);
            if(behind>-.8f*car.halfLength||behind<-(car.halfLength+3.0f)||
                side>car.halfWidth+1.0f)continue;
            const float impulse=std::clamp(car.speed*.19f,0.8f,3.6f);
            beginHop(sheet,cs*impulse+(random01()-.5f)*.5f,
                sn*impulse+(random01()-.5f)*.5f,
                .2f+std::min(.9f,car.speed*.04f),.65f+random01()*.8f);
            stirred=true;
            break;
        }
        if(stirred)continue;
        // Wind increases both the chance and length of a hop. Calm weather
        // leaves most rubbish resting on the ground.
        if(random01()<dt*(.018f+windSpeed*.25f)) {
            const float direction=windSpeed>.01f?std::atan2(wind.y,wind.x):random01()*TwoPi;
            const float length=.4f+windSpeed*(.4f+random01()*.85f);
            beginHop(sheet,std::cos(direction)*length+(random01()-.5f)*.35f,
                std::sin(direction)*length+(random01()-.5f)*.35f,
                .08f+windSpeed*.19f+random01()*.2f,
                .7f+random01()*1.1f);
        }
    }
}
