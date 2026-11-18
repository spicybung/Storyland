#include "src/storyland/storyland_scm_workbench.h"
#include "src/storyland/storyland_atomic_io.h"
#include "vendors/nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
#include "src/leeds/leeds_scm_definitions.inc"

constexpr size_t maximumScmBytes = 256u * 1024u * 1024u;
constexpr size_t maximumPatchBytes = 16u * 1024u * 1024u;

std::string trim(std::string value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    return value.substr(begin, value.find_last_not_of(" \t\r\n") - begin + 1);
}

std::string lower(std::string value) {
    for (char& character : value) character = char(std::tolower(static_cast<unsigned char>(character)));
    return value;
}

bool containsLowercase(const std::string& text, const std::string& needle) {
    return std::search(text.begin(),text.end(),needle.begin(),needle.end(),
        [](unsigned char left, char right) { return char(std::tolower(left)) == right; }) != text.end();
}

std::string hexNumber(uint64_t value, unsigned width = 0) {
    std::ostringstream output;
    output << "0x" << std::uppercase << std::hex << std::setw(int(width)) << std::setfill('0') << value;
    return output.str();
}

std::string hexBytes(const std::vector<uint8_t>& bytes, size_t begin, size_t end, bool spaces) {
    std::ostringstream output;
    output << std::hex << std::uppercase << std::setfill('0');
    for (size_t index = begin; index < end; ++index) {
        if (spaces && index > begin) output << ' ';
        output << std::setw(2) << unsigned(bytes[index]);
    }
    return output.str();
}

uint32_t rotateRight(uint32_t value, unsigned shift) {
    return (value >> shift) | (value << (32 - shift));
}

