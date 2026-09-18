#include "storyland_dma_validator.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <vector>

namespace {


uint32_t readU32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4u) return 0;
    return uint32_t(bytes[offset]) |
           (uint32_t(bytes[offset + 1]) << 8u) |
           (uint32_t(bytes[offset + 2]) << 16u) |
           (uint32_t(bytes[offset + 3]) << 24u);
}

uint64_t readU64(const uint8_t* bytes) {
    uint64_t value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

bool checkedAdd(size_t a, size_t b, size_t& result) {
    if (b > std::numeric_limits<size_t>::max() - a) return false;
    result = a + b;
    return true;
}

bool checkedMul(size_t a, size_t b, size_t& result) {
    if (a != 0u && b > std::numeric_limits<size_t>::max() / a) return false;
    result = a * b;
    return true;
}

size_t align4(size_t value) {
    if (value > std::numeric_limits<size_t>::max() - 3u) return std::numeric_limits<size_t>::max();
    return (value + 3u) & ~size_t(3u);
}

std::string hexValue(uint64_t value, int width = 8) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << value;
    return out.str();
}

void addIssue(StorylandDmaTlbReport& report, StorylandDmaIssueSeverity severity,
              size_t offset, uint32_t address, const std::string& message) {
    report.issues.push_back({severity, offset, address, message});
    if (severity == StorylandDmaIssueSeverity::Fatal) ++report.fatals;
    else if (severity == StorylandDmaIssueSeverity::Warning) ++report.warnings;
}

bool knownVifCommand(uint8_t command) {
    if (command <= 0x07u) return true;
    switch (command) {
    case 0x10u: // FLUSHE
    case 0x11u: // FLUSH
    case 0x13u: // FLUSHA
    case 0x14u: // MSCAL
    case 0x15u: // MSCALF
    case 0x17u: // MSCNT
    case 0x20u: // STMASK
    case 0x30u: // STROW
    case 0x31u: // STCOL
    case 0x4Au: // MPG
    case 0x50u: // DIRECT
    case 0x51u: // DIRECTHL
        return true;
    default:
        return command >= 0x60u && command <= 0x7Fu;
    }
}

struct VifState {
    uint32_t cycleCl = 1u;
    uint32_t cycleWl = 1u;
    uint32_t base = 0u;
    uint32_t offset = 0u;
    uint32_t tops = 0u;
    uint32_t itops = 0u;
    bool dbf = false;
};

void updateVifTopAfterMicroprogram(VifState& state) {
    state.dbf = !state.dbf;
    state.tops = (state.base + (state.dbf ? state.offset : 0u)) & 0x3FFu;
}

struct VifPayloadInfo {
    size_t bytes = 0;
    bool unpack = false;
    uint32_t unpackWriteCount = 0u;
    uint32_t unpackSourceCount = 0u;
    uint32_t unpackBase = 0u;
    uint32_t unpackLast = 0u;
};

bool vifPayloadInfo(uint32_t code, const VifState& state, VifPayloadInfo& info) {
    info = {};
    const uint8_t command = uint8_t((code >> 24u) & 0x7Fu);
    const uint32_t numRaw = (code >> 16u) & 0xFFu;
    const uint32_t num = numRaw == 0u ? 256u : numRaw;

    if (!knownVifCommand(command)) return false;

    if (command == 0x20u) {
        info.bytes = 4u;
        return true;
    }
    if (command == 0x30u || command == 0x31u) {
        info.bytes = 16u;
        return true;
    }
    if (command == 0x4Au) {
        return checkedMul(size_t(num), 8u, info.bytes);
    }
    if (command == 0x50u || command == 0x51u) {
        uint32_t qwc = code & 0xFFFFu;
        if (qwc == 0u) qwc = 65536u;
        return checkedMul(size_t(qwc), 16u, info.bytes);
    }
    if (command < 0x60u || command > 0x7Fu) return true;

    info.unpack = true;
    info.unpackWriteCount = num;

    const uint32_t vn = (command >> 2u) & 3u;
    const uint32_t vl = command & 3u;
    size_t bytesPerVector = 0u;
    if (vn == 3u && vl == 3u) {
        bytesPerVector = 2u; // V4-5 is one packed 16-bit word per source vector.
    } else {
        static constexpr std::array<uint32_t, 4> bitsPerComponent = {32u, 16u, 8u, 5u};
        const uint32_t components = vn + 1u;
        const uint32_t bits = components * bitsPerComponent[vl];
        bytesPerVector = (bits + 7u) / 8u;
    }

    const uint32_t cl = state.cycleCl == 0u ? 256u : state.cycleCl;
    const uint32_t wl = state.cycleWl == 0u ? 256u : state.cycleWl;
    uint32_t sourceCount = num;
    if (cl < wl) {
        const uint32_t fullBlocks = num / wl;
        const uint32_t remainder = num % wl;
        sourceCount = fullBlocks * cl + std::min(remainder, cl);
    }
    info.unpackSourceCount = sourceCount;

    size_t rawBytes = 0u;
    if (!checkedMul(size_t(sourceCount), bytesPerVector, rawBytes)) return false;
    info.bytes = align4(rawBytes);
    if (info.bytes == std::numeric_limits<size_t>::max()) return false;

    uint32_t base = code & 0x3FFu;
    if ((code & 0x8000u) != 0u) base = (base + state.tops) & 0x3FFu;
    info.unpackBase = base;

    uint32_t lastAddress = base;
    if (num != 0u) {
        const uint32_t writeIndex = num - 1u;
        if (cl >= wl) {
            const uint32_t block = writeIndex / wl;
            const uint32_t cycle = writeIndex % wl;
            lastAddress = (base + block * cl + cycle) & 0x3FFu;
        } else {
            lastAddress = (base + writeIndex) & 0x3FFu;
        }
    }
    info.unpackLast = lastAddress;
    return true;
}

