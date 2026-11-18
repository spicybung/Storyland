#include "src/validation/storyland_dma_validator.h"

// Bounded PS2 packet execution. The VU interpreter implements the integer, memory,
// and packet-output instructions used by packet-copy programs. Other microcode
// is explicitly incomplete; it is never reported as a full hardware pass.

#include <algorithm>
#include <array>
#include <bitset>
#include <cstring>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <vector>
#include <map>

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
    report.issues.push_back({severity, offset, address, message, {}, 0u, 0u});
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
    if (vl == 3u && vn != 3u) return false;
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


struct GsDryState {
    std::array<uint64_t, 0x80> regs{};
    std::bitset<0x80> written;

    void write(uint32_t address, uint64_t value, StorylandDmaTlbReport& report, size_t sourceOffset) {
        if (address >= regs.size()) {
            addIssue(report, StorylandDmaIssueSeverity::Warning, sourceOffset, address,
                     "GIF A+D targets a GS register outside the validated 0x00..0x7F range.");
            return;
        }
        regs[address] = value;
        written.set(address);
        ++report.gsRegisterWrites;

        // PCSX2's GS register handlers treat XYZ2/XYZF2 writes as vertex kicks.
        if (address == 0x05u || address == 0x04u) ++report.gsPrimitiveKicks;
    }
};

bool validGifRegisterDescriptor(uint32_t reg) {
    // GIF packed register descriptors are 4-bit values. 0xB is reserved/invalid,
    // but real streams may carry it as a no-op, so it is reported rather than fatal.
    return reg <= 0x0Fu;
}

struct GifDecoder {
    std::vector<uint8_t> carry;
    uint64_t expectedPacketBytes = 0u;
    bool haveTag = false;
    size_t packetStartOffset = 0u;
    uint32_t packetFlag = 0u;
    uint32_t packetNloop = 0u;
    uint32_t packetNreg = 0u;
    uint64_t packetRegs = 0u;
    GsDryState gs;

    bool decodeCompletePacket(const std::vector<uint8_t>& packet, StorylandDmaTlbReport& report) {
        if (packet.size() < 16u) return false;
        const uint8_t* payload = packet.data() + 16u;
        const size_t payloadBytes = packet.size() - 16u;

        if (packetFlag == 0u) { // PACKED
            const uint64_t totalRegs = uint64_t(packetNloop) * uint64_t(packetNreg);
            if (totalRegs > payloadBytes / 16u) return false;
            for (uint64_t i = 0; i < totalRegs; ++i) {
                const uint32_t descriptor = uint32_t((packetRegs >> ((i % packetNreg) * 4u)) & 0xFu);
                if (!validGifRegisterDescriptor(descriptor)) {
                    addIssue(report, StorylandDmaIssueSeverity::Warning, packetStartOffset, descriptor,
                             "GIF PACKED register descriptor is outside the 4-bit register range.");
                    continue;
                }
                ++report.gifPackedRegisters;
                const uint8_t* qword = payload + i * 16u;
                const uint64_t low = readU64(qword);
                const uint64_t high = readU64(qword + 8u);
                if (descriptor == 0x0Eu) { // A+D
                    gs.write(uint32_t(high & 0xFFu), low, report, packetStartOffset + 16u + size_t(i * 16u));
                } else if (descriptor == 0x05u || descriptor == 0x04u) {
                    // Packed XYZ2/XYZF2 directly kick a vertex in the GS path.
                    ++report.gsPrimitiveKicks;
                }
            }
        } else if (packetFlag == 1u) { // REGLIST
            const uint64_t totalRegs = uint64_t(packetNloop) * uint64_t(packetNreg);
            if (totalRegs > (payloadBytes / 8u)) return false;
            for (uint64_t i = 0; i < totalRegs; ++i) {
                const uint32_t descriptor = uint32_t((packetRegs >> ((i % packetNreg) * 4u)) & 0xFu);
                ++report.gifReglistRegisters;
                const uint64_t value = readU64(payload + i * 8u);
                if (descriptor <= 0x0Fu) gs.write(descriptor, value, report, packetStartOffset + 16u + size_t(i * 8u));
            }
        }
        // IMAGE/IMAGE2 data is intentionally not interpreted as registers; the packet
        // size has already been validated and it is consumed exactly as GS image data.
        return true;
    }

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
                const uint64_t high = readU64(carry.data() + 8u);
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
                packetFlag = flag;
                packetNloop = nloop;
                packetNreg = nreg;
                packetRegs = high;
                ++report.gifTags;
            }

            if (uint64_t(carry.size()) < expectedPacketBytes) return true;
            std::vector<uint8_t> packet(carry.begin(), carry.begin() + static_cast<std::ptrdiff_t>(expectedPacketBytes));
            if (!decodeCompletePacket(packet, report)) {
                addIssue(report, StorylandDmaIssueSeverity::Fatal, packetStartOffset, 0,
                         "GIF packet body is shorter than its PACKED/REGLIST register schedule.");
                return false;
            }
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
            addIssue(report, StorylandDmaIssueSeverity::Fatal, sourceOffset, 0,
                     "GIF/FIFO transfer ends before its declared packet body is complete.");
        }
    }
};

struct ExecutionWord {
    uint32_t value = 0;
    bool known = false;
};

void executionIssue(StorylandDmaTlbReport& report, StorylandDmaIssueSeverity severity,
    const char* stage, size_t offset, uint32_t address, uint32_t qwc,
    uint32_t command, const std::string& message) {
    if (severity == StorylandDmaIssueSeverity::Info) ++report.incompleteStages;
    if (report.issues.size() >= 128u && severity != StorylandDmaIssueSeverity::Fatal) return;
    addIssue(report, severity, offset, address, message);
    StorylandDmaIssue& issue = report.issues.back();
    issue.stage = stage;
    issue.qwc = qwc;
    issue.command = command;
}

uint64_t digestByte(uint64_t digest, uint8_t byte) {
    return (digest ^ uint64_t(byte)) * 1099511628211ull;
}

struct PacketStream {
    std::vector<uint8_t> bytes;
    std::vector<size_t> sourceOffsets;
    std::vector<uint32_t> sourceQwc;
    const std::vector<uint8_t>* regionSource = nullptr;
    size_t regionStart = 0;
    size_t regionSize = 0;

    PacketStream() = default;

    PacketStream(const std::vector<uint8_t>& source, size_t start, size_t count)
        : regionSource(&source), regionStart(start), regionSize(count) {}