std::string sha256(const std::vector<uint8_t>& bytes) {
    constexpr std::array<uint32_t,64> constants = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    std::array<uint32_t,8> state = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    const size_t blockCount = (bytes.size() + 9 + 63) / 64;
    const uint64_t bitLength = uint64_t(bytes.size()) * 8;
    for (size_t block = 0; block < blockCount; ++block) {
        std::array<uint32_t,64> words{};
        for (size_t index = 0; index < 64; ++index) {
            const size_t position = block * 64 + index;
            uint8_t value = 0;
            if (position < bytes.size()) value = bytes[position];
            else if (position == bytes.size()) value = 0x80;
            else if (position >= blockCount * 64 - 8)
                value = uint8_t(bitLength >> ((blockCount * 64 - 1 - position) * 8));
            words[index / 4] |= uint32_t(value) << (24 - (index % 4) * 8);
        }
        for (size_t index = 16; index < words.size(); ++index) {
            const uint32_t first = rotateRight(words[index - 15],7) ^ rotateRight(words[index - 15],18) ^ (words[index - 15] >> 3);
            const uint32_t second = rotateRight(words[index - 2],17) ^ rotateRight(words[index - 2],19) ^ (words[index - 2] >> 10);
            words[index] = words[index - 16] + first + words[index - 7] + second;
        }
        auto value = state;
        for (size_t index = 0; index < words.size(); ++index) {
            const uint32_t sum = rotateRight(value[4],6) ^ rotateRight(value[4],11) ^ rotateRight(value[4],25);
            const uint32_t choice = (value[4] & value[5]) ^ (~value[4] & value[6]);
            const uint32_t first = value[7] + sum + choice + constants[index] + words[index];
            const uint32_t sum2 = rotateRight(value[0],2) ^ rotateRight(value[0],13) ^ rotateRight(value[0],22);
            const uint32_t majority = (value[0] & value[1]) ^ (value[0] & value[2]) ^ (value[1] & value[2]);
            value = {first + sum2 + majority,value[0],value[1],value[2],value[3] + first,value[4],value[5],value[6]};
        }
        for (size_t index = 0; index < state.size(); ++index) state[index] += value[index];
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (uint32_t value : state) output << std::setw(8) << value;
    return output.str();
}

float floatBits(int32_t bits) {
    float value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string floatText(int32_t bits) {
    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<float>::max_digits10) << floatBits(bits);
    return output.str();
}

const StorylandScmDefinition* definitionFor(StorylandScmDialect dialect, uint16_t opcode) {
    const auto* begin = dialect == StorylandScmDialect::Vcs ? std::begin(vcsDefinitions) : std::begin(lcsDefinitions);
    const auto* end = dialect == StorylandScmDialect::Vcs ? std::end(vcsDefinitions) : std::end(lcsDefinitions);
    const auto* found = std::lower_bound(begin, end, opcode,
        [](const StorylandScmDefinition& item, uint16_t value) { return item.opcode < value; });
    return found != end && found->opcode == opcode ? found : nullptr;
}

char parameterType(const std::string& definition, size_t index) {
    const std::string prefix = "%" + std::to_string(index + 1);
    size_t at = 0;
    while ((at = definition.find(prefix, at)) != std::string::npos) {
        const size_t type = at + prefix.size();
        if (type < definition.size() && !std::isdigit(static_cast<unsigned char>(definition[type])))
            return definition[type];
        at = type;
    }
    return 'd';
}

bool functionCommand(StorylandScmDialect dialect, uint16_t opcode) {
    return dialect == StorylandScmDialect::Vcs ? opcode == 0x037A || opcode == 0x037B :
        opcode == 0x05AE || opcode == 0x05AF;
}

bool branchOperand(StorylandScmDialect dialect, uint16_t opcode, size_t index) {
    if (functionCommand(dialect, opcode)) return index == 3;
    if (index != 0) return false;
    if (opcode == 0x0002) return true;
    if (dialect == StorylandScmDialect::Vcs)
        return opcode == 0x0021 || opcode == 0x0022 || opcode == 0x0024 || opcode == 0x0025 ||
            opcode == 0x0079 || opcode == 0x01BA;
    return opcode == 0x004C || opcode == 0x004D || opcode == 0x004F || opcode == 0x0050 ||
        opcode == 0x00DC || opcode == 0x02D2;
}

bool referenceKind(StorylandScmOperandKind kind) {
    return kind == StorylandScmOperandKind::Global || kind == StorylandScmOperandKind::Local ||
        kind == StorylandScmOperandKind::GlobalArray || kind == StorylandScmOperandKind::LocalArray;
}

bool arrayKind(StorylandScmOperandKind kind) {
    return kind == StorylandScmOperandKind::GlobalArray || kind == StorylandScmOperandKind::LocalArray;
}

bool globalKind(StorylandScmOperandKind kind) {
    return kind == StorylandScmOperandKind::Global || kind == StorylandScmOperandKind::GlobalArray;
}

bool decodeOperand(const std::vector<uint8_t>& bytes, size_t& cursor, size_t end,
    StorylandScmDialect dialect, bool fixedText, StorylandScmOperandView& result, std::string& error) {
    result = {};
    result.offset = cursor;
    const auto take = [&](size_t count, uint32_t& value) {
        if (cursor > end || count > end - cursor) return false;
        value = 0;
        for (size_t index = 0; index < count; ++index) value |= uint32_t(bytes[cursor++]) << (8 * index);
        return true;
    };
    if (fixedText) {
        if (cursor > end || end - cursor < 8) { error = "Truncated fixed eight-byte text operand."; return false; }
        result.kind = StorylandScmOperandKind::Text;
        for (size_t index = 0; index < 8 && bytes[cursor + index]; ++index) {
            if (bytes[cursor + index] < 32 || bytes[cursor + index] > 126) {
                error = "Invalid fixed text operand; decoding stopped before guessing a boundary."; return false;
            }
            result.text.push_back(char(bytes[cursor + index]));
        }
        cursor += 8;
        result.description = "Fixed eight-byte LCS text; at most eight printable ASCII characters.";
    } else {
        if (cursor >= end) { error = "Missing operand."; return false; }
        result.encoding = bytes[cursor++];
        const unsigned kind = result.encoding;
        uint32_t value = 0;
        if (kind == 0) { error = "Unexpected variable-argument terminator."; return false; }
        if (kind == 1 || kind == 2) {
            result.kind = kind == 1 ? StorylandScmOperandKind::Integer : StorylandScmOperandKind::Float;
            result.description = "Compact zero; this one-byte operand cannot represent a nonzero value.";
        } else if (kind >= 3 && kind <= 9) {
            const size_t count = kind <= 5 ? kind - 2 : kind == 7 ? 1 : kind == 8 ? 2 : 4;
            if (!take(count, value)) { error = "Truncated numeric operand."; return false; }
            if (kind <= 5) {
                result.bits = int32_t(value << ((4 - count) * 8));
                result.kind = StorylandScmOperandKind::Float;
                result.description = "Compact float; omitted low bits must remain zero.";
            } else {
                result.bits = kind == 7 ? int8_t(value) : kind == 8 ? int16_t(value) : int32_t(value);
                result.kind = kind == 9 ? StorylandScmOperandKind::Float : StorylandScmOperandKind::Integer;
                result.description = kind == 7 ? "Signed 8-bit integer." : kind == 8 ? "Signed 16-bit integer." :
                    kind == 9 ? "32-bit float." : "Signed 32-bit integer; raw bits may also carry a float.";
            }
        } else if (kind == 10 && dialect == StorylandScmDialect::Vcs) {
            result.kind = StorylandScmOperandKind::Text;
            bool terminated = false;
            while (cursor < end && result.text.size() < 128) {
                const uint8_t character = bytes[cursor++];
                if (character == 0) { terminated = true; break; }
                if (character < 32 || character > 126) {
                    error = "Invalid VCS text operand; decoding stopped before guessing a boundary."; return false;
                }
                result.text.push_back(char(character));
            }
            if (!terminated) { error = "Unterminated VCS text operand or text exceeds 127 characters."; return false; }
            result.description = "NUL-terminated VCS text; edits retain the exact character count.";
        } else {
            const unsigned localStart = dialect == StorylandScmDialect::Vcs ? 13 : 12;
            const unsigned arrayStart = dialect == StorylandScmDialect::Vcs ? 109 : 108;
            const unsigned globalStart = dialect == StorylandScmDialect::Vcs ? 205 : 204;
            const unsigned timerStart = dialect == StorylandScmDialect::Vcs ? 11 : 10;
            if (kind < timerStart) { error = "Unsupported operand encoding."; return false; }
            const bool global = kind >= globalStart;
            const bool array = kind >= 230 || (kind >= arrayStart && kind < globalStart);
            if (global) {
                if (!take(1, value)) { error = "Truncated global variable reference."; return false; }
                result.variable = uint16_t((kind - (array ? 230 : globalStart)) * 256 + value);
            } else result.variable = uint16_t(kind < localStart ? 104 + kind - timerStart :
                kind - (array ? arrayStart : localStart));
            result.kind = global ? (array ? StorylandScmOperandKind::GlobalArray : StorylandScmOperandKind::Global) :
                (array ? StorylandScmOperandKind::LocalArray : StorylandScmOperandKind::Local);
            if (array) {
                uint32_t index = 0, count = 0;
                if (!take(1, index) || !take(1, count)) { error = "Truncated array descriptor."; return false; }
                if (index >= 96) { error = "Array index is not an addressable local variable."; return false; }
                result.indexVariable = uint8_t(index);
                result.arrayCount = uint8_t(count);
            }
            result.description = (global ? "Global slot " : "Local slot ") + std::to_string(result.variable);
            if (global) result.description += " (byte offset " + std::to_string(unsigned(result.variable) * 4) + ")";
            if (array) result.description += "; array uses local index " + std::to_string(result.indexVariable) +
                " and count " + std::to_string(result.arrayCount) + ". Edit as base,index,count.";
        }
    }
    result.size = cursor - result.offset;
    if (result.kind == StorylandScmOperandKind::Text) result.value = result.text;
    else if (referenceKind(result.kind)) {
        result.value = std::string(globalKind(result.kind) ? "g" : "l") + std::to_string(result.variable);
        if (arrayKind(result.kind)) result.value += "[l" + std::to_string(result.indexVariable) + ";" +
            std::to_string(result.arrayCount) + "]";
    } else result.value = result.kind == StorylandScmOperandKind::Float ? floatText(result.bits) : std::to_string(result.bits);
    return true;
}

std::string instructionText(const StorylandScmInstructionView& instruction) {
    std::string result;
    size_t largestParameter = 0;
    const auto& text = instruction.definition;
    for (size_t position = 0; position < text.size();) {
        if (text[position] != '%' || position + 1 >= text.size() ||
            !std::isdigit(static_cast<unsigned char>(text[position + 1]))) {
            result.push_back(text[position++]); continue;
        }
        const size_t close = text.find('%', position + 1);
        if (close == std::string::npos) { result.append(text, position, std::string::npos); break; }
        size_t digit = position + 1, number = 0;
        while (digit < close && std::isdigit(static_cast<unsigned char>(text[digit])))
            number = number * 10 + size_t(text[digit++] - '0');
        if (number && number <= instruction.operands.size()) {
            largestParameter = std::max(largestParameter,number);
            const auto& operand = instruction.operands[number - 1];
            result += operand.kind == StorylandScmOperandKind::Text ? '"' + operand.text + '"' : operand.value;
            if (operand.hasTarget) result += " -> " + hexNumber(operand.target);
        } else result.append(text, position, close - position + 1);
        position = close + 1;
    }
    if (largestParameter < instruction.operands.size()) {
        result += " | arguments:";
        for (size_t index = largestParameter; index < instruction.operands.size(); ++index) {
            const auto& operand = instruction.operands[index];
            result += ' ';
            result += operand.kind == StorylandScmOperandKind::Text ? '"' + operand.text + '"' : operand.value;
        }
    }
    if (instruction.inverted) result = "NOT " + result;
    return trim(result);
}

std::vector<StorylandScmInstructionView> decodeScope(const std::vector<uint8_t>& bytes,
    StorylandScmDialect dialect, const StorylandScmScope& scope) {
    std::vector<StorylandScmInstructionView> rows;
    size_t cursor = scope.begin;
    while (cursor < scope.end) {
        StorylandScmInstructionView row;
        row.offset = cursor;
        const auto fail = [&](const std::string& error) {
            row.valid = false;
            row.end = cursor;
            row.diagnostic = hexNumber(row.offset) + ": " + error + " Remaining bytes are not decoded.";
            row.text = row.diagnostic;
            row.hex = hexBytes(bytes, row.offset, std::min(scope.end, row.offset + 16), true);
            rows.push_back(std::move(row));
        };
        if (scope.end - cursor < 2) { fail("Truncated instruction word."); break; }
        const uint16_t word = uint16_t(bytes[cursor]) | uint16_t(bytes[cursor + 1]) << 8;
        cursor += 2;
        row.opcode = word & 0x7FFF;
        row.inverted = (word & 0x8000) != 0;
        const auto* definition = definitionFor(dialect, row.opcode);
        if (!definition) { fail("Unknown opcode " + hexNumber(row.opcode,4) + "."); break; }
        row.definition = definition->text;
        const bool call = functionCommand(dialect, row.opcode);
        size_t count = definition->parameters >= 0 ? size_t(definition->parameters) : 1;
        if (call) {
            if (scope.end - cursor < 3) { fail("Truncated CALL_FUNC argument header."); break; }
            const unsigned inputs = bytes[cursor], outputs = bytes[cursor + 1], base = bytes[cursor + 2];
            if (inputs > 32 || outputs > 32 || base + inputs + outputs > 128) {
                fail("Invalid CALL_FUNC local range."); break;
            }
            const char* names[] = {"Input count", "Output count", "Local stack base"};
            for (size_t index = 0; index < 3; ++index) {
                StorylandScmOperandView header;
                header.offset = cursor;
                header.size = 1;
                header.kind = StorylandScmOperandKind::FunctionHeader;
                header.bits = bytes[cursor++];
                header.value = std::to_string(header.bits);
                header.description = names[index];
                header.editable = false;
                row.operands.push_back(std::move(header));
            }
            count = 1 + inputs + outputs;
        }
        bool failed = false;
        const bool variable = definition->parameters < 0 && !call;
        for (size_t index = 0; index < count || (variable && cursor < scope.end && bytes[cursor] != 0); ++index) {
            if (index >= 97) { fail("Variable argument list exceeds the native local capacity."); failed = true; break; }
            const size_t number = row.operands.size();
            StorylandScmOperandView operand;
            std::string error;
            const char type = parameterType(row.definition, number);
            const bool fixed = dialect == StorylandScmDialect::Lcs && !call && (type == 'g' || type == 's');
            if (!decodeOperand(bytes,cursor,scope.end,dialect,fixed,operand,error)) {
                fail(error); failed = true; break;
            }
            if (call && index > unsigned(row.operands[0].bits) && !referenceKind(operand.kind)) {
                fail("CALL_FUNC output is not a variable reference."); failed = true; break;
            }
            operand.branch = branchOperand(dialect,row.opcode,number);
            if (operand.branch) {
                if (operand.kind == StorylandScmOperandKind::Integer || operand.kind == StorylandScmOperandKind::Float) {
                    operand.kind = StorylandScmOperandKind::Integer;
                    operand.value = std::to_string(operand.bits);
                    const int64_t target = operand.bits < 0 ? int64_t(scope.begin) - operand.bits : int64_t(operand.bits) + 8;
                    operand.hasTarget = target >= 0 && uint64_t(target) < bytes.size();
                    if (operand.hasTarget) operand.target = size_t(target);
                    operand.description += " Native label: negative is relative to this script; nonnegative is file offset minus eight.";
                } else {
                    operand.editable = false;
                    operand.description += " Dynamic branch target; its runtime destination cannot be validated by this editor.";
                }
            }
            row.operands.push_back(std::move(operand));
        }
        if (failed) break;
        if (variable) {
            if (cursor >= scope.end || bytes[cursor] != 0) { fail("Missing variable-argument terminator."); break; }
            ++cursor;
        }
        row.end = cursor;
        row.valid = true;
        row.text = instructionText(row);
        row.hex = hexBytes(bytes,row.offset,row.end,true);
        rows.push_back(std::move(row));
    }
    return rows;
}

bool parseInteger(const std::string& input, int64_t& value) {
    const std::string text = trim(input);
    if (text.empty()) return false;
    size_t first = text.front() == '-' || text.front() == '+' ? 1 : 0;
    const int base = text.size() > first + 2 && text[first] == '0' &&
        (text[first + 1] == 'x' || text[first + 1] == 'X') ? 16 : 10;
    try {
        size_t consumed = 0;
        value = std::stoll(text,&consumed,base);
        return consumed == text.size();
    } catch (const std::exception&) { return false; }
}

bool numericBytes(const StorylandScmOperandView& operand, int32_t bits,
    std::vector<uint8_t>& output, std::string& error) {
    output.assign(operand.size,0);
    output[0] = operand.encoding;
    const unsigned kind = operand.encoding;
    if (kind == 1 || kind == 2) {
        if (bits != 0) { error = "This compact zero has no payload. A nonzero value would move following instructions."; return false; }
        return true;
    }
    uint32_t value = uint32_t(bits);
    if (kind >= 3 && kind <= 5) {
        const unsigned shift = (6 - kind) * 8;
        const uint32_t mask = (uint32_t(1) << shift) - 1;
        if ((value & mask) != 0) { error = "This float cannot be represented exactly at the existing compact width."; return false; }
        value >>= shift;
    } else if ((kind == 7 && (bits < -128 || bits > 127)) ||
        (kind == 8 && (bits < -32768 || bits > 32767))) {
        error = "Integer is outside the existing signed operand width."; return false;
    }
    for (size_t index = 1; index < output.size(); ++index) output[index] = uint8_t(value >> ((index - 1) * 8));
    return true;
}

bool referenceBytes(const StorylandScmOperandView& operand, const std::string& input,
    StorylandScmDialect dialect, std::vector<uint8_t>& output, std::string& error) {
    std::string text = lower(trim(input));
    const bool global = globalKind(operand.kind), array = arrayKind(operand.kind);
    const size_t bracket = text.find('[');
    if (bracket != std::string::npos) {
        if (!array || text.back() != ']' || text.find('[',bracket + 1) != std::string::npos ||
            text.find(']') != text.size() - 1 || text.find(',') != std::string::npos) {
            error = "Use base[index;count] or base,index,count for an array."; return false;
        }
        text[bracket] = ',';
        text.pop_back();
        std::replace(text.begin(),text.end(),';',',');
    } else if (text.find_first_of("];[") != std::string::npos) {
        error = "Invalid variable reference syntax."; return false;
    }
    std::vector<int64_t> fields;
    std::istringstream stream(text);
    std::string part;
    while (std::getline(stream,part,',')) {
        part = trim(part);
        const bool fieldGlobal = fields.empty() && global;
        if (!part.empty() && (part.front() == 'g' || part.front() == 'l' || part.front() == '$')) {
            if ((part.front() == 'g' || part.front() == '$') != fieldGlobal || fields.size() > 1) {
                error = "A variable prefix must match the selected global or local reference."; return false;
            }
            part.erase(part.begin());
        }
        if (!part.empty() && part.back() == '@') {
            if (fieldGlobal || fields.size() > 1) { error = "Only local slots accept the @ suffix."; return false; }
            part.pop_back();
        }
        int64_t value = 0;
        if (!parseInteger(part,value) || value < 0) { error = "Use a nonnegative variable slot, or base,index,count for an array."; return false; }
        fields.push_back(value);
    }
    if (text.empty() || text.back() == ',' || fields.empty() || (fields.size() != 1 && !(array && fields.size() == 3))) {
        error = "Use a variable slot, or base,index,count for an array."; return false;
    }
    const int64_t base = fields[0];
    const int64_t index = fields.size() == 3 ? fields[1] : operand.indexVariable;
    const int64_t count = fields.size() == 3 ? fields[2] : operand.arrayCount;
    if (array && (index >= 96 || count < 1 || count > 255)) {
        error = "Arrays require a local index from 0 to 95 and a count from 1 to 255."; return false;
    }
    const unsigned globalStart = dialect == StorylandScmDialect::Vcs ? 205 : 204;
    const unsigned localStart = dialect == StorylandScmDialect::Vcs ? 13 : 12;
    const unsigned arrayStart = dialect == StorylandScmDialect::Vcs ? 109 : 108;
    const unsigned timerStart = dialect == StorylandScmDialect::Vcs ? 11 : 10;
    output.clear();
    if (global) {
        const int64_t limit = array ? 26 * 256 : (230 - globalStart) * 256;
        if (base >= limit) { error = "Global slot exceeds the existing reference encoding."; return false; }
        output.push_back(uint8_t((array ? 230 : globalStart) + base / 256));
        output.push_back(uint8_t(base));
    } else {
        if (array ? base >= 96 : base >= 96 && base != 104 && base != 105) {
            error = "Local references address slots 0 to 95, or the two scalar timers at 104 and 105."; return false;
        }
        output.push_back(uint8_t(!array && base >= 104 ? timerStart + base - 104 :
            (array ? arrayStart : localStart) + base));
    }
    if (array) { output.push_back(uint8_t(index)); output.push_back(uint8_t(count)); }
    return output.size() == operand.size;
}

bool protectedOutput(const std::filesystem::path& original, const std::filesystem::path& output, std::string& error) {
    if (output.empty()) { error = "Choose an output filename."; return false; }
    if (original.empty()) return true;
    std::error_code firstError, secondError;
    const auto first = std::filesystem::weakly_canonical(original,firstError);
    const auto second = std::filesystem::weakly_canonical(output,secondError);
    bool same = !firstError && !secondError && first == second;
#ifdef _WIN32
    if (!firstError && !secondError) same = lower(first.u8string()) == lower(second.u8string());
#endif
    std::error_code equivalentError;
    same = same || std::filesystem::equivalent(original,output,equivalentError);
    if (same) { error = "The opened SCM is protected. Choose a different file for the edited copy or export."; return false; }
    return true;
}

bool writeOutput(const std::filesystem::path& original, const std::filesystem::path& output,
    const std::vector<uint8_t>& bytes, std::string& error) {
    if (!protectedOutput(original,output,error)) return false;
    return storylandWriteFilesTransaction({{output,&bytes}},error);
}

std::vector<uint8_t> parseHex(const std::string& text) {
    if (text.empty() || text.size() % 2 || text.size() > maximumPatchBytes)
        throw std::runtime_error("Patch bytes must be a nonempty, even-length hexadecimal string.");
    std::vector<uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    const auto digit = [](char value) -> unsigned {
        if (value >= '0' && value <= '9') return unsigned(value - '0');
        if (value >= 'a' && value <= 'f') return unsigned(value - 'a' + 10);
        if (value >= 'A' && value <= 'F') return unsigned(value - 'A' + 10);
        throw std::runtime_error("Patch contains a non-hexadecimal byte.");
    };
    for (size_t index = 0; index < text.size(); index += 2)
        bytes.push_back(uint8_t(digit(text[index]) * 16 + digit(text[index + 1])));
    return bytes;
}

size_t jsonSize(const nlohmann::json& value) {
    if (!value.is_number_integer() || (value.is_number_integer() && !value.is_number_unsigned() && value.get<int64_t>() < 0))
        throw std::runtime_error("Patch offsets and sizes must be nonnegative integers.");
    const uint64_t number = value.get<uint64_t>();
    if (number > maximumScmBytes) throw std::runtime_error("Patch offset or size exceeds the SCM limit.");
    return size_t(number);
}

void requireKeys(const nlohmann::json& object, const std::set<std::string>& keys) {
    if (!object.is_object() || object.size() != keys.size()) throw std::runtime_error("Unexpected patch fields.");
    for (const auto& item : object.items())
        if (!keys.count(item.key())) throw std::runtime_error("Unknown patch field: " + item.key());
}
}

