#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#ifdef STORYLAND_USE_AV_HMAC
#include <cstdio>
extern "C" {
#include <libavutil/hmac.h>
}
#else
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#endif
#endif
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <deque>
#include "src/storyland/storyland_chat_channel.h"
#include "src/storyland/storyland_game_channel.h"
#include "src/storyland/storyland_coop_character.h"

struct StorylandNetworkPlayer {
    float x = 0, y = 0, z = 0, heading = 0, animationTime = 0;
    uint32_t motion = 0;
    float health = 100, armour = 0, recoveryTime = 0;
    float maximumHealth = 100, maximumArmour = 100;
    int32_t model = -1, area = 0;
    uint32_t vehicleIdentity = 0, vehiclePhase = 0;
    int32_t vehicleHandle = -1;
    uint32_t sessionFlags = 0, guestStage = 0;
    float gameHour = 12;
    uint32_t gameMilliseconds = 0;
    int32_t weather = -1;
    uint32_t flags = 4;
    uint32_t wanted = 0;
    int32_t weapon = -1;
    uint32_t shotSerial = 0;
    uint32_t life = 1;
    uint32_t actionFlags = 0;
    float elapsed = 0;
    float phase = 0;
    float moveForward = 0;
    float moveRight = 0;
    float aimX = 0;
    float aimY = 0;
    float aimZ = 0;
    float actionTime = 0;
    int32_t animation = -1;
    int32_t animationGroup = 0;
    float scriptTime = 0;
    int32_t vehicleModel = -1;
    int32_t seat = -1;
    uint32_t vehicleAction = 0;
    float vehicleFraction = 0;
    float carX = 0;
    float carY = 0;
    float carZ = 0;
    float carHeading = 0;
    float wheel = 0;
    float rearWheel = 0;
    float steering = 0;
    float suspension = 0;
    float roll = 0;
    float pitch = 0;
    float speed = 0;
    float carHealth = 1000;
    int32_t primaryColor = -1;
    int32_t secondaryColor = -1;
    uint32_t carFlags = 0;
    float wheelPosition0 = 0;
    float wheelPosition1 = 0;
    float wheelPosition2 = 0;
    float wheelPosition3 = 0;
    float door0 = 0;
    float door1 = 0;
    float door2 = 0;
    float door3 = 0;
    float motionTime = 0;
    float torsoX = 0, torsoY = 0, torsoZ = 0;
    float deathBlendRate = 4, deathSpeed = 1;
    std::string radioStation;
    uint32_t radioPosition = UINT32_MAX;
    bool seated() const { return vehicleModel >= 0 && (actionFlags & 8); }
    bool aiming() const { return (actionFlags & 1) != 0; }
    bool reloading() const { return (actionFlags & 2) != 0; }
    bool dead() const { return (flags & 1) != 0; }
    bool busted() const { return (flags & 2) != 0; }
    bool visible() const { return (flags & 4) != 0; }
    bool policeIgnore() const { return (flags & 8) != 0; }
};