struct GifDecoder {
    std::vector<uint8_t> carry;
    uint64_t expectedPacketBytes = 0u;
    bool haveTag = false;
    size_t packetStartOffset = 0u;

    bool feed(const uint8_t* data, size_t length, size_t sourceOffset, StorylandDmaTlbReport& report) {
        constexpr size_t maxCarry = 16u * 1024u * 1024u;
        if (length > maxCarry || carry.size() > maxCarry - length) {
            addIssue(report, StorylandDmaIssueSeverity::Fatal, sourceOffset, 0,
                     "GIF DIRECT stream exceeds the bounded packet buffer.");
            return false;
        }
        carry.insert(carry.end(), data, data + length);

        for (;;) {
            if (!haveTag) {
                if (carry.size() < 16u) return true;
                const uint64_t low = readU64(carry.data());
                const uint32_t nloop = uint32_t(low & 0x7FFFu);
                const uint32_t flag = uint32_t((low >> 58u) & 3u);
                uint32_t nreg = uint32_t((low >> 60u) & 0xFu);
                if (nreg == 0u) nreg = 16u;

                uint64_t payloadBytes = 0u;
                if (flag == 0u) {
                    payloadBytes = uint64_t(nloop) * uint64_t(nreg) * 16ull;
                } else if (flag == 1u) {
                    payloadBytes = ((uint64_t(nloop) * uint64_t(nreg) + 1ull) / 2ull) * 16ull;
                } else {
                    payloadBytes = uint64_t(nloop) * 16ull;
                }
                if (payloadBytes > maxCarry) {
                    addIssue(report, StorylandDmaIssueSeverity::Fatal, sourceOffset, 0,
                             "GIFtag declares a packet larger than the validator limit.");
                    return false;
                }
                expectedPacketBytes = 16ull + payloadBytes;
                haveTag = true;
                packetStartOffset = sourceOffset;
                ++report.gifTags;
            }

            if (uint64_t(carry.size()) < expectedPacketBytes) return true;
            report.gifPayloadBytes += expectedPacketBytes - 16ull;
            ++report.gifPackets;
            carry.erase(carry.begin(), carry.begin() + static_cast<std::ptrdiff_t>(expectedPacketBytes));
            haveTag = false;
            expectedPacketBytes = 0u;
            if (carry.empty()) return true;
        }
    }

    void finish(StorylandDmaTlbReport& report, size_t sourceOffset) const {
        if (haveTag || !carry.empty()) {
            addIssue(report, StorylandDmaIssueSeverity::Warning, sourceOffset, 0,
                     "GIF DIRECT data ends with an incomplete packet; a later DMA/VIF transfer may continue it at runtime.");
        }
    }
};

struct VifWalk {
    bool credible = false;
    bool overrun = false;
    size_t overrunOffset = 0u;
    uint32_t commands = 0u;
    uint32_t unpacks = 0u;
    uint32_t directs = 0u;
    size_t endOffset = 0u;
    VifState state;
    GifDecoder gif;
};

