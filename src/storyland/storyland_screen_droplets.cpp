#include "src/storyland/storyland_screen_droplets.h"

#include <algorithm>
#include <cmath>

namespace {
float dropletDot(const std::array<float,3>& a,const std::array<float,3>& b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
}

std::array<std::uint8_t,64*64*4> storylandScreenDropletMask() {
    std::array<std::uint8_t,64*64*4> pixels{};
    for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
        const float u=((float(x)+0.5f)/64.0f-0.5f)*2.0f;
        const float v=((float(y)+0.5f)/64.0f-0.5f)*2.0f;
        const std::uint8_t value=u*u+v*v<1.0f?255:0;
        for(int channel=0;channel<4;++channel)pixels[(y*64+x)*4+channel]=value;
    }
    return pixels;
}

std::array<StorylandScreenDropletVertex,4> storylandScreenDropletQuad(
    const StorylandScreenDroplet& drop,int width,int height) {
    if(width<=0||height<=0)return {};
    const float half=0.5f*drop.size*float(height)/480.0f;
    const float sampleRadius=(drop.magnification*260.0f+40.0f)*float(height)/480.0f;
    const float left=std::max(drop.x-sampleRadius,0.0f)/float(width);
    const float right=std::min(drop.x+sampleRadius,float(width))/float(width);
    const float top=std::max(drop.y-sampleRadius,0.0f)/float(height);
    const float bottom=std::min(drop.y+sampleRadius,float(height))/float(height);
    return {{{drop.x-half,drop.y-half,0,0,left,1.0f-bottom},
        {drop.x-half,drop.y+half,0,1,left,1.0f-top},
        {drop.x+half,drop.y+half,1,1,right,1.0f-top},
        {drop.x+half,drop.y-half,1,0,right,1.0f-bottom}}};
}

void StorylandScreenDroplets::reset() {
    drops_={};moving_={};
    activeCount_=movingCount_=0;
    haveCamera_=false;rainBudget_=sprayBudget_=splashClock_=0.0;splashTick_=-1;
    screenMove_={};cameraDistance_=forwardMotionAtThirty_=0.0f;
}

void StorylandScreenDroplets::setViewport(int width,int height) {
    if(width<=0||height<=0||width>16384||height>16384)return;
    if(width!=width_||height!=height_) {
        const float xScale=float(width)/float(width_),yScale=float(height)/float(height_);
        for(auto& drop:drops_)if(drop.active) {drop.x*=xScale;drop.y*=yScale;}
        width_=width;height_=height;
    }
}

float StorylandScreenDroplets::randomUnit() {
    randomState_^=randomState_<<13;randomState_^=randomState_>>17;randomState_^=randomState_<<5;
    return float(randomState_&0xFFFFFFu)/16777216.0f;
}

int StorylandScreenDroplets::spawn(float x,float y,float size,float lifetime,bool blood) {
    if(activeCount_>=MaximumDrops)return -1;
    for(std::size_t index=0;index<drops_.size();++index)if(!drops_[index].active) {
        auto& drop=drops_[index];drop={};drop.active=true;
        drop.x=x;drop.y=y;drop.size=size;drop.lifetime=lifetime;drop.blood=blood;
        drop.magnification=(16.0f-size)/12.0f;
        drop.color=blood?std::array<std::uint8_t,4>{255,0,0,255}:
            std::array<std::uint8_t,4>{255,255,255,255};
        ++activeCount_;return int(index);
    }
    return -1;
}

void StorylandScreenDroplets::attachMoving(int index) {
    if(index<0||movingCount_>=MaximumMovingDrops)return;
    for(std::size_t slot=0;slot<moving_.size();++slot)if(moving_[slot].index<0) {
        moving_[slot]={index,0.0f};drops_[std::size_t(index)].movingSlot=int(slot);
        ++movingCount_;return;
    }
}

void StorylandScreenDroplets::fillMoving(float amount,bool blood) {
    if(!std::isfinite(amount)||amount<=0)return;
    const float multiplier=forwardMotionAtThirty_>5.0f?1.5f:1.0f;
    const int count=int(std::min(float(MaximumMovingDrops),std::max(0.0f,amount*20.0f*multiplier)));
    for(int i=0;i<count&&activeCount_<MaximumDrops&&movingCount_<MaximumMovingDrops;++i) {
        const float x=std::floor(randomUnit()*float(width_));
        const float y=std::floor(randomUnit()*float(height_));
        const float size=4.0f+11.0f*randomUnit();
        attachMoving(spawn(x,y,size,2.0f,blood));
    }
}

void StorylandScreenDroplets::addWater(float strength) {fillMoving(strength,false);}
void StorylandScreenDroplets::addBlood(float strength) {fillMoving(strength,true);}

