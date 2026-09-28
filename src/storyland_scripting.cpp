#include "storyland_scripting.h"

#include <algorithm>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
constexpr int ScriptEditor = 7101;
constexpr int ScriptOutput = 7102;
constexpr int RunPython = 7103;
constexpr int RunCpp = 7104;
constexpr int CloseConsole = 7105;
constexpr int ConsoleHeading = 7106;
constexpr UINT ScriptComplete = WM_APP + 73;
HWND consoleWindow = nullptr;
HWND editorWindow = nullptr;
HWND outputWindow = nullptr;
StorylandScriptHost scriptHost;
bool running = false;
HBRUSH consoleBackground = nullptr;
HBRUSH consoleEditBackground = nullptr;

std::wstring wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    if (size <= 0) return L"";
    std::wstring result(size_t(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), result.data(), size);
    return result;
}

std::string utf8(const std::wstring& value) {
    if (value.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string result(size_t(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring readControl(HWND control) {
    std::wstring value(size_t(GetWindowTextLengthW(control)) + 1u, L'\0');
    GetWindowTextW(control, value.data(), int(value.size()));
    value.resize(wcslen(value.c_str()));
    return value;
}

void appendOutput(const std::wstring& message) {
    if (!outputWindow || !IsWindow(outputWindow)) return;
    const int end = GetWindowTextLengthW(outputWindow);
    SendMessageW(outputWindow, EM_SETSEL, end, end);
    SendMessageW(outputWindow, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(message.c_str()));
}

bool writeText(const std::filesystem::path& file, const std::string& text) {
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output.write(text.data(), std::streamsize(text.size()));
    return bool(output);
}

std::string readText(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::wstring quote(const std::filesystem::path& file) {
    return L"\"" + file.wstring() + L"\"";
}

struct ProcessResult {
    bool started = false;
    DWORD code = 1;
    std::string output;
};

ProcessResult runProcess(std::wstring command, const std::filesystem::path& capture,
                         const std::filesystem::path& workingDirectory) {
    ProcessResult result;
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    HANDLE output = CreateFileW(capture.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &attributes,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        result.output = "Could not create script output file.\n";
        return result;
    }
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               &attributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = output;
    startup.hStdError = output;
    startup.hStdInput = input == INVALID_HANDLE_VALUE ? GetStdHandle(STD_INPUT_HANDLE) : input;
    PROCESS_INFORMATION process{};
    result.started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                    nullptr, workingDirectory.c_str(), &startup, &process) != FALSE;
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    CloseHandle(output);
    if (result.started) {
        DWORD wait = WaitForSingleObject(process.hProcess, 30000);
        if (wait == WAIT_TIMEOUT) {
            TerminateProcess(process.hProcess, 124);
            WaitForSingleObject(process.hProcess, 2000);
            result.output = "Script exceeded the 30 second limit.\n";
        }
        GetExitCodeProcess(process.hProcess, &result.code);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    result.output += readText(capture);
    return result;
}

std::wstring findVcvars64() {
    wchar_t programFiles[32768] = {};
    if (!GetEnvironmentVariableW(L"ProgramFiles(x86)", programFiles, 32768)) return L"";
    const auto root = std::filesystem::path(programFiles) / L"Microsoft Visual Studio";
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) return L"";
    for (std::filesystem::directory_iterator year(root, error), end; !error && year != end; year.increment(error)) {
        if (!year->is_directory(error)) continue;
        for (std::filesystem::directory_iterator edition(year->path(), error), last; !error && edition != last; edition.increment(error)) {
            auto setup = edition->path() / L"VC" / L"Auxiliary" / L"Build" / L"vcvars64.bat";
            if (std::filesystem::is_regular_file(setup, error)) return setup.wstring();
        }
    }
    return L"";
}

const char* pythonBootstrap = R"PY(import sys
class Storyland:
    def open(self, path):
        print('SL\topen\t' + str(path).encode('utf-8').hex(), flush=True)
    def pan(self, x, y):
        print(f'SL\tpan\t{float(x)}\t{float(y)}', flush=True)
    def zoom(self, factor):
        print(f'SL\tzoom\t{float(factor)}', flush=True)
    def rotate(self, degrees, x, y, z):
        print(f'SL\trotate\t{float(degrees)}\t{float(x)}\t{float(y)}\t{float(z)}', flush=True)
    def reset(self):
        print('SL\treset', flush=True)
    def grid(self, enabled):
        print(f'SL\tgrid\t{int(bool(enabled))}', flush=True)
    def dark(self, enabled):
        print(f'SL\tdark\t{int(bool(enabled))}', flush=True)
storyland = Storyland()
with open(sys.argv[1], encoding='utf-8-sig') as file:
    source = file.read()
exec(compile(source, sys.argv[1], 'exec'), {'storyland': storyland, '__name__': '__main__'})
)PY";

const char* cppPreamble = R"CPP(#include <iostream>
#include <string>
struct Storyland {
    static std::string hex(const std::string& value) {
        constexpr char digits[] = "0123456789abcdef";
        std::string encoded;
        for (unsigned char c : value) { encoded += digits[c >> 4]; encoded += digits[c & 15]; }
        return encoded;
    }
    void open(const std::string& path) { std::cout << "SL\topen\t" << hex(path) << std::endl; }
    void pan(double x, double y) { std::cout << "SL\tpan\t" << x << '\t' << y << std::endl; }
    void zoom(double factor) { std::cout << "SL\tzoom\t" << factor << std::endl; }
    void rotate(double degrees, double x, double y, double z) {
        std::cout << "SL\trotate\t" << degrees << '\t' << x << '\t' << y << '\t' << z << std::endl;
    }
    void reset() { std::cout << "SL\treset" << std::endl; }
    void grid(bool enabled) { std::cout << "SL\tgrid\t" << enabled << std::endl; }
    void dark(bool enabled) { std::cout << "SL\tdark\t" << enabled << std::endl; }
};
int main() {
    Storyland storyland;
#line 1 "Storyland C++ console"
)CPP";

struct ScriptResult {
    bool python = false;
    DWORD code = 1;
    std::string output;
};

void runScript(HWND window, bool python, std::wstring source) {
    std::thread([window, python, source = std::move(source)] {
        auto result = std::make_unique<ScriptResult>();
        result->python = python;
        wchar_t temporary[32768] = {};
        if (!GetTempPathW(32768, temporary)) {
            result->output = "Could not locate a temporary directory.\n";
        } else {
            auto folder = std::filesystem::path(temporary) /
                (L"StorylandScript-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
            std::error_code error;
            std::filesystem::create_directories(folder, error);
            if (error) result->output = "Could not create a temporary script directory.\n";
            else {
                auto output = folder / L"output.txt";
                if (python) {
                    auto script = folder / L"script.py";
                    auto bootstrap = folder / L"bootstrap.py";
                    writeText(script, utf8(source));
                    writeText(bootstrap, pythonBootstrap);
                    auto command = L"py -3 -X utf8 -I " + quote(bootstrap) + L" " + quote(script);
                    ProcessResult process = runProcess(command, output, folder);
                    if (!process.started) {
                        command = L"python -X utf8 -I " + quote(bootstrap) + L" " + quote(script);
                        process = runProcess(command, output, folder);
                    }
                    result->code = process.code;
                    result->output = process.started ? process.output : "Python 3 was not found. Install Python or add it to PATH.\n";
                } else {
                    auto script = folder / L"script.cpp";
                    auto binary = folder / L"script.exe";
                    writeText(script, std::string(cppPreamble) + utf8(source) + "\nreturn 0;\n}\n");
                    const std::wstring setup = findVcvars64();
                    const std::wstring compiler = L"cl /nologo /EHsc /std:c++17 /utf-8 /Fe:" + quote(binary) + L" " + quote(script);
                    const std::wstring body = (setup.empty() ? L"" : L"call \"" + setup + L"\" >nul && ") + compiler;
                    ProcessResult built = runProcess(L"cmd.exe /d /s /c \"" + body + L"\"", output, folder);
                    result->output = built.started ? built.output : "Could not start the C++ compiler.\n";
                    result->code = built.code;
                    if (built.started && built.code == 0 && std::filesystem::is_regular_file(binary, error)) {
                        ProcessResult executed = runProcess(quote(binary), output, folder);
                        result->code = executed.code;
                        result->output += executed.output;
                    }
                }
                std::filesystem::remove_all(folder, error);
            }
        }
        if (!IsWindow(window) || !PostMessageW(window, ScriptComplete, 0, reinterpret_cast<LPARAM>(result.get()))) return;
        result.release();
    }).detach();
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string fromHex(const std::string& value) {
    if (value.size() % 2 != 0) return "";
    std::string decoded;
    for (size_t i = 0; i < value.size(); i += 2) {
        int high = hexDigit(value[i]), low = hexDigit(value[i + 1]);
        if (high < 0 || low < 0) return "";
        decoded.push_back(char((high << 4) | low));
    }
    return decoded;
}

void applyScriptResult(const ScriptResult& result) {
    std::istringstream lines(result.output);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.compare(0, 3, "SL\t") != 0) {
            appendOutput(wide(line) + L"\r\n");
            continue;
        }
        std::istringstream command(line.substr(3));
        std::string name;
        command >> name;
        try {
            if (name == "open") {
                std::string encoded;
                command >> encoded;
                if (scriptHost.open) scriptHost.open(wide(fromHex(encoded)));
            } else if (name == "pan") {
                float x, y;
                if (command >> x >> y && scriptHost.pan) scriptHost.pan(x, y);
            } else if (name == "zoom") {
                float factor;
                if (command >> factor && scriptHost.zoom) scriptHost.zoom(factor);
            } else if (name == "rotate") {
                float degrees, x, y, z;
                if (command >> degrees >> x >> y >> z && scriptHost.rotate)
                    scriptHost.rotate(degrees, x, y, z);
            } else if (name == "reset") {
                if (scriptHost.reset) scriptHost.reset();
            } else if (name == "grid" || name == "dark") {
                int enabled;
                if (command >> enabled) {
                    if (name == "grid" && scriptHost.grid) scriptHost.grid(enabled != 0);
                    if (name == "dark" && scriptHost.dark) scriptHost.dark(enabled != 0);
                }
            }
        } catch (const std::exception& error) {
            appendOutput(L"Script command failed: " + wide(error.what()) + L"\r\n");
        }
    }
    appendOutput(L"Exit code: " + std::to_wstring(result.code) + L"\r\n");
}

LRESULT CALLBACK consoleProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        consoleBackground = CreateSolidBrush(RGB(26, 28, 35));
        consoleEditBackground = CreateSolidBrush(RGB(35, 37, 45));
        HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HWND heading = CreateWindowW(L"STATIC", L"Scripting Console  |  Python / C++",
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 24, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ConsoleHeading)), nullptr, nullptr);
        SendMessageW(heading, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        editorWindow = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"storyland.zoom(0.85)\r\n",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            0, 0, 100, 100, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ScriptEditor)), nullptr, nullptr);
        outputWindow = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"Python: write statements using storyland.pan(x, y), zoom(factor), rotate(degrees, x, y, z), reset(), open(path), grid(bool), dark(bool).\r\n"
            L"C++: write statements using the same storyland methods. Requires Visual Studio C++ Build Tools.\r\n",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, 0, 100, 100, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ScriptOutput)), nullptr, nullptr);
        for (HWND child : {editorWindow, outputWindow}) SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        for (const auto& button : std::vector<std::pair<int, const wchar_t*>>{
                 {RunPython, L"Run Python"}, {RunCpp, L"Run C++"}, {CloseConsole, L"Close"}}) {
            HWND control = CreateWindowW(L"BUTTON", button.second, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 100, 30, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(button.first)), nullptr, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT rect{};
        GetClientRect(window, &rect);
        FillRect(reinterpret_cast<HDC>(wParam), &rect, consoleBackground);
        return 1;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const HWND control = reinterpret_cast<HWND>(lParam);
        const bool heading = control == GetDlgItem(window, ConsoleHeading);
        SetTextColor(dc, heading ? RGB(238, 78, 161) : RGB(237, 239, 244));
        SetBkColor(dc, heading ? RGB(26, 28, 35) : RGB(35, 37, 45));
        return reinterpret_cast<LRESULT>(heading ? consoleBackground : consoleEditBackground);
    }
    case WM_SIZE: {
        const int width = LOWORD(lParam), height = HIWORD(lParam);
        const int half = std::max(100, (height - 98) / 2);
        MoveWindow(GetDlgItem(window, ConsoleHeading), 12, 9, std::max(50, width - 24), 22, TRUE);
        MoveWindow(editorWindow, 12, 36, std::max(50, width - 24), half, TRUE);
        MoveWindow(outputWindow, 12, half + 42, std::max(50, width - 24), std::max(50, height - half - 94), TRUE);
        MoveWindow(GetDlgItem(window, RunPython), 12, height - 39, 110, 28, TRUE);
        MoveWindow(GetDlgItem(window, RunCpp), 130, height - 39, 110, 28, TRUE);
        MoveWindow(GetDlgItem(window, CloseConsole), std::max(250, width - 112), height - 39, 100, 28, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == CloseConsole) { DestroyWindow(window); return 0; }
        if ((LOWORD(wParam) == RunPython || LOWORD(wParam) == RunCpp) && !running) {
            const auto source = readControl(editorWindow);
            if (source.empty()) return 0;
            running = true;
            EnableWindow(GetDlgItem(window, RunPython), FALSE);
            EnableWindow(GetDlgItem(window, RunCpp), FALSE);
            appendOutput(LOWORD(wParam) == RunPython ? L"\r\nRunning Python...\r\n" : L"\r\nCompiling C++...\r\n");
            runScript(window, LOWORD(wParam) == RunPython, source);
            return 0;
        }
        break;
    case ScriptComplete: {
        std::unique_ptr<ScriptResult> result(reinterpret_cast<ScriptResult*>(lParam));
        applyScriptResult(*result);
        running = false;
        EnableWindow(GetDlgItem(window, RunPython), TRUE);
        EnableWindow(GetDlgItem(window, RunCpp), TRUE);
        return 0;
    }
    case WM_DESTROY:
        if (consoleEditBackground) { DeleteObject(consoleEditBackground); consoleEditBackground = nullptr; }
        if (consoleBackground) { DeleteObject(consoleBackground); consoleBackground = nullptr; }
        consoleWindow = editorWindow = outputWindow = nullptr;
        running = false;
        if (IsWindow(GetParent(window))) SendMessageW(GetParent(window), WM_SIZE, 0, 0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

void storylandShowScriptingConsole(HWND owner, StorylandScriptHost host) {
    scriptHost = std::move(host);
    if (consoleWindow && IsWindow(consoleWindow)) {
        ShowWindow(consoleWindow, SW_SHOW);
        SetFocus(editorWindow);
        return;
    }
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = consoleProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"StorylandScriptingConsoleWindow";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&windowClass);
    consoleWindow = CreateWindowExW(WS_EX_CLIENTEDGE, windowClass.lpszClassName,
        L"Storyland Scripting Console", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 320, 500, owner, nullptr, windowClass.hInstance, nullptr);
    if (consoleWindow) {
        SendMessageW(owner, WM_SIZE, 0, 0);
        SetFocus(editorWindow);
    }
}

HWND storylandScriptingConsoleWindow() { return consoleWindow; }
