#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <propvarutil.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace storylandWindowsAudio {
struct Session {
    uint32_t processId = 0;
    AudioSessionState state = AudioSessionStateInactive;
    bool stateKnown = false;
    bool muteKnown = false;
    bool muted = false;
    bool volumeKnown = false;
    float volume = 0;
    bool repairedMute = false;
    bool repairedVolume = false;
    bool channelVolumesKnown = false;
    std::vector<float> channelVolumes;
    bool repairedChannels = false;
    bool peakKnown = false;
    float peak = 0;
};

struct Endpoint {
    std::wstring id;
    std::wstring name;
    unsigned defaultRoles = 0;
    bool muteKnown = false;
    bool muted = false;
    bool volumeKnown = false;
    float volume = 0;
    bool peakKnown = false;
    float peak = 0;
    std::vector<Session> sessions;
};

inline std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(size_t(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()),
        result.data(), count, nullptr, nullptr);
    return result;
}

inline std::string hresult(HRESULT result) {
    std::ostringstream text;
    text << "HRESULT 0x" << std::hex << std::uppercase << std::setfill('0') <<
        std::setw(8) << uint32_t(result);
    return text.str();
}

struct Report {
    uint32_t processId = 0;
    unsigned repairedSessions = 0;
    std::vector<Endpoint> endpoints;
    std::vector<std::string> errors;

    std::string text() const {
        std::ostringstream output;
        output << "Core Audio process " << processId << ": " << endpoints.size() <<
            " active render endpoints, " << repairedSessions << " own sessions changed";
        for (const auto& endpoint : endpoints) {
            output << "\nEndpoint: " << utf8(endpoint.name) << " | " << utf8(endpoint.id) << " | defaults:";
            if (endpoint.defaultRoles & 1u) output << " console";
            if (endpoint.defaultRoles & 2u) output << " multimedia";
            if (endpoint.defaultRoles & 4u) output << " communications";
            if (!endpoint.defaultRoles) output << " none";
            output << " | mute=";
            if (endpoint.muteKnown) output << endpoint.muted; else output << "unknown";
            output << " | volume=";
            if (endpoint.volumeKnown) output << endpoint.volume; else output << "unknown";
            output << " | peak before endpoint volume=";
            if (endpoint.peakKnown) output << endpoint.peak; else output << "unknown";
            output << " | own sessions=" << endpoint.sessions.size();
            for (const auto& session : endpoint.sessions) {
                output << "\n  PID " << session.processId << " state=";
                if (session.stateKnown) output << unsigned(session.state); else output << "unknown";
                output << " mute=";
                if (session.muteKnown) output << session.muted; else output << "unknown";
                output << " volume=";
                if (session.volumeKnown) output << session.volume; else output << "unknown";
                output << " channels=";
                if (session.channelVolumesKnown) {
                    output << '[';
                    for (size_t index = 0; index < session.channelVolumes.size(); ++index) {
                        if (index) output << ',';
                        output << session.channelVolumes[index];
                    }
                    output << ']';
                } else output << "unknown";
                output << " peak=";
                if (session.peakKnown) output << session.peak; else output << "unknown";
                if (session.repairedChannels) output << " restored all-zero session channels";
                if (session.repairedMute) output << " unmuted";
                if (session.repairedVolume) output << " restored zero session volume";
            }
        }
        for (const auto& error : errors) output << "\n" << error;
        return output.str();
    }
};

template<class InterfaceType>
class Interface {
public:
    Interface() = default;
    ~Interface() { if (value) value->Release(); }
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    InterfaceType* operator->() const { return value; }
    InterfaceType* get() const { return value; }
    explicit operator bool() const { return value != nullptr; }
    InterfaceType** receive() {
        if (value) value->Release();
        value = nullptr;
        return &value;
    }
private:
    InterfaceType* value = nullptr;
};

