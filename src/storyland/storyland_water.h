#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

struct StorylandSwimmingParticleState {
    float previousX=0, previousY=0, previousZ=0;
    float strokeTime=0, rippleTime=0, wakeDistance=0,wakeTime=0;
    uint32_t strokeIndex=0,wakeIndex=0;
    std::array<std::array<float,3>,4> limbPositions{};
    uint8_t limbSampleMask=0,limbStateMask=0,limbUnderwaterMask=0;
    bool positioned=false;
};

struct StorylandCharacterWaterState {
    bool active=false, drowning=false;
    float air=30.0f, time=0, verticalSpeed=0;
    bool diving=false, underwater=false, diveStroke=false;
    float diveTime=0, divePitch=0, diveAnimationTime=0;
    StorylandSwimmingParticleState particles;
};

struct StorylandWaterCharacterResult {
    float height=0, damage=0;
    bool entered=false;
};

inline bool storylandWaterIsDeep(float feet, float surface, bool hasFloor, float floor) {
    return std::isfinite(feet)&&std::isfinite(surface)&&feet<=surface-0.65f&&
        (!hasFloor||floor<surface-0.7f);
}

inline StorylandWaterCharacterResult storylandStepWaterCharacter(
    StorylandCharacterWaterState& state, float feet, float surface, bool deep,
    bool canSwim, bool sprinting, bool pedestrian, float seconds,
    bool nativeVcs=false, float horizontalSpeed=0, int pedestrianType=0,
    bool canDrown=true, bool infiniteSwim=false) {
    StorylandWaterCharacterResult result{feet,0,false};
    if(!std::isfinite(seconds)||seconds<=0)return result;
    const float duration=std::min(seconds,0.1f);
    if(!deep||!std::isfinite(surface)) {
        if(nativeVcs) {
            state.active=false;state.drowning=false;state.verticalSpeed=0;
            state.diving=false;state.underwater=false;state.diveStroke=false;
            state.diveTime=0;state.divePitch=0;state.diveAnimationTime=0;
            state.particles={};
            state.air=std::min(30.0f,state.air+duration*50.0f*.002f*30.0f);
        } else state={};
        return result;
    }
    if(!state.active) {
        if(!nativeVcs) {state={};state.air=pedestrian?5.0f:30.0f;}
        state.active=true;state.time=0;
        result.entered=true;
    }
    const int steps=std::max(1,int(std::ceil(duration*60.0f)));
    const float dt=duration/float(steps);
    for(int index=0;index<steps;++index) {
        state.time+=dt;
        if(nativeVcs) {
            if(!(infiniteSwim&&pedestrianType==0)) {
                const float nativeSpeed=std::isfinite(horizontalSpeed)?std::fabs(horizontalSpeed)/50.0f:0;
                state.air-=dt*50.0f*(nativeSpeed*.005f+.00005f)*30.0f*
                    (pedestrianType==6?3.0f:1.0f);
                if(state.air<=0)state.air=canDrown?0.0f:.03f;
                state.drowning=canDrown&&state.air<=0;
            }
        } else {
            state.air=std::max(0.0f,state.air-dt*(sprinting&&!pedestrian?2.0f:1.0f));
            state.drowning=(!canSwim&&!pedestrian)||state.air<=0;
        }
        const float target=surface-(state.drowning?2.0f:1.0f);
        state.verticalSpeed+=(target-result.height)*8.0f*dt;
        state.verticalSpeed*=std::exp(-4.0f*dt);
        state.verticalSpeed=std::clamp(state.verticalSpeed,-2.5f,2.0f);
        result.height+=state.verticalSpeed*dt;
        if(state.drowning)result.damage+=dt*(nativeVcs?150.0f:pedestrian?25.0f:20.0f);
    }
    return result;
}

struct StorylandVehicleWaterState {
    float immersion=0;
    bool engineFlooded=false;
    bool buoyant=false; // Derived contact state; recomputed by the live water solver.
};