    size_t size() const { return regionSource ? regionSize : bytes.size(); }
    const uint8_t* data() const {
        return regionSource ? regionSource->data() + regionStart : bytes.data();
    }
    uint8_t byte(size_t offset) const { return data()[offset]; }
    size_t sourceOffset(size_t offset) const {
        return regionSource ? regionStart + offset : sourceOffsets[offset];
    }
    uint32_t quadwordCount(size_t offset) const {
        return regionSource ? 0u : sourceQwc[offset];
    }
    uint32_t word(size_t offset) const {
        if (offset > size() || size() - offset < 4u) return 0;
        const auto* value = data() + offset;
        return uint32_t(value[0]) | (uint32_t(value[1]) << 8u) |
            (uint32_t(value[2]) << 16u) | (uint32_t(value[3]) << 24u);
    }

    void append(const std::vector<uint8_t>& source, size_t start, size_t count, uint32_t qwc) {
        bytes.insert(bytes.end(), source.begin() + static_cast<std::ptrdiff_t>(start),
            source.begin() + static_cast<std::ptrdiff_t>(start + count));
        for (size_t index = 0; index < count; ++index) {
            sourceOffsets.push_back(start + index);
            sourceQwc.push_back(qwc);
        }
    }
};

bool collectDma(const std::vector<uint8_t>& bytes, const StorylandPs2DmaRegion& region,
    StorylandDmaTlbReport& report, PacketStream& stream) {
    const size_t end = std::min(region.end, bytes.size());
    size_t cursor = region.start;
    std::array<size_t, 2> returns{};
    size_t depth = 0;
    std::set<std::array<size_t, 4>> visited;
    ++report.dmaChains;
    const auto fail = [&](size_t offset, uint32_t address, uint32_t qwc, uint32_t tag,
                          const std::string& message) {
        executionIssue(report, StorylandDmaIssueSeverity::Fatal, "DMA", offset,
            address, qwc, tag, message);
        return false;
    };
    for (uint32_t steps = 0; steps < 65536u; ++steps) {
        if ((cursor & 15u) != 0 || cursor > end || end - cursor < 16u)
            return fail(cursor, uint32_t(cursor), 0, 0, "Source-chain tag is unaligned or outside its declared geometry region.");
        if (!visited.insert({cursor, depth, returns[0], returns[1]}).second)
            return fail(cursor, uint32_t(cursor), 0, readU32(bytes, cursor), "Source chain repeats the same tag and ASR state; DMA cannot terminate.");
        const uint32_t word = readU32(bytes, cursor);
        const uint32_t addressWord = readU32(bytes, cursor + 4u);
        const uint32_t qwc = word & 0xFFFFu;
        const uint32_t id = (word >> 28u) & 7u;
        const uint32_t address = addressWord & 0x7FFFFFFFu;
        const size_t count = size_t(qwc) * 16u;
        const bool reference = id == 0u || id == 3u || id == 4u;
        const bool addressed = reference || id == 2u || id == 5u;
        if ((word & 0x03FF0000u) != 0u)
            return fail(cursor, address, qwc, word, "Reserved DMA tag bits are set.");
        if (addressed && (address & 15u) != 0u)
            return fail(cursor, address, qwc, word, "DMA source or chain target is not qword aligned.");
        if (addressed && (addressWord & 0x80000000u) != 0u) {
            if (uint64_t(address) + uint64_t(reference ? count : 16u) > 0x4000ull)
                return fail(cursor, address, qwc, word, "DMA scratchpad range exceeds 16 KiB.");
            executionIssue(report, StorylandDmaIssueSeverity::Info, "DMA", cursor, address, qwc, word,
                "Scratchpad source requires the runtime RAM image; execution is incomplete.");
            return false;
        }
        if (addressed && uint64_t(address) + uint64_t(reference ? count : 16u) > 0x02000000ull)
            return fail(cursor, address, qwc, word, "DMA source or target exceeds 32 MiB EE RAM.");
        const size_t payload = reference ? size_t(address) : cursor + 16u;
        if (payload > end || count > end - payload)
            return fail(cursor, address, qwc, word, "QWC payload exceeds the declared file-backed geometry region.");
        if (stream.bytes.size() > 16u * 1024u * 1024u - count)
            return fail(cursor, address, qwc, word, "DMA stream exceeds the bounded 16 MiB execution budget.");
        ++report.dmaTags;
        report.dmaPayloadBytes += count;
        if (region.transferTagData) stream.append(bytes, cursor + 8u, 8u, qwc);
        stream.append(bytes, payload, count, qwc);
        const size_t sequential = cursor + 16u + (reference ? 0u : count);
        if (id == 0u || id == 7u || (id == 6u && depth == 0u)) return true;
        if (id == 2u || id == 5u) {
            if (id == 5u) {
                if (depth == returns.size())
                    return fail(cursor, address, qwc, word, "DMA CALL overflows the two-level ASR stack.");
                returns[depth++] = sequential;
            }
            cursor = address;
        } else if (id == 6u) {
            cursor = returns[--depth];
            returns[depth] = 0;
        } else {
            cursor = sequential;
        }
    }
    return fail(cursor, uint32_t(cursor), 0, 0, "DMA instruction budget exhausted before chain termination.");
}

bool validVuUpper(uint32_t code) {
    const uint32_t op = code & 63u;
    if (op < 0x30u) return true;
    if (op < 0x3Cu) return false;
    const uint32_t operation = (code >> 6u) & 31u;
    if (operation < 10u) return true;
    if (operation == 10u) return op != 0x3Fu;
    return operation == 11u;
}

bool validVuLower(uint32_t code) {
    const uint32_t op = code >> 25u;
    if (op == 0u || op == 1u || op == 4u || op == 5u || op == 8u || op == 9u ||
        (op >= 0x10u && op <= 0x18u) || op == 0x1Au || op == 0x1Bu || op == 0x1Cu ||
        op == 0x20u || op == 0x21u || op == 0x24u || op == 0x25u ||
        op == 0x28u || op == 0x29u || (op >= 0x2Cu && op <= 0x2Fu)) return true;
    if (op != 0x40u) return false;
    const uint32_t function = code & 63u;
    if (function == 0x30u || function == 0x31u || function == 0x32u ||
        function == 0x34u || function == 0x35u) return true;
    if (function < 0x3Cu) return false;
    const uint32_t subgroup = function & 3u;
    const uint32_t operation = (code >> 6u) & 31u;
    if (operation == 0x0Cu) return subgroup < 2u;
    if (operation >= 0x0Du && operation <= 0x10u) return true;
    if (operation == 0x19u) return subgroup == 0u;
    if (operation == 0x1Au) return subgroup < 2u;
    if (operation == 0x1Bu) return subgroup == 0u;
    if (operation == 0x1Cu) return true;
    if (operation == 0x1Du) return subgroup < 3u;
    if (operation == 0x1Eu) return true;
    return operation == 0x1Fu && subgroup < 3u;
}

