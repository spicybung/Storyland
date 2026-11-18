#include "src/audio/storyland_mission_voice_neural_backend.h"
#include "vendors/nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#endif

namespace storylandNeuralVoiceDetail {

using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr std::size_t maximumHeaderBytes = 16384;
constexpr std::size_t maximumRequestBytes = 16384;
constexpr std::size_t maximumDiagnosticBytes = 65536;
constexpr std::uint64_t maximumAudioSeconds = 120;

std::filesystem::path environmentPath(const char* name) {
#if defined(_WIN32)
    const std::wstring wideName(name, name + std::strlen(name));
    const DWORD required = GetEnvironmentVariableW(wideName.c_str(), nullptr, 0);
    if (required == 0) return {};
    std::wstring value(required, L'\0');
    const DWORD length = GetEnvironmentVariableW(wideName.c_str(), value.data(), required);
    if (length == 0 || length >= required) return {};
    value.resize(length);
    return std::filesystem::path(value);
#else
    const char* value = std::getenv(name);
    return value != nullptr ? std::filesystem::u8path(value) : std::filesystem::path{};
#endif
}

std::string selectedEngine() {
    const std::string value = environmentPath("STORYLAND_VOICE_BACKEND").u8string();
    return value.empty() ? "chatterbox" : value;
}

std::filesystem::path executableDirectory() {
#if defined(_WIN32)
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length > 0 && length < path.size()) {
        return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
    }
#elif defined(__APPLE__)
    std::array<char, 4096> path{};
    std::uint32_t length = static_cast<std::uint32_t>(path.size());
    if (_NSGetExecutablePath(path.data(), &length) == 0) {
        return std::filesystem::u8path(path.data()).parent_path();
    }
#else
    std::error_code error;
    const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) return path.parent_path();
#endif
    return {};
}

bool isFile(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && !error;
}

std::filesystem::path absolutePath(const std::filesystem::path& path) {
    std::error_code error;
    const auto result = std::filesystem::absolute(path, error);
    return error ? std::filesystem::path{} : result;
}

std::filesystem::path findInterpreter(const std::filesystem::path& requested) {
    if (requested.has_parent_path() || requested.is_absolute()) {
        const auto candidate = absolutePath(requested);
#if defined(_WIN32)
        return isFile(candidate) ? candidate : std::filesystem::path{};
#else
        return isFile(candidate) && access(candidate.c_str(), X_OK) == 0 ? candidate : std::filesystem::path{};
#endif
    }
#if defined(_WIN32)
    std::array<wchar_t, 32768> path{};
    const DWORD length = SearchPathW(nullptr, requested.c_str(), L".exe",
                                    static_cast<DWORD>(path.size()), path.data(), nullptr);
    if (length > 0 && length < path.size()) {
        return std::filesystem::path(std::wstring(path.data(), length));
    }
#else
    const char* search = std::getenv("PATH");
    if (search != nullptr) {
        const std::string paths(search);
        std::size_t begin = 0;
        while (begin <= paths.size()) {
            const auto separator = paths.find(':', begin);
            const auto end = separator == std::string::npos ? paths.size() : separator;
            if (end > begin) {
                const auto candidate = absolutePath(std::filesystem::u8path(paths.substr(begin, end - begin)) / requested);
                if (isFile(candidate) && access(candidate.c_str(), X_OK) == 0) return candidate;
            }
            if (separator == std::string::npos) break;
            begin = separator + 1;
        }
    }
#endif
    return {};
}

struct Settings {
    std::filesystem::path interpreter;
    std::filesystem::path worker;
    std::filesystem::path configuration;
    std::filesystem::file_time_type workerModified{};
    std::filesystem::file_time_type configurationModified{};
    std::chrono::seconds timeout{900};

    bool sameFiles(const Settings& other) const {
        return interpreter == other.interpreter && worker == other.worker &&
               configuration == other.configuration && workerModified == other.workerModified &&
               configurationModified == other.configurationModified;
    }
};