class Apartment {
public:
    Apartment() : result(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ~Apartment() { if (SUCCEEDED(result)) CoUninitialize(); }
    bool available() const { return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE; }
    HRESULT result;
};

inline void failure(Report& report, const std::string& operation, HRESULT result) {
    report.errors.push_back(operation + ": " + hresult(result));
}

struct SessionRepair {
    bool unmute = false;
    bool restoreVolume = false;
};

inline SessionRepair ownSessionRepair(const Session& session, uint32_t processId) {
    if (!processId || session.processId != processId) return {};
    return {session.muteKnown && session.muted, session.volumeKnown && session.volume == 0.0f};
}

inline void repairSession(ISimpleAudioVolume* volume, Session& session, Report& report) {
    const auto action = ownSessionRepair(session, report.processId);
    bool changed = false;
    if (action.unmute) {
        HRESULT result = volume->SetMute(FALSE, nullptr);
        if (FAILED(result)) failure(report, "Own-session SetMute", result);
        else {
            BOOL muted = FALSE;
            result = volume->GetMute(&muted);
            session.muteKnown = SUCCEEDED(result);
            if (SUCCEEDED(result)) {
                session.muted = muted != FALSE;
                session.repairedMute = !session.muted;
                changed = session.repairedMute;
            } else failure(report, "Own-session mute readback", result);
        }
    }
    if (action.restoreVolume) {
        HRESULT result = volume->SetMasterVolume(1.0f, nullptr);
        if (FAILED(result)) failure(report, "Own-session SetMasterVolume", result);
        else {
            result = volume->GetMasterVolume(&session.volume);
            session.volumeKnown = SUCCEEDED(result);
            if (SUCCEEDED(result)) {
                session.repairedVolume = session.volume > 0;
                changed = changed || session.repairedVolume;
            } else failure(report, "Own-session volume readback", result);
        }
    }
    if (changed) ++report.repairedSessions;
}

inline bool ownSessionChannelsNeedRepair(const Session& session, uint32_t processId) {
    return processId != 0 && session.processId == processId && session.channelVolumesKnown &&
        !session.channelVolumes.empty() && std::all_of(session.channelVolumes.begin(),
            session.channelVolumes.end(), [](float volume) { return volume == 0.0f; });
}

inline void readSessionChannels(IChannelAudioVolume* volume, Session& session, Report& report) {
    session.channelVolumesKnown = false;
    session.channelVolumes.clear();
    UINT32 count = 0;
    HRESULT result = volume->GetChannelCount(&count);
    if (result != S_OK || count == 0) {
        failure(report, "Read own-session channel count", result != S_OK ? result : E_UNEXPECTED);
        return;
    }
    std::vector<float> values(count);
    result = volume->GetAllVolumes(count, values.data());
    if (result != S_OK) {
        failure(report, "Read own-session channel volumes", result);
        return;
    }
    if (!std::all_of(values.begin(), values.end(), [](float value) {
        return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
    })) {
        failure(report, "Read own-session channel volumes", E_UNEXPECTED);
        return;
    }
    session.channelVolumes = std::move(values);
    session.channelVolumesKnown = true;
}

inline void repairSessionChannels(IChannelAudioVolume* volume, Session& session, Report& report) {
    if (!ownSessionChannelsNeedRepair(session, report.processId)) return;
    const bool previouslyRepaired = session.repairedMute || session.repairedVolume || session.repairedChannels;
    const std::vector<float> restored(session.channelVolumes.size(), 1.0f);
    const HRESULT result = volume->SetAllVolumes(UINT32(restored.size()), restored.data(), nullptr);
    if (result != S_OK) {
        failure(report, "Own-session SetAllVolumes", result);
        return;
    }
    readSessionChannels(volume, session, report);
    session.repairedChannels = session.channelVolumesKnown && !session.channelVolumes.empty() &&
        std::all_of(session.channelVolumes.begin(), session.channelVolumes.end(),
            [](float value) { return value == 1.0f; });
    if (session.repairedChannels && !previouslyRepaired) ++report.repairedSessions;
}

#ifdef __IAudioMeterInformation_INTERFACE_DEFINED__
inline void readPeak(IAudioMeterInformation* meter, bool& known, float& peak,
    Report& report, const char* operation) {
    known = false;
    const HRESULT result = meter->GetPeakValue(&peak);
    if (result != S_OK) failure(report, operation, result);
    else if (!std::isfinite(peak) || peak < 0.0f || peak > 1.0f)
        failure(report, operation, E_UNEXPECTED);
    else known = true;
}
#endif

inline Report collect(bool repair) {
    Report report;
    report.processId = GetCurrentProcessId();
    Apartment apartment;
    if (!apartment.available()) {
        failure(report, "Core Audio COM initialization", apartment.result);
        return report;
    }
    Interface<IMMDeviceEnumerator> enumerator;
    HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.receive()));
    if (FAILED(result)) {
        failure(report, "Create audio device enumerator", result);
        return report;
    }
    std::wstring defaults[3];
    for (unsigned role = 0; role < 3; ++role) {
        Interface<IMMDevice> device;
        result = enumerator->GetDefaultAudioEndpoint(eRender, ERole(role), device.receive());
        if (FAILED(result)) {
            failure(report, "Default render endpoint role " + std::to_string(role), result);
            continue;
        }
        LPWSTR id = nullptr;
        result = device->GetId(&id);
        if (SUCCEEDED(result) && id) defaults[role] = id;
        else failure(report, "Default endpoint ID role " + std::to_string(role), result);
        CoTaskMemFree(id);
    }
    Interface<IMMDeviceCollection> devices;
    result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, devices.receive());
    if (FAILED(result)) {
        failure(report, "Enumerate active render endpoints", result);
        return report;
    }
    UINT count = 0;
    result = devices->GetCount(&count);
    if (FAILED(result)) {
        failure(report, "Count active render endpoints", result);
        return report;
    }
    for (UINT index = 0; index < count; ++index) {
        Interface<IMMDevice> device;
        result = devices->Item(index, device.receive());
        if (FAILED(result)) {
            failure(report, "Read render endpoint " + std::to_string(index), result);
            continue;
        }
        Endpoint endpoint;
        LPWSTR id = nullptr;
        result = device->GetId(&id);
        if (SUCCEEDED(result) && id) endpoint.id = id;
        else failure(report, "Read endpoint ID " + std::to_string(index), result);
        CoTaskMemFree(id);
        for (unsigned role = 0; role < 3; ++role)
            if (!endpoint.id.empty() && endpoint.id == defaults[role]) endpoint.defaultRoles |= 1u << role;
        Interface<IPropertyStore> properties;
        result = device->OpenPropertyStore(STGM_READ, properties.receive());
        if (SUCCEEDED(result)) {
            PROPVARIANT value;
            PropVariantInit(&value);
            const PROPERTYKEY friendlyNameKey = {
                {0xA45C254E, 0xDF1C, 0x4EFD, {0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0}}, 14};
            result = properties->GetValue(friendlyNameKey, &value);
            if (SUCCEEDED(result) && value.vt == VT_LPWSTR && value.pwszVal) endpoint.name = value.pwszVal;
            else failure(report, "Read endpoint friendly name " + std::to_string(index),
                FAILED(result) ? result : DISP_E_TYPEMISMATCH);
            PropVariantClear(&value);
        } else failure(report, "Open endpoint property store " + std::to_string(index), result);
        Interface<IAudioEndpointVolume> endpointVolume;
        result = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(endpointVolume.receive()));
        if (SUCCEEDED(result)) {
            BOOL muted = FALSE;
            result = endpointVolume->GetMute(&muted);
            endpoint.muteKnown = SUCCEEDED(result);
            if (SUCCEEDED(result)) endpoint.muted = muted != FALSE;
            else failure(report, "Read endpoint mute " + std::to_string(index), result);
            result = endpointVolume->GetMasterVolumeLevelScalar(&endpoint.volume);
            endpoint.volumeKnown = SUCCEEDED(result);
            if (FAILED(result)) failure(report, "Read endpoint volume " + std::to_string(index), result);
        } else failure(report, "Activate endpoint volume " + std::to_string(index), result);
