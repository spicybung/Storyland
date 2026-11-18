#pragma once
#include <cstdint>
#include <string>

inline bool storylandValidChatText(const std::string& text, size_t maximumBytes = 768) {
    if (text.empty() || text.size() > maximumBytes) return false;
    bool visible = false;
    for (size_t index = 0; index < text.size();) {
        const uint8_t first = uint8_t(text[index++]);
        uint32_t code = first;
        unsigned trailing = 0;
        if (first >= 0xc2 && first <= 0xdf) { code = first & 31; trailing = 1; }
        else if (first >= 0xe0 && first <= 0xef) { code = first & 15; trailing = 2; }
        else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; trailing = 3; }
        else if (first >= 0x80) return false;
        if (trailing > text.size() - index) return false;
        for (unsigned part = 0; part < trailing; ++part) {
            const uint8_t byte = uint8_t(text[index++]);
            if ((byte & 0xc0) != 0x80) return false;
            code = (code << 6) | (byte & 63);
        }
        if ((trailing == 1 && code < 0x80) || (trailing == 2 && code < 0x800) ||
            (trailing == 3 && code < 0x10000) || code > 0x10ffff ||
            (code >= 0xd800 && code <= 0xdfff) || code < 32 ||
            (code >= 0x7f && code <= 0x9f) || (code >= 0x202a && code <= 0x202e) ||
            (code >= 0x2066 && code <= 0x2069)) return false;
        if (code != 32 && code != 0xa0) visible = true;
    }
    return visible;
}
