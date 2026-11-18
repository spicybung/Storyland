#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

struct StorylandUVKeyframe {
    float time = 0.0f;
    std::array<float,6> parameters{{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}};
};

struct StorylandUVAnimationTrack {
    std::string name;
    uint32_t interpolator = 0;
    float duration = 0.0f;
    std::array<uint32_t,8> nodeToChannel{};
    std::vector<StorylandUVKeyframe> frames;
    std::vector<std::vector<StorylandUVKeyframe>> nodes;
    size_t selectedNode = 0;
    const std::vector<StorylandUVKeyframe>& selectedFrames() const {
        if (nodes.empty()) return frames;
        if (selectedNode < nodes.size()) return nodes[selectedNode];
        static const std::vector<StorylandUVKeyframe> empty;
        return empty;
    }
    bool valid() const {
        return !selectedFrames().empty() && std::isfinite(duration) && duration >= 0.0f;
    }

    std::array<float,6> matrix(float seconds) const {
        std::array<float,6> identity{{1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}};
        if (!valid() || !std::isfinite(seconds)) return identity;
        const auto& keys = selectedFrames();
        float current = seconds;
        if (duration > 0.0f) {
            current = std::fmod(seconds, duration);
            if (current < 0.0f) current += duration;
        }
        auto upper = std::upper_bound(keys.begin(), keys.end(), current,
            [](float time, const StorylandUVKeyframe& frame) { return time < frame.time; });
        auto parameters = keys.front().parameters;
        if (upper == keys.end()) {
            parameters = keys.back().parameters;
        } else if (upper != keys.begin()) {
            const auto& previous = *(upper - 1);
            const float delta = upper->time - previous.time;
            const float fraction = delta > 0.000001f ?
                std::clamp((current - previous.time) / delta, 0.0f, 1.0f) : 0.0f;
            for (size_t index=0;index<parameters.size();++index) {
                float difference = upper->parameters[index] - previous.parameters[index];
                if (interpolator == 0x1C1u && index == 0) {
                    const float pi = 3.14159265358979323846f;
                    while (difference < -pi) difference += 2.0f*pi;
                    while (difference > pi) difference -= 2.0f*pi;
                }
                parameters[index] = previous.parameters[index] + difference * fraction;
            }
        }
        if (interpolator == 0x1C1u) {
            const float radians = parameters[0];
            const float scaleX = parameters[1];
            const float scaleY = parameters[2];
            const float skew = parameters[3];
            const float tx = parameters[4];
            const float ty = parameters[5];
            const float c = std::cos(radians),s = std::sin(radians);
            // RpUVAnimParamKeyFrameApply scales/skews first, then rotates the
            // translated result about (0.5,0.5). Scaling itself has no pivot.
            const float a = c*scaleX - s*skew;
            const float b = s*scaleX + c*skew;
            const float d = -s*scaleY;
            const float e = c*scaleY;
            return {{a,b,d,e,
                c*(tx-0.5f)-s*(ty-0.5f)+0.5f,
                s*(tx-0.5f)+c*(ty-0.5f)+0.5f}};
        }
        return parameters;
    }
    std::array<float,2> transform(float u,float v,float seconds) const {
        const auto m=matrix(seconds);
        return {{u*m[0]+v*m[2]+m[4], u*m[1]+v*m[3]+m[5]}};
    }
};

struct StorylandUVMaterialAnimation {
    std::array<StorylandUVAnimationTrack,2> channels;
    std::array<std::vector<StorylandUVAnimationTrack>,2> additionalChannels;
    bool animated(size_t channel=0) const {
        return channel<channels.size()&&channels[channel].valid();
    }
    std::array<float,2> transform(float u,float v,float seconds,size_t channel=0) const {
        if (!animated(channel)) return {{u,v}};
        std::array<float,2> uv{{u,v}};
        // RW preconcatenates each later node/slot into the channel matrix.
        const auto& additional = additionalChannels[channel];
        for (auto track = additional.rbegin(); track != additional.rend(); ++track)
            uv = track->transform(uv[0], uv[1], seconds);
        return channels[channel].transform(uv[0], uv[1], seconds);
    }
};