inline bool storylandStepWaterVehicle(StorylandVehicleWaterState& state,
    float& height, float& verticalSpeed, float& speed, float& lateralSpeed,
    float bodyHeight, float surface, bool hasWater, bool boat, bool destroyed,
    float gravity, float seconds, bool geometricBuoyancy=true) {
    if(!std::isfinite(seconds)||seconds<=0)return false;
    state.immersion=hasWater&&std::isfinite(surface)?
        std::clamp((surface-height)/std::max(0.3f,bodyHeight),0.0f,1.0f):0.0f;
    if(geometricBuoyancy)
        state.buoyant=boat&&hasWater&&std::isfinite(surface)&&height<=surface+.05f;
    if(!hasWater||!std::isfinite(surface)||(!boat&&height>surface+.05f))return false;
    const float duration=std::min(seconds,0.1f);
    const int steps=std::max(1,int(std::ceil(duration*60.0f)));
    const float dt=duration/float(steps);
    for(int index=0;index<steps;++index) {
        const float immersion=boat?1.0f:std::clamp((surface-height)/std::max(.3f,bodyHeight),0.0f,1.0f);
        speed*=std::exp(-(boat?.3f:2.5f)*immersion*dt);
        lateralSpeed*=std::exp(-4.0f*immersion*dt);
        if(boat&&!destroyed) {
            verticalSpeed+=(surface-height)*10.0f*dt;
            verticalSpeed*=std::exp(-4.5f*dt);
        } else {
            verticalSpeed-=std::max(0.0f,gravity)*(1.0f-.75f*immersion)*dt;
            verticalSpeed*=std::exp(-2.0f*immersion*dt);
        }
        verticalSpeed=std::clamp(verticalSpeed,-3.0f,3.0f);
        height+=verticalSpeed*dt;
    }
    state.immersion=std::clamp((surface-height)/std::max(.3f,bodyHeight),0.0f,1.0f);
    if(!boat&&state.immersion>.6f)state.engineFlooded=true;
    return true;
}

struct StorylandWaterGrid {
    int columns = 0, rows = 0;
    float west = 0, east = 0, south = 0, north = 0;
    float x(int column) const { return column == columns ? east : west + (east - west) * (float(column) / columns); }
    float y(int row) const { return row == rows ? north : south + (north - south) * (float(row) / rows); }
};

struct StorylandWaterGridRange {
    int firstColumn=0,lastColumn=0,firstRow=0,lastRow=0;
};

// Clip the cell indices so the grid does not swim along with the camera.
inline StorylandWaterGridRange storylandVisibleWaterGrid(const StorylandWaterGrid& grid,
    float cameraX,float cameraY,float distance) {
    StorylandWaterGridRange range;
    if(grid.columns<=0||grid.rows<=0||!std::isfinite(cameraX)||!std::isfinite(cameraY)||
        !std::isfinite(distance)||distance<=0)return range;
    const float west=std::max(grid.west,cameraX-distance),east=std::min(grid.east,cameraX+distance);
    const float south=std::max(grid.south,cameraY-distance),north=std::min(grid.north,cameraY+distance);
    if(west>=east||south>=north)return range;
    const float columnScale=grid.columns/(grid.east-grid.west),rowScale=grid.rows/(grid.north-grid.south);
    range.firstColumn=std::clamp(int(std::floor((west-grid.west)*columnScale)),0,grid.columns);
    range.lastColumn=std::clamp(int(std::ceil((east-grid.west)*columnScale)),0,grid.columns);
    range.firstRow=std::clamp(int(std::floor((south-grid.south)*rowScale)),0,grid.rows);
    range.lastRow=std::clamp(int(std::ceil((north-grid.south)*rowScale)),0,grid.rows);
    return range;
}

inline StorylandWaterGrid storylandWaterGrid(float west, float east, float south, float north) {
    StorylandWaterGrid grid;
    if (!std::isfinite(west) || !std::isfinite(east) || !std::isfinite(south) || !std::isfinite(north) ||
        west >= east || south >= north || !std::isfinite(east - west) || !std::isfinite(north - south)) return grid;
    grid.west = west; grid.east = east; grid.south = south; grid.north = north;
    grid.columns = int(std::clamp(std::ceil((east - west) / 20.0f), 1.0f, 128.0f));
    grid.rows = int(std::clamp(std::ceil((north - south) / 20.0f), 1.0f, 128.0f));
    return grid;
}
