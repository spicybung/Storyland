#include "storyland_model_validator.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <functional>
#include <set>
#include <sstream>

namespace {

uint32_t readU32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) return 0;
    return uint32_t(bytes[offset]) |
           (uint32_t(bytes[offset + 1]) << 8) |
           (uint32_t(bytes[offset + 2]) << 16) |
           (uint32_t(bytes[offset + 3]) << 24);
}


void writeU32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    if (offset + 4 > bytes.size()) return;
    bytes[offset + 0] = uint8_t(value & 0xFFu);
    bytes[offset + 1] = uint8_t((value >> 8) & 0xFFu);
    bytes[offset + 2] = uint8_t((value >> 16) & 0xFFu);
    bytes[offset + 3] = uint8_t((value >> 24) & 0xFFu);
}

void writeU16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value) {
    if (offset + 2 > bytes.size()) return;
    bytes[offset + 0] = uint8_t(value & 0xFFu);
    bytes[offset + 1] = uint8_t((value >> 8) & 0xFFu);
}

bool isLeedsMdl(const std::vector<uint8_t>& bytes) {
    return bytes.size() >= 0x20u && readU32(bytes, 0u) == 0x006D646Cu;
}

uint64_t leedsLogicalSize(const std::vector<uint8_t>& bytes) {
    if (!isLeedsMdl(bytes)) return bytes.size();
    const uint32_t declaredSize = readU32(bytes, 0x08u);
    if (declaredSize >= 0x20u && declaredSize <= bytes.size()) return declaredSize;
    return bytes.size();
}

struct LeedsRelocationLayout {
    uint32_t localTableOffset = 0;
    uint32_t globalTableOffset = 0;
    uint32_t headerCount = 0;
    uint32_t entryCount = 0;
    uint64_t entriesOffset = 0;
    uint64_t entriesEnd = 0;
    bool prefixedByLocalTablePointer = false;
};

LeedsRelocationLayout leedsRelocationLayout(const std::vector<uint8_t>& bytes) {
    LeedsRelocationLayout layout;
    if (!isLeedsMdl(bytes) || bytes.size() < 0x18u) return layout;

    layout.localTableOffset = readU32(bytes, 0x0Cu);
    layout.globalTableOffset = readU32(bytes, 0x10u);
    layout.headerCount = readU32(bytes, 0x14u);

    const uint64_t logicalSize = leedsLogicalSize(bytes);
    if (layout.globalTableOffset < 0x20u || layout.globalTableOffset >= logicalSize) return layout;

    // Leeds MDLs use two table placements:
    //
    //  1) local/global table offsets are the same. Relocation field offsets
    //     begin exactly at globalTable.
    //
    //  2) local/global table offsets are split by four bytes. globalTable[0]
    //     points back to the local table, and relocation field offsets begin
    //     at globalTable + 4.
    //
    // Retail PS2 PED/Clump MDLs use the split-table form. In that layout
    // headerCount counts DWORDs beginning at globalTableOffset:
    //
    //   [0] local-table self pointer
    //   [1..N] relocation field offsets
    //
    // Therefore the number of actual pointer-field rows is headerCount - 1.
    // This is confirmed by retail IMG models: the DWORD immediately after the
    // declared split table is string/data, not another relocation entry.
    if (layout.localTableOffset != layout.globalTableOffset &&
        uint64_t(layout.globalTableOffset) + 4ull <= logicalSize &&
        readU32(bytes, layout.globalTableOffset) == layout.localTableOffset) {
        layout.prefixedByLocalTablePointer = true;
        layout.entriesOffset = uint64_t(layout.globalTableOffset) + 4ull;
        layout.entryCount = layout.headerCount > 0u ? layout.headerCount - 1u : 0u;
    } else {
        layout.entriesOffset = layout.globalTableOffset;
        layout.entryCount = layout.headerCount;
    }

    layout.entriesEnd = layout.entriesOffset + uint64_t(layout.entryCount) * 4ull;
    return layout;
}

bool isExpectedPedStructuralRelocation(const std::vector<uint8_t>& bytes,
                                      uint64_t logicalSize,
                                      uint32_t candidate) {
    if (logicalSize < 0x30u || candidate + 4ull > logicalSize) return false;

    const uint32_t topLevel = readU32(bytes, 0x24u);
    if (topLevel < 0x20u || uint64_t(topLevel) + 0x10ull > logicalSize) return false;
    if (readU32(bytes, topLevel) != 0x0000AA02u) return false;

    const uint32_t clumpCycle = readU32(bytes, size_t(topLevel) + 0x08u);
    if (clumpCycle < 0x1Cu) return false;
    const uint32_t atomic = clumpCycle - 0x1Cu;
    if (uint64_t(atomic) + 0x30ull > logicalSize) return false;
    if (readU32(bytes, atomic) != 0x0004AA01u) return false;

    const uint32_t hierarchy = readU32(bytes, size_t(atomic) + 0x2Cu);

    const uint32_t fixedFields[] = {
        0x20u, 0x24u, 0x28u, 0x2Cu, 0x30u,
        topLevel + 0x04u, topLevel + 0x08u, topLevel + 0x0Cu,
        atomic + 0x04u, atomic + 0x08u, atomic + 0x0Cu,
        atomic + 0x14u, atomic + 0x18u, atomic + 0x1Cu,
        atomic + 0x20u, atomic + 0x2Cu,
    };
    for (uint32_t field : fixedFields)
        if (candidate == field) return true;

    if (hierarchy >= 0x20u && uint64_t(hierarchy) + 0x38ull <= logicalSize) {
        if (candidate == hierarchy + 0x30u || candidate == hierarchy + 0x34u)
            return true;
    }
    return false;
}

bool plausibleTrailingRelocation(const std::vector<uint8_t>& bytes, uint64_t logicalSize,
                                  uint32_t tableOffset, uint32_t candidate,
                                  const std::set<uint32_t>& known) {
    if ((candidate & 3u) != 0u || candidate < 0x20u) return false;
    if (uint64_t(candidate) + 4ull > logicalSize || candidate >= tableOffset) return false;
    if (known.find(candidate) != known.end()) return false;

    const uint32_t target = readU32(bytes, candidate);
    if (target == 0u || target == 0xFFFFFFFFu || target < 0x20u || uint64_t(target) >= logicalSize)
        return false;

    const uint32_t neighbours[] = {
        candidate >= 4u ? candidate - 4u : 0u,
        candidate >= 8u ? candidate - 8u : 0u,
        candidate + 4u, candidate + 8u
    };
    if (isExpectedPedStructuralRelocation(bytes, logicalSize, candidate))
        return true;

    for (uint32_t neighbour : neighbours) {
        if (known.find(neighbour) != known.end()) return true;
    }
    return false;
}

