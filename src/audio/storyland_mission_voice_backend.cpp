#include "src/audio/storyland_mission_voice_backend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace storylandMissionVoiceBackendDetail {

// The public eSpeak NG speak_lib.h ABI, revision 12 (GPL-3.0-or-later).
struct EspeakEvent {
    int type;
    unsigned int identifier;
    int textPosition;
    int length;
    int audioPosition;
    int sample;
    void* userData;
    union {
        int number;
        const char* name;
        char text[8];
    } id;
};

struct EspeakVoice {
    const char* name;
    const char* languages;
    const char* identifier;
    unsigned char gender;
    unsigned char age;
    unsigned char variant;
    unsigned char reserved;
    int score;
    void* spare;
};

using SynthCallback = int (*)(short*, int, EspeakEvent*);
using Initialize = int (*)(int, int, const char*, int);
using SetSynthCallback = void (*)(SynthCallback);
using SetVoiceByName = int (*)(const char*);
using ListVoices = const EspeakVoice** (*)(EspeakVoice*);
using SetParameter = int (*)(int, int, int);
using Synth = int (*)(const void*, std::size_t, unsigned int, int, unsigned int,
                      unsigned int, unsigned int*, void*);
using Synchronize = int (*)();
using Cancel = int (*)();
using Terminate = int (*)();
using Info = const char* (*)(const char**);

constexpr int synchronousOutput = 2;
constexpr int doNotExitOnMissingData = 0x8000;
constexpr int positionInCharacters = 1;
constexpr unsigned int utf8Text = 1;
constexpr unsigned int endPause = 0x1000;
constexpr std::size_t maximumDurationSeconds = 120;

struct VoiceBinding {
    std::string_view profile;
    const char* english;
    const char* variant;
};

constexpr std::array<VoiceBinding, 18> voiceBindings = {{
    {"vcs.victor_vance", "en-us", "m3"},
    {"vcs.lance_vance", "en-us", "m2"},
    {"vcs.jerry_martinez", "en-us", "m4"},
    {"vcs.phil_cassidy", "en-us", "m7"},
    {"vcs.louise_cassidy_williams", "en-us", "f3"},
    {"vcs.marty_jay_williams", "en-us", "croak"},
    {"vcs.armando_mendez", "en-us", "klatt"},
    {"vcs.diego_mendez", "en-us", "klatt2"},
    {"vcs.ricardo_diaz", "en-us", "m5"},
    {"vcs.reni_wassulmaier", "en-us", "f4"},
    {"vcs.umberto_robina", "en-us", "m6"},
    {"vcs.bryan_forbes", "en-us", "klatt3"},
    {"vcs.gonzalez", "en-us", "klatt4"},
    {"vcs.barry_mickelthwaite", "en", "klatt5"},
    {"vcs.mary_jo_cassidy", "en-us", "f2"},
    {"vcs.alberto_robina", "en-us", "m1"},
    {"vcs.janet_vance", "en-us", "f5"},
    {"vcs.phil_collins_standin", "en", "klatt6"}
}};

enum class CaptureFailure {
    None,
    Cancelled,
    OutputLimit,
    InvalidBuffer,
    SampleRateChanged,
    AllocationFailure
};

struct Capture {
    std::vector<std::int16_t> samples;
    const std::atomic_bool* cancelled = nullptr;
    std::uint32_t sampleRate = 0;
    std::size_t maximumSamples = 0;
    CaptureFailure failure = CaptureFailure::None;
};

std::atomic<Capture*> activeCapture{nullptr};

bool cancellationRequested(const std::atomic_bool* cancelled) noexcept {
    return cancelled != nullptr && cancelled->load(std::memory_order_relaxed);
}

int collectSamples(short* samples, int count, EspeakEvent* events) noexcept {
    static_assert(sizeof(short) == sizeof(std::int16_t), "eSpeak requires 16-bit PCM.");
    Capture* capture = activeCapture.load(std::memory_order_acquire);
    if (capture == nullptr) {
        return 1;
    }
    if (cancellationRequested(capture->cancelled)) {
        capture->failure = CaptureFailure::Cancelled;
        return 1;
    }
    if (capture->failure != CaptureFailure::None) {
        return 1;
    }
    if (count < 0 || (count > 0 && samples == nullptr)) {
        capture->failure = CaptureFailure::InvalidBuffer;
        return 1;
    }
    if (events != nullptr) {
        for (const EspeakEvent* event = events; event->type != 0; ++event) {
            if (event->type == 8 && event->id.number != static_cast<int>(capture->sampleRate)) {
                capture->failure = CaptureFailure::SampleRateChanged;
                return 1;
            }
        }
    }
    if (count == 0) {
        return 0;
    }
    const auto amount = static_cast<std::size_t>(count);
    if (amount > capture->maximumSamples - capture->samples.size()) {
        capture->failure = CaptureFailure::OutputLimit;
        return 1;
    }
    try {
        capture->samples.insert(capture->samples.end(), samples, samples + amount);
    } catch (...) {
        capture->failure = CaptureFailure::AllocationFailure;
        return 1;
    }
    return 0;
}

