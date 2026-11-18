#include "src/leeds/leeds_ctw_pak.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <cstring>
#include <cmath>
#include <system_error>
#include <limits>
#include <zlib.h>

namespace storyland_ctw {

bool isArchiveSource(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::error_code error;
    return !source.empty() && std::filesystem::equivalent(source, destination, error);
}

uint16_t read16(const unsigned char* data) {
    return uint16_t(uint16_t(data[0]) | (uint16_t(data[1]) << 8));
}

uint32_t read32(const unsigned char* data) {
    return uint32_t(data[0]) | (uint32_t(data[1]) << 8) |
           (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
}

bool readPakIndex(const std::filesystem::path& path, PakHeader& output, std::string& error) {
    output = {};
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) { error = "Cannot open PAK file."; return false; }
    const auto length = file.tellg();
    if (length < 44) { error = "PAK header is truncated."; return false; }
    const uint64_t fileSize = uint64_t(length);
    std::array<uint8_t, 44> bytes{};
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        error = "Cannot read PAK header."; return false;
    }
    const uint32_t magic = read32(bytes.data());
    if (magic != 0x504B3032u && magic != 0x504B3031u) {
        error = "Not a Chinatown Wars PK01/PK02 archive."; return false;
    }
    PakHeader candidate;
    const uint32_t pspCount = read32(bytes.data() + 36);
    const uint32_t pspData = read32(bytes.data() + 40);
    const bool psp = magic == 0x504B3032u && pspCount > 0 && pspCount <= 65536 &&
        pspData >= 44u + uint64_t(pspCount) * 2u && pspData % 2048u == 0 && pspData < fileSize;
    const uint32_t count = psp ? pspCount : read32(bytes.data() + 16);
    const uint64_t mobileEnd = uint64_t(read32(bytes.data() + 20)) * 4096u;
    if (!psp && (count == 0 || count > 65536 || mobileEnd < 4096 || mobileEnd > fileSize)) {
        error = "PAK resource directory is outside the file."; return false;
    }
    candidate.mobile = !psp;
    candidate.sectorSize = psp ? 2048 : 4096;
    candidate.entryCount = count;
    candidate.dataOffset = psp ? pspData : 0;
    candidate.bankStarts.fill(UINT32_MAX);
    const size_t bankCount = psp ? 8 : 3;
    for (size_t bank = 0; bank < bankCount; ++bank) {
        candidate.bankStarts[bank] = read32(bytes.data() + 4 + bank * 4);
        if (candidate.bankStarts[bank] != UINT32_MAX && candidate.bankStarts[bank] > count) {
            error = "PAK bank boundary exceeds the resource count."; return false;
        }
        if (bank && candidate.bankStarts[bank] < candidate.bankStarts[bank - 1]) {
            error = "PAK bank boundaries are not ordered."; return false;
        }
    }
    const size_t tableOffset = psp ? 44 : 24;
    std::vector<uint8_t> table(size_t(count) * 2);
    const size_t firstBytes = psp ? table.size() : std::min(table.size(), size_t(4096 - tableOffset));
    file.seekg(tableOffset);
    if (!file.read(reinterpret_cast<char*>(table.data()), firstBytes)) {
        error = "PAK sector table is truncated."; return false;
    }
    if (firstBytes < table.size()) {
        if (table.size() - firstBytes > fileSize - mobileEnd) {
            error = "Mobile PAK directory continuation is truncated."; return false;
        }
        file.seekg(std::streamoff(mobileEnd));
        if (!file.read(reinterpret_cast<char*>(table.data() + firstBytes), table.size() - firstBytes)) {
            error = "Cannot read Mobile PAK directory continuation."; return false;
        }
    }
    for (size_t index = 0; index < count; ++index) {
        const uint16_t sector = read16(table.data() + index * 2);
        uint64_t bank = 0;
        for (size_t boundary = 0; boundary < bankCount; ++boundary)
            if (index >= candidate.bankStarts[boundary]) ++bank;
        const uint64_t offset = candidate.dataOffset +
            (bank * 65536u + sector) * candidate.sectorSize;
        if ((!psp && offset < 4096) || offset >= (psp ? fileSize : mobileEnd) ||
            (!candidate.offsets.empty() && offset < candidate.offsets.back())) {
            error = "PAK resource offsets are invalid or out of order."; return false;
        }
        candidate.resourceIds.push_back(sector);
        candidate.offsets.push_back(offset);
    }
    candidate.offsets.push_back(psp ? fileSize : mobileEnd);
    output = std::move(candidate);
    error.clear();
    return true;
}