class StorylandMultiplayer {
public:
    static constexpr uint32_t ProtocolVersion = 11;
    using Delivery = StorylandChatChannel::Delivery;
    using ChatMessage = StorylandChatChannel::Message;
    const std::deque<ChatMessage>& chat() const { return chatChannel.history(); }
    bool sendChat(const std::string& text) {
        return paired && chatChannel.send(text);
    }
    StorylandMultiplayer() = default;
    StorylandMultiplayer(const StorylandMultiplayer&) = delete;
    StorylandMultiplayer& operator=(const StorylandMultiplayer&) = delete;
    ~StorylandMultiplayer() { disconnect(); }
    std::string status = "Disconnected";
    std::string invitation;
    StorylandNetworkPlayer remote;
    bool connected() const { return paired; }
    bool isHost() const { return hosting; }
    bool friendlyFire() const { return paired && !hosting ? (remote.sessionFlags & 1u) != 0 : configuredFriendlyFire; }
    bool setFriendlyFire(bool enabled) {
        if (active() && !hosting) return false;
        configuredFriendlyFire = enabled;
        return true;
    }
    uint32_t guestStage() const { return paired && !hosting ? remote.guestStage : configuredGuestStage; }
    void setGuestStage(uint32_t stage) { if (hosting) configuredGuestStage = std::min(stage, 2u); }
    const char* guestModelName(bool vcs) const { return storylandCoopGuestModelName(vcs, guestStage()); }
    bool sendGameMessage(const std::vector<uint8_t>& bytes) { return paired && gameChannel.send(bytes); }
    bool takeGameMessage(std::vector<uint8_t>& bytes) { return gameChannel.take(bytes); }
    size_t pendingGameMessages() const { return gameChannel.pending(); }
    uint16_t localPort() const {
        sockaddr_in address{};
        AddressLength length = sizeof(address);
        return active() && getsockname(socketHandle, reinterpret_cast<sockaddr*>(&address), &length) == 0
            ? ntohs(address.sin_port) : 0;
    }
    // Interpolate presentation only. Collision and authority consume the newest state.
    StorylandNetworkPlayer display(double now) const {
        auto result = remote;
        const float t = float(std::clamp((now-lastReceived)/snapshotInterval, 0.0, 1.0));
        const auto blend = [t](float a,float b) { return a+(b-a)*t; };
        const auto angle = [t](float a,float b) { return a+std::remainder(b-a,360.0f)*t; };
        if(previousRemote.life == remote.life && previousRemote.area == remote.area &&
            previousRemote.model == remote.model && std::hypot(remote.x-previousRemote.x,remote.y-previousRemote.y,remote.z-previousRemote.z)<25) {
            result.x=blend(previousRemote.x,remote.x); result.y=blend(previousRemote.y,remote.y); result.z=blend(previousRemote.z,remote.z);
            result.heading=angle(previousRemote.heading,remote.heading);
            if(previousRemote.motion==remote.motion)
                result.phase=std::fmod(previousRemote.phase+std::remainder(remote.phase-previousRemote.phase,1.0f)*t+1.0f,1.0f);
        }
        if(previousRemote.life == remote.life && previousRemote.area == remote.area &&
            previousRemote.vehicleIdentity == remote.vehicleIdentity && previousRemote.vehicleModel == remote.vehicleModel &&
            std::hypot(remote.carX-previousRemote.carX,remote.carY-previousRemote.carY,remote.carZ-previousRemote.carZ)<25) {
            result.carX=blend(previousRemote.carX,remote.carX); result.carY=blend(previousRemote.carY,remote.carY); result.carZ=blend(previousRemote.carZ,remote.carZ);
            result.carHeading=angle(previousRemote.carHeading,remote.carHeading);
        }
        const float age=float(std::clamp(now-lastReceived,0.0,0.1));
        result.animationTime+=age;result.motionTime+=age;result.actionTime+=age;result.scriptTime+=age;
        if(remote.dead())result.recoveryTime+=age;
        return result;
    }
    uint32_t remoteSequence() const { return receivedSequence; }
    bool active() const { return socketHandle != InvalidSocket; }

    void disconnect() {
        if (socketHandle != InvalidSocket) {
#ifdef _WIN32
            closesocket(socketHandle);
#else
            ::close(socketHandle);
#endif
        }
        socketHandle = InvalidSocket;
#ifdef _WIN32
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        algorithm = nullptr;
        if (winsockReady) WSACleanup();
        winsockReady = false;
#endif
        paired = false;
        hasPeer = false;
        sentSequence = receivedSequence = 0;
        lastReceived = lastSent = startedAt = 0;
        snapshotInterval = 0.05; configuredGuestStage = 0;
#ifdef _WIN32
        SecureZeroMemory(key.data(), key.size());
#elif defined(STORYLAND_USE_AV_HMAC)
        if (hmac) av_hmac_free(hmac);
        hmac = nullptr;
        volatile unsigned char* secret = key.data();
        for (size_t index = 0; index < key.size(); ++index) secret[index] = 0;
#else
        OPENSSL_cleanse(key.data(), key.size());
#endif
        chatChannel.disconnect();
        gameChannel.reset();
        remote = {}; previousRemote = {};
        invitation.clear();
        status = "Disconnected";
    }