std::vector<uint32_t> trailingRelocationsBeyondDeclaredCount(const std::vector<uint8_t>& bytes) {
    std::vector<uint32_t> recovered;
    if (!isLeedsMdl(bytes)) return recovered;

    const uint64_t logicalSize = leedsLogicalSize(bytes);
    const LeedsRelocationLayout layout = leedsRelocationLayout(bytes);
    if (layout.headerCount > 1000000u || layout.globalTableOffset < 0x20u) return recovered;
    if (layout.entriesEnd > logicalSize || layout.entriesEnd > bytes.size()) return recovered;

    std::set<uint32_t> known;
    for (uint32_t i = 0; i < layout.entryCount; ++i)
        known.insert(readU32(bytes, size_t(layout.entriesOffset) + size_t(i) * 4u));

    for (uint32_t extra = 0; extra < 32u; ++extra) {
        const uint64_t rowOffset64 = layout.entriesEnd + uint64_t(extra) * 4ull;
        if (rowOffset64 + 4ull > logicalSize || rowOffset64 + 4ull > bytes.size()) break;
        const uint32_t candidate = readU32(bytes, size_t(rowOffset64));
        if (!plausibleTrailingRelocation(bytes, logicalSize, layout.globalTableOffset, candidate, known)) break;
        recovered.push_back(candidate);
        known.insert(candidate);
    }
    return recovered;
}

std::string hexOffset(size_t value) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

void addIssue(StorylandModelIntegrityReport& report, StorylandModelIssueSeverity severity,
              size_t offset, const std::string& message) {
    report.issues.push_back({severity, offset, message});
    if (severity == StorylandModelIssueSeverity::Fatal) report.fatals++;
    else report.warnings++;
}

bool isSkinnedKind(StorylandModelKind kind) {
    return kind == StorylandModelKind::PedModel || kind == StorylandModelKind::CutsceneModel;
}

void validatePs2PedRuntimePartMaterialLayout(StorylandModelIntegrityReport& report,
                                             const StorylandModelFile& model,
                                             const std::vector<uint8_t>& bytes) {
    if (!isLeedsMdl(bytes) || !isSkinnedKind(model.modelKind())) return;

    const uint64_t logicalSize = leedsLogicalSize(bytes);
    if (logicalSize < 0x60u) return;

    const uint32_t clumpOffset = readU32(bytes, 0x24u);
    if (clumpOffset < 0x20u || uint64_t(clumpOffset) + 0x0Cull > logicalSize) return;

    const uint32_t clumpAtomicCycle = readU32(bytes, size_t(clumpOffset) + 0x08u);
    if (clumpAtomicCycle < 0x1Cu) return;

    const uint32_t atomicOffset = clumpAtomicCycle - 0x1Cu;
    if (uint64_t(atomicOffset) + 0x30ull > logicalSize) return;
    if (readU32(bytes, atomicOffset) != 0x0004AA01u) return;

    const uint32_t geometryOffset = readU32(bytes, size_t(atomicOffset) + 0x14u);
    if (geometryOffset < 0x20u || uint64_t(geometryOffset) + 0x60ull > logicalSize) return;
    if (readU32(bytes, geometryOffset) != 0x00000008u) return;

    const uint32_t materialTableOffset = readU32(bytes, size_t(geometryOffset) + 0x0Cu);
    const uint32_t materialDescriptorCount = readU32(bytes, size_t(geometryOffset) + 0x10u);
    const uint32_t packedGeometry = readU32(bytes, size_t(geometryOffset) + 0x30u);
    const uint32_t runtimePartCount = (packedGeometry >> 20u) & 0xFFFu;

    if (runtimePartCount == 0u || runtimePartCount > 512u) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, size_t(geometryOffset) + 0x30u,
                 "PS2 PED geometry has an invalid runtime part count (" +
                 std::to_string(runtimePartCount) + ").");
        return;
    }

    if (materialDescriptorCount == 0u || materialDescriptorCount > 512u) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, size_t(geometryOffset) + 0x10u,
                 "PS2 PED geometry has an invalid material descriptor count (" +
                 std::to_string(materialDescriptorCount) + ").");
        return;
    }

    // Runtime geometry parts and material descriptor slots are independent.
    // Retail Leeds PEDs routinely reuse one descriptor from several runtime parts,
    // so runtimePartCount may be greater than materialDescriptorCount. What matters
    // is that every part's tex_id resolves to an existing descriptor.

    if (materialTableOffset < 0x20u ||
        uint64_t(materialTableOffset) + uint64_t(materialDescriptorCount) * 4ull > logicalSize) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, size_t(geometryOffset) + 0x0Cu,
                 "PS2 PED material pointer table extends outside the declared MDL data.");
        return;
    }

    const uint64_t partTableOffset = uint64_t(geometryOffset) + 0x60ull;
    const uint64_t partTableEnd = partTableOffset + uint64_t(runtimePartCount) * 0x30ull;
    if (partTableEnd > logicalSize) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, size_t(geometryOffset) + 0x60u,
                 "PS2 PED runtime part table extends outside the declared MDL data.");
        return;
    }

    bool identityPartMapping = materialDescriptorCount == runtimePartCount;
    for (uint32_t partIndex = 0u; partIndex < runtimePartCount; ++partIndex) {
        const size_t partOffset = size_t(partTableOffset + uint64_t(partIndex) * 0x30ull);
        const uint16_t textureSlot = uint16_t(bytes[partOffset + 0x22u]) |
                                     (uint16_t(bytes[partOffset + 0x23u]) << 8u);
        if (textureSlot >= materialDescriptorCount) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, partOffset + 0x22u,
                     "PS2 PED runtime part " + std::to_string(partIndex) +
                     " references material descriptor " + std::to_string(textureSlot) +
                     ", outside the " + std::to_string(materialDescriptorCount) +
                     " available descriptor slots.");
        }
        if (textureSlot != partIndex) identityPartMapping = false;
    }

    // A short-lived BLeeds 1.4.8 regression rewrote every runtime part to its own
    // material slot. When a split mesh reused the same source material this produced
    // byte-identical duplicate descriptor objects and tex_id = part_index for every
    // part. Retail VCS PEDs reuse the existing descriptor index instead. Detect that
    // exact synthetic layout so Test Model catches already-exported files.
    if (identityPartMapping && materialDescriptorCount > 1u) {
        bool hasByteIdenticalDuplicateDescriptor = false;
        for (uint32_t i = 0u; i < materialDescriptorCount && !hasByteIdenticalDuplicateDescriptor; ++i) {
            const uint32_t descriptorA = readU32(bytes, size_t(materialTableOffset) + size_t(i) * 4u);
            if (descriptorA < 0x20u || uint64_t(descriptorA) + 0x10ull > logicalSize) continue;
            for (uint32_t j = i + 1u; j < materialDescriptorCount; ++j) {
                const uint32_t descriptorB = readU32(bytes, size_t(materialTableOffset) + size_t(j) * 4u);
                if (descriptorB < 0x20u || uint64_t(descriptorB) + 0x10ull > logicalSize) continue;
                if (std::memcmp(bytes.data() + descriptorA, bytes.data() + descriptorB, 0x10u) == 0) {
                    hasByteIdenticalDuplicateDescriptor = true;
                    break;
                }
            }
        }
        if (hasByteIdenticalDuplicateDescriptor) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, size_t(geometryOffset) + 0x10u,
                     "PS2 PED contains a synthetic one-material-slot-per-part layout with byte-identical duplicate descriptors. "
                     "Retail Leeds PEDs reuse descriptor indices across parts; this matches the BLeeds 1.4.8 lockstep material regression and is unsafe for replacement until canonicalized.");
        }
    }
}

