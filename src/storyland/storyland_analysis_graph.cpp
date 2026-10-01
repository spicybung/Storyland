#include "storyland_analysis_graph.h"

#include "leeds_texture.h"
#include "storyland_anim.h"
#include "storyland_model.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <functional>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>

namespace {

uint16_t readU16(const std::vector<uint8_t>& data, uint64_t offset) {
    if (offset > data.size() || data.size() - size_t(offset) < 2u) return 0u;
    const size_t o = size_t(offset);
    return uint16_t(data[o]) | uint16_t(uint16_t(data[o + 1u]) << 8u);
}

uint32_t readU32(const std::vector<uint8_t>& data, uint64_t offset) {
    if (offset > data.size() || data.size() - size_t(offset) < 4u) return 0u;
    const size_t o = size_t(offset);
    return uint32_t(data[o]) |
           (uint32_t(data[o + 1u]) << 8u) |
           (uint32_t(data[o + 2u]) << 16u) |
           (uint32_t(data[o + 3u]) << 24u);
}

std::string hexValue(uint64_t value, int width = 0) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex;
    if (width > 0) ss << std::setw(width) << std::setfill('0');
    ss << value;
    return ss.str();
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return char(std::tolower(ch));
    });
    return value;
}

std::string rwChunkName(uint32_t type) {
    switch (type) {
    case 0x01u: return "Struct";
    case 0x02u: return "String";
    case 0x03u: return "Extension";
    case 0x05u: return "Camera";
    case 0x06u: return "Texture";
    case 0x07u: return "Material";
    case 0x08u: return "Material List";
    case 0x0Eu: return "Frame List";
    case 0x0Fu: return "Geometry";
    case 0x10u: return "Clump";
    case 0x14u: return "Atomic";
    case 0x15u: return "Texture Native";
    case 0x16u: return "Texture Dictionary";
    case 0x1Au: return "Geometry List";
    case 0x116u: return "Skin PLG";
    case 0x11Eu: return "HAnim PLG";
    case 0x50Eu: return "Bin Mesh PLG";
    case 0x253F2FEu: return "Frame";
    default: {
        std::ostringstream ss;
        ss << "Chunk " << hexValue(type);
        return ss.str();
    }
    }
}

bool rwContainer(uint32_t type) {
    switch (type) {
    case 0x03u:
    case 0x06u:
    case 0x07u:
    case 0x08u:
    case 0x0Eu:
    case 0x0Fu:
    case 0x10u:
    case 0x14u:
    case 0x15u:
    case 0x16u:
    case 0x1Au:
        return true;
    default:
        return false;
    }
}

bool plausibleRwVersion(uint32_t version) {
    if (version == 0x1003FFFFu || version == 0x00000310u) return true;
    // Other RenderWare 3.x libraries encode a build stamp here. Do not accept
    // arbitrary high-entropy words because raw payloads can look like chunks.
    return (version & 0xFFFF0000u) == 0x10030000u ||
           (version & 0xFFFF0000u) == 0x18030000u;
}

std::string fixedAscii(const std::vector<uint8_t>& data, uint64_t offset, size_t maximum) {
    if (offset >= data.size()) return {};
    std::string text;
    for (size_t i = 0u; i < maximum && offset + i < data.size(); ++i) {
        const unsigned char ch = data[size_t(offset) + i];
        if (ch == 0u) break;
        if (ch < 32u || ch >= 127u) return {};
        text.push_back(char(ch));
    }
    return text;
}

} // namespace

int StorylandAnalysisGraph::addNode(
    int parent,
    uint64_t offset,
    uint64_t size,
    uint32_t type,
    uint32_t version,
    const std::string& label,
    const std::string& description) {
    StorylandAnalysisGraphNode node;
    node.parent = parent;
    node.offset = offset;
    node.size = size;
    node.type = type;
    node.version = version;
    node.label = label;
    node.description = description;
    graphNodes.push_back(std::move(node));
    return int(graphNodes.size() - 1u);
}

const std::vector<StorylandAnalysisGraphNode>& StorylandAnalysisGraph::nodes() const {
    return graphNodes;
}

const std::string& StorylandAnalysisGraph::summary() const {
    return graphSummary;
}