bool StorylandScmWorkbench::load(const std::filesystem::path& path, std::string& error) {
    StorylandScmFile source;
    if (!source.loadFromFile(path.wstring(),error)) return false;
    return load(source,error);
}

bool StorylandScmWorkbench::load(const StorylandScmFile& source, std::string& error) {
    if (!source.header().valid || source.header().dialect == StorylandScmDialect::Unknown) {
        error = "This file has no recognized LCS/VCS native SCM header and dialect."; return false;
    }
    if (source.rawBytes().empty() || source.rawBytes().size() > maximumScmBytes) {
        error = "SCM size is outside the supported range."; return false;
    }
    StorylandScmWorkbench candidate;
    if (!source.sourcePath().empty()) {
        std::error_code pathError;
        candidate.openedPath = std::filesystem::absolute(std::filesystem::path(source.sourcePath()),pathError);
        if (pathError) { error = "Could not resolve the opened SCM's source path."; return false; }
    }
    candidate.nativeHeader = source.header();
    candidate.gameDialect = source.header().dialect;
    candidate.original = source.rawBytes();
    candidate.working = candidate.original;
    candidate.sourceFingerprint = sha256(candidate.original);
    const size_t begin = source.header().mainScriptOffset, end = source.header().mainScriptSize;
    if (begin >= end || end > candidate.original.size()) { error = "MAIN script bounds are invalid."; return false; }
    candidate.scriptScopes.push_back({-1,begin,end,"MAIN","MAIN","Main script"});
    for (const auto& mission : source.missions()) {
        if (!mission.hasBinaryRange || !mission.binarySize) continue;
        const size_t offset = mission.binaryOffset, length = mission.binarySize;
        if (offset > candidate.original.size() || length > candidate.original.size() - offset || offset < end) {
            error = "Mission bounds overlap MAIN or extend outside the file."; return false;
        }
        candidate.scriptScopes.push_back({mission.id,offset,offset + length,
            mission.name,mission.scriptName,mission.category});
    }
    candidate.decodeSelected();
    *this = std::move(candidate);
    error.clear();
    return true;
}

