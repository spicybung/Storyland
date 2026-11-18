#pragma once
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

inline bool storylandAeroMaterialHasAuthoredGlow(const std::string& name) {
    std::string lower;
    lower.reserve(name.size());
    for (unsigned char letter : name) lower.push_back(char(std::tolower(letter)));
    for (const char* marker : {"neon", "emiss", "illumin", "fluo", "glow",
                              "lightbar", "lamp", "bulb", "backlit", "lit_sign",
                              "light_sign", "led", "lighttube", "lightstrip"}) {
        if (lower.find(marker) != std::string::npos) return true;
    }
    return false;
}

template<class Pixels>
float storylandAeroMaterialEmission(const std::string& name, const Pixels& rgba) {
    if (rgba.size() < 4) return 0.0f;
    const size_t count=rgba.size()/4;
    const size_t stride=std::max<size_t>(1,count/96);
    size_t bright=0,colored=0,samples=0;
    for(size_t pixel=0;pixel<count;pixel+=stride){
        const size_t position=pixel*4;
        if(rgba[position+3]<128)continue;
        const float red=float(rgba[position])/255.0f;
        const float green=float(rgba[position+1])/255.0f;
        const float blue=float(rgba[position+2])/255.0f;
        const float peak=std::max({red,green,blue});
        const float low=std::min({red,green,blue});
        ++samples;
        if(peak>.74f)++bright;
        if(peak>.72f&&peak-low>.43f)++colored;
    }
    const bool authored=storylandAeroMaterialHasAuthoredGlow(name);
    if(samples==0)return 0.0f;
    const float colouredPortion=float(colored)/float(samples);
    const float brightPortion=float(bright)/float(samples);
    if(authored) return std::clamp(.40f+brightPortion*.72f,.4f,1.10f);
    // Avoid colouring ordinary brick, paint or vegetation. Texture names are
    // the strongest signal; high-contrast strips are the secondary signal.
    if(colouredPortion>.19f&&brightPortion>.29f&&brightPortion<.76f)
        return std::min(.30f,colouredPortion*.43f);
    return 0.0f;
}