#ifdef __IAudioMeterInformation_INTERFACE_DEFINED__
        Interface<IAudioMeterInformation> endpointMeter;
        result = device->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(endpointMeter.receive()));
        if (SUCCEEDED(result)) readPeak(endpointMeter.get(), endpoint.peakKnown,
            endpoint.peak, report, "Read endpoint peak before endpoint volume");
#endif
        Interface<IAudioSessionManager2> manager;
        result = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(manager.receive()));
        if (SUCCEEDED(result)) {
            Interface<IAudioSessionEnumerator> sessions;
            result = manager->GetSessionEnumerator(sessions.receive());
            if (SUCCEEDED(result)) {
                int sessionCount = 0;
                result = sessions->GetCount(&sessionCount);
                if (FAILED(result)) failure(report, "Count endpoint sessions " + std::to_string(index), result);
                for (int sessionIndex = 0; SUCCEEDED(result) && sessionIndex < sessionCount; ++sessionIndex) {
                    Interface<IAudioSessionControl> control;
                    HRESULT sessionResult = sessions->GetSession(sessionIndex, control.receive());
                    if (FAILED(sessionResult)) {
                        failure(report, "Read audio session", sessionResult);
                        continue;
                    }
                    Interface<IAudioSessionControl2> control2;
                    sessionResult = control->QueryInterface(__uuidof(IAudioSessionControl2),
                        reinterpret_cast<void**>(control2.receive()));
                    if (FAILED(sessionResult)) {
                        failure(report, "Query audio session process interface", sessionResult);
                        continue;
                    }
                    DWORD processId = 0;
                    sessionResult = control2->GetProcessId(&processId);
                    if (FAILED(sessionResult)) {
                        failure(report, "Read audio session process ID", sessionResult);
                        continue;
                    }
                    if (processId != report.processId) continue;
                    if (sessionResult != S_OK) {
                        failure(report, "Own-session process ownership is not exclusive", sessionResult);
                        continue;
                    }
                    Session session;
                    session.processId = processId;
                    sessionResult = control2->GetState(&session.state);
                    session.stateKnown = SUCCEEDED(sessionResult);
                    if (FAILED(sessionResult)) failure(report, "Read own-session state", sessionResult);
                    Interface<ISimpleAudioVolume> volume;
                    sessionResult = control2->QueryInterface(__uuidof(ISimpleAudioVolume),
                        reinterpret_cast<void**>(volume.receive()));
                    if (SUCCEEDED(sessionResult)) {
                        BOOL muted = FALSE;
                        sessionResult = volume->GetMute(&muted);
                        session.muteKnown = SUCCEEDED(sessionResult);
                        if (SUCCEEDED(sessionResult)) session.muted = muted != FALSE;
                        else failure(report, "Read own-session mute", sessionResult);
                        sessionResult = volume->GetMasterVolume(&session.volume);
                        session.volumeKnown = SUCCEEDED(sessionResult);
                        if (FAILED(sessionResult)) failure(report, "Read own-session volume", sessionResult);
                        if (repair) repairSession(volume.get(), session, report);
                    } else failure(report, "Query own-session volume interface", sessionResult);
                    Interface<IChannelAudioVolume> channels;
                    sessionResult = control2->QueryInterface(__uuidof(IChannelAudioVolume),
                        reinterpret_cast<void**>(channels.receive()));
                    if (SUCCEEDED(sessionResult)) {
                        readSessionChannels(channels.get(), session, report);
                        if (repair) repairSessionChannels(channels.get(), session, report);
                    } else failure(report, "Query own-session channel volume interface", sessionResult);
#ifdef __IAudioMeterInformation_INTERFACE_DEFINED__
                    Interface<IAudioMeterInformation> meter;
                    sessionResult = control2->QueryInterface(__uuidof(IAudioMeterInformation),
                        reinterpret_cast<void**>(meter.receive()));
                    if (SUCCEEDED(sessionResult)) readPeak(meter.get(), session.peakKnown,
                        session.peak, report, "Read own-session peak");
#endif
                    endpoint.sessions.push_back(std::move(session));
                }
            } else failure(report, "Enumerate endpoint sessions " + std::to_string(index), result);
        } else failure(report, "Activate endpoint session manager " + std::to_string(index), result);
        report.endpoints.push_back(std::move(endpoint));
    }
    return report;
}

