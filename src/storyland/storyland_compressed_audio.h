#pragma once
#include "src/storyland/storyland_media.h"
#include "src/storyland/storyland_sdt.h"
#include <cmath>
#include <filesystem>
#include <functional>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/samplefmt.h>
}

inline std::string storylandAudioCodecError(int code) {
    char text[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, text, sizeof(text));
    return text[0] ? std::string(text) : "Audio decoder error " + std::to_string(code);
}

inline bool storylandAppendAudioFrameS16(const AVFrame* frame, std::vector<int16_t>& output, std::string& error) {
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
    if (additions > storyland_sdt_detail::maxDecodedPcmSamples || output.size() > storyland_sdt_detail::maxDecodedPcmSamples - size_t(additions)) {
        error = "Decoded audio would exceed Storyland's PCM safety limit.";
        return false;
    }
    // Keep vector's geometric growth. Reserving exactly one frame at a time
    // recopies the entire clip for every decoded packet of a long voice file.

    auto sampleToS16 = [&](const uint8_t* data, int index) -> int16_t {
        switch (packed) {
        case AV_SAMPLE_FMT_U8: {
            const int value = int(reinterpret_cast<const uint8_t*>(data)[index]) - 128;
            return int16_t(value * 256);
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
            return storyland_sdt_detail::clampPcm(int(std::lrint(double(value) * 32767.0)));
        }
        case AV_SAMPLE_FMT_DBL: {
            double value = reinterpret_cast<const double*>(data)[index];
            if (!std::isfinite(value)) value = 0.0;
            value = std::max(-1.0, std::min(1.0, value));
            return storyland_sdt_detail::clampPcm(int(std::lrint(value * 32767.0)));
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

inline bool storylandDecodeCompressedAudio(const std::filesystem::path& filePath,
    StorylandMediaClip& output, std::string& errorMessage,
    const std::function<bool()>& cancelled = {}) {
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
        errorMessage = "FFmpeg could not open this audio file: " + storylandAudioCodecError(result);
        cleanup();
        return false;
    }
    result = avformat_find_stream_info(format, nullptr);
    if (result < 0) {
        errorMessage = "FFmpeg could not read the audio stream information: " + storylandAudioCodecError(result);
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
        errorMessage = "Could not initialize the FFmpeg audio decoder: " + storylandAudioCodecError(result);
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
        if (cancelled && cancelled()) {
            errorMessage = "Audio decoding was cancelled.";
            cleanup();
            return false;
        }
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
            if (!storylandAppendAudioFrameS16(decoded, clip.pcm, errorMessage)) {
                cleanup();
                return false;
            }
            av_frame_unref(decoded);
            continue;
        }
        if (result == AVERROR_EOF) break;
        if (result != AVERROR(EAGAIN)) {
            errorMessage = "Audio decode failed: " + storylandAudioCodecError(result);
            cleanup();
            return false;
        }

        if (inputEof) {
            if (!sentFlush) {
                result = avcodec_send_packet(codec, nullptr);
                sentFlush = true;
                if (result < 0 && result != AVERROR_EOF) {
                    errorMessage = "Could not flush the audio decoder: " + storylandAudioCodecError(result);
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
            errorMessage = "Could not read the next audio packet: " + storylandAudioCodecError(result);
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
            errorMessage = "Could not submit an audio packet to the decoder: " + storylandAudioCodecError(result);
            cleanup();
            return false;
        }
    }

    cleanup();
    if (clip.pcm.empty() || sampleRate == 0 || channels == 0) {
        errorMessage = "The audio stream decoded to no PCM samples.";
        return false;
    }

    if (channels > 2) {
        const size_t frames = clip.pcm.size() / channels;
        std::vector<int16_t> stereo;
        stereo.reserve(frames * 2u);
        for (size_t frameIndex = 0; frameIndex < frames; ++frameIndex) {
            double left = 0, right = 0;
            unsigned leftCount = 0, rightCount = 0;
            for (unsigned channel = 0; channel < channels; ++channel) {
                const double sample = clip.pcm[frameIndex * channels + channel];
                if ((channel & 1u) == 0u) { left += sample; ++leftCount; }
                else { right += sample; ++rightCount; }
            }
            left /= double(leftCount);
            right /= double(rightCount ? rightCount : leftCount);
            stereo.push_back(int16_t(left));
            stereo.push_back(int16_t(right));
        }
        clip.pcm = std::move(stereo);
        channels = 2;
    }

    clip.sampleRate = sampleRate;
    clip.channels = channels;
    clip.durationSeconds = double(clip.pcm.size()) / (double(sampleRate) * double(channels));
    output = std::move(clip);
    errorMessage.clear();
    return true;
}
