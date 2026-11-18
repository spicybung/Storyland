#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

struct StorylandUnderwaterVolume {
    float west=0, east=0, south=0, north=0, surface=0, bottom=0;
};

struct StorylandUnderwaterFrame {
    static constexpr std::size_t MaximumVolumes=16;
    bool enabled=false;
    float immersion=0, cameraDepth=0, gameSeconds=0;
    std::array<float,3> camera{}, fogColor{}, colorGain{1,1,1}, waterColor{}, extinction{.105f,.047f,.035f};
    std::array<float,16> inverseViewProjection{};
    std::array<StorylandUnderwaterVolume,MaximumVolumes> volumes{};
    unsigned volumeCount=0;
};

inline float storylandWaterVisualWave(float x,float y,float seconds,float wind,bool aero) {
    if(!aero)return .055f*std::sin(x*.095f+seconds*1.5f)*std::cos(y*.085f+seconds*1.2f);
    const float strength=std::clamp(wind,0.0f,1.0f);
    return (.035f+.080f*strength)*std::sin(x*.095f+y*.034f+seconds*.72f)
        +(.026f+.048f*strength)*std::sin(x*-.041f+y*.108f+seconds*.91f+1.7f)
        +(.070f+.070f*strength)*std::sin(x*.018f+y*.011f+seconds*.31f+.9f)
        +(.055f+.050f*strength)*std::sin(x*-.013f+y*.021f+seconds*.24f+2.6f);
}

template<class WaterRegion>
float storylandWaterVisualSurface(const WaterRegion& region,float x,float y,float seconds,float wind,bool aero) {
    if(aero)return region.height+storylandWaterVisualWave(x,y,seconds,wind,true);
    const int columns=int(std::clamp(std::ceil((region.east-region.west)/20.0f),1.0f,128.0f));
    const int rows=int(std::clamp(std::ceil((region.north-region.south)/20.0f),1.0f,128.0f));
    const float column=std::clamp((x-region.west)/(region.east-region.west)*columns,0.0f,float(columns));
    const float row=std::clamp((y-region.south)/(region.north-region.south)*rows,0.0f,float(rows));
    const int firstColumn=std::min(int(column),columns-1),firstRow=std::min(int(row),rows-1);
    const float u=column-firstColumn,v=row-firstRow;
    const float west=region.west+(region.east-region.west)*float(firstColumn)/columns;
    const float east=region.west+(region.east-region.west)*float(firstColumn+1)/columns;
    const float south=region.south+(region.north-region.south)*float(firstRow)/rows;
    const float north=region.south+(region.north-region.south)*float(firstRow+1)/rows;
    const float a=storylandWaterVisualWave(west,south,seconds,wind,false);
    const float b=storylandWaterVisualWave(east,south,seconds,wind,false);
    const float c=storylandWaterVisualWave(west,north,seconds,wind,false);
    const float d=storylandWaterVisualWave(east,north,seconds,wind,false);
    return region.height+(u+v<=1?a+(b-a)*u+(c-a)*v:d+(c-d)*(1-u)+(b-d)*(1-v));
}

inline bool storylandUnderwaterInverseViewProjection(const float* projection,const float* view,
    std::array<float,16>& inverse) {
    double rows[4][8]{};
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column) {
        for(unsigned k=0;k<4;++k)rows[row][column]+=double(projection[k*4+row])*view[column*4+k];
        rows[row][column+4]=row==column?1.0:0.0;
    }
    for(unsigned column=0;column<4;++column) {
        unsigned pivot=column;
        for(unsigned row=column+1;row<4;++row)
            if(std::fabs(rows[row][column])>std::fabs(rows[pivot][column]))pivot=row;
        if(!std::isfinite(rows[pivot][column])||std::fabs(rows[pivot][column])<1.e-12)return false;
        for(unsigned k=0;k<8;++k)std::swap(rows[pivot][k],rows[column][k]);
        const double divisor=rows[column][column];
        for(double& value:rows[column])value/=divisor;
        for(unsigned row=0;row<4;++row)if(row!=column) {
            const double factor=rows[row][column];
            for(unsigned k=0;k<8;++k)rows[row][k]-=factor*rows[column][k];
        }
    }
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)
        inverse[column*4+row]=float(rows[row][column+4]);
    return true;
}

