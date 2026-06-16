#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")

using namespace Gdiplus;

static constexpr int IDC_TEXTURE_LIST = 1001;
static constexpr int IDC_OPEN_BUTTON = 1002;
static constexpr int IDC_EXPORT_BUTTON = 1003;
static constexpr int IDC_REPLACE_BUTTON = 1004;
static constexpr int IDC_SAVE_BUTTON = 1005;
static constexpr int IDC_INFO_LABEL = 1006;
static constexpr int IDC_PREVIEW = 1007;

static constexpr uint32_t InvalidOffset = 0xFFFFFFFFu;

struct TextureHeaderCandidate {
    uint32_t headerOffset = 0;
    uint32_t rasterOffset = 0;
    uint32_t packed = 0;
    bool hasVcsPrefix = false;
    uint8_t swizzleFlag = 0;
    uint8_t mipCount = 0;
    uint8_t unknownBits = 0;
    uint8_t bitsPerPixel = 0;
    uint8_t widthPower = 0;
    uint8_t heightPower = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t mainRasterBytes = 0;
    uint64_t allMipRasterBytes = 0;
    uint32_t paletteOffset = InvalidOffset;
    uint32_t paletteEntries = 0;
    int score = -1000000;
};

struct TextureEntry {
    size_t index = 0;
    std::string name;
    uint32_t containerOffset = 0;
    uint32_t textureHeaderOffset = 0;
    uint32_t previousContainerPointerOffset = 0;
    uint32_t nextContainerPointerOffset = 0;
    TextureHeaderCandidate header;
    bool replaced = false;
};

struct Rgba8 {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

class BinaryFile {
public:
    std::vector<uint8_t> bytes;
    std::filesystem::path path;

    bool load(const std::filesystem::path& inputPath, std::wstring& error) {
        path = inputPath;
        std::ifstream file(inputPath, std::ios::binary);
        if (!file) {
            error = L"Could not open file.";
            return false;
        }
        file.seekg(0, std::ios::end);
        const std::streamoff size = file.tellg();
        if (size <= 0) {
            error = L"File is empty.";
            return false;
        }
        file.seekg(0, std::ios::beg);
        bytes.resize(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(bytes.data()), size);
        if (!file) {
            error = L"Could not read the whole file.";
            return false;
        }
        return true;
    }

    bool saveAs(const std::filesystem::path& outputPath, std::wstring& error) const {
        std::ofstream file(outputPath, std::ios::binary);
        if (!file) {
            error = L"Could not create output file.";
            return false;
        }
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!file) {
            error = L"Could not write output file.";
            return false;
        }
        return true;
    }

    bool canRead(size_t offset, size_t count) const {
        return offset <= bytes.size() && count <= bytes.size() - offset;
    }

    uint16_t readU16(size_t offset) const {
        if (!canRead(offset, 2)) return 0;
        return static_cast<uint16_t>(bytes[offset]) |
               static_cast<uint16_t>(bytes[offset + 1] << 8);
    }

