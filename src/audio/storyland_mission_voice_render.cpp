#include "storyland_mission_voice_neural_backend.h"
#include "vendors/nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void writeLittleEndian(std::ostream& output, std::uint32_t value, int bytes) {
    for (int index = 0; index < bytes; ++index) {
        output.put(static_cast<char>((value >> (index * 8)) & 0xffu));
    }
}

bool writeWave(const std::filesystem::path& path, const StorylandMissionVoicePcm& pcm, std::string& error) {
    if (pcm.sampleRate == 0 || pcm.samples.empty() ||
        pcm.samples.size() > (std::numeric_limits<std::uint32_t>::max() - 36u) / 2u) {
        error = "The synthesized PCM cannot be stored in a WAV file.";
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not open the requested WAV output.";
        return false;
    }
    const auto bytes = static_cast<std::uint32_t>(pcm.samples.size() * 2u);
    output.write("RIFF", 4);
    writeLittleEndian(output, bytes + 36u, 4);
    output.write("WAVEfmt ", 8);
    writeLittleEndian(output, 16, 4);
    writeLittleEndian(output, 1, 2);
    writeLittleEndian(output, 1, 2);
    writeLittleEndian(output, pcm.sampleRate, 4);
    writeLittleEndian(output, pcm.sampleRate * 2u, 4);
    writeLittleEndian(output, 2, 2);
    writeLittleEndian(output, 16, 2);
    output.write("data", 4);
    writeLittleEndian(output, bytes, 4);
    for (std::int16_t sample : pcm.samples) {
        writeLittleEndian(output, static_cast<std::uint16_t>(sample), 2);
    }
    output.close();
    if (!output) {
        error = "The WAV output could not be written completely.";
        return false;
    }
    return true;
}

std::string jsonString(std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (const unsigned char character : value) {
        if (character == '"' || character == '\\') {
            result += '\\';
            result += static_cast<char>(character);
        } else if (character < 0x20) {
            result += "\\u00";
            result += hex[character >> 4];
            result += hex[character & 15];
        } else {
            result += static_cast<char>(character);
        }
    }
    result += '"';
    return result;
}

struct RenderRequest {
    StorylandPreparedMissionVoice prepared;
    std::filesystem::path output;
};

int render(StorylandMissionVoiceSynthesisBackend& backend, const RenderRequest& request) {
    const auto* line = request.prepared.line;
    StorylandMissionVoicePcm pcm;
    std::string error;
    if (!backend.synthesize(request.prepared, pcm, error)) {
        std::cerr << line->id << ": " << error << '\n';
        return 4;
    }
    if (!writeWave(request.output, pcm, error)) {
        std::cerr << line->id << ": " << error << '\n';
        return 5;
    }
    std::cout << "{\"line_id\":" << jsonString(line->id)
              << ",\"language\":" << jsonString(line->language)
              << ",\"profile_id\":" << jsonString(line->profileId)
              << ",\"voice_id\":" << jsonString(pcm.voiceId)
              << ",\"backend_version\":" << jsonString(pcm.backendVersion)
              << ",\"output\":" << jsonString(request.output.u8string())
              << ",\"sample_rate\":" << pcm.sampleRate
              << ",\"samples\":" << pcm.samples.size()
              << ",\"duration_seconds\":" << static_cast<double>(pcm.samples.size()) / pcm.sampleRate
              << ",\"words_per_minute\":" << pcm.wordsPerMinute
              << ",\"pitch\":" << pcm.pitch
              << ",\"volume\":" << pcm.volume
              << ",\"roughness_applied\":false}\n";
    std::cout.flush();
    return 0;
}

bool prepareRenderRequest(std::string_view lineId, const std::string& output,
                          const std::filesystem::path& base, RenderRequest& request, std::string& error) {
    const auto* line = storylandFindMissionVoiceLine(lineId);
    if (line == nullptr) {
        error = "The line is not in the approved dialogue catalogue: " + std::string(lineId);
        return false;
    }
    const auto policy = storylandPrepareMissionVoice({line->profileId, line->id, {}, {}}, request.prepared);
    if (!policy.accepted()) {
        error = policy.message;
        return false;
    }
    if (output.empty() || output.find('\0') != std::string::npos) {
        error = "An output WAV path is empty or contains a null character.";
        return false;
    }
    request.output = std::filesystem::u8path(output);
    if (request.output.is_relative()) request.output = base / request.output;
    std::error_code filesystemError;
    request.output = std::filesystem::weakly_canonical(request.output, filesystemError);
    if (filesystemError || request.output.filename().empty()) {
        error = "An output WAV path cannot be resolved.";
        return false;
    }
    if (!std::filesystem::is_directory(request.output.parent_path(), filesystemError) || filesystemError) {
        error = "The output WAV directory does not exist: " + request.output.parent_path().u8string();
        return false;
    }
    const bool exists = std::filesystem::exists(request.output, filesystemError);
    if (filesystemError || (exists && !std::filesystem::is_regular_file(request.output, filesystemError))) {
        error = "The output WAV path is not a regular file: " + request.output.u8string();
        return false;
    }
    return true;
}