bool StorylandScmWorkbench::loadFragment(const std::vector<uint8_t>& bytes, StorylandScmDialect dialect,
    const std::string& name, std::string& error) {
    if (bytes.empty() || bytes.size() > maximumScmBytes || dialect == StorylandScmDialect::Unknown) {
        error = "A nonempty fragment and explicit LCS/VCS dialect are required."; return false;
    }
    StorylandScmWorkbench candidate;
    candidate.gameDialect = dialect;
    candidate.original = bytes;
    candidate.working = bytes;
    candidate.sourceFingerprint = sha256(bytes);
    candidate.scriptScopes.push_back({-1,0,bytes.size(),name.empty() ? "Script fragment" : name,"","Fragment"});
    candidate.decodeSelected();
    *this = std::move(candidate);
    error.clear();
    return true;
}

void StorylandScmWorkbench::clear() { *this = {}; }

bool StorylandScmWorkbench::dirty() const { return modifiedBytes != 0; }

void StorylandScmWorkbench::decodeSelected() {
    decoded.clear();
    scopeDiagnostics.clear();
    if (currentScope >= scriptScopes.size()) return;
    decoded = decodeScope(working,gameDialect,scriptScopes[currentScope]);
    for (const auto& row : decoded) if (!row.valid) scopeDiagnostics.push_back(row.diagnostic);
}