VifWalk walkVif(const std::vector<uint8_t>& bytes, size_t start, size_t end, bool anchored,
                StorylandDmaTlbReport* reportForGif = nullptr, GifDecoder* sharedGif = nullptr) {
    VifWalk walk;
    if (sharedGif != nullptr) walk.gif = *sharedGif;
    size_t cursor = start;
    uint32_t meaningful = 0u;
    end = std::min(end, bytes.size());

    while (cursor <= end && end - cursor >= 4u && walk.commands < 65536u) {
        const size_t commandOffset = cursor;
        const uint32_t code = readU32(bytes, cursor);
        const uint8_t command = uint8_t((code >> 24u) & 0x7Fu);
        if (!knownVifCommand(command)) break;

        VifPayloadInfo payload;
        if (!vifPayloadInfo(code, walk.state, payload)) break;
        cursor += 4u;
        ++walk.commands;
        if (command != 0u) ++meaningful;
        if (payload.unpack) ++walk.unpacks;

        if (payload.bytes > end - cursor) {
            walk.overrun = true;
            walk.overrunOffset = commandOffset;
            break;
        }

        if (command == 0x01u) {
            uint32_t cl = code & 0xFFu;
            uint32_t wl = (code >> 8u) & 0xFFu;
            walk.state.cycleCl = cl == 0u ? 256u : cl;
            walk.state.cycleWl = wl == 0u ? 256u : wl;
        } else if (command == 0x02u) {
            walk.state.offset = code & 0x3FFu;
            walk.state.tops = walk.state.base & 0x3FFu;
            walk.state.dbf = false;
        } else if (command == 0x03u) {
            walk.state.base = code & 0x3FFu;
        } else if (command == 0x04u) {
            walk.state.itops = code & 0x3FFu;
        } else if (command == 0x14u || command == 0x15u || command == 0x17u) {
            updateVifTopAfterMicroprogram(walk.state);
        } else if (command == 0x50u || command == 0x51u) {
            ++walk.directs;
            if (reportForGif != nullptr) {
                ++reportForGif->directTransfers;
                if (!walk.gif.feed(bytes.data() + cursor, payload.bytes, cursor, *reportForGif)) {
                    walk.overrun = true;
                    walk.overrunOffset = commandOffset;
                    break;
                }
            }
        }

        cursor += payload.bytes;
        if (anchored && (walk.unpacks != 0u || walk.directs != 0u) && meaningful >= 2u) walk.credible = true;
        if (!anchored && walk.unpacks != 0u && meaningful >= 3u) walk.credible = true;
    }

    if (anchored && meaningful >= 2u && (walk.unpacks != 0u || walk.directs != 0u)) walk.credible = true;
    walk.endOffset = cursor;
    if (sharedGif != nullptr) *sharedGif = walk.gif;
    return walk;
}

struct DmaTag {
    uint16_t qwc = 0u;
    uint8_t id = 0u;
    bool irq = false;
    bool spr = false;
    uint32_t address = 0u;
};

bool decodeDmaTag(const std::vector<uint8_t>& bytes, size_t offset, DmaTag& tag) {
    if ((offset & 0x0Fu) != 0u || offset > bytes.size() || bytes.size() - offset < 16u) return false;
    const uint32_t word0 = readU32(bytes, offset);
    const uint32_t word1 = readU32(bytes, offset + 4u);
    if ((word0 & 0x03FF0000u) != 0u) return false;
    tag.qwc = uint16_t(word0 & 0xFFFFu);
    tag.id = uint8_t((word0 >> 28u) & 7u);
    tag.irq = (word0 & 0x80000000u) != 0u;
    tag.spr = (word1 & 0x80000000u) != 0u;
    tag.address = word1 & 0x7FFFFFFFu;
    return true;
}

bool dmaAddressFitsHardware(const DmaTag& tag, uint64_t byteCount) {
    const uint64_t limit = tag.spr ? 0x4000ull : 0x02000000ull;
    const uint64_t address = tag.address;
    return address <= limit && byteCount <= limit - address;
}

bool tagHasPlausibleVifCodes(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 16u) return false;
    uint32_t recognized = 0u;
    for (size_t codeOffset = offset + 8u; codeOffset < offset + 16u; codeOffset += 4u) {
        const uint32_t code = readU32(bytes, codeOffset);
        if (code == 0u || knownVifCommand(uint8_t((code >> 24u) & 0x7Fu))) ++recognized;
    }
    return recognized == 2u;
}

