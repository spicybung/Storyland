#pragma once
#include "src/storyland/storyland_sdt.h"
#include <array>
#include <functional>
#include <cctype>

inline int storylandPspVagBankNumber(const std::filesystem::path& path) {
    std::string name = path.filename().string();
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char character) { return char(std::tolower(character)); });
    const std::string suffix = "_psp.raw";
    if (name.size() <= 3 + suffix.size() || name.compare(0, 3, "sfx") != 0 ||
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) return -1;
    unsigned number = 0;
    for (size_t index = 3; index < name.size() - suffix.size(); ++index) {
        if (name[index] < '0' || name[index] > '9' || number > 67) return -1;
        number = number * 10 + unsigned(name[index] - '0');
    }
    return number <= 67 ? int(number) : -1;
}

struct StorylandVagBankSample {
    uint64_t offset = 0;
    uint32_t length = 0;
    uint32_t frequency = 0;
    bool headerPresent = false;
    std::string name;
};

inline bool storylandReadVagBankHeader(std::ifstream& input, uint64_t bankBytes,
    uint64_t offset, StorylandVagBankSample& sample, std::string& error) {
    sample = {};
    if (offset > bankBytes || bankBytes - offset < 48) {
        error = "The PSP VAG bank ends inside a sample header.";
        return false;
    }
    std::array<uint8_t, 48> header{};
    input.clear();
    input.seekg(std::streamoff(offset));
    if (!input.read(reinterpret_cast<char*>(header.data()), header.size())) {
        error = "The PSP VAG sample header could not be read.";
        return false;
    }
    const bool headerPresent = std::memcmp(header.data(), "VAGp", 4) == 0;
    const bool strippedHeader = std::all_of(header.begin(), header.begin() + 12,
        [](uint8_t value) { return value == 0; }) &&
        std::all_of(header.begin() + 20, header.end(), [](uint8_t value) { return value == 0; });
    const uint32_t payloadBytes = storyland_sdt_detail::readBe32(header.data() + 12);
    const uint32_t frequency = storyland_sdt_detail::readBe32(header.data() + 16);
    if ((!headerPresent && !strippedHeader) || !payloadBytes || payloadBytes % 16 ||
        payloadBytes > bankBytes - offset - 48 || payloadBytes > UINT32_MAX - 48 ||
        frequency < 2000 || frequency > 192000) {
        error = "The PSP VAG bank has an invalid sample header at byte " + std::to_string(offset) + ".";
        return false;
    }
    sample.offset = offset;
    sample.length = payloadBytes + 48;
    sample.frequency = frequency;
    sample.headerPresent = headerPresent;
    sample.name = storyland_sdt_detail::printableName(header.data() + 32, 16);
    error.clear();
    return true;
}

inline bool storylandVisitVagBank(const std::filesystem::path& path,
    const std::function<bool(size_t, const StorylandVagBankSample&)>& visit, std::string& error) {
    std::error_code filesystemError;
    const uint64_t bankBytes = std::filesystem::file_size(path, filesystemError);
    if (filesystemError || bankBytes < 64 || bankBytes > 512ull * 1024 * 1024) {
        error = "The PSP VAG bank is missing or has an invalid size.";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "The PSP VAG bank could not be opened.";
        return false;
    }
    uint64_t offset = 0;
    size_t slot = 0;
    while (offset < bankBytes) {
        if (slot >= 100000) {
            error = "The PSP VAG bank has too many samples.";
            return false;
        }
        StorylandVagBankSample sample;
        if (!storylandReadVagBankHeader(input, bankBytes, offset, sample, error)) return false;
        if (!visit(slot, sample)) { error.clear(); return true; }
        offset += sample.length;
        ++slot;
    }
    error.clear();
    return true;
}

inline bool storylandReadVagBankIndex(const std::filesystem::path& path,
    std::vector<StorylandVagBankSample>& samples, std::string& error) {
    std::vector<StorylandVagBankSample> decoded;
    if (!storylandVisitVagBank(path, [&](size_t, const StorylandVagBankSample& sample) {
            decoded.push_back(sample);
            return true;
        }, error)) {
        samples.clear();
        return false;
    }
    samples = std::move(decoded);
    return true;
}

template<class Clip>
inline bool storylandLoadVagBankPcm(const std::filesystem::path& path, size_t slot,
    Clip& output, std::string& error, uint64_t maximumBytes = 4u * 1024u * 1024u) {
    StorylandVagBankSample selected;
    bool found = false;
    if (!storylandVisitVagBank(path, [&](size_t index, const StorylandVagBankSample& sample) {
            if (index != slot) return true;
            selected = sample;
            found = true;
            return false;
        }, error)) return false;
    if (!found) {
        error = "The requested PSP VAG sample is outside the bank.";
        return false;
    }
    if (selected.length > maximumBytes) {
        error = "The PSP sound sample is too large for a gameplay effect.";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    input.seekg(std::streamoff(selected.offset));
    std::vector<uint8_t> bytes(selected.length);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(bytes.size()))) {
        error = "The PSP VAG sample could not be read.";
        return false;
    }
    Clip clip;
    clip.sourceIndex = uint32_t(slot);
    clip.sourceOffset = selected.offset;
    clip.sourceSize = selected.length;
    clip.name = selected.name.empty() ? "Sound " + std::to_string(slot) : selected.name;
    const size_t payloadOffset = selected.headerPresent ? 0 : 48;
    if (!storylandDecodeVagPcm(bytes, payloadOffset, bytes.size() - payloadOffset,
        selected.frequency, clip, error)) return false;
    output = std::move(clip);
    error.clear();
    return true;
}