bool readPakIndex(const std::string& filename, PakHeader& header, std::string& error) {
    return readPakIndex(std::filesystem::path(filename), header, error);
}

void PakArchive::clear() {
    path_.clear();
    header_ = {};
    resources_.clear();
    fileSize_ = 0;
    overlays_ = false;
}

bool PakArchive::loadFromFile(const std::filesystem::path& path, std::string& error) {
    clear();
    PakHeader candidate;
    if (!readPakIndex(path, candidate, error)) return false;
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) { error = "Cannot open PAK resources."; return false; }
    const uint64_t totalSize = uint64_t(input.tellg());
    std::vector<PakResource> found;
    found.reserve(candidate.entryCount);
    for (size_t index = 0; index < candidate.entryCount; ++index) {
        const uint64_t length = candidate.offsets[index + 1] - candidate.offsets[index];
        if (length > 256u * 1024u * 1024u) {
            error = "PAK resource exceeds the supported allocation size."; return false;
        }
        PakResource record;
        record.offset = candidate.offsets[index];
        record.storedSize = uint32_t(length);
        record.expandedSize = record.storedSize;
        record.resourceId = uint16_t(index);
        record.hasResourceId = true;
        std::vector<uint8_t> prefix(size_t(std::min<uint64_t>(length, 128)));
        input.seekg(std::streamoff(record.offset));
        if (!prefix.empty() && !input.read(reinterpret_cast<char*>(prefix.data()), prefix.size())) {
            error = "Cannot read indexed PAK resource."; return false;
        }
        if (prefix.size() >= 12 && read32(prefix.data()) == 0x41533031u) {
            record.compressed = true;
            record.compressedSize = read32(prefix.data() + 4);
            record.expandedSize = read32(prefix.data() + 8);
            if (record.compressedSize < 2 || record.expandedSize == 0 ||
                record.expandedSize > 256u * 1024u * 1024u ||
                uint64_t(record.compressedSize) + 12 > length) {
                error = "PAK AS01 block exceeds its indexed resource."; return false;
            }
            // Only inflate the header here; selection performs full checksum validation.
            std::vector<uint8_t> packed(std::min(record.compressedSize, uint32_t(4096)));
            input.seekg(std::streamoff(record.offset + 12));
            if (!input.read(reinterpret_cast<char*>(packed.data()), packed.size())) {
                error = "Cannot read PAK compressed header."; return false;
            }
            prefix.assign(128, 0);
            z_stream stream{};
            stream.next_in = packed.data(); stream.avail_in = uInt(packed.size());
            stream.next_out = prefix.data(); stream.avail_out = uInt(prefix.size());
            if (inflateInit(&stream) != Z_OK) { error = "Cannot initialize zlib."; return false; }
            const int status = inflate(&stream, Z_NO_FLUSH);
            const size_t produced = stream.total_out;
            inflateEnd(&stream);
            if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR) {
                error = "Invalid PAK compressed resource header."; return false;
            }
            prefix.resize(produced);
        }
        record.type = guessResourceExtension(prefix);
        if (!candidate.mobile && prefix.size() >= 16 &&
            (read32(prefix.data()) == 4 || read32(prefix.data()) == 5) &&
            prefix[4] <= 11 && prefix[5] <= 11 &&
            uint64_t(read32(prefix.data() + 12)) + 16 <= record.expandedSize)
            record.type = ".psptex";
        if (!candidate.mobile && prefix.size() >= 20 && read32(prefix.data()) == 255 &&
            read16(prefix.data() + 16) > 0 && 18ull + read16(prefix.data() + 16) * 4ull <= record.expandedSize)
            record.type = ".pspanimtex";
        if (candidate.mobile && prefix.size() >= 12) {
            const uint32_t width = read16(prefix.data()), height = read16(prefix.data() + 2);
            const uint16_t marker = read16(prefix.data() + 4), format = read16(prefix.data() + 6);
            if (width && height && width <= 4096 && height <= 4096 &&
                (marker & 0xFF00u) == 0x8C00u &&
                (format == 2 || format == 4 || format == 0x102 || format == 0x104) &&
                uint64_t(read32(prefix.data() + 8)) + 12 <= record.expandedSize)
                record.type = ".tex";
        }
        if (prefix.size() >= 44 && prefix[12] == '.' && prefix[13] == '/') {
            size_t end = 14;
            while (end < prefix.size() && prefix[end] >= 32 && prefix[end] < 127) ++end;
            if (end < prefix.size() && prefix[end] == 0) {
                record.name.assign(prefix.begin() + 14, prefix.begin() + end);
                record.type = ".vehicle";
            }
        }
        if (!candidate.mobile && prefix.size() >= 76 && read16(prefix.data()) == 1) {
            const uint32_t materials = read16(prefix.data() + 2);
            const uint32_t parts = read16(prefix.data() + 4);
            const uint32_t vertices = read16(prefix.data() + 6);
            const uint64_t auxiliarySize = uint64_t(read16(prefix.data() + 8)) * 8 +
                uint64_t(read16(prefix.data() + 12)) * 24 + uint64_t(read16(prefix.data() + 14)) * 6 +
                uint64_t(read16(prefix.data() + 16)) * 6 + uint64_t(read16(prefix.data() + 18)) * 8;
            if (parts && parts <= 128 && materials == parts && vertices &&
                uint64_t(76) + materials * 8 + parts * 16 + auxiliarySize + vertices * 12 <= record.expandedSize)
                record.type = ".pspmodel";
        }
        if (!candidate.mobile && record.type == ".bin" && prefix.size() >= 60) {
            const uint32_t a = read32(prefix.data()), b = read32(prefix.data() + 4);
            const uint32_t c = read32(prefix.data() + 8), d = read32(prefix.data() + 12);
            if (a >= 12 && a < b && b < c && c < d && uint64_t(d) + 60 <= record.expandedSize)
                record.type = ".world";
        }
        record.isPspExecutable = record.type == ".elf";
        found.push_back(std::move(record));
    }
    path_ = path;
    header_ = std::move(candidate);
    resources_ = std::move(found);
    fileSize_ = totalSize;
    error.clear();
    return true;
}