void validateLeedsRelocations(StorylandModelIntegrityReport& report,
                              const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 0x18u || readU32(bytes, 0u) != 0x006D646Cu) return;
    const uint32_t declaredSize = readU32(bytes, 0x08u);
    const LeedsRelocationLayout layout = leedsRelocationLayout(bytes);
    if (layout.headerCount == 0u) return;
    if (layout.headerCount > 1000000u) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0x14u, "Relocation entry count is outside the supported range.");
        return;
    }
    const uint64_t logicalSize = declaredSize >= 0x20u && declaredSize <= bytes.size() ? declaredSize : bytes.size();
    if (layout.globalTableOffset < 0x18u || layout.entriesEnd > logicalSize || layout.entriesEnd > bytes.size()) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0x10u, "Relocation table extends outside the declared MDL data.");
        return;
    }

    report.declaredRelocationFields = layout.entryCount;
    report.declaredRelocationTableDwords = layout.headerCount;
    report.relocationPrefixDwords = layout.prefixedByLocalTablePointer ? 1u : 0u;
    report.relocationFields += layout.entryCount;
    const std::vector<uint32_t> trailing = trailingRelocationsBeyondDeclaredCount(bytes);
    report.recoveredTrailingRelocations = trailing.size();
    if (!trailing.empty()) {
        addIssue(report, StorylandModelIssueSeverity::Warning, 0x14u,
                 "Relocation entry count stops before " + std::to_string(trailing.size()) +
                 " pointer-like field" + (trailing.size() == 1 ? std::string("") : std::string("s")) +
                 " that follow the declared split table. First trailing field is " + hexOffset(trailing.front()) +
                 ". Required runtime pointers are checked separately below.");
    }

    std::set<uint32_t> seenFields;
    for (uint32_t index = 0u; index < layout.entryCount; ++index) {
        const size_t tableEntryOffset = size_t(layout.entriesOffset) + size_t(index) * 4u;
        const uint32_t pointerFieldOffset = readU32(bytes, tableEntryOffset);
        if ((pointerFieldOffset & 3u) != 0u || uint64_t(pointerFieldOffset) + 4ull > logicalSize) {
            ++report.suspiciousPointers;
            addIssue(report, StorylandModelIssueSeverity::Fatal, tableEntryOffset,
                     "Relocation entry points outside the declared MDL data (" + hexOffset(pointerFieldOffset) + ").");
            continue;
        }
        if (!seenFields.insert(pointerFieldOffset).second) {
            addIssue(report, StorylandModelIssueSeverity::Warning, tableEntryOffset,
                     "Relocation table contains a duplicate pointer-field offset (" + hexOffset(pointerFieldOffset) + ").");
            continue;
        }

        const uint32_t value = readU32(bytes, pointerFieldOffset);
        if (value == 0u || value == 0xFFFFFFFFu) continue;
        if (uint64_t(value) < logicalSize) {
            ++report.fileLocalPointers;
            continue;
        }
        if (value < 0x02000000u || (value >= 0x70000000u && value < 0x70004000u)) {
            ++report.runtimePointers;
            addIssue(report, StorylandModelIssueSeverity::Warning, pointerFieldOffset,
                     "Relocation target is a PS2 runtime address and cannot be resolved from the standalone file (" + hexOffset(value) + ").");
            continue;
        }
        ++report.suspiciousPointers;
        addIssue(report, StorylandModelIssueSeverity::Fatal, pointerFieldOffset,
                 "Relocation target is outside the MDL and normal PS2 address ranges (" + hexOffset(value) + ").");
    }
}


bool isTrustedPointerField(const StorylandModelField& field) {
    static const std::set<std::string> names = {
        "collision_model_ptr", "element_group_ptr",
        "root_frame_ptr", "clump_atomic_cycle_next", "clump_atomic_cycle_prev",
        "parent_frame_ptr", "frame_atomic_cycle_next", "frame_atomic_cycle_prev",
        "geometry_ptr", "clump_ptr", "hierarchy_ptr",
        "material_list_ptr", "entries_ptr", "anchor_frame_ptr",
        "texture_name_ptr", "specular_or_matfx_ptr"
    };
    if (names.find(field.name) == names.end()) return false;
    if (field.group.rfind("PedData @", 0) == 0) return true;
    if (field.group.rfind("Clump #0 @", 0) == 0) return true;
    if (field.group.rfind("Atomic #0 @", 0) == 0) return true;
    if (field.group.rfind("RslGeometry @", 0) == 0) return true;
    if (field.group.rfind("RslMaterialList @", 0) == 0) return true;
    if (field.group.rfind("RslMaterial #", 0) == 0) return true;
    if (field.group.rfind("RslTAnim / HAnim hierarchy @", 0) == 0) return true;
    return false;
}

