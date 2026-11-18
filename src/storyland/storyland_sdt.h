#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
struct StorylandSdtSample {
    uint32_t offset=0,length=0,frequency=0;
    int32_t loopStart=0,loopEnd=-1;
};
struct StorylandSdtIndex {
    size_t stride=0;
    std::vector<StorylandSdtSample> samples;
};
// The caller supplies a complete row in a validated 12-byte or 20-byte layout.
inline StorylandSdtSample storylandReadSdtSample(const std::vector<uint8_t>& bytes,size_t offset,size_t stride) {
    const auto word=[&](size_t position) {
        return uint32_t(bytes[position])|(uint32_t(bytes[position+1])<<8)|
            (uint32_t(bytes[position+2])<<16)|(uint32_t(bytes[position+3])<<24);
    };
    StorylandSdtSample sample{word(offset),word(offset+4),word(offset+8)};
    if(stride==20) {sample.loopStart=int32_t(word(offset+12));sample.loopEnd=int32_t(word(offset+16));}
    return sample;
}
// Validate both layouts without allocating descriptors. A gameplay sound needs
// one row, but malformed unrelated rows and ambiguous layouts must still fail.
inline bool storylandSdtLayout(const std::vector<uint8_t>& bytes,uint64_t rawSize,
    size_t& selectedStride,std::string& error) {
    selectedStride=0;unsigned matches=0;
    for(size_t stride:{size_t(12),size_t(20)}) {
        if(bytes.empty()||bytes.size()%stride||bytes.size()/stride>100000)continue;
        bool valid=true,playable=false;
        for(size_t offset=0;offset<bytes.size();offset+=stride) {
            const auto sample=storylandReadSdtSample(bytes,offset,stride);
            if(stride==20&&sample.length&&sample.loopEnd!=-1&&
                (sample.loopStart<0||sample.loopEnd<sample.loopStart)) {valid=false;break;}
            if(sample.length) {
                playable=true;
                if(sample.offset>rawSize||sample.length>rawSize-sample.offset||sample.frequency<2000||sample.frequency>192000||
                    (stride==20&&sample.length%2)) {valid=false;break;}
            }
        }
        if(valid&&playable) {selectedStride=stride;++matches;}
    }
    if(matches!=1) {
        selectedStride=0;error=matches?"Ambiguous SDT layout: both 12-byte and 20-byte tables validate.":
            "SDT sample ranges/frequencies do not match its RAW bank.";return false;
    }
    error.clear();return true;
}
inline bool storylandDecodeSdtIndex(const std::vector<uint8_t>& bytes,uint64_t rawSize,
    StorylandSdtIndex& index,std::string& error) {
    index={};
    if(!storylandSdtLayout(bytes,rawSize,index.stride,error))return false;
    index.samples.reserve(bytes.size()/index.stride);
    for(size_t offset=0;offset<bytes.size();offset+=index.stride)
        index.samples.push_back(storylandReadSdtSample(bytes,offset,index.stride));
    return true;
}
inline bool storylandDecodeSdtSample(const std::vector<uint8_t>& bytes,uint64_t rawSize,size_t slot,
    StorylandSdtSample& sample,size_t& stride,std::string& error) {
    sample={};
    if(!storylandSdtLayout(bytes,rawSize,stride,error))return false;
    if(slot>=bytes.size()/stride) {
        error="The requested native SDT sample is empty or outside the table.";return false;
    }
    sample=storylandReadSdtSample(bytes,slot*stride,stride);
    if(!sample.length) {error="The requested native SDT sample is empty or outside the table.";return false;}
    return true;
}