bool StorylandAnalysisGraph::build(
    const std::vector<uint8_t>& bytes,
    const std::string& extensionLower,
    const std::string& displayName,
    std::string& errorMessage) {
    graphNodes.clear();
    graphSummary.clear();

    if (bytes.empty()) {
        errorMessage = "Cannot graph an empty resource.";
        return false;
    }

    const std::string ext = lowerAscii(extensionLower);
    if (ext == ".dff" || ext == ".txd") {
        return buildRenderWare(bytes, displayName, errorMessage);
    }
    if (ext == ".mdl") {
        return buildMdl(bytes, displayName, errorMessage);
    }
    if (ext == ".xtx" || ext == ".chk") {
        return buildTextureArchive(bytes, ext, displayName, errorMessage);
    }
    if (ext == ".anim") {
        return buildAnim(bytes, displayName, errorMessage);
    }

    errorMessage = "Analyze Graph currently supports only DFF, TXD, MDL, XTX, CHK and ANIM.";
    return false;
}

bool StorylandAnalysisGraph::buildRenderWare(
    const std::vector<uint8_t>& bytes,
    const std::string& displayName,
    std::string& errorMessage) {
    if (bytes.size() < 12u) {
        errorMessage = "RenderWare resource is too small for a chunk header.";
        return false;
    }

    const uint32_t rootType = readU32(bytes, 0u);
    const uint32_t rootSize = readU32(bytes, 4u);
    const uint32_t rootVersion = readU32(bytes, 8u);
    if ((rootType != 0x10u && rootType != 0x16u) ||
        uint64_t(rootSize) + 12ull > bytes.size() ||
        !plausibleRwVersion(rootVersion)) {
        errorMessage = "DFF/TXD does not begin with a valid RenderWare Clump or Texture Dictionary.";
        return false;
    }

    size_t malformedRanges = 0u;
    size_t chunkCount = 0u;

    std::function<void(int, uint64_t, uint64_t, unsigned)> parseRange;
    parseRange = [&](int parent, uint64_t start, uint64_t end, unsigned depth) {
        if (depth > 32u || start > end || end > bytes.size()) return;
        uint64_t cursor = start;

        while (cursor + 12ull <= end) {
            const uint32_t type = readU32(bytes, cursor + 0u);
            const uint32_t payloadSize = readU32(bytes, cursor + 4u);
            const uint32_t version = readU32(bytes, cursor + 8u);
            const uint64_t payload = cursor + 12ull;
            const uint64_t totalSize = 12ull + uint64_t(payloadSize);

            if (!plausibleRwVersion(version) ||
                totalSize > end - cursor) {
                const uint64_t remainder = end - cursor;
                if (remainder != 0u) {
                    addNode(
                        parent, cursor, remainder, 0u, 0u,
                        "Raw / unparsed data [" + std::to_string(remainder) +
                            " bytes @ " + hexValue(cursor) + "]",
                        "The remaining container bytes are not a valid child-chunk sequence.");
                    ++malformedRanges;
                }
                return;
            }

            std::string name = rwChunkName(type);
            if (type == 0x02u) {
                const std::string value = fixedAscii(bytes, payload, std::min<size_t>(payloadSize, 96u));
                if (!value.empty()) name += " \"" + value + "\"";
            } else if (type == 0x15u && payloadSize >= 0x30u) {
                // Native PSP beta TXD name is in the Struct child; attach a
                // friendly name when the layout is immediately recognizable.
                if (payload + 12ull <= end && readU32(bytes, payload) == 0x01u) {
                    const uint64_t structPayload = payload + 12ull;
                    if (structPayload + 0x28ull <= payload + payloadSize) {
                        const std::string value = fixedAscii(bytes, structPayload + 8ull, 32u);
                        if (!value.empty()) name += " \"" + value + "\"";
                    }
                }
            }

            std::ostringstream label;
            label << name
                  << " [" << payloadSize << " bytes @ " << hexValue(payload) << "]"
                  << " - [" << hexValue(type) << "]";

            std::ostringstream description;
            description << "Chunk header @ " << hexValue(cursor)
                        << ", payload @ " << hexValue(payload)
                        << ", payload size " << payloadSize
                        << ", total size " << totalSize
                        << ", version/build " << hexValue(version);

            const int node = addNode(parent, cursor, totalSize, type, version, label.str(), description.str());
            ++chunkCount;

            if (rwContainer(type) && payloadSize >= 12u) {
                parseRange(node, payload, payload + payloadSize, depth + 1u);
            }

            cursor += totalSize;
        }

        if (cursor < end) {
            const uint64_t remainder = end - cursor;
            addNode(
                parent, cursor, remainder, 0u, 0u,
                "Trailing data [" + std::to_string(remainder) +
                    " bytes @ " + hexValue(cursor) + "]",
                "Container tail is smaller than a RenderWare 12-byte chunk header.");
        }
    };

    parseRange(-1, 0u, uint64_t(rootSize) + 12ull, 0u);

    if (graphNodes.empty()) {
        errorMessage = "No RenderWare chunks could be decoded.";
        return false;
    }

    std::ostringstream summaryText;
    summaryText << displayName << ": " << chunkCount
                << " RenderWare chunks";
    if (malformedRanges != 0u) summaryText << ", " << malformedRanges << " raw/unparsed range(s)";
    graphSummary = summaryText.str();
    errorMessage.clear();
    return true;
}

