#include "storyland_media.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <new>
#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
}

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <mmsystem.h>
#endif

namespace {

constexpr uint64_t kMaxBufferedAudioBytes = 512ull * 1024ull * 1024ull;
constexpr uint64_t kMaxVideoFileBytes = 8ull * 1024ull * 1024ull * 1024ull;
constexpr size_t kMaxAudioClips = 65536u;
constexpr size_t kMaxDecodedPcmSamples = 64u * 1024u * 1024u;

uint16_t readLe16(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8u);
}

uint32_t readLe32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8u) | (uint32_t(p[2]) << 16u) | (uint32_t(p[3]) << 24u);
}

uint32_t readBe32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24u) | (uint32_t(p[1]) << 16u) | (uint32_t(p[2]) << 8u) | uint32_t(p[3]);
}

bool readFileBounded(const std::wstring& path, std::vector<uint8_t>& out, std::string& error, uint64_t maximumBytes = kMaxBufferedAudioBytes) {
    out.clear();
    std::error_code ec;
    const uint64_t size = std::filesystem::file_size(std::filesystem::path(path), ec);
    if (ec) {
        error = "Could not determine media file size: " + ec.message();
        return false;
    }
    if (size > maximumBytes || size > uint64_t(std::numeric_limits<size_t>::max()) ||
        size > uint64_t(std::numeric_limits<std::streamsize>::max())) {
        error = "Media file is too large to inspect safely.";
        return false;
    }
    std::ifstream input(std::filesystem::path(path), std::ios::binary);
    if (!input) {
        error = "Could not open media file.";
        return false;
    }
    out.resize(size_t(size));
    if (!out.empty()) {
        input.read(reinterpret_cast<char*>(out.data()), std::streamsize(out.size()));
        if (size_t(input.gcount()) != out.size()) {
            out.clear();
            error = "Could not read the complete media file.";
            return false;
        }
    }
    return true;
}

std::wstring lowerExtension(const std::wstring& path) {
    std::wstring ext = std::filesystem::path(path).extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) {
        return wchar_t(::towlower(c));
    });
    return ext;
}

std::string printableName(const uint8_t* bytes, size_t size) {
    std::string name;
    for (size_t i = 0; i < size; ++i) {
        const uint8_t c = bytes[i];
        if (c == 0) break;
        if (c < 0x20 || c > 0x7e) return std::string();
        name.push_back(char(c));
    }
    return name;
}

int16_t clampPcm(int value) {
    if (value < -32768) return -32768;
    if (value > 32767) return 32767;
    return int16_t(value);
}

#ifdef _WIN32
std::string hresultText(HRESULT hr) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << uint32_t(hr);
    return out.str();
}

void releaseUnknown(void*& object) {
    if (!object) return;
    reinterpret_cast<IUnknown*>(object)->Release();
    object = nullptr;
}
#endif

struct GameVideoDecoder {
    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    AVFrame* decoded = nullptr;
    AVPacket* packet = nullptr;
    SwsContext* sws = nullptr;
    int videoStream = -1;
    AVRational timeBase{1, 1};
    int64_t startPts = 0;
    bool hasStartPts = false;
    bool inputEof = false;
    bool sentFlush = false;
};

std::string ffmpegError(int code) {
    char text[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, text, sizeof(text));
    return text[0] ? std::string(text) : ("FFmpeg error " + std::to_string(code));
}

void closeGameVideoDecoder(GameVideoDecoder*& decoder) {
    if (!decoder) return;
    if (decoder->sws) sws_freeContext(decoder->sws);
    if (decoder->packet) av_packet_free(&decoder->packet);
    if (decoder->decoded) av_frame_free(&decoder->decoded);
    if (decoder->codec) avcodec_free_context(&decoder->codec);
    if (decoder->format) avformat_close_input(&decoder->format);
    delete decoder;
    decoder = nullptr;
}

bool decodeNextGameVideoFrame(GameVideoDecoder* decoder, StorylandVideoFrame& output,
                              double& positionSeconds, std::string& error) {
    if (!decoder || !decoder->format || !decoder->codec || !decoder->decoded || !decoder->packet) {
        error = "Game video decoder is not initialized.";
        return false;
    }

    for (;;) {
        int receiveResult = avcodec_receive_frame(decoder->codec, decoder->decoded);
        if (receiveResult == 0) {
            const int width = decoder->decoded->width;
            const int height = decoder->decoded->height;
            if (width <= 0 || height <= 0 || width > 8192 || height > 8192) {
                error = "Decoded game video frame has invalid dimensions.";
                return false;
            }

            decoder->sws = sws_getCachedContext(
                decoder->sws, width, height, static_cast<AVPixelFormat>(decoder->decoded->format),
                width, height, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!decoder->sws) {
                error = "Could not create the game video color converter.";
                return false;
            }

            const uint64_t bytes64 = uint64_t(width) * uint64_t(height) * 4ull;
            if (bytes64 > uint64_t(std::numeric_limits<size_t>::max())) {
                error = "Decoded game video frame is too large.";
                return false;
            }
            output.width = uint32_t(width);
            output.height = uint32_t(height);
            output.bgra.resize(size_t(bytes64));
            uint8_t* destinations[4] = {output.bgra.data(), nullptr, nullptr, nullptr};
            int destinationStrides[4] = {width * 4, 0, 0, 0};
            const int scaled = sws_scale(decoder->sws, decoder->decoded->data, decoder->decoded->linesize,
                                         0, height, destinations, destinationStrides);
            if (scaled != height) {
                error = "Could not convert the decoded game video frame to BGRA.";
                return false;
            }

            int64_t pts = decoder->decoded->best_effort_timestamp;
            if (pts == AV_NOPTS_VALUE) pts = decoder->decoded->pts;
            double seconds = positionSeconds;
            if (pts != AV_NOPTS_VALUE) {
                if (decoder->hasStartPts) pts -= decoder->startPts;
                seconds = double(pts) * av_q2d(decoder->timeBase);
                if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
            }
            positionSeconds = seconds;
            output.timestamp100ns = int64_t(std::llround(seconds * 10000000.0));
            av_frame_unref(decoder->decoded);
            error.clear();
            return true;
        }

        if (receiveResult != AVERROR(EAGAIN) && receiveResult != AVERROR_EOF) {
            error = "Game video frame decode failed: " + ffmpegError(receiveResult);
            return false;
        }
        if (receiveResult == AVERROR_EOF) {
            error.clear();
            return false;
        }

        for (;;) {
            if (decoder->inputEof) {
                if (!decoder->sentFlush) {
                    const int sendResult = avcodec_send_packet(decoder->codec, nullptr);
                    decoder->sentFlush = true;
                    if (sendResult < 0 && sendResult != AVERROR_EOF) {
                        error = "Could not flush the game video decoder: " + ffmpegError(sendResult);
                        return false;
                    }
                    break;
                }
                error.clear();
                return false;
            }

            av_packet_unref(decoder->packet);
            const int readResult = av_read_frame(decoder->format, decoder->packet);
            if (readResult == AVERROR_EOF) {
                decoder->inputEof = true;
                continue;
            }
            if (readResult < 0) {
                error = "Could not read the next game video packet: " + ffmpegError(readResult);
                return false;
            }
            if (decoder->packet->stream_index != decoder->videoStream) continue;

            const int sendResult = avcodec_send_packet(decoder->codec, decoder->packet);
            av_packet_unref(decoder->packet);
            if (sendResult == AVERROR(EAGAIN)) break;
            if (sendResult < 0) {
                error = "Could not submit a game video packet to the decoder: " + ffmpegError(sendResult);
                return false;
            }
            break;
        }
    }
}

bool seekGameVideo(GameVideoDecoder* decoder, double targetSeconds, std::string& error) {
    if (!decoder || !decoder->format || decoder->videoStream < 0) {
        error = "Game video decoder is not initialized.";
        return false;
    }
    targetSeconds = std::max(0.0, targetSeconds);
    int64_t targetPts = int64_t(std::llround(targetSeconds / av_q2d(decoder->timeBase)));
    if (decoder->hasStartPts) targetPts += decoder->startPts;
    const int result = av_seek_frame(decoder->format, decoder->videoStream, targetPts, AVSEEK_FLAG_BACKWARD);
    if (result < 0) {
        error = "Game video seek failed: " + ffmpegError(result);
        return false;
    }
    avcodec_flush_buffers(decoder->codec);
    decoder->inputEof = false;
    decoder->sentFlush = false;
    av_packet_unref(decoder->packet);
    av_frame_unref(decoder->decoded);
    error.clear();
    return true;
}

} // namespace

StorylandMediaFile::StorylandMediaFile() = default;

StorylandMediaFile::~StorylandMediaFile() {
    close();
}