void validateRequiredLeedsRelocations(StorylandModelIntegrityReport& report,
                                      const StorylandModelFile& model,
                                      const std::vector<uint8_t>& bytes) {
    if (!isLeedsMdl(bytes)) return;
    const uint64_t logicalSize = leedsLogicalSize(bytes);
    const LeedsRelocationLayout layout = leedsRelocationLayout(bytes);
    if (layout.headerCount > 1000000u || layout.globalTableOffset < 0x20u ||
        layout.entriesEnd > logicalSize) return;

    std::set<uint32_t> declared;
    for (uint32_t i = 0; i < layout.entryCount; ++i)
        declared.insert(readU32(bytes, size_t(layout.entriesOffset) + size_t(i) * 4u));
    const std::vector<uint32_t> trailing = trailingRelocationsBeyondDeclaredCount(bytes);
    const std::set<uint32_t> trailingSet(trailing.begin(), trailing.end());

    std::set<uint32_t> required;
    std::set<uint32_t> nonCriticalTrailing;
    for (const StorylandModelField& field : model.fields()) {
        if (!isTrustedPointerField(field)) continue;
        if ((field.offset & 3u) != 0u || uint64_t(field.offset) + 4ull > logicalSize) continue;
        const uint32_t value = readU32(bytes, field.offset);
        if (value == 0u || value == 0xFFFFFFFFu || value < 0x20u || uint64_t(value) >= logicalSize) continue;
        required.insert(field.offset);

        // Some older working exports left only the hierarchy anchor pointer
        // immediately after the declared table. That field is useful metadata,
        // but unlike Atomic.hierarchy_ptr it is not needed to enter the HAnim
        // hierarchy in the normal PED load path. Keep it visible as a warning
        // rather than treating a known-working file as an unsafe replacement.
        if (field.name == "anchor_frame_ptr")
            nonCriticalTrailing.insert(field.offset);
    }

    // Frame links are especially important because one missing relocation can turn
    // hierarchy traversal into a stream of TLB misses. Validate only the frames
    // that the model parser actually accepted into the armature.
    for (const StorylandModelBone& bone : model.armatureBones()) {
        const uint32_t base = bone.offset;
        const uint32_t pointerOffsets[] = {
            base + 0x04u, base + 0x08u, base + 0x0Cu,
            base + 0x90u, base + 0x94u, base + 0x98u,
            base + 0xA4u, base + 0xA8u
        };
        for (uint32_t fieldOffset : pointerOffsets) {
            if ((fieldOffset & 3u) != 0u || uint64_t(fieldOffset) + 4ull > logicalSize) continue;
            const uint32_t value = readU32(bytes, fieldOffset);
            if (value == 0u || value == 0xFFFFFFFFu || value < 0x20u || uint64_t(value) >= logicalSize) continue;
            required.insert(fieldOffset);
        }
    }

    for (uint32_t fieldOffset : required) {
        if (declared.find(fieldOffset) != declared.end()) continue;
        ++report.missingRequiredRelocations;

        const bool isTrailing = trailingSet.find(fieldOffset) != trailingSet.end();
        const bool allowTrailingAnchor = isTrailing &&
            nonCriticalTrailing.find(fieldOffset) != nonCriticalTrailing.end();

        if (allowTrailingAnchor) {
            addIssue(report, StorylandModelIssueSeverity::Warning, fieldOffset,
                     "Hierarchy anchor pointer follows the declared relocation table and will not be relocated. "
                     "This layout is tolerated for compatibility, but new exports should declare it.");
            continue;
        }

        addIssue(report, StorylandModelIssueSeverity::Fatal, fieldOffset,
                 isTrailing
                     ? "Required runtime pointer is present after the declared relocation table and will not be relocated by the game."
                     : "Required file-relative pointer field is missing from the relocation table.");
    }
}

void validateContainerHeader(StorylandModelIntegrityReport& report,
                             const StorylandModelFile& model,
                             const std::vector<uint8_t>& bytes) {
    if (model.isPmlcMdl()) {
        if (bytes.size() < 0x28u || std::memcmp(bytes.data(), "PMLC", 4u) != 0) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 0, "Missing PMLC model signature.");
            return;
        }
        const uint32_t declaredSize = readU32(bytes, 0x08u);
        const uint32_t firstEntry = readU32(bytes, 0x20u);
        if (declaredSize != 0u && declaredSize > bytes.size()) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 0x08u,
                     "PMLC declared file size extends beyond the available bytes.");
        }
        if (firstEntry == 0u || firstEntry > bytes.size() || bytes.size() - firstEntry < 16u) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 0x20u,
                     "PMLC first-entry pointer is outside the file.");
            return;
        }
        const uint32_t entryData = readU32(bytes, size_t(firstEntry) + 0x08u);
        if (entryData == 0u || entryData > bytes.size() || bytes.size() - entryData < 0x1Cu) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, size_t(firstEntry) + 0x08u,
                     "PMLC entry-data pointer is outside the file.");
            return;
        }
        const uint32_t pointers[] = {
            readU32(bytes, size_t(entryData) + 0x00u),
            readU32(bytes, size_t(entryData) + 0x10u),
            readU32(bytes, size_t(entryData) + 0x14u)
        };
        const size_t pointerOffsets[] = {size_t(entryData), size_t(entryData) + 0x10u, size_t(entryData) + 0x14u};
        for (size_t i = 0; i < 3u; ++i) {
            if (pointers[i] != 0u && pointers[i] >= bytes.size()) {
                addIssue(report, StorylandModelIssueSeverity::Fatal, pointerOffsets[i],
                         "PMLC hierarchy/object pointer is outside the file.");
            }
        }
        return;
    }

    if (model.isMobileLcsDff() || model.isPspNativeDff()) {
        if (bytes.size() < 12u) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 0, "RenderWare DFF header is truncated.");
            return;
        }
        const uint32_t type = readU32(bytes, 0u);
        const uint32_t payloadSize = readU32(bytes, 4u);
        if (type != 0x10u) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 0, "DFF does not begin with a RenderWare Clump chunk.");
        }
        if (uint64_t(payloadSize) + 12ull > uint64_t(bytes.size())) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 4u, "Top-level RenderWare chunk extends beyond the file.");
        }
        return;
    }

    if (bytes.size() < 0x20u || readU32(bytes, 0) != 0x006D646Cu) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 "Missing Leeds MDL chunk identifier 0x006D646C.");
        return;
    }
    const uint32_t declaredSize = readU32(bytes, 0x08u);
    if (declaredSize < 0x20u || declaredSize > bytes.size()) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0x08u,
                 "Declared MDL chunk size extends beyond the available file bytes.");
    } else if (bytes.size() - declaredSize >= 2048u) {
        addIssue(report, StorylandModelIssueSeverity::Warning, 0x08u,
                 "More than one IMG sector follows the declared MDL chunk; streaming allocation or padding should be checked.");
    }
}

} // namespace

