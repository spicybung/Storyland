#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <string>

struct StorylandPcHelpBindings {
    bool keyboard = true;
    bool driving = false;
    bool motorcycle = false;
    std::array<int, 256> keys{};

    StorylandPcHelpBindings() {
        for (size_t index = 0; index < keys.size(); ++index) keys[index] = int(index);
    }

    int physicalKey(int logical) const {
        if (logical < 0 || logical >= int(keys.size())) return logical;
        const int physical = keys[size_t(logical)];
        return physical > 0 && physical < int(keys.size()) ? physical : logical;
    }
};

constexpr uint32_t StorylandPcHelpKeyBase = 0xF0000;
constexpr int StorylandPcHelpMouseMotion = 256;

inline const char* storylandPcHelpButtonSprite(int key) {
    switch (key) {
    case 37: return "pc:button:left";
    case 38: return "pc:button:up";
    case 39: return "pc:button:right";
    case 40: return "pc:button:down";
    default: return nullptr;
    }
}

inline std::string storylandPcHelpKeyLabel(int key) {
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        return std::string(1, char(key));
    if (key >= 112 && key <= 135) return "F" + std::to_string(key - 111);
    if (key >= 96 && key <= 105) return "Num " + std::to_string(key - 96);
    switch (key) {
    case 1: return "Mouse 1";
    case 2: return "Mouse 2";
    case 4: return "Mouse 3";
    case 5: return "Mouse 4";
    case 6: return "Mouse 5";
    case 8: return "Backspace";
    case 9: return "Tab";
    case 13: return "Enter";
    case 16: case 160: case 161: return "Shift";
    case 17: case 162: case 163: return "Ctrl";
    case 18: case 164: case 165: return "Alt";
    case 19: return "Pause";
    case 20: return "Caps Lock";
    case 27: return "Esc";
    case 32: return "Space";
    case 33: return "Page Up";
    case 34: return "Page Down";
    case 35: return "End";
    case 36: return "Home";
    case 37: return "Left";
    case 38: return "Up";
    case 39: return "Right";
    case 40: return "Down";
    case 44: return "Print Screen";
    case 45: return "Insert";
    case 46: return "Delete";
    case 91: case 92: return "Super";
    case 93: return "Menu";
    case 106: return "Num *";
    case 107: return "Num +";
    case 109: return "Num -";
    case 110: return "Num .";
    case 111: return "Num /";
    case 144: return "Num Lock";
    case 145: return "Scroll Lock";
    case 186: return ";";
    case 187: return "=";
    case 188: return ",";
    case 189: return "-";
    case 190: return ".";
    case 191: return "/";
    case 192: return "`";
    case 219: return "[";
    case 220: return "\\";
    case 221: return "]";
    case 222: return "'";
    case StorylandPcHelpMouseMotion: return "Mouse";
    default: return "Key " + std::to_string(key);
    }
}

inline std::string storylandPcHelpKeyToken(int key) {
    const uint32_t code = StorylandPcHelpKeyBase + uint32_t(key);
    std::string token;
    token.push_back(char(0xF0 | (code >> 18)));
    token.push_back(char(0x80 | ((code >> 12) & 63)));
    token.push_back(char(0x80 | ((code >> 6) & 63)));
    token.push_back(char(0x80 | (code & 63)));
    return token;
}