void StorylandScreenDroplets::registerSplash(const std::array<float,3>& position,bool validPosition) {
    const auto origin=validPosition?position:previousCamera_;
    std::array<float,3> delta{};
    for(std::size_t axis=0;axis<3;++axis) {
        if(!std::isfinite(origin[axis]))return;
        delta[axis]=origin[axis]-previousCamera_[axis];
    }
    if(dropletDot(delta,delta)>=50.0f)return;
    splashPosition_=origin;splashTick_=14;splashClock_=0.0;
}

void StorylandScreenDroplets::deactivate(std::size_t index) {
    auto& drop=drops_[index];
    if(!drop.active)return;
    if(drop.movingSlot>=0) {
        moving_[std::size_t(drop.movingSlot)]={};--movingCount_;
    }
    drop.active=false;drop.movingSlot=-1;--activeCount_;
}

void StorylandScreenDroplets::trace(MovingDrop& moving) {
    if(activeCount_>=MaximumDrops)return;
    const auto drop=drops_[std::size_t(moving.index)];
    moving.distance=0.0f;
    spawn(drop.x,drop.y,4.0f,0.5f,drop.blood);
}

void StorylandScreenDroplets::advance(float seconds,const StorylandScreenDropletCamera& camera) {
    if(!std::isfinite(seconds)||seconds<0)return;
    for(const auto* basis:{&camera.position,&camera.right,&camera.up,&camera.forward})
        for(float value:*basis)if(!std::isfinite(value))return;
    const float dt=std::min(seconds,60.0f);
    std::array<float,3> delta{};
    if(haveCamera_)for(std::size_t axis=0;axis<3;++axis)delta[axis]=camera.position[axis]-previousCamera_[axis];
    previousCamera_=camera.position;haveCamera_=true;
    cameraDistance_=std::sqrt(std::max(0.0f,dropletDot(delta,delta)));
    screenMove_={-dropletDot(camera.right,delta)*10.0f,
        dropletDot(camera.up,delta)*10.0f,dropletDot(camera.forward,delta)*10.0f};
    if(!std::isfinite(cameraDistance_)||!std::isfinite(screenMove_[0])||
        !std::isfinite(screenMove_[1])||!std::isfinite(screenMove_[2])) {
        cameraDistance_=0.0f;screenMove_={};
    }
    const float referenceScale=dt>0?1.0f/(dt*30.0f):0.0f;
    forwardMotionAtThirty_=screenMove_[2]*referenceScale;
    const float distanceAtThirty=cameraDistance_*referenceScale;
    const float lateral=std::hypot(screenMove_[0],screenMove_[1]);
    if(camera.enabled&&!camera.rainBlocked&&std::isfinite(camera.rain)&&camera.rain>0&&dt>0) {
        const float angle=180.0f-std::acos(std::clamp(camera.forward[2],-1.0f,1.0f))*57.295779513f;
        const float amount=(std::max(angle,40.0f)-40.0f)/150.0f*
            std::clamp(camera.rain,0.0f,1.0f)*0.5f;
        const float multiplier=screenMove_[2]*referenceScale>5.0f?1.5f:1.0f;
        const int perReferenceFrame=int(amount*20.0f*multiplier);
        rainBudget_+=double(perReferenceFrame)*double(dt)*30.0;
        const int due=int(std::min(double(MaximumMovingDrops),std::floor(rainBudget_+0.000001)));
        rainBudget_=std::max(0.0,rainBudget_-std::floor(rainBudget_+0.000001));
        for(int i=0;i<due&&activeCount_<MaximumDrops&&movingCount_<MaximumMovingDrops;++i) {
            const float x=std::floor(randomUnit()*float(width_));
            const float y=std::floor(randomUnit()*float(height_));
            attachMoving(spawn(x,y,4.0f+11.0f*randomUnit(),2.0f,false));
        }
    }
    if(camera.enabled&&std::isfinite(camera.sprayStrength)&&camera.sprayStrength>0&&dt>0) {
        const int perReferenceFrame=int(std::min(float(MaximumMovingDrops),camera.sprayStrength*20.0f*
            (forwardMotionAtThirty_>5.0f?1.5f:1.0f)));
        sprayBudget_+=double(perReferenceFrame)*double(dt)*30.0;
        const int due=int(std::min(double(MaximumMovingDrops),std::floor(sprayBudget_+0.000001)));
        sprayBudget_=std::max(0.0,sprayBudget_-std::floor(sprayBudget_+0.000001));
        for(int i=0;i<due&&activeCount_<MaximumDrops&&movingCount_<MaximumMovingDrops;++i) {
            const float x=std::floor(randomUnit()*float(width_));
            const float y=std::floor(randomUnit()*float(height_));
            attachMoving(spawn(x,y,4.0f+11.0f*randomUnit(),2.0f,false));
        }
    }
    if(splashTick_>=0&&dt>0) {
        constexpr int counts[15]={125,250,500,1000,1000,0,0,0,0,0,0,0,0,0,0};
        splashClock_+=double(dt)*30.0;
        while(splashTick_>=0&&splashClock_+0.000001>=1.0) {
            splashClock_=std::max(0.0,splashClock_-1.0);
            std::array<float,3> separation{};
            for(std::size_t axis=0;axis<3;++axis)separation[axis]=splashPosition_[axis]-camera.position[axis];
            const float distance=std::sqrt(std::max(0.0f,dropletDot(separation,separation)));
            const float multiplier=std::max(0.0f,1.0f-(distance-5.0f)/15.0f);
            const int count=int(float(counts[splashTick_])*multiplier);
            for(int i=0;i<count&&activeCount_<MaximumDrops;++i) {
                const float x=std::floor(randomUnit()*float(width_));
                const float y=std::floor(randomUnit()*float(height_));
                spawn(x,y,4.0f+11.0f*randomUnit(),10.0f,false);
            }
            --splashTick_;
        }
    }
    if(camera.movingEnabled&&camera.enabled&&screenMove_[2]>0&&distanceAtThirty>0.3f) {
        for(auto& moving:moving_)if(moving.index>=0) {
            auto& drop=drops_[std::size_t(moving.index)];
            if(!drop.active) {moving={};--movingCount_;continue;}
            if(lateral*referenceScale>0.5f&&!camera.firstPerson) {
                moving.distance+=lateral;
                if(moving.distance>20.0f&&drop.color[3]>100)trace(moving);
                drop.x-=screenMove_[0];drop.y+=screenMove_[1];
            } else {
                const float distance=screenMove_[2]*0.2f;
                float x=drop.x-float(width_)*0.5f+screenMove_[0];
                float y=drop.y-float(height_)*(camera.firstPerson?1.2f:0.5f)-screenMove_[1];
                const float total=std::fabs(x)+std::fabs(y);
                if(total>0.001f) {x/=total;y/=total;}
                moving.distance+=distance;
                if(moving.distance>20.0f&&drop.color[3]>100)trace(moving);
                drop.x+=x*distance;drop.y+=y*distance;
            }
            if(drop.x<0||drop.y<0||drop.x>width_||drop.y>height_) {
                drop.movingSlot=-1;moving={};--movingCount_;
            }
        }
    }
    for(std::size_t index=0;index<drops_.size();++index)if(drops_[index].active) {
        auto& drop=drops_[index];drop.age+=dt;
        if(drop.age>=drop.lifetime)deactivate(index);
        else drop.color[3]=std::uint8_t(std::clamp(255.0f-drop.age/drop.lifetime*255.0f,0.0f,255.0f));
    }
}