    uint32_t readU32(size_t offset) const {
        if (!canRead(offset, 4)) return 0;
        return static_cast<uint32_t>(bytes[offset]) |
               (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
               (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
               (static_cast<uint32_t>(bytes[offset + 3]) << 24);
    }

    void writeU32(size_t offset, uint32_t value) {
        if (!canRead(offset, 4)) return;
        bytes[offset] = static_cast<uint8_t>(value & 0xFF);
        bytes[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        bytes[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        bytes[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    }
};

static std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) return L"";
    std::wstring wide(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), count);
    return wide;
}

static std::string fixedName64(const BinaryFile& file, size_t offset) {
    std::string name;
    if (!file.canRead(offset, 64)) return name;
    for (size_t i = 0; i < 64; ++i) {
        const uint8_t c = file.bytes[offset + i];
        if (c == 0) break;
        if (c >= 32 && c < 127) {
            name.push_back(static_cast<char>(c));
        } else {
            break;
        }
    }
    return name;
}

static uint64_t calcMipBytes(uint32_t width, uint32_t height, uint32_t bpp, uint32_t mipCount) {
    if (mipCount == 0) mipCount = 1;
    uint64_t total = 0;
    uint32_t w = width;
    uint32_t h = height;
    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        const uint64_t pixels = static_cast<uint64_t>(std::max(1u, w)) * static_cast<uint64_t>(std::max(1u, h));
        total += (pixels * bpp + 7) / 8;
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
    }
    return total;
}

static TextureHeaderCandidate decodePs2HeaderCandidate(const BinaryFile& file, uint32_t textureHeaderOffset, bool vcsPrefix) {
    TextureHeaderCandidate candidate;
    candidate.headerOffset = textureHeaderOffset;
    candidate.hasVcsPrefix = vcsPrefix;

    const uint32_t headerStart = textureHeaderOffset + (vcsPrefix ? 8u : 0u);
    if (!file.canRead(headerStart, 8)) return candidate;

    candidate.rasterOffset = file.readU32(headerStart);
    candidate.packed = file.readU32(headerStart + 4);
    candidate.swizzleFlag = static_cast<uint8_t>(candidate.packed & 0xFFu);
    candidate.mipCount = static_cast<uint8_t>((candidate.packed >> 8) & 0x0Fu);
    candidate.unknownBits = static_cast<uint8_t>((candidate.packed >> 12) & 0x03u);
    candidate.bitsPerPixel = static_cast<uint8_t>((candidate.packed >> 14) & 0x3Fu);
    candidate.widthPower = static_cast<uint8_t>((candidate.packed >> 20) & 0x3Fu);
    candidate.heightPower = static_cast<uint8_t>((candidate.packed >> 26) & 0x3Fu);

    if (candidate.widthPower < 31) candidate.width = 1u << candidate.widthPower;
    if (candidate.heightPower < 31) candidate.height = 1u << candidate.heightPower;

    candidate.mainRasterBytes = 0;
    if (candidate.width != 0 && candidate.height != 0) {
        candidate.mainRasterBytes = (static_cast<uint64_t>(candidate.width) * candidate.height * candidate.bitsPerPixel + 7) / 8;
        candidate.allMipRasterBytes = calcMipBytes(candidate.width, candidate.height, candidate.bitsPerPixel, candidate.mipCount == 0 ? 1 : candidate.mipCount);
    }

    int score = 0;
    if (candidate.rasterOffset < file.bytes.size()) score += 1000; else score -= 1000;
    if (candidate.width >= 4 && candidate.width <= 4096) score += 250; else score -= 250;
    if (candidate.height >= 4 && candidate.height <= 4096) score += 250; else score -= 250;
    if (candidate.bitsPerPixel == 4 || candidate.bitsPerPixel == 8 || candidate.bitsPerPixel == 16 || candidate.bitsPerPixel == 24 || candidate.bitsPerPixel == 32) score += 400; else score -= 400;
    if (candidate.mipCount >= 1 && candidate.mipCount <= 13) score += 100; else if (candidate.mipCount == 0) score -= 25; else score -= 100;
    if (candidate.mainRasterBytes > 0 && candidate.rasterOffset + candidate.mainRasterBytes <= file.bytes.size()) score += 1000; else score -= 1000;
    if (candidate.allMipRasterBytes > 0 && candidate.rasterOffset + candidate.allMipRasterBytes <= file.bytes.size()) score += 200; else score -= 100;
    if (vcsPrefix) {
        const uint32_t possibleZero = file.readU32(textureHeaderOffset);
        if (possibleZero == 0) score += 150;
    }

    if (candidate.bitsPerPixel == 4 || candidate.bitsPerPixel == 8) {
        const uint32_t entries = candidate.bitsPerPixel == 4 ? 16u : 256u;
        const uint64_t paletteOffset64 = static_cast<uint64_t>(candidate.rasterOffset) + candidate.allMipRasterBytes;
        if (paletteOffset64 <= std::numeric_limits<uint32_t>::max() && file.canRead(static_cast<size_t>(paletteOffset64), entries * 4u)) {
            candidate.paletteOffset = static_cast<uint32_t>(paletteOffset64);
            candidate.paletteEntries = entries;
            score += 250;
        }
    }

    candidate.score = score;
    return candidate;
}

static TextureHeaderCandidate choosePs2TextureHeader(const BinaryFile& file, uint32_t textureHeaderOffset) {
    TextureHeaderCandidate lcs = decodePs2HeaderCandidate(file, textureHeaderOffset, false);
    TextureHeaderCandidate vcs = decodePs2HeaderCandidate(file, textureHeaderOffset, true);
    return vcs.score > lcs.score ? vcs : lcs;
}

class LeedsTextureCollection {
public:
    BinaryFile file;
    std::vector<TextureEntry> textures;
    bool dirty = false;
    std::wstring lastError;

    bool open(const std::filesystem::path& path) {
        textures.clear();
        dirty = false;
        lastError.clear();
        if (!file.load(path, lastError)) return false;
        return parse();
    }