std::filesystem::path environmentPath(const char* name) {
#if defined(_WIN32)
    const std::wstring wideName(name, name + std::strlen(name));
    const DWORD required = GetEnvironmentVariableW(wideName.c_str(), nullptr, 0);
    if (required == 0) {
        return {};
    }
    std::wstring value(required, L'\0');
    const DWORD length = GetEnvironmentVariableW(wideName.c_str(), value.data(), required);
    if (length == 0 || length >= required) {
        return {};
    }
    value.resize(length);
    return std::filesystem::path(value);
#else
    const char* value = std::getenv(name);
    return value != nullptr ? std::filesystem::u8path(value) : std::filesystem::path{};
#endif
}

std::filesystem::path executableDirectory() {
#if defined(_WIN32)
    std::array<wchar_t, 32768> path{};
    const DWORD count = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (count > 0 && count < path.size()) {
        return std::filesystem::path(std::wstring(path.data(), count)).parent_path();
    }
#elif defined(__APPLE__)
    std::array<char, 4096> path{};
    std::uint32_t size = static_cast<std::uint32_t>(path.size());
    if (_NSGetExecutablePath(path.data(), &size) == 0) {
        return std::filesystem::u8path(path.data()).parent_path();
    }
#else
    std::error_code error;
    const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        return path.parent_path();
    }
#endif
    return {};
}