namespace StorylandRenderWareUV {
struct Chunk {uint32_t type=0;size_t begin=0,body=0,end=0;};
inline uint32_t read32(const std::vector<uint8_t>& bytes,size_t index) {
    return uint32_t(bytes[index])|uint32_t(bytes[index+1])<<8|
        uint32_t(bytes[index+2])<<16|uint32_t(bytes[index+3])<<24;
}
inline float readFloat(const std::vector<uint8_t>& bytes,size_t index) {
    const uint32_t bits=read32(bytes,index);float result=0.0f;
    std::memcpy(&result,&bits,sizeof(result));return result;
}
inline bool readChunk(const std::vector<uint8_t>& bytes,size_t offset,size_t limit,Chunk& chunk) {
    if(limit>bytes.size()||offset>limit||limit-offset<12)return false;
    const uint32_t size=read32(bytes,offset+4);
    if(size>limit-offset-12)return false;
    chunk={read32(bytes,offset),offset,offset+12,offset+12+size};
    return true;
}
inline std::vector<Chunk> children(const std::vector<uint8_t>& bytes,const Chunk& parent) {
    std::vector<Chunk> result;
    for(size_t offset=parent.body;offset<parent.end;) {
        Chunk chunk;
        if(!readChunk(bytes,offset,parent.end,chunk)||chunk.end<=offset)break;
        result.push_back(chunk);offset=chunk.end;
    }
    return result;
}
inline std::string readName(const std::vector<uint8_t>& bytes,size_t offset,size_t maxLength) {
    std::string result;
    for(size_t i=0;i<maxLength&&offset+i<bytes.size();++i){
        const char c=char(bytes[offset+i]);if(!c)break;
        if(c>='A'&&c<='Z')result.push_back(char(c-'A'+'a'));
        else result.push_back(c);
    }
    return result;
}
inline bool decodeTrack(const std::vector<uint8_t>& bytes,const Chunk& animation,StorylandUVAnimationTrack& out) {
    if(animation.type!=0x1Bu||animation.end-animation.body<88u)return false;
    const size_t start=animation.body;
    if(read32(bytes,start)!=0x100u)return false;
    const uint32_t type=read32(bytes,start+4);
    if(type!=0x1C0u&&type!=0x1C1u)return false;
    const uint32_t count=read32(bytes,start+8);
    const float duration=readFloat(bytes,start+16);
    const size_t custom=start+20;
    const size_t keys=custom+4+32+32;
    if(count==0||count>1000000u||keys>animation.end||
        count>(animation.end-keys)/32u||!std::isfinite(duration)||duration<0.0f)return false;
    StorylandUVAnimationTrack decoded;
    decoded.name=readName(bytes,custom+4,32);
    decoded.interpolator=type;decoded.duration=duration;
    if(decoded.name.empty())return false;
    for(size_t i=0;i<8;++i)decoded.nodeToChannel[i]=read32(bytes,custom+36+i*4);
    decoded.frames.reserve(count);
    std::vector<uint32_t> previousFrames;
    previousFrames.reserve(count);
    for(uint32_t frame=0;frame<count;++frame){
        size_t offset=keys+size_t(frame)*32u;
        StorylandUVKeyframe f;
        f.time=readFloat(bytes,offset);
        if(!std::isfinite(f.time)||f.time<0.0f||f.time>duration)return false;
        for(size_t i=0;i<6;++i){
            f.parameters[i]=readFloat(bytes,offset+4+i*4);
            if(!std::isfinite(f.parameters[i]))return false;
        }
        const uint32_t previous=read32(bytes,offset+28);
        if(previous>=count&&previous!=0xFFFFFFFFu)return false;
        previousFrames.push_back(previous);
        decoded.frames.push_back(f);
    }
    // RW stores interleaved node keyframes. The first frame whose predecessor
    // is frame zero identifies the node count; subsequent links own each key.
    size_t nodeCount = 1u;
    if (count > 1u) {
        nodeCount = 1u;
        while (nodeCount < count && previousFrames[nodeCount] != 0u) ++nodeCount;
        if (nodeCount > 8u || nodeCount * 2u > count) return false;
    }
    decoded.nodes.resize(nodeCount);
    std::vector<size_t> nodeForFrame(count, SIZE_MAX);
    std::vector<size_t> lastFrame(nodeCount, SIZE_MAX);
    for (size_t frame = 0; frame < count; ++frame) {
        size_t node = frame;
        if (frame >= nodeCount) {
            const uint32_t previous = previousFrames[frame];
            if (previous >= frame || nodeForFrame[previous] == SIZE_MAX) return false;
            node = nodeForFrame[previous];
            if (lastFrame[node] != previous ||
                decoded.frames[frame].time < decoded.frames[previous].time) return false;
        }
        nodeForFrame[frame] = node;
        lastFrame[node] = frame;
        decoded.nodes[node].push_back(decoded.frames[frame]);
    }
    out=std::move(decoded);return true;
}
inline std::map<std::string,StorylandUVAnimationTrack> dictionary(const std::vector<uint8_t>& bytes,const Chunk& section) {
    std::map<std::string,StorylandUVAnimationTrack> decoded;
    if(section.type!=0x2Bu)return decoded;
    auto list=children(bytes,section);
    if(list.empty()||list[0].type!=1u||list[0].end-list[0].body<4u)return decoded;
    const uint32_t count=read32(bytes,list[0].body);
    if(count>list.size()-1||count>10000u)return decoded;
    for(size_t i=1;i<=count;++i){
        StorylandUVAnimationTrack track;
        if(decodeTrack(bytes,list[i],track))decoded[track.name]=std::move(track);
    }
    return decoded;
}
inline StorylandUVMaterialAnimation material(const std::vector<uint8_t>& bytes,const Chunk& section,
        const std::map<std::string,StorylandUVAnimationTrack>& dictionaryTracks){
    StorylandUVMaterialAnimation result;
    if(section.type!=7u)return result;
    for(const auto& extension:children(bytes,section)){
        if(extension.type!=3u)continue;
        for(const auto& plugin:children(bytes,extension)){
            if(plugin.type!=0x135u)continue;
            for(const auto& st:children(bytes,plugin)){
                if(st.type!=1u||st.end-st.body<4u)continue;
                const uint32_t mask=read32(bytes,st.body);
                size_t nameOffset=st.body+4;
                for(size_t slot=0;slot<8;++slot)if(mask&(1u<<slot)){
                    if(nameOffset>st.end||st.end-nameOffset<32u)break;
                    const std::string name=readName(bytes,nameOffset,32);
                    nameOffset+=32;
                    auto entry=dictionaryTracks.find(name);
                    if(entry==dictionaryTracks.end())continue;
                    const size_t nodeCount = entry->second.nodes.empty() ? 1u : entry->second.nodes.size();
                    for(size_t node=0;node<nodeCount;++node) {
                        const uint32_t channel=entry->second.nodeToChannel[node];
                        if (channel >= 2u) continue;
                        StorylandUVAnimationTrack track;
                        track.name = entry->second.name;
                        track.interpolator = entry->second.interpolator;
                        track.duration = entry->second.duration;
                        track.nodeToChannel.fill(2u);
                        track.nodeToChannel[0] = channel;
                        track.frames = entry->second.nodes.empty() ? entry->second.frames :
                            entry->second.nodes[node];
                        if (!result.animated(channel)) result.channels[channel] = std::move(track);
                        else result.additionalChannels[channel].push_back(std::move(track));
                    }
                }
            }
        }
    }
    return result;
}
inline std::vector<StorylandUVMaterialAnimation> materials(const std::vector<uint8_t>& bytes,const Chunk& geometry,
        const std::map<std::string,StorylandUVAnimationTrack>& dictionaryTracks) {
    for(const auto& list:children(bytes,geometry))if(list.type==8u){
        auto entries=children(bytes,list);
        if(entries.empty()||entries.front().type!=1u||entries.front().end-entries.front().body<4)return {};
        const uint32_t count=read32(bytes,entries.front().body);
        if(count>65536u||size_t(count)>(entries.front().end-entries.front().body-4)/4)return {};
        std::vector<StorylandUVMaterialAnimation> result;
        result.reserve(count);
        size_t cursor=1;
        for(uint32_t i=0;i<count;++i){
            const int32_t reference=int32_t(read32(bytes,entries.front().body+4+size_t(i)*4));
            if(reference>=0&&uint32_t(reference)<i){result.push_back(result[size_t(reference)]);continue;}
            while(cursor<entries.size()&&entries[cursor].type!=7u)++cursor;
            if(reference==-1&&cursor<entries.size())result.push_back(material(bytes,entries[cursor++],dictionaryTracks));
            else result.emplace_back();
        }
        return result;
    }
    return {};
}
} // namespace StorylandRenderWareUV