bool StorylandScmWorkbench::selectScope(size_t index, std::string& error) {
    if (index >= scriptScopes.size()) { error = "The selected script scope does not exist."; return false; }
    currentScope = index;
    decodeSelected();
    error.clear();
    return true;
}

bool StorylandScmWorkbench::selectOffset(size_t offset, std::string& error) {
    for (size_t index = 0; index < scriptScopes.size(); ++index) {
        if (offset < scriptScopes[index].begin || offset >= scriptScopes[index].end) continue;
        const auto rows = decodeScope(working,gameDialect,scriptScopes[index]);
        if (std::any_of(rows.begin(),rows.end(),[&](const auto& row) { return row.valid && row.offset == offset; }))
            return selectScope(index,error);
        error = "The address is not a verified instruction boundary."; return false;
    }
    error = "The address is outside all script scopes.";
    return false;
}

std::vector<size_t> StorylandScmWorkbench::find(const std::string& query) const {
    std::vector<size_t> result;
    const std::string needle = lower(trim(query));
    const bool addressQuery = !needle.empty() && std::all_of(needle.begin(),needle.end(),
        [](unsigned char character) { return std::isxdigit(character) || character == 'x'; });
    for (size_t index = 0; index < decoded.size(); ++index) {
        const auto& row = decoded[index];
        if (needle.empty() || containsLowercase(row.text,needle) || containsLowercase(row.hex,needle) ||
            (addressQuery && (containsLowercase(hexNumber(row.offset),needle) ||
                containsLowercase(hexNumber(row.opcode,4),needle)))) result.push_back(index);
    }
    return result;
}

std::vector<StorylandScmReference> StorylandScmWorkbench::referencesTo(size_t target) const {
    std::vector<StorylandScmReference> result;
    for (size_t index = 0; index < scriptScopes.size(); ++index) {
        for (const auto& row : decodeScope(working,gameDialect,scriptScopes[index])) {
            if (!row.valid) continue;
            for (size_t operand = 0; operand < row.operands.size(); ++operand)
                if (row.operands[operand].branch && row.operands[operand].hasTarget && row.operands[operand].target == target)
                    result.push_back({index,row.offset,operand,row.text});
        }
    }
    return result;
}

