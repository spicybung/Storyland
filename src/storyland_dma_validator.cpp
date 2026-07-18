#include "storyland_dma_validator.h"

#include <algorithm>
#include <iomanip>
#include <limits>
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

std::string hexValue(uint64_t value, int width = 8) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << value;
    return out.str();
}

void addIssue(StorylandDmaTlbReport& report, StorylandDmaIssueSeverity severity,
              size_t offset, uint32_t address, const std::string& message) {
    report.issues.push_back({severity, offset, address, message});
    if (severity == StorylandDmaIssueSeverity::Fatal) report.fatals++;
    else if (severity == StorylandDmaIssueSeverity::Warning) report.warnings++;
}

bool knownVifCommand(uint8_t cmd) {
    if (cmd <= 0x07) return true;
    if (cmd == 0x10 || cmd == 0x11 || cmd == 0x13 || cmd == 0x14 ||
        cmd == 0x15 || cmd == 0x17 || cmd == 0x20 || cmd == 0x30 ||
        cmd == 0x31 || cmd == 0x4A || cmd == 0x50 || cmd == 0x51) return true;
    return cmd >= 0x60 && cmd <= 0x7F;
}

size_t align4(size_t value) { return (value + 3u) & ~size_t(3u); }

bool vifPayloadBytes(uint32_t code, size_t& outBytes) {
    const uint8_t cmd = uint8_t((code >> 24) & 0x7Fu);
    const uint32_t numRaw = (code >> 16) & 0xFFu;
    const uint32_t num = numRaw == 0 ? 256u : numRaw;
    outBytes = 0;
    if (cmd == 0x20) { outBytes = 4; return true; }
    if (cmd == 0x30 || cmd == 0x31) { outBytes = 16; return true; }
    if (cmd == 0x4A) { outBytes = size_t(num) * 8u; return true; }
    if (cmd == 0x50 || cmd == 0x51) { outBytes = size_t(code & 0xFFFFu) * 16u; return true; }
    if (cmd >= 0x60 && cmd <= 0x7F) {
        const uint32_t vn = (cmd >> 2) & 3u;
        const uint32_t vl = cmd & 3u;
        static const uint32_t bitsPerComponent[4] = {32u, 16u, 8u, 5u};
        const uint64_t bits = uint64_t(num) * uint64_t(vn + 1u) * bitsPerComponent[vl];
        outBytes = align4(size_t((bits + 7u) / 8u));
        return true;
    }
    return knownVifCommand(cmd);
}

struct VifWalk {
    bool credible = false;
    bool overrun = false;
    size_t overrunOffset = 0;
    uint32_t commands = 0;
    uint32_t unpacks = 0;
    size_t endOffset = 0;
};

VifWalk walkVif(const std::vector<uint8_t>& bytes, size_t start, size_t end) {
    VifWalk walk;
    size_t cursor = start;
    uint32_t nonNop = 0;
    while (cursor + 4 <= end && walk.commands < 4096) {
        const size_t commandOffset = cursor;
        const uint32_t code = readU32(bytes, cursor);
        const uint8_t cmd = uint8_t((code >> 24) & 0x7Fu);
        if (!knownVifCommand(cmd)) break;
        size_t payload = 0;
        if (!vifPayloadBytes(code, payload)) break;
        cursor += 4;
        walk.commands++;
        if (cmd != 0) nonNop++;
        if (cmd >= 0x60 && cmd <= 0x7F) walk.unpacks++;
        if (payload > end - cursor) {
            walk.overrun = true;
            walk.overrunOffset = commandOffset;
            break;
        }
        cursor += payload;
        if (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) {
            if (walk.unpacks != 0 && nonNop >= 2) walk.credible = true;
        }
    }
    if (walk.unpacks != 0 && nonNop >= 3) walk.credible = true;
    walk.endOffset = cursor;
    return walk;
}

struct DmaTag {
    uint16_t qwc = 0;
    uint8_t id = 0;
    bool irq = false;
    bool spr = false;
    uint32_t address = 0;
};