bool settings(Settings& result, std::string& error) {
    const auto directory = executableDirectory() / "speech" / "neural";
    result.worker = environmentPath("STORYLAND_VOICE_WORKER");
    if (result.worker.empty()) result.worker = directory / "storyland_neural_voice_worker.py";
    result.worker = absolutePath(result.worker);
    if (!isFile(result.worker)) {
        error = "The Chatterbox voice worker is missing. Set STORYLAND_VOICE_WORKER or install speech/neural beside Storyland.";
        return false;
    }
    result.configuration = environmentPath("STORYLAND_VOICE_CONFIG");
    if (result.configuration.empty()) result.configuration = directory / "voice_config.json";
    result.configuration = absolutePath(result.configuration);
    if (!isFile(result.configuration)) {
        error = "The Chatterbox voice configuration is missing. Set STORYLAND_VOICE_CONFIG to the model and character reference configuration.";
        return false;
    }
    auto interpreter = environmentPath("STORYLAND_VOICE_PYTHON");
    if (interpreter.empty()) {
#if defined(_WIN32)
        const std::array<std::filesystem::path, 4> candidates{{
            directory / "python" / "python.exe", directory / "venv" / "Scripts" / "python.exe",
            directory / ".venv" / "Scripts" / "python.exe", directory / "python.exe"
        }};
#else
        const std::array<std::filesystem::path, 5> candidates{{
            directory / "venv" / "bin" / "python", directory / "venv" / "bin" / "python3",
            directory / "python" / "bin" / "python3", directory / ".venv" / "bin" / "python",
            directory / ".venv" / "bin" / "python3"
        }};
#endif
        for (const auto& candidate : candidates) {
            if (isFile(candidate)) {
                interpreter = candidate;
                break;
            }
        }
    }
    if (interpreter.empty()) {
#if defined(_WIN32)
        interpreter = "python.exe";
#else
        interpreter = "python3";
#endif
    }
    result.interpreter = findInterpreter(interpreter);
    if (result.interpreter.empty()) {
        error = "The neural voice Python interpreter is unavailable. Set STORYLAND_VOICE_PYTHON to the configured Python executable.";
        return false;
    }
    std::error_code filesystemError;
    result.workerModified = std::filesystem::last_write_time(result.worker, filesystemError);
    if (filesystemError) {
        error = "The neural voice worker could not be inspected.";
        return false;
    }
    result.configurationModified = std::filesystem::last_write_time(result.configuration, filesystemError);
    if (filesystemError) {
        error = "The neural voice configuration could not be inspected.";
        return false;
    }
    const std::string timeout = environmentPath("STORYLAND_VOICE_TIMEOUT_SECONDS").u8string();
    if (!timeout.empty()) {
        try {
            std::size_t consumed = 0;
            const auto seconds = std::stoul(timeout, &consumed);
            if (consumed != timeout.size() || seconds < 15 || seconds > 3600) {
                error = "STORYLAND_VOICE_TIMEOUT_SECONDS must be an integer from 15 to 3600.";
                return false;
            }
            result.timeout = std::chrono::seconds(seconds);
        } catch (...) {
            error = "STORYLAND_VOICE_TIMEOUT_SECONDS must be an integer from 15 to 3600.";
            return false;
        }
    }
    return true;
}

bool cancelled(const std::atomic_bool* request, const std::atomic_bool& shutdown) {
    return shutdown.load(std::memory_order_relaxed) ||
           (request != nullptr && request->load(std::memory_order_relaxed));
}

#if defined(_WIN32)
void closeHandle(HANDLE& handle) {
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    handle = nullptr;
}

std::wstring quoteArgument(const std::wstring& argument) {
    std::wstring result = L"\"";
    std::size_t backslashes = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
        } else {
            result.append(backslashes * (character == L'"' ? 2 : 1), L'\\');
            backslashes = 0;
            if (character == L'"') result += L'\\';
            result += character;
        }
    }
    result.append(backslashes * 2, L'\\');
    result += L'"';
    return result;
}
#else
void closeDescriptor(int& descriptor) {
    if (descriptor >= 0) close(descriptor);
    descriptor = -1;
}

