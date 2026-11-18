#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <stdexcept>
#include "src/storyland/storyland_chat_text.h"
#include "vendors/nlohmann/json.hpp"

inline std::string storylandChatSourceLanguage(const std::string& text) {
    if (!storylandValidChatText(text)) throw std::runtime_error("Invalid chat text.");
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        const unsigned char first = static_cast<unsigned char>(text[i]);
        const unsigned char second = static_cast<unsigned char>(text[i + 1]);
        if ((first == 0xd0 && second >= 0x90 && second <= 0xbf) ||
            (first == 0xd1 && second >= 0x80 && second <= 0x8f) ||
            (first == 0xd0 && second == 0x81) || (first == 0xd1 && second == 0x91)) return "ru";
    }
    return "en";
}

inline std::string storylandChatUrlEncode(const std::string& text) {
    const char* digits = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') encoded += char(c);
        else { encoded += '%'; encoded += digits[c >> 4]; encoded += digits[c & 15]; }
    }
    return encoded;
}

inline std::vector<std::string> storylandChatTranslationParts(const std::string& text) {
    if (!storylandValidChatText(text)) throw std::runtime_error("Invalid chat text.");
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = std::min(start + 500, text.size());
        if (end < text.size()) {
            while ((static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
            const size_t space = text.rfind(' ', end - 1);
            if (space != std::string::npos && space > start) end = space + 1;
        }
        parts.push_back(text.substr(start, end - start));
        start = end;
    }
    return parts;
}

inline std::string storylandChatReadTranslation(const std::string& response, bool builtIn) {
    const auto parsed = nlohmann::json::parse(response, nullptr, false);
    if (!parsed.is_object()) throw std::runtime_error("Translation service returned an invalid response.");
    const nlohmann::json* data = &parsed;
    if (builtIn) {
        const auto status = parsed.find("responseStatus");
        if (status == parsed.end() || !(status->is_number_integer() || status->is_string()))
            throw std::runtime_error("Translation service returned an invalid status.");
        const bool success = status->is_string() ? status->get<std::string>() == "200" : *status == 200;
        if (!success) throw std::runtime_error("Translation unavailable: service limit or request rejected. Original message kept.");
        const auto exceeded = parsed.find("quotaFinished");
        if (exceeded != parsed.end() && exceeded->is_boolean() && exceeded->get<bool>())
            throw std::runtime_error("Daily translation limit reached. Original message kept.");
        const auto result = parsed.find("responseData");
        if (result == parsed.end() || !result->is_object())
            throw std::runtime_error("Translation service returned an invalid response.");
        data = &*result;
    }
    const auto translated = data->find("translatedText");
    if (translated == data->end() || !translated->is_string())
        throw std::runtime_error("Translation service returned an invalid response.");
    const std::string text = translated->get<std::string>();
    if (!storylandValidChatText(text, 4096))
        throw std::runtime_error("Translation contains unsupported text or exceeds the display limit.");
    return text;
}