namespace storyland_sdt_detail {
inline constexpr size_t maxDecodedPcmSamples=64u*1024u*1024u;
inline uint16_t readLe16(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1])<<8u);
}
inline uint32_t readBe32(const uint8_t* p) {
    return (uint32_t(p[0])<<24u) | (uint32_t(p[1])<<16u) | (uint32_t(p[2])<<8u) | uint32_t(p[3]);
}
inline int16_t clampPcm(int value) {
    return int16_t(std::clamp(value,-32768,32767));
}
inline std::string printableName(const uint8_t* bytes,size_t size) {
    std::string name;
    for(size_t i=0;i<size;++i) {
        const uint8_t c=bytes[i];
        if(c==0)break;
        if(c<0x20||c>0x7e)return {};
        name.push_back(char(c));
    }
    return name;
}
inline bool readFileBounded(const std::filesystem::path& path,std::vector<uint8_t>& out,std::string& error) {
    out.clear();std::error_code ec;
    const uint64_t size=std::filesystem::file_size(path,ec);
    if(ec) {error="Could not determine media file size: "+ec.message();return false;}
    if(size>512ull*1024ull*1024ull||size>uint64_t(std::numeric_limits<size_t>::max())||
        size>uint64_t(std::numeric_limits<std::streamsize>::max())) {
        error="Media file is too large to inspect safely.";return false;
    }
    std::ifstream input(path,std::ios::binary);
    if(!input) {error="Could not open media file.";return false;}
    out.resize(size_t(size));
    if(!out.empty()) {
        input.read(reinterpret_cast<char*>(out.data()),std::streamsize(out.size()));
        if(size_t(input.gcount())!=out.size()) {out.clear();error="Could not read the complete media file.";return false;}
    }
    return true;
}
}

// Shared native decoding keeps the Windows media player and portable mixer on
// the same validated descriptor, ADPCM predictor, loop and sample-rate path.
template<class Clip>
inline bool storylandDecodeVagPcm(const std::vector<uint8_t>& bytes, size_t offset, size_t size,
                                   uint32_t sampleRateHint, Clip& clip,
                                   std::string& errorMessage) {
    if (offset > bytes.size() || size > bytes.size() - offset || size < 16u) {
        errorMessage = "VAG range is truncated.";
        return false;
    }
    const uint8_t* base = bytes.data() + offset;
    const bool hasHeader = size>=0x30u && std::memcmp(base, "VAGp", 4u)==0;
    uint32_t dataSize = hasHeader ? storyland_sdt_detail::readBe32(base + 0x0Cu) : uint32_t(size);
    uint32_t sampleRate = hasHeader ? storyland_sdt_detail::readBe32(base + 0x10u) : sampleRateHint;
    if (sampleRate == 0u) sampleRate = sampleRateHint;
    if (sampleRate < 2000u || sampleRate > 192000u) {
        errorMessage = "VAG sample rate is outside a sane range.";
        return false;
    }
    const size_t headerBytes = hasHeader ? 0x30u : 0u;
    if (dataSize == 0u || dataSize > size - headerBytes) dataSize = uint32_t(size - headerBytes);
    dataSize &= ~15u;
    if (dataSize == 0u) {
        errorMessage = "VAG entry contains no complete ADPCM blocks.";
        return false;
    }

    static constexpr int coefficients[5][2] = {
        {0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}
    };
    const size_t blocks = dataSize / 16u;
    if (blocks > storyland_sdt_detail::maxDecodedPcmSamples / 28u) {
        errorMessage = "VAG entry would decode to too many PCM samples.";
        return false;
    }

    clip.pcm.clear();
    clip.loopStartFrame=-1;
    clip.loopEndFrame=-1;
    clip.pcm.reserve(blocks * 28u);
    int hist1 = 0;
    int hist2 = 0;
    const uint8_t* adpcm = base + headerBytes;
    for (size_t block = 0; block < blocks; ++block) {
        const uint8_t* packet = adpcm + block * 16u;
        const uint8_t predictor = packet[0] >> 4u;
        const uint8_t shift = packet[0] & 0x0Fu;
        const uint8_t flags = packet[1];
        if (predictor > 4u || shift > 12u) {
            errorMessage = "VAG block has an invalid predictor or shift value.";
            clip.pcm.clear();
            return false;
        }
        if ((flags & 4u) && !(flags & 1u)) clip.loopStartFrame=int64_t(clip.pcm.size());
        for (size_t i = 0; i < 28u; ++i) {
            const uint8_t packed = packet[2u + i / 2u];
            int nibble = (i & 1u) == 0u ? int(packed & 0x0Fu) : int(packed >> 4u);
            if (nibble >= 8) nibble -= 16;
            int sample = (nibble * 4096) >> shift;
            sample += (hist1 * coefficients[predictor][0] + hist2 * coefficients[predictor][1] + 32) >> 6;
            const int16_t pcm = storyland_sdt_detail::clampPcm(sample);
            hist2 = hist1;
            hist1 = pcm;
            clip.pcm.push_back(pcm);
        }
        if ((flags & 3u)==3u && clip.loopStartFrame>=0) clip.loopEndFrame=int64_t(clip.pcm.size());
        if ((flags & 0x01u) != 0u || flags == 0x07u) break;
    }

    clip.sampleRate = sampleRate;
    clip.channels = 1;
    clip.durationSeconds = clip.pcm.empty() ? 0.0 : double(clip.pcm.size()) / double(sampleRate);
    if (hasHeader) {
        const std::string headerName = storyland_sdt_detail::printableName(base + 0x20u, 16u);
        if (!headerName.empty()) clip.name = headerName;
    }
    errorMessage.clear();
    return true;
}

