#pragma once
#include "src/storyland/storyland_chat_text.h"
#include <deque>
#include <limits>

class StorylandChatChannel {
public:
    enum class Delivery { Pending, Delivered, Failed };
    struct Message {
        uint64_t ticket = 0;
        uint32_t sequence = 0;
        bool local = false;
        std::string text;
        Delivery delivery = Delivery::Delivered;
    };
    struct Outgoing { uint32_t sequence; std::string text; double lastSent = -1; unsigned attempts = 0; };
    const std::deque<Message>& history() const { return messages; }
    bool send(const std::string& text) {
        if (outgoing.size() >= 16 || sent == std::numeric_limits<uint32_t>::max() || !storylandValidChatText(text)) return false;
        outgoing.push_back({++sent, text});
        append(sent, true, text, Delivery::Pending);
        return true;
    }
    const Outgoing* next(double now, bool& timedOut) {
        timedOut = false;
        if (outgoing.empty()) return nullptr;
        auto& message = outgoing.front();
        if (message.lastSent >= 0 && now - message.lastSent < 0.25) return nullptr;
        if (message.attempts >= 32) { timedOut = true; return nullptr; }
        message.lastSent = now;
        ++message.attempts;
        return &message;
    }
    bool receive(uint32_t sequence, const std::string& text) {
        if (!sequence || !storylandValidChatText(text)) return false;
        if (sequence <= received) return true;
        if (sequence != received + 1) return false;
        received = sequence;
        append(sequence, false, text, Delivery::Delivered);
        return true;
    }
    void acknowledge(uint32_t sequence) {
        if (outgoing.empty() || outgoing.front().sequence != sequence) return;
        for (auto& message : messages)
            if (message.local && message.sequence == sequence) message.delivery = Delivery::Delivered;
        outgoing.pop_front();
    }
    void disconnect() {
        for (auto& message : messages)
            if (message.delivery == Delivery::Pending) message.delivery = Delivery::Failed;
        outgoing.clear(); sent = received = 0;
    }
    void reset() { disconnect(); messages.clear(); }
private:
    std::deque<Outgoing> outgoing;
    std::deque<Message> messages;
    uint32_t sent = 0, received = 0;
    uint64_t nextTicket = 1;
    void append(uint32_t sequence, bool local, const std::string& text, Delivery delivery) {
        if (messages.size() >= 100) messages.pop_front();
        messages.push_back({nextTicket++, sequence, local, text, delivery});
    }
};