std::string StorylandModelIntegrityReport::text() const {
    std::ostringstream out;
    out << "Model diagnostics\r\n\r\n"
        << "Target: " << label << "\r\n"
        << "Bytes: " << byteSize << "\r\n"
        << "Vertices: " << vertices << "\r\n"
        << "Triangles: " << triangles << "\r\n"
        << "Bones: " << bones << "\r\n"
        << "Weighted vertices: " << weightedVertices << "/" << vertices << "\r\n"
        << "Relocation pointer fields: " << relocationFields
        << " (declared pointer fields " << declaredRelocationFields
        << ", header numEntries " << declaredRelocationTableDwords
        << ", prefix DWORDs " << relocationPrefixDwords
        << ", trailing omitted " << recoveredTrailingRelocations
        << ", required missing " << missingRequiredRelocations
        << ", file-local " << fileLocalPointers
        << ", runtime " << runtimePointers
        << ", suspicious " << suspiciousPointers << ")\r\n"
        << "Warnings: " << warnings << "\r\n"
        << "Errors: " << fatals << "\r\n\r\n"
        << (safe() ? "RESULT: PASS" : "RESULT: FAIL") << "\r\n";
    if (!issues.empty()) out << "\r\nDiagnostics\r\n";
    for (const StorylandModelIntegrityIssue& issue : issues) {
        out << (issue.severity == StorylandModelIssueSeverity::Fatal ? "ERROR" : "WARNING")
            << " @ file+" << hexOffset(issue.offset) << " | " << issue.message << "\r\n";
    }
    return out.str();
}

StorylandModelIntegrityReport storylandValidateModelIntegrity(
    const StorylandModelFile& model,
    const std::vector<uint8_t>& bytes,
    const std::string& label
) {
    StorylandModelIntegrityReport report;
    report.label = label;
    report.byteSize = bytes.size();
    report.vertices = model.previewPoints().size();
    report.triangles = model.previewTriangles().size();
    report.bones = model.armatureBones().size();

    validateContainerHeader(report, model, bytes);

    if (report.vertices == 0 || report.triangles == 0) {
        if (model.isMobileLcsDff() || model.isPspNativeDff()) {
            addIssue(report, StorylandModelIssueSeverity::Warning, 0,
                     "No complete renderable geometry was decoded. Frame-only DFF clumps are valid; otherwise the geometry is missing or unsupported.");
        } else {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                     "No complete renderable geometry was decoded.");
        }
    }

    size_t invalidTriangles = 0;
    size_t degenerateTriangles = 0;
    for (const StorylandModelTriangle& triangle : model.previewTriangles()) {
        if (triangle.a >= report.vertices || triangle.b >= report.vertices || triangle.c >= report.vertices) {
            invalidTriangles++;
        } else if (triangle.a == triangle.b || triangle.a == triangle.c || triangle.b == triangle.c) {
            degenerateTriangles++;
        }
    }
    report.degenerateTriangles = degenerateTriangles;
    if (invalidTriangles != 0) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 std::to_string(invalidTriangles) + " triangles reference vertices outside the vertex array.");
    }
    if (degenerateTriangles != 0) {
        addIssue(report, StorylandModelIssueSeverity::Warning, 0,
                 std::to_string(degenerateTriangles) + " degenerate triangles were decoded.");
    }

    size_t nonFinitePoints = 0;
    float maxAbsCoordinate = 0.0f;
    for (const StorylandModelPoint& point : model.previewPoints()) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
            nonFinitePoints++;
            continue;
        }
        maxAbsCoordinate = std::max(maxAbsCoordinate, std::max({std::fabs(point.x), std::fabs(point.y), std::fabs(point.z)}));
    }
    if (nonFinitePoints != 0) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 std::to_string(nonFinitePoints) + " vertices contain non-finite coordinates.");
    }
    if (maxAbsCoordinate > 1000000.0f) {
        addIssue(report, StorylandModelIssueSeverity::Warning, 0,
                 "Geometry contains extremely large coordinates; check relocation data and vertex scale.");
    }

    size_t badUvs = 0;
    size_t extremeUvs = 0;
    for (const StorylandModelTexcoord& uv : model.previewTexcoords()) {
        if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) badUvs++;
        else if (std::fabs(uv.u) > 64.0f || std::fabs(uv.v) > 64.0f) extremeUvs++;
    }
    if (badUvs != 0u) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 std::to_string(badUvs) + " texture coordinates are non-finite.");
    } else if (extremeUvs != 0u) {
        addIssue(report, StorylandModelIssueSeverity::Warning, 0,
                 std::to_string(extremeUvs) + " texture coordinates are unusually far outside the normal tiled range.");
    }

    validateLeedsRelocations(report, bytes);
    validateRequiredLeedsRelocations(report, model, bytes);
    validatePs2PedRuntimePartMaterialLayout(report, model, bytes);

    const auto& bones = model.armatureBones();
    for (const StorylandModelBone& bone : bones) {
        if (bone.parentIndex == 0xFFFFFFFFu) {
            report.rootBones++;
        } else if (bone.parentIndex >= bones.size()) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, bone.offset,
                     "Bone '" + bone.name + "' has an out-of-range parent index.");
        } else if (bone.parentIndex == bone.index) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, bone.offset,
                     "Bone '" + bone.name + "' is parented to itself.");
        }
        if ((bone.hasWorldPosition && (!std::isfinite(bone.worldPosition.x) || !std::isfinite(bone.worldPosition.y) || !std::isfinite(bone.worldPosition.z))) ||
            (bone.hasPreviewPosition && (!std::isfinite(bone.previewPosition.x) || !std::isfinite(bone.previewPosition.y) || !std::isfinite(bone.previewPosition.z)))) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, bone.offset,
                     "Bone '" + bone.name + "' has a non-finite transform.");
        }
    }

    if (!bones.empty()) {
        std::set<uint32_t> cycleMembers;
        for (uint32_t start = 0; start < bones.size(); ++start) {
            std::set<uint32_t> visited;
            uint32_t cursor = start;
            while (cursor < bones.size() && bones[cursor].parentIndex != 0xFFFFFFFFu) {
                if (!visited.insert(cursor).second) {
                    cycleMembers.insert(cursor);
                    break;
                }
                cursor = bones[cursor].parentIndex;
            }
        }
        if (!cycleMembers.empty()) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, bones[*cycleMembers.begin()].offset,
                     "Skeleton parent links contain a cycle.");
        }
    }

    if (!isSkinnedKind(model.modelKind())) return report;

    if (bones.empty()) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 "Skinned model has no decoded skeleton.");
    } else if (!model.isPmlcMdl() && report.rootBones != 1u) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 "Skinned hierarchy should have one root; " + std::to_string(report.rootBones) + " were decoded.");
    } else if (model.isPmlcMdl() && report.rootBones == 0u) {
        addIssue(report, StorylandModelIssueSeverity::Warning, 0,
                 "PMLC skeleton has no resolved root after pointer traversal.");
    }

    const auto& skin = model.previewSkinWeights();
    if (!model.isPmlcMdl() && skin.size() != report.vertices) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 "Skin row count does not match the vertex count (" +
                 std::to_string(skin.size()) + " vs " + std::to_string(report.vertices) + ").");
    }

    size_t invalidBoneInfluences = 0;
    size_t badWeightSums = 0;
    const size_t rows = std::min(skin.size(), report.vertices);
    for (size_t vertex = 0; vertex < rows; ++vertex) {
        const StorylandModelSkinWeights& weights = skin[vertex];
        bool weighted = false;
        float sum = 0.0f;
        const uint32_t count = std::min<uint32_t>(weights.influenceCount, 4u);
        for (uint32_t influence = 0; influence < count; ++influence) {
            const StorylandModelSkinInfluence& item = weights.influences[influence];
            if (!std::isfinite(item.weight) || item.weight <= 0.00001f) continue;
            weighted = true;
            sum += item.weight;
            report.skinInfluences++;
            if (item.boneIndex >= bones.size()) invalidBoneInfluences++;
        }
        if (weighted) {
            report.weightedVertices++;
            if (sum < 0.90f || sum > 1.10f) badWeightSums++;
        }
    }

    if (invalidBoneInfluences != 0) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 std::to_string(invalidBoneInfluences) + " skin influences reference missing bones.");
    }
    if (badWeightSums != 0) {
        addIssue(report, StorylandModelIssueSeverity::Warning, 0,
                 std::to_string(badWeightSums) + " vertices have skin weights that do not sum close to 1.0.");
    }

    if (!model.isPmlcMdl() && report.vertices != 0) {
        const double coverage = double(report.weightedVertices) / double(report.vertices);
        if (coverage < 0.95) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                     "Only " + std::to_string(report.weightedVertices) + " of " +
                     std::to_string(report.vertices) + " vertices have usable skin weights.");
        } else if (report.weightedVertices != report.vertices) {
            addIssue(report, StorylandModelIssueSeverity::Warning, 0,
                     std::to_string(report.vertices - report.weightedVertices) +
                     " vertices have no usable skin influence.");
        }
    } else if (model.isPmlcMdl() && report.weightedVertices == 0u) {
        addIssue(report, StorylandModelIssueSeverity::Warning, 0,
                 "PMLC mesh has no decoded per-vertex skin weights; rigid parent-bone transforms are being used for preview geometry.");
    }

    return report;
}