std::vector<StorylandScmReference> StorylandScmWorkbench::variableReferences(bool global, uint16_t slot,
    bool allScopes) const {
    std::vector<StorylandScmReference> result;
    for (size_t index = 0; index < scriptScopes.size(); ++index) {
        if (!allScopes && index != currentScope) continue;
        for (const auto& row : decodeScope(working,gameDialect,scriptScopes[index])) {
            if (!row.valid) continue;
            for (size_t item = 0; item < row.operands.size(); ++item) {
                const auto& operand = row.operands[item];
                if (!referenceKind(operand.kind)) continue;
                const bool base = globalKind(operand.kind) == global &&
                    (operand.variable == slot || (arrayKind(operand.kind) && slot >= operand.variable &&
                        unsigned(slot) < unsigned(operand.variable) + operand.arrayCount));
                const bool arrayIndex = !global && arrayKind(operand.kind) && operand.indexVariable == slot;
                if (base || arrayIndex) result.push_back({index,row.offset,item,row.text});
            }
        }
    }
    return result;
}

bool StorylandScmWorkbench::editOperand(size_t offset, size_t operandIndex, const std::string& value,
    StorylandScmValueFormat format, std::string& error) {
    const auto found = std::find_if(decoded.begin(),decoded.end(),[&](const auto& row) { return row.offset == offset; });
    if (found == decoded.end() || !found->valid || operandIndex >= found->operands.size()) {
        error = "Select an operand of a decoded instruction."; return false;
    }
    const auto& operand = found->operands[operandIndex];
    if (!operand.editable) { error = "This structural or dynamic-target operand is read-only."; return false; }
    std::vector<uint8_t> replacement;
    if (operand.kind == StorylandScmOperandKind::Text) {
        if (format != StorylandScmValueFormat::Automatic && format != StorylandScmValueFormat::Text) {
            error = "Choose text editing for this operand."; return false;
        }
        if (std::any_of(value.begin(),value.end(),[](unsigned char character) { return character < 32 || character > 126; })) {
            error = "SCM text operands require printable ASCII characters."; return false;
        }
        if (operand.encoding == 10) {
            if (value.size() != operand.text.size()) {
                error = "This VCS text occupies " + std::to_string(operand.text.size()) +
                    " characters. Keep that exact length; full source compilation can relocate longer text."; return false;
            }
            replacement.push_back(10);
            replacement.insert(replacement.end(),value.begin(),value.end());
            replacement.push_back(0);
        } else {
            if (value.size() > 8) { error = "LCS fixed text has room for at most eight characters."; return false; }
            replacement.assign(8,0);
            std::copy(value.begin(),value.end(),replacement.begin());
        }
    } else if (referenceKind(operand.kind)) {
        if (format != StorylandScmValueFormat::Automatic && format != StorylandScmValueFormat::Reference) {
            error = "Choose variable reference editing for this operand."; return false;
        }
        if (!referenceBytes(operand,value,gameDialect,replacement,error)) return false;
    } else {
        if (format == StorylandScmValueFormat::Automatic)
            format = operand.kind == StorylandScmOperandKind::Float ? StorylandScmValueFormat::Float : StorylandScmValueFormat::Integer;
        int32_t bits = 0;
        if (format == StorylandScmValueFormat::Float) {
            if (operand.branch) { error = "A branch target is an integer label, not a float."; return false; }
            try {
                const std::string text = trim(value);
                size_t consumed = 0;
                const float number = std::stof(text,&consumed);
                if (consumed != text.size() || !std::isfinite(number)) throw std::runtime_error("invalid");
                std::memcpy(&bits,&number,sizeof(bits));
            } catch (const std::exception&) { error = "Enter a finite 32-bit float."; return false; }
        } else if (format == StorylandScmValueFormat::Integer || format == StorylandScmValueFormat::Target) {
            int64_t number = 0;
            if (!parseInteger(value,number)) { error = "Enter a complete decimal integer or 0x-prefixed hexadecimal integer."; return false; }
            if (format == StorylandScmValueFormat::Target) {
                if (!operand.branch || number < 0 || uint64_t(number) >= working.size()) {
                    error = "Enter an instruction's absolute file offset for a branch operand."; return false;
                }
                const auto& scope = scriptScopes[currentScope];
                if (operand.bits < 0 && uint64_t(number) > scope.begin) number = int64_t(scope.begin) - number;
                else {
                    if (number < 8) { error = "This target cannot use a nonnegative native file label."; return false; }
                    number -= 8;
                }
            }
            if (number < INT32_MIN || number > INT32_MAX) { error = "Value is outside the signed 32-bit range."; return false; }
            bits = int32_t(number);
        } else { error = "Choose an integer, float, or branch target interpretation."; return false; }
        if (!numericBytes(operand,bits,replacement,error)) return false;
    }
    StorylandScmChange change;
    change.offset = operand.offset;
    change.before.assign(working.begin() + operand.offset,working.begin() + operand.offset + operand.size);
    change.after = std::move(replacement);
    if (change.before == change.after) { error.clear(); return true; }
    return commit({currentScope,{std::move(change)}},error);
}