class Ps2PacketMachine {
public:
    StorylandDmaTlbReport& report;
    const StorylandPs2ValidationOptions& options;
    std::array<ExecutionWord, 4096> data{};
    std::array<uint32_t, 4096> micro{};
    std::bitset<2048> uploaded;
    std::array<std::array<ExecutionWord, 4>, 32> vf{};
    std::array<ExecutionWord, 16> vi{};
    VifState vif;
    std::array<ExecutionWord, 4> row{};
    std::array<ExecutionWord, 4> column{};
    uint32_t mask = 0;
    uint32_t mode = 0;
    uint32_t top = 0;
    uint32_t itop = 0;
    uint32_t continuation = 0;
    uint32_t currentQwc = 0;
    size_t lastDirectSource = 0;
    uint32_t lastDirectCode = 0;
    uint32_t lastDirectQwc = 0;
    uint64_t microClock = 0;
    std::array<uint64_t, 32> vectorReady{};
    int previousIntegerWrite = -1;
    ExecutionWord previousIntegerValue;
    int currentIntegerWrite = -1;
    ExecutionWord currentIntegerValue;
    bool continuationKnown = false;
    GifDecoder directGif;
    std::set<uint32_t> reportedExternalPrograms;
    std::set<uint32_t> reportedUnsupportedInstructions;

    Ps2PacketMachine(StorylandDmaTlbReport& output, const StorylandPs2ValidationOptions& settings)
        : report(output), options(settings) {
        vi[0] = {0, true};
        vf[0] = {{{0, true}, {0, true}, {0, true}, {0x3F800000u, true}}};
        for (auto& word : row) word = {0, true};
        for (auto& word : column) word = {0, true};
        report.fifoDigest = 14695981039346656037ull;
        const size_t count = settings.residentVu1Microcode.size() / 8u;
        if ((settings.residentVu1Microcode.size() & 7u) != 0u ||
            settings.residentVu1MicroAddress > 2048u || count > 2048u - settings.residentVu1MicroAddress) {
            executionIssue(report, StorylandDmaIssueSeverity::Fatal, "VU1/MPG", 0,
                settings.residentVu1MicroAddress, 0, 0, "Initial microcode is not an aligned VU1 micro-memory range.");
        } else {
            for (size_t index = 0; index < count; ++index) {
                const uint32_t address = settings.residentVu1MicroAddress + uint32_t(index);
                micro[address * 2u] = readU32(settings.residentVu1Microcode, index * 8u);
                micro[address * 2u + 1u] = readU32(settings.residentVu1Microcode, index * 8u + 4u);
                uploaded.set(address);
            }
        }
    }

    void incomplete(const char* stage, size_t source, uint32_t address, uint32_t command,
                    const std::string& message) {
        executionIssue(report, StorylandDmaIssueSeverity::Info, stage, source, address, currentQwc, command, message);
    }

    void contextualizeGifIssues(size_t first, size_t source, uint32_t qwc, uint32_t command) {
        for (size_t index = first; index < report.issues.size(); ++index) {
            auto& issue = report.issues[index];
            if (!issue.stage.empty()) continue;
            issue.stage = "GIF/FIFO";
            issue.offset = source;
            issue.qwc = qwc;
            issue.command = command;
        }
    }

    bool kick(uint32_t address, size_t source, uint32_t command) {
        ++report.vu1Xgkicks;
        GifDecoder fifo;
        std::set<uint32_t> visitedTags;
        uint32_t cursor = address & 0x3FFu;
        for (;;) {
            if (!visitedTags.insert(cursor).second) {
                executionIssue(report, StorylandDmaIssueSeverity::Fatal, "GIF/FIFO", source,
                    cursor * 16u, currentQwc, command, "XGKICK GIF chain loops through VU1 memory without an EOP tag.");
                return false;
            }
            for (uint32_t component = 0; component < 4u; ++component) {
                if (!data[cursor * 4u + component].known) {
                    incomplete("GIF/FIFO", source, cursor * 16u, command,
                        "XGKICK reads a GIFtag component supplied by unknown resident VU state.");
                    return false;
                }
            }
            const uint64_t low = uint64_t(data[cursor * 4u].value) |
                (uint64_t(data[cursor * 4u + 1u].value) << 32u);
            const uint32_t loops = uint32_t(low & 0x7FFFu);
            const uint32_t flag = uint32_t((low >> 58u) & 3u);
            uint32_t registers = uint32_t((low >> 60u) & 15u);
            if (!registers) registers = 16u;
            const uint32_t payload = flag == 0u ? loops * registers :
                flag == 1u ? (loops * registers + 1u) / 2u : loops;
            if (uint64_t(report.fifoQwordsTransferred) + payload + 1u > 0x100000ull) {
                incomplete("GIF/FIFO", source, cursor * 16u, command,
                    "XGKICK output exceeds the bounded 16 MiB resource FIFO budget; remaining output is unverified.");
                return false;
            }
            // VU1 data wraps at 16 KiB. Reading the same qwords repeatedly is
            // legal, so the packet is bounded by GIFtag fields rather than RAM size.
            std::vector<uint8_t> packet(size_t(payload + 1u) * 16u);
            for (uint32_t qword = 0; qword <= payload; ++qword) {
                const uint32_t memory = ((cursor + qword) & 0x3FFu) * 4u;
                for (uint32_t component = 0; component < 4u; ++component) {
                    const ExecutionWord word = data[memory + component];
                    if (!word.known) {
                        incomplete("GIF/FIFO", source, memory * 4u + component * 4u, command,
                            "XGKICK packet reads data not established by this execution.");
                        return false;
                    }
                    for (uint32_t byte = 0; byte < 4u; ++byte) {
                        const uint8_t value = uint8_t(word.value >> (byte * 8u));
                        packet[size_t(qword) * 16u + component * 4u + byte] = value;
                        report.fifoDigest = digestByte(report.fifoDigest, value);
                    }
                }
            }
            report.fifoQwordsTransferred += payload + 1u;
            const size_t issueStart = report.issues.size();
            if (!fifo.feed(packet.data(), packet.size(), source, report)) {
                contextualizeGifIssues(issueStart, source, currentQwc, command);
                return false;
            }
            if ((low & 0x8000u) != 0u) {
                fifo.finish(report, source);
                contextualizeGifIssues(issueStart, source, currentQwc, command);
                return report.safe();
            }
            cursor = (cursor + payload + 1u) & 0x3FFu;
        }
    }

