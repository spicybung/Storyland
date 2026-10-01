#pragma once

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct StorylandOutputFile {
    std::filesystem::path path;
    const std::vector<uint8_t>* bytes = nullptr;
};

namespace storyland_atomic_io_detail {

inline std::string formatByteCount(uint64_t bytes) {
    static const char* units[] = {"bytes", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < 5) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream ss;
    if (unit == 0) ss << bytes << " " << units[unit];
    else ss << std::fixed << std::setprecision(value < 10.0 ? 2 : value < 100.0 ? 1 : 0) << value << " " << units[unit];
    return ss.str();
}

#ifdef _WIN32
inline std::string windowsErrorDescription(DWORD code) {
    switch (code) {
        case ERROR_DISK_FULL:
        case ERROR_HANDLE_DISK_FULL:
            return "The destination drive is out of free space.";
        case ERROR_ACCESS_DENIED:
            return "Windows denied access to the destination. Check folder permissions, Controlled Folder Access, or antivirus protection.";
        case ERROR_SHARING_VIOLATION:
            return "Another program has the destination file open and is preventing Storyland from writing it.";
        case ERROR_LOCK_VIOLATION:
            return "Another program has locked part of the destination file.";
        case ERROR_WRITE_PROTECT:
            return "The destination drive or folder is write-protected.";
        case ERROR_PATH_NOT_FOUND:
            return "The destination folder no longer exists or is unavailable.";
        case ERROR_FILE_NOT_FOUND:
            return "Windows could not find the destination file or one of its required path components.";
        case ERROR_FILENAME_EXCED_RANGE:
            return "The destination path is too long for this Windows configuration.";
        case ERROR_NOT_READY:
            return "The destination drive is not ready.";
        case ERROR_DEVICE_NOT_CONNECTED:
            return "The destination drive or device was disconnected.";
#ifdef ERROR_NOT_ENOUGH_QUOTA
        case ERROR_NOT_ENOUGH_QUOTA:
            return "The storage quota for this destination has been exceeded.";
#endif
        default:
            break;
    }

    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr) return "Windows reported an I/O error.";

    std::wstring wide(buffer, length);
    LocalFree(buffer);
    while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n' || wide.back() == L' ' || wide.back() == L'.')) wide.pop_back();

    if (wide.empty()) return "Windows reported an I/O error.";
    int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return "Windows reported an I/O error.";
    std::string text(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), text.data(), needed, nullptr, nullptr);
    return text + ".";
}

inline bool queryFreeBytesForPath(const std::filesystem::path& path, uint64_t& freeBytes) {
    std::filesystem::path probe = path.parent_path();
    if (probe.empty()) probe = path.root_path();
    if (probe.empty()) probe = L".";

    ULARGE_INTEGER available = {};
    if (!GetDiskFreeSpaceExW(probe.c_str(), &available, nullptr, nullptr)) return false;
    freeBytes = available.QuadPart;
    return true;
}

inline std::string makeWindowsWriteError(const std::filesystem::path& path,
                                         DWORD code,
                                         uint64_t written,
                                         uint64_t total,
                                         const char* operation) {
    std::ostringstream ss;
    ss << windowsErrorDescription(code) << "\n\n";
    ss << "Storyland " << operation << ".\n";
    if (total != 0) {
        ss << "File size: " << formatByteCount(total) << " (" << total << " bytes)\n";
        ss << "Written before failure: " << formatByteCount(written) << " (" << written << " bytes)\n";
    }

    uint64_t freeBytes = 0;
    if (queryFreeBytesForPath(path, freeBytes)) {
        ss << "Free space reported by Windows: " << formatByteCount(freeBytes) << " (" << freeBytes << " bytes)\n";
    }

    ss << "Windows error: " << code << "\n";
    ss << "Path: " << path.string();
    if (code == ERROR_DISK_FULL || code == ERROR_HANDLE_DISK_FULL) {
        ss << "\n\nFree some space on that drive, then retry. If this path is inside your Windows TEMP folder, clearing old temporary files can also help.";
    }
    return ss.str();
}
#endif

inline bool writeAndVerify(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string& error) {
    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) { error = "Could not create output directory: " + ec.message(); return false; }
    }