bool StorylandScmWorkbench::checkChanges(const std::vector<StorylandScmChange>& edits, std::string& error) const {
    if (!loaded()) { error = "Open an SCM before editing."; return false; }
    auto candidate = working;
    size_t previousEnd = 0;
    for (const auto& edit : edits) {
        if (edit.before.empty() || edit.before.size() != edit.after.size() || edit.offset < previousEnd ||
            edit.offset > candidate.size() || edit.before.size() > candidate.size() - edit.offset) {
            error = "Edits must be ordered, nonoverlapping, equal-width byte ranges inside the SCM."; return false;
        }
        if (!std::equal(edit.before.begin(),edit.before.end(),working.begin() + edit.offset)) {
            error = "An edit's expected original bytes no longer match the working document."; return false;
        }
        std::copy(edit.after.begin(),edit.after.end(),candidate.begin() + edit.offset);
        previousEnd = edit.offset + edit.before.size();
    }
    std::vector<size_t> boundaries, changedTargets;
    size_t coveredChanges = 0, totalChanges = 0;
    for (size_t index = 0; index < working.size(); ++index) totalChanges += working[index] != candidate[index];
    std::set<std::pair<size_t,size_t>> visitedScopes;
    for (const auto& scope : scriptScopes) {
        const bool touched = std::any_of(edits.begin(),edits.end(),[&](const auto& edit) {
            return edit.offset < scope.end && edit.offset + edit.before.size() > scope.begin;
        });
        if (!touched) continue;
        if (!visitedScopes.insert({scope.begin,scope.end}).second) continue;
        const auto before = decodeScope(working,gameDialect,scope);
        const auto after = decodeScope(candidate,gameDialect,scope);
        if (before.size() != after.size()) { error = "Edit changes instruction boundaries."; return false; }
        for (size_t row = 0; row < before.size(); ++row) {
            const auto& first = before[row];
            const auto& second = after[row];
            if (first.offset != second.offset || first.end != second.end || first.valid != second.valid ||
                first.opcode != second.opcode || first.inverted != second.inverted || first.operands.size() != second.operands.size()) {
                error = "Edit changes an opcode, instruction boundary, or structural operand count."; return false;
            }
            if (!first.valid) continue;
            boundaries.push_back(second.offset);
            for (size_t index = 0; index < first.operands.size(); ++index) {
                const auto& old = first.operands[index];
                const auto& updated = second.operands[index];
                if (old.offset != updated.offset || old.size != updated.size || old.kind != updated.kind) {
                    error = "Edit changes an operand's storage type or encoded width."; return false;
                }
                size_t changed = 0;
                for (size_t byte = old.offset; byte < old.offset + old.size; ++byte) changed += working[byte] != candidate[byte];
                if (!changed) continue;
                if (!old.editable) { error = "Edit changes a protected structural or dynamic branch operand."; return false; }
                if (!referenceKind(old.kind) && old.encoding != updated.encoding) {
                    error = "Edit changes a numeric or text encoding tag."; return false;
                }
                if (updated.kind == StorylandScmOperandKind::Float && !std::isfinite(floatBits(updated.bits))) {
                    error = "Edited float must be finite."; return false;
                }
                if (referenceKind(updated.kind)) {
                    const size_t limit = globalKind(updated.kind) ? (nativeHeader.valid ? nativeHeader.globalVariableBytes / 4 : 65536) : 128;
                    const size_t last = updated.variable + (arrayKind(updated.kind) ? std::max(1u,unsigned(updated.arrayCount)) - 1 : 0);
                    if (last >= limit || (arrayKind(updated.kind) && updated.arrayCount == 0)) {
                        error = "Edited reference extends outside the declared variable storage."; return false;
                    }
                }
                if (updated.branch) {
                    if (!updated.hasTarget) { error = "Edited branch target lies outside the SCM."; return false; }
                    changedTargets.push_back(updated.target);
                }
                coveredChanges += changed;
            }
        }
    }
    if (coveredChanges != totalChanges) { error = "Edits may only change verified operand bytes; headers, opcodes and undecoded bytes are protected."; return false; }
    for (size_t target : changedTargets) {
        for (const auto& scope : scriptScopes) {
            if (target < scope.begin || target >= scope.end ||
                !visitedScopes.insert({scope.begin,scope.end}).second) continue;
            for (const auto& row : decodeScope(candidate,gameDialect,scope))
                if (row.valid) boundaries.push_back(row.offset);
        }
    }
    std::sort(boundaries.begin(),boundaries.end());
    for (size_t target : changedTargets) {
        if (!std::binary_search(boundaries.begin(),boundaries.end(),target)) {
            error = "Edited branch target is not a verified instruction boundary."; return false;
        }
    }
    error.clear();
    return true;
}

bool StorylandScmWorkbench::commit(Edit edit, std::string& error) {
    if (!checkChanges(edit.changes,error)) return false;
    for (const auto& change : edit.changes) {
        for (size_t index = 0; index < change.after.size(); ++index) {
            const size_t offset = change.offset + index;
            modifiedBytes -= working[offset] != original[offset];
            working[offset] = change.after[index];
            modifiedBytes += working[offset] != original[offset];
        }
    }
    undoHistory.push_back(std::move(edit));
    redoHistory.clear();
    decodeSelected();
    error.clear();
    return true;
}

bool StorylandScmWorkbench::undo(std::string& error) {
    if (undoHistory.empty()) { error = "There is no edit to undo."; return false; }
    Edit edit = std::move(undoHistory.back());
    undoHistory.pop_back();
    for (const auto& change : edit.changes) {
        for (size_t index = 0; index < change.before.size(); ++index) {
            const size_t offset = change.offset + index;
            modifiedBytes -= working[offset] != original[offset];
            working[offset] = change.before[index];
            modifiedBytes += working[offset] != original[offset];
        }
    }
    currentScope = edit.scope;
    redoHistory.push_back(std::move(edit));
    decodeSelected();
    error.clear();
    return true;
}

bool StorylandScmWorkbench::redo(std::string& error) {
    if (redoHistory.empty()) { error = "There is no edit to redo."; return false; }
    Edit edit = std::move(redoHistory.back());
    redoHistory.pop_back();
    for (const auto& change : edit.changes) {
        for (size_t index = 0; index < change.after.size(); ++index) {
            const size_t offset = change.offset + index;
            modifiedBytes -= working[offset] != original[offset];
            working[offset] = change.after[index];
            modifiedBytes += working[offset] != original[offset];
        }
    }
    currentScope = edit.scope;
    undoHistory.push_back(std::move(edit));
    decodeSelected();
    error.clear();
    return true;
}

void StorylandScmWorkbench::revert() {
    working = original;
    modifiedBytes = 0;
    undoHistory.clear();
    redoHistory.clear();
    decodeSelected();
}

std::vector<StorylandScmChange> StorylandScmWorkbench::changes() const {
    std::vector<StorylandScmChange> result;
    for (size_t offset = 0; offset < working.size();) {
        if (working[offset] == original[offset]) { ++offset; continue; }
        const size_t begin = offset++;
        while (offset < working.size() && working[offset] != original[offset]) ++offset;
        result.push_back({begin,{original.begin() + begin,original.begin() + offset},
            {working.begin() + begin,working.begin() + offset}});
    }
    return result;
}

bool StorylandScmWorkbench::validate(std::vector<std::string>& issues) const {
    issues.clear();
    if (!loaded()) { issues.push_back("No SCM is open."); return false; }
    std::vector<size_t> boundaries;
    std::vector<std::pair<size_t,size_t>> targets;
    std::set<std::pair<size_t,size_t>> visited;
    for (const auto& scope : scriptScopes) {
        if (!visited.insert({scope.begin,scope.end}).second) continue;
        for (const auto& row : decodeScope(working,gameDialect,scope)) {
            if (!row.valid) { issues.push_back(scope.name + ": " + row.diagnostic); continue; }
            boundaries.push_back(row.offset);
            for (const auto& operand : row.operands) {
                if (!operand.branch) continue;
                if (referenceKind(operand.kind)) {
                    issues.push_back(hexNumber(row.offset) + ": Dynamic branch target needs runtime validation.");
                } else if (!operand.hasTarget) issues.push_back(hexNumber(row.offset) + ": Branch points outside the file.");
                else targets.push_back({row.offset,operand.target});
            }
        }
    }
    std::sort(boundaries.begin(),boundaries.end());
    for (const auto& target : targets) if (!std::binary_search(boundaries.begin(),boundaries.end(),target.second))
        issues.push_back(hexNumber(target.first) + ": Branch target " + hexNumber(target.second) + " is not a verified instruction boundary.");
    return issues.empty();
}