StorylandMediaKind StorylandMediaFile::kind() const { return mediaKind; }
const std::wstring& StorylandMediaFile::sourcePath() const { return path; }
const std::vector<StorylandMediaClip>& StorylandMediaFile::clips() const { return audioClips; }
int StorylandMediaFile::selectedClip() const { return selectedAudioClip; }
const StorylandVideoFrame& StorylandMediaFile::videoFrame() const { return frame; }
uint32_t StorylandMediaFile::videoWidth() const { return decodedVideoWidth; }
uint32_t StorylandMediaFile::videoHeight() const { return decodedVideoHeight; }
double StorylandMediaFile::videoDurationSeconds() const { return decodedVideoDuration; }
double StorylandMediaFile::videoPositionSeconds() const { return decodedVideoPosition; }
double StorylandMediaFile::videoFrameRate() const { return decodedVideoFrameRate; }
uint64_t StorylandMediaFile::videoFrameIndex() const {
    if (!std::isfinite(decodedVideoPosition) || !std::isfinite(decodedVideoFrameRate) ||
        decodedVideoPosition <= 0.0 || decodedVideoFrameRate <= 0.0) return 0u;
    return uint64_t(std::llround(decodedVideoPosition * decodedVideoFrameRate));
}
uint64_t StorylandMediaFile::videoFrameCountEstimate() const {
    if (!std::isfinite(decodedVideoDuration) || !std::isfinite(decodedVideoFrameRate) ||
        decodedVideoDuration <= 0.0 || decodedVideoFrameRate <= 0.0) return 0u;
    const long double estimate = std::ceil(static_cast<long double>(decodedVideoDuration) * static_cast<long double>(decodedVideoFrameRate));
    if (estimate <= 0.0L) return 0u;
    if (estimate >= static_cast<long double>(std::numeric_limits<uint64_t>::max())) return std::numeric_limits<uint64_t>::max();
    return uint64_t(estimate);
}

bool StorylandMediaFile::currentVideoFrameIsOverridden() const {
    return videoFrameOverrides.find(videoFrameIndex()) != videoFrameOverrides.end();
}

bool StorylandMediaFile::replaceCurrentVideoFrame(const StorylandVideoFrame& replacement, std::string& errorMessage) {
    if (mediaKind != StorylandMediaKind::Video || !videoReady || frame.width == 0u || frame.height == 0u) {
        errorMessage = "No decoded video frame is selected.";
        return false;
    }
    if (replacement.width != frame.width || replacement.height != frame.height ||
        replacement.bgra.size() != size_t(frame.width) * size_t(frame.height) * 4u) {
        errorMessage = "Replacement frame dimensions do not match the decoded video frame.";
        return false;
    }
    StorylandVideoFrame stored = replacement;
    stored.timestamp100ns = frame.timestamp100ns;
    videoFrameOverrides[videoFrameIndex()] = stored;
    frame = std::move(stored);
    errorMessage.clear();
    return true;
}
bool StorylandMediaFile::videoDecoderReady() const { return videoReady; }
bool StorylandMediaFile::isPlaying() const { return audioPlaying || videoPlaying; }

void StorylandMediaFile::close() {
    stop();
    closeVideoDecoder();
    path.clear();
    mediaKind = StorylandMediaKind::None;
    audioClips.clear();
    selectedAudioClip = -1;
    frame = {};
    decodedVideoWidth = 0;
    decodedVideoHeight = 0;
    decodedVideoStride = 0;
    decodedVideoDuration = 0.0;
    decodedVideoPosition = 0.0;
    decodedVideoFrameRate = 30.0;
    nextVideoDecodeTickMs = 0;
    videoDecodePath.clear();
    videoFrameOverrides.clear();
}

bool StorylandMediaFile::loadFromFile(const std::wstring& filePath, std::string& errorMessage) {
    close();
    const std::wstring ext = lowerExtension(filePath);
    bool ok = false;
    if (ext == L".sdt") ok = loadSdt(filePath, errorMessage);
    else if (ext == L".raw" || ext == L".vag") ok = loadRaw(filePath, errorMessage);
    else if (ext == L".vb") ok = loadVb(filePath, errorMessage);
    else if (ext == L".wav") {
        ok = loadWav(filePath, errorMessage);
        if (!ok) {
            const std::string wavError = errorMessage;
            ok = loadCompressedAudio(filePath, errorMessage);
            if (!ok) errorMessage = wavError + " FFmpeg: " + errorMessage;
        }
    }
    else if (ext == L".at3" || ext == L".aa3" || ext == L".oma" ||
             ext == L".mp3" || ext == L".ogg" || ext == L".flac" ||
             ext == L".aac" || ext == L".m4a" || ext == L".wma" ||
             ext == L".ac3" || ext == L".aif" || ext == L".aiff" ||
             ext == L".adx") ok = loadCompressedAudio(filePath, errorMessage);
    else if (ext == L".pss" || ext == L".pmf" || ext == L".mpg" || ext == L".mpeg" || ext == L".mp4" ||
             ext == L".m4v" || ext == L".wmv" || ext == L".avi" || ext == L".mov" || ext == L".mkv" ||
             ext == L".ts" || ext == L".m2ts" || ext == L".mts" || ext == L".vob" ||
             ext == L".3gp" || ext == L".3g2" || ext == L".webm" || ext == L".ogv" || ext == L".flv") {
        ok = loadVideo(filePath, errorMessage);
    } else {
        errorMessage = "Unsupported media extension.";
        return false;
    }
    if (ok) path = filePath;
    return ok;
}

bool StorylandMediaFile::decodeVag(const std::vector<uint8_t>& bytes, size_t offset, size_t size,
                                   uint32_t sampleRateHint, StorylandMediaClip& clip,
                                   std::string& errorMessage) const {
    if (offset > bytes.size() || size > bytes.size() - offset || size < 0x30u) {
        errorMessage = "VAG range is truncated.";
        return false;
    }
    const uint8_t* base = bytes.data() + offset;
    if (std::memcmp(base, "VAGp", 4u) != 0) {
        errorMessage = "Audio entry does not begin with a VAGp header.";
        return false;
    }

    uint32_t dataSize = readBe32(base + 0x0Cu);
    uint32_t sampleRate = readBe32(base + 0x10u);
    if (sampleRate == 0u) sampleRate = sampleRateHint;
    if (sampleRate < 4000u || sampleRate > 192000u) {
        errorMessage = "VAG sample rate is outside a sane range.";
        return false;
    }
    if (dataSize == 0u || dataSize > size - 0x30u) dataSize = uint32_t(size - 0x30u);
    dataSize &= ~15u;
    if (dataSize == 0u) {
        errorMessage = "VAG entry contains no complete ADPCM blocks.";
        return false;
    }

    static constexpr int coefficients[5][2] = {
        {0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}
    };
    const size_t blocks = dataSize / 16u;
    if (blocks > kMaxDecodedPcmSamples / 28u) {
        errorMessage = "VAG entry would decode to too many PCM samples.";
        return false;
    }

    clip.pcm.clear();
    clip.pcm.reserve(blocks * 28u);
    int hist1 = 0;
    int hist2 = 0;
    const uint8_t* adpcm = base + 0x30u;
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
        for (size_t i = 0; i < 28u; ++i) {
            const uint8_t packed = packet[2u + i / 2u];
            int nibble = (i & 1u) == 0u ? int(packed & 0x0Fu) : int(packed >> 4u);
            if (nibble >= 8) nibble -= 16;
            int sample = (nibble << 12) >> shift;
            sample += (hist1 * coefficients[predictor][0] + hist2 * coefficients[predictor][1] + 32) >> 6;
            const int16_t pcm = clampPcm(sample);
            hist2 = hist1;
            hist1 = pcm;
            clip.pcm.push_back(pcm);
        }
        if ((flags & 0x01u) != 0u || flags == 0x07u) break;
    }

    clip.sampleRate = sampleRate;
    clip.channels = 1;
    clip.durationSeconds = clip.pcm.empty() ? 0.0 : double(clip.pcm.size()) / double(sampleRate);
    const std::string headerName = printableName(base + 0x20u, 16u);
    if (!headerName.empty()) clip.name = headerName;
    errorMessage.clear();
    return true;
}