#ifdef _WIN32
    // Do not follow reparse points for staged output. A junction/symlink in an
    // output path can redirect a supposedly local transaction somewhere else.
    if (!path.parent_path().empty()) {
        const DWORD parentAttrs = GetFileAttributesW(path.parent_path().c_str());
        if (parentAttrs != INVALID_FILE_ATTRIBUTES && (parentAttrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
            error = "Refusing to write through a reparse-point output directory.";
            return false;
        }
    }
    // Use Win32 directly for rebuilt IMG/DTZ output.  std::ofstream only reports a
    // generic failbit here, which made perfectly ordinary Windows failures (full
    // disk, AV/Controlled Folder Access, sharing/permission errors) look like a
    // Storyland rebuild bug.  Write in bounded chunks and report the real error.
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD winerr = GetLastError();
        error = makeWindowsWriteError(path, winerr, 0, bytes.size(), "could not create the temporary output file");
        return false;
    }
    size_t writtenTotal = 0;
    while (writtenTotal < bytes.size()) {
        const DWORD request = static_cast<DWORD>(std::min<size_t>(bytes.size() - writtenTotal, 8u * 1024u * 1024u));
        DWORD wrote = 0;
        if (!WriteFile(handle, bytes.data() + writtenTotal, request, &wrote, nullptr) || wrote != request) {
            const DWORD winerr = GetLastError();
            CloseHandle(handle);
            DeleteFileW(path.c_str());
            error = makeWindowsWriteError(path, winerr, writtenTotal + wrote, bytes.size(),
                                          "could not finish writing the temporary output file");
            return false;
        }
        writtenTotal += size_t(wrote);
    }
    if (!FlushFileBuffers(handle)) {
        const DWORD winerr = GetLastError();
        CloseHandle(handle);
        DeleteFileW(path.c_str());
        error = makeWindowsWriteError(path, winerr, bytes.size(), bytes.size(),
                                      "wrote the temporary output but Windows could not flush it safely to disk");
        return false;
    }
    CloseHandle(handle);
#else
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) { error = "Could not create staged output file: " + path.string(); return false; }
    if (!bytes.empty()) output.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    output.flush();
    if (!output) { error = "Could not completely write staged output file: " + path.string(); return false; }
    output.close();
#endif

    std::ifstream verify(path, std::ios::binary);
    if (!verify) { error = "Could not reopen staged output for verification."; return false; }
    verify.seekg(0, std::ios::end);
    const std::streamoff storedSize = verify.tellg();
    verify.seekg(0, std::ios::beg);
    if (storedSize < 0 || uint64_t(storedSize) != uint64_t(bytes.size())) {
        error = "Staged output size differs from the requested byte count.";
        return false;
    }
    std::vector<uint8_t> block(1024u * 1024u);
    size_t checked = 0;
    while (checked < bytes.size()) {
        const size_t count = std::min(block.size(), bytes.size() - checked);
        verify.read(reinterpret_cast<char*>(block.data()), std::streamsize(count));
        if (size_t(verify.gcount()) != count ||
            !std::equal(block.begin(), block.begin() + std::ptrdiff_t(count), bytes.begin() + std::ptrdiff_t(checked))) {
            error = "Staged output failed byte-for-byte verification.";
            return false;
        }
        checked += count;
    }
    return true;
}

struct CommitState {
    std::filesystem::path finalPath;
    std::filesystem::path backupPath;
    bool existed = false;
};