bool createPipe(std::array<int, 2>& descriptors) {
    if (pipe(descriptors.data()) != 0) return false;
    if (fcntl(descriptors[0], F_SETFD, FD_CLOEXEC) != 0 ||
        fcntl(descriptors[1], F_SETFD, FD_CLOEXEC) != 0) {
        closeDescriptor(descriptors[0]);
        closeDescriptor(descriptors[1]);
        return false;
    }
    return true;
}

class PipeSignalBlock {
public:
    PipeSignalBlock() {
        sigemptyset(&signals);
        sigaddset(&signals, SIGPIPE);
        active = pthread_sigmask(SIG_BLOCK, &signals, &previous) == 0;
        sigset_t pending{};
        if (active && sigpending(&pending) == 0) alreadyPending = sigismember(&pending, SIGPIPE) == 1;
    }
    ~PipeSignalBlock() {
        if (!active) return;
        if (brokenPipe && !alreadyPending) {
            const timespec timeout{0, 0};
            while (sigtimedwait(&signals, nullptr, &timeout) < 0 && errno == EINTR) {}
        }
        pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    }
    bool brokenPipe = false;
    bool valid() const { return active; }
private:
    sigset_t signals{};
    sigset_t previous{};
    bool active = false;
    bool alreadyPending = false;
};
#endif

class Worker {
public:
    std::timed_mutex mutex;
    std::atomic_bool shutdownRequested{false};
    std::atomic<std::uint64_t> shutdownGeneration{0};

    ~Worker() {
        shutdown();
    }

    void shutdown() {
        shutdownRequested.store(true, std::memory_order_relaxed);
        shutdownGeneration.fetch_add(1, std::memory_order_acq_rel);
        std::lock_guard<std::timed_mutex> lock(mutex);
        stop();
        shutdownRequested.store(false, std::memory_order_relaxed);
    }

    bool generate(const StorylandPreparedMissionVoice& prepared, StorylandMissionVoicePcm& pcm,
                  std::string& error, const std::atomic_bool* cancellation) {
        const auto generation = shutdownGeneration.load(std::memory_order_acquire);
        std::unique_lock<std::timed_mutex> lock(mutex, std::defer_lock);
        for (;;) {
            if (cancelled(cancellation, shutdownRequested) ||
                generation != shutdownGeneration.load(std::memory_order_acquire)) {
                error = "Neural voice synthesis was cancelled.";
                return false;
            }
            if (lock.try_lock_for(std::chrono::milliseconds(20))) break;
        }
        if (generation != shutdownGeneration.load(std::memory_order_acquire)) {
            error = "Neural voice synthesis was cancelled.";
            return false;
        }
        try {
            return generateLocked(prepared, pcm, error, cancellation);
        } catch (const std::exception& exception) {
            error = "Neural voice transport failed: " + std::string(exception.what());
            stop();
            return false;
        } catch (...) {
            error = "Neural voice transport failed unexpectedly.";
            stop();
            return false;
        }
    }

private:
    Settings current;
    std::uint64_t nextRequestId = 0;
    std::string outputBuffer;
    std::string diagnostics;
#if defined(_WIN32)
    HANDLE process = nullptr;
    HANDLE job = nullptr;
    HANDLE input = nullptr;
    HANDLE output = nullptr;
    HANDLE diagnostic = nullptr;
#else
    pid_t process = -1;
    int input = -1;
    int output = -1;
    int diagnostic = -1;
#endif

    bool running() const {
#if defined(_WIN32)
        return process != nullptr;
#else
        return process > 0;
#endif
    }

    void appendDiagnostic(const char* data, std::size_t bytes) {
        diagnostics.append(data, bytes);
        if (diagnostics.size() > maximumDiagnosticBytes) {
            diagnostics.erase(0, diagnostics.size() - maximumDiagnosticBytes);
        }
    }

