#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

struct StorylandAeroWave {
    float height=0.0f;
    float slopeX=0.0f;
    float slopeY=0.0f;
};

inline StorylandAeroWave storylandAeroWave(float x,float y,float seconds,float wind) {
    StorylandAeroWave result;
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(seconds))return result;
    const float strength=std::clamp(wind,0.0f,1.0f);
    const auto add=[&](float frequencyX,float frequencyY,float speed,float phase,float amplitude) {
        const float angle=x*frequencyX+y*frequencyY+seconds*speed+phase;
        result.height+=amplitude*std::sin(angle);
        result.slopeX+=amplitude*frequencyX*std::cos(angle);
        result.slopeY+=amplitude*frequencyY*std::cos(angle);
    };
    add(.095f,.034f,.72f,0.0f,.035f+.080f*strength);
    add(-.041f,.108f,.91f,1.7f,.026f+.048f*strength);
    add(.018f,.011f,.31f,.9f,.070f+.070f*strength);
    add(-.013f,.021f,.24f,2.6f,.055f+.050f*strength);
    return result;
}

// A moving, world-space Gray-Scott field. The grid follows the player by whole
// cells so existing reaction structures remain fixed in world coordinates.
class StorylandWaterReaction {
public:
    static constexpr int Width=96;
    static constexpr int Height=96;
    static constexpr float CellSize=.50f;
    static constexpr float WorldSize=Width*CellSize;

    StorylandWaterReaction() { clear(); }

    void clear() {
        chemicalA.assign(Width*Height,1.0f);
        chemicalB.assign(Width*Height,0.0f);
        nextA.resize(Width*Height);
        nextB.resize(Width*Height);
        pixels.assign(Width*Height*4,0);
        originX=originY=0.0f;
        started=false;
        accumulator=0.0f;
        lastContact=false;
        updatePixels();
    }

    void follow(float x,float y) {
        if(!std::isfinite(x)||!std::isfinite(y))return;
        const float newX=std::floor((x-WorldSize*.5f)/CellSize)*CellSize;
        const float newY=std::floor((y-WorldSize*.5f)/CellSize)*CellSize;
        if(!started){started=true;originX=newX;originY=newY;return;}
        const int shiftX=int(std::round((newX-originX)/CellSize));
        const int shiftY=int(std::round((newY-originY)/CellSize));
        if(shiftX==0&&shiftY==0)return;
        if(std::abs(shiftX)>=Width||std::abs(shiftY)>=Height) {
            std::fill(chemicalA.begin(),chemicalA.end(),1.0f);
            std::fill(chemicalB.begin(),chemicalB.end(),0.0f);
        } else {
            for(int row=0;row<Height;++row)for(int column=0;column<Width;++column) {
                const int sourceX=column+shiftX,sourceY=row+shiftY;
                const int target=row*Width+column;
                if(sourceX>=0&&sourceX<Width&&sourceY>=0&&sourceY<Height) {
                    const int source=sourceY*Width+sourceX;
                    nextA[target]=chemicalA[source];
                    nextB[target]=chemicalB[source];
                } else {nextA[target]=1.0f;nextB[target]=0.0f;}
            }
            chemicalA.swap(nextA);chemicalB.swap(nextB);
        }
        originX=newX;originY=newY;
    }

    void disturb(float x,float y,float intensity) {
        if(!started||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(intensity)||intensity<=0)return;
        const float centerX=(x-originX)/CellSize;
        const float centerY=(y-originY)/CellSize;
        const int startX=std::max(1,int(std::floor(centerX-6)));
        const int endX=std::min(Width-2,int(std::ceil(centerX+6)));
        const int startY=std::max(1,int(std::floor(centerY-6)));
        const int endY=std::min(Height-2,int(std::ceil(centerY+6)));
        for(int row=startY;row<=endY;++row)for(int column=startX;column<=endX;++column) {
            const float dx=(column-centerX)*CellSize,dy=(row-centerY)*CellSize;
            const float gaussian=std::exp(-(dx*dx+dy*dy)/1.25f)*std::clamp(intensity,0.0f,1.0f);
            const int index=row*Width+column;
            chemicalB[index]=std::clamp(chemicalB[index]+gaussian*.35f,0.0f,1.0f);
            chemicalA[index]=std::clamp(chemicalA[index]-gaussian*.16f,0.0f,1.0f);
        }
    }