bool storylandRepairLeedsPedDuplicateMaterialSlots(
    std::vector<uint8_t>& bytes,
    uint32_t& removedDescriptorSlots,
    std::string& detail
) {
    removedDescriptorSlots = 0u;
    detail.clear();
    if (!isLeedsMdl(bytes)) return true;

    const uint64_t logicalSize = leedsLogicalSize(bytes);
    if (logicalSize < 0x60u) return true;

    const uint32_t clumpOffset = readU32(bytes, 0x24u);
    if (clumpOffset < 0x20u || uint64_t(clumpOffset) + 0x0Cull > logicalSize ||
        readU32(bytes, clumpOffset) != 0x0000AA02u) {
        return true;
    }

    const uint32_t clumpAtomicCycle = readU32(bytes, size_t(clumpOffset) + 0x08u);
    if (clumpAtomicCycle < 0x1Cu) return true;
    const uint32_t atomicOffset = clumpAtomicCycle - 0x1Cu;
    if (uint64_t(atomicOffset) + 0x30ull > logicalSize ||
        readU32(bytes, atomicOffset) != 0x0004AA01u) {
        return true;
    }

    const uint32_t hierarchyOffset = readU32(bytes, size_t(atomicOffset) + 0x2Cu);
    if (hierarchyOffset < 0x20u || uint64_t(hierarchyOffset) + 8ull > logicalSize ||
        readU32(bytes, hierarchyOffset) != 0x00003000u) {
        return true;
    }

    const uint32_t geometryOffset = readU32(bytes, size_t(atomicOffset) + 0x14u);
    if (geometryOffset < 0x20u || uint64_t(geometryOffset) + 0x60ull > logicalSize ||
        readU32(bytes, geometryOffset) != 0x00000008u) {
        return true;
    }

    const uint32_t materialTableOffset = readU32(bytes, size_t(geometryOffset) + 0x0Cu);
    const uint32_t materialCount = readU32(bytes, size_t(geometryOffset) + 0x10u);
    const uint32_t packedGeometry = readU32(bytes, size_t(geometryOffset) + 0x30u);
    const uint32_t partCount = (packedGeometry >> 20u) & 0xFFFu;
    if (materialCount < 2u || materialCount > 512u || partCount != materialCount) return true;
    if (materialTableOffset < 0x20u ||
        uint64_t(materialTableOffset) + uint64_t(materialCount) * 4ull > logicalSize) {
        detail = "PED material descriptor table is outside the declared MDL data.";
        return false;
    }

    const uint64_t partTableOffset = uint64_t(geometryOffset) + 0x60ull;
    if (partTableOffset + uint64_t(partCount) * 0x30ull > logicalSize) {
        detail = "PED runtime part table is outside the declared MDL data.";
        return false;
    }

    std::vector<uint32_t> descriptorPointers(materialCount, 0u);
    for (uint32_t i = 0u; i < materialCount; ++i) {
        descriptorPointers[i] = readU32(bytes, size_t(materialTableOffset) + size_t(i) * 4u);
        if (descriptorPointers[i] < 0x20u || uint64_t(descriptorPointers[i]) + 0x10ull > logicalSize) {
            return true;
        }
        const size_t partOffset = size_t(partTableOffset + uint64_t(i) * 0x30ull);
        const uint16_t textureSlot = uint16_t(bytes[partOffset + 0x22u]) |
                                     (uint16_t(bytes[partOffset + 0x23u]) << 8u);
        if (textureSlot != i) return true;
    }

    std::vector<uint32_t> uniquePointers;
    std::vector<uint16_t> oldToNew(materialCount, 0u);
    uniquePointers.reserve(materialCount);

    for (uint32_t oldIndex = 0u; oldIndex < materialCount; ++oldIndex) {
        const uint32_t descriptor = descriptorPointers[oldIndex];
        uint32_t matchIndex = uint32_t(uniquePointers.size());
        for (uint32_t uniqueIndex = 0u; uniqueIndex < uniquePointers.size(); ++uniqueIndex) {
            const uint32_t existing = uniquePointers[uniqueIndex];
            if (std::memcmp(bytes.data() + descriptor, bytes.data() + existing, 0x10u) == 0) {
                matchIndex = uniqueIndex;
                break;
            }
        }
        if (matchIndex == uniquePointers.size()) uniquePointers.push_back(descriptor);
        oldToNew[oldIndex] = uint16_t(matchIndex);
    }

    if (uniquePointers.size() == descriptorPointers.size()) return true;
    if (uniquePointers.empty() || uniquePointers.size() > 0xFFFFu) {
        detail = "PED duplicate material slots could not be canonicalized safely.";
        return false;
    }

    for (size_t i = 0; i < uniquePointers.size(); ++i)
        writeU32(bytes, size_t(materialTableOffset) + i * 4u, uniquePointers[i]);
    writeU32(bytes, size_t(geometryOffset) + 0x10u, uint32_t(uniquePointers.size()));

    for (uint32_t partIndex = 0u; partIndex < partCount; ++partIndex) {
        const size_t partOffset = size_t(partTableOffset + uint64_t(partIndex) * 0x30ull);
        writeU16(bytes, partOffset + 0x22u, oldToNew[partIndex]);
    }

    removedDescriptorSlots = uint32_t(descriptorPointers.size() - uniquePointers.size());
    std::ostringstream out;
    out << "Canonicalized PS2 PED material mapping: " << descriptorPointers.size()
        << " synthetic one-per-part descriptor slots -> " << uniquePointers.size()
        << " unique descriptor slots; runtime part tex_id values were remapped to reuse byte-identical descriptors. "
        << "No geometry, DMA payload, texture names, or file offsets were moved.";
    detail = out.str();
    return true;
}