    void drainDiagnostics() {
        std::array<char, 4096> buffer{};
        std::size_t drained = 0;
        while (drained < maximumDiagnosticBytes) {
#if defined(_WIN32)
            if (diagnostic == nullptr) return;
            DWORD available = 0;
            if (!PeekNamedPipe(diagnostic, nullptr, 0, nullptr, &available, nullptr) || available == 0) return;
            DWORD received = 0;
            const DWORD wanted = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
            if (!ReadFile(diagnostic, buffer.data(), wanted, &received, nullptr) || received == 0) return;
            const auto bytes = static_cast<std::size_t>(received);
#else
            if (diagnostic < 0) return;
            const ssize_t received = read(diagnostic, buffer.data(), buffer.size());
            if (received <= 0) return;
            const auto bytes = static_cast<std::size_t>(received);
#endif
            appendDiagnostic(buffer.data(), bytes);
            drained += bytes;
        }
    }

    void stop() {
#if defined(_WIN32)
        if (job != nullptr) TerminateJobObject(job, 1);
        else if (process != nullptr) TerminateProcess(process, 1);
        if (process != nullptr) WaitForSingleObject(process, 2000);
        closeHandle(input);
        closeHandle(output);
        closeHandle(diagnostic);
        closeHandle(process);
        closeHandle(job);
#else
        if (process > 0) {
            kill(-process, SIGTERM);
            kill(process, SIGTERM);
            bool reaped = false;
            for (int attempt = 0; attempt < 20; ++attempt) {
                int status = 0;
                const pid_t result = waitpid(process, &status, WNOHANG);
                if (result == process || (result < 0 && errno == ECHILD)) {
                    reaped = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!reaped) {
                kill(-process, SIGKILL);
                kill(process, SIGKILL);
                int status = 0;
                while (waitpid(process, &status, 0) < 0 && errno == EINTR) {}
            }
        }
        process = -1;
        closeDescriptor(input);
        closeDescriptor(output);
        closeDescriptor(diagnostic);
#endif
        outputBuffer.clear();
    }

    bool start(const Settings& requested, std::string& error) {
        stop();
        diagnostics.clear();
#if defined(_WIN32)
        HANDLE childInput = nullptr;
        HANDLE childOutput = nullptr;
        HANDLE childDiagnostic = nullptr;
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        if (!CreatePipe(&childInput, &input, &security, 65536) ||
            !CreatePipe(&output, &childOutput, &security, 65536) ||
            !CreatePipe(&diagnostic, &childDiagnostic, &security, 65536) ||
            !SetHandleInformation(input, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(output, HANDLE_FLAG_INHERIT, 0) ||
            !SetHandleInformation(diagnostic, HANDLE_FLAG_INHERIT, 0)) {
            error = "Could not create the neural voice worker pipes (Windows error " + std::to_string(GetLastError()) + ").";
            closeHandle(childInput);
            closeHandle(childOutput);
            closeHandle(childDiagnostic);
            stop();
            return false;
        }
        job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (job == nullptr || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
            error = "Could not create the neural voice worker job.";
            closeHandle(childInput);
            closeHandle(childOutput);
            closeHandle(childDiagnostic);
            stop();
            return false;
        }
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = childInput;
        startup.StartupInfo.hStdOutput = childOutput;
        startup.StartupInfo.hStdError = childDiagnostic;
        SIZE_T attributeBytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
        std::vector<unsigned char> attributes(attributeBytes);
        startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        const bool initialized = attributeBytes != 0 &&
            InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributeBytes) != FALSE;
        const std::array<HANDLE, 3> inherited{{childInput, childOutput, childDiagnostic}};
        const bool assigned = initialized && UpdateProcThreadAttribute(
            startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            const_cast<HANDLE*>(inherited.data()), sizeof(inherited), nullptr, nullptr) != FALSE;
        std::wstring command = quoteArgument(requested.interpreter.wstring()) + L" -u -B " +
            quoteArgument(requested.worker.wstring()) + L" --config " + quoteArgument(requested.configuration.wstring());
        PROCESS_INFORMATION information{};
        const auto directory = requested.worker.parent_path();
        const bool created = assigned && CreateProcessW(
            requested.interpreter.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
            nullptr, directory.c_str(), &startup.StartupInfo, &information) != FALSE;
        const DWORD creationError = GetLastError();
        if (initialized) DeleteProcThreadAttributeList(startup.lpAttributeList);
        closeHandle(childInput);
        closeHandle(childOutput);
        closeHandle(childDiagnostic);
        if (!created) {
            error = "Could not start neural voice Python (Windows error " + std::to_string(creationError) + ").";
            stop();
            return false;
        }
        process = information.hProcess;
        if (!AssignProcessToJobObject(job, process) || ResumeThread(information.hThread) == static_cast<DWORD>(-1)) {
            TerminateProcess(process, 1);
            CloseHandle(information.hThread);
            error = "Could not activate the neural voice worker.";
            stop();
            return false;
        }
        CloseHandle(information.hThread);
#else
        std::array<int, 2> incoming{{-1, -1}};
        std::array<int, 2> outgoing{{-1, -1}};
        std::array<int, 2> errors{{-1, -1}};
        if (!createPipe(incoming) || !createPipe(outgoing) || !createPipe(errors)) {
            for (auto* pair : {&incoming, &outgoing, &errors}) {
                closeDescriptor((*pair)[0]);
                closeDescriptor((*pair)[1]);
            }
            error = "Could not create the neural voice worker pipes.";
            return false;
        }
        const std::string python = requested.interpreter.string();
        const std::string script = requested.worker.string();
        const std::string configuration = requested.configuration.string();
        const std::string directory = requested.worker.parent_path().string();
        const std::array<const char*, 7> arguments{{python.c_str(), "-u", "-B", script.c_str(), "--config", configuration.c_str(), nullptr}};
        const long descriptorLimit = std::max<long>(256, sysconf(_SC_OPEN_MAX));
        process = fork();
        if (process == 0) {
            if (setpgid(0, 0) != 0 || dup2(incoming[0], STDIN_FILENO) < 0 ||
                dup2(outgoing[1], STDOUT_FILENO) < 0 || dup2(errors[1], STDERR_FILENO) < 0 ||
                fcntl(STDIN_FILENO, F_SETFD, 0) != 0 || fcntl(STDOUT_FILENO, F_SETFD, 0) != 0 ||
                fcntl(STDERR_FILENO, F_SETFD, 0) != 0 ||
                chdir(directory.c_str()) != 0) _exit(126);
            bool closed = false;
#if defined(__linux__) && defined(SYS_close_range)
            closed = syscall(SYS_close_range, 3u, std::numeric_limits<unsigned int>::max(), 0u) == 0;
#endif
            if (!closed) {
                for (long descriptor = 3; descriptor < descriptorLimit; ++descriptor) close(static_cast<int>(descriptor));
            }
            execv(python.c_str(), const_cast<char* const*>(arguments.data()));
            constexpr char message[] = "Cannot execute the neural voice Python worker.\n";
            const auto ignored = write(STDERR_FILENO, message, sizeof(message) - 1);
            (void)ignored;
            _exit(127);
        }
        closeDescriptor(incoming[0]);
        closeDescriptor(outgoing[1]);
        closeDescriptor(errors[1]);
        if (process < 0) {
            closeDescriptor(incoming[1]);
            closeDescriptor(outgoing[0]);
            closeDescriptor(errors[0]);
            error = "Could not start the neural voice worker process.";
            return false;
        }
        setpgid(process, process);
        input = incoming[1];
        output = outgoing[0];
        diagnostic = errors[0];
        for (int descriptor : {input, output, diagnostic}) {
            const int flags = fcntl(descriptor, F_GETFL, 0);
            if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) != 0) {
                error = "Could not configure neural voice worker pipes.";
                stop();
                return false;
            }
        }
#endif
        current = requested;
        return true;
    }

    bool checkWait(const std::atomic_bool* cancellation, Clock::time_point deadline, std::string& error) {
        if (cancelled(cancellation, shutdownRequested)) {
            error = "Neural voice synthesis was cancelled.";
            return false;
        }
        if (Clock::now() >= deadline) {
            error = "Neural voice synthesis exceeded its worker timeout.";
            return false;
        }
        return true;
    }

    bool send(const std::string& request, const std::atomic_bool* cancellation,
              Clock::time_point deadline, std::string& error) {
        std::size_t offset = 0;
#if !defined(_WIN32)
        PipeSignalBlock signalBlock;
        if (!signalBlock.valid()) {
            error = "Could not protect the neural voice pipe from SIGPIPE.";
            return false;
        }
#endif
        while (offset < request.size()) {
            if (!checkWait(cancellation, deadline, error)) return false;
            drainDiagnostics();
#if defined(_WIN32)
            DWORD written = 0;
            if (!WriteFile(input, request.data() + offset, static_cast<DWORD>(request.size() - offset), &written, nullptr) || written == 0) {
                error = "The neural voice worker closed its request pipe.";
                return false;
            }
            offset += written;
#else
            const ssize_t written = write(input, request.data() + offset, request.size() - offset);
            if (written > 0) {
                offset += static_cast<std::size_t>(written);
                continue;
            }
            if (written < 0 && errno == EINTR) continue;
            if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                pollfd descriptor{input, POLLOUT, 0};
                poll(&descriptor, 1, 20);
                continue;
            }
            signalBlock.brokenPipe = written < 0 && errno == EPIPE;
            error = "The neural voice worker closed its request pipe.";
            return false;
#endif
        }
        return true;
    }

    bool readMore(const std::atomic_bool* cancellation, Clock::time_point deadline, std::string& error) {
        std::array<char, 8192> buffer{};
        while (checkWait(cancellation, deadline, error)) {
            drainDiagnostics();
#if defined(_WIN32)
            DWORD available = 0;
            if (!PeekNamedPipe(output, nullptr, 0, nullptr, &available, nullptr)) {
                error = "The neural voice worker closed its response pipe.";
                return false;
            }
            if (available > 0) {
                DWORD received = 0;
                const DWORD amount = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
                if (!ReadFile(output, buffer.data(), amount, &received, nullptr) || received == 0) {
                    error = "The neural voice worker response ended unexpectedly.";
                    return false;
                }
                outputBuffer.append(buffer.data(), received);
                return true;
            }
            if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
                error = "The neural voice worker exited before returning its response.";
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
#else
            const ssize_t received = read(output, buffer.data(), buffer.size());
            if (received > 0) {
                outputBuffer.append(buffer.data(), static_cast<std::size_t>(received));
                return true;
            }
            if (received == 0) {
                error = "The neural voice worker response ended unexpectedly.";
                return false;
            }
            if (errno == EINTR) continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                error = "The neural voice worker response could not be read.";
                return false;
            }
            std::array<pollfd, 2> descriptors{{{output, POLLIN, 0}, {diagnostic, POLLIN, 0}}};
            if (poll(descriptors.data(), descriptors.size(), 20) < 0 && errno != EINTR) {
                error = "Could not wait for neural voice output.";
                return false;
            }
#endif
        }
        return false;
    }

    bool readHeader(std::string& header, const std::atomic_bool* cancellation,
                    Clock::time_point deadline, std::string& error) {
        for (;;) {
            const auto newline = outputBuffer.find('\n');
            if (newline != std::string::npos) {
                if (newline > maximumHeaderBytes) break;
                header = outputBuffer.substr(0, newline);
                outputBuffer.erase(0, newline + 1);
                return true;
            }
            if (outputBuffer.size() > maximumHeaderBytes) break;
            if (!readMore(cancellation, deadline, error)) return false;
        }
        error = "The neural voice worker response header exceeds its size limit.";
        return false;
    }

    bool readPcm(std::vector<unsigned char>& bytes, const std::atomic_bool* cancellation,
                 Clock::time_point deadline, std::string& error) {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            if (!checkWait(cancellation, deadline, error)) return false;
            if (outputBuffer.empty() && !readMore(cancellation, deadline, error)) return false;
            const auto amount = std::min(bytes.size() - offset, outputBuffer.size());
            std::memcpy(bytes.data() + offset, outputBuffer.data(), amount);
            outputBuffer.erase(0, amount);
            offset += amount;
        }
        return true;
    }

    bool transportFailure(std::string& error) {
        drainDiagnostics();
        if (!diagnostics.empty()) {
            const auto begin = diagnostics.size() > 4096 ? diagnostics.size() - 4096 : 0;
            error += " Worker diagnostic: " + diagnostics.substr(begin);
        }
        stop();
        return false;
    }

    bool generateLocked(const StorylandPreparedMissionVoice& prepared, StorylandMissionVoicePcm& pcm,
                        std::string& error, const std::atomic_bool* cancellation) {
        if (cancelled(cancellation, shutdownRequested)) {
            error = "Neural voice synthesis was cancelled.";
            stop();
            return false;
        }
        Settings requested;
        if (!settings(requested, error)) return false;
        if (!running() || !requested.sameFiles(current)) {
            if (!start(requested, error)) return false;
        }
        const auto deadline = Clock::now() + requested.timeout;
        const std::uint64_t identifier = ++nextRequestId;
        const Json request{
            {"request_id", identifier}, {"profile_id", prepared.profile->id},
            {"line_id", prepared.line->id}, {"language", prepared.line->language},
            {"text", prepared.text}, {"emotion", prepared.emotion->id},
            {"controls", {{"rate_multiplier", prepared.speakingRateMultiplier},
                           {"pitch_offset_semitones", prepared.pitchOffsetSemitones},
                           {"energy", prepared.energy}, {"roughness", prepared.roughness}}}
        };
        const std::string serialized = request.dump() + '\n';
        if (serialized.size() > maximumRequestBytes) {
            error = "The neural voice request exceeds its transport limit.";
            return false;
        }
        if (!send(serialized, cancellation, deadline, error)) return transportFailure(error);
        std::string header;
        if (!readHeader(header, cancellation, deadline, error)) return transportFailure(error);
        const Json response = Json::parse(header, nullptr, false);
        if (!response.is_object() || !response.contains("request_id") ||
            !response["request_id"].is_number_unsigned() || response["request_id"].get<std::uint64_t>() != identifier ||
            !response.contains("status") || !response["status"].is_string()) {
            error = "The neural voice worker returned an invalid response or request identifier.";
            return transportFailure(error);
        }
        const std::string status = response["status"].get<std::string>();
        if (status == "error") {
            error = response.contains("message") && response["message"].is_string()
                ? response["message"].get<std::string>() : "The neural voice worker rejected the request.";
            if (error.empty()) error = "The neural voice worker rejected the request.";
            return false;
        }
        if (status != "ready" || !response.contains("sample_rate") || !response["sample_rate"].is_number_unsigned() ||
            !response.contains("sample_count") || !response["sample_count"].is_number_unsigned() ||
            !response.contains("pcm_bytes") || !response["pcm_bytes"].is_number_unsigned() ||
            !response.contains("voice_id") || !response["voice_id"].is_string() ||
            !response.contains("backend_version") || !response["backend_version"].is_string()) {
            error = "The neural voice worker returned incomplete PCM metadata.";
            return transportFailure(error);
        }
        const auto rate = response["sample_rate"].get<std::uint64_t>();
        const auto count = response["sample_count"].get<std::uint64_t>();
        const auto byteCount = response["pcm_bytes"].get<std::uint64_t>();
        const std::string voice = response["voice_id"].get<std::string>();
        const std::string version = response["backend_version"].get<std::string>();
        if (rate < 8000 || rate > 48000 || count == 0 || count > rate * maximumAudioSeconds ||
            byteCount != count * 2 || voice != "chatterbox:" + std::string(prepared.profile->id) ||
            version.empty() || version.size() > 512) {
            error = "The neural voice worker returned invalid audio bounds or a different character voice.";
            return transportFailure(error);
        }
        std::vector<unsigned char> bytes(static_cast<std::size_t>(byteCount));
        if (!readPcm(bytes, cancellation, deadline, error)) return transportFailure(error);
        if (cancelled(cancellation, shutdownRequested)) {
            error = "Neural voice synthesis was cancelled.";
            return transportFailure(error);
        }
        StorylandMissionVoicePcm result;
        result.samples.resize(static_cast<std::size_t>(count));
        for (std::size_t index = 0; index < result.samples.size(); ++index) {
            const std::uint16_t value = static_cast<std::uint16_t>(bytes[index * 2]) |
                static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[index * 2 + 1]) << 8);
            result.samples[index] = static_cast<std::int16_t>(value);
        }
        if (std::none_of(result.samples.begin(), result.samples.end(), [](std::int16_t value) { return value != 0; })) {
            error = "The neural voice worker produced silent PCM.";
            return false;
        }
        result.sampleRate = static_cast<std::uint32_t>(rate);
        result.voiceId = voice;
        result.backendVersion = version;
        result.roughnessApplied = false;
        pcm = std::move(result);
        drainDiagnostics();
        return true;
    }
};