bool fileLocalAddress(const DmaTag& tag, size_t fileSize, size_t& offset) {
    if (tag.spr) return false;
    if (uint64_t(tag.address) >= uint64_t(fileSize)) return false;
    offset = size_t(tag.address);
    return true;
}

bool candidateDmaChainIsStructurallyValid(const std::vector<uint8_t>& bytes, size_t root) {
    size_t cursor = root;
    std::vector<size_t> callStack;
    std::set<size_t> visited;

    for (uint32_t steps = 0u; steps < 4096u; ++steps) {
        if (cursor > bytes.size() || bytes.size() - cursor < 16u || (cursor & 0x0Fu) != 0u) return false;
        if (!visited.insert(cursor).second) return false;

        DmaTag tag;
        if (!decodeDmaTag(bytes, cursor, tag)) return false;

        const size_t inlineStart = cursor + 16u;
        size_t inlineBytes = 0u;
        size_t sequential = 0u;
        if (!checkedMul(size_t(tag.qwc), 16u, inlineBytes) ||
            !checkedAdd(inlineStart, inlineBytes, sequential)) return false;

        const bool inlinePayload = tag.id == 1u || tag.id == 2u || tag.id == 5u || tag.id == 6u || tag.id == 7u;
        if (inlinePayload && sequential > bytes.size()) return false;

        if (tag.id == 0u || tag.id == 7u) return true;
        if (tag.id == 1u) {
            cursor = sequential;
            continue;
        }
        if (tag.id == 2u) {
            if (tag.spr || tag.address >= bytes.size()) return true;
            if ((tag.address & 0x0Fu) != 0u) return false;
            cursor = size_t(tag.address);
            continue;
        }
        if (tag.id == 3u || tag.id == 4u) {
            cursor += 16u;
            continue;
        }
        if (tag.id == 5u) {
            if (callStack.size() >= 2u) return false;
            callStack.push_back(sequential);
            if (tag.spr || tag.address >= bytes.size()) return true;
            if ((tag.address & 0x0Fu) != 0u) return false;
            cursor = size_t(tag.address);
            continue;
        }
        if (tag.id == 6u) {
            if (callStack.empty()) return false;
            cursor = callStack.back();
            callStack.pop_back();
            continue;
        }
        return false;
    }
    return false;
}

void inspectExternalPayload(StorylandDmaTlbReport& report, const std::vector<uint8_t>& bytes,
                            size_t tagOffset, const DmaTag& tag, GifDecoder& gifState) {
    const uint64_t byteCount = uint64_t(tag.qwc) * 16ull;
    report.dmaPayloadBytes += byteCount;
    if (!dmaAddressFitsHardware(tag, byteCount)) {
        addIssue(report, StorylandDmaIssueSeverity::Fatal, tagOffset, tag.address,
                 tag.spr ? "DMA source range exceeds the 16 KiB scratchpad." :
                           "DMA source range exceeds 32 MiB EE physical RAM.");
        return;
    }

    size_t payloadOffset = 0u;
    if (!fileLocalAddress(tag, bytes.size(), payloadOffset)) {
        if (tag.qwc != 0u) {
            addIssue(report, StorylandDmaIssueSeverity::Warning, tagOffset, tag.address,
                     "DMA source is hardware-valid but not file-local; runtime relocation or upload must supply it.");
        }
        return;
    }
    if (byteCount > uint64_t(bytes.size() - payloadOffset)) {
        addIssue(report, StorylandDmaIssueSeverity::Warning, tagOffset, tag.address,
                 "DMA source begins in this file but extends beyond it; treat the address as runtime-relocated unless the file is an exact RAM image.");
        return;
    }

    const size_t payloadBytes = size_t(byteCount);
    VifWalk vif = walkVif(bytes, payloadOffset, payloadOffset + payloadBytes, true, &report, &gifState);
    if (vif.credible && !vif.overrun) {
        ++report.vifStreams;
        report.vifCommands += vif.commands;
        report.vifUnpacks += vif.unpacks;
    }
}

} // namespace