bool isFile(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

std::filesystem::path dataDirectory(const std::filesystem::path& candidate) {
    if (candidate.empty()) {
        return {};
    }
    for (const auto& path : {candidate, candidate / "espeak-ng-data"}) {
        if (isFile(path / "phontab") && isFile(path / "phonindex") && isFile(path / "phondata")) {
            return path;
        }
    }
    return {};
}

class Engine {
public:
    std::mutex mutex;
    StorylandMissionVoiceBackendInfo details;
    SetSynthCallback setSynthCallback = nullptr;
    SetVoiceByName setVoiceByName = nullptr;
    ListVoices listVoices = nullptr;
    SetParameter setParameter = nullptr;
    Synth synth = nullptr;
    Synchronize synchronize = nullptr;
    Cancel cancel = nullptr;
    std::uint32_t sampleRate = 0;

    ~Engine() {
        std::lock_guard<std::mutex> lock(mutex);
        unload();
    }

    bool initialize() {
        if (details.available) {
            return true;
        }
        details = {};
        const auto configuredData = environmentPath("STORYLAND_ESPEAK_DATA");
        auto selectedData = dataDirectory(configuredData);
        if (!configuredData.empty() && selectedData.empty()) {
            details.message = "STORYLAND_ESPEAK_DATA does not contain valid eSpeak NG voice data.";
            return false;
        }

        const auto configuredLibrary = environmentPath("STORYLAND_ESPEAK_LIBRARY");
        std::vector<std::filesystem::path> candidates;
        if (!configuredLibrary.empty()) {
            candidates.push_back(configuredLibrary);
        } else {
            const auto directory = executableDirectory();
#if defined(_WIN32)
            const std::array<const char*, 3> names{{"libespeak-ng.dll", "espeak-ng.dll", "libespeak-ng-1.dll"}};
#elif defined(__APPLE__)
            const std::array<const char*, 2> names{{"libespeak-ng.1.dylib", "libespeak-ng.dylib"}};
#else
            const std::array<const char*, 2> names{{"libespeak-ng.so.1", "libespeak-ng.so"}};
#endif
            if (!directory.empty()) {
                for (const char* name : names) {
                    candidates.push_back(directory / name);
                    candidates.push_back(directory / "lib" / name);
                    candidates.push_back(directory / "espeak-ng" / name);
                    candidates.push_back(directory / "speech" / name);
                }
            }
            for (const char* name : names) {
                candidates.emplace_back(name);
            }
        }
        std::string loadError;
        for (const auto& candidate : candidates) {
            if (load(candidate, loadError)) {
                details.libraryPath = loadedLibraryPath(candidate).u8string();
                break;
            }
        }
        if (module == nullptr) {
            details.message = "eSpeak NG runtime could not be loaded. Install it or set STORYLAND_ESPEAK_LIBRARY.";
            if (!loadError.empty()) {
                details.message += " " + loadError;
            }
            return false;
        }

        if (!symbol(initializeFunction, "espeak_Initialize") ||
            !symbol(setSynthCallback, "espeak_SetSynthCallback") ||
            !symbol(setVoiceByName, "espeak_SetVoiceByName") ||
            !symbol(listVoices, "espeak_ListVoices") ||
            !symbol(setParameter, "espeak_SetParameter") ||
            !symbol(synth, "espeak_Synth") ||
            !symbol(synchronize, "espeak_Synchronize") ||
            !symbol(cancel, "espeak_Cancel") ||
            !symbol(terminate, "espeak_Terminate") ||
            !symbol(info, "espeak_Info")) {
            unload();
            return false;
        }

        if (selectedData.empty()) {
            const auto libraryDirectory = std::filesystem::u8path(details.libraryPath).parent_path();
            const auto appDirectory = executableDirectory();
            for (const auto& candidate : {
                libraryDirectory / "espeak-ng-data",
                libraryDirectory.parent_path() / "share" / "espeak-ng-data",
                appDirectory / "espeak-ng-data",
                appDirectory / "espeak-ng" / "espeak-ng-data",
                appDirectory / "speech" / "espeak-ng-data"
            }) {
                selectedData = dataDirectory(candidate);
                if (!selectedData.empty()) {
                    break;
                }
            }
        }

        const std::string selectedDataPath = selectedData.u8string();
        const int rate = initializeFunction(
            synchronousOutput, 40, selectedDataPath.empty() ? nullptr : selectedDataPath.c_str(), doNotExitOnMissingData);
        initialized = true;
        if (rate <= 0 || rate > 192000) {
            details.message = "eSpeak NG could not initialize its voice data. Set STORYLAND_ESPEAK_DATA to the matching data directory.";
            unload();
            return false;
        }
        sampleRate = static_cast<std::uint32_t>(rate);
        const char* actualDataPath = nullptr;
        const char* version = info(&actualDataPath);
        details.version = version != nullptr ? version : "unknown";
        details.dataPath = actualDataPath != nullptr ? actualDataPath : selectedData.u8string();
        setSynthCallback(collectSamples);
        details.available = true;
        details.message = "eSpeak NG is ready. Original formant voices; roughness and actor likeness are not supported.";
        return true;
    }

private:
#if defined(_WIN32)
    HMODULE module = nullptr;
#else
    void* module = nullptr;
    std::vector<void*> auxiliaryModules;
#endif
    Initialize initializeFunction = nullptr;
    Terminate terminate = nullptr;
    Info info = nullptr;
    bool initialized = false;

    bool load(const std::filesystem::path& path, std::string& error) {
#if defined(_WIN32)
        const DWORD flags = path.is_absolute()
            ? LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
            : LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
        module = LoadLibraryExW(path.c_str(), nullptr, flags);
        if (module == nullptr) {
            error = "Windows loader error " + std::to_string(GetLastError()) + ".";
        }
#else
        if (!path.parent_path().empty()) {
            for (const char* dependency : {"libsonic.so.0", "libpcaudio.so.0"}) {
                const auto dependencyPath = path.parent_path() / dependency;
                if (!isFile(dependencyPath)) {
                    continue;
                }
                void* loadedDependency = dlopen(dependencyPath.c_str(), RTLD_NOW | RTLD_LOCAL);
                if (loadedDependency == nullptr) {
                    const char* message = dlerror();
                    error = message != nullptr ? message : "Bundled speech dependency could not be loaded.";
                    closeAuxiliaryModules();
                    return false;
                }
                auxiliaryModules.push_back(loadedDependency);
            }
        }
        module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (module == nullptr) {
            const char* message = dlerror();
            error = message != nullptr ? message : "Shared-library loader error.";
            closeAuxiliaryModules();
        }
#endif
        return module != nullptr;
    }

#if !defined(_WIN32)
    void closeAuxiliaryModules() {
        for (auto iterator = auxiliaryModules.rbegin(); iterator != auxiliaryModules.rend(); ++iterator) {
            dlclose(*iterator);
        }
        auxiliaryModules.clear();
    }
#endif

    std::filesystem::path loadedLibraryPath(const std::filesystem::path& fallback) {
#if defined(_WIN32)
        std::array<wchar_t, 32768> path{};
        const DWORD count = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (count > 0 && count < path.size()) {
            return std::filesystem::path(std::wstring(path.data(), count));
        }
#else
        Dl_info information{};
        void* address = dlsym(module, "espeak_Initialize");
        if (address != nullptr && dladdr(address, &information) != 0 && information.dli_fname != nullptr) {
            return std::filesystem::u8path(information.dli_fname);
        }
#endif
        return fallback;
    }

    template<class Function>
    bool symbol(Function& function, const char* name) {
#if defined(_WIN32)
        const auto address = GetProcAddress(module, name);
#else
        void* address = dlsym(module, name);
#endif
        if (address == nullptr) {
            details.message = std::string("eSpeak NG runtime is missing ") + name + ".";
            return false;
        }
        static_assert(sizeof(function) == sizeof(address), "Unsupported function-pointer ABI.");
        std::memcpy(&function, &address, sizeof(function));
        return true;
    }

    void unload() {
        if (initialized && terminate != nullptr) {
            terminate();
        }
        initialized = false;
        activeCapture.store(nullptr, std::memory_order_release);
        if (module != nullptr) {
#if defined(_WIN32)
            FreeLibrary(module);
#else
            dlclose(module);
#endif
        }
        module = nullptr;
#if !defined(_WIN32)
        closeAuxiliaryModules();
#endif
        initializeFunction = nullptr;
        setSynthCallback = nullptr;
        setVoiceByName = nullptr;
        listVoices = nullptr;
        setParameter = nullptr;
        synth = nullptr;
        synchronize = nullptr;
        cancel = nullptr;
        terminate = nullptr;
        info = nullptr;
        sampleRate = 0;
        details.available = false;
    }
};

Engine& engine() {
    static Engine instance;
    return instance;
}

bool languageListed(const EspeakVoice& voice, const std::string& language) {
    const char* entry = voice.languages;
    while (entry != nullptr && *entry != '\0') {
        ++entry;
        if (language == entry) {
            return true;
        }
        entry += std::strlen(entry) + 1;
    }
    return false;
}

bool verifyVoice(Engine& runtime, const std::string& language, const char* variant, std::string& error) {
    EspeakVoice criteria{};
    criteria.languages = language.c_str();
    const EspeakVoice** voices = runtime.listVoices(&criteria);
    bool foundLanguage = false;
    if (voices != nullptr) {
        for (std::size_t index = 0; voices[index] != nullptr; ++index) {
            if (languageListed(*voices[index], language)) {
                foundLanguage = true;
                break;
            }
        }
    }
    if (!foundLanguage) {
        error = "The requested eSpeak NG language is unavailable: " + language + ".";
        return false;
    }
    criteria.languages = "variant";
    voices = runtime.listVoices(&criteria);
    const std::string identifier = std::string("!v/") + variant;
    if (voices != nullptr) {
        for (std::size_t index = 0; voices[index] != nullptr; ++index) {
            if (voices[index]->identifier != nullptr && identifier == voices[index]->identifier) {
                return true;
            }
        }
    }
    error = "The requested original eSpeak NG voice variant is unavailable: " + identifier + ".";
    return false;
}

const char* captureError(CaptureFailure failure) {
    switch (failure) {
    case CaptureFailure::Cancelled: return "Speech synthesis was cancelled.";
    case CaptureFailure::OutputLimit: return "Speech synthesis exceeded the 120-second output limit.";
    case CaptureFailure::InvalidBuffer: return "eSpeak NG returned an invalid PCM buffer.";
    case CaptureFailure::SampleRateChanged: return "eSpeak NG changed its sample rate during synthesis.";
    case CaptureFailure::AllocationFailure: return "Speech synthesis could not allocate its PCM output.";
    case CaptureFailure::None: return "";
    }
    return "Speech synthesis failed.";
}

}