std::shared_ptr<Worker> sharedWorker() {
    static std::shared_ptr<Worker> instance = std::make_shared<Worker>();
    return instance;
}

bool validate(const StorylandPreparedMissionVoice& prepared,
              StorylandPreparedMissionVoice& checked, std::string& error) {
    if (prepared.profile == nullptr || prepared.line == nullptr || prepared.emotion == nullptr) {
        error = "Neural synthesis requires a prepared approved dialogue line.";
        return false;
    }
    if (prepared.line->language != "en" && prepared.line->language != "ru") {
        error = "Unsupported mission voice language: " + std::string(prepared.line->language) + ".";
        return false;
    }
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
    return true;
}

}

StorylandNeuralMissionVoiceBackend::StorylandNeuralMissionVoiceBackend()
    : runtime(storylandNeuralVoiceDetail::sharedWorker()) {}

bool StorylandNeuralMissionVoiceBackend::available(std::string& error) {
    storylandNeuralVoiceDetail::Settings configuration;
    error.clear();
    return storylandNeuralVoiceDetail::settings(configuration, error);
}

bool StorylandNeuralMissionVoiceBackend::synthesize(
    const StorylandPreparedMissionVoice& prepared, StorylandMissionVoicePcm& pcm,
    std::string& error, const std::atomic_bool* cancellation) {
    pcm = {};
    error.clear();
    StorylandPreparedMissionVoice checked;
    if (!storylandNeuralVoiceDetail::validate(prepared, checked, error)) return false;
    return runtime->generate(checked, pcm, error, cancellation);
}