std::string StorylandDmaTlbReport::text() const {
    std::ostringstream out;
    out << "PS2 DMA / GIF / VIF test\r\n\r\n"
        << "Target: " << label << "\r\n"
        << "Bytes inspected: " << byteSize << "\r\n"
        << "DMA tags: " << dmaTags << "\r\n"
        << "DMA chains: " << dmaChains << "\r\n"
        << "DMA payload bytes: " << dmaPayloadBytes << "\r\n"
        << "VIF streams: " << vifStreams << "\r\n"
        << "VIF commands: " << vifCommands << "\r\n"
        << "VIF UNPACK commands: " << vifUnpacks << "\r\n"
        << "DIRECT/DIRECTHL transfers: " << directTransfers << "\r\n"
        << "GIF tags: " << gifTags << "\r\n"
        << "Complete GIF packets: " << gifPackets << "\r\n"
        << "GIF payload bytes: " << gifPayloadBytes << "\r\n"
        << "Warnings: " << warnings << "\r\n"
        << "Fatal hazards: " << fatals << "\r\n\r\n";

    if (!safe()) out << "RESULT: FAIL - one or more decoded DMA/VIF/GIF ranges are invalid.\r\n";
    else if (dmaTags == 0u && vifStreams == 0u) out << "RESULT: LIMITED - no anchored DMA chain or credible VIF stream was found.\r\n";
    else out << "RESULT: PASS - decoded file-resident DMA/VIF/GIF data stayed within validated bounds.\r\n";

    if (warnings != 0u) {
        out << "Hardware-valid RAM addresses outside the file remain warnings because GAME.DTZ loading and runtime relocation can supply them.\r\n";
    }
    if (!issues.empty()) out << "\r\nIssues\r\n";
    for (const StorylandDmaIssue& issue : issues) {
        const char* severity = issue.severity == StorylandDmaIssueSeverity::Fatal ? "FATAL" :
                               issue.severity == StorylandDmaIssueSeverity::Warning ? "WARN" : "INFO";
        out << severity << " @ file+" << hexValue(issue.offset)
            << " addr=" << hexValue(issue.address) << " | " << issue.message << "\r\n";
    }
    return out.str();
}