bool decodeCredibleDmaTag(const std::vector<uint8_t>& bytes, size_t offset, DmaTag& tag) {
    if ((offset & 0x0Fu) != 0 || offset + 16 > bytes.size()) return false;
    const uint32_t word0 = readU32(bytes, offset);
    const uint32_t word1 = readU32(bytes, offset + 4);
    if ((word0 & 0x03FF0000u) != 0) return false; // reserved bits in a DMAtag
    tag.qwc = uint16_t(word0 & 0xFFFFu);
    tag.id = uint8_t((word0 >> 28) & 7u);
    tag.irq = (word0 & 0x80000000u) != 0;
    tag.spr = (word1 & 0x80000000u) != 0;
    tag.address = word1 & 0x7FFFFFFFu;
    if (tag.qwc > 0x4000u) return false;

    // A real tag normally carries zero or recognizable VIFcodes in its upper qword.
    for (size_t vif = offset + 8; vif < offset + 16; vif += 4) {
        const uint32_t code = readU32(bytes, vif);
        if (code != 0 && !knownVifCommand(uint8_t((code >> 24) & 0x7Fu))) return false;
    }
    const uint64_t inlineEnd = uint64_t(offset) + 16ull + uint64_t(tag.qwc) * 16ull;
    if ((tag.id == 1 || tag.id == 7) && inlineEnd > bytes.size()) return false;
    return true;
}

bool targetLooksImpossible(const DmaTag& tag) {
    if (tag.spr) return tag.address > 0x3FFFu;
    // DMA addresses are physical; retail EE main RAM is 32 MiB.
    return tag.address > 0x01FFFFFFu;
}

void inspectTagTarget(StorylandDmaTlbReport& report, size_t offset, const DmaTag& tag, size_t fileSize) {
    const bool addressTag = tag.id == 0 || tag.id == 2 || tag.id == 3 || tag.id == 4 || tag.id == 5;
    if (!addressTag) return;
    if (tag.address == 0 && (tag.id == 2 || tag.id == 3 || tag.id == 4 || tag.id == 5)) {
        addIssue(report, StorylandDmaIssueSeverity::Fatal, offset, tag.address,
                 "DMA tag requires a target but its ADDR is zero.");
        return;
    }
    if (targetLooksImpossible(tag)) {
        addIssue(report, StorylandDmaIssueSeverity::Fatal, offset, tag.address,
                 std::string(tag.spr ? "Scratchpad DMA target exceeds 16 KiB" : "DMA target exceeds 32 MiB EE physical RAM") +
                 "; this is a likely TLB/DMA miss.");
    } else if (!tag.spr && tag.address >= fileSize) {
        addIssue(report, StorylandDmaIssueSeverity::Warning, offset, tag.address,
                 "DMA target is runtime-RAM-valid but cannot be resolved inside this file; relocation must supply it.");
    }
}

} // namespace