template<class Clip>
inline bool storylandLoadSdtPcm(const std::filesystem::path& filePath, size_t clipIndex,
    Clip& output, std::string& errorMessage, uint64_t maximumBytes=4u*1024u*1024u) {
    std::vector<uint8_t> sdt;
    if (!storyland_sdt_detail::readFileBounded(filePath,sdt,errorMessage))return false;
    std::filesystem::path rawPath(filePath);
    rawPath.replace_extension(L".RAW");
    if (!std::filesystem::is_regular_file(rawPath))rawPath.replace_extension(L".raw");
    std::error_code filesystemError;
    const uint64_t rawSize=std::filesystem::file_size(rawPath,filesystemError);
    if(filesystemError) {errorMessage="The matching RAW bank was not found.";return false;}
    StorylandSdtSample sample;
    size_t stride=0;
    if(!storylandDecodeSdtSample(sdt,rawSize,clipIndex,sample,stride,errorMessage))return false;
    const uint32_t offset=sample.offset,length=sample.length,sampleRate=sample.frequency;
    if(length>maximumBytes) {errorMessage="The sound sample is too large for a gameplay effect.";return false;}
    std::ifstream rawFile(rawPath,std::ios::binary);
    if(!rawFile) {errorMessage="Could not open the retail RAW sound bank.";return false;}
    rawFile.seekg(std::streamoff(offset));
    std::vector<uint8_t> sampleBytes(length);
    if(!rawFile.read(reinterpret_cast<char*>(sampleBytes.data()),std::streamsize(length))) {
        errorMessage="The retail RAW sample could not be read.";return false;
    }
    Clip clip;
    clip.sourceIndex=uint32_t(clipIndex);
    clip.name="Sound "+std::to_string(clipIndex);
    clip.sourceOffset=offset;
    clip.sourceSize=length;
    clip.sampleRate=sampleRate;
    if(stride==20u) {
        if(length%2u) {errorMessage="PCM sound length must be even.";return false;}
        clip.channels=1;
        clip.pcm.reserve(length/2u);
        for(size_t sample=0;sample<length/2u;++sample)
            clip.pcm.push_back(int16_t(storyland_sdt_detail::readLe16(sampleBytes.data()+sample*2u)));
        clip.durationSeconds=double(clip.pcm.size())/double(sampleRate);
    } else if(!storylandDecodeVagPcm(sampleBytes,0,length,sampleRate,clip,errorMessage))return false;
    output=std::move(clip);
    errorMessage.clear();
    return true;
}

