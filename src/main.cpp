#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <GL/gl.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <cwchar>
#include <filesystem>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "leeds_texture.h"
#include "storyland_dtz.h"
#include "storyland_model.h"
#include "storyland_wbl.h"
#include "storyland_archive.h"
#include "storyland_atomic_io.h"
#include "storyland_dma_validator.h"
#include "storyland_psp_validator.h"
#include "storyland_model_validator.h"
#include "storyland_anim.h"
#include "storyland_sky.h"
#include "wic_image.h"
#include "resource.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Opengl32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")


#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#ifndef DWMWA_COLOR_DEFAULT
#define DWMWA_COLOR_DEFAULT 0xFFFFFFFF
#endif

typedef char GLchar;
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC)(GLenum type);
typedef void (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
typedef void (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef void (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (APIENTRY *PFNGLDELETESHADERPROC)(GLuint shader);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC)();
typedef void (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar* name);
typedef void (APIENTRY *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (APIENTRY *PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (APIENTRY *PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum texture);

#define ID_FILE_OPEN 1001
#define ID_FILE_SAVE_AS 1002
#define ID_FILE_EXPORT_TEXTURE 1003
#define ID_FILE_REPLACE_TEXTURE 1004
#define ID_FILE_RENAME_TEXTURE 1012
#define ID_FILE_EXPORT_TEXTURE_ARCHIVE 1013
#define ID_DTZ_PATCH_SELECTED 1005
#define ID_DTZ_PATCH_PLR_23 1006
#define ID_FILE_OPEN_DIR 1007
#define ID_HELP_ABOUT 1008
#define ID_FILE_EXPORT_LOG 1009
#define ID_VIEW_FLIP_MODEL_TEXTURE_V 1010
#define ID_FILE_OPEN_EMBEDDED 1011
#define ID_VIEW_FLIP_TEXTURE_PREVIEW_V 1014
#define ID_ARCHIVE_REPLACE_SELECTED_RESOURCE 1015
#define ID_ARCHIVE_REPLACE_MESH_WITH_RESOURCE_ID 1018
#define ID_ARCHIVE_CHANGE_SELECTED_MESH_RESOURCE_ID 1019
#define ID_ARCHIVE_EXPORT_LVZ_IMG_PAIR 1016
#define ID_ARCHIVE_OVERWRITE_LVZ_IMG_PAIR 1017
#define ID_DTZ_PATCH_DATA_FIELD 1020
#define ID_ANIM_PLAY_PAUSE 1021
#define ID_ANIM_STOP 1022
#define ID_ANIM_STEP_BACK 1023
#define ID_ANIM_STEP_FORWARD 1024
#define ID_VIEW_APPROX_ANIM_MESH_PREVIEW 1025
#define ID_VIEW_LOCK_ANIM_ROOT_PREVIEW 1026
#define ID_VIEW_LOCK_WEAPON_LOWER_BODY_PREVIEW 1027
#define ID_VIEW_WEAPON_UPPER_BODY_LAYER_MASK 1028
#define ID_VIEW_CUSTOM_EXPORT_MATRIX_SKIN_PREVIEW 1029
#define ID_FILE_OPEN_DTZ_IMG 1030
#define ID_DTZ_REPLACE_SELECTED_ENTRY 1031
#define ID_VIEW_RENDER_STORIES 1032
#define ID_VIEW_RENDER_TEXTURED 1033
#define ID_VIEW_RENDER_SOLID 1034
#define ID_VIEW_RENDER_WIREFRAME 1035
#define ID_VIEW_SHOW_GRID 1036
#define ID_VIEW_SHOW_BONES 1037
#define ID_VIEW_SHOW_BOUNDS 1038
#define ID_VIEW_SHOW_VIEWCUBE 1039
#define ID_VIEW_SHOW_SKY 1040
#define ID_VIEW_SKY_LCS 1041
#define ID_VIEW_SKY_VCS 1042
#define ID_VIEW_SKY_MIDNIGHT 1043
#define ID_VIEW_SKY_DAWN 1044
#define ID_VIEW_SKY_NOON 1045
#define ID_VIEW_SKY_SUNSET 1046
#define ID_VIEW_SKY_WEATHER_SUNNY 1047
#define ID_VIEW_SKY_WEATHER_CLOUDY 1048
#define ID_VIEW_SKY_WEATHER_RAINY 1049
#define ID_VIEW_SKY_WEATHER_FOGGY 1050
#define ID_FILE_EXPORT_SELECTED_RESOURCE 1051
#define ID_FILE_DMA_TLB_PREFLIGHT 1052
#define ID_VIEW_BACKGROUND_LIGHT 1053
#define ID_VIEW_BACKGROUND_DARK 1054
#define ID_FILE_QUIT 1055
#define ID_FILE_RECENT_CLEAR 1056
#define ID_HELP_WIKI 1057
#define ID_FILE_RECENT_BASE 3000
#define ID_TREE 2001
#define ID_DETAILS 2002
#define ID_PREVIEW 2003
#define ID_STATUS 2004
#define ID_MENU_STRIP 2005

static HINSTANCE gInstance = nullptr;
static HWND gMainWindow = nullptr;
static HWND gTree = nullptr;
static HWND gDetails = nullptr;
static HWND gPreview = nullptr;
static HWND gStatus = nullptr;
static HWND gMenuStrip = nullptr;
static HMENU gMainMenu = nullptr;
static HMENU gFileMenu = nullptr;
static HMENU gViewMenu = nullptr;
static HMENU gSkyMenu = nullptr;
static HMENU gHelpMenu = nullptr;
static HMENU gTexturePreviewMenu = nullptr;
static HMENU gRecentMenu = nullptr;
static HMENU gOpenGlMenu = nullptr;
static HMENU gBackgroundMenu = nullptr;
static bool gEyeFriendlyPaneBackground = true;
static HBRUSH gPaneBackgroundBrush = nullptr;
static HBRUSH gMenuBackgroundBrush = nullptr;
static std::wstring gStatusText;
static std::vector<std::wstring> gRecentFiles;

static COLORREF storylandPaneBackgroundColor() {
    return gEyeFriendlyPaneBackground ? RGB(35, 36, 42) : GetSysColor(COLOR_WINDOW);
}

static COLORREF storylandFrameBackgroundColor() {
    return gEyeFriendlyPaneBackground ? RGB(14, 15, 19) : GetSysColor(COLOR_BTNFACE);
}

static COLORREF storylandPaneTextColor() {
    return gEyeFriendlyPaneBackground ? RGB(232, 234, 239) : GetSysColor(COLOR_WINDOWTEXT);
}

static void applyStorylandNativeMenuTheme(HWND mainWindow) {
    // Windows does not expose menu-bar colors through WM_CTLCOLOR. Use its
    // guarded dark-mode hooks when available, while retaining our brush fallback.
    static HMODULE uxTheme = LoadLibraryW(L"uxtheme.dll");
    if (!uxTheme) return;
    using SetPreferredAppModeProc = int (WINAPI*)(int);
    using AllowDarkModeForWindowProc = BOOL (WINAPI*)(HWND, BOOL);
    using FlushMenuThemesProc = void (WINAPI*)();
    using SetWindowThemeProc = HRESULT (WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    static auto setPreferredAppMode = reinterpret_cast<SetPreferredAppModeProc>(
        GetProcAddress(uxTheme, MAKEINTRESOURCEA(135)));
    static auto allowDarkModeForWindow = reinterpret_cast<AllowDarkModeForWindowProc>(
        GetProcAddress(uxTheme, MAKEINTRESOURCEA(133)));
    static auto flushMenuThemes = reinterpret_cast<FlushMenuThemesProc>(
        GetProcAddress(uxTheme, MAKEINTRESOURCEA(136)));
    static auto setWindowTheme = reinterpret_cast<SetWindowThemeProc>(
        GetProcAddress(uxTheme, "SetWindowTheme"));
    if (setPreferredAppMode) setPreferredAppMode(gEyeFriendlyPaneBackground ? 1 : 3);
    if (mainWindow && allowDarkModeForWindow) allowDarkModeForWindow(mainWindow, gEyeFriendlyPaneBackground);
    if (setWindowTheme) {
        const wchar_t* theme = gEyeFriendlyPaneBackground ? L"DarkMode_Explorer" : nullptr;
        const HWND themedWindows[] = {mainWindow, gTree, gDetails, gStatus};
        for (HWND window : themedWindows) {
            if (window) setWindowTheme(window, theme, nullptr);
        }
    }
    if (flushMenuThemes) flushMenuThemes();
}

static void setStorylandClientEdge(HWND window, bool enabled) {
    if (!window) return;
    LONG_PTR extendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    LONG_PTR updatedStyle = enabled
        ? (extendedStyle | WS_EX_CLIENTEDGE)
        : (extendedStyle & ~LONG_PTR(WS_EX_CLIENTEDGE));
    if (updatedStyle == extendedStyle) return;
    SetWindowLongPtrW(window, GWL_EXSTYLE, updatedStyle);
    SetWindowPos(window, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static void applyStorylandPaneBackground(HWND mainWindow) {
    if (gPaneBackgroundBrush) {
        DeleteObject(gPaneBackgroundBrush);
        gPaneBackgroundBrush = nullptr;
    }
    const COLORREF paneColor = storylandPaneBackgroundColor();
    gPaneBackgroundBrush = CreateSolidBrush(paneColor);
    applyStorylandNativeMenuTheme(mainWindow);
    // The themed WS_EX_CLIENTEDGE bevel is always pale. Remove it in Dark mode
    // so the deliberately darker four-pixel pane gutters become the separators.
    setStorylandClientEdge(gTree, !gEyeFriendlyPaneBackground);
    setStorylandClientEdge(gPreview, !gEyeFriendlyPaneBackground);
    setStorylandClientEdge(gDetails, !gEyeFriendlyPaneBackground);
    if (gTree) {
        TreeView_SetBkColor(gTree, paneColor);
        TreeView_SetTextColor(gTree, storylandPaneTextColor());
        TreeView_SetLineColor(gTree, gEyeFriendlyPaneBackground ? RGB(70, 73, 83) : GetSysColor(COLOR_GRAYTEXT));
        InvalidateRect(gTree, nullptr, TRUE);
    }
    if (gDetails) InvalidateRect(gDetails, nullptr, TRUE);
    if (gStatus) {
        SendMessageW(gStatus, SB_SETBKCOLOR, 0,
            gEyeFriendlyPaneBackground ? LPARAM(RGB(25, 26, 31)) : LPARAM(CLR_DEFAULT));
        InvalidateRect(gStatus, nullptr, TRUE);
    }
    if (mainWindow) {
        if (gMenuBackgroundBrush) {
            DeleteObject(gMenuBackgroundBrush);
            gMenuBackgroundBrush = nullptr;
        }
        MENUINFO menuInfo{};
        menuInfo.cbSize = sizeof(menuInfo);
        menuInfo.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
        if (gEyeFriendlyPaneBackground) {
            gMenuBackgroundBrush = CreateSolidBrush(RGB(25, 26, 31));
            menuInfo.hbrBack = gMenuBackgroundBrush;
        } else {
            menuInfo.hbrBack = GetSysColorBrush(COLOR_MENU);
        }
        if (gMainMenu) SetMenuInfo(gMainMenu, &menuInfo);
        if (gMenuStrip) InvalidateRect(gMenuStrip, nullptr, TRUE);
        InvalidateRect(mainWindow, nullptr, TRUE);
    }
}

enum class StorylandTitleTint { Default, LCS, VCS, CTW };
static StorylandTitleTint gTitleTint = StorylandTitleTint::Default;

static LeedsTextureArchive gTextureArchive;
static StorylandDtzArchive gDtzArchive;
static StorylandModelFile gModelFile;
static StorylandWblFile gWblFile;
static StorylandArchiveBrowser gArchiveBrowser;
static StorylandAnimFile gAnimFile;
static RgbaImage gCurrentImage;
static HBITMAP gTextureBitmap = nullptr;
static bool gTexturePreviewFlipV = false;

static bool gAnimPlaying = false;
static float gAnimCurrentTime = 0.0f;
static DWORD gAnimLastTick = 0;
static bool gModelAnimLoaded = false;
static bool gApproximateAnimMeshPreview = true;
static bool gLockAnimRootPreview = true;
static bool gLockWeaponLowerBodyPreview = true;
static bool gWeaponUpperBodyLayerMaskPreview = true;
static bool gCustomExportMatrixSkinPreview = false;
static std::wstring gModelAnimPath;
static std::wstring gModelAnimStatus;

static LeedsTextureArchive gModelTextureArchive;
static RgbaImage gModelTextureImage;
static std::wstring gModelTexturePath;
static std::string gModelTextureName;
static std::wstring gModelTextureStatus;
static int gModelTextureIndex = -1;
static bool gModelTextureLoaded = false;
static bool gModelTextureUploadNeeded = false;
static GLuint gModelTextureId = 0;
static bool gModelFlipTextureV = false;

enum class StorylandOpenGlRenderMode { Stories, Textured, Solid, Wireframe };
static StorylandOpenGlRenderMode gOpenGlRenderMode = StorylandOpenGlRenderMode::Stories;
static bool gOpenGlShowGrid = false;
static bool gOpenGlShowBones = false;
static bool gOpenGlShowBounds = false;
static bool gOpenGlShowViewCube = true;
static bool gModelViewCubeDrag = false;
static GLuint gStoriesShaderProgram = 0;
static bool gStoriesShaderTried = false;
static bool gStoriesShaderReady = false;
static std::string gStoriesShaderStatus;
static StorylandSky gStoriesSky;
static std::string gStoriesSkyDataStatus;
static DWORD gStoriesSkyLastTick = 0;

static PFNGLCREATESHADERPROC pglCreateShader = nullptr;
static PFNGLSHADERSOURCEPROC pglShaderSource = nullptr;
static PFNGLCOMPILESHADERPROC pglCompileShader = nullptr;
static PFNGLGETSHADERIVPROC pglGetShaderiv = nullptr;
static PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog = nullptr;
static PFNGLDELETESHADERPROC pglDeleteShader = nullptr;
static PFNGLCREATEPROGRAMPROC pglCreateProgram = nullptr;
static PFNGLATTACHSHADERPROC pglAttachShader = nullptr;
static PFNGLLINKPROGRAMPROC pglLinkProgram = nullptr;
static PFNGLGETPROGRAMIVPROC pglGetProgramiv = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog = nullptr;
static PFNGLDELETEPROGRAMPROC pglDeleteProgram = nullptr;
static PFNGLUSEPROGRAMPROC pglUseProgram = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = nullptr;
static PFNGLUNIFORM1IPROC pglUniform1i = nullptr;
static PFNGLUNIFORM1FPROC pglUniform1f = nullptr;
static PFNGLUNIFORM3FPROC pglUniform3f = nullptr;
static PFNGLACTIVETEXTUREPROC pglActiveTexture = nullptr;

struct StorylandModelTextureRegion {
    std::string name;
    int sourceIndex = -1;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

static std::vector<StorylandModelTextureRegion> gModelTextureRegions;

struct StorylandVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct StorylandQuat {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

static StorylandSkyRotation skyRotationFromModelRotation(const StorylandQuat& rotation) {
    return {rotation.w, rotation.x, rotation.y, rotation.z};
}

static float vecDot(const StorylandVec3& a, const StorylandVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static StorylandVec3 vecCross(const StorylandVec3& a, const StorylandVec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static StorylandVec3 vecNormalize(StorylandVec3 v) {
    float len = std::sqrt(std::max(0.00000001f, vecDot(v, v)));
    v.x /= len;
    v.y /= len;
    v.z /= len;
    return v;
}

static StorylandQuat quatNormalize(StorylandQuat q) {
    float lenSq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
    if (lenSq < 0.00000001f) lenSq = 0.00000001f;
    float len = std::sqrt(lenSq);
    q.w /= len;
    q.x /= len;
    q.y /= len;
    q.z /= len;
    return q;
}

static StorylandQuat quatMul(const StorylandQuat& a, const StorylandQuat& b) {
    return quatNormalize({
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
    });
}

static StorylandQuat quatFromAxisAngle(float degrees, float x, float y, float z) {
    StorylandVec3 axis = vecNormalize({x, y, z});
    float radians = degrees * 0.01745329251994329577f;
    float half = radians * 0.5f;
    float s = std::sin(half);
    return quatNormalize({std::cos(half), axis.x * s, axis.y * s, axis.z * s});
}

static StorylandQuat quatFromVectors(StorylandVec3 from, StorylandVec3 to) {
    from = vecNormalize(from);
    to = vecNormalize(to);
    float dot = vecDot(from, to);
    if (dot < -1.0f) dot = -1.0f;
    else if (dot > 1.0f) dot = 1.0f;
    if (dot > 0.9999f) return {};
    if (dot < -0.9999f) {
        StorylandVec3 axis = vecCross({1.0f, 0.0f, 0.0f}, from);
        if (vecDot(axis, axis) < 0.000001f) axis = vecCross({0.0f, 1.0f, 0.0f}, from);
        axis = vecNormalize(axis);
        return {0.0f, axis.x, axis.y, axis.z};
    }
    StorylandVec3 axis = vecCross(from, to);
    return quatNormalize({1.0f + dot, axis.x, axis.y, axis.z});
}

static void glMultModelQuat(const StorylandQuat& q) {
    StorylandQuat n = quatNormalize(q);
    float xx = n.x * n.x;
    float yy = n.y * n.y;
    float zz = n.z * n.z;
    float xy = n.x * n.y;
    float xz = n.x * n.z;
    float yz = n.y * n.z;
    float wx = n.w * n.x;
    float wy = n.w * n.y;
    float wz = n.w * n.z;
    GLfloat m[16] = {
        1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),        2.0f * (xz - wy),        0.0f,
        2.0f * (xy - wz),        1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),        0.0f,
        2.0f * (xz + wy),        2.0f * (yz - wx),        1.0f - 2.0f * (xx + yy), 0.0f,
        0.0f,                    0.0f,                    0.0f,                    1.0f
    };
    glMultMatrixf(m);
}

static StorylandVec3 arcballPointFromMouse(HWND hwnd, int x, int y) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    float width = float(std::max<LONG>(1, rc.right - rc.left));
    float height = float(std::max<LONG>(1, rc.bottom - rc.top));
    float scale = std::min(width, height) * 0.48f;
    float px = (float(x) - width * 0.5f) / scale;
    float py = (height * 0.5f - float(y)) / scale;
    float len2 = px * px + py * py;
    if (len2 <= 1.0f) {
        return vecNormalize({px, py, std::sqrt(1.0f - len2)});
    }
    float inv = 1.0f / std::sqrt(len2);
    return {px * inv, py * inv, 0.0f};
}

static RECT viewCubeRect(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    int width = std::max<LONG>(1, rc.right - rc.left);
    int size = std::min(112, std::max(72, width / 8));
    int margin = 12;
    RECT out{};
    out.right = rc.right - margin;
    out.left = out.right - size;
    out.top = rc.top + margin;
    out.bottom = out.top + size;
    return out;
}

static bool pointInRect(const RECT& rc, int x, int y) {
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}

static bool pointInViewCube(HWND hwnd, int x, int y) {
    if (!gOpenGlShowViewCube) return false;
    return pointInRect(viewCubeRect(hwnd), x, y);
}

static StorylandVec3 viewCubePointFromMouse(HWND hwnd, int x, int y) {
    RECT rc = viewCubeRect(hwnd);
    float width = float(std::max<LONG>(1, rc.right - rc.left));
    float height = float(std::max<LONG>(1, rc.bottom - rc.top));
    float scale = std::min(width, height) * 0.46f;
    float px = (float(x) - float(rc.left + rc.right) * 0.5f) / scale;
    float py = (float(rc.top + rc.bottom) * 0.5f - float(y)) / scale;
    float len2 = px * px + py * py;
    if (len2 <= 1.0f) return vecNormalize({px, py, std::sqrt(1.0f - len2)});
    float inv = 1.0f / std::sqrt(len2);
    return {px * inv, py * inv, 0.0f};
}

static StorylandVec3 rotateVecByQuat(const StorylandQuat& q, StorylandVec3 v) {
    StorylandQuat n = quatNormalize(q);
    StorylandVec3 u{n.x, n.y, n.z};
    float s = n.w;
    StorylandVec3 uv = vecCross(u, v);
    StorylandVec3 uuv = vecCross(u, uv);
    return {
        v.x + 2.0f * (s * uv.x + uuv.x),
        v.y + 2.0f * (s * uv.y + uuv.y),
        v.z + 2.0f * (s * uv.z + uuv.z)
    };
}

static HGLRC gOpenGlContext = nullptr;
static bool gOpenGlReady = false;
static bool gModelLeftDrag = false;
static bool gModelRightDrag = false;
static POINT gModelLastMouse = {};
static StorylandQuat gModelViewRotation = {};
static StorylandQuat gModelDragStartRotation = {};
static StorylandVec3 gModelDragStartPoint = {};
static float gModelDistance = 3.5f;
static float gModelPanX = 0.0f;
static float gModelPanY = 0.0f;

static enum class StorylandMode { Empty, TextureArchive, DtzArchive, ModelFile, WblFile, ArchiveFile, AnimFile } gMode = StorylandMode::Empty;
static int gSelectedIndex = -1;


enum class StorylandTreeKind {
    None,
    Texture,
    DtzOverview,
    DtzHeader,
    DtzResourceHint,
    DtzSectorRecord,
    DtzDirEntry,
    DtzDataBlock,
    DtzDataField,
    ModelField,
    ModelBone,
    WblOverview,
    WblSection,
    WblMesh,
    WblBox,
    ArchiveEntry,
    ArchiveMeshResource,
    ArchiveTextureResource,
    ArchiveDirectTexture,
    ArchiveAnimationResource,
    AnimOverview,
    AnimClip,
    AnimTrack,
    AnimField,
    AnimString
};

static StorylandTreeKind gSelectedKind = StorylandTreeKind::None;

enum class DtzEmbeddedPreviewKind {
    None,
    TextureArchive,
    ModelFile
};

static DtzEmbeddedPreviewKind gDtzEmbeddedPreviewKind = DtzEmbeddedPreviewKind::None;
static int gDtzEmbeddedPreviewIndex = -1;
static std::wstring gDtzEmbeddedPreviewPath;

struct StorylandTreePayload {
    StorylandTreeKind kind = StorylandTreeKind::None;
    int index = -1;
};

static std::vector<StorylandTreePayload> gTreePayloads;

static std::vector<uint32_t> gArchiveMeshResourceIds;
static std::vector<uint32_t> gArchiveTextureIds;
static std::vector<int> gArchiveAnimationEntryIndices;

static LPARAM addTreePayload(StorylandTreeKind kind, int index) {
    gTreePayloads.push_back({kind, index});
    return LPARAM(gTreePayloads.size());
}

static StorylandTreePayload payloadFromLParam(LPARAM value) {
    if (value <= 0) return {};
    size_t index = size_t(value - 1);
    if (index >= gTreePayloads.size()) return {};
    return gTreePayloads[index];
}

static HTREEITEM addTreeItem(HTREEITEM parent, const std::wstring& text, StorylandTreeKind kind = StorylandTreeKind::None, int index = -1) {
    TVINSERTSTRUCTW insert = {};
    insert.hParent = parent;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM;
    insert.item.pszText = const_cast<LPWSTR>(text.c_str());
    insert.item.lParam = addTreePayload(kind, index);
    return reinterpret_cast<HTREEITEM>(SendMessageW(gTree, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&insert)));
}

static void expandTreeItem(HTREEITEM item) {
    if (item) SendMessageW(gTree, TVM_EXPAND, TVE_EXPAND, reinterpret_cast<LPARAM>(item));
}

static std::wstring widen(const std::string& text) {
    if (text.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
    return result;
}

static std::string narrow(const std::wstring& text) {
    if (text.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

static std::wstring getExtensionLower(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"";
    std::wstring ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return wchar_t(towlower(c)); });
    return ext;
}

static std::wstring canonicalDtzImgResourceName(const std::wstring& displayName) {
    std::wstring lower = displayName;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return wchar_t(towlower(c)); });

    const wchar_t* knownExtensions[] = {
        L".mdl", L".dff", L".wbl", L".xtx", L".chk", L".tex", L".anim", L".col", L".col2", L".dat", L".ipl", L".ide", L".cut", L".dir", L".bin", L".dtz"
    };

    size_t bestPos = std::wstring::npos;
    size_t bestEnd = std::wstring::npos;
    for (const wchar_t* extension : knownExtensions) {
        size_t pos = lower.find(extension);
        if (pos == std::wstring::npos) continue;
        size_t end = pos + wcslen(extension);
        if (bestPos == std::wstring::npos || pos < bestPos) {
            bestPos = pos;
            bestEnd = end;
        }
    }
    if (bestEnd != std::wstring::npos) return displayName.substr(0, bestEnd);

    size_t noteStart = displayName.find(L" (");
    if (noteStart != std::wstring::npos) return displayName.substr(0, noteStart);
    return displayName;
}

static std::wstring getDtzImgResourceExtensionLower(const std::wstring& displayName) {
    return getExtensionLower(canonicalDtzImgResourceName(displayName));
}

static std::wstring getDirectoryPart(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return L"";
    return path.substr(0, slash + 1);
}

static std::wstring getFileStemPart(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    size_t start = slash == std::wstring::npos ? 0 : slash + 1;
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || dot < start) dot = path.size();
    return path.substr(start, dot - start);
}

static std::wstring lowerWide(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return wchar_t(towlower(c)); });
    return value;
}

static bool containsWideNoCase(const std::wstring& text, const std::wstring& part) {
    if (part.empty()) return true;
    return lowerWide(text).find(lowerWide(part)) != std::wstring::npos;
}

static StorylandTitleTint titleTintFromPath(const std::wstring& path) {
    std::wstring ext = getExtensionLower(path);

    if (ext == L".chk") return StorylandTitleTint::LCS;
    if (ext == L".xtx") return StorylandTitleTint::VCS;
    if (ext == L".wbl" || ext == L".tex") return StorylandTitleTint::CTW;

    std::wstring lower = lowerWide(path);
    if (lower.find(L"chinatown") != std::wstring::npos || lower.find(L"ctw") != std::wstring::npos) return StorylandTitleTint::CTW;
    if (lower.find(L"vice city stories") != std::wstring::npos || lower.find(L"vcs") != std::wstring::npos) return StorylandTitleTint::VCS;
    if (lower.find(L"liberty city stories") != std::wstring::npos || lower.find(L"lcs") != std::wstring::npos) return StorylandTitleTint::LCS;

    return StorylandTitleTint::Default;
}

static void applyStorylandTitleTint(StorylandTitleTint tint) {
    gTitleTint = tint;
    if (!gMainWindow) return;

    DWORD captionColor = DWMWA_COLOR_DEFAULT;
    DWORD borderColor = DWMWA_COLOR_DEFAULT;
    DWORD textColor = DWMWA_COLOR_DEFAULT;

    switch (tint) {
    case StorylandTitleTint::LCS:
        captionColor = RGB(184, 224, 255);
        borderColor = RGB(105, 166, 218);
        textColor = RGB(18, 34, 50);
        break;
    case StorylandTitleTint::VCS:
        captionColor = RGB(255, 188, 218);
        borderColor = RGB(224, 108, 168);
        textColor = RGB(54, 24, 42);
        break;
    case StorylandTitleTint::CTW:
        captionColor = RGB(84, 8, 18);
        borderColor = RGB(130, 20, 30);
        textColor = RGB(255, 236, 236);
        break;
    default:
        break;
    }

    DwmSetWindowAttribute(gMainWindow, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
    DwmSetWindowAttribute(gMainWindow, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
    DwmSetWindowAttribute(gMainWindow, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));
}

static void applyStorylandTitleTintForPath(const std::wstring& path) {
    applyStorylandTitleTint(titleTintFromPath(path));
}

static bool fileExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static constexpr size_t STORYLAND_MAX_RECENT_FILES = 8;

static std::wstring escapeMenuLabel(std::wstring label) {
    size_t position = 0;
    while ((position = label.find(L'&', position)) != std::wstring::npos) {
        label.insert(position, 1, L'&');
        position += 2;
    }
    return label;
}

static void saveRecentFiles() {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Reigns Studios\\Storyland", 0, nullptr,
                        0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    for (size_t index = 0; index < STORYLAND_MAX_RECENT_FILES; ++index) {
        std::wstring valueName = L"Recent" + std::to_wstring(index);
        if (index < gRecentFiles.size()) {
            const std::wstring& path = gRecentFiles[index];
            RegSetValueExW(key, valueName.c_str(), 0, REG_SZ,
                reinterpret_cast<const BYTE*>(path.c_str()), DWORD((path.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(key, valueName.c_str());
        }
    }
    RegCloseKey(key);
}

static void rebuildRecentMenu() {
    if (!gRecentMenu) return;
    while (GetMenuItemCount(gRecentMenu) > 0) DeleteMenu(gRecentMenu, 0, MF_BYPOSITION);
    if (gRecentFiles.empty()) {
        AppendMenuW(gRecentMenu, MF_STRING | MF_GRAYED, ID_FILE_RECENT_BASE, L"(Empty)");
        return;
    }
    for (size_t index = 0; index < gRecentFiles.size(); ++index) {
        std::filesystem::path path(gRecentFiles[index]);
        std::wstring label = L"&" + std::to_wstring(index + 1) + L"  " +
            escapeMenuLabel(path.filename().wstring()) + L"  -  " + escapeMenuLabel(path.parent_path().wstring());
        AppendMenuW(gRecentMenu, MF_STRING, ID_FILE_RECENT_BASE + UINT(index), label.c_str());
    }
    AppendMenuW(gRecentMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(gRecentMenu, MF_STRING, ID_FILE_RECENT_CLEAR, L"Clear list");
}

static void loadRecentFiles() {
    gRecentFiles.clear();
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Reigns Studios\\Storyland", 0,
                      KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return;
    for (size_t index = 0; index < STORYLAND_MAX_RECENT_FILES; ++index) {
        wchar_t path[32768] = {};
        DWORD type = 0;
        DWORD bytes = sizeof(path);
        std::wstring valueName = L"Recent" + std::to_wstring(index);
        if (RegQueryValueExW(key, valueName.c_str(), nullptr, &type,
                            reinterpret_cast<BYTE*>(path), &bytes) == ERROR_SUCCESS &&
            type == REG_SZ && path[0] && fileExists(path)) {
            std::wstring candidate(path);
            if (std::find(gRecentFiles.begin(), gRecentFiles.end(), candidate) == gRecentFiles.end())
                gRecentFiles.push_back(std::move(candidate));
        }
    }
    RegCloseKey(key);
    saveRecentFiles();
}

static void addRecentFile(const std::wstring& path) {
    if (path.empty() || !fileExists(path)) return;
    gRecentFiles.erase(std::remove_if(gRecentFiles.begin(), gRecentFiles.end(),
        [&](const std::wstring& existing) { return _wcsicmp(existing.c_str(), path.c_str()) == 0; }), gRecentFiles.end());
    gRecentFiles.insert(gRecentFiles.begin(), path);
    if (gRecentFiles.size() > STORYLAND_MAX_RECENT_FILES) gRecentFiles.resize(STORYLAND_MAX_RECENT_FILES);
    saveRecentFiles();
    rebuildRecentMenu();
}

static void openStorylandFile(const std::wstring& path);
static bool writeWholeFileBinary(const std::wstring& path, const std::vector<uint8_t>& bytes, std::string& error);
static std::wstring buildDtzPreviewExtractRoot();
static void clearDtzEmbeddedPreviewState();
static bool currentModeUsesInteractiveModelViewport();
static void drawOpenGlViewCube(HWND hwnd, int width, int height);
static void drawOpenGlViewCubeLabels(HWND hwnd, HDC dc);
static void drawWblPreviewOpenGl(HWND hwnd, HDC dc, RECT rc);
static void applyModelViewportZoom(float scale);
static void fitModelViewportCloser();
static bool handleModelViewportShortcut(WPARAM key);
static bool prepareDtzDirEntryPreview(int index, std::wstring& previewSummary);
static void openSelectedDtzDirEntryStandalone();

static bool sameWideNoCase(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (towlower(a[i]) != towlower(b[i])) return false;
    }
    return true;
}

static bool textureArchiveExtension(const std::wstring& path) {
    std::wstring ext = getExtensionLower(path);
    return ext == L".xtx" || ext == L".chk" || ext == L".tex";
}

static void appendUniquePath(std::vector<std::wstring>& paths, const std::wstring& path) {
    if (path.empty()) return;
    for (const std::wstring& existing : paths) {
        if (sameWideNoCase(existing, path)) return;
    }
    paths.push_back(path);
}

static std::vector<std::wstring> collectCompanionTextureCandidates(const std::wstring& modelPath) {
    std::vector<std::wstring> paths;
    std::wstring directory = getDirectoryPart(modelPath);
    std::wstring stem = getFileStemPart(modelPath);

    const wchar_t* directExtensions[] = {L".xtx", L".chk", L".tex", L".XTX", L".CHK", L".TEX"};
    for (const wchar_t* extension : directExtensions) {
        std::wstring candidate = directory + stem + extension;
        if (fileExists(candidate)) appendUniquePath(paths, candidate);
    }

    WIN32_FIND_DATAW findData = {};
    HANDLE find = FindFirstFileW((directory + L"*").c_str(), &findData);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring candidate = directory + findData.cFileName;
            if (!textureArchiveExtension(candidate)) continue;
            std::wstring candidateStem = getFileStemPart(candidate);
            if (sameWideNoCase(candidateStem, stem)) appendUniquePath(paths, candidate);
        } while (FindNextFileW(find, &findData));
        FindClose(find);
    }

    const wchar_t* patterns[] = {L"*.xtx", L"*.chk", L"*.tex", L"*.XTX", L"*.CHK", L"*.TEX"};
    for (const wchar_t* pattern : patterns) {
        find = FindFirstFileW((directory + pattern).c_str(), &findData);
        if (find == INVALID_HANDLE_VALUE) continue;
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            appendUniquePath(paths, directory + findData.cFileName);
        } while (FindNextFileW(find, &findData));
        FindClose(find);
    }

    return paths;
}

static std::string asciiLower(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return result;
}

static std::string lowerStemUtf8(const std::wstring& path) {
    return asciiLower(narrow(getFileStemPart(path)));
}

static std::wstring hexWide(uint32_t value, int width = 8) {
    std::wstringstream ss;
    ss << L"0x" << std::uppercase << std::hex << std::setw(width) << std::setfill(L'0') << value;
    return ss.str();
}

static void setStatus(const std::wstring& text) {
    gStatusText = text;
    SendMessageW(gStatus, SB_SETTEXTW,
        gEyeFriendlyPaneBackground ? SBT_OWNERDRAW : 0,
        reinterpret_cast<LPARAM>(gStatusText.c_str()));
}

static std::wstring buildModelStatusLine() {
    std::wstringstream status;
    status << L"MDL | " << widen(gModelFile.modelKindName())
           << L" | " << gModelFile.armatureBones().size() << L" bones";
    if (gModelTextureLoaded && !gModelTexturePath.empty()) {
        status << L" | " << std::filesystem::path(gModelTexturePath).filename().wstring()
               << L" | " << gModelTextureRegions.size() << L" textures"
               << L" | atlas " << gModelTextureImage.width << L"x" << gModelTextureImage.height;
    } else {
        status << L" | no textures";
    }
    if (gModelAnimLoaded) status << L" | ANIM";
    return status.str();
}

static void setDetails(const std::wstring& text) {
    SetWindowTextW(gDetails, text.c_str());
}

static std::wstring openFileDialog(const wchar_t* filter) {
    wchar_t fileName[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = gMainWindow;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return L"";
    return fileName;
}

static std::wstring saveFileDialog(const wchar_t* filter, const wchar_t* defaultExt) {
    wchar_t fileName[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = gMainWindow;
    ofn.lpstrFilter = filter;
    ofn.lpstrDefExt = defaultExt;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return L"";
    return fileName;
}

static std::wstring saveFileDialogWithInitial(const wchar_t* filter, const wchar_t* defaultExt, const std::wstring& initialPath) {
    wchar_t fileName[MAX_PATH] = {};
    if (!initialPath.empty()) {
        wcsncpy_s(fileName, initialPath.c_str(), MAX_PATH - 1);
    }

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = gMainWindow;
    ofn.lpstrFilter = filter;
    ofn.lpstrDefExt = defaultExt;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return L"";
    return fileName;
}

static std::wstring sameFolderSameStemWithExtension(const std::wstring& path, const std::wstring& extensionWithDot) {
    size_t slash = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');

    bool dotIsInFileName = dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash);
    if (dotIsInFileName) {
        return path.substr(0, dot) + extensionWithDot;
    }

    return path + extensionWithDot;
}

static std::wstring storylandTempLogPath() {
    wchar_t tempPath[MAX_PATH] = {};
    DWORD count = GetTempPathW(MAX_PATH, tempPath);
    std::wstring root = count > 0 ? std::wstring(tempPath) : L".\\";
    if (!root.empty() && root.back() != L'\\' && root.back() != L'/') root += L"\\";
    return root + L"Storyland_export_log.txt";
}

static void copyTextToClipboard(const std::wstring& text) {
    if (!OpenClipboard(gMainWindow)) return;
    EmptyClipboard();

    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory) {
        void* data = GlobalLock(memory);
        if (data) {
            memcpy(data, text.c_str(), bytes);
            GlobalUnlock(memory);
            SetClipboardData(CF_UNICODETEXT, memory);
        } else {
            GlobalFree(memory);
        }
    }

    CloseClipboard();
}

static bool readBinaryFileForUi(const std::wstring& path, std::vector<uint8_t>& bytes, std::string& error) {
    FILE* file = nullptr;
#ifdef _WIN32
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr) {
        error = "Could not open the selected file.";
        return false;
    }
#else
    file = fopen(std::string(path.begin(), path.end()).c_str(), "rb");
    if (!file) {
        error = "Could not open the selected file.";
        return false;
    }
#endif

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        error = "Could not seek the selected file.";
        return false;
    }

    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        error = "Could not get selected file size.";
        return false;
    }

    rewind(file);
    bytes.resize(size_t(size));
    if (!bytes.empty()) {
        size_t readCount = fread(bytes.data(), 1, bytes.size(), file);
        if (readCount != bytes.size()) {
            fclose(file);
            error = "Could not read the selected file.";
            return false;
        }
    }

    fclose(file);
    return true;
}

static bool askUnsigned(const wchar_t* title, const wchar_t* label, uint32_t initialValue, uint32_t& valueOut, bool includeShiftCheck, bool& shiftOut) {
    struct DialogState { const wchar_t* label; uint32_t value; bool includeShift; bool shift; } state{label, initialValue, includeShiftCheck, true};

    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = gInstance;
    wc.lpszClassName = L"StorylandInputBoxClass";
    RegisterClassW(&wc);

    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, wc.lpszClassName, title, WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 360, includeShiftCheck ? 170 : 130, gMainWindow, nullptr, gInstance, nullptr);
    if (!dialog) return false;

    CreateWindowW(L"STATIC", state.label, WS_CHILD | WS_VISIBLE, 12, 14, 320, 20, dialog, nullptr, gInstance, nullptr);
    HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(initialValue).c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 12, 38, 320, 24, dialog, nullptr, gInstance, nullptr);
    HWND check = nullptr;
    if (includeShiftCheck) {
        check = CreateWindowW(L"BUTTON", L"Shift later start sectors by delta", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 12, 70, 270, 22, dialog, nullptr, gInstance, nullptr);
        SendMessageW(check, BM_SETCHECK, BST_CHECKED, 0);
    }
    HWND ok = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 170, includeShiftCheck ? 104 : 70, 75, 26, dialog, reinterpret_cast<HMENU>(IDOK), gInstance, nullptr);
    HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 255, includeShiftCheck ? 104 : 70, 75, 26, dialog, reinterpret_cast<HMENU>(IDCANCEL), gInstance, nullptr);
    SendMessageW(edit, EM_SETSEL, 0, -1);

    RECT parentRect = {};
    GetWindowRect(gMainWindow, &parentRect);
    SetWindowPos(dialog, HWND_TOP, parentRect.left + 80, parentRect.top + 80, 0, 0, SWP_NOSIZE);
    EnableWindow(gMainWindow, FALSE);
    ShowWindow(dialog, SW_SHOW);
    SetFocus(edit);

    bool accepted = false;
    MSG msg = {};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.hwnd == ok || msg.hwnd == cancel || IsChild(dialog, msg.hwnd)) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
                wchar_t buffer[64] = {};
                GetWindowTextW(edit, buffer, 64);
                valueOut = wcstoul(buffer, nullptr, 0);
                shiftOut = check ? (SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
                accepted = true;
                DestroyWindow(dialog);
                break;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                DestroyWindow(dialog);
                break;
            }
        }
        if (msg.message == WM_COMMAND && LOWORD(msg.wParam) == IDOK) {
            wchar_t buffer[64] = {};
            GetWindowTextW(edit, buffer, 64);
            valueOut = wcstoul(buffer, nullptr, 0);
            shiftOut = check ? (SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
            accepted = true;
            DestroyWindow(dialog);
            break;
        }
        if (msg.message == WM_COMMAND && LOWORD(msg.wParam) == IDCANCEL) {
            DestroyWindow(dialog);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(gMainWindow, TRUE);
    SetActiveWindow(gMainWindow);
    return accepted;
}

static bool askString(const wchar_t* title, const wchar_t* label, const std::wstring& initialValue, std::wstring& valueOut) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = gInstance;
    wc.lpszClassName = L"StorylandStringInputBoxClass";
    RegisterClassW(&wc);

    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        wc.lpszClassName,
        title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        420,
        140,
        gMainWindow,
        nullptr,
        gInstance,
        nullptr
    );
    if (!dialog) return false;

    CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE, 12, 14, 380, 20, dialog, nullptr, gInstance, nullptr);
    HWND edit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        initialValue.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        12,
        38,
        380,
        24,
        dialog,
        nullptr,
        gInstance,
        nullptr
    );
    HWND ok = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 232, 76, 75, 26, dialog, reinterpret_cast<HMENU>(IDOK), gInstance, nullptr);
    HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 317, 76, 75, 26, dialog, reinterpret_cast<HMENU>(IDCANCEL), gInstance, nullptr);
    SendMessageW(edit, EM_SETLIMITTEXT, 63, 0);
    SendMessageW(edit, EM_SETSEL, 0, -1);

    RECT parentRect = {};
    GetWindowRect(gMainWindow, &parentRect);
    SetWindowPos(dialog, HWND_TOP, parentRect.left + 80, parentRect.top + 80, 0, 0, SWP_NOSIZE);
    EnableWindow(gMainWindow, FALSE);
    ShowWindow(dialog, SW_SHOW);
    SetFocus(edit);

    bool accepted = false;
    MSG msg = {};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.hwnd == ok || msg.hwnd == cancel || IsChild(dialog, msg.hwnd)) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
                wchar_t buffer[256] = {};
                GetWindowTextW(edit, buffer, 256);
                valueOut = buffer;
                accepted = true;
                DestroyWindow(dialog);
                break;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                DestroyWindow(dialog);
                break;
            }
        }
        if (msg.message == WM_COMMAND && LOWORD(msg.wParam) == IDOK) {
            wchar_t buffer[256] = {};
            GetWindowTextW(edit, buffer, 256);
            valueOut = buffer;
            accepted = true;
            DestroyWindow(dialog);
            break;
        }
        if (msg.message == WM_COMMAND && LOWORD(msg.wParam) == IDCANCEL) {
            DestroyWindow(dialog);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(gMainWindow, TRUE);
    SetActiveWindow(gMainWindow);
    return accepted;
}

static std::string narrowAscii(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t c : text) {
        if (c < 32 || c >= 127) return std::string();
        out.push_back(char(c));
    }
    return out;
}

static void deleteTextureBitmap() {
    if (gTextureBitmap) {
        DeleteObject(gTextureBitmap);
        gTextureBitmap = nullptr;
    }
}

static void createTextureBitmapFromImage() {
    deleteTextureBitmap();
    if (gCurrentImage.width <= 0 || gCurrentImage.height <= 0 || gCurrentImage.rgba.empty()) return;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = gCurrentImage.width;
    bmi.bmiHeader.biHeight = -gCurrentImage.height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint8_t> bgra(size_t(gCurrentImage.width) * size_t(gCurrentImage.height) * 4);
    for (size_t i = 0; i + 3 < gCurrentImage.rgba.size(); i += 4) {
        bgra[i + 0] = gCurrentImage.rgba[i + 2];
        bgra[i + 1] = gCurrentImage.rgba[i + 1];
        bgra[i + 2] = gCurrentImage.rgba[i + 0];
        bgra[i + 3] = gCurrentImage.rgba[i + 3];
    }

    HDC dc = GetDC(gPreview);
    void* bits = nullptr;
    gTextureBitmap = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (gTextureBitmap && bits) memcpy(bits, bgra.data(), bgra.size());
    ReleaseDC(gPreview, dc);
}


static void clearModelTexture() {
    if (gModelTextureId != 0 && gOpenGlReady && gPreview && gOpenGlContext) {
        HDC dc = GetDC(gPreview);
        if (dc && wglMakeCurrent(dc, gOpenGlContext)) {
            glDeleteTextures(1, &gModelTextureId);
            wglMakeCurrent(nullptr, nullptr);
        }
        if (dc) ReleaseDC(gPreview, dc);
    }

    gModelTextureArchive = LeedsTextureArchive();
    gModelTextureImage = {};
    gModelTexturePath.clear();
    gModelTextureName.clear();
    gModelTextureStatus.clear();
    gModelTextureIndex = -1;
    gModelTextureLoaded = false;
    gModelTextureUploadNeeded = false;
    gModelTextureRegions.clear();
    gModelTextureId = 0;
}

static int scoreModelTextureEntry(const LeedsTextureEntry& entry, const std::vector<std::string>& hints, const std::string& modelStem) {
    std::string textureName = asciiLower(entry.name);
    int score = 0;

    if (!modelStem.empty()) {
        if (textureName == modelStem) score += 2000;
        if (textureName.find(modelStem) != std::string::npos) score += 900;
        if (modelStem.find(textureName) != std::string::npos && textureName.size() >= 3) score += 350;
    }

    for (const std::string& hintRaw : hints) {
        std::string hint = asciiLower(hintRaw);
        if (hint.empty()) continue;
        if (textureName == hint) score += 10000;
        else if (textureName.find(hint) != std::string::npos) score += 5000;
        else if (hint.find(textureName) != std::string::npos && textureName.size() >= 3) score += 2200;
    }

    if (textureName.find("head") != std::string::npos) score += 30;
    if (textureName.find("body") != std::string::npos) score += 25;
    if (textureName.find("top") != std::string::npos) score += 20;
    if (textureName.find("skin") != std::string::npos) score += 20;
    return score;
}

static bool tryLoadModelTextureArchive(const std::wstring& archivePath, const std::string& modelStem, int baseScore, std::wstring& bestPath, int& bestTextureIndex, int& bestScore) {
    if (!fileExists(archivePath)) return false;

    std::string error;
    LeedsTextureArchive archive;
    if (!archive.loadFromFile(archivePath, LeedsPlatform::Auto, error)) return false;

    const auto& textures = archive.textures();
    if (textures.empty()) return false;

    int localBestIndex = 0;
    int localBestScore = std::numeric_limits<int>::min();
    const auto& hints = gModelFile.textureNameHints();

    for (size_t index = 0; index < textures.size(); ++index) {
        int score = baseScore + scoreModelTextureEntry(textures[index], hints, modelStem);
        if (score > localBestScore) {
            localBestScore = score;
            localBestIndex = int(index);
        }
    }

    if (localBestScore > bestScore) {
        bestPath = archivePath;
        bestTextureIndex = localBestIndex;
        bestScore = localBestScore;
    }
    return true;
}

static bool findBestCompanionTextureArchive(const std::wstring& modelPath, std::wstring& archivePathOut, int& textureIndexOut) {
    std::wstring directory = getDirectoryPart(modelPath);
    std::wstring stem = getFileStemPart(modelPath);
    std::string modelStem = lowerStemUtf8(modelPath);

    int bestScore = std::numeric_limits<int>::min();
    archivePathOut.clear();
    textureIndexOut = -1;

    const wchar_t* extensions[] = {L".xtx", L".chk", L".tex", L".XTX", L".CHK", L".TEX"};
    for (const wchar_t* extension : extensions) {


        tryLoadModelTextureArchive(directory + stem + extension, modelStem, 50000, archivePathOut, textureIndexOut, bestScore);
    }

    WIN32_FIND_DATAW findData = {};
    const wchar_t* patterns[] = {L"*.xtx", L"*.chk", L"*.tex", L"*.XTX", L"*.CHK", L"*.TEX"};
    for (const wchar_t* pattern : patterns) {
        HANDLE find = FindFirstFileW((directory + pattern).c_str(), &findData);
        if (find == INVALID_HANDLE_VALUE) continue;
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring candidate = directory + findData.cFileName;
            int archiveBaseScore = 0;
            std::string candidateStem = lowerStemUtf8(candidate);
            if (!modelStem.empty()) {
                if (candidateStem == modelStem) archiveBaseScore += 50000;
                else if (candidateStem.find(modelStem) != std::string::npos || modelStem.find(candidateStem) != std::string::npos) archiveBaseScore += 1200;
            }
            tryLoadModelTextureArchive(candidate, modelStem, archiveBaseScore, archivePathOut, textureIndexOut, bestScore);
        } while (FindNextFileW(find, &findData));
        FindClose(find);
    }

    return !archivePathOut.empty() && textureIndexOut >= 0;
}

static bool decodePreferredOrFallbackTexture(int preferredIndex, RgbaImage& image, int& decodedIndexOut, std::string& errorOut) {
    const auto& textures = gModelTextureArchive.textures();
    decodedIndexOut = -1;
    errorOut.clear();
    if (textures.empty()) {
        errorOut = "Archive contains no texture entries.";
        return false;
    }

    std::vector<int> trialOrder;
    if (preferredIndex >= 0 && size_t(preferredIndex) < textures.size()) {
        trialOrder.push_back(preferredIndex);
    }
    for (size_t index = 0; index < textures.size(); ++index) {
        if (int(index) != preferredIndex) trialOrder.push_back(int(index));
    }

    for (int index : trialOrder) {
        std::string localError;
        RgbaImage candidate;
        if (gModelTextureArchive.decodeTexture(size_t(index), candidate, localError)) {
            image = std::move(candidate);
            decodedIndexOut = index;
            return true;
        }
        if (!localError.empty()) {
            if (!errorOut.empty()) errorOut += "; ";
            errorOut += textures[size_t(index)].name + ": " + localError;
        }
    }

    if (errorOut.empty()) errorOut = "Every texture entry failed to decode.";
    return false;
}


static bool buildModelTextureAtlasFromCurrentArchive(int preferredIndex, int& firstDecodedIndexOut, int& decodedCountOut, std::string& errorOut) {
    firstDecodedIndexOut = -1;
    decodedCountOut = 0;
    errorOut.clear();
    gModelTextureImage = {};
    gModelTextureRegions.clear();

    const auto& textures = gModelTextureArchive.textures();
    if (textures.empty()) {
        errorOut = "Archive contains no texture entries.";
        return false;
    }

    std::vector<int> trialOrder;
    if (preferredIndex >= 0 && size_t(preferredIndex) < textures.size()) trialOrder.push_back(preferredIndex);
    for (size_t index = 0; index < textures.size(); ++index) {
        if (int(index) != preferredIndex) trialOrder.push_back(int(index));
    }

    struct DecodedTextureForAtlas {
        int sourceIndex = -1;
        RgbaImage image;
    };

    std::vector<DecodedTextureForAtlas> decoded;
    decoded.reserve(trialOrder.size());

    for (int index : trialOrder) {
        if (index < 0 || size_t(index) >= textures.size()) continue;
        std::string localError;
        RgbaImage image;
        if (gModelTextureArchive.decodeTexture(size_t(index), image, localError) && image.width > 0 && image.height > 0 && !image.rgba.empty()) {
            if (firstDecodedIndexOut < 0) firstDecodedIndexOut = index;
            decoded.push_back({index, std::move(image)});
        } else if (!localError.empty()) {
            if (!errorOut.empty()) errorOut += "; ";
            errorOut += textures[size_t(index)].name + ": " + localError;
        }
    }

    if (decoded.empty()) {
        if (errorOut.empty()) errorOut = "Every texture entry failed to decode.";
        return false;
    }

    const int maximumAtlasWidth = 4096;
    int atlasWidth = 0;
    int atlasHeight = 0;
    int rowWidth = 0;
    int rowHeight = 0;
    for (const auto& item : decoded) {
        if (rowWidth > 0 && rowWidth + item.image.width > maximumAtlasWidth) {
            atlasWidth = std::max(atlasWidth, rowWidth);
            atlasHeight += rowHeight;
            rowWidth = 0;
            rowHeight = 0;
        }
        rowWidth += item.image.width;
        rowHeight = std::max(rowHeight, item.image.height);
    }
    atlasWidth = std::max(atlasWidth, rowWidth);
    atlasHeight += rowHeight;
    atlasWidth = std::max(1, atlasWidth);
    atlasHeight = std::max(1, atlasHeight);

    RgbaImage atlas;
    atlas.width = atlasWidth;
    atlas.height = atlasHeight;
    atlas.rgba.assign(size_t(atlasWidth) * size_t(atlasHeight) * 4u, 0);

    int cursorX = 0;
    int cursorY = 0;
    rowHeight = 0;
    for (const auto& item : decoded) {
        if (cursorX > 0 && cursorX + item.image.width > maximumAtlasWidth) {
            cursorX = 0;
            cursorY += rowHeight;
            rowHeight = 0;
        }

        for (int y = 0; y < item.image.height; ++y) {
            for (int x = 0; x < item.image.width; ++x) {
                size_t src = (size_t(y) * size_t(item.image.width) + size_t(x)) * 4u;
                size_t dst = (size_t(cursorY + y) * size_t(atlasWidth) + size_t(cursorX + x)) * 4u;
                atlas.rgba[dst + 0] = item.image.rgba[src + 0];
                atlas.rgba[dst + 1] = item.image.rgba[src + 1];
                atlas.rgba[dst + 2] = item.image.rgba[src + 2];
                atlas.rgba[dst + 3] = item.image.rgba[src + 3];
            }
        }

        StorylandModelTextureRegion region;
        region.sourceIndex = item.sourceIndex;
        region.name = textures[size_t(item.sourceIndex)].name;
        region.x = cursorX;
        region.y = cursorY;
        region.width = item.image.width;
        region.height = item.image.height;
        region.u0 = float(cursorX) / float(atlasWidth);
        region.v0 = float(cursorY) / float(atlasHeight);
        region.u1 = float(cursorX + item.image.width) / float(atlasWidth);
        region.v1 = float(cursorY + item.image.height) / float(atlasHeight);
        gModelTextureRegions.push_back(region);

        cursorX += item.image.width;
        rowHeight = std::max(rowHeight, item.image.height);
    }

    gModelTextureImage = std::move(atlas);
    decodedCountOut = int(gModelTextureRegions.size());
    if (decodedCountOut == 1) {
        gModelTextureName = gModelTextureRegions.front().name;
    } else {
        std::ostringstream label;
        label << "atlas " << decodedCountOut << " textures";
        gModelTextureName = label.str();
    }
    return true;
}

static bool loadCompanionTextureForCurrentModel(const std::wstring& modelPath, std::wstring& statusOut) {
    clearModelTexture();

    std::vector<std::wstring> candidatePaths = collectCompanionTextureCandidates(modelPath);
    if (candidatePaths.empty()) {
        statusOut = L"No companion .xtx/.chk/.tex found beside MDL.";
        gModelTextureStatus = statusOut;
        return false;
    }

    std::string modelStem = lowerStemUtf8(modelPath);
    const auto& hints = gModelFile.textureNameHints();
    std::wstring triedSummary;
    int failedArchiveCount = 0;
    int failedDecodeCount = 0;

    for (size_t candidateIndex = 0; candidateIndex < candidatePaths.size(); ++candidateIndex) {
        const std::wstring& archivePath = candidatePaths[candidateIndex];
        LeedsTextureArchive archive;
        std::string error;
        if (!archive.loadFromFile(archivePath, LeedsPlatform::Auto, error)) {
            ++failedArchiveCount;
            if (triedSummary.size() < 900) {
                triedSummary += L"\r\n  open failed: " + archivePath + L" -> " + widen(error);
            }
            continue;
        }

        const auto& textures = archive.textures();
        if (textures.empty()) {
            ++failedArchiveCount;
            if (triedSummary.size() < 900) triedSummary += L"\r\n  empty archive: " + archivePath;
            continue;
        }

        int baseScore = 0;
        std::wstring archiveStemWide = getFileStemPart(archivePath);
        if (sameWideNoCase(archiveStemWide, getFileStemPart(modelPath))) baseScore += 100000;

        int preferredIndex = 0;
        int preferredScore = std::numeric_limits<int>::min();
        for (size_t textureIndex = 0; textureIndex < textures.size(); ++textureIndex) {
            int score = baseScore + scoreModelTextureEntry(textures[textureIndex], hints, modelStem);
            if (score > preferredScore) {
                preferredScore = score;
                preferredIndex = int(textureIndex);
            }
        }

        gModelTextureArchive = std::move(archive);
        int decodedIndex = -1;
        int decodedCount = 0;
        if (!buildModelTextureAtlasFromCurrentArchive(preferredIndex, decodedIndex, decodedCount, error)) {
            ++failedDecodeCount;
            gModelTextureArchive = LeedsTextureArchive();
            gModelTextureRegions.clear();
            if (triedSummary.size() < 900) {
                triedSummary += L"\r\n  decode failed: " + archivePath + L" -> " + widen(error);
            }
            continue;
        }

        gModelTexturePath = archivePath;
        gModelTextureIndex = decodedIndex;
        gModelTextureLoaded = true;
        gModelTextureUploadNeeded = true;

        std::wstringstream ss;
        ss << L"Auto-loaded companion texture atlas: " << archivePath << L" -> " << decodedCount
           << L" decoded texture" << (decodedCount == 1 ? L"" : L"s")
           << L" (atlas " << gModelTextureImage.width << L"x" << gModelTextureImage.height << L")";
        if (decodedIndex != preferredIndex) {
            ss << L" [preferred entry " << preferredIndex << L", first decoded entry " << decodedIndex << L"]";
        }
        if (candidateIndex > 0) {
            ss << L" [candidate " << (candidateIndex + 1) << L"/" << candidatePaths.size() << L"]";
        }
        statusOut = ss.str();
        gModelTextureStatus = statusOut;
        return true;
    }

    std::wstringstream ss;
    ss << L"Companion texture candidates found, but none loaded. Candidates=" << candidatePaths.size()
       << L", open failures=" << failedArchiveCount
       << L", decode failures=" << failedDecodeCount;
    if (!triedSummary.empty()) ss << triedSummary;
    statusOut = ss.str();
    gModelTextureStatus = statusOut;
    return false;
}

static bool uploadModelTextureIfNeeded() {
    if (!gModelTextureLoaded || gModelTextureImage.rgba.empty()) return false;

    if (gModelTextureId == 0) {
        glGenTextures(1, &gModelTextureId);
        gModelTextureUploadNeeded = true;
    }
    if (gModelTextureId == 0) return false;

    if (gModelTextureUploadNeeded) {
        glBindTexture(GL_TEXTURE_2D, gModelTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            gModelTextureImage.width,
            gModelTextureImage.height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            gModelTextureImage.rgba.data()
        );
        gModelTextureUploadNeeded = false;
    } else {
        glBindTexture(GL_TEXTURE_2D, gModelTextureId);
    }

    return true;
}

static bool modelTexcoordsLookUsable(const std::vector<StorylandModelTexcoord>& texcoords) {
    if (texcoords.size() < 3) return false;
    float minU = texcoords[0].u;
    float maxU = texcoords[0].u;
    float minV = texcoords[0].v;
    float maxV = texcoords[0].v;
    for (const auto& uv : texcoords) {
        if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) return false;
        minU = std::min(minU, uv.u);
        maxU = std::max(maxU, uv.u);
        minV = std::min(minV, uv.v);
        maxV = std::max(maxV, uv.v);
    }
    return std::fabs(maxU - minU) > 0.00001f || std::fabs(maxV - minV) > 0.00001f;
}

static float applyModelTextureVOption(float v) {
    if (gModelFlipTextureV) return 1.0f - v;
    return v;
}

static void emitModelPreviewTexcoord(
    size_t vertexIndex,
    const std::vector<StorylandModelPoint>& points,
    const std::vector<StorylandModelTexcoord>& texcoords,
    bool useRealTexcoords,
    float minX,
    float minY,
    float minZ,
    float spanX,
    float spanY,
    float spanZ
) {
    if (useRealTexcoords && vertexIndex < texcoords.size()) {
        const auto& uv = texcoords[vertexIndex];
        glTexCoord2f(uv.u, applyModelTextureVOption(uv.v));
        return;
    }

    if (vertexIndex >= points.size()) {
        glTexCoord2f(0.0f, 0.0f);
        return;
    }

    const auto& p = points[vertexIndex];
    float u = 0.0f;
    float v = 0.0f;

    if (spanX >= 0.00001f) {
        u = (p.x - minX) / spanX;
    }

    if (spanZ >= 0.00001f) {
        v = 1.0f - ((p.z - minZ) / spanZ);
    } else if (spanY >= 0.00001f) {
        v = 1.0f - ((p.y - minY) / spanY);
    }

    glTexCoord2f(u, v);
}


static void computeModelPreviewTexcoord(
    size_t vertexIndex,
    const std::vector<StorylandModelPoint>& points,
    const std::vector<StorylandModelTexcoord>& texcoords,
    bool useRealTexcoords,
    float minX,
    float minY,
    float minZ,
    float spanX,
    float spanY,
    float spanZ,
    float& uOut,
    float& vOut
) {
    if (useRealTexcoords && vertexIndex < texcoords.size()) {
        const auto& uv = texcoords[vertexIndex];
        uOut = uv.u;
        vOut = applyModelTextureVOption(uv.v);
        return;
    }

    if (vertexIndex >= points.size()) {
        uOut = 0.0f;
        vOut = 0.0f;
        return;
    }

    const auto& p = points[vertexIndex];
    float u = 0.0f;
    float v = 0.0f;
    if (spanX >= 0.00001f) u = (p.x - minX) / spanX;
    if (spanZ >= 0.00001f) v = 1.0f - ((p.z - minZ) / spanZ);
    else if (spanY >= 0.00001f) v = 1.0f - ((p.y - minY) / spanY);
    uOut = u;
    vOut = applyModelTextureVOption(v);
}

static int findModelTextureRegionByNeedles(std::initializer_list<const char*> needles) {
    for (const char* needle : needles) {
        for (size_t index = 0; index < gModelTextureRegions.size(); ++index) {
            std::string name = asciiLower(gModelTextureRegions[index].name);
            if (name.find(needle) != std::string::npos) return int(index);
        }
    }
    return -1;
}

static int findModelTextureRegionByMaterialName(const std::string& materialName) {
    if (materialName.empty()) return -1;
    std::string material = asciiLower(materialName);
    for (size_t index = 0; index < gModelTextureRegions.size(); ++index) {
        std::string region = asciiLower(gModelTextureRegions[index].name);
        if (region == material) return int(index);
    }
    for (size_t index = 0; index < gModelTextureRegions.size(); ++index) {
        std::string region = asciiLower(gModelTextureRegions[index].name);
        if (region.find(material) != std::string::npos || material.find(region) != std::string::npos) return int(index);
    }
    return -1;
}

static int chooseModelTextureRegionForTriangle(
    const StorylandModelTriangle& tri,
    const std::vector<StorylandModelPoint>& points,
    float minX,
    float minY,
    float minZ,
    float spanX,
    float spanY,
    float spanZ
) {
    if (gModelTextureRegions.empty()) return -1;
    if (gModelTextureRegions.size() == 1) return 0;

    if (tri.materialIndex != 0xFFFFFFFFu) {
        const auto& materialNames = gModelFile.previewMaterialTextureNames();
        if (size_t(tri.materialIndex) < materialNames.size()) {
            int exactRegion = findModelTextureRegionByMaterialName(materialNames[size_t(tri.materialIndex)]);
            if (exactRegion >= 0) return exactRegion;
        }
    }

    if (tri.a >= points.size() || tri.b >= points.size() || tri.c >= points.size()) return 0;

    const auto& a = points[tri.a];
    const auto& b = points[tri.b];
    const auto& c = points[tri.c];
    float cx = (a.x + b.x + c.x) / 3.0f;
    float cy = (a.y + b.y + c.y) / 3.0f;
    float cz = (a.z + b.z + c.z) / 3.0f;

    float nx = spanX > 0.00001f ? (cx - minX) / spanX : 0.5f;
    float ny = spanY > 0.00001f ? (cy - minY) / spanY : 0.5f;
    float nz = spanZ > 0.00001f ? (cz - minZ) / spanZ : 0.5f;
    float sideAmount = std::fabs(nx - 0.5f);

    int chosen = -1;
    if (nz < 0.12f) chosen = findModelTextureRegionByNeedles({"shoe", "boot", "feet", "foot"});
    if (chosen < 0 && nz < 0.46f) chosen = findModelTextureRegionByNeedles({"trouser", "pants", "jean", "leg"});
    if (chosen < 0 && sideAmount > 0.27f && nz > 0.35f && nz < 0.82f) chosen = findModelTextureRegionByNeedles({"arm", "hand", "skin"});
    if (chosen < 0 && nz > 0.78f) chosen = findModelTextureRegionByNeedles({"head", "face", "hair"});
    if (chosen < 0 && nz > 0.42f && nz < 0.78f && sideAmount < 0.14f) chosen = findModelTextureRegionByNeedles({"tie"});
    if (chosen < 0 && nz > 0.38f && nz < 0.82f) chosen = findModelTextureRegionByNeedles({"shirt", "top", "torso", "body"});
    if (chosen < 0) chosen = findModelTextureRegionByNeedles({"body", "top", "shirt", "head", "trouser", "pants"});
    if (chosen < 0) chosen = 0;
    return chosen;
}

static void emitModelPreviewTexcoordInRegion(
    size_t vertexIndex,
    int regionIndex,
    const std::vector<StorylandModelPoint>& points,
    const std::vector<StorylandModelTexcoord>& texcoords,
    bool useRealTexcoords,
    float minX,
    float minY,
    float minZ,
    float spanX,
    float spanY,
    float spanZ
) {
    float u = 0.0f;
    float v = 0.0f;
    computeModelPreviewTexcoord(vertexIndex, points, texcoords, useRealTexcoords, minX, minY, minZ, spanX, spanY, spanZ, u, v);
    if (regionIndex >= 0 && size_t(regionIndex) < gModelTextureRegions.size()) {
        const auto& region = gModelTextureRegions[size_t(regionIndex)];
        float atlasU = region.u0 + u * (region.u1 - region.u0);
        float atlasV = region.v0 + v * (region.v1 - region.v0);
        glTexCoord2f(atlasU, atlasV);
    } else {
        glTexCoord2f(u, v);
    }
}

static void clearView() {
    clearDtzEmbeddedPreviewState();
    clearModelTexture();
    if (gTree) TreeView_DeleteAllItems(gTree);
    gTreePayloads.clear();
    setDetails(L"");
    gCurrentImage = {};
    deleteTextureBitmap();
    InvalidateRect(gPreview, nullptr, TRUE);
    gSelectedIndex = -1;
    gSelectedKind = StorylandTreeKind::None;
}

static void resetModelViewport() {
    gModelViewRotation = {};
    gModelDragStartRotation = gModelViewRotation;
    gModelDragStartPoint = {};
    gModelViewCubeDrag = false;
    gModelDistance = 3.5f;
    gModelPanX = 0.0f;
    gModelPanY = 0.0f;
}

static void applyModelViewportZoom(float scale) {
    if (scale <= 0.0f) return;
    gModelDistance *= scale;
    if (gModelDistance < 0.05f) gModelDistance = 0.05f;
    else if (gModelDistance > 240.0f) gModelDistance = 240.0f;
    if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
}

static void fitModelViewportCloser() {
    gModelDistance = 2.25f;
    gModelPanX = 0.0f;
    gModelPanY = 0.0f;
    if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
}

static const wchar_t* openGlRenderModeName(StorylandOpenGlRenderMode mode) {
    switch (mode) {
    case StorylandOpenGlRenderMode::Stories: return L"Stories shader";
    case StorylandOpenGlRenderMode::Textured: return L"Texture";
    case StorylandOpenGlRenderMode::Solid: return L"Solid";
    case StorylandOpenGlRenderMode::Wireframe: return L"Wire";
    }
    return L"OpenGL";
}

static void setOpenGlRenderMode(StorylandOpenGlRenderMode mode) {
    gOpenGlRenderMode = mode;
    if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
}

static const wchar_t* storiesSkyGameName() {
    return gStoriesSky.game() == StorylandSkyGame::Lcs ? L"LCS" : L"VCS";
}

static const wchar_t* storiesSkyWeatherName() {
    switch (gStoriesSky.weather()) {
    case StorylandSkyWeather::Sunny: return L"Sunny";
    case StorylandSkyWeather::Cloudy: return L"Cloudy";
    case StorylandSkyWeather::Rainy: return L"Rainy";
    case StorylandSkyWeather::Foggy: return L"Foggy";
    }
    return L"Weather";
}

static void stepStoriesSkyTime(float hours) {
    gStoriesSky.stepTime(hours);
    int totalMinutes = int(std::lround(gStoriesSky.time() * 60.0f)) % (24 * 60);
    if (totalMinutes < 0) totalMinutes += 24 * 60;
    std::wostringstream status;
    status << storiesSkyGameName() << L" sky  "
           << std::setw(2) << std::setfill(L'0') << (totalMinutes / 60) << L":"
           << std::setw(2) << std::setfill(L'0') << (totalMinutes % 60) << L"  "
           << storiesSkyWeatherName() << L"  |  [ previous hour, ] next hour";
    setStatus(status.str());
    if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
}

static void rotateModelViewport(float degrees, float x, float y, float z) {
    gModelViewRotation = quatMul(quatFromAxisAngle(degrees, x, y, z), gModelViewRotation);
    if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
}

static bool moveArchiveViewportKey(WPARAM key) {
    if (gMode != StorylandMode::ArchiveFile) return false;

    float panStep = std::max(0.04f, gModelDistance * 0.055f);
    float zoomIn = 0.78f;
    float zoomOut = 1.28f;

    switch (key) {
    case 'W':
        applyModelViewportZoom(zoomIn);
        setStatus(L"LVZ/IMG: moved in. W/S move in/out, A/D strafe, Q/E vertical, right-drag pan.");
        return true;
    case 'S':
        applyModelViewportZoom(zoomOut);
        setStatus(L"LVZ/IMG: moved out. W/S move in/out, A/D strafe, Q/E vertical, right-drag pan.");
        return true;
    case 'A':
        gModelPanX += panStep;
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(L"LVZ/IMG: strafe left/right with A/D.");
        return true;
    case 'D':
        gModelPanX -= panStep;
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(L"LVZ/IMG: strafe left/right with A/D.");
        return true;
    case 'Q':
        gModelPanY -= panStep;
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(L"LVZ/IMG: vertical move with Q/E.");
        return true;
    case 'E':
        gModelPanY += panStep;
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(L"LVZ/IMG: vertical move with Q/E.");
        return true;
    }

    return false;
}

static bool handleModelViewportShortcut(WPARAM key) {
    if (!currentModeUsesInteractiveModelViewport()) return false;
    if (moveArchiveViewportKey(key)) return true;

    switch (key) {
    case VK_OEM_4: // [
        stepStoriesSkyTime(-1.0f);
        return true;
    case VK_OEM_6: // ]
        stepStoriesSkyTime(1.0f);
        return true;

    case VK_OEM_PLUS:
    case VK_ADD:
    case VK_PRIOR:
        applyModelViewportZoom(0.82f);
        setStatus(L"Preview zoomed in. Keys: + / - zoom, F fit, R/0 reset, 1-4 render. LVZ/IMG: W/S/A/D/Q/E move.");
        return true;

    case VK_OEM_MINUS:
    case VK_SUBTRACT:
    case VK_NEXT:
        applyModelViewportZoom(1.22f);
        setStatus(L"Preview zoomed out. Keys: + / - zoom, F fit, R/0 reset, 1-4 render. LVZ/IMG: W/S/A/D/Q/E move.");
        return true;

    case VK_LEFT:
        rotateModelViewport(-5.0f, 0.0f, 1.0f, 0.0f);
        return true;
    case VK_RIGHT:
        rotateModelViewport(5.0f, 0.0f, 1.0f, 0.0f);
        return true;
    case VK_UP:
        rotateModelViewport(-5.0f, 1.0f, 0.0f, 0.0f);
        return true;
    case VK_DOWN:
        rotateModelViewport(5.0f, 1.0f, 0.0f, 0.0f);
        return true;

    case 'F':
        fitModelViewportCloser();
        setStatus(L"Preview fit closer. Keys: + / - zoom, F fit, R/0 reset, 1-4 render. LVZ/IMG: W/S/A/D/Q/E move.");
        return true;

    case 'R':
    case '0':
        resetModelViewport();
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(L"Preview view reset. Keys: + / - zoom, F fit, R/0 reset, 1-4 render. LVZ/IMG: W/S/A/D/Q/E move.");
        return true;

    case '1':
        setOpenGlRenderMode(StorylandOpenGlRenderMode::Stories);
        setStatus(L"OpenGL render: Stories shader.");
        return true;
    case '2':
        setOpenGlRenderMode(StorylandOpenGlRenderMode::Textured);
        setStatus(L"OpenGL render: texture.");
        return true;
    case '3':
        setOpenGlRenderMode(StorylandOpenGlRenderMode::Solid);
        setStatus(L"OpenGL render: solid.");
        return true;
    case '4':
        setOpenGlRenderMode(StorylandOpenGlRenderMode::Wireframe);
        setStatus(L"OpenGL render: wireframe.");
        return true;
    case 'G':
        gOpenGlShowGrid = !gOpenGlShowGrid;
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(gOpenGlShowGrid ? L"OpenGL grid on." : L"OpenGL grid off.");
        return true;
    case 'B':
        gOpenGlShowBones = !gOpenGlShowBones;
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(gOpenGlShowBones ? L"OpenGL bones on." : L"OpenGL bones off.");
        return true;
    case 'N':
        gOpenGlShowBounds = !gOpenGlShowBounds;
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(gOpenGlShowBounds ? L"OpenGL bounds on." : L"OpenGL bounds off.");
        return true;
    case 'C':
        gOpenGlShowViewCube = !gOpenGlShowViewCube;
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(gOpenGlShowViewCube ? L"Viewport cube on." : L"Viewport cube off.");
        return true;
    }

    return false;
}

static void* getOpenGlProcAddress(const char* name) {
    void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (proc == reinterpret_cast<void*>(0x1) || proc == reinterpret_cast<void*>(0x2) || proc == reinterpret_cast<void*>(0x3) || proc == reinterpret_cast<void*>(-1)) {
        proc = nullptr;
    }
    if (!proc) {
        HMODULE module = GetModuleHandleW(L"opengl32.dll");
        if (module) proc = reinterpret_cast<void*>(GetProcAddress(module, name));
    }
    return proc;
}

static bool loadStoriesShaderFunctions() {
    pglCreateShader = reinterpret_cast<PFNGLCREATESHADERPROC>(getOpenGlProcAddress("glCreateShader"));
    pglShaderSource = reinterpret_cast<PFNGLSHADERSOURCEPROC>(getOpenGlProcAddress("glShaderSource"));
    pglCompileShader = reinterpret_cast<PFNGLCOMPILESHADERPROC>(getOpenGlProcAddress("glCompileShader"));
    pglGetShaderiv = reinterpret_cast<PFNGLGETSHADERIVPROC>(getOpenGlProcAddress("glGetShaderiv"));
    pglGetShaderInfoLog = reinterpret_cast<PFNGLGETSHADERINFOLOGPROC>(getOpenGlProcAddress("glGetShaderInfoLog"));
    pglDeleteShader = reinterpret_cast<PFNGLDELETESHADERPROC>(getOpenGlProcAddress("glDeleteShader"));
    pglCreateProgram = reinterpret_cast<PFNGLCREATEPROGRAMPROC>(getOpenGlProcAddress("glCreateProgram"));
    pglAttachShader = reinterpret_cast<PFNGLATTACHSHADERPROC>(getOpenGlProcAddress("glAttachShader"));
    pglLinkProgram = reinterpret_cast<PFNGLLINKPROGRAMPROC>(getOpenGlProcAddress("glLinkProgram"));
    pglGetProgramiv = reinterpret_cast<PFNGLGETPROGRAMIVPROC>(getOpenGlProcAddress("glGetProgramiv"));
    pglGetProgramInfoLog = reinterpret_cast<PFNGLGETPROGRAMINFOLOGPROC>(getOpenGlProcAddress("glGetProgramInfoLog"));
    pglDeleteProgram = reinterpret_cast<PFNGLDELETEPROGRAMPROC>(getOpenGlProcAddress("glDeleteProgram"));
    pglUseProgram = reinterpret_cast<PFNGLUSEPROGRAMPROC>(getOpenGlProcAddress("glUseProgram"));
    pglGetUniformLocation = reinterpret_cast<PFNGLGETUNIFORMLOCATIONPROC>(getOpenGlProcAddress("glGetUniformLocation"));
    pglUniform1i = reinterpret_cast<PFNGLUNIFORM1IPROC>(getOpenGlProcAddress("glUniform1i"));
    pglUniform1f = reinterpret_cast<PFNGLUNIFORM1FPROC>(getOpenGlProcAddress("glUniform1f"));
    pglUniform3f = reinterpret_cast<PFNGLUNIFORM3FPROC>(getOpenGlProcAddress("glUniform3f"));
    pglActiveTexture = reinterpret_cast<PFNGLACTIVETEXTUREPROC>(getOpenGlProcAddress("glActiveTexture"));

    return pglCreateShader && pglShaderSource && pglCompileShader && pglGetShaderiv && pglGetShaderInfoLog &&
           pglDeleteShader && pglCreateProgram && pglAttachShader && pglLinkProgram && pglGetProgramiv &&
           pglGetProgramInfoLog && pglDeleteProgram && pglUseProgram && pglGetUniformLocation && pglUniform1i &&
           pglUniform1f && pglUniform3f;
}

static const char* storylandStoriesVertexShaderSource() {
    return
        "#version 120\n"
        "varying vec4 vColor;\n"
        "varying vec2 vTex0;\n"
        "varying vec3 vLighting;\n"
        "varying float vSpecular;\n"
        "varying float vFog;\n"
        "uniform vec3 uAmbientColor;\n"
        "uniform vec3 uDirectionalColor;\n"
        "uniform vec3 uSunDirection;\n"
        "uniform float uFogStart;\n"
        "uniform float uFarClip;\n"
        "void main(){\n"
        "  vec4 eyePos = gl_ModelViewMatrix * gl_Vertex;\n"
        "  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        "  vColor = gl_Color;\n"
        "  vTex0 = gl_MultiTexCoord0.xy;\n"
        "  vec3 n = normalize(gl_NormalMatrix * gl_Normal);\n"
        "  vec3 lightDir = normalize(gl_NormalMatrix * uSunDirection);\n"
        "  float direct = max(dot(n, lightDir), 0.0);\n"
        "  float bounce = max(dot(n, -lightDir), 0.0) * 0.22;\n"
        "  vLighting = clamp(uAmbientColor + uDirectionalColor * (direct + bounce), 0.0, 1.35);\n"
        "  vec3 viewDir = normalize(-eyePos.xyz);\n"
        "  vSpecular = pow(max(dot(reflect(-lightDir, n), viewDir), 0.0), 18.0) * 0.12;\n"
        "  float distanceFromCamera = length(eyePos.xyz);\n"
        "  float fogRange = max(uFarClip - uFogStart, 0.001);\n"
        "  vFog = clamp((uFarClip - distanceFromCamera) / fogRange, 0.0, 1.0);\n"
        "}\n";
}

static const char* storylandStoriesFragmentShaderSource() {
    return
        "#version 120\n"
        "uniform sampler2D tex0;\n"
        "uniform int uUseTexture;\n"
        "uniform int uRenderMode;\n"
        "uniform vec3 uFogColor;\n"
        "uniform vec3 uDirectionalColor;\n"
        "varying vec4 vColor;\n"
        "varying vec2 vTex0;\n"
        "varying vec3 vLighting;\n"
        "varying float vSpecular;\n"
        "varying float vFog;\n"
        "void main(){\n"
        "  vec4 base = vec4(vColor.rgb, 1.0);\n"
        "  if(uUseTexture != 0){ vec4 tex = texture2D(tex0, vTex0); base.rgb *= tex.rgb; }\n"
        "  vec3 lit = base.rgb;\n"
        "  if(uRenderMode == 0 || uRenderMode == 2){\n"
        "    lit = base.rgb * vLighting + vSpecular * uDirectionalColor;\n"
        "  }\n"
        "  lit = mix(uFogColor, clamp(lit, 0.0, 1.0), vFog);\n"
        "  gl_FragColor = vec4(lit, 1.0);\n"
        "}\n";
}

static GLuint compileStoriesShader(GLenum type, const char* source, std::string& error) {
    GLuint shader = pglCreateShader(type);
    if (shader == 0) {
        error = "glCreateShader failed";
        return 0;
    }

    const GLchar* sources[] = { source };
    pglShaderSource(shader, 1, sources, nullptr);
    pglCompileShader(shader);

    GLint ok = 0;
    pglGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint length = 0;
        pglGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(size_t(std::max(1, length)), '\0');
        GLsizei written = 0;
        pglGetShaderInfoLog(shader, GLsizei(log.size()), &written, log.data());
        if (written >= 0 && size_t(written) < log.size()) log.resize(size_t(written));
        error = log.empty() ? "shader compile failed" : log;
        pglDeleteShader(shader);
        return 0;
    }

    return shader;
}

static bool buildStoriesShaderProgram() {
    if (gStoriesShaderTried) return gStoriesShaderReady;
    gStoriesShaderTried = true;
    gStoriesShaderStatus.clear();

    if (!loadStoriesShaderFunctions()) {
        gStoriesShaderStatus = "OpenGL shader functions unavailable; fixed pipeline fallback active";
        return false;
    }

    std::string error;
    GLuint vertexShader = compileStoriesShader(GL_VERTEX_SHADER, storylandStoriesVertexShaderSource(), error);
    if (vertexShader == 0) {
        gStoriesShaderStatus = "vertex shader: " + error;
        return false;
    }

    GLuint fragmentShader = compileStoriesShader(GL_FRAGMENT_SHADER, storylandStoriesFragmentShaderSource(), error);
    if (fragmentShader == 0) {
        pglDeleteShader(vertexShader);
        gStoriesShaderStatus = "fragment shader: " + error;
        return false;
    }

    GLuint program = pglCreateProgram();
    pglAttachShader(program, vertexShader);
    pglAttachShader(program, fragmentShader);
    pglLinkProgram(program);
    pglDeleteShader(vertexShader);
    pglDeleteShader(fragmentShader);

    GLint linked = 0;
    pglGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length = 0;
        pglGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(size_t(std::max(1, length)), '\0');
        GLsizei written = 0;
        pglGetProgramInfoLog(program, GLsizei(log.size()), &written, log.data());
        if (written >= 0 && size_t(written) < log.size()) log.resize(size_t(written));
        pglDeleteProgram(program);
        gStoriesShaderStatus = log.empty() ? "shader link failed" : log;
        return false;
    }

    gStoriesShaderProgram = program;
    gStoriesShaderReady = true;
    gStoriesShaderStatus = "Stories GLSL shader active";
    return true;
}

static void stopStoriesShaderProgram() {
    if (pglUseProgram) pglUseProgram(0);
}

static bool beginStoriesShaderProgram(bool useTexture) {
    if (!buildStoriesShaderProgram()) return false;
    if (!pglUseProgram || gStoriesShaderProgram == 0) return false;

    pglUseProgram(gStoriesShaderProgram);
    GLint loc = pglGetUniformLocation(gStoriesShaderProgram, "tex0");
    if (loc >= 0) pglUniform1i(loc, 0);
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uUseTexture");
    if (loc >= 0) pglUniform1i(loc, useTexture ? 1 : 0);
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uRenderMode");
    if (loc >= 0) {
        int mode = 0;
        if (gOpenGlRenderMode == StorylandOpenGlRenderMode::Textured) mode = 1;
        else if (gOpenGlRenderMode == StorylandOpenGlRenderMode::Solid) mode = 2;
        else if (gOpenGlRenderMode == StorylandOpenGlRenderMode::Wireframe) mode = 3;
        pglUniform1i(loc, mode);
    }
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uFogColor");
    if (loc >= 0) {
        const StorylandSkyColor& fog = gStoriesSky.state().fog;
        pglUniform3f(loc, fog.r, fog.g, fog.b);
    }
    const StorylandSkyState& sky = gStoriesSky.state();
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uAmbientColor");
    if (loc >= 0) pglUniform3f(loc, sky.ambient.r, sky.ambient.g, sky.ambient.b);
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uDirectionalColor");
    if (loc >= 0) pglUniform3f(loc, sky.directional.r, sky.directional.g, sky.directional.b);
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uSunDirection");
    if (loc >= 0) pglUniform3f(loc, sky.sunDirectionX, sky.sunDirectionY, sky.sunDirectionZ);
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uFogStart");
    if (loc >= 0) pglUniform1f(loc, sky.previewFogStart);
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uFarClip");
    if (loc >= 0) pglUniform1f(loc, sky.previewFarClip);
    return true;
}

static void setupFixedPipelineStoriesLighting(bool lit) {
    if (!lit) {
        glDisable(GL_LIGHTING);
        glDisable(GL_COLOR_MATERIAL);
        return;
    }

    glEnable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    const StorylandSkyState& sky = gStoriesSky.state();
    GLfloat ambientModel[] = {sky.ambient.r, sky.ambient.g, sky.ambient.b, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientModel);
    GLfloat light0Pos[] = {sky.sunDirectionX, sky.sunDirectionY, sky.sunDirectionZ, 0.0f};
    GLfloat light0Diffuse[] = {sky.directional.r, sky.directional.g, sky.directional.b, 1.0f};
    GLfloat light0Spec[] = {sky.directional.r * 0.18f, sky.directional.g * 0.18f, sky.directional.b * 0.18f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0Spec);
    GLfloat light1Pos[] = {-sky.sunDirectionX, -sky.sunDirectionY, 0.35f, 0.0f};
    GLfloat light1Diffuse[] = {sky.ambient.r * 0.45f, sky.ambient.g * 0.45f, sky.ambient.b * 0.45f, 1.0f};
    GLfloat light1Spec[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, light1Pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1Diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1Spec);
    GLfloat materialSpec[] = {0.18f, 0.18f, 0.16f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 18.0f);
}

static bool initializeOpenGlPreview(HWND hwnd) {
    if (gOpenGlReady) return true;

    HDC dc = GetDC(hwnd);
    if (!dc) return false;

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(dc, &pfd);
    if (pixelFormat == 0) {
        ReleaseDC(hwnd, dc);
        return false;
    }

    if (!SetPixelFormat(dc, pixelFormat, &pfd)) {
        ReleaseDC(hwnd, dc);
        return false;
    }

    gOpenGlContext = wglCreateContext(dc);
    if (!gOpenGlContext) {
        ReleaseDC(hwnd, dc);
        return false;
    }

    if (!wglMakeCurrent(dc, gOpenGlContext)) {
        wglDeleteContext(gOpenGlContext);
        gOpenGlContext = nullptr;
        ReleaseDC(hwnd, dc);
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glClearColor(0.115f, 0.120f, 0.128f, 1.0f);

    buildStoriesShaderProgram();

    wglMakeCurrent(nullptr, nullptr);
    ReleaseDC(hwnd, dc);
    gOpenGlReady = true;
    return true;
}

static void destroyOpenGlPreview() {
    clearModelTexture();
    if (gOpenGlContext) {
        HDC dc = gPreview ? GetDC(gPreview) : nullptr;
        if (dc) wglMakeCurrent(dc, gOpenGlContext);
        if (gStoriesShaderProgram != 0 && pglDeleteProgram) {
            pglDeleteProgram(gStoriesShaderProgram);
        }
        gStoriesShaderProgram = 0;
        gStoriesShaderReady = false;
        gStoriesShaderTried = false;
        if (dc) {
            wglMakeCurrent(nullptr, nullptr);
            ReleaseDC(gPreview, dc);
        } else {
            wglMakeCurrent(nullptr, nullptr);
        }
        wglDeleteContext(gOpenGlContext);
        gOpenGlContext = nullptr;
    }
    gOpenGlReady = false;
}

static void setPerspectiveProjection(double fovDegrees, double aspect, double zNear, double zFar) {
    const double pi = 3.14159265358979323846;
    double f = 1.0 / std::tan((fovDegrees * pi / 180.0) * 0.5);

    double matrix[16] = {};
    matrix[0] = f / aspect;
    matrix[5] = f;
    matrix[10] = (zFar + zNear) / (zNear - zFar);
    matrix[11] = -1.0;
    matrix[14] = (2.0 * zFar * zNear) / (zNear - zFar);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(matrix);
}

static void setStoriesViewportClearColor() {
    const StorylandSkyColor& lower = gStoriesSky.state().lower;
    if (gOpenGlRenderMode == StorylandOpenGlRenderMode::Stories && gStoriesSky.isEnabled()) {
        glClearColor(lower.r, lower.g, lower.b, 1.0f);
    } else {
        glClearColor(0.115f, 0.120f, 0.128f, 1.0f);
    }
}

static void drawStoriesViewportSky(float radius) {
    if (gOpenGlRenderMode != StorylandOpenGlRenderMode::Stories || !gStoriesSky.isEnabled()) return;
    stopStoriesShaderProgram();
    gStoriesSky.drawBackground(skyRotationFromModelRotation(gModelViewRotation), radius);
}

static void drawOpenGlTextOverlayFallback(HDC dc, const RECT& rc, const std::wstring& text) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(230, 230, 230));
    TextOutW(dc, rc.left + 8, rc.top + 8, text.c_str(), int(text.size()));
}


static void populateTextureList() {
    clearView();
    const auto& textures = gTextureArchive.textures();
    HTREEITEM root = addTreeItem(TVI_ROOT, L"Texture Archive / CHK-XTX-TEX");
    for (size_t i = 0; i < textures.size(); ++i) {
        const LeedsTextureEntry& e = textures[i];
        std::wstringstream line;
        line << i << L"  " << widen(e.name) << L"  " << e.width << L"x" << e.height << L"  " << int(e.bpp) << L"bpp";
        HTREEITEM item = addTreeItem(root, line.str(), StorylandTreeKind::Texture, int(i));
        std::wstringstream meta;
        meta << L"header " << hexWide(e.textureHeaderOffset, 6) << L" / raster " << hexWide(e.rasterOffset, 6) << L" / block " << e.blockSize << L" bytes";
        addTreeItem(item, meta.str());
        if (e.kind == TextureKind::Ps2 && e.bpp == 4 && (e.swizzleMask & 1)) {
            addTreeItem(item, L"decode note: PS2 4bpp uses packed-byte unswizzle before nibble expansion");
        }
    }
    expandTreeItem(root);
    setStatus(std::to_wstring(textures.size()) + L" textures loaded");
}


static uint32_t readCurrentDtzU32(size_t offset) {
    const std::vector<uint8_t>& bytes = gDtzArchive.unpackedBytes();
    if (offset + 4 > bytes.size()) return 0;
    return uint32_t(bytes[offset]) |
           (uint32_t(bytes[offset + 1]) << 8) |
           (uint32_t(bytes[offset + 2]) << 16) |
           (uint32_t(bytes[offset + 3]) << 24);
}

static uint16_t readCurrentDtzU16(size_t offset) {
    const std::vector<uint8_t>& bytes = gDtzArchive.unpackedBytes();
    if (offset + 2 > bytes.size()) return 0;
    return uint16_t(bytes[offset] | (uint16_t(bytes[offset + 1]) << 8));
}

static std::wstring readCurrentDtzFourCc() {
    const std::vector<uint8_t>& bytes = gDtzArchive.unpackedBytes();
    if (bytes.size() < 4) return L"????";
    std::wstring value;
    value.push_back(wchar_t(bytes[0]));
    value.push_back(wchar_t(bytes[1]));
    value.push_back(wchar_t(bytes[2]));
    value.push_back(wchar_t(bytes[3]));
    return value;
}

static bool isCurrentDtzOffsetValid(uint32_t offset) {
    const std::vector<uint8_t>& bytes = gDtzArchive.unpackedBytes();
    return offset != 0 && offset < bytes.size();
}

static std::wstring describeCurrentDtzPointer(uint32_t offset) {
    if (offset == 0) return L"null";
    std::wstringstream ss;
    ss << hexWide(offset, 6);
    if (isCurrentDtzOffsetValid(offset)) ss << L" valid";
    else ss << L" outside raw DTZ";
    return ss.str();
}

static void appendDtzPointerOverviewLine(
    std::wstringstream& ss,
    const wchar_t* name,
    uint32_t fieldOffset,
    uint32_t pointerValue,
    const wchar_t* analogue,
    const wchar_t* note,
    uint32_t countValue = 0,
    uint32_t rowSize = 0
) {
    ss << L"  " << hexWide(fieldOffset, 4) << L"  "
       << name << L": " << describeCurrentDtzPointer(pointerValue);

    if (countValue != 0) {
        ss << L"  count=" << countValue;
    }

    if (rowSize != 0) {
        ss << L"  row=" << rowSize << L" bytes";
        if (countValue != 0) {
            uint64_t byteCount = uint64_t(countValue) * uint64_t(rowSize);
            ss << L"  span=" << byteCount << L" bytes";
            if (pointerValue != 0) {
                ss << L"  end=" << hexWide(uint32_t(pointerValue + uint32_t(byteCount)), 6);
            }
        }
    }

    if (analogue && analogue[0]) {
        ss << L"  analogue=" << analogue;
    }

    if (note && note[0]) {
        ss << L"  note=" << note;
    }

    ss << L"\r\n";
}

static void appendDtzScalarOverviewLine(
    std::wstringstream& ss,
    const wchar_t* name,
    uint32_t fieldOffset,
    uint32_t value,
    const wchar_t* note
) {
    ss << L"  " << hexWide(fieldOffset, 4) << L"  "
       << name << L": " << hexWide(value) << L" / " << value;
    if (note && note[0]) ss << L"  note=" << note;
    ss << L"\r\n";
}

static void appendDtzSectorCandidateOverview(std::wstringstream& ss) {
    const auto& records = gDtzArchive.sectorRecords();
    const auto& dirEntries = gDtzArchive.dirEntries();

    size_t namedRecords = 0;
    size_t plrCandidates = 0;
    size_t matchedDirEntries = 0;

    for (const auto& record : records) {
        if (!record.resourceName.empty()) namedRecords++;
        if (record.startSector == 23) plrCandidates++;
    }

    for (const auto& entry : dirEntries) {
        if (!entry.matchingRecordIndices.empty()) matchedDirEntries++;
    }

    ss << L"Streaming / gta3PS2.img runtime map:\r\n";
    ss << L"  Internal GAME.DTZ streaming entries shown: " << dirEntries.size() << L"\r\n";
    ss << L"  Internal entries with one or more GAME.DTZ records: " << matchedDirEntries << L"\r\n";
    ss << L"  Raw GAME.DTZ sector-like records found: " << records.size() << L"\r\n";
    ss << L"  Named/known records: " << namedRecords << L"\r\n";
    ss << L"  PLR.mdl start=23 candidates: " << plrCandidates << L"\r\n";

    if (!gDtzArchive.companionDirPath().empty()) {
        ss << L"  Optional beta-build .dir loaded: " << gDtzArchive.companionDirPath() << L"\r\n";
    } else {
        ss << L"  Beta-build .dir: none loaded (normal for retail LCS/VCS). GAME.DTZ supplies the retail map.\r\n";
    }

    size_t shown = 0;
    for (const auto& record : records) {
        if (record.startSector != 23 && record.resourceName != "plr.mdl") continue;
        ss << L"    PLR candidate at DTZ record " << hexWide(record.recordOffset, 6)
           << L" start=" << record.startSector
           << L" count=" << record.sectorCount
           << L" end=" << (record.startSector + record.sectorCount)
           << L" bytes=" << uint64_t(record.sectorCount) * 2048ull
           << L"\r\n";
        shown++;
    }
    if (shown == 0) {
        ss << L"    No start=23 PLR.mdl candidate was found in the current raw-sector scan.\r\n";
    }

    ss << L"\r\n";
}

static void selectDtzOverview() {
    gSelectedIndex = 0;
    gSelectedKind = StorylandTreeKind::DtzOverview;

    const uint32_t relocationTable = readCurrentDtzU32(0x10);
    const uint32_t relocationCount = readCurrentDtzU32(0x14);
    const uint32_t vmtHashTable = readCurrentDtzU32(0x18);
    const uint16_t vmtHashCountA = readCurrentDtzU16(0x1C);
    const uint16_t vmtHashCountB = readCurrentDtzU16(0x1E);
    const uint32_t ideCount = readCurrentDtzU32(0x38);
    const uint32_t twoDfxCount = readCurrentDtzU32(0x54);
    const uint32_t cullCount = readCurrentDtzU32(0x94);

    std::wstringstream ss;
    ss << L"GAME.DTZ detailed overview\r\n\r\n";
    ss << L"Input path: " << gDtzArchive.sourcePath() << L"\r\n";
    ss << L"Input form: " << (gDtzArchive.wasCompressedInput() ? L"zlib/deflate packed file, inflated for inspection" : L"raw/unpacked GTAG/GATG block") << L"\r\n";
    ss << L"Raw signature: " << readCurrentDtzFourCc() << L"\r\n";
    ss << L"Raw byte size: " << gDtzArchive.unpackedBytes().size() << L" bytes\r\n";
    if (gDtzArchive.hasCompanionImg()) {
        ss << L"Loaded companion IMG: " << gDtzArchive.companionImgPath() << L"\r\n";
        ss << L"Loaded companion IMG size: " << gDtzArchive.companionImgSize() << L" bytes / " << ((gDtzArchive.companionImgSize() + 2047ull) / 2048ull) << L" logical sectors\r\n";
    } else {
        ss << L"Loaded companion IMG: none; load gta3PS2.img/gta3PSP.img to make sector-count edits resize the physical IMG too.\r\n";
    }
    ss << L"Header-declared DTZ size: " << readCurrentDtzU32(0x08) << L" bytes\r\n";
    ss << L"Header-declared data-before-relocation-table size: " << readCurrentDtzU32(0x0C) << L" bytes\r\n\r\n";

    ss << L"Core relocation / method tables:\r\n";
    appendDtzScalarOverviewLine(ss, L"VersionOrUnknown", 0x04, readCurrentDtzU32(0x04), L"usually 1");
    appendDtzPointerOverviewLine(ss, L"RelocationTable", 0x10, relocationTable, L"pointer relocation offsets", L"global pointer fixup list", relocationCount, 4);
    appendDtzPointerOverviewLine(ss, L"VmtHashOffsetTable", 0x18, vmtHashTable, L"Jenkins hash -> VMT/method replacement table", L"0x1C stores two u16 section counts");
    ss << L"  0x001C  VMT/Jenkins hash counts: sectionA=" << vmtHashCountA << L"  sectionB=" << vmtHashCountB << L"\r\n\r\n";

    appendDtzSectorCandidateOverview(ss);

    const auto& scannedBlocks = gDtzArchive.dataBlocks();
    const auto& scannedFields = gDtzArchive.dataFields();
    size_t editableScannedFields = 0;
    for (const auto& field : scannedFields) {
        if (field.editable) editableScannedFields++;
    }
    ss << L"Real DTZ data scan / editable DAT analogues:\r\n";
    ss << L"  Scanned header/data blocks: " << scannedBlocks.size() << L"\r\n";
    ss << L"  Structured fields decoded from actual bytes: " << scannedFields.size() << L"\r\n";
    ss << L"  Editable GAME.DTZ data fields: " << editableScannedFields << L"\r\n";
    ss << L"  Open the tree node 'Real scanned DTZ data blocks / editable DAT analogues' to inspect object.dat, carcols palette, cull.ipl, STAT_* pedstats, particle-style rows, and generic real header ranges.\r\n\r\n";

    ss << L"World placement / map structures:\r\n";
    appendDtzPointerOverviewLine(ss, L"gpThePaths", 0x20, readCurrentDtzU32(0x20), L"paths", L"path data");
    appendDtzPointerOverviewLine(ss, L"BuildingPoolIPL", 0x24, readCurrentDtzU32(0x24), L"Buildings IPL", L"static object placement");
    appendDtzPointerOverviewLine(ss, L"TreadableIPL", 0x28, readCurrentDtzU32(0x28), L"Treadable IPL", L"roads/treadables");
    appendDtzPointerOverviewLine(ss, L"DummyIPL", 0x2C, readCurrentDtzU32(0x2C), L"Dummy IPL", L"dynamic dummy objects");
    appendDtzPointerOverviewLine(ss, L"EntryInfoNodes", 0x30, readCurrentDtzU32(0x30), L"streaming dynamic object nodes", L"EntryInfoNodes");
    appendDtzPointerOverviewLine(ss, L"PtrNodes", 0x34, readCurrentDtzU32(0x34), L"streaming map PtrNodes", L"points into IPL-backed nodes");
    appendDtzPointerOverviewLine(ss, L"ZoneData", 0x48, readCurrentDtzU32(0x48), L"info.zon / navig.zon", L"zone data");
    appendDtzPointerOverviewLine(ss, L"CWorld::ms_aSectors", 0x4C, readCurrentDtzU32(0x4C), L"world sector array", L"references PtrNodes");
    appendDtzPointerOverviewLine(ss, L"CWorld::ms_bigBuildingsList", 0x50, readCurrentDtzU32(0x50), L"big buildings list", L"4 PtrNode pointers");
    appendDtzPointerOverviewLine(ss, L"cull.ipl table", 0x98, readCurrentDtzU32(0x98), L"cull.ipl", L"16-byte rows when count is valid", cullCount, 16);
    ss << L"\r\n";

    ss << L"Model definitions / streaming / archive hooks:\r\n";
    appendDtzPointerOverviewLine(ss, L"IDE pointer table", 0x3C, readCurrentDtzU32(0x3C), L"binary IDE table", L"model IDs are assigned by pointer index; null pointer means free ID", ideCount, 4);
    appendDtzPointerOverviewLine(ss, L"VehicleClassTable", 0x40, readCurrentDtzU32(0x40), L"car class table", L"11 classes, usually 25 IDE ids per class");
    appendDtzScalarOverviewLine(ss, L"VehicleClassItemCount", 0x44, readCurrentDtzU32(0x44), L"count per vehicle class row");
    appendDtzPointerOverviewLine(ss, L"2dfx table", 0x58, readCurrentDtzU32(0x58), L"2dfx", L"64-byte effect rows", twoDfxCount, 64);
    appendDtzPointerOverviewLine(ss, L"HardcodedModelIndexArray", 0x5C, readCurrentDtzU32(0x5C), L"hardcoded model indices", L"model index lookup array");
    appendDtzPointerOverviewLine(ss, L"CStreamingInfo::ms_aInfoForModel", 0x7C, readCurrentDtzU32(0x7C), L"GTA3PS*.DIR analogue", L"critical gta3PS2.img sector metadata used at runtime");
    appendDtzPointerOverviewLine(ss, L"EmbeddedMdlClumpPointerTable", 0xC0, readCurrentDtzU32(0xC0), L"embedded MDL clumps", L"offsets to clumps for DTZ-embedded models");
    appendDtzPointerOverviewLine(ss, L"CUTS.DIR", 0xC4, readCurrentDtzU32(0xC4), L"CUTS.DIR analogue", L"cutscene directory data");
    ss << L"\r\n";

    ss << L"Texture / collision / animation loader blocks:\r\n";
    appendDtzPointerOverviewLine(ss, L"TexList", 0x60, readCurrentDtzU32(0x60), L"CHK/TexList loader block", L"texture archive loading table");
    appendDtzPointerOverviewLine(ss, L"COL2 loader block", 0x68, readCurrentDtzU32(0x68), L"COL2 loader", L"collision archive loader block");
    appendDtzPointerOverviewLine(ss, L"DummyCollision", 0x70, readCurrentDtzU32(0x70), L"collision dummy", L"fallback/dummy collision");
    appendDtzPointerOverviewLine(ss, L"AnimLoaderBlock", 0x80, readCurrentDtzU32(0x80), L"ANIM / ped.ifp", L"animation loader block");
    appendDtzPointerOverviewLine(ss, L"menu.chk", 0xD4, readCurrentDtzU32(0xD4), L"frontend.txd/menu.chk analogue", L"compressed deflate/zlib texture archive when present");
    appendDtzScalarOverviewLine(ss, L"fonts.chk real size", 0xD8, readCurrentDtzU32(0xD8), L"real/unpacked size field on documented builds");
    appendDtzPointerOverviewLine(ss, L"fonts.chk", 0xDC, readCurrentDtzU32(0xDC), L"fonts.txd/fonts.chk analogue", L"compressed deflate/zlib texture archive when present");
    ss << L"\r\n";

    ss << L"Gameplay data analogues:\r\n";
    appendDtzPointerOverviewLine(ss, L"object.dat", 0x74, readCurrentDtzU32(0x74), L"object.dat", L"documented as 117 rows of 32 bytes", 117, 32);
    appendDtzPointerOverviewLine(ss, L"carcols palette", 0x78, readCurrentDtzU32(0x78), L"carcols.dat RGBA palette", L"256 RGBA colors", 256, 4);
    appendDtzPointerOverviewLine(ss, L"fistfite.dat", 0x84, readCurrentDtzU32(0x84), L"fistfite.dat", L"melee/fistfight data");
    appendDtzPointerOverviewLine(ss, L"PedAnimInfo", 0x88, readCurrentDtzU32(0x88), L"PedAnimInfo[]", L"ped animation info table");
    appendDtzPointerOverviewLine(ss, L"ped.dat", 0x8C, readCurrentDtzU32(0x8C), L"ped.dat", L"pedestrian behavior data");
    appendDtzPointerOverviewLine(ss, L"pedstats.dat", 0x90, readCurrentDtzU32(0x90), L"pedstats.dat", L"pedestrian stat records");
    appendDtzPointerOverviewLine(ss, L"waterpro.dat", 0xA4, readCurrentDtzU32(0xA4), L"waterpro.dat", L"water surface data");
    appendDtzPointerOverviewLine(ss, L"HANDLING.CFG", 0xA8, readCurrentDtzU32(0xA8), L"handling.dat/HANDLING.CFG", L"vehicle handling data");
    appendDtzPointerOverviewLine(ss, L"surface.dat", 0xAC, readCurrentDtzU32(0xAC), L"surface.dat", L"surface material data");
    appendDtzPointerOverviewLine(ss, L"timecyc.dat", 0xB0, readCurrentDtzU32(0xB0), L"timecyc.dat", L"weather/timecycle data");
    appendDtzPointerOverviewLine(ss, L"pedgrp.dat", 0xB4, readCurrentDtzU32(0xB4), L"pedgrp.dat", L"ped group data");
    appendDtzPointerOverviewLine(ss, L"particle.cfg", 0xB8, readCurrentDtzU32(0xB8), L"particle.cfg", L"particle effect configuration");
    appendDtzPointerOverviewLine(ss, L"weapon.dat", 0xBC, readCurrentDtzU32(0xBC), L"weapon.dat", L"weapon data");
    appendDtzPointerOverviewLine(ss, L"ferry.dat", 0xC8, readCurrentDtzU32(0xC8), L"ferry.dat", L"ferry path data");
    appendDtzPointerOverviewLine(ss, L"tracks.dat/tracks2.dat", 0xCC, readCurrentDtzU32(0xCC), L"tracks.dat + tracks2.dat", L"track path data");
    appendDtzPointerOverviewLine(ss, L"flight.dat", 0xD0, readCurrentDtzU32(0xD0), L"flight.dat", L"flight path data");
    ss << L"\r\n";

    ss << L"Notes:\r\n"
       << L"  - Offsets shown here are raw/unpacked GAME.DTZ offsets.\r\n"
       << L"  - A nonzero pointer outside the raw byte size is suspicious for this file/version.\r\n"
       << L"  - gta3PS2.img sector sizes are 2048-byte logical archive sectors, not PCSX2 raw DVD 2064-byte blocks.\r\n"
       << L"  - Retail LCS/VCS use GAME.DTZ for the runtime map; optional .dir support is only for beta-build archives.\r\n"
       << L"  - When a companion IMG is loaded, sector-count patches also insert/delete 2048-byte sector spans in the IMG when shifting is enabled.\r\n";

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}


static void populateDtzList() {
    clearView();
    std::wstring rootTitle = L"GAME.DTZ";
    if (gDtzArchive.hasCompanionImg()) rootTitle += L" + gta3PS*.img";
    HTREEITEM root = addTreeItem(TVI_ROOT, rootTitle);

    if (gDtzArchive.hasCompanionImg()) {
        std::wstringstream imgLine;
        imgLine << L"Loaded companion IMG: " << gDtzArchive.companionImgPath()
                << L"  size=" << gDtzArchive.companionImgSize()
                << L" bytes  sectors=" << ((gDtzArchive.companionImgSize() + 2047ull) / 2048ull);
        addTreeItem(root, imgLine.str());
    } else {
        addTreeItem(root, L"No companion IMG loaded; sector patches only affect GAME.DTZ until gta3PS2.img/gta3PSP.img is loaded.");
    }

    HTREEITEM overviewRoot = addTreeItem(root, L"GTAG header overview", StorylandTreeKind::DtzOverview, 0);
    addTreeItem(overviewRoot, L"Core header: signature, size, relocation table, VMT/Jenkins hash tables");
    addTreeItem(overviewRoot, L"gta3PS2.img map: internal GAME.DTZ records (retail); optional .dir is beta-build only");
    addTreeItem(overviewRoot, L"World/IPL/IDE/streaming pointers: paths, IPL pools, IDE table, CStreamingInfo");
    addTreeItem(overviewRoot, L"Data blocks below are decoded from the file, not placeholder labels");

    HTREEITEM dataRoot = addTreeItem(root, L"DAT-style blocks from GAME.DTZ / editable fields");
    const auto& dataBlocks = gDtzArchive.dataBlocks();
    const auto& dataFields = gDtzArchive.dataFields();
    if (dataBlocks.empty()) {
        addTreeItem(dataRoot, L"No DAT analogue blocks were decoded from this GAME.DTZ yet.");
    }
    for (size_t blockIndex = 0; blockIndex < dataBlocks.size(); ++blockIndex) {
        const auto& block = dataBlocks[blockIndex];
        std::wstringstream line;
        line << hexWide(block.offset, 6)
             << L".." << hexWide(block.inferredEnd, 6)
             << L"  " << widen(block.name)
             << L"  size=" << block.size;
        if (block.rowSize != 0) line << L"  rowSize=" << block.rowSize;
        if (block.rowCount != 0) line << L"  rows=" << block.rowCount;
        if (block.editable) line << L"  EDITABLE";
        HTREEITEM blockItem = addTreeItem(dataRoot, line.str(), StorylandTreeKind::DtzDataBlock, int(blockIndex));

        uint32_t previousRow = 0xFFFFFFFFu;
        HTREEITEM rowItem = nullptr;
        size_t fieldsShownForBlock = 0;
        for (size_t fieldIndex = 0; fieldIndex < dataFields.size(); ++fieldIndex) {
            const auto& field = dataFields[fieldIndex];
            if (field.blockIndex != blockIndex) continue;
            if (field.rowIndex != previousRow) {
                previousRow = field.rowIndex;
                rowItem = addTreeItem(blockItem, widen(field.rowLabel));
            }
            std::wstringstream fieldLine;
            fieldLine << hexWide(field.absoluteOffset, 6)
                      << L"  " << widen(field.name)
                      << L"  " << widen(field.type)
                      << L" = " << widen(field.valueText);
            if (field.editable) fieldLine << L"  *";
            addTreeItem(rowItem ? rowItem : blockItem, fieldLine.str(), StorylandTreeKind::DtzDataField, int(fieldIndex));
            fieldsShownForBlock++;
        }
        if (fieldsShownForBlock == 0) {
            addTreeItem(blockItem, L"Raw scanned range only; no structured editable fields named yet.");
        }

        std::string lowerName = block.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (lowerName.find("timecyc") != std::string::npos || lowerName.find("weapon") != std::string::npos) {
            expandTreeItem(blockItem);
        }
    }

    const auto& dirEntries = gDtzArchive.dirEntries();
    if (!dirEntries.empty()) {
        HTREEITEM previewRoot = addTreeItem(root, L"Previewable resources from loaded IMG (MDL/DFF + XTX/CHK/TEX)");
        HTREEITEM modelRoot = addTreeItem(previewRoot, L"Models shown through the OpenGL MDL viewer");
        HTREEITEM textureRoot = addTreeItem(previewRoot, L"Texture archives shown through the texture viewer");
        size_t modelShown = 0;
        size_t textureShown = 0;
        for (size_t i = 0; i < dirEntries.size(); ++i) {
            const auto& entry = dirEntries[i];
            std::wstring entryName = widen(entry.name);
            std::wstring resourceName = canonicalDtzImgResourceName(entryName);
            std::wstring ext = getExtensionLower(resourceName);
            if (ext == L".mdl" || ext == L".dff" || ext == L".xtx" || ext == L".chk" || ext == L".tex") {
                std::wstringstream line;
                line << entryName
                     << L"  start=" << entry.startSector
                     << L"  count=" << entry.sectorCount
                     << L"  budget=" << (uint64_t(entry.sectorCount) * 2048ull) << L" bytes";
                if (ext == L".mdl" || ext == L".dff") {
                    addTreeItem(modelRoot, line.str(), StorylandTreeKind::DtzDirEntry, int(i));
                    modelShown++;
                } else {
                    addTreeItem(textureRoot, line.str(), StorylandTreeKind::DtzDirEntry, int(i));
                    textureShown++;
                }
            }
        }
        if (modelShown == 0) addTreeItem(modelRoot, L"No .mdl/.dff entries were named in the current internal map.");
        if (textureShown == 0) addTreeItem(textureRoot, L"No .xtx/.chk/.tex entries were named in the current internal map.");
        expandTreeItem(previewRoot);
    }

    HTREEITEM imgRoot = addTreeItem(root, L"GAME.DTZ internal gta3PS2.img sector map");
    if (!dirEntries.empty()) {
        for (size_t i = 0; i < dirEntries.size(); ++i) {
            const auto& entry = dirEntries[i];
            std::wstringstream line;
            uint64_t imgOffset = uint64_t(entry.startSector) * 2048ull;
            uint64_t imgBytes = uint64_t(entry.sectorCount) * 2048ull;
            line << widen(entry.name)
                 << L"  start=" << entry.startSector
                 << L"  count=" << entry.sectorCount
                 << L"  end=" << (entry.startSector + entry.sectorCount)
                 << L"  bytes=" << imgBytes
                 << L"  imgOff=" << imgOffset
                 << L"  DTZ records=" << entry.matchingRecordIndices.size();
            addTreeItem(imgRoot, line.str(), StorylandTreeKind::DtzDirEntry, int(i));
        }
    } else {
        addTreeItem(imgRoot, L"No internal sector entries were decoded from GAME.DTZ. This means the current scanner did not recognize the runtime map layout for this file yet.");
        addTreeItem(imgRoot, L"No beta-build .dir loaded; retail GAME.DTZ supplies the archive map.");
    }


    HTREEITEM headerRoot = addTreeItem(root, L"Header fields (raw GTAG/GATG)");
    const auto& headers = gDtzArchive.headerFields();
    for (size_t i = 0; i < headers.size(); ++i) {
        const auto& field = headers[i];
        std::wstringstream line;
        line << hexWide(field.offset, 4) << L"  " << widen(field.name) << L" = " << hexWide(field.value);
        addTreeItem(headerRoot, line.str(), StorylandTreeKind::DtzHeader, int(i));
    }

    HTREEITEM resourceRoot = addTreeItem(root, L"Named DTZ header/resource pointers");
    const auto& hints = gDtzArchive.resourceHints();
    for (size_t i = 0; i < hints.size(); ++i) {
        const auto& hint = hints[i];
        std::wstringstream line;
        line << hexWide(hint.offset, 6) << L"  " << widen(hint.name);
        addTreeItem(resourceRoot, line.str(), StorylandTreeKind::DtzResourceHint, int(i));
    }

    HTREEITEM fallbackRoot = addTreeItem(root, L"Raw stream records");
    const auto& records = gDtzArchive.sectorRecords();
    size_t shown = 0;
    for (size_t i = 0; i < records.size() && shown < 500; ++i) {
        const auto& r = records[i];
        std::wstringstream line;
        if (!r.resourceName.empty()) line << widen(r.resourceName) << L"  ";
        else line << L"record " << i << L"  ";
        line << L"start=" << r.startSector
             << L"  count=" << r.sectorCount
             << L"  end=" << (r.startSector + r.sectorCount)
             << L"  offset=" << hexWide(r.recordOffset, 6);
        addTreeItem(fallbackRoot, line.str(), StorylandTreeKind::DtzSectorRecord, int(i));
        shown++;
    }
    if (shown == 0) addTreeItem(fallbackRoot, L"No internal streaming records to show.");
    if (records.size() > shown) {
        std::wstringstream more;
        more << L"Showing first " << shown << L" of " << records.size() << L" records; sector map above contains the de-duplicated editable entries.";
        addTreeItem(fallbackRoot, more.str());
    }

    expandTreeItem(root);
    expandTreeItem(overviewRoot);
    expandTreeItem(dataRoot);
    expandTreeItem(imgRoot);
    std::wstring status = std::to_wstring(dataBlocks.size()) + L" DAT blocks, " + std::to_wstring(dirEntries.size()) + L" internal streaming entries, " + std::to_wstring(records.size()) + L" raw DTZ records decoded from GAME.DTZ";
    if (gDtzArchive.hasCompanionImg()) status += L"; companion IMG loaded";
    status += L".";
    setStatus(status);
    selectDtzOverview();
}

static void populateModelList() {
    clearView();
    HTREEITEM root = addTreeItem(TVI_ROOT, L"MDL / Leeds model");
    std::wstringstream summary;
    summary << L"Detected " << widen(gModelFile.modelKindName()) << L"  size=" << gModelFile.fileSize() << L" bytes";
    addTreeItem(root, summary.str());

    const auto& bones = gModelFile.armatureBones();
    HTREEITEM armatureRoot = nullptr;
    if (!bones.empty()) {
        std::wstringstream armatureTitle;
        armatureTitle << L"Imported armature / ped skeleton  bones=" << bones.size();
        armatureRoot = addTreeItem(root, armatureTitle.str());
        for (size_t i = 0; i < bones.size(); ++i) {
            const auto& bone = bones[i];
            std::wstringstream line;
            line << L"#" << bone.index << L"  " << widen(bone.name)
                 << L"  " << widen(bone.sectionKind)
                 << L"  frame=" << hexWide(bone.offset, 6);
            if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
                line << L"  parent=#" << bone.parentIndex << L":" << widen(bones[size_t(bone.parentIndex)].name);
            } else {
                line << L"  parent=<root/unresolved>";
            }
            addTreeItem(armatureRoot, line.str(), StorylandTreeKind::ModelBone, int(i));
        }
    }

    if (gModelAnimLoaded) {
        std::wstringstream animTitle;
        animTitle << L"Loaded .anim applied to this MDL  tracks=" << gAnimFile.tracks().size();
        HTREEITEM animRoot = addTreeItem(root, animTitle.str(), StorylandTreeKind::AnimOverview, 0);

        const auto& animClips = gAnimFile.clips();
        for (const auto& clip : animClips) {
            std::wstringstream clipLine;
            clipLine << L"clip #" << clip.index << L"  " << widen(clip.name);
            if (clip.index == gAnimFile.activeClipIndex()) clipLine << L"  [ACTIVE]";
            clipLine << L"  entry=" << hexWide(clip.entryOffset, 6)
                     << L"  channels=" << clip.channelCount
                     << L"  duration=" << clip.duration;
            addTreeItem(animRoot, clipLine.str(), StorylandTreeKind::AnimClip, int(clip.index));
        }

        const auto& animTracks = gAnimFile.tracks();
        for (size_t i = 0; i < animTracks.size(); ++i) {
            const auto& track = animTracks[i];
            std::wstringstream line;
            line << L"#" << track.index << L"  " << widen(track.name)
                 << L"  clip=" << track.clipIndex
                 << L"  boneId=" << track.boneId
                 << L"  keys=" << track.keys.size()
                 << L"  stride=" << track.keyStride
                 << L"  " << widen(track.source);
            addTreeItem(animRoot, line.str(), StorylandTreeKind::AnimTrack, int(i));
        }

        expandTreeItem(animRoot);
    }

    const auto& fields = gModelFile.fields();
    HTREEITEM structuresRoot = nullptr;
    HTREEITEM headerStructuresRoot = nullptr;
    HTREEITEM sceneStructuresRoot = nullptr;
    HTREEITEM hierarchyStructuresRoot = nullptr;
    HTREEITEM platformStructuresRoot = nullptr;
    HTREEITEM diagnosticStructuresRoot = nullptr;
    auto structureParentForGroup = [&](const std::string& group) -> HTREEITEM {
        if (!structuresRoot) structuresRoot = addTreeItem(root, L"Decoded Leeds / RSL structures");
        auto begins = [&](const char* prefix) { return group.rfind(prefix, 0) == 0; };
        if (begins("sChunkHeader") || begins("Top-level") || begins("Resolved model") ||
            begins("PedData") || begins("SimpleModelData") || begins("ElementGroupModelData")) {
            if (!headerStructuresRoot) headerStructuresRoot = addTreeItem(structuresRoot, L"Model headers and family data");
            return headerStructuresRoot;
        }
        if (begins("RslTAnim") || begins("RslNode")) {
            if (!hierarchyStructuresRoot) hierarchyStructuresRoot = addTreeItem(structuresRoot, L"Frames and RslTAnim hierarchy");
            return hierarchyStructuresRoot;
        }
        if (begins("sPspGeometry") || begins("sPspGeometryMesh") || begins("sPs2Geometry")) {
            if (!platformStructuresRoot) platformStructuresRoot = addTreeItem(structuresRoot, L"Platform geometry streams");
            return platformStructuresRoot;
        }
        if (begins("Pointer Table")) {
            if (!diagnosticStructuresRoot) diagnosticStructuresRoot = addTreeItem(structuresRoot, L"Pointer diagnostics");
            return diagnosticStructuresRoot;
        }
        if (!sceneStructuresRoot) sceneStructuresRoot = addTreeItem(structuresRoot, L"Clumps, atomics, geometry and materials");
        return sceneStructuresRoot;
    };
    std::vector<std::string> groups;
    std::vector<HTREEITEM> groupItems;
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto& f = fields[i];
        auto found = std::find(groups.begin(), groups.end(), f.group);
        HTREEITEM groupItem = nullptr;
        if (found == groups.end()) {
            groups.push_back(f.group);
            groupItem = addTreeItem(structureParentForGroup(f.group), widen(f.group));
            groupItems.push_back(groupItem);
        } else {
            groupItem = groupItems[size_t(found - groups.begin())];
        }

        std::wstringstream line;
        line << hexWide(f.offset, 6) << L"  " << widen(f.name) << L" = " << hexWide(f.value);
        if (!f.note.empty()) line << L"  " << widen(f.note);
        addTreeItem(groupItem, line.str(), StorylandTreeKind::ModelField, int(i));
    }

    if (fields.empty()) {
        HTREEITEM linesRoot = addTreeItem(root, L"Raw notes");
        for (const auto& line : gModelFile.lines()) addTreeItem(linesRoot, widen(line.text));
    }

    expandTreeItem(root);
    if (armatureRoot) expandTreeItem(armatureRoot);
    if (structuresRoot) expandTreeItem(structuresRoot);
    if (headerStructuresRoot) expandTreeItem(headerStructuresRoot);
    if (hierarchyStructuresRoot) expandTreeItem(hierarchyStructuresRoot);
    setStatus(buildModelStatusLine());
}


static std::string archiveEntryExtensionLower(const std::string& name) {
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = name.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return ext;
}

static int archiveEntryNumberFromName(const std::string& name) {
    size_t underscore = name.find('_');
    if (underscore == std::string::npos) return -1;
    size_t pos = underscore + 1;
    int value = 0;
    bool found = false;
    while (pos < name.size() && std::isdigit(static_cast<unsigned char>(name[pos]))) {
        value = value * 10 + (name[pos] - '0');
        found = true;
        ++pos;
    }
    return found ? value : -1;
}

static bool archiveEntryIsWorld(const StorylandArchiveEntry& entry) {
    std::string ext = archiveEntryExtensionLower(entry.name);
    return ext == ".wrld" || ext == ".area";
}

static bool archiveEntryIsModel(const StorylandArchiveEntry& entry) {
    std::string ext = archiveEntryExtensionLower(entry.name);
    return ext == ".mdl" || ext == ".dff";
}

static bool archiveEntryIsTextureArchive(const StorylandArchiveEntry& entry) {
    std::string ext = archiveEntryExtensionLower(entry.name);
    return ext == ".xtx" || ext == ".chk" || ext == ".tex";
}

static bool archiveEntryIsAnimation(const StorylandArchiveEntry& entry) {
    std::string ext = archiveEntryExtensionLower(entry.name);
    return ext == ".anim" || ext == ".ifp" || ext == ".anm";
}

static void rebuildArchiveResourceScrollLists() {
    gArchiveMeshResourceIds.clear();
    gArchiveTextureIds.clear();
    gArchiveAnimationEntryIndices.clear();

    std::set<uint32_t> meshIds;
    std::set<uint32_t> textureIds;

    for (const auto& mesh : gArchiveBrowser.worldMeshes()) {
        meshIds.insert(mesh.resourceIndex);
        for (const auto& triangle : mesh.triangles) {
            if (triangle.textureId != 0xFFFFFFFFu) textureIds.insert(triangle.textureId);
        }
    }

    for (uint32_t id : meshIds) gArchiveMeshResourceIds.push_back(id);
    for (uint32_t id : textureIds) gArchiveTextureIds.push_back(id);

    const auto& entries = gArchiveBrowser.entries();
    for (size_t i = 0; i < entries.size(); ++i) {
        if (archiveEntryIsAnimation(entries[i])) gArchiveAnimationEntryIndices.push_back(int(i));
    }
}

static bool meshUsesTextureId(const StorylandWorldMesh& mesh, uint32_t textureId) {
    for (const auto& triangle : mesh.triangles) {
        if (triangle.textureId == textureId) return true;
    }
    return false;
}


static void selectWblOverview() {
    gSelectedKind = StorylandTreeKind::WblOverview;
    gSelectedIndex = 0;

    std::wstringstream ss;
    ss << L"WBL / Chinatown Wars worldblock\r\n";
    ss << L"Path: " << gWblFile.sourcePath() << L"\r\n";
    ss << L"Size: " << gWblFile.fileSize() << L" bytes / " << gWblFile.pageCount() << L" pages\r\n";
    ss << L"Origin raw: X=" << gWblFile.originXRaw() << L" Y=" << gWblFile.originYRaw() << L"\r\n";
    ss << L"Origin: X=" << gWblFile.originX() << L" Y=" << gWblFile.originY() << L"\r\n";
    ss << L"Parsed meshes: " << gWblFile.meshes().size() << L"\r\n";
    ss << L"Preview vertices: " << gWblFile.vertices().size() << L"\r\n";
    ss << L"Preview triangles: " << gWblFile.triangles().size() << L"\r\n";
    ss << L"Sector instances: " << gWblFile.tableA() << L", " << gWblFile.tableB() << L", " << gWblFile.tableC() << L", " << gWblFile.tableD() << L"\r\n\r\n";
    for (const auto& line : gWblFile.lines()) ss << widen(line) << L"\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void selectWblSection(int index) {
    const auto& sections = gWblFile.sections();
    if (index < 0 || size_t(index) >= sections.size()) return;
    gSelectedKind = StorylandTreeKind::WblSection;
    gSelectedIndex = index;
    const auto& section = sections[size_t(index)];

    std::wstringstream ss;
    ss << L"WBL section\r\n";
    ss << L"Name: " << widen(section.name) << L"\r\n";
    ss << L"Offset: " << hexWide(section.offset, 6) << L"\r\n";
    ss << L"Count: " << section.count << L"\r\n";
    ss << L"Stride: " << section.stride << L"\r\n";
    ss << L"Bytes: " << section.byteSize << L"\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void selectWblMesh(int index) {
    const auto& meshes = gWblFile.meshes();
    if (index < 0 || size_t(index) >= meshes.size()) return;
    gSelectedKind = StorylandTreeKind::WblMesh;
    gSelectedIndex = index;
    const auto& mesh = meshes[size_t(index)];

    std::wstringstream ss;
    ss << L"WBL mesh\r\n";
    ss << L"Index: " << mesh.index << L"\r\n";
    ss << L"Offset: " << hexWide(mesh.offset, 6) << L"\r\n";
    ss << L"Resource id: " << mesh.resourceId << L"\r\n";
    ss << L"Materials: " << int(mesh.materialCount) << L"\r\n";
    ss << L"Vertices: " << mesh.vertexCount << L"\r\n";
    ss << L"Triangles: " << mesh.triangleCount << L"\r\n";
    ss << L"Scale: " << mesh.scaleFactor << L"\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void selectWblBox(int index) {
    const auto& boxes = gWblFile.boxes();
    if (index < 0 || size_t(index) >= boxes.size()) return;
    gSelectedKind = StorylandTreeKind::WblBox;
    gSelectedIndex = index;
    const auto& box = boxes[size_t(index)];

    std::wstringstream ss;
    ss << L"WBL box record\r\n";
    ss << L"Index: " << box.index << L"\r\n";
    ss << L"Offset: " << hexWide(box.offset, 6) << L"\r\n";
    ss << L"Tag: " << hexWide(box.tag) << L"\r\n";
    ss << L"Raw min: " << box.rawMinX << L", " << box.rawMinY << L", " << box.rawMinZ << L"\r\n";
    ss << L"Raw max: " << box.rawMaxX << L", " << box.rawMaxY << L", " << box.rawMaxZ << L"\r\n";
    ss << L"Preview min: " << box.minX << L", " << box.minY << L", " << box.minZ << L"\r\n";
    ss << L"Preview max: " << box.maxX << L", " << box.maxY << L", " << box.maxZ << L"\r\n";
    ss << L"Visible: " << (box.visible ? L"yes" : L"no") << L"\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void populateWblList() {
    clearView();
    gMode = StorylandMode::WblFile;

    HTREEITEM root = addTreeItem(TVI_ROOT, L"WBL / Chinatown Wars worldblock", StorylandTreeKind::WblOverview, 0);
    addTreeItem(root, L"Source: " + gWblFile.sourcePath());
    for (const auto& line : gWblFile.lines()) addTreeItem(root, widen(line));

    HTREEITEM sectionRoot = addTreeItem(root, L"Header sections");
    const auto& sections = gWblFile.sections();
    for (size_t i = 0; i < sections.size(); ++i) {
        const auto& section = sections[i];
        std::wstringstream s;
        s << widen(section.name) << L" offset=" << hexWide(section.offset, 6)
          << L" count=" << section.count << L" stride=" << section.stride;
        addTreeItem(sectionRoot, s.str(), StorylandTreeKind::WblSection, int(i));
    }

    HTREEITEM meshRoot = addTreeItem(root, L"Meshes / preview geometry");
    const auto& meshes = gWblFile.meshes();
    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto& mesh = meshes[i];
        std::wstringstream s;
        s << L"#" << mesh.index
          << L" resource=" << mesh.resourceId
          << L" off=" << hexWide(mesh.offset, 6)
          << L" verts=" << mesh.vertexCount
          << L" tris=" << mesh.triangleCount
          << L" mats=" << int(mesh.materialCount);
        addTreeItem(meshRoot, s.str(), StorylandTreeKind::WblMesh, int(i));
        if (i >= 1200) break;
    }
    if (meshes.empty()) addTreeItem(meshRoot, L"No WBL meshes decoded.");

    expandTreeItem(root);
    expandTreeItem(sectionRoot);
    expandTreeItem(meshRoot);
    selectWblOverview();
}

static void selectWblPayload(const StorylandTreePayload& payload) {
    if (payload.kind == StorylandTreeKind::WblOverview) selectWblOverview();
    else if (payload.kind == StorylandTreeKind::WblSection) selectWblSection(payload.index);
    else if (payload.kind == StorylandTreeKind::WblMesh) selectWblMesh(payload.index);
    else if (payload.kind == StorylandTreeKind::WblBox) selectWblBox(payload.index);
}

static void addArchiveEntryTreeItem(HTREEITEM parent, const StorylandArchiveEntry& entry, size_t entryIndex) {
    std::wstringstream line;
    line << widen(entry.name)
         << L" | start=" << entry.startSector
         << L" count=" << entry.sectorCount
         << L" bytes=" << entry.byteSize
         << L" offset=" << entry.byteOffset;
    if (entry.usesLvzChunkHeader) {
        line << L" header=" << hexWide(entry.lvzHeaderOffset, 6);
    }
    addTreeItem(parent, line.str(), StorylandTreeKind::ArchiveEntry, int(entryIndex));
}

static void populateArchiveList() {
    clearView();
    rebuildArchiveResourceScrollLists();

    HTREEITEM root = addTreeItem(TVI_ROOT, gArchiveBrowser.hasLvzContext() ? L"LVZ + IMG archive browse" : L"IMG archive browse");
    if (gArchiveBrowser.hasLvzContext()) addTreeItem(root, L"LVZ: " + gArchiveBrowser.lvzPath());
    if (gArchiveBrowser.hasImgContext()) addTreeItem(root, L"IMG: " + gArchiveBrowser.imgPath());
    if (gArchiveBrowser.hasLvzContext()) addTreeItem(root, L"No .DIR: retail LVZ+IMG uses LVZ chunk headers + IMG payloads.");
    if (!gArchiveBrowser.levelSummary().empty()) addTreeItem(root, widen(gArchiveBrowser.levelSummary()));

    HTREEITEM resolutionRoot = addTreeItem(root, L"Resource resolution (placement RES -> IMG payload)");
    HTREEITEM resolvedRoot = addTreeItem(resolutionRoot, L"Resolved exactly");
    HTREEITEM linkedRoot = addTreeItem(resolutionRoot, L"Resolved through one linked sector");
    HTREEITEM conflictRoot = addTreeItem(resolutionRoot, L"Conflicting RES payloads (not guessed)");
    HTREEITEM missingRoot = addTreeItem(resolutionRoot, L"Missing / undecoded payloads");
    HTREEITEM sectorRoot = addTreeItem(root, L"IMG sector Resource[] tables");
    HTREEITEM meshResourceRoot = addTreeItem(root, L"Placed model resources (grouped by RES id)");
    HTREEITEM materialRoot = addTreeItem(root, L"Materials and textures");
    HTREEITEM textureIdRoot = addTreeItem(materialRoot, L"Material RES ids referenced by meshes");
    HTREEITEM boundTextureRoot = addTreeItem(materialRoot, L"Bound textures from master Resource[]");
    HTREEITEM directTextureRoot = addTreeItem(materialRoot, L"Unbound decoded textures (diagnostics)");
    HTREEITEM rawRoot = addTreeItem(root, L"Raw chunks and standalone assets");
    HTREEITEM worldRoot = addTreeItem(rawRoot, L"WRLD / AERA chunks");
    HTREEITEM modelRoot = addTreeItem(rawRoot, L"Standalone MDL-DFF chunks");
    HTREEITEM textureRoot = addTreeItem(rawRoot, L"Standalone XTX-CHK-TEX chunks");
    HTREEITEM animationRoot = addTreeItem(rawRoot, L"Animations");
    HTREEITEM otherRoot = addTreeItem(rawRoot, L"Other resources");

    const auto& entries = gArchiveBrowser.entries();
    const auto& placements = gArchiveBrowser.placements();
    const auto& sectors = gArchiveBrowser.sectors();
    const auto& resourceRows = gArchiveBrowser.imgResourceRows();
    const auto& resolutions = gArchiveBrowser.resourceResolutions();

    int worldCount = 0;
    int standaloneModelCount = 0;
    int standaloneTextureCount = 0;
    int animationCount = 0;
    int otherCount = 0;

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        if (archiveEntryIsWorld(entry)) {
            addArchiveEntryTreeItem(worldRoot, entry, i);
            worldCount++;
        } else if (archiveEntryIsModel(entry)) {
            addArchiveEntryTreeItem(modelRoot, entry, i);
            standaloneModelCount++;
        } else if (archiveEntryIsTextureArchive(entry)) {
            addArchiveEntryTreeItem(textureRoot, entry, i);
            standaloneTextureCount++;
        } else if (archiveEntryIsAnimation(entry)) {
            addArchiveEntryTreeItem(animationRoot, entry, i);
            animationCount++;
        } else {
            addArchiveEntryTreeItem(otherRoot, entry, i);
            otherCount++;
        }
    }

    for (size_t i = 0; i < gArchiveMeshResourceIds.size(); ++i) {
        uint32_t resourceId = gArchiveMeshResourceIds[i];
        uint32_t instanceCount = 0;
        uint32_t triangleCount = 0;
        for (const auto& placement : placements) {
            if (placement.resourceIndex == resourceId) instanceCount++;
        }
        for (const auto& mesh : gArchiveBrowser.worldMeshes()) {
            if (mesh.resourceIndex == resourceId) triangleCount += uint32_t(mesh.triangles.size());
        }

        std::wstringstream line;
        line << L"resource " << resourceId
             << L" | instances=" << instanceCount
             << L" | triangles=" << triangleCount;
        addTreeItem(meshResourceRoot, line.str(), StorylandTreeKind::ArchiveMeshResource, int(i));
    }

    std::map<uint32_t, size_t> meshListIndex;
    for (size_t i = 0; i < gArchiveMeshResourceIds.size(); ++i) meshListIndex[gArchiveMeshResourceIds[i]] = i;
    uint32_t sameSectorCount = 0, linkedCount = 0, conflictCount = 0, missingCount = 0;
    for (const auto& resolution : resolutions) {
        std::wstringstream line;
        line << L"sector " << resolution.sectorIndex
             << L" [" << resolution.sectorX << L"," << resolution.sectorY << L"]"
             << L" | RES=" << resolution.resourceId
             << L" | placements=" << resolution.placementCount
             << L" | source=" << widen(resolution.source);
        if (resolution.payloadOffset != 0) line << L" | payload=" << hexWide(resolution.payloadOffset, 8);
        if (resolution.candidateCount > 1) line << L" | candidates=" << resolution.candidateCount;
        HTREEITEM parent = missingRoot;
        if (resolution.source == "same-sector") { parent = resolvedRoot; sameSectorCount++; }
        else if (resolution.source == "unique linked sector" || resolution.source == "same-row" || resolution.source == "official AERA" || resolution.source == "master LVZ" || resolution.source == "placement-fit exact RES" || resolution.source == "IMG continuation") { parent = linkedRoot; linkedCount++; }
        else if (resolution.source == "conflict") { parent = conflictRoot; conflictCount++; }
        else missingCount++;
        auto listIndex = meshListIndex.find(resolution.resourceId);
        if (listIndex != meshListIndex.end() && parent != conflictRoot && parent != missingRoot) {
            addTreeItem(parent, line.str(), StorylandTreeKind::ArchiveMeshResource, int(listIndex->second));
        } else {
            addTreeItem(parent, line.str());
        }
    }

    for (const auto& sector : sectors) {
        uint32_t rowCount = 0, usedCount = 0, decodedCount = 0;
        for (const auto& row : resourceRows) {
            if (row.sectorIndex != sector.sectorIndex) continue;
            rowCount++;
            if (row.usedByPlacement) usedCount++;
            if (row.decodedAsMesh) decodedCount++;
        }
        std::wstringstream sectorLine;
        sectorLine << L"sector " << sector.sectorIndex
                   << L" [" << sector.sectorX << L"," << sector.sectorY << L"]"
                   << L" | Resource[]=" << rowCount
                   << L" | placed=" << usedCount
                   << L" | meshes=" << decodedCount
                   << L" | IMG=" << hexWide(sector.imgOffset, 8);
        HTREEITEM sectorItem = addTreeItem(sectorRoot, sectorLine.str());
        for (const auto& row : resourceRows) {
            if (row.sectorIndex != sector.sectorIndex || !row.usedByPlacement) continue;
            std::wstringstream rowLine;
            rowLine << L"row[" << row.rowIndex << L"] RES=" << row.resourceId
                    << L" table=" << hexWide(row.tableOffset, 8)
                    << L" payload=" << hexWide(row.payloadOffset, 8)
                    << L" bytes=" << row.payloadSize
                    << (row.decodedAsMesh ? L" | mesh" : L" | undecoded");
            addTreeItem(sectorItem, rowLine.str());
        }
    }

    for (size_t i = 0; i < gArchiveTextureIds.size(); ++i) {
        uint32_t textureId = gArchiveTextureIds[i];
        uint32_t meshCount = 0;
        uint32_t triangleCount = 0;
        for (const auto& mesh : gArchiveBrowser.worldMeshes()) {
            bool meshHasTexture = false;
            for (const auto& triangle : mesh.triangles) {
                if (triangle.textureId == textureId) {
                    triangleCount++;
                    meshHasTexture = true;
                }
            }
            if (meshHasTexture) meshCount++;
        }

        std::wstringstream line;
        line << L"texture/material id " << textureId
             << L" | meshResources=" << meshCount
             << L" | triangles=" << triangleCount;
        addTreeItem(textureIdRoot, line.str(), StorylandTreeKind::ArchiveTextureResource, int(i));
    }

    const auto& directTextures = gArchiveBrowser.directTextures();
    uint32_t boundTextureCount = 0;
    for (size_t i = 0; i < directTextures.size(); ++i) {
        const auto& texture = directTextures[i];
        std::wstringstream line;
        line << widen(texture.name)
             << L" | " << texture.width << L"x" << texture.height
             << L" | bpp=" << texture.bpp
             << L" | header=" << hexWide(texture.headerOffset, 6)
             << L" | data=" << hexWide(texture.dataOffset, 6);
        if (texture.materialId >= 0) {
            boundTextureCount++;
            line << L" | RES=" << texture.materialId << L" | " << widen(texture.source);
            addTreeItem(boundTextureRoot, line.str(), StorylandTreeKind::ArchiveDirectTexture, int(i));
        } else {
            line << L" | " << widen(texture.source);
            addTreeItem(directTextureRoot, line.str(), StorylandTreeKind::ArchiveDirectTexture, int(i));
        }
    }

    if (gArchiveAnimationEntryIndices.empty()) {
        addTreeItem(animationRoot, L"No standalone animation chunks detected in this LVZ/IMG.");
    }

    std::wstringstream details;
    details << L"LVZ + IMG archive\r\n\r\n"
            << L"WRLD/AREA chunks: " << worldCount << L"\r\n"
            << L"Placed mesh resources: " << gArchiveMeshResourceIds.size() << L"\r\n"
            << L"Texture/material IDs: " << gArchiveTextureIds.size() << L"\r\n"
            << L"Decoded direct LVZ/AREA textures: " << gArchiveBrowser.directTextures().size() << L"\r\n"
            << L"Textures bound to material RES ids: " << boundTextureCount << L"\r\n"
            << L"Standalone MDL chunks: " << standaloneModelCount << L"\r\n"
            << L"Standalone texture chunks: " << standaloneTextureCount << L"\r\n"
            << L"Animation chunks: " << animationCount << L"\r\n"
            << L"Other chunks: " << otherCount << L"\r\n"
            << L"Parsed sectors: " << sectors.size() << L"\r\n"
            << L"Parsed visible placements: " << placements.size() << L"\r\n"
            << L"Parsed real mesh resources: " << gArchiveBrowser.worldMeshes().size() << L"\r\n\r\n"
            << L"IMG Resource[] rows: " << resourceRows.size() << L"\r\n"
            << L"Exact same-sector bindings: " << sameSectorCount << L"\r\n"
            << L"Unique linked-sector bindings: " << linkedCount << L"\r\n"
            << L"Conflicts deliberately not guessed: " << conflictCount << L"\r\n"
            << L"Missing / undecoded bindings: " << missingCount << L"\r\n\r\n"
            << L"Resource resolution keeps IPL instance ids separate from model RES ids and shows the exact IMG provenance.\r\n"
            << L"Bound textures use master LVZ Resource[] indices; unbound scan results are kept under diagnostics.\r\n"
            << L"Double-click a directly openable embedded MDL/XTX/CHK/TEX/DTZ/BIN entry to inspect it as its own file.\r\n";
    setDetails(details.str());

    expandTreeItem(root);
    expandTreeItem(resolutionRoot);
    expandTreeItem(meshResourceRoot);

    gSelectedIndex = -1;
    gSelectedKind = StorylandTreeKind::None;
    InvalidateRect(gPreview, nullptr, TRUE);
    setStatus(std::to_wstring(gArchiveMeshResourceIds.size()) + L" placed mesh resources; " + std::to_wstring(gArchiveTextureIds.size()) + L" material IDs; " + std::to_wstring(gArchiveBrowser.directTextures().size()) + L" decoded direct LVZ/AREA textures.");
}

static void selectArchiveRoot() {
    gSelectedIndex = -1;
    gSelectedKind = StorylandTreeKind::None;

    const auto& entries = gArchiveBrowser.entries();
    const auto& placements = gArchiveBrowser.placements();
    const auto& sectors = gArchiveBrowser.sectors();
    int worldCount = 0;
    int modelCount = 0;
    int textureCount = 0;
    int otherCount = 0;
    for (const auto& entry : entries) {
        if (archiveEntryIsWorld(entry)) worldCount++;
        else if (archiveEntryIsModel(entry)) modelCount++;
        else if (archiveEntryIsTextureArchive(entry)) textureCount++;
        else otherCount++;
    }

    std::wstringstream ss;
    ss << L"LVZ + IMG archive preview\r\n\r\n"
       << L"LVZ: " << gArchiveBrowser.lvzPath() << L"\r\n"
       << L"IMG: " << gArchiveBrowser.imgPath() << L"\r\n"
       << L"DIR source: none; retail LVZ+IMG mode reconstructs entries from LVZ chunk headers.\r\n\r\n"
       << L"WRLD/AREA chunks: " << worldCount << L"\r\n"
       << L"Model chunks: " << modelCount << L"\r\n"
       << L"Texture chunks: " << textureCount << L"\r\n"
       << L"Other chunks: " << otherCount << L"\r\n"
       << L"Parsed sectors: " << sectors.size() << L"\r\n"
       << L"Parsed visible placements: " << placements.size() << L"\r\n"
       << L"Parsed real mesh resources: " << gArchiveBrowser.worldMeshes().size() << L"\r\n\r\n"
       << L"The OpenGL viewport shows the whole map/resource layout when no singular WRLD/AREA entry is selected.\r\n"
       << L"WRLD/AREA placement is parsed from sLevelSectorDirectory, sector pass pointers, and 0x50-byte sGeomInstance rows with sector-origin translation.\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectArchiveEntry(int index) {
    const auto& entries = gArchiveBrowser.entries();
    if (index < 0 || size_t(index) >= entries.size()) return;

    const auto& entry = entries[size_t(index)];
    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::ArchiveEntry;

    std::wstring ext = widen(archiveEntryExtensionLower(entry.name));

    std::wstringstream ss;
    ss << (gArchiveBrowser.hasLvzContext() ? L"LVZ + IMG entry\r\n\r\n" : L"IMG archive entry\r\n\r\n")
       << L"Name: " << widen(entry.name) << L"\r\n"
       << L"Kind: " << ext << L"\r\n"
       << L"Index: " << entry.index << L"\r\n"
       << L"Start sector: " << entry.startSector << L"\r\n"
       << L"Sector count: " << entry.sectorCount << L"\r\n"
       << L"Byte offset: " << entry.byteOffset << L"\r\n"
       << L"Byte size: " << entry.byteSize << L"\r\n";

    if (entry.usesLvzChunkHeader) {
        ss << L"LVZ chunk header: " << hexWide(entry.lvzHeaderOffset, 6) << L"\r\n";
    }
    if (gArchiveBrowser.hasLvzContext()) ss << L"LVZ context: " << gArchiveBrowser.lvzPath() << L"\r\n";
    if (gArchiveBrowser.hasImgContext()) ss << L"IMG source: " << gArchiveBrowser.imgPath() << L"\r\n";
    if (gArchiveBrowser.hasLvzContext()) ss << L"DIR source: none; LVZ+IMG mode reconstructs entries from LVZ chunk headers.\r\n";

    if (archiveEntryIsWorld(entry)) {
        ss << L"\r\nPreview: this WRLD/AREA sector is shown alone in the OpenGL viewport.\r\n";
    } else if (archiveEntryIsModel(entry)) {
        ss << L"\r\nPreview: double-click this model to open it in the MDL viewer.\r\n";
    } else if (archiveEntryIsTextureArchive(entry)) {
        ss << L"\r\nPreview: double-click this texture archive to open it in the texture viewer.\r\n";
    } else {
        ss << L"\r\nThis resource is listed, but not directly decoded yet.\r\n";
    }

    ss << L"\r\nFile > Open always opens a disk file. Double-click an embedded entry to inspect it.\r\n";

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}


static void selectArchiveMeshResource(int listIndex) {
    if (listIndex < 0 || size_t(listIndex) >= gArchiveMeshResourceIds.size()) return;

    uint32_t resourceId = gArchiveMeshResourceIds[size_t(listIndex)];
    gSelectedIndex = listIndex;
    gSelectedKind = StorylandTreeKind::ArchiveMeshResource;

    uint32_t instanceCount = 0;
    uint32_t meshCount = 0;
    uint32_t triangleCount = 0;
    uint32_t sectorCount = 0;
    std::set<uint32_t> sectorsUsed;

    for (const auto& placement : gArchiveBrowser.placements()) {
        if (placement.resourceIndex != resourceId) continue;
        instanceCount++;
        sectorsUsed.insert(placement.sectorIndex);
    }

    for (const auto& mesh : gArchiveBrowser.worldMeshes()) {
        if (mesh.resourceIndex != resourceId) continue;
        meshCount++;
        triangleCount += uint32_t(mesh.triangles.size());
    }

    sectorCount = uint32_t(sectorsUsed.size());

    std::wstringstream ss;
    ss << L"LVZ + IMG placed mesh resource\r\n\r\n"
       << L"Resource id: " << resourceId << L"\r\n"
       << L"Parsed mesh variants: " << meshCount << L"\r\n"
       << L"Placed instances: " << instanceCount << L"\r\n"
       << L"Sectors used: " << sectorCount << L"\r\n"
       << L"Triangles in parsed resource meshes: " << triangleCount << L"\r\n\r\n"
       << L"The OpenGL viewport is isolated to this real placed mesh resource.\r\n"
       << L"Right-click this mesh resource > Replace Selected LVZ+IMG Resource From File to replace this mesh resource in-place. If the selected file is a normal .mdl/.wrld Leeds chunk, Storyland converts its Leeds strip geometry into the existing runtime-safe sector payload slot, preserves this resource id, and does not redirect the Resource[] row.\r\n"
       << L"Right-click this mesh resource > Clone Selected Mesh Resource From Resource ID to clone another already parsed sector mesh resource, like resource 376, into this one while keeping this resource id.\r\n"
       << L"Right-click this mesh resource > Change Selected Mesh Resource ID only when you explicitly want to change the id afterwards.\r\n"
       << L"Storyland preserves the selected resource id, Resource[] pointer wrapper, WRLD sector size, and later LVZ IMG offsets for runtime-safe mesh replacement.\r\n"
       << L"This is the useful scroll-through list for LVZ/IMG world meshes, because many Stories level resources are not standalone .mdl files.\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectArchiveTextureResource(int listIndex) {
    if (listIndex < 0 || size_t(listIndex) >= gArchiveTextureIds.size()) return;

    uint32_t textureId = gArchiveTextureIds[size_t(listIndex)];
    gSelectedIndex = listIndex;
    gSelectedKind = StorylandTreeKind::ArchiveTextureResource;

    uint32_t meshCount = 0;
    uint32_t triangleCount = 0;
    uint32_t instanceCount = 0;
    std::set<uint32_t> resourcesUsed;

    for (const auto& mesh : gArchiveBrowser.worldMeshes()) {
        bool meshHasTexture = false;
        for (const auto& triangle : mesh.triangles) {
            if (triangle.textureId == textureId) {
                triangleCount++;
                meshHasTexture = true;
            }
        }
        if (meshHasTexture) {
            meshCount++;
            resourcesUsed.insert(mesh.resourceIndex);
        }
    }

    for (const auto& placement : gArchiveBrowser.placements()) {
        if (resourcesUsed.find(placement.resourceIndex) != resourcesUsed.end()) instanceCount++;
    }

    std::wstringstream ss;
    ss << L"LVZ + IMG texture/material reference\r\n\r\n"
       << L"Texture/material id: " << textureId << L"\r\n"
       << L"Mesh resources using it: " << meshCount << L"\r\n"
       << L"Placed instances using those resources: " << instanceCount << L"\r\n"
       << L"Triangles using this id: " << triangleCount << L"\r\n\r\n"
       << L"The OpenGL viewport is isolated to triangles using this material texture id.\r\n"
       << L"Full decoded texture binding still needs the direct texture-resource table bound to these material ids.\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectArchiveDirectTexture(int listIndex) {
    const auto& textures = gArchiveBrowser.directTextures();
    if (listIndex < 0 || size_t(listIndex) >= textures.size()) return;

    const auto& texture = textures[size_t(listIndex)];
    gSelectedIndex = listIndex;
    gSelectedKind = StorylandTreeKind::ArchiveDirectTexture;

    std::wstringstream ss;
    ss << L"Decoded direct LVZ/AREA texture\r\n\r\n"
       << L"Name: " << widen(texture.name) << L"\r\n"
       << L"Index: " << texture.index << L"\r\n"
       << L"Size: " << texture.width << L"x" << texture.height << L"\r\n"
       << L"BPP: " << texture.bpp << L"\r\n"
       << L"Header offset: " << hexWide(texture.headerOffset, 6) << L"\r\n"
       << L"Data offset: " << hexWide(texture.dataOffset, 6) << L"\r\n"
       << L"Format flags: " << hexWide(texture.formatFlags, 4) << L"\r\n"
       << L"Raster flags: " << hexWide(texture.rasterFlags, 8) << L"\r\n"
       << L"Material RES id: " << (texture.materialId >= 0 ? std::to_wstring(texture.materialId) : L"unbound") << L"\r\n"
       << L"Provenance: " << widen(texture.source) << L"\r\n\r\n"
       << L"The OpenGL viewport is showing this decoded texture image from the LVZ stream.\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectArchiveAnimationResource(int listIndex) {
    if (listIndex < 0 || size_t(listIndex) >= gArchiveAnimationEntryIndices.size()) return;

    int entryIndex = gArchiveAnimationEntryIndices[size_t(listIndex)];
    gSelectedIndex = listIndex;
    gSelectedKind = StorylandTreeKind::ArchiveAnimationResource;

    std::wstringstream ss;
    ss << L"LVZ + IMG animation resource\r\n\r\n";
    if (entryIndex >= 0 && size_t(entryIndex) < gArchiveBrowser.entries().size()) {
        const auto& entry = gArchiveBrowser.entries()[size_t(entryIndex)];
        ss << L"Name: " << widen(entry.name) << L"\r\n"
           << L"Entry index: " << entry.index << L"\r\n"
           << L"Byte offset: " << entry.byteOffset << L"\r\n"
           << L"Byte size: " << entry.byteSize << L"\r\n";
    }
    ss << L"\r\nAnimation decode is not implemented yet, but this list is separated so it can be wired to an ANIM/IFP viewer next.\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}


static void populateAnimList();
static void populateModelList();

static void selectAnimOverview() {
    gSelectedKind = StorylandTreeKind::AnimOverview;
    gSelectedIndex = 0;

    std::wstringstream ss;
    ss << L"Storyland .anim player / scanner\r\n\r\n"
       << L"Path: " << gAnimFile.sourcePath() << L"\r\n"
       << L"Raw size: " << gAnimFile.rawBytes().size() << L" bytes\r\n"
       << L"Tracks: " << gAnimFile.tracks().size() << L"\r\n"
       << L"Frames: " << gAnimFile.frameCount() << L"\r\n"
       << L"FPS: " << gAnimFile.framesPerSecond() << L"\r\n"
       << L"Duration: " << gAnimFile.durationSeconds() << L" seconds\r\n"
       << L"Current time: " << gAnimCurrentTime << L" seconds\r\n"
       << L"Playback: " << (gAnimPlaying ? L"playing" : L"paused") << L"\r\n"
       << L"Attached to MDL: " << (gMode == StorylandMode::ModelFile && gModelAnimLoaded ? L"yes" : L"no") << L"\r\n"
       << L"Keyframe scan: " << (gAnimFile.hasKeyframeCandidates() ? L"Leeds compressed transform channels decoded" : L"no Leeds clip/channel descriptors decoded yet; static inspection only") << L"\r\n";
    if (!gAnimFile.clips().empty()) {
        const StorylandAnimClip& clip = gAnimFile.clips()[gAnimFile.activeClipIndex()];
        ss << L"Active clip: #" << clip.index << L" " << widen(clip.name)
           << L" entry=" << hexWide(clip.entryOffset, 6)
           << L" channels=" << clip.channelCount
           << L" duration=" << clip.duration << L"\r\n";
    }
    ss << L"\r\n";

    ss << L"Viewer keys:\r\n"
       << L"  + / Page Up zoom in, - / Page Down zoom out, F fit closer, R or 0 reset.\r\n"
       << L"  LMB rotate, RMB pan, mouse wheel zoom.\r\n\r\n";

    ss << L"String/name hints found in the file:\r\n";
    const auto& hints = gAnimFile.stringHints();
    if (hints.empty()) {
        ss << L"  none\r\n";
    } else {
        for (size_t i = 0; i < hints.size() && i < 80; ++i) {
            ss << L"  " << i << L": " << widen(hints[i]) << L"\r\n";
        }
        if (hints.size() > 80) ss << L"  ... " << (hints.size() - 80) << L" more\r\n";
    }

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void selectAnimClip(int index) {
    const auto& clips = gAnimFile.clips();
    if (index < 0 || size_t(index) >= clips.size()) return;

    uint32_t clipIndex = uint32_t(index);
    if (!gAnimFile.setActiveClipIndex(clipIndex)) return;

    gAnimCurrentTime = 0.0f;
    gSelectedKind = StorylandTreeKind::AnimClip;
    gSelectedIndex = index;

    const StorylandAnimClip& clip = gAnimFile.clips()[clipIndex];

    std::wstringstream ss;
    ss << L".anim clip selected\r\n\r\n"
       << L"Clip index: " << clip.index << L"\r\n"
       << L"Name: " << widen(clip.name) << L"\r\n"
       << L"Entry offset: " << hexWide(clip.entryOffset, 6) << L"\r\n"
       << L"Channel table: " << hexWide(clip.channelTableOffset, 6) << L"\r\n"
       << L"Channels: " << clip.channelCount << L"\r\n"
       << L"Flags: " << hexWide(clip.flags) << L"\r\n"
       << L"Duration: " << clip.duration << L" seconds\r\n"
       << L"Decoded tracks for this clip: " << gAnimFile.tracks().size() << L"\r\n\r\n"
       << L"This clip is now the active playback/deformation clip.\r\n";

    setDetails(ss.str());

    if (gMode == StorylandMode::ModelFile) {
        populateModelList();
    } else if (gMode == StorylandMode::AnimFile) {
        populateAnimList();
    }

    InvalidateRect(gPreview, nullptr, FALSE);
}


static void selectAnimTrack(int index) {
    const auto& tracks = gAnimFile.tracks();
    if (index < 0 || size_t(index) >= tracks.size()) return;

    gSelectedKind = StorylandTreeKind::AnimTrack;
    gSelectedIndex = index;

    const auto& track = tracks[size_t(index)];
    std::wstringstream ss;
    ss << L".anim track\r\n\r\n"
       << L"Track index: " << track.index << L"\r\n"
       << L"Name: " << widen(track.name) << L"\r\n"
       << L"Clip index: " << track.clipIndex << L"\r\n"
       << L"Channel index: " << track.channelIndex << L"\r\n"
       << L"Bone id: " << track.boneId << L"\r\n"
       << L"Bone index: " << track.boneIndex << L"\r\n"
       << L"Parent index: ";
    if (track.parentIndex == 0xFFFFFFFFu) ss << L"<root>";
    else ss << track.parentIndex;
    ss << L"\r\n"
       << L"Descriptor offset: " << hexWide(track.offset, 6) << L"\r\n"
       << L"Key data offset: " << hexWide(track.keyDataOffset, 6) << L"\r\n"
       << L"Key stride: " << track.keyStride << L" bytes\r\n"
       << L"Tag: " << hexWide(track.tag) << L"\r\n"
       << L"Source: " << widen(track.source) << L"\r\n"
       << L"Keys: " << track.keys.size() << L"\r\n\r\n";

    size_t showCount = std::min<size_t>(track.keys.size(), 80);
    for (size_t i = 0; i < showCount; ++i) {
        const auto& key = track.keys[i];
        ss << L"key[" << i << L"]"
           << L" t=" << key.time
           << L" off=" << hexWide(key.offset, 6)
           << L" T=(" << key.tx << L", " << key.ty << L", " << key.tz << L")"
           << L" Q=(" << key.qx << L", " << key.qy << L", " << key.qz << L", " << key.qw << L")"
           << L"\r\n";
    }
    if (track.keys.size() > showCount) ss << L"... " << (track.keys.size() - showCount) << L" more keys\r\n";

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void selectAnimField(int index) {
    const auto& fields = gAnimFile.fields();
    if (index < 0 || size_t(index) >= fields.size()) return;

    gSelectedKind = StorylandTreeKind::AnimField;
    gSelectedIndex = index;

    const auto& field = fields[size_t(index)];
    std::wstringstream ss;
    ss << L".anim raw field\r\n\r\n"
       << L"Group: " << widen(field.group) << L"\r\n"
       << L"Name: " << widen(field.name) << L"\r\n"
       << L"Offset: " << hexWide(field.offset, 6) << L"\r\n"
       << L"Value: " << hexWide(field.value) << L" / " << field.value << L"\r\n"
       << L"Note: " << widen(field.note) << L"\r\n";

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void selectAnimString(int index) {
    const auto& hints = gAnimFile.stringHints();
    if (index < 0 || size_t(index) >= hints.size()) return;

    gSelectedKind = StorylandTreeKind::AnimString;
    gSelectedIndex = index;

    std::wstringstream ss;
    ss << L".anim string/name hint\r\n\r\n"
       << L"Index: " << index << L"\r\n"
       << L"Text: " << widen(hints[size_t(index)]) << L"\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void selectAnimPayload(const StorylandTreePayload& payload) {
    if (payload.kind == StorylandTreeKind::AnimOverview) selectAnimOverview();
    else if (payload.kind == StorylandTreeKind::AnimClip) selectAnimClip(payload.index);
    else if (payload.kind == StorylandTreeKind::AnimTrack) selectAnimTrack(payload.index);
    else if (payload.kind == StorylandTreeKind::AnimField) selectAnimField(payload.index);
    else if (payload.kind == StorylandTreeKind::AnimString) selectAnimString(payload.index);
}

static void populateAnimList() {
    clearView();

    HTREEITEM root = addTreeItem(TVI_ROOT, L"ANIM / Leeds animation");
    addTreeItem(root, L"Detected animation file", StorylandTreeKind::AnimOverview, 0);

    const auto& clips = gAnimFile.clips();
    std::wstringstream clipTitle;
    clipTitle << L"Decoded Leeds animation clips  clips=" << clips.size();
    HTREEITEM clipsRoot = addTreeItem(root, clipTitle.str());
    for (const auto& clip : clips) {
        std::wstringstream line;
        line << L"#" << clip.index << L"  " << widen(clip.name);
        if (clip.index == gAnimFile.activeClipIndex()) line << L"  [ACTIVE]";
        line << L"  entry=" << hexWide(clip.entryOffset, 6)
             << L"  channels=" << clip.channelCount
             << L"  duration=" << clip.duration;
        addTreeItem(clipsRoot, line.str(), StorylandTreeKind::AnimClip, int(clip.index));
    }

    const auto& tracks = gAnimFile.tracks();
    std::wstringstream trackTitle;
    trackTitle << L"Animation tracks / transform probes  tracks=" << tracks.size();
    HTREEITEM tracksRoot = addTreeItem(root, trackTitle.str());
    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto& track = tracks[i];
        std::wstringstream line;
        line << L"#" << track.index << L"  " << widen(track.name)
             << L"  keys=" << track.keys.size()
             << L"  source=" << widen(track.source);
        addTreeItem(tracksRoot, line.str(), StorylandTreeKind::AnimTrack, int(i));
    }

    const auto& fields = gAnimFile.fields();
    HTREEITEM headerRoot = addTreeItem(root, L"Raw header / dword probes");
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i];
        std::wstringstream line;
        line << hexWide(field.offset, 6) << L"  " << widen(field.name)
             << L" = " << hexWide(field.value);
        addTreeItem(headerRoot, line.str(), StorylandTreeKind::AnimField, int(i));
    }

    const auto& hints = gAnimFile.stringHints();
    HTREEITEM stringRoot = addTreeItem(root, L"String/name hints");
    for (size_t i = 0; i < hints.size() && i < 250; ++i) {
        std::wstringstream line;
        line << L"#" << i << L"  " << widen(hints[i]);
        addTreeItem(stringRoot, line.str(), StorylandTreeKind::AnimString, int(i));
    }
    if (hints.empty()) addTreeItem(stringRoot, L"No useful ASCII name strings found.");

    expandTreeItem(root);
    expandTreeItem(tracksRoot);

    setStatus(L"ANIM loaded; " + widen(gAnimFile.summaryLine()));
    selectAnimOverview();
}

static StorylandModelPoint displayBonePosition(const StorylandModelBone& bone);
static float lengthStorylandPoint(const StorylandModelPoint& a);
static bool boneHasVisiblePreviewPosition(const StorylandModelBone& bone);

static std::vector<StorylandQuat> gLastAnimatedBoneWorldRotations;
static std::vector<StorylandModelPoint> gLastAnimatedBoneRawPositions;

static StorylandQuat normalizeStorylandQuat(StorylandQuat q) {
    float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (!std::isfinite(length) || length < 0.00001f) return {};
    q.x /= length;
    q.y /= length;
    q.z /= length;
    q.w /= length;
    return q;
}

static StorylandQuat multiplyStorylandQuatRaw(const StorylandQuat& a, const StorylandQuat& b) {
    StorylandQuat out;
    out.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    out.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    out.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    out.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return out;
}

static StorylandQuat multiplyStorylandQuat(const StorylandQuat& a, const StorylandQuat& b) {
    return normalizeStorylandQuat(multiplyStorylandQuatRaw(a, b));
}

static StorylandQuat conjugateStorylandQuat(const StorylandQuat& q) {
    StorylandQuat out;
    out.x = -q.x;
    out.y = -q.y;
    out.z = -q.z;
    out.w = q.w;
    return out;
}

static StorylandQuat modelBoneBindWorldRotation(const StorylandModelBone& bone) {
    StorylandQuat q;
    q.x = bone.worldRotationX;
    q.y = bone.worldRotationY;
    q.z = bone.worldRotationZ;
    q.w = bone.worldRotationW;
    if (!bone.hasWorldRotation) {
        return {};
    }
    return normalizeStorylandQuat(q);
}

static StorylandQuat modelBoneBindLocalRotation(const StorylandModelBone& bone) {
    StorylandQuat q;
    q.x = bone.localRotationX;
    q.y = bone.localRotationY;
    q.z = bone.localRotationZ;
    q.w = bone.localRotationW;
    if (!bone.hasLocalRotation) {
        return {};
    }
    return normalizeStorylandQuat(q);
}

static StorylandQuat rebaseLocalDeltaToBindWorldBasis(const StorylandQuat& localDelta, const StorylandModelBone& bone) {
    StorylandQuat bindWorld = modelBoneBindWorldRotation(bone);
    return multiplyStorylandQuat(multiplyStorylandQuat(bindWorld, localDelta), conjugateStorylandQuat(bindWorld));
}

static StorylandQuat rebaseLocalDeltaToExplicitBindWorldBasis(
    const StorylandQuat& localDelta,
    const StorylandQuat& bindWorld
) {
    StorylandQuat normalizedBindWorld = normalizeStorylandQuat(bindWorld);
    return multiplyStorylandQuat(
        multiplyStorylandQuat(normalizedBindWorld, normalizeStorylandQuat(localDelta)),
        conjugateStorylandQuat(normalizedBindWorld)
    );
}

static StorylandQuat rebaseLocalAbsolutePoseToBindWorldDelta(
    const StorylandQuat& localAbsolutePose,
    const StorylandModelBone& bone
) {
    StorylandQuat bindLocal = modelBoneBindLocalRotation(bone);
    StorylandQuat sourceDelta = multiplyStorylandQuat(conjugateStorylandQuat(bindLocal), normalizeStorylandQuat(localAbsolutePose));
    return rebaseLocalDeltaToBindWorldBasis(sourceDelta, bone);
}

static std::string normalizeBoneLookupName(const std::string& text);
static bool currentAttachedAnimLooksWeaponLike();
static bool animPreviewShouldFreezeBoneAtBindPose(const StorylandModelBone& bone);
static bool animPreviewShouldLockRootTranslation(const StorylandModelBone& bone);
static bool animPreviewShouldLockRootRotation(const StorylandModelBone& bone);
static bool currentModelLooksLikeLcsPedSkeleton(const std::vector<StorylandModelBone>& bones);

static StorylandQuat slerpIdentityToQuat(const StorylandQuat& q, float factor) {
    StorylandQuat target = normalizeStorylandQuat(q);
    if (target.w < 0.0f) {
        target.x = -target.x;
        target.y = -target.y;
        target.z = -target.z;
        target.w = -target.w;
    }

    if (factor < 0.0f) factor = 0.0f;
    else if (factor > 1.0f) factor = 1.0f;

    float dot = target.w;
    if (dot < -1.0f) dot = -1.0f;
    else if (dot > 1.0f) dot = 1.0f;
    if (dot > 0.9995f) {
        StorylandQuat out;
        out.x = target.x * factor;
        out.y = target.y * factor;
        out.z = target.z * factor;
        out.w = 1.0f + (target.w - 1.0f) * factor;
        return normalizeStorylandQuat(out);
    }

    float theta = std::acos(dot);
    float sinTheta = std::sin(theta);
    if (std::fabs(sinTheta) < 0.00001f) {
        return {};
    }

    float scaleA = std::sin((1.0f - factor) * theta) / sinTheta;
    float scaleB = std::sin(factor * theta) / sinTheta;

    StorylandQuat out;
    out.x = target.x * scaleB;
    out.y = target.y * scaleB;
    out.z = target.z * scaleB;
    out.w = 1.0f * scaleA + target.w * scaleB;
    return normalizeStorylandQuat(out);
}

static float quatDeltaAngleRadians(const StorylandQuat& q) {
    StorylandQuat normalized = normalizeStorylandQuat(q);
    float w = normalized.w;
    if (w < -1.0f) w = -1.0f;
    else if (w > 1.0f) w = 1.0f;
    w = std::fabs(w);
    return 2.0f * std::acos(w);
}

static StorylandQuat clampPreviewDeltaRotation(const StorylandQuat& q, float maxRadians) {
    float angle = quatDeltaAngleRadians(q);
    if (!std::isfinite(angle) || angle <= maxRadians || angle < 0.00001f) {
        return normalizeStorylandQuat(q);
    }

    return slerpIdentityToQuat(q, maxRadians / angle);
}

static bool animPreviewShouldLockRootLikeBone(const StorylandModelBone& bone) {
    return animPreviewShouldFreezeBoneAtBindPose(bone);
}

static float maxPreviewRotationForBone(const StorylandModelBone& bone) {
    std::string name = normalizeBoneLookupName(bone.name);


    if (currentModelLooksLikeLcsPedSkeleton(gModelFile.armatureBones())) {
        if (name.find("finger") != std::string::npos || name.find("toe") != std::string::npos) return 1.20f;
        if (name.find("hand") != std::string::npos || name.find("foot") != std::string::npos) return 1.55f;
        if (name.find("forearm") != std::string::npos || name.find("calf") != std::string::npos) return 1.55f;
        if (name.find("upperarm") != std::string::npos || name.find("thigh") != std::string::npos) return 1.55f;
        if (name.find("clavicle") != std::string::npos) return 1.25f;
        if (name == "spine" || name == "spine1" || name == "neck") return 1.10f;
        return 1.25f;
    }

    if (name == "spine" || name == "spine1" || name == "neck") {
        return currentAttachedAnimLooksWeaponLike() ? 0.45f : 0.70f;
    }

    if (name.find("clavicle") != std::string::npos) {
        return currentAttachedAnimLooksWeaponLike() ? 0.70f : 0.90f;
    }

    if (name.find("upperarm") != std::string::npos || name.find("forearm") != std::string::npos ||
        name.find("hand") != std::string::npos || name.find("finger") != std::string::npos) {
        return currentAttachedAnimLooksWeaponLike() ? 0.95f : 1.25f;
    }

    if (name.find("thigh") != std::string::npos || name.find("calf") != std::string::npos ||
        name.find("foot") != std::string::npos || name.find("toe") != std::string::npos) {
        if (currentAttachedAnimLooksWeaponLike()) {
            return 0.35f;
        }
        return 0.85f;
    }

    return 0.85f;
}

static StorylandModelPoint rotatePointByQuat(const StorylandQuat& q, const StorylandModelPoint& p) {
    StorylandQuat v;
    v.x = p.x;
    v.y = p.y;
    v.z = p.z;
    v.w = 0.0f;

    StorylandQuat inv;
    inv.x = -q.x;
    inv.y = -q.y;
    inv.z = -q.z;
    inv.w = q.w;

    StorylandQuat rotated = multiplyStorylandQuatRaw(multiplyStorylandQuatRaw(q, v), inv);
    return StorylandModelPoint{rotated.x, rotated.y, rotated.z};
}

static StorylandModelPoint addStorylandPoint(const StorylandModelPoint& a, const StorylandModelPoint& b) {
    return StorylandModelPoint{a.x + b.x, a.y + b.y, a.z + b.z};
}

static StorylandModelPoint subtractStorylandPoint(const StorylandModelPoint& a, const StorylandModelPoint& b) {
    return StorylandModelPoint{a.x - b.x, a.y - b.y, a.z - b.z};
}

static StorylandModelPoint scaleStorylandPoint(const StorylandModelPoint& a, float scale) {
    return StorylandModelPoint{a.x * scale, a.y * scale, a.z * scale};
}

static std::string storylandLowerAscii(const std::string& text) {
    std::string out = text;
    for (char& ch : out) {
        if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
    }
    return out;
}

static void replaceAllStorylandAscii(std::string& value, const std::string& oldText, const std::string& newText) {
    if (oldText.empty()) return;
    size_t pos = 0;
    while ((pos = value.find(oldText, pos)) != std::string::npos) {
        value.replace(pos, oldText.size(), newText);
        pos += newText.size();
    }
}

static std::string normalizeBoneLookupName(const std::string& text) {
    std::string lower = storylandLowerAscii(text);
    for (char& ch : lower) {
        if (ch == ' ' || ch == '-' || ch == '.') ch = '_';
    }
    while (lower.find("__") != std::string::npos) replaceAllStorylandAscii(lower, "__", "_");

    if (lower.rfind("bip01_", 0) == 0) lower = lower.substr(6);
    if (lower == "scene_root" || lower == "pivots" || lower == "male_base" || lower == "female_base" ||
        lower == "male_base01" || lower == "female_base01") {
        lower = "root";
    }

    replaceAllStorylandAscii(lower, "right_", "r_");
    replaceAllStorylandAscii(lower, "left_", "l_");
    replaceAllStorylandAscii(lower, "upper_arm", "upperarm");
    replaceAllStorylandAscii(lower, "lower_arm", "forearm");
    replaceAllStorylandAscii(lower, "lowerarm", "forearm");
    replaceAllStorylandAscii(lower, "shin", "calf");

    return lower;
}

static uint32_t bleedsDirectIdFromNormalizedBoneName(const std::string& name) {
    if (name == "root") return 0;
    if (name == "pelvis") return 1;
    if (name == "spine") return 2;
    if (name == "spine1") return 3;
    if (name == "neck") return 4;
    if (name == "head") return 5;
    if (name == "r_clavicle") return 21;
    if (name == "r_upperarm") return 22;
    if (name == "r_forearm") return 23;
    if (name == "r_hand") return 24;
    if (name == "r_finger") return 25;
    if (name == "l_clavicle") return 31;
    if (name == "l_upperarm") return 32;
    if (name == "l_forearm") return 33;
    if (name == "l_hand") return 34;
    if (name == "l_finger") return 35;
    if (name == "l_thigh") return 41;
    if (name == "l_calf") return 42;
    if (name == "l_foot") return 43;
    if (name == "l_toe0" || name == "l_toe") return 54;
    if (name == "r_thigh") return 51;
    if (name == "r_calf") return 52;
    if (name == "r_foot") return 53;
    if (name == "r_toe0" || name == "r_toe") return 55;
    if (name == "jaw") return 8;
    return 0xFFFFFFFFu;
}

static uint32_t bleedsDirectIdForModelBone(const StorylandModelBone& bone) {
    uint32_t byName = bleedsDirectIdFromNormalizedBoneName(normalizeBoneLookupName(bone.name));
    if (byName != 0xFFFFFFFFu) return byName;
    if (bone.boneId != 0xFFFFFFFFu && bone.boneId != 255u) return bone.boneId;
    return 0xFFFFFFFFu;
}

static bool bleedsDirectIdIsLowerBody(uint32_t directId) {
    return directId == 41u || directId == 42u || directId == 43u || directId == 54u || directId == 2000u ||
           directId == 51u || directId == 52u || directId == 53u || directId == 55u || directId == 2001u;
}

static bool stringLooksWeaponAnimName(const std::string& text) {
    std::string name = storylandLowerAscii(text);
    return name.find("weapon") != std::string::npos ||
           name.find("ak") != std::string::npos ||
           name.find("m4") != std::string::npos ||
           name.find("uzi") != std::string::npos ||
           name.find("pistol") != std::string::npos ||
           name.find("colt") != std::string::npos ||
           name.find("python") != std::string::npos ||
           name.find("tec") != std::string::npos ||
           name.find("m60") != std::string::npos ||
           name.find("shotgun") != std::string::npos ||
           name.find("rifle") != std::string::npos ||
           name.find("sniper") != std::string::npos ||
           name.find("rocket") != std::string::npos ||
           name.find("grenade") != std::string::npos ||
           name.find("flame") != std::string::npos ||
           name.find("chainsaw") != std::string::npos ||
           name.find("csaw") != std::string::npos ||
           name.find("knife") != std::string::npos ||
           name.find("sword") != std::string::npos ||
           name.find("bat") != std::string::npos ||
           name.find("baseball") != std::string::npos;
}

static bool currentAttachedAnimLooksWeaponLike() {
    if (!gModelAnimLoaded) {
        return false;
    }

    const auto& clips = gAnimFile.clips();
    if (!clips.empty() && gAnimFile.activeClipIndex() < clips.size()) {
        if (stringLooksWeaponAnimName(clips[gAnimFile.activeClipIndex()].name)) {
            return true;
        }
    }

    for (const auto& hint : gAnimFile.stringHints()) {
        if (stringLooksWeaponAnimName(hint)) {
            return true;
        }
    }

    return false;
}

static const StorylandAnimTrack* findAnimTrackForModelBone(size_t boneIndex, const StorylandModelBone& bone) {
    const auto& tracks = gAnimFile.tracks();
    if (tracks.empty()) return nullptr;

    uint32_t directId = bleedsDirectIdForModelBone(bone);
    if (directId != 0xFFFFFFFFu && directId != 255u) {
        for (const auto& track : tracks) {
            if (track.boneId == directId && track.boneId != 255u) return &track;
        }
    }

    std::string boneName = normalizeBoneLookupName(bone.name);


    for (const auto& track : tracks) {
        if (normalizeBoneLookupName(track.name) == boneName) return &track;
    }

    return nullptr;
}

struct StorylandPreviewBindAffine {
    bool valid = false;
    StorylandModelPoint meshMin;
    StorylandModelPoint meshMax;
    StorylandModelPoint rawMin;
    StorylandModelPoint rawMax;
};

static bool modelBoneWorldPositionIsUseful(const StorylandModelBone& bone) {
    if (!bone.hasWorldPosition) return false;
    if (!std::isfinite(bone.worldPosition.x) ||
        !std::isfinite(bone.worldPosition.y) ||
        !std::isfinite(bone.worldPosition.z)) {
        return false;
    }

    float worldLengthSq =
        bone.worldPosition.x * bone.worldPosition.x +
        bone.worldPosition.y * bone.worldPosition.y +
        bone.worldPosition.z * bone.worldPosition.z;


    return worldLengthSq > 0.0000001f;
}

static StorylandModelPoint rawBoneBindPosition(const StorylandModelBone& bone) {
    if (modelBoneWorldPositionIsUseful(bone)) return bone.worldPosition;


    if (bone.hasComposedPosition) return bone.composedPosition;
    if (bone.hasPreviewPosition) return bone.previewPosition;
    if (bone.hasLocalPosition) return bone.localPosition;
    if (bone.hasWorldPosition) return bone.worldPosition;
    return displayBonePosition(bone);
}

static bool modelUsesPreviewSpaceBindPositions(const std::vector<StorylandModelBone>& bones) {
    if (bones.empty()) return false;

    uint32_t usefulWorldPositions = 0;
    uint32_t previewSpacePositions = 0;
    uint32_t decodedMeshSpaceHints = 0;

    for (const StorylandModelBone& bone : bones) {
        if (modelBoneWorldPositionIsUseful(bone)) {
            usefulWorldPositions++;
        }

        if (bone.hasPreviewPosition || bone.hasComposedPosition) {
            previewSpacePositions++;
        }

        if (bone.previewPositionSource.find("decoded mesh space") != std::string::npos ||
            bone.previewPositionSource.find("skin-weight centroid") != std::string::npos) {
            decodedMeshSpaceHints++;
        }
    }


    return usefulWorldPositions == 0u &&
           previewSpacePositions >= std::max<uint32_t>(4u, uint32_t(bones.size() / 2u)) &&
           decodedMeshSpaceHints >= std::max<uint32_t>(2u, uint32_t(bones.size() / 4u));
}

static float robustQuantileFloat(std::vector<float> values, double q) {
    if (values.empty()) return 0.0f;
    std::sort(values.begin(), values.end());
    double scaled = q * double(values.size() - 1);
    size_t lo = size_t(std::floor(scaled));
    size_t hi = size_t(std::ceil(scaled));
    float t = float(scaled - double(lo));
    return values[lo] * (1.0f - t) + values[hi] * t;
}

static bool computePreviewBindAffine(const std::vector<StorylandModelPoint>& sourcePoints, StorylandPreviewBindAffine& affine) {
    affine = {};

    const auto& bones = gModelFile.armatureBones();
    if (sourcePoints.empty() || bones.empty()) return false;

    std::vector<float> xs;
    std::vector<float> ys;
    std::vector<float> zs;
    xs.reserve(sourcePoints.size());
    ys.reserve(sourcePoints.size());
    zs.reserve(sourcePoints.size());

    for (const StorylandModelPoint& point : sourcePoints) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) continue;
        xs.push_back(point.x);
        ys.push_back(point.y);
        zs.push_back(point.z);
    }

    if (xs.empty() || ys.empty() || zs.empty()) return false;

    affine.meshMin = StorylandModelPoint{
        robustQuantileFloat(xs, 0.005),
        robustQuantileFloat(ys, 0.005),
        robustQuantileFloat(zs, 0.005)
    };
    affine.meshMax = StorylandModelPoint{
        robustQuantileFloat(xs, 0.995),
        robustQuantileFloat(ys, 0.995),
        robustQuantileFloat(zs, 0.995)
    };

    bool haveRawBounds = false;
    for (const StorylandModelBone& bone : bones) {
        if (!bone.hasWorldPosition && !bone.hasLocalPosition) continue;
        StorylandModelPoint p = rawBoneBindPosition(bone);
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) continue;

        if (!haveRawBounds) {
            affine.rawMin = p;
            affine.rawMax = p;
            haveRawBounds = true;
        } else {
            affine.rawMin.x = std::min(affine.rawMin.x, p.x);
            affine.rawMin.y = std::min(affine.rawMin.y, p.y);
            affine.rawMin.z = std::min(affine.rawMin.z, p.z);
            affine.rawMax.x = std::max(affine.rawMax.x, p.x);
            affine.rawMax.y = std::max(affine.rawMax.y, p.y);
            affine.rawMax.z = std::max(affine.rawMax.z, p.z);
        }
    }

    if (!haveRawBounds) return false;

    if (std::fabs(affine.meshMax.x - affine.meshMin.x) < 0.00001f ||
        std::fabs(affine.meshMax.y - affine.meshMin.y) < 0.00001f ||
        std::fabs(affine.meshMax.z - affine.meshMin.z) < 0.00001f ||
        std::fabs(affine.rawMax.x - affine.rawMin.x) < 0.00001f ||
        std::fabs(affine.rawMax.y - affine.rawMin.y) < 0.00001f ||
        std::fabs(affine.rawMax.z - affine.rawMin.z) < 0.00001f) {
        return false;
    }

    affine.valid = true;
    return true;
}

static StorylandModelPoint mapAffineAxisPoint(
    const StorylandModelPoint& point,
    const StorylandModelPoint& sourceMin,
    const StorylandModelPoint& sourceMax,
    const StorylandModelPoint& targetMin,
    const StorylandModelPoint& targetMax
) {
    auto axis = [](float value, float srcMin, float srcMax, float dstMin, float dstMax) -> float {
        float span = std::max(0.00001f, srcMax - srcMin);
        float t = (value - srcMin) / span;
        return dstMin + t * (dstMax - dstMin);
    };

    return StorylandModelPoint{
        axis(point.x, sourceMin.x, sourceMax.x, targetMin.x, targetMax.x),
        axis(point.y, sourceMin.y, sourceMax.y, targetMin.y, targetMax.y),
        axis(point.z, sourceMin.z, sourceMax.z, targetMin.z, targetMax.z)
    };
}

static StorylandModelPoint previewPointToRawBindSpace(const StorylandPreviewBindAffine& affine, const StorylandModelPoint& point) {
    if (!affine.valid) return point;
    return mapAffineAxisPoint(point, affine.meshMin, affine.meshMax, affine.rawMin, affine.rawMax);
}

static StorylandModelPoint rawBindPointToPreviewSpace(const StorylandPreviewBindAffine& affine, const StorylandModelPoint& point) {
    if (!affine.valid) return point;
    return mapAffineAxisPoint(point, affine.rawMin, affine.rawMax, affine.meshMin, affine.meshMax);
}


static float wrappedAnimTime(float seconds) {
    float duration = gAnimFile.durationSeconds();
    if (!std::isfinite(duration) || duration <= 0.0001f) {
        duration = 1.0f;
    }

    if (!std::isfinite(seconds)) {
        seconds = 0.0f;
    }

    while (seconds < 0.0f) {
        seconds += duration;
    }

    while (seconds > duration) {
        seconds -= duration;
    }

    return seconds;
}

static bool sampleAnimTrackTransform(
    const StorylandAnimTrack& track,
    float seconds,
    StorylandQuat& q,
    StorylandModelPoint& translation
) {
    q = {};
    translation = {};
    if (track.keys.empty()) return false;

    seconds = wrappedAnimTime(seconds);

    const StorylandAnimKey* a = &track.keys.front();
    const StorylandAnimKey* b = &track.keys.back();

    for (size_t i = 0; i < track.keys.size(); ++i) {
        if (track.keys[i].time <= seconds) a = &track.keys[i];
        if (track.keys[i].time >= seconds) {
            b = &track.keys[i];
            break;
        }
    }

    float span = std::max(0.0001f, b->time - a->time);
    float factor = std::max(0.0f, std::min(1.0f, (seconds - a->time) / span));

    StorylandQuat qa{a->qx, a->qy, a->qz, a->qw};
    StorylandQuat qb{b->qx, b->qy, b->qz, b->qw};

    if ((qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w) < 0.0f) {
        qb.x = -qb.x;
        qb.y = -qb.y;
        qb.z = -qb.z;
        qb.w = -qb.w;
    }

    q.x = qa.x * (1.0f - factor) + qb.x * factor;
    q.y = qa.y * (1.0f - factor) + qb.y * factor;
    q.z = qa.z * (1.0f - factor) + qb.z * factor;
    q.w = qa.w * (1.0f - factor) + qb.w * factor;
    q = normalizeStorylandQuat(q);

    translation.x = a->tx * (1.0f - factor) + b->tx * factor;
    translation.y = a->ty * (1.0f - factor) + b->ty * factor;
    translation.z = a->tz * (1.0f - factor) + b->tz * factor;

    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w) &&
           std::isfinite(translation.x) && std::isfinite(translation.y) && std::isfinite(translation.z);
}

static bool sampleAnimTrackDeltaTransform(
    const StorylandAnimTrack& track,
    float seconds,
    StorylandQuat& deltaRotation,
    StorylandModelPoint& deltaTranslation
) {
    deltaRotation = {};
    deltaTranslation = {};

    if (track.keys.empty()) return false;

    StorylandQuat currentRotation{};
    StorylandModelPoint currentTranslation{};
    if (!sampleAnimTrackTransform(track, seconds, currentRotation, currentTranslation)) {
        return false;
    }

    const StorylandAnimKey& first = track.keys.front();
    StorylandQuat baseRotation{first.qx, first.qy, first.qz, first.qw};
    baseRotation = normalizeStorylandQuat(baseRotation);

    StorylandModelPoint baseTranslation{first.tx, first.ty, first.tz};

    deltaRotation = multiplyStorylandQuat(currentRotation, conjugateStorylandQuat(baseRotation));
    deltaTranslation = subtractStorylandPoint(currentTranslation, baseTranslation);

    if (!std::isfinite(deltaRotation.x) || !std::isfinite(deltaRotation.y) ||
        !std::isfinite(deltaRotation.z) || !std::isfinite(deltaRotation.w) ||
        !std::isfinite(deltaTranslation.x) || !std::isfinite(deltaTranslation.y) ||
        !std::isfinite(deltaTranslation.z)) {
        deltaRotation = {};
        deltaTranslation = {};
        return false;
    }

    return true;
}


struct StorylandLcsBasisMap {
    bool valid = false;
    StorylandModelPoint rawOrigin;
    StorylandModelPoint rawSide;
    StorylandModelPoint rawUp;
    StorylandModelPoint rawForward;
    StorylandModelPoint displayOrigin;
    StorylandModelPoint displaySide;
    StorylandModelPoint displayUp;
    StorylandModelPoint displayForward;
    float sideScale = 1.0f;
    float upScale = 1.0f;
    float forwardScale = 1.0f;
};

static bool gLcsRawHierarchySkinStateValid = false;
static StorylandLcsBasisMap gLcsLastBasisMap;
static std::vector<StorylandModelPoint> gLcsLastRawBindPositions;
static std::vector<StorylandModelPoint> gLcsLastAnimatedRawPositions;
static std::vector<StorylandQuat> gLcsLastBindWorldRotations;
static std::vector<StorylandQuat> gLcsLastAnimatedWorldRotations;

static std::vector<StorylandModelPoint> buildLcsRawHierarchyBindPositions(
    const std::vector<StorylandQuat>& bindWorldRotations
) {
    const auto& bones = gModelFile.armatureBones();
    std::vector<StorylandModelPoint> rawPositions(bones.size());

    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const StorylandModelBone& bone = bones[boneIndex];
        if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < boneIndex) {
            size_t parentIndex = size_t(bone.parentIndex);
            rawPositions[boneIndex] = addStorylandPoint(
                rawPositions[parentIndex],
                rotatePointByQuat(bindWorldRotations[parentIndex], bone.localPosition)
            );
        } else {
            rawPositions[boneIndex] = StorylandModelPoint{};
        }
    }

    return rawPositions;
}

static uint32_t findLcsModelBoneByDirectId(uint32_t directId) {
    const auto& bones = gModelFile.armatureBones();
    for (uint32_t i = 0; i < bones.size(); ++i) {
        if (bleedsDirectIdForModelBone(bones[i]) == directId) return i;
    }
    return 0xFFFFFFFFu;
}

static StorylandModelPoint normalizeLocalPointForBasis(
    const StorylandModelPoint& point,
    const StorylandModelPoint& fallback
) {
    float lenSq = point.x * point.x + point.y * point.y + point.z * point.z;
    if (!std::isfinite(lenSq) || lenSq < 0.00000001f) return fallback;
    float invLen = 1.0f / std::sqrt(lenSq);
    return StorylandModelPoint{point.x * invLen, point.y * invLen, point.z * invLen};
}

static float dotLocalPointForBasis(const StorylandModelPoint& a, const StorylandModelPoint& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static StorylandModelPoint crossLocalPointForBasis(const StorylandModelPoint& a, const StorylandModelPoint& b) {
    return StorylandModelPoint{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static float lengthLocalPointForBasis(const StorylandModelPoint& point) {
    float lenSq = point.x * point.x + point.y * point.y + point.z * point.z;
    if (!std::isfinite(lenSq) || lenSq <= 0.0f) return 0.0f;
    return std::sqrt(lenSq);
}

static StorylandModelPoint subtractLocalPointForBasis(
    const StorylandModelPoint& a,
    const StorylandModelPoint& b
) {
    return StorylandModelPoint{a.x - b.x, a.y - b.y, a.z - b.z};
}

static StorylandModelPoint addScaledLocalPointForBasis(
    const StorylandModelPoint& base,
    const StorylandModelPoint& axis,
    float scale
) {
    return StorylandModelPoint{
        base.x + axis.x * scale,
        base.y + axis.y * scale,
        base.z + axis.z * scale
    };
}

static bool buildLcsBasisMapFromRawToDisplay(
    const std::vector<StorylandModelPoint>& rawBind,
    StorylandLcsBasisMap& outMap
) {
    const auto& bones = gModelFile.armatureBones();
    outMap = {};
    if (bones.empty() || rawBind.size() != bones.size()) return false;

    uint32_t pelvis = findLcsModelBoneByDirectId(1u);
    uint32_t spine1 = findLcsModelBoneByDirectId(3u);
    uint32_t head = findLcsModelBoneByDirectId(5u);
    uint32_t leftHand = findLcsModelBoneByDirectId(34u);
    uint32_t rightHand = findLcsModelBoneByDirectId(24u);
    uint32_t leftClav = findLcsModelBoneByDirectId(31u);
    uint32_t rightClav = findLcsModelBoneByDirectId(21u);

    if (pelvis == 0xFFFFFFFFu || spine1 == 0xFFFFFFFFu || head == 0xFFFFFFFFu) return false;
    if (leftHand == 0xFFFFFFFFu || rightHand == 0xFFFFFFFFu) {
        leftHand = leftClav;
        rightHand = rightClav;
    }
    if (leftHand == 0xFFFFFFFFu || rightHand == 0xFFFFFFFFu) return false;

    StorylandModelPoint rawOrigin = rawBind[pelvis];
    StorylandModelPoint rawUpVector = subtractLocalPointForBasis(rawBind[head], rawBind[pelvis]);
    StorylandModelPoint rawSideVector = subtractLocalPointForBasis(rawBind[leftHand], rawBind[rightHand]);

    StorylandModelPoint displayOrigin = displayBonePosition(bones[pelvis]);
    StorylandModelPoint displayUpVector = subtractLocalPointForBasis(displayBonePosition(bones[head]), displayBonePosition(bones[pelvis]));
    StorylandModelPoint displaySideVector = subtractLocalPointForBasis(displayBonePosition(bones[leftHand]), displayBonePosition(bones[rightHand]));

    float rawUpLen = std::max(0.0001f, lengthLocalPointForBasis(rawUpVector));
    float rawSideLen = std::max(0.0001f, lengthLocalPointForBasis(rawSideVector));
    float displayUpLen = std::max(0.0001f, lengthLocalPointForBasis(displayUpVector));
    float displaySideLen = std::max(0.0001f, lengthLocalPointForBasis(displaySideVector));

    StorylandModelPoint rawUp = normalizeLocalPointForBasis(rawUpVector, StorylandModelPoint{0.0f, 0.0f, 1.0f});
    StorylandModelPoint rawSide = normalizeLocalPointForBasis(rawSideVector, StorylandModelPoint{0.0f, 1.0f, 0.0f});
    StorylandModelPoint rawForward = normalizeLocalPointForBasis(crossLocalPointForBasis(rawSide, rawUp), StorylandModelPoint{1.0f, 0.0f, 0.0f});
    rawSide = normalizeLocalPointForBasis(crossLocalPointForBasis(rawUp, rawForward), rawSide);

    StorylandModelPoint displayUp = normalizeLocalPointForBasis(displayUpVector, StorylandModelPoint{0.0f, 0.0f, 1.0f});
    StorylandModelPoint displaySide = normalizeLocalPointForBasis(displaySideVector, StorylandModelPoint{0.0f, 1.0f, 0.0f});
    StorylandModelPoint displayForward = normalizeLocalPointForBasis(crossLocalPointForBasis(displaySide, displayUp), StorylandModelPoint{1.0f, 0.0f, 0.0f});
    displaySide = normalizeLocalPointForBasis(crossLocalPointForBasis(displayUp, displayForward), displaySide);

    float rawForwardLen = std::max(0.0001f, (rawSideLen + rawUpLen) * 0.5f);
    float displayForwardLen = std::max(0.0001f, (displaySideLen + displayUpLen) * 0.5f);

    outMap.valid = true;
    outMap.rawOrigin = rawOrigin;
    outMap.rawSide = rawSide;
    outMap.rawUp = rawUp;
    outMap.rawForward = rawForward;
    outMap.displayOrigin = displayOrigin;
    outMap.displaySide = displaySide;
    outMap.displayUp = displayUp;
    outMap.displayForward = displayForward;
    outMap.sideScale = displaySideLen / rawSideLen;
    outMap.upScale = displayUpLen / rawUpLen;
    outMap.forwardScale = displayForwardLen / rawForwardLen;
    return true;
}

static StorylandModelPoint mapLcsRawHierarchyPointToDisplay(
    const StorylandLcsBasisMap& map,
    const StorylandModelPoint& rawPoint
) {
    if (!map.valid) return rawPoint;

    StorylandModelPoint rel = subtractLocalPointForBasis(rawPoint, map.rawOrigin);
    float side = dotLocalPointForBasis(rel, map.rawSide) * map.sideScale;
    float up = dotLocalPointForBasis(rel, map.rawUp) * map.upScale;
    float forward = dotLocalPointForBasis(rel, map.rawForward) * map.forwardScale;

    StorylandModelPoint out = map.displayOrigin;
    out = addScaledLocalPointForBasis(out, map.displaySide, side);
    out = addScaledLocalPointForBasis(out, map.displayUp, up);
    out = addScaledLocalPointForBasis(out, map.displayForward, forward);
    return out;
}

static StorylandModelPoint mapLcsDisplayPointToRawHierarchy(
    const StorylandLcsBasisMap& map,
    const StorylandModelPoint& displayPoint
) {
    if (!map.valid) return displayPoint;

    StorylandModelPoint rel = subtractLocalPointForBasis(displayPoint, map.displayOrigin);

    float side = dotLocalPointForBasis(rel, map.displaySide) / std::max(0.0001f, map.sideScale);
    float up = dotLocalPointForBasis(rel, map.displayUp) / std::max(0.0001f, map.upScale);
    float forward = dotLocalPointForBasis(rel, map.displayForward) / std::max(0.0001f, map.forwardScale);

    StorylandModelPoint out = map.rawOrigin;
    out = addScaledLocalPointForBasis(out, map.rawSide, side);
    out = addScaledLocalPointForBasis(out, map.rawUp, up);
    out = addScaledLocalPointForBasis(out, map.rawForward, forward);
    return out;
}

static std::vector<StorylandModelPoint> buildLcsStrictLocalHierarchyAnimatedBonePositions() {
    const auto& bones = gModelFile.armatureBones();
    std::vector<StorylandModelPoint> animatedRaw(bones.size());
    std::vector<StorylandModelPoint> animatedPreview(bones.size());
    std::vector<StorylandQuat> bindWorldRotations(bones.size());
    std::vector<StorylandQuat> animatedWorldRotations(bones.size());

    if (bones.empty()) {
        gLastAnimatedBoneWorldRotations.clear();
        gLastAnimatedBoneRawPositions.clear();
        gLcsRawHierarchySkinStateValid = false;
        gLcsLastRawBindPositions.clear();
        gLcsLastAnimatedRawPositions.clear();
        gLcsLastBindWorldRotations.clear();
        gLcsLastAnimatedWorldRotations.clear();
        return animatedPreview;
    }

    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const StorylandModelBone& bone = bones[boneIndex];

        StorylandQuat localBindRotation = modelBoneBindLocalRotation(bone);
        StorylandQuat bindWorldRotation = localBindRotation;
        if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < boneIndex) {
            bindWorldRotation = multiplyStorylandQuat(bindWorldRotations[size_t(bone.parentIndex)], localBindRotation);
        }
        bindWorldRotations[boneIndex] = normalizeStorylandQuat(bindWorldRotation);
    }

    std::vector<StorylandModelPoint> rawBindPositions = buildLcsRawHierarchyBindPositions(bindWorldRotations);

    StorylandLcsBasisMap basisMap;
    buildLcsBasisMapFromRawToDisplay(rawBindPositions, basisMap);

    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const StorylandModelBone& bone = bones[boneIndex];

        StorylandQuat localPoseRotation = modelBoneBindLocalRotation(bone);

        const StorylandAnimTrack* track = findAnimTrackForModelBone(boneIndex, bone);
        if (track && !animPreviewShouldFreezeBoneAtBindPose(bone)) {
            StorylandModelPoint ignoredTranslation{};
            (void)sampleAnimTrackTransform(*track, gAnimCurrentTime, localPoseRotation, ignoredTranslation);
        }

        if (animPreviewShouldLockRootRotation(bone)) {
            localPoseRotation = modelBoneBindLocalRotation(bone);
        }

        localPoseRotation = normalizeStorylandQuat(localPoseRotation);

        if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
            size_t parentIndex = size_t(bone.parentIndex);
            StorylandQuat parentAnimatedRotation = animatedWorldRotations[parentIndex];
            StorylandModelPoint parentAnimatedPosition = animatedRaw[parentIndex];

            animatedRaw[boneIndex] = addStorylandPoint(
                parentAnimatedPosition,
                rotatePointByQuat(parentAnimatedRotation, bone.localPosition)
            );

            animatedWorldRotations[boneIndex] = normalizeStorylandQuat(
                multiplyStorylandQuat(parentAnimatedRotation, localPoseRotation)
            );
        } else {
            animatedRaw[boneIndex] = rawBindPositions[boneIndex];
            animatedWorldRotations[boneIndex] = localPoseRotation;
        }
    }

    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        animatedPreview[boneIndex] = mapLcsRawHierarchyPointToDisplay(basisMap, animatedRaw[boneIndex]);
    }

    gLastAnimatedBoneWorldRotations.assign(bones.size(), StorylandQuat{});
    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {


        gLastAnimatedBoneWorldRotations[boneIndex] = normalizeStorylandQuat(
            multiplyStorylandQuat(animatedWorldRotations[boneIndex], conjugateStorylandQuat(bindWorldRotations[boneIndex]))
        );
    }

    gLcsLastBasisMap = basisMap;
    gLcsLastRawBindPositions = rawBindPositions;
    gLcsLastAnimatedRawPositions = animatedRaw;
    gLcsLastBindWorldRotations = bindWorldRotations;
    gLcsLastAnimatedWorldRotations = animatedWorldRotations;
    gLcsRawHierarchySkinStateValid =
        basisMap.valid &&
        gLcsLastRawBindPositions.size() == bones.size() &&
        gLcsLastAnimatedRawPositions.size() == bones.size() &&
        gLcsLastBindWorldRotations.size() == bones.size() &&
        gLcsLastAnimatedWorldRotations.size() == bones.size();

    gLastAnimatedBoneRawPositions = animatedPreview;
    return animatedPreview;
}


static std::vector<StorylandModelPoint> buildAnimatedModelBonePositions() {
    const auto& bones = gModelFile.armatureBones();
    const auto& sourcePoints = gModelFile.previewPoints();

    bool bindPositionsAlreadyInPreviewSpace = modelUsesPreviewSpaceBindPositions(bones);

    StorylandPreviewBindAffine affine;
    if (!bindPositionsAlreadyInPreviewSpace) {
        computePreviewBindAffine(sourcePoints, affine);
    }

    std::vector<StorylandModelPoint> animatedRaw;
    animatedRaw.reserve(bones.size());

    for (const auto& bone : bones) {
        animatedRaw.push_back(rawBoneBindPosition(bone));
    }

    gLastAnimatedBoneRawPositions = animatedRaw;

    std::vector<StorylandQuat> bindWorldRotations(bones.size());
    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const StorylandModelBone& bone = bones[boneIndex];

        StorylandQuat localBindRotation = modelBoneBindLocalRotation(bone);
        StorylandQuat bindWorldRotation = localBindRotation;

        if (modelBoneWorldPositionIsUseful(bone) && bone.hasWorldRotation) {
            bindWorldRotation = modelBoneBindWorldRotation(bone);
        } else if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < boneIndex) {


            bindWorldRotation = multiplyStorylandQuat(bindWorldRotations[size_t(bone.parentIndex)], localBindRotation);
        }

        bindWorldRotations[boneIndex] = normalizeStorylandQuat(bindWorldRotation);
    }

    std::vector<StorylandModelPoint> animatedPreview;
    animatedPreview.reserve(animatedRaw.size());
    for (const StorylandModelPoint& rawPoint : animatedRaw) {
        animatedPreview.push_back(bindPositionsAlreadyInPreviewSpace ? rawPoint : rawBindPointToPreviewSpace(affine, rawPoint));
    }

    if (!gModelAnimLoaded || !gAnimFile.hasDecodedMotion() || bones.empty()) {
        gLastAnimatedBoneWorldRotations.assign(bones.size(), StorylandQuat{});
        return animatedPreview;
    }

    if (currentModelLooksLikeLcsPedSkeleton(bones)) {


        return buildLcsStrictLocalHierarchyAnimatedBonePositions();
    }

    std::vector<StorylandQuat> worldRotations(bones.size());
    for (StorylandQuat& rotation : worldRotations) {
        rotation = {};
    }
    gLastAnimatedBoneWorldRotations.assign(bones.size(), StorylandQuat{});
    std::vector<bool> solved(bones.size(), false);

    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const StorylandModelBone& bone = bones[boneIndex];

        StorylandModelPoint bindWorld = rawBoneBindPosition(bone);
        StorylandModelPoint bindParentWorld{};
        StorylandModelPoint localOffset = bindWorld;

        StorylandQuat parentRotation{};
        StorylandModelPoint parentAnimatedRaw{};

        if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
            size_t parentIndex = size_t(bone.parentIndex);
            bindParentWorld = rawBoneBindPosition(bones[parentIndex]);
            localOffset = subtractStorylandPoint(bindWorld, bindParentWorld);
            parentRotation = worldRotations[parentIndex];
            parentAnimatedRaw = animatedRaw[parentIndex];
        }

        if (animPreviewShouldFreezeBoneAtBindPose(bone)) {
            worldRotations[boneIndex] = {};
            animatedRaw[boneIndex] = bindWorld;
            solved[boneIndex] = true;
            continue;
        }

        StorylandQuat bindBasisDeltaRotation{};
        StorylandModelPoint localDeltaTranslation{};
        const StorylandAnimTrack* track = findAnimTrackForModelBone(boneIndex, bone);
        if (track) {
            bool useLcsAbsoluteWeaponPose =
                currentAttachedAnimLooksWeaponLike() &&
                currentModelLooksLikeLcsPedSkeleton(gModelFile.armatureBones());

            if (currentAttachedAnimLooksWeaponLike() && !useLcsAbsoluteWeaponPose) {


                StorylandQuat localLayerDeltaRotation{};
                if (sampleAnimTrackDeltaTransform(*track, gAnimCurrentTime, localLayerDeltaRotation, localDeltaTranslation)) {
                    bindBasisDeltaRotation = rebaseLocalDeltaToExplicitBindWorldBasis(
                        localLayerDeltaRotation,
                        bindWorldRotations[boneIndex]
                    );
                }
            } else {


                StorylandQuat localPoseRotation = modelBoneBindLocalRotation(bone);
                StorylandModelPoint sampledTranslation{};
                if (sampleAnimTrackTransform(*track, gAnimCurrentTime, localPoseRotation, sampledTranslation)) {
                    if (!useLcsAbsoluteWeaponPose) {
                        localDeltaTranslation = subtractStorylandPoint(sampledTranslation, bone.localPosition);
                    } else {


                        localDeltaTranslation = {};
                    }
                }

                StorylandQuat bindLocalRotation = modelBoneBindLocalRotation(bone);
                bool useLcsAbsolutePoseDeltaOrder =
                    currentModelLooksLikeLcsPedSkeleton(gModelFile.armatureBones());

                StorylandQuat sourceDelta{};
                if (useLcsAbsolutePoseDeltaOrder) {


                    sourceDelta = multiplyStorylandQuat(
                        normalizeStorylandQuat(localPoseRotation),
                        conjugateStorylandQuat(bindLocalRotation)
                    );
                } else {
                    sourceDelta = multiplyStorylandQuat(
                        conjugateStorylandQuat(bindLocalRotation),
                        normalizeStorylandQuat(localPoseRotation)
                    );
                }
                bindBasisDeltaRotation = rebaseLocalDeltaToExplicitBindWorldBasis(
                    sourceDelta,
                    bindWorldRotations[boneIndex]
                );
            }
        }

        if (animPreviewShouldLockRootTranslation(bone)) {
            localDeltaTranslation = {};
        }

        if (animPreviewShouldLockRootRotation(bone)) {
            bindBasisDeltaRotation = {};
        }

        bindBasisDeltaRotation = clampPreviewDeltaRotation(bindBasisDeltaRotation, maxPreviewRotationForBone(bone));

        StorylandQuat worldRotation = bindBasisDeltaRotation;
        if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
            worldRotation = multiplyStorylandQuat(parentRotation, bindBasisDeltaRotation);
        }
        worldRotations[boneIndex] = worldRotation;
        solved[boneIndex] = true;

        const float translationPreviewScale = 0.15f;

        if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
            StorylandModelPoint rotatedOffset = rotatePointByQuat(parentRotation, localOffset);
            StorylandModelPoint translated = scaleStorylandPoint(localDeltaTranslation, translationPreviewScale);

            float localLength = std::max(0.001f, lengthStorylandPoint(localOffset));
            float maxTranslatedLength = localLength * 0.75f;
            float translatedLength = lengthStorylandPoint(translated);
            if (translatedLength > maxTranslatedLength) {
                translated = scaleStorylandPoint(translated, maxTranslatedLength / translatedLength);
            }

            animatedRaw[boneIndex] = addStorylandPoint(addStorylandPoint(parentAnimatedRaw, rotatedOffset), translated);
        } else {
            StorylandModelPoint translated = scaleStorylandPoint(localDeltaTranslation, translationPreviewScale);
            animatedRaw[boneIndex] = addStorylandPoint(bindWorld, translated);
        }
    }

    gLastAnimatedBoneWorldRotations = worldRotations;
    gLastAnimatedBoneRawPositions = animatedRaw;

    animatedPreview.clear();
    animatedPreview.reserve(animatedRaw.size());
    for (const StorylandModelPoint& rawPoint : animatedRaw) {
        animatedPreview.push_back(bindPositionsAlreadyInPreviewSpace ? rawPoint : rawBindPointToPreviewSpace(affine, rawPoint));
    }

    return animatedPreview;
}


static StorylandModelPoint displayAnimatedModelBonePosition(size_t boneIndex) {
    const auto& bones = gModelFile.armatureBones();
    if (boneIndex >= bones.size()) return StorylandModelPoint{};

    if (!gModelAnimLoaded || !gAnimFile.hasDecodedMotion()) {
        return displayBonePosition(bones[boneIndex]);
    }

    std::vector<StorylandModelPoint> animated = buildAnimatedModelBonePositions();
    if (boneIndex >= animated.size()) return displayBonePosition(bones[boneIndex]);
    return animated[boneIndex];
}

static float dotStorylandPoint(const StorylandModelPoint& a, const StorylandModelPoint& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float lengthSqStorylandPoint(const StorylandModelPoint& a) {
    return dotStorylandPoint(a, a);
}

static float lengthStorylandPoint(const StorylandModelPoint& a) {
    return std::sqrt(std::max(0.0f, lengthSqStorylandPoint(a)));
}

static StorylandModelPoint crossStorylandPoint(const StorylandModelPoint& a, const StorylandModelPoint& b) {
    return StorylandModelPoint{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static StorylandModelPoint normalizeStorylandPoint(const StorylandModelPoint& a, const StorylandModelPoint& fallback) {
    float length = lengthStorylandPoint(a);
    if (!std::isfinite(length) || length < 0.00001f) return fallback;
    return scaleStorylandPoint(a, 1.0f / length);
}

static StorylandModelPoint choosePerpendicularAxis(const StorylandModelPoint& axis) {
    StorylandModelPoint xAxis{1.0f, 0.0f, 0.0f};
    StorylandModelPoint yAxis{0.0f, 1.0f, 0.0f};
    StorylandModelPoint zAxis{0.0f, 0.0f, 1.0f};

    StorylandModelPoint candidate = crossStorylandPoint(axis, zAxis);
    if (lengthSqStorylandPoint(candidate) > 0.0001f) return normalizeStorylandPoint(candidate, xAxis);

    candidate = crossStorylandPoint(axis, yAxis);
    if (lengthSqStorylandPoint(candidate) > 0.0001f) return normalizeStorylandPoint(candidate, xAxis);

    return xAxis;
}

static StorylandModelPoint mapPointFromBindSegmentToAnimatedSegment(
    const StorylandModelPoint& point,
    const StorylandModelPoint& bindA,
    const StorylandModelPoint& bindB,
    const StorylandModelPoint& animA,
    const StorylandModelPoint& animB
) {
    StorylandModelPoint bindVector = subtractStorylandPoint(bindB, bindA);
    StorylandModelPoint animVector = subtractStorylandPoint(animB, animA);

    float bindLength = std::max(0.0001f, lengthStorylandPoint(bindVector));
    float animLength = std::max(0.0001f, lengthStorylandPoint(animVector));

    StorylandModelPoint bindAxis = normalizeStorylandPoint(bindVector, StorylandModelPoint{0.0f, 0.0f, 1.0f});
    StorylandModelPoint animAxis = normalizeStorylandPoint(animVector, bindAxis);

    StorylandModelPoint bindSide = choosePerpendicularAxis(bindAxis);
    StorylandModelPoint bindUp = normalizeStorylandPoint(crossStorylandPoint(bindAxis, bindSide), StorylandModelPoint{0.0f, 1.0f, 0.0f});

    StorylandModelPoint animSide = choosePerpendicularAxis(animAxis);
    StorylandModelPoint animUp = normalizeStorylandPoint(crossStorylandPoint(animAxis, animSide), StorylandModelPoint{0.0f, 1.0f, 0.0f});

    StorylandModelPoint relative = subtractStorylandPoint(point, bindA);
    float along = dotStorylandPoint(relative, bindAxis);
    float side = dotStorylandPoint(relative, bindSide);
    float up = dotStorylandPoint(relative, bindUp);

    float normalizedAlong = along / bindLength;
    StorylandModelPoint animatedBase = addStorylandPoint(animA, scaleStorylandPoint(animAxis, normalizedAlong * animLength));

    return addStorylandPoint(
        addStorylandPoint(animatedBase, scaleStorylandPoint(animSide, side)),
        scaleStorylandPoint(animUp, up)
    );
}

static float distanceSqPointToSegment(
    const StorylandModelPoint& point,
    const StorylandModelPoint& a,
    const StorylandModelPoint& b,
    float& outT
) {
    StorylandModelPoint ab = subtractStorylandPoint(b, a);
    float abLengthSq = lengthSqStorylandPoint(ab);
    if (abLengthSq < 0.000001f) {
        outT = 0.0f;
        return lengthSqStorylandPoint(subtractStorylandPoint(point, a));
    }

    outT = dotStorylandPoint(subtractStorylandPoint(point, a), ab) / abLengthSq;
    outT = std::max(0.0f, std::min(1.0f, outT));

    StorylandModelPoint closest = addStorylandPoint(a, scaleStorylandPoint(ab, outT));
    return lengthSqStorylandPoint(subtractStorylandPoint(point, closest));
}

static bool boneSegmentIsUsefulForMeshDeform(const StorylandModelBone& bone, const StorylandModelBone& parentBone) {
    if (!boneHasVisiblePreviewPosition(bone)) return false;
    if (!boneHasVisiblePreviewPosition(parentBone)) return false;

    std::string name = normalizeBoneLookupName(bone.name);
    if (name == "root" || name == "pivots" || name == "male_base" || name == "female_base" || name == "scene_root") {
        return false;
    }

    return true;
}


static bool modelHasUsableTrueSkinWeights(const std::vector<StorylandModelPoint>& sourcePoints) {
    const auto& skinWeights = gModelFile.previewSkinWeights();
    if (skinWeights.size() != sourcePoints.size() || skinWeights.empty()) {
        return false;
    }

    uint32_t validCount = 0;
    for (const auto& weights : skinWeights) {
        if (weights.valid && weights.influenceCount > 0u) {
            validCount++;
        }
    }

    return validCount > 0u;
}

enum class StorylandPedSkinPaletteKind {
    CanonicalVcs,
    CanonicalLcs,
    ImportedHierarchy,
};

static const wchar_t* pedSkinPaletteKindName(StorylandPedSkinPaletteKind kind) {
    switch (kind) {
    case StorylandPedSkinPaletteKind::CanonicalVcs: return L"VCS canonical jaw palette";
    case StorylandPedSkinPaletteKind::CanonicalLcs: return L"LCS canonical no-jaw palette";
    case StorylandPedSkinPaletteKind::ImportedHierarchy: return L"imported hierarchy order";
    default: break;
    }

    return L"unknown";
}

static const char* lcsPedSkinPaletteNameFromIndex(uint32_t skinPaletteIndex) {


    static const char* names[] = {
        "root",
        "pelvis",
        "spine",
        "spine1",
        "neck",
        "head",
        "bip01_l_clavicle",
        "l_upperarm",
        "l_forearm",
        "l_hand",
        "l_finger",
        "bip01_r_clavicle",
        "r_upperarm",
        "r_forearm",
        "r_hand",
        "r_finger",
        "l_thigh",
        "l_calf",
        "l_foot",
        "l_toe0",
        "r_thigh",
        "r_calf",
        "r_foot",
        "r_toe0",
    };

    if (skinPaletteIndex >= (sizeof(names) / sizeof(names[0]))) {
        return nullptr;
    }

    return names[skinPaletteIndex];
}

static const char* vcsPedSkinPaletteNameFromIndex(uint32_t skinPaletteIndex) {
    static const char* names[] = {
        "root",
        "pelvis",
        "spine",
        "spine1",
        "neck",
        "head",
        "jaw",
        "bip01_l_clavicle",
        "l_upperarm",
        "l_forearm",
        "l_hand",
        "l_finger",
        "bip01_r_clavicle",
        "r_upperarm",
        "r_forearm",
        "r_hand",
        "r_finger",
        "l_thigh",
        "l_calf",
        "l_foot",
        "l_toe0",
        "r_thigh",
        "r_calf",
        "r_foot",
        "r_toe0",
    };

    if (skinPaletteIndex >= (sizeof(names) / sizeof(names[0]))) {
        return nullptr;
    }

    return names[skinPaletteIndex];
}

static const char* pedSkinPaletteNameFromIndex(
    uint32_t skinPaletteIndex,
    StorylandPedSkinPaletteKind kind,
    const std::vector<StorylandModelBone>& bones
) {
    if (kind == StorylandPedSkinPaletteKind::CanonicalVcs) {
        return vcsPedSkinPaletteNameFromIndex(skinPaletteIndex);
    }

    if (kind == StorylandPedSkinPaletteKind::CanonicalLcs) {
        return lcsPedSkinPaletteNameFromIndex(skinPaletteIndex);
    }

    if (kind == StorylandPedSkinPaletteKind::ImportedHierarchy) {
        if (skinPaletteIndex < bones.size()) {
            return bones[skinPaletteIndex].name.c_str();
        }

        return nullptr;
    }

    return nullptr;
}

static bool currentModelLooksLikeLcsPedSkeleton(const std::vector<StorylandModelBone>& bones) {
    bool hasJaw = false;
    bool hasLcsToeId = false;
    bool hasVcsDisplayToeId = false;
    bool hasZeroWorldSkeleton = false;
    uint32_t usefulWorldPositions = 0;

    for (const StorylandModelBone& bone : bones) {
        std::string name = normalizeBoneLookupName(bone.name);
        uint32_t directId = bleedsDirectIdForModelBone(bone);

        if (name == "jaw" || directId == 8u) hasJaw = true;
        if (directId == 54u || directId == 55u || bone.boneId == 54u || bone.boneId == 55u) hasLcsToeId = true;
        if (directId == 2000u || directId == 2001u || bone.boneId == 2000u || bone.boneId == 2001u) hasVcsDisplayToeId = true;
        if (modelBoneWorldPositionIsUseful(bone)) usefulWorldPositions++;
    }

    if (!bones.empty() && usefulWorldPositions == 0u) {
        hasZeroWorldSkeleton = true;
    }


    if (hasLcsToeId && !hasJaw) return true;


    if (bones.size() == 24u && !hasJaw && !hasVcsDisplayToeId) return true;


    if (hasZeroWorldSkeleton && !hasJaw) return true;

    return false;
}

static StorylandPedSkinPaletteKind detectPedSkinPaletteKindForCurrentModel(
    const std::vector<StorylandModelBone>& bones
) {
    if (currentModelLooksLikeLcsPedSkeleton(bones)) {
        return StorylandPedSkinPaletteKind::CanonicalLcs;
    }

    return StorylandPedSkinPaletteKind::CanonicalVcs;
}

static uint32_t findModelBoneIndexForSkinPaletteIndex(
    uint32_t skinPaletteIndex,
    const std::vector<StorylandModelBone>& bones,
    StorylandPedSkinPaletteKind kind
) {
    const char* paletteName = pedSkinPaletteNameFromIndex(skinPaletteIndex, kind, bones);
    if (paletteName == nullptr) {
        return 0xFFFFFFFFu;
    }

    if (kind == StorylandPedSkinPaletteKind::ImportedHierarchy) {
        if (skinPaletteIndex < bones.size()) {
            return skinPaletteIndex;
        }

        return 0xFFFFFFFFu;
    }

    std::string targetName = normalizeBoneLookupName(paletteName);

    for (uint32_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        if (normalizeBoneLookupName(bones[boneIndex].name) == targetName) {
            return boneIndex;
        }
    }

    uint32_t targetDirectId = bleedsDirectIdFromNormalizedBoneName(targetName);
    if (targetDirectId != 0xFFFFFFFFu) {
        for (uint32_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
            if (bleedsDirectIdForModelBone(bones[boneIndex]) == targetDirectId) {
                return boneIndex;
            }
        }
    }

    return 0xFFFFFFFFu;
}

static float distancePointToBoneSegment(
    const StorylandModelPoint& point,
    uint32_t modelBoneIndex,
    const std::vector<StorylandModelBone>& bones
) {
    if (modelBoneIndex >= bones.size()) {
        return 1000000.0f;
    }

    StorylandModelPoint bonePosition = displayBonePosition(bones[modelBoneIndex]);
    StorylandModelPoint segmentStart = bonePosition;
    StorylandModelPoint segmentEnd = bonePosition;

    const StorylandModelBone& bone = bones[modelBoneIndex];
    if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
        segmentStart = displayBonePosition(bones[bone.parentIndex]);
    } else {
        for (const StorylandModelBone& child : bones) {
            if (child.parentIndex == modelBoneIndex) {
                segmentEnd = displayBonePosition(child);
                break;
            }
        }
    }

    StorylandModelPoint segment = subtractStorylandPoint(segmentEnd, segmentStart);
    float segmentLengthSq = segment.x * segment.x + segment.y * segment.y + segment.z * segment.z;

    if (segmentLengthSq <= 0.0000001f) {
        return lengthStorylandPoint(subtractStorylandPoint(point, bonePosition));
    }

    StorylandModelPoint pointDelta = subtractStorylandPoint(point, segmentStart);
    float t = (pointDelta.x * segment.x + pointDelta.y * segment.y + pointDelta.z * segment.z) / segmentLengthSq;
    t = std::max(0.0f, std::min(1.0f, t));

    StorylandModelPoint closest;
    closest.x = segmentStart.x + segment.x * t;
    closest.y = segmentStart.y + segment.y * t;
    closest.z = segmentStart.z + segment.z * t;

    return lengthStorylandPoint(subtractStorylandPoint(point, closest));
}

static float scorePedSkinPaletteKind(
    StorylandPedSkinPaletteKind kind,
    const std::vector<StorylandModelBone>& bones,
    const std::vector<StorylandModelSkinWeights>& skinWeights,
    const std::vector<StorylandModelPoint>& sourcePoints
) {
    if (bones.empty() || skinWeights.empty() || sourcePoints.empty() || skinWeights.size() != sourcePoints.size()) {
        return 1000000000.0f;
    }

    float score = 0.0f;
    float totalWeight = 0.0f;
    uint32_t usableInfluences = 0;
    uint32_t rejectedInfluences = 0;

    for (size_t vertexIndex = 0; vertexIndex < sourcePoints.size(); ++vertexIndex) {
        const StorylandModelSkinWeights& weights = skinWeights[vertexIndex];
        if (!weights.valid || weights.influenceCount == 0u) {
            continue;
        }

        for (uint32_t influenceIndex = 0; influenceIndex < weights.influenceCount; ++influenceIndex) {
            const StorylandModelSkinInfluence& influence = weights.influences[influenceIndex];
            if (influence.weight <= 0.00001f) {
                continue;
            }

            uint32_t modelBoneIndex = findModelBoneIndexForSkinPaletteIndex(influence.boneIndex, bones, kind);
            if (modelBoneIndex == 0xFFFFFFFFu || modelBoneIndex >= bones.size()) {
                rejectedInfluences++;
                score += 1000.0f * influence.weight;
                totalWeight += influence.weight;
                continue;
            }

            float distance = distancePointToBoneSegment(sourcePoints[vertexIndex], modelBoneIndex, bones);
            if (!std::isfinite(distance)) {
                distance = 1000.0f;
            }

            score += distance * influence.weight;
            totalWeight += influence.weight;
            usableInfluences++;
        }
    }

    if (usableInfluences == 0u || totalWeight <= 0.00001f) {
        return 1000000000.0f;
    }

    float normalizedScore = score / totalWeight;
    normalizedScore += float(rejectedInfluences) * 10.0f;
    return normalizedScore;
}

static StorylandPedSkinPaletteKind chooseBestPedSkinPaletteKind(
    const std::vector<StorylandModelBone>& bones,
    const std::vector<StorylandModelSkinWeights>& skinWeights,
    const std::vector<StorylandModelPoint>& sourcePoints
) {
    StorylandPedSkinPaletteKind bestKind = StorylandPedSkinPaletteKind::CanonicalVcs;
    float bestScore = scorePedSkinPaletteKind(bestKind, bones, skinWeights, sourcePoints);

    const StorylandPedSkinPaletteKind candidates[] = {
        StorylandPedSkinPaletteKind::CanonicalLcs,
        StorylandPedSkinPaletteKind::ImportedHierarchy,
    };

    for (StorylandPedSkinPaletteKind candidate : candidates) {
        float candidateScore = scorePedSkinPaletteKind(candidate, bones, skinWeights, sourcePoints);
        if (candidateScore < bestScore) {
            bestScore = candidateScore;
            bestKind = candidate;
        }
    }

    return bestKind;
}


static bool modelTextureSetLooksLikeCustomBleedsPedExport() {
    bool hasCustomPedTextureName = false;

    auto inspectName = [&](const std::string& rawName) {
        std::string name = asciiLower(rawName);

        if (name.find("csplr") != std::string::npos ||
            name.find("plr12") != std::string::npos ||
            name.find("cj") != std::string::npos ||
            name.find("cj_") != std::string::npos ||
            name.find("_cj") != std::string::npos ||
            name.find("custom") != std::string::npos ||
            name.find("bleeds") != std::string::npos) {
            hasCustomPedTextureName = true;
        }
    };

    inspectName(gModelTextureName);
    for (const StorylandModelTextureRegion& region : gModelTextureRegions) {
        inspectName(region.name);
    }

    return hasCustomPedTextureName;
}

static bool shouldUseCustomExportMatrixSkinPreview() {
    return gCustomExportMatrixSkinPreview || modelTextureSetLooksLikeCustomBleedsPedExport();
}

struct StorylandPreviewSkinMatrix {
    bool valid = false;
    StorylandQuat bindRotation;
    StorylandQuat animatedRotation;
    StorylandModelPoint bindPosition;
    StorylandModelPoint animatedPosition;
};

static StorylandModelPoint transformPreviewPointBySkinMatrix(
    const StorylandModelPoint& bindPreviewPoint,
    const StorylandPreviewSkinMatrix& matrix
) {
    StorylandModelPoint bindLocal = rotatePointByQuat(
        conjugateStorylandQuat(matrix.bindRotation),
        subtractStorylandPoint(bindPreviewPoint, matrix.bindPosition)
    );

    return addStorylandPoint(
        matrix.animatedPosition,
        rotatePointByQuat(matrix.animatedRotation, bindLocal)
    );
}

static bool buildPreviewSkinMatrixPaletteFromCurrentHAnimSkeleton(
    const std::vector<StorylandModelPoint>& animatedPreviewBones,
    std::vector<StorylandPreviewSkinMatrix>& palette
) {
    palette.clear();

    const auto& bones = gModelFile.armatureBones();
    if (bones.empty() || animatedPreviewBones.size() != bones.size()) {
        return false;
    }

    if (gLastAnimatedBoneWorldRotations.size() != bones.size()) {
        return false;
    }

    palette.resize(bones.size());

    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const StorylandModelBone& bone = bones[boneIndex];

        StorylandQuat bindRotation = modelBoneBindWorldRotation(bone);
        if (!bone.hasWorldRotation) {
            if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < boneIndex && palette[size_t(bone.parentIndex)].valid) {
                bindRotation = multiplyStorylandQuat(
                    palette[size_t(bone.parentIndex)].bindRotation,
                    modelBoneBindLocalRotation(bone)
                );
            } else {
                bindRotation = modelBoneBindLocalRotation(bone);
            }
        }

        bindRotation = normalizeStorylandQuat(bindRotation);

        StorylandQuat animatedDelta = gLastAnimatedBoneWorldRotations[boneIndex];
        animatedDelta = normalizeStorylandQuat(animatedDelta);

        StorylandPreviewSkinMatrix matrix;
        matrix.valid = true;
        matrix.bindRotation = bindRotation;
        matrix.animatedRotation = multiplyStorylandQuat(animatedDelta, bindRotation);
        matrix.bindPosition = displayBonePosition(bone);
        matrix.animatedPosition = animatedPreviewBones[boneIndex];

        palette[boneIndex] = matrix;
    }

    return true;
}

static float customExportMaxPreviewRotationForBone(const StorylandModelBone& bone) {
    std::string name = normalizeBoneLookupName(bone.name);


    if (name == "root" || name == "pelvis") {
        return 1.60f;
    }

    if (name == "spine" || name == "spine1" || name == "neck" || name == "head" || name == "jaw") {
        return 1.45f;
    }

    if (name.find("clavicle") != std::string::npos ||
        name.find("upperarm") != std::string::npos ||
        name.find("forearm") != std::string::npos ||
        name.find("hand") != std::string::npos ||
        name.find("finger") != std::string::npos) {
        return 1.85f;
    }

    if (name.find("thigh") != std::string::npos ||
        name.find("calf") != std::string::npos ||
        name.find("foot") != std::string::npos ||
        name.find("toe") != std::string::npos) {
        return 1.65f;
    }

    return 1.50f;
}

static std::vector<StorylandModelPoint> buildCustomExportDeltaAnimatedModelBonePositions() {
    const auto& bones = gModelFile.armatureBones();
    const auto& sourcePoints = gModelFile.previewPoints();

    bool bindPositionsAlreadyInPreviewSpace = modelUsesPreviewSpaceBindPositions(bones);

    StorylandPreviewBindAffine affine;
    if (!bindPositionsAlreadyInPreviewSpace) {
        computePreviewBindAffine(sourcePoints, affine);
    }

    std::vector<StorylandModelPoint> animatedRaw;
    animatedRaw.reserve(bones.size());

    for (const auto& bone : bones) {
        animatedRaw.push_back(rawBoneBindPosition(bone));
    }

    gLastAnimatedBoneRawPositions = animatedRaw;

    std::vector<StorylandModelPoint> animatedPreview;
    animatedPreview.reserve(animatedRaw.size());
    for (const StorylandModelPoint& rawPoint : animatedRaw) {
        animatedPreview.push_back(bindPositionsAlreadyInPreviewSpace ? rawPoint : rawBindPointToPreviewSpace(affine, rawPoint));
    }

    if (!gModelAnimLoaded || !gAnimFile.hasDecodedMotion() || bones.empty()) {
        gLastAnimatedBoneWorldRotations.assign(bones.size(), StorylandQuat{});
        return animatedPreview;
    }

    std::vector<StorylandQuat> worldRotations(bones.size());
    for (StorylandQuat& rotation : worldRotations) {
        rotation = {};
    }
    gLastAnimatedBoneWorldRotations.assign(bones.size(), StorylandQuat{});
    std::vector<bool> solved(bones.size(), false);

    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const StorylandModelBone& bone = bones[boneIndex];

        StorylandModelPoint bindWorld = rawBoneBindPosition(bone);
        StorylandModelPoint bindParentWorld{};
        StorylandModelPoint localOffset = bindWorld;

        StorylandQuat parentRotation{};
        StorylandModelPoint parentAnimatedRaw{};

        if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
            size_t parentIndex = size_t(bone.parentIndex);
            bindParentWorld = rawBoneBindPosition(bones[parentIndex]);
            localOffset = subtractStorylandPoint(bindWorld, bindParentWorld);
            parentRotation = worldRotations[parentIndex];
            parentAnimatedRaw = animatedRaw[parentIndex];
        }

        StorylandQuat bindBasisDeltaRotation{};
        StorylandModelPoint localDeltaTranslation{};
        const StorylandAnimTrack* track = findAnimTrackForModelBone(boneIndex, bone);
        if (track) {


            StorylandQuat localTrackDeltaRotation{};
            if (sampleAnimTrackDeltaTransform(*track, gAnimCurrentTime, localTrackDeltaRotation, localDeltaTranslation)) {
                bindBasisDeltaRotation = rebaseLocalDeltaToBindWorldBasis(localTrackDeltaRotation, bone);
            }
        }

        if (animPreviewShouldLockRootTranslation(bone)) {
            localDeltaTranslation = {};
        }

        bindBasisDeltaRotation = clampPreviewDeltaRotation(bindBasisDeltaRotation, customExportMaxPreviewRotationForBone(bone));

        StorylandQuat worldRotation = bindBasisDeltaRotation;
        if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
            worldRotation = multiplyStorylandQuat(parentRotation, bindBasisDeltaRotation);
        }
        worldRotations[boneIndex] = worldRotation;
        solved[boneIndex] = true;

        const float translationPreviewScale = 0.15f;

        if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
            StorylandModelPoint rotatedOffset = rotatePointByQuat(parentRotation, localOffset);
            StorylandModelPoint translated = scaleStorylandPoint(localDeltaTranslation, translationPreviewScale);

            float localLength = std::max(0.001f, lengthStorylandPoint(localOffset));
            float maxTranslatedLength = localLength * 0.75f;
            float translatedLength = lengthStorylandPoint(translated);
            if (translatedLength > maxTranslatedLength) {
                translated = scaleStorylandPoint(translated, maxTranslatedLength / translatedLength);
            }

            animatedRaw[boneIndex] = addStorylandPoint(addStorylandPoint(parentAnimatedRaw, rotatedOffset), translated);
        } else {
            StorylandModelPoint translated = scaleStorylandPoint(localDeltaTranslation, translationPreviewScale);
            animatedRaw[boneIndex] = addStorylandPoint(bindWorld, translated);
        }
    }

    gLastAnimatedBoneWorldRotations = worldRotations;
    gLastAnimatedBoneRawPositions = animatedRaw;

    animatedPreview.clear();
    animatedPreview.reserve(animatedRaw.size());
    for (const StorylandModelPoint& rawPoint : animatedRaw) {
        animatedPreview.push_back(bindPositionsAlreadyInPreviewSpace ? rawPoint : rawBindPointToPreviewSpace(affine, rawPoint));
    }

    return animatedPreview;
}


static std::vector<StorylandModelPoint> buildCustomExportMatrixSkinnedPreviewPoints(
    const std::vector<StorylandModelPoint>& sourcePoints
) {
    const auto& bones = gModelFile.armatureBones();
    const auto& skinWeights = gModelFile.previewSkinWeights();

    if (bones.empty() || skinWeights.size() != sourcePoints.size()) {
        return sourcePoints;
    }


    std::vector<StorylandModelPoint> animatedPreviewBones = buildCustomExportDeltaAnimatedModelBonePositions();
    if (animatedPreviewBones.size() != bones.size()) {
        return sourcePoints;
    }

    std::vector<StorylandPreviewSkinMatrix> matrixPalette;
    if (!buildPreviewSkinMatrixPaletteFromCurrentHAnimSkeleton(animatedPreviewBones, matrixPalette)) {
        return sourcePoints;
    }

    StorylandPedSkinPaletteKind skinPaletteKind = chooseBestPedSkinPaletteKind(bones, skinWeights, sourcePoints);

    StorylandModelPoint meshMin = sourcePoints.front();
    StorylandModelPoint meshMax = sourcePoints.front();
    for (const StorylandModelPoint& point : sourcePoints) {
        meshMin.x = std::min(meshMin.x, point.x);
        meshMin.y = std::min(meshMin.y, point.y);
        meshMin.z = std::min(meshMin.z, point.z);
        meshMax.x = std::max(meshMax.x, point.x);
        meshMax.y = std::max(meshMax.y, point.y);
        meshMax.z = std::max(meshMax.z, point.z);
    }

    float meshSpan = std::max(meshMax.x - meshMin.x, std::max(meshMax.y - meshMin.y, meshMax.z - meshMin.z));
    float maxTrueSkinPreviewDisplacement = std::max(0.02f, meshSpan * 4.0f);

    std::vector<StorylandModelPoint> animatedPoints;
    animatedPoints.reserve(sourcePoints.size());

    uint32_t validSkinnedVertices = 0;

    for (size_t vertexIndex = 0; vertexIndex < sourcePoints.size(); ++vertexIndex) {
        const StorylandModelPoint& bindPoint = sourcePoints[vertexIndex];
        const StorylandModelSkinWeights& weights = skinWeights[vertexIndex];

        if (!weights.valid || weights.influenceCount == 0u) {
            animatedPoints.push_back(bindPoint);
            continue;
        }

        StorylandModelPoint blendedPoint{};
        float totalWeight = 0.0f;

        for (uint32_t influenceIndex = 0; influenceIndex < weights.influenceCount; ++influenceIndex) {
            const StorylandModelSkinInfluence& influence = weights.influences[influenceIndex];
            if (influence.weight <= 0.00001f) continue;

            uint32_t modelBoneIndex = findModelBoneIndexForSkinPaletteIndex(influence.boneIndex, bones, skinPaletteKind);
            if (modelBoneIndex == 0xFFFFFFFFu || modelBoneIndex >= bones.size()) continue;

            const StorylandPreviewSkinMatrix& matrix = matrixPalette[modelBoneIndex];
            if (!matrix.valid) continue;

            StorylandModelPoint transformedPoint = transformPreviewPointBySkinMatrix(bindPoint, matrix);

            blendedPoint.x += transformedPoint.x * influence.weight;
            blendedPoint.y += transformedPoint.y * influence.weight;
            blendedPoint.z += transformedPoint.z * influence.weight;
            totalWeight += influence.weight;
        }

        if (totalWeight <= 0.00001f ||
            !std::isfinite(blendedPoint.x) ||
            !std::isfinite(blendedPoint.y) ||
            !std::isfinite(blendedPoint.z)) {
            animatedPoints.push_back(bindPoint);
            continue;
        }

        if (std::fabs(totalWeight - 1.0f) > 0.0001f) {
            blendedPoint.x /= totalWeight;
            blendedPoint.y /= totalWeight;
            blendedPoint.z /= totalWeight;
        }

        StorylandModelPoint displacement = subtractStorylandPoint(blendedPoint, bindPoint);
        float displacementLength = lengthStorylandPoint(displacement);
        if (displacementLength > maxTrueSkinPreviewDisplacement) {
            blendedPoint = addStorylandPoint(
                bindPoint,
                scaleStorylandPoint(displacement, maxTrueSkinPreviewDisplacement / displacementLength)
            );
        }

        animatedPoints.push_back(blendedPoint);
        validSkinnedVertices++;
    }

    if (validSkinnedVertices == 0u) {
        return sourcePoints;
    }

    return animatedPoints;
}

static StorylandModelPoint transformPreviewPointByWeightedBone(
    const StorylandModelPoint& previewPoint,
    const StorylandModelBone& bone,
    const StorylandModelPoint& animatedPreviewBonePosition,
    const StorylandQuat& animatedBoneWorldDeltaRotation
) {
    StorylandModelPoint bindPreviewBonePosition = displayBonePosition(bone);
    StorylandModelPoint localPoint = subtractStorylandPoint(previewPoint, bindPreviewBonePosition);
    StorylandModelPoint rotatedLocalPoint = rotatePointByQuat(animatedBoneWorldDeltaRotation, localPoint);
    return addStorylandPoint(animatedPreviewBonePosition, rotatedLocalPoint);
}

static bool lcsWeaponPreviewBoneShouldUseParentPivot(const StorylandModelBone& bone) {
    if (!currentAttachedAnimLooksWeaponLike()) return false;
    if (!currentModelLooksLikeLcsPedSkeleton(gModelFile.armatureBones())) return false;

    std::string name = normalizeBoneLookupName(bone.name);
    return name.find("clavicle") != std::string::npos ||
           name.find("upperarm") != std::string::npos ||
           name.find("forearm") != std::string::npos ||
           name.find("hand") != std::string::npos ||
           name.find("finger") != std::string::npos;
}

static uint32_t lcsWeaponPreviewPivotBoneIndex(
    uint32_t animatedBoneIndex,
    const std::vector<StorylandModelBone>& bones
) {
    if (animatedBoneIndex >= bones.size()) return animatedBoneIndex;
    const StorylandModelBone& bone = bones[animatedBoneIndex];

    if (!lcsWeaponPreviewBoneShouldUseParentPivot(bone)) {
        return animatedBoneIndex;
    }

    if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
        return bone.parentIndex;
    }

    return animatedBoneIndex;
}


static bool lcsMeshControllerNameIsTinyTerminal(const StorylandModelBone& bone) {
    std::string name = normalizeBoneLookupName(bone.name);
    return name.find("finger") != std::string::npos ||
           name.find("toe") != std::string::npos;
}

static uint32_t lcsPreviewControllerFromWeightedBone(uint32_t modelBoneIndex, const std::vector<StorylandModelBone>& bones) {
    if (modelBoneIndex >= bones.size()) return 0xFFFFFFFFu;

    const StorylandModelBone& bone = bones[modelBoneIndex];


    if (lcsMeshControllerNameIsTinyTerminal(bone) &&
        bone.parentIndex != 0xFFFFFFFFu &&
        bone.parentIndex < bones.size()) {
        return bone.parentIndex;
    }

    return modelBoneIndex;
}

static bool findUsefulLcsSegmentForController(
    uint32_t controllerIndex,
    const std::vector<StorylandModelBone>& bones,
    uint32_t& outA,
    uint32_t& outB
) {
    outA = 0xFFFFFFFFu;
    outB = 0xFFFFFFFFu;

    if (controllerIndex >= bones.size()) return false;

    const StorylandModelBone& bone = bones[controllerIndex];


    if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
        const StorylandModelBone& parent = bones[bone.parentIndex];
        if (boneSegmentIsUsefulForMeshDeform(bone, parent)) {
            outA = bone.parentIndex;
            outB = controllerIndex;
            return true;
        }
    }


    for (uint32_t childIndex = 0; childIndex < bones.size(); ++childIndex) {
        if (bones[childIndex].parentIndex != controllerIndex) continue;
        if (lcsMeshControllerNameIsTinyTerminal(bones[childIndex])) continue;

        if (boneSegmentIsUsefulForMeshDeform(bones[childIndex], bone)) {
            outA = controllerIndex;
            outB = childIndex;
            return true;
        }
    }


    if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
        uint32_t parentIndex = bone.parentIndex;
        const StorylandModelBone& parent = bones[parentIndex];
        if (parent.parentIndex != 0xFFFFFFFFu && parent.parentIndex < bones.size()) {
            const StorylandModelBone& grandParent = bones[parent.parentIndex];
            if (boneSegmentIsUsefulForMeshDeform(parent, grandParent)) {
                outA = parent.parentIndex;
                outB = parentIndex;
                return true;
            }
        }
    }

    return false;
}

static uint32_t chooseDominantLcsWeightedBoneForVertex(
    const StorylandModelSkinWeights& weights,
    const std::vector<StorylandModelBone>& bones,
    StorylandPedSkinPaletteKind skinPaletteKind
) {
    uint32_t bestBone = 0xFFFFFFFFu;
    float bestWeight = -1.0f;

    uint32_t bestNonRootBone = 0xFFFFFFFFu;
    float bestNonRootWeight = -1.0f;

    for (uint32_t influenceIndex = 0; influenceIndex < weights.influenceCount; ++influenceIndex) {
        const StorylandModelSkinInfluence& influence = weights.influences[influenceIndex];
        if (influence.weight <= 0.00001f) continue;

        uint32_t modelBoneIndex = findModelBoneIndexForSkinPaletteIndex(influence.boneIndex, bones, skinPaletteKind);
        if (modelBoneIndex == 0xFFFFFFFFu || modelBoneIndex >= bones.size()) continue;

        uint32_t controllerIndex = lcsPreviewControllerFromWeightedBone(modelBoneIndex, bones);
        if (controllerIndex == 0xFFFFFFFFu || controllerIndex >= bones.size()) continue;

        if (influence.weight > bestWeight) {
            bestWeight = influence.weight;
            bestBone = controllerIndex;
        }

        uint32_t directId = bleedsDirectIdForModelBone(bones[controllerIndex]);
        if (directId != 0u && directId != 1u && influence.weight > bestNonRootWeight) {
            bestNonRootWeight = influence.weight;
            bestNonRootBone = controllerIndex;
        }
    }


    if (bestNonRootBone != 0xFFFFFFFFu && bestNonRootWeight >= bestWeight * 0.45f) {
        return bestNonRootBone;
    }

    return bestBone;
}

static std::vector<StorylandModelPoint> buildLcsAllAnimDominantSegmentMeshPreviewPoints(
    const std::vector<StorylandModelPoint>& sourcePoints,
    const std::vector<StorylandModelPoint>& animatedPreviewBones,
    StorylandPedSkinPaletteKind skinPaletteKind
) {
    const auto& bones = gModelFile.armatureBones();
    const auto& skinWeights = gModelFile.previewSkinWeights();

    if (sourcePoints.empty() || bones.empty() || animatedPreviewBones.size() != bones.size() ||
        skinWeights.size() != sourcePoints.size()) {
        return sourcePoints;
    }

    StorylandModelPoint meshMin = sourcePoints.front();
    StorylandModelPoint meshMax = sourcePoints.front();
    for (const StorylandModelPoint& point : sourcePoints) {
        meshMin.x = std::min(meshMin.x, point.x);
        meshMin.y = std::min(meshMin.y, point.y);
        meshMin.z = std::min(meshMin.z, point.z);
        meshMax.x = std::max(meshMax.x, point.x);
        meshMax.y = std::max(meshMax.y, point.y);
        meshMax.z = std::max(meshMax.z, point.z);
    }

    float meshSpan = std::max(meshMax.x - meshMin.x, std::max(meshMax.y - meshMin.y, meshMax.z - meshMin.z));
    float maxDisplacement = std::max(0.05f, meshSpan * 0.85f);

    std::vector<StorylandModelPoint> animatedPoints;
    animatedPoints.reserve(sourcePoints.size());

    for (size_t vertexIndex = 0; vertexIndex < sourcePoints.size(); ++vertexIndex) {
        const StorylandModelPoint& previewPoint = sourcePoints[vertexIndex];
        const StorylandModelSkinWeights& weights = skinWeights[vertexIndex];

        if (!weights.valid || weights.influenceCount == 0u) {
            animatedPoints.push_back(previewPoint);
            continue;
        }

        uint32_t controllerIndex = chooseDominantLcsWeightedBoneForVertex(weights, bones, skinPaletteKind);
        if (controllerIndex == 0xFFFFFFFFu || controllerIndex >= bones.size()) {
            animatedPoints.push_back(previewPoint);
            continue;
        }

        uint32_t segmentA = 0xFFFFFFFFu;
        uint32_t segmentB = 0xFFFFFFFFu;
        if (!findUsefulLcsSegmentForController(controllerIndex, bones, segmentA, segmentB) ||
            segmentA >= bones.size() || segmentB >= bones.size() ||
            segmentA >= animatedPreviewBones.size() || segmentB >= animatedPreviewBones.size()) {
            animatedPoints.push_back(previewPoint);
            continue;
        }

        StorylandModelPoint mapped = mapPointFromBindSegmentToAnimatedSegment(
            previewPoint,
            displayBonePosition(bones[segmentA]),
            displayBonePosition(bones[segmentB]),
            animatedPreviewBones[segmentA],
            animatedPreviewBones[segmentB]
        );

        StorylandModelPoint displacement = subtractStorylandPoint(mapped, previewPoint);
        float displacementLength = lengthStorylandPoint(displacement);
        if (displacementLength > maxDisplacement) {
            mapped = addStorylandPoint(previewPoint, scaleStorylandPoint(displacement, maxDisplacement / displacementLength));
        }

        animatedPoints.push_back(mapped);
    }

    return animatedPoints;
}


static uint32_t lcsRawLbsControllerBoneIndex(uint32_t modelBoneIndex, const std::vector<StorylandModelBone>& bones) {
    if (modelBoneIndex >= bones.size()) return 0xFFFFFFFFu;

    std::string name = normalizeBoneLookupName(bones[modelBoneIndex].name);


    if ((name.find("finger") != std::string::npos || name.find("toe") != std::string::npos) &&
        bones[modelBoneIndex].parentIndex != 0xFFFFFFFFu &&
        bones[modelBoneIndex].parentIndex < bones.size()) {
        return bones[modelBoneIndex].parentIndex;
    }

    return modelBoneIndex;
}

static StorylandModelPoint transformLcsRawPointByBoneLbs(
    const StorylandModelPoint& rawPoint,
    uint32_t boneIndex
) {
    if (!gLcsRawHierarchySkinStateValid ||
        boneIndex >= gLcsLastRawBindPositions.size() ||
        boneIndex >= gLcsLastAnimatedRawPositions.size() ||
        boneIndex >= gLcsLastBindWorldRotations.size() ||
        boneIndex >= gLcsLastAnimatedWorldRotations.size()) {
        return rawPoint;
    }

    StorylandModelPoint bindPosition = gLcsLastRawBindPositions[boneIndex];
    StorylandModelPoint animatedPosition = gLcsLastAnimatedRawPositions[boneIndex];

    StorylandQuat inverseBindRotation = conjugateStorylandQuat(gLcsLastBindWorldRotations[boneIndex]);
    StorylandQuat animatedRotation = gLcsLastAnimatedWorldRotations[boneIndex];

    StorylandModelPoint bindRelative = subtractStorylandPoint(rawPoint, bindPosition);
    StorylandModelPoint localToBone = rotatePointByQuat(inverseBindRotation, bindRelative);
    return addStorylandPoint(animatedPosition, rotatePointByQuat(animatedRotation, localToBone));
}

static std::vector<StorylandModelPoint> buildLcsRawSpaceLbsPreviewPoints(
    const std::vector<StorylandModelPoint>& sourcePoints,
    StorylandPedSkinPaletteKind skinPaletteKind
) {
    const auto& bones = gModelFile.armatureBones();
    const auto& skinWeights = gModelFile.previewSkinWeights();

    if (!gLcsRawHierarchySkinStateValid ||
        sourcePoints.empty() ||
        bones.empty() ||
        skinWeights.size() != sourcePoints.size()) {
        return sourcePoints;
    }

    StorylandModelPoint meshMin = sourcePoints.front();
    StorylandModelPoint meshMax = sourcePoints.front();
    for (const StorylandModelPoint& point : sourcePoints) {
        meshMin.x = std::min(meshMin.x, point.x);
        meshMin.y = std::min(meshMin.y, point.y);
        meshMin.z = std::min(meshMin.z, point.z);
        meshMax.x = std::max(meshMax.x, point.x);
        meshMax.y = std::max(meshMax.y, point.y);
        meshMax.z = std::max(meshMax.z, point.z);
    }

    float meshSpan = std::max(meshMax.x - meshMin.x, std::max(meshMax.y - meshMin.y, meshMax.z - meshMin.z));
    float maxDisplacement = std::max(0.08f, meshSpan * 1.10f);

    std::vector<StorylandModelPoint> animatedPoints;
    animatedPoints.reserve(sourcePoints.size());

    for (size_t vertexIndex = 0; vertexIndex < sourcePoints.size(); ++vertexIndex) {
        const StorylandModelPoint& previewPoint = sourcePoints[vertexIndex];
        const StorylandModelSkinWeights& weights = skinWeights[vertexIndex];

        if (!weights.valid || weights.influenceCount == 0u) {
            animatedPoints.push_back(previewPoint);
            continue;
        }

        StorylandModelPoint rawPoint = mapLcsDisplayPointToRawHierarchy(gLcsLastBasisMap, previewPoint);
        StorylandModelPoint blendedRaw{};
        float totalWeight = 0.0f;

        for (uint32_t influenceIndex = 0; influenceIndex < weights.influenceCount; ++influenceIndex) {
            const StorylandModelSkinInfluence& influence = weights.influences[influenceIndex];
            if (influence.weight <= 0.00001f) continue;

            uint32_t modelBoneIndex = findModelBoneIndexForSkinPaletteIndex(influence.boneIndex, bones, skinPaletteKind);
            if (modelBoneIndex == 0xFFFFFFFFu || modelBoneIndex >= bones.size()) continue;

            uint32_t controllerIndex = lcsRawLbsControllerBoneIndex(modelBoneIndex, bones);
            if (controllerIndex == 0xFFFFFFFFu || controllerIndex >= bones.size()) continue;

            StorylandModelPoint transformedRaw = transformLcsRawPointByBoneLbs(rawPoint, controllerIndex);
            blendedRaw.x += transformedRaw.x * influence.weight;
            blendedRaw.y += transformedRaw.y * influence.weight;
            blendedRaw.z += transformedRaw.z * influence.weight;
            totalWeight += influence.weight;
        }

        if (totalWeight <= 0.00001f ||
            !std::isfinite(blendedRaw.x) ||
            !std::isfinite(blendedRaw.y) ||
            !std::isfinite(blendedRaw.z)) {
            animatedPoints.push_back(previewPoint);
            continue;
        }

        if (std::fabs(totalWeight - 1.0f) > 0.0001f) {
            blendedRaw.x /= totalWeight;
            blendedRaw.y /= totalWeight;
            blendedRaw.z /= totalWeight;
        }

        StorylandModelPoint mappedPreview = mapLcsRawHierarchyPointToDisplay(gLcsLastBasisMap, blendedRaw);

        StorylandModelPoint displacement = subtractStorylandPoint(mappedPreview, previewPoint);
        float displacementLength = lengthStorylandPoint(displacement);
        if (displacementLength > maxDisplacement) {
            mappedPreview = addStorylandPoint(previewPoint, scaleStorylandPoint(displacement, maxDisplacement / displacementLength));
        }

        animatedPoints.push_back(mappedPreview);
    }

    return animatedPoints;
}

static std::vector<StorylandModelPoint> buildTrueSkinnedAnimatedPreviewPoints(
    const std::vector<StorylandModelPoint>& sourcePoints
) {
    if (!gModelAnimLoaded || !gAnimFile.hasDecodedMotion() || sourcePoints.empty()) {
        return sourcePoints;
    }

    const auto& bones = gModelFile.armatureBones();
    const auto& skinWeights = gModelFile.previewSkinWeights();

    if (bones.empty() || skinWeights.size() != sourcePoints.size()) {
        return sourcePoints;
    }


    std::vector<StorylandModelPoint> animatedPreviewBones = buildAnimatedModelBonePositions();
    if (animatedPreviewBones.size() != bones.size()) {
        return sourcePoints;
    }

    if (gLastAnimatedBoneWorldRotations.size() != bones.size()) {
        gLastAnimatedBoneWorldRotations.assign(bones.size(), StorylandQuat{});
    }

    if (shouldUseCustomExportMatrixSkinPreview()) {
        return buildCustomExportMatrixSkinnedPreviewPoints(sourcePoints);
    }


    StorylandPedSkinPaletteKind skinPaletteKind = detectPedSkinPaletteKindForCurrentModel(bones);

    if (skinPaletteKind == StorylandPedSkinPaletteKind::CanonicalLcs &&
        currentModelLooksLikeLcsPedSkeleton(bones)) {


        return buildLcsRawSpaceLbsPreviewPoints(sourcePoints, skinPaletteKind);
    }

    StorylandModelPoint meshMin = sourcePoints.front();
    StorylandModelPoint meshMax = sourcePoints.front();
    for (const StorylandModelPoint& point : sourcePoints) {
        meshMin.x = std::min(meshMin.x, point.x);
        meshMin.y = std::min(meshMin.y, point.y);
        meshMin.z = std::min(meshMin.z, point.z);
        meshMax.x = std::max(meshMax.x, point.x);
        meshMax.y = std::max(meshMax.y, point.y);
        meshMax.z = std::max(meshMax.z, point.z);
    }

    float meshSpan = std::max(meshMax.x - meshMin.x, std::max(meshMax.y - meshMin.y, meshMax.z - meshMin.z));
    float displacementScale = 0.40f;
    if (skinPaletteKind == StorylandPedSkinPaletteKind::CanonicalLcs && currentAttachedAnimLooksWeaponLike()) {


        displacementScale = 0.45f;
    } else if (skinPaletteKind == StorylandPedSkinPaletteKind::CanonicalLcs) {
        displacementScale = 0.55f;
    }
    float maxTrueSkinPreviewDisplacement = std::max(0.02f, meshSpan * displacementScale);

    std::vector<StorylandModelPoint> animatedPoints;
    animatedPoints.reserve(sourcePoints.size());

    uint32_t validSkinnedVertices = 0;

    for (size_t vertexIndex = 0; vertexIndex < sourcePoints.size(); ++vertexIndex) {
        const StorylandModelPoint& previewPoint = sourcePoints[vertexIndex];
        const StorylandModelSkinWeights& weights = skinWeights[vertexIndex];

        if (!weights.valid || weights.influenceCount == 0u) {
            animatedPoints.push_back(previewPoint);
            continue;
        }

        StorylandModelPoint blendedPreview{};
        float totalWeight = 0.0f;

        for (uint32_t influenceIndex = 0; influenceIndex < weights.influenceCount; ++influenceIndex) {
            const StorylandModelSkinInfluence& influence = weights.influences[influenceIndex];
            if (influence.weight <= 0.00001f) continue;

            uint32_t modelBoneIndex = findModelBoneIndexForSkinPaletteIndex(influence.boneIndex, bones, skinPaletteKind);
            if (modelBoneIndex == 0xFFFFFFFFu || modelBoneIndex >= bones.size()) continue;

            if (skinPaletteKind == StorylandPedSkinPaletteKind::CanonicalLcs && currentAttachedAnimLooksWeaponLike()) {
                std::string mappedBoneName = normalizeBoneLookupName(bones[modelBoneIndex].name);
                if (mappedBoneName.find("finger") != std::string::npos || mappedBoneName.find("toe") != std::string::npos) {


                    continue;
                }
            }

            StorylandModelPoint transformedPreview = previewPoint;

            bool usedLcsSegmentRetarget = false;
            if (skinPaletteKind == StorylandPedSkinPaletteKind::CanonicalLcs &&
                currentModelLooksLikeLcsPedSkeleton(bones) &&
                modelBoneIndex < bones.size()) {
                const StorylandModelBone& mappedBone = bones[modelBoneIndex];
                if (mappedBone.parentIndex != 0xFFFFFFFFu && mappedBone.parentIndex < bones.size()) {
                    uint32_t parentIndex = mappedBone.parentIndex;
                    const StorylandModelBone& parentBone = bones[parentIndex];

                    if (boneSegmentIsUsefulForMeshDeform(mappedBone, parentBone) &&
                        parentIndex < animatedPreviewBones.size() &&
                        modelBoneIndex < animatedPreviewBones.size()) {
                        transformedPreview = mapPointFromBindSegmentToAnimatedSegment(
                            previewPoint,
                            displayBonePosition(parentBone),
                            displayBonePosition(mappedBone),
                            animatedPreviewBones[parentIndex],
                            animatedPreviewBones[modelBoneIndex]
                        );
                        usedLcsSegmentRetarget = true;
                    }
                }
            }

            if (!usedLcsSegmentRetarget) {
                uint32_t pivotBoneIndex = modelBoneIndex;
                if (!(skinPaletteKind == StorylandPedSkinPaletteKind::CanonicalLcs && currentAttachedAnimLooksWeaponLike())) {
                    pivotBoneIndex = lcsWeaponPreviewPivotBoneIndex(modelBoneIndex, bones);
                    if (pivotBoneIndex >= bones.size()) pivotBoneIndex = modelBoneIndex;
                }

                transformedPreview = transformPreviewPointByWeightedBone(
                    previewPoint,
                    bones[pivotBoneIndex],
                    animatedPreviewBones[pivotBoneIndex],
                    gLastAnimatedBoneWorldRotations[modelBoneIndex]
                );
            }

            blendedPreview.x += transformedPreview.x * influence.weight;
            blendedPreview.y += transformedPreview.y * influence.weight;
            blendedPreview.z += transformedPreview.z * influence.weight;
            totalWeight += influence.weight;
        }

        if (totalWeight <= 0.00001f ||
            !std::isfinite(blendedPreview.x) || !std::isfinite(blendedPreview.y) || !std::isfinite(blendedPreview.z)) {
            animatedPoints.push_back(previewPoint);
            continue;
        }

        if (std::fabs(totalWeight - 1.0f) > 0.0001f) {
            blendedPreview.x /= totalWeight;
            blendedPreview.y /= totalWeight;
            blendedPreview.z /= totalWeight;
        }


        StorylandModelPoint displacement = subtractStorylandPoint(blendedPreview, previewPoint);
        float displacementLength = lengthStorylandPoint(displacement);
        if (displacementLength > maxTrueSkinPreviewDisplacement) {
            blendedPreview = addStorylandPoint(previewPoint, scaleStorylandPoint(displacement, maxTrueSkinPreviewDisplacement / displacementLength));
        }

        animatedPoints.push_back(blendedPreview);
        validSkinnedVertices++;
    }

    if (validSkinnedVertices == 0u) {
        return sourcePoints;
    }

    return animatedPoints;
}


static std::vector<StorylandModelPoint> buildAnimatedModelPreviewPoints(const std::vector<StorylandModelPoint>& sourcePoints) {
    if (!gModelAnimLoaded || !gAnimFile.hasDecodedMotion() || sourcePoints.empty()) {
        return sourcePoints;
    }

    if (modelHasUsableTrueSkinWeights(sourcePoints)) {
        return buildTrueSkinnedAnimatedPreviewPoints(sourcePoints);
    }

    if (!gApproximateAnimMeshPreview) {
        return sourcePoints;
    }

    const auto& bones = gModelFile.armatureBones();
    if (bones.empty()) return sourcePoints;

    std::vector<StorylandModelPoint> animatedBones = buildAnimatedModelBonePositions();
    if (animatedBones.size() != bones.size()) return sourcePoints;

    struct Segment {
        size_t parentIndex = 0;
        size_t childIndex = 0;
        StorylandModelPoint bindA;
        StorylandModelPoint bindB;
        StorylandModelPoint animA;
        StorylandModelPoint animB;
    };

    std::vector<Segment> segments;
    segments.reserve(bones.size());

    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const StorylandModelBone& bone = bones[boneIndex];
        if (bone.parentIndex == 0xFFFFFFFFu || bone.parentIndex >= bones.size()) continue;

        const StorylandModelBone& parentBone = bones[size_t(bone.parentIndex)];
        if (!boneSegmentIsUsefulForMeshDeform(bone, parentBone)) continue;

        Segment segment;
        segment.parentIndex = size_t(bone.parentIndex);
        segment.childIndex = boneIndex;
        segment.bindA = displayBonePosition(parentBone);
        segment.bindB = displayBonePosition(bone);
        segment.animA = animatedBones[segment.parentIndex];
        segment.animB = animatedBones[segment.childIndex];

        if (lengthSqStorylandPoint(subtractStorylandPoint(segment.bindB, segment.bindA)) < 0.000001f) continue;
        if (lengthSqStorylandPoint(subtractStorylandPoint(segment.animB, segment.animA)) < 0.000001f) continue;

        segments.push_back(segment);
    }

    if (segments.empty()) return sourcePoints;

    StorylandModelPoint meshMin = sourcePoints.front();
    StorylandModelPoint meshMax = sourcePoints.front();
    for (const StorylandModelPoint& point : sourcePoints) {
        meshMin.x = std::min(meshMin.x, point.x);
        meshMin.y = std::min(meshMin.y, point.y);
        meshMin.z = std::min(meshMin.z, point.z);
        meshMax.x = std::max(meshMax.x, point.x);
        meshMax.y = std::max(meshMax.y, point.y);
        meshMax.z = std::max(meshMax.z, point.z);
    }

    float meshSpan = std::max(meshMax.x - meshMin.x, std::max(meshMax.y - meshMin.y, meshMax.z - meshMin.z));
    float maxPreviewDisplacement = std::max(0.01f, meshSpan * 0.22f);

    std::vector<StorylandModelPoint> animatedPoints;
    animatedPoints.reserve(sourcePoints.size());

    for (const StorylandModelPoint& point : sourcePoints) {
        const Segment* bestSegment = nullptr;
        float bestDistanceSq = std::numeric_limits<float>::max();
        float bestT = 0.0f;

        for (const Segment& segment : segments) {
            float t = 0.0f;
            float distanceSq = distanceSqPointToSegment(point, segment.bindA, segment.bindB, t);
            if (distanceSq < bestDistanceSq) {
                bestDistanceSq = distanceSq;
                bestT = t;
                bestSegment = &segment;
            }
        }

        if (!bestSegment) {
            animatedPoints.push_back(point);
            continue;
        }

        StorylandModelPoint animatedPoint = mapPointFromBindSegmentToAnimatedSegment(
            point,
            bestSegment->bindA,
            bestSegment->bindB,
            bestSegment->animA,
            bestSegment->animB
        );

        if (!std::isfinite(animatedPoint.x) || !std::isfinite(animatedPoint.y) || !std::isfinite(animatedPoint.z)) {
            animatedPoints.push_back(point);
            continue;
        }

        StorylandModelPoint displacement = subtractStorylandPoint(animatedPoint, point);
        float displacementLength = lengthStorylandPoint(displacement);
        if (displacementLength > maxPreviewDisplacement) {
            animatedPoint = addStorylandPoint(point, scaleStorylandPoint(displacement, maxPreviewDisplacement / displacementLength));
        }

        animatedPoints.push_back(animatedPoint);
    }

    return animatedPoints;
}


static void selectModelBone(int index);
static void selectAnimPayload(const StorylandTreePayload& payload);

static bool currentModelCanUsePedCutsceneAnimation() {
    StorylandModelKind kind = gModelFile.modelKind();
    return (kind == StorylandModelKind::PedModel || kind == StorylandModelKind::CutsceneModel) &&
           !gModelFile.armatureBones().empty();
}


static bool animPreviewShouldFreezeBoneAtBindPose(const StorylandModelBone& bone) {


    if (currentModelLooksLikeLcsPedSkeleton(gModelFile.armatureBones())) {
        return false;
    }

    if (!currentAttachedAnimLooksWeaponLike()) {
        return false;
    }

    std::string name = normalizeBoneLookupName(bone.name);
    uint32_t directId = bleedsDirectIdForModelBone(bone);

    bool lowerBody =
        name.find("thigh") != std::string::npos ||
        name.find("calf") != std::string::npos ||
        name.find("foot") != std::string::npos ||
        name.find("toe") != std::string::npos ||
        bleedsDirectIdIsLowerBody(directId);

    bool armLayerBone =
        name.find("clavicle") != std::string::npos ||
        name.find("upperarm") != std::string::npos ||
        name.find("forearm") != std::string::npos ||
        name.find("hand") != std::string::npos ||
        name.find("finger") != std::string::npos ||
        directId == 21u || directId == 22u || directId == 23u || directId == 24u || directId == 25u ||
        directId == 31u || directId == 32u || directId == 33u || directId == 34u || directId == 35u;

    if (gWeaponUpperBodyLayerMaskPreview) {
        return !armLayerBone;
    }

    if (gLockWeaponLowerBodyPreview && lowerBody) {
        return true;
    }

    return false;
}

static bool animPreviewShouldLockRootTranslation(const StorylandModelBone& bone) {
    if (!gLockAnimRootPreview) return false;


    if (currentModelLooksLikeLcsPedSkeleton(gModelFile.armatureBones())) {
        return false;
    }

    return bleedsDirectIdForModelBone(bone) == 0u;
}

static bool animPreviewShouldLockRootRotation(const StorylandModelBone& bone) {


    if (currentModelLooksLikeLcsPedSkeleton(gModelFile.armatureBones())) return false;
    if (!currentAttachedAnimLooksWeaponLike()) return false;
    return bleedsDirectIdForModelBone(bone) == 0u;
}

static bool animPlaybackTargetIsActive() {
    return gMode == StorylandMode::AnimFile || (gMode == StorylandMode::ModelFile && gModelAnimLoaded);
}

static void refreshAnimDetailsAfterTimeChange() {
    if (gMode == StorylandMode::AnimFile) selectAnimOverview();
    else if (gMode == StorylandMode::ModelFile && gSelectedKind == StorylandTreeKind::ModelBone) selectModelBone(gSelectedIndex);
    else if (gMode == StorylandMode::ModelFile && (gSelectedKind == StorylandTreeKind::AnimOverview || gSelectedKind == StorylandTreeKind::AnimTrack || gSelectedKind == StorylandTreeKind::AnimField || gSelectedKind == StorylandTreeKind::AnimString)) {
        selectAnimPayload({gSelectedKind, gSelectedIndex});
    }
}

static bool advanceAnimationPlaybackFrame();

static VOID CALLBACK storylandAnimationTimerProc(HWND, UINT, UINT_PTR, DWORD) {
    advanceAnimationPlaybackFrame();
}

static void startAnimationPlaybackTimer() {
    if (gMainWindow) {
        SetTimer(gMainWindow, 1, 16, storylandAnimationTimerProc);
    }
    if (gPreview) {
        InvalidateRect(gPreview, nullptr, FALSE);
    }
}

static bool advanceAnimationPlaybackFrame() {
    if (!animPlaybackTargetIsActive()) {
        return false;
    }

    DWORD now = GetTickCount();
    if (gAnimLastTick == 0) {
        gAnimLastTick = now;
        return true;
    }

    float delta = float(now - gAnimLastTick) / 1000.0f;
    gAnimLastTick = now;

    if (!gAnimPlaying) {
        return true;
    }

    if (!std::isfinite(delta) || delta < 0.0f) {
        delta = 0.0f;
    }
    if (delta > 0.10f) {
        delta = 0.10f;
    }

    float duration = std::max(0.001f, gAnimFile.durationSeconds());
    gAnimCurrentTime += delta;
    while (gAnimCurrentTime >= duration) {
        gAnimCurrentTime -= duration;
    }
    while (gAnimCurrentTime < 0.0f) {
        gAnimCurrentTime += duration;
    }

    if (gPreview) {
        InvalidateRect(gPreview, nullptr, FALSE);
    }

    return true;
}

static void toggleAnimPlayback() {
    if (!animPlaybackTargetIsActive()) return;
    gAnimPlaying = !gAnimPlaying;
    gAnimLastTick = GetTickCount();
    startAnimationPlaybackTimer();
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void stopAnimPlayback() {
    if (!animPlaybackTargetIsActive()) return;
    gAnimPlaying = false;
    gAnimCurrentTime = 0.0f;
    gAnimLastTick = GetTickCount();
    InvalidateRect(gPreview, nullptr, FALSE);
    refreshAnimDetailsAfterTimeChange();
}

static void stepAnimPlayback(float seconds) {
    if (!animPlaybackTargetIsActive()) return;
    gAnimPlaying = false;
    float duration = std::max(0.001f, gAnimFile.durationSeconds());
    gAnimCurrentTime += seconds;
    while (gAnimCurrentTime < 0.0f) gAnimCurrentTime += duration;
    while (gAnimCurrentTime >= duration) gAnimCurrentTime -= duration;
    gAnimLastTick = GetTickCount();
    InvalidateRect(gPreview, nullptr, FALSE);
    refreshAnimDetailsAfterTimeChange();
}

static void drawAnimPreviewOpenGl(HWND hwnd, HDC dc, RECT rc) {
    std::vector<StorylandAnimPoseBone> pose = gAnimFile.poseAt(gAnimCurrentTime);

    if (!gOpenGlReady && !initializeOpenGlPreview(hwnd)) {
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        TextOutW(dc, rc.left + 8, rc.top + 8, L"OpenGL init failed for .anim preview", 35);
        return;
    }

    if (!wglMakeCurrent(dc, gOpenGlContext)) {
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        TextOutW(dc, rc.left + 8, rc.top + 8, L"OpenGL context activation failed", 33);
        return;
    }

    int width = std::max<int>(1, static_cast<int>(rc.right - rc.left));
    int height = std::max<int>(1, static_cast<int>(rc.bottom - rc.top));
    glViewport(0, 0, width, height);
    setStoriesViewportClearColor();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    double aspect = double(width) / double(height);
    setPerspectiveProjection(45.0, aspect, 0.01, 500.0);
    drawStoriesViewportSky(100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(gModelPanX, gModelPanY, -gModelDistance);
    glMultModelQuat(gModelViewRotation);

    float minX = -0.85f, maxX = 0.85f;
    float minY = -0.35f, maxY = 0.35f;
    float minZ = 0.0f, maxZ = 1.48f;
    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    float largestSpan = std::max(maxX - minX, std::max(maxY - minY, maxZ - minZ));
    float modelScale = 2.0f / std::max(0.001f, largestSpan);

    glScalef(modelScale, modelScale, modelScale);
    glTranslatef(-centerX, -centerY, -centerZ);

    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glColor3f(0.28f, 0.28f, 0.28f);
    float gridSize = 1.2f;
    for (int i = -10; i <= 10; ++i) {
        float d = float(i) * gridSize / 10.0f;
        glVertex3f(-gridSize, d, 0.0f); glVertex3f(gridSize, d, 0.0f);
        glVertex3f(d, -gridSize, 0.0f); glVertex3f(d, gridSize, 0.0f);
    }
    glColor3f(1.0f, 0.2f, 0.2f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(gridSize, 0.0f, 0.0f);
    glColor3f(0.2f, 1.0f, 0.2f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, gridSize, 0.0f);
    glColor3f(0.2f, 0.45f, 1.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, gridSize);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    glLineWidth(4.0f);
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.84f, 0.12f);
    for (const auto& bone : pose) {
        if (!bone.visible) continue;
        if (bone.parentIndex == 0xFFFFFFFFu || bone.parentIndex >= pose.size()) continue;
        const auto& parent = pose[size_t(bone.parentIndex)];
        if (!parent.visible) continue;
        glVertex3f(parent.x, parent.y, parent.z);
        glVertex3f(bone.x, bone.y, bone.z);
    }
    glEnd();

    glPointSize(7.0f);
    glBegin(GL_POINTS);
    glColor3f(0.0f, 0.95f, 1.0f);
    for (const auto& bone : pose) {
        if (!bone.visible) continue;
        glVertex3f(bone.x, bone.y, bone.z);
    }
    glEnd();

    glColor3f(0.75f, 0.75f, 0.75f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(minX, minY, minZ);
    glVertex3f(maxX, minY, minZ);
    glVertex3f(maxX, maxY, minZ);
    glVertex3f(minX, maxY, minZ);
    glEnd();
    glBegin(GL_LINE_LOOP);
    glVertex3f(minX, minY, maxZ);
    glVertex3f(maxX, minY, maxZ);
    glVertex3f(maxX, maxY, maxZ);
    glVertex3f(minX, maxY, maxZ);
    glEnd();
    glBegin(GL_LINES);
    glVertex3f(minX, minY, minZ); glVertex3f(minX, minY, maxZ);
    glVertex3f(maxX, minY, minZ); glVertex3f(maxX, minY, maxZ);
    glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, maxY, maxZ);
    glVertex3f(minX, maxY, minZ); glVertex3f(minX, maxY, maxZ);
    glEnd();

    glFlush();
    SwapBuffers(dc);
    wglMakeCurrent(nullptr, nullptr);

    std::wstring title = L"OpenGL ANIM viewport  |  tracks=" + std::to_wstring(gAnimFile.tracks().size()) +
        L"  |  frames=" + std::to_wstring(gAnimFile.frameCount()) +
        L"  |  time=" + std::to_wstring(gAnimCurrentTime) +
        L" / " + std::to_wstring(gAnimFile.durationSeconds()) +
        L"  |  " + (gAnimPlaying ? L"playing" : L"paused") +
        L"  |  LMB rotate, RMB pan, wheel zoom";
}


static void selectArchivePayload(const StorylandTreePayload& payload) {
    if (payload.kind == StorylandTreeKind::ArchiveEntry) selectArchiveEntry(payload.index);
    else if (payload.kind == StorylandTreeKind::ArchiveMeshResource) selectArchiveMeshResource(payload.index);
    else if (payload.kind == StorylandTreeKind::ArchiveTextureResource) selectArchiveTextureResource(payload.index);
    else if (payload.kind == StorylandTreeKind::ArchiveDirectTexture) selectArchiveDirectTexture(payload.index);
    else if (payload.kind == StorylandTreeKind::ArchiveAnimationResource) selectArchiveAnimationResource(payload.index);
    else selectArchiveRoot();
}

static void selectTexture(int index) {
    const auto& textures = gTextureArchive.textures();
    if (index < 0 || size_t(index) >= textures.size()) return;
    gSelectedIndex = index;
    std::string error;
    if (!gTextureArchive.decodeTexture(size_t(index), gCurrentImage, error)) {
        setDetails(L"Decode failed: " + widen(error));
        return;
    }
    createTextureBitmapFromImage();

    const auto& e = textures[size_t(index)];
    std::wstringstream ss;
    ss << L"Texture: " << widen(e.name) << L"\r\n"
       << L"Kind: " << (e.kind == TextureKind::CtwTex ? L"CTW TEX" : e.kind == TextureKind::Ps2 ? L"PS2" : e.kind == TextureKind::Psp ? L"PSP" : L"Unknown") << L"\r\n"
       << L"Size: " << e.width << L" x " << e.height << L"\r\n"
       << L"BPP: " << int(e.bpp) << L"\r\n"
       << L"Mip count: " << int(e.mipCount) << L"\r\n"
       << L"Container: " << hexWide(e.containerBase, 6) << L"\r\n"
       << L"Texture header: " << hexWide(e.textureHeaderOffset, 6) << L"\r\n"
       << L"Raster: " << hexWide(e.rasterOffset, 6) << L"\r\n"
       << L"Block size: " << e.blockSize << L" bytes\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectDtzHeaderField(int index) {
    const auto& fields = gDtzArchive.headerFields();
    if (index < 0 || size_t(index) >= fields.size()) return;
    const auto& f = fields[size_t(index)];
    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::DtzHeader;
    std::wstringstream ss;
    ss << L"GAME.DTZ header field\r\n\r\n"
       << L"Offset: " << hexWide(f.offset, 4) << L"\r\n"
       << L"Name: " << widen(f.name) << L"\r\n"
       << L"Value: " << hexWide(f.value) << L" / " << f.value << L"\r\n"
       << L"Note: " << widen(f.note) << L"\r\n\r\n"
       << L"Input was " << (gDtzArchive.wasCompressedInput() ? L"compressed zlib/deflate" : L"already unpacked/raw") << L".\r\n"
       << L"Unpacked bytes: " << gDtzArchive.unpackedBytes().size() << L"\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectDtzResourceHint(int index) {
    const auto& hints = gDtzArchive.resourceHints();
    if (index < 0 || size_t(index) >= hints.size()) return;
    const auto& h = hints[size_t(index)];
    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::DtzResourceHint;
    std::wstringstream ss;
    ss << L"DTZ resource pointer\r\n\r\n"
       << L"Name: " << widen(h.name) << L"\r\n"
       << L"Resource offset: " << hexWide(h.offset, 6) << L"\r\n"
       << L"Size: " << h.size << L"\r\n"
       << L"Note: " << widen(h.note) << L"\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static std::wstring buildDtzPreviewEntryExtractRoot(int index) {
    std::wstring root = buildDtzPreviewExtractRoot();
    if (!root.empty() && root.back() != L'\\' && root.back() != L'/') root += L"\\";
    root += L"entry_" + std::to_wstring(index);
    CreateDirectoryW(root.c_str(), nullptr);
    return root;
}

static bool findBestDtzModelCompanionTextureIndex(size_t modelIndex, size_t& companionTextureIndexOut, std::wstring& reasonOut) {
    reasonOut.clear();
    const auto& entries = gDtzArchive.dirEntries();
    if (modelIndex >= entries.size()) return false;

    const auto& modelEntry = entries[modelIndex];
    std::wstring modelName = canonicalDtzImgResourceName(widen(modelEntry.name));
    std::wstring modelStem = getFileStemPart(modelName);

    size_t sameStemIndex = 0;
    if (gDtzArchive.findDirEntryByStemAndExtension(modelStem, {L".xtx", L".chk", L".tex"}, sameStemIndex)) {
        companionTextureIndexOut = sameStemIndex;
        reasonOut = L"same cleaned stem";
        return true;
    }

    uint32_t modelStart = modelEntry.startSector;
    size_t bestIndex = size_t(-1);
    uint32_t bestDistance = 0xFFFFFFFFu;

    for (size_t index = 0; index < entries.size(); ++index) {
        if (index == modelIndex) continue;
        const auto& candidate = entries[index];
        std::wstring candidateName = canonicalDtzImgResourceName(widen(candidate.name));
        std::wstring candidateExt = getExtensionLower(candidateName);
        if (!(candidateExt == L".xtx" || candidateExt == L".chk" || candidateExt == L".tex")) continue;
        if (candidate.startSector > modelStart) continue;

        uint32_t candidateEnd = candidate.startSector + candidate.sectorCount;
        if (candidateEnd == modelStart) {
            companionTextureIndexOut = index;
            reasonOut = L"previous DTZ texture entry ends exactly at this model start";
            return true;
        }

        if (candidateEnd < modelStart) {
            uint32_t distance = modelStart - candidateEnd;
            if (distance <= 16u && distance < bestDistance) {
                bestDistance = distance;
                bestIndex = index;
            }
        }
    }

    if (bestIndex != size_t(-1)) {
        companionTextureIndexOut = bestIndex;
        reasonOut = L"nearest previous texture archive in DTZ sector order";
        return true;
    }

    return false;
}

static bool prepareDtzDirEntryPreview(int index, std::wstring& previewSummary) {
    previewSummary.clear();
    clearDtzEmbeddedPreviewState();

    const auto& entries = gDtzArchive.dirEntries();
    if (index < 0 || size_t(index) >= entries.size()) {
        previewSummary = L"Preview unavailable: selected internal IMG entry index is out of range.";
        return false;
    }

    const auto& entry = entries[size_t(index)];
    std::wstring entryName = widen(entry.name);
    std::wstring resourceName = canonicalDtzImgResourceName(entryName);
    std::wstring entryExt = getExtensionLower(resourceName);
    bool previewableTexture = (entryExt == L".xtx" || entryExt == L".chk" || entryExt == L".tex");
    bool previewableModel = (entryExt == L".mdl" || entryExt == L".dff");
    if (!previewableTexture && !previewableModel) {
        previewSummary = L"Preview not wired yet for this resource type. Live DTZ+IMG preview supports .mdl/.dff and .chk/.xtx/.tex entries first.";
        return false;
    }
    if (!gDtzArchive.hasCompanionImg()) {
        previewSummary = L"Preview unavailable: no companion gta3PS2.img/gta3PSP.img is loaded for this GAME.DTZ.";
        return false;
    }

    std::vector<uint8_t> primaryBytes;
    std::string error;
    if (!gDtzArchive.extractDirEntryBytes(size_t(index), primaryBytes, error)) {
        previewSummary = L"Preview extraction failed: " + widen(error);
        return false;
    }

    std::wstring extractRoot = buildDtzPreviewEntryExtractRoot(index);
    std::wstring primaryPath = extractRoot + L"\\" + resourceName;
    if (!writeWholeFileBinary(primaryPath, primaryBytes, error)) {
        previewSummary = L"Preview extraction failed: " + widen(error);
        return false;
    }

    if (previewableTexture) {
        if (!gTextureArchive.loadFromFile(primaryPath, LeedsPlatform::Auto, error)) {
            previewSummary = L"Texture preview load failed: " + widen(error);
            return false;
        }
        if (gTextureArchive.textures().empty()) {
            previewSummary = L"Texture preview loaded the archive file, but no textures were decoded from it.";
            return false;
        }
        if (!gTextureArchive.decodeTexture(0, gCurrentImage, error)) {
            previewSummary = L"Texture preview decode failed: " + widen(error);
            return false;
        }
        createTextureBitmapFromImage();
        gDtzEmbeddedPreviewKind = DtzEmbeddedPreviewKind::TextureArchive;
        gDtzEmbeddedPreviewIndex = index;
        gDtzEmbeddedPreviewPath = primaryPath;

        const auto& firstTexture = gTextureArchive.textures()[0];
        std::wstringstream ss;
        ss << L"Texture preview active in the right pane. Storyland extracted this internal IMG entry to a temp file and decoded the first texture in the archive.\r\n"
           << L"Preview source: " << primaryPath << L"\r\n"
           << L"Decoded textures in archive: " << gTextureArchive.textures().size() << L"\r\n"
           << L"Showing texture 0: " << widen(firstTexture.name) << L"  " << firstTexture.width << L"x" << firstTexture.height << L"  bpp=" << int(firstTexture.bpp);
        previewSummary = ss.str();
        return true;
    }

    size_t companionTextureIndex = 0;
    std::wstring companionTextureReason;
    std::wstring extractedCompanionTexturePath;
    if (findBestDtzModelCompanionTextureIndex(size_t(index), companionTextureIndex, companionTextureReason)) {
        std::vector<uint8_t> textureBytes;
        if (gDtzArchive.extractDirEntryBytes(companionTextureIndex, textureBytes, error)) {
            std::wstring companionTextureName = canonicalDtzImgResourceName(widen(gDtzArchive.dirEntries()[companionTextureIndex].name));
            std::wstring companionTexturePath = extractRoot + L"\\" + companionTextureName;
            if (writeWholeFileBinary(companionTexturePath, textureBytes, error)) {
                extractedCompanionTexturePath = companionTexturePath;
            }
        }
    }

    gModelAnimLoaded = false;
    gModelAnimPath.clear();
    gModelAnimStatus.clear();
    gAnimPlaying = false;
    gAnimCurrentTime = 0.0f;

    if (!gModelFile.loadFromFile(primaryPath, error)) {
        previewSummary = L"Model preview load failed: " + widen(error);
        return false;
    }

    resetModelViewport();
    std::wstring textureStatus;
    loadCompanionTextureForCurrentModel(primaryPath, textureStatus);
    gDtzEmbeddedPreviewKind = DtzEmbeddedPreviewKind::ModelFile;
    gDtzEmbeddedPreviewIndex = index;
    gDtzEmbeddedPreviewPath = primaryPath;

    std::wstringstream ss;
    ss << L"OpenGL model preview active in the right pane. Storyland extracted this internal IMG entry to a temp file and loaded it with the normal MDL viewer path.\r\n"
       << L"Preview source: " << primaryPath << L"\r\n"
       << L"Detected model family: " << widen(gModelFile.modelKindName()) << L"\r\n"
       << L"Texture status: " << textureStatus << L"\r\n";
    if (!extractedCompanionTexturePath.empty()) {
        ss << L"DTZ companion texture: " << extractedCompanionTexturePath << L" (" << companionTextureReason << L")\r\n";
    } else {
        ss << L"DTZ companion texture: none extracted for this model entry.\r\n";
    }
    ss << L"Viewport controls: LMB rotate, RMB pan, wheel zoom, double-click reset.";
    previewSummary = ss.str();
    return true;
}

static void openSelectedDtzDirEntryStandalone() {
    if (gMode != StorylandMode::DtzArchive || gSelectedKind != StorylandTreeKind::DtzDirEntry || gSelectedIndex < 0) {
        MessageBoxW(gMainWindow, L"Select a GAME.DTZ internal IMG entry first.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    const auto& entries = gDtzArchive.dirEntries();
    if (size_t(gSelectedIndex) >= entries.size()) return;
    const auto& entry = entries[size_t(gSelectedIndex)];
    std::wstring entryName = widen(entry.name);
    std::wstring resourceName = canonicalDtzImgResourceName(entryName);
    std::wstring entryExt = getExtensionLower(resourceName);
    if (!(entryExt == L".mdl" || entryExt == L".dff" || entryExt == L".xtx" || entryExt == L".chk" || entryExt == L".tex" || entryExt == L".dtz" || entryExt == L".bin")) {
        setStatus(L"Selected DTZ+IMG internal entry is listed, but standalone open is only wired for MDL/DFF/XTX/CHK/TEX/DTZ/BIN right now.");
        return;
    }

    std::vector<uint8_t> bytes;
    std::string error;
    if (!gDtzArchive.extractDirEntryBytes(size_t(gSelectedIndex), bytes, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"DTZ internal extract failed", MB_ICONERROR);
        return;
    }

    std::wstring extractRoot = buildDtzPreviewEntryExtractRoot(gSelectedIndex);
    std::wstring primaryPath = extractRoot + L"\\" + resourceName;
    if (!writeWholeFileBinary(primaryPath, bytes, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"DTZ internal extract failed", MB_ICONERROR);
        return;
    }

    if (entryExt == L".mdl" || entryExt == L".dff") {
        size_t companionIndex = 0;
        std::wstring companionReason;
        if (findBestDtzModelCompanionTextureIndex(size_t(gSelectedIndex), companionIndex, companionReason)) {
            std::vector<uint8_t> textureBytes;
            if (gDtzArchive.extractDirEntryBytes(companionIndex, textureBytes, error)) {
                std::wstring companionName = canonicalDtzImgResourceName(widen(gDtzArchive.dirEntries()[companionIndex].name));
                std::wstring companionPath = extractRoot + L"\\" + companionName;
                writeWholeFileBinary(companionPath, textureBytes, error);
            }
        }
    }

    openStorylandFile(primaryPath);
}

static void selectDtzDirEntry(int index) {
    const auto& entries = gDtzArchive.dirEntries();
    if (index < 0 || size_t(index) >= entries.size()) return;
    const auto& entry = entries[size_t(index)];
    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::DtzDirEntry;

    std::wstringstream ss;
    ss << L"GAME.DTZ internal gta3PS2.img sector map entry\r\n\r\n"
       << L"Name: " << widen(entry.name) << L"\r\n"
       << L"Internal map index: " << entry.dirIndex << L"\r\n"
       << L"Start sector: " << entry.startSector << L"\r\n"
       << L"Sector count: " << entry.sectorCount << L"\r\n"
       << L"End sector: " << (entry.startSector + entry.sectorCount) << L"\r\n"
       << L"Byte budget: " << (uint64_t(entry.sectorCount) * 2048ull) << L" bytes\r\n"
       << L"KiB budget: " << ((uint64_t(entry.sectorCount) * 2048ull) / 1024ull) << L" KiB\r\n"
       << L"IMG byte offset: " << (uint64_t(entry.startSector) * 2048ull) << L"\r\n"
       << L"IMG byte end: " << ((uint64_t(entry.startSector) + uint64_t(entry.sectorCount)) * 2048ull) << L"\r\n"
       << L"Matching DTZ records: " << entry.matchingRecordIndices.size() << L"\r\n";
    if (entry.countDiffersFromCompanionDir) {
        ss << L"Matched GAME.DTZ record sector count: " << entry.matchedDtzSectorCount << L"\r\n"
           << L"Browser/preview sector count source: loaded .dir pair. Same-start GAME.DTZ count does not match.\r\n"
           << L"This avoids extracting a truncated or over-long model/texture slice for preview while still keeping the GAME.DTZ record linked for patching.\r\n";
    }
    ss << L"\r\n";

    if (gDtzArchive.hasCompanionImg()) {
        uint64_t imgOffset = uint64_t(entry.startSector) * 2048ull;
        uint64_t imgEnd = uint64_t(entry.startSector + entry.sectorCount) * 2048ull;
        ss << L"Companion IMG loaded: " << gDtzArchive.companionImgPath() << L"\r\n"
           << L"Companion IMG size: " << gDtzArchive.companionImgSize() << L" bytes\r\n"
           << L"This entry range is " << (imgEnd <= gDtzArchive.companionImgSize() ? L"inside" : L"past") << L" the loaded IMG size.\r\n\r\n";
        if (imgOffset > gDtzArchive.companionImgSize()) {
            ss << L"Warning: start offset is beyond the loaded IMG, so this GAME.DTZ and IMG probably do not belong together.\r\n\r\n";
        }
    } else {
        ss << L"No companion IMG loaded. Load gta3PS2.img/gta3PSP.img if you want this same patch action to grow/shrink the physical IMG layout too.\r\n\r\n";
    }

    if (!gDtzArchive.companionDirPath().empty()) {
        ss << L"Optional beta-build .dir loaded: " << gDtzArchive.companionDirPath() << L"\r\n\r\n";
    }

    if (entry.matchingRecordIndices.empty()) {
        ss << L"This internal map row is not backed by a recognized editable GAME.DTZ sector record, so patching is blocked for this line.\r\n";
    } else {
        ss << L"This line comes from GAME.DTZ sector records. Right-click > Patch selected sector count edits matching GAME.DTZ count fields and can shift later internal starts by the sector delta. If a companion IMG is loaded and shifting is enabled, Storyland also inserts/deletes the matching 2048-byte sector span in the IMG.\r\n";
    }

    std::wstring previewSummary;
    prepareDtzDirEntryPreview(index, previewSummary);
    if (!previewSummary.empty()) {
        ss << L"\r\nPreview section\r\n\r\n" << previewSummary << L"\r\n";
    }

    std::wstring ext = getDtzImgResourceExtensionLower(widen(entry.name));
    if (ext == L".mdl" || ext == L".dff" || ext == L".xtx" || ext == L".chk" || ext == L".tex") {
        ss << L"\r\nTip: double-click this entry in the tree to open it as a standalone extracted file too.\r\n";
    }

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectDtzSectorRecord(int index) {
    const auto& records = gDtzArchive.sectorRecords();
    if (index < 0 || size_t(index) >= records.size()) return;
    const auto& r = records[size_t(index)];
    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::DtzSectorRecord;
    std::wstringstream ss;
    ss << L"GAME.DTZ sector mapping record\r\n\r\n";
    if (!r.resourceName.empty()) ss << L"Known target: " << widen(r.resourceName) << L"\r\n";
    ss << L"Record offset: " << hexWide(r.recordOffset, 6) << L"\r\n"
       << L"Start field offset: " << hexWide(r.startOffset, 6) << L"\r\n"
       << L"Count field offset: " << hexWide(r.countOffset, 6) << L"\r\n"
       << L"Start sector: " << r.startSector << L"\r\n"
       << L"Sector count: " << r.sectorCount << L"\r\n"
       << L"End sector: " << (r.startSector + r.sectorCount) << L"\r\n"
       << L"Byte budget: " << (uint64_t(r.sectorCount) * 2048ull) << L" bytes\r\n"
       << L"IMG byte offset: " << (uint64_t(r.startSector) * 2048ull) << L"\r\n"
       << L"IMG byte end: " << ((uint64_t(r.startSector) + uint64_t(r.sectorCount)) * 2048ull) << L"\r\n"
       << L"Source: " << widen(r.source) << L"\r\n"
       << L"Note: " << widen(r.note) << L"\r\n\r\n"
       << L"This is the value Storyland patches. For PLR.mdl, the important record is start=23. Original retail count was 44; the proven expanded test count was 68.\r\n"
       << L"If count changes and this is inside gta3PS2.img mapping, later start sectors must be shifted by the delta. When a companion IMG is loaded, that same shift can physically move the later IMG bytes.\r\n";
    if (gDtzArchive.hasCompanionImg()) {
        ss << L"\r\nCompanion IMG loaded: " << gDtzArchive.companionImgPath() << L"  size=" << gDtzArchive.companionImgSize() << L" bytes\r\n";
    }
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectDtzDataBlock(int index) {
    const auto& blocks = gDtzArchive.dataBlocks();
    const auto& fields = gDtzArchive.dataFields();
    if (index < 0 || size_t(index) >= blocks.size()) return;
    const auto& block = blocks[size_t(index)];
    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::DtzDataBlock;

    size_t fieldCount = 0;
    size_t editableCount = 0;
    for (const auto& field : fields) {
        if (field.blockIndex == size_t(index)) {
            fieldCount++;
            if (field.editable) editableCount++;
        }
    }

    std::wstringstream ss;
    ss << L"Real scanned GAME.DTZ data block\r\n\r\n"
       << L"Name: " << widen(block.name) << L"\r\n"
       << L"Parser: " << widen(block.parser) << L"\r\n"
       << L"Header field offset: " << hexWide(block.headerOffset, 4) << L"\r\n"
       << L"Data offset: " << hexWide(block.offset, 6) << L"\r\n"
       << L"Inferred end: " << hexWide(block.inferredEnd, 6) << L"\r\n"
       << L"Inferred size: " << block.size << L" bytes\r\n"
       << L"Row size: " << block.rowSize << L"\r\n"
       << L"Row count: " << block.rowCount << L"\r\n"
       << L"Structured fields shown: " << fieldCount << L"\r\n"
       << L"Editable fields shown: " << editableCount << L"\r\n"
       << L"Editable block: " << (block.editable ? L"yes" : L"no") << L"\r\n\r\n"
       << L"Note: " << widen(block.note) << L"\r\n\r\n"
       << L"This is derived from the actual unpacked GAME.DTZ bytes. Header-pointer ranges use real pointer values and the next higher valid header pointer/relocation boundary, not a hard-coded pretend offset list.\r\n";
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectDtzDataField(int index) {
    const auto& fields = gDtzArchive.dataFields();
    if (index < 0 || size_t(index) >= fields.size()) return;
    const auto& field = fields[size_t(index)];
    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::DtzDataField;

    std::wstringstream ss;
    ss << L"Editable/viewable GAME.DTZ data field\r\n\r\n"
       << L"Block: " << widen(field.blockName) << L"\r\n"
       << L"Record: " << widen(field.rowLabel) << L"\r\n"
       << L"Field: " << widen(field.name) << L"\r\n"
       << L"Type: " << widen(field.type) << L"\r\n"
       << L"Absolute offset: " << hexWide(field.absoluteOffset, 6) << L"\r\n"
       << L"Relative offset: " << hexWide(field.relativeOffset, 4) << L"\r\n"
       << L"Size: " << field.size << L" byte(s)\r\n"
       << L"Value: " << widen(field.valueText) << L"\r\n"
       << L"Editable: " << (field.editable ? L"yes" : L"no") << L"\r\n\r\n"
       << L"Note: " << widen(field.note) << L"\r\n\r\n";
    if (field.editable) {
        ss << L"Right-click > Patch selected GAME.DTZ data field to modify this field inside GAME.DTZ. Then use Save As / Rebuild GAME.DTZ to write the changed file.\r\n";
    } else {
        ss << L"This field is view-only here. Pointer/name fields need safer relocation handling first.\r\n";
    }
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}


static void selectDtzPayload(const StorylandTreePayload& payload) {
    if (payload.kind != StorylandTreeKind::DtzDirEntry) clearDtzEmbeddedPreviewState();
    if (payload.kind == StorylandTreeKind::DtzOverview) selectDtzOverview();
    else if (payload.kind == StorylandTreeKind::DtzHeader) selectDtzHeaderField(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzResourceHint) selectDtzResourceHint(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzSectorRecord) selectDtzSectorRecord(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzDirEntry) selectDtzDirEntry(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzDataBlock) selectDtzDataBlock(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzDataField) selectDtzDataField(payload.index);
}


static bool boneHasVisiblePreviewPosition(const StorylandModelBone& bone) {
    if (!bone.hasPreviewPosition) return false;
    if (bone.previewPositionSource.find("hidden") != std::string::npos) return false;
    if (bone.previewPositionSource.find("not drawn") != std::string::npos) return false;
    return true;
}

static StorylandModelPoint displayBonePosition(const StorylandModelBone& bone) {
    if (bone.hasPreviewPosition) return bone.previewPosition;
    if (bone.hasComposedPosition) return bone.composedPosition;
    if (bone.hasLocalPosition) return bone.localPosition;
    return bone.worldPosition;
}

static void selectModelBone(int index) {
    const auto& bones = gModelFile.armatureBones();
    if (index < 0 || size_t(index) >= bones.size()) return;
    const auto& bone = bones[size_t(index)];
    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::ModelBone;

    StorylandModelPoint position = displayAnimatedModelBonePosition(size_t(index));
    std::wstringstream ss;
    ss << L"Imported pedmodel armature bone/frame\r\n\r\n"
       << L"Index: " << bone.index << L"\r\n"
       << L"Name: " << widen(bone.name) << L"\r\n"
       << L"Section: " << widen(bone.sectionKind) << L"\r\n"
       << L"Frame offset: " << hexWide(bone.offset, 6) << L"\r\n";
    if (bone.nodeId == 0xFFFFFFFFu) {
        ss << L"Node id: <none>\r\n";
    } else {
        ss << L"Node id: " << hexWide(bone.nodeId) << L"\r\n";
    }
    if (bone.boneId == 0xFFFFFFFFu) {
        ss << L"Hierarchy bone id: <none>\r\n";
    } else {
        ss << L"Hierarchy bone id: " << bone.boneId << L" / " << hexWide(bone.boneId) << L"\r\n";
    }
    if (bone.hasWorldRotation) {
        ss << L"Bind world rotation: ("
           << bone.worldRotationX << L", "
           << bone.worldRotationY << L", "
           << bone.worldRotationZ << L", "
           << bone.worldRotationW << L")\r\n";
    }
    if (bone.hasLocalRotation) {
        ss << L"Bind local rotation: ("
           << bone.localRotationX << L", "
           << bone.localRotationY << L", "
           << bone.localRotationZ << L", "
           << bone.localRotationW << L")\r\n";
    }
    if (bone.nameOffset != 0xFFFFFFFFu) {
        ss << L"Name string offset: " << hexWide(bone.nameOffset, 6) << L"\r\n";
    } else {
        ss << L"Name source: ped fallback / frame order\r\n";
    }
    if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
        ss << L"Parent index: " << bone.parentIndex << L"\r\n"
           << L"Parent name: " << widen(bones[size_t(bone.parentIndex)].name) << L"\r\n"
           << L"Parent offset: " << hexWide(bone.parentOffset, 6) << L"\r\n";
    } else {
        ss << L"Parent: <root/unresolved>\r\n";
    }

    ss << L"\r\nLocal/model matrix position: ("
       << bone.localPosition.x << L", " << bone.localPosition.y << L", " << bone.localPosition.z << L")"
       << (bone.hasLocalPosition ? L"" : L"  [not sane]") << L"\r\n";
    ss << L"World/LTM matrix position: ("
       << bone.worldPosition.x << L", " << bone.worldPosition.y << L", " << bone.worldPosition.z << L")"
       << (bone.hasWorldPosition ? L"" : L"  [using local fallback]") << L"\r\n";
    ss << L"Parent-accumulated local-frame position: ("
       << bone.composedPosition.x << L", " << bone.composedPosition.y << L", " << bone.composedPosition.z << L")"
       << (bone.hasComposedPosition ? L"" : L"  [not resolved]") << L"\r\n";
    ss << L"Viewport display position: (" << position.x << L", " << position.y << L", " << position.z << L")\r\n";
    ss << L"Viewport source: " << widen(bone.previewPositionSource) << L"\r\n\r\n";
    if (gModelAnimLoaded) {
        ss << L"Attached animation: " << gModelAnimPath << L"\r\n";
        ss << L"Animation time: " << gAnimCurrentTime << L" / " << gAnimFile.durationSeconds() << L"\r\n";
        ss << L"Animation decode state: " << (gAnimFile.hasDecodedMotion() ? L"Leeds compressed transform channels decoded and sampled" : L"raw/static inspection only") << L"\r\n";
        const StorylandAnimTrack* matchedTrack = findAnimTrackForModelBone(size_t(index), bone);
        if (matchedTrack) {
            ss << L"Matched animation track: #" << matchedTrack->index
               << L" clip=" << matchedTrack->clipIndex
               << L" channel=" << matchedTrack->channelIndex
               << L" boneId=" << matchedTrack->boneId
               << L" keys=" << matchedTrack->keys.size()
               << L" stride=" << matchedTrack->keyStride
               << L" keyData=" << hexWide(matchedTrack->keyDataOffset, 6)
               << L"\r\n";
        } else {
            ss << L"Matched animation track: <none>\r\n";
        }
    }

    ss << L"Source: RslNode1/RslNode2 frame data. Preview uses accumulated local frame space.\r\n";

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectModelField(int index) {
    const auto& fields = gModelFile.fields();

    size_t weightedVertices = 0;
    uint32_t influenceTotal = 0;
    for (const auto& weights : gModelFile.previewSkinWeights()) {
        if (!weights.valid) continue;
        weightedVertices++;
        influenceTotal += weights.influenceCount;
    }

    std::wstringstream ss;
    ss << L"MDL\r\n"
       << L"Type: " << widen(gModelFile.modelKindName()) << L"\r\n"
       << L"Size: " << gModelFile.fileSize() << L" bytes / " << ((gModelFile.fileSize() + 2047) / 2048) << L" sectors\r\n";

    if (!gModelTextureStatus.empty()) {
        ss << L"Texture: " << gModelTextureStatus << L"\r\n";
    }
    if (gModelTextureLoaded) {
        ss << L"Texture archive: " << gModelTexturePath << L"\r\n"
           << L"Texture atlas: " << widen(gModelTextureName)
           << L" [" << gModelTextureImage.width << L"x" << gModelTextureImage.height << L"]\r\n";
    }

    ss << L"\r\nPreview\r\n"
       << L"Vertices: " << gModelFile.previewPoints().size() << L"\r\n"
       << L"Triangles: " << gModelFile.previewTriangles().size() << L"\r\n"
       << L"Texcoords: " << gModelFile.previewTexcoords().size() << L"\r\n"
       << L"Bones: " << gModelFile.armatureBones().size() << L"\r\n";

    if (!gModelFile.previewSkinWeights().empty()) {
        ss << L"Skin: " << weightedVertices << L"/" << gModelFile.previewSkinWeights().size()
           << L" weighted vertices";
        if (influenceTotal != 0) ss << L", " << influenceTotal << L" influences";
        ss << L"\r\n";
    }

    const auto& materialNames = gModelFile.previewMaterialTextureNames();
    if (!materialNames.empty()) {
        ss << L"Materials:";
        size_t shown = 0;
        for (size_t i = 0; i < materialNames.size() && shown < 12; ++i) {
            if (materialNames[i].empty()) continue;
            ss << L" " << i << L":" << widen(materialNames[i]);
            shown++;
        }
        if (materialNames.size() > shown) ss << L" +" << (materialNames.size() - shown) << L" more";
        ss << L"\r\n";
    }

    if (gModelAnimLoaded) {
        ss << L"\r\nANIM\r\n"
           << L"File: " << gModelAnimPath << L"\r\n"
           << L"Time: " << gAnimCurrentTime << L" / " << gAnimFile.durationSeconds() << L"\r\n"
           << L"State: " << (gAnimFile.hasDecodedMotion() ? L"decoded" : L"inspection only") << L"\r\n";
    }

    if (index >= 0 && size_t(index) < fields.size()) {
        const auto& f = fields[size_t(index)];
        ss << L"\r\nField\r\n"
           << L"Group: " << widen(f.group) << L"\r\n"
           << L"Name: " << widen(f.name) << L"\r\n"
           << L"Offset: " << hexWide(f.offset, 6) << L"\r\n"
           << L"Value: " << hexWide(f.value) << L"\r\n";
        if (!f.note.empty()) ss << L"Note: " << widen(f.note) << L"\r\n";
    }

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static std::wstring findGameDtzBesideImg(const std::wstring& imgPath) {
    std::wstring directory = getDirectoryPart(imgPath);
    const wchar_t* directNames[] = {
        L"GAME.DTZ", L"game.dtz", L"Game.dtz", L"GAME.BIN", L"game.bin", L"Game.bin"
    };

    for (const wchar_t* name : directNames) {
        std::wstring candidate = directory + name;
        if (fileExists(candidate)) return candidate;
    }

    WIN32_FIND_DATAW findData = {};
    HANDLE find = FindFirstFileW((directory + L"*.dtz").c_str(), &findData);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring candidate = directory + findData.cFileName;
            std::wstring stem = getFileStemPart(candidate);
            if (sameWideNoCase(stem, L"GAME")) {
                FindClose(find);
                return candidate;
            }
        } while (FindNextFileW(find, &findData));
        FindClose(find);
    }

    find = FindFirstFileW((directory + L"*.bin").c_str(), &findData);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring candidate = directory + findData.cFileName;
            std::wstring stem = getFileStemPart(candidate);
            if (sameWideNoCase(stem, L"GAME")) {
                FindClose(find);
                return candidate;
            }
        } while (FindNextFileW(find, &findData));
        FindClose(find);
    }

    return L"";
}

static bool openImgWithGameDtzPair(const std::wstring& imgPath, std::string& error) {
    std::wstring dtzPath = findGameDtzBesideImg(imgPath);
    if (dtzPath.empty()) {
        error = "Could not find GAME.DTZ/GAME.BIN beside the selected IMG.";
        return false;
    }

    if (!gDtzArchive.loadFromFile(dtzPath, error)) return false;
    if (!gDtzArchive.loadCompanionImg(imgPath, error)) return false;

    gMode = StorylandMode::DtzArchive;
    populateDtzList();
    SetWindowTextW(gMainWindow, L"Storyland - GAME.DTZ + gta3PS*.img");
    applyStorylandTitleTintForPath(imgPath);
    setStatus(L"Opened GAME.DTZ + IMG pair from IMG: " + imgPath);
    return true;
}

static void openStorylandFile(const std::wstring& path) {
    if (path.empty()) return;
    std::wstring ext = getExtensionLower(path);
    std::string error;
    if (ext == L".anim" || ext == L".chk" || ext == L".xtx" || ext == L".tex" ||
        ext == L".img" || ext == L".lvz" || ext == L".wbl" || ext == L".dir" ||
        ext == L".dtz" || ext == L".bin" || ext == L".mdl" || ext == L".dff") {
        addRecentFile(path);
    }

    if (ext == L".anim") {
        if (!gAnimFile.loadFromFile(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland ANIM open failed", MB_ICONERROR);
            return;
        }

        gAnimPlaying = true;
        gAnimCurrentTime = 0.0f;
        gAnimLastTick = GetTickCount();
        gModelAnimStatus = widen(gAnimFile.summaryLine());

        if (gMode == StorylandMode::ModelFile && currentModelCanUsePedCutsceneAnimation()) {
            gModelAnimLoaded = true;
            gModelAnimPath = path;
            populateModelList();

            std::wstring textureStatus;
            loadCompanionTextureForCurrentModel(gModelFile.sourcePath(), textureStatus);

            selectModelField(0);
            startAnimationPlaybackTimer();
            SetWindowTextW(gMainWindow, L"Storyland - MDL Viewer + ANIM");
            applyStorylandTitleTintForPath(path);
            setStatus(L"Attached ANIM to current MDL: " + gModelAnimStatus);
            InvalidateRect(gPreview, nullptr, FALSE);

            return;
        }

        if (gMode == StorylandMode::ModelFile && !currentModelCanUsePedCutsceneAnimation()) {
            setStatus(L"ANIM opened standalone; current model type does not expose a ped/cutscene armature.");
        }

        clearView();
        gModelAnimLoaded = false;
        gModelAnimPath.clear();

        gMode = StorylandMode::AnimFile;
        resetModelViewport();
        gModelDistance = 3.0f;
        populateAnimList();
        startAnimationPlaybackTimer();
        SetWindowTextW(gMainWindow, L"Storyland - ANIM Player");
        applyStorylandTitleTintForPath(path);
        return;
    }

    clearView();

    if (ext == L".chk" || ext == L".xtx" || ext == L".tex") {
        if (!gTextureArchive.loadFromFile(path, LeedsPlatform::Auto, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland texture open failed", MB_ICONERROR);
            return;
        }
        gMode = StorylandMode::TextureArchive;
        populateTextureList();
        SetWindowTextW(gMainWindow, L"Storyland - Texture Archive");
        applyStorylandTitleTintForPath(path);
        return;
    }

    if (ext == L".img") {
        if (gMode == StorylandMode::DtzArchive) {
            if (!gDtzArchive.loadCompanionImg(path, error)) {
                MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland DTZ+IMG open failed", MB_ICONERROR);
                return;
            }
            populateDtzList();
            SetWindowTextW(gMainWindow, L"Storyland - GAME.DTZ + gta3PS*.img");
            applyStorylandTitleTintForPath(path);
            setStatus(L"Loaded companion IMG for current GAME.DTZ: " + path);
            return;
        }

        std::string lvzError;
        if (gArchiveBrowser.loadImgFromFile(path, lvzError)) {
            gMode = StorylandMode::ArchiveFile;
            resetModelViewport();
            gModelDistance = 8.0f;
            populateArchiveList();
            SetWindowTextW(gMainWindow, L"Storyland - IMG Browser");
            applyStorylandTitleTintForPath(path);
            return;
        }

        std::string dtzError;
        if (openImgWithGameDtzPair(path, dtzError)) {
            return;
        }

        MessageBoxW(gMainWindow, (widen(lvzError) + L"\r\n\r\nDTZ+IMG fallback also failed:\r\n" + widen(dtzError)).c_str(), L"Storyland IMG open failed", MB_ICONERROR);
        return;
    }

    if (ext == L".lvz") {
        if (!gArchiveBrowser.loadLvzWithCompanionImg(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland LVZ open failed", MB_ICONERROR);
            return;
        }
        gMode = StorylandMode::ArchiveFile;
        resetModelViewport();
        gModelDistance = 8.0f;
        populateArchiveList();
        SetWindowTextW(gMainWindow, L"Storyland - LVZ + IMG Browser");
        applyStorylandTitleTintForPath(path);
        return;
    }

    if (ext == L".wbl") {
        if (!gWblFile.loadFromFile(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland CTW WBL open failed", MB_ICONERROR);
            return;
        }
        gMode = StorylandMode::WblFile;
        resetModelViewport();
        gModelDistance = 5.0f;
        gModelPanX = 0.0f;
        gModelPanY = 0.0f;
        populateWblList();
        SetWindowTextW(gMainWindow, L"Storyland - CTW WBL Viewer");
        applyStorylandTitleTintForPath(path);
        InvalidateRect(gPreview, nullptr, FALSE);
        return;
    }

    if (ext == L".dir") {
        if (gMode != StorylandMode::DtzArchive) {
            MessageBoxW(gMainWindow, L"Open GAME.DTZ first. Retail LCS/VCS use GAME.DTZ; load a .dir only for a beta-build archive that actually has one.", L"Storyland", MB_ICONINFORMATION);
            return;
        }
        if (!gDtzArchive.loadCompanionDir(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland DIR open failed", MB_ICONERROR);
            return;
        }
        populateDtzList();
        SetWindowTextW(gMainWindow, L"Storyland - GAME.DTZ + gta3PS2.dir");
        applyStorylandTitleTintForPath(path);
        return;
    }

    if (ext == L".dtz" || ext == L".bin") {
        if (!gDtzArchive.loadFromFile(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland DTZ open failed", MB_ICONERROR);
            return;
        }
        gMode = StorylandMode::DtzArchive;
        populateDtzList();
        SetWindowTextW(gMainWindow, gDtzArchive.hasCompanionImg() ? L"Storyland - GAME.DTZ + gta3PS*.img" : L"Storyland - GAME.DTZ");
        applyStorylandTitleTintForPath(path);
        return;
    }

    if (ext == L".mdl" || ext == L".dff") {
        gModelAnimLoaded = false;
        gModelAnimPath.clear();
        gModelAnimStatus.clear();

        if (!gModelFile.loadFromFile(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland MDL open failed", MB_ICONERROR);
            return;
        }
        gMode = StorylandMode::ModelFile;
        resetModelViewport();
        populateModelList();


        std::wstring textureStatus;
        loadCompanionTextureForCurrentModel(path, textureStatus);

        std::wstring companionAnim = getDirectoryPart(path) + getFileStemPart(path) + L".anim";
        if (currentModelCanUsePedCutsceneAnimation() && fileExists(companionAnim)) {
            std::string animError;
            if (gAnimFile.loadFromFile(companionAnim, animError)) {
                gModelAnimLoaded = true;
                gModelAnimPath = companionAnim;
                gModelAnimStatus = widen(gAnimFile.summaryLine());
                gAnimPlaying = true;
                gAnimCurrentTime = 0.0f;
                gAnimLastTick = GetTickCount();
                startAnimationPlaybackTimer();
                populateModelList();
                loadCompanionTextureForCurrentModel(path, textureStatus);
            }
        }

        selectModelField(0);
        SetWindowTextW(gMainWindow, gModelAnimLoaded ? L"Storyland - MDL Viewer + ANIM" : L"Storyland - MDL Viewer");
        StorylandTitleTint modelTint = titleTintFromPath(path);
        if (modelTint == StorylandTitleTint::Default && !gModelTexturePath.empty()) modelTint = titleTintFromPath(gModelTexturePath);
        applyStorylandTitleTint(modelTint);
        setStatus(buildModelStatusLine());
        InvalidateRect(gPreview, nullptr, FALSE);
        return;
    }

    MessageBoxW(gMainWindow, L"Unknown extension. Storyland opens .chk/.xtx/.tex textures, .mdl/.dff/.wbl models, .anim animations, .dtz/.bin GAME.DTZ, .img/.lvz archives, and .dir sector maps after GAME.DTZ is open.", L"Storyland", MB_ICONINFORMATION);
}

static bool writeWholeFileBinary(const std::wstring& path, const std::vector<uint8_t>& bytes, std::string& error) {
    return storylandWriteFilesTransaction({{std::filesystem::path(path), &bytes}}, error);
}

static std::wstring buildArchiveExtractRoot() {
    wchar_t tempPath[MAX_PATH] = {};
    DWORD len = GetTempPathW(MAX_PATH, tempPath);
    std::wstring root = len > 0 ? std::wstring(tempPath) : L".";
    if (!root.empty() && root.back() != L'\\' && root.back() != L'/') root += L"\\";
    root += L"StorylandEmbedded";
    CreateDirectoryW(root.c_str(), nullptr);
    return root;
}

static std::wstring buildDtzPreviewExtractRoot() {
    std::wstring root = buildArchiveExtractRoot();
    if (!root.empty() && root.back() != L'\\' && root.back() != L'/') root += L"\\";
    root += L"DtzImgPairPreview";
    CreateDirectoryW(root.c_str(), nullptr);
    return root;
}

static void clearDtzEmbeddedPreviewState() {
    gDtzEmbeddedPreviewKind = DtzEmbeddedPreviewKind::None;
    gDtzEmbeddedPreviewIndex = -1;
    gDtzEmbeddedPreviewPath.clear();
    gCurrentImage = {};
    deleteTextureBitmap();
    clearModelTexture();
}

static bool currentModeUsesInteractiveModelViewport() {
    if (gMode == StorylandMode::ModelFile || gMode == StorylandMode::ArchiveFile || gMode == StorylandMode::AnimFile || gMode == StorylandMode::WblFile) return true;
    if (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile) return true;
    return false;
}

static void openSelectedArchiveEntry() {
    if (gMode != StorylandMode::ArchiveFile || gSelectedKind != StorylandTreeKind::ArchiveEntry || gSelectedIndex < 0) {
        MessageBoxW(gMainWindow, L"Select an archive entry first.", L"Storyland", MB_ICONINFORMATION);
        return;
    }
    const auto& entries = gArchiveBrowser.entries();
    if (size_t(gSelectedIndex) >= entries.size()) return;
    const auto& entry = entries[size_t(gSelectedIndex)];

    std::wstring name = widen(entry.name);
    std::wstring ext = getExtensionLower(name);
    if (ext == L".wrld" || ext == L".area") {
        selectArchiveEntry(gSelectedIndex);
        setStatus(L"WRLD/AREA sector is shown alone in the LVZ/IMG OpenGL preview.");
        return;
    }
    if (!(ext == L".mdl" || ext == L".dff" || ext == L".xtx" || ext == L".chk" || ext == L".tex" || ext == L".dtz" || ext == L".bin")) {
        selectArchiveEntry(gSelectedIndex);
        setStatus(L"Selected embedded resource is listed but not directly openable yet.");
        return;
    }

    std::vector<uint8_t> bytes;
    std::string error;
    if (!gArchiveBrowser.extractEntryBytes(size_t(gSelectedIndex), bytes, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Embedded extract failed", MB_ICONERROR);
        return;
    }

    std::wstring extractRoot = buildArchiveExtractRoot();
    std::wstring primaryPath = extractRoot + L"\\" + name;
    if (!writeWholeFileBinary(primaryPath, bytes, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Embedded extract failed", MB_ICONERROR);
        return;
    }

    if (ext == L".mdl" || ext == L".dff") {
        std::wstring stem = getFileStemPart(name);
        size_t companionIndex = 0;
        if (gArchiveBrowser.findEntryByStemAndExtension(stem, {L".xtx", L".chk", L".tex"}, companionIndex)) {
            std::vector<uint8_t> textureBytes;
            if (gArchiveBrowser.extractEntryBytes(companionIndex, textureBytes, error)) {
                std::wstring companionPath = extractRoot + L"\\" + widen(gArchiveBrowser.entries()[companionIndex].name);
                writeWholeFileBinary(companionPath, textureBytes, error);
            }
        }
    }

    openStorylandFile(primaryPath);
}

static bool writeUtf8TextFile(const std::wstring& path, const std::wstring& text, std::string& error) {
    FILE* file = nullptr;
#ifdef _WIN32
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr) {
        error = "Could not open log output file.";
        return false;
    }
#else
    file = fopen(std::string(path.begin(), path.end()).c_str(), "wb");
    if (!file) {
        error = "Could not open log output file.";
        return false;
    }
#endif
    const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    fwrite(bom, 1, 3, file);
    int bytesNeeded = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytesNeeded <= 0) {
        fclose(file);
        error = "Could not convert log text to UTF-8.";
        return false;
    }
    std::string utf8(size_t(bytesNeeded - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, utf8.data(), bytesNeeded, nullptr, nullptr);
    if (!utf8.empty()) fwrite(utf8.data(), 1, utf8.size(), file);
    fclose(file);
    return true;
}

static std::wstring getDetailsText() {
    int length = GetWindowTextLengthW(gDetails);
    if (length <= 0) return L"";
    std::wstring text(size_t(length) + 1u, L'\0');
    GetWindowTextW(gDetails, text.data(), length + 1);
    text.resize(size_t(length));
    return text;
}

static std::wstring buildExportLogText() {
    std::wstringstream ss;
    ss << L"Storyland export log\r\n";
    ss << L"====================\r\n\r\n";

    if (gMode == StorylandMode::ModelFile) {
        ss << L"Mode: MDL / Leeds model\r\n";
        ss << L"Model kind: " << widen(gModelFile.modelKindName()) << L"\r\n";
        ss << L"File size: " << gModelFile.fileSize() << L" bytes / " << ((gModelFile.fileSize() + 2047) / 2048) << L" sectors\r\n";
        ss << L"Preview vertices: " << gModelFile.previewPoints().size() << L"\r\n";
        ss << L"Preview triangles: " << gModelFile.previewTriangles().size() << L"\r\n";
        ss << L"Preview texcoords: " << gModelFile.previewTexcoords().size() << L"\r\n";
        ss << L"Armature bones: " << gModelFile.armatureBones().size() << L"\r\n";
        if (!gModelFile.armatureBones().empty()) {
            ss << L"\r\nArmature / ped skeleton\r\n-----------------------\r\n";
            const auto& bones = gModelFile.armatureBones();
            for (const auto& bone : bones) {
                ss << L"#" << bone.index << L" " << widen(bone.name)
                   << L" section=" << widen(bone.sectionKind)
                   << L" frame=" << hexWide(bone.offset, 6);
                if (bone.boneId == 0xFFFFFFFFu) ss << L" boneId=<none>";
                else ss << L" boneId=" << bone.boneId;
                if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < bones.size()) {
                    ss << L" parent=#" << bone.parentIndex << L":" << widen(bones[size_t(bone.parentIndex)].name);
                } else {
                    ss << L" parent=<root/unresolved>";
                }
                ss << L" local=(" << bone.localPosition.x << L", " << bone.localPosition.y << L", " << bone.localPosition.z << L")";
                ss << L" world=(" << bone.worldPosition.x << L", " << bone.worldPosition.y << L", " << bone.worldPosition.z << L")";
                ss << L" composed=(" << bone.composedPosition.x << L", " << bone.composedPosition.y << L", " << bone.composedPosition.z << L")";
                ss << L" preview=(" << bone.previewPosition.x << L", " << bone.previewPosition.y << L", " << bone.previewPosition.z << L")";
                ss << L" previewSource=" << widen(bone.previewPositionSource);
                ss << L"\r\n";
            }
        }
        ss << L"Texture loaded: " << (gModelTextureLoaded ? L"yes" : L"no") << L"\r\n";
        ss << L"Texture status: " << (gModelTextureStatus.empty() ? L"none" : gModelTextureStatus) << L"\r\n";
        if (gModelTextureLoaded) {
            ss << L"Texture archive: " << gModelTexturePath << L"\r\n";
            ss << L"Texture atlas: " << widen(gModelTextureName) << L" firstIndex=" << gModelTextureIndex << L"\r\n";
            ss << L"Texture atlas size: " << gModelTextureImage.width << L"x" << gModelTextureImage.height << L"\r\n";
            ss << L"Texture atlas entries:";
            for (const auto& region : gModelTextureRegions) {
                ss << L" " << region.sourceIndex << L":" << widen(region.name);
            }
            ss << L"\r\n";
        }
        ss << L"\r\nFields\r\n------\r\n";
        for (const auto& field : gModelFile.fields()) {
            ss << widen(field.group) << L" | " << hexWide(field.offset, 6) << L" | " << widen(field.name) << L" = " << hexWide(field.value);
            if (!field.note.empty()) ss << L" | " << widen(field.note);
            ss << L"\r\n";
        }
        ss << L"\r\nParser notes\r\n------------\r\n";
        for (const auto& line : gModelFile.lines()) ss << widen(line.text) << L"\r\n";
    } else if (gMode == StorylandMode::TextureArchive) {
        ss << L"Mode: CHK/XTX/TEX texture archive\r\n\r\n";
        const auto& textures = gTextureArchive.textures();
        ss << L"Texture count: " << textures.size() << L"\r\n\r\n";
        for (size_t i = 0; i < textures.size(); ++i) {
            const auto& e = textures[i];
            ss << L"[" << i << L"] " << widen(e.name)
               << L" kind=" << (e.kind == TextureKind::CtwTex ? L"CTW TEX" : e.kind == TextureKind::Ps2 ? L"PS2" : e.kind == TextureKind::Psp ? L"PSP" : L"Unknown")
               << L" size=" << e.width << L"x" << e.height
               << L" bpp=" << int(e.bpp)
               << L" mip=" << int(e.mipCount)
               << L" container=" << hexWide(e.containerBase, 6)
               << L" header=" << hexWide(e.textureHeaderOffset, 6)
               << L" raster=" << hexWide(e.rasterOffset, 6)
               << L" block=" << e.blockSize << L"\r\n";
        }
    } else if (gMode == StorylandMode::DtzArchive) {
        ss << L"Mode: GAME.DTZ\r\n\r\n";
        ss << L"Compressed input: " << (gDtzArchive.wasCompressedInput() ? L"yes" : L"no") << L"\r\n";
        ss << L"Unpacked bytes: " << gDtzArchive.unpackedBytes().size() << L"\r\n\r\n";
        ss << L"Header fields\r\n-------------\r\n";
        for (const auto& f : gDtzArchive.headerFields()) {
            ss << hexWide(f.offset, 4) << L" | " << widen(f.name) << L" = " << hexWide(f.value) << L" | " << widen(f.note) << L"\r\n";
        }
        ss << L"\r\nDIR sector map\r\n--------------\r\n";
        for (const auto& entry : gDtzArchive.dirEntries()) {
            ss << widen(entry.name) << L" | index=" << entry.dirIndex
               << L" start=" << entry.startSector
               << L" count=" << entry.sectorCount
               << L" end=" << (entry.startSector + entry.sectorCount)
               << L" bytes=" << (entry.sectorCount * 2048u)
               << L" dtz_matches=" << entry.matchingRecordIndices.size() << L"\r\n";
        }
        ss << L"\r\nRaw sector pairs\r\n----------------\r\n";
        for (const auto& r : gDtzArchive.sectorRecords()) {
            ss << hexWide(r.recordOffset, 6) << L" | " << widen(r.resourceName)
               << L" start=" << r.startSector
               << L" count=" << r.sectorCount
               << L" end=" << (r.startSector + r.sectorCount)
               << L" startField=" << hexWide(r.startOffset, 6)
               << L" countField=" << hexWide(r.countOffset, 6)
               << L" | " << widen(r.note) << L"\r\n";
        }
    } else if (gMode == StorylandMode::ArchiveFile) {
        ss << L"Mode: LVZ + IMG archive\r\n\r\n";
        if (gArchiveBrowser.hasLvzContext()) ss << L"LVZ: " << gArchiveBrowser.lvzPath() << L"\r\n";
        if (gArchiveBrowser.hasImgContext()) ss << L"IMG: " << gArchiveBrowser.imgPath() << L"\r\n";
        ss << L"DIR: none; retail LVZ+IMG mode\r\n";
        ss << L"Summary: " << widen(gArchiveBrowser.levelSummary()) << L"\r\n";
        ss << L"IMG size: " << gArchiveBrowser.imgFileSize() << L" bytes\r\n";
        ss << L"Parsed sectors: " << gArchiveBrowser.sectors().size() << L"\r\n";
        ss << L"Parsed visible placements: " << gArchiveBrowser.placements().size() << L"\r\n";
        ss << L"Parsed real mesh resources: " << gArchiveBrowser.worldMeshes().size() << L"\r\n\r\n";

        ss << L"Sectors\r\n-------\r\n";
        for (const auto& sector : gArchiveBrowser.sectors()) {
            ss << L"sector=" << sector.sectorIndex
               << L" x=" << sector.sectorX
               << L" y=" << sector.sectorY
               << L" imgOffset=" << sector.imgOffset
               << L" bytes=" << sector.byteSize
               << L" origin=(" << sector.originX << L"," << sector.originY << L"," << sector.originZ << L")"
               << L" header=" << hexWide(sector.headerOffset, 6)
               << L"\r\n";
        }

        ss << L"\r\nVisible placements\r\n------------------\r\n";
        for (const auto& placement : gArchiveBrowser.placements()) {
            ss << L"res=" << placement.resourceIndex
               << L" ipl=" << placement.iplId
               << L" sector=" << placement.sectorIndex
               << L" sx=" << placement.sectorX
               << L" sy=" << placement.sectorY
               << L" pass=" << widen(placement.passName)
               << L" imgOffset=" << placement.imgOffset
               << L" pos=(" << placement.x << L"," << placement.y << L"," << placement.z << L")"
               << L" radius=" << placement.boundRadius
               << L"\r\n";
        }

        ss << L"\r\nReal mesh resources\r\n-------------------\r\n";
        for (const auto& mesh : gArchiveBrowser.worldMeshes()) {
            ss << L"sector=" << mesh.sectorIndex
               << L" res=" << mesh.resourceIndex
               << L" raw=0x" << std::hex << mesh.rawOffset << std::dec
               << L" materials=" << mesh.materialCount
               << L" vertices=" << mesh.vertices.size()
               << L" triangles=" << mesh.triangles.size()
               << L"\r\n";
        }

        ss << L"\r\nEntries\r\n-------\r\n";
        for (const auto& entry : gArchiveBrowser.entries()) {
            ss << widen(entry.name)
               << L" | index=" << entry.index
               << L" start=" << entry.startSector
               << L" count=" << entry.sectorCount
               << L" bytes=" << entry.byteSize
               << L" imgOffset=" << entry.byteOffset
               << L" lvzHeader=" << hexWide(entry.lvzHeaderOffset, 6)
               << L"\r\n";
        }
    } else if (gMode == StorylandMode::AnimFile) {
        ss << L"Mode: ANIM / Leeds animation\r\n";
        ss << L"Path: " << gAnimFile.sourcePath() << L"\r\n";
        ss << L"Raw bytes: " << gAnimFile.rawBytes().size() << L"\r\n";
        ss << L"Tracks: " << gAnimFile.tracks().size() << L"\r\n";
        ss << L"Frames: " << gAnimFile.frameCount() << L"\r\n";
        ss << L"FPS: " << gAnimFile.framesPerSecond() << L"\r\n";
        ss << L"Duration: " << gAnimFile.durationSeconds() << L"\r\n";
        ss << L"Current time: " << gAnimCurrentTime << L"\r\n";
        ss << L"Keyframe candidates: " << (gAnimFile.hasKeyframeCandidates() ? L"yes" : L"no") << L"\r\n\r\n";

        ss << L"Tracks\r\n------\r\n";
        for (const auto& track : gAnimFile.tracks()) {
            ss << L"#" << track.index << L" " << widen(track.name)
               << L" bone=" << track.boneIndex
               << L" parent=";
            if (track.parentIndex == 0xFFFFFFFFu) ss << L"<root>";
            else ss << track.parentIndex;
            ss << L" keys=" << track.keys.size()
               << L" offset=" << hexWide(track.offset, 6)
               << L" source=" << widen(track.source) << L"\r\n";

            size_t showKeys = std::min<size_t>(track.keys.size(), 16);
            for (size_t i = 0; i < showKeys; ++i) {
                const auto& key = track.keys[i];
                ss << L"    key[" << i << L"] t=" << key.time
                   << L" off=" << hexWide(key.offset, 6)
                   << L" T=(" << key.tx << L", " << key.ty << L", " << key.tz << L")"
                   << L" Q=(" << key.qx << L", " << key.qy << L", " << key.qz << L", " << key.qw << L")\r\n";
            }
            if (track.keys.size() > showKeys) ss << L"    ... " << (track.keys.size() - showKeys) << L" more keys\r\n";
        }

        ss << L"\r\nString/name hints\r\n-----------------\r\n";
        for (const auto& hint : gAnimFile.stringHints()) {
            ss << widen(hint) << L"\r\n";
        }

        ss << L"\r\nRaw header fields\r\n-----------------\r\n";
        for (const auto& field : gAnimFile.fields()) {
            ss << widen(field.group) << L" | " << hexWide(field.offset, 6)
               << L" | " << widen(field.name) << L" = " << hexWide(field.value)
               << L" | " << widen(field.note) << L"\r\n";
        }
    } else {
        ss << L"No file is currently loaded.\r\n";
    }

    std::wstring currentDetails = getDetailsText();
    if (!currentDetails.empty()) {
        ss << L"\r\nCurrent details panel\r\n---------------------\r\n" << currentDetails << L"\r\n";
    }
    return ss.str();
}

static void exportCurrentLog() {
    std::wstring logText = buildExportLogText();

    std::wstring initialPath = storylandTempLogPath();
    std::wstring path = saveFileDialogWithInitial(L"Text log\0*.txt\0All files\0*.*\0", L"txt", initialPath);

    if (path.empty()) {
        copyTextToClipboard(logText);

        std::string tempError;
        if (writeUtf8TextFile(initialPath, logText, tempError)) {
            MessageBoxW(
                gMainWindow,
                (L"Save dialog was cancelled or blocked. The log was copied to the clipboard and also written to:\r\n" + initialPath).c_str(),
                L"Export log fallback",
                MB_ICONINFORMATION
            );
            setStatus(L"Exported log fallback: " + initialPath);
        } else {
            MessageBoxW(
                gMainWindow,
                L"Save dialog was cancelled or blocked. The log was copied to the clipboard.",
                L"Export log fallback",
                MB_ICONINFORMATION
            );
            setStatus(L"Copied log to clipboard.");
        }
        return;
    }

    std::string error;
    if (!writeUtf8TextFile(path, logText, error)) {
        copyTextToClipboard(logText);

        std::string tempError;
        if (writeUtf8TextFile(initialPath, logText, tempError)) {
            std::wstring message =
                widen(error) +
                L"\r\n\r\nStoryland also copied the log to the clipboard and wrote a fallback copy to:\r\n" +
                initialPath;
            MessageBoxW(gMainWindow, message.c_str(), L"Export log failed; fallback saved", MB_ICONWARNING);
            setStatus(L"Exported log fallback: " + initialPath);
        } else {
            std::wstring message =
                widen(error) +
                L"\r\n\r\nStoryland copied the log to the clipboard, but the temp fallback also failed:\r\n" +
                widen(tempError);
            MessageBoxW(gMainWindow, message.c_str(), L"Export log failed; copied to clipboard", MB_ICONWARNING);
            setStatus(L"Copied log to clipboard.");
        }
        return;
    }

    setStatus(L"Exported log: " + path);
}


static void changeSelectedMeshResourceId() {
    bool selectedMeshResource =
        gMode == StorylandMode::ArchiveFile &&
        gSelectedKind == StorylandTreeKind::ArchiveMeshResource &&
        gSelectedIndex >= 0 &&
        size_t(gSelectedIndex) < gArchiveMeshResourceIds.size();

    if (!selectedMeshResource) {
        MessageBoxW(
            gMainWindow,
            L"Select one of the Real mesh resources parsed from sectors first.",
            L"Storyland",
            MB_ICONINFORMATION
        );
        return;
    }

    uint32_t oldResourceId = gArchiveMeshResourceIds[size_t(gSelectedIndex)];
    uint32_t newResourceId = oldResourceId;
    bool ignoredShift = false;
    if (!askUnsigned(
        L"Change Mesh Resource ID",
        L"New resource id. This updates Resource[] rows and sGeomInstance rows:",
        newResourceId,
        newResourceId,
        false,
        ignoredShift
    )) {
        return;
    }

    std::string report;
    std::string error;
    if (!gArchiveBrowser.changeWorldMeshResourceId(oldResourceId, newResourceId, report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Change resource id failed", MB_ICONERROR);
        return;
    }

    populateArchiveList();
    rebuildArchiveResourceScrollLists();

    int newIndex = -1;
    for (size_t i = 0; i < gArchiveMeshResourceIds.size(); ++i) {
        if (gArchiveMeshResourceIds[i] == newResourceId) {
            newIndex = int(i);
            break;
        }
    }

    if (newIndex >= 0) {
        selectArchiveMeshResource(newIndex);
    }

    setDetails(widen(report));
    setStatus(L"Mesh resource id changed in memory; use File > Export/Rebuild LVZ+IMG Pair to write it.");
}

static void replaceSelectedMeshResourceWithResourceId() {
    bool selectedMeshResource =
        gMode == StorylandMode::ArchiveFile &&
        gSelectedKind == StorylandTreeKind::ArchiveMeshResource &&
        gSelectedIndex >= 0 &&
        size_t(gSelectedIndex) < gArchiveMeshResourceIds.size();

    if (!selectedMeshResource) {
        MessageBoxW(
            gMainWindow,
            L"Select one of the Real mesh resources parsed from sectors first.",
            L"Storyland",
            MB_ICONINFORMATION
        );
        return;
    }

    uint32_t targetResourceId = gArchiveMeshResourceIds[size_t(gSelectedIndex)];
    uint32_t sourceResourceId = targetResourceId;
    bool ignoredShift = false;
    if (!askUnsigned(
        L"Clone Mesh Resource",
        L"Source resource id to clone into the selected resource:",
        sourceResourceId,
        sourceResourceId,
        false,
        ignoredShift
    )) {
        return;
    }

    if (sourceResourceId == targetResourceId) {
        MessageBoxW(gMainWindow, L"Source and target resource ids are the same.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    std::vector<uint8_t> replacementBytes;
    std::string error;
    if (!gArchiveBrowser.extractWorldMeshResourceBytes(sourceResourceId, replacementBytes, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Mesh resource clone failed", MB_ICONERROR);
        return;
    }

    std::string report;
    if (!gArchiveBrowser.replaceWorldMeshResourceBytes(targetResourceId, replacementBytes, report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Mesh resource clone failed", MB_ICONERROR);
        return;
    }

    populateArchiveList();
    rebuildArchiveResourceScrollLists();

    int newIndex = -1;
    for (size_t i = 0; i < gArchiveMeshResourceIds.size(); ++i) {
        if (gArchiveMeshResourceIds[i] == targetResourceId) {
            newIndex = int(i);
            break;
        }
    }

    if (newIndex >= 0) {
        selectArchiveMeshResource(newIndex);
    }

    std::wstring details = widen(report);
    details += L"\r\n\r\nCloned from existing real mesh resource id ";
    details += std::to_wstring(sourceResourceId);
    details += L".";
    setDetails(details);

    setStatus(L"Sector mesh resource cloned in memory; use File > Export/Rebuild LVZ+IMG Pair to write it.");
}

static void replaceSelectedArchiveResourceFromFile() {
    bool selectedArchiveEntry =
        gMode == StorylandMode::ArchiveFile &&
        gSelectedKind == StorylandTreeKind::ArchiveEntry &&
        gSelectedIndex >= 0;

    bool selectedMeshResource =
        gMode == StorylandMode::ArchiveFile &&
        gSelectedKind == StorylandTreeKind::ArchiveMeshResource &&
        gSelectedIndex >= 0 &&
        size_t(gSelectedIndex) < gArchiveMeshResourceIds.size();

    if (!selectedArchiveEntry && !selectedMeshResource) {
        MessageBoxW(
            gMainWindow,
            L"Select a concrete LVZ+IMG entry or one of the Real mesh resources parsed from sectors first.",
            L"Storyland",
            MB_ICONINFORMATION
        );
        return;
    }

    std::wstring path = openFileDialog(L"BLeeds / Leeds resource\0*.mdl;*.dff;*.wbl;*.xtx;*.chk;*.tex;*.wrld;*.area;*.bin\0All files\0*.*\0");
    if (path.empty()) return;

    std::vector<uint8_t> replacementBytes;
    std::string error;
    if (!readBinaryFileForUi(path, replacementBytes, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Replacement load failed", MB_ICONERROR);
        return;
    }

    std::string report;
    if (selectedMeshResource) {
        uint32_t resourceId = gArchiveMeshResourceIds[size_t(gSelectedIndex)];
        if (!gArchiveBrowser.replaceWorldMeshResourceBytes(resourceId, replacementBytes, report, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"LVZ+IMG mesh resource replacement failed", MB_ICONERROR);
            return;
        }

        populateArchiveList();
        rebuildArchiveResourceScrollLists();

        int newIndex = -1;
        for (size_t i = 0; i < gArchiveMeshResourceIds.size(); ++i) {
            if (gArchiveMeshResourceIds[i] == resourceId) {
                newIndex = int(i);
                break;
            }
        }

        if (newIndex >= 0) {
            selectArchiveMeshResource(newIndex);
            setDetails(widen(report));
        } else {
            setDetails(widen(report) + L"\r\n\r\nThe replacement no longer parses as a visible sector mesh resource, so it is not in the mesh resource list after rebuild.");
            InvalidateRect(gPreview, nullptr, TRUE);
        }

        setStatus(L"Sector mesh resource replaced in memory; use File > Export/Rebuild LVZ+IMG Pair to write it.");
        return;
    }

    if (!gArchiveBrowser.replaceEntryBytes(size_t(gSelectedIndex), replacementBytes, report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"LVZ+IMG replacement failed", MB_ICONERROR);
        return;
    }

    populateArchiveList();
    setDetails(widen(report));
    setStatus(L"LVZ+IMG resource replaced in memory; use File > Export/Rebuild LVZ+IMG Pair to write it.");
}

static void exportSelectedResourceBytes() {
    std::vector<uint8_t> bytes;
    std::wstring suggestedName;
    std::string error;

    if (gMode == StorylandMode::ArchiveFile &&
        gSelectedKind == StorylandTreeKind::ArchiveEntry && gSelectedIndex >= 0 &&
        size_t(gSelectedIndex) < gArchiveBrowser.entries().size()) {
        const StorylandArchiveEntry& entry = gArchiveBrowser.entries()[size_t(gSelectedIndex)];
        suggestedName = widen(entry.name);
        if (!gArchiveBrowser.extractEntryBytes(size_t(gSelectedIndex), bytes, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Resource export failed", MB_ICONERROR);
            return;
        }
    } else if (gMode == StorylandMode::ArchiveFile &&
               gSelectedKind == StorylandTreeKind::ArchiveMeshResource && gSelectedIndex >= 0 &&
               size_t(gSelectedIndex) < gArchiveMeshResourceIds.size()) {
        const uint32_t resourceId = gArchiveMeshResourceIds[size_t(gSelectedIndex)];
        suggestedName = L"resource_" + std::to_wstring(resourceId) + L".wrld";
        if (!gArchiveBrowser.extractWorldMeshResourceBytes(resourceId, bytes, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Mesh resource export failed", MB_ICONERROR);
            return;
        }
    } else if (gMode == StorylandMode::DtzArchive &&
               gSelectedKind == StorylandTreeKind::DtzDirEntry && gSelectedIndex >= 0 &&
               size_t(gSelectedIndex) < gDtzArchive.dirEntries().size()) {
        suggestedName = canonicalDtzImgResourceName(widen(gDtzArchive.dirEntries()[size_t(gSelectedIndex)].name));
        if (!gDtzArchive.extractDirEntryBytes(size_t(gSelectedIndex), bytes, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"DTZ stream export failed", MB_ICONERROR);
            return;
        }
    } else {
        MessageBoxW(gMainWindow, L"Select an LVZ+IMG entry, placed mesh resource, or GAME.DTZ internal stream first.", L"Export Selected Resource", MB_ICONINFORMATION);
        return;
    }

    if (suggestedName.empty()) suggestedName = L"storyland_resource.bin";
    std::wstring outputPath = saveFileDialogWithInitial(
        L"Leeds resource\0*.mdl;*.dff;*.wbl;*.xtx;*.chk;*.tex;*.wrld;*.area;*.anim;*.bin\0All files\0*.*\0",
        L"bin",
        suggestedName
    );
    if (outputPath.empty()) return;
    if (!writeWholeFileBinary(outputPath, bytes, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Resource export failed", MB_ICONERROR);
        return;
    }
    setStatus(L"Exported and verified selected resource: " + outputPath + L" (" + std::to_wstring(bytes.size()) + L" bytes)");
}

static bool loadedModelUsesPspGeometry() {
    for (const StorylandModelField& field : gModelFile.fields()) {
        if (field.group.rfind("sPspGeometry @", 0) == 0 ||
            field.group.rfind("sPspGeometryMesh #", 0) == 0) return true;
    }
    return false;
}

static void runSelectedDmaTlbPreflight() {
    std::vector<uint8_t> bytes;
    std::string label;
    std::string error;
    bool loadedModel = false;

    if (gMode == StorylandMode::ModelFile && !gModelFile.sourcePath().empty()) {
        loadedModel = true;
        label = "loaded model: " + narrow(gModelFile.sourcePath());
        if (!readBinaryFileForUi(gModelFile.sourcePath(), bytes, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"DMA/VIF Test", MB_ICONERROR);
            return;
        }
    } else if (gMode == StorylandMode::ArchiveFile &&
               gSelectedKind == StorylandTreeKind::ArchiveMeshResource && gSelectedIndex >= 0 &&
               size_t(gSelectedIndex) < gArchiveMeshResourceIds.size()) {
        const uint32_t resourceId = gArchiveMeshResourceIds[size_t(gSelectedIndex)];
        label = "LVZ/IMG mesh resource " + std::to_string(resourceId);
        if (!gArchiveBrowser.extractWorldMeshResourceBytes(resourceId, bytes, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"DMA/VIF Test", MB_ICONERROR);
            return;
        }
    } else if (gMode == StorylandMode::ArchiveFile &&
               gSelectedKind == StorylandTreeKind::ArchiveEntry && gSelectedIndex >= 0 &&
               size_t(gSelectedIndex) < gArchiveBrowser.entries().size()) {
        label = "LVZ/IMG entry: " + gArchiveBrowser.entries()[size_t(gSelectedIndex)].name;
        if (!gArchiveBrowser.extractEntryBytes(size_t(gSelectedIndex), bytes, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"DMA/VIF Test", MB_ICONERROR);
            return;
        }
    } else if (gMode == StorylandMode::DtzArchive &&
               gSelectedKind == StorylandTreeKind::DtzDirEntry && gSelectedIndex >= 0 &&
               size_t(gSelectedIndex) < gDtzArchive.dirEntries().size()) {
        label = "GAME.DTZ IMG stream: " + gDtzArchive.dirEntries()[size_t(gSelectedIndex)].name;
        if (!gDtzArchive.extractDirEntryBytes(size_t(gSelectedIndex), bytes, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"DMA/VIF Test", MB_ICONERROR);
            return;
        }
    } else {
        MessageBoxW(
            gMainWindow,
            L"Open an MDL, or select an LVZ/IMG entry, placed mesh resource, or GAME.DTZ internal model stream first.",
            L"DMA/VIF Test",
            MB_ICONINFORMATION
        );
        return;
    }

    bool overallSafe = true;
    std::string details;
    const bool pspModel = loadedModel && loadedModelUsesPspGeometry();
    if (loadedModel) {
        const StorylandModelIntegrityReport modelReport = storylandValidateModelIntegrity(gModelFile, bytes, label);
        overallSafe = modelReport.safe();
        details = modelReport.text() + "\r\n";
    }
    if (pspModel) {
        const StorylandPspDmaReport pspReport = storylandValidatePspDmaGe(gModelFile, bytes, label);
        overallSafe = overallSafe && pspReport.safe();
        details += pspReport.text();
        setStatus(overallSafe ? L"DMA/VIF | PSP GE streams OK" : L"DMA/VIF | PSP stream fault");
    } else {
        const StorylandDmaTlbReport dmaReport = storylandValidatePs2DmaTlb(bytes, label);
        overallSafe = overallSafe && dmaReport.safe();
        details += dmaReport.text();
        setStatus(overallSafe
            ? (dmaReport.dmaTags == 0 ? L"DMA/VIF | PS2 limited" : L"DMA/VIF | PS2 OK")
            : L"DMA/VIF | PS2 fault");
    }
    setDetails(widen(details));
    if (!overallSafe) {
        MessageBoxW(
            gMainWindow,
            loadedModel
                ? L"Model or stream test failed. Check the details panel."
                : L"DMA/VIF test failed. Check the details panel.",
            L"DMA/VIF Test Failed",
            MB_ICONERROR
        );
    }
}


static void exportLvzImgPair() {
    if (gMode != StorylandMode::ArchiveFile || !gArchiveBrowser.hasLvzContext() || !gArchiveBrowser.hasImgContext()) {
        MessageBoxW(gMainWindow, L"Open a retail LVZ+IMG pair first.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    std::wstring lvzPath = saveFileDialog(L"LVZ file\0*.lvz\0All files\0*.*\0", L"lvz");
    if (lvzPath.empty()) return;

    std::wstring defaultImgPath = sameFolderSameStemWithExtension(lvzPath, L".img");
    std::wstring imgPath = saveFileDialogWithInitial(
        L"IMG file\0*.img\0All files\0*.*\0",
        L"img",
        defaultImgPath
    );
    if (imgPath.empty()) return;

    if (lvzPath == imgPath) {
        MessageBoxW(gMainWindow, L"The LVZ output path and IMG output path cannot be the same file.", L"Export LVZ+IMG Pair", MB_ICONERROR);
        return;
    }

    bool compressLvz = MessageBoxW(
        gMainWindow,
        L"Save LVZ compressed with zlib?\n\nChoose Yes for a game-style .LVZ.\nChoose No to save the inflated raw LVZ for inspection.",
        L"Export LVZ+IMG Pair",
        MB_YESNO | MB_ICONQUESTION
    ) == IDYES;

    std::string error;
    if (!gArchiveBrowser.saveLvzImgPair(lvzPath, imgPath, compressLvz, error)) {
        std::wstring message = widen(error);
        message += L"\r\n\r\nIf this says Access is denied for the .IMG, export to a new filename such as mainla_patched.img, or close anything using mainla.img.";
        MessageBoxW(gMainWindow, message.c_str(), L"LVZ+IMG export failed", MB_ICONERROR);
        return;
    }

    setStatus(L"Exported and verified LVZ+IMG transaction: " + lvzPath + L" + " + imgPath);
}


static void overwriteCurrentLvzImgPair() {
    if (gMode != StorylandMode::ArchiveFile || !gArchiveBrowser.hasLvzContext() || !gArchiveBrowser.hasImgContext()) {
        MessageBoxW(gMainWindow, L"Open a retail LVZ+IMG pair first.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    if (MessageBoxW(
        gMainWindow,
        L"This will overwrite the currently opened .LVZ and .IMG files.\n\nIf PCSX2, another tool, or the game folder has the .IMG locked, this will fail with Access is denied.\n\nContinue?",
        L"Overwrite LVZ+IMG Pair",
        MB_YESNO | MB_ICONWARNING
    ) != IDYES) {
        return;
    }

    bool compressLvz = MessageBoxW(
        gMainWindow,
        L"Write the LVZ compressed with zlib?\n\nChoose Yes for normal game-style LVZ.",
        L"Overwrite LVZ+IMG Pair",
        MB_YESNO | MB_ICONQUESTION
    ) == IDYES;

    std::string error;
    if (!gArchiveBrowser.overwriteCurrentLvzImgPair(compressLvz, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Overwrite LVZ+IMG failed", MB_ICONERROR);
        return;
    }

    setStatus(L"Overwrote and verified current LVZ+IMG pair transaction.");
}

static void exportEditedTextureArchive() {
    if (gMode != StorylandMode::TextureArchive) {
        MessageBoxW(gMainWindow, L"Open a .chk/.xtx/.tex texture archive first.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    std::wstring path = saveFileDialog(L"Texture archive\0*.chk;*.xtx;*.tex\0All files\0*.*\0", L"xtx");
    if (path.empty()) return;

    std::string error;
    if (!gTextureArchive.saveToFile(path, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Export texture archive failed", MB_ICONERROR);
        return;
    }

    setStatus(L"Exported edited texture archive: " + path);
}

static void saveCurrentAs() {
    std::string error;
    if (gMode == StorylandMode::TextureArchive) {
        std::wstring path = saveFileDialog(L"Texture archive\0*.chk;*.xtx;*.tex\0All files\0*.*\0", L"xtx");
        if (!path.empty() && !gTextureArchive.saveToFile(path, error)) MessageBoxW(gMainWindow, widen(error).c_str(), L"Save failed", MB_ICONERROR);
    } else if (gMode == StorylandMode::DtzArchive) {
        std::wstring path = saveFileDialog(L"DTZ raw or compressed\0*.dtz;*.bin\0All files\0*.*\0", L"dtz");
        if (!path.empty()) {
            bool compress = MessageBoxW(gMainWindow, L"Save as compressed zlib DTZ?\n\nChoose No to save raw GTAG bytes. Compressed output is checked before it is written. If an IMG is loaded, it is saved beside the new GAME.DTZ.", L"Storyland DTZ Save", MB_YESNO | MB_ICONQUESTION) == IDYES;
            if (!gDtzArchive.saveToFile(path, compress, error)) {
                MessageBoxW(gMainWindow, widen(error).c_str(), L"Save failed", MB_ICONERROR);
            } else if (gDtzArchive.hasCompanionImg()) {
                setStatus(gDtzArchive.hasCompanionDir()
                    ? L"Saved and verified rebuilt GAME.DTZ + IMG + explicitly loaded beta-build DIR transaction."
                    : L"Saved and verified rebuilt retail GAME.DTZ + companion IMG transaction.");
            } else {
                setStatus(L"Saved and verified rebuilt GAME.DTZ.");
            }
        }
    }
}

static void exportSelectedTexture() {
    if (gMode != StorylandMode::TextureArchive || gSelectedIndex < 0) return;
    std::wstring path = saveFileDialog(L"PNG image\0*.png\0All files\0*.*\0", L"png");
    if (path.empty()) return;
    std::string error;
    if (!savePngWithWic(path, gCurrentImage, error)) MessageBoxW(gMainWindow, widen(error).c_str(), L"Export failed", MB_ICONERROR);
}

static void replaceSelectedTexture() {
    if (gMode != StorylandMode::TextureArchive || gSelectedIndex < 0) return;
    std::wstring path = openFileDialog(L"Image\0*.png;*.bmp;*.jpg;*.jpeg;*.tif;*.tiff\0All files\0*.*\0");
    if (path.empty()) return;

    RgbaImage image;
    std::string error;
    if (!loadImageWithWic(path, image, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Image load failed", MB_ICONERROR);
        return;
    }

    uint32_t targetBpp = 0;
    bool ignoredShift = false;
    if (!askUnsigned(
        L"Replacement BPP",
        L"Target BPP: 0 keeps current, or use 4, 8, 16, 32. Use 8/32 to avoid palette crushing.",
        targetBpp,
        targetBpp,
        false,
        ignoredShift
    )) {
        return;
    }

    if (!gTextureArchive.replaceTextureAsBpp(size_t(gSelectedIndex), image, uint8_t(targetBpp), error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Replace failed", MB_ICONERROR);
        return;
    }

    populateTextureList();
    int nextIndex = std::min<int>(gSelectedIndex, int(gTextureArchive.textures().size()) - 1);
    if (nextIndex >= 0) selectTexture(nextIndex);
    setStatus(L"Texture changed | right-click > Export edited texture archive");
}

static void renameSelectedTexture() {
    if (gMode != StorylandMode::TextureArchive || gSelectedIndex < 0) return;

    const auto& textures = gTextureArchive.textures();
    if (size_t(gSelectedIndex) >= textures.size()) return;

    std::wstring currentName = widen(textures[size_t(gSelectedIndex)].name);
    std::wstring newNameWide;
    if (!askString(L"Rename Texture", L"New texture name, max 63 printable ASCII characters:", currentName, newNameWide)) {
        return;
    }

    std::string newName = narrowAscii(newNameWide);
    if (newName.empty()) {
        MessageBoxW(gMainWindow, L"Texture name must be printable ASCII and cannot be empty.", L"Rename failed", MB_ICONERROR);
        return;
    }

    std::string error;
    if (!gTextureArchive.renameTexture(size_t(gSelectedIndex), newName, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Rename failed", MB_ICONERROR);
        return;
    }

    populateTextureList();
    int nextIndex = std::min<int>(gSelectedIndex, int(gTextureArchive.textures().size()) - 1);
    if (nextIndex >= 0) selectTexture(nextIndex);
    setStatus(L"Texture renamed | right-click > Export edited texture archive");
}

static void openCompanionDirForCurrentDtz() {
    if (gMode != StorylandMode::DtzArchive) {
        MessageBoxW(gMainWindow, L"Open GAME.DTZ first. Retail LCS/VCS use GAME.DTZ; load a .dir only for a beta-build archive that actually has one.", L"Storyland", MB_ICONINFORMATION);
        return;
    }
    std::wstring path = openFileDialog(L"GTA Stories DIR\0*.dir\0All files\0*.*\0");
    if (path.empty()) return;
    std::string error;
    if (!gDtzArchive.loadCompanionDir(path, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"DIR open failed", MB_ICONERROR);
        return;
    }
    populateDtzList();
    setStatus(L"Loaded optional beta-build .dir: " + path);
}

static void openCompanionImgForCurrentDtz() {
    if (gMode != StorylandMode::DtzArchive) {
        MessageBoxW(gMainWindow, L"Open GAME.DTZ first, then load the matching gta3PS2.img or gta3PSP.img.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    std::wstring path = openFileDialog(L"GTA Stories IMG\0*.img\0All files\0*.*\0");
    if (path.empty()) return;

    std::string error;
    if (!gDtzArchive.loadCompanionImg(path, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"IMG open failed", MB_ICONERROR);
        return;
    }

    populateDtzList();
    SetWindowTextW(gMainWindow, L"Storyland - GAME.DTZ + gta3PS*.img");
    applyStorylandTitleTintForPath(path);
    setStatus(L"Loaded companion IMG for GAME.DTZ sector map: " + path);
}

static void showAboutDialog() {
    MessageBoxW(
        gMainWindow,
        L"Storyland 1.0.3\r\n\r\nauthor: spicybung\r\nhttps://github.com/spicybung/BLeeds\r\nReigns Studios\r\n\r\nAn analyzer, editor, and viewer for Grand Theft Auto Stories file formats.",
        L"About Storyland",
        MB_OK | MB_ICONINFORMATION
    );
}

static bool selectedDtzEntryLooksReplaceable() {
    if (gMode != StorylandMode::DtzArchive) return false;
    if (gSelectedKind != StorylandTreeKind::DtzDirEntry || gSelectedIndex < 0) return false;
    const auto& entries = gDtzArchive.dirEntries();
    if (size_t(gSelectedIndex) >= entries.size()) return false;
    std::wstring name = widen(entries[size_t(gSelectedIndex)].name);
    std::wstring ext = getExtensionLower(name);
    return ext == L".mdl" || ext == L".dff" ||
           ext == L".xtx" || ext == L".chk" || ext == L".tex" ||
           ext == L".anim" || ext == L".cam" || ext == L".cut" ||
           ext == L".col" || ext == L".bin";
}

static void replaceSelectedDtzDirEntryFromFile() {
    if (gMode != StorylandMode::DtzArchive || gSelectedKind != StorylandTreeKind::DtzDirEntry || gSelectedIndex < 0) {
        MessageBoxW(gMainWindow, L"Select a GAME.DTZ internal IMG entry first.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    const auto& entries = gDtzArchive.dirEntries();
    if (size_t(gSelectedIndex) >= entries.size()) return;

    uint32_t selectedStartSector = entries[size_t(gSelectedIndex)].startSector;
    std::wstring selectedNameBefore = widen(entries[size_t(gSelectedIndex)].name);

    static const wchar_t replacementFilter[] =
        L"Leeds / Stories resource\0*.mdl;*.dff;*.xtx;*.chk;*.tex;*.anim;*.cam;*.cut;*.col;*.col2;*.bin\0"
        L"Model files\0*.mdl;*.dff\0"
        L"Texture archives\0*.xtx;*.chk;*.tex\0"
        L"Animation / cutscene / collision\0*.anim;*.cam;*.cut;*.col;*.col2\0"
        L"All files\0*.*\0";
    std::wstring replacementPath = openFileDialog(replacementFilter);
    if (replacementPath.empty()) return;

    std::vector<uint8_t> replacementBytes;
    std::string error;
    if (!readBinaryFileForUi(replacementPath, replacementBytes, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"DTZ+IMG replacement load failed", MB_ICONERROR);
        return;
    }

    std::string report;
    if (!gDtzArchive.replaceDirEntryBytes(size_t(gSelectedIndex), replacementBytes, true, report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"DTZ+IMG replacement failed", MB_ICONERROR);
        return;
    }

    populateDtzList();


    int replacementPreviewIndex = -1;
    const auto& updatedEntries = gDtzArchive.dirEntries();
    for (size_t index = 0; index < updatedEntries.size(); ++index) {
        if (updatedEntries[index].startSector == selectedStartSector) {
            replacementPreviewIndex = int(index);
            break;
        }
    }

    std::wstring detailText = widen(report);
    if (replacementPreviewIndex >= 0) {
        selectDtzDirEntry(replacementPreviewIndex);
        detailText += L"\r\n\r\nRe-selected edited entry for preview: ";
        detailText += widen(updatedEntries[size_t(replacementPreviewIndex)].name);
        detailText += L" at start sector ";
        detailText += std::to_wstring(selectedStartSector);
    } else {
        detailText += L"\r\n\r\nEdited entry ";
        detailText += selectedNameBefore;
        detailText += L" was patched, but Storyland could not find the same start sector after rebuilding the browser list.";
    }

    setDetails(detailText);
    setStatus(L"Internal DTZ+IMG entry replaced in memory; use File > Save As / Rebuild GAME.DTZ to write GAME.DTZ and the companion IMG.");
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void patchSelectedDtzRecord() {
    if (gMode != StorylandMode::DtzArchive || gSelectedIndex < 0) return;

    uint32_t newCount = 0;
    if (gSelectedKind == StorylandTreeKind::DtzDirEntry) {
        const auto& entries = gDtzArchive.dirEntries();
        if (size_t(gSelectedIndex) >= entries.size()) return;
        newCount = entries[size_t(gSelectedIndex)].sectorCount;
    } else if (gSelectedKind == StorylandTreeKind::DtzSectorRecord) {
        const auto& records = gDtzArchive.sectorRecords();
        if (size_t(gSelectedIndex) >= records.size()) return;
        newCount = records[size_t(gSelectedIndex)].sectorCount;
    } else {
        MessageBoxW(gMainWindow, L"Select a gta3PS2.img sector-map line or a raw DTZ sector record first.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    bool shift = true;
    if (!askUnsigned(L"Patch Sector Count", L"New sector count:", newCount, newCount, true, shift)) return;

    std::string report, error;
    bool ok = false;
    if (gSelectedKind == StorylandTreeKind::DtzDirEntry) {
        ok = gDtzArchive.patchDirEntry(size_t(gSelectedIndex), newCount, shift, report, error);
    } else {
        ok = gDtzArchive.patchSectorRecord(size_t(gSelectedIndex), newCount, shift, report, error);
    }

    if (!ok) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Patch failed", MB_ICONERROR);
        return;
    }
    populateDtzList();
    setDetails(widen(report));
}

static void patchPlrPair23() {
    if (gMode != StorylandMode::DtzArchive) return;
    uint32_t newCount = 68;
    bool shift = true;
    if (!askUnsigned(L"Patch PLR-style pair", L"New sector count for old pair start=23 count=44:", newCount, newCount, true, shift)) return;
    std::string report, error;
    if (!gDtzArchive.patchExactSectorPair(23, 44, newCount, shift, report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Patch failed", MB_ICONERROR);
        return;
    }
    populateDtzList();
    setDetails(widen(report));
}

static std::wstring editableDtzFieldInitialText(const StorylandDtzDataField& field) {
    std::string value = field.valueText;
    size_t slash = value.find(" /");
    if (slash != std::string::npos) value = value.substr(0, slash);
    size_t raw = value.find(" raw=");
    if (raw != std::string::npos) value = value.substr(0, raw);
    return widen(value);
}

static void patchSelectedDtzDataField() {
    if (gMode != StorylandMode::DtzArchive || gSelectedKind != StorylandTreeKind::DtzDataField || gSelectedIndex < 0) {
        MessageBoxW(gMainWindow, L"Select an editable GAME.DTZ data field first.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    const auto& fields = gDtzArchive.dataFields();
    if (size_t(gSelectedIndex) >= fields.size()) return;
    const auto& field = fields[size_t(gSelectedIndex)];
    if (!field.editable) {
        MessageBoxW(gMainWindow, L"Selected GAME.DTZ data field is view-only in this patch.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    std::wstring newValue = editableDtzFieldInitialText(field);
    std::wstring label = std::wstring(L"New value for ") + widen(field.name) + L" (" + widen(field.type) + L")";
    if (!askString(L"Patch GAME.DTZ Data Field", label.c_str(), newValue, newValue)) return;

    std::string report;
    std::string error;
    if (!gDtzArchive.patchDataField(size_t(gSelectedIndex), narrow(newValue), report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"GAME.DTZ data patch failed", MB_ICONERROR);
        return;
    }

    populateDtzList();
    setDetails(widen(report));
}


static void drawTexturePreview(HDC dc, RECT rc) {
    FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    if (!gTextureBitmap) return;
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, gTextureBitmap);
    int srcW = gCurrentImage.width;
    int srcH = gCurrentImage.height;
    int dstW = rc.right - rc.left;
    int dstH = rc.bottom - rc.top;
    double scale = std::min(double(dstW) / std::max(1, srcW), double(dstH) / std::max(1, srcH));
    int outW = std::max(1, int(srcW * scale));
    int outH = std::max(1, int(srcH * scale));
    int x = rc.left + (dstW - outW) / 2;
    int y = rc.top + (dstH - outH) / 2;
    SetStretchBltMode(dc, HALFTONE);
    if (gTexturePreviewFlipV) {
        StretchBlt(dc, x, y, outW, outH, mem, 0, srcH - 1, srcW, -srcH, SRCCOPY);
    } else {
        StretchBlt(dc, x, y, outW, outH, mem, 0, 0, srcW, srcH, SRCCOPY);
    }
    SelectObject(mem, old);
    DeleteDC(mem);
}


static void drawArchivePreviewBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ, bool filled) {
    if (filled) {
        glBegin(GL_QUADS);
        glVertex3f(minX, minY, minZ); glVertex3f(maxX, minY, minZ); glVertex3f(maxX, minY, maxZ); glVertex3f(minX, minY, maxZ);
        glVertex3f(minX, maxY, minZ); glVertex3f(minX, maxY, maxZ); glVertex3f(maxX, maxY, maxZ); glVertex3f(maxX, maxY, minZ);
        glVertex3f(minX, minY, minZ); glVertex3f(minX, maxY, minZ); glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, minY, minZ);
        glVertex3f(maxX, minY, minZ); glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, maxY, maxZ); glVertex3f(maxX, minY, maxZ);
        glVertex3f(maxX, minY, maxZ); glVertex3f(maxX, maxY, maxZ); glVertex3f(minX, maxY, maxZ); glVertex3f(minX, minY, maxZ);
        glVertex3f(minX, minY, maxZ); glVertex3f(minX, maxY, maxZ); glVertex3f(minX, maxY, minZ); glVertex3f(minX, minY, minZ);
        glEnd();
    }

    glBegin(GL_LINES);
    glVertex3f(minX, minY, minZ); glVertex3f(maxX, minY, minZ);
    glVertex3f(maxX, minY, minZ); glVertex3f(maxX, minY, maxZ);
    glVertex3f(maxX, minY, maxZ); glVertex3f(minX, minY, maxZ);
    glVertex3f(minX, minY, maxZ); glVertex3f(minX, minY, minZ);

    glVertex3f(minX, maxY, minZ); glVertex3f(maxX, maxY, minZ);
    glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, maxY, maxZ);
    glVertex3f(maxX, maxY, maxZ); glVertex3f(minX, maxY, maxZ);
    glVertex3f(minX, maxY, maxZ); glVertex3f(minX, maxY, minZ);

    glVertex3f(minX, minY, minZ); glVertex3f(minX, maxY, minZ);
    glVertex3f(maxX, minY, minZ); glVertex3f(maxX, maxY, minZ);
    glVertex3f(maxX, minY, maxZ); glVertex3f(maxX, maxY, maxZ);
    glVertex3f(minX, minY, maxZ); glVertex3f(minX, maxY, maxZ);
    glEnd();
}

static void drawArchivePreviewOpenGl(HWND hwnd, HDC dc, RECT rc) {
    if (!gOpenGlReady && !initializeOpenGlPreview(hwnd)) {
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        TextOutW(dc, rc.left + 8, rc.top + 8, L"OpenGL init failed for LVZ/IMG preview", 38);
        return;
    }

    if (!wglMakeCurrent(dc, gOpenGlContext)) {
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        TextOutW(dc, rc.left + 8, rc.top + 8, L"OpenGL context activation failed", 33);
        return;
    }

    int width = std::max<int>(1, static_cast<int>(rc.right - rc.left));
    int height = std::max<int>(1, static_cast<int>(rc.bottom - rc.top));
    glViewport(0, 0, width, height);
    setStoriesViewportClearColor();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);

    if (gSelectedKind == StorylandTreeKind::ArchiveDirectTexture &&
        gSelectedIndex >= 0 &&
        size_t(gSelectedIndex) < gArchiveBrowser.directTextures().size()) {
        const auto& texture = gArchiveBrowser.directTextures()[size_t(gSelectedIndex)];

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, double(width), double(height), 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        int imageWidth = std::max(1, texture.width);
        int imageHeight = std::max(1, texture.height);
        float scaleX = float(width - 32) / float(imageWidth);
        float scaleY = float(height - 32) / float(imageHeight);
        float drawScale = std::max(1.0f, std::min(scaleX, scaleY));
        float drawWidth = float(imageWidth) * drawScale;
        float drawHeight = float(imageHeight) * drawScale;
        float x = (float(width) - drawWidth) * 0.5f;
        float y = (float(height) - drawHeight) * 0.5f;

        GLuint previewTexture = 0;
        glGenTextures(1, &previewTexture);
        glBindTexture(GL_TEXTURE_2D, previewTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            imageWidth,
            imageHeight,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            texture.rgba.data()
        );

        glEnable(GL_TEXTURE_2D);
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        if (gTexturePreviewFlipV) {
            glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(x + drawWidth, y);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(x + drawWidth, y + drawHeight);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y + drawHeight);
        } else {
            glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(x + drawWidth, y);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(x + drawWidth, y + drawHeight);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y + drawHeight);
        }
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &previewTexture);

        glFlush();
        SwapBuffers(dc);
        wglMakeCurrent(nullptr, nullptr);

        std::wstring title = L"OpenGL LVZ/AREA texture viewport  |  ";
        title += widen(texture.name);
        title += L"  |  " + std::to_wstring(texture.width) + L"x" + std::to_wstring(texture.height);
        title += L"  |  bpp=" + std::to_wstring(texture.bpp);
        title += L"  |  header=" + hexWide(texture.headerOffset, 6);
        title += L"  |  no .DIR";
            return;
    }

    double aspect = double(width) / double(height);
    setPerspectiveProjection(45.0, aspect, 0.05, 2000.0);
    drawStoriesViewportSky(100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float archiveCameraDistance = gModelDistance;
    if (archiveCameraDistance < 0.05f) archiveCameraDistance = 0.05f;
    glTranslatef(gModelPanX, gModelPanY, -archiveCameraDistance);

    const auto& entries = gArchiveBrowser.entries();
    const auto& placements = gArchiveBrowser.placements();
    const auto& sectors = gArchiveBrowser.sectors();
    const auto& meshes = gArchiveBrowser.worldMeshes();

    bool selectedEntry = gSelectedKind == StorylandTreeKind::ArchiveEntry &&
                         gSelectedIndex >= 0 &&
                         size_t(gSelectedIndex) < entries.size();

    int selectedSectorIndex = -1;
    int selectedResourceIndex = -1;
    int selectedTextureId = -1;
    std::string selectedExt;
    std::string selectedName;

    if (selectedEntry) {
        const auto& selected = entries[size_t(gSelectedIndex)];
        selectedExt = archiveEntryExtensionLower(selected.name);
        selectedName = selected.name;

        if (selectedExt == ".wrld" || selectedExt == ".area") {
            for (const auto& sector : sectors) {
                if (sector.imgOffset == selected.byteOffset || sector.headerOffset == selected.lvzHeaderOffset) {
                    selectedSectorIndex = int(sector.sectorIndex);
                    break;
                }
            }
        } else if (selectedExt == ".mdl" || selectedExt == ".dff") {
            selectedResourceIndex = gSelectedIndex;
        }
    } else if (gSelectedKind == StorylandTreeKind::ArchiveMeshResource &&
               gSelectedIndex >= 0 &&
               size_t(gSelectedIndex) < gArchiveMeshResourceIds.size()) {
        selectedResourceIndex = int(gArchiveMeshResourceIds[size_t(gSelectedIndex)]);
        selectedName = "resource_" + std::to_string(selectedResourceIndex);
    } else if (gSelectedKind == StorylandTreeKind::ArchiveTextureResource &&
               gSelectedIndex >= 0 &&
               size_t(gSelectedIndex) < gArchiveTextureIds.size()) {
        selectedTextureId = int(gArchiveTextureIds[size_t(gSelectedIndex)]);
        selectedName = "texture_" + std::to_string(selectedTextureId);
    } else if (gSelectedKind == StorylandTreeKind::ArchiveAnimationResource) {
        selectedName = "animation";
    }

    struct PreviewPoint {
        float x;
        float y;
        float z;
        float radius;
        uint32_t passIndex;
        uint32_t sectorIndex;
        uint32_t resourceIndex;
        bool isSectorFallback;
    };

    std::vector<PreviewPoint> points;
    points.reserve(placements.size() + sectors.size());

    std::map<uint64_t, const StorylandWorldMesh*> meshByKey;
    for (const auto& mesh : meshes) {
        uint64_t key = (uint64_t(mesh.sectorIndex) << 32) | uint64_t(mesh.resourceIndex);
        if (meshByKey.find(key) == meshByKey.end()) meshByKey[key] = &mesh;
    }

    for (const auto& placement : placements) {
        if (selectedSectorIndex >= 0 && int(placement.sectorIndex) != selectedSectorIndex) continue;
        if (selectedResourceIndex >= 0 && int(placement.resourceIndex) != selectedResourceIndex) continue;
        if (selectedTextureId >= 0) {
            uint64_t key = (uint64_t(placement.sectorIndex) << 32) | uint64_t(placement.resourceIndex);
            auto meshFoundForTexture = meshByKey.find(key);
            if (meshFoundForTexture == meshByKey.end() || meshFoundForTexture->second == nullptr) continue;
            if (!meshUsesTextureId(*meshFoundForTexture->second, uint32_t(selectedTextureId))) continue;
        }

        PreviewPoint point;
        point.x = placement.x;
        point.y = placement.y;
        point.z = placement.z;
        point.radius = std::max(1.0f, std::min(20.0f, placement.boundRadius));
        point.passIndex = placement.passIndex;
        point.sectorIndex = placement.sectorIndex;
        point.resourceIndex = placement.resourceIndex;
        point.isSectorFallback = false;
        points.push_back(point);
    }

    if (points.empty()) {
        for (const auto& sector : sectors) {
            if (selectedSectorIndex >= 0 && int(sector.sectorIndex) != selectedSectorIndex) continue;

            PreviewPoint point;
            point.x = sector.originX;
            point.y = sector.originY;
            point.z = sector.originZ;
            point.radius = 8.0f;
            point.passIndex = 0;
            point.sectorIndex = sector.sectorIndex;
            point.resourceIndex = 0;
            point.isSectorFallback = true;
            points.push_back(point);
        }
    }

    if (points.empty()) {
        glFlush();
        SwapBuffers(dc);
        wglMakeCurrent(nullptr, nullptr);
        drawOpenGlTextOverlayFallback(dc, rc, L"OpenGL LVZ+IMG viewport | no parsed placements yet");
        return;
    }

    float minX = points[0].x;
    float maxX = points[0].x;
    float minY = points[0].y;
    float maxY = points[0].y;
    float minZ = points[0].z;
    float maxZ = points[0].z;

    for (const auto& point : points) {
        minX = std::min(minX, point.x - point.radius);
        maxX = std::max(maxX, point.x + point.radius);
        minY = std::min(minY, point.y - point.radius);
        maxY = std::max(maxY, point.y + point.radius);
        minZ = std::min(minZ, point.z - point.radius);
        maxZ = std::max(maxZ, point.z + point.radius);
    }

    bool shouldIncludeSectorExtents = selectedResourceIndex < 0 && selectedTextureId < 0;
    if (shouldIncludeSectorExtents) {
        for (const auto& sector : sectors) {
            if (selectedSectorIndex >= 0 && int(sector.sectorIndex) != selectedSectorIndex) continue;
            minX = std::min(minX, sector.originX - 70.0f);
            maxX = std::max(maxX, sector.originX + 70.0f);
            minY = std::min(minY, sector.originY - 70.0f);
            maxY = std::max(maxY, sector.originY + 70.0f);
            minZ = std::min(minZ, sector.originZ - 5.0f);
            maxZ = std::max(maxZ, sector.originZ + 20.0f);
        }
    }

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    float spanX = std::max(1.0f, maxX - minX);
    float spanY = std::max(1.0f, maxY - minY);
    float spanZ = std::max(1.0f, maxZ - minZ);
    float largestSpan = std::max(spanX, std::max(spanY, spanZ));
    float modelScale = 2.4f / std::max(1.0f, largestSpan);

    glMultModelQuat(gModelViewRotation);
    glScalef(modelScale, modelScale, modelScale);
    glTranslatef(-centerX, -centerY, -centerZ);

    float gridStep = largestSpan / 20.0f;
    if (gridStep < 50.0f) gridStep = 50.0f;
    float gridMinX = std::floor(minX / gridStep) * gridStep;
    float gridMaxX = std::ceil(maxX / gridStep) * gridStep;
    float gridMinY = std::floor(minY / gridStep) * gridStep;
    float gridMaxY = std::ceil(maxY / gridStep) * gridStep;

    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glColor3f(0.13f, 0.13f, 0.13f);
    for (float x = gridMinX; x <= gridMaxX; x += gridStep) {
        glVertex3f(x, gridMinY, 0.0f);
        glVertex3f(x, gridMaxY, 0.0f);
    }
    for (float y = gridMinY; y <= gridMaxY; y += gridStep) {
        glVertex3f(gridMinX, y, 0.0f);
        glVertex3f(gridMaxX, y, 0.0f);
    }
    glEnd();

    glBegin(GL_LINES);
    glColor3f(0.18f, 0.18f, 0.20f);
    if (shouldIncludeSectorExtents) {
    for (const auto& sector : sectors) {
        if (selectedSectorIndex >= 0 && int(sector.sectorIndex) != selectedSectorIndex) continue;

        float sx = sector.originX;
        float sy = sector.originY;
        float sz = sector.originZ;
        float halfX = 62.5f;
        float halfY = 54.125f;

        glVertex3f(sx - halfX, sy - halfY, sz); glVertex3f(sx + halfX, sy - halfY, sz);
        glVertex3f(sx + halfX, sy - halfY, sz); glVertex3f(sx + halfX, sy + halfY, sz);
        glVertex3f(sx + halfX, sy + halfY, sz); glVertex3f(sx - halfX, sy + halfY, sz);
        glVertex3f(sx - halfX, sy + halfY, sz); glVertex3f(sx - halfX, sy - halfY, sz);
    }
    }
    glEnd();

    size_t drawnInstances = 0;
    size_t drawnTriangles = 0;

    std::map<uint32_t, const StorylandDirectTextureResource*> boundTextureByMaterial;
    for (const auto& texture : gArchiveBrowser.directTextures()) {
        if (texture.materialId < 0 || texture.width <= 0 || texture.height <= 0 || texture.rgba.empty()) continue;
        boundTextureByMaterial.emplace(uint32_t(texture.materialId), &texture);
    }
    std::map<uint32_t, GLuint> uploadedArchiveTextures;
    auto textureForMaterial = [&](uint32_t materialId) -> GLuint {
        auto uploaded = uploadedArchiveTextures.find(materialId);
        if (uploaded != uploadedArchiveTextures.end()) return uploaded->second;
        auto found = boundTextureByMaterial.find(materialId);
        if (found == boundTextureByMaterial.end() || found->second == nullptr) return 0;
        const StorylandDirectTextureResource& texture = *found->second;
        GLuint handle = 0;
        glGenTextures(1, &handle);
        if (handle == 0) return 0;
        glBindTexture(GL_TEXTURE_2D, handle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width, texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture.rgba.data());
        uploadedArchiveTextures[materialId] = handle;
        return handle;
    };

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    bool triangleBatchOpen = false;
    GLuint activeTexture = GLuint(-1);
    uint32_t activeMaterial = 0xFFFFFFFEu;
    for (const auto& placement : placements) {
        if (selectedSectorIndex >= 0 && int(placement.sectorIndex) != selectedSectorIndex) continue;
        if (selectedResourceIndex >= 0 && int(placement.resourceIndex) != selectedResourceIndex) continue;
        uint64_t key = (uint64_t(placement.sectorIndex) << 32) | uint64_t(placement.resourceIndex);
        auto found = meshByKey.find(key);
        if (found == meshByKey.end() || found->second == nullptr) continue;
        const StorylandWorldMesh& mesh = *found->second;
        if (mesh.vertices.empty() || mesh.triangles.empty()) continue;

        drawnInstances++;
        for (const StorylandWorldMeshTriangle& tri : mesh.triangles) {
            if (selectedTextureId >= 0 && int(tri.textureId) != selectedTextureId) continue;
            if (tri.a >= mesh.vertices.size() || tri.b >= mesh.vertices.size() || tri.c >= mesh.vertices.size()) continue;
            uint32_t desiredMaterial = tri.textureId;
            GLuint textureHandle = activeTexture == GLuint(-1) ? 0 : activeTexture;
            if (desiredMaterial != activeMaterial) {
                if (triangleBatchOpen) glEnd();
                activeMaterial = desiredMaterial;
                textureHandle = desiredMaterial == 0xFFFFFFFFu ? 0 : textureForMaterial(desiredMaterial);
                activeTexture = textureHandle;
                if (textureHandle != 0) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, textureHandle);
                    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                } else {
                    glDisable(GL_TEXTURE_2D);
                }
                glBegin(GL_TRIANGLES);
                triangleBatchOpen = true;
            }
            uint32_t seed = tri.textureId == 0xFFFFFFFFu ? placement.resourceIndex : tri.textureId;
            float r = 0.30f + float((seed * 37u + 31u) & 0x7Fu) / 255.0f;
            float g = 0.30f + float((seed * 67u + 91u) & 0x7Fu) / 255.0f;
            float b = 0.30f + float((seed * 97u + 17u) & 0x7Fu) / 255.0f;
            if (textureHandle != 0) glColor3f(1.0f, 1.0f, 1.0f);
            else glColor3f(r, g, b);

            const StorylandWorldMeshVertex* verts[3] = {
                &mesh.vertices[tri.a],
                &mesh.vertices[tri.b],
                &mesh.vertices[tri.c]
            };
            for (const StorylandWorldMeshVertex* vertex : verts) {
                if (textureHandle != 0) glTexCoord2f(vertex->u, vertex->v);
                float wx = placement.matrix[0] * vertex->x + placement.matrix[4] * vertex->y + placement.matrix[8]  * vertex->z + placement.matrix[12];
                float wy = placement.matrix[1] * vertex->x + placement.matrix[5] * vertex->y + placement.matrix[9]  * vertex->z + placement.matrix[13];
                float wz = placement.matrix[2] * vertex->x + placement.matrix[6] * vertex->y + placement.matrix[10] * vertex->z + placement.matrix[14];
                glVertex3f(wx, wy, wz);
            }

            drawnTriangles++;
        }
    }
    if (triangleBatchOpen) glEnd();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    for (const auto& uploaded : uploadedArchiveTextures) {
        GLuint handle = uploaded.second;
        if (handle != 0) glDeleteTextures(1, &handle);
    }

    if (drawnTriangles == 0) {
        glPointSize(selectedSectorIndex >= 0 || selectedResourceIndex >= 0 ? 5.0f : 3.0f);
        glBegin(GL_POINTS);
        for (const auto& point : points) {
            glColor3f(point.isSectorFallback ? 0.35f : 0.78f, point.isSectorFallback ? 0.55f : 0.78f, point.isSectorFallback ? 1.0f : 0.70f);
            glVertex3f(point.x, point.y, point.z);
        }
        glEnd();
        glPointSize(1.0f);
    }

    drawOpenGlViewCube(hwnd, width, height);

    glFlush();
    SwapBuffers(dc);
    drawOpenGlViewCubeLabels(hwnd, dc);
    wglMakeCurrent(nullptr, nullptr);

    std::wstring title = L"OpenGL LVZ+IMG mesh viewport  |  ";
    if (selectedEntry || gSelectedKind == StorylandTreeKind::ArchiveMeshResource || gSelectedKind == StorylandTreeKind::ArchiveTextureResource || gSelectedKind == StorylandTreeKind::ArchiveAnimationResource) {
        title += L"selected=" + widen(selectedName);
    } else {
        title += L"real sector overlay meshes";
    }
    title += L"  |  meshResources=" + std::to_wstring(meshes.size());
    title += L"  |  instances=" + std::to_wstring(drawnInstances);
    title += L"  |  tris=" + std::to_wstring(drawnTriangles);
    title += L"  |  LMB rotate, RMB pan, wheel/+/- zoom, W/S/A/D/Q/E move";
}


static StorylandModelPoint previewTriangleNormal(const StorylandModelPoint& a, const StorylandModelPoint& b, const StorylandModelPoint& c) {
    StorylandModelPoint ab{b.x - a.x, b.y - a.y, b.z - a.z};
    StorylandModelPoint ac{c.x - a.x, c.y - a.y, c.z - a.z};
    StorylandModelPoint n = crossStorylandPoint(ab, ac);
    return normalizeStorylandPoint(n, StorylandModelPoint{0.0f, 0.0f, 1.0f});
}

static std::vector<StorylandModelPoint> buildPreviewGouraudNormals(
    const std::vector<StorylandModelPoint>& points,
    const std::vector<StorylandModelTriangle>& triangles
) {
    // Area-weighted accumulation matches RenderWare's smooth indexed-vertex
    // lighting and keeps tiny triangles from overpowering their neighbours.
    std::vector<StorylandModelPoint> normals(points.size());
    for (const StorylandModelTriangle& triangle : triangles) {
        if (triangle.a >= points.size() || triangle.b >= points.size() || triangle.c >= points.size()) continue;
        const StorylandModelPoint& a = points[triangle.a];
        const StorylandModelPoint& b = points[triangle.b];
        const StorylandModelPoint& c = points[triangle.c];
        StorylandModelPoint ab{b.x - a.x, b.y - a.y, b.z - a.z};
        StorylandModelPoint ac{c.x - a.x, c.y - a.y, c.z - a.z};
        StorylandModelPoint face = crossStorylandPoint(ab, ac);
        if (!std::isfinite(face.x) || !std::isfinite(face.y) || !std::isfinite(face.z) ||
            lengthSqStorylandPoint(face) <= 0.00000001f) {
            continue;
        }
        for (uint32_t index : {triangle.a, triangle.b, triangle.c}) {
            normals[index].x += face.x;
            normals[index].y += face.y;
            normals[index].z += face.z;
        }
    }
    for (StorylandModelPoint& normal : normals) {
        normal = normalizeStorylandPoint(normal, StorylandModelPoint{0.0f, 0.0f, 1.0f});
    }
    return normals;
}

static void emitPreviewVertexWithNormal(const StorylandModelPoint& p, const StorylandModelPoint& n) {
    glNormal3f(n.x, n.y, n.z);
    glVertex3f(p.x, p.y, p.z);
}

static void drawOpenGlViewCubeFaces() {
    glBegin(GL_QUADS);

    glColor3f(0.68f, 0.18f, 0.16f);
    glVertex3f(0.42f, -0.42f, -0.42f); glVertex3f(0.42f, 0.42f, -0.42f); glVertex3f(0.42f, 0.42f, 0.42f); glVertex3f(0.42f, -0.42f, 0.42f);

    glColor3f(0.22f, 0.58f, 0.22f);
    glVertex3f(-0.42f, 0.42f, -0.42f); glVertex3f(0.42f, 0.42f, -0.42f); glVertex3f(0.42f, 0.42f, 0.42f); glVertex3f(-0.42f, 0.42f, 0.42f);

    glColor3f(0.18f, 0.32f, 0.70f);
    glVertex3f(-0.42f, -0.42f, 0.42f); glVertex3f(0.42f, -0.42f, 0.42f); glVertex3f(0.42f, 0.42f, 0.42f); glVertex3f(-0.42f, 0.42f, 0.42f);

    glColor3f(0.42f, 0.12f, 0.11f);
    glVertex3f(-0.42f, -0.42f, -0.42f); glVertex3f(-0.42f, -0.42f, 0.42f); glVertex3f(-0.42f, 0.42f, 0.42f); glVertex3f(-0.42f, 0.42f, -0.42f);

    glColor3f(0.12f, 0.36f, 0.12f);
    glVertex3f(-0.42f, -0.42f, -0.42f); glVertex3f(-0.42f, -0.42f, 0.42f); glVertex3f(0.42f, -0.42f, 0.42f); glVertex3f(0.42f, -0.42f, -0.42f);

    glColor3f(0.10f, 0.20f, 0.44f);
    glVertex3f(-0.42f, -0.42f, -0.42f); glVertex3f(0.42f, -0.42f, -0.42f); glVertex3f(0.42f, 0.42f, -0.42f); glVertex3f(-0.42f, 0.42f, -0.42f);

    glEnd();
}

static void drawOpenGlViewCube(HWND hwnd, int width, int height) {
    if (!gOpenGlShowViewCube) return;

    RECT cubeRc = viewCubeRect(hwnd);
    int cubeW = std::max<int>(1, int(cubeRc.right - cubeRc.left));
    int cubeH = std::max<int>(1, int(cubeRc.bottom - cubeRc.top));
    float cubeCenterX = float(cubeRc.left + cubeRc.right) * 0.5f;
    float cubeCenterY = float(height) - float(cubeRc.top + cubeRc.bottom) * 0.5f;
    float cubeScale = float(std::min(cubeW, cubeH)) * 0.78f;

    stopStoriesShaderProgram();
    setupFixedPipelineStoriesLighting(false);

    // Draw directly into the full scene framebuffer.  Clearing depth cannot
    // alter a single colour pixel, so the orientation cube has no backing tile
    // on any driver/pixel format.
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, double(width), 0.0, double(height), -4.0, 4.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(cubeCenterX, cubeCenterY, 0.0f);
    // X/Y are expressed in overlay pixels; Z must remain in the small
    // orthographic depth range or the cube faces are clipped into a hollow,
    // distorted outline.
    glScalef(cubeScale, cubeScale, 1.0f);
    glMultModelQuat(gModelViewRotation);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawOpenGlViewCubeFaces();
    glDisable(GL_BLEND);

    glLineWidth(1.2f);
    glColor3f(0.82f, 0.86f, 0.88f);
    glBegin(GL_LINES);
    auto edge = [](float ax, float ay, float az, float bx, float by, float bz) {
        glVertex3f(ax, ay, az); glVertex3f(bx, by, bz);
    };
    float s = 0.42f;
    edge(-s,-s,-s, s,-s,-s); edge(s,-s,-s, s,s,-s); edge(s,s,-s, -s,s,-s); edge(-s,s,-s, -s,-s,-s);
    edge(-s,-s, s, s,-s, s); edge(s,-s, s, s,s, s); edge(s,s, s, -s,s, s); edge(-s,s, s, -s,-s, s);
    edge(-s,-s,-s, -s,-s, s); edge(s,-s,-s, s,-s, s); edge(s,s,-s, s,s, s); edge(-s,s,-s, -s,s, s);
    glEnd();

    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.18f, 0.14f); glVertex3f(0,0,0); glVertex3f(0.95f,0,0);
    glColor3f(0.20f, 0.95f, 0.25f); glVertex3f(0,0,0); glVertex3f(0,0.95f,0);
    glColor3f(0.25f, 0.52f, 1.0f); glVertex3f(0,0,0); glVertex3f(0,0,0.95f);
    glEnd();

    glPointSize(5.0f);
    glBegin(GL_POINTS);
    glColor3f(1.0f, 0.18f, 0.14f); glVertex3f(0.95f,0,0);
    glColor3f(0.20f, 0.95f, 0.25f); glVertex3f(0,0.95f,0);
    glColor3f(0.25f, 0.52f, 1.0f); glVertex3f(0,0,0.95f);
    glEnd();

}

static void drawOpenGlViewCubeLabels(HWND hwnd, HDC dc) {
    if (!gOpenGlShowViewCube) return;

    RECT rc = viewCubeRect(hwnd);
    int size = std::max<int>(1, int(rc.right - rc.left));
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    float labelRadius = float(size) * 0.38f;

    auto labelPoint = [&](StorylandVec3 axis) -> POINT {
        StorylandVec3 r = rotateVecByQuat(gModelViewRotation, axis);
        POINT p{};
        p.x = int(std::round(float(cx) + r.x * labelRadius));
        p.y = int(std::round(float(cy) - r.y * labelRadius));
        return p;
    };

    SetBkMode(dc, TRANSPARENT);
    HFONT oldFont = nullptr;
    HFONT font = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    if (font) oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));

    auto drawLabel = [&](const wchar_t* text, COLORREF color, POINT p) {
        SetTextColor(dc, color);
        TextOutW(dc, p.x - 5, p.y - 7, text, 1);
    };

    drawLabel(L"X", RGB(255, 80, 64), labelPoint({1.0f, 0.0f, 0.0f}));
    drawLabel(L"Y", RGB(80, 240, 96), labelPoint({0.0f, 1.0f, 0.0f}));
    drawLabel(L"Z", RGB(96, 150, 255), labelPoint({0.0f, 0.0f, 1.0f}));

    if (oldFont) SelectObject(dc, oldFont);
    if (font) DeleteObject(font);
}


static void drawWblPreviewOpenGl(HWND hwnd, HDC dc, RECT rc) {
    const auto& vertices = gWblFile.vertices();
    const auto& triangles = gWblFile.triangles();
    const auto& meshes = gWblFile.meshes();

    if (!gOpenGlReady && !initializeOpenGlPreview(hwnd)) {
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        TextOutW(dc, rc.left + 8, rc.top + 8, L"OpenGL init failed", 18);
        return;
    }

    if (!wglMakeCurrent(dc, gOpenGlContext)) {
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        TextOutW(dc, rc.left + 8, rc.top + 8, L"OpenGL context activation failed", 33);
        return;
    }

    int width = std::max<int>(1, int(rc.right - rc.left));
    int height = std::max<int>(1, int(rc.bottom - rc.top));
    glViewport(0, 0, width, height);
    setStoriesViewportClearColor();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);

    double aspect = double(width) / double(height);
    setPerspectiveProjection(45.0, aspect, 0.01, 5000.0);
    drawStoriesViewportSky(100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(gModelPanX, gModelPanY, -gModelDistance);
    glMultModelQuat(gModelViewRotation);

    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    bool haveBounds = gWblFile.hasBounds(minX, minY, minZ, maxX, maxY, maxZ);
    if (!haveBounds) {
        minX = minY = minZ = -1.0f;
        maxX = maxY = maxZ = 1.0f;
    }

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    float spanX = std::max(0.001f, maxX - minX);
    float spanY = std::max(0.001f, maxY - minY);
    float spanZ = std::max(0.001f, maxZ - minZ);
    float largestSpan = std::max(spanX, std::max(spanY, spanZ));
    float modelScale = 2.0f / std::max(0.001f, largestSpan);

    glScalef(modelScale, modelScale, modelScale);
    glTranslatef(-centerX, -centerY, -centerZ);

    setupFixedPipelineStoriesLighting(gOpenGlRenderMode != StorylandOpenGlRenderMode::Wireframe);

    auto setColorForTexture = [](uint16_t textureId) {
        uint32_t seed = uint32_t(textureId) * 1103515245u + 12345u;
        float r = 0.35f + float((seed >> 16) & 0x7F) / 255.0f;
        float g = 0.35f + float((seed >> 8) & 0x7F) / 255.0f;
        float b = 0.35f + float(seed & 0x7F) / 255.0f;
        glColor3f(std::min(0.95f, r), std::min(0.95f, g), std::min(0.95f, b));
    };

    auto emitTri = [&](const StorylandWblTriangle& tri) {
        if (tri.a >= vertices.size() || tri.b >= vertices.size() || tri.c >= vertices.size()) return;
        const auto& a = vertices[tri.a];
        const auto& b = vertices[tri.b];
        const auto& c = vertices[tri.c];
        if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z)) return;
        if (!std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.z)) return;
        if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z)) return;
        float abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
        float acx = c.x - a.x, acy = c.y - a.y, acz = c.z - a.z;
        float nx = aby * acz - abz * acy;
        float ny = abz * acx - abx * acz;
        float nz = abx * acy - aby * acx;
        float nlen = std::sqrt(std::max(0.000001f, nx * nx + ny * ny + nz * nz));
        glNormal3f(nx / nlen, ny / nlen, nz / nlen);
        setColorForTexture(tri.textureId);
        glVertex3f(a.x, a.y, a.z);
        glVertex3f(b.x, b.y, b.z);
        glVertex3f(c.x, c.y, c.z);
    };

    uint32_t firstTri = 0;
    uint32_t triCount = uint32_t(triangles.size());
    if (gSelectedKind == StorylandTreeKind::WblMesh && gSelectedIndex >= 0 && size_t(gSelectedIndex) < meshes.size()) {
        const auto& mesh = meshes[size_t(gSelectedIndex)];
        firstTri = mesh.firstTriangle;
        triCount = mesh.triangleCount;
    }
    uint32_t endTri = std::min<uint32_t>(uint32_t(triangles.size()), firstTri + triCount);

    if (gOpenGlShowGrid) {
        setupFixedPipelineStoriesLighting(false);
        float gridSize = std::max(1.0f, largestSpan);
        glBegin(GL_LINES);
        glColor3f(0.20f, 0.21f, 0.23f);
        for (int i = -10; i <= 10; ++i) {
            float d = float(i) * gridSize / 10.0f;
            glVertex3f(-gridSize, d, 0.0f); glVertex3f(gridSize, d, 0.0f);
            glVertex3f(d, -gridSize, 0.0f); glVertex3f(d, gridSize, 0.0f);
        }
        glEnd();
        setupFixedPipelineStoriesLighting(gOpenGlRenderMode != StorylandOpenGlRenderMode::Wireframe);
    }

    if (!triangles.empty() && gOpenGlRenderMode != StorylandOpenGlRenderMode::Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBegin(GL_TRIANGLES);
        for (uint32_t i = firstTri; i < endTri; ++i) emitTri(triangles[i]);
        glEnd();
    }

    setupFixedPipelineStoriesLighting(false);
    if (!triangles.empty() && (gOpenGlRenderMode == StorylandOpenGlRenderMode::Wireframe || gOpenGlShowBounds)) {
        glLineWidth(1.0f);
        glColor3f(0.88f, 0.90f, 0.92f);
        glBegin(GL_LINES);
        for (uint32_t i = firstTri; i < endTri; ++i) {
            const auto& tri = triangles[i];
            if (tri.a >= vertices.size() || tri.b >= vertices.size() || tri.c >= vertices.size()) continue;
            const auto& a = vertices[tri.a];
            const auto& b = vertices[tri.b];
            const auto& c = vertices[tri.c];
            glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z);
            glVertex3f(b.x, b.y, b.z); glVertex3f(c.x, c.y, c.z);
            glVertex3f(c.x, c.y, c.z); glVertex3f(a.x, a.y, a.z);
        }
        glEnd();
    }

    drawOpenGlViewCube(hwnd, width, height);
    glFlush();
    SwapBuffers(dc);
    drawOpenGlViewCubeLabels(hwnd, dc);
    wglMakeCurrent(nullptr, nullptr);
}

static void drawModelPreviewOpenGl(HWND hwnd, HDC dc, RECT rc) {
    const auto& sourcePts = gModelFile.previewPoints();
    std::vector<StorylandModelPoint> animatedPts = buildAnimatedModelPreviewPoints(sourcePts);
    const bool usingAnimatedMeshPreview = gModelAnimLoaded && gAnimFile.hasDecodedMotion();
    const auto& pts = animatedPts;
    const bool rigidModelPreview = !usingAnimatedMeshPreview &&
        (gModelFile.modelKind() == StorylandModelKind::SimpleModel ||
         gModelFile.modelKind() == StorylandModelKind::VehicleModel ||
         gModelFile.modelKind() == StorylandModelKind::WorldModel);
    const bool staticDecodedModelPreview = !usingAnimatedMeshPreview;
    const auto& tris = gModelFile.previewTriangles();
    const auto& texcoords = gModelFile.previewTexcoords();
    const auto& bones = gModelFile.armatureBones();
    if (!gOpenGlReady && !initializeOpenGlPreview(hwnd)) {
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        std::wstring title = L"OpenGL init failed. MDL: " + widen(gModelFile.modelKindName()) + L" points=" + std::to_wstring(pts.size());
        TextOutW(dc, rc.left + 8, rc.top + 8, title.c_str(), int(title.size()));
        return;
    }

    if (!wglMakeCurrent(dc, gOpenGlContext)) {
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        TextOutW(dc, rc.left + 8, rc.top + 8, L"OpenGL context activation failed", 33);
        return;
    }

    int width = std::max<int>(1, static_cast<int>(rc.right - rc.left));
    int height = std::max<int>(1, static_cast<int>(rc.bottom - rc.top));
    glViewport(0, 0, width, height);

    setStoriesViewportClearColor();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDisable(GL_ALPHA_TEST);

    bool textureAvailable = gModelTextureLoaded && !gModelTextureRegions.empty() && uploadModelTextureIfNeeded();
    bool hasRealTexcoords = texcoords.size() == pts.size() && modelTexcoordsLookUsable(texcoords);
    bool canUseTexture = false;
    bool usingProjectedTexcoords = false;
    glDisable(GL_TEXTURE_2D);

    double aspect = double(width) / double(height);
    setPerspectiveProjection(45.0, aspect, 0.01, 500.0);
    drawStoriesViewportSky(100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(gModelPanX, gModelPanY, -gModelDistance);
    glMultModelQuat(gModelViewRotation);

    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    bool hasBounds = false;
    auto includePointInBounds = [&](float x, float y, float z) {
        if (!hasBounds) {
            minX = maxX = x;
            minY = maxY = y;
            minZ = maxZ = z;
            hasBounds = true;
        } else {
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
            minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
        }
    };
    auto robustAxisRange = [](std::vector<float> values, float& outMin, float& outMax) -> bool {
        if (values.empty()) return false;
        std::sort(values.begin(), values.end());
        auto sample = [&](double q) -> float {
            double scaled = q * double(values.size() - 1);
            size_t lo = size_t(std::floor(scaled));
            size_t hi = size_t(std::ceil(scaled));
            float t = float(scaled - double(lo));
            return values[lo] * (1.0f - t) + values[hi] * t;
        };
        outMin = sample(0.005);
        outMax = sample(0.995);
        if (!std::isfinite(outMin) || !std::isfinite(outMax) || outMax <= outMin) {
            outMin = values.front();
            outMax = values.back();
        }
        return std::isfinite(outMin) && std::isfinite(outMax) && outMax > outMin;
    };

    std::vector<float> pointXs;
    std::vector<float> pointYs;
    std::vector<float> pointZs;
    pointXs.reserve(pts.size());
    pointYs.reserve(pts.size());
    pointZs.reserve(pts.size());
    for (const auto& p : pts) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) continue;
        pointXs.push_back(p.x);
        pointYs.push_back(p.y);
        pointZs.push_back(p.z);
    }

    bool haveRobustMeshBounds = false;
    if (!pointXs.empty() && robustAxisRange(pointXs, minX, maxX) && robustAxisRange(pointYs, minY, maxY) && robustAxisRange(pointZs, minZ, maxZ)) {
        haveRobustMeshBounds = true;
        hasBounds = true;
    } else {
        for (const auto& p : pts) {
            includePointInBounds(p.x, p.y, p.z);
        }
    }

    float spanX = std::max(0.001f, maxX - minX);
    float spanY = std::max(0.001f, maxY - minY);
    float spanZ = std::max(0.001f, maxZ - minZ);

    float rejectMinX = minX - spanX * 0.20f;
    float rejectMaxX = maxX + spanX * 0.20f;
    float rejectMinY = minY - spanY * 0.20f;
    float rejectMaxY = maxY + spanY * 0.20f;
    float rejectMinZ = minZ - spanZ * 0.20f;
    float rejectMaxZ = maxZ + spanZ * 0.20f;

    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const auto& bone = bones[boneIndex];
        if (!boneHasVisiblePreviewPosition(bone)) continue;
        StorylandModelPoint p = displayAnimatedModelBonePosition(boneIndex);
        includePointInBounds(p.x, p.y, p.z);
    }

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    spanX = std::max(0.001f, maxX - minX);
    spanY = std::max(0.001f, maxY - minY);
    spanZ = std::max(0.001f, maxZ - minZ);
    float largestSpan = std::max(spanX, std::max(spanY, spanZ));
    float modelScale = 2.0f / std::max(0.001f, largestSpan);

    canUseTexture = textureAvailable && !pts.empty();
    usingProjectedTexcoords = canUseTexture && !hasRealTexcoords;

    glScalef(modelScale, modelScale, modelScale);
    glTranslatef(-centerX, -centerY, -centerZ);

    float gridSize = std::max(1.0f, largestSpan);
    if (gOpenGlShowGrid) {
        setupFixedPipelineStoriesLighting(false);
        glDisable(GL_TEXTURE_2D);
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        glColor3f(0.20f, 0.21f, 0.23f);
        for (int i = -10; i <= 10; ++i) {
            float d = float(i) * gridSize / 10.0f;
            glVertex3f(-gridSize, d, 0.0f); glVertex3f(gridSize, d, 0.0f);
            glVertex3f(d, -gridSize, 0.0f); glVertex3f(d, gridSize, 0.0f);
        }
        glColor3f(0.95f, 0.22f, 0.18f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(gridSize, 0.0f, 0.0f);
        glColor3f(0.20f, 0.85f, 0.25f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, gridSize, 0.0f);
        glColor3f(0.25f, 0.48f, 1.0f); glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(0.0f, 0.0f, gridSize);
        glEnd();
    }

    if (!tris.empty()) {
        const float safePreviewEdgeLimit = std::max(0.035f, largestSpan * 0.16f);
        const float safePreviewEdgeLimitSq = safePreviewEdgeLimit * safePreviewEdgeLimit;
        const float safePreviewTriangleAreaLimitSq = safePreviewEdgeLimitSq * safePreviewEdgeLimitSq * 0.75f;

        auto previewEdgeLengthSq = [](const StorylandModelPoint& a, const StorylandModelPoint& b) -> float {
            float dx = b.x - a.x;
            float dy = b.y - a.y;
            float dz = b.z - a.z;
            return dx * dx + dy * dy + dz * dz;
        };

        auto previewTriangleAreaSq = [](const StorylandModelPoint& a, const StorylandModelPoint& b, const StorylandModelPoint& c) -> float {
            float abX = b.x - a.x;
            float abY = b.y - a.y;
            float abZ = b.z - a.z;
            float acX = c.x - a.x;
            float acY = c.y - a.y;
            float acZ = c.z - a.z;
            float crossX = abY * acZ - abZ * acY;
            float crossY = abZ * acX - abX * acZ;
            float crossZ = abX * acY - abY * acX;
            return crossX * crossX + crossY * crossY + crossZ * crossZ;
        };

        auto pointIsInsideRobustPreviewRange = [&](const StorylandModelPoint& p) -> bool {
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return false;
            if (usingAnimatedMeshPreview) return true;
            if (!haveRobustMeshBounds) return true;
            return p.x >= rejectMinX && p.x <= rejectMaxX &&
                   p.y >= rejectMinY && p.y <= rejectMaxY &&
                   p.z >= rejectMinZ && p.z <= rejectMaxZ;
        };

        auto previewTriangleIsSafe = [&](const StorylandModelPoint& a, const StorylandModelPoint& b, const StorylandModelPoint& c) -> bool {
            if (staticDecodedModelPreview) {


                return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z) &&
                       std::isfinite(b.x) && std::isfinite(b.y) && std::isfinite(b.z) &&
                       std::isfinite(c.x) && std::isfinite(c.y) && std::isfinite(c.z);
            }

            if (!pointIsInsideRobustPreviewRange(a) || !pointIsInsideRobustPreviewRange(b) || !pointIsInsideRobustPreviewRange(c)) return false;
            float ab = previewEdgeLengthSq(a, b);
            float bc = previewEdgeLengthSq(b, c);
            float ca = previewEdgeLengthSq(c, a);
            if (!std::isfinite(ab) || !std::isfinite(bc) || !std::isfinite(ca)) return false;
            if (!usingAnimatedMeshPreview && (ab > safePreviewEdgeLimitSq || bc > safePreviewEdgeLimitSq || ca > safePreviewEdgeLimitSq)) return false;
            float areaSq = previewTriangleAreaSq(a, b, c);
            if (!std::isfinite(areaSq)) return false;
            if (!usingAnimatedMeshPreview && areaSq > safePreviewTriangleAreaLimitSq) return false;
            return true;
        };

        const bool wireOnly = gOpenGlRenderMode == StorylandOpenGlRenderMode::Wireframe;
        const bool wantsTexture = canUseTexture &&
            (gOpenGlRenderMode == StorylandOpenGlRenderMode::Stories ||
             gOpenGlRenderMode == StorylandOpenGlRenderMode::Textured);
        const bool wantsLighting =
            gOpenGlRenderMode == StorylandOpenGlRenderMode::Stories ||
            gOpenGlRenderMode == StorylandOpenGlRenderMode::Solid;

        if (wantsTexture) {
            if (pglActiveTexture) pglActiveTexture(GL_TEXTURE0);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, gModelTextureId);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        } else {
            glDisable(GL_TEXTURE_2D);
        }

        bool usingStoriesShader = false;
        if (!wireOnly && (gOpenGlRenderMode == StorylandOpenGlRenderMode::Stories ||
                          gOpenGlRenderMode == StorylandOpenGlRenderMode::Textured ||
                          gOpenGlRenderMode == StorylandOpenGlRenderMode::Solid)) {
            usingStoriesShader = beginStoriesShaderProgram(wantsTexture);
        }
        if (!usingStoriesShader) setupFixedPipelineStoriesLighting(wantsLighting && !wireOnly);

        const std::vector<StorylandModelPoint> gouraudNormals =
            (!wireOnly && wantsLighting) ? buildPreviewGouraudNormals(pts, tris) : std::vector<StorylandModelPoint>();

        if (!wireOnly) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glBegin(GL_TRIANGLES);
            if (gOpenGlRenderMode == StorylandOpenGlRenderMode::Solid) glColor3f(0.76f, 0.78f, 0.74f);
            else glColor3f(1.0f, 1.0f, 1.0f);
            for (const auto& tri : tris) {
                if (tri.a >= pts.size() || tri.b >= pts.size() || tri.c >= pts.size()) continue;
                const auto& a = pts[tri.a];
                const auto& b = pts[tri.b];
                const auto& c = pts[tri.c];
                if (!previewTriangleIsSafe(a, b, c)) continue;
                StorylandModelPoint faceNormal = previewTriangleNormal(a, b, c);
                const StorylandModelPoint& normalA = tri.a < gouraudNormals.size() ? gouraudNormals[tri.a] : faceNormal;
                const StorylandModelPoint& normalB = tri.b < gouraudNormals.size() ? gouraudNormals[tri.b] : faceNormal;
                const StorylandModelPoint& normalC = tri.c < gouraudNormals.size() ? gouraudNormals[tri.c] : faceNormal;
                int textureRegion = wantsTexture ? chooseModelTextureRegionForTriangle(tri, pts, minX, minY, minZ, spanX, spanY, spanZ) : -1;
                if (wantsTexture) emitModelPreviewTexcoordInRegion(tri.a, textureRegion, pts, texcoords, hasRealTexcoords, minX, minY, minZ, spanX, spanY, spanZ);
                emitPreviewVertexWithNormal(a, normalA);
                if (wantsTexture) emitModelPreviewTexcoordInRegion(tri.b, textureRegion, pts, texcoords, hasRealTexcoords, minX, minY, minZ, spanX, spanY, spanZ);
                emitPreviewVertexWithNormal(b, normalB);
                if (wantsTexture) emitModelPreviewTexcoordInRegion(tri.c, textureRegion, pts, texcoords, hasRealTexcoords, minX, minY, minZ, spanX, spanY, spanZ);
                emitPreviewVertexWithNormal(c, normalC);
            }
            glEnd();
            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        stopStoriesShaderProgram();
        setupFixedPipelineStoriesLighting(false);
        glDisable(GL_TEXTURE_2D);

        if (wireOnly) {
            glLineWidth(1.25f);
            glBegin(GL_LINES);
            glColor3f(0.86f, 0.88f, 0.82f);
            auto emitSafePreviewWireEdge = [&](const StorylandModelPoint& a, const StorylandModelPoint& b) {
                float edgeLengthSq = previewEdgeLengthSq(a, b);
                if (!std::isfinite(edgeLengthSq)) return;
                if (!staticDecodedModelPreview && !usingAnimatedMeshPreview && edgeLengthSq > safePreviewEdgeLimitSq) return;
                glVertex3f(a.x, a.y, a.z);
                glVertex3f(b.x, b.y, b.z);
            };
            for (const auto& tri : tris) {
                if (tri.a >= pts.size() || tri.b >= pts.size() || tri.c >= pts.size()) continue;
                const auto& a = pts[tri.a];
                const auto& b = pts[tri.b];
                const auto& c = pts[tri.c];
                if (!previewTriangleIsSafe(a, b, c)) continue;
                emitSafePreviewWireEdge(a, b);
                emitSafePreviewWireEdge(b, c);
                emitSafePreviewWireEdge(c, a);
            }
            glEnd();
        }
    } else if (!pts.empty()) {
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        glColor3f(0.88f, 0.88f, 0.82f);
        for (const auto& p : pts) {
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();
    }

    if (!bones.empty() && gOpenGlShowBones) {
        stopStoriesShaderProgram();
        setupFixedPipelineStoriesLighting(false);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);
        glLineWidth(3.0f);
        glBegin(GL_LINES);
        glColor3f(1.0f, 0.86f, 0.18f);
        for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
            const auto& bone = bones[boneIndex];
            if (!boneHasVisiblePreviewPosition(bone)) continue;
            if (bone.parentIndex == 0xFFFFFFFFu || bone.parentIndex >= bones.size()) continue;
            const StorylandModelBone& parentBone = bones[size_t(bone.parentIndex)];
            if (!boneHasVisiblePreviewPosition(parentBone)) continue;
            StorylandModelPoint parent = displayAnimatedModelBonePosition(size_t(bone.parentIndex));
            StorylandModelPoint child = displayAnimatedModelBonePosition(boneIndex);
            glVertex3f(parent.x, parent.y, parent.z);
            glVertex3f(child.x, child.y, child.z);
        }
        glEnd();

        glPointSize(6.0f);
        glBegin(GL_POINTS);
        glColor3f(0.1f, 0.95f, 1.0f);
        for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
            const auto& bone = bones[boneIndex];
            if (!boneHasVisiblePreviewPosition(bone)) continue;
            StorylandModelPoint p = displayAnimatedModelBonePosition(boneIndex);
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();
    }

    if (gOpenGlShowBounds) {
        stopStoriesShaderProgram();
        setupFixedPipelineStoriesLighting(false);
        glLineWidth(1.0f);
        glColor3f(0.55f, 0.58f, 0.62f);
        glBegin(GL_LINE_LOOP);
        glVertex3f(minX, minY, minZ);
        glVertex3f(maxX, minY, minZ);
        glVertex3f(maxX, maxY, minZ);
        glVertex3f(minX, maxY, minZ);
        glEnd();
        glBegin(GL_LINE_LOOP);
        glVertex3f(minX, minY, maxZ);
        glVertex3f(maxX, minY, maxZ);
        glVertex3f(maxX, maxY, maxZ);
        glVertex3f(minX, maxY, maxZ);
        glEnd();
        glBegin(GL_LINES);
        glVertex3f(minX, minY, minZ); glVertex3f(minX, minY, maxZ);
        glVertex3f(maxX, minY, minZ); glVertex3f(maxX, minY, maxZ);
        glVertex3f(maxX, maxY, minZ); glVertex3f(maxX, maxY, maxZ);
        glVertex3f(minX, maxY, minZ); glVertex3f(minX, maxY, maxZ);
        glEnd();
    }

    drawOpenGlViewCube(hwnd, width, height);

    glFlush();
    SwapBuffers(dc);
    drawOpenGlViewCubeLabels(hwnd, dc);
    wglMakeCurrent(nullptr, nullptr);
}

static LRESULT CALLBACK previewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (gMode == StorylandMode::TextureArchive) drawTexturePreview(dc, rc);
        else if (gMode == StorylandMode::ModelFile) drawModelPreviewOpenGl(hwnd, dc, rc);
        else if (gMode == StorylandMode::WblFile) drawWblPreviewOpenGl(hwnd, dc, rc);
        else if (gMode == StorylandMode::ArchiveFile) drawArchivePreviewOpenGl(hwnd, dc, rc);
        else if (gMode == StorylandMode::AnimFile) drawAnimPreviewOpenGl(hwnd, dc, rc);
        else if (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::TextureArchive) drawTexturePreview(dc, rc);
        else if (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile) drawModelPreviewOpenGl(hwnd, dc, rc);
        else {
            HBRUSH emptyPreviewBrush = CreateSolidBrush(RGB(181, 221, 242));
            FillRect(dc, &rc, emptyPreviewBrush);
            DeleteObject(emptyPreviewBrush);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_KEYDOWN && handleModelViewportShortcut(wParam)) {
        return 0;
    }
    if (msg == WM_LBUTTONDOWN && currentModeUsesInteractiveModelViewport()) {
        gModelLeftDrag = true;
        gModelLastMouse.x = GET_X_LPARAM(lParam);
        gModelLastMouse.y = GET_Y_LPARAM(lParam);
        gModelViewCubeDrag = pointInViewCube(hwnd, gModelLastMouse.x, gModelLastMouse.y);
        gModelDragStartPoint = gModelViewCubeDrag ?
            viewCubePointFromMouse(hwnd, gModelLastMouse.x, gModelLastMouse.y) :
            arcballPointFromMouse(hwnd, gModelLastMouse.x, gModelLastMouse.y);
        gModelDragStartRotation = gModelViewRotation;
        SetFocus(hwnd);
        SetCapture(hwnd);
        return 0;
    }
    if (msg == WM_RBUTTONDOWN && currentModeUsesInteractiveModelViewport()) {
        gModelRightDrag = true;
        gModelLastMouse.x = GET_X_LPARAM(lParam);
        gModelLastMouse.y = GET_Y_LPARAM(lParam);
        SetCapture(hwnd);
        return 0;
    }
    if ((msg == WM_LBUTTONUP || msg == WM_RBUTTONUP) && currentModeUsesInteractiveModelViewport()) {
        if (msg == WM_LBUTTONUP) {
            gModelLeftDrag = false;
            gModelViewCubeDrag = false;
        }
        if (msg == WM_RBUTTONUP) gModelRightDrag = false;
        if (!gModelLeftDrag && !gModelRightDrag) ReleaseCapture();
        return 0;
    }
    if (msg == WM_MOUSEMOVE && currentModeUsesInteractiveModelViewport()) {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int dx = x - gModelLastMouse.x;
        int dy = y - gModelLastMouse.y;
        gModelLastMouse.x = x;
        gModelLastMouse.y = y;
        if (gModelLeftDrag) {
            StorylandVec3 currentPoint = gModelViewCubeDrag ?
                viewCubePointFromMouse(hwnd, x, y) :
                arcballPointFromMouse(hwnd, x, y);
            StorylandQuat dragDelta = quatFromVectors(gModelDragStartPoint, currentPoint);
            gModelViewRotation = quatMul(dragDelta, gModelDragStartRotation);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (gModelRightDrag) {
            float panScale = (gMode == StorylandMode::ArchiveFile) ? std::max(0.006f, gModelDistance * 0.004f) : 0.005f;
            gModelPanX += float(dx) * panScale;
            gModelPanY -= float(dy) * panScale;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
    }
    if (msg == WM_MOUSEWHEEL && currentModeUsesInteractiveModelViewport()) {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        applyModelViewportZoom(delta > 0 ? 0.88f : 1.14f);
        return 0;
    }
    if (msg == WM_LBUTTONDBLCLK && currentModeUsesInteractiveModelViewport()) {
        resetModelViewport();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void layoutChildren(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);

    const int menuHeight = 28;
    const int statusHeight = 22;
    const int clientWidth = static_cast<int>(rc.right - rc.left);
    const int clientHeight = static_cast<int>(rc.bottom - rc.top);

    SendMessageW(gStatus, WM_SIZE, 0, 0);

    MoveWindow(
        gStatus,
        0,
        clientHeight - statusHeight,
        clientWidth,
        statusHeight,
        TRUE
    );
    MoveWindow(gMenuStrip, 0, 0, clientWidth, menuHeight, TRUE);

    const int leftW = std::max<int>(300, clientWidth / 3);
    const int rightW = clientWidth - leftW;
    const int topH = clientHeight - statusHeight - menuHeight;
    const int previewH = std::max<int>(180, topH / 2);
    const int rightX = leftW + 4;
    const int rightClientW = std::max<int>(0, rightW - 4);

    MoveWindow(gTree, 0, menuHeight, leftW, topH, TRUE);

    MoveWindow(
        gPreview,
        rightX,
        menuHeight,
        rightClientW,
        previewH,
        TRUE
    );

    MoveWindow(
        gDetails,
        rightX,
        menuHeight + previewH + 4,
        rightClientW,
        topH - previewH - 4,
        TRUE
    );
}


static void selectPayloadForCurrentMode(const StorylandTreePayload& payload) {
    if (gMode == StorylandMode::TextureArchive && payload.kind == StorylandTreeKind::Texture) selectTexture(payload.index);
    else if (gMode == StorylandMode::DtzArchive) selectDtzPayload(payload);
    else if (gMode == StorylandMode::ModelFile && payload.kind == StorylandTreeKind::ModelField) selectModelField(payload.index);
    else if (gMode == StorylandMode::ModelFile && payload.kind == StorylandTreeKind::ModelBone) selectModelBone(payload.index);
    else if (gMode == StorylandMode::ModelFile && (payload.kind == StorylandTreeKind::AnimOverview || payload.kind == StorylandTreeKind::AnimClip || payload.kind == StorylandTreeKind::AnimTrack || payload.kind == StorylandTreeKind::AnimField || payload.kind == StorylandTreeKind::AnimString)) selectAnimPayload(payload);
    else if (gMode == StorylandMode::ArchiveFile) selectArchivePayload(payload);
    else if (gMode == StorylandMode::WblFile) selectWblPayload(payload);
    else if (gMode == StorylandMode::AnimFile) selectAnimPayload(payload);
}

static bool selectTreeItemAtScreenPoint(POINT screenPoint) {
    if (!gTree) return false;

    POINT clientPoint = screenPoint;
    ScreenToClient(gTree, &clientPoint);

    TVHITTESTINFO hit = {};
    hit.pt = clientPoint;
    HTREEITEM item = TreeView_HitTest(gTree, &hit);
    if (!item) return false;

    TreeView_SelectItem(gTree, item);

    TVITEMW treeItem = {};
    treeItem.mask = TVIF_PARAM;
    treeItem.hItem = item;
    if (TreeView_GetItem(gTree, &treeItem)) {
        selectPayloadForCurrentMode(payloadFromLParam(treeItem.lParam));
    }

    return true;
}

static POINT contextMenuPointFromSelection() {
    POINT point = {};
    if (!gTree) return point;

    HTREEITEM selected = TreeView_GetSelection(gTree);
    if (selected) {
        RECT itemRect = {};
        if (TreeView_GetItemRect(gTree, selected, &itemRect, TRUE)) {
            point.x = itemRect.left;
            point.y = itemRect.bottom;
            ClientToScreen(gTree, &point);
            return point;
        }
    }

    RECT treeRect = {};
    GetWindowRect(gTree, &treeRect);
    point.x = treeRect.left + 24;
    point.y = treeRect.top + 24;
    return point;
}

static bool addContextMenuItem(HMENU menu, UINT commandId, const wchar_t* label) {
    AppendMenuW(menu, MF_STRING, commandId, label);
    return true;
}

static bool addContextMenuSeparatorIfNeeded(HMENU menu, bool hasItems) {
    if (hasItems) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    return hasItems;
}

static bool buildTreeContextMenu(HMENU menu) {
    bool hasItems = false;

    if (gMode == StorylandMode::DtzArchive) {
        if (gSelectedKind == StorylandTreeKind::DtzDirEntry) {
            hasItems = addContextMenuItem(menu, ID_FILE_OPEN_EMBEDDED, L"Open selected internal IMG entry standalone...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_FILE_EXPORT_SELECTED_RESOURCE, L"Export selected internal IMG entry...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_FILE_DMA_TLB_PREFLIGHT, L"Run DMA/VIF Test...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_DTZ_REPLACE_SELECTED_ENTRY, L"Replace selected internal IMG entry from file...") || hasItems;
            addContextMenuSeparatorIfNeeded(menu, hasItems);
            hasItems = addContextMenuItem(menu, ID_DTZ_PATCH_SELECTED, L"Patch selected sector count...") || hasItems;
        } else if (gSelectedKind == StorylandTreeKind::DtzSectorRecord) {
            hasItems = addContextMenuItem(menu, ID_DTZ_PATCH_SELECTED, L"Patch selected sector count...") || hasItems;
        } else if (gSelectedKind == StorylandTreeKind::DtzDataField) {
            hasItems = addContextMenuItem(menu, ID_DTZ_PATCH_DATA_FIELD, L"Patch selected GAME.DTZ data field...") || hasItems;
        }
    } else if (gMode == StorylandMode::ArchiveFile) {
        if (gSelectedKind == StorylandTreeKind::ArchiveEntry) {
            hasItems = addContextMenuItem(menu, ID_FILE_OPEN_EMBEDDED, L"Open selected embedded entry...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_FILE_EXPORT_SELECTED_RESOURCE, L"Export selected LVZ+IMG entry...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_FILE_DMA_TLB_PREFLIGHT, L"Run DMA/VIF Test...") || hasItems;
            addContextMenuSeparatorIfNeeded(menu, hasItems);
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_REPLACE_SELECTED_RESOURCE, L"Replace selected LVZ+IMG resource from file...") || hasItems;
        } else if (gSelectedKind == StorylandTreeKind::ArchiveMeshResource) {
            hasItems = addContextMenuItem(menu, ID_FILE_EXPORT_SELECTED_RESOURCE, L"Export selected mesh resource...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_FILE_DMA_TLB_PREFLIGHT, L"Run DMA/VIF Test...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_REPLACE_SELECTED_RESOURCE, L"Replace selected mesh resource from file...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_REPLACE_MESH_WITH_RESOURCE_ID, L"Clone selected mesh resource from Resource ID...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_CHANGE_SELECTED_MESH_RESOURCE_ID, L"Change selected mesh resource ID...") || hasItems;
        }
    } else if (gMode == StorylandMode::ModelFile) {
        hasItems = addContextMenuItem(menu, ID_FILE_DMA_TLB_PREFLIGHT, L"Run DMA/VIF Test...") || hasItems;
    } else if (gMode == StorylandMode::TextureArchive) {
        if (gSelectedKind == StorylandTreeKind::Texture && gSelectedIndex >= 0) {
            hasItems = addContextMenuItem(menu, ID_FILE_EXPORT_TEXTURE, L"Export selected texture PNG...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_FILE_REPLACE_TEXTURE, L"Replace selected texture...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_FILE_RENAME_TEXTURE, L"Rename selected texture...") || hasItems;
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            hasItems = addContextMenuItem(menu, ID_FILE_EXPORT_TEXTURE_ARCHIVE, L"Export edited texture archive...") || hasItems;
        }
    }

    return hasItems;
}

static void showTreeContextMenu(HWND hwnd, LPARAM lParam) {
    POINT screenPoint = {};
    if (lParam == -1) {
        screenPoint = contextMenuPointFromSelection();
    } else {
        screenPoint.x = GET_X_LPARAM(lParam);
        screenPoint.y = GET_Y_LPARAM(lParam);
        selectTreeItemAtScreenPoint(screenPoint);
    }

    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    bool hasItems = buildTreeContextMenu(menu);
    if (hasItems) {
        TrackPopupMenu(
            menu,
            TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
            screenPoint.x,
            screenPoint.y,
            0,
            hwnd,
            nullptr
        );
    }

    DestroyMenu(menu);
}

static int gMenuStripHover = -1;

static RECT storylandTopMenuRect(int index, int height) {
    static const int widths[] = {50, 54, 50};
    int left = 0;
    for (int i = 0; i < index; ++i) left += widths[i];
    return RECT{left, 0, left + widths[index], height};
}

static int storylandTopMenuAt(POINT point, int height) {
    if (point.y < 0 || point.y >= height) return -1;
    for (int index = 0; index < 3; ++index) {
        RECT item = storylandTopMenuRect(index, height);
        if (point.x >= item.left && point.x < item.right) return index;
    }
    return -1;
}

static void openStorylandTopMenu(HWND strip, int index) {
    const HMENU menus[] = {gFileMenu, gViewMenu, gHelpMenu};
    if (index < 0 || index >= 3 || !menus[index]) return;
    RECT client{};
    GetClientRect(strip, &client);
    RECT item = storylandTopMenuRect(index, client.bottom);
    POINT popup{item.left, client.bottom};
    ClientToScreen(strip, &popup);
    gMenuStripHover = index;
    InvalidateRect(strip, nullptr, FALSE);
    SetForegroundWindow(gMainWindow);
    TrackPopupMenuEx(menus[index], TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
        popup.x, popup.y, gMainWindow, nullptr);
    PostMessageW(gMainWindow, WM_NULL, 0, 0);
    gMenuStripHover = -1;
    InvalidateRect(strip, nullptr, FALSE);
}

static LRESULT CALLBACK storylandMenuStripProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT client{};
        GetClientRect(hwnd, &client);
        COLORREF background = gEyeFriendlyPaneBackground ? RGB(25, 26, 31) : GetSysColor(COLOR_MENU);
        COLORREF hover = gEyeFriendlyPaneBackground ? RGB(47, 49, 57) : GetSysColor(COLOR_MENUHILIGHT);
        COLORREF text = gEyeFriendlyPaneBackground ? RGB(238, 239, 243) : GetSysColor(COLOR_MENUTEXT);
        HBRUSH backgroundBrush = CreateSolidBrush(background);
        FillRect(dc, &client, backgroundBrush);
        DeleteObject(backgroundBrush);
        HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, text);
        static const wchar_t* labels[] = {L"File", L"View", L"Help"};
        for (int index = 0; index < 3; ++index) {
            RECT item = storylandTopMenuRect(index, client.bottom);
            if (index == gMenuStripHover) {
                HBRUSH hoverBrush = CreateSolidBrush(hover);
                FillRect(dc, &item, hoverBrush);
                DeleteObject(hoverBrush);
            }
            DrawTextW(dc, labels[index], -1, &item,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        SelectObject(dc, oldFont);
        EndPaint(hwnd, &paint);
        return 0;
    }
    if (message == WM_MOUSEMOVE) {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT client{};
        GetClientRect(hwnd, &client);
        int hover = storylandTopMenuAt(point, client.bottom);
        if (hover != gMenuStripHover) {
            gMenuStripHover = hover;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tracking);
        return 0;
    }
    if (message == WM_MOUSELEAVE) {
        gMenuStripHover = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (message == WM_LBUTTONDOWN) {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT client{};
        GetClientRect(hwnd, &client);
        openStorylandTopMenu(hwnd, storylandTopMenuAt(point, client.bottom));
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static void createMenuBar(HWND hwnd) {
    gMainMenu = CreateMenu();

    gFileMenu = CreatePopupMenu();
    AppendMenuW(gFileMenu, MF_STRING, ID_FILE_OPEN, L"Open...");
    gRecentMenu = CreatePopupMenu();
    rebuildRecentMenu();
    AppendMenuW(gFileMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gRecentMenu), L"Open recent");
    AppendMenuW(gFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(gFileMenu, MF_STRING, ID_FILE_SAVE_AS, L"Save As / Rebuild GAME.DTZ...");
    AppendMenuW(gFileMenu, MF_STRING, ID_FILE_EXPORT_LOG, L"Export Log...");
    AppendMenuW(gFileMenu, MF_STRING, ID_FILE_DMA_TLB_PREFLIGHT, L"Run DMA/VIF Test...");
    AppendMenuW(gFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(gFileMenu, MF_STRING, ID_ARCHIVE_EXPORT_LVZ_IMG_PAIR, L"Export/Rebuild LVZ+IMG Pair...");
    AppendMenuW(gFileMenu, MF_STRING, ID_ARCHIVE_OVERWRITE_LVZ_IMG_PAIR, L"Overwrite Current LVZ+IMG Pair...");
    AppendMenuW(gFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(gFileMenu, MF_STRING, ID_FILE_QUIT, L"Quit");
    AppendMenuW(gMainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gFileMenu), L"File");

    gViewMenu = CreatePopupMenu();
    HMENU view = gViewMenu;
    gTexturePreviewMenu = CreatePopupMenu();
    AppendMenuW(gTexturePreviewMenu, MF_STRING, ID_VIEW_FLIP_TEXTURE_PREVIEW_V, L"Flip archive texture V");
    CheckMenuItem(gTexturePreviewMenu, ID_VIEW_FLIP_TEXTURE_PREVIEW_V, MF_BYCOMMAND | (gTexturePreviewFlipV ? MF_CHECKED : MF_UNCHECKED));
    AppendMenuW(gTexturePreviewMenu, MF_STRING, ID_VIEW_FLIP_MODEL_TEXTURE_V, L"Flip MDL texture V");
    CheckMenuItem(gTexturePreviewMenu, ID_VIEW_FLIP_MODEL_TEXTURE_V, MF_BYCOMMAND | (gModelFlipTextureV ? MF_CHECKED : MF_UNCHECKED));
    AppendMenuW(view, MF_POPUP, reinterpret_cast<UINT_PTR>(gTexturePreviewMenu), L"Texture preview");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    gOpenGlMenu = CreatePopupMenu();
    AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_RENDER_STORIES, L"Stories shader");
    AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_RENDER_TEXTURED, L"Textured");
    AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_RENDER_SOLID, L"Solid");
    AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_RENDER_WIREFRAME, L"Wireframe");
    UINT checkedRender = ID_VIEW_RENDER_STORIES;
    if (gOpenGlRenderMode == StorylandOpenGlRenderMode::Textured) checkedRender = ID_VIEW_RENDER_TEXTURED;
    else if (gOpenGlRenderMode == StorylandOpenGlRenderMode::Solid) checkedRender = ID_VIEW_RENDER_SOLID;
    else if (gOpenGlRenderMode == StorylandOpenGlRenderMode::Wireframe) checkedRender = ID_VIEW_RENDER_WIREFRAME;
    CheckMenuRadioItem(gOpenGlMenu, ID_VIEW_RENDER_STORIES, ID_VIEW_RENDER_WIREFRAME, checkedRender, MF_BYCOMMAND);
    AppendMenuW(gOpenGlMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_SHOW_GRID, L"Show grid");
    CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_GRID, MF_BYCOMMAND | (gOpenGlShowGrid ? MF_CHECKED : MF_UNCHECKED));
    AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_SHOW_BONES, L"Show bones");
    CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_BONES, MF_BYCOMMAND | (gOpenGlShowBones ? MF_CHECKED : MF_UNCHECKED));
    AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_SHOW_BOUNDS, L"Show bounds");
    CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_BOUNDS, MF_BYCOMMAND | (gOpenGlShowBounds ? MF_CHECKED : MF_UNCHECKED));
    AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_SHOW_VIEWCUBE, L"Show viewport cube");
    CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_VIEWCUBE, MF_BYCOMMAND | (gOpenGlShowViewCube ? MF_CHECKED : MF_UNCHECKED));
    AppendMenuW(view, MF_POPUP, reinterpret_cast<UINT_PTR>(gOpenGlMenu), L"OpenGL preview");

    gSkyMenu = CreatePopupMenu();
    HMENU sky = gSkyMenu;
    AppendMenuW(sky, MF_STRING, ID_VIEW_SHOW_SKY, L"Show Stories sky");
    CheckMenuItem(sky, ID_VIEW_SHOW_SKY, MF_BYCOMMAND | (gStoriesSky.isEnabled() ? MF_CHECKED : MF_UNCHECKED));
    AppendMenuW(sky, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(sky, MF_STRING, ID_VIEW_SKY_LCS, L"Liberty City Stories");
    AppendMenuW(sky, MF_STRING, ID_VIEW_SKY_VCS, L"Vice City Stories");
    CheckMenuRadioItem(sky, ID_VIEW_SKY_LCS, ID_VIEW_SKY_VCS,
                       gStoriesSky.game() == StorylandSkyGame::Lcs ? ID_VIEW_SKY_LCS : ID_VIEW_SKY_VCS,
                       MF_BYCOMMAND);
    AppendMenuW(sky, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(sky, MF_STRING, ID_VIEW_SKY_MIDNIGHT, L"Midnight");
    AppendMenuW(sky, MF_STRING, ID_VIEW_SKY_DAWN, L"Dawn");
    AppendMenuW(sky, MF_STRING, ID_VIEW_SKY_NOON, L"Noon");
    AppendMenuW(sky, MF_STRING, ID_VIEW_SKY_SUNSET, L"Sunset");
    AppendMenuW(sky, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(sky, MF_STRING, ID_VIEW_SKY_WEATHER_SUNNY, L"Sunny");
    AppendMenuW(sky, MF_STRING, ID_VIEW_SKY_WEATHER_CLOUDY, L"Cloudy");
    AppendMenuW(sky, MF_STRING, ID_VIEW_SKY_WEATHER_RAINY, L"Rainy");
    AppendMenuW(sky, MF_STRING, ID_VIEW_SKY_WEATHER_FOGGY, L"Foggy");
    CheckMenuRadioItem(sky, ID_VIEW_SKY_WEATHER_SUNNY, ID_VIEW_SKY_WEATHER_FOGGY,
                       ID_VIEW_SKY_WEATHER_SUNNY, MF_BYCOMMAND);
    AppendMenuW(view, MF_POPUP, reinterpret_cast<UINT_PTR>(sky), L"Stories sky");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    gBackgroundMenu = CreatePopupMenu();
    AppendMenuW(gBackgroundMenu, MF_STRING, ID_VIEW_BACKGROUND_LIGHT, L"Light");
    AppendMenuW(gBackgroundMenu, MF_STRING, ID_VIEW_BACKGROUND_DARK, L"Dark");
    CheckMenuRadioItem(gBackgroundMenu, ID_VIEW_BACKGROUND_LIGHT, ID_VIEW_BACKGROUND_DARK,
                       gEyeFriendlyPaneBackground ? ID_VIEW_BACKGROUND_DARK : ID_VIEW_BACKGROUND_LIGHT,
                       MF_BYCOMMAND);
    AppendMenuW(view, MF_POPUP, reinterpret_cast<UINT_PTR>(gBackgroundMenu), L"Background");
    AppendMenuW(gMainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"View");

    gHelpMenu = CreatePopupMenu();
    AppendMenuW(gHelpMenu, MF_STRING, ID_HELP_ABOUT, L"About Storyland...");
    AppendMenuW(gHelpMenu, MF_STRING, ID_HELP_WIKI, L"Wiki");
    AppendMenuW(gMainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gHelpMenu), L"Help");
}

static LRESULT CALLBACK mainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        createMenuBar(hwnd);
        WNDCLASSW menuStripClass{};
        menuStripClass.lpfnWndProc = storylandMenuStripProc;
        menuStripClass.hInstance = gInstance;
        menuStripClass.lpszClassName = L"StorylandMenuStripClass";
        menuStripClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        RegisterClassW(&menuStripClass);
        gMenuStrip = CreateWindowExW(0, menuStripClass.lpszClassName, nullptr,
            WS_CHILD | WS_VISIBLE, 0, 0, 100, 28, hwnd,
            reinterpret_cast<HMENU>(ID_MENU_STRIP), gInstance, nullptr);
        gTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | WS_VSCROLL | WS_HSCROLL, 0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(ID_TREE), gInstance, nullptr);
        WNDCLASSW previewClass = {};
        previewClass.lpfnWndProc = previewProc;
        previewClass.hInstance = gInstance;
        previewClass.lpszClassName = L"StorylandPreviewClass";
        previewClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        previewClass.style = CS_OWNDC | CS_DBLCLKS;
        RegisterClassW(&previewClass);
        gPreview = CreateWindowExW(WS_EX_CLIENTEDGE, previewClass.lpszClassName, nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(ID_PREVIEW), gInstance, nullptr);
        initializeOpenGlPreview(gPreview);
        gDetails = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(ID_DETAILS), gInstance, nullptr);
        gStatus = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(ID_STATUS), gInstance, nullptr);
        applyStorylandPaneBackground(hwnd);
        gStoriesSkyLastTick = GetTickCount();
        SetTimer(hwnd, 2, 33, nullptr);
        setStatus(L"Open .chk/.xtx/.tex textures, .mdl/.wbl models, .anim files, or GAME.DTZ");
        return 0;
    }
    case WM_SIZE:
        layoutChildren(hwnd);
        return 0;

    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(hwnd, &client);
        HBRUSH brush = CreateSolidBrush(storylandFrameBackgroundColor());
        FillRect(reinterpret_cast<HDC>(wParam), &client, brush);
        DeleteObject(brush);
        return 1;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        if (reinterpret_cast<HWND>(lParam) == gDetails) {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, storylandPaneTextColor());
            SetBkColor(dc, storylandPaneBackgroundColor());
            return reinterpret_cast<LRESULT>(gPaneBackgroundBrush);
        }
        break;

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (draw && draw->CtlID == ID_STATUS) {
            HBRUSH brush = CreateSolidBrush(gEyeFriendlyPaneBackground
                ? RGB(25, 26, 31) : GetSysColor(COLOR_BTNFACE));
            FillRect(draw->hDC, &draw->rcItem, brush);
            DeleteObject(brush);
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, storylandPaneTextColor());
            HFONT font = reinterpret_cast<HFONT>(SendMessageW(gStatus, WM_GETFONT, 0, 0));
            HGDIOBJ oldFont = font ? SelectObject(draw->hDC, font) : nullptr;
            RECT textRect = draw->rcItem;
            textRect.left += 4;
            DrawTextW(draw->hDC, gStatusText.c_str(), int(gStatusText.size()), &textRect,
                DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (oldFont) SelectObject(draw->hDC, oldFont);
            return TRUE;
        }
        break;
    }

    case WM_TIMER:
        if (wParam == 1 && advanceAnimationPlaybackFrame()) {
            return 0;
        }
        if (wParam == 2) {
            DWORD tick = GetTickCount();
            float elapsedSeconds = float(tick - gStoriesSkyLastTick) / 1000.0f;
            gStoriesSkyLastTick = tick;
            gStoriesSky.update(std::min(elapsedSeconds, 0.25f));
            if (gOpenGlRenderMode == StorylandOpenGlRenderMode::Stories && gStoriesSky.isEnabled()) {
                InvalidateRect(gPreview, nullptr, FALSE);
            }
            return 0;
        }
        break;

    case WM_CONTEXTMENU:
        if (reinterpret_cast<HWND>(wParam) == gTree) {
            showTreeContextMenu(hwnd, lParam);
            return 0;
        }
        break;

    case WM_DWMCOLORIZATIONCOLORCHANGED:
        applyStorylandTitleTint(gTitleTint);
        return 0;

    case WM_SYSCHAR:
        switch (towlower(wchar_t(wParam))) {
        case L'f': openStorylandTopMenu(gMenuStrip, 0); return 0;
        case L'v': openStorylandTopMenu(gMenuStrip, 1); return 0;
        case L'h': openStorylandTopMenu(gMenuStrip, 2); return 0;
        default: break;
        }
        break;

    case WM_COMMAND: {
        const UINT commandId = LOWORD(wParam);
        if (commandId >= ID_FILE_RECENT_BASE &&
            commandId < ID_FILE_RECENT_BASE + STORYLAND_MAX_RECENT_FILES) {
            const size_t recentIndex = size_t(commandId - ID_FILE_RECENT_BASE);
            if (recentIndex < gRecentFiles.size()) {
                const std::wstring recentPath = gRecentFiles[recentIndex];
                openStorylandFile(recentPath);
            }
            return 0;
        }
        switch (commandId) {
        case ID_FILE_OPEN: {
            std::wstring path = openFileDialog(L"Storyland files\0*.dtz;*.bin;*.img;*.lvz;*.area;*.wbl;*.mdl;*.dff;*.anim;*.chk;*.xtx;*.tex\0All files\0*.*\0");
            openStorylandFile(path);
            break;
        }
        case ID_FILE_OPEN_EMBEDDED:
            if (gMode == StorylandMode::ArchiveFile && gSelectedKind == StorylandTreeKind::ArchiveEntry && gSelectedIndex >= 0) openSelectedArchiveEntry();
            else if (gMode == StorylandMode::DtzArchive && gSelectedKind == StorylandTreeKind::DtzDirEntry && gSelectedIndex >= 0) openSelectedDtzDirEntryStandalone();
            else MessageBoxW(gMainWindow, L"Select an embedded archive entry or GAME.DTZ internal IMG entry first.", L"Storyland", MB_ICONINFORMATION);
            break;
        case ID_FILE_RECENT_CLEAR:
            gRecentFiles.clear();
            saveRecentFiles();
            rebuildRecentMenu();
            break;
        case ID_FILE_QUIT:
            SendMessageW(hwnd, WM_CLOSE, 0, 0);
            break;
        case ID_FILE_SAVE_AS: saveCurrentAs(); break;
        case ID_FILE_EXPORT_LOG: exportCurrentLog(); break;
        case ID_FILE_EXPORT_SELECTED_RESOURCE: exportSelectedResourceBytes(); break;
        case ID_FILE_DMA_TLB_PREFLIGHT: runSelectedDmaTlbPreflight(); break;
        case ID_FILE_EXPORT_TEXTURE: exportSelectedTexture(); break;
        case ID_FILE_REPLACE_TEXTURE: replaceSelectedTexture(); break;
        case ID_FILE_RENAME_TEXTURE: renameSelectedTexture(); break;
        case ID_FILE_EXPORT_TEXTURE_ARCHIVE: exportEditedTextureArchive(); break;
        case ID_DTZ_REPLACE_SELECTED_ENTRY: replaceSelectedDtzDirEntryFromFile(); break;
        case ID_ARCHIVE_REPLACE_SELECTED_RESOURCE: replaceSelectedArchiveResourceFromFile(); break;
        case ID_ARCHIVE_REPLACE_MESH_WITH_RESOURCE_ID: replaceSelectedMeshResourceWithResourceId(); break;
        case ID_ARCHIVE_CHANGE_SELECTED_MESH_RESOURCE_ID: changeSelectedMeshResourceId(); break;
        case ID_ARCHIVE_EXPORT_LVZ_IMG_PAIR: exportLvzImgPair(); break;
        case ID_ARCHIVE_OVERWRITE_LVZ_IMG_PAIR: overwriteCurrentLvzImgPair(); break;
        case ID_VIEW_BACKGROUND_LIGHT:
            gEyeFriendlyPaneBackground = false;
            CheckMenuRadioItem(gBackgroundMenu, ID_VIEW_BACKGROUND_LIGHT, ID_VIEW_BACKGROUND_DARK,
                               ID_VIEW_BACKGROUND_LIGHT, MF_BYCOMMAND);
            applyStorylandPaneBackground(hwnd);
            applyStorylandTitleTint(gTitleTint);
            setStatus(gStatusText);
            break;
        case ID_VIEW_BACKGROUND_DARK:
            gEyeFriendlyPaneBackground = true;
            CheckMenuRadioItem(gBackgroundMenu, ID_VIEW_BACKGROUND_LIGHT, ID_VIEW_BACKGROUND_DARK,
                               ID_VIEW_BACKGROUND_DARK, MF_BYCOMMAND);
            applyStorylandPaneBackground(hwnd);
            applyStorylandTitleTint(gTitleTint);
            setStatus(gStatusText);
            break;
        case ID_VIEW_FLIP_TEXTURE_PREVIEW_V:
            gTexturePreviewFlipV = !gTexturePreviewFlipV;
            CheckMenuItem(gTexturePreviewMenu, ID_VIEW_FLIP_TEXTURE_PREVIEW_V, MF_BYCOMMAND | (gTexturePreviewFlipV ? MF_CHECKED : MF_UNCHECKED));
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_FLIP_MODEL_TEXTURE_V:
            gModelFlipTextureV = !gModelFlipTextureV;
            CheckMenuItem(gTexturePreviewMenu, ID_VIEW_FLIP_MODEL_TEXTURE_V, MF_BYCOMMAND | (gModelFlipTextureV ? MF_CHECKED : MF_UNCHECKED));
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_RENDER_STORIES:
            setOpenGlRenderMode(StorylandOpenGlRenderMode::Stories);
            CheckMenuRadioItem(gOpenGlMenu, ID_VIEW_RENDER_STORIES, ID_VIEW_RENDER_WIREFRAME, ID_VIEW_RENDER_STORIES, MF_BYCOMMAND);
            break;
        case ID_VIEW_RENDER_TEXTURED:
            setOpenGlRenderMode(StorylandOpenGlRenderMode::Textured);
            CheckMenuRadioItem(gOpenGlMenu, ID_VIEW_RENDER_STORIES, ID_VIEW_RENDER_WIREFRAME, ID_VIEW_RENDER_TEXTURED, MF_BYCOMMAND);
            break;
        case ID_VIEW_RENDER_SOLID:
            setOpenGlRenderMode(StorylandOpenGlRenderMode::Solid);
            CheckMenuRadioItem(gOpenGlMenu, ID_VIEW_RENDER_STORIES, ID_VIEW_RENDER_WIREFRAME, ID_VIEW_RENDER_SOLID, MF_BYCOMMAND);
            break;
        case ID_VIEW_RENDER_WIREFRAME:
            setOpenGlRenderMode(StorylandOpenGlRenderMode::Wireframe);
            CheckMenuRadioItem(gOpenGlMenu, ID_VIEW_RENDER_STORIES, ID_VIEW_RENDER_WIREFRAME, ID_VIEW_RENDER_WIREFRAME, MF_BYCOMMAND);
            break;
        case ID_VIEW_SHOW_GRID:
            gOpenGlShowGrid = !gOpenGlShowGrid;
            CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_GRID, MF_BYCOMMAND | (gOpenGlShowGrid ? MF_CHECKED : MF_UNCHECKED));
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SHOW_BONES:
            gOpenGlShowBones = !gOpenGlShowBones;
            CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_BONES, MF_BYCOMMAND | (gOpenGlShowBones ? MF_CHECKED : MF_UNCHECKED));
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SHOW_BOUNDS:
            gOpenGlShowBounds = !gOpenGlShowBounds;
            CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_BOUNDS, MF_BYCOMMAND | (gOpenGlShowBounds ? MF_CHECKED : MF_UNCHECKED));
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SHOW_VIEWCUBE:
            gOpenGlShowViewCube = !gOpenGlShowViewCube;
            CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_VIEWCUBE, MF_BYCOMMAND | (gOpenGlShowViewCube ? MF_CHECKED : MF_UNCHECKED));
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SHOW_SKY:
            gStoriesSky.setEnabled(!gStoriesSky.isEnabled());
            CheckMenuItem(gSkyMenu, ID_VIEW_SHOW_SKY, MF_BYCOMMAND |
                          (gStoriesSky.isEnabled() ? MF_CHECKED : MF_UNCHECKED));
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SKY_LCS:
            gStoriesSky.setGame(StorylandSkyGame::Lcs);
            CheckMenuRadioItem(gSkyMenu, ID_VIEW_SKY_LCS, ID_VIEW_SKY_VCS,
                               ID_VIEW_SKY_LCS, MF_BYCOMMAND);
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SKY_VCS:
            gStoriesSky.setGame(StorylandSkyGame::Vcs);
            CheckMenuRadioItem(gSkyMenu, ID_VIEW_SKY_LCS, ID_VIEW_SKY_VCS,
                               ID_VIEW_SKY_VCS, MF_BYCOMMAND);
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SKY_MIDNIGHT:
            gStoriesSky.setTime(0.0f);
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SKY_DAWN:
            gStoriesSky.setTime(6.0f);
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SKY_NOON:
            gStoriesSky.setTime(12.0f);
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SKY_SUNSET:
            gStoriesSky.setTime(19.0f);
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SKY_WEATHER_SUNNY:
            gStoriesSky.setWeather(StorylandSkyWeather::Sunny);
            CheckMenuRadioItem(gSkyMenu, ID_VIEW_SKY_WEATHER_SUNNY,
                               ID_VIEW_SKY_WEATHER_FOGGY, ID_VIEW_SKY_WEATHER_SUNNY, MF_BYCOMMAND);
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SKY_WEATHER_CLOUDY:
            gStoriesSky.setWeather(StorylandSkyWeather::Cloudy);
            CheckMenuRadioItem(gSkyMenu, ID_VIEW_SKY_WEATHER_SUNNY,
                               ID_VIEW_SKY_WEATHER_FOGGY, ID_VIEW_SKY_WEATHER_CLOUDY, MF_BYCOMMAND);
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SKY_WEATHER_RAINY:
            gStoriesSky.setWeather(StorylandSkyWeather::Rainy);
            CheckMenuRadioItem(gSkyMenu, ID_VIEW_SKY_WEATHER_SUNNY,
                               ID_VIEW_SKY_WEATHER_FOGGY, ID_VIEW_SKY_WEATHER_RAINY, MF_BYCOMMAND);
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_VIEW_SKY_WEATHER_FOGGY:
            gStoriesSky.setWeather(StorylandSkyWeather::Foggy);
            CheckMenuRadioItem(gSkyMenu, ID_VIEW_SKY_WEATHER_SUNNY,
                               ID_VIEW_SKY_WEATHER_FOGGY, ID_VIEW_SKY_WEATHER_FOGGY, MF_BYCOMMAND);
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        case ID_DTZ_PATCH_SELECTED: patchSelectedDtzRecord(); break;
        case ID_DTZ_PATCH_DATA_FIELD: patchSelectedDtzDataField(); break;
        case ID_DTZ_PATCH_PLR_23: patchPlrPair23(); break;
        case ID_HELP_ABOUT: showAboutDialog(); break;
        case ID_HELP_WIKI:
            ShellExecuteW(hwnd, L"open", L"https://github.com/spicybung/BLeeds/wiki", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        return 0;
    }
    case WM_NOTIFY: {
        LPNMHDR header = reinterpret_cast<LPNMHDR>(lParam);
        if (header && header->idFrom == ID_TREE && header->code == TVN_SELCHANGEDW) {
            NMTREEVIEWW* changed = reinterpret_cast<NMTREEVIEWW*>(lParam);
            StorylandTreePayload payload = payloadFromLParam(changed->itemNew.lParam);
            selectPayloadForCurrentMode(payload);
            return 0;
        }
        if (header && header->idFrom == ID_TREE && header->code == NM_DBLCLK) {
            if (gMode == StorylandMode::ArchiveFile && gSelectedKind == StorylandTreeKind::ArchiveEntry && gSelectedIndex >= 0) {
                openSelectedArchiveEntry();
                return 0;
            }
            if (gMode == StorylandMode::DtzArchive && gSelectedKind == StorylandTreeKind::DtzDirEntry && gSelectedIndex >= 0) {
                openSelectedDtzDirEntryStandalone();
                return 0;
            }
        }
        break;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 2);
        deleteTextureBitmap();
        destroyOpenGlPreview();
        if (gPaneBackgroundBrush) {
            DeleteObject(gPaneBackgroundBrush);
            gPaneBackgroundBrush = nullptr;
        }
        if (gMenuBackgroundBrush) {
            DeleteObject(gMenuBackgroundBrush);
            gMenuBackgroundBrush = nullptr;
        }
        if (gMainMenu) {
            DestroyMenu(gMainMenu);
            gMainMenu = nullptr;
            gFileMenu = nullptr;
            gViewMenu = nullptr;
            gSkyMenu = nullptr;
            gHelpMenu = nullptr;
            gTexturePreviewMenu = nullptr;
            gRecentMenu = nullptr;
            gOpenGlMenu = nullptr;
            gBackgroundMenu = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static HICON loadStorylandIcon(HINSTANCE instance, int width, int height) {
    HICON icon = static_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_STORYLAND),
        IMAGE_ICON,
        width,
        height,
        LR_DEFAULTCOLOR | LR_SHARED
    ));
    if (icon) return icon;
    return LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
}

struct StorylandSplashDrop {
    float headY = 0.0f;
    float speed = 7.0f;
    int trail = 10;
    uint32_t seed = 0;
};

static std::vector<StorylandSplashDrop> gSplashDrops;
static int gSplashTick = 0;
static int gSplashCell = 16;
static bool gSplashSkipRequested = false;

static float splashPaletteProgress(float phase = 0.0f) {
    float value = std::clamp((float(gSplashTick) - 5.0f) / 40.0f + phase, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

static COLORREF splashBlend(COLORREF blue, COLORREF pink, float amount) {
    auto channel = [&](BYTE a, BYTE b) {
        return BYTE(std::clamp(int(std::lround(float(a) + (float(b) - float(a)) * amount)), 0, 255));
    };
    return RGB(channel(GetRValue(blue), GetRValue(pink)),
               channel(GetGValue(blue), GetGValue(pink)),
               channel(GetBValue(blue), GetBValue(pink)));
}

static COLORREF splashScale(COLORREF colour, float scale) {
    return RGB(BYTE(std::clamp(int(GetRValue(colour) * scale), 0, 255)),
               BYTE(std::clamp(int(GetGValue(colour) * scale), 0, 255)),
               BYTE(std::clamp(int(GetBValue(colour) * scale), 0, 255)));
}

static uint32_t splashHash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return value;
}

static void resetSplashDrops(int width, int height) {
    int columns = std::max(1, (width + gSplashCell - 1) / gSplashCell);
    gSplashDrops.resize(size_t(columns));
    for (int column = 0; column < columns; ++column) {
        uint32_t random = splashHash(0x52454947u + uint32_t(column) * 0x9E3779B9u);
        StorylandSplashDrop& drop = gSplashDrops[size_t(column)];
        drop.seed = random;
        drop.headY = -float(random % uint32_t(std::max(1, height + 320)));
        drop.speed = 5.0f + float((random >> 9) % 9u);
        drop.trail = 7 + int((random >> 17) % 15u);
    }
}

static void drawSplashMatrixRain(HDC dc, const RECT& client) {
    static const wchar_t glyphs[] =
        L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ<>[]{}+=*#:/\\|"
        L"ｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓ";
    constexpr size_t glyphCount = (sizeof(glyphs) / sizeof(glyphs[0])) - 1u;

    HFONT rainFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN, L"Consolas");
    HGDIOBJ oldFont = SelectObject(dc, rainFont);
    SetBkMode(dc, TRANSPARENT);

    for (size_t column = 0; column < gSplashDrops.size(); ++column) {
        const StorylandSplashDrop& drop = gSplashDrops[column];
        const float columnPhase = gSplashDrops.size() > 1
            ? (float(column) / float(gSplashDrops.size() - 1u) - 0.5f) * 0.34f : 0.0f;
        const float palette = splashPaletteProgress(columnPhase);
        const COLORREF bright = splashBlend(RGB(214, 238, 255), RGB(255, 211, 237), palette);
        const COLORREF body = splashBlend(RGB(30, 132, 255), RGB(255, 116, 196), palette);
        int x = int(column) * gSplashCell;
        for (int trailIndex = drop.trail; trailIndex >= 0; --trailIndex) {
            int y = int(drop.headY) - trailIndex * gSplashCell;
            if (y < -gSplashCell || y >= client.bottom) continue;
            uint32_t random = splashHash(drop.seed ^ uint32_t(gSplashTick * 37 - trailIndex * 101));
            wchar_t glyph = glyphs[random % glyphCount];
            if (trailIndex == 0) {
                SetTextColor(dc, bright);
            } else {
                int strength = 28 + (drop.trail - trailIndex) * 155 / std::max(1, drop.trail);
                SetTextColor(dc, splashScale(body, float(strength) / 210.0f));
            }
            TextOutW(dc, x, y, &glyph, 1);
        }
    }

    SelectObject(dc, oldFont);
    DeleteObject(rainFont);
}

static const wchar_t* reignsStudiosBlockLogo() {
    return
        L" ▄▄▄▄▄▄▄       ▄▄▄▄  ▄▄▄     ▄▄▄▄▄    ▄▄▄   ▄▄▄    ▄▄▄▄▄▄▄        ▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄▄▄  ▄▄▄   ▄▄▄ ▄▄▄▄▄▄▄     ▄▄▄     ▄▄▄       ▄▄▄▄▄▄▄\r\n"
        L"▐███▓▓▓▓▀▄  ▄██████▌▐███▌ ▄███████▌  ▐███▄ ▐███▌ ▄████████▌     ▄████████▌▐█████████▌▐███▌ ▐███▌▐████████▄ ▐███▌ ▄███████▄  ▄████████▌\r\n"
        L"▐░░░▌ ▐░░▐ ▐░░░█▀   ▐░░░▌▐░░░█▀      ▐░░░░▌▐░░░▌▐░░░▌ ▐░░░▌    ▐░░░▌ ▐░░░▌   ▐░░░▌   ▐░░░▌ ▐░░░▌▐░░░▌ ▐░░░▌▐░░░▌▐░░░▌ ▐░░░▌▐░░░▌ ▐░░░▌\r\n"
        L"▐▒▒▒▌  ▒▒▐ ▐▒▒▒▌    ▐▒▒▒▌▐▒▒▒▌       ▐▒▒▒▌▌▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌    ▐▒▒▒▌ ▐▒▒▒▌   ▐▒▒▒▌   ▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌\r\n"
        L"▐▓▓▓▌ ▐▓▓▌ ▐▓▓▓▓▓▓  ▐▓▓▓▌▐▓▓▓▌██████▌▐▓▓▓▌▐▐▓▓▓▌▐▓▓▓▌  ▀▀▀     ▐▓▓▓▌  ▀▀▀    ▐▓▓▓▌   ▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌  ▀▀▀\r\n"
        L"▐███▌ ███  ▐████▀▀  ▐███▌▐███▌ ▐███▌ ▐███▌▐▌███▌ ▀███████▄      ▀███████▄    ▐███▌   ▐███▌ ▐███▌▐███▌ ▐███▌▐███▌▐███▌ ▐███▌ ▀███████▄\r\n"
        L"▐░░░▌▐░░▌  ▐░░░▌    ▐░░░▌▐░░░▌ ▐░░░▌ ▐░░░▌ ▌░░░▌ ▄▄▄  ▐░░░▌     ▄▄▄  ▐░░░▌   ▐░░░▌   ▐░░░▌ ▐░░░▌▐░░░▌ ▐░░░▌▐░░░▌▐░░░▌ ▐░░░▌ ▄▄▄  ▐░░░▌\r\n"
        L"▐▒▒▒▌▓▒█   ▐▒▒▒▌    ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌ ▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌    ▐▒▒▒▌ ▐▒▒▒▌   ▐▒▒▒▌   ▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌\r\n"
        L"▐▓▓▓▌▐▓▓█▄ ▐▓▓▓▓▄▄▄ ▐▓▓▓▌▐▓▓▓▓▄▓▓▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌    ▐▓▓▓▌ ▐▓▓▓▌   ▐▓▓▓▌    ▌▓▓▓▄▓▓▓▐ ▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌\r\n"
        L"▐███▌ ▀███▌ ▀▄█████▌▐███▌ ▀▄████████▌▐███▌ ▐███▌▐████████▀     ▐████████▀    ▐███▌    ▀▄█████▄▀ ▐████████▀ ▐███▌ ▀███████▀ ▐████████▀";
}

static void drawReignsStudiosLogo(HDC dc, const RECT& client) {
    int width = client.right - client.left;
    int logoWidth = width < 1100 ? 5 : 6;
    int logoHeight = width < 1100 ? 10 : 12;
    HFONT logoFont = CreateFontW(-logoHeight, logoWidth, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN, L"Consolas");
    HFONT labelFont = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT storylandFont = CreateFontW(-38, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, logoFont);
    RECT logo{18, std::max(50, int(client.bottom) / 2 - 116), client.right - 18, client.bottom - 120};
    int reveal = std::clamp((gSplashTick - 5) * 19, 0, 255);
    const float palette = splashPaletteProgress();
    const COLORREF logoColour = splashBlend(RGB(48, 152, 255), RGB(255, 128, 204), palette);
    const COLORREF logoShadow = splashBlend(RGB(0, 32, 96), RGB(78, 20, 88), palette);

    RECT shadow = logo;
    OffsetRect(&shadow, 2, 2);
    SetTextColor(dc, splashScale(logoShadow, float(reveal) / 255.0f));
    DrawTextW(dc, reignsStudiosBlockLogo(), -1, &shadow,
              DT_CENTER | DT_TOP | DT_NOPREFIX | DT_NOCLIP);
    SetTextColor(dc, splashScale(logoColour, float(reveal) / 255.0f));
    DrawTextW(dc, reignsStudiosBlockLogo(), -1, &logo,
              DT_CENTER | DT_TOP | DT_NOPREFIX | DT_NOCLIP);

    if (gSplashTick >= 17) {
        int lowerY = client.bottom - 103;
        HICON icon = loadStorylandIcon(gInstance, 46, 46);
        if (icon) DrawIconEx(dc, client.right / 2 - 122, lowerY + 15, icon, 46, 46, 0, nullptr, DI_NORMAL);

        SelectObject(dc, labelFont);
        SetTextColor(dc, splashBlend(RGB(120, 194, 255), RGB(255, 151, 214), palette));
        RECT presents{0, lowerY, client.right, lowerY + 26};
        DrawTextW(dc, L"REIGNS STUDIOS PRESENTS", -1, &presents,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(dc, storylandFont);
        SetTextColor(dc, splashBlend(RGB(226, 241, 255), RGB(255, 211, 235), palette));
        RECT storyland{0, lowerY + 24, client.right, client.bottom - 18};
        DrawTextW(dc, L"STORYLAND", -1, &storyland,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    SelectObject(dc, oldFont);
    DeleteObject(logoFont);
    DeleteObject(labelFont);
    DeleteObject(storylandFont);
}

static LRESULT CALLBACK storylandSplashProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_CREATE) {
        gSplashTick = 0;
        gSplashSkipRequested = false;
        SetTimer(hwnd, 1, 40, nullptr);
        return 0;
    }
    if (message == WM_SIZE) {
        resetSplashDrops(LOWORD(lParam), HIWORD(lParam));
        return 0;
    }
    if (message == WM_TIMER && wParam == 1) {
        gSplashTick++;
        RECT client{};
        GetClientRect(hwnd, &client);
        for (StorylandSplashDrop& drop : gSplashDrops) {
            drop.headY += drop.speed;
            if (drop.headY - float(drop.trail * gSplashCell) > float(client.bottom)) {
                uint32_t random = splashHash(drop.seed ^ uint32_t(gSplashTick * 0x9E37));
                drop.headY = -float(40u + random % 360u);
                drop.speed = 5.0f + float((random >> 8) % 9u);
                drop.trail = 7 + int((random >> 18) % 15u);
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (message == WM_LBUTTONDOWN || message == WM_KEYDOWN) {
        gSplashSkipRequested = true;
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT client{};
        GetClientRect(hwnd, &client);
        int width = std::max<LONG>(1, client.right - client.left);
        int height = std::max<LONG>(1, client.bottom - client.top);
        HDC memoryDc = CreateCompatibleDC(dc);
        HBITMAP frame = CreateCompatibleBitmap(dc, width, height);
        HGDIOBJ oldBitmap = SelectObject(memoryDc, frame);
        const float palette = splashPaletteProgress();
        HBRUSH background = CreateSolidBrush(splashBlend(RGB(0, 4, 14), RGB(12, 4, 22), palette));
        FillRect(memoryDc, &client, background);
        DeleteObject(background);

        drawSplashMatrixRain(memoryDc, client);
        drawReignsStudiosLogo(memoryDc, client);

        HPEN border = CreatePen(PS_SOLID, 2,
            splashBlend(RGB(32, 112, 224), RGB(255, 128, 204), palette));
        HGDIOBJ oldPen = SelectObject(memoryDc, border);
        HGDIOBJ oldBrush = SelectObject(memoryDc, GetStockObject(NULL_BRUSH));
        RoundRect(memoryDc, 1, 1, client.right - 1, client.bottom - 1, 24, 24);
        SelectObject(memoryDc, oldBrush);
        SelectObject(memoryDc, oldPen);
        DeleteObject(border);

        BitBlt(dc, 0, 0, width, height, memoryDc, 0, 0, SRCCOPY);
        SelectObject(memoryDc, oldBitmap);
        DeleteObject(frame);
        DeleteDC(memoryDc);
        EndPaint(hwnd, &paint);
        return 0;
    }
    if (message == WM_DESTROY) {
        KillTimer(hwnd, 1);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static void pumpSplashMessages(HWND splash) {
    MSG message{};
    while (PeekMessageW(&message, splash, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

static HWND createStorylandSplash(HINSTANCE instance, ULONGLONG& startedAt) {
    WNDCLASSEXW splashClass{};
    splashClass.cbSize = sizeof(splashClass);
    splashClass.style = CS_DROPSHADOW;
    splashClass.lpfnWndProc = storylandSplashProc;
    splashClass.hInstance = instance;
    splashClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    splashClass.lpszClassName = L"StorylandSplashWindow";
    RegisterClassExW(&splashClass);

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int availableWidth = std::max<LONG>(640, workArea.right - workArea.left);
    int availableHeight = std::max<LONG>(420, workArea.bottom - workArea.top);
    int width = std::min(1320, availableWidth - 64);
    int height = std::min(600, availableHeight - 64);
    int x = workArea.left + (availableWidth - width) / 2;
    int y = workArea.top + (availableHeight - height) / 2;
    HWND splash = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        splashClass.lpszClassName,
        L"Storyland",
        WS_POPUP,
        x, y, width, height,
        nullptr, nullptr, instance, nullptr
    );
    if (!splash) return nullptr;

    SetWindowRgn(splash, CreateRoundRectRgn(0, 0, width + 1, height + 1, 26, 26), TRUE);
    SetLayeredWindowAttributes(splash, 0, 0, LWA_ALPHA);
    ShowWindow(splash, SW_SHOW);
    SetForegroundWindow(splash);
    SetFocus(splash);
    UpdateWindow(splash);
    for (BYTE alpha = 0; alpha < 238; alpha = BYTE(alpha + 17)) {
        SetLayeredWindowAttributes(splash, 0, alpha, LWA_ALPHA);
        pumpSplashMessages(splash);
        Sleep(8);
    }
    SetLayeredWindowAttributes(splash, 0, 255, LWA_ALPHA);
    startedAt = GetTickCount64();
    return splash;
}

static void finishStorylandSplash(HWND splash, ULONGLONG startedAt) {
    if (!splash) return;
    while (!gSplashSkipRequested && GetTickCount64() - startedAt < 1900u) {
        pumpSplashMessages(splash);
        Sleep(10);
    }
    for (int alpha = 255; alpha >= 0; alpha -= 17) {
        SetLayeredWindowAttributes(splash, 0, BYTE(alpha), LWA_ALPHA);
        pumpSplashMessages(splash);
        Sleep(8);
    }
    DestroyWindow(splash);
}

static void loadEmbeddedVcsTimecycle() {
    HRSRC resource = FindResourceW(
        gInstance,
        MAKEINTRESOURCEW(IDR_VCS_TIMECYC),
        MAKEINTRESOURCEW(10) // RT_RCDATA, explicitly wide for FindResourceW
    );
    if (!resource) {
        gStoriesSkyDataStatus = "Embedded VCS timecycle resource was not found; using fallback colours.";
        return;
    }
    HGLOBAL loadedResource = LoadResource(gInstance, resource);
    DWORD resourceSize = SizeofResource(gInstance, resource);
    const char* resourceBytes = loadedResource ?
        static_cast<const char*>(LockResource(loadedResource)) : nullptr;
    if (!resourceBytes || resourceSize == 0) {
        gStoriesSkyDataStatus = "Embedded VCS timecycle resource is empty; using fallback colours.";
        return;
    }

    std::string error;
    if (!gStoriesSky.loadVcsTimecycleText(std::string(resourceBytes, resourceSize), error)) {
        gStoriesSkyDataStatus = error;
        return;
    }
    gStoriesSkyDataStatus = "Embedded VCS timecycle loaded.";
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR commandLine, int showCommand) {
    gInstance = hInstance;
    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES };
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    loadRecentFiles();

    ULONGLONG splashStartedAt = 0;
    HWND splash = createStorylandSplash(hInstance, splashStartedAt);
    loadEmbeddedVcsTimecycle();
    // Establish the default Dark menu theme before Windows creates the menu bar.
    applyStorylandNativeMenuTheme(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = mainProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"StorylandMainWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = loadStorylandIcon(hInstance, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    wc.hIconSm = loadStorylandIcon(hInstance, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);

    gMainWindow = CreateWindowExW(0, wc.lpszClassName, L"Storyland", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1180, 760, nullptr, nullptr, hInstance, nullptr);
    if (!gMainWindow) return 1;

    HICON storylandLargeIcon = loadStorylandIcon(hInstance, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    HICON storylandSmallIcon = loadStorylandIcon(hInstance, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    if (storylandLargeIcon) SendMessageW(gMainWindow, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(storylandLargeIcon));
    if (storylandSmallIcon) SendMessageW(gMainWindow, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(storylandSmallIcon));

    finishStorylandSplash(splash, splashStartedAt);
    ShowWindow(gMainWindow, showCommand);
    UpdateWindow(gMainWindow);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1 && wcslen(argv[1]) > 0 && GetFileAttributesW(argv[1]) != INVALID_FILE_ATTRIBUTES) {
        openStorylandFile(argv[1]);
    }
    if (argv) LocalFree(argv);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && handleModelViewportShortcut(msg.wParam)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return int(msg.wParam);
}