bool StorylandMediaFile::loadVb(const std::wstring& filePath, std::string& errorMessage) {
    std::vector<uint8_t> bytes;
    if (!readFileBounded(filePath, bytes, errorMessage)) return false;

    static constexpr uint32_t sampleRate = 32000u;
    static constexpr size_t interleaveBytes = 0x2000u;
    static constexpr size_t adpcmBlockBytes = 16u;
    static constexpr size_t samplesPerBlock = 28u;
    static constexpr int coefficients[5][2] = {
        {0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}
    };

    if (bytes.empty() || (bytes.size() % adpcmBlockBytes) != 0u) {
        errorMessage = "VB stream size is not aligned to 16-byte PS2 ADPCM blocks.";
        return false;
    }
    if (bytes.size() < interleaveBytes * 2u) {
        errorMessage = "VB stream is too small to contain one stereo interleave pair.";
        return false;
    }

    const size_t blockCount = bytes.size() / adpcmBlockBytes;
    for (size_t block = 0; block < blockCount; ++block) {
        const uint8_t control = bytes[block * adpcmBlockBytes];
        const uint8_t predictor = control >> 4u;
        const uint8_t shift = control & 0x0Fu;
        if (predictor > 4u || shift > 12u) {
            std::ostringstream message;
            message << "VB stream is not valid raw PS2 ADPCM: invalid block at 0x"
                    << std::hex << (block * adpcmBlockBytes) << ".";
            errorMessage = message.str();
            return false;
        }
    }

    const uint64_t decodedSamplesPerChannel =
        (uint64_t(bytes.size()) / uint64_t(adpcmBlockBytes) / 2ull) * uint64_t(samplesPerBlock);
    const uint64_t interleavedSampleCount = decodedSamplesPerChannel * 2ull;
    if (interleavedSampleCount > uint64_t(kMaxDecodedPcmSamples)) {
        errorMessage = "VB stream would decode to too many PCM samples.";
        return false;
    }

    StorylandMediaClip clip;
    clip.name = std::filesystem::path(filePath).stem().string();
    clip.sourceOffset = 0u;
    clip.sourceSize = bytes.size();
    clip.sampleRate = sampleRate;
    clip.channels = 2u;
    clip.pcm.reserve(size_t(interleavedSampleCount));

    std::array<int, 2> hist1{0, 0};
    std::array<int, 2> hist2{0, 0};
    std::array<std::vector<int16_t>, 2> channelChunk;
    const size_t blocksPerInterleave = interleaveBytes / adpcmBlockBytes;
    for (auto& channel : channelChunk) channel.reserve(blocksPerInterleave * samplesPerBlock);

    auto decodeChannelChunk = [&](size_t byteOffset, size_t byteCount, int channelIndex) -> bool {
        channelChunk[size_t(channelIndex)].clear();
        const size_t chunkBlocks = byteCount / adpcmBlockBytes;
        for (size_t block = 0; block < chunkBlocks; ++block) {
            const uint8_t* packet = bytes.data() + byteOffset + block * adpcmBlockBytes;
            const uint8_t predictor = packet[0] >> 4u;
            const uint8_t shift = packet[0] & 0x0Fu;
            for (size_t i = 0; i < samplesPerBlock; ++i) {
                const uint8_t packed = packet[2u + i / 2u];
                int nibble = (i & 1u) == 0u ? int(packed & 0x0Fu) : int(packed >> 4u);
                if (nibble >= 8) nibble -= 16;
                int sample = (nibble << 12) >> shift;
                sample += (hist1[size_t(channelIndex)] * coefficients[predictor][0] +
                           hist2[size_t(channelIndex)] * coefficients[predictor][1] + 32) >> 6;
                const int16_t pcm = clampPcm(sample);
                hist2[size_t(channelIndex)] = hist1[size_t(channelIndex)];
                hist1[size_t(channelIndex)] = pcm;
                channelChunk[size_t(channelIndex)].push_back(pcm);
            }
        }
        return true;
    };

    size_t offset = 0u;
    while (offset < bytes.size()) {
        const size_t leftBytes = std::min(interleaveBytes, bytes.size() - offset);
        if ((leftBytes % adpcmBlockBytes) != 0u) {
            errorMessage = "VB left-channel interleave is truncated.";
            return false;
        }
        if (!decodeChannelChunk(offset, leftBytes, 0)) return false;
        offset += leftBytes;

        if (offset >= bytes.size()) {
            errorMessage = "VB stream ends after a left-channel interleave without matching right-channel data.";
            return false;
        }
        const size_t rightBytes = std::min(interleaveBytes, bytes.size() - offset);
        if ((rightBytes % adpcmBlockBytes) != 0u) {
            errorMessage = "VB right-channel interleave is truncated.";
            return false;
        }
        if (!decodeChannelChunk(offset, rightBytes, 1)) return false;
        offset += rightBytes;

        if (channelChunk[0].size() != channelChunk[1].size()) {
            errorMessage = "VB stereo interleave channels contain different sample counts.";
            return false;
        }
        for (size_t i = 0; i < channelChunk[0].size(); ++i) {
            clip.pcm.push_back(channelChunk[0][i]);
            clip.pcm.push_back(channelChunk[1][i]);
        }
    }

    if (clip.pcm.empty()) {
        errorMessage = "VB stream decoded to no PCM samples.";
        return false;
    }

    clip.durationSeconds = double(clip.pcm.size() / 2u) / double(sampleRate);
    audioClips.clear();
    audioClips.push_back(std::move(clip));
    selectedAudioClip = 0;
    mediaKind = StorylandMediaKind::Audio;
    errorMessage.clear();
    return true;
}

bool StorylandMediaFile::loadSdt(const std::wstring& filePath, std::string& errorMessage) {
    std::vector<uint8_t> sdt;
    if (!readFileBounded(filePath, sdt, errorMessage)) return false;
    if (sdt.empty() || (sdt.size() % 12u) != 0u) {
        errorMessage = "SDT size is not a whole number of 12-byte entries.";
        return false;
    }
    const size_t entryCount = sdt.size() / 12u;
    if (entryCount > kMaxAudioClips) {
        errorMessage = "SDT contains too many audio entries to inspect safely.";
        return false;
    }

    std::filesystem::path rawPath(filePath);
    rawPath.replace_extension(L".RAW");
    if (!std::filesystem::exists(rawPath)) {
        rawPath.replace_extension(L".raw");
    }
    if (!std::filesystem::exists(rawPath)) {
        errorMessage = "The matching RAW sound bank was not found beside the SDT file.";
        return false;
    }

    std::vector<uint8_t> raw;
    if (!readFileBounded(rawPath.wstring(), raw, errorMessage)) return false;
    audioClips.clear();
    audioClips.reserve(entryCount);
    for (size_t index = 0; index < entryCount; ++index) {
        const uint8_t* row = sdt.data() + index * 12u;
        const uint32_t offset = readLe32(row + 0u);
        const uint32_t size = readLe32(row + 4u);
        const uint32_t sampleRate = readLe32(row + 8u);
        if (size == 0u) continue;
        if (offset > raw.size() || size > raw.size() - offset) {
            errorMessage = "SDT entry " + std::to_string(index) + " points outside the RAW sound bank.";
            audioClips.clear();
            return false;
        }
        StorylandMediaClip clip;
        clip.sourceOffset = offset;
        clip.sourceSize = size;
        clip.sampleRate = sampleRate;
        clip.name = "Sound " + std::to_string(index);
        std::string decodeError;
        if (!decodeVag(raw, offset, size, sampleRate, clip, decodeError)) {
            errorMessage = "SDT entry " + std::to_string(index) + ": " + decodeError;
            audioClips.clear();
            return false;
        }
        if (clip.name.empty()) clip.name = "Sound " + std::to_string(index);
        audioClips.push_back(std::move(clip));
    }
    if (audioClips.empty()) {
        errorMessage = "SDT contains no playable VAG entries.";
        return false;
    }
    selectedAudioClip = 0;
    mediaKind = StorylandMediaKind::AudioArchive;
    errorMessage.clear();
    return true;
}

bool StorylandMediaFile::loadRaw(const std::wstring& filePath, std::string& errorMessage) {
    std::vector<uint8_t> raw;
    if (!readFileBounded(filePath, raw, errorMessage)) return false;
    if (raw.size() < 0x30u) {
        errorMessage = "RAW/VAG file is too small to contain a VAG stream.";
        return false;
    }

    std::vector<size_t> offsets;
    offsets.reserve(64u);
    const std::array<uint8_t, 4> vagMagic{{'V', 'A', 'G', 'p'}};
    auto cursor = raw.begin();
    while (cursor != raw.end()) {
        cursor = std::search(cursor, raw.end(), vagMagic.begin(), vagMagic.end());
        if (cursor == raw.end()) break;
        const size_t offset = size_t(std::distance(raw.begin(), cursor));
        if (raw.size() - offset >= 0x30u) offsets.push_back(offset);
        if (offsets.size() > kMaxAudioClips) {
            errorMessage = "RAW sound bank contains too many VAG streams to inspect safely.";
            return false;
        }
        cursor += 4;
    }

    if (offsets.empty()) {
        errorMessage = "No VAGp audio stream was found in the RAW/VAG file.";
        return false;
    }

    audioClips.clear();
    audioClips.reserve(offsets.size());
    for (size_t index = 0; index < offsets.size(); ++index) {
        const size_t offset = offsets[index];
        const uint32_t declaredDataSize = readBe32(raw.data() + offset + 0x0Cu);
        const size_t nextOffset = index + 1u < offsets.size() ? offsets[index + 1u] : raw.size();
        const uint64_t declaredTotal = 0x30ull + uint64_t(declaredDataSize);
        size_t available = nextOffset > offset ? nextOffset - offset : raw.size() - offset;
        if (declaredDataSize != 0u && declaredTotal <= uint64_t(raw.size() - offset)) {
            available = std::min<size_t>(available, size_t(declaredTotal));
        }
        if (available < 0x40u) continue;

        StorylandMediaClip clip;
        clip.name = "Sound " + std::to_string(index);
        clip.sourceOffset = offset;
        clip.sourceSize = available;
        std::string decodeError;
        if (!decodeVag(raw, offset, available, 0u, clip, decodeError)) continue;
        if (clip.name.empty()) clip.name = "Sound " + std::to_string(index);
        audioClips.push_back(std::move(clip));
    }

    if (audioClips.empty()) {
        errorMessage = "VAG signatures were found, but none contained a valid bounded audio stream.";
        return false;
    }

    selectedAudioClip = 0;
    mediaKind = audioClips.size() > 1u ? StorylandMediaKind::AudioArchive : StorylandMediaKind::Audio;
    errorMessage.clear();
    return true;
}