    void advance(float seconds,float playerX,float playerY,bool contact,float motion) {
        if(!std::isfinite(seconds)||seconds<0)return;
        follow(playerX,playerY);
        if(contact) {
            // A new touch triggers a stronger splash. Continued motion creates wakes.
            disturb(playerX,playerY,lastContact?.11f+std::clamp(motion,0.0f,8.0f)*.055f:.75f);
        }
        lastContact=contact;
        accumulator+=std::clamp(seconds,0.0f,.1f)*30.0f;
        int steps=0;
        while(accumulator>=1.0f&&steps<4) {
            step();accumulator-=1.0f;++steps;
        }
        if(accumulator>4.0f)accumulator=4.0f;
        updatePixels();
    }

    float sample(float x,float y) const {
        if(!started||!std::isfinite(x)||!std::isfinite(y))return 0.0f;
        const float sx=(x-originX)/CellSize,sy=(y-originY)/CellSize;
        if(sx<1||sy<1||sx>=Width-2||sy>=Height-2)return 0.0f;
        const int ix=int(sx),iy=int(sy);
        const float tx=sx-ix,ty=sy-iy;
        const auto value=[&](int px,int py){return chemicalB[py*Width+px];};
        const float lower=value(ix,iy)*(1-tx)+value(ix+1,iy)*tx;
        const float upper=value(ix,iy+1)*(1-tx)+value(ix+1,iy+1)*tx;
        return std::clamp((lower*(1-ty)+upper*ty)*3.0f,0.0f,1.0f);
    }

    float west() const {return originX;}
    float south() const {return originY;}
    float extent() const {return WorldSize;}
    bool ready() const {return started;}
    const std::vector<uint8_t>& image() const {return pixels;}

private:
    void step() {
        constexpr float diffuseA=.18f,diffuseB=.09f,feed=.035f,kill=.061f,timeStep=.75f;
        for(int row=0;row<Height;++row)for(int column=0;column<Width;++column) {
            const int index=row*Width+column;
            if(column==0||row==0||column==Width-1||row==Height-1) {
                nextA[index]=1.0f;nextB[index]=0.0f;continue;
            }
            const auto laplacian=[&](const std::vector<float>& field) {
                const float value=field[index];
                return field[index-1]+field[index+1]+field[index-Width]+field[index+Width]-4.0f*value;
            };
            const float a=chemicalA[index],b=chemicalB[index];
            const float reaction=a*b*b;
            nextA[index]=std::clamp(a+(diffuseA*laplacian(chemicalA)-reaction+feed*(1.0f-a))*timeStep,0.0f,1.0f);
            nextB[index]=std::clamp(b+(diffuseB*laplacian(chemicalB)+reaction-(feed+kill)*b)*timeStep,0.0f,1.0f);
        }
        chemicalA.swap(nextA);chemicalB.swap(nextB);
    }

    void updatePixels() {
        for(size_t index=0;index<chemicalB.size();++index) {
            const uint8_t value=uint8_t(std::clamp(chemicalB[index]*765.0f,0.0f,255.0f));
            pixels[index*4]=value;
            pixels[index*4+1]=value;
            pixels[index*4+2]=value;
            pixels[index*4+3]=255;
        }
    }

    std::vector<float> chemicalA,chemicalB,nextA,nextB;
    std::vector<uint8_t> pixels;
    float originX=0.0f,originY=0.0f,accumulator=0.0f;
    bool started=false,lastContact=false;
};
