#include "src/graphics/storyland_antialiasing.h"
#include <GL/gl.h>
#include <cstdint>
#include <initializer_list>

int storylandChooseAntialiasedPixelFormat(HDC target, const PIXELFORMATDESCRIPTOR& descriptor) {
    const int existing = GetPixelFormat(target);
    if (existing != 0) return existing;

    int selected = ChoosePixelFormat(target, &descriptor);
    const HWND targetWindow = WindowFromDC(target);
    RECT rectangle{};
    GetWindowRect(targetWindow, &rectangle);
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"StorylandAntialiasingBootstrap";
    WNDCLASSW windowClass{};
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    if (!RegisterClassW(&windowClass)) return selected;

    const HWND temporaryWindow = CreateWindowExW(0, className, L"", WS_POPUP,
        rectangle.left, rectangle.top, 1, 1, nullptr, nullptr, instance, nullptr);
    const HDC temporaryDc = temporaryWindow ? GetDC(temporaryWindow) : nullptr;
    const HDC previousDc = wglGetCurrentDC();
    const HGLRC previousContext = wglGetCurrentContext();
    HGLRC temporaryContext = nullptr;
    bool madeCurrent = false;
    if (temporaryDc) {
        const int format = ChoosePixelFormat(temporaryDc, &descriptor);
        if (format != 0 && SetPixelFormat(temporaryDc, format, &descriptor)) {
            temporaryContext = wglCreateContext(temporaryDc);
            madeCurrent = temporaryContext && wglMakeCurrent(temporaryDc, temporaryContext);
        }
    }
    if (madeCurrent) {
        using ChooseFormat = BOOL (WINAPI*)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
        const PROC address = wglGetProcAddress("wglChoosePixelFormatARB");
        const auto value = reinterpret_cast<intptr_t>(address);
        if (value != 0 && value != 1 && value != 2 && value != 3 && value != -1) {
            const auto chooseFormat = reinterpret_cast<ChooseFormat>(address);
            for (int samples : {4, 2}) {
                const int attributes[] = {
                    0x2001, TRUE,   // WGL_DRAW_TO_WINDOW_ARB
                    0x2010, TRUE,   // WGL_SUPPORT_OPENGL_ARB
                    0x2011, TRUE,   // WGL_DOUBLE_BUFFER_ARB
                    0x2013, 0x202B, // WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB
                    0x2014, 32,     // WGL_COLOR_BITS_ARB
                    0x2022, 24,     // WGL_DEPTH_BITS_ARB
                    0x2023, 8,      // WGL_STENCIL_BITS_ARB
                    0x2041, 1,      // WGL_SAMPLE_BUFFERS_ARB
                    0x2042, samples,// WGL_SAMPLES_ARB
                    0
                };
                int format = 0;
                UINT count = 0;
                if (chooseFormat(target, attributes, nullptr, 1, &format, &count) && count && format) {
                    selected = format;
                    break;
                }
            }
        }
        wglMakeCurrent(previousDc, previousContext);
    }
    if (temporaryContext) wglDeleteContext(temporaryContext);
    if (temporaryDc) ReleaseDC(temporaryWindow, temporaryDc);
    if (temporaryWindow) DestroyWindow(temporaryWindow);
    UnregisterClassW(className, instance);
    return selected;
}
