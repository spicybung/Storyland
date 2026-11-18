#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

class StorylandRadioVb {
public:
    bool open(const std::filesystem::path& path, std::string& error) {
        input.close(); input.clear();
        bytes=0;position=0;history1={};history2={};
        input.open(path,std::ios::binary);
        if(!input) {error="Could not open the retail VB radio stream.";return false;}
        input.seekg(0,std::ios::end);
        const auto size=input.tellg();
        if(size<=0 || uint64_t(size)%16384u!=0) {
            error="Retail VB stereo stream has an incomplete 0x2000-byte channel pair.";return false;
        }
        bytes=uint64_t(size);position=0;history1={};history2={};
        input.seekg(0,std::ios::beg);error.clear();return true;
    }
    bool seek(double seconds, std::vector<int16_t>& pcm, std::string& error,
        const std::function<bool()>& cancelled = {}) {
        if(!bytes||!std::isfinite(seconds)||seconds<0) {error="Invalid VB radio seek.";return false;}
        const uint64_t sample=uint64_t(std::fmod(seconds,duration())*32000.0);
        constexpr uint64_t samplesPerPair=512u*28u;
        const uint64_t pair=sample/samplesPerPair;
        // Reconstruct the predictor from preceding audio instead of resetting it at the target.
        input.clear();input.seekg(0,std::ios::beg);position=0;history1={};history2={};
        bool eof=false;
        for(uint64_t index=0;index<=pair;++index) {
            if(cancelled && cancelled()) {error="Radio seek cancelled.";return false;}
            if(!read(pcm,eof,error)||eof)return false;
        }
        const size_t skip=size_t(sample%samplesPerPair)*2;
        pcm.erase(pcm.begin(),pcm.begin()+skip);
        error.clear();return true;
    }
    double duration() const {return double(bytes/32u*28u)/32000.0;}
    uint64_t sourceBytes() const {return bytes;}
    bool read(std::vector<int16_t>& pcm, bool& eof, std::string& error) {
        pcm.clear();eof=false;
        if(!input.is_open() || !bytes) {error="No valid VB radio stream is open.";return false;}
        if(position==bytes) {eof=true;error.clear();return true;}
        std::array<uint8_t,16384> compressed{};
        input.read(reinterpret_cast<char*>(compressed.data()),std::streamsize(compressed.size()));
        if(size_t(input.gcount())!=compressed.size()) {error="Could not read the next VB stereo interleave.";return false;}
        position+=compressed.size();
        constexpr int coefficients[5][2]={{0,0},{60,0},{115,-52},{98,-55},{122,-60}};
        constexpr size_t samples=8192u/16u*28u;
        pcm.resize(samples*2u);
        for(size_t channel=0;channel<2;++channel) for(size_t block=0;block<512;++block) {
            const auto* packet=compressed.data()+channel*8192u+block*16u;
            const unsigned predictor=packet[0]>>4,shift=packet[0]&15;
            if(predictor>4 || shift>12) {error="Invalid ADPCM predictor or shift in radio stream.";return false;}
            for(size_t sample=0;sample<28;++sample) {
                const uint8_t packed=packet[2+sample/2];
                int nibble=(sample&1)?packed>>4:packed&15;
                if(nibble>=8)nibble-=16;
                const int predicted=(history1[channel]*coefficients[predictor][0]+history2[channel]*coefficients[predictor][1]+32)>>6;
                const int decoded=std::clamp(((nibble*4096)>>shift)+predicted,-32768,32767);
                history2[channel]=history1[channel];history1[channel]=decoded;
                pcm[(block*28u+sample)*2u+channel]=int16_t(decoded);
            }
        }
        error.clear();return true;
    }
private:
    std::ifstream input;
    uint64_t bytes=0,position=0;
    std::array<int,2> history1{},history2{};
};