bool PakArchive::extractResource(size_t index, std::vector<uint8_t>& bytes, std::string& error) const {
    bytes.clear();
    if (index >= resources_.size() || path_.empty()) { error = "Invalid CTW PAK resource index."; return false; }
    const auto& entry = resources_[index];
    if (!entry.compressed) {
        std::ifstream file(path_, std::ios::binary);
        if (!file || !file.seekg(std::streamoff(entry.offset))) {
            error = "Cannot seek to PAK resource."; return false;
        }
        bytes.resize(entry.expandedSize);
        if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
            bytes.clear(); error = "PAK resource is truncated."; return false;
        }
        error.clear(); return true;
    }
    if (entry.expandedSize > uLongf((std::numeric_limits<uLongf>::max)()) ||
        entry.compressedSize > uLong((std::numeric_limits<uLong>::max)())) {
        error = "PAK compressed resource is too large for zlib.";
        return false;
    }
    std::ifstream file(path_, std::ios::binary);
    if (!file || !file.seekg(std::streamoff(entry.offset + 12u))) {
        error = "Could not seek to PAK resource.";
        return false;
    }
    std::vector<uint8_t> packed(entry.compressedSize);
    if (!file.read(reinterpret_cast<char*>(packed.data()), std::streamsize(packed.size()))) {
        error = "PAK compressed resource is truncated.";
        return false;
    }
    bytes.resize(entry.expandedSize);
    uLongf decoded = uLongf(bytes.size());
    const int result = uncompress(bytes.data(), &decoded, packed.data(), uLong(packed.size()));
    if (result != Z_OK || decoded != entry.expandedSize) {
        bytes.clear();
        error = "CTW AS01 zlib extraction failed or expanded size does not match.";
        return false;
    }
    error.clear();
    return true;
}

bool PakArchive::exportResource(size_t index, const std::filesystem::path& path, std::string& error) const {
    if (isArchiveSource(path_, path)) {
        error = "Cannot export a resource over its source archive.";
        return false;
    }
    std::vector<uint8_t> bytes;
    if (!extractResource(index, bytes, error)) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output || !output.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()))) {
        error = "Could not write extracted PAK resource.";
        return false;
    }
    output.close();
    if (!output) {
        error = "Could not finish writing extracted PAK resource.";
        return false;
    }
    error.clear();
    return true;
}