inline void recordOutputDiagnostic(const std::string& message) {
    OutputDebugStringA(("Storyland audio: "+message+"\n").c_str());
    const DWORD size=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);
    if(size<2||size>32768)return;
    std::wstring directory(size,L'\0');
    const DWORD copied=GetEnvironmentVariableW(L"LOCALAPPDATA",directory.data(),size);
    if(!copied||copied>=size)return;
    directory.resize(copied);
    const auto folder=std::filesystem::path(directory)/L"Storyland";
    std::error_code error;std::filesystem::create_directories(folder,error);
    if(error)return;
    std::ofstream file(folder/L"program_audio.log",std::ios::app);
    if(file)file<<message<<'\n';
}

inline bool recoveredOwnSession(const Report& report) {
    if (!report.processId) return false;
    for (const auto& endpoint : report.endpoints) for (const auto& session : endpoint.sessions) {
        if (session.processId != report.processId || !session.muteKnown || session.muted ||
            !session.volumeKnown || !std::isfinite(session.volume) ||
            session.volume <= 0.0f || session.volume > 1.0f ||
            !session.channelVolumesKnown || session.channelVolumes.empty()) continue;
        const bool validChannels = std::all_of(session.channelVolumes.begin(),
            session.channelVolumes.end(), [](float volume) {
                return std::isfinite(volume) && volume >= 0.0f && volume <= 1.0f;
            });
        const bool enabledChannel = std::any_of(session.channelVolumes.begin(),
            session.channelVolumes.end(), [](float volume) { return volume > 0.0f; });
        if (validChannels && enabledChannel) return true;
    }
    return false;
}

inline void recoverOutputSessionOnce(const char* backend) {
    static std::mutex mutex;
    static unsigned attempts = 0;
    static bool completed = false;
    std::lock_guard<std::mutex> guard(mutex);
    if (completed || attempts >= 3) return;
    ++attempts;
    const auto report = collect(true);
    completed = recoveredOwnSession(report);
    recordOutputDiagnostic(std::string("Output backend: ") + (backend ? backend : "unknown") +
        " | session recovery attempt " + std::to_string(attempts) + "/3\n" + report.text());
}

inline Report inspectOwnSessions() { return collect(false); }
inline Report repairOwnSession() { return collect(true); }
}
#endif
