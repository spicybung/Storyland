#pragma once
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <vector>

class StorylandGameChannel {
public:
    static constexpr size_t MaximumPayload = 944;
    static constexpr size_t MaximumPending = 64;
    struct Message {
        uint32_t sequence = 0;
        std::vector<uint8_t> bytes;
        double lastSent = -1;
        unsigned attempts = 0;
        bool acknowledged = false;
    };
    bool send(const std::vector<uint8_t>& bytes) {
        if (bytes.empty() || bytes.size() > MaximumPayload || outgoing.size() >= MaximumPending ||
            sent == std::numeric_limits<uint32_t>::max()) return false;
        outgoing.push_back({++sent, bytes});
        return true;
    }
    const Message* next(double now, bool& timedOut) {
        timedOut = false;
        if (outgoing.empty()) return nullptr;
        size_t inspected = 0;
        for (auto& message : outgoing) {
            if (++inspected > 16) break;
            if (message.acknowledged || (message.lastSent >= 0 && now - message.lastSent < 0.1)) continue;
            if (message.attempts >= 80) { timedOut = true; return nullptr; }
            message.lastSent = now;
            ++message.attempts;
            return &message;
        }
        return nullptr;
    }
    bool receive(uint32_t sequence, const uint8_t* data, size_t size) {
        if (!sequence || !size || size > MaximumPayload || !data) return false;
        if (sequence <= received) return true;
        if (uint64_t(sequence) > uint64_t(received) + MaximumPending) return false;
        if (reordered.count(sequence)) return true;
        if (incoming.size() + reordered.size() >= MaximumPending) return false;
        reordered.emplace(sequence, Message{sequence, std::vector<uint8_t>(data, data + size)});
        promote();
        return true;
    }
    void acknowledge(uint32_t sequence) {
        for (auto& message : outgoing) {
            if (message.sequence == sequence && message.attempts) { message.acknowledged = true; break; }
        }
        while (!outgoing.empty() && outgoing.front().acknowledged) outgoing.pop_front();
    }
    bool take(std::vector<uint8_t>& bytes) {
        if (incoming.empty()) return false;
        bytes = std::move(incoming.front().bytes);
        incoming.pop_front();
        return true;
    }
    size_t pending() const { return outgoing.size(); }
    void reset() { outgoing.clear(); incoming.clear(); reordered.clear(); sent = received = 0; }
private:
    void promote() {
        while (received != std::numeric_limits<uint32_t>::max()) {
            auto next = reordered.find(received + 1);
            if (next == reordered.end()) break;
            incoming.push_back(std::move(next->second));
            reordered.erase(next);
            ++received;
        }
    }
    std::map<uint32_t, Message> reordered;
    std::deque<Message> outgoing, incoming;
    uint32_t sent = 0, received = 0;
};