bool StorylandMediaFile::loadWav(const std::wstring& filePath, std::string& errorMessage) {
    std::vector<uint8_t> wav;
    if (!readFileBounded(filePath, wav, errorMessage)) return false;
    if (wav.size() < 12u || std::memcmp(wav.data(), "RIFF", 4u) != 0 || std::memcmp(wav.data() + 8u, "WAVE", 4u) != 0) {
        errorMessage = "Not a RIFF/WAVE file.";
        return false;
    }
    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t sampleRate = 0;
    size_t dataOffset = 0, dataSize = 0;
    size_t cursor = 12u;
    while (cursor <= wav.size() && wav.size() - cursor >= 8u) {
        const uint32_t chunkSize = readLe32(wav.data() + cursor + 4u);
        const size_t payload = cursor + 8u;
        if (payload > wav.size() || chunkSize > wav.size() - payload) {
            errorMessage = "WAVE chunk extends beyond the file.";
            return false;
        }
        if (std::memcmp(wav.data() + cursor, "fmt ", 4u) == 0 && chunkSize >= 16u) {
            format = readLe16(wav.data() + payload + 0u);
            channels = readLe16(wav.data() + payload + 2u);
            sampleRate = readLe32(wav.data() + payload + 4u);
            bits = readLe16(wav.data() + payload + 14u);
        } else if (std::memcmp(wav.data() + cursor, "data", 4u) == 0) {
            dataOffset = payload;
            dataSize = chunkSize;
        }
        const size_t step = 8u + size_t(chunkSize) + (chunkSize & 1u);
        if (step < 8u || step > wav.size() - cursor) break;
        cursor += step;
    }
    if (format != 1u || (channels != 1u && channels != 2u) ||
        (bits != 8u && bits != 16u) || sampleRate < 4000u || sampleRate > 192000u || dataSize == 0u) {
        errorMessage = "Only bounded PCM 8/16-bit mono or stereo WAVE files are supported.";
        return false;
    }
    const size_t sampleBytes = bits / 8u;
    const size_t frameBytes = sampleBytes * channels;
    const size_t frames = dataSize / frameBytes;
    if (frames > kMaxDecodedPcmSamples / channels) {
        errorMessage = "WAVE file contains too many samples to decode safely.";
        return false;
    }
    StorylandMediaClip clip;
    clip.name = std::filesystem::path(filePath).filename().string();
    clip.sourceOffset = dataOffset;
    clip.sourceSize = dataSize;
    clip.sampleRate = sampleRate;
    clip.channels = channels;
    clip.pcm.reserve(frames * channels);
    for (size_t i = 0; i < frames * channels; ++i) {
        if (bits == 16u) {
            clip.pcm.push_back(int16_t(readLe16(wav.data() + dataOffset + i * 2u)));
        } else {
            clip.pcm.push_back(int16_t((int(wav[dataOffset + i]) - 128) << 8));
        }
    }
    clip.durationSeconds = double(frames) / double(sampleRate);
    audioClips = {std::move(clip)};
    selectedAudioClip = 0;
    mediaKind = StorylandMediaKind::Audio;
    errorMessage.clear();
    return true;
}


static bool appendAudioFrameS16(const AVFrame* frame, std::vector<int16_t>& output, std::string& error) {
    if (!frame) {
        error = "FFmpeg returned an empty audio frame.";
        return false;
    }
    const int channels = frame->ch_layout.nb_channels;
    const int samples = frame->nb_samples;
    if (channels <= 0 || channels > 32 || samples < 0) {
        error = "Decoded audio frame has invalid channel/sample counts.";
        return false;
    }
    if (samples == 0) return true;

    const AVSampleFormat format = static_cast<AVSampleFormat>(frame->format);
    const bool planar = av_sample_fmt_is_planar(format) != 0;
    const AVSampleFormat packed = planar ? av_get_packed_sample_fmt(format) : format;
    if (packed == AV_SAMPLE_FMT_NONE) {
        error = "Decoded audio uses an unsupported FFmpeg sample format.";
        return false;
    }

    const uint64_t additions = uint64_t(samples) * uint64_t(channels);
    if (additions > kMaxDecodedPcmSamples || output.size() > kMaxDecodedPcmSamples - size_t(additions)) {
        error = "Decoded audio would exceed Storyland's PCM safety limit.";
        return false;
    }
    output.reserve(output.size() + size_t(additions));

    auto sampleToS16 = [&](const uint8_t* data, int index) -> int16_t {
        switch (packed) {
        case AV_SAMPLE_FMT_U8: {
            const int value = int(reinterpret_cast<const uint8_t*>(data)[index]) - 128;
            return int16_t(value << 8);
        }
        case AV_SAMPLE_FMT_S16:
            return reinterpret_cast<const int16_t*>(data)[index];
        case AV_SAMPLE_FMT_S32: {
            const int32_t value = reinterpret_cast<const int32_t*>(data)[index];
            return int16_t(value >> 16);
        }
        case AV_SAMPLE_FMT_S64: {
            const int64_t value = reinterpret_cast<const int64_t*>(data)[index];
            return int16_t(value >> 48);
        }
        case AV_SAMPLE_FMT_FLT: {
            float value = reinterpret_cast<const float*>(data)[index];
            if (!std::isfinite(value)) value = 0.0f;
            value = std::max(-1.0f, std::min(1.0f, value));
            return clampPcm(int(std::lrint(double(value) * 32767.0)));
        }
        case AV_SAMPLE_FMT_DBL: {
            double value = reinterpret_cast<const double*>(data)[index];
            if (!std::isfinite(value)) value = 0.0;
            value = std::max(-1.0, std::min(1.0, value));
            return clampPcm(int(std::lrint(value * 32767.0)));
        }
        default:
            return 0;
        }
    };

    switch (packed) {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S64:
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_DBL:
        break;
    default:
        error = "Decoded audio sample format is not supported by Storyland.";
        return false;
    }

    for (int sample = 0; sample < samples; ++sample) {
        for (int channel = 0; channel < channels; ++channel) {
            if (planar) {
                if (!frame->extended_data || !frame->extended_data[channel]) {
                    error = "Decoded planar audio frame is missing a channel plane.";
                    return false;
                }
                output.push_back(sampleToS16(frame->extended_data[channel], sample));
            } else {
                if (!frame->extended_data || !frame->extended_data[0]) {
                    error = "Decoded packed audio frame has no sample data.";
                    return false;
                }
                output.push_back(sampleToS16(frame->extended_data[0], sample * channels + channel));
            }
        }
    }
    return true;
}

bool StorylandMediaFile::loadCompressedAudio(const std::wstring& filePath, std::string& errorMessage) {
    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* decoded = nullptr;

    auto cleanup = [&]() {
        if (packet) av_packet_free(&packet);
        if (decoded) av_frame_free(&decoded);
        if (codec) avcodec_free_context(&codec);
        if (format) avformat_close_input(&format);
    };

    const std::string utf8Path = std::filesystem::path(filePath).u8string();
    int result = avformat_open_input(&format, utf8Path.c_str(), nullptr, nullptr);
    if (result < 0) {
        errorMessage = "FFmpeg could not open this audio file: " + ffmpegError(result);
        cleanup();
        return false;
    }
    result = avformat_find_stream_info(format, nullptr);
    if (result < 0) {
        errorMessage = "FFmpeg could not read the audio stream information: " + ffmpegError(result);
        cleanup();
        return false;
    }

    const int streamIndex = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        errorMessage = "No decodable audio stream was found in this file.";
        cleanup();
        return false;
    }
    AVStream* stream = format->streams[streamIndex];
    const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        errorMessage = "FFmpeg has no decoder for this audio codec.";
        cleanup();
        return false;
    }

    codec = avcodec_alloc_context3(decoder);
    if (!codec) {
        errorMessage = "Could not allocate the FFmpeg audio decoder.";
        cleanup();
        return false;
    }
    result = avcodec_parameters_to_context(codec, stream->codecpar);
    if (result < 0 || (result = avcodec_open2(codec, decoder, nullptr)) < 0) {
        errorMessage = "Could not initialize the FFmpeg audio decoder: " + ffmpegError(result);
        cleanup();
        return false;
    }

    packet = av_packet_alloc();
    decoded = av_frame_alloc();
    if (!packet || !decoded) {
        errorMessage = "Could not allocate FFmpeg audio decode buffers.";
        cleanup();
        return false;
    }

    StorylandMediaClip clip;
    clip.name = std::filesystem::path(filePath).stem().u8string();
    clip.sourceOffset = 0;
    std::error_code sizeError;
    clip.sourceSize = std::filesystem::file_size(std::filesystem::path(filePath), sizeError);
    if (sizeError) clip.sourceSize = 0;

    uint32_t sampleRate = 0;
    uint16_t channels = 0;
    bool sentFlush = false;
    bool inputEof = false;

    for (;;) {
        result = avcodec_receive_frame(codec, decoded);
        if (result == 0) {
            const int frameChannels = decoded->ch_layout.nb_channels;
            const int frameRate = decoded->sample_rate > 0 ? decoded->sample_rate : codec->sample_rate;
            if (frameRate <= 0 || frameRate > 384000 || frameChannels <= 0 || frameChannels > 32) {
                errorMessage = "Decoded audio frame reports invalid sample-rate/channel metadata.";
                cleanup();
                return false;
            }
            if (sampleRate == 0) {
                sampleRate = uint32_t(frameRate);
                channels = uint16_t(frameChannels);
            } else if (sampleRate != uint32_t(frameRate) || channels != uint16_t(frameChannels)) {
                errorMessage = "Audio stream changes sample rate or channel count mid-stream, which Storyland cannot play safely.";
                cleanup();
                return false;
            }
            if (!appendAudioFrameS16(decoded, clip.pcm, errorMessage)) {
                cleanup();
                return false;
            }
            av_frame_unref(decoded);
            continue;
        }
        if (result == AVERROR_EOF) break;
        if (result != AVERROR(EAGAIN)) {
            errorMessage = "Audio decode failed: " + ffmpegError(result);
            cleanup();
            return false;
        }

        if (inputEof) {
            if (!sentFlush) {
                result = avcodec_send_packet(codec, nullptr);
                sentFlush = true;
                if (result < 0 && result != AVERROR_EOF) {
                    errorMessage = "Could not flush the audio decoder: " + ffmpegError(result);
                    cleanup();
                    return false;
                }
                continue;
            }
            break;
        }

        av_packet_unref(packet);
        result = av_read_frame(format, packet);
        if (result == AVERROR_EOF) {
            inputEof = true;
            continue;
        }
        if (result < 0) {
            errorMessage = "Could not read the next audio packet: " + ffmpegError(result);
            cleanup();
            return false;
        }
        if (packet->stream_index != streamIndex) {
            av_packet_unref(packet);
            continue;
        }
        result = avcodec_send_packet(codec, packet);
        av_packet_unref(packet);
        if (result < 0 && result != AVERROR(EAGAIN)) {
            errorMessage = "Could not submit an audio packet to the decoder: " + ffmpegError(result);
            cleanup();
            return false;
        }
    }

    cleanup();
    if (clip.pcm.empty() || sampleRate == 0 || channels == 0) {
        errorMessage = "The audio stream decoded to no PCM samples.";
        return false;
    }

    clip.sampleRate = sampleRate;
    clip.channels = channels;
    clip.durationSeconds = double(clip.pcm.size()) / (double(sampleRate) * double(channels));
    audioClips.clear();
    audioClips.push_back(std::move(clip));
    selectedAudioClip = 0;
    mediaKind = StorylandMediaKind::Audio;
    errorMessage.clear();
    return true;
}