inline std::string storylandPcHelpAction(const std::string& action, const StorylandPcHelpBindings& bindings) {
    const auto keys = [&](std::initializer_list<int> logical) {
        std::string result;
        for (int key : logical) {
            if (!result.empty()) result += " / ";
            result += storylandPcHelpKeyToken(bindings.physicalKey(key));
        }
        return result;
    };
    if (action == "PED_SPRINT" || action == "PDSPR" || action == "SWIMF") return keys({16});
    if (action == "PED_JUMPING" || action == "SWIMO" || action == "VEHICLE_HANDBRAKE") return keys({32});
    if (action == "VEHICLE_ENTER_EXIT" || action == "VEEE") return keys({'F'});
    if (action == "PED_FIREWEAPON" || action == "PDFW" || action == "VEWEP") return keys({'G', 1});
    if (action == "PED_LOCK_TARGET" || action == "PDLT") return keys({'E'});
    if (action == "PED_CYCLE_WEAPON_RIGHT" || action == "WEAPON_SWITCH_NEXT" ||
        action == "PED_CYCLE_WEAPON" || action == "CYCLE_WEAPON" || action == "SWITCH_WEAPON" ||
        action == "PDCWR" || action == "PDWEP") return keys({'Q'});
    if (action == "PED_RELOAD" || action == "RELOAD" || action == "VEHICLE_RADIO_STATION") return keys({'R'});
    if (action == "CAMERA_RECENTER" || action == "CAMERA_CENTRE") return keys({'B'});
    if (action == "VEHICLE_ACCELERATE" || action == "VEACC")
        return bindings.motorcycle ? keys({'W'}) : keys({'W', 38});
    if (action == "VEHICLE_BRAKE" || action == "VEBRK" || action == "VEHB")
        return bindings.motorcycle ? keys({'S'}) : keys({'S', 40});
    if (action == "VEHICLE_STEERUP" || action == "VEHICLE_LEAN_FORWARD") return keys({38});
    if (action == "VEHICLE_STEERDOWN" || action == "VEHICLE_LEAN_BACK" ||
        action == "VEHICLE_LEAN_BACKWARD") return keys({40});
    if (action == "VEHICLE_STEER_LEFT") return keys({'A', 37});
    if (action == "VEHICLE_STEER_RIGHT") return keys({'D', 39});
    if (action == "VEHICLE_LOOKLEFT") return keys({'Q'});
    if (action == "VEHICLE_LOOKRIGHT") return keys({'E'});
    if (action == "VEHICLE_HORN" || action == "VEHN") return keys({'H'});
    if (action == "FREE1") return keys({2});
    if (action == "FREE2") return keys({StorylandPcHelpMouseMotion});
    if (action == "SWIMD" || action == "PED_MOVE") return keys({'W', 'A', 'S', 'D'});
    if (action == "FIREH") return keys({'I', 'J', 'K', 'L'});
    if (action == "UP" || action == "DPAD_UP") return keys({38});
    if (action == "DOWN" || action == "DPAD_DOWN") return keys({40});
    if (action == "LEFT" || action == "DPAD_LEFT") return keys({37});
    if (action == "RIGHT" || action == "DPAD_RIGHT") return keys({39});
    if (action == "UD") return keys({38, 40});
    if (action == "LR") return keys({37, 39});
    if (action == "T") return keys({'F'});
    if (action == "S") return bindings.driving ?
        (bindings.motorcycle ? keys({'S'}) : keys({'S', 40})) : keys({32});
    if (action == "X") return bindings.driving ?
        (bindings.motorcycle ? keys({'W'}) : keys({'W', 38})) : keys({16});
    if (action == "O") return keys({'G', 1});
    if (action == "L1") return keys({bindings.driving ? 'R' : 'B'});
    if (action == "L2") return bindings.driving ? keys({'Q'}) : keys({'Z'});
    if (action == "R1") return bindings.driving ? keys({32}) : keys({'E'});
    if (action == "R2") return bindings.driving ? keys({'E'}) : keys({'C'});
    return {};
}

inline std::string storylandExpandPcHelpControlText(const std::string& text, const StorylandPcHelpBindings& bindings) {
    std::string expanded;
    for (size_t cursor = 0; cursor < text.size();) {
        if (text[cursor] != '~') {
            expanded.push_back(text[cursor++]);
            continue;
        }
        const size_t end = text.find('~', cursor + 1);
        if (end == std::string::npos) {
            expanded.append(text, cursor, text.size() - cursor);
            break;
        }
        std::string token = text.substr(cursor + 1, end - cursor - 1);
        size_t consumed = end + 1;
        const bool control = token == "k";
        if (control) {
            size_t opening = consumed;
            while (opening < text.size() && std::isspace(static_cast<unsigned char>(text[opening]))) ++opening;
            const size_t closing = opening < text.size() && text[opening] == '~' ? text.find('~', opening + 1) : std::string::npos;
            if (closing != std::string::npos && closing > opening + 1) {
                token = text.substr(opening + 1, closing - opening - 1);
                consumed = closing + 1;
            }
        }
        const std::string replacement = storylandPcHelpAction(token, bindings);
        if (!replacement.empty()) expanded += replacement;
        else if (control && consumed != end + 1) {
            std::replace(token.begin(), token.end(), '_', ' ');
            expanded += "[" + token + "]";
        } else expanded.append(text, cursor, consumed - cursor);
        cursor = consumed;
    }
    return expanded;
}
