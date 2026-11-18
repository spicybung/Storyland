#pragma once
#include "storyland_version.h"
#include <windows.h>
#include <winhttp.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include "vendors/nlohmann/json.hpp"
#include "src/storyland/storyland_chat_translation_text.h"

class StorylandChatTranslation {
public:
    struct Result { uint64_t ticket; std::string text; std::string error; };
    ~StorylandChatTranslation() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true; jobs.clear();
            cancelled = true;
        }
        ready.notify_one();
        if (worker.joinable()) worker.join();
    }
    bool submit(uint64_t ticket, const std::string& text, const std::string& target,
        const std::string& endpoint, const std::string& key) {
        if (!storylandValidChatText(text) || (target != "en" && target != "ru") ||
            endpoint.size() > 2048 || key.size() > 1024) return false;
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping || jobs.size() >= 16) return false;
        if (!worker.joinable()) worker = std::thread([this] { run(); });
        jobs.push_back({ticket, text, target, endpoint, key, generation});
        ready.notify_one();
        return true;
    }
    std::vector<Result> collect() {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<Result> output;
        output.swap(results);
        return output;
    }
    void cancelQueued() {
        std::lock_guard<std::mutex> lock(mutex);
        jobs.clear(); results.clear(); ++generation;
        cancelled = true;
    }
private:
    struct Job { uint64_t ticket; std::string text, target, endpoint, key; uint64_t generation; };
    struct InternetHandle {
        HINTERNET value = nullptr;
        explicit InternetHandle(HINTERNET value) : value(value) {}
        ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
        InternetHandle(const InternetHandle&) = delete;
        InternetHandle& operator=(const InternetHandle&) = delete;
    };
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<Job> jobs;
    std::vector<Result> results;
    std::thread worker;
    bool stopping = false;
    uint64_t generation = 0;
    std::atomic<bool> cancelled{false};
    Result requestTranslation(const Job& job, bool builtIn) {
        Result result{job.ticket, {}, {}};
        const auto failed = [&](const char* error) { result.text.clear(); result.error = error; return result; };
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, job.endpoint.data(), int(job.endpoint.size()), nullptr, 0);
        if (length <= 0) return failed("Invalid translation URL.");
        std::wstring url(size_t(length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, job.endpoint.data(), int(job.endpoint.size()), url.data(), length);
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = DWORD(-1);
        parts.dwUserNameLength = parts.dwPasswordLength = DWORD(-1);
        if (!WinHttpCrackUrl(url.c_str(), DWORD(url.size()), 0, &parts) || !parts.dwHostNameLength ||
            parts.dwUserNameLength || parts.dwPasswordLength) return failed("Invalid translation URL.");
        const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        const bool local = host == L"localhost" || host == L"127.0.0.1" || host == L"[::1]" || host == L"::1";
        if (parts.nScheme != INTERNET_SCHEME_HTTPS && !(parts.nScheme == INTERNET_SCHEME_HTTP && local))
            return failed("Use HTTPS, or an HTTP translator on this computer.");
        std::wstring path = parts.dwUrlPathLength ? std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) : L"/translate";
        if (path.empty() || path == L"/") path = L"/translate";
        if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        InternetHandle session(WinHttpOpen(L"Storyland/" STORYLAND_VERSION_WIDE, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.value) return failed("Cannot start translation connection.");
        WinHttpSetTimeouts(session.value, 2000, 2000, 2000, 2000);
        InternetHandle connection(WinHttpConnect(session.value, host.c_str(), parts.nPort, 0));
        if (!connection.value) return failed("Cannot connect to translation service.");
        InternetHandle request(WinHttpOpenRequest(connection.value, builtIn ? L"GET" : L"POST", path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
        if (!request.value) return failed("Cannot create translation request.");
        DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));
        nlohmann::json object = {{"q", job.text}, {"source", "auto"}, {"target", job.target}, {"format", "text"}};
        if (!job.key.empty()) object["api_key"] = job.key;
        std::string body = builtIn ? std::string{} : object.dump();
        if (cancelled) return failed("Translation cancelled.");
        if (!WinHttpSendRequest(request.value, L"Content-Type: application/json\r\n", DWORD(-1),
            body.data(), DWORD(body.size()), DWORD(body.size()), 0))
            return failed("Translation service did not respond.");
        if (cancelled) return failed("Translation cancelled.");
        if (!WinHttpReceiveResponse(request.value, nullptr))
            return failed("Translation service did not respond.");
        if (cancelled) return failed("Translation cancelled.");
        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) || status != 200)
            return failed("Translation rejected. Check the service, API key and request limit.");
        std::string response;
        const ULONGLONG deadline = GetTickCount64() + 10000;
        for (;;) {
            if (cancelled) return failed("Translation cancelled.");
            char buffer[4096]; DWORD received = 0;
            if (GetTickCount64() > deadline || !WinHttpReadData(request.value, buffer, sizeof(buffer), &received))
                return failed("Translation response timed out.");
            if (!received) break;
            if (received > 65536 - response.size()) return failed("Translation response is too large.");
            response.append(buffer, received);
        }
        try { result.text = storylandChatReadTranslation(response, builtIn); }
        catch (const std::exception& error) { return failed(error.what()); }
        return result;
    }
    Result translate(const Job& job) {
        const std::string source = storylandChatSourceLanguage(job.text);
        if (source == job.target) return {job.ticket, job.text, {}};
        if (!job.endpoint.empty()) return requestTranslation(job, false);
        Result result{job.ticket, {}, {}};
        for (const auto& part : storylandChatTranslationParts(job.text)) {
            if (cancelled) return {job.ticket, {}, "Translation cancelled."};
            const size_t first = part.find_first_not_of(' ');
            if (first == std::string::npos) { result.text += part; continue; }
            const size_t last = part.find_last_not_of(' ');
            Job request = job;
            request.text = part.substr(first, last - first + 1);
            request.endpoint = "https://api.mymemory.translated.net/get?q=" +
                storylandChatUrlEncode(request.text) + "&langpair=" + source + "%7C" + job.target;
            const Result translated = requestTranslation(request, true);
            if (!translated.error.empty()) return translated;
            result.text += part.substr(0, first) + translated.text + part.substr(last + 1);
            if (result.text.size() > 4096) return {job.ticket, {}, "Translation exceeds the display limit."};
        }
        return result;
    }
    void run() {
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock, [&] { return stopping || !jobs.empty(); });
                if (stopping) return;
                cancelled = false;
                job = std::move(jobs.front()); jobs.pop_front();
            }
            Result result;
            try { result = translate(job); }
            catch (const std::exception&) { result = {job.ticket, {}, "Translation failed."}; }
            std::lock_guard<std::mutex> lock(mutex);
            if (stopping) return;
            if (job.generation != generation) continue;
            if (results.size() >= 100) results.erase(results.begin());
            results.push_back(std::move(result));
        }
    }
};