bool StorylandMediaFile::selectClip(size_t index, std::string& errorMessage) {
    if (index >= audioClips.size()) {
        errorMessage = "Audio selection is out of range.";
        return false;
    }
    stopWaveOut();
    selectedAudioClip = int(index);
    errorMessage.clear();
    return true;
}

bool StorylandMediaFile::startWaveOut(const StorylandMediaClip& clip, std::string& errorMessage) {
#ifdef _WIN32
    stopWaveOut();
    if (clip.pcm.empty() || clip.sampleRate == 0u || (clip.channels != 1u && clip.channels != 2u)) {
        errorMessage = "Selected sound has no decoded PCM audio.";
        return false;
    }
    if (clip.pcm.size() > std::numeric_limits<DWORD>::max() / sizeof(int16_t)) {
        errorMessage = "Selected sound is too large for the Windows wave output API.";
        return false;
    }
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = clip.channels;
    format.nSamplesPerSec = clip.sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = WORD(format.nChannels * sizeof(int16_t));
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    HWAVEOUT handle = nullptr;
    MMRESULT result = waveOutOpen(&handle, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR || !handle) {
        errorMessage = "Windows could not open an audio output device.";
        return false;
    }

    waveBytes.resize(clip.pcm.size() * sizeof(int16_t));
    std::memcpy(waveBytes.data(), clip.pcm.data(), waveBytes.size());
    WAVEHDR* header = new WAVEHDR{};
    header->lpData = reinterpret_cast<LPSTR>(waveBytes.data());
    header->dwBufferLength = DWORD(waveBytes.size());
    if (waveOutPrepareHeader(handle, header, sizeof(*header)) != MMSYSERR_NOERROR ||
        waveOutWrite(handle, header, sizeof(*header)) != MMSYSERR_NOERROR) {
        waveOutReset(handle);
        waveOutUnprepareHeader(handle, header, sizeof(*header));
        waveOutClose(handle);
        delete header;
        waveBytes.clear();
        errorMessage = "Windows could not queue the decoded audio buffer.";
        return false;
    }
    waveOutHandle = handle;
    waveHeader = header;
    audioPlaying = true;
    errorMessage.clear();
    return true;
#else
    (void)clip;
    errorMessage = "Audio playback is available in the Windows build.";
    return false;
#endif
}

void StorylandMediaFile::stopWaveOut() {
#ifdef _WIN32
    HWAVEOUT handle = reinterpret_cast<HWAVEOUT>(waveOutHandle);
    WAVEHDR* header = reinterpret_cast<WAVEHDR*>(waveHeader);
    if (handle) {
        waveOutReset(handle);
        if (header) waveOutUnprepareHeader(handle, header, sizeof(*header));
        waveOutClose(handle);
    }
    delete header;
    waveOutHandle = nullptr;
    waveHeader = nullptr;
    waveBytes.clear();
#endif
    audioPlaying = false;
}

bool StorylandMediaFile::play(std::string& errorMessage) {
    if (mediaKind == StorylandMediaKind::Audio || mediaKind == StorylandMediaKind::AudioArchive) {
        if (selectedAudioClip < 0 || size_t(selectedAudioClip) >= audioClips.size()) {
            errorMessage = "Select a sound first.";
            return false;
        }
        return startWaveOut(audioClips[size_t(selectedAudioClip)], errorMessage);
    }
    if (mediaKind == StorylandMediaKind::Video) {
        if (!videoReady) {
            errorMessage = "Video decoder is not ready.";
            return false;
        }
#ifdef _WIN32
        if (!sourceReader && !gameVideoDecoder) {
            errorMessage = "Video decoder is not ready.";
            return false;
        }
        if (decodedVideoDuration > 0.0 && decodedVideoPosition >= decodedVideoDuration - 0.05) {
            if (gameVideoDecoder) {
                if (!seekGameVideo(reinterpret_cast<GameVideoDecoder*>(gameVideoDecoder), 0.0, errorMessage)) return false;
            } else {
                PROPVARIANT position;
                PropVariantInit(&position);
                position.vt = VT_I8;
                position.hVal.QuadPart = 0;
                const HRESULT seekResult = reinterpret_cast<IMFSourceReader*>(sourceReader)->SetCurrentPosition(GUID_NULL, position);
                PropVariantClear(&position);
                if (FAILED(seekResult)) {
                    errorMessage = "Video could not be rewound (" + hresultText(seekResult) + ").";
                    return false;
                }
            }
            decodedVideoPosition = 0.0;
            frame = {};
        }
        nextVideoDecodeTickMs = 0;
#endif
        videoPlaying = true;
        errorMessage.clear();
        return true;
    }
    errorMessage = "No playable media is open.";
    return false;
}

void StorylandMediaFile::stop() {
    stopWaveOut();
    videoPlaying = false;
}