bool storylandRepairVcsRetailPlayerHAnimIds(
    std::vector<uint8_t>& bytes,
    uint32_t& repairedHierarchyRows,
    uint32_t& repairedFrameIds,
    std::string& detail
) {
    repairedHierarchyRows = 0u;
    repairedFrameIds = 0u;
    detail.clear();

    if (!isLeedsMdl(bytes)) {
        detail = "The selected VCS plr.mdl replacement is not a Leeds MDL container.";
        return false;
    }

    const uint64_t logicalSize = leedsLogicalSize(bytes);
    if (logicalSize < 0x200u) {
        detail = "The selected VCS plr.mdl replacement is too small to contain the retail PED hierarchy.";
        return false;
    }

    const uint32_t clumpOffset = readU32(bytes, 0x24u);
    if (clumpOffset < 0x20u || uint64_t(clumpOffset) + 0x10ull > logicalSize ||
        readU32(bytes, clumpOffset) != 0x0000AA02u) {
        detail = "The selected VCS plr.mdl replacement does not contain the expected Leeds Clump header.";
        return false;
    }

    const uint32_t clumpAtomicCycle = readU32(bytes, size_t(clumpOffset) + 0x08u);
    if (clumpAtomicCycle < 0x1Cu) {
        detail = "The selected VCS plr.mdl replacement has an invalid Atomic cycle pointer.";
        return false;
    }
    const uint32_t atomicOffset = clumpAtomicCycle - 0x1Cu;
    if (uint64_t(atomicOffset) + 0x30ull > logicalSize ||
        readU32(bytes, atomicOffset) != 0x0004AA01u) {
        detail = "The selected VCS plr.mdl replacement does not contain the expected Leeds Atomic.";
        return false;
    }

    const uint32_t hierarchyOffset = readU32(bytes, size_t(atomicOffset) + 0x2Cu);
    if (hierarchyOffset < 0x20u || uint64_t(hierarchyOffset) + 0x38ull > logicalSize ||
        readU32(bytes, hierarchyOffset) != 0x00003000u) {
        detail = "The selected VCS plr.mdl replacement does not contain the expected RslTAnim/HAnim hierarchy.";
        return false;
    }

    const uint32_t hierarchyCount = readU32(bytes, size_t(hierarchyOffset) + 0x04u);
    const uint32_t entriesOffset = readU32(bytes, size_t(hierarchyOffset) + 0x30u);
    const uint32_t anchorFrame = readU32(bytes, size_t(hierarchyOffset) + 0x34u);
    if (hierarchyCount != 25u) {
        detail = "The base VCS plr.mdl must contain the retail 25-node PED hierarchy.";
        return false;
    }
    if (entriesOffset < 0x20u || uint64_t(entriesOffset) + 25ull * 8ull > logicalSize) {
        detail = "The VCS player HAnim entry table is outside the declared MDL data.";
        return false;
    }
    if (anchorFrame < 0x20u || uint64_t(anchorFrame) + 0xA0ull > logicalSize) {
        detail = "The VCS player hierarchy anchor frame is invalid.";
        return false;
    }

    static constexpr uint8_t kRetailIds[25] = {
        0, 1, 2, 3, 4, 5, 6,
        31, 32, 33, 34, 35,
        21, 22, 23, 24, 25,
        41, 42, 43, 255,
        51, 52, 53, 255
    };
    static constexpr uint8_t kRetailTypes[25] = {
        0, 0, 0, 2, 0, 2, 1,
        2, 0, 0, 0, 1,
        0, 0, 0, 0, 1,
        2, 0, 0, 1,
        0, 0, 0, 1
    };

    // The retail player hierarchy uses node_index 0..24. The specific broken
    // BLeeds export seen in the field used this permuted semantic-ID sequence.
    // Refuse to rewrite any other 25-node layout automatically so an LCS player
    // model (or a different custom skeleton) is never silently converted to VCS.
    static constexpr uint8_t kKnownBadBLeedsIds[25] = {
        0, 1, 2, 35, 21, 53, 255,
        42, 43, 255, 51, 52,
        22, 23, 24, 25, 41,
        31, 32, 33, 34,
        3, 4, 5, 6
    };

    std::set<uint32_t> seenNodeIndices;
    uint8_t currentIdsByNode[25] = {};
    uint8_t currentTypesByNode[25] = {};
    for (uint32_t row = 0u; row < 25u; ++row) {
        const size_t rowOffset = size_t(entriesOffset) + size_t(row) * 8u;
        const uint32_t packed = readU32(bytes, rowOffset);
        const uint32_t nodeIndex = (packed >> 8u) & 0xFFu;
        if (nodeIndex >= 25u || !seenNodeIndices.insert(nodeIndex).second) {
            detail = "The VCS player HAnim table does not contain a unique node_index 0..24 mapping.";
            return false;
        }
        currentIdsByNode[nodeIndex] = uint8_t(packed & 0xFFu);
        currentTypesByNode[nodeIndex] = uint8_t((packed >> 16u) & 0xFFu);
    }

    bool canonicalIds = true;
    bool knownBadIds = true;
    bool canonicalTypes = true;
    for (uint32_t i = 0u; i < 25u; ++i) {
        canonicalIds = canonicalIds && currentIdsByNode[i] == kRetailIds[i];
        knownBadIds = knownBadIds && currentIdsByNode[i] == kKnownBadBLeedsIds[i];
        canonicalTypes = canonicalTypes && currentTypesByNode[i] == kRetailTypes[i];
    }
    if (!canonicalIds && !knownBadIds) {
        detail = "The selected plr.mdl does not match either the retail VCS HAnim identity sequence or the known BLeeds permuted sequence; Storyland will not guess a bone-ID conversion.";
        return false;
    }

    // Patch the semantic HAnim ID/type by node_index rather than by file row so
    // a harmless row ordering difference cannot assign the wrong identity.
    for (uint32_t row = 0u; row < 25u; ++row) {
        const size_t rowOffset = size_t(entriesOffset) + size_t(row) * 8u;
        const uint32_t packed = readU32(bytes, rowOffset);
        const uint32_t nodeIndex = (packed >> 8u) & 0xFFu;
        const uint32_t canonical =
            (packed & 0xFF00FF00u) |
            uint32_t(kRetailIds[nodeIndex]) |
            (uint32_t(kRetailTypes[nodeIndex]) << 16u);
        if (canonical != packed) {
            writeU32(bytes, rowOffset, canonical);
            ++repairedHierarchyRows;
        }
    }

    // RslNode stores the same semantic ID at +0x9C. The VCS player frame tree
    // traverses in the same canonical node-index order as the HAnim table.
    std::vector<uint32_t> frames;
    std::set<uint32_t> seenFrames;
    std::function<bool(uint32_t)> walkFrame = [&](uint32_t frameOffset) -> bool {
        if (frameOffset == 0u) return true;
        if (frameOffset < 0x20u || uint64_t(frameOffset) + 0xA0ull > logicalSize) return false;
        if (!seenFrames.insert(frameOffset).second) return false;
        frames.push_back(frameOffset);
        if (frames.size() > 25u) return false;
        const uint32_t child = readU32(bytes, size_t(frameOffset) + 0x90u);
        const uint32_t sibling = readU32(bytes, size_t(frameOffset) + 0x94u);
        if (child != 0u && !walkFrame(child)) return false;
        if (sibling != 0u && !walkFrame(sibling)) return false;
        return true;
    };
    if (!walkFrame(anchorFrame) || frames.size() != 25u) {
        detail = "The VCS player frame tree is not the expected acyclic 25-node retail hierarchy.";
        return false;
    }

    for (size_t nodeIndex = 0u; nodeIndex < frames.size(); ++nodeIndex) {
        const size_t idOffset = size_t(frames[nodeIndex]) + 0x9Cu;
        const uint32_t current = readU32(bytes, idOffset);
        if (current != uint32_t(kRetailIds[nodeIndex])) {
            writeU32(bytes, idOffset, uint32_t(kRetailIds[nodeIndex]));
            ++repairedFrameIds;
        }
    }

    if (repairedHierarchyRows != 0u || repairedFrameIds != 0u) {
        std::ostringstream out;
        out << "Restored retail VCS player HAnim identity mapping: "
            << repairedHierarchyRows << " hierarchy row" << (repairedHierarchyRows == 1u ? "" : "s")
            << " and " << repairedFrameIds << " RslNode bone ID" << (repairedFrameIds == 1u ? "" : "s")
            << " corrected. Geometry, skin weights, DMA/VIF data, materials and file offsets were not changed.";
        detail = out.str();
    }
    return true;
}