    bool host(uint16_t port, uint32_t game) {
        disconnect();
        hosting = true;
        if (!open(game)) return false;
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = htons(port);
        local.sin_addr.s_addr = INADDR_ANY;
        if (bind(socketHandle, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == -1)
            return fail("Cannot bind host UDP port");
#ifdef _WIN32
        if (BCryptGenRandom(nullptr, key.data(), ULONG(key.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
#elif defined(STORYLAND_USE_AV_HMAC)
        std::FILE* random = std::fopen("/dev/urandom", "rb");
        if (!random) return fail("Cannot open the system random device");
        const size_t randomBytes = std::fread(key.data(), 1, key.size(), random);
        std::fclose(random);
        if (randomBytes != key.size())
#else
        if (RAND_bytes(key.data(), int(key.size())) != 1)
#endif
            return fail("Cannot generate invitation code");
        const char* digits = "0123456789abcdef";
        for (const auto value : key) {
            invitation += digits[value >> 4];
            invitation += digits[value & 15];
        }
        status = "Hosting: waiting for player 2";
        return true;
    }

    bool join(const char* address, uint16_t port, const char* code, uint32_t game) {
        disconnect();
        hosting = false;
        if (std::strlen(code) != 32) return fail("Invitation code must contain 32 hexadecimal characters");
        std::array<unsigned char, 16> parsed{};
        for (size_t index = 0; index < 32; ++index) {
            const char c = code[index];
            const int digit = c >= '0' && c <= '9' ? c - '0' :
                c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (digit < 0) return fail("Invalid invitation code");
            parsed[index / 2] = static_cast<unsigned char>(parsed[index / 2] * 16 + digit);
        }
        peer = {};
        peer.sin_family = AF_INET;
        peer.sin_port = htons(port);
#ifdef _WIN32
        if (InetPtonA(AF_INET, address, &peer.sin_addr) != 1)
#else
        if (inet_pton(AF_INET, address, &peer.sin_addr) != 1)
#endif
            return fail("Enter the host's IPv4 address");
        if (!open(game)) return false;
        key = parsed;
        hasPeer = true;
        status = "Connecting to host";
        return true;
    }

    void update(const StorylandNetworkPlayer& local, double now) {
        if (!active()) return;
        if (startedAt == 0) startedAt = now;
        // Fixed packet and per-frame limits bound work even on a public socket.
        for (int count = 0; count < 16; ++count) {
            ChatPacket buffer{};
            sockaddr_in sender{};
            AddressLength senderBytes = sizeof(sender);
            const int bytes = int(recvfrom(socketHandle, reinterpret_cast<char*>(buffer.data()),
                int(buffer.size()), ReceiveFlags, reinterpret_cast<sockaddr*>(&sender), &senderBytes));
            if (bytes == -1) {
                const int error = socketError();
                if (wouldBlock(error)) break;
                if (ignorableReceiveError(error)) continue;
                fail("Network receive failed");
                return;
            }
            if (hasPeer && (sender.sin_addr.s_addr != peer.sin_addr.s_addr || sender.sin_port != peer.sin_port)) continue;
            if (bytes == int(buffer.size())) {
                if (paired) {
                    if (get(buffer, 0) == 0x534c474du) receiveGameMessage(buffer);
                    else receiveChat(buffer);
                }
                continue;
            }
            Packet packet{};
            if (bytes == 312) {
                std::array<unsigned char,312> legacy{};
                std::memcpy(legacy.data(),buffer.data(),legacy.size());
                if (get(legacy,0)==0x53544C32u && get(legacy,8)==gameId &&
                    get(legacy,4)!=ProtocolVersion && authenticated(legacy)) {
                    fail("Co-op versions differ. Both players need the same Storyland update.");
                    return;
                }
                continue;
            }
            if (bytes == 284) {
                std::array<unsigned char,284> legacy{};
                std::memcpy(legacy.data(),buffer.data(),legacy.size());
                if (get(legacy,0)==0x53544C32u && get(legacy,8)==gameId &&
                    get(legacy,4)!=ProtocolVersion && authenticated(legacy)) {
                    fail("Co-op versions differ. Both players need the same Storyland update.");
                    return;
                }
                continue;
            }
            if (bytes == 276 && bytes != int(packet.size())) {
                std::array<unsigned char,276> legacy{};
                std::memcpy(legacy.data(),buffer.data(),legacy.size());
                if (get(legacy,0)==0x53544C32u && get(legacy,8)==gameId &&
                    get(legacy,4)!=ProtocolVersion && authenticated(legacy)) {
                    fail("Co-op versions differ. Both players need the same Storyland update.");
                    return;
                }
                continue;
            }
            if (bytes != int(packet.size())) continue;
            std::memcpy(packet.data(), buffer.data(), packet.size());
            if (get(packet, 0) != 0x53544C32u || get(packet, 8) != gameId) continue;
            std::array<unsigned char, 32> signature{};
            if (!sign(packet, signature)) { fail("Packet authentication failed"); return; }
            unsigned char difference = 0;
            for (size_t i = 0; i < signature.size(); ++i) difference |= signature[i] ^ packet[packet.size() - 32 + i];
            if (difference) continue;
            if (get(packet, 4) != ProtocolVersion) {
                fail("Co-op versions differ. Both players need the same Storyland update.");
                return;
            }
            const uint32_t sequence = get(packet, 12);
            if (paired && int32_t(sequence - receivedSequence) <= 0) continue;
            StorylandNetworkPlayer state;
            state.x = getFloat(packet, 16); state.y = getFloat(packet, 20); state.z = getFloat(packet, 24);
            state.heading = getFloat(packet, 28); state.animationTime = getFloat(packet, 32);
            state.motion = get(packet, 36);
            state.health = getFloat(packet, 40); state.armour = getFloat(packet, 44);
            state.recoveryTime = getFloat(packet, 48); state.flags = get(packet, 52);
            state.wanted = get(packet, 56);
            state.weapon = int32_t(get(packet, 60));
            state.shotSerial = uint32_t(get(packet, 64));
            state.life = uint32_t(get(packet, 68));
            state.actionFlags = uint32_t(get(packet, 72));
            state.elapsed = getFloat(packet, 76);
            state.phase = getFloat(packet, 80);
            state.moveForward = getFloat(packet, 84);
            state.moveRight = getFloat(packet, 88);
            state.aimX = getFloat(packet, 92);
            state.aimY = getFloat(packet, 96);
            state.aimZ = getFloat(packet, 100);
            state.actionTime = getFloat(packet, 104);
            state.animation = int32_t(get(packet, 108));
            state.animationGroup = int32_t(get(packet, 112));
            state.scriptTime = getFloat(packet, 116);
            state.vehicleModel = int32_t(get(packet, 120));
            state.seat = int32_t(get(packet, 124));
            state.vehicleAction = uint32_t(get(packet, 128));
            state.vehicleFraction = getFloat(packet, 132);
            state.carX = getFloat(packet, 136);
            state.carY = getFloat(packet, 140);
            state.carZ = getFloat(packet, 144);
            state.carHeading = getFloat(packet, 148);
            state.wheel = getFloat(packet, 152);
            state.rearWheel = getFloat(packet, 156);
            state.steering = getFloat(packet, 160);
            state.suspension = getFloat(packet, 164);
            state.roll = getFloat(packet, 168);
            state.pitch = getFloat(packet, 172);
            state.speed = getFloat(packet, 176);
            state.carHealth = getFloat(packet, 180);
            state.primaryColor = int32_t(get(packet, 184));
            state.secondaryColor = int32_t(get(packet, 188));
            state.carFlags = uint32_t(get(packet, 192));
            state.wheelPosition0 = getFloat(packet, 196);
            state.wheelPosition1 = getFloat(packet, 200);
            state.wheelPosition2 = getFloat(packet, 204);
            state.wheelPosition3 = getFloat(packet, 208);
            state.door0 = getFloat(packet, 212);
            state.door1 = getFloat(packet, 216);
            state.door2 = getFloat(packet, 220);
            state.door3 = getFloat(packet, 224);
            state.motionTime = getFloat(packet, 228);
            state.torsoX=getFloat(packet,232);state.torsoY=getFloat(packet,236);state.torsoZ=getFloat(packet,240);
            state.deathBlendRate=getFloat(packet,244);state.deathSpeed=getFloat(packet,248);
            for (size_t index=0; index<24 && packet[252+index]; ++index)
                state.radioStation.push_back(char(packet[252+index]));
            state.radioPosition=uint32_t(get(packet,276));
            state.maximumHealth=getFloat(packet,280); state.maximumArmour=getFloat(packet,284);
            state.model=int32_t(get(packet,288)); state.area=int32_t(get(packet,292));
            state.vehicleIdentity=get(packet,296); state.vehicleHandle=int32_t(get(packet,300));
            state.vehiclePhase=get(packet,304); state.sessionFlags=get(packet,308);
            state.guestStage=get(packet,312); state.gameHour=getFloat(packet,316);
            state.gameMilliseconds=get(packet,320); state.weather=int32_t(get(packet,324));
            if (!std::isfinite(state.maximumHealth) || state.maximumHealth <= 0 || state.maximumHealth > 100000 ||
                !std::isfinite(state.maximumArmour) || state.maximumArmour < 0 || state.maximumArmour > 100000 ||
                state.model < -1 || state.model > 100000 || state.area < 0 || state.area > 255 ||
                state.vehicleHandle < -1 || state.vehicleHandle > 1000000 || state.vehiclePhase > 16 ||
                state.sessionFlags > 1 || state.guestStage > 2 || !std::isfinite(state.gameHour) ||
                state.gameHour < 0 || state.gameHour >= 24 || state.weather < -1 || state.weather > 255) continue;
            if (state.radioPosition!=UINT32_MAX && state.radioPosition>86400000u) continue;
            if (!std::all_of(state.radioStation.begin(),state.radioStation.end(),[](unsigned char c) {
                return (c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='_' || c=='-';
            })) continue;
            if (!std::isfinite(state.x) || !std::isfinite(state.y) || !std::isfinite(state.z) ||
                !std::isfinite(state.heading) || !std::isfinite(state.animationTime) ||
                std::fabs(state.x) > 100000 || std::fabs(state.y) > 100000 || std::fabs(state.z) > 100000 ||
                std::fabs(state.heading) > 36000 || state.animationTime < 0 || state.animationTime > 1e8f || state.motion > 12 ||
                !std::isfinite(state.health) || state.health < 0 || state.health > 100000 ||
                !std::isfinite(state.armour) || state.armour < 0 || state.armour > 100000 ||
                !std::isfinite(state.recoveryTime) || state.recoveryTime < 0 || state.recoveryTime > 3600 ||
                state.flags > 15 || state.wanted > 6 || (state.dead() && state.busted())) continue;
            if (!std::isfinite(state.elapsed) || std::fabs(state.elapsed) > 100000000.0f ||
                !std::isfinite(state.phase) || std::fabs(state.phase) > 100000000.0f ||
                !std::isfinite(state.moveForward) || std::fabs(state.moveForward) > 1000000.0f ||
                !std::isfinite(state.moveRight) || std::fabs(state.moveRight) > 1000000.0f ||
                !std::isfinite(state.aimX) || std::fabs(state.aimX) > 1000000.0f ||
                !std::isfinite(state.aimY) || std::fabs(state.aimY) > 1000000.0f ||
                !std::isfinite(state.aimZ) || std::fabs(state.aimZ) > 1000000.0f ||
                !std::isfinite(state.actionTime) || std::fabs(state.actionTime) > 1000000.0f ||
                !std::isfinite(state.scriptTime) || std::fabs(state.scriptTime) > 100000000.0f ||
                !std::isfinite(state.vehicleFraction) || std::fabs(state.vehicleFraction) > 1000000.0f ||
                !std::isfinite(state.carX) || std::fabs(state.carX) > 1000000.0f ||
                !std::isfinite(state.carY) || std::fabs(state.carY) > 1000000.0f ||
                !std::isfinite(state.carZ) || std::fabs(state.carZ) > 1000000.0f ||
                !std::isfinite(state.carHeading) || std::fabs(state.carHeading) > 1000000.0f ||
                !std::isfinite(state.wheel) || std::fabs(state.wheel) > 1000000.0f ||
                !std::isfinite(state.rearWheel) || std::fabs(state.rearWheel) > 1000000.0f ||
                !std::isfinite(state.steering) || std::fabs(state.steering) > 1000000.0f ||
                !std::isfinite(state.suspension) || std::fabs(state.suspension) > 1000000.0f ||
                !std::isfinite(state.roll) || std::fabs(state.roll) > 1000000.0f ||
                !std::isfinite(state.pitch) || std::fabs(state.pitch) > 1000000.0f ||
                !std::isfinite(state.speed) || std::fabs(state.speed) > 1000000.0f ||
                !std::isfinite(state.carHealth) || std::fabs(state.carHealth) > 1000000.0f ||
                !std::isfinite(state.wheelPosition0) || std::fabs(state.wheelPosition0) > 1000000.0f ||
                !std::isfinite(state.wheelPosition1) || std::fabs(state.wheelPosition1) > 1000000.0f ||
                !std::isfinite(state.wheelPosition2) || std::fabs(state.wheelPosition2) > 1000000.0f ||
                !std::isfinite(state.wheelPosition3) || std::fabs(state.wheelPosition3) > 1000000.0f ||
                !std::isfinite(state.door0) || std::fabs(state.door0) > 1000000.0f ||
                !std::isfinite(state.door1) || std::fabs(state.door1) > 1000000.0f ||
                !std::isfinite(state.door2) || std::fabs(state.door2) > 1000000.0f ||
                !std::isfinite(state.door3) || std::fabs(state.door3) > 1000000.0f ||
                state.weapon < -1 || state.weapon > 255 ||
                !state.life ||
                state.actionFlags > 63 ||
                !std::isfinite(state.torsoX) || !std::isfinite(state.torsoY) || !std::isfinite(state.torsoZ) ||
                ((state.actionFlags&32) && std::hypot(state.torsoX-state.x,state.torsoY-state.y,state.torsoZ-state.z)>10) ||
                state.carFlags > 127 ||
                state.vehicleModel < -1 || state.vehicleModel > 100000 ||
                state.seat < -1 || state.seat > 7 ||
                state.vehicleAction > 3 ||
                state.vehicleFraction < 0 || state.vehicleFraction > 1 ||
                state.animation < -1 || state.animation > 4096 ||
                state.animationGroup < -1 || state.animationGroup > 4096 ||
                !std::isfinite(state.motionTime) || state.motionTime < 0 || state.motionTime > 1e8f ||
                !std::isfinite(state.deathBlendRate) || state.deathBlendRate < 0 || state.deathBlendRate > 1000 ||
                !std::isfinite(state.deathSpeed) || state.deathSpeed < 0 || state.deathSpeed > 1000 ||
                state.primaryColor < -1 || state.primaryColor > 255 ||
                state.secondaryColor < -1 || state.secondaryColor > 255) continue;
            previousRemote = paired ? display(now) : state;
            if (paired && now > lastReceived)
                snapshotInterval = std::clamp(snapshotInterval * .75 + (now-lastReceived) * .25, .025, .2);
            peer = sender;
            hasPeer = paired = true;
            remote = state;
            receivedSequence = sequence;
            lastReceived = now;
            status = "Connected: 2 players";
        }
        if ((paired && now - lastReceived > 10) || (!hosting && !paired && now - startedAt > 15)) {
            fail("Connection timed out. Host or join again.");
            return;
        }
        if (paired) {
            bool timedOut = false;
            const auto* message = chatChannel.next(now, timedOut);
            if (timedOut) { fail("Chat delivery timed out. Reconnect to continue."); return; }
            if (message) sendChatPacket(2, message->sequence, message->text);
            for (unsigned count = 0; count < 8; ++count) {
                const auto* gameMessage = gameChannel.next(now, timedOut);
                if (timedOut) { fail("Gameplay message delivery timed out. Reconnect to continue."); return; }
                if (!gameMessage) break;
                sendGamePacket(1, gameMessage->sequence, gameMessage->bytes);
            }
        }
        if (!hasPeer || now - lastSent < 0.05) return;
        Packet packet{};
        put(packet, 0, 0x53544C32u); put(packet, 4, ProtocolVersion); put(packet, 8, gameId);
        put(packet, 12, ++sentSequence);
        putFloat(packet, 16, local.x); putFloat(packet, 20, local.y); putFloat(packet, 24, local.z);
        putFloat(packet, 28, local.heading); putFloat(packet, 32, local.animationTime);
        put(packet, 36, local.motion);
        putFloat(packet, 40, local.health); putFloat(packet, 44, local.armour);
        putFloat(packet, 48, local.recoveryTime); put(packet, 52, local.flags);
        put(packet, 56, local.wanted);
        put(packet, 60, local.weapon);
        put(packet, 64, local.shotSerial);
        put(packet, 68, local.life);
        put(packet, 72, local.actionFlags);
        putFloat(packet, 76, local.elapsed);
        putFloat(packet, 80, local.phase);
        putFloat(packet, 84, local.moveForward);
        putFloat(packet, 88, local.moveRight);
        putFloat(packet, 92, local.aimX);
        putFloat(packet, 96, local.aimY);
        putFloat(packet, 100, local.aimZ);
        putFloat(packet, 104, local.actionTime);
        put(packet, 108, local.animation);
        put(packet, 112, local.animationGroup);
        putFloat(packet, 116, local.scriptTime);
        put(packet, 120, local.vehicleModel);
        put(packet, 124, local.seat);
        put(packet, 128, local.vehicleAction);
        putFloat(packet, 132, local.vehicleFraction);
        putFloat(packet, 136, local.carX);
        putFloat(packet, 140, local.carY);
        putFloat(packet, 144, local.carZ);
        putFloat(packet, 148, local.carHeading);
        putFloat(packet, 152, local.wheel);
        putFloat(packet, 156, local.rearWheel);
        putFloat(packet, 160, local.steering);
        putFloat(packet, 164, local.suspension);
        putFloat(packet, 168, local.roll);
        putFloat(packet, 172, local.pitch);
        putFloat(packet, 176, local.speed);
        putFloat(packet, 180, local.carHealth);
        put(packet, 184, local.primaryColor);
        put(packet, 188, local.secondaryColor);
        put(packet, 192, local.carFlags);
        putFloat(packet, 196, local.wheelPosition0);
        putFloat(packet, 200, local.wheelPosition1);
        putFloat(packet, 204, local.wheelPosition2);
        putFloat(packet, 208, local.wheelPosition3);
        putFloat(packet, 212, local.door0);
        putFloat(packet, 216, local.door1);
        putFloat(packet, 220, local.door2);
        putFloat(packet, 224, local.door3);
        putFloat(packet, 228, local.motionTime);
        putFloat(packet,232,local.torsoX);putFloat(packet,236,local.torsoY);putFloat(packet,240,local.torsoZ);
        putFloat(packet,244,local.deathBlendRate);putFloat(packet,248,local.deathSpeed);
        if (local.radioStation.size()<=24 && std::all_of(local.radioStation.begin(),local.radioStation.end(),[](unsigned char c) {
            return (c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='_' || c=='-';
        })) std::memcpy(packet.data()+252,local.radioStation.data(),local.radioStation.size());
        put(packet,276,local.radioPosition<=86400000u ? local.radioPosition : UINT32_MAX);
        putFloat(packet,280,local.maximumHealth); putFloat(packet,284,local.maximumArmour);
        put(packet,288,uint32_t(local.model)); put(packet,292,uint32_t(local.area));
        put(packet,296,local.vehicleIdentity); put(packet,300,uint32_t(local.vehicleHandle));
        put(packet,304,local.vehiclePhase); put(packet,308,friendlyFire() ? 1u : 0u);
        put(packet,312,guestStage()); putFloat(packet,316,local.gameHour);
        put(packet,320,local.gameMilliseconds); put(packet,324,uint32_t(local.weather));
        std::array<unsigned char, 32> signature{};
        if (!sign(packet, signature)) { fail("Packet signing failed"); return; }
        std::memcpy(packet.data() + packet.size() - 32, signature.data(), signature.size());
        const int sent = sendto(socketHandle, reinterpret_cast<const char*>(packet.data()), int(packet.size()),
            0, reinterpret_cast<const sockaddr*>(&peer), sizeof(peer));
        if (sent == -1 && !wouldBlock(socketError())) { fail("Network send failed"); return; }
        lastSent = now;
    }

private:
    StorylandNetworkPlayer previousRemote;
    using Packet = std::array<unsigned char, 360>;
    using ChatPacket = std::array<unsigned char, 1024>;
    StorylandChatChannel chatChannel;
    StorylandGameChannel gameChannel;
    void sendGamePacket(uint32_t kind, uint32_t sequence, const std::vector<uint8_t>& bytes) {
        ChatPacket packet{};
        put(packet, 0, 0x534c474du); put(packet, 4, ProtocolVersion); put(packet, 8, gameId);
        put(packet, 12, sequence); put(packet, 16, kind); put(packet, 20, uint32_t(bytes.size()));
        if (!bytes.empty()) std::memcpy(packet.data() + 24, bytes.data(), bytes.size());
        std::array<unsigned char, 32> signature{};
        if (!sign(packet, signature)) return;
        std::memcpy(packet.data() + packet.size() - 32, signature.data(), signature.size());
        sendto(socketHandle, reinterpret_cast<const char*>(packet.data()), int(packet.size()), 0,
            reinterpret_cast<const sockaddr*>(&peer), sizeof(peer));
    }
    void receiveGameMessage(const ChatPacket& packet) {
        if (get(packet, 4) != ProtocolVersion || get(packet, 8) != gameId) return;
        std::array<unsigned char, 32> signature{};
        if (!sign(packet, signature)) return;
        unsigned char difference = 0;
        for (size_t i = 0; i < signature.size(); ++i) difference |= signature[i] ^ packet[packet.size() - 32 + i];
        if (difference) return;
        const uint32_t sequence = get(packet, 12), kind = get(packet, 16), length = get(packet, 20);
        if (kind == 2 && length == 0) gameChannel.acknowledge(sequence);
        else if (kind == 1 && length <= StorylandGameChannel::MaximumPayload &&
            gameChannel.receive(sequence, packet.data() + 24, length)) sendGamePacket(2, sequence, {});
    }
    void sendChatPacket(uint32_t kind, uint32_t sequence, const std::string& text) {
        ChatPacket packet{};
        put(packet, 0, 0x534c4348u); put(packet, 4, ProtocolVersion); put(packet, 8, gameId);
        put(packet, 12, sequence); put(packet, 16, kind); put(packet, 20, uint32_t(text.size()));
        std::memcpy(packet.data() + 24, text.data(), text.size());
        std::array<unsigned char, 32> signature{};
        if (!sign(packet, signature)) return;
        std::memcpy(packet.data() + packet.size() - 32, signature.data(), signature.size());
        sendto(socketHandle, reinterpret_cast<const char*>(packet.data()), int(packet.size()), 0,
            reinterpret_cast<const sockaddr*>(&peer), sizeof(peer));
    }
    void receiveChat(const ChatPacket& packet) {
        if (get(packet, 0) != 0x534c4348u || get(packet, 4) != ProtocolVersion || get(packet, 8) != gameId) return;
        std::array<unsigned char, 32> signature{};
        if (!sign(packet, signature)) return;
        unsigned char difference = 0;
        for (size_t i = 0; i < signature.size(); ++i) difference |= signature[i] ^ packet[packet.size() - 32 + i];
        if (difference) return;
        const uint32_t sequence = get(packet, 12), kind = get(packet, 16), length = get(packet, 20);
        if (kind == 3 && length == 0) {
            chatChannel.acknowledge(sequence);
        } else if (kind == 2 && length > 0 && length <= 768) {
            const std::string text(reinterpret_cast<const char*>(packet.data() + 24), length);
            if (chatChannel.receive(sequence, text)) sendChatPacket(3, sequence, {});
        }
    }
#ifdef _WIN32
    using Socket = SOCKET;
    using AddressLength = int;
    static constexpr Socket InvalidSocket = INVALID_SOCKET;
    static constexpr int ReceiveFlags = 0;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    bool winsockReady = false;
    static int socketError() { return WSAGetLastError(); }
    static bool wouldBlock(int error) { return error == WSAEWOULDBLOCK; }
    static bool ignorableReceiveError(int error) { return error == WSAEMSGSIZE || error == WSAECONNRESET; }
#else
    using Socket = int;
    using AddressLength = socklen_t;
    static constexpr Socket InvalidSocket = -1;
    static constexpr int ReceiveFlags = MSG_TRUNC;
    static int socketError() { return errno; }
    static bool wouldBlock(int error) { return error == EAGAIN || error == EWOULDBLOCK; }
    static bool ignorableReceiveError(int error) { return error == EMSGSIZE || error == ECONNRESET || error == EINTR; }
#endif
#ifdef STORYLAND_USE_AV_HMAC
    AVHMAC* hmac = nullptr;
#endif
    Socket socketHandle = InvalidSocket;
    bool hosting = false, paired = false, hasPeer = false;
    bool configuredFriendlyFire = false;
    uint32_t configuredGuestStage = 0;
    double snapshotInterval = .05;
    sockaddr_in peer{};
    std::array<unsigned char, 16> key{};
    uint32_t gameId = 0, sentSequence = 0, receivedSequence = 0;
    double lastReceived = 0, lastSent = 0, startedAt = 0;
    bool fail(const char* text) { disconnect(); status = text; return false; }
    bool open(uint32_t game) {
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data)) return fail("Winsock initialization failed");
        winsockReady = true;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0)
            return fail("SHA-256 initialization failed");
#elif defined(STORYLAND_USE_AV_HMAC)
        hmac = av_hmac_alloc(AV_HMAC_SHA256);
        if (!hmac) return fail("SHA-256 initialization failed");
#endif
        socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketHandle == InvalidSocket) return fail("Cannot create network socket");
#ifdef _WIN32
        u_long nonblocking = 1;
        if (ioctlsocket(socketHandle, FIONBIO, &nonblocking)) return fail("Cannot set nonblocking socket");
#else
        const int flags = fcntl(socketHandle, F_GETFL, 0);
        if (flags < 0 || fcntl(socketHandle, F_SETFL, flags | O_NONBLOCK) < 0)
            return fail("Cannot set nonblocking socket");
#endif
        gameId = game;
        chatChannel.reset();
        return true;
    }
    template<size_t N> bool sign(const std::array<unsigned char, N>& packet, std::array<unsigned char, 32>& signature) {
#ifdef _WIN32
        BCRYPT_HASH_HANDLE hash = nullptr;
        if (BCryptCreateHash(algorithm, &hash, nullptr, 0, key.data(), ULONG(key.size()), 0) < 0) return false;
        const bool success = BCryptHashData(hash, const_cast<PUCHAR>(packet.data()), ULONG(N - 32), 0) >= 0 &&
            BCryptFinishHash(hash, signature.data(), ULONG(signature.size()), 0) >= 0;
        BCryptDestroyHash(hash);
        return success;
#elif defined(STORYLAND_USE_AV_HMAC)
        return av_hmac_calc(hmac, packet.data(), N - 32, key.data(), key.size(),
            signature.data(), signature.size()) == int(signature.size());
#else
        unsigned int length = 0;
        return HMAC(EVP_sha256(), key.data(), int(key.size()), packet.data(), N - 32,
            signature.data(), &length) != nullptr && length == signature.size();
#endif
    }
    template<size_t N> bool authenticated(const std::array<unsigned char,N>& packet) {
        std::array<unsigned char,32> signature{};
        if(!sign(packet,signature))return false;
        unsigned char difference=0;
        for(size_t index=0;index<signature.size();++index)
            difference|=signature[index]^packet[N-signature.size()+index];
        return difference==0;
    }
    template<size_t N> static void put(std::array<unsigned char, N>& packet, size_t offset, uint32_t value) {
        for (size_t i = 0; i < 4; ++i) packet[offset + i] = static_cast<unsigned char>(value >> (24 - i * 8));
    }
    template<size_t N> static uint32_t get(const std::array<unsigned char, N>& packet, size_t offset) {
        uint32_t value = 0;
        for (size_t i = 0; i < 4; ++i) value = (value << 8) | packet[offset + i];
        return value;
    }
    static void putFloat(Packet& packet, size_t offset, float value) {
        uint32_t bits; std::memcpy(&bits, &value, 4); put(packet, offset, bits);
    }
    static float getFloat(const Packet& packet, size_t offset) {
        const uint32_t bits = get(packet, offset); float value; std::memcpy(&value, &bits, 4); return value;
    }
};