#ifdef _WIN32
static bool extractPmfAvcElementaryStream(const std::wstring& sourcePath,
                                          const std::filesystem::path& outputPath,
                                          std::string& error) {
    std::ifstream input(std::filesystem::path(sourcePath), std::ios::binary);
    if (!input) {
        error = "Could not open PMF for AVC extraction.";
        return false;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff fileSizeSigned = input.tellg();
    if (fileSizeSigned < 0x20) {
        error = "PMF is truncated.";
        return false;
    }
    const uint64_t fileSize = uint64_t(fileSizeSigned);
    input.seekg(0, std::ios::beg);

    std::array<uint8_t, 16> header{};
    input.read(reinterpret_cast<char*>(header.data()), std::streamsize(header.size()));
    if (size_t(input.gcount()) != header.size() || std::memcmp(header.data(), "PSMF", 4u) != 0) {
        error = "PMF does not contain a valid PSMF header.";
        return false;
    }

    const uint32_t streamOffset = readBe32(header.data() + 8u);
    if (streamOffset < 0x20u || uint64_t(streamOffset) >= fileSize) {
        error = "PMF MPEG stream offset is invalid.";
        return false;
    }

    input.seekg(std::streamoff(streamOffset), std::ios::beg);
    std::vector<uint8_t> programStream(size_t(fileSize - uint64_t(streamOffset)));
    if (!programStream.empty()) {
        input.read(reinterpret_cast<char*>(programStream.data()), std::streamsize(programStream.size()));
        if (size_t(input.gcount()) != programStream.size()) {
            error = "PMF MPEG program stream is truncated.";
            return false;
        }
    }

    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not create the temporary AVC stream for PMF playback.";
        return false;
    }

    size_t pos = 0;
    uint64_t written = 0;
    bool foundVideoPacket = false;

    auto findStartCode = [&](size_t from) -> size_t {
        for (size_t i = from; i + 3u < programStream.size(); ++i) {
            if (programStream[i] == 0x00u && programStream[i + 1u] == 0x00u && programStream[i + 2u] == 0x01u)
                return i;
        }
        return programStream.size();
    };

    while (pos + 6u <= programStream.size()) {
        const size_t packet = findStartCode(pos);
        if (packet + 6u > programStream.size()) break;

        const uint8_t streamId = programStream[packet + 3u];

        if (streamId == 0xBAu) {
            if (packet + 14u > programStream.size()) break;
            const size_t stuffing = size_t(programStream[packet + 13u] & 0x07u);
            pos = std::min(programStream.size(), packet + 14u + stuffing);
            continue;
        }

        if (streamId == 0xB9u) {
            pos = packet + 4u;
            continue;
        }

        const uint16_t packetLength = uint16_t(programStream[packet + 4u] << 8u) |
                                      uint16_t(programStream[packet + 5u]);
        size_t packetEnd = 0;
        if (packetLength != 0u) {
            packetEnd = packet + 6u + size_t(packetLength);
            if (packetEnd > programStream.size()) packetEnd = programStream.size();
        } else {
            packetEnd = findStartCode(packet + 6u);
            if (packetEnd <= packet + 6u) packetEnd = programStream.size();
        }

        if (streamId >= 0xE0u && streamId <= 0xEFu) {
            foundVideoPacket = true;
            size_t payload = packet + 6u;
            if (payload + 3u <= packetEnd) {
                const uint8_t markerBits = programStream[payload] & 0xC0u;
                if (markerBits == 0x80u) {
                    const size_t pesHeaderDataLength = size_t(programStream[payload + 2u]);
                    const size_t candidate = payload + 3u + pesHeaderDataLength;
                    if (candidate <= packetEnd) payload = candidate;
                    else payload = packetEnd;
                } else {
                    while (payload < packetEnd && programStream[payload] == 0xFFu) ++payload;
                    if (payload + 2u <= packetEnd && (programStream[payload] & 0xC0u) == 0x40u) payload += 2u;
                    if (payload < packetEnd) {
                        if ((programStream[payload] & 0xF0u) == 0x20u) payload += std::min<size_t>(5u, packetEnd - payload);
                        else if ((programStream[payload] & 0xF0u) == 0x30u) payload += std::min<size_t>(10u, packetEnd - payload);
                        else if (programStream[payload] == 0x0Fu) ++payload;
                    }
                }
            }

            if (payload < packetEnd) {
                const size_t bytes = packetEnd - payload;
                output.write(reinterpret_cast<const char*>(programStream.data() + payload), std::streamsize(bytes));
                if (!output) {
                    error = "Could not write the temporary AVC stream for PMF playback.";
                    return false;
                }
                written += uint64_t(bytes);
            }
        }

        pos = packetEnd > packet ? packetEnd : packet + 4u;
    }

    output.close();
    if (!foundVideoPacket || written < 16u) {
        error = "PMF does not contain a usable AVC video stream.";
        return false;
    }

    std::ifstream verify(outputPath, std::ios::binary);
    std::array<uint8_t, 4096> probe{};
    verify.read(reinterpret_cast<char*>(probe.data()), std::streamsize(probe.size()));
    const size_t probeSize = size_t(std::max<std::streamsize>(0, verify.gcount()));
    bool hasAnnexB = false;
    for (size_t i = 0; i + 4u <= probeSize; ++i) {
        if (probe[i] == 0x00u && probe[i + 1u] == 0x00u &&
            ((probe[i + 2u] == 0x01u) ||
             (i + 4u <= probeSize && probe[i + 2u] == 0x00u && probe[i + 3u] == 0x01u))) {
            hasAnnexB = true;
            break;
        }
    }
    if (!hasAnnexB) {
        error = "PMF video packets were found, but the AVC payload is not Annex-B H.264.";
        return false;
    }

    return true;
}

static bool makeMpegDecodeAlias(const std::wstring& sourcePath, const std::wstring& extension,
                                std::wstring& aliasPath, std::string& error) {
    wchar_t tempDirectory[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, tempDirectory);
    if (length == 0 || length >= MAX_PATH) {
        error = "Could not obtain the Windows temporary directory.";
        return false;
    }

    const ULONGLONG tick = GetTickCount64();
    const DWORD processId = GetCurrentProcessId();
    std::filesystem::path privateDirectory;
    std::error_code ec;
    bool created = false;
    for (unsigned attempt = 0; attempt < 64u; ++attempt) {
        privateDirectory = std::filesystem::path(tempDirectory) /
            (L"StorylandVideo_" + std::to_wstring(processId) + L"_" +
             std::to_wstring(tick) + L"_" + std::to_wstring(attempt));
        ec.clear();
        created = std::filesystem::create_directory(privateDirectory, ec);
        if (created) break;
        if (ec && ec != std::errc::file_exists) {
            error = "Could not create a private temporary directory for video playback.";
            return false;
        }
    }
    if (!created) {
        error = "Could not allocate a private temporary directory for video playback.";
        return false;
    }

    const std::filesystem::path alias = privateDirectory /
        (extension == L".pmf" ? L"stream.h264" : L"stream.mpg");

    bool ok = false;
    if (extension == L".pmf") {
        ok = extractPmfAvcElementaryStream(sourcePath, alias, error);
    } else {
        ok = CopyFileW(sourcePath.c_str(), alias.c_str(), TRUE) != FALSE;
        if (!ok) error = "Could not create the temporary MPEG alias for video playback.";
    }

    if (!ok) {
        std::filesystem::remove(alias, ec);
        std::filesystem::remove(privateDirectory, ec);
        return false;
    }

    aliasPath = alias.wstring();
    return true;
}
#endif