template<class WaterRegions>
StorylandUnderwaterFrame storylandUnderwaterFrame(const WaterRegions& regions,
    const std::array<float,3>& camera,float seconds,float wind,bool aero,unsigned interior,
    const std::array<float,3>& ambient,const float* projection,const float* view,float hour=12) {
    StorylandUnderwaterFrame frame;
    frame.camera=camera;frame.gameSeconds=seconds;
    if(interior||!std::isfinite(seconds)||!std::isfinite(camera[0])||!std::isfinite(camera[1])||
        !std::isfinite(camera[2])||!storylandUnderwaterInverseViewProjection(projection,view,frame.inverseViewProjection))return frame;
    std::array<float,StorylandUnderwaterFrame::MaximumVolumes> distances;
    distances.fill(1.e30f);
    for(const auto& region:regions) {
        if(!std::isfinite(region.west)||!std::isfinite(region.east)||!std::isfinite(region.south)||
            !std::isfinite(region.north)||!std::isfinite(region.height)||region.east<=region.west||region.north<=region.south)continue;
        if(camera[2]<region.height-50.0f)continue;
        const float x=std::clamp(camera[0],region.west,region.east);
        const float y=std::clamp(camera[1],region.south,region.north);
        const float distance=(x-camera[0])*(x-camera[0])+(y-camera[1])*(y-camera[1]);
        if(distance>250.0f*250.0f)continue;
        const float surface=storylandWaterVisualSurface(region,x,y,seconds,wind,aero);
        if(camera[0]>=region.west&&camera[0]<=region.east&&camera[1]>=region.south&&camera[1]<=region.north) {
            const float depth=surface-camera[2];
            frame.cameraDepth=std::max(frame.cameraDepth,std::max(0.0f,depth));
            const float transition=std::clamp((depth+.04f)/.16f,0.0f,1.0f);
            frame.immersion=std::max(frame.immersion,transition*transition*(3.0f-2.0f*transition));
        }
        unsigned slot=frame.volumeCount;
        if(slot==frame.MaximumVolumes) {
            slot=unsigned(std::max_element(distances.begin(),distances.end())-distances.begin());
            if(distance>=distances[slot])continue;
        } else ++frame.volumeCount;
        distances[slot]=distance;
        frame.volumes[slot]={region.west,region.east,region.south,region.north,surface,region.height-50.0f};
    }
    // Supplied SA timecyc.dat weather 20: fog RGB, water RGB, filter RGB sum, fog start, far clip.
    constexpr std::array<float,9> hours{0,5,6,7,12,19,20,22,24};
    constexpr std::array<std::array<float,11>,8> samples{{
        {16.666667f,16.666667f,16.666667f,59,68,77,128,212,99,5,300},
        {16.666667f,16.666667f,16.666667f,59,68,77,94,211,115,5,300},
        {16.666667f,16.666667f,16.666667f,62,72,77,124,238,130,5,300},
        {46.666667f,46.666667f,46.666667f,107,117,122,124,201,124,5,300},
        {73.333333f,73.333333f,73.333333f,141,141,140,124,194,133,5,300},
        {73.333333f,73.333333f,73.333333f,116,135,144,134,185,107,5,300},
        {60,60,60,132,176,189,63,211,119,5,300},
        {46.666667f,46.666667f,46.666667f,161,176,189,124,209,111,5,300}
    }};
    hour=std::isfinite(hour)?std::fmod(std::fmod(hour,24.0f)+24.0f,24.0f):12.0f;
    unsigned first=0;
    while(first<7&&hour>=hours[first+1])++first;
    const unsigned next=(first+1)%8;
    const float blend=(hour-hours[first])/(hours[first+1]-hours[first]);
    std::array<float,11> sample{};
    for(unsigned field=0;field<sample.size();++field)
        sample[field]=samples[first][field]+(samples[next][field]-samples[first][field])*blend;
    const float maximumGain=1.0f+std::max({sample[6],sample[7],sample[8]})/255.0f;
    const float darkness=std::max(.04f,1.0f-frame.cameraDepth/90.0f);
    for(unsigned channel=0;channel<3;++channel) {
        frame.colorGain[channel]=(1.0f+sample[6+channel]/255.0f)/maximumGain;
        frame.fogColor[channel]=sample[channel]/255.0f*frame.colorGain[channel]*darkness;
        frame.waterColor[channel]=sample[3+channel]/255.0f;
    }
    (void)ambient;
    frame.enabled=frame.volumeCount!=0;
    return frame;
}

inline float storylandUnderwaterSegmentLength(const StorylandUnderwaterVolume& volume,
    const std::array<float,3>& first,const std::array<float,3>& last) {
    const std::array<float,3> minimum{volume.west,volume.south,volume.bottom};
    const std::array<float,3> maximum{volume.east,volume.north,volume.surface};
    float enter=0,leave=1,lengthSquared=0;
    for(unsigned axis=0;axis<3;++axis) {
        const float direction=last[axis]-first[axis];lengthSquared+=direction*direction;
        if(std::fabs(direction)<1.e-7f) {
            if(first[axis]<minimum[axis]||first[axis]>maximum[axis])return 0;
        } else {
            float nearDistance=(minimum[axis]-first[axis])/direction;
            float farDistance=(maximum[axis]-first[axis])/direction;
            if(nearDistance>farDistance)std::swap(nearDistance,farDistance);
            enter=std::max(enter,nearDistance);leave=std::min(leave,farDistance);
        }
    }
    return std::max(0.0f,leave-enter)*std::sqrt(lengthSquared);
}

inline std::array<float,3> storylandUnderwaterTransmit(const StorylandUnderwaterFrame& frame,
    const std::array<float,3>& color,float distance) {
    std::array<float,3> result{};
    for(unsigned axis=0;axis<3;++axis) {
        const float transmission=std::exp(-std::max(0.0f,distance)*frame.extinction[axis]);
        const float wet=1-std::exp(-std::max(0.0f,distance)*.16f);
        const float gain=1+(frame.colorGain[axis]-1)*wet;
        const float darkness=1+(std::max(.04f,1.0f-frame.cameraDepth/90.0f)-1)*std::clamp((frame.immersion-.535f)/.465f,0.0f,1.0f);
        result[axis]=color[axis]*gain*darkness*transmission+frame.fogColor[axis]*(1-transmission);
    }
    return result;
}