    bool executeMicro(uint32_t entry, size_t source) {
        ++report.vu1MicroCalls;
        if (!uploaded.test(entry & 0x7FFu)) {
            ++report.vu1MicroCallsExternal;
            if (reportedExternalPrograms.insert(entry).second)
                incomplete("VU1", source, entry * 8u, 0,
                    "MSCAL/MSCNT entry depends on resident microcode absent from this resource; VU output is unverified.");
            invalidateMicroState();
            return false;
        }
        ++report.vu1MicroCallsWithResidentCode;
        uint32_t pc = entry & 0x7FFu;
        previousIntegerWrite = -1;
        microClock += 4u;
        uint32_t ending = 0;
        bool branchPending = false;
        uint32_t branchTarget = 0;
        for (uint32_t instruction = 0; instruction < options.maximumVu1Instructions; ++instruction) {
            if (!uploaded.test(pc)) {
                incomplete("VU1", source, pc * 8u, 0,
                    "Microprogram execution reaches code outside the uploaded micro-memory image.");
                invalidateMicroState();
                return false;
            }
            const uint32_t lower = micro[pc * 2u];
            const uint32_t upper = micro[pc * 2u + 1u];
            ++report.vu1InstructionsExecuted;
            if (!validVuUpper(upper) || ((upper & 0x80000000u) == 0u && !validVuLower(lower))) {
                executionIssue(report, StorylandDmaIssueSeverity::Fatal, "VU1", source, pc * 8u,
                    currentQwc, !validVuUpper(upper) ? upper : lower, "Reserved VU1 micro-instruction opcode reached during execution.");
                return false;
            }
            // Functional packet execution intentionally stops before arithmetic or
            // flag/pipeline instructions whose full VU timing is not implemented.
            if ((upper & 0x07FFFFFFu) != 0x000002FFu) {
                unsupported(source, pc, upper, "upper arithmetic/flag instruction");
                return false;
            }
            ++microClock;
            currentIntegerWrite = -1;
            const bool oldBranch = branchPending;
            const uint32_t oldTarget = branchTarget;
            branchPending = false;
            if ((upper & 0x80000000u) == 0u &&
                !executeLower(lower, source, pc, branchPending, branchTarget)) return false;
            previousIntegerWrite = currentIntegerWrite;
            previousIntegerValue = currentIntegerValue;
            if (oldBranch && branchPending) {
                unsupported(source, pc, lower, "branch in a branch delay slot");
                return false;
            }
            const bool finishing = ending == 1u;
            if (ending) --ending;
            if ((upper & 0x40000000u) != 0u) ending = 1u;
            pc = oldBranch ? oldTarget : ((pc + 1u) & 0x7FFu);
            if (finishing) {
                continuation = pc;
                continuationKnown = true;
                ++report.vu1ProgramsCompleted;
                return true;
            }
        }
        executionIssue(report, StorylandDmaIssueSeverity::Fatal, "VU1", source, pc * 8u,
            currentQwc, micro[pc * 2u], "VU1 execution budget exhausted without reaching an E-bit delay-slot termination.");
        return false;
    }

    void unsupported(size_t source, uint32_t pc, uint32_t code, const char* detail) {
        ++report.vu1UnsupportedInstructions;
        if (reportedUnsupportedInstructions.insert(code).second)
            incomplete("VU1", source, pc * 8u, code,
                std::string("Unsupported ") + detail + "; execution is incomplete, not a hardware pass.");
        invalidateMicroState();
    }

    void invalidateMicroState() {
        for (auto& word : data) word.known = false;
        for (size_t index = 1; index < vi.size(); ++index) vi[index].known = false;
        for (size_t index = 1; index < vf.size(); ++index)
            for (auto& word : vf[index]) word.known = false;
        continuationKnown = false;
    }