bool StorylandMediaFile::loadVideo(const std::wstring& filePath, std::string& errorMessage) {
#ifdef _WIN32
    std::error_code ec;
    const uint64_t fileSize = std::filesystem::file_size(std::filesystem::path(filePath), ec);
    if (ec || fileSize == 0u || fileSize > kMaxVideoFileBytes) {
        errorMessage = ec ? "Could not determine video file size." : "Video file is empty or too large to inspect safely.";
        return false;
    }

    const std::wstring extension = lowerExtension(filePath);
    if (extension == L".pmf" || extension == L".pss") {
        AVFormatContext* format = nullptr;
        const std::string utf8Path = std::filesystem::path(filePath).u8string();
        int ff = avformat_open_input(&format, utf8Path.c_str(), nullptr, nullptr);
        if (ff < 0 || !format) {
            if (format) avformat_close_input(&format);
            errorMessage = "FFmpeg could not open this game video: " + ffmpegError(ff);
            return false;
        }
        ff = avformat_find_stream_info(format, nullptr);
        if (ff < 0) {
            avformat_close_input(&format);
            errorMessage = "FFmpeg could not read the game video stream table: " + ffmpegError(ff);
            return false;
        }
        const int streamIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (streamIndex < 0) {
            avformat_close_input(&format);
            errorMessage = "This game video does not contain a decodable video stream.";
            return false;
        }
        AVStream* stream = format->streams[streamIndex];
        const AVCodec* codecDefinition = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codecDefinition) {
            avformat_close_input(&format);
            errorMessage = "No FFmpeg decoder is available for this game video codec.";
            return false;
        }
        AVCodecContext* codec = avcodec_alloc_context3(codecDefinition);
        if (!codec) {
            avformat_close_input(&format);
            errorMessage = "Could not allocate the game video decoder.";
            return false;
        }
        ff = avcodec_parameters_to_context(codec, stream->codecpar);
        if (ff >= 0) ff = avcodec_open2(codec, codecDefinition, nullptr);
        if (ff < 0) {
            avcodec_free_context(&codec);
            avformat_close_input(&format);
            errorMessage = "Could not initialize the game video codec: " + ffmpegError(ff);
            return false;
        }

        GameVideoDecoder* decoder = new (std::nothrow) GameVideoDecoder();
        if (!decoder) {
            avcodec_free_context(&codec);
            avformat_close_input(&format);
            errorMessage = "Could not allocate the game video decoder state.";
            return false;
        }
        decoder->format = format;
        decoder->codec = codec;
        decoder->decoded = av_frame_alloc();
        decoder->packet = av_packet_alloc();
        decoder->videoStream = streamIndex;
        decoder->timeBase = stream->time_base;
        decoder->hasStartPts = stream->start_time != AV_NOPTS_VALUE;
        decoder->startPts = decoder->hasStartPts ? stream->start_time : 0;
        if (!decoder->decoded || !decoder->packet || decoder->timeBase.num == 0 || decoder->timeBase.den == 0) {
            closeGameVideoDecoder(decoder);
            errorMessage = "Could not allocate FFmpeg frame/packet state for the game video.";
            return false;
        }

        decodedVideoWidth = uint32_t(std::max(0, codec->width));
        decodedVideoHeight = uint32_t(std::max(0, codec->height));
        if (decodedVideoWidth == 0 || decodedVideoHeight == 0 || decodedVideoWidth > 8192u || decodedVideoHeight > 8192u) {
            closeGameVideoDecoder(decoder);
            errorMessage = "Game video frame dimensions are invalid or unsupported.";
            return false;
        }
        AVRational rate = av_guess_frame_rate(format, stream, nullptr);
        if (rate.num > 0 && rate.den > 0) decodedVideoFrameRate = av_q2d(rate);
        if (!std::isfinite(decodedVideoFrameRate) || decodedVideoFrameRate < 1.0 || decodedVideoFrameRate > 240.0)
            decodedVideoFrameRate = 30.0;
        if (stream->duration != AV_NOPTS_VALUE)
            decodedVideoDuration = double(stream->duration) * av_q2d(stream->time_base);
        else if (format->duration != AV_NOPTS_VALUE)
            decodedVideoDuration = double(format->duration) / double(AV_TIME_BASE);
        else
            decodedVideoDuration = 0.0;
        decodedVideoStride = int32_t(decodedVideoWidth * 4u);
        decodedVideoPosition = 0.0;
        frame = {};
        gameVideoDecoder = decoder;
        mediaKind = StorylandMediaKind::Video;
        videoReady = true;
        videoPlaying = true;
        nextVideoDecodeTickMs = 0;
        if (!tickVideo(errorMessage)) {
            closeVideoDecoder();
            mediaKind = StorylandMediaKind::None;
            return false;
        }
        videoPlaying = true;
        errorMessage.clear();
        return true;
    }

    static bool mediaFoundationStarted = false;
    if (!mediaFoundationStarted) {
        const HRESULT startup = MFStartup(MF_VERSION, MFSTARTUP_FULL);
        if (FAILED(startup)) {
            errorMessage = "Windows Media Foundation startup failed (" + hresultText(startup) + ").";
            return false;
        }
        mediaFoundationStarted = true;
    }

    IMFAttributes* readerAttributes = nullptr;
    HRESULT hr = MFCreateAttributes(&readerAttributes, 2u);
    if (FAILED(hr) || !readerAttributes) {
        errorMessage = "Windows could not create video decoder attributes (" + hresultText(hr) + ").";
        return false;
    }
    readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

    readerAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    std::wstring decoderPath = filePath;
    IMFSourceReader* reader = nullptr;

    hr = MFCreateSourceReaderFromURL(decoderPath.c_str(), readerAttributes, &reader);
    readerAttributes->Release();
    if (FAILED(hr) || !reader) {
        errorMessage = "Windows Media Foundation could not open this video (" + hresultText(hr) + ").";
        closeVideoDecoder();
        return false;
    }

    IMFMediaType* requested = nullptr;
    hr = MFCreateMediaType(&requested);
    if (SUCCEEDED(hr)) hr = requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) hr = requested->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (SUCCEEDED(hr)) hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, requested);
    if (requested) requested->Release();
    if (FAILED(hr)) {
        reader->Release();
        errorMessage = "Windows could not negotiate a 32-bit video frame format (" + hresultText(hr) + ").";
        closeVideoDecoder();
        return false;
    }

    IMFMediaType* current = nullptr;
    hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &current);
    UINT32 width = 0, height = 0;
    UINT32 frameRateNumerator = 0, frameRateDenominator = 0;
    LONG stride = 0;
    if (SUCCEEDED(hr) && current) hr = MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &width, &height);
    if (SUCCEEDED(hr) && current) {
        UINT32 storedStride = 0;
        if (SUCCEEDED(current->GetUINT32(MF_MT_DEFAULT_STRIDE, &storedStride))) {
            stride = static_cast<LONG>(storedStride);
        } else {
            stride = LONG(width * 4u);
        }
    }
    if (SUCCEEDED(hr) && current && SUCCEEDED(MFGetAttributeRatio(current, MF_MT_FRAME_RATE, &frameRateNumerator, &frameRateDenominator)) &&
        frameRateNumerator != 0u && frameRateDenominator != 0u) {
        decodedVideoFrameRate = double(frameRateNumerator) / double(frameRateDenominator);
        if (!std::isfinite(decodedVideoFrameRate) || decodedVideoFrameRate < 1.0 || decodedVideoFrameRate > 240.0)
            decodedVideoFrameRate = 30.0;
    }
    if (current) current->Release();
    if (FAILED(hr) || width == 0u || height == 0u || width > 8192u || height > 8192u) {
        reader->Release();
        errorMessage = "Video frame dimensions are invalid or unsupported.";
        closeVideoDecoder();
        return false;
    }

    PROPVARIANT duration;
    PropVariantInit(&duration);
    hr = reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration);
    if (SUCCEEDED(hr) && duration.vt == VT_UI8) decodedVideoDuration = double(duration.uhVal.QuadPart) / 10000000.0;
    PropVariantClear(&duration);

    sourceReader = reader;
    decodedVideoWidth = width;
    decodedVideoHeight = height;
    decodedVideoStride = stride == 0 ? int32_t(width * 4u) : int32_t(stride);
    mediaKind = StorylandMediaKind::Video;
    videoReady = true;
    videoPlaying = true;
    decodedVideoPosition = 0.0;
    nextVideoDecodeTickMs = 0;
    frame = {};

    if (!tickVideo(errorMessage)) {
        closeVideoDecoder();
        mediaKind = StorylandMediaKind::None;
        return false;
    }
    videoPlaying = true;
    errorMessage.clear();
    return true;
#else
    (void)filePath;
    errorMessage = "Video playback is available in the Windows build.";
    return false;
#endif
}

bool StorylandMediaFile::tickVideo(std::string& errorMessage) {
#ifdef _WIN32
    if (mediaKind != StorylandMediaKind::Video || !videoReady) return true;
    if (!sourceReader && !gameVideoDecoder) return true;
    if (!videoPlaying && !frame.bgra.empty()) return true;
    if (gameVideoDecoder) {
        const uint64_t nowMs = GetTickCount64();
        if (!frame.bgra.empty() && nextVideoDecodeTickMs != 0u && nowMs < nextVideoDecodeTickMs) return true;
        const bool gotFrame = decodeNextGameVideoFrame(reinterpret_cast<GameVideoDecoder*>(gameVideoDecoder),
                                                       frame, decodedVideoPosition, errorMessage);
        if (!gotFrame) {
            if (errorMessage.empty()) {
                videoPlaying = false;
                return true;
            }
            return false;
        }
        decodedVideoWidth = frame.width;
        decodedVideoHeight = frame.height;
        decodedVideoStride = int32_t(decodedVideoWidth * 4u);
        {
            auto overrideIt = videoFrameOverrides.find(videoFrameIndex());
            if (overrideIt != videoFrameOverrides.end()) {
                const int64_t decodedTimestamp = frame.timestamp100ns;
                frame = overrideIt->second;
                frame.timestamp100ns = decodedTimestamp;
            }
        }
        const double frameMilliseconds = 1000.0 / std::max(1.0, decodedVideoFrameRate);
        nextVideoDecodeTickMs = nowMs + uint64_t(std::max(1.0, frameMilliseconds));
        errorMessage.clear();
        return true;
    }
    const uint64_t nowMs = GetTickCount64();
    if (!frame.bgra.empty() && nextVideoDecodeTickMs != 0u && nowMs < nextVideoDecodeTickMs) return true;
    IMFSourceReader* reader = reinterpret_cast<IMFSourceReader*>(sourceReader);
    DWORD streamFlags = 0;
    LONGLONG timestamp = 0;
    IMFSample* sample = nullptr;
    HRESULT hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &streamFlags, &timestamp, &sample);
    if (FAILED(hr)) {
        errorMessage = "Video decoding failed (" + hresultText(hr) + ").";
        return false;
    }
    if ((streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) != 0u) {
        videoPlaying = false;
        if (sample) sample->Release();
        return true;
    }
    if (!sample) return true;

    IMFMediaBuffer* buffer = nullptr;
    hr = sample->ConvertToContiguousBuffer(&buffer);
    if (FAILED(hr) || !buffer) {
        sample->Release();
        errorMessage = "Video sample could not be converted to a contiguous frame.";
        return false;
    }
    BYTE* data = nullptr;
    DWORD maxLength = 0, currentLength = 0;
    hr = buffer->Lock(&data, &maxLength, &currentLength);
    const uint64_t rowBytes = uint64_t(decodedVideoWidth) * 4ull;
    const uint64_t strideBytes = uint64_t(std::abs(int64_t(decodedVideoStride)));
    const uint64_t required = strideBytes * uint64_t(decodedVideoHeight);
    const uint64_t tightSize = rowBytes * uint64_t(decodedVideoHeight);
    if (FAILED(hr) || !data || strideBytes < rowBytes || currentLength < required ||
        tightSize > uint64_t(std::numeric_limits<size_t>::max())) {
        if (SUCCEEDED(hr)) buffer->Unlock();
        buffer->Release();
        sample->Release();
        errorMessage = "Decoded video frame is truncated or has an invalid stride.";
        return false;
    }
    frame.width = decodedVideoWidth;
    frame.height = decodedVideoHeight;
    frame.timestamp100ns = timestamp;
    frame.bgra.resize(size_t(tightSize));
    for (uint32_t y = 0; y < decodedVideoHeight; ++y) {
        const uint32_t sourceY = decodedVideoStride < 0 ? (decodedVideoHeight - 1u - y) : y;
        const BYTE* sourceRow = data + uint64_t(sourceY) * strideBytes;
        uint8_t* targetRow = frame.bgra.data() + uint64_t(y) * rowBytes;
        std::memcpy(targetRow, sourceRow, size_t(rowBytes));
    }
    buffer->Unlock();
    buffer->Release();
    sample->Release();
    decodedVideoPosition = double(timestamp) / 10000000.0;
    {
        auto overrideIt = videoFrameOverrides.find(videoFrameIndex());
        if (overrideIt != videoFrameOverrides.end()) {
            const int64_t decodedTimestamp = frame.timestamp100ns;
            frame = overrideIt->second;
            frame.timestamp100ns = decodedTimestamp;
        }
    }
    const double frameMilliseconds = 1000.0 / std::max(1.0, decodedVideoFrameRate);
    nextVideoDecodeTickMs = nowMs + uint64_t(std::max(1.0, frameMilliseconds));
    errorMessage.clear();
    return true;