    bool saveAs(const std::filesystem::path& path) {
        if (!file.saveAs(path, lastError)) return false;
        dirty = false;
        return true;
    }

private:
    bool parse() {
        if (!file.canRead(0, 0x30)) {
            lastError = L"File is too small for a CHK/XTX collection header.";
            return false;
        }

        const uint32_t signature = file.readU32(0);
        const bool looksLikeXet = (signature & 0x00FFFFFFu) == 0x00746578u; // 'x' 'e' 't'
        const bool looksLikeTcdt = signature == 0x54444354u; // 'TCDT'
        if (!looksLikeXet && !looksLikeTcdt) {
            lastError = L"Header is not xet/TCDT. This first version is for LCS/VCS CHK/XTX plus partial MH2 detection.";
            return false;
        }

        if (looksLikeTcdt) {
            lastError = L"TCDT/Manhunt 2 containers are detected but not implemented in this first GUI build.";
            return false;
        }

        const uint32_t collectionSize = file.readU32(0x08);
        const uint32_t firstContainerPointerOffset = file.readU32(0x28);
        const uint32_t lastContainerPointerOffset = file.readU32(0x2C);

        if (collectionSize != 0 && collectionSize > file.bytes.size() + 4096) {
            lastError = L"Collection size in header is much larger than the file. Refusing to parse.";
            return false;
        }
        if (!file.canRead(firstContainerPointerOffset, 4)) {
            lastError = L"First-container pointer offset is outside the file.";
            return false;
        }
        if (!file.canRead(lastContainerPointerOffset, 4)) {
            lastError = L"Last-container pointer offset is outside the file.";
            return false;
        }

        uint32_t containerOffset = file.readU32(firstContainerPointerOffset);
        const uint32_t lastContainerOffset = file.readU32(lastContainerPointerOffset);
        std::vector<uint32_t> visited;

        for (size_t safety = 0; safety < 10000; ++safety) {
            if (!file.canRead(containerOffset, 80)) break;
            if (std::find(visited.begin(), visited.end(), containerOffset) != visited.end()) break;
            visited.push_back(containerOffset);

            TextureEntry entry;
            entry.index = textures.size();
            entry.containerOffset = containerOffset;
            entry.textureHeaderOffset = file.readU32(containerOffset + 0);
            entry.previousContainerPointerOffset = file.readU32(containerOffset + 8);
            entry.nextContainerPointerOffset = file.readU32(containerOffset + 12);
            entry.name = fixedName64(file, containerOffset + 16);
            if (entry.name.empty()) {
                std::ostringstream name;
                name << "texture_" << entry.index;
                entry.name = name.str();
            }
            entry.header = choosePs2TextureHeader(file, entry.textureHeaderOffset);
            textures.push_back(entry);

            if (containerOffset == lastContainerOffset) break;
            if (!file.canRead(entry.nextContainerPointerOffset, 4)) break;
            const uint32_t nextContainerOffset = file.readU32(entry.nextContainerPointerOffset);
            if (nextContainerOffset == 0 || nextContainerOffset == containerOffset) break;
            containerOffset = nextContainerOffset;
        }

        if (textures.empty()) {
            lastError = L"No texture containers could be followed from the collection linked list.";
            return false;
        }
        return true;
    }
};

static Rgba8 readPaletteEntryPs2(const BinaryFile& file, uint32_t paletteOffset, uint32_t index) {
    const size_t offset = static_cast<size_t>(paletteOffset) + static_cast<size_t>(index) * 4;
    Rgba8 color;
    if (!file.canRead(offset, 4)) return color;
    color.r = file.bytes[offset + 0];
    color.g = file.bytes[offset + 1];
    color.b = file.bytes[offset + 2];
    color.a = file.bytes[offset + 3];
    if (color.a < 128) {
        color.a = static_cast<uint8_t>(std::min(255, color.a * 2));
    }
    return color;
}

static std::vector<Rgba8> loadPalette(const BinaryFile& file, const TextureHeaderCandidate& header) {
    std::vector<Rgba8> palette;
    if (header.paletteOffset == InvalidOffset || header.paletteEntries == 0) return palette;
    palette.reserve(header.paletteEntries);
    for (uint32_t i = 0; i < header.paletteEntries; ++i) {
        palette.push_back(readPaletteEntryPs2(file, header.paletteOffset, i));
    }
    return palette;
}

static uint32_t ps2Unswizzle8Index(uint32_t x, uint32_t y, uint32_t width) {
    const uint32_t pageX = x & ~127u;
    const uint32_t pageY = y & ~63u;
    const uint32_t pagesPerRow = (width + 127u) / 128u;
    const uint32_t page = (pageY / 64u) * pagesPerRow + (pageX / 128u);
    const uint32_t px = x & 127u;
    const uint32_t py = y & 63u;
    const uint32_t block = ((py >> 4u) * 4u) + (px >> 5u);
    const uint32_t column = ((py & 0x0Fu) * 2u) + ((px >> 4u) & 1u);
    const uint32_t cx = px & 0x0Fu;
    return page * 8192u + block * 1024u + column * 32u + cx * 2u + ((py >> 3u) & 1u);
}

static uint32_t ps2Unswizzle4ByteIndex(uint32_t x, uint32_t y, uint32_t width) {
    return ps2Unswizzle8Index(x / 2u, y, std::max(1u, width / 2u));
}

static uint8_t readIndexedPixel(const BinaryFile& file, const TextureHeaderCandidate& header, uint32_t x, uint32_t y) {
    if (header.bitsPerPixel == 8) {
        uint32_t index = y * header.width + x;
        if (header.swizzleFlag > 0) {
            index = ps2Unswizzle8Index(x, y, header.width);
        }
        const size_t offset = static_cast<size_t>(header.rasterOffset) + index;
        if (!file.canRead(offset, 1)) return 0;
        return file.bytes[offset];
    }

    if (header.bitsPerPixel == 4) {
        uint32_t byteIndex = y * ((header.width + 1u) / 2u) + (x / 2u);
        if (header.swizzleFlag > 0) {
            byteIndex = ps2Unswizzle4ByteIndex(x, y, header.width);
        }
        const size_t offset = static_cast<size_t>(header.rasterOffset) + byteIndex;
        if (!file.canRead(offset, 1)) return 0;
        const uint8_t packed = file.bytes[offset];
        return (x & 1u) ? static_cast<uint8_t>((packed >> 4u) & 0x0Fu) : static_cast<uint8_t>(packed & 0x0Fu);
    }

    return 0;
}

static void writeIndexedPixel(BinaryFile& file, const TextureHeaderCandidate& header, uint32_t x, uint32_t y, uint8_t value) {
    if (header.bitsPerPixel == 8) {
        uint32_t index = y * header.width + x;
        if (header.swizzleFlag > 0) {
            index = ps2Unswizzle8Index(x, y, header.width);
        }
        const size_t offset = static_cast<size_t>(header.rasterOffset) + index;
        if (file.canRead(offset, 1)) file.bytes[offset] = value;
        return;
    }

    if (header.bitsPerPixel == 4) {
        uint32_t byteIndex = y * ((header.width + 1u) / 2u) + (x / 2u);
        if (header.swizzleFlag > 0) {
            byteIndex = ps2Unswizzle4ByteIndex(x, y, header.width);
        }
        const size_t offset = static_cast<size_t>(header.rasterOffset) + byteIndex;
        if (!file.canRead(offset, 1)) return;
        if (x & 1u) {
            file.bytes[offset] = static_cast<uint8_t>((file.bytes[offset] & 0x0Fu) | ((value & 0x0Fu) << 4u));
        } else {
            file.bytes[offset] = static_cast<uint8_t>((file.bytes[offset] & 0xF0u) | (value & 0x0Fu));
        }
    }
}

static Rgba8 readTrueColorPixel(const BinaryFile& file, const TextureHeaderCandidate& header, uint32_t x, uint32_t y) {
    Rgba8 color;
    const uint64_t pixelIndex = static_cast<uint64_t>(y) * header.width + x;
    if (header.bitsPerPixel == 32) {
        const uint64_t offset64 = static_cast<uint64_t>(header.rasterOffset) + pixelIndex * 4;
        if (!file.canRead(static_cast<size_t>(offset64), 4)) return color;
        color.r = file.bytes[static_cast<size_t>(offset64) + 0];
        color.g = file.bytes[static_cast<size_t>(offset64) + 1];
        color.b = file.bytes[static_cast<size_t>(offset64) + 2];
        color.a = file.bytes[static_cast<size_t>(offset64) + 3];
        if (color.a < 128) color.a = static_cast<uint8_t>(std::min(255, color.a * 2));
        return color;
    }

    if (header.bitsPerPixel == 24) {
        const uint64_t offset64 = static_cast<uint64_t>(header.rasterOffset) + pixelIndex * 3;
        if (!file.canRead(static_cast<size_t>(offset64), 3)) return color;
        color.r = file.bytes[static_cast<size_t>(offset64) + 0];
        color.g = file.bytes[static_cast<size_t>(offset64) + 1];
        color.b = file.bytes[static_cast<size_t>(offset64) + 2];
        color.a = 255;
        return color;
    }

    if (header.bitsPerPixel == 16) {
        const uint64_t offset64 = static_cast<uint64_t>(header.rasterOffset) + pixelIndex * 2;
        if (!file.canRead(static_cast<size_t>(offset64), 2)) return color;
        const uint16_t raw = file.readU16(static_cast<size_t>(offset64));
        color.r = static_cast<uint8_t>(((raw >> 0) & 0x1F) * 255 / 31);
        color.g = static_cast<uint8_t>(((raw >> 5) & 0x1F) * 255 / 31);
        color.b = static_cast<uint8_t>(((raw >> 10) & 0x1F) * 255 / 31);
        color.a = (raw & 0x8000) ? 255 : 0;
        return color;
    }

    return color;
}

static bool decodeTextureToBitmap(const BinaryFile& file, const TextureEntry& entry, Bitmap*& bitmap, std::wstring& error) {
    const TextureHeaderCandidate& header = entry.header;
    if (header.width == 0 || header.height == 0) {
        error = L"Texture has invalid dimensions.";
        return false;
    }

    bitmap = new Bitmap(static_cast<INT>(header.width), static_cast<INT>(header.height), PixelFormat32bppARGB);
    if (!bitmap || bitmap->GetLastStatus() != Ok) {
        delete bitmap;
        bitmap = nullptr;
        error = L"Could not allocate preview bitmap.";
        return false;
    }

    std::vector<Rgba8> palette = loadPalette(file, header);
    for (uint32_t y = 0; y < header.height; ++y) {
        for (uint32_t x = 0; x < header.width; ++x) {
            Rgba8 c;
            if (header.bitsPerPixel == 4 || header.bitsPerPixel == 8) {
                const uint8_t index = readIndexedPixel(file, header, x, y);
                if (index < palette.size()) c = palette[index];
                else c = { index, index, index, 255 };
            } else {
                c = readTrueColorPixel(file, header, x, y);
            }
            bitmap->SetPixel(static_cast<INT>(x), static_cast<INT>(y), Color(c.a, c.r, c.g, c.b));
        }
    }
    return true;
}

static int nearestPaletteColor(const std::vector<Rgba8>& palette, const Rgba8& color) {
    int bestIndex = 0;
    uint64_t bestDistance = std::numeric_limits<uint64_t>::max();
    for (size_t i = 0; i < palette.size(); ++i) {
        const int dr = static_cast<int>(palette[i].r) - static_cast<int>(color.r);
        const int dg = static_cast<int>(palette[i].g) - static_cast<int>(color.g);
        const int db = static_cast<int>(palette[i].b) - static_cast<int>(color.b);
        const int da = static_cast<int>(palette[i].a) - static_cast<int>(color.a);
        const uint64_t d = static_cast<uint64_t>(dr * dr + dg * dg + db * db + da * da);
        if (d < bestDistance) {
            bestDistance = d;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

static bool readBitmapFileToRgba(const std::filesystem::path& path, uint32_t expectedWidth, uint32_t expectedHeight, std::vector<Rgba8>& pixels, std::wstring& error) {
    Bitmap bitmap(path.c_str());
    if (bitmap.GetLastStatus() != Ok) {
        error = L"GDI+ could not open the replacement image. Use PNG/BMP/TGA-like formats supported by Windows codecs.";
        return false;
    }
    if (bitmap.GetWidth() != expectedWidth || bitmap.GetHeight() != expectedHeight) {
        std::wstringstream ss;
        ss << L"Replacement image must be exactly " << expectedWidth << L"x" << expectedHeight << L" for this safe in-place replacement.";
        error = ss.str();
        return false;
    }
    pixels.resize(static_cast<size_t>(expectedWidth) * expectedHeight);
    for (uint32_t y = 0; y < expectedHeight; ++y) {
        for (uint32_t x = 0; x < expectedWidth; ++x) {
            Color color;
            bitmap.GetPixel(static_cast<INT>(x), static_cast<INT>(y), &color);
            pixels[static_cast<size_t>(y) * expectedWidth + x] = {
                static_cast<uint8_t>(color.GetR()),
                static_cast<uint8_t>(color.GetG()),
                static_cast<uint8_t>(color.GetB()),
                static_cast<uint8_t>(color.GetA())
            };
        }
    }
    return true;
}

static bool replaceTextureFromImage(LeedsTextureCollection& collection, TextureEntry& entry, const std::filesystem::path& imagePath, std::wstring& error) {
    const TextureHeaderCandidate& header = entry.header;
    std::vector<Rgba8> pixels;
    if (!readBitmapFileToRgba(imagePath, header.width, header.height, pixels, error)) return false;

    if (header.bitsPerPixel == 4 || header.bitsPerPixel == 8) {
        std::vector<Rgba8> palette = loadPalette(collection.file, header);
        if (palette.empty()) {
            error = L"This indexed texture has no palette where the current scanner expected one. Replacement refused.";
            return false;
        }
        const int maxIndex = header.bitsPerPixel == 4 ? 15 : 255;
        for (uint32_t y = 0; y < header.height; ++y) {
            for (uint32_t x = 0; x < header.width; ++x) {
                const Rgba8& color = pixels[static_cast<size_t>(y) * header.width + x];
                const uint8_t index = static_cast<uint8_t>(std::min(maxIndex, nearestPaletteColor(palette, color)));
                writeIndexedPixel(collection.file, header, x, y, index);
            }
        }
    } else if (header.bitsPerPixel == 32) {
        for (uint32_t y = 0; y < header.height; ++y) {
            for (uint32_t x = 0; x < header.width; ++x) {
                const uint64_t offset64 = static_cast<uint64_t>(header.rasterOffset) + (static_cast<uint64_t>(y) * header.width + x) * 4;
                if (!collection.file.canRead(static_cast<size_t>(offset64), 4)) continue;
                const Rgba8& color = pixels[static_cast<size_t>(y) * header.width + x];
                collection.file.bytes[static_cast<size_t>(offset64) + 0] = color.r;
                collection.file.bytes[static_cast<size_t>(offset64) + 1] = color.g;
                collection.file.bytes[static_cast<size_t>(offset64) + 2] = color.b;
                collection.file.bytes[static_cast<size_t>(offset64) + 3] = static_cast<uint8_t>(std::min(128, static_cast<int>(color.a / 2)));
            }
        }
    } else if (header.bitsPerPixel == 24) {
        for (uint32_t y = 0; y < header.height; ++y) {
            for (uint32_t x = 0; x < header.width; ++x) {
                const uint64_t offset64 = static_cast<uint64_t>(header.rasterOffset) + (static_cast<uint64_t>(y) * header.width + x) * 3;
                if (!collection.file.canRead(static_cast<size_t>(offset64), 3)) continue;
                const Rgba8& color = pixels[static_cast<size_t>(y) * header.width + x];
                collection.file.bytes[static_cast<size_t>(offset64) + 0] = color.r;
                collection.file.bytes[static_cast<size_t>(offset64) + 1] = color.g;
                collection.file.bytes[static_cast<size_t>(offset64) + 2] = color.b;
            }
        }
    } else {
        error = L"Replacement for this BPP is not implemented yet.";
        return false;
    }

    entry.replaced = true;
    collection.dirty = true;
    return true;
}

static int getPngEncoderClsid(CLSID* clsid) {
    UINT count = 0;
    UINT size = 0;
    GetImageEncodersSize(&count, &size);
    if (size == 0) return -1;
    std::vector<uint8_t> buffer(size);
    ImageCodecInfo* info = reinterpret_cast<ImageCodecInfo*>(buffer.data());
    GetImageEncoders(count, size, info);
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(info[i].MimeType, L"image/png") == 0) {
            *clsid = info[i].Clsid;
            return static_cast<int>(i);
        }
    }
    return -1;
}

static std::optional<std::filesystem::path> openFileDialog(HWND owner, const wchar_t* title, const wchar_t* filter) {
    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrTitle = title;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return std::nullopt;
    return std::filesystem::path(filename);
}

static std::optional<std::filesystem::path> saveFileDialog(HWND owner, const wchar_t* title, const wchar_t* filter, const wchar_t* defaultExtension) {
    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrTitle = title;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = defaultExtension;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return std::nullopt;
    return std::filesystem::path(filename);
}

class AppWindow {
public:
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HWND list = nullptr;
    HWND info = nullptr;
    HWND preview = nullptr;
    LeedsTextureCollection collection;
    Bitmap* currentBitmap = nullptr;
    ULONG_PTR gdiplusToken = 0;

    ~AppWindow() {
        delete currentBitmap;
        if (gdiplusToken) GdiplusShutdown(gdiplusToken);
    }

    bool create(HINSTANCE hInstance) {
        instance = hInstance;
        GdiplusStartupInput gdiplusStartupInput;
        if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) return false;

        INITCOMMONCONTROLSEX icc = {};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&icc);

        WNDCLASSW wc = {};
        wc.lpfnWndProc = AppWindow::windowProcedure;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"LeedsTextureViewerWindow";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&wc);