    bool executeLower(uint32_t code, size_t source, uint32_t pc,
                      bool& branch, uint32_t& target) {
        const uint32_t op = code >> 25u;
        const uint32_t ft = (code >> 16u) & 31u;
        const uint32_t fs = (code >> 11u) & 31u;
        const uint32_t fd = (code >> 6u) & 31u;
        const uint32_t it = ft & 15u;
        const uint32_t is = fs & 15u;
        const uint32_t id = fd & 15u;
        const int32_t immediate = (code & 0x400u) ? int32_t(code & 0x7FFu) - 0x800 : int32_t(code & 0x7FFu);
        const auto enabled = [&](uint32_t component) { return (code & (1u << (24u - component))) != 0u; };
        const auto writeInteger = [&](uint32_t reg, ExecutionWord word) {
            if (reg) {
                currentIntegerWrite = int(reg);
                currentIntegerValue = vi[reg];
                word.value &= 0xFFFFu;
                vi[reg] = word;
            }
        };
        const auto requireAddress = [&](uint32_t reg) {
            if (vi[reg].known) return true;
            incomplete("VU1", source, pc * 8u, code, "Memory/branch address depends on unknown resident VI state.");
            invalidateMicroState();
            return false;
        };
        const auto requireVector = [&](uint32_t reg) {
            if (microClock >= vectorReady[reg]) return true;
            unsupported(source, pc, code, "VF dependency inside an unmodeled VU load pipeline latency");
            return false;
        };
        const auto branchInteger = [&](uint32_t reg) {
            return int(reg) == previousIntegerWrite ? previousIntegerValue : vi[reg];
        };
        if (op == 8u || op == 9u) {
            const uint32_t amount = ((code >> 10u) & 0x7800u) | (code & 0x7FFu);
            writeInteger(it, {op == 8u ? vi[is].value + amount : vi[is].value - amount, vi[is].known});
            return true;
        }
        if (op == 0u || op == 1u || op == 4u || op == 5u) {
            const uint32_t base = op == 1u ? it : is;
            if (!requireAddress(base)) return false;
            if (op == 1u && !requireVector(fs)) return false;
            const uint32_t address = (vi[base].value + uint32_t(immediate)) & 0x3FFu;
            for (uint32_t component = 0; component < 4u; ++component) {
                if (!enabled(component)) continue;
                ExecutionWord& memory = data[address * 4u + component];
                if (op == 0u && ft) {
                    vf[ft][component] = memory;
                    vectorReady[ft] = microClock + 4u;
                }
                else if (op == 1u) memory = vf[fs][component];
                else if (op == 4u) writeInteger(it, memory);
                else if (op == 5u) memory = {vi[it].value & 0xFFFFu, vi[it].known};
            }
            return true;
        }
        if (op == 0x20u || op == 0x21u || op == 0x24u || op == 0x25u ||
            op == 0x28u || op == 0x29u || (op >= 0x2Cu && op <= 0x2Fu)) {
            bool take = true;
            if (op == 0x24u || op == 0x25u) {
                const ExecutionWord sourceValue = branchInteger(is);
                if (!sourceValue.known) {
                    incomplete("VU1", source, pc * 8u, code, "JR/JALR depends on unknown delayed VI state.");
                    invalidateMicroState();
                    return false;
                }
                target = sourceValue.value & 0x7FFu;
            } else {
                target = (pc + 1u + uint32_t(immediate)) & 0x7FFu;
                if (op >= 0x28u) {
                    const ExecutionWord sourceValue = branchInteger(is);
                    const ExecutionWord targetValue = branchInteger(it);
                    if (!sourceValue.known || ((op == 0x28u || op == 0x29u) && !targetValue.known)) {
                        incomplete("VU1", source, pc * 8u, code, "Conditional branch depends on unknown delayed VI state.");
                        invalidateMicroState();
                        return false;
                    }
                    const int16_t value = int16_t(sourceValue.value);
                    if (op == 0x28u) take = uint16_t(sourceValue.value) == uint16_t(targetValue.value);
                    else if (op == 0x29u) take = uint16_t(sourceValue.value) != uint16_t(targetValue.value);
                    else if (op == 0x2Cu) take = value < 0;
                    else if (op == 0x2Du) take = value > 0;
                    else if (op == 0x2Eu) take = value <= 0;
                    else take = value >= 0;
                }
            }
            if (op == 0x21u || op == 0x25u) writeInteger(it, {(pc + 2u) & 0x7FFu, true});
            branch = take;
            return true;
        }
        if (op == 0x40u) {
            const uint32_t function = code & 63u;
            if (function == 0x30u || function == 0x31u || function == 0x34u || function == 0x35u) {
                uint32_t value = 0;
                if (function == 0x30u) value = vi[is].value + vi[it].value;
                else if (function == 0x31u) value = vi[is].value - vi[it].value;
                else if (function == 0x34u) value = vi[is].value & vi[it].value;
                else value = vi[is].value | vi[it].value;
                writeInteger(id, {value, vi[is].known && vi[it].known});
                return true;
            }
            if (function == 0x32u) {
                const int32_t amount = (fd & 16u) ? int32_t(fd) - 32 : int32_t(fd);
                writeInteger(it, {vi[is].value + uint32_t(amount), vi[is].known});
                return true;
            }
            if (function >= 0x3Cu) {
                const uint32_t subgroup = function & 3u;
                if (fd == 0x0Cu && subgroup < 2u) {
                    if (!requireVector(fs)) return false;
                    const auto original = vf[fs];
                    for (uint32_t component = 0; component < 4u; ++component)
                        if (ft && enabled(component)) vf[ft][component] = original[(component + subgroup) & 3u];
                    return true;
                }
                if (fd == 0x0Fu && subgroup == 0u) {
                    if (!requireVector(fs)) return false;
                    writeInteger(it, vf[fs][(code >> 21u) & 3u]);
                    return true;
                }
                if (fd == 0x0Fu && subgroup == 1u) {
                    for (uint32_t component = 0; component < 4u; ++component)
                        if (ft && enabled(component)) vf[ft][component] = {uint32_t(int32_t(int16_t(vi[is].value))), vi[is].known};
                    return true;
                }
                if (fd == 0x1Au && subgroup < 2u) {
                    writeInteger(it, {subgroup == 0u ? top : itop, true});
                    return true;
                }
                if (fd == 0x1Bu && subgroup == 0u) {
                    if (!requireAddress(is)) return false;
                    return kick(vi[is].value, source, code);
                }
                if (fd == 0x0Eu && subgroup == 3u) return true; // WAITQ: no Q-producing operation is supported.
            }
        }
        unsupported(source, pc, code, "lower VU instruction");
        return false;
    }

    bool executeVif(const PacketStream& stream) {
        ++report.vifStreams;
        size_t cursor = 0;
        while (cursor < stream.size()) {
            const size_t source = stream.sourceOffset(cursor);
            const uint32_t qwc = stream.quadwordCount(cursor);
            currentQwc = qwc;
            if (stream.size() - cursor < 4u) {
                executionIssue(report, StorylandDmaIssueSeverity::Fatal, "VIF1", source, 0, qwc, 0, "FIFO ends inside a VIFcode word.");
                return false;
            }
            const uint32_t code = stream.word(cursor);
            const uint32_t command = (code >> 24u) & 0x7Fu;
            VifPayloadInfo payload;
            if (!vifPayloadInfo(code, vif, payload)) {
                executionIssue(report, StorylandDmaIssueSeverity::Fatal, "VIF1", source, code & 0xFFFFu, qwc, code,
                    command >= 0x60u ? "Reserved VIF UNPACK format." : "Unknown or reserved VIF1 command.");
                return false;
            }
            cursor += 4u;
            ++report.vifCommands;
            if (payload.bytes > stream.size() - cursor) {
                executionIssue(report, StorylandDmaIssueSeverity::Fatal, "VIF1/FIFO", source,
                    code & 0x3FFu, qwc, code, "VIFcode payload requires " + std::to_string(payload.bytes) +
                    " bytes, but only " + std::to_string(stream.size() - cursor) + " DMA FIFO bytes remain.");
                return false;
            }
            if (command == 1u) {
                vif.cycleCl = (code & 255u) ? code & 255u : 256u;
                vif.cycleWl = ((code >> 8u) & 255u) ? (code >> 8u) & 255u : 256u;
            } else if (command == 2u) {
                vif.offset = code & 0x3FFu;
                vif.tops = vif.base;
                vif.dbf = false;
            } else if (command == 3u) vif.base = code & 0x3FFu;
            else if (command == 4u) vif.itops = code & 0x3FFu;
            else if (command == 5u) mode = code & 3u;
            else if (command == 0x20u) mask = stream.word(cursor);
            else if (command == 0x30u || command == 0x31u) {
                auto& registers = command == 0x30u ? row : column;
                for (uint32_t component = 0; component < 4u; ++component)
                    registers[component] = {stream.word(cursor + component * 4u), true};
            } else if (command == 0x4Au) {
                const uint32_t start = code & 0x7FFu;
                const uint32_t count = ((code >> 16u) & 255u) ? (code >> 16u) & 255u : 256u;
                for (uint32_t index = 0; index < count; ++index) {
                    const uint32_t address = (start + index) & 0x7FFu;
                    micro[address * 2u] = stream.word(cursor + index * 8u);
                    micro[address * 2u + 1u] = stream.word(cursor + index * 8u + 4u);
                    uploaded.set(address);
                }
                report.vu1MicroInstructionsUploaded += count;
            } else if (command == 0x14u || command == 0x15u || command == 0x17u) {
                top = vif.tops;
                itop = vif.itops;
                updateVifTopAfterMicroprogram(vif);
                if (command == 0x17u && !continuationKnown) {
                    ++report.vu1MicroCalls;
                    ++report.vu1MicroCallsExternal;
                    if (reportedExternalPrograms.insert(0xFFFFFFFFu).second)
                        incomplete("VU1", source, 0, code, "MSCNT requires an unavailable previous VU1 continuation PC.");
                } else executeMicro(command == 0x17u ? continuation : code & 0x7FFu, source);
                if (!report.safe()) return false;
            } else if (payload.unpack) {
                unpack(stream, cursor, code, payload);
                ++report.vifUnpacks;
            } else if (command == 0x50u || command == 0x51u) {
                ++report.directTransfers;
                report.fifoQwordsTransferred += uint32_t(payload.bytes / 16u);
                for (size_t byte = 0; byte < payload.bytes; ++byte)
                    report.fifoDigest = digestByte(report.fifoDigest, stream.byte(cursor + byte));
                lastDirectSource = source;
                lastDirectCode = code;
                lastDirectQwc = qwc;
                const size_t issueStart = report.issues.size();
                if (!directGif.feed(stream.data() + cursor, payload.bytes, stream.sourceOffset(cursor), report)) {
                    contextualizeGifIssues(issueStart, source, qwc, code);
                    return false;
                }
            }
            cursor += payload.bytes;
        }
        const size_t issueStart = report.issues.size();
        directGif.finish(report, stream.size() == 0u ? 0u : stream.sourceOffset(stream.size() - 1u));
        contextualizeGifIssues(issueStart, lastDirectSource, lastDirectQwc, lastDirectCode);
        return report.safe();
    }