StorylandDmaTlbReport storylandValidatePs2DmaTlb(const std::vector<uint8_t>& bytes, const std::string& label) {
    StorylandDmaTlbReport report;
    report.label = label;
    report.byteSize = bytes.size();
    if (bytes.empty()) {
        addIssue(report, StorylandDmaIssueSeverity::Fatal, 0u, 0u, "Resource is empty.");
        return report;
    }

    // Locate source-chain roots. A legal tag header plus valid upper-qword
    // VIFcodes is required; this avoids treating ordinary vertex data as chain tags.
    std::set<size_t> roots;
    for (size_t offset = 0u; offset <= bytes.size() && bytes.size() - offset >= 16u; offset += 16u) {
        DmaTag tag;
        if (!decodeDmaTag(bytes, offset, tag) || !tagHasPlausibleVifCodes(bytes, offset)) continue;
        // Source-chain roots used by Leeds model packets keep the DMA PCE bits clear.
        // Requiring that here prevents ordinary VIF words such as 0x14000006 from
        // being promoted to a CNT tag merely because they happen to be 16-byte aligned.
        const uint32_t word0 = readU32(bytes, offset);
        if (((word0 >> 26u) & 3u) != 0u) continue;
        if (tag.id == 1u || tag.id == 5u || tag.id == 7u) {
            // A source-chain root with inline QWC must fit inside the selected
            // resource. Rejecting impossible roots here prevents arbitrary VIF
            // or vertex words from being promoted to a fatal DMA chain.
            size_t inlineBytes = 0u;
            size_t sequential = 0u;
            if (!checkedMul(size_t(tag.qwc), 16u, inlineBytes) ||
                !checkedAdd(offset + 16u, inlineBytes, sequential) ||
                sequential > bytes.size()) {
                continue;
            }
            if (candidateDmaChainIsStructurallyValid(bytes, offset)) roots.insert(offset);
        }
    }

    std::set<size_t> globallyVisited;
    for (size_t root : roots) {
        if (globallyVisited.count(root) != 0u) continue;
        ++report.dmaChains;
        size_t cursor = root;
        std::vector<size_t> callStack;
        GifDecoder chainGif;

        for (uint32_t steps = 0u; steps < 65536u; ++steps) {
            if (cursor > bytes.size() || bytes.size() - cursor < 16u || (cursor & 0x0Fu) != 0u) {
                addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, uint32_t(cursor),
                         "DMA chain reaches a truncated or unaligned tag.");
                break;
            }
            if (!globallyVisited.insert(cursor).second) {
                addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, uint32_t(cursor),
                         "DMA chain loops back to an already visited tag.");
                break;
            }

            DmaTag tag;
            if (!decodeDmaTag(bytes, cursor, tag)) {
                addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, 0u, "DMA tag header is invalid.");
                break;
            }
            ++report.dmaTags;
            const size_t inlineStart = cursor + 16u;
            size_t inlineBytes = 0u;
            if (!checkedMul(size_t(tag.qwc), 16u, inlineBytes)) {
                addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, tag.address, "DMA QWC byte count overflowed the host size type.");
                break;
            }
            size_t sequential = 0u;
            if (!checkedAdd(inlineStart, inlineBytes, sequential)) {
                addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, tag.address, "DMA inline range overflowed the host size type.");
                break;
            }

            const bool inlinePayload = tag.id == 1u || tag.id == 2u || tag.id == 5u || tag.id == 6u || tag.id == 7u;
            if (inlinePayload) {
                report.dmaPayloadBytes += uint64_t(inlineBytes);
                if (sequential > bytes.size()) {
                    addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, tag.address,
                             "DMA inline payload extends beyond the resource.");
                    break;
                }
                if (inlineBytes != 0u) {
                    VifWalk vif = walkVif(bytes, inlineStart, sequential, true, &report, &chainGif);
                    if (vif.credible && !vif.overrun) {
                        ++report.vifStreams;
                        report.vifCommands += vif.commands;
                        report.vifUnpacks += vif.unpacks;
                    }
                }
            }

            if (tag.id == 0u || tag.id == 3u || tag.id == 4u) {
                inspectExternalPayload(report, bytes, cursor, tag, chainGif);
            }

            if (tag.id == 0u || tag.id == 7u) break; // REFE / END
            if (tag.id == 1u) { // CNT
                cursor = sequential;
                continue;
            }
            if (tag.id == 2u) { // NEXT
                if (tag.spr) {
                    addIssue(report, StorylandDmaIssueSeverity::Warning, cursor, tag.address,
                             "NEXT target is in scratchpad and cannot be resolved from this file.");
                    break;
                }
                if (tag.address >= bytes.size()) {
                    addIssue(report, StorylandDmaIssueSeverity::Warning, cursor, tag.address,
                             "NEXT target is hardware-valid but outside this file; runtime relocation must supply it.");
                    break;
                }
                cursor = size_t(tag.address);
                continue;
            }
            if (tag.id == 3u || tag.id == 4u) { // REF / REFS: next tag is sequential, payload is external.
                cursor = cursor + 16u;
                continue;
            }
            if (tag.id == 5u) { // CALL
                if (callStack.size() >= 2u) {
                    addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, tag.address,
                             "DMA CALL stack exceeds the PS2 two-level ASR stack.");
                    break;
                }
                callStack.push_back(sequential);
                if (tag.spr || tag.address >= bytes.size()) {
                    addIssue(report, StorylandDmaIssueSeverity::Warning, cursor, tag.address,
                             "DMA CALL target cannot be resolved inside this file.");
                    break;
                }
                cursor = size_t(tag.address);
                continue;
            }
            if (tag.id == 6u) { // RET transfers its inline QWC, then returns.
                if (callStack.empty()) {
                    addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, 0u, "DMA RET has no matching CALL.");
                    break;
                }
                cursor = callStack.back();
                callStack.pop_back();
                continue;
            }
        }
        chainGif.finish(report, cursor);
    }

    // A raw VIF payload has no DMA chain. Only use the bounded fallback scan when
    // the anchored chain walk found no VIF stream, so one packet is not counted twice.
    if (report.vifStreams == 0u) {
        size_t coveredUntil = 0u;
        for (size_t offset = 0u; offset <= bytes.size() && bytes.size() - offset >= 4u; offset += 4u) {
            if (offset < coveredUntil) continue;
            const uint8_t command = uint8_t((readU32(bytes, offset) >> 24u) & 0x7Fu);
            if (command < 0x60u || command > 0x7Fu) continue;
            VifWalk vif = walkVif(bytes, offset, bytes.size(), false, &report);
            if (!vif.credible || vif.overrun) continue;
            ++report.vifStreams;
            report.vifCommands += vif.commands;
            report.vifUnpacks += vif.unpacks;
            coveredUntil = std::max(coveredUntil, vif.endOffset);
            vif.gif.finish(report, vif.endOffset);
        }
    }

    if (report.vifStreams == 0u) {
        addIssue(report, StorylandDmaIssueSeverity::Warning, 0u, 0u,
                 "No credible VIF UNPACK or DIRECT stream was found in the selected bytes. The resource either has no PS2 VIF payload here or stores it behind a wrapper/relocation.");
    }
    return report;
}