bool sameOutput(const std::filesystem::path& left, const std::filesystem::path& right) {
#if defined(_WIN32)
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
    return left == right;
#endif
}

int renderBatch(const std::filesystem::path& requestedPath) {
    std::error_code filesystemError;
    const auto batchPath = std::filesystem::weakly_canonical(requestedPath, filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(batchPath, filesystemError)) {
        std::cerr << "The batch request file is unavailable.\n";
        return 2;
    }
    const auto size = std::filesystem::file_size(batchPath, filesystemError);
    if (filesystemError || size == 0 || size > 1024 * 1024) {
        std::cerr << "The batch request file must be between 1 byte and 1 MiB.\n";
        return 2;
    }
    std::ifstream input(batchPath, std::ios::binary);
    const auto batch = nlohmann::json::parse(input, nullptr, false);
    if (!batch.is_array() || batch.empty() || batch.size() > 100) {
        std::cerr << "The batch must be a JSON array containing 1 to 100 approved line requests.\n";
        return 2;
    }
    std::vector<RenderRequest> requests;
    requests.reserve(batch.size());
    for (const auto& item : batch) {
        if (!item.is_object() || !item.contains("line_id") || !item["line_id"].is_string() ||
            !item.contains("output") || !item["output"].is_string()) {
            std::cerr << "Each batch item requires string line_id and output fields.\n";
            return 2;
        }
        RenderRequest request;
        std::string error;
        if (!prepareRenderRequest(item["line_id"].get<std::string>(), item["output"].get<std::string>(),
                                  batchPath.parent_path(), request, error)) {
            std::cerr << "Batch item " << requests.size() + 1 << ": " << error << '\n';
            return 2;
        }
        if (sameOutput(request.output, batchPath)) {
            std::cerr << "An output WAV cannot replace the batch request file.\n";
            return 2;
        }
        for (const auto& previous : requests) {
            if (sameOutput(previous.output, request.output)) {
                std::cerr << "Batch outputs must use distinct paths: " << request.output.u8string() << '\n';
                return 2;
            }
        }
        requests.push_back(std::move(request));
    }
    StorylandMissionVoiceSynthesisBackend backend;
    for (const auto& request : requests) {
        const int result = render(backend, request);
        if (result != 0) return result;
    }
    backend.shutdown();
    return 0;
}

}

int storylandVoiceRenderMain(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--list") {
            for (std::size_t index = 0; index < storylandMissionVoiceLineCount(); ++index) {
                const auto* line = storylandMissionVoiceLineAt(index);
                std::cout << line->id << '\t' << line->language << '\t' << line->profileId << '\t' << line->text << '\n';
            }
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--batch") {
            return renderBatch(std::filesystem::u8path(argv[2]));
        }
        if (argc != 5 || std::string_view(argv[1]) != "--line" || std::string_view(argv[3]) != "--output") {
            std::cerr << "Usage: storyland-voice-render --line APPROVED_LINE_ID --output OUTPUT.wav\n"
                         "       storyland-voice-render --batch REQUESTS.json\n"
                         "       storyland-voice-render --list\n";
            return 2;
        }
        RenderRequest request;
        std::string error;
        if (!prepareRenderRequest(argv[2], argv[4], std::filesystem::current_path(), request, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        StorylandMissionVoiceSynthesisBackend backend;
        const int result = render(backend, request);
        backend.shutdown();
        return result;
    } catch (const std::exception& error) {
        std::cerr << "Voice rendering failed: " << error.what() << '\n';
        return 2;
    }
}

#if defined(_WIN32)
int wmain(int argc, wchar_t** wideArguments) {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        const int required = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, wideArguments[index], -1, nullptr, 0, nullptr, nullptr);
        if (required <= 0) {
            std::cerr << "A command-line argument is not valid Unicode.\n";
            return 2;
        }
        std::string argument(static_cast<std::size_t>(required), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wideArguments[index], -1,
                               argument.data(), required, nullptr, nullptr) != required) {
            std::cerr << "A command-line argument could not be converted to UTF-8.\n";
            return 2;
        }
        argument.resize(static_cast<std::size_t>(required - 1));
        arguments.push_back(std::move(argument));
    }
    std::vector<char*> pointers;
    pointers.reserve(arguments.size() + 1);
    for (auto& argument : arguments) {
        pointers.push_back(argument.data());
    }
    pointers.push_back(nullptr);
    const UINT previousCodePage = GetConsoleOutputCP();
    if (previousCodePage != 0) {
        SetConsoleOutputCP(CP_UTF8);
    }
    const int result = storylandVoiceRenderMain(argc, pointers.data());
    std::cout.flush();
    std::cerr.flush();
    if (previousCodePage != 0) {
        SetConsoleOutputCP(previousCodePage);
    }
    return result;
}
#else
int main(int argc, char** argv) {
    return storylandVoiceRenderMain(argc, argv);
}
#endif