    void unpack(const PacketStream& stream, size_t payload, uint32_t code, const VifPayloadInfo& info) {
        const uint32_t command = (code >> 24u) & 0x7Fu;
        const uint32_t components = ((command >> 2u) & 3u) + 1u;
        const uint32_t format = command & 3u;
        const uint32_t componentBytes = format == 3u ? 0u : 4u >> format;
        const uint32_t vectorBytes = format == 3u ? 2u : componentBytes * components;
        const bool unsignedValues = (code & 0x4000u) != 0u;
        size_t input = payload;
        uint32_t cycle = 0;
        uint32_t address = info.unpackBase;
        for (uint32_t output = 0; output < info.unpackWriteCount; ++output) {
            std::array<ExecutionWord, 4> values{};
            if (format == 3u && input + 2u <= stream.size()) {
                const uint16_t packed = uint16_t(stream.byte(input)) | uint16_t(stream.byte(input + 1u)) << 8u;
                values = {{{uint32_t((packed & 31u) << 3u), true},
                    {uint32_t((packed >> 5u) & 31u) << 3u, true},
                    {uint32_t((packed >> 10u) & 31u) << 3u, true},
                    {uint32_t((packed >> 15u) & 1u) << 7u, true}}};
            } else if (format != 3u) {
                for (uint32_t component = 0; component < 4u; ++component) {
                    const uint32_t sourceComponent = components == 1u ? 0u : components == 2u ? component & 1u : component;
                    const size_t offset = input + sourceComponent * componentBytes;
                    if (offset > stream.size() || componentBytes > stream.size() - offset) continue;
                    uint32_t value = 0;
                    for (uint32_t byte = 0; byte < componentBytes; ++byte)
                        value |= uint32_t(stream.byte(offset + byte)) << (byte * 8u);
                    if (!unsignedValues && componentBytes < 4u) {
                        const uint32_t bits = componentBytes * 8u;
                        if ((value & (1u << (bits - 1u))) != 0u) value |= 0xFFFFFFFFu << bits;
                    }
                    values[component] = {value, true};
                }
            }
            for (uint32_t component = 0; component < 4u; ++component) {
                const uint32_t selector = (command & 0x10u) ?
                    (mask >> (std::min(cycle, 3u) * 8u + component * 2u)) & 3u : 0u;
                if (selector == 3u) continue;
                ExecutionWord value = values[component];
                if (selector == 1u) value = row[component];
                else if (selector == 2u) value = column[std::min(cycle, 3u)];
                else if (format != 3u) {
                    if (mode == 1u || mode == 2u) {
                        value = {value.value + row[component].value, value.known && row[component].known};
                        if (mode == 2u) row[component] = value;
                    } else if (mode == 3u) row[component] = value;
                }
                data[(address & 0x3FFu) * 4u + component] = value;
            }
            ++report.vu1DataQwordsWritten;
            ++address;
            ++cycle;
            if (vif.cycleCl < vif.cycleWl) {
                if (cycle <= vif.cycleCl) input += vectorBytes;
                else if (cycle == vif.cycleWl) cycle = 0;
            } else {
                input += vectorBytes;
                if (cycle == vif.cycleWl) {
                    address += vif.cycleCl - vif.cycleWl;
                    cycle = 0;
                }
            }
        }
    }
};

bool nativeAtomic(uint32_t word) {
    return word == 0x0004AA01u || word == 0x0000AA01u || word == 0x01050001u ||
        word == 0x01000001u || (word & 0xFFFF00FFu) == 0x00040001u;
}