#else
    (void)errorMessage;
    return false;
#endif
}

bool StorylandMediaFile::stepVideoFrame(int direction, std::string& errorMessage) {
#ifdef _WIN32
    if (mediaKind != StorylandMediaKind::Video || !videoReady || (!sourceReader && !gameVideoDecoder)) {
        errorMessage = "No decoded video is open.";
        return false;
    }
    if (direction == 0) {
        errorMessage.clear();
        return true;
    }

    videoPlaying = false;
    nextVideoDecodeTickMs = 0;

    if (gameVideoDecoder) {
        GameVideoDecoder* decoder = reinterpret_cast<GameVideoDecoder*>(gameVideoDecoder);
        if (direction > 0) {
            videoPlaying = true;
            const bool ok = tickVideo(errorMessage);
            videoPlaying = false;
            nextVideoDecodeTickMs = 0;
            return ok;
        }
        const double fps = (std::isfinite(decodedVideoFrameRate) && decodedVideoFrameRate > 0.0)
            ? decodedVideoFrameRate : 30.0;
        const double frameSeconds = 1.0 / fps;
        const double current = std::max(0.0, decodedVideoPosition);
        const double target = std::max(0.0, current - frameSeconds * 1.10);
        if (!seekGameVideo(decoder, target, errorMessage)) return false;
        frame = {};
        decodedVideoPosition = target;
        const double tolerance = frameSeconds * 0.45;
        for (unsigned attempt = 0; attempt < 600u; ++attempt) {
            videoPlaying = true;
            nextVideoDecodeTickMs = 0;
            if (!tickVideo(errorMessage)) {
                videoPlaying = false;
                return false;
            }
            videoPlaying = false;
            if (!frame.bgra.empty() && decodedVideoPosition + tolerance >= target) {
                errorMessage.clear();
                return true;
            }
        }
        errorMessage = "Game video frame seek did not reach the requested frame.";
        return false;
    }

    IMFSourceReader* reader = reinterpret_cast<IMFSourceReader*>(sourceReader);

    if (direction > 0) {
        videoPlaying = true;
        const bool ok = tickVideo(errorMessage);
        videoPlaying = false;
        nextVideoDecodeTickMs = 0;
        return ok;
    }

    const double fps = (std::isfinite(decodedVideoFrameRate) && decodedVideoFrameRate > 0.0)
        ? decodedVideoFrameRate : 30.0;
    const double frameSeconds = 1.0 / fps;
    const double current = std::max(0.0, decodedVideoPosition);
    const double target = std::max(0.0, current - frameSeconds * 1.10);

    PROPVARIANT position;
    PropVariantInit(&position);
    position.vt = VT_I8;
    position.hVal.QuadPart = LONGLONG(std::llround(target * 10000000.0));
    const HRESULT seekResult = reader->SetCurrentPosition(GUID_NULL, position);
    PropVariantClear(&position);
    if (FAILED(seekResult)) {
        errorMessage = "Video frame seek failed (" + hresultText(seekResult) + ").";
        return false;
    }

    frame = {};
    decodedVideoPosition = target;
    const double tolerance = frameSeconds * 0.45;
    for (unsigned attempt = 0; attempt < 240u; ++attempt) {
        videoPlaying = true;
        nextVideoDecodeTickMs = 0;
        if (!tickVideo(errorMessage)) {
            videoPlaying = false;
            return false;
        }
        videoPlaying = false;
        if (!frame.bgra.empty() && decodedVideoPosition + tolerance >= target) {
            errorMessage.clear();
            return true;
        }
    }

    errorMessage = "Video frame seek did not reach the requested frame.";
    return false;
#else
    (void)direction;
    errorMessage = "Video frame stepping is available in the Windows build.";
    return false;
#endif
}


bool StorylandMediaFile::seekVideoFrame(uint64_t frameIndex, std::string& errorMessage) {
#ifdef _WIN32
    if (mediaKind != StorylandMediaKind::Video || !videoReady || (!sourceReader && !gameVideoDecoder)) {
        errorMessage = "No decoded video is open.";
        return false;
    }
    const double fps = (std::isfinite(decodedVideoFrameRate) && decodedVideoFrameRate > 0.0)
        ? decodedVideoFrameRate : 30.0;
    const uint64_t frameCount = videoFrameCountEstimate();
    if (frameCount > 0u && frameIndex >= frameCount) frameIndex = frameCount - 1u;
    const double target = double(frameIndex) / fps;

    videoPlaying = false;
    nextVideoDecodeTickMs = 0;
    frame = {};

    if (gameVideoDecoder) {
        GameVideoDecoder* decoder = reinterpret_cast<GameVideoDecoder*>(gameVideoDecoder);
        const double seekStart = std::max(0.0, target - 1.0 / fps);
        if (!seekGameVideo(decoder, seekStart, errorMessage)) return false;
        decodedVideoPosition = seekStart;
    } else {
        IMFSourceReader* reader = reinterpret_cast<IMFSourceReader*>(sourceReader);
        PROPVARIANT position;
        PropVariantInit(&position);
        position.vt = VT_I8;
        position.hVal.QuadPart = LONGLONG(std::llround(std::max(0.0, target - 1.0 / fps) * 10000000.0));
        const HRESULT seekResult = reader->SetCurrentPosition(GUID_NULL, position);
        PropVariantClear(&position);
        if (FAILED(seekResult)) {
            errorMessage = "Video frame seek failed (" + hresultText(seekResult) + ").";
            return false;
        }
        decodedVideoPosition = std::max(0.0, target - 1.0 / fps);
    }

    const double tolerance = (0.55 / fps);
    for (unsigned attempt = 0; attempt < 1200u; ++attempt) {
        videoPlaying = true;
        nextVideoDecodeTickMs = 0;
        if (!tickVideo(errorMessage)) {
            videoPlaying = false;
            return false;
        }
        videoPlaying = false;
        if (!frame.bgra.empty() && decodedVideoPosition + tolerance >= target) {
            auto overrideIt = videoFrameOverrides.find(frameIndex);
            if (overrideIt != videoFrameOverrides.end()) {
                const int64_t decodedTimestamp = frame.timestamp100ns;
                frame = overrideIt->second;
                frame.timestamp100ns = decodedTimestamp;
            }
            decodedVideoPosition = target;
            errorMessage.clear();
            return true;
        }
    }
    errorMessage = "Video scrubber could not decode the requested frame.";
    return false;
#else
    (void)frameIndex;
    errorMessage = "Video frame seeking is available in the Windows build.";
    return false;
#endif
}


void StorylandMediaFile::closeVideoDecoder() {
#ifdef _WIN32
    releaseUnknown(sourceReader);
    GameVideoDecoder* decoder = reinterpret_cast<GameVideoDecoder*>(gameVideoDecoder);
    closeGameVideoDecoder(decoder);
    gameVideoDecoder = nullptr;
    if (!videoDecodePath.empty()) {
        std::error_code ignored;
        const std::filesystem::path alias(videoDecodePath);
        std::filesystem::remove(alias, ignored);
        if (!alias.parent_path().empty()) std::filesystem::remove(alias.parent_path(), ignored);
    }
#endif
    videoDecodePath.clear();
    videoReady = false;
    videoPlaying = false;
    nextVideoDecodeTickMs = 0;
}

std::string StorylandMediaFile::summary() const {
    std::ostringstream out;
    if (mediaKind == StorylandMediaKind::Audio || mediaKind == StorylandMediaKind::AudioArchive) {
        out << (mediaKind == StorylandMediaKind::AudioArchive ? "Sound archive" : "Audio")
            << " | clips=" << audioClips.size();
        if (selectedAudioClip >= 0 && size_t(selectedAudioClip) < audioClips.size()) {
            const StorylandMediaClip& clip = audioClips[size_t(selectedAudioClip)];
            out << " | selected=" << selectedAudioClip
                << " | " << clip.sampleRate << " Hz"
                << " | " << clip.channels << " ch"
                << " | " << std::fixed << std::setprecision(2) << clip.durationSeconds << " s";
        }
    } else if (mediaKind == StorylandMediaKind::Video) {
        out << "Video | " << decodedVideoWidth << "x" << decodedVideoHeight
            << " | " << std::fixed << std::setprecision(3) << decodedVideoFrameRate << " fps";
        if (decodedVideoDuration > 0.0) out << " | " << std::fixed << std::setprecision(2) << decodedVideoDuration << " s";
    } else {
        out << "No media loaded";
    }
    return out.str();
}
