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
#include <sstream>

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
}

bool StorylandMediaFile::loadFromFile(const std::wstring& filePath, std::string& errorMessage) {
    close();
    const std::wstring ext = lowerExtension(filePath);
    bool ok = false;
    if (ext == L".sdt") ok = loadSdt(filePath, errorMessage);
    else if (ext == L".raw" || ext == L".vag") ok = loadRaw(filePath, errorMessage);
    else if (ext == L".wav") ok = loadWav(filePath, errorMessage);
    else if (ext == L".pss" || ext == L".mpg" || ext == L".mpeg" || ext == L".mp4" ||
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
        if (!sourceReader) {
            errorMessage = "Video decoder is not ready.";
            return false;
        }
        if (decodedVideoDuration > 0.0 && decodedVideoPosition >= decodedVideoDuration - 0.05) {
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
static bool makePssDecodeAlias(const std::wstring& sourcePath, std::wstring& aliasPath, std::string& error) {
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
            error = "Could not create a private temporary directory for PSS playback.";
            return false;
        }
    }
    if (!created) {
        error = "Could not allocate a private temporary directory for PSS playback.";
        return false;
    }

    const std::filesystem::path alias = privateDirectory / L"stream.mpg";
    if (!CopyFileW(sourcePath.c_str(), alias.c_str(), TRUE)) {
        std::filesystem::remove(privateDirectory, ec);
        error = "Could not create the temporary MPEG alias for PSS playback.";
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

    static bool mediaFoundationStarted = false;
    if (!mediaFoundationStarted) {
        const HRESULT startup = MFStartup(MF_VERSION, MFSTARTUP_LITE);
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
    if (FAILED(hr) && lowerExtension(filePath) == L".pss") {
        if (!makePssDecodeAlias(filePath, videoDecodePath, errorMessage)) {
            readerAttributes->Release();
            return false;
        }
        decoderPath = videoDecodePath;
        hr = MFCreateSourceReaderFromURL(decoderPath.c_str(), readerAttributes, &reader);
    }
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
    if (mediaKind != StorylandMediaKind::Video || !videoReady || !sourceReader) return true;
    if (!videoPlaying && !frame.bgra.empty()) return true;
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
    const double frameMilliseconds = 1000.0 / std::max(1.0, decodedVideoFrameRate);
    nextVideoDecodeTickMs = nowMs + uint64_t(std::max(1.0, frameMilliseconds));
    errorMessage.clear();
    return true;
#else
    (void)errorMessage;
    return false;
#endif
}

void StorylandMediaFile::closeVideoDecoder() {
#ifdef _WIN32
    releaseUnknown(sourceReader);
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
        out << "Video | " << decodedVideoWidth << "x" << decodedVideoHeight;
        if (decodedVideoDuration > 0.0) out << " | " << std::fixed << std::setprecision(2) << decodedVideoDuration << " s";
    } else {
        out << "No media loaded";
    }
    return out.str();
}