std::vector<StorylandPs2DmaRegion> nativeModelRegions(const std::vector<uint8_t>& bytes,
    StorylandDmaTlbReport& report) {
    std::vector<StorylandPs2DmaRegion> regions;
    if (bytes.size() < 0x60u || readU32(bytes, 0) != 0x006D646Cu) return regions;
    std::set<size_t> pointerLocations;
    const size_t relocationTable = readU32(bytes, 0x10u);
    const uint32_t relocationCount = readU32(bytes, 0x14u);
    if (relocationCount && relocationTable <= bytes.size() &&
        size_t(relocationCount) <= (bytes.size() - relocationTable) / 4u) {
        for (uint32_t index = 0; index < relocationCount; ++index)
            pointerLocations.insert(readU32(bytes, relocationTable + size_t(index) * 4u));
    }
    std::set<size_t> geometrySeen;
    for (size_t atomic = 0x20u; atomic <= bytes.size() && bytes.size() - atomic >= 0x38u; atomic += 4u) {
        if (!nativeAtomic(readU32(bytes, atomic))) continue;
        if (!pointerLocations.empty() && pointerLocations.count(atomic + 0x14u) == 0u) continue;
        const size_t geometry = readU32(bytes, atomic + 0x14u);
        if (geometry == 0u || geometry > bytes.size() || bytes.size() - geometry < 0x60u) continue;
        const uint32_t materials = readU32(bytes, geometry + 0x10u);
        const size_t materialTable = readU32(bytes, geometry + 0x0Cu);
        if (!materials || materials > 512u || materialTable == 0u ||
            materialTable > bytes.size() || size_t(materials) * 4u > bytes.size() - materialTable) continue;
        const size_t global = geometry + 0x20u;
        const uint32_t packed = readU32(bytes, global + 0x10u);
        const size_t size = packed & 0xFFFFFu;
        const uint32_t parts = packed >> 20u;
        const uint32_t first = readU32(bytes, global + 0x18u) >> 16u;
        // The PSP header starts with a byte size and strip count; PS2 starts
        // with a bounding sphere. Test the native PSP layout before PS2 fields.
        const uint32_t pspSize = readU32(bytes, global);
        const uint32_t pspStrips = readU32(bytes, global + 8u);
        const uint32_t pspVertices = bytes.size() - global >= 0x44u ? readU32(bytes, global + 0x40u) : 0u;
        if (pspSize >= 0x48u && pspSize <= 0x10000000u &&
            pspStrips > 0u && pspStrips <= 512u && 0x48u + size_t(pspStrips) * 0x30u <= pspSize &&
            pspVertices >= 0x48u + size_t(pspStrips) * 0x30u && pspVertices < pspSize) continue;
        if (!parts || !first) continue;
        if (!geometrySeen.insert(geometry).second) continue;
        if (size > bytes.size() - global || first < 0x40u + size_t(parts) * 0x30u ||
            first >= size || (first & 15u) != 0u) {
            executionIssue(report, StorylandDmaIssueSeverity::Fatal, "DMA/layout", global + 0x10u,
                uint32_t(global), 0, packed, "Declared PS2 geometry part table or DMA block range is invalid.");
            continue;
        }
        for (uint32_t index = 0; index < parts; ++index) {
            const size_t row = global + 0x40u + size_t(index) * 0x30u;
            const uint32_t relative = readU32(bytes, row + 0x1Cu);
            const uint32_t material = readU32(bytes, row + 0x20u) >> 16u;
            if (relative >= size - first || (relative & 15u) != 0u || material >= materials) {
                executionIssue(report, StorylandDmaIssueSeverity::Fatal, "DMA/layout", row + 0x1Cu,
                    relative, 0, 0, "Geometry part points outside its DMA block or material table.");
                continue;
            }
            regions.push_back({global + first + relative, global + size, false});
            ++report.declaredDmaRoots;
        }
    }
    std::sort(regions.begin(), regions.end(), [](const StorylandPs2DmaRegion& left, const StorylandPs2DmaRegion& right) {
        return left.start < right.start;
    });
    regions.erase(std::unique(regions.begin(), regions.end(), [](const StorylandPs2DmaRegion& left, const StorylandPs2DmaRegion& right) {
        return left.start == right.start && left.end == right.end;
    }), regions.end());
    return regions;
}

} // namespace

std::string StorylandDmaTlbReport::firstError() const {
    for (const auto& issue : issues) {
        if (issue.severity != StorylandDmaIssueSeverity::Fatal) continue;
        std::ostringstream out;
        out << "PS2 DMA/VIF/VU/FIFO [" << (issue.stage.empty() ? "GIF/FIFO" : issue.stage) << "]"
            << " error: " << label << "\nfile+" << hexValue(issue.offset)
            << " address=" << hexValue(issue.address) << " QWC=" << issue.qwc
            << " command=" << hexValue(issue.command) << "\n" << issue.message;
        return out.str();
    }
    return {};
}

std::string StorylandDmaTlbReport::text() const {
    std::ostringstream out;
    out << "PS2 DMA / VIF1 / VU1 / GIF FIFO execution test\r\n\r\n"
        << "Target: " << label << "\r\nBytes inspected: " << byteSize
        << "\r\nDeclared native DMA roots: " << declaredDmaRoots
        << "\r\nDMA tags / chains: " << dmaTags << " / " << dmaChains
        << "\r\nDMA payload bytes: " << dmaPayloadBytes
        << "\r\nVIF streams / commands / UNPACKs: " << vifStreams << " / " << vifCommands << " / " << vifUnpacks
        << "\r\nUNPACK qwords written: " << vu1DataQwordsWritten
        << "\r\nMPG microinstructions uploaded: " << vu1MicroInstructionsUploaded
        << "\r\nVU1 calls / resident / external: " << vu1MicroCalls << " / " << vu1MicroCallsWithResidentCode << " / " << vu1MicroCallsExternal
        << "\r\nVU1 instructions executed / programs completed: " << vu1InstructionsExecuted << " / " << vu1ProgramsCompleted
        << "\r\nUnsupported VU1 instructions: " << vu1UnsupportedInstructions
        << "\r\nXGKICKs / DIRECT transfers: " << vu1Xgkicks << " / " << directTransfers
        << "\r\nFIFO qwords transferred: " << fifoQwordsTransferred
        << "\r\nGIF tags / completed packets: " << gifTags << " / " << gifPackets
        << "\r\nGIF payload bytes: " << gifPayloadBytes
        << "\r\nGIF PACKED / REGLIST registers: " << gifPackedRegisters << " / " << gifReglistRegisters
        << "\r\nGS A+D/REGLIST writes / vertex kicks: " << gsRegisterWrites << " / " << gsPrimitiveKicks
        << "\r\nIncomplete stages: " << incompleteStages
        << "\r\nWarnings / fatal errors: " << warnings << " / " << fatals << "\r\n\r\n";
    if (!safe()) out << "RESULT: FAIL - decoded execution encountered an invalid DMA/VIF/VU/FIFO result.\r\n";
    else if (!complete()) out << "RESULT: LIMITED - decoded packet execution passed; unavailable resident state or unsupported stages prevent full VU/FIFO validation.\r\n";
    else out << "RESULT: PASS - all supplied packet instructions completed within the implemented execution subset.\r\n";
    out << "This is a bounded packet interpreter, not a complete PS2 emulator or a cycle-accurate GS renderer.\r\n";
    for (const auto& issue : issues) {
        const char* severity = issue.severity == StorylandDmaIssueSeverity::Fatal ? "FATAL" :
            issue.severity == StorylandDmaIssueSeverity::Warning ? "WARN" : "LIMIT";
        out << severity << " [" << (issue.stage.empty() ? "GIF/FIFO" : issue.stage)
            << "] file+" << hexValue(issue.offset) << " address=" << hexValue(issue.address)
            << " QWC=" << issue.qwc << " command=" << hexValue(issue.command) << " | " << issue.message << "\r\n";
    }
    return out.str();
}