bool StorylandAnalysisGraph::buildMdl(
    const std::vector<uint8_t>& bytes,
    const std::string& displayName,
    std::string& errorMessage) {
    if (bytes.size() < 0x20u || readU32(bytes, 0u) != 0x006D646Cu) {
        errorMessage = "Not a Leeds MDL container.";
        return false;
    }

    StorylandModelFile model;
    std::string parseError;
    if (!model.loadFromMemory(bytes, L"analysis.mdl", parseError)) {
        errorMessage = "MDL parser rejected the resource: " + parseError;
        return false;
    }

    const int root = addNode(
        -1, 0u, bytes.size(), 0x006D646Cu, 0u,
        "Leeds MDL [" + std::to_string(bytes.size()) + " bytes]",
        "Rockstar Leeds model container.");

    const uint64_t headerSize = std::min<uint64_t>(0x40u, bytes.size());
    const int header = addNode(root, 0u, headerSize, 0u, 0u, "Header", "Top-level Leeds MDL header.");

    std::map<std::string, int> fieldGroups;
    for (const StorylandModelField& field : model.fields()) {
        int parent = header;
        if (!field.group.empty()) {
            auto found = fieldGroups.find(field.group);
            if (found == fieldGroups.end()) {
                const int groupNode = addNode(root, 0u, 0u, 0u, 0u, field.group);
                fieldGroups.emplace(field.group, groupNode);
                parent = groupNode;
            } else {
                parent = found->second;
            }
        }

        std::ostringstream label;
        label << field.name << " = " << hexValue(field.value, 8)
              << " [@ " << hexValue(field.offset) << "]";
        addNode(parent, field.offset, 4u, 0u, 0u, label.str(), field.note);
    }

    const uint32_t geometryOffset = bytes.size() >= 0x24u ? readU32(bytes, 0x20u) : 0u;
    const uint32_t localReloc = bytes.size() >= 0x10u ? readU32(bytes, 0x0Cu) : 0u;
    const uint32_t globalReloc = bytes.size() >= 0x14u ? readU32(bytes, 0x10u) : 0u;
    const uint32_t relocationCount = bytes.size() >= 0x18u ? readU32(bytes, 0x14u) : 0u;

    if (geometryOffset != 0u && geometryOffset < bytes.size()) {
        uint64_t geometryEnd = bytes.size();
        if (localReloc > geometryOffset && localReloc <= bytes.size()) geometryEnd = localReloc;
        addNode(
            root, geometryOffset, geometryEnd - geometryOffset, 0u, 0u,
            "Geometry / DMA-VIF payload [" + std::to_string(geometryEnd - geometryOffset) +
                " bytes @ " + hexValue(geometryOffset) + "]",
            "Serialized geometry and PS2 packet region.");
    }

    if (localReloc != 0u && localReloc < bytes.size()) {
        addNode(root, localReloc, std::min<uint64_t>(4u, bytes.size() - localReloc),
                0u, 0u, "Local relocation anchor [@ " + hexValue(localReloc) + "]");
    }
    if (globalReloc != 0u && globalReloc < bytes.size()) {
        const uint64_t tableBytes = std::min<uint64_t>(
            uint64_t(relocationCount) * 4ull,
            bytes.size() - globalReloc);
        addNode(
            root, globalReloc, tableBytes, 0u, 0u,
            "Global relocation table [" + std::to_string(relocationCount) +
                " entries @ " + hexValue(globalReloc) + "]",
            "File-relative pointer fields patched by the Leeds runtime loader.");
    }

    const int framesRoot = addNode(
        root, 0u, 0u, 0u, 0u,
        "Frame / HAnim graph [" + std::to_string(model.armatureBones().size()) + "]");

    const auto& bones = model.armatureBones();
    std::vector<int> boneNodes(bones.size(), -1);
    size_t unresolved = bones.size();
    for (size_t pass = 0u; pass <= bones.size() && unresolved != 0u; ++pass) {
        bool progress = false;
        for (size_t i = 0u; i < bones.size(); ++i) {
            if (boneNodes[i] >= 0) continue;
            const StorylandModelBone& bone = bones[i];

            int parentNode = framesRoot;
            if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
                const int candidate = boneNodes[bone.parentIndex];
                if (candidate < 0) continue;
                parentNode = candidate;
            }

            std::ostringstream label;
            label << (bone.name.empty() ? "unnamed" : bone.name)
                  << " [node=" << hexValue(bone.nodeId)
                  << " bone=" << hexValue(bone.boneId)
                  << " @ " << hexValue(bone.offset) << "]";
            boneNodes[i] = addNode(
                parentNode, bone.offset,
                bone.offset < bytes.size() ? std::min<uint64_t>(0xB0u, bytes.size() - bone.offset) : 0u,
                bone.nodeId, 0u, label.str(), bone.sectionKind);
            --unresolved;
            progress = true;
        }
        if (!progress) break;
    }

    for (size_t i = 0u; i < bones.size(); ++i) {
        if (boneNodes[i] >= 0) continue;
        const StorylandModelBone& bone = bones[i];
        boneNodes[i] = addNode(
            framesRoot, bone.offset,
            bone.offset < bytes.size() ? std::min<uint64_t>(0xB0u, bytes.size() - bone.offset) : 0u,
            bone.nodeId, 0u,
            (bone.name.empty() ? "unnamed" : bone.name) + " [unresolved parent]",
            bone.sectionKind);
    }

    std::ostringstream summaryText;
    summaryText << displayName << ": Leeds MDL, "
                << model.armatureBones().size() << " frames/bones, "
                << model.previewPoints().size() << " preview vertices, "
                << model.previewTriangles().size() << " preview triangles";
    graphSummary = summaryText.str();
    errorMessage.clear();
    return true;
}