std::string StorylandDmaTlbReport::text() const {
    std::ostringstream out;
    out << "PS2 DMA / TLB preflight\r\n\r\n"
        << "Target: " << label << "\r\n"
        << "Bytes inspected: " << byteSize << "\r\n"
        << "Credible DMA tags: " << dmaTags << "\r\n"
        << "DMA chains: " << dmaChains << "\r\n"
        << "Credible VIF streams: " << vifStreams << "\r\n"
        << "VIF commands: " << vifCommands << "\r\n"
        << "VIF UNPACK commands: " << vifUnpacks << "\r\n"
        << "Warnings: " << warnings << "\r\n"
        << "Fatal hazards: " << fatals << "\r\n\r\n"
        << (!safe() ? "RESULT: FAIL - do not install this resource until the fatal addresses are fixed."
                    : dmaTags == 0
                        ? "RESULT: LIMITED - no definite file-local DMA/TLB escape was found, but runtime relocation and streaming allocation were not emulated."
                        : "RESULT: PASS - no definite DMA/TLB escape was found in the decoded file-resident chain.")
        << "\r\n";
    if (dmaTags == 0)
        out << "Note: no credible file-resident DMAtags were found. Headerless VIF payloads were bounds-checked, but a standalone MDL cannot prove that GAME.DTZ assigned enough IMG sectors or that runtime pointers relocate safely.\r\n";
    if (warnings != 0)
        out << "Runtime relocation warnings are not automatic failures because file offsets are not final EE addresses.\r\n";
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
        addIssue(report, StorylandDmaIssueSeverity::Fatal, 0, 0, "Resource is empty.");
        return report;
    }

    std::set<size_t> credibleVifStarts;
    size_t coveredVifUntil = 0;
    for (size_t offset = 0; offset + 4 <= bytes.size(); offset += 4) {
        if (offset < coveredVifUntil) continue;
        const uint8_t cmd = uint8_t((readU32(bytes, offset) >> 24) & 0x7Fu);
        if (cmd < 0x60 || cmd > 0x7F) continue;
        const VifWalk walk = walkVif(bytes, offset, bytes.size());
        // Headerless streams are discovered heuristically by scanning every
        // aligned dword.  Retail MDL payloads contain vertex/animation words
        // that can accidentally decode as several legal VIF commands.  If
        // such an inferred walk overruns EOF, it is not an anchored stream and
        // therefore cannot prove a DMA/TLB hazard.  Reject the candidate
        // instead of failing an otherwise valid retail model.
        if (!walk.credible || walk.overrun) continue;
        credibleVifStarts.insert(offset);
        coveredVifUntil = std::max(coveredVifUntil, walk.endOffset);
        report.vifStreams++;
        report.vifCommands += walk.commands;
        report.vifUnpacks += walk.unpacks;
    }

    std::set<size_t> candidateTags;
    for (size_t offset = 0; offset + 16 <= bytes.size(); offset += 16) {
        DmaTag tag;
        if (!decodeCredibleDmaTag(bytes, offset, tag)) continue;
        if (tag.qwc == 0) continue;
        if (tag.id != 1 && tag.id != 7) continue; // chain roots are inline CNT/END packets
        const uint32_t tagVif0 = readU32(bytes, offset + 8);
        const uint32_t tagVif1 = readU32(bytes, offset + 12);
        if (tagVif0 == 0 && tagVif1 == 0) continue;
        // A root candidate must lead to a credible inline VIF payload. This
        // avoids treating aligned vertex/material data as random DMAtags.
        const size_t inlineStart = offset + 16;
        const VifWalk inlineVif = walkVif(bytes, inlineStart, std::min(bytes.size(), inlineStart + size_t(tag.qwc) * 16u));
        if (!inlineVif.credible) continue;
        candidateTags.insert(offset);
    }

    std::set<size_t> visited;
    for (size_t root : candidateTags) {
        if (visited.count(root)) continue;
        report.dmaChains++;
        size_t cursor = root;
        std::vector<size_t> callStack;
        for (uint32_t steps = 0; steps < 65536; ++steps) {
            if (!visited.insert(cursor).second) {
                addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, 0, "DMA chain loops back to an already visited tag.");
                break;
            }
            DmaTag tag;
            if (!decodeCredibleDmaTag(bytes, cursor, tag)) {
                addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, 0, "DMA chain reaches an invalid or truncated tag.");
                break;
            }
            report.dmaTags++;
            inspectTagTarget(report, cursor, tag, bytes.size());

            const size_t inlineStart = cursor + 16;
            const size_t sequential = inlineStart + size_t(tag.qwc) * 16u;
            if (tag.id == 7 || tag.id == 0) break; // END / REFE
            if (tag.id == 1 || tag.id == 3 || tag.id == 4) { cursor = (tag.id == 1) ? sequential : cursor + 16; continue; }
            if (tag.id == 2) { // NEXT
                if (tag.address >= bytes.size()) break; // valid runtime relocation warning/fatal already reported
                cursor = tag.address;
                continue;
            }
            if (tag.id == 5) { // CALL, two address registers on the PS2
                if (callStack.size() >= 2) {
                    addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, tag.address, "DMA CALL stack exceeds the PS2 two-level ASR stack.");
                    break;
                }
                callStack.push_back(sequential);
                if (tag.address >= bytes.size()) break;
                cursor = tag.address;
                continue;
            }
            if (tag.id == 6) { // RET
                if (callStack.empty()) {
                    addIssue(report, StorylandDmaIssueSeverity::Fatal, cursor, 0, "DMA RET has no matching CALL.");
                    break;
                }
                cursor = callStack.back();
                callStack.pop_back();
                continue;
            }
            break;
        }
    }

    if (report.vifStreams == 0)
        addIssue(report, StorylandDmaIssueSeverity::Warning, 0, 0, "No credible VIF UNPACK stream was found; geometry may use an unsupported wrapper or be non-PS2 data.");
    return report;
}
