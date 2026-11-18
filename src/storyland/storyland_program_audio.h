#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include "src/storyland/storyland_audio_output.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

class StorylandProgramAudio {
public:
    StorylandProgramAudio() = default;
    ~StorylandProgramAudio() { stop(); }

    StorylandProgramAudio(const StorylandProgramAudio&) = delete;
    StorylandProgramAudio& operator=(const StorylandProgramAudio&) = delete;

    bool playResource(int resourceId, bool loop, std::wstring& error) {
        HMODULE module = GetModuleHandleW(nullptr);
        if (!module) {
            error = L"Unable to locate Storyland's executable";
            return false;
        }
        HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), L"WAVE");
        if (!resource) {
            error = L"Embedded audio resource is missing (ID " + std::to_wstring(resourceId) + L")";
            return false;
        }
        HGLOBAL loaded = LoadResource(module, resource);
        const uint8_t* bytes = loaded ? static_cast<const uint8_t*>(LockResource(loaded)) : nullptr;
        DWORD count = SizeofResource(module, resource);
        if (!bytes || count == 0) {
            error = L"Embedded audio resource is empty";
            return false;
        }
        return playWav(bytes, count, loop, error);
    }

    bool playFile(const std::filesystem::path& path, bool loop, std::wstring& error) {
        std::error_code filesystemError;
        const uintmax_t count = std::filesystem::file_size(path, filesystemError);
        if (filesystemError || count < 44 || count > 128u * 1024u * 1024u) {
            error = L"Audio file is missing, empty or too large";
            return false;
        }
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            error = L"Unable to open audio file";
            return false;
        }
        std::vector<uint8_t> bytes(static_cast<size_t>(count));
        input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()));
        if (input.gcount() != std::streamsize(bytes.size())) {
            error = L"Unable to read complete audio file";
            return false;
        }
        return playWav(bytes.data(), bytes.size(), loop, error);
    }

    bool playPcm16(const int16_t* pcm, size_t sampleCount, uint32_t sampleRate,
                   uint16_t channels, bool loop, std::wstring& error) {
        if (!pcm || sampleCount == 0 || (channels != 1 && channels != 2) ||
            sampleRate < 8000 || sampleRate > 192000 || sampleCount % channels != 0 ||
            sampleCount > std::numeric_limits<UINT32>::max() / sizeof(int16_t)) {
            error = L"Decoded audio samples have invalid PCM parameters";
            return false;
        }
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = channels;
        format.nSamplesPerSec = sampleRate;
        format.wBitsPerSample = 16;
        format.nBlockAlign = static_cast<uint16_t>(channels * sizeof(int16_t));
        format.nAvgBytesPerSec = sampleRate * format.nBlockAlign;
        return startPcm(reinterpret_cast<const uint8_t*>(pcm),
            sampleCount * sizeof(int16_t), format, loop, error);
    }

    const wchar_t* outputBackend() const {
        return backendDescription.c_str();
    }

    bool isPlaying() const {
        return output.queued() != 0;
    }

    uint32_t playbackMilliseconds() const {
        return output.milliseconds();
    }

    void stop() {
        output.close();
        samples.clear();
        backendDescription = L"none";
    }

private:
    static uint16_t read16(const uint8_t* bytes) {
        return uint16_t(bytes[0]) | uint16_t(uint16_t(bytes[1]) << 8);
    }

    static uint32_t read32(const uint8_t* bytes) {
        return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
            (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
    }

    bool playWav(const uint8_t* bytes, size_t length, bool loop, std::wstring& error) {
        stop();
        if (!bytes || length < 44 || std::memcmp(bytes, "RIFF", 4) != 0 ||
            std::memcmp(bytes + 8, "WAVE", 4) != 0) {
            error = L"Audio is not a supported WAV file";
            return false;
        }
        const uint64_t riffEnd = uint64_t(read32(bytes + 4)) + 8;
        if (riffEnd < 44 || riffEnd > length) {
            error = L"Audio WAV header is truncated";
            return false;
        }
        WAVEFORMATEX format{};
        const uint8_t* pcm = nullptr;
        size_t pcmSize = 0;
        bool foundFormat = false;
        for (size_t cursor = 12; cursor + 8 <= riffEnd;) {
            const uint32_t chunkLength = read32(bytes + cursor + 4);
            const uint64_t end = uint64_t(cursor) + 8 + chunkLength;
            if (end > riffEnd) {
                error = L"Audio WAV chunk exceeds its file";
                return false;
            }
            const uint8_t* chunk = bytes + cursor + 8;
            if (std::memcmp(bytes + cursor, "fmt ", 4) == 0 && chunkLength >= 16) {
                format.wFormatTag = read16(chunk);
                format.nChannels = read16(chunk + 2);
                format.nSamplesPerSec = read32(chunk + 4);
                format.nAvgBytesPerSec = read32(chunk + 8);
                format.nBlockAlign = read16(chunk + 12);
                format.wBitsPerSample = read16(chunk + 14);
                foundFormat = true;
            } else if (std::memcmp(bytes + cursor, "data", 4) == 0 && !pcm) {
                pcm = chunk;
                pcmSize = chunkLength;
            }
            cursor = static_cast<size_t>(end + (chunkLength & 1u));
        }
        if (!foundFormat || !pcm || !pcmSize ||
            format.wFormatTag != WAVE_FORMAT_PCM ||
            (format.wBitsPerSample != 8 && format.wBitsPerSample != 16) ||
            (format.nChannels != 1 && format.nChannels != 2) ||
            format.nSamplesPerSec < 8000 || format.nSamplesPerSec > 192000 ||
            format.nBlockAlign != (format.nChannels * format.wBitsPerSample / 8) ||
            format.nAvgBytesPerSec != format.nSamplesPerSec * format.nBlockAlign ||
            pcmSize % format.nBlockAlign != 0 ||
            pcmSize > std::numeric_limits<UINT32>::max()) {
            error = L"Audio WAV has unsupported PCM parameters";
            return false;
        }

        return startPcm(pcm, pcmSize, format, loop, error);
    }

    bool startPcm(const uint8_t* pcm, size_t pcmSize, const WAVEFORMATEX& format,
                  bool loop, std::wstring& error) {
        stop();
        const size_t sampleCount = pcmSize / (format.wBitsPerSample / 8u);
        if (sampleCount > std::numeric_limits<uint32_t>::max() / sizeof(int16_t)) {
            error = L"The PCM recording exceeds the audio output buffer limit";
            return false;
        }
        samples.resize(sampleCount);
        if (format.wBitsPerSample == 16) {
            std::memcpy(samples.data(), pcm, pcmSize);
        } else {
            for (size_t index = 0; index < sampleCount; ++index)
                samples[index] = int16_t((int(pcm[index]) - 128) * 256);
        }
        std::string outputError;
        if (!output.open(format.nSamplesPerSec, format.nChannels, outputError)) {
            error.assign(outputError.begin(), outputError.end());
            stop();
            return false;
        }
        if (!output.submit(samples.data(), uint32_t(samples.size() * sizeof(int16_t)), loop) ||
            !output.pause(false)) {
            error = L"The audio output could not start the PCM recording";
            stop();
            return false;
        }
        const char* backend = output.backendName();
        backendDescription.assign(backend, backend + std::strlen(backend));
        error.clear();
        return true;
    }

    StorylandAudioOutput output;
    std::vector<int16_t> samples;
    std::wstring backendDescription = L"none";
};