bool StorylandAnalysisGraph::buildTextureArchive(
    const std::vector<uint8_t>& bytes,
    const std::string& extensionLower,
    const std::string& displayName,
    std::string& errorMessage) {
    LeedsTextureArchive archive;
    std::string parseError;
    if (!archive.loadFromMemory(bytes, LeedsPlatform::Auto, parseError, L"analysis" + std::wstring(extensionLower.begin(), extensionLower.end()))) {
        errorMessage = "Texture archive parser rejected the resource: " + parseError;
        return false;
    }

    const int root = addNode(
        -1, 0u, bytes.size(), 0u, 0u,
        (extensionLower == ".chk" ? "CHK texture archive" : "XTX texture archive") +
            std::string(" [") + std::to_string(bytes.size()) + " bytes]",
        displayName);

    if (bytes.size() >= 0x20u) {
        addNode(root, 0u, std::min<uint64_t>(0x50u, bytes.size()), 0u, 0u, "Container header");
    }

    const auto& textures = archive.textures();
    for (size_t i = 0u; i < textures.size(); ++i) {
        const LeedsTextureEntry& entry = textures[i];
        std::ostringstream label;
        label << "Texture #" << i << "  " << entry.name
              << "  " << entry.width << "x" << entry.height
              << "  " << unsigned(entry.bpp) << "bpp"
              << "  mips=" << unsigned(entry.mipCount);

        const uint64_t containerOffset = entry.containerBase;
        const uint64_t containerBytes =
            containerOffset < bytes.size()
                ? std::min<uint64_t>(0x60u, bytes.size() - containerOffset)
                : 0u;
        const int textureNode = addNode(root, containerOffset, containerBytes, 0u, 0u, label.str());

        if (entry.textureHeaderOffset < bytes.size()) {
            addNode(
                textureNode,
                entry.textureHeaderOffset,
                std::min<uint64_t>(16u, bytes.size() - entry.textureHeaderOffset),
                0u, 0u,
                "Texture header [@ " + hexValue(entry.textureHeaderOffset) + "]");
        }
        if (entry.rasterOffset < bytes.size()) {
            const uint64_t rasterBytes =
                std::min<uint64_t>(entry.blockSize, bytes.size() - entry.rasterOffset);
            addNode(
                textureNode, entry.rasterOffset, rasterBytes, 0u, 0u,
                "Raster / palette payload [" + std::to_string(rasterBytes) +
                    " bytes @ " + hexValue(entry.rasterOffset) + "]");
        }
    }

    if (bytes.size() >= 0x18u &&
        bytes[0] == 'x' && bytes[1] == 'e' && bytes[2] == 't') {
        const uint32_t relocationOffset = readU32(bytes, 0x0Cu);
        const uint32_t relocationCount = readU32(bytes, 0x14u);
        if (relocationOffset < bytes.size()) {
            const uint64_t relocationBytes = std::min<uint64_t>(
                uint64_t(relocationCount) * 4ull,
                bytes.size() - relocationOffset);
            addNode(
                root, relocationOffset, relocationBytes, 0u, 0u,
                "Relocation table [" + std::to_string(relocationCount) +
                    " entries @ " + hexValue(relocationOffset) + "]");
        }
    }

    graphSummary =
        displayName + ": " + std::to_string(textures.size()) +
        " decoded texture/material record(s)";
    errorMessage.clear();
    return true;
}