bool StorylandEspeakMissionVoiceBackend::available(std::string& error) {
    auto& runtime = storylandMissionVoiceBackendDetail::engine();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    const bool ready = runtime.initialize();
    error = ready ? std::string{} : runtime.details.message;
    return ready;
}

StorylandMissionVoiceBackendInfo StorylandEspeakMissionVoiceBackend::status() {
    auto& runtime = storylandMissionVoiceBackendDetail::engine();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    runtime.initialize();
    return runtime.details;
}

bool StorylandEspeakMissionVoiceBackend::synthesize(
    const StorylandPreparedMissionVoice& prepared,
    StorylandMissionVoicePcm& pcm,
    std::string& error,
    const std::atomic_bool* cancelled) {
    using namespace storylandMissionVoiceBackendDetail;
    pcm = {};
    error.clear();
    if (prepared.profile == nullptr || prepared.line == nullptr || prepared.emotion == nullptr) {
        error = "Speech synthesis requires a prepared approved dialogue line.";
        return false;
    }
    if (prepared.line->language != "en" && prepared.line->language != "ru") {
        error = "Unsupported mission voice language: " + std::string(prepared.line->language) + ".";
        return false;
    }
    StorylandPreparedMissionVoice checked;
    const auto policy = storylandPrepareMissionVoice(
        {prepared.profile->id, prepared.line->id, prepared.text, prepared.emotion->id}, checked);
    if (!policy.accepted()) {
        error = policy.message;
        return false;
    }
    if (checked.line->language != prepared.line->language) {
        error = "Runtime language cannot replace the approved dialogue language.";
        return false;
    }
    if (cancellationRequested(cancelled)) {
        error = "Speech synthesis was cancelled.";
        return false;
    }
    const VoiceBinding* binding = nullptr;
    for (const auto& candidate : voiceBindings) {
        if (candidate.profile == checked.profile->id) {
            binding = &candidate;
            break;
        }
    }
    if (binding == nullptr) {
        error = "This character has no original eSpeak NG voice binding.";
        return false;
    }

    auto& runtime = engine();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    if (cancellationRequested(cancelled)) {
        error = "Speech synthesis was cancelled.";
        return false;
    }
    if (!runtime.initialize()) {
        error = runtime.details.message;
        return false;
    }
    const std::string language = checked.line->language == "ru" ? "ru" : binding->english;
    if (!verifyVoice(runtime, language, binding->variant, error)) {
        return false;
    }
    const std::string voice = language + "+" + binding->variant;
    if (runtime.setVoiceByName(voice.c_str()) != 0) {
        error = "eSpeak NG could not select the requested voice: " + voice + ".";
        return false;
    }

    const int wordsPerMinute = std::clamp(static_cast<int>(std::lround(175.0f * checked.speakingRateMultiplier)), 130, 230);
    const int pitch = std::clamp(static_cast<int>(std::lround(50.0f + 5.0f * checked.pitchOffsetSemitones)), 20, 80);
    const int volume = std::clamp(static_cast<int>(std::lround(55.0f + 40.0f * checked.energy)), 55, 95);
    for (const auto& parameter : std::array<std::pair<int, int>, 5>{{
        {1, wordsPerMinute}, {2, volume}, {3, pitch}, {4, 50}, {7, 0}
    }}) {
        const int result = runtime.setParameter(parameter.first, parameter.second, 0);
        if (result != 0) {
            error = "eSpeak NG rejected speech parameter " + std::to_string(parameter.first) +
                    " (error " + std::to_string(result) + ").";
            return false;
        }
    }

    Capture capture;
    capture.cancelled = cancelled;
    capture.sampleRate = runtime.sampleRate;
    capture.maximumSamples = static_cast<std::size_t>(runtime.sampleRate) * maximumDurationSeconds;
    const std::string text(checked.text);
    activeCapture.store(&capture, std::memory_order_release);
    const int synthesisResult = runtime.synth(
        text.c_str(), text.size() + 1, 0, positionInCharacters, 0,
        utf8Text | endPause, nullptr, &capture);
    const int synchronizationResult = runtime.synchronize();
    activeCapture.store(nullptr, std::memory_order_release);
    if (cancellationRequested(cancelled)) {
        capture.failure = CaptureFailure::Cancelled;
    }
    if (capture.failure != CaptureFailure::None || synthesisResult != 0 || synchronizationResult != 0) {
        runtime.cancel();
        error = capture.failure != CaptureFailure::None
            ? captureError(capture.failure)
            : "eSpeak NG failed to synthesize the approved dialogue.";
        return false;
    }
    if (capture.samples.empty() ||
        std::none_of(capture.samples.begin(), capture.samples.end(), [](std::int16_t sample) { return sample != 0; })) {
        error = "eSpeak NG returned no audible speech samples.";
        return false;
    }
    pcm.samples = std::move(capture.samples);
    pcm.sampleRate = runtime.sampleRate;
    pcm.voiceId = voice;
    pcm.backendVersion = runtime.details.version;
    pcm.wordsPerMinute = wordsPerMinute;
    pcm.pitch = pitch;
    pcm.volume = volume;
    pcm.roughnessApplied = false;
    return true;
}