namespace {

bool validPspNeighborTable(const uint8_t* bytes, size_t length) {
    if (length < 40 || read32(bytes) != 40 || read32(bytes + 36) != length)
        return false;
    uint32_t previous = 40;
    for (unsigned index = 1; index < 10; ++index) {
        const uint32_t offset = read32(bytes + index * 4);
        if (offset < previous || offset > length) return false;
        const size_t span = size_t(offset - previous);
        if (span != 0) {
            const uint8_t* neighbor = bytes + previous;
            if (span < 8 || read32(neighbor) >= 118 || read32(neighbor + 4) >= 84)
                return false;
            if (span != 8) {
                if (span < 12) return false;
                const uint32_t groups = read32(neighbor + 8);
                const uint64_t records = 12u + uint64_t(groups) * 16u;
                if (records > span) return false;
                uint64_t count = 0;
                for (uint32_t group = 0; group < groups; ++group)
                    count += read16(neighbor + 12 + size_t(group) * 16 + 12);
                if (records + count * 12u != span) return false;
            }
        }
        previous = offset;
    }
    return true;
}

}

bool decodePspWorldSection(const std::vector<uint8_t>& bytes, unsigned section,
                           PakWorldSection& decoded, std::string& error) {
    decoded = {};
    if (section >= 4 || bytes.size() < 60) {
        error = "Invalid CTW world section.";
        return false;
    }
    uint64_t previous = 60;
    for (unsigned index = 0; index < 4; ++index) {
        const uint64_t end = 60u + uint64_t(read32(bytes.data() + index * 4));
        if (end < previous + 12 || end > bytes.size()) {
            error = "CTW world quadrant offsets are truncated or out of order.";
            return false;
        }
        previous = end;
    }
    const size_t start = 60u + (section ? read32(bytes.data() + (section - 1) * 4) : 0u);
    const size_t end = 60u + read32(bytes.data() + section * 4);
    const size_t length = end - start;
    const uint8_t* header = bytes.data() + start;
    const uint32_t count = read16(header);
    const uint16_t shadows = read16(header + 2);
    const uint32_t groups = read32(header + 4);
    const uint32_t lights = read32(header + 8);
    const uint64_t first = 12u + uint64_t(groups) * 16u;
    const uint64_t afterRecords = first + uint64_t(count) * 12u;
    const uint64_t afterEffects = afterRecords + uint64_t(shadows) * 20u + uint64_t(lights) * 40u;
    bool validOwnRecords = afterEffects <= length;
    if (validOwnRecords) {
        uint64_t groupedCount = 0;
        for (uint32_t group = 0; group < groups; ++group)
            groupedCount += read16(header + 12 + size_t(group) * 16 + 12);
        validOwnRecords = groupedCount == count;
    }
    if (!validOwnRecords) {
        if (validPspNeighborTable(header, length)) {
            // Border sectors contain neighbor references without their own placement header.
            decoded.boundaryOnly = true;
            error.clear();
            return true;
        }
        error = "CTW world placement groups are truncated or have inconsistent counts.";
        return false;
    }
    if (afterEffects < length &&
        !validPspNeighborTable(header + size_t(afterEffects), length - size_t(afterEffects))) {
        error = "CTW world neighbor table is malformed.";
        return false;
    }
    decoded.groupCount = groups;
    decoded.shadowCount = shadows;
    decoded.lightCount = lights;
    decoded.placements.reserve(count);
    for (uint32_t index = 0; index < count; ++index) {
        const uint8_t* record = header + size_t(first) + size_t(index) * 12;
        const uint16_t packedId = read16(record);
        PakPlacement placement;
        placement.instanceId = packedId & 0x07FFu;
        placement.flags = packedId & 0xF800u;
        placement.modelId = read16(record + 2);
        placement.angle = read16(record + 4);
        placement.rawX = int16_t(read16(record + 6));
        placement.rawY = int16_t(read16(record + 8));
        placement.rawZ = int16_t(read16(record + 10));
        placement.x = placement.rawX / 128.0f;
        placement.y = placement.rawY / 128.0f;
        placement.z = placement.rawZ / 128.0f;
        decoded.placements.push_back(placement);
    }
    error.clear();
    return true;
}