bool StorylandAnalysisGraph::buildAnim(
    const std::vector<uint8_t>& bytes,
    const std::string& displayName,
    std::string& errorMessage) {
    StorylandAnimFile anim;
    std::string parseError;
    if (!anim.loadFromMemory(bytes, L"analysis.anim", parseError)) {
        errorMessage = "ANIM parser rejected the resource: " + parseError;
        return false;
    }

    const int root = addNode(
        -1, 0u, bytes.size(), 0x616E696Du, 0u,
        "Leeds ANIM [" + std::to_string(bytes.size()) + " bytes]",
        displayName);
    const int header = addNode(
        root, 0u, std::min<uint64_t>(0x40u, bytes.size()), 0u, 0u, "Header / descriptor fields");

    for (const StorylandAnimField& field : anim.fields()) {
        std::ostringstream label;
        label << field.name << " = " << hexValue(field.value, 8)
              << " [@ " << hexValue(field.offset) << "]";
        addNode(header, field.offset, 4u, 0u, 0u, label.str(), field.note);
    }

    const int clipsRoot = addNode(
        root, 0u, 0u, 0u, 0u,
        "Animation clips [" + std::to_string(anim.clips().size()) + "]");

    std::unordered_map<uint32_t, int> clipNodes;
    for (const StorylandAnimClip& clip : anim.clips()) {
        std::ostringstream label;
        label << "Clip #" << clip.index << "  "
              << (clip.name.empty() ? "unnamed" : clip.name)
              << "  channels=" << clip.channelCount
              << "  duration=" << clip.duration
              << "  [@ " << hexValue(clip.entryOffset) << "]";
        const int node = addNode(
            clipsRoot, clip.entryOffset, 0u, 0u, 0u, label.str(),
            "Channel table @ " + hexValue(clip.channelTableOffset));
        clipNodes[clip.index] = node;
    }

    const int orphanTracks = addNode(root, 0u, 0u, 0u, 0u, "Unassigned animation tracks");
    for (const StorylandAnimTrack& track : anim.tracks()) {
        int parent = orphanTracks;
        const auto found = clipNodes.find(track.clipIndex);
        if (found != clipNodes.end()) parent = found->second;

        std::ostringstream label;
        label << "Track #" << track.index << "  "
              << (track.name.empty() ? "unnamed" : track.name)
              << "  bone=" << hexValue(track.boneId)
              << "  keys=" << track.keys.size()
              << "  [@ " << hexValue(track.offset) << "]";
        std::ostringstream description;
        description << "Channel " << track.channelIndex
                    << ", key data @ " << hexValue(track.keyDataOffset)
                    << ", stride " << track.keyStride
                    << ", source " << track.source;
        addNode(parent, track.offset, track.keyStride, track.tag, 0u, label.str(), description.str());
    }

    const int stringsRoot = addNode(
        root, 0u, 0u, 0u, 0u,
        "String / name hints [" + std::to_string(anim.stringHints().size()) + "]");
    for (size_t i = 0u; i < anim.stringHints().size() && i < 512u; ++i) {
        addNode(stringsRoot, 0u, 0u, 0u, 0u, anim.stringHints()[i]);
    }

    graphSummary =
        displayName + ": " + std::to_string(anim.clips().size()) +
        " clips, " + std::to_string(anim.tracks().size()) + " tracks";
    errorMessage.clear();
    return true;
}