void StorylandNeuralMissionVoiceBackend::shutdown() {
    runtime->shutdown();
}

std::string StorylandMissionVoiceSynthesisBackend::name() const {
    const auto selected = storylandNeuralVoiceDetail::selectedEngine();
    if (selected == "chatterbox") return "Chatterbox neural character voice";
    if (selected == "espeak") return "eSpeak NG formant fallback";
    return "Unknown voice backend: " + selected;
}

bool StorylandMissionVoiceSynthesisBackend::available(std::string& error) {
    const auto selected = storylandNeuralVoiceDetail::selectedEngine();
    if (selected == "chatterbox") return neural.available(error);
    if (selected == "espeak") return formant.available(error);
    error = "Unknown STORYLAND_VOICE_BACKEND value: " + selected + ". Use chatterbox or explicit espeak.";
    return false;
}

bool StorylandMissionVoiceSynthesisBackend::synthesize(
    const StorylandPreparedMissionVoice& prepared, StorylandMissionVoicePcm& pcm,
    std::string& error, const std::atomic_bool* cancellation) {
    const auto selected = storylandNeuralVoiceDetail::selectedEngine();
    if (selected == "chatterbox") return neural.synthesize(prepared, pcm, error, cancellation);
    if (selected == "espeak") return formant.synthesize(prepared, pcm, error, cancellation);
    pcm = {};
    error = "Unknown STORYLAND_VOICE_BACKEND value: " + selected + ". Use chatterbox or explicit espeak.";
    return false;
}

void StorylandMissionVoiceSynthesisBackend::shutdown() {
    neural.shutdown();
}