bool decodePspWorldSection(const std::vector<uint8_t>& bytes, unsigned section,
                           std::vector<PakPlacement>& placements, std::string& error) {
    PakWorldSection decoded;
    const bool success = decodePspWorldSection(bytes, section, decoded, error);
    placements = std::move(decoded.placements);
    return success;
}

bool decodePspModel(const std::vector<uint8_t>& bytes, PakModel& model, std::string& error) {
    model = {};
    if (bytes.size() < 76 || read16(bytes.data()) != 1) {
        error = "Not a CTW PSP model."; return false;
    }
    const size_t materials = read16(bytes.data() + 2);
    const size_t parts = read16(bytes.data() + 4);
    const size_t vertices = read16(bytes.data() + 6);
    const size_t partTable = 76 + materials * 8;
    const size_t auxiliarySize = size_t(read16(bytes.data() + 8)) * 8 +
        size_t(read16(bytes.data() + 12)) * 24 + size_t(read16(bytes.data() + 14)) * 6 +
        size_t(read16(bytes.data() + 16)) * 6 + size_t(read16(bytes.data() + 18)) * 8;
    const size_t vertexData = partTable + parts * 16 + auxiliarySize;
    if (!parts || parts > 128 || parts != materials || !vertices ||
        vertexData + vertices * 12 > bytes.size()) {
        error = "CTW PSP model tables are truncated or unsupported."; return false;
    }
    float scale;
    const uint32_t scaleBits = read32(bytes.data() + 72);
    std::memcpy(&scale, &scaleBits, sizeof(scale));
    if (!std::isfinite(scale) || scale <= 0 || scale > 1000) {
        error = "CTW PSP model has an invalid vertex scale."; return false;
    }
    std::array<float, 4> textureTransform{};
    for (size_t component = 0; component < textureTransform.size(); ++component) {
        // GE texture scale/offset commands preserve the upper 24 bits of each float.
        const uint32_t bits = read32(bytes.data() + 32 + component * 4) & 0xFFFFFF00u;
        std::memcpy(&textureTransform[component], &bits, sizeof(float));
        if (!std::isfinite(textureTransform[component])) {
            error = "CTW PSP model has an invalid texture transform.";
            return false;
        }
    }
    size_t first = 0;
    for (size_t part = 0; part < parts; ++part) {
        const size_t count = read16(bytes.data() + partTable + part * 16);
        if (count > vertices - first) { error = "CTW model strip exceeds its vertices."; return false; }
        const uint8_t* material = bytes.data() + 76 + part * 8;
        model.parts.push_back({uint32_t(first), uint32_t(count), read16(material), material[2], material[3]});
        first += count;
    }
    if (first != vertices) { model = {}; error = "CTW model strip counts do not match."; return false; }
    model.vertices.reserve(vertices);
    for (size_t index = 0; index < vertices; ++index) {
        const uint8_t* vertex = bytes.data() + vertexData + index * 12;
        model.vertices.push_back({int16_t(read16(vertex + 6)) * scale,
            int16_t(read16(vertex + 8)) * scale, int16_t(read16(vertex + 10)) * scale,
            (vertex[0] / 128.0f) * textureTransform[0] - textureTransform[2],
            (vertex[1] / 128.0f) * textureTransform[1] - textureTransform[3],
            int8_t(vertex[2]) / 128.0f, int8_t(vertex[3]) / 128.0f, int8_t(vertex[4]) / 128.0f});
        if (!std::isfinite(model.vertices.back().u) || !std::isfinite(model.vertices.back().v)) {
            model = {};
            error = "CTW PSP model texture coordinates overflow their transform.";
            return false;
        }
    }
    error.clear(); return true;
}