static StorylandPs2ExecutionResult storylandExecutePs2DmaTlbImpl(const std::vector<uint8_t>& bytes,
    const std::string& label, const StorylandPs2ValidationOptions& options, bool computeVu1Digest) {
    StorylandPs2ExecutionResult result;
    StorylandDmaTlbReport& report = result.report;
    report.label = label;
    report.byteSize = bytes.size();
    if (bytes.empty()) {
        executionIssue(report, StorylandDmaIssueSeverity::Fatal, "DMA", 0, 0, 0, 0, "Resource is empty.");
        return result;
    }
    Ps2PacketMachine machine(report, options);
    if (!report.safe()) return result;
    std::vector<StorylandPs2DmaRegion> regions = options.dmaRegions;
    if (regions.empty() && options.vifRegions.empty()) regions = nativeModelRegions(bytes, report);
    for (const auto& region : regions) {
        PacketStream stream;
        if (collectDma(bytes, region, report, stream) && report.safe()) machine.executeVif(stream);
        if (!report.safe()) break;
    }
    if (report.safe()) {
        for (const auto& region : options.vifRegions) {
            if (region.start > region.end || region.end > bytes.size()) {
                executionIssue(report, StorylandDmaIssueSeverity::Fatal, "VIF1", region.start, 0, 0, 0,
                    "Explicit VIF range exceeds the supplied resource.");
                break;
            }
            const PacketStream stream(bytes, region.start, region.end - region.start);
            machine.executeVif(stream);
            if (!report.safe()) break;
        }
    }
    if (regions.empty() && options.vifRegions.empty() && report.safe()) {
        executionIssue(report, StorylandDmaIssueSeverity::Info, "DMA", 0, 0, 0, 0,
            "No declared native PS2 packet roots were found. Arbitrary data is not scanned or promoted to executable DMA.");
    }
    if (computeVu1Digest) report.vu1DataDigest = 14695981039346656037ull;
    if (computeVu1Digest || options.captureVu1Data) for (const auto& word : machine.data) {
        if (computeVu1Digest) {
            report.vu1DataDigest = digestByte(report.vu1DataDigest, word.known ? 1u : 0u);
            for (uint32_t byte = 0; byte < 4u; ++byte)
                report.vu1DataDigest = digestByte(report.vu1DataDigest, uint8_t(word.value >> (byte * 8u)));
        }
        if (options.captureVu1Data) {
            result.vu1Data.push_back(word.value);
            result.vu1DataKnown.push_back(word.known ? 1u : 0u);
        }
    }
    return result;
}

StorylandPs2ExecutionResult storylandExecutePs2DmaTlb(const std::vector<uint8_t>& bytes,
    const std::string& label, const StorylandPs2ValidationOptions& options) {
    return storylandExecutePs2DmaTlbImpl(bytes, label, options, true);
}

StorylandDmaTlbReport storylandValidatePs2DmaTlb(const std::vector<uint8_t>& bytes, const std::string& label) {
    return storylandExecutePs2DmaTlb(bytes, label, StorylandPs2ValidationOptions{}).report;
}

StorylandDmaTlbReport storylandValidatePs2WorldResource(const std::vector<uint8_t>& bytes,
    size_t start, size_t end, uint32_t materialRowLength, const std::string& label, bool computeVu1Digest) {
    StorylandDmaTlbReport report;
    report.label = label;
    report.byteSize = end >= start ? end - start : 0;
    const auto fail = [&](size_t offset, const std::string& message) {
        executionIssue(report, StorylandDmaIssueSeverity::Fatal, "VIF1/layout", offset,
            uint32_t(offset), 0, 0, message);
        return report;
    };
    if (start > end || end > bytes.size() || end - start < 4u)
        return fail(start, "World resource descriptor is outside its file-backed range.");
    const uint32_t header = readU32(bytes, start);
    const uint32_t count = header & 0xFFFFu;
    const uint32_t descriptorBytes = header >> 16u;
    if (!count) {
        executionIssue(report, StorylandDmaIssueSeverity::Info, "VIF1/layout", start, 0, 0, 0,
            "Empty world geometry has no packet program to execute.");
        return report;
    }
    if ((materialRowLength != 22u && materialRowLength != 24u) ||
        descriptorBytes < size_t(count) * materialRowLength || descriptorBytes > 0x4000u ||
        descriptorBytes > end - start - 4u)
        return fail(start, "Declared world material table exceeds its resource range.");
    const uint32_t alignedDescriptor = ((4u + count * materialRowLength + 15u) & ~15u) - 4u;
    if (descriptorBytes != alignedDescriptor && descriptorBytes != count * materialRowLength)
        return fail(start, "World material count, descriptor size, and row stride disagree.");
    uint64_t budget = 0;
    for (uint32_t index = 0; index < count; ++index) {
        const size_t row = start + 4u + size_t(index) * materialRowLength;
        const uint32_t packetBytes = materialRowLength == 24u ? readU32(bytes, row) >> 1u :
            (readU32(bytes, row) >> 16u) & 0x7FFFu;
        if (!packetBytes || packetBytes > 0x400000u)
            return fail(row, "World material declares an empty or excessive VIF packet budget.");
        budget += packetBytes;
        if (budget > 0x02000000u) return fail(row, "World VIF packet budgets exceed 32 MiB.");
    }
    size_t stream = start + 4u + descriptorBytes;
    const size_t paddingStart = stream;
    while (stream < end && bytes[stream] == 0xAAu && stream - paddingStart < 0x1000u) ++stream;
    stream = align4(stream);
    if (stream > end || budget > uint64_t(end - stream))
        return fail(stream, "Declared material packet bytes extend past the world resource.");
    StorylandPs2ValidationOptions options;
    options.vifRegions.push_back({stream, stream + size_t(budget), false});
    auto result = storylandExecutePs2DmaTlbImpl(bytes, label, options, computeVu1Digest).report;
    result.byteSize = end - start;
    return result;
}

StorylandDmaTlbReport storylandValidatePs2WorldResource(const std::vector<uint8_t>& bytes,
    size_t start, size_t end, uint32_t materialRowLength, const std::string& label) {
    return storylandValidatePs2WorldResource(bytes, start, end, materialRowLength, label, true);
}