void StorylandScreenDropletPlayback::reset() {
    simulation_.reset();haveFrame_=haveSerials_=false;
    waterSerial_=bloodSerial_=0;
}

void StorylandScreenDropletPlayback::advance(bool enabled,float gameSeconds,
    std::uint32_t waterSerial,std::uint32_t bloodSerial,float waterStrength,
    float bloodStrength,const StorylandScreenDropletCamera& camera,int width,int height) {
    if(!enabled) {
        reset();waterSerial_=waterSerial;bloodSerial_=bloodSerial;haveSerials_=true;return;
    }
    if(!std::isfinite(gameSeconds)||width<=0||height<=0)return;
    simulation_.setViewport(width,height);
    float elapsed=0.0f;
    if(haveFrame_&&gameSeconds>=lastGameSeconds_)elapsed=gameSeconds-lastGameSeconds_;
    else if(haveFrame_) {simulation_.reset();haveSerials_=false;}
    haveFrame_=true;lastGameSeconds_=gameSeconds;
    simulation_.advance(elapsed,camera);
    if(!haveSerials_) {
        waterSerial_=waterSerial;bloodSerial_=bloodSerial;haveSerials_=true;
    }
    const std::uint32_t waterEvents=waterSerial-waterSerial_;
    const std::uint32_t bloodEvents=bloodSerial-bloodSerial_;
    waterSerial_=waterSerial;bloodSerial_=bloodSerial;
    if(waterEvents>0&&waterEvents<0x80000000u&&std::isfinite(waterStrength)&&waterStrength>0)
        simulation_.registerSplash(camera.splashPosition,camera.splashPositionValid);
    if(bloodEvents<0x80000000u)
        for(std::uint32_t event=0;event<std::min(bloodEvents,700u);++event)
            simulation_.addBlood(bloodStrength);
}