bool decodePspTexture(const std::vector<uint8_t>& bytes, uint32_t& width, uint32_t& height,
                      std::vector<uint8_t>& rgba, std::string& error) {
    rgba.clear(); width = height = 0;
    if (bytes.size() < 16) { error = "PSP texture header is truncated."; return false; }
    const uint32_t format = read32(bytes.data());
    if ((format != 4 && format != 5) || bytes[4] > 11 || bytes[5] > 11) {
        error = "Unsupported CTW PSP texture format."; return false;
    }
    const uint32_t textureWidth = 1u << bytes[4];
    const uint32_t textureHeight = 1u << bytes[5];
    const uint32_t rowBytes = std::max(16u, format == 4 ? textureWidth / 2 : textureWidth);
    const uint64_t pixelBytes = uint64_t(rowBytes) * textureHeight;
    const uint32_t stored = read32(bytes.data() + 12);
    if (uint64_t(stored) + 16 > bytes.size() || stored < pixelBytes + 4 ||
        (stored - pixelBytes) % 4 != 0) {
        error = "PSP texture pixels or palette are truncated."; return false;
    }
    const size_t paletteCount = size_t((stored - pixelBytes) / 4);
    if (paletteCount > (format == 4 ? 16u : 256u)) {
        error = "PSP texture has an unsupported palette layout."; return false;
    }
    const uint8_t* pixels = bytes.data() + 16;
    const uint8_t* palette = pixels + pixelBytes;
    const bool swizzled = bytes[7] != 0;
    rgba.resize(size_t(textureWidth) * textureHeight * 4);
    for (uint32_t y = 0; y < textureHeight; ++y) {
        for (uint32_t x = 0; x < textureWidth; ++x) {
            const uint32_t byteX = format == 4 ? x / 2 : x;
            const size_t offset = swizzled ?
                size_t(y / 8) * rowBytes * 8 + size_t(byteX / 16) * 128 + (y % 8) * 16 + byteX % 16 :
                size_t(y) * rowBytes + byteX;
            if (offset >= pixelBytes) {
                rgba.clear(); error = "PSP texture swizzle address is outside its pixels."; return false;
            }
            const uint8_t packed = pixels[offset];
            const size_t index = format == 4 ? ((x & 1) ? packed >> 4 : packed & 15) : packed;
            if (index >= paletteCount) {
                rgba.clear(); error = "PSP texture references a missing palette entry."; return false;
            }
            std::copy_n(palette + index * 4, 4, rgba.data() + (size_t(y) * textureWidth + x) * 4);
        }
    }
    width = textureWidth; height = textureHeight;
    error.clear(); return true;
}

std::string guessResourceExtension(const std::vector<uint8_t>& bytes) {
    if (bytes.size() >= 4) {
        const uint32_t magic = read32(bytes.data());
        if (magic == 0x464C457Fu) return ".elf";
        if (bytes.size() >= 8 && std::memcmp(bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0) return ".png";
        if (magic == 0x20534444u) return ".dds";
        if (magic == 0x46464952u && bytes.size() >= 12 &&
            std::memcmp(bytes.data() + 8, "WAVE", 4) == 0) return ".wav";
        if (bytes.size() >= 6 && (std::memcmp(bytes.data(), "GIF87a", 6) == 0 ||
            std::memcmp(bytes.data(), "GIF89a", 6) == 0)) return ".gif";
        if (magic == 0x04034B50u) return ".zip";
        if (magic == 0x475F5344u && bytes.size() >= 6 && bytes[4] == 'X' && bytes[5] == 'T') return ".gxt";
        if (magic == 0x33435447u) return ".gxt";
        if (magic == 0x2E47494Du) return ".gim"; // MIG. PSP graphics container.
        if (magic == 0x54444354u) return ".tex"; // TCDT texture collection.
        if (bytes.size() >= 0x30u && magic == 0x1000u &&
            read32(bytes.data() + 8u) == 0x1000u &&
            read32(bytes.data() + 16u) == 0x1000u) return ".wbl";
        if (bytes.size() >= 12u) {
            const uint16_t width=read16(bytes.data());
            const uint16_t height=read16(bytes.data()+2u);
            const uint16_t marker=read16(bytes.data()+4u);
            const uint16_t format=read16(bytes.data()+6u);
            const uint32_t stored=read32(bytes.data()+8u);
            if (width>0&&height>0&&width<=4096&&height<=4096&&
                (marker&0xFF00u)==0x8C00u&&
                (format==2u||format==4u||format==0x102u||format==0x104u)&&
                stored>0u&&uint64_t(stored)+12u<=bytes.size())return ".tex";
        }
    }
    return ".bin";
}

}