inline bool commitOne(const std::filesystem::path& staged, const std::filesystem::path& finalPath,
                      const std::filesystem::path& backup, CommitState& state, std::string& error) {
    std::error_code ec;
    state = {finalPath, backup, std::filesystem::exists(finalPath, ec) && !ec};
    std::filesystem::remove(backup, ec);
#ifdef _WIN32
    if (state.existed) {
        DWORD attributes = GetFileAttributesW(finalPath.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY))
            SetFileAttributesW(finalPath.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
        if (!ReplaceFileW(finalPath.c_str(), staged.c_str(), backup.c_str(), REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
            error = "Could not atomically replace output file (Windows error " + std::to_string(GetLastError()) + ").";
            return false;
        }
    } else if (!MoveFileExW(staged.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "Could not publish staged output file (Windows error " + std::to_string(GetLastError()) + ").";
        return false;
    }
#else
    if (state.existed) {
        std::filesystem::rename(finalPath, backup, ec);
        if (ec) { error = "Could not back up existing output: " + ec.message(); return false; }
    }
    ec.clear();
    std::filesystem::rename(staged, finalPath, ec);
    if (ec) {
        if (state.existed) { std::error_code ignored; std::filesystem::rename(backup, finalPath, ignored); }
        error = "Could not publish staged output: " + ec.message();
        return false;
    }
#endif
    return true;
}

inline void rollback(const CommitState& state) {
    std::error_code ignored;
#ifdef _WIN32
    if (state.existed && std::filesystem::exists(state.backupPath, ignored))
        ReplaceFileW(state.finalPath.c_str(), state.backupPath.c_str(), nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr);
    else if (!state.existed)
        DeleteFileW(state.finalPath.c_str());
#else
    std::filesystem::remove(state.finalPath, ignored);
    if (state.existed) std::filesystem::rename(state.backupPath, state.finalPath, ignored);
#endif
}

} // namespace storyland_atomic_io_detail

inline bool storylandWriteFilesTransaction(const std::vector<StorylandOutputFile>& files, std::string& error) {
    error.clear();
    if (files.empty()) { error = "No output files were supplied."; return false; }

    std::set<std::wstring> uniquePaths;
    std::vector<std::filesystem::path> stagedPaths;
    std::vector<std::filesystem::path> backupPaths;
    for (const StorylandOutputFile& file : files) {
        if (file.path.empty() || file.bytes == nullptr) { error = "An output path or byte buffer is missing."; return false; }
        std::error_code pathError;
        if (std::filesystem::exists(file.path, pathError) && !pathError && std::filesystem::is_directory(file.path, pathError)) {
            error = "The output path is a directory, not a file.";
            return false;
        }
        pathError.clear();
        const std::filesystem::file_status outputStatus = std::filesystem::symlink_status(file.path, pathError);
        if (!pathError && std::filesystem::is_symlink(outputStatus)) {
            error = "Refusing to overwrite a symbolic link.";
            return false;
        }
#ifdef _WIN32
        const DWORD outputAttrs = GetFileAttributesW(file.path.c_str());
        if (outputAttrs != INVALID_FILE_ATTRIBUTES && (outputAttrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
            error = "Refusing to overwrite a Windows reparse point.";
            return false;
        }
#endif
        std::wstring key = file.path.lexically_normal().wstring();
#ifdef _WIN32
        std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
#endif
        if (!uniquePaths.insert(key).second) { error = "Two outputs resolve to the same path."; return false; }
        const auto nonce = static_cast<unsigned long long>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()) ^
#ifdef _WIN32
            static_cast<unsigned long long>(GetCurrentProcessId());
#else
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(file.bytes));
#endif
        const std::wstring suffix = L"." + std::to_wstring(nonce);
        std::filesystem::path staged = file.path; staged += suffix + L".storyland_tmp";
        std::filesystem::path backup = file.path; backup += suffix + L".storyland_bak";
        std::error_code ignored;
        std::filesystem::remove(staged, ignored);
        std::filesystem::remove(backup, ignored);
        if (!storyland_atomic_io_detail::writeAndVerify(staged, *file.bytes, error)) {
            for (const auto& oldStage : stagedPaths) std::filesystem::remove(oldStage, ignored);
            std::filesystem::remove(staged, ignored);
            return false;
        }
        stagedPaths.push_back(staged);
        backupPaths.push_back(backup);
    }

    std::vector<storyland_atomic_io_detail::CommitState> committed;
    for (size_t index = 0; index < files.size(); ++index) {
        storyland_atomic_io_detail::CommitState state;
        if (!storyland_atomic_io_detail::commitOne(stagedPaths[index], files[index].path, backupPaths[index], state, error)) {
            for (auto it = committed.rbegin(); it != committed.rend(); ++it) storyland_atomic_io_detail::rollback(*it);
            std::error_code ignored;
            for (const auto& staged : stagedPaths) std::filesystem::remove(staged, ignored);
            for (const auto& backup : backupPaths) std::filesystem::remove(backup, ignored);
            return false;
        }
        committed.push_back(state);
    }
    std::error_code ignored;
    for (const auto& backup : backupPaths) std::filesystem::remove(backup, ignored);
    return true;
}