std::string StorylandScmWorkbench::disassembly(bool allScopes) const {
    std::ostringstream output;
    output << "Storyland SCM workbench\nGame: " << (gameDialect == StorylandScmDialect::Vcs ? "VCS" : "LCS")
        << "\nOriginal SHA-256: " << sourceFingerprint
        << "\nNative labels: negative = script start - value; nonnegative = value + 8.\n"
        << "This listing describes bytecode; it is not a claim that every opcode is implemented by the engine.\n";
    for (size_t index = 0; index < scriptScopes.size(); ++index) {
        if (!allScopes && index != currentScope) continue;
        const auto& scope = scriptScopes[index];
        output << "\n[" << index << "] " << scope.name << " | mission " << scope.mission << " | "
            << hexNumber(scope.begin) << ".." << hexNumber(scope.end) << "\n";
        for (const auto& row : decodeScope(working,gameDialect,scope))
            output << hexNumber(row.offset,8) << "  " << hexNumber(row.opcode,4) << "  " << row.text << "\n    " << row.hex << "\n";
    }
    return output.str();
}

bool StorylandScmWorkbench::exportDisassembly(const std::filesystem::path& path, bool allScopes, std::string& error) const {
    if (!loaded()) { error = "Open an SCM before exporting a listing."; return false; }
    const std::string text = disassembly(allScopes);
    return writeOutput(openedPath,path,{text.begin(),text.end()},error);
}

bool StorylandScmWorkbench::saveCopy(const std::filesystem::path& path, std::string& error) const {
    if (!loaded()) { error = "Open an SCM before saving a copy."; return false; }
    return writeOutput(openedPath,path,working,error);
}

std::string StorylandScmWorkbench::patchText() const {
    nlohmann::json result = {
        {"format","storyland-scm-patch"},{"version",1},
        {"game",gameDialect == StorylandScmDialect::Vcs ? "vcs" : "lcs"},
        {"source",{{"bytes",original.size()},{"sha256",sourceFingerprint}}},
        {"changes",nlohmann::json::array()}
    };
    for (const auto& edit : changes()) result["changes"].push_back({
        {"offset",edit.offset},{"before",hexBytes(edit.before,0,edit.before.size(),false)},
        {"after",hexBytes(edit.after,0,edit.after.size(),false)}
    });
    return result.dump(2) + "\n";
}

bool StorylandScmWorkbench::applyPatchText(const std::string& text, std::string& error) {
    if (!loaded()) { error = "Open the patch's original SCM first."; return false; }
    if (dirty()) { error = "Apply a patch to a clean original document; save or revert current edits first."; return false; }
    if (text.size() > maximumPatchBytes) { error = "Patch exceeds the 16 MiB limit."; return false; }
    try {
        bool duplicate = false;
        std::vector<std::set<std::string>> objects;
        const auto callback = [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json& value) {
            if (depth > 16) throw std::runtime_error("Patch JSON nesting exceeds the supported schema.");
            if (event == nlohmann::json::parse_event_t::object_start) objects.emplace_back();
            else if (event == nlohmann::json::parse_event_t::object_end) objects.pop_back();
            else if (event == nlohmann::json::parse_event_t::key && !objects.back().insert(value.get<std::string>()).second) duplicate = true;
            return true;
        };
        const auto patch = nlohmann::json::parse(text,callback);
        if (duplicate) throw std::runtime_error("Patch contains duplicate JSON keys.");
        requireKeys(patch,{"format","version","game","source","changes"});
        requireKeys(patch.at("source"),{"bytes","sha256"});
        if (patch.at("format") != "storyland-scm-patch" || !patch.at("version").is_number_integer() ||
            patch.at("version") != 1 || patch.at("game") != (gameDialect == StorylandScmDialect::Vcs ? "vcs" : "lcs"))
            throw std::runtime_error("Patch format, version or game does not match this document.");
        if (jsonSize(patch.at("source").at("bytes")) != original.size() || patch.at("source").at("sha256") != sourceFingerprint)
            throw std::runtime_error("Patch belongs to different original SCM bytes (SHA-256 or size mismatch).");
        const auto& entries = patch.at("changes");
        if (!entries.is_array() || entries.size() > 65536) throw std::runtime_error("Patch changes must be an array with at most 65,536 entries.");
        Edit edit;
        edit.scope = currentScope;
        for (const auto& entry : entries) {
            requireKeys(entry,{"offset","before","after"});
            edit.changes.push_back({jsonSize(entry.at("offset")),parseHex(entry.at("before").get<std::string>()),
                parseHex(entry.at("after").get<std::string>())});
        }
        if (edit.changes.empty()) { error.clear(); return true; }
        return commit(std::move(edit),error);
    } catch (const std::exception& exception) {
        error = std::string("Could not apply SCM patch: ") + exception.what(); return false;
    }
}

bool StorylandScmWorkbench::exportPatch(const std::filesystem::path& path, std::string& error) const {
    if (!loaded()) { error = "Open an SCM before exporting a patch."; return false; }
    const auto text = patchText();
    return writeOutput(openedPath,path,{text.begin(),text.end()},error);
}

bool StorylandScmWorkbench::importPatch(const std::filesystem::path& path, std::string& error) {
    std::ifstream file(path,std::ios::binary | std::ios::ate);
    if (!file) { error = "Could not open the SCM patch."; return false; }
    const auto length = file.tellg();
    if (length < 0 || uint64_t(length) > maximumPatchBytes) { error = "Patch exceeds the 16 MiB limit."; return false; }
    std::string text(size_t(length),'\0');
    file.seekg(0);
    if (!text.empty() && !file.read(text.data(),std::streamsize(text.size()))) { error = "Could not read the complete SCM patch."; return false; }
    return applyPatchText(text,error);
}