        window = CreateWindowExW(0, wc.lpszClassName, L"Leeds CHK/XTX Texture Viewer", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1120, 720, nullptr, nullptr, hInstance, this);
        if (!window) return false;
        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
        return true;
    }

    void layout() {
        RECT rc;
        GetClientRect(window, &rc);
        const int margin = 8;
        const int buttonHeight = 30;
        const int top = margin;
        const int listWidth = 640;
        MoveWindow(GetDlgItem(window, IDC_OPEN_BUTTON), margin, top, 110, buttonHeight, TRUE);
        MoveWindow(GetDlgItem(window, IDC_EXPORT_BUTTON), margin + 120, top, 110, buttonHeight, TRUE);
        MoveWindow(GetDlgItem(window, IDC_REPLACE_BUTTON), margin + 240, top, 120, buttonHeight, TRUE);
        MoveWindow(GetDlgItem(window, IDC_SAVE_BUTTON), margin + 370, top, 110, buttonHeight, TRUE);
        MoveWindow(list, margin, top + buttonHeight + margin, listWidth, rc.bottom - top - buttonHeight - margin * 2, TRUE);
        MoveWindow(preview, margin + listWidth + margin, top + buttonHeight + margin, rc.right - listWidth - margin * 3, rc.bottom - top - buttonHeight - 120, TRUE);
        MoveWindow(info, margin + listWidth + margin, rc.bottom - 105, rc.right - listWidth - margin * 3, 95, TRUE);
    }

    void buildControls() {
        CreateWindowW(L"BUTTON", L"Open", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(IDC_OPEN_BUTTON), instance, nullptr);
        CreateWindowW(L"BUTTON", L"Export PNG", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(IDC_EXPORT_BUTTON), instance, nullptr);
        CreateWindowW(L"BUTTON", L"Replace", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(IDC_REPLACE_BUTTON), instance, nullptr);
        CreateWindowW(L"BUTTON", L"Save As", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, window, reinterpret_cast<HMENU>(IDC_SAVE_BUTTON), instance, nullptr);

        list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                               0, 0, 0, 0, window, reinterpret_cast<HMENU>(IDC_TEXTURE_LIST), instance, nullptr);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        addColumn(0, L"#", 50);
        addColumn(1, L"Name", 220);
        addColumn(2, L"Size", 100);
        addColumn(3, L"BPP", 60);
        addColumn(4, L"Mip", 60);
        addColumn(5, L"Raster", 90);
        addColumn(6, L"Header", 90);
        addColumn(7, L"Flags", 90);

        preview = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_BLACKFRAME,
                                  0, 0, 0, 0, window, reinterpret_cast<HMENU>(IDC_PREVIEW), instance, nullptr);
        info = CreateWindowW(L"STATIC", L"Open a .chk or .xtx file.", WS_CHILD | WS_VISIBLE | SS_LEFT,
                             0, 0, 0, 0, window, reinterpret_cast<HMENU>(IDC_INFO_LABEL), instance, nullptr);
        layout();
    }

    void addColumn(int index, const wchar_t* title, int width) {
        LVCOLUMNW column = {};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = const_cast<wchar_t*>(title);
        column.cx = width;
        column.iSubItem = index;
        ListView_InsertColumn(list, index, &column);
    }

    int selectedIndex() const {
        return ListView_GetNextItem(list, -1, LVNI_SELECTED);
    }

    void populateList() {
        ListView_DeleteAllItems(list);
        for (size_t i = 0; i < collection.textures.size(); ++i) {
            const TextureEntry& entry = collection.textures[i];
            const TextureHeaderCandidate& h = entry.header;
            std::wstring indexText = std::to_wstring(i);
            LVITEMW item = {};
            item.mask = LVIF_TEXT;
            item.iItem = static_cast<int>(i);
            item.pszText = indexText.data();
            ListView_InsertItem(list, &item);

            setSubItem(i, 1, utf8ToWide(entry.name));
            setSubItem(i, 2, std::to_wstring(h.width) + L"x" + std::to_wstring(h.height));
            setSubItem(i, 3, std::to_wstring(h.bitsPerPixel));
            setSubItem(i, 4, std::to_wstring(h.mipCount));
            setSubItem(i, 5, hex32(h.rasterOffset));
            setSubItem(i, 6, hex32(entry.textureHeaderOffset));
            std::wstring flags = h.hasVcsPrefix ? L"VCS+8" : L"Base";
            if (h.swizzleFlag) flags += L" swz";
            if (entry.replaced) flags += L" *";
            setSubItem(i, 7, flags);
        }
        if (!collection.textures.empty()) {
            ListView_SetItemState(list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            showSelectedTexture();
        }
    }

    void setSubItem(size_t row, int column, const std::wstring& text) {
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(row);
        item.iSubItem = column;
        item.pszText = const_cast<wchar_t*>(text.c_str());
        ListView_SetItem(list, &item);
    }

    static std::wstring hex32(uint32_t value) {
        wchar_t buffer[32];
        swprintf_s(buffer, L"0x%08X", value);
        return buffer;
    }

    void showSelectedTexture() {
        const int index = selectedIndex();
        if (index < 0 || static_cast<size_t>(index) >= collection.textures.size()) return;
        TextureEntry& entry = collection.textures[static_cast<size_t>(index)];
        delete currentBitmap;
        currentBitmap = nullptr;
        std::wstring error;
        if (!decodeTextureToBitmap(collection.file, entry, currentBitmap, error)) {
            SetWindowTextW(info, error.c_str());
            InvalidateRect(preview, nullptr, TRUE);
            return;
        }
        const TextureHeaderCandidate& h = entry.header;
        std::wstringstream ss;
        ss << L"Name: " << utf8ToWide(entry.name)
           << L"\nContainer: " << hex32(entry.containerOffset)
           << L"  Texture header: " << hex32(entry.textureHeaderOffset)
           << L"  Raster: " << hex32(h.rasterOffset)
           << L"\nSize: " << h.width << L"x" << h.height
           << L"  BPP: " << static_cast<int>(h.bitsPerPixel)
           << L"  Mips: " << static_cast<int>(h.mipCount)
           << L"  Swizzle flag: " << static_cast<int>(h.swizzleFlag)
           << L"  Palette: " << (h.paletteOffset == InvalidOffset ? L"none/unknown" : hex32(h.paletteOffset));
        SetWindowTextW(info, ss.str().c_str());
        InvalidateRect(preview, nullptr, TRUE);
    }

    void drawPreview(HDC hdc) {
        RECT rc;
        GetClientRect(preview, &rc);
        HDC memDc = CreateCompatibleDC(hdc);
        HBITMAP backBuffer = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        HGDIOBJ oldBitmap = SelectObject(memDc, backBuffer);
        FillRect(memDc, &rc, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));

        if (currentBitmap) {
            Graphics graphics(memDc);
            graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
            graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
            const int sourceW = static_cast<int>(currentBitmap->GetWidth());
            const int sourceH = static_cast<int>(currentBitmap->GetHeight());
            const double sx = static_cast<double>(rc.right - rc.left) / std::max(1, sourceW);
            const double sy = static_cast<double>(rc.bottom - rc.top) / std::max(1, sourceH);
            const double scale = std::min(sx, sy);
            const int drawW = std::max(1, static_cast<int>(sourceW * scale));
            const int drawH = std::max(1, static_cast<int>(sourceH * scale));
            const int x = (rc.right - drawW) / 2;
            const int y = (rc.bottom - drawH) / 2;
            graphics.DrawImage(currentBitmap, x, y, drawW, drawH);
        }

        BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDc, 0, 0, SRCCOPY);
        SelectObject(memDc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(memDc);
    }

    void openCollection() {
        auto path = openFileDialog(window, L"Open CHK/XTX", L"Leeds textures (*.chk;*.xtx)\0*.chk;*.xtx\0All files\0*.*\0");
        if (!path) return;
        if (!collection.open(*path)) {
            MessageBoxW(window, collection.lastError.c_str(), L"Open failed", MB_ICONERROR);
            return;
        }
        populateList();
    }

    void exportSelected() {
        const int index = selectedIndex();
        if (index < 0 || static_cast<size_t>(index) >= collection.textures.size()) return;
        showSelectedTexture();
        if (!currentBitmap) return;
        auto path = saveFileDialog(window, L"Export selected texture as PNG", L"PNG image (*.png)\0*.png\0All files\0*.*\0", L"png");
        if (!path) return;
        CLSID pngClsid;
        if (getPngEncoderClsid(&pngClsid) < 0) {
            MessageBoxW(window, L"Could not find PNG encoder.", L"Export failed", MB_ICONERROR);
            return;
        }
        if (currentBitmap->Save(path->c_str(), &pngClsid, nullptr) != Ok) {
            MessageBoxW(window, L"GDI+ failed while saving PNG.", L"Export failed", MB_ICONERROR);
        }
    }

    void replaceSelected() {
        const int index = selectedIndex();
        if (index < 0 || static_cast<size_t>(index) >= collection.textures.size()) return;
        auto path = openFileDialog(window, L"Choose replacement image", L"Images (*.png;*.bmp;*.jpg;*.jpeg)\0*.png;*.bmp;*.jpg;*.jpeg\0All files\0*.*\0");
        if (!path) return;
        std::wstring error;
        TextureEntry& entry = collection.textures[static_cast<size_t>(index)];
        if (!replaceTextureFromImage(collection, entry, *path, error)) {
            MessageBoxW(window, error.c_str(), L"Replacement failed", MB_ICONERROR);
            return;
        }
        populateList();
        ListView_SetItemState(list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        showSelectedTexture();
    }

    void saveCollection() {
        if (collection.file.bytes.empty()) return;
        auto path = saveFileDialog(window, L"Save patched CHK/XTX as", L"Leeds textures (*.chk;*.xtx)\0*.chk;*.xtx\0All files\0*.*\0", L"xtx");
        if (!path) return;
        if (!collection.saveAs(*path)) {
            MessageBoxW(window, collection.lastError.c_str(), L"Save failed", MB_ICONERROR);
        }
    }

    static LRESULT CALLBACK windowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        AppWindow* self = nullptr;
        if (message == WM_NCCREATE) {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = reinterpret_cast<AppWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->window = hwnd;
        } else {
            self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (!self) return DefWindowProcW(hwnd, message, wParam, lParam);

        switch (message) {
        case WM_CREATE:
            self->buildControls();
            return 0;
        case WM_SIZE:
            self->layout();
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case IDC_OPEN_BUTTON: self->openCollection(); return 0;
            case IDC_EXPORT_BUTTON: self->exportSelected(); return 0;
            case IDC_REPLACE_BUTTON: self->replaceSelected(); return 0;
            case IDC_SAVE_BUTTON: self->saveCollection(); return 0;
            }
            break;
        case WM_NOTIFY:
            if (reinterpret_cast<NMHDR*>(lParam)->hwndFrom == self->list && reinterpret_cast<NMHDR*>(lParam)->code == LVN_ITEMCHANGED) {
                const NMLISTVIEW* nmlv = reinterpret_cast<NMLISTVIEW*>(lParam);
                if ((nmlv->uNewState & LVIS_SELECTED) != 0) self->showSelectedTexture();
            }
            break;
        case WM_PAINT:
            if (reinterpret_cast<HWND>(wParam) == self->preview) break;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
};

static LRESULT CALLBACK previewSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData) {
    AppWindow* app = reinterpret_cast<AppWindow*>(refData);
    if (message == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        app->drawPreview(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    AppWindow app;
    if (!app.create(hInstance)) return 1;
    SetWindowSubclass(app.preview, previewSubclassProc, 1, reinterpret_cast<DWORD_PTR>(&app));

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
