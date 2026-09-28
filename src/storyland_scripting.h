#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>
#include <string>

struct StorylandScriptHost {
    std::function<void(const std::wstring&)> open;
    std::function<void(float, float)> pan;
    std::function<void(float)> zoom;
    std::function<void(float, float, float, float)> rotate;
    std::function<void()> reset;
    std::function<void(bool)> grid;
    std::function<void(bool)> dark;
};

void storylandShowScriptingConsole(HWND owner, StorylandScriptHost host);
HWND storylandScriptingConsoleWindow();