bool storylandRepairLeedsRelocationCount(
    std::vector<uint8_t>& bytes,
    uint32_t& addedEntries,
    std::string& detail
) {
    addedEntries = 0;
    detail.clear();
    if (!isLeedsMdl(bytes)) return true;

    const uint32_t oldCount = readU32(bytes, 0x14u);
    const std::vector<uint32_t> trailing = trailingRelocationsBeyondDeclaredCount(bytes);
    if (trailing.empty()) return true;
    if (oldCount > 1000000u || trailing.size() > uint64_t(0xFFFFFFFFu - oldCount)) {
        detail = "Relocation count cannot be repaired safely.";
        return false;
    }

    const uint32_t newCount = oldCount + uint32_t(trailing.size());
    writeU32(bytes, 0x14u, newCount);
    addedEntries = uint32_t(trailing.size());

    std::ostringstream out;
    out << "Corrected Leeds relocation count " << oldCount << " -> " << newCount
        << ". The omitted relocation field";
    if (trailing.size() != 1) out << "s";
    out << " already existed in the file";
    if (!trailing.empty()) {
        out << " (";
        for (size_t i = 0; i < trailing.size(); ++i) {
            if (i) out << ", ";
            out << hexOffset(trailing[i]);
        }
        out << ")";
    }
    out << "; no relocation rows or payload bytes were invented.";
    detail = out.str();
    return true;
}
