#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <mmsystem.h>
#include <wincodec.h>
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <cwchar>
#include <filesystem>
#include <exception>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "leeds_texture.h"
#include "storyland_dtz.h"
#include "storyland_model.h"
#include "storyland_analysis_graph.h"
#include "storyland_opengl_renderer.h"
#include "storyland_shaders.h"
#include "storyland_wbl.h"
#include "storyland_archive.h"
#include "storyland_atomic_io.h"
#include "storyland_dma_validator.h"
#include "storyland_psp_validator.h"
#include "storyland_model_validator.h"
#include "storyland_media.h"
#include "storyland_anim.h"
#include "storyland_sky.h"
#include "storyland_scm.h"
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

#pragma comment(lib, "Winmm.lib")

static bool gStorylandVcsThemeContext = false;

static void stopStorylandTheme() {
    // PlaySound uses the process-wide waveform channel. Passing nullptr stops
    // the currently playing asynchronous resource immediately.
    PlaySoundW(nullptr, nullptr, 0);
}

static bool playStorylandTheme(bool /*introSting*/, bool vcsTheme = false) {
    HMODULE module = GetModuleHandleW(nullptr);
    if (!module) return false;

    const int resourceId = vcsTheme ? IDR_VCS_ERROR_WARNING_THEME : IDR_REIGNS_ERROR_THEME;

    // Both themes are embedded as PCM WAVE resources. PlaySound can stream them
    // directly from the executable, so warning/error playback does not depend on
    // a temporary file, Media Player registration, or an MPEG codec.
    return PlaySoundW(
        MAKEINTRESOURCEW(resourceId),
        module,
        SND_RESOURCE | SND_ASYNC | SND_NODEFAULT | (vcsTheme ? SND_LOOP : 0)
    ) != FALSE;
}

static int storylandMessageBoxW(HWND owner, LPCWSTR text, LPCWSTR caption, UINT type) {
    const bool isError = (type & MB_ICONERROR) == MB_ICONERROR;
    const bool isWarning = (type & MB_ICONWARNING) == MB_ICONWARNING;

    if (gStorylandVcsThemeContext && (isError || isWarning)) {
        // VCS warnings and errors use the VCS-specific user-supplied 8-bit theme.
        // Restarting it here also covers catastrophic errors while a VCS file is
        // the active context.
        playStorylandTheme(false, true);
    } else if (isError) {
        // Preserve the existing Reigns error theme for non-VCS errors.
        playStorylandTheme(false, false);
    }

    const int result = ::MessageBoxW(owner, text, caption, type);
    if (isError || isWarning) stopStorylandTheme();
    return result;
}

#define MessageBoxW storylandMessageBoxW

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

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
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
#define ID_DTZ_REBUILD_AS 1002
#define ID_FILE_EXPORT_TEXTURE 1003
#define ID_FILE_REPLACE_TEXTURE 1004
#define ID_FILE_RENAME_TEXTURE 1012
#define ID_DTZ_PATCH_SELECTED 1005
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
#define ID_VIEW_PRELIGHT_COMBINED 1110
#define ID_VIEW_PRELIGHT_RAW 1111
#define ID_VIEW_PRELIGHT_OFF 1112
#define ID_MODEL_EDIT_PRELIGHT 1113
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
#define ID_VIEW_BACKGROUND_LIGHT 1053
#define ID_VIEW_BACKGROUND_DARK 1054
#define ID_FILE_EXIT 1055
#define ID_FILE_RECENT_CLEAR 1056
#define ID_HELP_WIKI 1057
#define ID_FILE_EXPORT_CURRENT 1066
#define ID_FILE_EXPORT_AS 1067
#define ID_DTZ_FIND 1068
#define ID_DTZ_RENAME_RESOURCE 1069
#define ID_FILE_NEW 1070
#define ID_FILE_NEW_MODEL 1079
#define ID_FILE_NEW_TEXTURE 1080
#define ID_TEXTURE_ADD 1071
#define ID_TEXTURE_SWAP 1072
#define ID_MODEL_IMPORT_DATA 1073
#define ID_TEXTURE_DUPLICATE 1074
#define ID_TEXTURE_REMOVE 1075
#define ID_TEXTURE_VALIDATE 1076
#define ID_VIEW_AUTO_MODEL_TEXTURE_V 1077
#define ID_RESOURCE_ANALYZE 1078
#define ID_RESOURCE_ANALYZE_GRAPH 1081
#define ID_RESOURCE_ANALYZE_DATA 1082
#define ID_RESOURCE_ANALYZE_BACK 1083
#define ID_FILE_NEW_TEXTURE_XTX 1084
#define ID_FILE_NEW_TEXTURE_CHK 1085
#define ID_FILE_NEW_TEXTURE_TXD 1086
#define ID_FILE_NEW_MODEL_PS2_SIMPLE 1087
#define ID_FILE_NEW_MODEL_PS2_PED 1088
#define ID_FILE_NEW_MODEL_PS2_CUTSCENE 1089
#define ID_FILE_NEW_MODEL_PS2_VEHICLE 1090
#define ID_FILE_NEW_MODEL_PS2_WORLD 1091
#define ID_FILE_NEW_MODEL_PSP_SIMPLE 1092
#define ID_FILE_NEW_MODEL_PSP_PED 1093
#define ID_FILE_NEW_MODEL_PSP_CUTSCENE 1094
#define ID_FILE_NEW_MODEL_PSP_VEHICLE 1095
#define ID_FILE_NEW_MODEL_PSP_WORLD 1096
#define ID_FILE_NEW_TEXTURE_PS2_XTX 1097
#define ID_FILE_NEW_TEXTURE_PS2_CHK 1098
#define ID_FILE_NEW_TEXTURE_PSP_TXD 1099
#define ID_FILE_NEW_MODEL_PSP_DFF_SIMPLE 1100
#define ID_FILE_NEW_MODEL_PSP_DFF_PED 1101
#define ID_FILE_NEW_MODEL_PSP_DFF_CUTSCENE 1102
#define ID_FILE_NEW_MODEL_PSP_DFF_VEHICLE 1103
#define ID_FILE_NEW_MODEL_PSP_DFF_WORLD 1104
#define ID_FILE_NEW_TEXTURE_PSP_XTX 1105
#define ID_FILE_NEW_TEXTURE_PSP_CHK 1106
#define ID_FILE_RELOAD 1114
#define ID_VIEW_SCRIPTING_CONSOLE 1107
#define ID_ARCHIVE_TEST_LVZ_IMG_PAIR 1108
#define ID_ARCHIVE_ADD_RESOURCE 1115
#define ID_MEDIA_REPLACE_CURRENT_FRAME 1109
#define ID_SCRIPT_NEW 2101
#define ID_SCRIPT_OPEN 2102
#define ID_SCRIPT_SAVE 2103
#define ID_SCRIPT_RUN 2104
#define ID_SCRIPT_EDITOR 2105
#define ID_SCRIPT_OUTPUT 2106
#define ID_VIEW_SHOW_2DFX_LIGHTS 1059
#define ID_SCM_CONFIGURE_SANNY 1060
#define ID_SCM_DECOMPILE_PS2 1061
#define ID_SCM_DECOMPILE_PSP 1062
#define ID_SCM_EXPORT_SOURCE 1063
#define ID_SCM_COMPILE 1064
#define ID_SCM_REFRESH_MISSIONS 1065
#define ID_FILE_RECENT_BASE 3000
#define ID_TREE 2001
#define ID_DETAILS 2002
#define ID_PREVIEW 2003
#define ID_STATUS 2004
#define ID_MENU_STRIP 2005
#define ID_ACTION_BAR 2006
#define ID_ACTION_PRIMARY 2007
#define ID_ACTION_SECONDARY 2008
#define ID_ACTION_TERTIARY 2009
#define ID_ACTION_QUATERNARY 2010
#define ID_MEDIA_TIMELINE 2011

static HINSTANCE gInstance = nullptr;
static HWND gMainWindow = nullptr;
static HWND gTree = nullptr;
static HWND gDetails = nullptr;
static HWND gPreview = nullptr;
static HWND gStatus = nullptr;
static HWND gMenuStrip = nullptr;
static HWND gActionBar = nullptr;
static HWND gActionPrimary = nullptr;
static HWND gActionSecondary = nullptr;
static HWND gActionTertiary = nullptr;
static HWND gActionQuaternary = nullptr;
static HWND gMediaTimeline = nullptr;
static bool gMediaTimelineInternalUpdate = false;
static HWND gRenderPieWindow = nullptr;
static HMENU gMainMenu = nullptr;
static HMENU gFileMenu = nullptr;
static HMENU gViewMenu = nullptr;
static HMENU gSkyMenu = nullptr;
static HMENU gHelpMenu = nullptr;
static HMENU gTexturePreviewMenu = nullptr;
static HMENU gRecentMenu = nullptr;
static HMENU gOpenGlMenu = nullptr;
static HMENU gBackgroundMenu = nullptr;
static HMENU gScmMenu = nullptr;
static HMENU gNewMenu = nullptr;
static bool gEyeFriendlyPaneBackground = true;
static HBRUSH gPaneBackgroundBrush = nullptr;
static HBRUSH gMenuBackgroundBrush = nullptr;
static HFONT gUiFont = nullptr;
static std::wstring gStatusText;
static std::vector<std::wstring> gRecentFiles;

static COLORREF storylandPaneBackgroundColor() {
    return gEyeFriendlyPaneBackground ? RGB(35, 36, 42) : RGB(248, 252, 255);
}

static COLORREF storylandFrameBackgroundColor() {
    return gEyeFriendlyPaneBackground ? RGB(14, 15, 19) : RGB(213, 229, 243);
}

static COLORREF storylandPaneTextColor() {
    return gEyeFriendlyPaneBackground ? RGB(232, 234, 239) : RGB(33, 54, 78);
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
        const HWND themedWindows[] = {
            mainWindow, gTree, gDetails, gStatus,
            gActionPrimary, gActionSecondary, gActionTertiary, gActionQuaternary
        };
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
    // The themed WS_EX_CLIENTEDGE bevel is always pale and ignores the active
    // LCS/VCS/CTW tint. Storyland draws the pane frame itself in the client area.
    setStorylandClientEdge(gTree, false);
    setStorylandClientEdge(gPreview, false);
    setStorylandClientEdge(gDetails, false);
    if (gTree) {
        TreeView_SetBkColor(gTree, paneColor);
        TreeView_SetTextColor(gTree, storylandPaneTextColor());
        TreeView_SetLineColor(gTree, gEyeFriendlyPaneBackground ? RGB(70, 73, 83) : RGB(141, 169, 195));
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

enum class StorylandTitleTint { Default, LCS, VCS, ViceCity, SanAndreas, CTW, BloodRed };
static StorylandTitleTint gTitleTint = StorylandTitleTint::Default;

static LeedsTextureArchive gTextureArchive;
static StorylandDtzArchive gDtzArchive;
static StorylandModelFile gModelFile;
static StorylandOpenGLRenderer gOpenGlRenderer;
static StorylandWblFile gWblFile;
static StorylandArchiveBrowser gArchiveBrowser;
struct StorylandAreaArchive {
    std::wstring name;
    std::unique_ptr<StorylandArchiveBrowser> browser;
    std::wstring loadError;
};
static std::array<StorylandAreaArchive, 3> gAreaArchives;
static int gActiveAreaArchive = -1;
static StorylandAnimFile gAnimFile;
static StorylandScmFile gScmFile;
static StorylandMediaFile gMediaFile;
static std::wstring gSannyBuilderPath;
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
static bool gModelTextureVAuto = false;
static bool gModelFlipTextureV = false;
static bool gModelDetectedFlipTextureV = false;
static std::wstring gModelTextureVDetectionReason;

enum class StorylandOpenGlRenderMode { Stories, Textured, Solid, Wireframe };
static StorylandOpenGlRenderMode gOpenGlRenderMode = StorylandOpenGlRenderMode::Stories;
static bool gOpenGlShowGrid = false;
static bool gOpenGlShowBones = false;
static bool gOpenGlShowBounds = false;
static bool gOpenGlShowViewCube = true;
static bool gOpenGlQuadView = false;

enum StorylandRenderPieCommand : UINT {
    ID_RENDER_PIE_STORIES = 50001,
    ID_RENDER_PIE_SOLID,
    ID_RENDER_PIE_TEXTURED,
    ID_RENDER_PIE_WIREFRAME
};
static bool gOpenGlShow2dfxLights = true;
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

static StorylandQuat storylandQuatFromAnimXyzw(float qx, float qy, float qz, float qw) {
    // Leeds ANIM stores quaternion components as X,Y,Z,W.
    // StorylandQuat stores W,X,Y,Z.
    return {qw, qx, qy, qz};
}

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
static GLuint gArchiveTexturePreviewId = 0;
static int gArchiveTexturePreviewIndex = -1;
static uint32_t gArchiveTexturePreviewHeaderOffset = 0;
static int gArchiveTexturePreviewWidth = 0;
static int gArchiveTexturePreviewHeight = 0;
static bool gModelLeftDrag = false;
static bool gModelRightDrag = false;
static POINT gModelLastMouse = {};
static StorylandQuat gModelViewRotation = {};
static StorylandQuat gModelDragStartRotation = {};
static StorylandVec3 gModelDragStartPoint = {};
static float gModelDistance = 3.5f;
static float gModelPanX = 0.0f;
static float gModelPanY = 0.0f;
enum class StorylandPrelightViewMode { Combined, Raw, Off };
static StorylandPrelightViewMode gPrelightViewMode = StorylandPrelightViewMode::Combined;
static int gSelectedPrelightVertex = -1;

static enum class StorylandMode { Empty, TextureArchive, DtzArchive, ModelFile, WblFile, ArchiveFile, AnimFile, ScmFile, MediaFile } gMode = StorylandMode::Empty;
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
    DtzLeeds2dfx,
    DtzLeeds2dfxWorld,
    DtzFindResult,
    DtzArea,
    DtzAreaReturn,
    ModelField,
    ModelBone,
    ModelPrelight,
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
    AnimString,
    ScmOverview,
    ScmSource,
    ScmMission,
    MediaOverview,
    MediaClip,
    AnalyzeGraphNode,
    AnalyzeGraphBack,
    ModelDffGraphNode
};

static StorylandTreeKind gSelectedKind = StorylandTreeKind::None;

static StorylandAnalysisGraph gAnalysisGraph;
static std::vector<uint8_t> gAnalysisGraphBytes;
static std::wstring gAnalysisGraphName;
static bool gAnalyzeGraphActive = false;
static StorylandAnalysisGraph gModelDffStructureGraph;
static std::vector<uint8_t> gModelDffStructureBytes;
static std::wstring gModelDffStructureName;
static bool gModelDffStructureTreeActive = false;

enum class DtzEmbeddedPreviewKind {
    None,
    TextureArchive,
    ModelFile
};

static DtzEmbeddedPreviewKind gDtzEmbeddedPreviewKind = DtzEmbeddedPreviewKind::None;
static int gDtzEmbeddedPreviewIndex = -1;
static std::wstring gDtzEmbeddedPreviewPath;

struct StorylandDtzFindResult {
    StorylandTreeKind targetKind = StorylandTreeKind::None;
    int targetIndex = -1;
    int dirEntryIndex = -1;
    int textureIndex = -1;
    bool deepTextureName = false;
    std::wstring label;
    std::wstring context;
};

struct StorylandDtzIndexedTextureName {
    int dirEntryIndex = -1;
    int textureIndex = -1;
    std::wstring textureName;
};

static std::wstring gDtzFindQuery;
static std::vector<StorylandDtzFindResult> gDtzFindResults;
static std::vector<StorylandDtzIndexedTextureName> gDtzTextureNameIndex;
static bool gDtzTextureNameIndexReady = false;
static size_t gDtzTextureArchivesScanned = 0;
static size_t gDtzTextureArchivesRejected = 0;
static bool gOpeningDtzStandaloneChild = false;
static bool gDtzReturnAvailable = false;
static int gDtzReturnSelectedIndex = -1;
static StorylandTreeKind gDtzReturnTreeKind = StorylandTreeKind::None;
static int gDtzReturnTreeIndex = -1;
static StorylandTitleTint gDtzReturnTint = StorylandTitleTint::Default;

struct StorylandTreePayload {
    StorylandTreeKind kind = StorylandTreeKind::None;
    int index = -1;
};

static std::vector<StorylandTreePayload> gTreePayloads;
static int gTreeMutationDepth = 0;

static std::vector<uint32_t> gArchiveMeshResourceIds;
static std::vector<uint32_t> gArchiveTextureIds;
static std::vector<int> gArchiveAnimationEntryIndices;

struct ArchiveViewportPick {
    int x = 0;
    int y = 0;
    int radius = 10;
    float depth = 1.0f;
    uint32_t resourceId = 0;
};

static std::vector<ArchiveViewportPick> gArchiveViewportPicks;
static bool gModelDragMoved = false;
static POINT gModelMouseDownPoint = {};
static bool gArchiveViewFocusActive = false;
static float gArchiveViewFocusX = 0.0f;
static float gArchiveViewFocusY = 0.0f;
static float gArchiveViewFocusZ = 0.0f;
static float gArchiveViewFocusSpan = 1.0f;

static HWND gScriptWindow = nullptr;
static HWND gScriptToolbar = nullptr;
static HWND gScriptEditor = nullptr;
static HWND gScriptOutput = nullptr;
static HFONT gScriptFont = nullptr;
static HBRUSH gScriptBackgroundBrush = nullptr;
static HBRUSH gScriptToolbarBrush = nullptr;

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


static std::wstring widenResourceName(const std::string& text);

static std::wstring selectedTreeDisplayText() {
    // Model hierarchy rows describe bones, fields and diagnostics. They must never
    // replace the file/resource name shown in the renderer overlay.
    if (gMode == StorylandMode::ModelFile && !gModelFile.sourcePath().empty()) {
        const std::wstring fileName = std::filesystem::path(gModelFile.sourcePath()).filename().wstring();
        if (!fileName.empty()) return fileName;
    }
    if (gMode == StorylandMode::TextureArchive && !gTextureArchive.sourcePath().empty()) {
        const std::wstring fileName = std::filesystem::path(gTextureArchive.sourcePath()).filename().wstring();
        if (!fileName.empty()) return fileName;
    }
    if (gDtzEmbeddedPreviewKind != DtzEmbeddedPreviewKind::None && gDtzEmbeddedPreviewIndex >= 0) {
        const auto& entries = gDtzArchive.dirEntries();
        const size_t previewIndex = size_t(gDtzEmbeddedPreviewIndex);
        if (previewIndex < entries.size()) return widenResourceName(entries[previewIndex].name);
        if (!gDtzEmbeddedPreviewPath.empty()) {
            const std::wstring fileName = std::filesystem::path(gDtzEmbeddedPreviewPath).filename().wstring();
            if (!fileName.empty()) return fileName;
        }
    }

    if (!gTree) return {};
    HTREEITEM selected = TreeView_GetSelection(gTree);
    if (!selected) return {};

    wchar_t buffer[512] = {};
    TVITEMW item = {};
    item.mask = TVIF_TEXT | TVIF_PARAM;
    item.hItem = selected;
    item.pszText = buffer;
    item.cchTextMax = int(std::size(buffer));
    if (!TreeView_GetItem(gTree, &item)) return {};

    const StorylandTreePayload payload = payloadFromLParam(item.lParam);

    // The viewport label must use the authoritative resource name rather than
    // whatever descriptive text the tree row happens to contain.  Tree rows may
    // include offsets, ids and binary-derived annotations that are useful in the
    // browser but are not valid file names.
    if (payload.index >= 0) {
        const size_t index = size_t(payload.index);
        if (payload.kind == StorylandTreeKind::DtzDirEntry) {
            const auto& entries = gDtzArchive.dirEntries();
            if (index < entries.size()) {
                return widenResourceName(entries[index].name);
            }
        }
        if (payload.kind == StorylandTreeKind::ArchiveEntry) {
            const auto& entries = gArchiveBrowser.entries();
            if (index < entries.size()) {
                return widenResourceName(entries[index].name);
            }
        }
    }

    return buffer;
}

static void drawSelectedViewportLabel(HDC dc, const RECT& rc) {
    std::wstring text = selectedTreeDisplayText();
    if (text.empty()) return;
    RECT box{rc.left + 10, rc.top + 10, rc.right - 10, rc.top + 38};
    HBRUSH background = CreateSolidBrush(RGB(24, 26, 31));
    FillRect(dc, &box, background);
    DeleteObject(background);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(238, 240, 244));
    RECT textRect{box.left + 8, box.top + 5, box.right - 8, box.bottom - 4};
    DrawTextW(dc, text.c_str(), -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
}

static HTREEITEM findTreePayloadItemRecursive(HTREEITEM item, StorylandTreeKind kind, int index) {
    while (item) {
        TVITEMW treeItem = {};
        treeItem.mask = TVIF_PARAM;
        treeItem.hItem = item;
        if (TreeView_GetItem(gTree, &treeItem)) {
            StorylandTreePayload payload = payloadFromLParam(treeItem.lParam);
            if (payload.kind == kind && payload.index == index) return item;
        }
        HTREEITEM child = TreeView_GetChild(gTree, item);
        if (child) {
            HTREEITEM found = findTreePayloadItemRecursive(child, kind, index);
            if (found) return found;
        }
        item = TreeView_GetNextSibling(gTree, item);
    }
    return nullptr;
}

static bool selectTreePayloadItem(StorylandTreeKind kind, int index) {
    if (!gTree) return false;
    HTREEITEM root = TreeView_GetRoot(gTree);
    HTREEITEM item = findTreePayloadItemRecursive(root, kind, index);
    if (!item) return false;
    TreeView_SelectItem(gTree, item);
    TreeView_EnsureVisible(gTree, item);
    return true;
}

static std::wstring widen(const std::string& text) {
    if (text.empty()) return L"";

    auto convert = [&](UINT codePage, DWORD flags) -> std::wstring {
        const int size = MultiByteToWideChar(codePage, flags, text.data(), int(text.size()), nullptr, 0);
        if (size <= 0) return {};
        std::wstring result(size_t(size), L'\0');
        if (MultiByteToWideChar(codePage, flags, text.data(), int(text.size()), result.data(), size) != size) return {};
        return result;
    };

    std::wstring result = convert(CP_UTF8, MB_ERR_INVALID_CHARS);
    if (result.empty()) result = convert(1252, 0);
    if (result.empty()) {
        result.reserve(text.size());
        for (unsigned char ch : text) result.push_back(ch >= 0x20 && ch < 0x7f ? wchar_t(ch) : L'?');
    }

    for (wchar_t& ch : result) {
        if (ch < 0x20 && ch != L'\t') ch = L' ';
        if (ch >= 0xD800 && ch <= 0xDFFF) ch = L'?';
    }
    return result;
}

static std::wstring widenResourceName(const std::string& text) {
    if (text.empty()) return L"";
    std::wstring result;
    result.reserve(text.size());
    for (unsigned char ch : text) {
        if (ch == 0) break;
        if (ch >= 0x20 && ch < 0x7f) result.push_back(wchar_t(ch));
        else result.push_back(L'?');
    }
    while (!result.empty() && (result.back() == L' ' || result.back() == L'?')) result.pop_back();
    return result.empty() ? L"unnamed" : result;
}

static std::wstring archiveExtensionForIdent(uint32_t ident) {
    switch (ident) {
    case 0x006D646Cu: return L".mdl";   // ldm\0
    case 0x00746578u: return L".xtx";   // tex\0
    case 0x57524C44u: return L".wrld";  // DLRW
    case 0x41455241u:                   // AREA
    case 0x41524541u: return L".area";  // AERA
    case 0x47544147u: return L".dtz";   // GTAG
    case 0x6D696E61u: return L".anim";  // anim
    case 0x636F6C32u: return L".col";   // col2
    default: return L".bin";
    }
}

static std::wstring archiveDisplayName(const StorylandArchiveEntry& entry, size_t entryIndex) {
    std::wstring display = widenResourceName(entry.name);
    size_t useful = 0;
    size_t question = 0;
    for (wchar_t ch : display) {
        if (ch == L'?') ++question;
        else if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
                 (ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'-' || ch == L'.') ++useful;
    }
    const bool bad = display.empty() || display == L"unnamed" || useful < 2 ||
        (question >= 3 && question * 2 >= display.size());
    if (!bad) return display;

    std::wstring ext = archiveExtensionForIdent(entry.chunkIdent);
    if (ext.empty()) ext = L".bin";
    return L"resource" + std::to_wstring(entryIndex) + ext;
}

static std::string narrow(const std::wstring& text) {
    if (text.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return "";
    std::string result(size_t(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), size, nullptr, nullptr) <= 0) return "";
    result.resize(size_t(size - 1));
    return result;
}

static std::wstring getExtensionLower(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"";
    std::wstring ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return wchar_t(towlower(c)); });
    return ext;
}

static bool isStorylandAudioExtension(const std::wstring& ext) {
    return ext == L".sdt" || ext == L".raw" || ext == L".vag" || ext == L".vb" ||
           ext == L".wav" || ext == L".at3" || ext == L".aa3" || ext == L".oma" ||
           ext == L".mp3" || ext == L".ogg" || ext == L".flac" || ext == L".aac" ||
           ext == L".m4a" || ext == L".wma" || ext == L".ac3" || ext == L".aif" ||
           ext == L".aiff" || ext == L".adx";
}

static bool isStorylandVideoExtension(const std::wstring& ext) {
    return ext == L".pss" || ext == L".pmf" || ext == L".mpg" || ext == L".mpeg" || ext == L".mp4" ||
           ext == L".m4v" || ext == L".wmv" || ext == L".avi" || ext == L".mov" || ext == L".mkv" ||
           ext == L".ts" || ext == L".m2ts" || ext == L".mts" || ext == L".vob" ||
           ext == L".3gp" || ext == L".3g2" || ext == L".webm" || ext == L".ogv" || ext == L".flv";
}

static bool isStorylandMediaExtension(const std::wstring& ext) {
    return isStorylandAudioExtension(ext) || isStorylandVideoExtension(ext);
}

static std::wstring canonicalDtzImgResourceName(const std::wstring& displayName) {
    std::wstring lower = displayName;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return wchar_t(towlower(c)); });

    const wchar_t* knownExtensions[] = {
        L".mdl", L".dff", L".wbl", L".xtx", L".chk", L".tex", L".txd", L".anim", L".col", L".col2", L".dat", L".ipl", L".ide", L".cut", L".dir", L".bin", L".dtz", L".zmg"
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

static bool isReservedWindowsBaseName(const std::wstring& fileName) {
    std::wstring base = fileName;
    size_t dot = base.find(L'.');
    if (dot != std::wstring::npos) base.resize(dot);
    while (!base.empty() && (base.back() == L' ' || base.back() == L'.')) base.pop_back();
    std::transform(base.begin(), base.end(), base.begin(), [](wchar_t c) { return wchar_t(towupper(c)); });
    if (base == L"CON" || base == L"PRN" || base == L"AUX" || base == L"NUL") return true;
    if (base.size() == 4u && (base.rfind(L"COM", 0u) == 0u || base.rfind(L"LPT", 0u) == 0u) &&
        base[3] >= L'1' && base[3] <= L'9') return true;
    return false;
}

static std::wstring safeEmbeddedFileName(const std::wstring& rawName, const std::wstring& fallbackName) {
    size_t slash = rawName.find_last_of(L"\\/");
    std::wstring name = slash == std::wstring::npos ? rawName : rawName.substr(slash + 1u);
    if (name.empty()) name = fallbackName;

    for (wchar_t& ch : name) {
        const bool invalid = ch < 0x20 || ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
                             ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*';
        if (invalid) ch = L'_';
    }
    while (!name.empty() && (name.back() == L' ' || name.back() == L'.')) name.pop_back();
    while (!name.empty() && (name.front() == L' ' || name.front() == L'.')) name.erase(name.begin());
    if (name.empty() || name == L"." || name == L"..") name = fallbackName;
    if (isReservedWindowsBaseName(name)) name.insert(name.begin(), L'_');

    constexpr size_t maximumNameLength = 180u;
    if (name.size() > maximumNameLength) {
        std::wstring extension = getExtensionLower(name);
        if (extension.size() > 16u) extension.clear();
        const size_t keepStem = maximumNameLength > extension.size() ? maximumNameLength - extension.size() : maximumNameLength;
        name = name.substr(0u, keepStem) + extension;
    }
    return name;
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

    if (ext == L".chk" || ext == L".txd") return StorylandTitleTint::LCS;
    if (ext == L".xtx" || ext == L".scm") return StorylandTitleTint::VCS;
    if (ext == L".wbl" || ext == L".tex") return StorylandTitleTint::CTW;

    std::wstring lower = lowerWide(path);
    if (lower.find(L"chinatown") != std::wstring::npos || lower.find(L"ctw") != std::wstring::npos) return StorylandTitleTint::CTW;
    if (lower.find(L"vice city stories") != std::wstring::npos || lower.find(L"vcs") != std::wstring::npos) return StorylandTitleTint::VCS;
    if (lower.find(L"vice city") != std::wstring::npos || lower.find(L"gtavc") != std::wstring::npos) return StorylandTitleTint::ViceCity;
    if (lower.find(L"san andreas") != std::wstring::npos ||
        lower.find(L"gta sa") != std::wstring::npos ||
        lower.find(L"gtasa") != std::wstring::npos) return StorylandTitleTint::SanAndreas;
    if (lower.find(L"liberty city stories") != std::wstring::npos || lower.find(L"lcs") != std::wstring::npos) return StorylandTitleTint::LCS;

    return StorylandTitleTint::Default;
}

static COLORREF storylandPaneBorderColor() {
    switch (gTitleTint) {
    case StorylandTitleTint::LCS: return RGB(181, 221, 242);
    case StorylandTitleTint::VCS: return RGB(238, 48, 145);
    case StorylandTitleTint::ViceCity: return RGB(255, 188, 218);
    case StorylandTitleTint::SanAndreas: return RGB(255, 132, 36);
    case StorylandTitleTint::CTW: return RGB(255, 56, 68);
    case StorylandTitleTint::BloodRed: return RGB(116, 0, 10);
    default: return gEyeFriendlyPaneBackground ? RGB(74, 78, 90) : RGB(126, 166, 204);
    }
}

static COLORREF storylandBlendUiColor(COLORREF base, COLORREF accent, int accentPercent) {
    accentPercent = std::clamp(accentPercent, 0, 100);
    const int basePercent = 100 - accentPercent;
    return RGB(
        (GetRValue(base) * basePercent + GetRValue(accent) * accentPercent) / 100,
        (GetGValue(base) * basePercent + GetGValue(accent) * accentPercent) / 100,
        (GetBValue(base) * basePercent + GetBValue(accent) * accentPercent) / 100);
}

static void applyStorylandTitleTint(StorylandTitleTint tint) {
    gTitleTint = tint;
    gStorylandVcsThemeContext = (tint == StorylandTitleTint::VCS);
    if (!gMainWindow) return;

    DWORD captionColor = DWMWA_COLOR_DEFAULT;
    DWORD borderColor = DWMWA_COLOR_DEFAULT;
    DWORD textColor = DWMWA_COLOR_DEFAULT;

    BOOL immersiveDarkMode = gEyeFriendlyPaneBackground ? TRUE : FALSE;
    switch (tint) {
    case StorylandTitleTint::LCS:
        captionColor = RGB(181, 221, 242);
        borderColor = RGB(181, 221, 242);
        textColor = RGB(18, 34, 50);
        immersiveDarkMode = FALSE;
        break;
    case StorylandTitleTint::VCS:
        // Vice City Stories: hot Rockstar Leeds pink rather than the pale
        // Vice City pastel used by the older game.
        captionColor = RGB(238, 48, 145);
        borderColor = RGB(238, 48, 145);
        textColor = RGB(255, 250, 253);
        immersiveDarkMode = FALSE;
        break;
    case StorylandTitleTint::ViceCity:
        captionColor = RGB(255, 188, 218);
        borderColor = RGB(255, 188, 218);
        textColor = RGB(54, 24, 42);
        immersiveDarkMode = FALSE;
        break;
    case StorylandTitleTint::SanAndreas:
        captionColor = RGB(255, 132, 36);
        borderColor = RGB(255, 132, 36);
        textColor = RGB(33, 20, 8);
        immersiveDarkMode = FALSE;
        break;
    case StorylandTitleTint::CTW:
        captionColor = RGB(218, 34, 46);
        borderColor = RGB(255, 56, 68);
        textColor = RGB(255, 248, 240);
        break;
    case StorylandTitleTint::BloodRed:
        captionColor = RGB(116, 0, 10);
        borderColor = RGB(156, 18, 27);
        textColor = RGB(255, 242, 242);
        break;
    default:
        break;
    }

    DwmSetWindowAttribute(gMainWindow, DWMWA_USE_IMMERSIVE_DARK_MODE, &immersiveDarkMode, sizeof(immersiveDarkMode));
    DwmSetWindowAttribute(gMainWindow, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
    DwmSetWindowAttribute(gMainWindow, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
    DwmSetWindowAttribute(gMainWindow, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));

    SetWindowPos(gMainWindow, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    RedrawWindow(gMainWindow, nullptr, nullptr,
        RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
    for (HWND pane : {gTree, gPreview, gDetails, gActionBar}) {
        if (pane) {
            RedrawWindow(
                pane,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_FRAME);
        }
    }
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
static bool writeUtf8TextFile(const std::wstring& path, const std::wstring& text, std::string& error);
static std::wstring getDetailsText();
static void populateScmList();
static void selectScmPayload(const StorylandTreePayload& payload);
static bool decompileCurrentScm(const std::string& modeId, bool showFailureDialog);
static void compileCurrentScm();
static void exportCurrentScmSource();
static void configureSannyBuilder();
static void refreshScmMissionTree();
static std::wstring buildDtzPreviewExtractRoot();
static void clearDtzEmbeddedPreviewState();
static bool currentModeUsesInteractiveModelViewport();
static void drawOpenGlViewCube(HWND hwnd, HDC dc, int width, int height);
static void showRenderModePie(HWND owner);
static void drawWblPreviewOpenGl(HWND hwnd, HDC dc, RECT rc);
static void applyModelViewportZoom(float scale);
static void fitModelViewportCloser();
static bool handleModelViewportShortcut(WPARAM key);
static bool canViewSelectedInRenderer();
static bool viewSelectedInRenderer();
static void showRendererContextMenu(HWND hwnd, int clientX, int clientY);
static std::string archiveEntryExtensionLower(const std::string& name);
static bool prepareDtzDirEntryPreview(int index, std::wstring& previewSummary);
static void openDtzDirEntryStandaloneByIndex(int entryIndex, int preferredTextureIndex);
static void openSelectedDtzDirEntryStandalone();
static void performDtzFind();
static void returnToGameDtz();
static const wchar_t* leeds2dfxEffectTypeName(uint8_t type);
static void rebuildFileMenu();
static void rebuildViewMenu();
static void updateModelTextureVAutoDetection();
static bool effectiveModelTextureVFlip();
static void updateActionBar();
static void createNewStorylandResource();
static void addTextureMaterial();
static void applyAnimationToCurrentModel();
static void editSelectedTextureMaterial();
static int chooseTextureBpp(UINT initialBpp);
static bool clampEditableStoriesTexturesTo8Bpp(std::wstring& summaryOut, std::string& errorMessage);
static void swapSelectedTextureData();
static void duplicateSelectedTexture();
static void removeSelectedTexture();
static void validateCurrentTextureArchive();
static void runCurrentTextureTest();
static void importModelDataIntoCurrentDraft();
static void layoutChildren(HWND hwnd);
static void refreshModeUi();
static void exportCurrentOpenedFile(bool exportAs);
static void flushStoryland(bool showStatus = false);
static void runCurrentModelTest();
static void populateMediaList();
static void selectMediaPayload(const StorylandTreePayload& payload);
static void exitAnalyzeGraph();
static void selectAnalyzeGraphNode(int index);
static void analyzeCurrentResourceGraph();
static void analyzeCurrentResourceData();

static bool sameWideNoCase(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (towlower(a[i]) != towlower(b[i])) return false;
    }
    return true;
}

static std::wstring normalizedFullPath(const std::wstring& path) {
    if (path.empty()) return L"";
    std::vector<wchar_t> buffer(32768, L'\0');
    DWORD length = GetFullPathNameW(path.c_str(), DWORD(buffer.size()), buffer.data(), nullptr);
    std::wstring result;
    if (length > 0 && length < buffer.size()) result.assign(buffer.data(), length);
    else result = path;
    std::replace(result.begin(), result.end(), L'/', L'\\');
    while (result.size() > 3 && !result.empty() && result.back() == L'\\') result.pop_back();
    return result;
}

static bool sameFilesystemPathNoCase(const std::wstring& a, const std::wstring& b) {
    return sameWideNoCase(normalizedFullPath(a), normalizedFullPath(b));
}

static bool textureArchiveExtension(const std::wstring& path) {
    std::wstring ext = getExtensionLower(path);
    return ext == L".xtx" || ext == L".chk" || ext == L".tex" || ext == L".txd";
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

    const wchar_t* directExtensions[] = {L".xtx", L".chk", L".tex", L".txd", L".XTX", L".CHK", L".TEX", L".TXD"};
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

    const wchar_t* patterns[] = {L"*.xtx", L"*.chk", L"*.tex", L"*.txd", L"*.XTX", L"*.CHK", L"*.TEX", L"*.TXD"};
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
        SBT_OWNERDRAW,
        reinterpret_cast<LPARAM>(gStatusText.c_str()));
}

static std::wstring buildModelStatusLine() {
    std::wstringstream status;
    const wchar_t* modelFormat = gModelFile.isMobileLcsDff() ? L"Mobile LCS DFF | "
                               : gModelFile.isPspNativeDff() ? L"PSP DFF | "
                               : gModelFile.isGtaSaDff() ? L"GTA SA DFF | "
                               : L"MDL | ";
    status << modelFormat
           << widen(gModelFile.modelKindName())
           << L" | " << gModelFile.armatureBones().size() << L" frames/bones"
           << L" | " << gModelFile.preview2dfxLights().size() << L" 2DFX lights";
    if (gModelTextureLoaded && !gModelTexturePath.empty()) {
        status << L" | " << std::filesystem::path(gModelTexturePath).filename().wstring()
               << L" | " << gModelTextureRegions.size() << L" textures"
               << L" | sheet " << gModelTextureImage.width << L"x" << gModelTextureImage.height
               << L" | Flip V " << (gModelFlipTextureV ? L"on" : L"off");
    } else {
        status << L" | no textures";
    }
    if (gModelAnimLoaded) status << L" | ANIM";
    return status.str();
}

static void setDetails(const std::wstring& text) {
    SetWindowTextW(gDetails, text.c_str());
}

static void setDetailsReadOnly(bool readOnly) {
    if (!gDetails) return;
    SendMessageW(gDetails, EM_SETREADONLY, readOnly ? TRUE : FALSE, 0);
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

static void clearView();
static void resetModelViewport();
static void populateModelList();
static void populateTextureList();
static void selectTexture(int index);

static std::wstring saveNewTypedResourceDialog(const wchar_t* filter, const wchar_t* defaultExt) {
    wchar_t fileName[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = gMainWindow;
    ofn.lpstrFilter = filter;
    ofn.lpstrDefExt = defaultExt;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) return L"";
    return fileName;
}

struct StorylandNewResourceTileState {
    int result = -1;
};

static LRESULT CALLBACK storylandNewResourceTileProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    StorylandNewResourceTileState* state =
        reinterpret_cast<StorylandNewResourceTileState*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const CREATESTRUCTW* create =
            reinterpret_cast<const CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<StorylandNewResourceTileState*>(
            create ? create->lpCreateParams : nullptr);
        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }

    constexpr int kLeft = 61001;
    constexpr int kRight = 61002;

    switch (message) {
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        if (id == kLeft || id == kRight) {
            if (state) state->result = id == kLeft ? 0 : 1;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static int chooseNewResourceTile(
    const wchar_t* title,
    const wchar_t* leftLabel,
    const wchar_t* rightLabel
) {
    static ATOM tileClass = 0;
    if (tileClass == 0) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = storylandNewResourceTileProc;
        wc.hInstance = gInstance;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        wc.lpszClassName = L"StorylandNewResourceTileClass";
        tileClass = RegisterClassW(&wc);
        if (tileClass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return -1;
        }
    }

    StorylandNewResourceTileState state;
    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        L"StorylandNewResourceTileClass",
        title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        220, 142,
        gMainWindow,
        nullptr,
        gInstance,
        &state);
    if (!dialog) return -1;

    constexpr int kLeft = 61001;
    constexpr int kRight = 61002;
    HWND left = CreateWindowW(
        L"BUTTON",
        leftLabel,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        24, 22, 76, 68,
        dialog,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kLeft)),
        gInstance,
        nullptr);
    HWND right = CreateWindowW(
        L"BUTTON",
        rightLabel,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        116, 22, 76, 68,
        dialog,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRight)),
        gInstance,
        nullptr);

    if (gUiFont) {
        SendMessageW(
            left,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(gUiFont),
            TRUE);
        SendMessageW(
            right,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(gUiFont),
            TRUE);
    }

    RECT parentRect = {};
    GetWindowRect(gMainWindow, &parentRect);
    SetWindowPos(
        dialog,
        HWND_TOP,
        parentRect.left + (parentRect.right - parentRect.left - 220) / 2,
        parentRect.top + 90,
        0,
        0,
        SWP_NOSIZE);

    EnableWindow(gMainWindow, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);
    SetFocus(left);

    MSG msg = {};
    while (IsWindow(dialog)) {
        const BOOL result = GetMessageW(&msg, nullptr, 0, 0);
        if (result <= 0) {
            if (result == 0) PostQuitMessage(int(msg.wParam));
            break;
        }

        if (msg.message == WM_KEYDOWN &&
            msg.wParam == VK_ESCAPE &&
            (msg.hwnd == dialog || IsChild(dialog, msg.hwnd))) {
            SendMessageW(dialog, WM_CLOSE, 0, 0);
            continue;
        }

        if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(gMainWindow, TRUE);
    SetActiveWindow(gMainWindow);
    SetForegroundWindow(gMainWindow);
    return state.result;
}

enum class StorylandNewPlatform {
    Ps2,
    Psp
};

enum class StorylandNewModelContainer {
    Mdl,
    Dff
};

static const wchar_t* newPlatformName(StorylandNewPlatform platform) {
    return platform == StorylandNewPlatform::Ps2 ? L"PS2" : L"PSP";
}

static const wchar_t* newModelKindLabel(StorylandModelKind kind) {
    switch (kind) {
    case StorylandModelKind::SimpleModel: return L"Simple Model";
    case StorylandModelKind::PedModel: return L"Ped Model";
    case StorylandModelKind::CutsceneModel: return L"Cutscene Model";
    case StorylandModelKind::VehicleModel: return L"Vehicle Model";
    case StorylandModelKind::WorldModel: return L"World Model";
    default: return L"Model";
    }
}

static void beginNewModelResource(
    StorylandNewPlatform platform,
    StorylandNewModelContainer container,
    StorylandModelKind kind
) {
    const bool dff = container == StorylandNewModelContainer::Dff;

    const wchar_t* filter = dff
        ? L"LCS PSP Beta DFF (*.dff)\0*.dff\0\0"
        : (platform == StorylandNewPlatform::Ps2
            ? L"PS2 Stories MDL (*.mdl)\0*.mdl\0\0"
            : L"PSP Stories Retail MDL (*.mdl)\0*.mdl\0\0");
    const wchar_t* extension = dff ? L"dff" : L"mdl";

    const std::wstring path =
        saveNewTypedResourceDialog(filter, extension);
    if (path.empty()) return;

    clearView();
    gDtzReturnAvailable = false;

    if (dff) {
        gModelFile.createEmptyDffDraft(path, kind);
    } else {
        gModelFile.createEmptyDraft(
            path,
            kind,
            platform == StorylandNewPlatform::Psp);
    }

    gMode = StorylandMode::ModelFile;
    gModelAnimLoaded = false;
    gModelAnimPath.clear();
    gModelAnimStatus.clear();
    resetModelViewport();
    populateModelList();

    const std::wstring formatName =
        dff ? L"DFF (LCS beta)" : L"MDL";
    const std::wstring title =
        L"Storyland - New " +
        std::wstring(newPlatformName(platform)) +
        L" " + formatName +
        L" " + newModelKindLabel(kind);
    SetWindowTextW(gMainWindow, title.c_str());

    applyStorylandTitleTintForPath(path);
    refreshModeUi();

    setStatus(
        L"New " +
        std::wstring(newPlatformName(platform)) +
        L" " + formatName +
        L" " + newModelKindLabel(kind) +
        L" | ready to edit.");
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void beginNewModelResource() {
    beginNewModelResource(
        StorylandNewPlatform::Ps2,
        StorylandNewModelContainer::Mdl,
        StorylandModelKind::SimpleModel);
}

static int gNewTextureDefaultBpp = 4;

enum class StorylandNewTextureContainer {
    Xtx,
    Chk,
    Txd
};

static RgbaImage newTextureSlotPlaceholder() {
    RgbaImage image;
    image.width = 64;
    image.height = 64;
    image.rgba.resize(64u * 64u * 4u, 255u);

    // Neutral two-colour placeholder. Slot 0 is immediately visible/editable
    // instead of creating an empty archive with no selectable material.
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const bool light = ((x / 8) + (y / 8)) % 2 == 0;
            const uint8_t value = light ? 190u : 90u;
            const size_t pixel =
                (size_t(y) * size_t(image.width) + size_t(x)) * 4u;
            image.rgba[pixel + 0u] = value;
            image.rgba[pixel + 1u] = value;
            image.rgba[pixel + 2u] = value;
            image.rgba[pixel + 3u] = 255u;
        }
    }
    return image;
}

static void beginNewTextureResource(StorylandNewTextureContainer container) {
    const int choice = chooseNewResourceTile(
        L"New Texture",
        L"4BPP",
        L"8BPP");
    if (choice < 0) return;

    gNewTextureDefaultBpp = choice == 0 ? 4 : 8;

    const wchar_t* filter = nullptr;
    const wchar_t* extension = nullptr;
    const wchar_t* title = nullptr;
    switch (container) {
    case StorylandNewTextureContainer::Xtx:
        filter = L"Stories XTX Texture Archive (*.xtx)\0*.xtx\0\0";
        extension = L"xtx";
        title = L"Storyland - New XTX";
        break;
    case StorylandNewTextureContainer::Chk:
        filter = L"Stories CHK Texture Archive (*.chk)\0*.chk\0\0";
        extension = L"chk";
        title = L"Storyland - New CHK";
        break;
    case StorylandNewTextureContainer::Txd:
        filter = L"LCS Beta RenderWare TXD (*.txd)\0*.txd\0\0";
        extension = L"txd";
        title = L"Storyland - New LCS Beta TXD";
        break;
    }

    const std::wstring path =
        saveNewTypedResourceDialog(filter, extension);
    if (path.empty()) return;

    clearView();
    gDtzReturnAvailable = false;

    const RgbaImage placeholder = newTextureSlotPlaceholder();
    std::string error;
    bool created = false;

    if (container == StorylandNewTextureContainer::Txd) {
        created = gTextureArchive.createLcsBetaTxd(
            path,
            "texture0",
            placeholder,
            uint8_t(gNewTextureDefaultBpp),
            error);
    } else {
        created = gTextureArchive.createEmptyPs2(path, error);
        if (created) {
            created = gTextureArchive.addTexture(
                "texture0",
                placeholder,
                uint8_t(gNewTextureDefaultBpp),
                error);
        }
    }

    if (!created) {
        MessageBoxW(
            gMainWindow,
            widen(error).c_str(),
            L"New texture archive failed",
            MB_ICONERROR);
        return;
    }

    gMode = StorylandMode::TextureArchive;
    populateTextureList();

    // A new texture archive always starts with an actual editable material.
    // Selecting slot 0 also enables Edit Material and Remove Material
    // immediately instead of requiring another action first.
    if (!gTextureArchive.textures().empty()) {
        selectTexture(0);
    }

    SetWindowTextW(gMainWindow, title);
    applyStorylandTitleTintForPath(path);
    refreshModeUi();

    const wchar_t* formatName =
        container == StorylandNewTextureContainer::Xtx ? L"XTX" :
        container == StorylandNewTextureContainer::Chk ? L"CHK" :
                                                         L"TXD";
    setStatus(
        L"New " + std::wstring(formatName) +
        L" created | slot 0 'texture0' | " +
        std::to_wstring(gNewTextureDefaultBpp) +
        L"bpp | Edit Material is ready.");
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
    return root + L"Storyland_export_log_" + std::to_wstring(GetCurrentProcessId()) + L".txt";
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
    bytes.clear();
    FILE* file = nullptr;
#ifdef _WIN32
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr) {
        error = "Could not open the selected file.";
        return false;
    }
    if (_fseeki64(file, 0, SEEK_END) != 0) {
        fclose(file);
        error = "Could not seek the selected file.";
        return false;
    }
    const __int64 fileSize = _ftelli64(file);
    constexpr uint64_t kMaximumUiFileBytes = 2ull * 1024ull * 1024ull * 1024ull;
    if (fileSize < 0 || uint64_t(fileSize) > kMaximumUiFileBytes ||
        uint64_t(fileSize) > uint64_t((std::numeric_limits<size_t>::max)())) {
        fclose(file);
        error = "Selected file is too large to load safely.";
        return false;
    }
    if (_fseeki64(file, 0, SEEK_SET) != 0) {
        fclose(file);
        error = "Could not rewind the selected file.";
        return false;
    }
    bytes.resize(size_t(fileSize));
#else
    file = fopen(std::filesystem::path(path).u8string().c_str(), "rb");
    if (!file) {
        error = "Could not open the selected file.";
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        error = "Could not seek the selected file.";
        return false;
    }
    const long fileSize = ftell(file);
    constexpr uint64_t kMaximumUiFileBytes = 2ull * 1024ull * 1024ull * 1024ull;
    if (fileSize < 0 || uint64_t(fileSize) > kMaximumUiFileBytes ||
        uint64_t(fileSize) > uint64_t((std::numeric_limits<size_t>::max)())) {
        fclose(file);
        error = "Selected file is too large to load safely.";
        return false;
    }
    rewind(file);
    bytes.resize(size_t(fileSize));
#endif

    if (!bytes.empty()) {
        const size_t readCount = fread(bytes.data(), 1, bytes.size(), file);
        if (readCount != bytes.size()) {
            fclose(file);
            bytes.clear();
            error = "Could not read the complete selected file.";
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
    gModelDetectedFlipTextureV = false;
    gModelTextureVDetectionReason.clear();
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

    const wchar_t* extensions[] = {L".xtx", L".chk", L".tex", L".txd", L".XTX", L".CHK", L".TEX", L".TXD"};
    for (const wchar_t* extension : extensions) {


        tryLoadModelTextureArchive(directory + stem + extension, modelStem, 50000, archivePathOut, textureIndexOut, bestScore);
    }

    WIN32_FIND_DATAW findData = {};
    const wchar_t* patterns[] = {L"*.xtx", L"*.chk", L"*.tex", L"*.txd", L"*.XTX", L"*.CHK", L"*.TEX", L"*.TXD"};
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

    // GTA SA DFFs use TXD dictionaries. Do not attach a random Stories
    // XTX/CHK from the same folder (that is what produced BFOTR.xtx on masha.dff).
    if (gModelFile.isGtaSaDff()) {
        candidatePaths.erase(
            std::remove_if(
                candidatePaths.begin(),
                candidatePaths.end(),
                [](const std::wstring& candidate) {
                    return getExtensionLower(candidate) != L".txd";
                }),
            candidatePaths.end());
    }

    if (candidatePaths.empty()) {
        statusOut = gModelFile.isGtaSaDff()
            ? L"No matching GTA SA TXD was found beside the model."
            : L"No matching texture file was found beside the model.";
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

        if (gModelFile.isGtaSaDff() &&
            baseScore == 0 &&
            preferredScore < 5000) {
            // Different-name TXDs are only accepted when a material texture
            // name actually matches the DFF. Generic words such as "body"
            // are not enough to attach an unrelated dictionary.
            continue;
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
        if (getExtensionLower(archivePath) == L".xtx") {
            // A companion XTX positively identifies this Leeds model session as
            // Vice City Stories even when the MDL itself has an ambiguous name
            // such as plr.mdl or pump.mdl.
            gStorylandVcsThemeContext = true;
        }
        gModelTextureIndex = decodedIndex;
        gModelTextureLoaded = true;
        gModelTextureUploadNeeded = true;

        std::wstringstream ss;
        ss << L"Texture: " << archivePath << L" -> " << decodedCount
           << L" texture" << (decodedCount == 1 ? L"" : L"s")
           << L" (" << gModelTextureImage.width << L"x" << gModelTextureImage.height << L")";
        if (decodedIndex != preferredIndex) {
            ss << L" [selected " << preferredIndex << L", loaded " << decodedIndex << L"]";
        }
        if (candidateIndex > 0) {
            ss << L" [file " << (candidateIndex + 1) << L"/" << candidatePaths.size() << L"]";
        }
        statusOut = ss.str();
        gModelTextureStatus = statusOut;
        return true;
    }

    std::wstringstream ss;
    ss << L"Matching texture files were found, but none could be opened. Files=" << candidatePaths.size()
       << L", open failures=" << failedArchiveCount
       << L", texture read failures=" << failedDecodeCount;
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

static bool effectiveModelTextureVFlip() {
    return gModelFlipTextureV;
}

static float applyModelTextureVOption(float v) {
    if (effectiveModelTextureVFlip()) return 1.0f - v;
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

static float wrapModelTextureCoordinate(float value) {
    if (!std::isfinite(value)) return 0.0f;
    value -= std::floor(value);
    if (value < 0.0f) value += 1.0f;
    return value;
}

static uint8_t sampleModelTextureAtlasAlpha(int regionIndex, float u, float v) {
    if (gModelTextureImage.width <= 0 || gModelTextureImage.height <= 0 || gModelTextureImage.rgba.empty()) return 255u;
    if (regionIndex < 0 || size_t(regionIndex) >= gModelTextureRegions.size()) return 255u;

    const auto& region = gModelTextureRegions[size_t(regionIndex)];
    const float localU = wrapModelTextureCoordinate(u);
    const float localV = wrapModelTextureCoordinate(v);
    const float atlasU = region.u0 + localU * (region.u1 - region.u0);
    const float atlasV = region.v0 + localV * (region.v1 - region.v0);

    int x = int(std::floor(atlasU * float(gModelTextureImage.width)));
    int y = int(std::floor(atlasV * float(gModelTextureImage.height)));
    x = std::max(0, std::min(gModelTextureImage.width - 1, x));
    y = std::max(0, std::min(gModelTextureImage.height - 1, y));

    const size_t pixel = (size_t(y) * size_t(gModelTextureImage.width) + size_t(x)) * 4u;
    if (pixel + 3u >= gModelTextureImage.rgba.size()) return 255u;
    return gModelTextureImage.rgba[pixel + 3u];
}

static std::wstring modelTextureVFormatFallbackReason() {
    if (gModelFile.isPspNativeDff()) {
        return L"PSP native DFF UVs and decoded PSP texture rows use the same top-origin convention.";
    }
    if (gModelFile.isMobileLcsDff()) {
        return L"Mobile LCS RenderWare UVs and decoded texture rows use the same top-origin convention.";
    }
    const std::wstring extension = getExtensionLower(gModelFile.sourcePath());
    if (extension == L".mdl") {
        return L"Leeds MDL packed UVs and decoded XTX/CHK rows use the native top-origin convention.";
    }
    if (extension == L".dff") {
        return L"RenderWare DFF UVs use the native top-origin convention used by Storyland's decoded texture rows.";
    }
    return L"No contrary row-order marker was found; native decoded texture orientation is retained.";
}

static void updateModelTextureVAutoDetection() {
    gModelDetectedFlipTextureV = false;

    const auto& points = gModelFile.previewPoints();
    const auto& triangles = gModelFile.previewTriangles();
    const auto& texcoords = gModelFile.previewTexcoords();
    const auto& prelights = gModelFile.previewPrelights();
    if (!gModelTextureLoaded || gModelTextureRegions.empty() || gModelTextureImage.rgba.empty()) {
        gModelTextureVDetectionReason = L"Texture V: no matching texture is loaded; normal coordinates are kept.";
        return;
    }
    if (points.empty() || triangles.empty() || texcoords.size() != points.size() || !modelTexcoordsLookUsable(texcoords)) {
        gModelTextureVDetectionReason = L"Texture V: model UV data is incomplete; normal coordinates are kept.";
        return;
    }

    float minX = points[0].x, maxX = points[0].x;
    float minY = points[0].y, maxY = points[0].y;
    float minZ = points[0].z, maxZ = points[0].z;
    for (const auto& point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) continue;
        minX = std::min(minX, point.x); maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y); maxY = std::max(maxY, point.y);
        minZ = std::min(minZ, point.z); maxZ = std::max(maxZ, point.z);
    }
    const float spanX = std::max(0.001f, maxX - minX);
    const float spanY = std::max(0.001f, maxY - minY);
    const float spanZ = std::max(0.001f, maxZ - minZ);

    uint64_t normalAlpha = 0u;
    uint64_t flippedAlpha = 0u;
    size_t sampleCount = 0u;
    size_t decisiveSamples = 0u;
    const size_t triangleLimit = std::min<size_t>(triangles.size(), 4096u);

    auto scoreSample = [&](int regionIndex, float u, float v) {
        const uint8_t normal = sampleModelTextureAtlasAlpha(regionIndex, u, v);
        const uint8_t flipped = sampleModelTextureAtlasAlpha(regionIndex, u, 1.0f - v);
        normalAlpha += normal;
        flippedAlpha += flipped;
        ++sampleCount;
        if (std::abs(int(normal) - int(flipped)) >= 32) ++decisiveSamples;
    };

    for (size_t triangleIndex = 0; triangleIndex < triangleLimit; ++triangleIndex) {
        const auto& tri = triangles[triangleIndex];
        if (tri.a >= points.size() || tri.b >= points.size() || tri.c >= points.size()) continue;
        if (tri.a >= texcoords.size() || tri.b >= texcoords.size() || tri.c >= texcoords.size()) continue;

        const int regionIndex = chooseModelTextureRegionForTriangle(
            tri, points, minX, minY, minZ, spanX, spanY, spanZ);
        if (regionIndex < 0) continue;

        const auto& uvA = texcoords[tri.a];
        const auto& uvB = texcoords[tri.b];
        const auto& uvC = texcoords[tri.c];
        if (!std::isfinite(uvA.u) || !std::isfinite(uvA.v) ||
            !std::isfinite(uvB.u) || !std::isfinite(uvB.v) ||
            !std::isfinite(uvC.u) || !std::isfinite(uvC.v)) continue;

        scoreSample(regionIndex, uvA.u, uvA.v);
        scoreSample(regionIndex, uvB.u, uvB.v);
        scoreSample(regionIndex, uvC.u, uvC.v);

        const float minTriU = std::min(uvA.u, std::min(uvB.u, uvC.u));
        const float maxTriU = std::max(uvA.u, std::max(uvB.u, uvC.u));
        const float minTriV = std::min(uvA.v, std::min(uvB.v, uvC.v));
        const float maxTriV = std::max(uvA.v, std::max(uvB.v, uvC.v));
        if ((maxTriU - minTriU) < 0.5f && (maxTriV - minTriV) < 0.5f) {
            scoreSample(regionIndex, (uvA.u + uvB.u + uvC.u) / 3.0f, (uvA.v + uvB.v + uvC.v) / 3.0f);
        }
    }

    if (sampleCount >= 24u && decisiveSamples >= 12u) {
        const uint64_t maximumScore = uint64_t(sampleCount) * 255ull;
        const uint64_t difference = normalAlpha > flippedAlpha ? normalAlpha - flippedAlpha : flippedAlpha - normalAlpha;
        if (difference * 100ull >= maximumScore * 4ull) {
            gModelDetectedFlipTextureV = flippedAlpha > normalAlpha;
            std::wstringstream ss;
            ss << L"Texture V check selected "
               << (gModelDetectedFlipTextureV ? L"flipped V" : L"native V")
               << L" (normal score=" << normalAlpha
               << L", flipped score=" << flippedAlpha
               << L", decisive samples=" << decisiveSamples << L"/" << sampleCount << L").";
            gModelTextureVDetectionReason = ss.str();
            return;
        }
    }

    gModelDetectedFlipTextureV = false;
    gModelTextureVDetectionReason = L"Texture V check: " + modelTextureVFormatFallbackReason();
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

static HWND createStorylandResourceTree(HWND parent) {
    HWND tree = CreateWindowExW(
        0,
        WC_TREEVIEWW,
        nullptr,
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS |
            TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS |
            TVS_SHOWSELALWAYS | TVS_NOTOOLTIPS,
        0, 0, 100, 100,
        parent,
        reinterpret_cast<HMENU>(ID_TREE),
        gInstance,
        nullptr);

    if (!tree) return nullptr;

    if (gUiFont) {
        SendMessageW(tree, WM_SETFONT, reinterpret_cast<WPARAM>(gUiFont), TRUE);
    }

    const COLORREF paneColor = storylandPaneBackgroundColor();
    TreeView_SetBkColor(tree, paneColor);
    TreeView_SetTextColor(tree, storylandPaneTextColor());
    TreeView_SetLineColor(
        tree,
        gEyeFriendlyPaneBackground ? RGB(70, 73, 83) : RGB(141, 169, 195));

    return tree;
}

static void replaceResourceTreeWithoutDeletingItems() {
    if (!gMainWindow) {
        gTreePayloads.clear();
        return;
    }

    HWND oldTree = gTree;
    HWND newTree = createStorylandResourceTree(gMainWindow);
    if (!newTree) {
        // If Windows cannot create a replacement control, keep the existing
        // tree rather than destroying its items through COMCTL32.
        gTreePayloads.clear();
        return;
    }

    if (oldTree) {
        RECT oldRect = {};
        if (GetWindowRect(oldTree, &oldRect)) {
            POINT topLeft{oldRect.left, oldRect.top};
            ScreenToClient(gMainWindow, &topLeft);
            MoveWindow(
                newTree,
                topLeft.x,
                topLeft.y,
                oldRect.right - oldRect.left,
                oldRect.bottom - oldRect.top,
                FALSE);
        }

        // The repeated COMCTL32 0x78F13 crash occurs while native TreeView
        // storage is being torn down.  Do not ask the control to delete its
        // existing items during a resource switch.  Retire it as a hidden
        // child and let Windows destroy all child controls once, when the main
        // window itself is destroyed.  Removing the control ID also prevents
        // stale WM_NOTIFY traffic from being mistaken for the active tree.
        SetWindowLongPtrW(oldTree, GWLP_ID, 0);
        ShowWindow(oldTree, SW_HIDE);
        EnableWindow(oldTree, FALSE);
    }

    gTree = newTree;
    gTreePayloads.clear();
    ShowWindow(gTree, SW_SHOW);
    InvalidateRect(gTree, nullptr, TRUE);
}

static void clearView() {
    gArchiveViewFocusActive = false;
    setDetailsReadOnly(true);
    clearDtzEmbeddedPreviewState();
    gModelDffStructureTreeActive = false;
    gModelDffStructureBytes.clear();
    gModelDffStructureName.clear();
    clearModelTexture();
    replaceResourceTreeWithoutDeletingItems();
    setDetails(L"");
    gCurrentImage = {};
    deleteTextureBitmap();
    InvalidateRect(gPreview, nullptr, TRUE);
    gSelectedIndex = -1;
    gSelectedKind = StorylandTreeKind::None;
}

static void resetModelViewport() {
    gArchiveViewFocusActive = false;
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

static bool selectedArchiveResourceId(uint32_t& resourceIdOut) {
    if (gMode != StorylandMode::ArchiveFile) return false;

    if (gSelectedKind == StorylandTreeKind::ArchiveMeshResource &&
        gSelectedIndex >= 0 && size_t(gSelectedIndex) < gArchiveMeshResourceIds.size()) {
        resourceIdOut = gArchiveMeshResourceIds[size_t(gSelectedIndex)];
        return true;
    }

    if (gSelectedKind == StorylandTreeKind::ArchiveEntry &&
        gSelectedIndex >= 0 && size_t(gSelectedIndex) < gArchiveBrowser.entries().size()) {
        const std::string ext = archiveEntryExtensionLower(gArchiveBrowser.entries()[size_t(gSelectedIndex)].name);
        if (ext == ".mdl" || ext == ".dff") {
            resourceIdOut = uint32_t(gSelectedIndex);
            return true;
        }
    }
    return false;
}

static bool canViewSelectedInRenderer() {
    if (!currentModeUsesInteractiveModelViewport()) return false;
    if (gMode == StorylandMode::ModelFile ||
        (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile)) {
        return true;
    }
    if (gMode == StorylandMode::ArchiveFile) {
        uint32_t resourceId = 0;
        if (!selectedArchiveResourceId(resourceId)) return false;
        for (const auto& placement : gArchiveBrowser.placements()) {
            if (placement.resourceIndex == resourceId) return true;
        }
    }
    return false;
}

static bool viewSelectedInRenderer() {
    if (gMode == StorylandMode::ModelFile ||
        (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile)) {
        gArchiveViewFocusActive = false;
        gModelViewRotation = {};
        gModelDragStartRotation = gModelViewRotation;
        gModelDistance = 2.25f;
        gModelPanX = 0.0f;
        gModelPanY = 0.0f;
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(L"View centered on the selected model.");
        return true;
    }

    if (gMode != StorylandMode::ArchiveFile) return false;

    uint32_t resourceId = 0;
    if (!selectedArchiveResourceId(resourceId)) return false;

    bool found = false;
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    for (const auto& placement : gArchiveBrowser.placements()) {
        if (placement.resourceIndex != resourceId) continue;
        const float radius = std::max(0.5f, std::min(100.0f, std::fabs(placement.boundRadius)));
        if (!found) {
            minX = placement.x - radius; maxX = placement.x + radius;
            minY = placement.y - radius; maxY = placement.y + radius;
            minZ = placement.z - radius; maxZ = placement.z + radius;
            found = true;
        } else {
            minX = std::min(minX, placement.x - radius); maxX = std::max(maxX, placement.x + radius);
            minY = std::min(minY, placement.y - radius); maxY = std::max(maxY, placement.y + radius);
            minZ = std::min(minZ, placement.z - radius); maxZ = std::max(maxZ, placement.z + radius);
        }
    }
    if (!found) return false;

    gArchiveViewFocusX = (minX + maxX) * 0.5f;
    gArchiveViewFocusY = (minY + maxY) * 0.5f;
    gArchiveViewFocusZ = (minZ + maxZ) * 0.5f;
    const float spanX = std::max(1.0f, maxX - minX);
    const float spanY = std::max(1.0f, maxY - minY);
    const float spanZ = std::max(1.0f, maxZ - minZ);
    gArchiveViewFocusSpan = std::max(2.0f, std::max(spanX, std::max(spanY, spanZ)));
    gArchiveViewFocusActive = true;

    // "View Selected" is an inspection view: put the camera directly in front
    // of the selected placed resource instead of preserving an arbitrary orbit.
    gModelViewRotation = {};
    gModelDragStartRotation = gModelViewRotation;
    gModelDistance = 3.0f;
    gModelPanX = 0.0f;
    gModelPanY = 0.0f;
    if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
    setStatus(L"View centered in front of selected LVZ/IMG resource " + std::to_wstring(resourceId) + L".");
    return true;
}

static void showRendererContextMenu(HWND hwnd, int clientX, int clientY) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    constexpr UINT kResetView = 1;
    constexpr UINT kViewSelected = 2;
    AppendMenuW(menu, MF_STRING, kResetView, L"Reset View");
    AppendMenuW(menu, MF_STRING | (canViewSelectedInRenderer() ? MF_ENABLED : MF_GRAYED),
                kViewSelected, L"View Selected");

    POINT screenPoint{clientX, clientY};
    ClientToScreen(hwnd, &screenPoint);
    const UINT command = TrackPopupMenu(menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
        screenPoint.x, screenPoint.y, 0, hwnd, nullptr);
    DestroyMenu(menu);

    if (command == kResetView) {
        resetModelViewport();
        if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
        setStatus(L"Viewport reset.");
    } else if (command == kViewSelected) {
        viewSelectedInRenderer();
    }
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

static RECT renderPieButtonRect(UINT id) {
    const int cx = 80;
    const int cy = 80;
    switch (id) {
    case ID_RENDER_PIE_WIREFRAME: return RECT{4, 58, 56, 102};
    case ID_RENDER_PIE_SOLID:     return RECT{104, 58, 156, 102};
    case ID_RENDER_PIE_TEXTURED:  return RECT{52, 108, 108, 156};
    case ID_RENDER_PIE_STORIES:   return RECT{52, 4, 108, 52};
    default: return RECT{};
    }
}

static int renderPieSelectionFromPoint(POINT pt) {
    const float dx = float(pt.x - 80);
    const float dy = float(pt.y - 80);
    if (std::abs(dx) <= 10.0f && std::abs(dy) <= 10.0f) return -1;
    if (std::abs(dx) > std::abs(dy)) return dx < 0.0f ? 0 : 1;
    return dy < 0.0f ? 3 : 2;
}

static void drawRenderPieButton(HDC dc, UINT id, const wchar_t* label, bool active, bool hovered) {
    RECT rc = renderPieButtonRect(id);
    if (active || hovered) {
        HBRUSH brush = CreateSolidBrush(active ? RGB(54, 91, 139) : RGB(62, 79, 108));
        HGDIOBJ old = SelectObject(dc, brush);
        RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
        SelectObject(dc, old);
        DeleteObject(brush);
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(245, 245, 250));
    DrawTextW(dc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static LRESULT CALLBACK renderPieProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client{}; GetClientRect(hwnd, &client);
        HBRUSH background = CreateSolidBrush(RGB(31, 34, 41));
        FillRect(dc, &client, background);
        DeleteObject(background);

        POINT mouse{}; GetCursorPos(&mouse); ScreenToClient(hwnd, &mouse);
        const int hover = renderPieSelectionFromPoint(mouse);
        const int cx = 80, cy = 80, radius = 74;
        HBRUSH fill = CreateSolidBrush(RGB(42, 46, 56));
        HPEN ring = CreatePen(PS_SOLID, 1, RGB(170, 178, 196));
        HGDIOBJ oldBrush = SelectObject(dc, fill);
        HGDIOBJ oldPen = SelectObject(dc, ring);
        Ellipse(dc, cx - radius, cy - radius, cx + radius, cy + radius);
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush);
        DeleteObject(ring); DeleteObject(fill);
        HPEN inner = CreatePen(PS_SOLID, 1, RGB(135, 140, 155));
        oldPen = SelectObject(dc, inner);
        Ellipse(dc, cx - 18, cy - 18, cx + 18, cy + 18);
        MoveToEx(dc, cx - 18, cy, nullptr); LineTo(dc, cx - radius + 8, cy);
        MoveToEx(dc, cx + 18, cy, nullptr); LineTo(dc, cx + radius - 8, cy);
        MoveToEx(dc, cx, cy - 18, nullptr); LineTo(dc, cx, cy - radius + 8);
        MoveToEx(dc, cx, cy + 18, nullptr); LineTo(dc, cx, cy + radius - 8);
        SelectObject(dc, oldPen); DeleteObject(inner);

        drawRenderPieButton(dc, ID_RENDER_PIE_WIREFRAME, L"Wireframe", gOpenGlRenderMode == StorylandOpenGlRenderMode::Wireframe, hover == 0);
        drawRenderPieButton(dc, ID_RENDER_PIE_SOLID, L"Solid", gOpenGlRenderMode == StorylandOpenGlRenderMode::Solid, hover == 1);
        drawRenderPieButton(dc, ID_RENDER_PIE_TEXTURED, L"Material", gOpenGlRenderMode == StorylandOpenGlRenderMode::Textured, hover == 2);
        drawRenderPieButton(dc, ID_RENDER_PIE_STORIES, L"Render", gOpenGlRenderMode == StorylandOpenGlRenderMode::Stories, hover == 3);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int selection = renderPieSelectionFromPoint(pt);
        if (selection >= 0) {
            const StorylandOpenGlRenderMode modes[4] = {
                StorylandOpenGlRenderMode::Wireframe,
                StorylandOpenGlRenderMode::Solid,
                StorylandOpenGlRenderMode::Textured,
                StorylandOpenGlRenderMode::Stories
            };
            setOpenGlRenderMode(modes[selection]);
            setStatus(std::wstring(L"Viewport shading: ") + openGlRenderModeName(modes[selection]) + L".");
            ShowWindow(hwnd, SW_HIDE);
            SetFocus(gPreview ? gPreview : gMainWindow);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || wParam == 'Z') {
            ShowWindow(hwnd, SW_HIDE);
            SetFocus(gPreview ? gPreview : gMainWindow);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void showRenderModePie(HWND owner) {
    if (!gPreview) return;
    static const wchar_t* kClassName = L"StorylandRenderPieClassV2";
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = renderPieProc;
        wc.hInstance = gInstance;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kClassName;
        registered = RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    if (!registered) return;

    if (!gRenderPieWindow || !IsWindow(gRenderPieWindow)) {
        gRenderPieWindow = CreateWindowExW(
            WS_EX_TOOLWINDOW, kClassName, L"", WS_CHILD | WS_CLIPSIBLINGS,
            0, 0, 160, 160, gPreview, nullptr, gInstance, nullptr);
    }
    if (!gRenderPieWindow) return;

    // Keep the render selector physically circular.  Using a color-keyed layered
    // child window made the control disappear on some Windows/common-controls
    // configurations and was the reason the pie appeared to have been removed.
    // A real window region gives us the ODFF-style circular selector without a
    // rectangular background or layered-child quirks.
    HRGN pieRegion = CreateEllipticRgn(0, 0, 160, 160);
    SetWindowRgn(gRenderPieWindow, pieRegion, TRUE);

    if (IsWindowVisible(gRenderPieWindow)) {
        ShowWindow(gRenderPieWindow, SW_HIDE);
        SetFocus(gPreview);
        return;
    }

    RECT rc{}; GetClientRect(gPreview, &rc);
    const int width = 160, height = 160;
    POINT mouse{}; GetCursorPos(&mouse); ScreenToClient(gPreview, &mouse);
    const int x = std::clamp(int(mouse.x) - width / 2, 0, std::max(0, int(rc.right - rc.left) - width));
    const int y = std::clamp(int(mouse.y) - height / 2, 0, std::max(0, int(rc.bottom - rc.top) - height));
    MoveWindow(gRenderPieWindow, x, y, width, height, TRUE);
    ShowWindow(gRenderPieWindow, SW_SHOW);
    BringWindowToTop(gRenderPieWindow);
    SetFocus(gRenderPieWindow);
    InvalidateRect(gRenderPieWindow, nullptr, FALSE);
}

static bool handleModelViewportShortcut(WPARAM key) {
    if (!currentModeUsesInteractiveModelViewport()) return false;
    if (moveArchiveViewportKey(key)) return true;

    const bool fastMove = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const float panStep = std::max(0.025f, gModelDistance * (fastMove ? 0.12f : 0.045f));
    switch (key) {
    case 'W': applyModelViewportZoom(fastMove ? 0.68f : 0.88f); setStatus(L"Viewport dolly: W/S. A/D strafe, Q/E vertical, Shift = faster."); return true;
    case 'S': applyModelViewportZoom(fastMove ? 1.46f : 1.14f); setStatus(L"Viewport dolly: W/S. A/D strafe, Q/E vertical, Shift = faster."); return true;
    case 'A': gModelPanX += panStep; if (gPreview) InvalidateRect(gPreview, nullptr, FALSE); return true;
    case 'D': gModelPanX -= panStep; if (gPreview) InvalidateRect(gPreview, nullptr, FALSE); return true;
    case 'Q': gModelPanY -= panStep; if (gPreview) InvalidateRect(gPreview, nullptr, FALSE); return true;
    case 'E': gModelPanY += panStep; if (gPreview) InvalidateRect(gPreview, nullptr, FALSE); return true;
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
        setStatus(gOpenGlShowViewCube ? L"Viewport gizmo on." : L"Viewport gizmo off.");
        return true;
    case 'Z':
        showRenderModePie(gMainWindow);
        return true;
    case VK_BACK:
        if (gDtzReturnAvailable) {
            returnToGameDtz();
            return true;
        }
        break;
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
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uPrelightMode");
    if (loc >= 0) {
        int prelightMode = 0;
        if (gPrelightViewMode == StorylandPrelightViewMode::Raw) prelightMode = 1;
        else if (gPrelightViewMode == StorylandPrelightViewMode::Off) prelightMode = 2;
        pglUniform1i(loc, prelightMode);
    }
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uFogColor");
    if (loc >= 0) {
        const StorylandSkyColor& fog = gStoriesSky.state().fog;
        pglUniform3f(loc, fog.r, fog.g, fog.b);
    }
    const StorylandSkyState& sky = gStoriesSky.state();
    loc = pglGetUniformLocation(gStoriesShaderProgram, "uAmbientColor");
    if (loc >= 0) pglUniform3f(loc, sky.objectAmbient.r, sky.objectAmbient.g, sky.objectAmbient.b);
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
    glDisable(GL_LIGHT1);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    const StorylandSkyState& sky = gStoriesSky.state();
    GLfloat ambientModel[] = {sky.objectAmbient.r, sky.objectAmbient.g, sky.objectAmbient.b, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientModel);
    GLfloat light0Pos[] = {sky.sunDirectionX, sky.sunDirectionY, sky.sunDirectionZ, 0.0f};
    GLfloat light0Diffuse[] = {sky.directional.r, sky.directional.g, sky.directional.b, 1.0f};
    GLfloat light0Spec[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0Spec);
    GLfloat materialSpec[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
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
        if (gArchiveTexturePreviewId != 0) {
            glDeleteTextures(1, &gArchiveTexturePreviewId);
            gArchiveTexturePreviewId = 0;
        }
        gArchiveTexturePreviewIndex = -1;
        gArchiveTexturePreviewHeaderOffset = 0;
        gArchiveTexturePreviewWidth = 0;
        gArchiveTexturePreviewHeight = 0;
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
    // The visible Stories atmosphere is drawn as a full-screen timecycle gradient.
    // Keep the framebuffer clear neutral so a failed/disabled sky pass cannot turn
    // the whole viewport into one flat timecycle colour.
    glClearColor(0.115f, 0.120f, 0.128f, 1.0f);
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

static std::string viewportLabelAscii(const std::wstring& text) {
    std::string result;
    result.reserve(text.size());
    for (wchar_t ch : text) {
        if (ch >= 0x20 && ch <= 0x7e) result.push_back(char(ch));
        else if (ch == L'\t') result.push_back(' ');
        else result.push_back('?');
    }
    return result;
}

static void drawSelectedViewportLabelOpenGl(HDC dc, int width, int height) {
    const std::wstring wideText = selectedTreeDisplayText();
    if (wideText.empty() || width <= 0 || height <= 0) return;
    const std::string text = viewportLabelAscii(wideText);
    if (text.empty()) return;

    HFONT font = CreateFontW(
        -16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_DONTCARE, L"Consolas");
    if (!font) return;
    HGDIOBJ oldFont = SelectObject(dc, font);

    GLuint listBase = glGenLists(96);
    if (listBase == 0 || !wglUseFontBitmapsW(dc, 32, 96, listBase)) {
        if (listBase != 0) glDeleteLists(listBase, 96);
        SelectObject(dc, oldFont);
        DeleteObject(font);
        return;
    }

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT | GL_TRANSFORM_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, double(width), 0.0, double(height), -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const float boxWidth = std::min(float(width - 20), 18.0f + float(text.size()) * 8.5f);
    glColor4f(0.055f, 0.060f, 0.075f, 0.42f);
    glBegin(GL_QUADS);
    glVertex2f(10.0f, float(height) - 38.0f);
    glVertex2f(10.0f + boxWidth, float(height) - 38.0f);
    glVertex2f(10.0f + boxWidth, float(height) - 10.0f);
    glVertex2f(10.0f, float(height) - 10.0f);
    glEnd();

    glColor4f(0.72f, 0.75f, 0.82f, 0.38f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(10.5f, float(height) - 37.5f);
    glVertex2f(9.5f + boxWidth, float(height) - 37.5f);
    glVertex2f(9.5f + boxWidth, float(height) - 10.5f);
    glVertex2f(10.5f, float(height) - 10.5f);
    glEnd();

    glColor4f(0.95f, 0.96f, 0.98f, 0.98f);
    glRasterPos2f(18.0f, float(height) - 29.0f);
    glListBase(listBase - 32);
    glCallLists(GLsizei(text.size()), GL_UNSIGNED_BYTE, text.data());

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();

    glDeleteLists(listBase, 96);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}


static void drawViewportTextOpenGl(HDC dc, int width, int height, int x, int yFromTop,
                                   const std::wstring& wideText, bool semibold = false) {
    if (!dc || wideText.empty() || width <= 0 || height <= 0) return;
    const std::string text = viewportLabelAscii(wideText);
    if (text.empty()) return;

    HFONT font = CreateFontW(
        -15, 0, 0, 0, semibold ? FW_SEMIBOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    if (!font) return;

    HGDIOBJ oldFont = SelectObject(dc, font);
    GLuint listBase = glGenLists(96);
    if (listBase == 0 || !wglUseFontBitmapsW(dc, 32, 96, listBase)) {
        if (listBase != 0) glDeleteLists(listBase, 96);
        SelectObject(dc, oldFont);
        DeleteObject(font);
        return;
    }

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT | GL_TRANSFORM_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, double(width), 0.0, double(height), -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const float px = float(x);
    const float py = float(height - yFromTop - 15);

    // Small two-pass shadow instead of an opaque label rectangle.
    glColor4f(0.0f, 0.0f, 0.0f, 0.72f);
    glRasterPos2f(px + 1.0f, py - 1.0f);
    glListBase(listBase - 32);
    glCallLists(GLsizei(text.size()), GL_UNSIGNED_BYTE, text.data());

    glColor4f(0.94f, 0.95f, 0.97f, 0.98f);
    glRasterPos2f(px, py);
    glCallLists(GLsizei(text.size()), GL_UNSIGNED_BYTE, text.data());

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();

    glDeleteLists(listBase, 96);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}


static void populateTextureList() {
    clearView();
    const auto& textures = gTextureArchive.textures();

    const std::wstring archiveExtension =
        getExtensionLower(gTextureArchive.sourcePath());
    const wchar_t* archiveLabel =
        archiveExtension == L".txd" ? L"LCS Beta RenderWare TXD" :
        archiveExtension == L".chk" ? L"Stories CHK Texture Archive" :
        archiveExtension == L".xtx" ? L"Stories XTX Texture Archive" :
        archiveExtension == L".tex" ? L"CTW TEX Texture" :
                                      L"Texture Archive";
    HTREEITEM root = addTreeItem(TVI_ROOT, archiveLabel);
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
    size_t matchedDirEntries = 0;
    size_t fullyBackedEntries = 0;
    size_t partiallyBackedEntries = 0;

    for (const auto& record : records) {
        if (!record.resourceName.empty()) namedRecords++;
    }

    for (const auto& entry : dirEntries) {
        if (!entry.matchingRecordIndices.empty()) matchedDirEntries++;
        if (entry.fullyBackedByImg) fullyBackedEntries++;
        else if (entry.availableBytes != 0u) partiallyBackedEntries++;
    }

    ss << L"Streaming / gta3PS*.img runtime map:\r\n";
    ss << L"  Internal GAME.DTZ streaming entries shown: " << dirEntries.size() << L"\r\n";
    ss << L"  Internal entries with one or more GAME.DTZ records: " << matchedDirEntries << L"\r\n";
    ss << L"  Raw GAME.DTZ sector-like records found: " << records.size() << L"\r\n";
    ss << L"  Entries with recovered names: " << namedRecords << L"\r\n";
    if (gDtzArchive.hasCompanionImg()) {
        ss << L"  Entries fully backed by the loaded IMG: " << fullyBackedEntries << L"\r\n";
        ss << L"  Entries only partially backed by the loaded IMG: " << partiallyBackedEntries << L"\r\n";
    }

    if (!gDtzArchive.companionDirPath().empty()) {
        ss << L"  Optional classic/beta DIR loaded: " << gDtzArchive.companionDirPath() << L"\r\n";
    } else {
        ss << L"  Optional classic/beta DIR: none loaded. Retail GAME.DTZ metadata is used when present.\r\n";
    }

    ss << L"  Sector positions are not used to guess VCS/LCS resource names; names come from DTZ metadata, hashes, DIR rows, or payload signatures.\r\n\r\n";
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
    size_t decodedLeeds2dfxEffects = 0;
    size_t decodedLeeds2dfxLights = 0;
    size_t modelLinkedLeeds2dfxEffects = 0;
    size_t worldLeeds2dfxLights = 0;
    const auto& nativeEffects = gDtzArchive.leeds2dfxEffects();
    for (const auto& effect : nativeEffects) {
        if (!effect.valid) continue;
        decodedLeeds2dfxEffects++;
        if (effect.isLight) decodedLeeds2dfxLights++;
        if (effect.hasModelAssociation) modelLinkedLeeds2dfxEffects++;
    }
    for (const auto& instance : gDtzArchive.leeds2dfxWorldInstances()) {
        if (!instance.valid || instance.effectIndex >= nativeEffects.size()) continue;
        if (nativeEffects[instance.effectIndex].isLight) worldLeeds2dfxLights++;
    }

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
        ss << L"Loaded companion IMG: none; load gta3PS2.img/gta3PSP.img/GTA3PSPHR.IMG to make sector-count edits resize the physical IMG too.\r\n";
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

    ss << L"Leeds native 2DFX / C2dEffect table:\r\n";
    ss << L"  Header-declared effects: " << twoDfxCount << L"\r\n";
    ss << L"  Valid decoded effects: " << decodedLeeds2dfxEffects << L"\r\n";
    ss << L"  Valid decoded light definitions: " << decodedLeeds2dfxLights << L"\r\n";
    ss << L"  Definitions linked through CBaseModelInfo ranges: " << modelLinkedLeeds2dfxEffects << L"\r\n";
    ss << L"  World light placements: " << worldLeeds2dfxLights << L"\r\n";
    ss << L"  The right viewport uses each allocated CEntity world matrix to place native model-space effects. It falls back to a model-definition atlas only when no allocated pool instances can be resolved.\r\n\r\n";

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
    appendDtzPointerOverviewLine(ss, L"CStreamingInfo::ms_aInfoForModel", 0x7C, readCurrentDtzU32(0x7C), L"GTA3PS*.DIR analogue", L"critical gta3PS*.img sector metadata used at runtime");
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
       << L"  - gta3PS*.img ranges use 2048-byte logical archive sectors. On PS2 these are not PCSX2 raw DVD 2064-byte blocks.\r\n"
       << L"  - Retail LCS/VCS use GAME.DTZ for the runtime map; optional .dir support is only for beta-build archives.\r\n"
       << L"  - When a companion IMG is loaded, sector-count patches also insert/delete 2048-byte sector spans in the IMG when shifting is enabled.\r\n";

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}


static void clearDtzFindState(bool clearQuery) {
    gDtzFindResults.clear();
    gDtzTextureNameIndex.clear();
    gDtzTextureNameIndexReady = false;
    gDtzTextureArchivesScanned = 0;
    gDtzTextureArchivesRejected = 0;
    if (clearQuery) gDtzFindQuery.clear();
}

static bool isDtzTextureArchiveExtension(const std::wstring& extension) {
    return extension == L".xtx" || extension == L".chk" || extension == L".tex" || extension == L".txd";
}

static bool buildDtzTextureNameIndex(std::wstring& summaryOut) {
    summaryOut.clear();
    if (gDtzTextureNameIndexReady) return true;
    gDtzTextureNameIndex.clear();
    gDtzTextureArchivesScanned = 0;
    gDtzTextureArchivesRejected = 0;

    if (!gDtzArchive.hasCompanionImg()) {
        summaryOut = L"Deep texture-name search needs the companion gta3PS*.img loaded for this GAME.DTZ.";
        return false;
    }

    const auto& entries = gDtzArchive.dirEntries();
    constexpr uint64_t kMaxTotalExtractedBytes = 512ull * 1024ull * 1024ull;
    constexpr size_t kMaxTextureNames = 250000u;
    constexpr size_t kMaxTexturesPerArchive = 8192u;
    uint64_t totalExtractedBytes = 0;
    bool budgetReached = false;

    for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex) {
        const auto& entry = entries[entryIndex];
        std::wstring resourceName = safeEmbeddedFileName(
            canonicalDtzImgResourceName(widenResourceName(entry.name)), L"resource.bin");
        const std::wstring extension = getExtensionLower(resourceName);
        if (!isDtzTextureArchiveExtension(extension)) continue;

        ++gDtzTextureArchivesScanned;
        if (entry.byteLength > kMaxTotalExtractedBytes ||
            totalExtractedBytes > kMaxTotalExtractedBytes - entry.byteLength) {
            ++gDtzTextureArchivesRejected;
            budgetReached = true;
            continue;
        }

        std::vector<uint8_t> bytes;
        std::string extractError;
        if (!gDtzArchive.extractDirEntryBytes(entryIndex, bytes, extractError)) {
            ++gDtzTextureArchivesRejected;
            continue;
        }
        totalExtractedBytes += uint64_t(bytes.size());

        LeedsTextureArchive archive;
        std::string parseError;
        if (!archive.loadFromMemory(bytes, LeedsPlatform::Auto, parseError, resourceName)) {
            ++gDtzTextureArchivesRejected;
            continue;
        }

        const auto& textures = archive.textures();
        const size_t textureLimit = std::min(textures.size(), kMaxTexturesPerArchive);
        for (size_t textureIndex = 0; textureIndex < textureLimit; ++textureIndex) {
            if (gDtzTextureNameIndex.size() >= kMaxTextureNames) {
                budgetReached = true;
                break;
            }
            std::wstring textureName = widen(textures[textureIndex].name);
            if (textureName.empty()) continue;
            gDtzTextureNameIndex.push_back({int(entryIndex), int(textureIndex), std::move(textureName)});
        }
        if (gDtzTextureNameIndex.size() >= kMaxTextureNames) break;

        if ((gDtzTextureArchivesScanned & 63u) == 0u) {
            setStatus(L"GAME.DTZ Find | indexing texture names... " +
                      std::to_wstring(gDtzTextureArchivesScanned) + L" archives scanned");
            if (gStatus) UpdateWindow(gStatus);
        }
    }

    gDtzTextureNameIndexReady = true;
    std::wstringstream summary;
    summary << L"Indexed " << gDtzTextureNameIndex.size() << L" texture names from "
            << gDtzTextureArchivesScanned << L" internal texture archives";
    if (gDtzTextureArchivesRejected != 0) summary << L"; " << gDtzTextureArchivesRejected << L" archives could not be safely parsed";
    if (budgetReached) summary << L"; safety scan budget reached";
    summary << L".";
    summaryOut = summary.str();
    return true;
}

static void populateArchiveList();
static void refreshModeUi();
static void returnToDtzFromAreaArchive();

static void loadAreaArchivesForCurrentDtz() {
    if (gActiveAreaArchive >= 0 && gAreaArchives[size_t(gActiveAreaArchive)].browser) {
        std::swap(gArchiveBrowser, *gAreaArchives[size_t(gActiveAreaArchive)].browser);
    }
    gActiveAreaArchive = -1;
    for (auto& area : gAreaArchives) area = {};
    if (!gDtzArchive.hasCompanionImg()) return;

    std::filesystem::path root(gDtzArchive.sourcePath());
    root = root.parent_path();
    std::wstring folderName = root.filename().wstring();
    std::transform(folderName.begin(), folderName.end(), folderName.begin(),
        [](wchar_t c) { return wchar_t(towlower(c)); });
    std::vector<std::filesystem::path> roots{root};
    if ((folderName == L"ps2" || folderName == L"models" || folderName == L"data") &&
        root.has_parent_path()) {
        roots.push_back(root.parent_path());
    }
    std::vector<std::filesystem::path> folders = roots;
    std::error_code ec;
    // The retail layout can nest area archives below PS2/LEVELS or PS2/MODELS.
    // Search a bounded number of directory levels without walking the whole drive.
    size_t levelStart = 0;
    for (int depth = 0; depth < 3 && levelStart < folders.size(); ++depth) {
        const size_t levelEnd = folders.size();
        for (size_t folderIndex = levelStart; folderIndex < levelEnd && folders.size() < 256u; ++folderIndex) {
            ec.clear();
            for (const auto& item : std::filesystem::directory_iterator(folders[folderIndex], ec)) {
                if (ec || folders.size() >= 256u) break;
                if (item.is_directory(ec)) folders.push_back(item.path());
            }
        }
        levelStart = levelEnd;
    }

    static constexpr const wchar_t* names[] = {L"BEACH", L"MALL", L"MAINLA"};
    for (size_t index = 0; index < gAreaArchives.size(); ++index) {
        StorylandAreaArchive& area = gAreaArchives[index];
        area.name = names[index];
        for (const auto& folder : folders) {
            ec.clear();
            for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
                if (ec) break;
                if (!item.is_regular_file(ec)) continue;
                std::wstring stem = item.path().stem().wstring();
                std::transform(stem.begin(), stem.end(), stem.begin(),
                    [](wchar_t c) { return wchar_t(towlower(c)); });
                std::wstring wanted = area.name;
                std::transform(wanted.begin(), wanted.end(), wanted.begin(),
                    [](wchar_t c) { return wchar_t(towlower(c)); });
                if (stem != wanted || getExtensionLower(item.path().wstring()) != L".lvz") continue;

                auto candidate = std::make_unique<StorylandArchiveBrowser>();
                std::string error;
                if (candidate->loadLvzWithCompanionImg(item.path().wstring(), error)) {
                    area.browser = std::move(candidate);
                    area.loadError.clear();
                } else {
                    area.loadError = widen(error);
                }
                break;
            }
            if (area.browser) break;
        }
    }
}

static const StorylandArchiveBrowser* loadedDtzAreaBrowser(size_t index) {
    if (index >= gAreaArchives.size() || !gAreaArchives[index].browser) return nullptr;
    return int(index) == gActiveAreaArchive ? &gArchiveBrowser : gAreaArchives[index].browser.get();
}

static void activateAreaArchive(int index) {
    if (index < 0 || size_t(index) >= gAreaArchives.size()) return;
    StorylandAreaArchive& area = gAreaArchives[size_t(index)];
    if (!area.browser) return;
    if (gActiveAreaArchive == index) {
        gMode = StorylandMode::ArchiveFile;
        populateArchiveList();
        refreshModeUi();
        return;
    }
    if (gActiveAreaArchive >= 0 && gAreaArchives[size_t(gActiveAreaArchive)].browser) {
        std::swap(gArchiveBrowser, *gAreaArchives[size_t(gActiveAreaArchive)].browser);
    } else {
        gArchiveBrowser = StorylandArchiveBrowser{};
    }
    std::swap(gArchiveBrowser, *area.browser);
    gActiveAreaArchive = index;
    gMode = StorylandMode::ArchiveFile;
    resetModelViewport();
    gModelDistance = 8.0f;
    populateArchiveList();
    SetWindowTextW(gMainWindow, (L"Storyland - " + area.name + L" LVZ + IMG").c_str());
    refreshModeUi();
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
        HTREEITEM areasRoot = addTreeItem(root, L"Area archives loaded with GAME.DTZ");
        for (size_t index = 0; index < gAreaArchives.size(); ++index) {
            const auto& area = gAreaArchives[index];
            std::wstring label = area.name + L".LVZ + " + area.name + L".IMG";
            if (area.browser) {
                const StorylandArchiveBrowser& browser = int(index) == gActiveAreaArchive
                    ? gArchiveBrowser : *area.browser;
                label += L"  |  " + std::to_wstring(browser.worldMeshes().size()) + L" meshes";
                addTreeItem(areasRoot, label, StorylandTreeKind::DtzArea, int(index));
            } else {
                label += area.loadError.empty() ? L"  |  not found" : L"  |  could not load: " + area.loadError;
                addTreeItem(areasRoot, label);
            }
        }
        expandTreeItem(areasRoot);
    } else {
        addTreeItem(root, L"No companion IMG loaded; sector patches only affect GAME.DTZ until gta3PS2.img/gta3PSP.img/GTA3PSPHR.IMG is loaded.");
    }

    HTREEITEM overviewRoot = addTreeItem(root, L"GTAG header overview", StorylandTreeKind::DtzOverview, 0);
    addTreeItem(overviewRoot, L"Core header: signature, size, relocation table, VMT/Jenkins hash tables");
    addTreeItem(overviewRoot, L"gta3PS*.img map: GAME.DTZ runtime records when present; optional DIR supports classic/beta LCS layouts");
    addTreeItem(overviewRoot, L"World/IPL/IDE/streaming pointers: paths, IPL pools, IDE table, CStreamingInfo");
    addTreeItem(overviewRoot, L"Data blocks below are decoded from the file, not placeholder labels");

    HTREEITEM findRoot = nullptr;
    if (!gDtzFindQuery.empty()) {
        std::wstringstream findTitle;
        findTitle << L"Find results for \"" << gDtzFindQuery << L"\" (" << gDtzFindResults.size() << L")";
        findRoot = addTreeItem(root, findTitle.str());
        if (gDtzFindResults.empty()) {
            addTreeItem(findRoot, L"No matches. Find searches DTZ names/fields and texture names inside internal XTX/CHK/TEX/TXD archives.");
        } else {
            for (size_t resultIndex = 0; resultIndex < gDtzFindResults.size(); ++resultIndex) {
                addTreeItem(findRoot, gDtzFindResults[resultIndex].label, StorylandTreeKind::DtzFindResult, int(resultIndex));
            }
        }
        expandTreeItem(findRoot);
    }

    const auto& leeds2dfxEffects = gDtzArchive.leeds2dfxEffects();
    const auto& leeds2dfxWorldInstances = gDtzArchive.leeds2dfxWorldInstances();
    size_t leeds2dfxValidCount = 0;
    size_t leeds2dfxLightCount = 0;
    size_t leeds2dfxAssociatedCount = 0;
    size_t leeds2dfxWorldLightCount = 0;
    for (const auto& effect : leeds2dfxEffects) {
        if (!effect.valid) continue;
        leeds2dfxValidCount++;
        if (effect.isLight) leeds2dfxLightCount++;
        if (effect.hasModelAssociation) leeds2dfxAssociatedCount++;
    }
    for (const auto& instance : leeds2dfxWorldInstances) {
        if (!instance.valid || instance.effectIndex >= leeds2dfxEffects.size()) continue;
        if (leeds2dfxEffects[instance.effectIndex].isLight) leeds2dfxWorldLightCount++;
    }

    std::wstringstream leeds2dfxTitle;
    leeds2dfxTitle << L"Leeds GAME.DTZ native 2DFX  definitions=" << leeds2dfxValidCount
                   << L"  lights=" << leeds2dfxLightCount
                   << L"  model-owned=" << leeds2dfxAssociatedCount
                   << L"  world lights=" << leeds2dfxWorldLightCount;
    HTREEITEM leeds2dfxRoot = addTreeItem(root, leeds2dfxTitle.str());

    HTREEITEM definitionsRoot = addTreeItem(leeds2dfxRoot, L"Global 64-byte C2dEffect definitions");
    if (leeds2dfxValidCount == 0) {
        addTreeItem(definitionsRoot, L"No valid native C2dEffect rows were decoded from GAME.DTZ header fields 0x54/0x58.");
    } else {
        for (size_t effectIndex = 0; effectIndex < leeds2dfxEffects.size(); ++effectIndex) {
            const auto& effect = leeds2dfxEffects[effectIndex];
            if (!effect.valid) continue;
            std::wstringstream line;
            line << L"#" << effect.index
                 << L"  " << leeds2dfxEffectTypeName(effect.effectType)
                 << L"  local=(" << effect.localX << L", " << effect.localY << L", " << effect.localZ << L")"
                 << L"  rgba=(" << int(effect.red) << L", " << int(effect.green) << L", " << int(effect.blue) << L", " << int(effect.alpha) << L")";
            if (effect.isLight) {
                line << L"  outer=" << effect.pointLightRange
                     << L"  size=" << effect.coronaSize
                     << L"  flash=" << int(effect.lightType)
                     << L"  flags=" << hexWide(effect.flags, 2);
            }
            if (effect.hasModelAssociation) {
                line << L"  model=" << effect.modelIndex
                     << L"  hash=" << hexWide(effect.modelHash)
                     << L"  modelEffect=" << effect.modelEffectIndex;
            } else {
                line << L"  model=<unresolved>";
            }
            addTreeItem(definitionsRoot, line.str(), StorylandTreeKind::DtzLeeds2dfx, int(effectIndex));
        }
    }

    std::wstringstream worldTitle;
    worldTitle << L"World placements  lights=" << leeds2dfxWorldLightCount
               << L"  all-effects=" << leeds2dfxWorldInstances.size();
    HTREEITEM worldRoot = addTreeItem(leeds2dfxRoot, worldTitle.str());
    if (leeds2dfxWorldInstances.empty()) {
        addTreeItem(worldRoot, L"No allocated CEntity instances owning native 2DFX were resolved from the three GAME.DTZ pools.");
    } else {
        for (size_t instanceIndex = 0; instanceIndex < leeds2dfxWorldInstances.size(); ++instanceIndex) {
            const auto& instance = leeds2dfxWorldInstances[instanceIndex];
            if (!instance.valid || instance.effectIndex >= leeds2dfxEffects.size()) continue;
            const auto& effect = leeds2dfxEffects[instance.effectIndex];
            if (!effect.isLight) continue;
            std::wstringstream line;
            line << L"#" << instance.index
                 << L"  " << widen(instance.poolName) << L"[" << instance.poolIndex << L"]"
                 << L"  model=" << instance.modelIndex
                 << L"  effect=" << instance.effectIndex
                 << L"  world=(" << instance.worldX << L", " << instance.worldY << L", " << instance.worldZ << L")"
                 << L"  rgba=(" << int(effect.red) << L", " << int(effect.green) << L", " << int(effect.blue) << L", " << int(effect.alpha) << L")";
            addTreeItem(worldRoot, line.str(), StorylandTreeKind::DtzLeeds2dfxWorld, int(instanceIndex));
        }
    }
    HTREEITEM dataRoot = addTreeItem(root, L"GAME.DTZ data editor");
    HTREEITEM editableDataRoot = addTreeItem(dataRoot, L"Editable decoded data (weapons, weather/timecycle, colors, handling, etc.)");
    HTREEITEM rawDataRoot = addTreeItem(dataRoot, L"Raw inferred ranges (advanced / read-only)");
    const auto& dataBlocks = gDtzArchive.dataBlocks();
    const auto& dataFields = gDtzArchive.dataFields();
    if (dataBlocks.empty()) {
        addTreeItem(editableDataRoot, L"No editable DAT-style structures were decoded from this GAME.DTZ.");
        addTreeItem(rawDataRoot, L"No raw GAME.DTZ ranges were decoded.");
    }
    size_t editableBlockCount = 0;
    size_t rawBlockCount = 0;
    for (size_t blockIndex = 0; blockIndex < dataBlocks.size(); ++blockIndex) {
        const auto& block = dataBlocks[blockIndex];
        const bool rawOnly = block.parser == "raw-scanned-header-pointer";
        HTREEITEM parentRoot = rawOnly ? rawDataRoot : editableDataRoot;
        if (rawOnly) rawBlockCount++; else editableBlockCount++;

        std::wstringstream line;
        line << widen(block.name)
             << L"  [" << hexWide(block.offset, 6) << L".." << hexWide(block.inferredEnd, 6) << L"]"
             << L"  " << block.size << L" bytes";
        if (!rawOnly && block.rowSize != 0) line << L"  row=" << block.rowSize;
        if (!rawOnly && block.rowCount != 0) line << L"  count=" << block.rowCount;
        if (block.editable) line << L"  EDITABLE";
        HTREEITEM blockItem = addTreeItem(parentRoot, line.str(), StorylandTreeKind::DtzDataBlock, int(blockIndex));

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
            fieldLine << widen(field.name)
                      << L"  " << widen(field.type)
                      << L" = " << widen(field.valueText)
                      << L"  @" << hexWide(field.absoluteOffset, 6);
            if (field.editable) fieldLine << L"  *";
            addTreeItem(rowItem ? rowItem : blockItem, fieldLine.str(), StorylandTreeKind::DtzDataField, int(fieldIndex));
            fieldsShownForBlock++;
        }
        if (fieldsShownForBlock == 0) {
            addTreeItem(blockItem, L"No named fields. Select this block to inspect its inferred range.");
        }
    }
    if (editableBlockCount == 0 && !dataBlocks.empty()) {
        addTreeItem(editableDataRoot, L"No safely editable decoded blocks were recognized in this GAME.DTZ.");
    }
    if (rawBlockCount == 0 && !dataBlocks.empty()) {
        addTreeItem(rawDataRoot, L"No extra raw inferred ranges.");
    }

    const auto& dirEntries = gDtzArchive.dirEntries();
    if (!dirEntries.empty()) {
        HTREEITEM previewRoot = addTreeItem(root, L"Previewable resources from loaded IMG (MDL/DFF + XTX/CHK/TEX/TXD)");
        HTREEITEM modelRoot = addTreeItem(previewRoot, L"Models shown through the OpenGL MDL viewer");
        HTREEITEM textureRoot = addTreeItem(previewRoot, L"Texture archives shown through the texture viewer");
        size_t modelShown = 0;
        size_t textureShown = 0;
        for (size_t i = 0; i < dirEntries.size(); ++i) {
            const auto& entry = dirEntries[i];
            std::wstring entryName = widenResourceName(entry.name);
            std::wstring resourceName = safeEmbeddedFileName(canonicalDtzImgResourceName(entryName), L"resource.bin");
            std::wstring ext = getExtensionLower(resourceName);
            if (ext == L".mdl" || ext == L".dff" || ext == L".xtx" || ext == L".chk" || ext == L".tex" || ext == L".txd") {
                std::wstringstream line;
                line << entryName
                     << L"  [sector " << entry.startSector << L" +" << entry.sectorCount << L"]"
                     << L"  " << ((uint64_t(entry.sectorCount) * 2048ull + 1023ull) / 1024ull) << L" KiB";
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
        if (textureShown == 0) addTreeItem(textureRoot, L"No .xtx/.chk/.tex/.txd entries were named in the current internal map.");
        expandTreeItem(previewRoot);
    }

    HTREEITEM imgRoot = addTreeItem(root, L"GAME.DTZ + gta3ps*.img directory");
    if (!dirEntries.empty()) {
        for (size_t i = 0; i < dirEntries.size(); ++i) {
            const auto& entry = dirEntries[i];
            std::wstringstream line;
            uint64_t imgBytes = uint64_t(entry.sectorCount) * 2048ull;
            line << widenResourceName(entry.name)
                 << L"  [" << entry.startSector << L"-" << (entry.startSector + entry.sectorCount) << L"]"
                 << L"  +" << entry.sectorCount << L" sectors"
                 << L"  " << ((imgBytes + 1023ull) / 1024ull) << L" KiB";
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
        if (!r.resourceName.empty()) line << widenResourceName(r.resourceName) << L"  ";
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
    expandTreeItem(leeds2dfxRoot);
    expandTreeItem(imgRoot);
    // Keep the GAME.DTZ data editor collapsed on open. Some retail DTZ files
    // expose dozens of inferred ranges; auto-expanding them buried the actual
    // streaming/resource map and made the left pane look corrupted.
    std::wstring status = std::to_wstring(leeds2dfxWorldLightCount) + L" native Leeds world-light instances from " + std::to_wstring(leeds2dfxLightCount) + L" C2dEffect light definitions, " + std::to_wstring(dataBlocks.size()) + L" DAT blocks, " + std::to_wstring(dirEntries.size()) + L" internal streaming entries, " + std::to_wstring(records.size()) + L" raw DTZ records decoded from GAME.DTZ";
    if (gDtzArchive.hasCompanionImg()) {
        status += L"; companion IMG loaded";
        size_t loadedAreas = 0;
        for (const auto& area : gAreaArchives) if (area.browser) ++loadedAreas;
        status += L"; " + std::to_wstring(loadedAreas) + L" area LVZ/IMG pairs loaded";
    }
    status += L".";
    setStatus(status);
    selectDtzOverview();
}

static void returnToDtzFromAreaArchive() {
    if (gActiveAreaArchive < 0) return;
    gMode = StorylandMode::DtzArchive;
    clearDtzFindState(true);
    populateDtzList();
    SetWindowTextW(gMainWindow, L"Storyland - GAME.DTZ + gta3PS*.img + area archives");
    StorylandTitleTint tint = titleTintFromPath(gDtzArchive.sourcePath());
    applyStorylandTitleTint(tint == StorylandTitleTint::Default ? StorylandTitleTint::LCS : tint);
    refreshModeUi();
}


static void selectModelDffGraphNode(int index) {
    const auto& nodes = gModelDffStructureGraph.nodes();
    if (index < 0 || size_t(index) >= nodes.size()) return;

    gSelectedKind = StorylandTreeKind::ModelDffGraphNode;
    gSelectedIndex = index;
    const StorylandAnalysisGraphNode& node = nodes[size_t(index)];

    std::wostringstream ss;
    ss << L"DFF STRUCTURE\r\n\r\n"
       << L"Resource: " << gModelDffStructureName << L"\r\n"
       << L"Node: " << widen(node.label) << L"\r\n"
       << L"Offset: 0x" << std::uppercase << std::hex << node.offset << std::dec << L"\r\n"
       << L"Size: " << node.size << L" bytes";
    if (node.size != 0u) {
        ss << L" (0x" << std::uppercase << std::hex << node.size << std::dec << L")";
    }
    ss << L"\r\n";
    if (node.type != 0u) {
        ss << L"Chunk/plugin ID: 0x" << std::uppercase << std::hex << node.type << std::dec << L"\r\n";
    }
    if (node.version != 0u) {
        ss << L"RenderWare build/version: 0x" << std::uppercase << std::hex << node.version << std::dec << L"\r\n";
    }
    if (!node.description.empty()) {
        ss << L"Description: " << widen(node.description) << L"\r\n";
    }

    ss << L"\r\nDecoded model\r\n"
       << L"Type: " << widen(gModelFile.modelKindName()) << L"\r\n"
       << L"Vertices: " << gModelFile.previewPoints().size() << L"\r\n"
       << L"Triangles: " << gModelFile.previewTriangles().size() << L"\r\n"
       << L"Texcoords: " << gModelFile.previewTexcoords().size() << L"\r\n"
       << L"Frames/HAnim bones: " << gModelFile.armatureBones().size() << L"\r\n";

    size_t weightedVertices = 0u;
    uint64_t influences = 0u;
    for (const StorylandModelSkinWeights& weights : gModelFile.previewSkinWeights()) {
        if (!weights.valid) continue;
        ++weightedVertices;
        influences += std::min<uint32_t>(weights.influenceCount, 4u);
    }
    if (!gModelFile.previewSkinWeights().empty()) {
        ss << L"Skin: " << weightedVertices << L"/" << gModelFile.previewSkinWeights().size()
           << L" weighted vertices, " << influences << L" influences\r\n";
    }

    const auto& materials = gModelFile.previewMaterialTextureNames();
    if (!materials.empty()) {
        ss << L"Materials: " << materials.size() << L"\r\n";
    }

    if (node.offset < gModelDffStructureBytes.size() && node.size != 0u) {
        const uint64_t available =
            std::min<uint64_t>(node.size, gModelDffStructureBytes.size() - size_t(node.offset));
        const size_t dump = size_t(std::min<uint64_t>(available, 256u));
        ss << L"\r\nHEX / ASCII (" << dump;
        if (available > dump) ss << L" of " << available;
        ss << L" bytes)\r\n";

        for (size_t row = 0u; row < dump; row += 16u) {
            ss << std::setw(8) << std::setfill(L'0') << std::hex << (node.offset + row) << L"  ";
            for (size_t column = 0u; column < 16u; ++column) {
                if (row + column < dump) {
                    ss << std::setw(2)
                       << unsigned(gModelDffStructureBytes[size_t(node.offset) + row + column])
                       << L" ";
                } else {
                    ss << L"   ";
                }
            }
            ss << L" ";
            for (size_t column = 0u; column < 16u && row + column < dump; ++column) {
                const unsigned char ch =
                    gModelDffStructureBytes[size_t(node.offset) + row + column];
                ss << wchar_t(ch >= 32u && ch < 127u ? ch : '.');
            }
            ss << L"\r\n";
        }
        ss << std::dec << std::setfill(L' ');
    }

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, FALSE);
}


static void addModelPrelightTree(HTREEITEM parent) {
    const auto& prelights = gModelFile.previewPrelights();
    size_t validCount = 0u;
    for (const StorylandModelPrelight& c : prelights) if (c.valid) ++validCount;
    if (validCount == 0u) return;

    std::wostringstream title;
    title << L"Prelight / vertex colours  " << validCount << L"/" << prelights.size();
    HTREEITEM prelightRoot = addTreeItem(parent, title.str());
    HTREEITEM group = nullptr;
    size_t currentGroup = size_t(-1);
    for (size_t i = 0u; i < prelights.size(); ++i) {
        const StorylandModelPrelight& c = prelights[i];
        if (!c.valid) continue;
        const size_t groupIndex = i / 256u;
        if (groupIndex != currentGroup) {
            currentGroup = groupIndex;
            std::wostringstream groupLabel;
            groupLabel << L"vertices " << groupIndex * 256u << L"-" << (groupIndex * 256u + 255u);
            group = addTreeItem(prelightRoot, groupLabel.str());
        }
        std::wostringstream line;
        line << L"#" << i << L"  RGBA(" << unsigned(c.red) << L", " << unsigned(c.green) << L", " << unsigned(c.blue) << L", " << unsigned(c.alpha) << L")";
        if (c.fileOffset != 0xFFFFFFFFu) line << L"  @ " << hexWide(c.fileOffset, 8);
        addTreeItem(group ? group : prelightRoot, line.str(), StorylandTreeKind::ModelPrelight, int(i));
    }
    expandTreeItem(prelightRoot);
}

static void selectModelPrelight(int index) {
    const auto& prelights = gModelFile.previewPrelights();
    if (index < 0 || size_t(index) >= prelights.size() || !prelights[size_t(index)].valid) return;
    gSelectedKind = StorylandTreeKind::ModelPrelight;
    gSelectedIndex = index;
    gSelectedPrelightVertex = index;
    const StorylandModelPrelight& c = prelights[size_t(index)];
    std::wostringstream ss;
    ss << L"PRELIGHT / VERTEX COLOUR\r\n\r\n"
       << L"Vertex: " << index << L"\r\n"
       << L"RGBA: " << unsigned(c.red) << L", " << unsigned(c.green) << L", " << unsigned(c.blue) << L", " << unsigned(c.alpha) << L"\r\n";
    if (c.fileOffset != 0xFFFFFFFFu) ss << L"Source offset: " << hexWide(c.fileOffset, 8) << L"\r\n";
    ss << L"\r\nDouble-click this row to edit RGB. Alpha is preserved exactly.";
    setDetails(ss.str());
    if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
}

static void editSelectedModelPrelight() {
    if (gSelectedPrelightVertex < 0) return;
    const auto& prelights = gModelFile.previewPrelights();
    if (size_t(gSelectedPrelightVertex) >= prelights.size()) return;
    const StorylandModelPrelight before = prelights[size_t(gSelectedPrelightVertex)];
    if (!before.valid) return;

    static COLORREF customColours[16] = {};
    CHOOSECOLORW picker = {};
    picker.lStructSize = sizeof(picker);
    picker.hwndOwner = gMainWindow;
    picker.rgbResult = RGB(before.red, before.green, before.blue);
    picker.lpCustColors = customColours;
    picker.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&picker)) return;

    std::string error;
    if (!gModelFile.setPreviewPrelightColor(
            size_t(gSelectedPrelightVertex),
            GetRValue(picker.rgbResult), GetGValue(picker.rgbResult), GetBValue(picker.rgbResult), before.alpha,
            error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Prelight edit failed", MB_OK | MB_ICONERROR);
        return;
    }
    const auto& updated = gModelFile.previewPrelights();
    if (size_t(gSelectedPrelightVertex) < updated.size()) {
        const StorylandModelPrelight& c = updated[size_t(gSelectedPrelightVertex)];
        HTREEITEM selected = gTree ? TreeView_GetSelection(gTree) : nullptr;
        if (selected) {
            std::wostringstream label;
            label << L"#" << gSelectedPrelightVertex << L"  RGBA(" << unsigned(c.red) << L", " << unsigned(c.green) << L", " << unsigned(c.blue) << L", " << unsigned(c.alpha) << L")";
            if (c.fileOffset != 0xFFFFFFFFu) label << L"  @ " << hexWide(c.fileOffset, 8);
            std::wstring labelText = label.str();
            TVITEMW item = {};
            item.mask = TVIF_TEXT; item.hItem = selected; item.pszText = labelText.data();
            TreeView_SetItem(gTree, &item);
        }
        selectModelPrelight(gSelectedPrelightVertex);
    }
    setStatus(L"Prelight vertex colour edited in the model byte stream. Export to save the change.");
}

static bool populateModelDffStructureTree() {
    if (!gModelFile.isPspNativeDff() && !gModelFile.isMobileLcsDff()) {
        std::wstring extension = std::filesystem::path(gModelFile.sourcePath()).extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t ch) {
            return wchar_t(std::towlower(ch));
        });
        if (extension != L".dff") return false;
    }

    gModelDffStructureBytes = gModelFile.rawBytes();
    gModelDffStructureName = gModelFile.sourcePath();
    if (gModelDffStructureBytes.empty()) return false;

    std::string error;
    const std::string displayName = narrow(gModelDffStructureName);
    if (!gModelDffStructureGraph.build(
            gModelDffStructureBytes, ".dff",
            displayName.empty() ? "model.dff" : displayName,
            error)) {
        gModelDffStructureTreeActive = false;
        return false;
    }

    gModelDffStructureTreeActive = true;
    const auto& nodes = gModelDffStructureGraph.nodes();
    std::vector<HTREEITEM> treeItems(nodes.size(), nullptr);

    for (size_t i = 0u; i < nodes.size(); ++i) {
        const StorylandAnalysisGraphNode& node = nodes[i];
        HTREEITEM parent = TVI_ROOT;
        if (node.parent >= 0 &&
            size_t(node.parent) < treeItems.size() &&
            treeItems[size_t(node.parent)] != nullptr) {
            parent = treeItems[size_t(node.parent)];
        }
        treeItems[i] = addTreeItem(
            parent,
            widen(node.label),
            StorylandTreeKind::ModelDffGraphNode,
            int(i));
    }

    if (!treeItems.empty() && treeItems[0]) {
        addModelPrelightTree(treeItems[0]);
        TreeView_Expand(gTree, treeItems[0], TVE_EXPAND);

        // Match the useful RW Analyze presentation: expose the immediate
        // FrameList/GeometryList/Atomic branches without exploding every leaf.
        HTREEITEM child = TreeView_GetChild(gTree, treeItems[0]);
        while (child) {
            TreeView_Expand(gTree, child, TVE_EXPAND);
            child = TreeView_GetNextSibling(gTree, child);
        }

        TreeView_SelectItem(gTree, treeItems[0]);
        selectModelDffGraphNode(0);
    }

    setStatus(
        L"DFF structure tree | " +
        widen(gModelDffStructureGraph.summary()) +
        L" | select any chunk/plugin to inspect its bytes");
    return true;
}

static void populateModelList() {
    clearView();
    gModelDffStructureTreeActive = false;
    gModelDffStructureBytes.clear();
    gModelDffStructureName.clear();

    if (populateModelDffStructureTree()) {
        return;
    }

    const wchar_t* modelRootLabel =
        gModelFile.isMobileLcsDff() ? L"Mobile LCS RenderWare DFF" :
        gModelFile.isPspNativeDff() ? L"LCS PSP RenderWare DFF" :
                                      L"MDL / Leeds model";
    HTREEITEM root = addTreeItem(TVI_ROOT, modelRootLabel);
    std::wstringstream summary;
    summary << L"Detected ";
    if (gModelFile.isMobileLcsDff()) summary << L"Mobile LCS RenderWare 3.1 DFF / ";
    else if (gModelFile.isPspNativeDff()) summary << L"LCS PSP beta RenderWare DFF / ";
    summary << widen(gModelFile.modelKindName()) << L"  size=" << gModelFile.fileSize() << L" bytes";
    addTreeItem(root, summary.str());
    addModelPrelightTree(root);

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

    const auto& modelLights = gModelFile.preview2dfxLights();
    if (!modelLights.empty()) {
        std::wstringstream lightTitle;
        lightTitle << L"RenderWare DFF-plugin 2DFX corona lights  count=" << modelLights.size();
        HTREEITEM lightsRoot = addTreeItem(root, lightTitle.str());
        for (size_t i = 0; i < modelLights.size(); ++i) {
            const auto& light = modelLights[i];
            std::wstringstream line;
            line << L"#" << i << L" pos=(" << light.position.x << L", " << light.position.y << L", " << light.position.z << L")"
                 << L" rgba=(" << int(light.red) << L", " << int(light.green) << L", " << int(light.blue) << L", " << int(light.alpha) << L")"
                 << L" corona=" << light.coronaSize << L" range=" << light.pointLightRange;
            if (!light.coronaTextureName.empty()) line << L" tex=" << widen(light.coronaTextureName);
            addTreeItem(lightsRoot, line.str());
        }
        expandTreeItem(lightsRoot);
    }

    if (gModelAnimLoaded) {
        std::wstringstream animTitle;
        animTitle << L"Loaded .anim applied to this MDL  tracks=" << gAnimFile.tracks().size();
        HTREEITEM animRoot = addTreeItem(root, animTitle.str(), StorylandTreeKind::AnimOverview, 0);

        const auto& animClips = gAnimFile.clips();
        for (const auto& clip : animClips) {
            std::wstringstream clipLine;
            clipLine << L"clip #" << clip.index << L"  " << widen(clip.name);
            if (gAnimFile.activeClipIndex() < gAnimFile.clips().size() &&
            clip.index == gAnimFile.activeClipIndex()) clipLine << L"  [ACTIVE]";
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
    return ext == ".xtx" || ext == ".chk" || ext == ".tex" || ext == ".txd";
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
    line << archiveDisplayName(entry, entryIndex)
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

    if (gActiveAreaArchive >= 0) {
        HTREEITEM gameRoot = addTreeItem(TVI_ROOT, L"GAME.DTZ + area archives");
        addTreeItem(gameRoot, L"Back to GAME.DTZ", StorylandTreeKind::DtzAreaReturn, 0);
        for (size_t areaIndex = 0; areaIndex < gAreaArchives.size(); ++areaIndex) {
            if (gAreaArchives[areaIndex].browser || int(areaIndex) == gActiveAreaArchive) {
                const std::wstring label = gAreaArchives[areaIndex].name + L".LVZ + IMG" +
                    (int(areaIndex) == gActiveAreaArchive ? L"  [current]" : L"");
                addTreeItem(gameRoot, label, StorylandTreeKind::DtzArea, int(areaIndex));
            }
        }
        expandTreeItem(gameRoot);
    }
    const wchar_t* archiveRootLabel = gArchiveBrowser.hasLvzContext() ? L"LVZ + IMG archive browse" :
        gArchiveBrowser.isPs2StoriesRawImg() ? L"PS2 Stories raw IMG diagnostic browse" :
        gArchiveBrowser.hasClassicDirContext() ? L"Classic IMG + DIR browse" :
        L"Mobile LCS raw gta3.img browse";
    HTREEITEM root = addTreeItem(TVI_ROOT, archiveRootLabel);
    if (gArchiveBrowser.hasLvzContext()) addTreeItem(root, L"LVZ: " + gArchiveBrowser.lvzPath());
    if (gArchiveBrowser.hasImgContext()) addTreeItem(root, L"IMG: " + gArchiveBrowser.imgPath());
    if (gArchiveBrowser.hasLvzContext()) addTreeItem(root, L"No .DIR: retail LVZ+IMG uses LVZ chunk headers + IMG payloads.");
    if (!gArchiveBrowser.levelSummary().empty()) addTreeItem(root, widen(gArchiveBrowser.levelSummary()));

    if (!gArchiveBrowser.hasLvzContext()) {
        const auto& entries = gArchiveBrowser.entries();

        if (gArchiveBrowser.isPs2StoriesRawImg()) {
            HTREEITEM xtxRoot = addTreeItem(root, L"PS2 Stories XTX candidates");
            HTREEITEM mdlRoot = addTreeItem(root, L"PS2 Stories MDL candidates");
            HTREEITEM animRoot = addTreeItem(root, L"PS2 Stories ANIM candidates");
            HTREEITEM colRoot = addTreeItem(root, L"PS2 Stories COL2 candidates");
            size_t xtxCount = 0, mdlCount = 0, animCount = 0, colCount = 0;
            for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex) {
                const auto& entry = entries[entryIndex];
                if (entry.chunkIdent == 0x00746578u) { addArchiveEntryTreeItem(xtxRoot, entry, entryIndex); ++xtxCount; }
                else if (entry.chunkIdent == 0x006d646cu) { addArchiveEntryTreeItem(mdlRoot, entry, entryIndex); ++mdlCount; }
                else if (entry.chunkIdent == 0x6d696e61u) { addArchiveEntryTreeItem(animRoot, entry, entryIndex); ++animCount; }
                else if (entry.chunkIdent == 0x636f6c32u) { addArchiveEntryTreeItem(colRoot, entry, entryIndex); ++colCount; }
            }
            std::wstringstream details;
            details << L"PS2 GTA Stories raw IMG diagnostic browse\r\n\r\n"
                    << L"IMG: " << gArchiveBrowser.imgPath() << L"\r\n"
                    << L"IMG bytes: " << gArchiveBrowser.imgFileSize() << L"\r\n"
                    << L"XTX candidate starts: " << xtxCount << L"\r\n"
                    << L"MDL candidate starts: " << mdlCount << L"\r\n"
                    << L"ANIM candidate starts: " << animCount << L"\r\n"
                    << L"COL2 candidate starts: " << colCount << L"\r\n\r\n"
                    << L"This is not being identified as Mobile LCS. Storyland found the sector-aligned Leeds PS2 MDL/XTX population used by LCS/VCS PS2.\r\n"
                    << L"Without GAME.DTZ or an LVZ, names and exact allocation lengths are not authoritative. Candidate ranges are inferred only for inspection; do not use them for replacement.\r\n"
                    << L"Put the matching GAME.DTZ beside gta3PS2.img to recover the retail streaming table and exact names/sectors.\r\n";
            setDetails(details.str());
            expandTreeItem(root);
            expandTreeItem(mdlRoot);
            gSelectedIndex = -1;
            gSelectedKind = StorylandTreeKind::None;
            InvalidateRect(gPreview, nullptr, TRUE);
            setStatus(L"PS2 Stories raw IMG detected; GAME.DTZ/LVZ metadata is required for authoritative indexing.");
            return;
        }

        if (gArchiveBrowser.hasClassicDirContext()) {
            HTREEITEM entriesRoot = addTreeItem(root, L"Classic IMG directory entries");
            for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex) {
                addArchiveEntryTreeItem(entriesRoot, entries[entryIndex], entryIndex);
            }
            std::wstringstream details;
            details << L"Classic GTA IMG + DIR archive\r\n\r\n"
                    << L"IMG: " << gArchiveBrowser.imgPath() << L"\r\n"
                    << L"IMG bytes: " << gArchiveBrowser.imgFileSize() << L"\r\n"
                    << L"Directory entries: " << entries.size() << L"\r\n\r\n"
                    << L"Storyland only uses a same-stem DIR automatically. GTA3.DIR is accepted as a fallback only for gta3.img/GTA3PSPHR.IMG, so an unrelated GTA3.DIR cannot hijack gta3PS2.img.\r\n";
            setDetails(details.str());
            expandTreeItem(root);
            expandTreeItem(entriesRoot);
            gSelectedIndex = -1;
            gSelectedKind = StorylandTreeKind::None;
            InvalidateRect(gPreview, nullptr, TRUE);
            setStatus(std::to_wstring(entries.size()) + L" classic IMG directory entries loaded.");
            return;
        }

        // Mobile LCS raw RenderWare archive.  This branch is entered only after
        // PS2 Stories MDL/XTX classification has been ruled out.
        HTREEITEM modelRoot = addTreeItem(root, L"Mobile LCS RenderWare DFF clumps");
        HTREEITEM textureRoot = addTreeItem(root, L"Mobile LCS RenderWare PSP texture dictionaries");
        size_t modelCount = 0u;
        size_t textureDictionaryCount = 0u;
        size_t namedTextureCount = 0u;
        for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex) {
            if (entries[entryIndex].chunkIdent == 0x10u) {
                addArchiveEntryTreeItem(modelRoot, entries[entryIndex], entryIndex);
                modelCount++;
            } else if (entries[entryIndex].chunkIdent == 0x16u) {
                addArchiveEntryTreeItem(textureRoot, entries[entryIndex], entryIndex);
                textureDictionaryCount++;
                namedTextureCount += entries[entryIndex].textureNames.size();
            }
        }
        if (modelCount == 0u) addTreeItem(modelRoot, L"No sector-aligned Mobile LCS DFF clumps were found.");
        if (textureDictionaryCount == 0u) addTreeItem(textureRoot, L"No sector-aligned Mobile LCS PSP texture dictionaries were found.");

        std::wstringstream details;
        details << L"Mobile LCS raw gta3.img archive\r\n\r\n"
                << L"IMG: " << gArchiveBrowser.imgPath() << L"\r\n"
                << L"IMG bytes: " << gArchiveBrowser.imgFileSize() << L"\r\n"
                << L"Mobile LCS DFF clumps: " << modelCount << L"\r\n"
                << L"Mobile LCS texture dictionaries: " << textureDictionaryCount << L"\r\n"
                << L"Named embedded textures: " << namedTextureCount << L"\r\n\r\n"
                << L"This classification is used only after the PS2 Stories MDL/XTX probe and classic DIR probe fail.\r\n"
                << L"Double-click a DFF to open its Mobile LCS native geometry and matching embedded texture dictionary.\r\n";
        setDetails(details.str());
        expandTreeItem(root);
        expandTreeItem(modelRoot);
        gSelectedIndex = -1;
        gSelectedKind = StorylandTreeKind::None;
        InvalidateRect(gPreview, nullptr, TRUE);
        setStatus(std::to_wstring(modelCount) + L" Mobile LCS DFFs and " + std::to_wstring(textureDictionaryCount) + L" embedded TXDs found.");
        return;
    }

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
        if (resolution.source == "same-sector" || resolution.source == "same-sector verified") {
            parent = resolvedRoot;
            sameSectorCount++;
        }
        else if (resolution.source == "unique linked sector" ||
                 resolution.source == "unique linked sector verified" ||
                 resolution.source == "same-row" ||
                 resolution.source == "same-row verified" ||
                 resolution.source == "official AERA" ||
                 resolution.source == "master LVZ" ||
                 resolution.source == "placement-fit exact RES" ||
                 resolution.source == "IMG continuation") {
            parent = linkedRoot;
            linkedCount++;
        }
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
    if (!gArchiveBrowser.hasLvzContext()) {
        if (gArchiveBrowser.isPs2StoriesRawImg()) {
            std::wstringstream ss;
            ss << L"PS2 GTA Stories raw IMG diagnostic browse\r\n\r\n"
               << L"IMG: " << gArchiveBrowser.imgPath() << L"\r\n"
               << L"IMG bytes: " << gArchiveBrowser.imgFileSize() << L"\r\n\r\n"
               << widen(gArchiveBrowser.levelSummary()) << L"\r\n\r\n"
               << L"This raw archive is not Mobile LCS. GAME.DTZ or LVZ metadata is required before Storyland treats sector allocations and resource names as authoritative.\r\n";
            setDetails(ss.str());
            InvalidateRect(gPreview, nullptr, TRUE);
            return;
        }
        if (gArchiveBrowser.hasClassicDirContext()) {
            std::wstringstream ss;
            ss << L"Classic GTA IMG + DIR archive\r\n\r\n"
               << L"IMG: " << gArchiveBrowser.imgPath() << L"\r\n"
               << L"IMG bytes: " << gArchiveBrowser.imgFileSize() << L"\r\n"
               << L"Directory entries: " << entries.size() << L"\r\n";
            setDetails(ss.str());
            InvalidateRect(gPreview, nullptr, TRUE);
            return;
        }
        size_t modelCount = 0u;
        size_t textureDictionaryCount = 0u;
        size_t namedTextureCount = 0u;
        for (const auto& entry : entries) {
            if (entry.chunkIdent == 0x10u) modelCount++;
            else if (entry.chunkIdent == 0x16u) {
                textureDictionaryCount++;
                namedTextureCount += entry.textureNames.size();
            }
        }
        std::wstringstream ss;
        ss << L"Mobile LCS raw gta3.img archive\r\n\r\n"
           << L"IMG: " << gArchiveBrowser.imgPath() << L"\r\n"
           << L"IMG bytes: " << gArchiveBrowser.imgFileSize() << L"\r\n"
           << L"Sector-aligned Mobile LCS DFF clumps: " << modelCount << L"\r\n"
           << L"Sector-aligned Mobile LCS texture dictionaries: " << textureDictionaryCount << L"\r\n"
           << L"Named embedded textures: " << namedTextureCount << L"\r\n";
        setDetails(ss.str());
        InvalidateRect(gPreview, nullptr, TRUE);
        return;
    }

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
       << L"Name: " << archiveDisplayName(entry, size_t(index)) << L"\r\n"
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
        ss << L"\r\nPreview: double-click this model to open it in the MDL viewer. A matching embedded Mobile LCS TXD is extracted and attached automatically when one is found.\r\n";
    } else if (archiveEntryIsTextureArchive(entry)) {
        if (!entry.textureNames.empty()) {
            ss << L"\r\nEmbedded textures: " << entry.textureNames.size() << L"\r\n";
            const size_t visibleTextureCount = std::min<size_t>(entry.textureNames.size(), 64u);
            for (size_t textureIndex = 0u; textureIndex < visibleTextureCount; ++textureIndex) {
                ss << L"  " << widen(entry.textureNames[textureIndex]) << L"\r\n";
            }
            if (visibleTextureCount < entry.textureNames.size()) {
                ss << L"  ... " << (entry.textureNames.size() - visibleTextureCount) << L" more\r\n";
            }
        }
        ss << L"\r\nPreview: double-click this Mobile LCS PSP texture dictionary to open it in the texture viewer.\r\n";
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
       << L"The OpenGL viewport remains on the full map. Every placed instance of this resource is highlighted red without opening or isolating its WRLD sector.\r\n"
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
        ss << L"Name: " << widenResourceName(entry.name) << L"\r\n"
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
    gAnimLastTick = GetTickCount();
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
        if (gAnimFile.activeClipIndex() < gAnimFile.clips().size() &&
            clip.index == gAnimFile.activeClipIndex()) line << L"  [ACTIVE]";
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

    StorylandQuat qa = storylandQuatFromAnimXyzw(a->qx, a->qy, a->qz, a->qw);
    StorylandQuat qb = storylandQuatFromAnimXyzw(b->qx, b->qy, b->qz, b->qw);

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
    StorylandQuat baseRotation = storylandQuatFromAnimXyzw(first.qx, first.qy, first.qz, first.qw);
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

    std::vector<StorylandModelPoint> bindRawPositions;
    bindRawPositions.reserve(bones.size());
    for (const auto& bone : bones) bindRawPositions.push_back(rawBoneBindPosition(bone));

    std::vector<StorylandModelPoint> animatedRaw = bindRawPositions;
    gLastAnimatedBoneRawPositions = animatedRaw;

    std::vector<StorylandQuat> bindWorldRotations(bones.size());
    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const StorylandModelBone& bone = bones[boneIndex];
        StorylandQuat localBindRotation = modelBoneBindLocalRotation(bone);
        StorylandQuat bindWorldRotation = localBindRotation;

        if (bone.hasWorldRotation) {
            bindWorldRotation = modelBoneBindWorldRotation(bone);
        } else if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < boneIndex) {
            bindWorldRotation = multiplyStorylandQuat(
                bindWorldRotations[size_t(bone.parentIndex)],
                localBindRotation);
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

    // Match the corrected BLeeds ANIM solver: every Leeds key is an ABSOLUTE
    // local MDL pose.  Build the target hierarchy from those local poses, then
    // derive a world-space skin delta against the MDL bind hierarchy.  Do not
    // clamp rotations, invent frame-zero deltas, freeze unkeyed limbs, or scale
    // translation channels.  An unkeyed child remains at its bind-local pose
    // and therefore follows its animated parent naturally.
    std::vector<StorylandQuat> targetWorldRotations(bones.size());
    std::vector<StorylandModelPoint> targetWorldPositions = bindRawPositions;

    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        const StorylandModelBone& bone = bones[boneIndex];
        const bool hasParent = bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex < boneIndex;
        const size_t parentIndex = hasParent ? size_t(bone.parentIndex) : 0u;

        StorylandQuat localPoseRotation = modelBoneBindLocalRotation(bone);
        StorylandModelPoint localPoseTranslation{};

        if (bone.hasLocalPosition) {
            localPoseTranslation = bone.localPosition;
        } else if (hasParent) {
            // Recover local bind translation from the imported world bind when
            // the MDL parser did not expose a direct local position field.
            StorylandModelPoint worldOffset = subtractStorylandPoint(
                bindRawPositions[boneIndex], bindRawPositions[parentIndex]);
            localPoseTranslation = rotatePointByQuat(
                conjugateStorylandQuat(bindWorldRotations[parentIndex]), worldOffset);
        } else {
            localPoseTranslation = bindRawPositions[boneIndex];
        }

        const StorylandAnimTrack* track = findAnimTrackForModelBone(boneIndex, bone);
        if (track) {
            StorylandQuat sampledRotation{};
            StorylandModelPoint sampledTranslation{};
            if (sampleAnimTrackTransform(*track, gAnimCurrentTime, sampledRotation, sampledTranslation)) {
                if ((track->channelFlags & 0x0001u) != 0u) {
                    localPoseRotation = normalizeStorylandQuat(sampledRotation);
                }
                if ((track->channelFlags & 0x0002u) != 0u && !animPreviewShouldLockRootTranslation(bone)) {
                    localPoseTranslation = sampledTranslation;
                }
            }
        }

        localPoseRotation = normalizeStorylandQuat(localPoseRotation);

        if (hasParent) {
            targetWorldRotations[boneIndex] = normalizeStorylandQuat(
                multiplyStorylandQuat(targetWorldRotations[parentIndex], localPoseRotation));
            targetWorldPositions[boneIndex] = addStorylandPoint(
                targetWorldPositions[parentIndex],
                rotatePointByQuat(targetWorldRotations[parentIndex], localPoseTranslation));
        } else {
            targetWorldRotations[boneIndex] = localPoseRotation;
            // BLeeds locks only root translation by default; root rotation is
            // still animation-driven. Preserve the imported bind translation
            // unless the caller explicitly exposes root motion later.
            targetWorldPositions[boneIndex] = animPreviewShouldLockRootTranslation(bone)
                ? bindRawPositions[boneIndex]
                : localPoseTranslation;
        }
    }

    gLastAnimatedBoneWorldRotations.resize(bones.size());
    for (size_t boneIndex = 0; boneIndex < bones.size(); ++boneIndex) {
        gLastAnimatedBoneWorldRotations[boneIndex] = normalizeStorylandQuat(
            multiplyStorylandQuat(
                targetWorldRotations[boneIndex],
                conjugateStorylandQuat(bindWorldRotations[boneIndex])));
    }

    animatedRaw = targetWorldPositions;
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
    // Custom BLeeds-exported PEDs use the same absolute-local Leeds hierarchy
    // solver as retail PEDs.  The old duplicate path had its own rotation clamps
    // and 0.15 translation scaling, which is exactly the kind of divergence that
    // made Storyland visibly disagree with BLeeds.
    return buildAnimatedModelBonePositions();
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

static StorylandModelPoint transformRawPointByWeightedBone(
    const StorylandModelPoint& rawPoint,
    const StorylandModelPoint& bindRawBonePosition,
    const StorylandModelPoint& animatedRawBonePosition,
    const StorylandQuat& animatedBoneWorldDeltaRotation
) {
    // Leeds ANIM rotations and imported MDL bind rotations live in MDL/raw
    // coordinate space. Do the actual skin rotation there. Applying this
    // quaternion directly to Storyland's fitted preview-space point is wrong
    // whenever preview-space includes an axis swap or non-uniform fit.
    StorylandModelPoint localPoint = subtractStorylandPoint(rawPoint, bindRawBonePosition);
    StorylandModelPoint rotatedLocalPoint = rotatePointByQuat(animatedBoneWorldDeltaRotation, localPoint);
    return addStorylandPoint(animatedRawBonePosition, rotatedLocalPoint);
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

    // VCS/retail Leeds PED preview points are fitted into Storyland display
    // space. The fitted space is not guaranteed to share the MDL's rotation
    // basis (and can be non-uniformly scaled), so raw Leeds quaternions must
    // never be applied directly to those display-space points. Build one
    // invertible raw<->preview fit and skin in raw MDL space first.
    StorylandPreviewBindAffine rawSkinAffine;
    const bool rawSkinAffineValid = computePreviewBindAffine(sourcePoints, rawSkinAffine);
    std::vector<StorylandModelPoint> bindRawBonePositions;
    bindRawBonePositions.reserve(bones.size());
    for (const StorylandModelBone& bone : bones) {
        bindRawBonePositions.push_back(rawBoneBindPosition(bone));
    }
    const bool rawSpaceSkinAvailable =
        rawSkinAffineValid &&
        bindRawBonePositions.size() == bones.size() &&
        gLastAnimatedBoneRawPositions.size() == bones.size();

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

                if (rawSpaceSkinAvailable) {
                    const StorylandModelPoint rawPoint = previewPointToRawBindSpace(rawSkinAffine, previewPoint);
                    const StorylandModelPoint transformedRaw = transformRawPointByWeightedBone(
                        rawPoint,
                        bindRawBonePositions[pivotBoneIndex],
                        gLastAnimatedBoneRawPositions[pivotBoneIndex],
                        gLastAnimatedBoneWorldRotations[modelBoneIndex]
                    );
                    transformedPreview = rawBindPointToPreviewSpace(rawSkinAffine, transformedRaw);
                } else {
                    transformedPreview = transformPreviewPointByWeightedBone(
                        previewPoint,
                        bones[pivotBoneIndex],
                        animatedPreviewBones[pivotBoneIndex],
                        gLastAnimatedBoneWorldRotations[modelBoneIndex]
                    );
                }
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
        float bestDistanceSq = (std::numeric_limits<float>::max)();
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
    if (gMode == StorylandMode::AnimFile && gSelectedKind == StorylandTreeKind::AnimOverview) selectAnimOverview();
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
    if (payload.kind == StorylandTreeKind::DtzAreaReturn) returnToDtzFromAreaArchive();
    else if (payload.kind == StorylandTreeKind::DtzArea) activateAreaArchive(payload.index);
    else if (payload.kind == StorylandTreeKind::ArchiveEntry) selectArchiveEntry(payload.index);
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
    uint8_t minAlpha = 255u, maxAlpha = 0u;
    bool anyAlpha = false;
    for (size_t p = 3u; p < gCurrentImage.rgba.size(); p += 4u) {
        minAlpha = std::min<uint8_t>(minAlpha, gCurrentImage.rgba[p]);
        maxAlpha = std::max<uint8_t>(maxAlpha, gCurrentImage.rgba[p]);
        if (gCurrentImage.rgba[p] != 255u) anyAlpha = true;
    }
    std::wstringstream ss;
    ss << L"Texture: " << widen(e.name) << L"\r\n"
       << L"Kind: " << (e.kind == TextureKind::CtwTex ? L"CTW TEX" :
                           e.kind == TextureKind::Dds ? L"DDS" :
                           e.kind == TextureKind::Ps2 ? L"PS2" :
                           e.kind == TextureKind::Psp ? L"PSP" :
                           e.kind == TextureKind::RwPsp ? L"PSP RenderWare" : L"Unknown") << L"\r\n"
       << L"Size: " << e.width << L" x " << e.height << L"\r\n"
       << L"BPP: " << int(e.bpp) << L"\r\n"
       << L"Mip count: " << int(e.mipCount) << L"\r\n"
       << L"Swizzle field: " << int(e.swizzleMask) << (e.swizzleMask ? L" (encoded/swizzled)" : L" (linear)") << L"\r\n"
       << L"PS2 flags: " << hexWide(e.flags, 8) << L"\r\n"
       << L"Alpha: " << (anyAlpha ? L"present" : L"opaque") << L"  range=" << int(minAlpha) << L".." << int(maxAlpha) << L"\r\n"
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
    if (gDtzArchive.findDirEntryByStemAndExtension(modelStem, {L".xtx", L".chk", L".tex", L".txd"}, sameStemIndex)) {
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
        if (!(candidateExt == L".xtx" || candidateExt == L".chk" || candidateExt == L".tex" || candidateExt == L".txd")) continue;
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
    std::wstring entryName = widenResourceName(entry.name);
    std::wstring resourceName = safeEmbeddedFileName(canonicalDtzImgResourceName(entryName), L"resource.bin");
    std::wstring entryExt = getExtensionLower(resourceName);
    bool previewableTexture = (entryExt == L".xtx" || entryExt == L".chk" || entryExt == L".tex" || entryExt == L".txd");
    bool previewableModel = (entryExt == L".mdl" || entryExt == L".dff");
    if (!previewableTexture && !previewableModel) {
        previewSummary = L"Preview not wired yet for this resource type. Live DTZ+IMG preview supports .mdl/.dff and .chk/.xtx/.tex/.txd entries first.";
        return false;
    }
    if (!gDtzArchive.hasCompanionImg()) {
        previewSummary = L"Preview unavailable: no companion gta3PS2.img/gta3PSP.img/GTA3PSPHR.IMG is loaded for this GAME.DTZ.";
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
        std::wstring clampSummary;
        if (!clampEditableStoriesTexturesTo8Bpp(clampSummary, error)) {
            previewSummary = L"Texture preview BPP conversion failed: " + widen(error);
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
           << L"Decoded textures in archive: " << gTextureArchive.textures().size() << L"\r\n";
        if (!clampSummary.empty()) {
            ss << L"BPP conversion: " << clampSummary << L"\r\n";
        }
        ss << L"Showing texture 0: " << widen(firstTexture.name) << L"  " << firstTexture.width << L"x" << firstTexture.height << L"  bpp=" << int(firstTexture.bpp);
        previewSummary = ss.str();
        return true;
    }

    size_t companionTextureIndex = 0;
    std::wstring companionTextureReason;
    std::wstring extractedCompanionTexturePath;
    if (findBestDtzModelCompanionTextureIndex(size_t(index), companionTextureIndex, companionTextureReason)) {
        std::vector<uint8_t> textureBytes;
        if (gDtzArchive.extractDirEntryBytes(companionTextureIndex, textureBytes, error)) {
            std::wstring companionTextureName = safeEmbeddedFileName(
                canonicalDtzImgResourceName(widen(gDtzArchive.dirEntries()[companionTextureIndex].name)), L"texture.xtx");
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

    gModelTextureVAuto = false;
    gModelFlipTextureV = false;
    gModelDetectedFlipTextureV = false;
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
       << L"Texture status: " << textureStatus << L"\r\n"
       << L"Flip V coordinate: " << (gModelFlipTextureV ? L"on" : L"off") << L"\r\n";
    if (!extractedCompanionTexturePath.empty()) {
        ss << L"DTZ companion texture: " << extractedCompanionTexturePath << L" (" << companionTextureReason << L")\r\n";
    } else {
        ss << L"DTZ companion texture: none extracted for this model entry.\r\n";
    }
    ss << L"Viewport controls: LMB rotate, RMB pan, wheel zoom, double-click reset.";
    previewSummary = ss.str();
    return true;
}

static void openDtzDirEntryStandaloneByIndex(int entryIndex, int preferredTextureIndex) {
    if (gMode != StorylandMode::DtzArchive || entryIndex < 0) {
        MessageBoxW(gMainWindow, L"Select a GAME.DTZ internal IMG entry first.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    const auto& entries = gDtzArchive.dirEntries();
    if (size_t(entryIndex) >= entries.size()) return;
    const auto& entry = entries[size_t(entryIndex)];
    std::wstring entryName = widenResourceName(entry.name);
    std::wstring resourceName = safeEmbeddedFileName(canonicalDtzImgResourceName(entryName), L"resource.bin");
    std::wstring entryExt = getExtensionLower(resourceName);
    if (!(entryExt == L".mdl" || entryExt == L".dff" || entryExt == L".xtx" || entryExt == L".chk" || entryExt == L".tex" || entryExt == L".txd" || entryExt == L".dtz")) {
        setStatus(L"Selected DTZ+IMG entry is not a safely enterable resource. Raw/unknown data stays browse-only.");
        return;
    }

    std::vector<uint8_t> bytes;
    std::string error;
    if (!gDtzArchive.extractDirEntryBytes(size_t(entryIndex), bytes, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"DTZ internal extract failed", MB_ICONERROR);
        return;
    }

    std::wstring extractRoot = buildDtzPreviewEntryExtractRoot(entryIndex);
    std::wstring primaryPath = extractRoot + L"\\" + resourceName;
    if (!writeWholeFileBinary(primaryPath, bytes, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"DTZ internal extract failed", MB_ICONERROR);
        return;
    }

    if (entryExt == L".mdl" || entryExt == L".dff") {
        size_t companionIndex = 0;
        std::wstring companionReason;
        if (findBestDtzModelCompanionTextureIndex(size_t(entryIndex), companionIndex, companionReason)) {
            std::vector<uint8_t> textureBytes;
            if (gDtzArchive.extractDirEntryBytes(companionIndex, textureBytes, error)) {
                std::wstring companionName = safeEmbeddedFileName(
                    canonicalDtzImgResourceName(widen(gDtzArchive.dirEntries()[companionIndex].name)), L"texture.xtx");
                std::wstring companionPath = extractRoot + L"\\" + companionName;
                writeWholeFileBinary(companionPath, textureBytes, error);
            }
        }
    }

    const bool returnableChild = entryExt == L".mdl" || entryExt == L".dff" || isDtzTextureArchiveExtension(entryExt);
    if (returnableChild) {
        gDtzReturnAvailable = true;
        gDtzReturnSelectedIndex = entryIndex;
        gDtzReturnTreeKind = gSelectedKind;
        gDtzReturnTreeIndex = gSelectedIndex;
        gDtzReturnTint = gTitleTint;
    }

    gOpeningDtzStandaloneChild = returnableChild;
    openStorylandFile(primaryPath);
    gOpeningDtzStandaloneChild = false;

    if (returnableChild && gMode != StorylandMode::ModelFile && gMode != StorylandMode::TextureArchive) {
        gDtzReturnAvailable = false;
        gDtzReturnSelectedIndex = -1;
        gDtzReturnTreeKind = StorylandTreeKind::None;
        gDtzReturnTreeIndex = -1;
    }

    if (returnableChild && preferredTextureIndex >= 0 && gMode == StorylandMode::TextureArchive &&
        size_t(preferredTextureIndex) < gTextureArchive.textures().size()) {
        selectTreePayloadItem(StorylandTreeKind::Texture, preferredTextureIndex);
        selectTexture(preferredTextureIndex);
        setStatus(L"Opened texture match from GAME.DTZ | Back to GAME.DTZ returns to the archive browser.");
    } else if (returnableChild && (gMode == StorylandMode::ModelFile || gMode == StorylandMode::TextureArchive)) {
        setStatus(L"Opened internal GAME.DTZ resource | Back to GAME.DTZ returns to the archive browser.");
    }
    updateActionBar();
    layoutChildren(gMainWindow);
}

static void openSelectedDtzDirEntryStandalone() {
    if (gMode != StorylandMode::DtzArchive || gSelectedKind != StorylandTreeKind::DtzDirEntry || gSelectedIndex < 0) {
        MessageBoxW(gMainWindow, L"Select a GAME.DTZ internal IMG entry first.", L"Storyland", MB_ICONINFORMATION);
        return;
    }
    openDtzDirEntryStandaloneByIndex(gSelectedIndex, -1);
}

static void selectDtzDirEntry(int index) {
    const auto& entries = gDtzArchive.dirEntries();
    if (index < 0 || size_t(index) >= entries.size()) return;
    const auto& entry = entries[size_t(index)];
    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::DtzDirEntry;

    std::wstringstream ss;
    ss << L"GAME.DTZ + gta3ps*.img directory entry\r\n\r\n"
       << L"Name: " << widenResourceName(entry.name) << L"\r\n"
       << L"Internal map index: " << entry.dirIndex << L"\r\n"
       << L"Start sector: " << entry.startSector << L"\r\n"
       << L"Sector count: " << entry.sectorCount << L"\r\n"
       << L"End sector: " << (uint64_t(entry.startSector) + uint64_t(entry.sectorCount)) << L"\r\n"
       << L"Byte budget: " << entry.byteLength << L" bytes\r\n"
       << L"KiB budget: " << (entry.byteLength / 1024ull) << L" KiB\r\n"
       << L"IMG byte offset: " << entry.byteOffset << L"\r\n"
       << L"IMG byte end: " << (entry.byteOffset + entry.byteLength) << L"\r\n"
       << L"Detected payload type: " << widen(entry.detectedExtension.empty() ? std::string("unknown") : entry.detectedExtension) << L"\r\n"
       << L"Matching DTZ records: " << entry.matchingRecordIndices.size() << L"\r\n";
    if (entry.countDiffersFromCompanionDir) {
        ss << L"Matched GAME.DTZ record sector count: " << entry.matchedDtzSectorCount << L"\r\n"
           << L"Browser/preview sector count source: loaded .dir pair. Same-start GAME.DTZ count does not match.\r\n"
           << L"This avoids extracting a truncated or over-long model/texture slice for preview while still keeping the GAME.DTZ record linked for patching.\r\n";
    }
    ss << L"\r\n";

    if (gDtzArchive.hasCompanionImg()) {
        const uint64_t imgOffset = entry.byteOffset;
        ss << L"Companion IMG loaded: " << gDtzArchive.companionImgPath() << L"\r\n"
           << L"Companion IMG size: " << gDtzArchive.companionImgSize() << L" bytes\r\n"
           << L"Available bytes for this entry: " << entry.availableBytes << L" / " << entry.byteLength << L"\r\n"
           << L"This entry range is " << (entry.fullyBackedByImg ? L"inside" : L"past") << L" the loaded IMG size.\r\n\r\n";
        if (imgOffset > gDtzArchive.companionImgSize()) {
            ss << L"Warning: start offset is beyond the loaded IMG. The selected GAME.DTZ entry cannot address data in this IMG. Verify the DTZ/IMG pair.\r\n\r\n";
        }
    } else {
        ss << L"No companion IMG loaded. Load gta3PS2.img/gta3PSP.img/GTA3PSPHR.IMG if you want this same patch action to grow/shrink the physical IMG layout too.\r\n\r\n";
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
    if (gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile && gModelFile.isPmlcMdl()) {
        applyStorylandTitleTint(StorylandTitleTint::BloodRed);
    } else {
        StorylandTitleTint dtzTint = titleTintFromPath(gDtzArchive.sourcePath());
        if (dtzTint == StorylandTitleTint::Default) dtzTint = StorylandTitleTint::LCS;
        applyStorylandTitleTint(dtzTint);
    }
    if (!previewSummary.empty()) {
        ss << L"\r\nPreview section\r\n\r\n" << previewSummary << L"\r\n";
    }

    std::wstring ext = getDtzImgResourceExtensionLower(widenResourceName(entry.name));
    if (ext == L".mdl" || ext == L".dff" || ext == L".xtx" || ext == L".chk" || ext == L".tex" || ext == L".txd") {
        ss << L"\r\nTip: double-click this entry in the tree to open it as a standalone extracted file too.\r\n";
    }

    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectDtzFindResult(int index) {
    if (index < 0 || size_t(index) >= gDtzFindResults.size()) return;
    const StorylandDtzFindResult& result = gDtzFindResults[size_t(index)];
    gSelectedKind = StorylandTreeKind::DtzFindResult;
    gSelectedIndex = index;

    std::wstringstream ss;
    ss << L"GAME.DTZ Find result\r\n\r\n"
       << L"Query: " << gDtzFindQuery << L"\r\n"
       << L"Match: " << result.label << L"\r\n"
       << L"Where: " << result.context << L"\r\n";

    std::wstring previewSummary;
    if (result.dirEntryIndex >= 0 && size_t(result.dirEntryIndex) < gDtzArchive.dirEntries().size()) {
        const auto& entry = gDtzArchive.dirEntries()[size_t(result.dirEntryIndex)];
        ss << L"Internal IMG entry: " << widenResourceName(entry.name) << L"\r\n"
           << L"Start sector: " << entry.startSector << L"\r\n"
           << L"Sector count: " << entry.sectorCount << L"\r\n"
           << L"IMG byte offset: " << entry.byteOffset << L"\r\n";
        if (result.deepTextureName) {
            ss << L"Deep texture-name match index: " << result.textureIndex << L"\r\n";
        }
        ss << L"\r\n";

        if (prepareDtzDirEntryPreview(result.dirEntryIndex, previewSummary)) {
            if (result.deepTextureName &&
                gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::TextureArchive &&
                result.textureIndex >= 0 &&
                size_t(result.textureIndex) < gTextureArchive.textures().size()) {
                std::string decodeError;
                if (gTextureArchive.decodeTexture(size_t(result.textureIndex), gCurrentImage, decodeError)) {
                    createTextureBitmapFromImage();
                    const auto& texture = gTextureArchive.textures()[size_t(result.textureIndex)];
                    previewSummary += L"\r\nMatched texture preview: " + widen(texture.name) +
                        L"  " + std::to_wstring(texture.width) + L"x" + std::to_wstring(texture.height) + L".";
                } else {
                    previewSummary += L"\r\nMatched texture preview could not be decoded: " + widen(decodeError);
                }
            }
        }
    } else {
        clearDtzEmbeddedPreviewState();
    }

    if (!previewSummary.empty()) ss << L"Preview\r\n\r\n" << previewSummary << L"\r\n\r\n";
    if (result.dirEntryIndex >= 0) {
        ss << L"Double-click this Find result to open the matched internal resource. "
              L"Use Back to GAME.DTZ to return without reopening the archive.\r\n";
    } else {
        ss << L"Double-click this Find result to jump to the matching GAME.DTZ item.\r\n";
    }

    StorylandTitleTint dtzTint = titleTintFromPath(gDtzArchive.sourcePath());
    if (gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile && gModelFile.isPmlcMdl()) {
        dtzTint = StorylandTitleTint::BloodRed;
    } else if (dtzTint == StorylandTitleTint::Default) {
        dtzTint = StorylandTitleTint::LCS;
    }
    applyStorylandTitleTint(dtzTint);
    setDetails(ss.str());
    setStatus(L"GAME.DTZ Find | result " + std::to_wstring(index + 1) + L" of " + std::to_wstring(gDtzFindResults.size()));
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void openDtzFindResult(int index) {
    if (index < 0 || size_t(index) >= gDtzFindResults.size()) return;
    const StorylandDtzFindResult result = gDtzFindResults[size_t(index)];
    if (result.dirEntryIndex >= 0) {
        openDtzDirEntryStandaloneByIndex(result.dirEntryIndex, result.deepTextureName ? result.textureIndex : -1);
        return;
    }
    if (result.targetKind != StorylandTreeKind::None && result.targetIndex >= 0) {
        if (!selectTreePayloadItem(result.targetKind, result.targetIndex)) {
            setStatus(L"Find result source item is no longer present; run Find again.");
        }
    }
}

static void performDtzFind() {
    if (gMode != StorylandMode::DtzArchive) {
        MessageBoxW(gMainWindow, L"Open GAME.DTZ first.", L"Find in GAME.DTZ", MB_ICONINFORMATION);
        return;
    }

    std::wstring query = gDtzFindQuery;
    if (!askString(L"Find in GAME.DTZ", L"Find resource, model, archive, field, or texture name:", query, query)) return;
    while (!query.empty() && iswspace(query.front())) query.erase(query.begin());
    while (!query.empty() && iswspace(query.back())) query.pop_back();
    if (query.empty()) {
        MessageBoxW(gMainWindow, L"Enter a name or partial name to find.", L"Find in GAME.DTZ", MB_ICONINFORMATION);
        return;
    }

    gDtzFindQuery = query;
    gDtzFindResults.clear();
    constexpr size_t kMaxResults = 1000u;
    std::set<std::wstring> seen;

    auto addResult = [&](StorylandDtzFindResult result, const std::wstring& uniqueKey) {
        if (gDtzFindResults.size() >= kMaxResults) return;
        if (!seen.insert(uniqueKey).second) return;
        gDtzFindResults.push_back(std::move(result));
    };

    const auto& entries = gDtzArchive.dirEntries();
    for (size_t entryIndex = 0; entryIndex < entries.size() && gDtzFindResults.size() < kMaxResults; ++entryIndex) {
        const auto& entry = entries[entryIndex];
        const std::wstring entryName = widenResourceName(entry.name);
        const std::wstring cleanedName = canonicalDtzImgResourceName(entryName);
        if (!containsWideNoCase(entryName, query) && !containsWideNoCase(cleanedName, query)) continue;
        StorylandDtzFindResult result;
        result.targetKind = StorylandTreeKind::DtzDirEntry;
        result.targetIndex = int(entryIndex);
        result.dirEntryIndex = int(entryIndex);
        result.label = L"[resource] " + cleanedName;
        result.context = L"GAME.DTZ + gta3ps*.img directory";
        addResult(std::move(result), L"entry:" + std::to_wstring(entryIndex));
    }

    const auto& headers = gDtzArchive.headerFields();
    for (size_t index = 0; index < headers.size() && gDtzFindResults.size() < kMaxResults; ++index) {
        std::wstring searchable = widen(headers[index].name) + L" " + widen(headers[index].note);
        if (!containsWideNoCase(searchable, query)) continue;
        StorylandDtzFindResult result;
        result.targetKind = StorylandTreeKind::DtzHeader;
        result.targetIndex = int(index);
        result.label = L"[header] " + widen(headers[index].name);
        result.context = L"GTAG/GATG header field";
        addResult(std::move(result), L"header:" + std::to_wstring(index));
    }

    const auto& hints = gDtzArchive.resourceHints();
    for (size_t index = 0; index < hints.size() && gDtzFindResults.size() < kMaxResults; ++index) {
        std::wstring searchable = widen(hints[index].name) + L" " + widen(hints[index].note);
        if (!containsWideNoCase(searchable, query)) continue;
        StorylandDtzFindResult result;
        result.targetKind = StorylandTreeKind::DtzResourceHint;
        result.targetIndex = int(index);
        result.label = L"[pointer] " + widen(hints[index].name);
        result.context = L"named GAME.DTZ resource pointer";
        addResult(std::move(result), L"hint:" + std::to_wstring(index));
    }

    const auto& blocks = gDtzArchive.dataBlocks();
    for (size_t index = 0; index < blocks.size() && gDtzFindResults.size() < kMaxResults; ++index) {
        std::wstring searchable = widen(blocks[index].name) + L" " + widen(blocks[index].parser) + L" " + widen(blocks[index].note);
        if (!containsWideNoCase(searchable, query)) continue;
        StorylandDtzFindResult result;
        result.targetKind = StorylandTreeKind::DtzDataBlock;
        result.targetIndex = int(index);
        result.label = L"[data] " + widen(blocks[index].name);
        result.context = L"decoded GAME.DTZ data block";
        addResult(std::move(result), L"block:" + std::to_wstring(index));
    }

    const auto& fields = gDtzArchive.dataFields();
    for (size_t index = 0; index < fields.size() && gDtzFindResults.size() < kMaxResults; ++index) {
        const auto& field = fields[index];
        std::wstring searchable = widen(field.blockName) + L" " + widen(field.rowLabel) + L" " + widen(field.name) + L" " + widen(field.valueText);
        if (!containsWideNoCase(searchable, query)) continue;
        StorylandDtzFindResult result;
        result.targetKind = StorylandTreeKind::DtzDataField;
        result.targetIndex = int(index);
        result.label = L"[field] " + widen(field.blockName) + L" / " + widen(field.name);
        result.context = L"decoded GAME.DTZ data field";
        addResult(std::move(result), L"field:" + std::to_wstring(index));
    }

    const auto& records = gDtzArchive.sectorRecords();
    for (size_t index = 0; index < records.size() && gDtzFindResults.size() < kMaxResults; ++index) {
        if (records[index].resourceName.empty()) continue;
        std::wstring recordName = widen(records[index].resourceName);
        if (!containsWideNoCase(recordName, query)) continue;
        StorylandDtzFindResult result;
        result.targetKind = StorylandTreeKind::DtzSectorRecord;
        result.targetIndex = int(index);
        result.label = L"[stream] " + recordName;
        result.context = L"raw GAME.DTZ stream record";
        addResult(std::move(result), L"record:" + std::to_wstring(index));
    }

    std::wstring deepIndexSummary;
    const bool deepIndexReady = buildDtzTextureNameIndex(deepIndexSummary);
    if (deepIndexReady) {
        for (const auto& indexed : gDtzTextureNameIndex) {
            if (gDtzFindResults.size() >= kMaxResults) break;
            if (!containsWideNoCase(indexed.textureName, query)) continue;
            if (indexed.dirEntryIndex < 0 || size_t(indexed.dirEntryIndex) >= entries.size()) continue;
            std::wstring archiveName = canonicalDtzImgResourceName(widen(entries[size_t(indexed.dirEntryIndex)].name));
            StorylandDtzFindResult result;
            result.targetKind = StorylandTreeKind::DtzDirEntry;
            result.targetIndex = indexed.dirEntryIndex;
            result.dirEntryIndex = indexed.dirEntryIndex;
            result.textureIndex = indexed.textureIndex;
            result.deepTextureName = true;
            result.label = L"[texture] " + archiveName + L" > " + indexed.textureName;
            result.context = L"texture name decoded from internal " + archiveName;
            addResult(std::move(result), L"texture:" + std::to_wstring(indexed.dirEntryIndex) + L":" + std::to_wstring(indexed.textureIndex));
        }
    }

    populateDtzList();
    refreshModeUi();
    if (!gDtzFindResults.empty()) {
        selectTreePayloadItem(StorylandTreeKind::DtzFindResult, 0);
        std::wstring status = L"GAME.DTZ Find | " + std::to_wstring(gDtzFindResults.size()) + L" matches for \"" + query + L"\"";
        if (!deepIndexSummary.empty()) status += L" | " + deepIndexSummary;
        if (gDtzFindResults.size() >= kMaxResults) status += L" | result limit reached";
        setStatus(status);
    } else {
        std::wstring message = L"No matches for \"" + query + L"\".";
        if (!deepIndexSummary.empty()) message += L"\r\n\r\n" + deepIndexSummary;
        setDetails(message);
        setStatus(L"GAME.DTZ Find | no matches for \"" + query + L"\"");
    }
}

static void returnToGameDtz() {
    if (!gDtzReturnAvailable || gDtzArchive.sourcePath().empty()) {
        MessageBoxW(gMainWindow, L"There is no GAME.DTZ browser to return to.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    const StorylandTreeKind returnKind = gDtzReturnTreeKind;
    const int returnTreeIndex = gDtzReturnTreeIndex;
    const int returnEntryIndex = gDtzReturnSelectedIndex;
    const StorylandTitleTint returnTint = gDtzReturnTint;

    gMediaFile.close();
    gDtzReturnAvailable = false;
    gDtzReturnSelectedIndex = -1;
    gDtzReturnTreeKind = StorylandTreeKind::None;
    gDtzReturnTreeIndex = -1;

    gMode = StorylandMode::DtzArchive;
    resetModelViewport();
    gModelDistance = 4.0f;
    populateDtzList();
    SetWindowTextW(gMainWindow, gDtzArchive.hasCompanionImg() ? L"Storyland - GAME.DTZ + gta3PS*.img" : L"Storyland - GAME.DTZ");

    StorylandTitleTint tint = returnTint;
    if (tint == StorylandTitleTint::BloodRed || tint == StorylandTitleTint::Default) {
        tint = titleTintFromPath(gDtzArchive.sourcePath());
        if (tint == StorylandTitleTint::Default) tint = StorylandTitleTint::LCS;
    }
    applyStorylandTitleTint(tint);
    refreshModeUi();

    bool restored = false;
    if (returnKind != StorylandTreeKind::None && returnTreeIndex >= 0) {
        restored = selectTreePayloadItem(returnKind, returnTreeIndex);
    }
    if (!restored && returnEntryIndex >= 0) {
        restored = selectTreePayloadItem(StorylandTreeKind::DtzDirEntry, returnEntryIndex);
    }
    if (!restored) selectTreePayloadItem(StorylandTreeKind::DtzOverview, 0);
    setStatus(L"Returned to GAME.DTZ browser.");
}

static void selectDtzSectorRecord(int index) {
    const auto& records = gDtzArchive.sectorRecords();
    if (index < 0 || size_t(index) >= records.size()) return;
    const auto& r = records[size_t(index)];
    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::DtzSectorRecord;
    std::wstringstream ss;
    ss << L"GAME.DTZ + gta3ps*.img directory record\r\n\r\n";
    if (!r.resourceName.empty()) ss << L"Known target: " << widenResourceName(r.resourceName) << L"\r\n";
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
       << L"This is the exact sector allocation Storyland patches. No resource identity is inferred from a hard-coded sector number.\r\n"
       << L"If the count changes, later IMG allocations must move by the same sector delta when shifting is enabled. With a companion IMG loaded, Storyland performs the matching physical byte insertion/removal as part of the same edit.\r\n";
    if (gDtzArchive.hasCompanionImg()) {
        ss << L"\r\nCompanion IMG loaded: " << gDtzArchive.companionImgPath() << L"  size=" << gDtzArchive.companionImgSize() << L" bytes\r\n";
    }
    setDetails(ss.str());
    InvalidateRect(gPreview, nullptr, TRUE);
}


static const wchar_t* leeds2dfxEffectTypeName(uint8_t type) {
    switch (type) {
    case 0: return L"Light";
    case 1: return L"Particle";
    case 2: return L"Attractor";
    case 3: return L"Ped behaviour";
    default: return L"Unknown";
    }
}

static const wchar_t* leeds2dfxLightTypeName(uint8_t type) {
    switch (type) {
    case 0: return L"Always on";
    case 1: return L"On at night";
    case 2: return L"Flicker";
    case 3: return L"Flicker at night";
    case 4: return L"Flash 1";
    case 5: return L"Flash 1 at night";
    case 6: return L"Flash 2";
    case 7: return L"Flash 2 at night";
    case 8: return L"Flash 3";
    case 9: return L"Flash 3 at night";
    case 10: return L"Random flicker";
    case 11: return L"Random flicker at night";
    case 12: return L"Special";
    case 13: return L"Bridge flash 1";
    case 14: return L"Bridge flash 2";
    default: return L"Unknown";
    }
}

static void appendDtzLeeds2dfxPayloadDetails(std::wstringstream& ss, const StorylandDtz2dfxEffect& effect) {
    if (effect.effectType == 0u) {
        ss << L"\r\nLeeds C2dEffect::Light payload\r\n\r\n"
           << L"Distance / corona far clip (+0x18): " << effect.coronaFarClip << L"\r\n"
           << L"Outer point-light range (+0x1C): " << effect.pointLightRange << L"\r\n"
           << L"Corona size (+0x20): " << effect.coronaSize << L"\r\n"
           << L"Inner range / shadow size (+0x24): " << effect.shadowSize << L"\r\n"
           << L"Flash mode (+0x28): " << leeds2dfxLightTypeName(effect.lightType) << L" (" << int(effect.lightType) << L")\r\n"
           << L"Wet-road reflection (+0x29): " << int(effect.roadReflection) << L"\r\n"
           << L"Flare type (+0x2A): " << int(effect.flareType) << L"\r\n"
           << L"Shadow intensity (+0x2B): " << int(effect.shadowIntensity) << L"\r\n"
           << L"Flags (+0x2C): " << hexWide(effect.flags, 2) << L"\r\n"
           << L"  LOS check: " << ((effect.flags & 0x01u) ? L"yes" : L"no") << L"\r\n"
           << L"  Normal fog: " << ((effect.flags & 0x02u) ? L"yes" : L"no") << L"\r\n"
           << L"  Always fog: " << ((effect.flags & 0x04u) ? L"yes" : L"no") << L"\r\n"
           << L"  Hide-object flag: " << ((effect.flags & 0x08u) ? L"yes" : L"no") << L"\r\n"
           << L"  Long-distance flag: " << ((effect.flags & 0x10u) ? L"yes" : L"no") << L"\r\n"
           << L"Corona texture pointer (+0x30): " << hexWide(effect.coronaTexturePointer) << L"\r\n"
           << L"Shadow texture pointer (+0x34): " << hexWide(effect.shadowTexturePointer) << L"\r\n";
    } else if (effect.effectType == 1u) {
        ss << L"\r\nLeeds C2dEffect::Particle payload\r\n\r\n"
           << L"Particle subtype (+0x18): " << effect.particleSubtype << L"\r\n"
           << L"Direction (+0x1C): (" << effect.directionX << L", " << effect.directionY << L", " << effect.directionZ << L")\r\n"
           << L"Particle scale (+0x28): " << effect.particleScale << L"\r\n";
    } else if (effect.effectType == 2u) {
        ss << L"\r\nLeeds C2dEffect::Attractor payload\r\n\r\n"
           << L"Direction (+0x18): (" << effect.directionX << L", " << effect.directionY << L", " << effect.directionZ << L")\r\n"
           << L"Attractor subtype (+0x24): " << int(effect.attractorSubtype) << L"\r\n"
           << L"Probability (+0x25): " << int(effect.attractorProbability) << L"\r\n";
    } else if (effect.effectType == 3u) {
        ss << L"\r\nLeeds C2dEffect::PedBehaviour payload\r\n\r\n"
           << L"Direction (+0x18): (" << effect.directionX << L", " << effect.directionY << L", " << effect.directionZ << L")\r\n"
           << L"Rotation (+0x24): (" << effect.pedRotationX << L", " << effect.pedRotationY << L", " << effect.pedRotationZ << L")\r\n"
           << L"Ped behaviour subtype (+0x30): " << int(effect.pedSubtype) << L"\r\n";
    }
}

static void selectDtzLeeds2dfx(int index) {
    const auto& effects = gDtzArchive.leeds2dfxEffects();
    if (index < 0 || size_t(index) >= effects.size()) return;
    const auto& effect = effects[size_t(index)];

    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::DtzLeeds2dfx;

    std::wstringstream ss;
    ss << L"Leeds-engine GAME.DTZ C2dEffect definition\r\n\r\n"
       << L"Global effect index: " << effect.index << L"\r\n"
       << L"Serialized 64-byte row offset: " << hexWide(effect.rowOffset, 6) << L"\r\n"
       << L"Valid decoded row: " << (effect.valid ? L"yes" : L"no") << L"\r\n"
       << L"Effect type (+0x14): " << leeds2dfxEffectTypeName(effect.effectType) << L" (" << int(effect.effectType) << L")\r\n"
       << L"Model-local position float4 (+0x00): (" << effect.localX << L", " << effect.localY << L", " << effect.localZ << L", " << effect.positionW << L")\r\n"
       << L"RGBA (+0x10): (" << int(effect.red) << L", " << int(effect.green) << L", " << int(effect.blue) << L", " << int(effect.alpha) << L")\r\n";

    if (effect.hasModelAssociation) {
        ss << L"Owning model index: " << effect.modelIndex << L"\r\n"
           << L"Owning model hash: " << hexWide(effect.modelHash) << L"\r\n"
           << L"Owning CBaseModelInfo offset: " << hexWide(effect.modelInfoOffset, 6) << L"\r\n"
           << L"Model info type: " << int(effect.modelType) << L"\r\n"
           << L"Effect index inside model range: " << effect.modelEffectIndex << L"\r\n";
        if (!effect.modelName.empty()) ss << L"Model name: " << widen(effect.modelName) << L"\r\n";
    } else {
        ss << L"Owning model: unresolved from the current CBaseModelInfo pointer table\r\n";
    }

    appendDtzLeeds2dfxPayloadDetails(ss, effect);

    ss << L"\r\nRendering source\r\n\r\n"
       << L"Storyland first resolves this model-local effect through allocated BUILDING, TREADABLE, and DUMMY CEntity records and applies each entity's native right/up/at/position matrix. "
       << L"The separated model-local atlas is used only when no allocated world instances can be recovered. RenderWare DFF-plugin 2DFX is a separate, lower-priority source used while viewing an individual DFF.\r\n";

    setDetails(ss.str());
    setStatus(L"Selected Leeds GAME.DTZ C2dEffect #" + std::to_wstring(effect.index));
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void selectDtzLeeds2dfxWorld(int index) {
    const auto& instances = gDtzArchive.leeds2dfxWorldInstances();
    const auto& effects = gDtzArchive.leeds2dfxEffects();
    if (index < 0 || size_t(index) >= instances.size()) return;
    const auto& instance = instances[size_t(index)];
    if (instance.effectIndex >= effects.size()) return;
    const auto& effect = effects[instance.effectIndex];

    gSelectedIndex = index;
    gSelectedKind = StorylandTreeKind::DtzLeeds2dfxWorld;

    std::wstringstream ss;
    ss << L"Leeds GAME.DTZ native world 2DFX instance\r\n\r\n"
       << L"World instance index: " << instance.index << L"\r\n"
       << L"Pool: " << widen(instance.poolName) << L"\r\n"
       << L"Pool slot: " << instance.poolIndex << L"\r\n"
       << L"Pool allocation flag: " << hexWide(instance.poolFlag, 2) << L"\r\n"
       << L"CEntity offset: " << hexWide(instance.entityOffset, 6) << L"\r\n"
       << L"Primary model index (+0x56): " << instance.modelIndex << L"\r\n"
       << L"Secondary model index (+0x58): " << instance.secondaryModelIndex << L"\r\n"
       << L"Level (+0x5A): " << int(instance.level) << L"\r\n"
       << L"Area (+0x5B): " << int(instance.area) << L"\r\n"
       << L"C2dEffect index: " << instance.effectIndex << L"\r\n"
       << L"C2dEffect type: " << leeds2dfxEffectTypeName(effect.effectType) << L"\r\n"
       << L"\r\nNative CEntity matrix\r\n\r\n"
       << L"Right: (" << instance.rightX << L", " << instance.rightY << L", " << instance.rightZ << L")\r\n"
       << L"Up: (" << instance.upX << L", " << instance.upY << L", " << instance.upZ << L")\r\n"
       << L"At: (" << instance.atX << L", " << instance.atY << L", " << instance.atZ << L")\r\n"
       << L"Entity position: (" << instance.entityX << L", " << instance.entityY << L", " << instance.entityZ << L")\r\n"
       << L"Model-local effect position: (" << effect.localX << L", " << effect.localY << L", " << effect.localZ << L")\r\n"
       << L"Resolved world position: (" << instance.worldX << L", " << instance.worldY << L", " << instance.worldZ << L")\r\n"
       << L"RGBA: (" << int(effect.red) << L", " << int(effect.green) << L", " << int(effect.blue) << L", " << int(effect.alpha) << L")\r\n";

    appendDtzLeeds2dfxPayloadDetails(ss, effect);
    ss << L"\r\nThis is the native Leeds light/effect placement used by the GAME.DTZ viewport. It is not a RenderWare DFF-plugin helper and is not placed in the model-local fallback atlas.\r\n";

    setDetails(ss.str());
    setStatus(L"Selected native Leeds world 2DFX instance #" + std::to_wstring(instance.index));
    InvalidateRect(gPreview, nullptr, FALSE);
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
        ss << L"Right-click > Edit... to modify this field. Then rebuild GAME.DTZ to write the change.\r\n";
    } else {
        ss << L"This field is read-only.\r\n";
    }

    // weapon.dat rows carry the model id in the same decoded record. Resolve
    // that model through the current GAME.DTZ streaming map and preview it
    // without changing the user's selected field.
    std::string lowerBlock = field.blockName;
    std::transform(lowerBlock.begin(), lowerBlock.end(), lowerBlock.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (lowerBlock.find("weapon") != std::string::npos) {
        int64_t modelId = -1;
        int64_t model2Id = -1;
        for (const auto& sibling : fields) {
            if (sibling.blockIndex != field.blockIndex || sibling.rowIndex != field.rowIndex) continue;
            auto parseInteger = [](const std::string& text, int64_t& out) -> bool {
                char* end = nullptr;
                errno = 0;
                long long value = std::strtoll(text.c_str(), &end, 0);
                if (errno != 0 || end == text.c_str()) return false;
                out = int64_t(value);
                return true;
            };
            if (sibling.relativeOffset == 0x60u) parseInteger(sibling.valueText, modelId);
            else if (sibling.relativeOffset == 0x64u) parseInteger(sibling.valueText, model2Id);
        }

        auto appendWeaponModel = [&](int64_t id, const wchar_t* label) {
            if (id < 0 || id > 0xFFFFFFFFll) return;
            size_t entryIndex = 0;
            std::string entryName;
            if (!gDtzArchive.findModelDirEntryByModelId(uint32_t(id), entryIndex, entryName)) {
                ss << L"\r\n" << label << L": model ID " << id << L" (not present in the loaded IMG map)\r\n";
                return;
            }
            ss << L"\r\n" << label << L": model ID " << id << L" -> " << widen(entryName) << L"\r\n";
            if (gDtzArchive.hasCompanionImg() && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::None) {
                std::wstring previewSummary;
                if (prepareDtzDirEntryPreview(int(entryIndex), previewSummary)) {
                    ss << previewSummary << L"\r\n";
                } else if (!previewSummary.empty()) {
                    ss << L"Preview: " << previewSummary << L"\r\n";
                }
            }
        };
        appendWeaponModel(modelId, L"Primary weapon model");
        if (model2Id >= 0 && model2Id != modelId) appendWeaponModel(model2Id, L"Secondary weapon model");
    }

    setDetails(ss.str());
    updateActionBar();
    if (gMainWindow) layoutChildren(gMainWindow);
    InvalidateRect(gPreview, nullptr, TRUE);
}


static void selectDtzPayload(const StorylandTreePayload& payload) {
    if (payload.kind == StorylandTreeKind::DtzArea) {
        activateAreaArchive(payload.index);
        return;
    }
    if (payload.kind != StorylandTreeKind::DtzDirEntry) {
        clearDtzEmbeddedPreviewState();
        StorylandTitleTint dtzTint = titleTintFromPath(gDtzArchive.sourcePath());
        if (dtzTint == StorylandTitleTint::Default) dtzTint = StorylandTitleTint::LCS;
        applyStorylandTitleTint(dtzTint);
    }
    if (payload.kind == StorylandTreeKind::DtzOverview) selectDtzOverview();
    else if (payload.kind == StorylandTreeKind::DtzHeader) selectDtzHeaderField(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzResourceHint) selectDtzResourceHint(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzSectorRecord) selectDtzSectorRecord(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzDirEntry) selectDtzDirEntry(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzDataBlock) selectDtzDataBlock(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzDataField) selectDtzDataField(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzLeeds2dfx) selectDtzLeeds2dfx(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzLeeds2dfxWorld) selectDtzLeeds2dfxWorld(payload.index);
    else if (payload.kind == StorylandTreeKind::DtzFindResult) selectDtzFindResult(payload.index);
    rebuildViewMenu();
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
    loadAreaArchivesForCurrentDtz();

    clearDtzFindState(true);
    gMode = StorylandMode::DtzArchive;
    refreshModeUi();
    resetModelViewport();
    gModelDistance = 4.0f;
    populateDtzList();
    SetWindowTextW(gMainWindow, L"Storyland - GAME.DTZ + gta3PS*.img");
    StorylandTitleTint pairTint = titleTintFromPath(dtzPath);
    if (pairTint == StorylandTitleTint::Default) pairTint = StorylandTitleTint::LCS;
    applyStorylandTitleTint(pairTint);
    setStatus(L"Opened GAME.DTZ + IMG pair from IMG: " + imgPath);
    return true;
}

static bool readUtf8OrAnsiTextFile(const std::wstring& path, std::string& text, std::string& error) {
    constexpr uint64_t kMaxTextBytes = 64ull * 1024ull * 1024ull;
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr) {
        error = "Could not open text file.";
        return false;
    }
    if (_fseeki64(file, 0, SEEK_END) != 0) {
        fclose(file); error = "Could not seek text file."; return false;
    }
    const __int64 signedSize = _ftelli64(file);
    if (signedSize < 0 || uint64_t(signedSize) > kMaxTextBytes ||
        uint64_t(signedSize) > uint64_t((std::numeric_limits<size_t>::max)())) {
        fclose(file); error = "Text file is too large to load safely."; return false;
    }
    if (_fseeki64(file, 0, SEEK_SET) != 0) {
        fclose(file); error = "Could not rewind text file."; return false;
    }
    std::vector<uint8_t> bytes;
    bytes.resize(static_cast<size_t>(signedSize));
    if (!bytes.empty() && fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) {
        fclose(file); error = "Could not read complete text file."; return false;
    }
    fclose(file);

    size_t offset = 0;
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) offset = 3;
    if (offset >= bytes.size()) text.clear();
    else text.assign(reinterpret_cast<const char*>(bytes.data() + offset), bytes.size() - offset);
    error.clear();
    return true;
}

static std::wstring readRegistryString(HKEY root, const wchar_t* keyPath, const wchar_t* valueName) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, keyPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return L"";

    DWORD type = 0;
    DWORD byteCount = 0;
    constexpr DWORD kMaxRegistryStringBytes = 1024u * 1024u;
    if (RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &byteCount) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) ||
        byteCount < sizeof(wchar_t) || byteCount > kMaxRegistryStringBytes ||
        (byteCount % sizeof(wchar_t)) != 0u) {
        RegCloseKey(key);
        return L"";
    }

    std::vector<wchar_t> buffer(size_t(byteCount / sizeof(wchar_t)) + 2u, L'\0');
    if (RegQueryValueExW(key, valueName, nullptr, &type,
                        reinterpret_cast<BYTE*>(buffer.data()), &byteCount) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return L"";
    }
    RegCloseKey(key);

    std::wstring value(buffer.data());
    if (type == REG_EXPAND_SZ && !value.empty()) {
        DWORD needed = ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
        if (needed > 0) {
            std::vector<wchar_t> expanded(size_t(needed), L'\0');
            if (ExpandEnvironmentStringsW(value.c_str(), expanded.data(), needed) > 0) value = expanded.data();
        }
    }
    return value;
}

static void saveSannyBuilderPath(const std::wstring& path) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Reigns Studios\\Storyland", 0, nullptr,
                        0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(key, L"SannyBuilderPath", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(path.c_str()), DWORD((path.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

static std::wstring executableDirectory() {
    std::vector<wchar_t> buffer(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
    if (length == 0 || length >= buffer.size()) return L"";
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path().wstring();
}

static std::wstring findSannyBuilderExecutable() {
    if (!gSannyBuilderPath.empty() && fileExists(gSannyBuilderPath)) return gSannyBuilderPath;

    std::wstring configured = readRegistryString(
        HKEY_CURRENT_USER,
        L"Software\\Reigns Studios\\Storyland",
        L"SannyBuilderPath"
    );
    if (!configured.empty() && fileExists(configured)) {
        gSannyBuilderPath = configured;
        return gSannyBuilderPath;
    }

    const wchar_t* appPathsKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\sanny.exe";
    for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        std::wstring appPath = readRegistryString(root, appPathsKey, nullptr);
        if (!appPath.empty() && fileExists(appPath)) {
            gSannyBuilderPath = appPath;
            return gSannyBuilderPath;
        }
    }

    std::wstring besideStoryland;
    const std::wstring appDir = executableDirectory();
    if (!appDir.empty()) {
        besideStoryland = (std::filesystem::path(appDir) / L"sanny.exe").wstring();
        if (fileExists(besideStoryland)) {
            gSannyBuilderPath = besideStoryland;
            return gSannyBuilderPath;
        }
    }

    wchar_t found[MAX_PATH] = {};
    DWORD length = SearchPathW(nullptr, L"sanny.exe", nullptr, MAX_PATH, found, nullptr);
    if (length > 0 && length < MAX_PATH && fileExists(found)) {
        gSannyBuilderPath = found;
        return gSannyBuilderPath;
    }

    return L"";
}

static std::wstring quoteCommandArgument(const std::wstring& argument) {
    if (argument.empty()) return L"\"\"";
    if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;

    std::wstring result = L"\"";
    size_t backslashes = 0;
    for (wchar_t ch : argument) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'\"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'\"');
            backslashes = 0;
            continue;
        }
        result.append(backslashes, L'\\');
        backslashes = 0;
        result.push_back(ch);
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

static bool runSannyBuilder(const std::wstring& executable,
                            const std::vector<std::wstring>& arguments,
                            DWORD& exitCode,
                            std::string& error) {
    if (!fileExists(executable)) {
        error = "Sanny Builder executable was not found.";
        return false;
    }

    std::wstring commandLine = quoteCommandArgument(executable);
    for (const std::wstring& argument : arguments) {
        commandLine += L" ";
        commandLine += quoteCommandArgument(argument);
    }

    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION process = {};
    std::wstring workingDirectory = std::filesystem::path(executable).parent_path().wstring();
    BOOL created = CreateProcessW(
        executable.c_str(),
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
        &startup,
        &process
    );
    if (!created) {
        std::ostringstream ss;
        ss << "Could not start Sanny Builder. Win32 error " << GetLastError() << ".";
        error = ss.str();
        return false;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    exitCode = 0xFFFFFFFFu;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

static std::wstring scmTempDirectory() {
    wchar_t tempPath[MAX_PATH] = {};
    DWORD length = GetTempPathW(MAX_PATH, tempPath);
    std::filesystem::path root = length > 0 ? std::filesystem::path(tempPath) : std::filesystem::temp_directory_path();
    root /= L"StorylandSCM_" + std::to_wstring(GetCurrentProcessId());
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return root.wstring();
}

static std::wstring uniqueScmTempPath(const wchar_t* extension) {
    std::wstringstream name;
    name << L"storyland_" << GetCurrentProcessId() << L"_" << GetTickCount64() << extension;
    return (std::filesystem::path(scmTempDirectory()) / name.str()).wstring();
}

static std::wstring findSannyMissionNamesFile(const std::wstring& sannyExecutable, const std::string& modeId) {
    std::filesystem::path base = std::filesystem::path(sannyExecutable).parent_path();
    std::wstring mode = widen(modeId);
    for (int level = 0; level < 3 && !base.empty(); ++level) {
        std::filesystem::path candidate = base / L"data" / mode / L"missions.txt";
        if (fileExists(candidate.wstring())) return candidate.wstring();
        std::filesystem::path parent = base.parent_path();
        if (parent == base) break;
        base = parent;
    }
    return L"";
}

static void applyScmMissionNames(const std::wstring& sannyExecutable, const std::string& requestedMode = std::string()) {
    const std::string mode = requestedMode.empty() ? gScmFile.modeId() : requestedMode;
    if (mode.empty()) return;
    std::wstring namesPath = findSannyMissionNamesFile(sannyExecutable, mode);
    if (namesPath.empty()) return;
    std::string ignored;
    gScmFile.applyMissionNamesFile(namesPath, ignored);
}

static bool ensureSannyBuilderConfigured(bool allowDialog) {
    if (!findSannyBuilderExecutable().empty()) return true;
    if (!allowDialog) return false;

    std::wstring selected = openFileDialog(L"Sanny Builder CLI (sanny.exe)\0sanny.exe\0Executable files\0*.exe\0All files\0*.*\0");
    if (selected.empty()) return false;
    if (!fileExists(selected)) return false;

    gSannyBuilderPath = selected;
    saveSannyBuilderPath(selected);
    return true;
}

static void configureSannyBuilder() {
    std::wstring selected = openFileDialog(L"Sanny Builder CLI (sanny.exe)\0sanny.exe\0Executable files\0*.exe\0All files\0*.*\0");
    if (selected.empty()) return;
    if (!fileExists(selected)) {
        MessageBoxW(gMainWindow, L"The selected Sanny Builder executable does not exist.", L"Storyland VCS SCM", MB_ICONERROR);
        return;
    }
    gSannyBuilderPath = selected;
    saveSannyBuilderPath(selected);
    setStatus(L"Sanny Builder configured: " + selected);
}

static bool commitScmSourceEditor(bool showError) {
    if (gMode != StorylandMode::ScmFile) return true;

    std::string error;
    if (gSelectedKind == StorylandTreeKind::ScmSource) {
        const std::wstring sourceWide = getDetailsText();
        const std::string mode = gScmFile.modeId().empty() ? "vcs_ps2" : gScmFile.modeId();
        if (!gScmFile.setDecompiledSource(narrow(sourceWide), mode, error)) {
            if (showError) MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland VCS SCM source", MB_ICONERROR);
            return false;
        }
    } else if (gSelectedKind == StorylandTreeKind::ScmMission) {
        const auto& missions = gScmFile.missions();
        if (gSelectedIndex < 0 || size_t(gSelectedIndex) >= missions.size()) return true;
        if (missions[size_t(gSelectedIndex)].source.empty()) return true;

        if (!gScmFile.replaceMissionSource(size_t(gSelectedIndex), narrow(getDetailsText()), error)) {
            if (showError) MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland VCS SCM mission", MB_ICONERROR);
            return false;
        }
    } else {
        return true;
    }

    std::wstring sanny = findSannyBuilderExecutable();
    if (!sanny.empty()) applyScmMissionNames(sanny);
    return true;
}

static void selectScmOverview() {
    setDetailsReadOnly(true);
    gSelectedKind = StorylandTreeKind::ScmOverview;
    gSelectedIndex = 0;

    const StorylandScmHeader& header = gScmFile.header();
    std::wstringstream ss;
    ss << L"Storyland Vice City Stories SCM mission editor\r\n\r\n"
       << L"Path: " << gScmFile.sourcePath() << L"\r\n"
       << L"Raw SCM size: " << gScmFile.rawBytes().size() << L" bytes\r\n"
       << L"Native Stories header: " << (header.valid ? L"decoded" : L"not recognized") << L"\r\n"
       << L"Sanny mode: " << (gScmFile.modeId().empty() ? L"not decompiled" : widen(gScmFile.modeId())) << L"\r\n"
       << L"Decompiled source: " << (gScmFile.hasDecompiledSource() ? L"yes" : L"no") << L"\r\n"
       << L"Mission rows: " << gScmFile.missions().size() << L"\r\n";

    if (header.valid) {
        const wchar_t target = header.targetGame ? wchar_t(static_cast<unsigned char>(header.targetGame)) : L'?';
        ss << L"\r\nNative SCM header\r\n"
           << L"Target byte: '" << target << L"'"
           << (header.targetGame == 'm' ? L" (Miami / Vice City Stories)" : L"") << L"\r\n"
           << L"Main script offset: 0x" << std::hex << std::uppercase << header.mainScriptOffset
           << L"\r\nMain script size: 0x" << header.mainScriptSize << L" (" << std::dec << header.mainScriptSize << L" bytes)\r\n"
           << L"Global variable space: " << header.globalVariableBytes << L" bytes\r\n"
           << L"Save variables: " << header.saveVariableCount << L"\r\n"
           << L"Object definitions: " << header.objectCount << L"\r\n"
           << L"True globals: " << header.trueGlobalCount << L"\r\n"
           << L"Most globals: " << header.mostGlobalCount << L"\r\n"
           << L"Mission count: " << header.missionCount << L"\r\n"
           << L"Exclusive mission count: " << header.exclusiveMissionCount << L"\r\n"
           << L"Largest mission script: " << header.largestMissionScriptSize << L" bytes\r\n"
           << L"Mission offset table: 0x" << std::hex << std::uppercase << header.missionTableOffset << std::dec << L"\r\n";
    }

    ss << L"\r\n";
    std::wstring sanny = findSannyBuilderExecutable();
    if (sanny.empty()) {
        ss << L"Sanny Builder CLI was not found. Native mission offsets and raw mission byte ranges are still available.\r\n"
           << L"Configure sanny.exe to decompile mission bytecode into editable source and compile it back to VCS SCM.\r\n"
           << L"Use File > VCS SCM missions > Configure Sanny Builder.\r\n";
    } else {
        ss << L"Sanny Builder: " << sanny << L"\r\n"
           << L"Decompiled mission nodes are individually editable. The full source node remains available for whole-file edits.\r\n"
           << L"Use Refresh mission list after adding/removing DEFINE MISSION declarations.\r\n";
    }

    setDetails(ss.str());
    setStatus(L"VCS SCM | " + widen(gScmFile.summaryLine()));
    InvalidateRect(gPreview, nullptr, TRUE);
}

static void selectScmSource() {
    if (!gScmFile.hasDecompiledSource()) {
        setDetailsReadOnly(true);
        gSelectedKind = StorylandTreeKind::ScmSource;
        gSelectedIndex = 0;
        setDetails(L"No decompiled source is loaded. Configure Sanny Builder and decompile this SCM as VCS PS2 or VCS PSP.");
        return;
    }

    gSelectedKind = StorylandTreeKind::ScmSource;
    gSelectedIndex = 0;
    setDetailsReadOnly(false);
    setDetails(widen(gScmFile.sourceText()));
    setStatus(L"Editing full VCS SCM source | compile from File > VCS SCM missions");
}

static std::wstring formatScmMissionHexPreview(const StorylandScmMission& mission) {
    if (!mission.hasBinaryRange) return L"No native byte range was decoded for this mission.";

    std::vector<uint8_t> bytes;
    const auto& raw = gScmFile.rawBytes();
    const size_t begin = size_t(mission.binaryOffset);
    if (begin < raw.size()) {
        size_t available = std::min<size_t>(size_t(mission.binarySize), raw.size() - begin);
        size_t shown = std::min<size_t>(available, 1024u);
        bytes.assign(raw.begin() + begin, raw.begin() + begin + shown);
    }

    std::wstringstream ss;
    ss << L"VCS mission #" << mission.id << L"\r\n"
       << L"Name: " << widen(mission.name) << L"\r\n"
       << L"Binary offset: 0x" << std::hex << std::uppercase << mission.binaryOffset << std::dec << L"\r\n"
       << L"Binary size: " << mission.binarySize << L" bytes\r\n";
    if (!mission.label.empty()) ss << L"Sanny label: @" << widen(mission.label) << L"\r\n";
    ss << L"\r\nRaw byte preview";
    if (mission.binarySize > bytes.size()) ss << L" (first " << bytes.size() << L" bytes)";
    ss << L"\r\n\r\n";

    for (size_t row = 0; row < bytes.size(); row += 16) {
        ss << std::hex << std::uppercase << std::setfill(L'0') << std::setw(8)
           << (uint64_t(mission.binaryOffset) + row) << L"  ";
        for (size_t column = 0; column < 16; ++column) {
            if (row + column < bytes.size()) {
                ss << std::setw(2) << unsigned(bytes[row + column]) << L' ';
            } else {
                ss << L"   ";
            }
        }
        ss << L" ";
        for (size_t column = 0; column < 16 && row + column < bytes.size(); ++column) {
            const uint8_t value = bytes[row + column];
            ss << wchar_t(value >= 0x20 && value <= 0x7E ? value : '.');
        }
        ss << L"\r\n";
    }
    return ss.str();
}

static void selectScmMission(int index) {
    const auto& missions = gScmFile.missions();
    if (index < 0 || size_t(index) >= missions.size()) return;

    gSelectedKind = StorylandTreeKind::ScmMission;
    gSelectedIndex = index;
    const StorylandScmMission& mission = missions[size_t(index)];

    if (!mission.source.empty()) {
        setDetailsReadOnly(false);
        setDetails(widen(mission.source));
        std::wstringstream status;
        status << L"Editing VCS mission #" << mission.id << L" | " << widen(mission.name);
        if (!mission.label.empty()) status << L" | @" << widen(mission.label);
        if (mission.hasBinaryRange) {
            status << L" | original binary 0x" << std::hex << std::uppercase << mission.binaryOffset
                   << L" +0x" << mission.binarySize << std::dec;
        }
        setStatus(status.str());
    } else {
        setDetailsReadOnly(true);
        setDetails(formatScmMissionHexPreview(mission));
        setStatus(L"VCS mission #" + std::to_wstring(mission.id) + L" | native binary view | decompile with Sanny to edit");
    }
}

static void selectScmPayload(const StorylandTreePayload& payload) {
    if (payload.kind == StorylandTreeKind::ScmOverview) selectScmOverview();
    else if (payload.kind == StorylandTreeKind::ScmSource) selectScmSource();
    else if (payload.kind == StorylandTreeKind::ScmMission) selectScmMission(payload.index);
}

static void populateScmList() {
    clearView();
    setDetailsReadOnly(true);

    HTREEITEM root = addTreeItem(TVI_ROOT, L"MAIN.SCM / Vice City Stories missions");
    addTreeItem(root, L"Overview", StorylandTreeKind::ScmOverview, 0);
    addTreeItem(root,
        gScmFile.hasDecompiledSource() ? L"Full Decompiled Source (editable)" : L"Full Decompiled Source (decompile required)",
        StorylandTreeKind::ScmSource,
        0);

    std::wstringstream missionsTitle;
    missionsTitle << L"Missions (" << gScmFile.missions().size() << L")";
    if (gScmFile.hasNativeMissionTable()) missionsTitle << L" - native table";
    if (gScmFile.hasDecompiledSource()) missionsTitle << L" + Sanny source";
    HTREEITEM missionsRoot = addTreeItem(root, missionsTitle.str());
    const auto& missions = gScmFile.missions();
    for (size_t index = 0; index < missions.size(); ++index) {
        const StorylandScmMission& mission = missions[index];
        std::wstringstream label;
        label << L"#" << std::setw(3) << std::setfill(L'0') << mission.id
              << L"  " << widen(mission.name);
        if (!mission.label.empty()) label << L"  [@" << widen(mission.label) << L"]";
        if (mission.hasBinaryRange) {
            label << L"  [0x" << std::hex << std::uppercase << mission.binaryOffset
                  << L" +0x" << mission.binarySize << std::dec << L"]";
        }
        if (!mission.source.empty()) label << L"  (editable)";
        addTreeItem(missionsRoot, label.str(), StorylandTreeKind::ScmMission, int(index));
    }

    expandTreeItem(root);
    if (!missions.empty()) expandTreeItem(missionsRoot);
    selectScmOverview();
}

static bool decompileCurrentScm(const std::string& modeId, bool showFailureDialog) {
    if (gMode != StorylandMode::ScmFile || gScmFile.sourcePath().empty()) {
        if (showFailureDialog) MessageBoxW(gMainWindow, L"Open a .scm file first.", L"Storyland VCS SCM", MB_ICONINFORMATION);
        return false;
    }
    if (!ensureSannyBuilderConfigured(showFailureDialog)) {
        if (showFailureDialog) {
            MessageBoxW(gMainWindow,
                L"Sanny Builder CLI (sanny.exe) is required to decompile and compile VCS SCM bytecode.",
                L"Storyland VCS SCM",
                MB_ICONINFORMATION);
        }
        return false;
    }

    const std::wstring outputPath = uniqueScmTempPath(L".txt");
    DeleteFileW(outputPath.c_str());

    DWORD exitCode = 0;
    std::string processError;
    std::vector<std::wstring> arguments = {
        L"--mode", widen(modeId),
        L"--decompile", gScmFile.sourcePath(), outputPath
    };

    if (!runSannyBuilder(gSannyBuilderPath, arguments, exitCode, processError)) {
        DeleteFileW(outputPath.c_str());
        if (showFailureDialog) MessageBoxW(gMainWindow, widen(processError).c_str(), L"Sanny Builder decompile failed", MB_ICONERROR);
        return false;
    }
    if (exitCode != 0 || !fileExists(outputPath)) {
        DeleteFileW(outputPath.c_str());
        if (showFailureDialog) {
            std::wstringstream message;
            message << L"Sanny Builder did not produce decompiled source.\r\nExit code: " << exitCode;
            MessageBoxW(gMainWindow, message.str().c_str(), L"Sanny Builder decompile failed", MB_ICONERROR);
        }
        return false;
    }

    std::string source;
    std::string readError;
    if (!readUtf8OrAnsiTextFile(outputPath, source, readError)) {
        DeleteFileW(outputPath.c_str());
        if (showFailureDialog) MessageBoxW(gMainWindow, widen(readError).c_str(), L"Storyland VCS SCM", MB_ICONERROR);
        return false;
    }
    DeleteFileW(outputPath.c_str());

    std::string parseError;
    if (!gScmFile.setDecompiledSource(source, modeId, parseError)) {
        if (showFailureDialog) MessageBoxW(gMainWindow, widen(parseError).c_str(), L"Storyland VCS SCM", MB_ICONERROR);
        return false;
    }
    applyScmMissionNames(gSannyBuilderPath);
    populateScmList();
    SetWindowTextW(gMainWindow, modeId == "vcs_psp" ? L"Storyland - VCS PSP SCM Mission Editor" : L"Storyland - VCS PS2 SCM Mission Editor");
    applyStorylandTitleTint(StorylandTitleTint::VCS);
    setStatus(L"Decompiled VCS SCM | " + widen(gScmFile.summaryLine()));
    return true;
}

static void refreshScmMissionTree() {
    if (gMode != StorylandMode::ScmFile) {
        MessageBoxW(gMainWindow, L"Open a .scm file first.", L"Storyland VCS SCM", MB_ICONINFORMATION);
        return;
    }
    if (!commitScmSourceEditor(true)) return;
    populateScmList();
    setStatus(L"Refreshed VCS mission tree | " + std::to_wstring(gScmFile.missions().size()) + L" mission blocks");
}

static void exportCurrentScmSource() {
    if (gMode != StorylandMode::ScmFile) {
        MessageBoxW(gMainWindow, L"Open a .scm file first.", L"Storyland VCS SCM", MB_ICONINFORMATION);
        return;
    }
    if (!commitScmSourceEditor(true)) return;
    if (!gScmFile.hasDecompiledSource()) {
        MessageBoxW(gMainWindow, L"Decompile this SCM before exporting source.", L"Storyland VCS SCM", MB_ICONINFORMATION);
        return;
    }

    std::wstring outputPath = saveFileDialog(L"Sanny Builder source\0*.txt\0All files\0*.*\0", L"txt");
    if (outputPath.empty()) return;
    std::string error;
    if (!writeUtf8TextFile(outputPath, widen(gScmFile.sourceText()), error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland VCS SCM source export", MB_ICONERROR);
        return;
    }
    setStatus(L"Exported decompiled VCS SCM source: " + outputPath);
}

static std::wstring findSannyCompileLog(const std::wstring& sannyExecutable,
                                        const std::wstring& sourcePath,
                                        const std::wstring& outputPath) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path(sourcePath).parent_path() / L"compile.log",
        std::filesystem::path(outputPath).parent_path() / L"compile.log",
        std::filesystem::path(sannyExecutable).parent_path() / L"compile.log"
    };
    for (const auto& candidate : candidates) if (fileExists(candidate.wstring())) return candidate.wstring();
    return L"";
}

static void compileCurrentScm() {
    if (gMode != StorylandMode::ScmFile) {
        MessageBoxW(gMainWindow, L"Open a .scm file first.", L"Storyland VCS SCM", MB_ICONINFORMATION);
        return;
    }
    if (!commitScmSourceEditor(true)) return;
    if (!gScmFile.hasDecompiledSource()) {
        MessageBoxW(gMainWindow, L"Decompile this SCM before compiling it.", L"Storyland VCS SCM", MB_ICONINFORMATION);
        return;
    }
    if (!ensureSannyBuilderConfigured(true)) return;

    std::wstring outputPath = saveFileDialog(L"Compiled VCS SCM\0*.scm\0All files\0*.*\0", L"scm");
    if (outputPath.empty()) return;
    if (sameFilesystemPathNoCase(outputPath, gScmFile.sourcePath())) {
        MessageBoxW(gMainWindow,
            L"Storyland will not overwrite the SCM that is currently open. Choose a different output file so the retail/source SCM stays intact.",
            L"Storyland VCS SCM",
            MB_ICONWARNING);
        return;
    }

    const std::wstring sourcePath = uniqueScmTempPath(L".txt");
    std::string writeError;
    if (!writeUtf8TextFile(sourcePath, widen(gScmFile.sourceText()), writeError)) {
        DeleteFileW(sourcePath.c_str());
        MessageBoxW(gMainWindow, widen(writeError).c_str(), L"Storyland VCS SCM compile", MB_ICONERROR);
        return;
    }

    DeleteFileW(outputPath.c_str());
    std::string mode = gScmFile.modeId().empty() ? "vcs_ps2" : gScmFile.modeId();
    DWORD exitCode = 0;
    std::string processError;
    std::vector<std::wstring> arguments = {
        L"--mode", widen(mode),
        L"--compile", sourcePath, outputPath
    };

    if (!runSannyBuilder(gSannyBuilderPath, arguments, exitCode, processError)) {
        DeleteFileW(sourcePath.c_str());
        MessageBoxW(gMainWindow, widen(processError).c_str(), L"Sanny Builder compile failed", MB_ICONERROR);
        return;
    }
    if (exitCode != 0 || !fileExists(outputPath)) {
        std::wstringstream message;
        message << L"Sanny Builder did not produce a compiled SCM.\r\nExit code: " << exitCode;
        std::wstring logPath = findSannyCompileLog(gSannyBuilderPath, sourcePath, outputPath);
        if (!logPath.empty()) {
            std::string logText;
            std::string readError;
            if (readUtf8OrAnsiTextFile(logPath, logText, readError) && !logText.empty()) {
                std::wstring wideLog = widen(logText);
                if (wideLog.size() > 7000) wideLog.resize(7000);
                message << L"\r\n\r\n" << wideLog;
            }
        }
        DeleteFileW(sourcePath.c_str());
        DeleteFileW(outputPath.c_str());
        MessageBoxW(gMainWindow, message.str().c_str(), L"Sanny Builder compile failed", MB_ICONERROR);
        return;
    }

    DeleteFileW(sourcePath.c_str());
    setStatus(L"Compiled " + widen(mode) + L" SCM: " + outputPath);
    MessageBoxW(gMainWindow, L"VCS SCM compiled successfully.", L"Storyland VCS SCM", MB_ICONINFORMATION);
}


static std::wstring formatMediaSeconds(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
    const int total = int(seconds + 0.5);
    const int minutes = total / 60;
    const int secs = total % 60;
    std::wstringstream out;
    out << minutes << L":" << std::setw(2) << std::setfill(L'0') << secs;
    return out.str();
}

static void updateMediaTimelineFromDecoder() {
    if (!gMediaTimeline) return;
    const bool video = gMode == StorylandMode::MediaFile && gMediaFile.kind() == StorylandMediaKind::Video;
    ShowWindow(gMediaTimeline, video ? SW_SHOW : SW_HIDE);
    if (!video) return;

    const uint64_t totalFrames = gMediaFile.videoFrameCountEstimate();
    const int maximum = int(std::max<uint64_t>(1u, std::min<uint64_t>(totalFrames > 0u ? totalFrames - 1u : 1u, 1000000u)));
    uint64_t currentFrame = gMediaFile.videoFrameIndex();
    int position = 0;
    if (totalFrames > 1u) {
        position = int(std::min<uint64_t>(uint64_t(maximum),
            (currentFrame * uint64_t(maximum)) / (totalFrames - 1u)));
    }
    gMediaTimelineInternalUpdate = true;
    SendMessageW(gMediaTimeline, TBM_SETRANGEMIN, FALSE, 0);
    SendMessageW(gMediaTimeline, TBM_SETRANGEMAX, FALSE, maximum);
    SendMessageW(gMediaTimeline, TBM_SETPAGESIZE, 0, std::max(1, maximum / 20));
    SendMessageW(gMediaTimeline, TBM_SETPOS, TRUE, position);
    gMediaTimelineInternalUpdate = false;
}

static void updateMediaVideoDetails() {
    if (gMediaFile.kind() != StorylandMediaKind::Video) return;
    std::wstringstream details;
    details << L"Video\r\n\r\n"
            << L"Path: " << gMediaFile.sourcePath() << L"\r\n"
            << L"Size: " << gMediaFile.videoWidth() << L"x" << gMediaFile.videoHeight() << L"\r\n"
            << L"Frame rate: " << std::fixed << std::setprecision(3) << gMediaFile.videoFrameRate() << L" fps\r\n"
            << L"Frame: " << gMediaFile.videoFrameIndex() << L"\r\n"
            << L"Position: " << std::fixed << std::setprecision(3) << gMediaFile.videoPositionSeconds() << L" s\r\n"
            << L"Duration: " << formatMediaSeconds(gMediaFile.videoDurationSeconds()) << L"\r\n"
            << L"Decoder: FFmpeg/Media Foundation game-video path\r\n"
            << L"Frame scrubber: frame-accurate seek\r\n"
            << L"Current frame replacement: " << (gMediaFile.currentVideoFrameIsOverridden() ? L"edited in current session" : L"original decoded frame") << L"\r\n"
            << L"Frame stepping: Previous Frame / Next Frame\r\n";
    setDetails(details.str());
}

static void drawMediaFrameImage(HDC dc, const RECT& bounds, const StorylandVideoFrame& frame) {
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top ||
        frame.width == 0u || frame.height == 0u ||
        frame.bgra.size() < uint64_t(frame.width) * uint64_t(frame.height) * 4u) return;

    const double scale = std::min(double(bounds.right - bounds.left) / double(frame.width),
                                  double(bounds.bottom - bounds.top) / double(frame.height));
    const int drawW = std::max(1, int(double(frame.width) * scale));
    const int drawH = std::max(1, int(double(frame.height) * scale));
    const int x = bounds.left + (bounds.right - bounds.left - drawW) / 2;
    const int y = bounds.top + (bounds.bottom - bounds.top - drawH) / 2;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = LONG(frame.width);
    info.bmiHeader.biHeight = -LONG(frame.height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, x, y, drawW, drawH, 0, 0, int(frame.width), int(frame.height),
                  frame.bgra.data(), &info, DIB_RGB_COLORS, SRCCOPY);
}

static void drawMediaPreview(HDC dc, RECT rc) {
    HBRUSH background = CreateSolidBrush(storylandPaneBackgroundColor());
    FillRect(dc, &rc, background);
    DeleteObject(background);

    if (gMediaFile.kind() == StorylandMediaKind::Video) {
        const StorylandVideoFrame& frame = gMediaFile.videoFrame();
        RECT frameImage{rc.left + 8, rc.top + 30, rc.right - 8, rc.bottom - 30};
        drawMediaFrameImage(dc, frameImage, frame);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, storylandPaneTextColor());
        std::wstringstream playerStatus;
        playerStatus << L"Video  " << gMediaFile.videoWidth() << L"x" << gMediaFile.videoHeight()
                     << L"  |  frame " << gMediaFile.videoFrameIndex() << L" / "
                     << (gMediaFile.videoFrameCountEstimate() > 0u ? gMediaFile.videoFrameCountEstimate() - 1u : 0u)
                     << L"  |  " << std::fixed << std::setprecision(3) << gMediaFile.videoPositionSeconds() << L" s";
        if (gMediaFile.currentVideoFrameIsOverridden()) playerStatus << L"  |  REPLACED";
        const std::wstring status = playerStatus.str();
        TextOutW(dc, rc.left + 10, rc.top + 7, status.c_str(), int(status.size()));

        const wchar_t* hint = L"Drag the frame slider to scrub like a video editor. Right-click and choose Replace Current Frame Image to edit this frame.";
        TextOutW(dc, rc.left + 10, rc.bottom - 22, hint, int(wcslen(hint)));
        if (frame.bgra.empty()) {
            const wchar_t* prompt = L"Press Play, step a frame, or drag the timeline to decode a frame.";
            TextOutW(dc, frameImage.left + 4, frameImage.top + 4, prompt, int(wcslen(prompt)));
        }
        return;
    }

    const auto& clips = gMediaFile.clips();
    const int selected = gMediaFile.selectedClip();
    if (selected < 0 || size_t(selected) >= clips.size()) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, storylandPaneTextColor());
        const wchar_t* text = L"No audio clip selected.";
        TextOutW(dc, rc.left + 12, rc.top + 12, text, int(wcslen(text)));
        return;
    }

    const StorylandMediaClip& clip = clips[size_t(selected)];
    RECT wave = rc;
    InflateRect(&wave, -14, -36);
    if (wave.right <= wave.left || wave.bottom <= wave.top || clip.pcm.empty()) return;

    HPEN axisPen = CreatePen(PS_SOLID, 1, gEyeFriendlyPaneBackground ? RGB(84, 88, 98) : RGB(150, 150, 150));
    HGDIOBJ oldPen = SelectObject(dc, axisPen);
    const int centerY = (wave.top + wave.bottom) / 2;
    MoveToEx(dc, wave.left, centerY, nullptr);
    LineTo(dc, wave.right, centerY);
    SelectObject(dc, oldPen);
    DeleteObject(axisPen);

    HPEN wavePen = CreatePen(PS_SOLID, 1, storylandPaneBorderColor());
    oldPen = SelectObject(dc, wavePen);
    const int width = std::max<int>(1, int(wave.right - wave.left));
    const size_t channels = std::max<size_t>(1u, clip.channels);
    const size_t frameCount = clip.pcm.size() / channels;
    for (int x = 0; x < width; ++x) {
        const size_t begin = (uint64_t(x) * frameCount) / size_t(width);
        const size_t end = std::max(begin + 1u, (uint64_t(x + 1) * frameCount) / size_t(width));
        int minSample = 32767;
        int maxSample = -32768;
        for (size_t frameIndex = begin; frameIndex < std::min(end, frameCount); ++frameIndex) {
            int mixed = 0;
            for (size_t channel = 0; channel < channels; ++channel) mixed += clip.pcm[frameIndex * channels + channel];
            mixed /= int(channels);
            minSample = std::min(minSample, mixed);
            maxSample = std::max(maxSample, mixed);
        }
        const int halfH = std::max<int>(1, int((wave.bottom - wave.top) / 2 - 2));
        const int y1 = centerY - (maxSample * halfH) / 32768;
        const int y2 = centerY - (minSample * halfH) / 32768;
        MoveToEx(dc, wave.left + x, y1, nullptr);
        LineTo(dc, wave.left + x, y2 + 1);
    }
    SelectObject(dc, oldPen);
    DeleteObject(wavePen);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, storylandPaneTextColor());
    std::wstring title = widen(clip.name) + L"  |  " + std::to_wstring(clip.sampleRate) + L" Hz  |  " +
                         std::to_wstring(clip.channels) + L" ch  |  " + formatMediaSeconds(clip.durationSeconds);
    TextOutW(dc, rc.left + 12, rc.top + 10, title.c_str(), int(title.size()));
}

static void selectMediaClip(int index, bool autoplay) {
    if (index < 0 || size_t(index) >= gMediaFile.clips().size()) return;
    std::string error;
    if (!gMediaFile.selectClip(size_t(index), error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland audio", MB_ICONERROR);
        return;
    }
    gSelectedKind = StorylandTreeKind::MediaClip;
    gSelectedIndex = index;
    const StorylandMediaClip& clip = gMediaFile.clips()[size_t(index)];
    std::wstringstream details;
    details << L"Audio clip\r\n\r\n"
            << L"Name: " << widen(clip.name) << L"\r\n"
            << L"Offset: " << hexWide(uint32_t(std::min<uint64_t>(clip.sourceOffset, 0xFFFFFFFFull))) << L"\r\n"
            << L"Stored bytes: " << clip.sourceSize << L"\r\n"
            << L"Sample rate: " << clip.sampleRate << L" Hz\r\n"
            << L"Channels: " << clip.channels << L"\r\n"
            << L"Decoded PCM samples: " << clip.pcm.size() << L"\r\n"
            << L"Duration: " << formatMediaSeconds(clip.durationSeconds) << L"\r\n";
    setDetails(details.str());
    if (autoplay) {
        if (!gMediaFile.play(error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland audio playback", MB_ICONERROR);
        }
    }
    updateActionBar();
    layoutChildren(gMainWindow);
    InvalidateRect(gPreview, nullptr, TRUE);
    setStatus(L"Audio | " + widen(gMediaFile.summary()));
}

static void selectMediaPayload(const StorylandTreePayload& payload) {
    if (payload.kind == StorylandTreeKind::MediaClip) {
        selectMediaClip(payload.index, true);
    } else if (payload.kind == StorylandTreeKind::MediaOverview) {
        gSelectedKind = StorylandTreeKind::MediaOverview;
        gSelectedIndex = 0;
        setDetails(widen(gMediaFile.summary()));
        InvalidateRect(gPreview, nullptr, TRUE);
    }
}

static void populateMediaList() {
    clearView();
    HTREEITEM root = addTreeItem(TVI_ROOT, L"Media");
    addTreeItem(root, widen(gMediaFile.summary()), StorylandTreeKind::MediaOverview, 0);
    if (gMediaFile.kind() == StorylandMediaKind::Audio || gMediaFile.kind() == StorylandMediaKind::AudioArchive) {
        HTREEITEM sounds = addTreeItem(root, L"Sounds (select to play)");
        const auto& clips = gMediaFile.clips();
        for (size_t index = 0; index < clips.size(); ++index) {
            const auto& clip = clips[index];
            std::wstringstream label;
            label << L"#" << index << L"  " << widen(clip.name)
                  << L"  " << clip.sampleRate << L" Hz  " << formatMediaSeconds(clip.durationSeconds);
            addTreeItem(sounds, label.str(), StorylandTreeKind::MediaClip, int(index));
        }
        expandTreeItem(sounds);
        if (!clips.empty()) selectMediaClip(0, false);
    } else {
        updateMediaVideoDetails();
    }
    expandTreeItem(root);
}

static void openStorylandFile(const std::wstring& path) {
    if (gAnalyzeGraphActive) {
        gAnalyzeGraphActive = false;
        gAnalysisGraphBytes.clear();
        gAnalysisGraphName.clear();
        ShowWindow(gPreview, SW_SHOW);
    }
    if (path.empty()) return;
    if (!gOpeningDtzStandaloneChild) {
        gDtzReturnAvailable = false;
        gDtzReturnSelectedIndex = -1;
        gDtzReturnTreeKind = StorylandTreeKind::None;
        gDtzReturnTreeIndex = -1;
    }
    std::wstring ext = getExtensionLower(path);
    std::string error;
    if (!gOpeningDtzStandaloneChild &&
        (ext == L".anim" || ext == L".chk" || ext == L".xtx" || ext == L".tex" || ext == L".txd" ||
         ext == L".img" || ext == L".lvz" || ext == L".wbl" || ext == L".dir" || ext == L".scm" ||
         ext == L".dtz" || ext == L".bin" || ext == L".mdl" || ext == L".dff" ||
         isStorylandMediaExtension(ext))) {
        addRecentFile(path);
    }

    if (isStorylandMediaExtension(ext)) {
        if (!gMediaFile.loadFromFile(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland media open failed", MB_ICONERROR);
            return;
        }
        clearView();
        gMode = StorylandMode::MediaFile;
        populateMediaList();
        SetWindowTextW(gMainWindow,
            gMediaFile.kind() == StorylandMediaKind::Video ? L"Storyland - Video Player" : L"Storyland - Audio Player");
        applyStorylandTitleTintForPath(path);
        refreshModeUi();
        InvalidateRect(gPreview, nullptr, TRUE);
        setStatus(L"Media | " + widen(gMediaFile.summary()));
        return;
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
        refreshModeUi();
        return;
    }

    clearView();

    if (ext == L".scm") {
        if (!gScmFile.loadFromFile(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland VCS SCM open failed", MB_ICONERROR);
            return;
        }
        gMode = StorylandMode::ScmFile;
        SetWindowTextW(gMainWindow, L"Storyland - VCS SCM Mission Editor");
        applyStorylandTitleTint(StorylandTitleTint::VCS);
        refreshModeUi();

        const std::string preferredMode = containsWideNoCase(path, L"psp") ? "vcs_psp" : "vcs_ps2";
        if (!decompileCurrentScm(preferredMode, false)) {
            const std::wstring sanny = findSannyBuilderExecutable();
            if (!sanny.empty()) applyScmMissionNames(sanny, preferredMode);
            populateScmList();
            if (gScmFile.hasNativeMissionTable()) {
                setStatus(L"Opened VCS SCM | native mission table: " + std::to_wstring(gScmFile.missions().size()) +
                          L" missions | configure/decompile with Sanny to edit bytecode as source");
            } else {
                setStatus(L"Opened VCS SCM | native Stories mission table was not recognized | configure Sanny to decompile");
            }
        }
        return;
    }

    if (ext == L".chk" || ext == L".xtx" || ext == L".tex" || ext == L".txd") {
        if (!gTextureArchive.loadFromFile(path, LeedsPlatform::Auto, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland texture open failed", MB_ICONERROR);
            return;
        }

        std::wstring clampSummary;
        if (!clampEditableStoriesTexturesTo8Bpp(clampSummary, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Texture BPP conversion failed", MB_ICONERROR);
            return;
        }

        gMode = StorylandMode::TextureArchive;
        populateTextureList();

        const auto& textureBytes = gTextureArchive.rawBytes();
        const bool gtaSaTxd =
            ext == L".txd" &&
            textureBytes.size() >= 12u &&
            textureBytes[0] == 0x16u &&
            textureBytes[1] == 0x00u &&
            textureBytes[2] == 0x00u &&
            textureBytes[3] == 0x00u &&
            (uint32_t(textureBytes[8]) |
             (uint32_t(textureBytes[9]) << 8u) |
             (uint32_t(textureBytes[10]) << 16u) |
             (uint32_t(textureBytes[11]) << 24u)) ==
                0x1803FFFFu;

        SetWindowTextW(
            gMainWindow,
            gtaSaTxd ? L"Storyland - GTA SA TXD Editor" :
            ext == L".txd" ? L"Storyland - LCS Beta TXD Editor" :
            ext == L".chk" ? L"Storyland - CHK Editor" :
            ext == L".xtx" ? L"Storyland - XTX Editor" :
                             L"Storyland - Texture Archive");
        applyStorylandTitleTint(
            gtaSaTxd
                ? StorylandTitleTint::SanAndreas
                : titleTintFromPath(path));
        refreshModeUi();
        if (!clampSummary.empty()) {
            setStatus(L"Opened texture archive | " + clampSummary);
        }
        return;
    }

    if (ext == L".img") {
        if (gMode == StorylandMode::DtzArchive) {
            if (!gDtzArchive.loadCompanionImg(path, error)) {
                MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland DTZ+IMG open failed", MB_ICONERROR);
                return;
            }
            loadAreaArchivesForCurrentDtz();
            clearDtzFindState(true);
            populateDtzList();
            SetWindowTextW(gMainWindow, L"Storyland - GAME.DTZ + gta3PS*.img");
            applyStorylandTitleTint(gTitleTint == StorylandTitleTint::Default ? StorylandTitleTint::LCS : gTitleTint);
            setStatus(L"Loaded companion IMG for current GAME.DTZ: " + path);
            return;
        }

        // Retail Stories IMG files should bind to GAME.DTZ first when it is
        // available.  GAME.DTZ is the authoritative streaming map; raw signature
        // scanning is only a fallback for incomplete/test archives.
        std::string dtzPairError;
        if (openImgWithGameDtzPair(path, dtzPairError)) {
            return;
        }

        std::string lvzError;
        if (gArchiveBrowser.loadImgFromFile(path, lvzError)) {
            gActiveAreaArchive = -1;
            gMode = StorylandMode::ArchiveFile;
            resetModelViewport();
            gModelDistance = 8.0f;
            populateArchiveList();
            SetWindowTextW(gMainWindow, L"Storyland - IMG Browser");
            applyStorylandTitleTint(titleTintFromPath(path));
            refreshModeUi();
            return;
        }

        MessageBoxW(gMainWindow, (widen(lvzError) + L"\r\n\r\nGAME.DTZ pairing was also unavailable/invalid:\r\n" + widen(dtzPairError)).c_str(), L"Storyland IMG open failed", MB_ICONERROR);
        return;
    }

    if (ext == L".lvz") {
        gActiveAreaArchive = -1;
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
        refreshModeUi();
        return;
    }

    if (ext == L".zmg") {
        gActiveAreaArchive = -1;
        if (!gArchiveBrowser.loadZmgFromFile(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland ZMG open failed", MB_ICONERROR);
            return;
        }
        gMode = StorylandMode::ArchiveFile;
        resetModelViewport();
        populateArchiveList();
        SetWindowTextW(gMainWindow, L"Storyland - LCS Beta ZMG Analyzer");
        applyStorylandTitleTint(StorylandTitleTint::LCS);
        refreshModeUi();
        setStatus(L"Opened LCS beta ZMG | " + widen(gArchiveBrowser.levelSummary()));
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
        refreshModeUi();
        InvalidateRect(gPreview, nullptr, FALSE);
        return;
    }

    if (ext == L".dir") {
        if (gMode == StorylandMode::DtzArchive) {
            if (!gDtzArchive.loadCompanionDir(path, error)) {
                MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland DIR open failed", MB_ICONERROR);
                return;
            }
            clearDtzFindState(true);
            populateDtzList();
            SetWindowTextW(gMainWindow, L"Storyland - GAME.DTZ + GTA IMG/DIR");
            applyStorylandTitleTint(gTitleTint == StorylandTitleTint::Default ? StorylandTitleTint::LCS : gTitleTint);
            return;
        }

        if (!gArchiveBrowser.loadDirWithCompanionImg(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland beta IMG/DIR open failed", MB_ICONERROR);
            return;
        }
        gActiveAreaArchive = -1;
        gMode = StorylandMode::ArchiveFile;
        resetModelViewport();
        populateArchiveList();
        SetWindowTextW(gMainWindow, L"Storyland - LCS Beta IMG/DIR Browser");
        applyStorylandTitleTint(StorylandTitleTint::LCS);
        refreshModeUi();
        setStatus(L"Opened LCS beta/classic IMG/DIR | " + widen(gArchiveBrowser.levelSummary()));
        return;
    }

    if (ext == L".dtz" || ext == L".bin") {
        if (!gDtzArchive.loadFromFile(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland DTZ open failed", MB_ICONERROR);
            return;
        }
        loadAreaArchivesForCurrentDtz();
        clearDtzFindState(true);
        gMode = StorylandMode::DtzArchive;
        resetModelViewport();
        gModelDistance = 4.0f;
        populateDtzList();
        SetWindowTextW(gMainWindow, gDtzArchive.hasCompanionImg() ? L"Storyland - GAME.DTZ + gta3PS*.img" : L"Storyland - GAME.DTZ");
        StorylandTitleTint dtzTint = titleTintFromPath(path);
        if (dtzTint == StorylandTitleTint::Default) dtzTint = StorylandTitleTint::LCS;
        applyStorylandTitleTint(dtzTint);
        refreshModeUi();
        InvalidateRect(gPreview, nullptr, FALSE);
        return;
    }

    if (ext == L".mdl" || ext == L".dff") {
        // A standalone model opened from File > Open must completely replace any
        // GAME.DTZ/IMG embedded-resource selection.  Otherwise the details pane
        // and renderer label can continue to describe the previously selected
        // resource (for example plr.mdl) after another MDL has been opened.
        clearDtzEmbeddedPreviewState();
        gActiveAreaArchive = -1;
        gSelectedKind = StorylandTreeKind::None;
        gSelectedIndex = -1;
        gDtzReturnAvailable = false;
        gDtzReturnSelectedIndex = -1;
        gDtzReturnTreeKind = StorylandTreeKind::None;
        gDtzReturnTreeIndex = -1;
        setDetails(L"");

        gModelAnimLoaded = false;
        gModelAnimPath.clear();
        gModelAnimStatus.clear();

        gModelTextureVAuto = false;
        gModelFlipTextureV = false;
        gModelDetectedFlipTextureV = false;
        if (!gModelFile.loadFromFile(path, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland MDL open failed", MB_ICONERROR);
            return;
        }
        gMode = StorylandMode::ModelFile;
        resetModelViewport();
        populateModelList();
        selectModelField(0);

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

        if (!gModelDffStructureTreeActive) selectModelField(0);
        if (gModelFile.isMobileLcsDff()) SetWindowTextW(gMainWindow, L"Storyland - Mobile LCS DFF Viewer");
        else if (gModelFile.isPspNativeDff()) SetWindowTextW(gMainWindow, L"Storyland - LCS PSP DFF Viewer");
        else if (gModelFile.isGtaSaDff()) SetWindowTextW(gMainWindow, L"Storyland - GTA SA DFF Viewer");
        else SetWindowTextW(gMainWindow, gModelAnimLoaded ? L"Storyland - MDL Viewer + ANIM" : L"Storyland - MDL Viewer");

        StorylandTitleTint modelTint =
            gModelFile.isGtaSaDff()
                ? StorylandTitleTint::SanAndreas
                : (gModelFile.isMobileLcsDff() || gModelFile.isPspNativeDff())
                    ? StorylandTitleTint::LCS
                    : titleTintFromPath(path);
        if (gModelFile.isPmlcMdl()) modelTint = StorylandTitleTint::BloodRed;
        else if (modelTint == StorylandTitleTint::Default && !gModelTexturePath.empty()) modelTint = titleTintFromPath(gModelTexturePath);
        applyStorylandTitleTint(modelTint);
        refreshModeUi();
        setStatus(buildModelStatusLine());
        InvalidateRect(gPreview, nullptr, FALSE);
        return;
    }

    MessageBoxW(gMainWindow, L"Unknown extension. Storyland opens SCM, GAME.DTZ, IMG/DIR/LVZ/ZMG, WBL, MDL/DFF, ANIM, CHK/XTX/TEX/TXD, SDT/RAW/VAG/WAV/AT3 audio, and PSS/PMF/common video files.", L"Storyland", MB_ICONINFORMATION);
}

static bool writeWholeFileBinary(const std::wstring& path, const std::vector<uint8_t>& bytes, std::string& error) {
    return storylandWriteFilesTransaction({{std::filesystem::path(path), &bytes}}, error);
}

static std::wstring buildArchiveExtractRoot() {
    wchar_t tempPath[MAX_PATH] = {};
    DWORD len = GetTempPathW(MAX_PATH, tempPath);
    std::wstring root = len > 0 ? std::wstring(tempPath) : L".";
    if (!root.empty() && root.back() != L'\\' && root.back() != L'/') root += L"\\";
    root += L"StorylandEmbedded_" + std::to_wstring(GetCurrentProcessId());
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
    if (gMode == StorylandMode::DtzArchive) {
        if (gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile) return true;
        if (gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::None && !gDtzArchive.leeds2dfxEffects().empty()) return true;
    }
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

    std::wstring displayName = widenResourceName(entry.name);
    std::wstring name = safeEmbeddedFileName(displayName, L"resource.bin");
    std::wstring ext = getExtensionLower(name);
    if (ext == L".wrld" || ext == L".area") {
        selectArchiveEntry(gSelectedIndex);
        setStatus(L"WRLD/AREA sector is shown alone in the LVZ/IMG OpenGL preview.");
        return;
    }
    if (!(ext == L".mdl" || ext == L".dff" || ext == L".xtx" || ext == L".chk" || ext == L".tex" || ext == L".txd" || ext == L".dtz")) {
        selectArchiveEntry(gSelectedIndex);
        setStatus(L"Selected embedded resource is listed but not directly openable. Only recognized model, texture, and DTZ entries can be entered safely.");
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
        bool foundCompanion = gArchiveBrowser.findEntryByStemAndExtension(stem, {L".xtx", L".chk", L".tex", L".txd"}, companionIndex);
        bool mobileMatchedCompanion = false;
        if (!foundCompanion && gArchiveBrowser.findMobileLcsTextureDictionaryForEntry(size_t(gSelectedIndex), companionIndex)) {
            foundCompanion = true;
            mobileMatchedCompanion = true;
        }
        if (foundCompanion) {
            std::vector<uint8_t> textureBytes;
            if (gArchiveBrowser.extractEntryBytes(companionIndex, textureBytes, error)) {
                std::wstring companionName = mobileMatchedCompanion
                    ? stem + L".txd"
                    : widen(gArchiveBrowser.entries()[companionIndex].name);
                companionName = safeEmbeddedFileName(companionName, L"texture.xtx");
                std::wstring companionPath = extractRoot + L"\\" + companionName;
                writeWholeFileBinary(companionPath, textureBytes, error);
            }
        }
    }

    openStorylandFile(primaryPath);
}

static bool writeUtf8TextFile(const std::wstring& path, const std::wstring& text, std::string& error) {
    if (text.size() > size_t((std::numeric_limits<int>::max)())) {
        error = "Text output is too large to encode safely.";
        return false;
    }
    int bytesNeeded = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    if (!text.empty() && bytesNeeded <= 0) {
        error = "Could not convert text to UTF-8.";
        return false;
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(size_t(bytesNeeded) + 3u);
    bytes.push_back(0xEFu);
    bytes.push_back(0xBBu);
    bytes.push_back(0xBFu);
    if (bytesNeeded > 0) {
        const size_t oldSize = bytes.size();
        bytes.resize(oldSize + size_t(bytesNeeded));
        if (WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()),
                                reinterpret_cast<char*>(bytes.data() + oldSize), bytesNeeded,
                                nullptr, nullptr) != bytesNeeded) {
            error = "Could not convert text to UTF-8.";
            return false;
        }
    }
    return storylandWriteFilesTransaction({{std::filesystem::path(path), &bytes}}, error);
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

    if (gMode == StorylandMode::ScmFile) {
        const StorylandScmHeader& header = gScmFile.header();
        ss << L"Mode: Stories SCM / VCS mission script\r\n";
        ss << L"Path: " << gScmFile.sourcePath() << L"\r\n";
        ss << L"Raw size: " << gScmFile.rawBytes().size() << L" bytes\r\n";
        ss << L"Sanny mode: " << (gScmFile.modeId().empty() ? L"not decompiled" : widen(gScmFile.modeId())) << L"\r\n";
        ss << L"Source loaded: " << (gScmFile.hasDecompiledSource() ? L"yes" : L"no") << L"\r\n";
        ss << L"Native header: " << (header.valid ? L"decoded" : L"not recognized") << L"\r\n";
        if (header.valid) {
            ss << L"Target: " << wchar_t(static_cast<unsigned char>(header.targetGame)) << L"\r\n";
            ss << L"Main script offset: 0x" << std::hex << std::uppercase << header.mainScriptOffset << std::dec << L"\r\n";
            ss << L"Main script size: " << header.mainScriptSize << L" bytes\r\n";
            ss << L"Global variable bytes: " << header.globalVariableBytes << L"\r\n";
            ss << L"Save variables: " << header.saveVariableCount << L"\r\n";
            ss << L"Objects: " << header.objectCount << L"\r\n";
            ss << L"True globals: " << header.trueGlobalCount << L"\r\n";
            ss << L"Most globals: " << header.mostGlobalCount << L"\r\n";
            ss << L"Largest mission: " << header.largestMissionScriptSize << L" bytes\r\n";
            ss << L"Mission count: " << header.missionCount << L"\r\n";
            ss << L"Exclusive mission count: " << header.exclusiveMissionCount << L"\r\n";
            ss << L"Mission table offset: 0x" << std::hex << std::uppercase << header.missionTableOffset << std::dec << L"\r\n";
        }
        ss << L"\r\nMissions\r\n--------\r\n";
        const auto& missions = gScmFile.missions();
        for (const StorylandScmMission& mission : missions) {
            ss << L"#" << mission.id << L" " << widen(mission.name);
            if (!mission.label.empty()) ss << L" @" << widen(mission.label);
            if (mission.hasBinaryRange) {
                ss << L" | binary=0x" << std::hex << std::uppercase << mission.binaryOffset
                   << L" size=0x" << mission.binarySize << std::dec;
            }
            if (!mission.source.empty()) {
                ss << L" | source=" << mission.sourceBegin << L".." << mission.sourceEnd;
            }
            ss << L"\r\n";
        }
    } else if (gMode == StorylandMode::ModelFile) {
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
            ss << widenResourceName(entry.name) << L" | index=" << entry.dirIndex
               << L" start=" << entry.startSector
               << L" count=" << entry.sectorCount
               << L" end=" << (entry.startSector + entry.sectorCount)
               << L" bytes=" << (entry.sectorCount * 2048u)
               << L" dtz_matches=" << entry.matchingRecordIndices.size() << L"\r\n";
        }
        ss << L"\r\nRaw sector pairs\r\n----------------\r\n";
        for (const auto& r : gDtzArchive.sectorRecords()) {
            ss << hexWide(r.recordOffset, 6) << L" | " << widenResourceName(r.resourceName)
               << L" start=" << r.startSector
               << L" count=" << r.sectorCount
               << L" end=" << (r.startSector + r.sectorCount)
               << L" startField=" << hexWide(r.startOffset, 6)
               << L" countField=" << hexWide(r.countOffset, 6)
               << L" | " << widen(r.note) << L"\r\n";
        }
    } else if (gMode == StorylandMode::ArchiveFile) {
        ss << (gArchiveBrowser.hasLvzContext() ? L"Mode: LVZ + IMG archive\r\n\r\n" : L"Mode: Mobile LCS raw gta3.img archive\r\n\r\n");
        if (gArchiveBrowser.hasLvzContext()) ss << L"LVZ: " << gArchiveBrowser.lvzPath() << L"\r\n";
        if (gArchiveBrowser.hasImgContext()) ss << L"IMG: " << gArchiveBrowser.imgPath() << L"\r\n";
        ss << (gArchiveBrowser.hasLvzContext() ? L"DIR: none; retail LVZ+IMG mode\r\n" : L"Directory: sector-scanned Mobile LCS RenderWare clumps; no GAME.DTZ/LVZ/DIR\r\n");
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
            ss << widenResourceName(entry.name)
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
    setStatus(L"Mesh resource id changed in memory; right-click the archive tree to rebuild or overwrite the LVZ + IMG pair.");
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

    setStatus(L"Sector mesh resource cloned in memory; right-click the archive tree to rebuild or overwrite the LVZ + IMG pair.");
}

static void addResourceToCurrentArchive() {
    if (gMode != StorylandMode::ArchiveFile || !gArchiveBrowser.hasLvzContext() || !gArchiveBrowser.hasImgContext()) {
        MessageBoxW(gMainWindow,
            L"Open a retail LVZ + IMG pair before adding a resource.",
            L"Add Resource", MB_ICONINFORMATION);
        return;
    }

    std::wstring sourcePath = openFileDialog(
        L"Leeds resources\0*.mdl;*.xtx;*.chk;*.tex\0Models\0*.mdl\0Textures\0*.xtx;*.chk;*.tex\0All files\0*.*\0");
    if (sourcePath.empty()) return;

    std::vector<uint8_t> bytes;
    std::string readError;
    if (!readBinaryFileForUi(sourcePath, bytes, readError)) {
        MessageBoxW(gMainWindow, widen(readError).c_str(), L"Add Resource", MB_ICONERROR);
        return;
    }

    std::string resourceName = narrow(std::filesystem::path(sourcePath).filename().wstring());
    std::string report;
    std::string error;
    if (!gArchiveBrowser.addResourceBytes(resourceName, bytes, report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Add Resource failed", MB_ICONERROR);
        return;
    }

    populateArchiveList();
    const auto& entries = gArchiveBrowser.entries();
    int addedIndex = -1;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (_stricmp(entries[i].name.c_str(), resourceName.c_str()) == 0) {
            addedIndex = int(i);
            break;
        }
    }
    if (addedIndex >= 0) {
        selectTreePayloadItem(StorylandTreeKind::ArchiveEntry, addedIndex);
        gSelectedKind = StorylandTreeKind::ArchiveEntry;
        gSelectedIndex = addedIndex;
    }

    setDetails(widen(report));
    setStatus(L"Added " + std::filesystem::path(sourcePath).filename().wstring() +
              L" to the loaded LVZ + IMG pair in memory.");
    refreshModeUi();
    InvalidateRect(gPreview, nullptr, FALSE);
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

    std::wstring path = openFileDialog(L"BLeeds / Leeds resource\0*.mdl;*.dff;*.wbl;*.xtx;*.chk;*.tex;*.txd;*.wrld;*.area;*.bin\0All files\0*.*\0");
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

        setStatus(L"Sector mesh resource replaced in memory; right-click the archive tree to rebuild or overwrite the LVZ + IMG pair.");
        return;
    }

    const StorylandArchiveEntry selectedBeforeReplace = gArchiveBrowser.entries()[size_t(gSelectedIndex)];
    if (!gArchiveBrowser.replaceEntryBytes(size_t(gSelectedIndex), replacementBytes, report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"LVZ+IMG replacement failed", MB_ICONERROR);
        return;
    }

    populateArchiveList();
    int rebuiltIndex = -1;
    const auto& rebuiltEntries = gArchiveBrowser.entries();
    for (size_t i = 0; i < rebuiltEntries.size(); ++i) {
        const StorylandArchiveEntry& entry = rebuiltEntries[i];
        const bool sameIdent = entry.chunkIdent == selectedBeforeReplace.chunkIdent ||
            (archiveEntryIsWorld(entry) && archiveEntryIsWorld(selectedBeforeReplace));
        if (entry.byteOffset == selectedBeforeReplace.byteOffset && sameIdent) {
            rebuiltIndex = int(i);
            break;
        }
    }
    if (rebuiltIndex >= 0) {
        selectTreePayloadItem(StorylandTreeKind::ArchiveEntry, rebuiltIndex);
        selectArchiveEntry(rebuiltIndex);
    }
    setDetails(widen(report));
    setStatus(L"LVZ+IMG resource replaced in-place and verified; original IMG offsets remain stable.");
}

static void testCurrentLvzImgPair() {
    if (gMode != StorylandMode::ArchiveFile || !gArchiveBrowser.hasLvzContext() || !gArchiveBrowser.hasImgContext()) {
        MessageBoxW(gMainWindow, L"Open an LVZ + IMG pair first.", L"Test LVZ/IMG Pair", MB_ICONINFORMATION);
        return;
    }
    std::string report;
    std::string error;
    if (!gArchiveBrowser.validateLvzImgPair(report, error)) {
        setDetails(widen(error));
        MessageBoxW(gMainWindow, L"LVZ/IMG pair test failed. The detailed report is shown in the lower pane.", L"Test LVZ/IMG Pair", MB_ICONERROR);
        setStatus(L"LVZ/IMG test: FAIL");
        return;
    }
    setDetails(widen(report));
    setStatus(L"LVZ/IMG test: PASS - archive ranges, WRLD resources, geometry, and PS2 DMA/VIF/GIF structural checks passed.");
}

static StorylandVideoFrame makeReplacementVideoFrame(const RgbaImage& image, uint32_t width, uint32_t height) {
    StorylandVideoFrame frame;
    frame.width = width;
    frame.height = height;
    if (width == 0u || height == 0u || image.width <= 0 || image.height <= 0 || image.rgba.empty()) return frame;
    frame.bgra.resize(size_t(width) * size_t(height) * 4u);
    for (uint32_t y = 0; y < height; ++y) {
        const uint32_t sy = std::min<uint32_t>(uint32_t(image.height - 1), uint32_t((uint64_t(y) * uint64_t(image.height)) / height));
        for (uint32_t x = 0; x < width; ++x) {
            const uint32_t sx = std::min<uint32_t>(uint32_t(image.width - 1), uint32_t((uint64_t(x) * uint64_t(image.width)) / width));
            const size_t src = (size_t(sy) * size_t(image.width) + size_t(sx)) * 4u;
            const size_t dst = (size_t(y) * size_t(width) + size_t(x)) * 4u;
            frame.bgra[dst + 0] = image.rgba[src + 2];
            frame.bgra[dst + 1] = image.rgba[src + 1];
            frame.bgra[dst + 2] = image.rgba[src + 0];
            frame.bgra[dst + 3] = image.rgba[src + 3];
        }
    }
    return frame;
}

static void replaceCurrentVideoFrameImage() {
    if (gMode != StorylandMode::MediaFile || gMediaFile.kind() != StorylandMediaKind::Video || gMediaFile.videoFrame().bgra.empty()) {
        MessageBoxW(gMainWindow, L"Decode or scrub to a video frame first.", L"Replace Current Frame Image", MB_ICONINFORMATION);
        return;
    }
    const std::wstring path = openFileDialog(L"Image\0*.png;*.bmp;*.jpg;*.jpeg;*.tif;*.tiff\0All files\0*.*\0");
    if (path.empty()) return;
    RgbaImage image;
    std::string error;
    if (!loadImageWithWic(path, image, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Frame image load failed", MB_ICONERROR);
        return;
    }
    StorylandVideoFrame replacement = makeReplacementVideoFrame(image, gMediaFile.videoWidth(), gMediaFile.videoHeight());
    if (!gMediaFile.replaceCurrentVideoFrame(replacement, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Replace Current Frame Image failed", MB_ICONERROR);
        return;
    }
    updateMediaVideoDetails();
    updateMediaTimelineFromDecoder();
    InvalidateRect(gPreview, nullptr, FALSE);
    setStatus(L"Replaced decoded frame " + std::to_wstring(gMediaFile.videoFrameIndex()) + L" in the current editing session. Scrubbing away and back preserves the frame edit.");
}

static void exportSelectedResourceBytes() {
    std::vector<uint8_t> bytes;
    std::wstring suggestedName;
    std::string error;

    if (gMode == StorylandMode::ArchiveFile &&
        gSelectedKind == StorylandTreeKind::ArchiveEntry && gSelectedIndex >= 0 &&
        size_t(gSelectedIndex) < gArchiveBrowser.entries().size()) {
        const StorylandArchiveEntry& entry = gArchiveBrowser.entries()[size_t(gSelectedIndex)];
        suggestedName = widenResourceName(entry.name);
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
        L"Leeds resource\0*.mdl;*.dff;*.wbl;*.xtx;*.chk;*.tex;*.txd;*.wrld;*.area;*.anim;*.bin\0All files\0*.*\0",
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

static void exportCurrentOpenedFile(bool exportAs) {
    const bool modelMode = gMode == StorylandMode::ModelFile;
    const bool textureMode = gMode == StorylandMode::TextureArchive;
    if (!modelMode && !textureMode) return;

    const std::wstring sourcePath = modelMode ? gModelFile.sourcePath() : gTextureArchive.sourcePath();
    if (sourcePath.empty()) {
        MessageBoxW(gMainWindow, L"The current file has no source path.", L"Storyland export", MB_ICONERROR);
        return;
    }

    std::wstring outputPath = sourcePath;
    if (exportAs) {
        const std::wstring extension = getExtensionLower(sourcePath);
        const std::wstring fileName = std::filesystem::path(sourcePath).filename().wstring();
        if (modelMode) {
            outputPath = saveFileDialogWithInitial(
                L"Model files\0*.mdl;*.dff\0MDL\0*.mdl\0DFF\0*.dff\0All files\0*.*\0",
                extension == L".dff" ? L"dff" : L"mdl",
                fileName);
        } else {
            outputPath = saveFileDialogWithInitial(
                L"Texture archives\0*.chk;*.xtx;*.tex;*.txd\0All files\0*.*\0",
                extension.size() > 1 ? extension.substr(1).c_str() : L"xtx",
                fileName);
        }
        if (outputPath.empty()) return;
    } else {
        std::wstring prompt = L"Export the current file back to:\r\n\r\n" + sourcePath +
                              L"\r\n\r\nThe original file will be replaced atomically.";
        if (MessageBoxW(gMainWindow, prompt.c_str(), L"Export current file", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    }

    std::string error;
    bool saved = false;

    if (modelMode) {
        saved = gModelFile.saveToFile(outputPath, error);
    } else {
        // Export and Export As MUST use the exact same serialized XTX snapshot.
        // Serialize once to a neutral temporary path, validate/read that exact byte
        // stream, then transaction-write those bytes to the user-selected target.
        // This prevents destination path/name from ever changing XTX serialization.
        wchar_t tempDir[MAX_PATH] = {};
        DWORD tempCount = GetTempPathW(MAX_PATH, tempDir);
        std::wstring tempRoot = tempCount > 0 ? std::wstring(tempDir) : L".\\";
        if (!tempRoot.empty() && tempRoot.back() != L'\\' && tempRoot.back() != L'/') tempRoot += L"\\";
        const std::wstring tempPath = tempRoot + L"Storyland_XTX_export_snapshot_" +
                                      std::to_wstring(GetCurrentProcessId()) + L".xtx";

        std::error_code removeEc;
        std::filesystem::remove(std::filesystem::path(tempPath), removeEc);

        if (gTextureArchive.saveToFile(tempPath, error)) {
            std::vector<uint8_t> serializedBytes;
            if (!readBinaryFileForUi(tempPath, serializedBytes, error)) {
                saved = false;
            } else {
                LeedsTextureArchive verification;
                std::string verificationError;
                if (!verification.loadFromMemory(
                        serializedBytes,
                        LeedsPlatform::Ps2,
                        verificationError,
                        outputPath)) {
                    error = "Serialized XTX failed reload verification: " + verificationError;
                    saved = false;
                } else {
                    std::string structureReport;
                    if (!verification.validateStructure(structureReport, verificationError)) {
                        error = "Serialized XTX failed runtime validation: " + verificationError;
                        saved = false;
                    } else {
                        saved = storylandWriteFilesTransaction(
                            {{std::filesystem::path(outputPath), &serializedBytes}}, error);
                        if (saved) {
                            std::vector<uint8_t> writtenBytes;
                            std::string readbackError;
                            if (!readBinaryFileForUi(outputPath, writtenBytes, readbackError) ||
                                writtenBytes != serializedBytes) {
                                error = readbackError.empty()
                                    ? "XTX export readback mismatch: output differs from the validated export snapshot."
                                    : "XTX export readback failed: " + readbackError;
                                saved = false;
                            }
                        }
                    }
                }
            }
        }

        std::filesystem::remove(std::filesystem::path(tempPath), removeEc);
    }

    if (!saved) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Storyland export failed", MB_ICONERROR);
        return;
    }
    setStatus((exportAs ? L"Exported as: " : L"Exported: ") + outputPath +
              (textureMode ? L" | byte-verified XTX snapshot" : L""));
}

static bool loadedModelUsesPspGeometry() {
    for (const StorylandModelField& field : gModelFile.fields()) {
        if (field.group.rfind("sPspGeometry @", 0) == 0 ||
            field.group.rfind("sPspGeometryMesh #", 0) == 0) return true;
    }
    return false;
}

static void runCurrentModelTest() {
    const bool modelAvailable =
        gMode == StorylandMode::ModelFile ||
        (gMode == StorylandMode::DtzArchive &&
         gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile);
    if (!modelAvailable) {
        MessageBoxW(gMainWindow, L"Open or preview an MDL/DFF first.", L"Test Model", MB_ICONINFORMATION);
        return;
    }

    // Test the exact bytes Storyland is currently displaying. Embedded DTZ/IMG
    // models do not necessarily have a standalone filesystem path, and the old
    // button incorrectly tried to reopen sourcePath().
    std::vector<uint8_t> bytes = gModelFile.rawBytes();
    if (bytes.empty()) {
        MessageBoxW(gMainWindow, L"The current model has no in-memory bytes to test.", L"Test Model", MB_ICONERROR);
        return;
    }

    const auto read32 = [&](size_t offset) -> uint32_t {
        if (offset > bytes.size() || bytes.size() - offset < 4u) return 0u;
        return uint32_t(bytes[offset]) |
               (uint32_t(bytes[offset + 1u]) << 8u) |
               (uint32_t(bytes[offset + 2u]) << 16u) |
               (uint32_t(bytes[offset + 3u]) << 24u);
    };

    const bool renderWareDff =
        bytes.size() >= 12u && read32(0u) == 0x10u;
    const bool leedsMdl =
        bytes.size() >= 0x20u && read32(0u) == 0x006D646Cu;

    const std::string pathLabel =
        gModelFile.sourcePath().empty()
            ? std::string("<in-memory model>")
            : narrow(gModelFile.sourcePath());
    const std::string label = "model: " + pathLabel;

    const StorylandModelIntegrityReport modelReport =
        storylandValidateModelIntegrity(gModelFile, bytes, label);

    uint32_t warnings = modelReport.warnings;
    uint32_t errors = modelReport.fatals;
    std::vector<std::string> extraWarnings;
    std::vector<std::string> extraErrors;

    StorylandDmaTlbReport dmaReport;
    StorylandPspDmaReport pspReport;
    bool ranPs2PacketValidation = false;
    bool ranPspMdlValidation = false;

    if (leedsMdl) {
        dmaReport = storylandValidatePs2DmaTlb(bytes, label);
        warnings += dmaReport.warnings;
        errors += dmaReport.fatals;
        ranPs2PacketValidation = true;

        const bool noPs2Streams =
            dmaReport.dmaTags == 0 &&
            dmaReport.vifStreams == 0 &&
            dmaReport.gifTags == 0;
        if (noPs2Streams && !loadedModelUsesPspGeometry()) {
            ++warnings;
            extraWarnings.push_back("No PS2 DMA/VIF/GIF stream was found in this MDL.");
        }
        if (dmaReport.dmaTags > 0 && dmaReport.vifStreams == 0) {
            ++warnings;
            extraWarnings.push_back("DMA tags were found, but no VIF command stream was recognized.");
        }
        if (dmaReport.directTransfers > 0 && dmaReport.gifTags == 0) {
            ++warnings;
            extraWarnings.push_back(
                "VIF DIRECT/DIRECTHL data was found, but no GIF tag was recognized inside the direct transfer.");
        }

        if (loadedModelUsesPspGeometry()) {
            pspReport = storylandValidatePspDmaGe(gModelFile, bytes, label);
            warnings += pspReport.warnings;
            errors += pspReport.fatals;
            ranPspMdlValidation = true;
        }
    }

    size_t rwChunkCount = 0u;
    size_t rwFrameCount = 0u;
    size_t rwHAnimCount = 0u;
    size_t rwSkinCount = 0u;
    size_t rwGeometryCount = 0u;
    if (renderWareDff) {
        StorylandAnalysisGraph graph;
        std::string graphError;
        if (!graph.build(bytes, ".dff", pathLabel.empty() ? "model.dff" : pathLabel, graphError)) {
            ++errors;
            extraErrors.push_back("RenderWare chunk graph validation failed: " + graphError);
        } else {
            rwChunkCount = graph.nodes().size();
            for (const StorylandAnalysisGraphNode& node : graph.nodes()) {
                if (node.type == 0x0Eu) ++rwFrameCount;
                else if (node.type == 0x11Eu) ++rwHAnimCount;
                else if (node.type == 0x116u) ++rwSkinCount;
                else if (node.type == 0x0Fu) ++rwGeometryCount;
            }
        }

        // A DFF must not be penalized for lacking PS2 DMA/VIF/GIF packets.
        // RenderWare geometry, HAnim and Skin PLG are its actual runtime format.
        if (gModelFile.modelKind() == StorylandModelKind::PedModel ||
            gModelFile.modelKind() == StorylandModelKind::CutsceneModel) {
            if (gModelFile.armatureBones().empty()) {
                ++errors;
                extraErrors.push_back(
                    "Skinned RenderWare DFF contains a PED/Cutscene model but no FrameList/HAnim skeleton was decoded.");
            }

            size_t weighted = 0u;
            for (const StorylandModelSkinWeights& row : gModelFile.previewSkinWeights()) {
                if (row.valid) ++weighted;
            }
            if (!gModelFile.previewPoints().empty() && weighted == 0u) {
                ++errors;
                extraErrors.push_back(
                    "Skinned RenderWare DFF has geometry but no usable Skin PLG vertex weights were decoded.");
            }
        }
    }

    const auto& textureNames = gModelFile.previewMaterialTextureNames();
    const auto& textureHints = gModelFile.textureNameHints();
    const size_t textureReferences = std::max(textureNames.size(), textureHints.size());
    if (textureReferences > 0u && !gModelTextureLoaded) {
        ++warnings;
        extraWarnings.push_back("The model references textures, but no companion texture archive was loaded.");
    }
    if (gModelTextureLoaded &&
        (gModelTextureImage.width <= 0 ||
         gModelTextureImage.height <= 0 ||
         gModelTextureImage.rgba.empty())) {
        ++errors;
        extraErrors.push_back(
            "A companion texture archive was found, but the preview atlas could not be decoded safely.");
    }

    bool companionStructureChecked = false;
    bool companionStructureValid = false;
    std::string companionStructureReport;
    std::string companionStructureError;
    size_t companionPs2Textures = 0u;
    size_t companionUnsupportedBpp = 0u;
    if (gModelTextureLoaded) {
        companionStructureChecked = true;
        companionStructureValid = gModelTextureArchive.validateStructure(
            companionStructureReport, companionStructureError);
        if (!companionStructureValid) {
            ++errors;
            extraErrors.push_back(
                "Companion texture archive failed retail/runtime structure validation: " +
                companionStructureError);
        }

        for (const LeedsTextureEntry& entry : gModelTextureArchive.textures()) {
            if (entry.kind != TextureKind::Ps2) continue;
            ++companionPs2Textures;
            if (entry.bpp != 4u && entry.bpp != 8u) {
                ++companionUnsupportedBpp;
            }
        }
        if (companionUnsupportedBpp != 0u) {
            ++errors;
            extraErrors.push_back(
                "Companion PS2 Stories texture archive contains " +
                std::to_string(companionUnsupportedBpp) +
                " texture(s) outside the supported retail 4bpp/8bpp authoring range.");
        }
    }

    std::ostringstream report;
    const char* overallStatus = errors ? "FAIL" : warnings ? "PASS WITH WARNINGS" : "PASS";

    report << "============================================================\r\n"
           << "MODEL TEST  |  " << overallStatus << "\r\n"
           << "============================================================\r\n"
           << "File       : " << pathLabel << "\r\n"
           << "Container  : " << (renderWareDff ? "RenderWare DFF" : leedsMdl ? "Leeds MDL" : "Unknown") << "\r\n"
           << "Size       : " << bytes.size() << " bytes\r\n"
           << "Errors     : " << errors << "\r\n"
           << "Warnings   : " << warnings << "\r\n\r\n";

    report << "============================================================\r\n"
           << "GEOMETRY\r\n"
           << "============================================================\r\n"
           << "Vertices          : " << modelReport.vertices << "\r\n"
           << "Triangles         : " << modelReport.triangles << "\r\n"
           << "Degenerate        : " << modelReport.degenerateTriangles << "\r\n"
           << "Bones / frames    : " << modelReport.bones << "\r\n"
           << "Root bones        : " << modelReport.rootBones << "\r\n"
           << "Skin influences   : " << modelReport.skinInfluences << "\r\n"
           << "Weighted vertices : " << modelReport.weightedVertices << "/" << modelReport.vertices << "\r\n\r\n";

    report << "============================================================\r\n"
           << "TEXTURES\r\n"
           << "============================================================\r\n"
           << "References        : " << textureReferences << "\r\n"
           << "Companion archive : " << (gModelTextureLoaded ? "loaded" : "not loaded") << "\r\n";
    if (gModelTextureLoaded) {
        const std::filesystem::path companionPath(gModelTextureArchive.sourcePath());
        report << "Archive file      : "
               << (companionPath.empty() ? std::string("(embedded/in-memory)") : companionPath.filename().string())
               << "\r\n"
               << "Materials         : " << gModelTextureArchive.textures().size() << "\r\n"
               << "PS2 materials     : " << companionPs2Textures << "\r\n"
               << "4/8bpp compliance : " << (companionUnsupportedBpp == 0u ? "PASS" : "FAIL") << "\r\n"
               << "Runtime layout    : "
               << (!companionStructureChecked ? "not checked" : companionStructureValid ? "PASS" : "FAIL")
               << "\r\n";
        if (leedsMdl && companionPs2Textures != 0u) {
            report << "UV convention     : native Leeds top-origin; no automatic V inversion\r\n";
        }
        if (companionStructureValid && !companionStructureReport.empty()) {
            std::string oneLine = companionStructureReport;
            for (char& c : oneLine) {
                if (c == '\n' || c == '\r') c = ' ';
            }
            while (!oneLine.empty() && oneLine.back() == ' ') oneLine.pop_back();
            if (!oneLine.empty()) report << "Layout detail     : " << oneLine << "\r\n";
        }
    }
    report << "\r\n";

    if (renderWareDff) {
        report << "============================================================\r\n"
               << "RENDERWARE\r\n"
               << "============================================================\r\n"
               << "Build / version   : 0x" << std::hex << std::uppercase << read32(8u) << std::dec << "\r\n"
               << "Chunks             : " << rwChunkCount << "\r\n"
               << "Geometry chunks    : " << rwGeometryCount << "\r\n"
               << "FrameList chunks   : " << rwFrameCount << "\r\n"
               << "HAnim chunks       : " << rwHAnimCount << "\r\n"
               << "Skin chunks        : " << rwSkinCount << "\r\n\r\n";
    } else {
        report << "============================================================\r\n"
               << "RELOCATIONS\r\n"
               << "============================================================\r\n"
               << "Declared fields    : " << modelReport.declaredRelocationFields << "\r\n"
               << "Missing required   : " << modelReport.missingRequiredRelocations << "\r\n"
               << "Trailing recovered : " << modelReport.recoveredTrailingRelocations << "\r\n"
               << "File-local targets : " << modelReport.fileLocalPointers << "\r\n"
               << "Runtime targets    : " << modelReport.runtimePointers << "\r\n"
               << "Suspicious targets : " << modelReport.suspiciousPointers << "\r\n\r\n";
    }

    if (ranPs2PacketValidation) {
        report << "============================================================\r\n"
               << "PS2 DMA / VIF / GIF / VU1\r\n"
               << "============================================================\r\n"
               << "DMA tags           : " << dmaReport.dmaTags << "\r\n"
               << "DMA chains         : " << dmaReport.dmaChains << "\r\n"
               << "DMA payload        : " << dmaReport.dmaPayloadBytes << " bytes\r\n"
               << "VIF streams        : " << dmaReport.vifStreams << "\r\n"
               << "VIF commands       : " << dmaReport.vifCommands << "\r\n"
               << "UNPACK commands    : " << dmaReport.vifUnpacks << "\r\n"
               << "DIRECT transfers   : " << dmaReport.directTransfers << "\r\n"
               << "GIF tags           : " << dmaReport.gifTags << "\r\n"
               << "GIF packets        : " << dmaReport.gifPackets << "\r\n"
               << "GS register writes : " << dmaReport.gsRegisterWrites << "\r\n"
               << "GS primitive kicks : " << dmaReport.gsPrimitiveKicks << "\r\n"
               << "VU1 data qwords    : " << dmaReport.vu1DataQwordsWritten << "\r\n"
               << "VU1 MPG uploaded   : " << dmaReport.vu1MicroInstructionsUploaded << "\r\n"
               << "VU1 MSCAL calls    : " << dmaReport.vu1MicroCalls << "\r\n"
               << "Resident-code calls: " << dmaReport.vu1MicroCallsExternal << "\r\n";
        if (dmaReport.vifStreams != 0u && dmaReport.directTransfers == 0u && dmaReport.gifTags == 0u && dmaReport.vu1MicroCalls != 0u) {
            report << "Packet route       : VIF -> resident VU1 microcode (no inline GIF expected)\r\n";
        }
        report << "\r\n";
    }

    if (ranPspMdlValidation) {
        report << "============================================================\r\n"
               << "PSP GEOMETRY\r\n"
               << "============================================================\r\n"
               << "Geometry blocks    : " << pspReport.geometryBlocks << "\r\n"
               << "Mesh streams       : " << pspReport.meshStreams << "\r\n"
               << "Vertex streams     : " << pspReport.vertexStreams << "\r\n\r\n";
    }

    report << "============================================================\r\n"
           << "ISSUES\r\n"
           << "============================================================\r\n";
    bool wroteIssue = false;
    for (const StorylandModelIntegrityIssue& issue : modelReport.issues) {
        report << (issue.severity == StorylandModelIssueSeverity::Fatal ? "ERROR   " : "WARNING ")
               << "0x" << std::hex << std::uppercase << issue.offset << std::dec
               << "  " << issue.message << "\r\n";
        wroteIssue = true;
    }
    if (ranPs2PacketValidation) {
        for (const StorylandDmaIssue& issue : dmaReport.issues) {
            if (issue.severity == StorylandDmaIssueSeverity::Info) continue;
            report << (issue.severity == StorylandDmaIssueSeverity::Fatal ? "ERROR   " : "WARNING ")
                   << "0x" << std::hex << std::uppercase << issue.offset << std::dec
                   << "  " << issue.message << "\r\n";
            wroteIssue = true;
        }
    }
    if (ranPspMdlValidation) {
        for (const StorylandDmaIssue& issue : pspReport.issues) {
            if (issue.severity == StorylandDmaIssueSeverity::Info) continue;
            report << (issue.severity == StorylandDmaIssueSeverity::Fatal ? "ERROR   " : "WARNING ")
                   << "0x" << std::hex << std::uppercase << issue.offset << std::dec
                   << "  " << issue.message << "\r\n";
            wroteIssue = true;
        }
    }
    for (const std::string& message : extraErrors) {
        report << "ERROR        " << message << "\r\n";
        wroteIssue = true;
    }
    for (const std::string& message : extraWarnings) {
        report << "WARNING      " << message << "\r\n";
        wroteIssue = true;
    }
    if (!wroteIssue) report << "None.\r\n";

    report << "\r\n============================================================\r\n"
           << "RESULT\r\n"
           << "============================================================\r\n";
    if (errors != 0u) {
        report << "FAIL - structural errors were detected.\r\n";
    } else if (warnings != 0u) {
        report << "PASS WITH WARNINGS - validated ranges are safe; review ISSUES.\r\n";
    } else {
        report << "PASS - all applicable checks completed without warnings.\r\n";
    }

    setDetails(widen(report.str()));
    if (errors != 0u) {
        setStatus(
            L"Test Model | failed | " +
            std::to_wstring(errors) + L" errors, " +
            std::to_wstring(warnings) + L" warnings");
        MessageBoxW(
            gMainWindow,
            L"Model test failed. The details panel lists the errors and warnings.",
            L"Test Model",
            MB_OK | MB_ICONERROR);
    } else if (warnings != 0u) {
        setStatus(
            L"Test Model | passed with " +
            std::to_wstring(warnings) + L" warnings");
        MessageBoxW(
            gMainWindow,
            L"Model test passed with warnings. The details panel lists them.",
            L"Test Model",
            MB_OK | MB_ICONWARNING);
    } else {
        setStatus(L"Test Model | passed");
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

static void rebuildCurrentDtzAs() {
    if (gMode != StorylandMode::DtzArchive) return;

    std::wstring path = saveFileDialog(L"DTZ raw or compressed\0*.dtz;*.bin\0All files\0*.*\0", L"dtz");
    if (path.empty()) return;

    const bool compress = MessageBoxW(
        gMainWindow,
        L"Save as compressed zlib DTZ?\n\nChoose No to save raw GTAG bytes. Compressed output is checked before it is written. If an IMG is loaded, it is saved beside the new GAME.DTZ.",
        L"Rebuild GAME.DTZ",
        MB_YESNO | MB_ICONQUESTION
    ) == IDYES;

    std::string error;
    if (!gDtzArchive.saveToFile(path, compress, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"GAME.DTZ rebuild failed", MB_ICONERROR);
        return;
    }

    if (gDtzArchive.hasCompanionImg()) {
        setStatus(gDtzArchive.hasCompanionDir()
            ? L"Saved and verified rebuilt GAME.DTZ + IMG + explicitly loaded beta-build DIR transaction."
            : L"Saved and verified rebuilt retail GAME.DTZ + companion IMG transaction.");
    } else {
        setStatus(L"Saved and verified rebuilt GAME.DTZ.");
    }
}

static void exportSelectedTexture() {
    if (gMode != StorylandMode::TextureArchive || gSelectedIndex < 0) return;
    std::wstring path = saveFileDialog(L"PNG image\0*.png\0All files\0*.*\0", L"png");
    if (path.empty()) return;
    std::string error;
    if (!savePngWithWic(path, gCurrentImage, error)) MessageBoxW(gMainWindow, widen(error).c_str(), L"Export failed", MB_ICONERROR);
}

static int chooseTextureBpp(UINT initialBpp) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return -1;
    AppendMenuW(menu, MF_STRING | (initialBpp == 4u ? MF_CHECKED : 0), 4u, L"4 bpp  (16 colours)");
    AppendMenuW(menu, MF_STRING | (initialBpp == 8u ? MF_CHECKED : 0), 8u, L"8 bpp  (256 colours)");
    POINT pt{}; GetCursorPos(&pt);
    UINT chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, gMainWindow, nullptr);
    DestroyMenu(menu);
    if (chosen != 4u && chosen != 8u) return -1;
    return int(chosen);
}

static bool clampEditableStoriesTexturesTo8Bpp(std::wstring& summaryOut, std::string& errorMessage) {
    summaryOut.clear();
    size_t clamped = 0;

    for (size_t textureIndex = 0; textureIndex < gTextureArchive.textures().size(); ++textureIndex) {
        const auto& entry = gTextureArchive.textures()[textureIndex];
        if (entry.bpp <= 8u) continue;

        const bool editableStoriesTexture =
            entry.kind == TextureKind::Ps2 ||
            entry.kind == TextureKind::Psp ||
            entry.kind == TextureKind::RwPsp;
        if (!editableStoriesTexture) continue;

        RgbaImage decoded;
        if (!gTextureArchive.decodeTexture(textureIndex, decoded, errorMessage)) {
            errorMessage = "Could not decode texture '" + entry.name + "' before clamping it to 8bpp: " + errorMessage;
            return false;
        }

        if (!gTextureArchive.replaceTextureAsBpp(textureIndex, decoded, 8u, errorMessage)) {
            errorMessage = "Could not clamp texture '" + entry.name + "' to 8bpp: " + errorMessage;
            return false;
        }
        ++clamped;
    }

    if (clamped != 0u) {
        summaryOut = std::to_wstring(clamped) +
            (clamped == 1u ? L" texture was clamped to 8bpp." : L" textures were clamped to 8bpp.");
    }
    return true;
}

static void applyAnimationToCurrentModel() {
    if (!(gMode == StorylandMode::ModelFile ||
          (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile))) return;
    if (!currentModelCanUsePedCutsceneAnimation()) {
        MessageBoxW(gMainWindow, L"This MDL does not expose a ped/cutscene armature that can accept Leeds ANIM data.", L"Apply Animation", MB_ICONINFORMATION);
        return;
    }
    std::wstring path = openFileDialog(L"Rockstar Leeds ANIM\0*.anim\0All files\0*.*\0");
    if (path.empty()) return;
    std::string error;
    if (!gAnimFile.loadFromFile(path, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Apply Animation failed", MB_ICONERROR);
        return;
    }
    gModelAnimLoaded = true;
    gModelAnimPath = path;
    gModelAnimStatus = widen(gAnimFile.summaryLine());
    gAnimPlaying = true;
    gAnimCurrentTime = 0.0f;
    gAnimLastTick = GetTickCount();
    startAnimationPlaybackTimer();
    if (gMode == StorylandMode::ModelFile) populateModelList();
    setStatus(L"Applied ANIM to current armature MDL: " + gModelAnimStatus);
    if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
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

    uint32_t currentBpp = 8u;
    bool clampedFromHigherBpp = false;
    if (size_t(gSelectedIndex) < gTextureArchive.textures().size()) {
        const uint8_t parsedBpp = gTextureArchive.textures()[size_t(gSelectedIndex)].bpp;
        if (parsedBpp == 4u || parsedBpp == 8u) {
            currentBpp = parsedBpp;
        } else if (parsedBpp > 8u) {
            currentBpp = 8u;
            clampedFromHigherBpp = true;
        }
    }

    int chosenBpp = 8;
    if (!clampedFromHigherBpp) {
        chosenBpp = chooseTextureBpp(currentBpp);
        if (chosenBpp < 0) return;
    }

    if (!gTextureArchive.replaceTextureAsBpp(size_t(gSelectedIndex), image, uint8_t(chosenBpp), error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Replace failed", MB_ICONERROR);
        return;
    }

    // VCS player authoring frequently starts from a retail plr.xtx and replaces
    // the images with torso.png / jeans.png / head.png / shoes.png. Keeping the
    // old retail slot name while changing only the raster creates an internally
    // valid XTX whose runtime lookup key no longer matches the companion MDL.
    // For these four canonical authoring names, make the image basename the XTX
    // lookup key as part of the replacement operation.
    std::wstring replacementStem = std::filesystem::path(path).stem().wstring();
    std::transform(replacementStem.begin(), replacementStem.end(), replacementStem.begin(), [](wchar_t ch) {
        return wchar_t(std::towlower(ch));
    });
    if (replacementStem == L"torso" || replacementStem == L"jeans" ||
        replacementStem == L"head" || replacementStem == L"shoes") {
        std::string canonicalName = narrowAscii(replacementStem);
        if (canonicalName.empty()) {
            MessageBoxW(gMainWindow,
                L"The replacement texture basename contains characters that cannot be represented safely as an XTX material name.",
                L"Texture rename after replacement failed",
                MB_ICONERROR);
            return;
        }
        std::string renameError;
        const auto& afterReplace = gTextureArchive.textures();
        if (size_t(gSelectedIndex) < afterReplace.size() &&
            [&]() {
                std::string existingName = afterReplace[size_t(gSelectedIndex)].name;
                std::transform(existingName.begin(), existingName.end(), existingName.begin(), [](unsigned char ch) {
                    return char(std::tolower(ch));
                });
                return existingName != canonicalName;
            }()) {
            if (!gTextureArchive.renameTexture(size_t(gSelectedIndex), canonicalName, renameError)) {
                MessageBoxW(gMainWindow, widen(renameError).c_str(), L"Texture rename after replacement failed", MB_ICONERROR);
                return;
            }
        }
    }

    populateTextureList();
    int nextIndex = std::min<int>(gSelectedIndex, int(gTextureArchive.textures().size()) - 1);
    if (nextIndex >= 0) selectTexture(nextIndex);
    if (clampedFromHigherBpp) {
        setStatus(L"Texture changed | source material was above 8bpp and was clamped to 8bpp | right-click > Export edited texture archive");
    } else {
        setStatus(L"Texture changed | right-click > Export edited texture archive");
    }
}

static void editSelectedTextureMaterial() {
    replaceSelectedTexture();
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
    clearDtzFindState(true);
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

    clearDtzFindState(true);
    populateDtzList();
    SetWindowTextW(gMainWindow, L"Storyland - GAME.DTZ + gta3PS*.img");
    applyStorylandTitleTintForPath(path);
    setStatus(L"Loaded companion IMG for GAME.DTZ + gta3ps*.img directory: " + path);
}


static void addTextureMaterial() {
    if (gMode != StorylandMode::TextureArchive) return;

    std::wstring imagePath = openFileDialog(L"Image\0*.png;*.bmp;*.jpg;*.jpeg;*.tif;*.tiff\0All files\0*.*\0");
    if (imagePath.empty()) return;
    RgbaImage image;
    std::string error;
    if (!loadImageWithWic(imagePath, image, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Add material failed", MB_ICONERROR);
        return;
    }

    std::wstring defaultName = getFileStemPart(imagePath);
    if (defaultName.empty()) defaultName = L"texture";
    std::string name = narrowAscii(defaultName);
    if (name.empty()) name = "texture";
    if (name.size() > 63u) name.resize(63u);

    // The selected image filename is already a sane material name. Do not force
    // the user to type it again. Only add a suffix when that name already exists.
    auto nameExists = [&](const std::string& candidate) {
        for (const auto& texture : gTextureArchive.textures()) {
            if (_stricmp(texture.name.c_str(), candidate.c_str()) == 0) return true;
        }
        return false;
    };
    if (nameExists(name)) {
        const std::string base = name;
        for (unsigned suffix = 2u; suffix < 10000u; ++suffix) {
            std::string candidate = base + "_" + std::to_string(suffix);
            if (candidate.size() > 63u) candidate.resize(63u);
            if (!nameExists(candidate)) { name = candidate; break; }
        }
    }

    const int bppChoice = chooseNewResourceTile(
        L"New Material",
        L"4BPP",
        L"8BPP");
    if (bppChoice < 0) return;
    const int chosenBpp = bppChoice == 0 ? 4 : 8;
    gNewTextureDefaultBpp = chosenBpp;

    if (!gTextureArchive.addTexture(name, image, uint8_t(chosenBpp), error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Add material failed", MB_ICONERROR);
        return;
    }
    populateTextureList();
    const int newIndex = int(gTextureArchive.textures().size()) - 1;
    if (newIndex >= 0) selectTexture(newIndex);
    refreshModeUi();
    setStatus(L"Added texture/material '" + widen(name) + L"' as " + std::to_wstring(chosenBpp) + L"bpp.");
}

static void swapSelectedTextureData() {
    if (gMode != StorylandMode::TextureArchive || gSelectedIndex < 0) return;
    const auto& textures = gTextureArchive.textures();
    if (textures.size() < 2u || size_t(gSelectedIndex) >= textures.size()) return;

    std::wstringstream prompt;
    prompt << L"Swap texture data from '" << widen(textures[size_t(gSelectedIndex)].name) << L"' with which texture index?\r\n\r\n";
    for (size_t i = 0; i < textures.size() && i < 32u; ++i)
        prompt << i << L": " << widen(textures[i].name) << L"\r\n";

    uint32_t other = uint32_t(gSelectedIndex == 0 ? 1 : 0);
    bool ignored = false;
    if (!askUnsigned(L"Swap Texture Data", prompt.str().c_str(), other, other, false, ignored)) return;
    if (other >= textures.size() || int(other) == gSelectedIndex) {
        MessageBoxW(gMainWindow, L"Choose a different valid texture index.", L"Swap Texture Data", MB_ICONERROR);
        return;
    }

    std::string error;
    const int keepIndex = gSelectedIndex;
    if (!gTextureArchive.swapTextureData(size_t(gSelectedIndex), size_t(other), error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Swap Texture Data failed", MB_ICONERROR);
        return;
    }
    populateTextureList();
    selectTexture(keepIndex);
    refreshModeUi();
    setStatus(L"Swapped texture raster/palette data while preserving texture names.");
}

static void duplicateSelectedTexture() {
    if (gMode != StorylandMode::TextureArchive || gSelectedIndex < 0) return;
    const auto& textures = gTextureArchive.textures();
    if (size_t(gSelectedIndex) >= textures.size()) return;

    std::wstring suggested = widen(textures[size_t(gSelectedIndex)].name) + L"_copy";
    std::wstring nameWide;
    if (!askString(L"Duplicate Texture", L"New texture/material name:", suggested, nameWide)) return;
    std::string name = narrowAscii(nameWide);
    std::string error;
    if (!gTextureArchive.duplicateTexture(size_t(gSelectedIndex), name, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Duplicate texture failed", MB_ICONERROR);
        return;
    }
    populateTextureList();
    int newIndex = std::min<int>(gSelectedIndex + 1, int(gTextureArchive.textures().size()) - 1);
    if (newIndex >= 0) selectTexture(newIndex);
    refreshModeUi();
    setStatus(L"Duplicated encoded texture data without decode/re-encode loss.");
}

static void removeSelectedTexture() {
    if (gMode != StorylandMode::TextureArchive || gSelectedIndex < 0) return;
    const auto& textures = gTextureArchive.textures();
    if (size_t(gSelectedIndex) >= textures.size()) return;
    std::wstring message = L"Remove texture/material '" + widen(textures[size_t(gSelectedIndex)].name) + L"'?";
    if (MessageBoxW(gMainWindow, message.c_str(), L"Remove Texture", MB_YESNO | MB_ICONWARNING) != IDYES) return;

    std::string error;
    int oldIndex = gSelectedIndex;
    if (!gTextureArchive.removeTexture(size_t(gSelectedIndex), error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Remove texture failed", MB_ICONERROR);
        return;
    }
    populateTextureList();
    if (!gTextureArchive.textures().empty()) {
        selectTexture(std::min<int>(oldIndex, int(gTextureArchive.textures().size()) - 1));
    }
    refreshModeUi();
    setStatus(L"Removed texture/material and rebuilt linked-list + relocations.");
}

static void validateCurrentTextureArchive() {
    if (gMode != StorylandMode::TextureArchive &&
        !(gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::TextureArchive)) return;
    std::string report, error;
    if (!gTextureArchive.validateStructure(report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Texture Archive Validation", MB_ICONERROR);
        return;
    }
    setDetails(widen(report));
    setStatus(L"Texture archive validation passed.");
}

static const wchar_t* textureKindLabel(TextureKind kind) {
    switch (kind) {
    case TextureKind::CtwTex: return L"CTW TEX";
    case TextureKind::Dds: return L"DDS";
    case TextureKind::Ps2: return L"PS2";
    case TextureKind::Psp: return L"PSP";
    case TextureKind::RwPsp: return L"PSP RenderWare";
    case TextureKind::RwPc: return L"PC RenderWare";
    default: return L"Unknown";
    }
}

static void runCurrentTextureTest() {
    const bool standaloneArchive = gMode == StorylandMode::TextureArchive;
    const bool embeddedArchive =
        gMode == StorylandMode::DtzArchive &&
        gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::TextureArchive;
    if (!standaloneArchive && !embeddedArchive) {
        MessageBoxW(gMainWindow, L"Open or preview a texture archive first.", L"Test Texture", MB_ICONINFORMATION);
        return;
    }

    const auto& textures = gTextureArchive.textures();
    std::string structureReport;
    std::string structureError;
    const bool structureOk = gTextureArchive.validateStructure(structureReport, structureError);

    size_t decodePass = 0;
    size_t decodeFail = 0;
    size_t ps2Count = 0;
    size_t pspCount = 0;
    size_t invalidStoriesBpp = 0;
    size_t invalidDimensions = 0;
    size_t invalidMipCount = 0;
    size_t alphaTextures = 0;
    std::vector<std::wstring> issues;

    // Test Texture must be safe even for malformed archives. Do not run every
    // raster through the full preview decoder here: validation is a metadata/
    // bounds operation, not a rendering stress test. A corrupt raster must be
    // reported, never allowed to corrupt process memory and crash later inside
    // a Windows common control.
    const size_t archiveSize = gTextureArchive.rawBytes().size();
    for (size_t i = 0; i < textures.size(); ++i) {
        const LeedsTextureEntry& entry = textures[i];
        if (entry.kind == TextureKind::Ps2) ++ps2Count;
        if (entry.kind == TextureKind::Psp || entry.kind == TextureKind::RwPsp) ++pspCount;

        if (entry.kind == TextureKind::Ps2 && entry.bpp != 4 && entry.bpp != 8) {
            ++invalidStoriesBpp;
            std::wstringstream issue;
            issue << L"Texture " << i << L" ('" << widen(entry.name) << L"') uses "
                  << int(entry.bpp) << L"bpp; Stories PS2 authoring must be 4bpp or 8bpp.";
            issues.push_back(issue.str());
        }
        if (entry.width <= 0 || entry.height <= 0) {
            ++invalidDimensions;
            std::wstringstream issue;
            issue << L"Texture " << i << L" ('" << widen(entry.name) << L"') has invalid dimensions "
                  << entry.width << L"x" << entry.height << L".";
            issues.push_back(issue.str());
        }
        if (entry.mipCount == 0) {
            ++invalidMipCount;
            std::wstringstream issue;
            issue << L"Texture " << i << L" ('" << widen(entry.name) << L"') reports zero mip levels.";
            issues.push_back(issue.str());
        }

        uint64_t pixels = 0;
        uint64_t rasterBytes = 0;
        if (entry.width > 0 && entry.height > 0) {
            pixels = uint64_t(entry.width) * uint64_t(entry.height);
            if (entry.bpp == 4) rasterBytes = (pixels + 1u) / 2u;
            else if (entry.bpp == 8) rasterBytes = pixels;
            else if (entry.bpp == 16) rasterBytes = pixels * 2u;
            else if (entry.bpp == 32) rasterBytes = pixels * 4u;
        }

        const uint64_t rasterEnd = uint64_t(entry.rasterOffset) + rasterBytes;
        if (rasterBytes == 0u || uint64_t(entry.rasterOffset) >= uint64_t(archiveSize) ||
            rasterEnd > uint64_t(archiveSize)) {
            ++decodeFail;
            std::wstringstream issue;
            issue << L"Texture " << i << L" ('" << widen(entry.name)
                  << L"') has raster bytes outside the archive.";
            issues.push_back(issue.str());
        } else {
            ++decodePass;
        }
    }
    const int testedTextureIndex = embeddedArchive ? (textures.empty() ? -1 : 0) : gSelectedIndex;
    const bool selectedValid =
        testedTextureIndex >= 0 && size_t(testedTextureIndex) < textures.size();
    const LeedsTextureEntry* selected = selectedValid ? &textures[size_t(testedTextureIndex)] : nullptr;

    std::wstringstream ss;
    ss << L"============================================================\r\n"
       << L"TEXTURE TEST\r\n"
       << L"============================================================\r\n";

    if (!gTextureArchive.sourcePath().empty()) {
        ss << L"Archive            : " << gTextureArchive.sourcePath() << L"\r\n";
    } else if (!gDtzEmbeddedPreviewPath.empty()) {
        ss << L"Archive            : " << gDtzEmbeddedPreviewPath << L"\r\n";
    }

    ss << L"Archive bytes       : " << gTextureArchive.rawBytes().size() << L"\r\n"
       << L"Materials           : " << textures.size() << L"\r\n"
       << L"PS2                 : " << ps2Count << L"\r\n"
       << L"PSP                 : " << pspCount << L"\r\n"
       << L"Raster ranges       : " << decodePass << L" / " << textures.size() << L"\r\n"
       << L"With alpha          : " << alphaTextures << L"\r\n\r\n";

    ss << L"------------------------------------------------------------\r\n"
       << L"SELECTED TEXTURE\r\n"
       << L"------------------------------------------------------------\r\n";

    if (selected) {
        ss << L"Index               : " << testedTextureIndex << L"\r\n"
           << L"Name                : " << widen(selected->name) << L"\r\n"
           << L"Kind                : " << textureKindLabel(selected->kind) << L"\r\n"
           << L"Dimensions          : " << selected->width << L" x " << selected->height << L"\r\n"
           << L"BPP                 : " << int(selected->bpp) << L"\r\n"
           << L"Mip count           : " << int(selected->mipCount) << L"\r\n"
           << L"Swizzle             : " << (selected->swizzleMask ? L"encoded/swizzled" : L"linear") << L"\r\n"
           << L"Texture header      : " << hexWide(selected->textureHeaderOffset, 6) << L"\r\n"
           << L"Raster              : " << hexWide(selected->rasterOffset, 6) << L"\r\n"
           << L"Block size          : " << selected->blockSize << L" bytes\r\n";
    } else {
        ss << L"No texture is currently selected.\r\n";
    }

    ss << L"\r\n"
       << L"------------------------------------------------------------\r\n"
       << L"STORIES AUTHORING RULES\r\n"
       << L"------------------------------------------------------------\r\n"
       << L"PS2 4/8bpp only     : " << (invalidStoriesBpp == 0 ? L"PASS" : L"FAIL") << L"\r\n"
       << L"Valid dimensions    : " << (invalidDimensions == 0 ? L"PASS" : L"FAIL") << L"\r\n"
       << L"Mip metadata        : " << (invalidMipCount == 0 ? L"PASS" : L"FAIL") << L"\r\n"
       << L"Raster bounds       : " << (decodeFail == 0 ? L"PASS" : L"FAIL") << L"\r\n";

    ss << L"\r\n"
       << L"------------------------------------------------------------\r\n"
       << L"ARCHIVE STRUCTURE\r\n"
       << L"------------------------------------------------------------\r\n"
       << L"Result              : " << (structureOk ? L"PASS" : L"FAIL") << L"\r\n";

    if (structureOk) {
        if (!structureReport.empty()) {
            ss << widen(structureReport) << L"\r\n";
        }
    } else if (!structureError.empty()) {
        ss << L"Error               : " << widen(structureError) << L"\r\n";
    }

    if (!issues.empty()) {
        ss << L"\r\n"
           << L"------------------------------------------------------------\r\n"
           << L"ISSUES\r\n"
           << L"------------------------------------------------------------\r\n";
        for (const std::wstring& issue : issues) {
            ss << L"- " << issue << L"\r\n";
        }
    }

    const bool passed = structureOk && decodeFail == 0 && invalidStoriesBpp == 0 &&
                        invalidDimensions == 0 && invalidMipCount == 0;

    ss << L"\r\n"
       << L"============================================================\r\n"
       << L"RESULT: " << (passed ? L"PASS" : L"FAIL") << L"\r\n"
       << L"============================================================\r\n";

    setDetails(ss.str());
    setStatus(passed ? L"Test Texture | PASS" : L"Test Texture | FAIL");
}

static void importModelDataIntoCurrentDraft() {
    if (gMode != StorylandMode::ModelFile) return;
    std::wstring source = openFileDialog(L"Stories MDL\0*.mdl\0All files\0*.*\0");
    if (source.empty()) return;
    std::string error;
    if (!gModelFile.importMdlDataFromFile(source, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Import MDL data failed", MB_ICONERROR);
        return;
    }
    gModelAnimLoaded = false;
    gModelAnimPath.clear();
    gModelAnimStatus.clear();
    resetModelViewport();
    populateModelList();
    selectModelField(0);
    refreshModeUi();
    setStatus(L"Imported MDL data and classified the model as " + widen(gModelFile.modelKindName()) + L".");
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void createNewStorylandResource() {
    beginNewModelResource();
}


static void showAboutDialog() {
    MessageBoxW(
        gMainWindow,
        L"Storyland 1.1.5.2\r\n\r\nauthor: spicybung\r\nhttps://github.com/spicybung/BLeeds\r\nReigns Studios\r\n\r\nAn analyzer, editor, and viewer for Grand Theft Auto Stories file formats.",
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
        L"Leeds / Stories resource\0*.mdl;*.dff;*.xtx;*.chk;*.tex;*.txd;*.anim;*.cam;*.cut;*.col;*.col2;*.bin\0"
        L"Model files\0*.mdl;*.dff\0"
        L"Texture archives\0*.xtx;*.chk;*.tex;*.txd\0"
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

    // Keep an immutable copy for MDL replacement. Container insertion must never
    // change model bytes behind the user's back.
    const std::vector<uint8_t> replacementBytesFromDisk = replacementBytes;

    std::string textureRuntimeRepairDetail;
    const std::wstring selectedExtension = getExtensionLower(selectedNameBefore);
    const std::wstring replacementExtension = getExtensionLower(replacementPath);
    const bool looksLikeLeedsMdl =
        replacementBytes.size() >= 4u &&
        replacementBytes[0] == 'l' && replacementBytes[1] == 'd' &&
        replacementBytes[2] == 'm' && replacementBytes[3] == 0;
    const bool looksLikePs2StoriesXet =
        replacementBytes.size() >= 0x50u &&
        replacementBytes[0] == 'x' && replacementBytes[1] == 'e' && replacementBytes[2] == 't' &&
        replacementBytes[0x20u] == 0x06u && replacementBytes[0x21u] == 0x86u &&
        replacementBytes[0x22u] == 0x00u && replacementBytes[0x23u] == 0x00u;
    if (looksLikePs2StoriesXet) {
        LeedsTextureArchive candidateTexture;
        std::string textureLoadError;
        if (!candidateTexture.loadFromMemory(
                replacementBytes,
                LeedsPlatform::Ps2,
                textureLoadError,
                replacementPath)) {
            MessageBoxW(
                gMainWindow,
                widen(textureLoadError).c_str(),
                L"Texture replacement rejected",
                MB_ICONERROR);
            return;
        }

        std::string textureValidationReport;
        std::string textureValidationError;
        if (!candidateTexture.validateStructure(textureValidationReport, textureValidationError)) {
            std::string combined = textureValidationError;
            combined += "\n\nStoryland did NOT rewrite this XTX. DTZ/IMG replacement is now byte-preserving. ";
            combined += "Open the XTX separately, repair/export it explicitly, then replace it again.";
            MessageBoxW(
                gMainWindow,
                widen(combined).c_str(),
                L"Texture replacement rejected",
                MB_ICONERROR);
            return;
        }

        // Hard invariant: DTZ/IMG insertion must never silently normalize or rebuild
        // an XTX. The exact file selected by the user is the exact byte stream that
        // goes into gta3PS*.img. This mirrors the MDL byte-preservation rule below.
        if (replacementBytes != replacementBytesFromDisk) {
            MessageBoxW(
                gMainWindow,
                L"Storyland attempted to modify the XTX before IMG insertion. The replacement was cancelled. "
                L"DTZ/IMG replacement must preserve XTX bytes exactly.",
                L"XTX byte-preservation guard",
                MB_OK | MB_ICONERROR);
            return;
        }

    }

    if (selectedExtension == L".mdl" || replacementExtension == L".mdl" || looksLikeLeedsMdl) {
        // DTZ/IMG replacement is a container operation. The selected MDL must be
        // inserted byte-for-byte exactly as supplied by the user. Do NOT run any
        // model "repair", normalization, HAnim remap, material-slot rewrite, DMA
        // rewrite, or relocation-count rewrite here. Those transformations are
        // appropriate only for an explicit model-edit/export operation.
        //
        // In particular, storylandRepairVcsRetailPlayerHAnimIds() used to permute
        // the 22 VCS player hierarchy IDs immediately before insertion, meaning a
        // known-good cjmdl5.mdl was silently changed while GAME.DTZ/IMG sector
        // repacking itself was correct. Validation below is read-only.

        StorylandModelFile candidateModel;
        std::string candidateLoadError;
        if (!candidateModel.loadFromMemory(replacementBytes, replacementPath, candidateLoadError)) {
            MessageBoxW(gMainWindow, widen(candidateLoadError).c_str(),
                        L"Model replacement rejected", MB_ICONERROR);
            return;
        }
        const StorylandModelIntegrityReport candidateReport =
            storylandValidateModelIntegrity(candidateModel, replacementBytes,
                                            "replacement: " + narrow(replacementPath));
        if (!candidateReport.safe()) {
            const std::wstring details = widen(candidateReport.text());
            setDetails(details);
            MessageBoxW(
                gMainWindow,
                L"The replacement model failed structural validation and was not written into the IMG. "
                L"Use the details panel to inspect the failing pointer, relocation, hierarchy, geometry, or skin checks.",
                L"Model replacement rejected",
                MB_OK | MB_ICONERROR);
            return;
        }

        // Hard invariant: model validation is read-only. If a future code change
        // mutates the candidate bytes, refuse the replacement instead of silently
        // corrupting a known-good MDL.
        if (replacementBytes != replacementBytesFromDisk) {
            MessageBoxW(
                gMainWindow,
                L"Storyland attempted to modify the MDL before IMG insertion. The replacement was cancelled. "
                L"DTZ/IMG replacement must preserve MDL bytes exactly.",
                L"MDL byte-preservation guard",
                MB_OK | MB_ICONERROR);
            return;
        }
    }

    std::string report;
    if (!gDtzArchive.replaceDirEntryBytes(size_t(gSelectedIndex), replacementBytes, true, report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"DTZ+IMG replacement failed", MB_ICONERROR);
        return;
    }

    clearDtzFindState(true);
    populateDtzList();


    int replacementPreviewIndex = -1;
    const auto& updatedEntries = gDtzArchive.dirEntries();
    for (size_t index = 0; index < updatedEntries.size(); ++index) {
        if (updatedEntries[index].startSector == selectedStartSector) {
            replacementPreviewIndex = int(index);
            break;
        }
    }

    if (!textureRuntimeRepairDetail.empty()) {
        report += "\n";
        report += textureRuntimeRepairDetail;
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
    setStatus(L"Internal DTZ+IMG entry replaced in memory; right-click the GAME.DTZ tree and choose Rebuild GAME.DTZ As... to write GAME.DTZ and the companion IMG.");
    InvalidateRect(gPreview, nullptr, FALSE);
}

static void renameSelectedDtzDirEntry() {
    if (gMode != StorylandMode::DtzArchive || gSelectedKind != StorylandTreeKind::DtzDirEntry || gSelectedIndex < 0) {
        MessageBoxW(gMainWindow, L"Select a GAME.DTZ internal IMG resource first.", L"Rename resource", MB_ICONINFORMATION);
        return;
    }

    const auto& entries = gDtzArchive.dirEntries();
    if (size_t(gSelectedIndex) >= entries.size()) return;
    const StorylandDtzDirEntry selected = entries[size_t(gSelectedIndex)];

    std::wstring currentName = widen(selected.name);
    std::wstring newName = currentName;
    if (!askString(L"Rename GAME.DTZ Resource", L"Resource name:", currentName, newName)) return;

    std::string report;
    std::string error;
    if (!gDtzArchive.renameDirEntry(size_t(gSelectedIndex), narrow(newName), report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"Rename failed", MB_ICONERROR);
        return;
    }

    clearDtzFindState(true);
    populateDtzList();

    int reselect = -1;
    const auto& updated = gDtzArchive.dirEntries();
    for (size_t index = 0; index < updated.size(); ++index) {
        if (updated[index].startSector == selected.startSector &&
            updated[index].sectorCount == selected.sectorCount) {
            reselect = int(index);
            break;
        }
    }
    if (reselect >= 0) {
        selectTreePayloadItem(StorylandTreeKind::DtzDirEntry, reselect);
        selectDtzDirEntry(reselect);
    }
    setDetails(widen(report));
    setStatus(L"Resource renamed in GAME.DTZ memory; rebuild GAME.DTZ to save the name change.");
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
        MessageBoxW(gMainWindow, L"Select a gta3PS*.img sector-map line or a raw DTZ sector record first.", L"Storyland", MB_ICONINFORMATION);
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
    clearDtzFindState(true);
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
        MessageBoxW(gMainWindow, L"Selected GAME.DTZ data field is read-only.", L"Storyland", MB_ICONINFORMATION);
        return;
    }

    std::wstring newValue = editableDtzFieldInitialText(field);
    std::wstring label = std::wstring(L"New value for ") + widen(field.name) + L" (" + widen(field.type) + L")";
    if (!askString(L"Edit GAME.DTZ Field", label.c_str(), newValue, newValue)) return;

    std::string report;
    std::string error;
    if (!gDtzArchive.patchDataField(size_t(gSelectedIndex), narrow(newValue), report, error)) {
        MessageBoxW(gMainWindow, widen(error).c_str(), L"GAME.DTZ field edit failed", MB_ICONERROR);
        return;
    }

    clearDtzFindState(true);
    populateDtzList();
    setDetails(widen(report));
}


static void drawTexturePreview(HDC dc, RECT rc) {
    int dstW = std::max<LONG>(1, rc.right - rc.left);
    int dstH = std::max<LONG>(1, rc.bottom - rc.top);

    HDC frameDc = CreateCompatibleDC(dc);
    if (!frameDc) return;

    HBITMAP frameBitmap = CreateCompatibleBitmap(dc, dstW, dstH);
    if (!frameBitmap) {
        DeleteDC(frameDc);
        return;
    }

    HGDIOBJ oldFrameBitmap = SelectObject(frameDc, frameBitmap);
    RECT frameRect{0, 0, dstW, dstH};
    FillRect(frameDc, &frameRect, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

    if (gTextureBitmap && gCurrentImage.width > 0 && gCurrentImage.height > 0) {
        HDC textureDc = CreateCompatibleDC(dc);
        if (textureDc) {
            HGDIOBJ oldTextureBitmap = SelectObject(textureDc, gTextureBitmap);
            int srcW = gCurrentImage.width;
            int srcH = gCurrentImage.height;
            double scale = std::min(double(dstW) / double(srcW), double(dstH) / double(srcH));
            int outW = std::max(1, int(double(srcW) * scale));
            int outH = std::max(1, int(double(srcH) * scale));
            int x = (dstW - outW) / 2;
            int y = (dstH - outH) / 2;

            SetStretchBltMode(frameDc, HALFTONE);
            SetBrushOrgEx(frameDc, 0, 0, nullptr);
            if (gTexturePreviewFlipV) {
                StretchBlt(frameDc, x, y, outW, outH, textureDc, 0, srcH - 1, srcW, -srcH, SRCCOPY);
            } else {
                StretchBlt(frameDc, x, y, outW, outH, textureDc, 0, 0, srcW, srcH, SRCCOPY);
            }

            SelectObject(textureDc, oldTextureBitmap);
            DeleteDC(textureDc);
        }
    }

    BitBlt(dc, rc.left, rc.top, dstW, dstH, frameDc, 0, 0, SRCCOPY);
    SelectObject(frameDc, oldFrameBitmap);
    DeleteObject(frameBitmap);
    DeleteDC(frameDc);
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

static bool projectArchiveViewportPoint(
    float x,
    float y,
    float z,
    const GLdouble model[16],
    const GLdouble projection[16],
    const GLint viewport[4],
    int& screenX,
    int& screenY,
    float& depth) {
    const GLdouble in[4] = {x, y, z, 1.0};
    GLdouble eye[4] = {};
    GLdouble clip[4] = {};

    for (int row = 0; row < 4; ++row) {
        eye[row] = model[row] * in[0] + model[4 + row] * in[1] +
                   model[8 + row] * in[2] + model[12 + row] * in[3];
    }
    for (int row = 0; row < 4; ++row) {
        clip[row] = projection[row] * eye[0] + projection[4 + row] * eye[1] +
                    projection[8 + row] * eye[2] + projection[12 + row] * eye[3];
    }
    if (std::fabs(clip[3]) < 1.0e-9 || clip[3] <= 0.0) return false;

    const GLdouble ndcX = clip[0] / clip[3];
    const GLdouble ndcY = clip[1] / clip[3];
    const GLdouble ndcZ = clip[2] / clip[3];
    if (ndcX < -1.15 || ndcX > 1.15 || ndcY < -1.15 || ndcY > 1.15 || ndcZ < -1.0 || ndcZ > 1.0) return false;

    screenX = viewport[0] + int((ndcX + 1.0) * 0.5 * GLdouble(viewport[2]));
    const int openGlY = viewport[1] + int((ndcY + 1.0) * 0.5 * GLdouble(viewport[3]));
    screenY = viewport[3] - 1 - openGlY;
    depth = float((ndcZ + 1.0) * 0.5);
    return true;
}

static void drawArchivePreviewOpenGl(HWND hwnd, HDC dc, RECT rc) {
    gArchiveViewportPicks.clear();
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

        bool textureChanged =
            gArchiveTexturePreviewId == 0 ||
            gArchiveTexturePreviewIndex != gSelectedIndex ||
            gArchiveTexturePreviewHeaderOffset != texture.headerOffset ||
            gArchiveTexturePreviewWidth != imageWidth ||
            gArchiveTexturePreviewHeight != imageHeight;

        if (gArchiveTexturePreviewId == 0) {
            glGenTextures(1, &gArchiveTexturePreviewId);
        }

        glBindTexture(GL_TEXTURE_2D, gArchiveTexturePreviewId);
        if (textureChanged) {
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
            gArchiveTexturePreviewIndex = gSelectedIndex;
            gArchiveTexturePreviewHeaderOffset = texture.headerOffset;
            gArchiveTexturePreviewWidth = imageWidth;
            gArchiveTexturePreviewHeight = imageHeight;
        }

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

    bool shouldIncludeSectorExtents = selectedTextureId < 0;
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

    if (gArchiveViewFocusActive) {
        centerX = gArchiveViewFocusX;
        centerY = gArchiveViewFocusY;
        centerZ = gArchiveViewFocusZ;
        largestSpan = std::max(1.0f, gArchiveViewFocusSpan);

        const float gridHalfExtent = largestSpan * 2.0f;
        minX = centerX - gridHalfExtent; maxX = centerX + gridHalfExtent;
        minY = centerY - gridHalfExtent; maxY = centerY + gridHalfExtent;
        minZ = centerZ - gridHalfExtent; maxZ = centerZ + gridHalfExtent;
    }

    float modelScale = 2.4f / std::max(1.0f, largestSpan);

    glMultModelQuat(gModelViewRotation);
    glScalef(modelScale, modelScale, modelScale);
    glTranslatef(-centerX, -centerY, -centerZ);

    gArchiveViewportPicks.clear();
    GLdouble pickModel[16] = {};
    GLdouble pickProjection[16] = {};
    GLint pickViewport[4] = {};
    glGetDoublev(GL_MODELVIEW_MATRIX, pickModel);
    glGetDoublev(GL_PROJECTION_MATRIX, pickProjection);
    glGetIntegerv(GL_VIEWPORT, pickViewport);
    for (const auto& placement : placements) {
        if (selectedSectorIndex >= 0 && int(placement.sectorIndex) != selectedSectorIndex) continue;
        if (selectedTextureId >= 0) {
            uint64_t key = (uint64_t(placement.sectorIndex) << 32) | uint64_t(placement.resourceIndex);
            auto meshFoundForTexture = meshByKey.find(key);
            if (meshFoundForTexture == meshByKey.end() || meshFoundForTexture->second == nullptr) continue;
            if (!meshUsesTextureId(*meshFoundForTexture->second, uint32_t(selectedTextureId))) continue;
        }

        int screenX = 0;
        int screenY = 0;
        float depth = 1.0f;
        if (!projectArchiveViewportPoint(placement.x, placement.y, placement.z, pickModel, pickProjection, pickViewport, screenX, screenY, depth)) continue;

        int edgeX = 0;
        int edgeY = 0;
        float edgeDepth = 1.0f;
        int radius = 10;
        const float worldRadius = std::max(1.0f, std::min(20.0f, placement.boundRadius));
        if (projectArchiveViewportPoint(placement.x + worldRadius, placement.y, placement.z, pickModel, pickProjection, pickViewport, edgeX, edgeY, edgeDepth)) {
            radius = std::max(8, std::min(42, std::abs(edgeX - screenX) + 5));
        }
        gArchiveViewportPicks.push_back({screenX, screenY, radius, depth, placement.resourceIndex});
    }

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

    // Leeds WRLD stores geometry in render passes.  Drawing every pass as opaque
    // geometry makes the night-light shells and alpha meshes look like enormous
    // malformed buildings even though their transforms are valid.  Match the
    // retail pass intent: opaque first, then no-z-write, transparent, and finally
    // night-only light geometry.  SUPERLOD/LOD were already excluded while
    // parsing the placement lists.
    const bool worldNight = gStoriesSky.time() < 6.0f || gStoriesSky.time() >= 20.0f;
    auto placementRenderPhase = [&](const StorylandWorldPlacement& placement) -> int {
        if (placement.passName == "LIGHTS") return worldNight ? 3 : -1;
        if (placement.passName == "TRANSPARENT") return 2;
        if (placement.passName == "NOZWRITE") return 1;
        return 0;
    };

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    for (int renderPhase = 0; renderPhase < 4; ++renderPhase) {
        if (renderPhase == 0) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        } else if (renderPhase == 1) {
            glDisable(GL_BLEND);
            glDepthMask(GL_FALSE);
        } else if (renderPhase == 2) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
        } else {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDepthMask(GL_FALSE);
        }

        bool triangleBatchOpen = false;
        GLuint activeTexture = GLuint(-1);
        uint32_t activeMaterial = 0xFFFFFFFEu;
        for (const auto& placement : placements) {
            if (placementRenderPhase(placement) != renderPhase) continue;
            if (selectedSectorIndex >= 0 && int(placement.sectorIndex) != selectedSectorIndex) continue;
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
                const bool isSelectedResource = selectedResourceIndex >= 0 &&
                                                int(placement.resourceIndex) == selectedResourceIndex;
                uint32_t seed = tri.textureId == 0xFFFFFFFFu ? placement.resourceIndex : tri.textureId;
                float r = 0.30f + float((seed * 37u + 31u) & 0x7Fu) / 255.0f;
                float g = 0.30f + float((seed * 67u + 91u) & 0x7Fu) / 255.0f;
                float b = 0.30f + float((seed * 97u + 17u) & 0x7Fu) / 255.0f;
                const float fallbackAlpha = renderPhase == 2 ? 0.45f : (renderPhase == 3 ? 0.70f : 1.0f);
                if (isSelectedResource) {
                    glColor4f(1.0f, 0.12f, 0.12f, renderPhase >= 2 ? 0.85f : 1.0f);
                } else if (textureHandle != 0) {
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                } else {
                    glColor4f(r, g, b, fallbackAlpha);
                }

                const StorylandWorldMeshVertex* verts[3] = {
                    &mesh.vertices[tri.a],
                    &mesh.vertices[tri.b],
                    &mesh.vertices[tri.c]
                };

                // The retail sGeomInstance bounding sphere is authoritative for
                // this placement.  A decoded strip can contain false tail data
                // when the WRLD payload parser walks past the real packet end.
                // Those false vertices are exactly what produce the remaining
                // giant wedges/slabs even when the instance matrix itself is
                // correct.  Do not draw a triangle if any transformed vertex
                // falls materially outside the retail placement sphere.
                float transformed[3][3] = {};
                bool triangleInsideRetailBound = true;
                const float retailRadius = std::max(0.01f, std::fabs(placement.boundRadius));
                const float retailLimit = retailRadius * 1.20f + 0.50f;
                const float retailLimitSquared = retailLimit * retailLimit;
                for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex) {
                    const StorylandWorldMeshVertex* vertex = verts[vertexIndex];
                    const float wx = placement.matrix[0] * vertex->x + placement.matrix[4] * vertex->y + placement.matrix[8]  * vertex->z + placement.matrix[12];
                    const float wy = placement.matrix[1] * vertex->x + placement.matrix[5] * vertex->y + placement.matrix[9]  * vertex->z + placement.matrix[13];
                    const float wz = placement.matrix[2] * vertex->x + placement.matrix[6] * vertex->y + placement.matrix[10] * vertex->z + placement.matrix[14];
                    transformed[vertexIndex][0] = wx;
                    transformed[vertexIndex][1] = wy;
                    transformed[vertexIndex][2] = wz;
                    const float dx = wx - placement.boundX;
                    const float dy = wy - placement.boundY;
                    const float dz = wz - placement.boundZ;
                    if (dx * dx + dy * dy + dz * dz > retailLimitSquared) {
                        triangleInsideRetailBound = false;
                        break;
                    }
                }
                if (!triangleInsideRetailBound) continue;

                for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex) {
                    if (textureHandle != 0) glTexCoord2f(verts[vertexIndex]->u, verts[vertexIndex]->v);
                    glVertex3f(transformed[vertexIndex][0], transformed[vertexIndex][1], transformed[vertexIndex][2]);
                }

                drawnTriangles++;
            }
        }
        if (triangleBatchOpen) glEnd();
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
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
            if (selectedResourceIndex >= 0 && int(point.resourceIndex) == selectedResourceIndex) {
                glColor3f(1.0f, 0.12f, 0.12f);
            } else {
                glColor3f(point.isSectorFallback ? 0.35f : 0.78f,
                          point.isSectorFallback ? 0.55f : 0.78f,
                          point.isSectorFallback ? 1.0f : 0.70f);
            }
            glVertex3f(point.x, point.y, point.z);
        }
        glEnd();
        glPointSize(1.0f);
    }

    drawOpenGlViewCube(hwnd, dc, width, height);
    drawSelectedViewportLabelOpenGl(dc, width, height);

    glFlush();
    SwapBuffers(dc);
    wglMakeCurrent(nullptr, nullptr);

    std::wstring title = L"OpenGL LVZ+IMG mesh viewport  |  ";
    if (gSelectedKind == StorylandTreeKind::ArchiveMeshResource && selectedResourceIndex >= 0) {
        title += L"highlighted=" + widen(selectedName);
    } else if (selectedEntry || gSelectedKind == StorylandTreeKind::ArchiveTextureResource || gSelectedKind == StorylandTreeKind::ArchiveAnimationResource) {
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

static void drawOpenGlViewCube(HWND hwnd, HDC dc, int width, int height) {
    if (!gOpenGlShowViewCube) return;

    RECT rc = viewCubeRect(hwnd);
    const float centerX = float(rc.left + rc.right) * 0.5f;
    const float centerYTop = float(rc.top + rc.bottom) * 0.5f;
    const float centerY = float(height) - centerYTop;
    const float axisLength = 36.0f;

    stopStoriesShaderProgram();
    setupFixedPipelineStoriesLighting(false);

    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, double(width), 0.0, double(height), -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    struct Axis2D { StorylandVec3 axis; float r, g, b; };
    const Axis2D axes[3] = {
        {{1.0f, 0.0f, 0.0f}, 235.0f / 255.0f, 75.0f / 255.0f, 75.0f / 255.0f},
        {{0.0f, 1.0f, 0.0f}, 85.0f / 255.0f, 220.0f / 255.0f, 105.0f / 255.0f},
        {{0.0f, 0.0f, 1.0f}, 90.0f / 255.0f, 145.0f / 255.0f, 245.0f / 255.0f}
    };

    auto projected = [&](StorylandVec3 axis, int index) {
        StorylandVec3 v = rotateVecByQuat(gModelViewRotation, axis);
        float sx = v.x;
        float sy = v.y;
        float len = std::sqrt(sx * sx + sy * sy);
        if (len < 0.08f) {
            sx = index == 0 ? 1.0f : 0.3f;
            sy = index == 2 ? 1.0f : -0.3f;
            len = std::sqrt(sx * sx + sy * sy);
        }
        sx /= len;
        sy /= len;
        return StorylandVec3{sx, sy, 0.0f};
    };

    glLineWidth(2.2f);
    glBegin(GL_LINES);
    for (int i = 0; i < 3; ++i) {
        StorylandVec3 p = projected(axes[i].axis, i);
        glColor3f(axes[i].r, axes[i].g, axes[i].b);
        glVertex2f(centerX, centerY);
        glVertex2f(centerX + p.x * axisLength, centerY + p.y * axisLength);
    }
    glEnd();

    auto circle = [&](float x, float y, float radius, float r, float g, float b) {
        glColor3f(r, g, b);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int step = 0; step <= 20; ++step) {
            const float a = float(step) * 6.28318530718f / 20.0f;
            glVertex2f(x + std::cos(a) * radius, y + std::sin(a) * radius);
        }
        glEnd();
    };

    circle(centerX, centerY, 3.0f, 0.78f, 0.79f, 0.82f);
    for (int i = 0; i < 3; ++i) {
        StorylandVec3 p = projected(axes[i].axis, i);
        circle(centerX + p.x * axisLength, centerY + p.y * axisLength,
               7.0f, axes[i].r, axes[i].g, axes[i].b);
    }

    // Draw X/Y/Z into the OpenGL back buffer before SwapBuffers.
    // Drawing these later with GDI caused the labels to flicker because the next
    // OpenGL frame immediately replaced the GDI pixels.
    if (dc) {
        HFONT font = CreateFontW(
            -13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        if (font) {
            HGDIOBJ oldFont = SelectObject(dc, font);
            GLuint listBase = glGenLists(96);
            if (listBase != 0 && wglUseFontBitmapsW(dc, 32, 96, listBase)) {
                const char labels[3] = {'X', 'Y', 'Z'};
                glColor3f(0.98f, 0.98f, 0.99f);
                for (int i = 0; i < 3; ++i) {
                    StorylandVec3 q = projected(axes[i].axis, i);
                    const float x = centerX + q.x * axisLength - 4.0f;
                    const float y = centerY + q.y * axisLength - 5.0f;
                    glRasterPos2f(x, y);
                    glListBase(listBase - 32);
                    glCallLists(1, GL_UNSIGNED_BYTE, &labels[i]);
                }
                glDeleteLists(listBase, 96);
            } else if (listBase != 0) {
                glDeleteLists(listBase, 96);
            }
            SelectObject(dc, oldFont);
            DeleteObject(font);
        }
    }
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

    drawOpenGlViewCube(hwnd, dc, width, height);
    glFlush();
    SwapBuffers(dc);
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
    const auto& prelights = gModelFile.previewPrelights();
    const auto& bones = gModelFile.armatureBones();
    const auto& lights2dfx = gModelFile.preview2dfxLights();
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
    setStoriesViewportClearColor();
    gOpenGlRenderer.BeginFrame(width, height);

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

    for (const auto& light : lights2dfx) {
        includePointInBounds(light.position.x, light.position.y, light.position.z);
    }

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    spanX = std::max(0.001f, maxX - minX);
    spanY = std::max(0.001f, maxY - minY);
    spanZ = std::max(0.001f, maxZ - minZ);
    float largestSpan = std::max(spanX, std::max(spanY, spanZ));
    float modelScale = 2.0f / std::max(0.001f, largestSpan);

    canUseTexture = textureAvailable && !pts.empty() && hasRealTexcoords;
    usingProjectedTexcoords = false;

    glScalef(modelScale, modelScale, modelScale);
    glTranslatef(-centerX, -centerY, -centerZ);

    float gridSize = std::max(1.0f, largestSpan);
    if (gOpenGlShowGrid) {
        setupFixedPipelineStoriesLighting(false);
        gOpenGlRenderer.DrawGrid(gridSize, gridSize / 10.0f);
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
            auto setVertexPrelight = [&](uint32_t vertexIndex) {
                if (gOpenGlRenderMode == StorylandOpenGlRenderMode::Solid) {
                    glColor4f(0.76f, 0.78f, 0.74f, 1.0f);
                    return;
                }
                if (gPrelightViewMode == StorylandPrelightViewMode::Off ||
                    vertexIndex >= prelights.size() || !prelights[vertexIndex].valid) {
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                    return;
                }
                const StorylandModelPrelight& c = prelights[vertexIndex];
                glColor4ub(c.red, c.green, c.blue, c.alpha);
            };
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
                setVertexPrelight(tri.a);
                if (wantsTexture) emitModelPreviewTexcoordInRegion(tri.a, textureRegion, pts, texcoords, hasRealTexcoords, minX, minY, minZ, spanX, spanY, spanZ);
                emitPreviewVertexWithNormal(a, normalA);
                setVertexPrelight(tri.b);
                if (wantsTexture) emitModelPreviewTexcoordInRegion(tri.b, textureRegion, pts, texcoords, hasRealTexcoords, minX, minY, minZ, spanX, spanY, spanZ);
                emitPreviewVertexWithNormal(b, normalB);
                setVertexPrelight(tri.c);
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

    if (gOpenGlShow2dfxLights && !lights2dfx.empty()) {
        stopStoriesShaderProgram();
        setupFixedPipelineStoriesLighting(false);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        GLfloat modelView[16] = {};
        glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
        StorylandModelPoint cameraRight{modelView[0], modelView[4], modelView[8]};
        StorylandModelPoint cameraUp{modelView[1], modelView[5], modelView[9]};
        float rightLength = std::sqrt(cameraRight.x * cameraRight.x + cameraRight.y * cameraRight.y + cameraRight.z * cameraRight.z);
        float upLength = std::sqrt(cameraUp.x * cameraUp.x + cameraUp.y * cameraUp.y + cameraUp.z * cameraUp.z);
        if (rightLength > 0.000001f) { cameraRight.x /= rightLength; cameraRight.y /= rightLength; cameraRight.z /= rightLength; }
        if (upLength > 0.000001f) { cameraUp.x /= upLength; cameraUp.y /= upLength; cameraUp.z /= upLength; }

        const bool night = gStoriesSky.time() < 6.0f || gStoriesSky.time() >= 20.0f;
        const DWORD lightTick = GetTickCount();
        auto drawCoronaFan = [&](const StorylandModelLight2dfx& light, float radius, float alphaScale) {
            const int segments = 24;
            glBegin(GL_TRIANGLE_FAN);
            glColor4ub(light.red, light.green, light.blue,
                       uint8_t(std::clamp(float(light.alpha) * alphaScale, 0.0f, 255.0f)));
            glVertex3f(light.position.x, light.position.y, light.position.z);
            glColor4ub(light.red, light.green, light.blue, 0u);
            for (int segment = 0; segment <= segments; ++segment) {
                float angle = float(segment) * 6.28318530718f / float(segments);
                float cs = std::cos(angle);
                float sn = std::sin(angle);
                glVertex3f(
                    light.position.x + cameraRight.x * (cs * radius) + cameraUp.x * (sn * radius),
                    light.position.y + cameraRight.y * (cs * radius) + cameraUp.y * (sn * radius),
                    light.position.z + cameraRight.z * (cs * radius) + cameraUp.z * (sn * radius));
            }
            glEnd();
        };

        for (const auto& light : lights2dfx) {
            const bool dayOnly = (light.flags1 & 0x20u) != 0u;
            const bool nightOnly = (light.flags1 & 0x40u) != 0u;
            if (dayOnly && night && !nightOnly) continue;
            if (nightOnly && !night && !dayOnly) continue;
            const bool blinking = (light.flags1 & 0x80u) != 0u || (light.flags2 & 0x02u) != 0u || (light.flags2 & 0x10u) != 0u;
            if (blinking && ((lightTick / 420u) & 1u) == 0u) continue;

            if (light.pointLightRange > 0.0f) {
                float ringRadius = std::max(0.02f, light.pointLightRange);
                glLineWidth(1.0f);
                glColor4ub(light.red, light.green, light.blue, 72u);
                glBegin(GL_LINE_LOOP);
                for (int segment = 0; segment < 32; ++segment) {
                    float angle = float(segment) * 6.28318530718f / 32.0f;
                    glVertex3f(light.position.x + std::cos(angle) * ringRadius,
                               light.position.y + std::sin(angle) * ringRadius,
                               light.position.z);
                }
                glEnd();
            }

            if ((light.flags1 & 0x08u) == 0u) {
                float radius = std::max(0.025f, light.coronaSize * 0.12f);
                drawCoronaFan(light, radius * 1.85f, 0.20f);
                drawCoronaFan(light, radius, 0.82f);
            }
        }

        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
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
        gOpenGlRenderer.DrawBounds(minX, minY, minZ, maxX, maxY, maxZ);
    }

    if (gOpenGlQuadView) {
        stopStoriesShaderProgram();
        setupFixedPipelineStoriesLighting(false);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        setStoriesViewportClearColor();
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        struct QuadViewDef { int x, y, w, h; StorylandQuat rotation; bool perspective; };
        const int halfW = std::max(1, width / 2);
        const int halfH = std::max(1, height / 2);
        const QuadViewDef views[4] = {
            {0, halfH, halfW, height - halfH, StorylandQuat{}, false},
            {halfW, halfH, width - halfW, height - halfH, quatFromAxisAngle(-90.0f, 0.0f, 1.0f, 0.0f), false},
            {0, 0, halfW, halfH, quatFromAxisAngle(-90.0f, 1.0f, 0.0f, 0.0f), false},
            {halfW, 0, width - halfW, halfH, quatMul(quatFromAxisAngle(-22.0f, 1.0f, 0.0f, 0.0f), quatFromAxisAngle(38.0f, 0.0f, 1.0f, 0.0f)), true}
        };

        for (const QuadViewDef& view : views) {
            glViewport(view.x, view.y, std::max(1, view.w), std::max(1, view.h));
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            const double qAspect = double(std::max(1, view.w)) / double(std::max(1, view.h));
            if (view.perspective) {
                setPerspectiveProjection(42.0, qAspect, 0.01, 500.0);
            } else {
                const double extent = 1.55;
                if (qAspect >= 1.0) glOrtho(-extent * qAspect, extent * qAspect, -extent, extent, -50.0, 50.0);
                else glOrtho(-extent, extent, -extent / qAspect, extent / qAspect, -50.0, 50.0);
            }
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            if (view.perspective) glTranslatef(0.0f, 0.0f, -4.25f);
            glMultModelQuat(view.rotation);
            glScalef(modelScale, modelScale, modelScale);
            glTranslatef(-centerX, -centerY, -centerZ);

            if (gOpenGlShowGrid) {
                glLineWidth(1.0f);
                glColor3f(0.19f, 0.20f, 0.23f);
                glBegin(GL_LINES);
                for (int i = -10; i <= 10; ++i) {
                    float d = float(i) * gridSize / 10.0f;
                    glVertex3f(-gridSize, d, 0.0f); glVertex3f(gridSize, d, 0.0f);
                    glVertex3f(d, -gridSize, 0.0f); glVertex3f(d, gridSize, 0.0f);
                }
                glEnd();
            }

            if (!tris.empty()) {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                glColor3f(0.56f, 0.59f, 0.64f);
                glBegin(GL_TRIANGLES);
                for (const auto& tri : tris) {
                    if (tri.a >= pts.size() || tri.b >= pts.size() || tri.c >= pts.size()) continue;
                    const StorylandModelPoint& a = pts[tri.a];
                    const StorylandModelPoint& b = pts[tri.b];
                    const StorylandModelPoint& c = pts[tri.c];
                    StorylandModelPoint n = previewTriangleNormal(a, b, c);
                    glNormal3f(n.x, n.y, n.z);
                    glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z); glVertex3f(c.x, c.y, c.z);
                }
                glEnd();
                glDisable(GL_CULL_FACE);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glLineWidth(1.0f);
                glColor3f(0.12f, 0.13f, 0.15f);
                glBegin(GL_TRIANGLES);
                for (const auto& tri : tris) {
                    if (tri.a >= pts.size() || tri.b >= pts.size() || tri.c >= pts.size()) continue;
                    glVertex3f(pts[tri.a].x, pts[tri.a].y, pts[tri.a].z);
                    glVertex3f(pts[tri.b].x, pts[tri.b].y, pts[tri.b].z);
                    glVertex3f(pts[tri.c].x, pts[tri.c].y, pts[tri.c].z);
                }
                glEnd();
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            } else {
                glPointSize(3.0f);
                glColor3f(0.72f, 0.75f, 0.82f);
                glBegin(GL_POINTS);
                for (const auto& p : pts) glVertex3f(p.x, p.y, p.z);
                glEnd();
            }
        }

        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0.0, double(width), 0.0, double(height), -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
        glColor3f(0.40f, 0.42f, 0.46f);
        glBegin(GL_LINES);
        glVertex2f(float(halfW), 0.0f); glVertex2f(float(halfW), float(height));
        glVertex2f(0.0f, float(halfH)); glVertex2f(float(width), float(halfH));
        glEnd();

        drawSelectedViewportLabelOpenGl(dc, width, height);
        glFlush();
        SwapBuffers(dc);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(210, 214, 222));
        TextOutW(dc, 10, 8, L"Front", 5);
        TextOutW(dc, halfW + 10, 8, L"Right", 5);
        TextOutW(dc, 10, halfH + 8, L"Top", 3);
        TextOutW(dc, halfW + 10, halfH + 8, L"Perspective", 11);
        wglMakeCurrent(nullptr, nullptr);
        return;
    }

    drawOpenGlViewCube(hwnd, dc, width, height);
    drawSelectedViewportLabelOpenGl(dc, width, height);

    glFlush();
    SwapBuffers(dc);
    wglMakeCurrent(nullptr, nullptr);
}


struct StorylandDtz2dfxPreviewLight {
    const StorylandDtz2dfxEffect* effect = nullptr;
    const StorylandDtz2dfxWorldInstance* worldInstance = nullptr;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

static float leeds2dfxLightPulse(const StorylandDtz2dfxEffect& effect, DWORD tick, bool night) {
    const bool nightOnly =
        effect.lightType == 1u || effect.lightType == 3u || effect.lightType == 5u ||
        effect.lightType == 7u || effect.lightType == 9u || effect.lightType == 11u;
    float activeScale = (nightOnly && !night) ? 0.18f : 1.0f;

    switch (effect.lightType) {
    case 2:
    case 3: {
        float phase = float((tick + effect.index * 137u) % 900u) / 900.0f;
        activeScale *= 0.35f + 0.65f * std::fabs(std::sin(phase * 18.8495559f));
        break;
    }
    case 4:
    case 5:
        activeScale *= (((tick / 520u) + effect.index) & 1u) ? 1.0f : 0.20f;
        break;
    case 6:
    case 7:
        activeScale *= (((tick / 780u) + effect.index) % 3u) == 0u ? 1.0f : 0.18f;
        break;
    case 8:
    case 9:
        activeScale *= (((tick / 260u) + effect.index) & 1u) ? 1.0f : 0.12f;
        break;
    case 10:
    case 11: {
        uint32_t seed = uint32_t(tick / 110u) ^ (effect.index * 1664525u + 1013904223u);
        seed ^= seed >> 16;
        activeScale *= (seed & 3u) == 0u ? 0.16f : 0.72f + float((seed >> 8) & 0xFFu) / 900.0f;
        break;
    }
    case 13:
        activeScale *= (((tick / 640u) + effect.index) & 1u) ? 1.0f : 0.16f;
        break;
    case 14:
        activeScale *= (((tick / 640u) + effect.index + 1u) & 1u) ? 1.0f : 0.16f;
        break;
    default:
        break;
    }

    return std::clamp(activeScale, 0.08f, 1.0f);
}

static bool dtz2dfxPreviewLightSelected(const StorylandDtz2dfxPreviewLight& preview) {
    if (gSelectedKind == StorylandTreeKind::DtzLeeds2dfxWorld) {
        return preview.worldInstance != nullptr && gSelectedIndex == int(preview.worldInstance->index);
    }
    if (gSelectedKind == StorylandTreeKind::DtzLeeds2dfx) {
        return preview.effect != nullptr && gSelectedIndex == int(preview.effect->index);
    }
    return false;
}

static std::vector<StorylandDtz2dfxPreviewLight> buildDtz2dfxPreviewLights() {
    const auto& effects = gDtzArchive.leeds2dfxEffects();
    const auto& worldInstances = gDtzArchive.leeds2dfxWorldInstances();

    std::vector<StorylandDtz2dfxPreviewLight> worldLights;
    worldLights.reserve(worldInstances.size());
    for (const auto& instance : worldInstances) {
        if (!instance.valid || instance.effectIndex >= effects.size()) continue;
        const auto& effect = effects[instance.effectIndex];
        if (!effect.valid || !effect.isLight) continue;

        StorylandDtz2dfxPreviewLight preview;
        preview.effect = &effect;
        preview.worldInstance = &instance;
        preview.x = instance.worldX;
        preview.y = instance.worldY;
        preview.z = instance.worldZ;
        worldLights.push_back(preview);
    }
    if (!worldLights.empty()) return worldLights;

    std::map<int32_t, std::vector<const StorylandDtz2dfxEffect*>> groups;
    for (const auto& effect : effects) {
        if (!effect.valid || !effect.isLight) continue;
        int32_t groupKey = effect.hasModelAssociation ? effect.modelIndex : -1;
        groups[groupKey].push_back(&effect);
    }

    struct GroupLayout {
        int32_t key = -1;
        std::vector<const StorylandDtz2dfxEffect*> effects;
        float centerX = 0.0f;
        float centerY = 0.0f;
        float centerZ = 0.0f;
        float span = 1.0f;
    };

    std::vector<GroupLayout> layouts;
    layouts.reserve(groups.size());
    float maximumGroupSpan = 1.0f;
    for (const auto& pair : groups) {
        GroupLayout layout;
        layout.key = pair.first;
        layout.effects = pair.second;

        bool haveBounds = false;
        float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
        float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
        for (const StorylandDtz2dfxEffect* effect : layout.effects) {
            if (!haveBounds) {
                minX = maxX = effect->localX;
                minY = maxY = effect->localY;
                minZ = maxZ = effect->localZ;
                haveBounds = true;
            } else {
                minX = std::min(minX, effect->localX);
                minY = std::min(minY, effect->localY);
                minZ = std::min(minZ, effect->localZ);
                maxX = std::max(maxX, effect->localX);
                maxY = std::max(maxY, effect->localY);
                maxZ = std::max(maxZ, effect->localZ);
            }
        }
        layout.centerX = (minX + maxX) * 0.5f;
        layout.centerY = (minY + maxY) * 0.5f;
        layout.centerZ = (minZ + maxZ) * 0.5f;
        layout.span = std::max(1.0f, std::max(maxX - minX, std::max(maxY - minY, maxZ - minZ)));
        maximumGroupSpan = std::max(maximumGroupSpan, layout.span);
        layouts.push_back(std::move(layout));
    }

    std::vector<StorylandDtz2dfxPreviewLight> previewLights;
    size_t totalLights = 0;
    for (const GroupLayout& layout : layouts) totalLights += layout.effects.size();
    previewLights.reserve(totalLights);
    if (layouts.empty()) return previewLights;

    int columns = std::max(1, int(std::ceil(std::sqrt(double(layouts.size())))));
    int rows = std::max(1, int((layouts.size() + size_t(columns) - 1u) / size_t(columns)));
    float spacing = std::max(6.0f, maximumGroupSpan * 1.55f + 2.0f);

    for (size_t groupIndex = 0; groupIndex < layouts.size(); ++groupIndex) {
        const GroupLayout& layout = layouts[groupIndex];
        int column = int(groupIndex % size_t(columns));
        int row = int(groupIndex / size_t(columns));
        float groupX = (float(column) - float(columns - 1) * 0.5f) * spacing;
        float groupY = (float(rows - 1) * 0.5f - float(row)) * spacing;

        for (const StorylandDtz2dfxEffect* effect : layout.effects) {
            StorylandDtz2dfxPreviewLight preview;
            preview.effect = effect;
            preview.x = effect->localX - layout.centerX + groupX;
            preview.y = effect->localY - layout.centerY + groupY;
            preview.z = effect->localZ - layout.centerZ;
            previewLights.push_back(preview);
        }
    }

    return previewLights;
}

static void drawDtzLeeds2dfxPreviewOpenGl(HWND hwnd, HDC dc, RECT rc) {
    std::array<const StorylandArchiveBrowser*, 3> areaBrowsers{};
    size_t loadedAreas = 0;
    for (size_t i = 0; i < areaBrowsers.size(); ++i) {
        areaBrowsers[i] = loadedDtzAreaBrowser(i);
        if (areaBrowsers[i]) ++loadedAreas;
    }
    if (!gOpenGlShow2dfxLights && loadedAreas == 0) {
        HBRUSH brush = CreateSolidBrush(RGB(181, 221, 242));
        FillRect(dc, &rc, brush);
        DeleteObject(brush);
        std::wstring text = L"Leeds GAME.DTZ 2DFX display is disabled in View > OpenGL preview.";
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(18, 34, 50));
        TextOutW(dc, rc.left + 12, rc.top + 12, text.c_str(), int(text.size()));
        return;
    }
    std::vector<StorylandDtz2dfxPreviewLight> lights;
    if (gOpenGlShow2dfxLights) lights = buildDtz2dfxPreviewLights();
    if (lights.empty() && loadedAreas == 0) {
        HBRUSH brush = CreateSolidBrush(RGB(181, 221, 242));
        FillRect(dc, &rc, brush);
        DeleteObject(brush);
        std::wstring text = L"No valid Leeds GAME.DTZ C2dEffect lights were decoded.";
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(18, 34, 50));
        TextOutW(dc, rc.left + 12, rc.top + 12, text.c_str(), int(text.size()));
        return;
    }

    if (!gOpenGlReady && !initializeOpenGlPreview(hwnd)) {
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        std::wstring title = L"OpenGL init failed for Leeds GAME.DTZ 2DFX preview.";
        TextOutW(dc, rc.left + 8, rc.top + 8, title.c_str(), int(title.size()));
        return;
    }
    if (!wglMakeCurrent(dc, gOpenGlContext)) {
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        std::wstring title = L"OpenGL context activation failed for Leeds GAME.DTZ 2DFX preview.";
        TextOutW(dc, rc.left + 8, rc.top + 8, title.c_str(), int(title.size()));
        return;
    }

    const int width = std::max<int>(1, int(rc.right - rc.left));
    const int height = std::max<int>(1, int(rc.bottom - rc.top));
    glViewport(0, 0, width, height);
    setStoriesViewportClearColor();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    double aspect = double(width) / double(height);
    setPerspectiveProjection(45.0, aspect, 0.01, 1000.0);
    drawStoriesViewportSky(100.0f);

    bool haveBounds = false;
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    for (const auto& light : lights) {
        if (!haveBounds) {
            minX = maxX = light.x;
            minY = maxY = light.y;
            minZ = maxZ = light.z;
            haveBounds = true;
        } else {
            minX = std::min(minX, light.x); maxX = std::max(maxX, light.x);
            minY = std::min(minY, light.y); maxY = std::max(maxY, light.y);
            minZ = std::min(minZ, light.z); maxZ = std::max(maxZ, light.z);
        }
    }
    // The decoded area placement matrices and DTZ CEntity light positions share
    // world coordinates. Include both in the camera framing and draw both below.
    for (const StorylandArchiveBrowser* browser : areaBrowsers) {
        if (!browser) continue;
        for (const auto& placement : browser->placements()) {
            const float radius = std::isfinite(placement.boundRadius)
                ? std::clamp(placement.boundRadius, 0.0f, 500.0f) : 0.0f;
            if (!std::isfinite(placement.x) || !std::isfinite(placement.y) ||
                !std::isfinite(placement.z)) continue;
            if (!haveBounds) {
                minX = maxX = placement.x;
                minY = maxY = placement.y;
                minZ = maxZ = placement.z;
                haveBounds = true;
            }
            minX = std::min(minX, placement.x - radius); maxX = std::max(maxX, placement.x + radius);
            minY = std::min(minY, placement.y - radius); maxY = std::max(maxY, placement.y + radius);
            minZ = std::min(minZ, placement.z - radius); maxZ = std::max(maxZ, placement.z + radius);
        }
    }

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    float spanX = std::max(0.001f, maxX - minX);
    float spanY = std::max(0.001f, maxY - minY);
    float spanZ = std::max(0.001f, maxZ - minZ);
    float largestSpan = std::max(spanX, std::max(spanY, spanZ));
    float modelScale = 3.6f / std::max(1.0f, largestSpan);
    const bool worldPositioned = !lights.empty() && lights.front().worldInstance != nullptr;
    const float minimumVisibleRadius = worldPositioned ? std::max(0.10f, largestSpan * 0.0015f) : 0.06f;
    const int coronaSegments = lights.size() > 1500u ? 12 : (lights.size() > 500u ? 16 : 24);
    const bool drawAllRangeRings = lights.size() < 400u;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(gModelPanX, gModelPanY, -gModelDistance);
    glMultModelQuat(gModelViewRotation);
    glScalef(modelScale, modelScale, modelScale);
    glTranslatef(-centerX, -centerY, -centerZ);

    stopStoriesShaderProgram();
    setupFixedPipelineStoriesLighting(false);

    size_t drawnAreaInstances = 0;
    size_t drawnAreaTriangles = 0;
    size_t drawnTexturedTriangles = 0;
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    for (size_t areaIndex = 0; areaIndex < areaBrowsers.size(); ++areaIndex) {
        const StorylandArchiveBrowser* browser = areaBrowsers[areaIndex];
        if (!browser) continue;

        std::map<uint64_t, const StorylandWorldMesh*> meshByKey;
        for (const auto& mesh : browser->worldMeshes()) {
            const uint64_t key = (uint64_t(mesh.sectorIndex) << 32) | mesh.resourceIndex;
            meshByKey.emplace(key, &mesh);
        }

        std::map<uint32_t, const StorylandDirectTextureResource*> boundTextureByMaterial;
        for (const auto& texture : browser->directTextures()) {
            if (texture.materialId < 0 || texture.width <= 0 || texture.height <= 0 || texture.rgba.empty()) continue;
            boundTextureByMaterial.emplace(uint32_t(texture.materialId), &texture);
        }

        std::map<uint32_t, GLuint> uploadedTextures;
        auto textureForMaterial = [&](uint32_t materialId) -> GLuint {
            const auto uploaded = uploadedTextures.find(materialId);
            if (uploaded != uploadedTextures.end()) return uploaded->second;

            const auto foundTexture = boundTextureByMaterial.find(materialId);
            if (foundTexture == boundTextureByMaterial.end() || foundTexture->second == nullptr) return 0;

            const StorylandDirectTextureResource& texture = *foundTexture->second;
            GLuint handle = 0;
            glGenTextures(1, &handle);
            if (handle == 0) return 0;

            glBindTexture(GL_TEXTURE_2D, handle);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width, texture.height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, texture.rgba.data());
            uploadedTextures.emplace(materialId, handle);
            return handle;
        };

        // Render WRLD pass classes with the same intent as the standalone
        // LVZ+IMG viewport.  In particular, LIGHTS is a night-only geometry
        // pass and TRANSPARENT must not be drawn as opaque world geometry.
        const bool areaNight = gStoriesSky.time() < 6.0f || gStoriesSky.time() >= 20.0f;
        auto placementRenderPhase = [&](const StorylandWorldPlacement& placement) -> int {
            if (placement.passName == "LIGHTS") return areaNight ? 3 : -1;
            if (placement.passName == "TRANSPARENT") return 2;
            if (placement.passName == "NOZWRITE") return 1;
            return 0;
        };

        for (int renderPhase = 0; renderPhase < 4; ++renderPhase) {
            if (renderPhase == 0) {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            } else if (renderPhase == 1) {
                glDisable(GL_BLEND);
                glDepthMask(GL_FALSE);
            } else if (renderPhase == 2) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            } else {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glDepthMask(GL_FALSE);
            }

            GLuint activeTexture = GLuint(-1);
            uint32_t activeMaterial = 0xFFFFFFFEu;
            bool triangleBatchOpen = false;

            for (const auto& placement : browser->placements()) {
                if (placementRenderPhase(placement) != renderPhase) continue;
                const uint64_t key = (uint64_t(placement.sectorIndex) << 32) | placement.resourceIndex;
                const auto found = meshByKey.find(key);
                if (found == meshByKey.end() || found->second == nullptr) continue;

                const StorylandWorldMesh& mesh = *found->second;
                if (mesh.vertices.empty() || mesh.triangles.empty()) continue;
                ++drawnAreaInstances;

                for (const auto& triangle : mesh.triangles) {
                    if (triangle.a >= mesh.vertices.size() || triangle.b >= mesh.vertices.size() ||
                        triangle.c >= mesh.vertices.size()) continue;

                    const uint32_t desiredMaterial = triangle.textureId;
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

                    if (textureHandle != 0) {
                        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                        ++drawnTexturedTriangles;
                    } else {
                        const uint32_t seed = triangle.textureId == 0xFFFFFFFFu
                            ? placement.resourceIndex : triangle.textureId;
                        const float tint = areaIndex == 0 ? 0.07f : (areaIndex == 1 ? 0.0f : 0.12f);
                        const float alpha = renderPhase == 2 ? 0.45f : (renderPhase == 3 ? 0.70f : 1.0f);
                        glColor4f(0.32f + float((seed * 37u + 31u) & 0x7Fu) / 255.0f + tint,
                                  0.32f + float((seed * 67u + 91u) & 0x7Fu) / 255.0f + tint,
                                  0.32f + float((seed * 97u + 17u) & 0x7Fu) / 255.0f,
                                  alpha);
                    }

                    const StorylandWorldMeshVertex* vertices[3] = {
                        &mesh.vertices[triangle.a],
                        &mesh.vertices[triangle.b],
                        &mesh.vertices[triangle.c]
                    };

                    float transformed[3][3] = {};
                    bool triangleInsideRetailBound = true;
                    const float retailRadius = std::max(0.01f, std::fabs(placement.boundRadius));
                    const float retailLimit = retailRadius * 1.20f + 0.50f;
                    const float retailLimitSquared = retailLimit * retailLimit;
                    for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex) {
                        const StorylandWorldMeshVertex* vertex = vertices[vertexIndex];
                        const float wx = placement.matrix[0] * vertex->x + placement.matrix[4] * vertex->y + placement.matrix[8]  * vertex->z + placement.matrix[12];
                        const float wy = placement.matrix[1] * vertex->x + placement.matrix[5] * vertex->y + placement.matrix[9]  * vertex->z + placement.matrix[13];
                        const float wz = placement.matrix[2] * vertex->x + placement.matrix[6] * vertex->y + placement.matrix[10] * vertex->z + placement.matrix[14];
                        transformed[vertexIndex][0] = wx;
                        transformed[vertexIndex][1] = wy;
                        transformed[vertexIndex][2] = wz;
                        const float dx = wx - placement.boundX;
                        const float dy = wy - placement.boundY;
                        const float dz = wz - placement.boundZ;
                        if (dx * dx + dy * dy + dz * dz > retailLimitSquared) {
                            triangleInsideRetailBound = false;
                            break;
                        }
                    }
                    if (!triangleInsideRetailBound) continue;

                    for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex) {
                        if (textureHandle != 0) glTexCoord2f(vertices[vertexIndex]->u, vertices[vertexIndex]->v);
                        glVertex3f(transformed[vertexIndex][0], transformed[vertexIndex][1], transformed[vertexIndex][2]);
                    }
                    ++drawnAreaTriangles;
                }
            }

            if (triangleBatchOpen) glEnd();
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        for (const auto& uploaded : uploadedTextures) {
            GLuint handle = uploaded.second;
            if (handle != 0) glDeleteTextures(1, &handle);
        }
    }
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    if (gOpenGlShowGrid) {
        float gridExtent = std::max(2.0f, largestSpan * 0.6f);
        float step = gridExtent / 10.0f;
        glLineWidth(1.0f);
        glColor4f(0.18f, 0.23f, 0.28f, 0.55f);
        glBegin(GL_LINES);
        for (int index = -10; index <= 10; ++index) {
            float d = float(index) * step;
            glVertex3f(centerX - gridExtent, centerY + d, centerZ);
            glVertex3f(centerX + gridExtent, centerY + d, centerZ);
            glVertex3f(centerX + d, centerY - gridExtent, centerZ);
            glVertex3f(centerX + d, centerY + gridExtent, centerZ);
        }
        glEnd();
    }

    GLfloat modelView[16] = {};
    glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
    StorylandModelPoint cameraRight{modelView[0], modelView[4], modelView[8]};
    StorylandModelPoint cameraUp{modelView[1], modelView[5], modelView[9]};
    float rightLength = std::sqrt(cameraRight.x * cameraRight.x + cameraRight.y * cameraRight.y + cameraRight.z * cameraRight.z);
    float upLength = std::sqrt(cameraUp.x * cameraUp.x + cameraUp.y * cameraUp.y + cameraUp.z * cameraUp.z);
    if (rightLength > 0.000001f) {
        cameraRight.x /= rightLength; cameraRight.y /= rightLength; cameraRight.z /= rightLength;
    }
    if (upLength > 0.000001f) {
        cameraUp.x /= upLength; cameraUp.y /= upLength; cameraUp.z /= upLength;
    }

    const bool night = gStoriesSky.time() < 6.0f || gStoriesSky.time() >= 20.0f;
    const DWORD tick = GetTickCount();
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    auto drawCorona = [&](const StorylandDtz2dfxPreviewLight& preview, float radius, float alphaScale) {
        const auto& effect = *preview.effect;
        const int segments = coronaSegments;
        glBegin(GL_TRIANGLE_FAN);
        glColor4ub(effect.red, effect.green, effect.blue,
                   uint8_t(std::clamp(float(effect.alpha) * alphaScale, 0.0f, 255.0f)));
        glVertex3f(preview.x, preview.y, preview.z);
        glColor4ub(effect.red, effect.green, effect.blue, 0u);
        for (int segment = 0; segment <= segments; ++segment) {
            float angle = float(segment) * 6.28318530718f / float(segments);
            float cs = std::cos(angle);
            float sn = std::sin(angle);
            glVertex3f(
                preview.x + cameraRight.x * cs * radius + cameraUp.x * sn * radius,
                preview.y + cameraRight.y * cs * radius + cameraUp.y * sn * radius,
                preview.z + cameraRight.z * cs * radius + cameraUp.z * sn * radius);
        }
        glEnd();
    };

    for (const auto& preview : lights) {
        const auto& effect = *preview.effect;
        const bool selected = dtz2dfxPreviewLightSelected(preview);
        float pulse = leeds2dfxLightPulse(effect, tick, night);
        float range = std::fabs(effect.pointLightRange);
        if (range > 0.0001f && (drawAllRangeRings || selected)) {
            float ringRadius = std::max(minimumVisibleRadius, range);
            glLineWidth(selected ? 2.5f : 1.0f);
            glColor4ub(effect.red, effect.green, effect.blue, uint8_t(45.0f + 95.0f * pulse));
            glBegin(GL_LINE_LOOP);
            const int ringSegments = lights.size() > 1000u ? 16 : 36;
            for (int segment = 0; segment < ringSegments; ++segment) {
                float angle = float(segment) * 6.28318530718f / float(ringSegments);
                glVertex3f(preview.x + std::cos(angle) * ringRadius,
                           preview.y + std::sin(angle) * ringRadius,
                           preview.z);
            }
            glEnd();
        }

        float radius = std::max(minimumVisibleRadius, std::fabs(effect.coronaSize) * 0.18f);
        if ((effect.flags & 0x10u) != 0u) radius *= 1.35f;
        if (lights.size() > 1500u) {
            drawCorona(preview, radius * 1.85f, 0.20f * pulse);
            drawCorona(preview, radius, 0.88f * pulse);
        } else {
            drawCorona(preview, radius * 2.3f, 0.12f * pulse);
            drawCorona(preview, radius * 1.45f, 0.30f * pulse);
            drawCorona(preview, radius, 0.88f * pulse);
        }

        if (selected) {
            float marker = std::max(minimumVisibleRadius * 2.0f, radius * 1.8f);
            glLineWidth(2.5f);
            glColor4ub(255u, 255u, 255u, 220u);
            glBegin(GL_LINES);
            glVertex3f(preview.x - marker, preview.y, preview.z);
            glVertex3f(preview.x + marker, preview.y, preview.z);
            glVertex3f(preview.x, preview.y - marker, preview.z);
            glVertex3f(preview.x, preview.y + marker, preview.z);
            glVertex3f(preview.x, preview.y, preview.z - marker);
            glVertex3f(preview.x, preview.y, preview.z + marker);
            glEnd();
        }
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);

    drawOpenGlViewCube(hwnd, dc, width, height);

    std::wstringstream overlay;
    if (loadedAreas) {
        overlay << L"GAME.DTZ  |  " << loadedAreas << L" areas (";
        bool first = true;
        for (size_t index = 0; index < areaBrowsers.size(); ++index) {
            if (!areaBrowsers[index]) continue;
            if (!first) overlay << L", ";
            overlay << gAreaArchives[index].name;
            first = false;
        }
        overlay << L")  |  " << drawnAreaInstances << L" models  |  "
                << drawnAreaTriangles << L" triangles  |  "
                << lights.size() << L" lights";
    } else if (worldPositioned) {
        overlay << L"GAME.DTZ world preview  |  " << lights.size()
                << L" lights  |  building / treadable / dummy placements";
    } else {
        overlay << L"GAME.DTZ  |  " << lights.size()
                << L" lights  |  model preview";
    }
    drawViewportTextOpenGl(dc, width, height, 10, 10, overlay.str(), true);

    glFlush();
    SwapBuffers(dc);
    wglMakeCurrent(nullptr, nullptr);
}

static bool selectArchiveResourceFromViewportPoint(int x, int y) {
    if (gMode != StorylandMode::ArchiveFile || gArchiveViewportPicks.empty()) return false;

    const ArchiveViewportPick* best = nullptr;
    double bestScore = (std::numeric_limits<double>::max)();
    for (const ArchiveViewportPick& pick : gArchiveViewportPicks) {
        const int dx = x - pick.x;
        const int dy = y - pick.y;
        const double distanceSquared = double(dx) * double(dx) + double(dy) * double(dy);
        const double radiusSquared = double(pick.radius) * double(pick.radius);
        if (distanceSquared > radiusSquared) continue;
        const double score = distanceSquared + double(pick.depth) * 64.0;
        if (score < bestScore) {
            bestScore = score;
            best = &pick;
        }
    }
    if (!best) return false;

    for (size_t i = 0; i < gArchiveMeshResourceIds.size(); ++i) {
        if (gArchiveMeshResourceIds[i] != best->resourceId) continue;
        selectTreePayloadItem(StorylandTreeKind::ArchiveMeshResource, int(i));
        selectArchiveMeshResource(int(i));
        setStatus(L"Selected resource " + std::to_wstring(best->resourceId) + L" from the viewport; its placed instances are highlighted red. Right-click it in the resource tree to export or replace it.");
        return true;
    }
    return false;
}

static LRESULT CALLBACK previewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (gMode == StorylandMode::MediaFile) {
            const int width = rc.right - rc.left;
            const int height = rc.bottom - rc.top;
            HDC bufferDc = CreateCompatibleDC(dc);
            HBITMAP bufferBitmap = bufferDc && width > 0 && height > 0
                ? CreateCompatibleBitmap(dc, width, height) : nullptr;
            if (bufferBitmap) {
                HGDIOBJ previousBitmap = SelectObject(bufferDc, bufferBitmap);
                drawMediaPreview(bufferDc, rc);
                BitBlt(dc, rc.left, rc.top, width, height, bufferDc, rc.left, rc.top, SRCCOPY);
                SelectObject(bufferDc, previousBitmap);
                DeleteObject(bufferBitmap);
            } else {
                drawMediaPreview(dc, rc);
            }
            if (bufferDc) DeleteDC(bufferDc);
        }
        else if (gMode == StorylandMode::TextureArchive) drawTexturePreview(dc, rc);
        else if (gMode == StorylandMode::ModelFile) drawModelPreviewOpenGl(hwnd, dc, rc);
        else if (gMode == StorylandMode::WblFile) drawWblPreviewOpenGl(hwnd, dc, rc);
        else if (gMode == StorylandMode::ArchiveFile) drawArchivePreviewOpenGl(hwnd, dc, rc);
        else if (gMode == StorylandMode::AnimFile) drawAnimPreviewOpenGl(hwnd, dc, rc);
        else if (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::TextureArchive) drawTexturePreview(dc, rc);
        else if (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile) drawModelPreviewOpenGl(hwnd, dc, rc);
        else if (gMode == StorylandMode::DtzArchive) drawDtzLeeds2dfxPreviewOpenGl(hwnd, dc, rc);
        else {
            HBRUSH emptyPreviewBrush = CreateSolidBrush(RGB(181, 221, 242));
            FillRect(dc, &rc, emptyPreviewBrush);
            DeleteObject(emptyPreviewBrush);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_KEYDOWN && wParam == VK_SPACE && currentModeUsesInteractiveModelViewport()) {
        resetModelViewport();
        InvalidateRect(hwnd, nullptr, FALSE);
        setStatus(L"Viewport reset.");
        return 0;
    }
    if (msg == WM_KEYDOWN && handleModelViewportShortcut(wParam)) {
        return 0;
    }
    if (msg == WM_LBUTTONDOWN && currentModeUsesInteractiveModelViewport()) {
        gModelLeftDrag = true;
        gModelDragMoved = false;
        gModelLastMouse.x = GET_X_LPARAM(lParam);
        gModelLastMouse.y = GET_Y_LPARAM(lParam);
        gModelMouseDownPoint = gModelLastMouse;
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
        gModelDragMoved = false;
        gModelLastMouse.x = GET_X_LPARAM(lParam);
        gModelLastMouse.y = GET_Y_LPARAM(lParam);
        gModelMouseDownPoint = gModelLastMouse;
        SetFocus(hwnd);
        SetCapture(hwnd);
        return 0;
    }
    if ((msg == WM_LBUTTONUP || msg == WM_RBUTTONUP) && currentModeUsesInteractiveModelViewport()) {
        if (msg == WM_LBUTTONUP) {
            const bool wasViewCubeDrag = gModelViewCubeDrag;
            const bool wasClick = !gModelDragMoved && !wasViewCubeDrag;
            const int clickX = GET_X_LPARAM(lParam);
            const int clickY = GET_Y_LPARAM(lParam);
            gModelLeftDrag = false;
            gModelViewCubeDrag = false;
            if (wasClick && selectArchiveResourceFromViewportPoint(clickX, clickY)) {
                ReleaseCapture();
                return 0;
            }
        }
        if (msg == WM_RBUTTONUP) {
            const bool wasClick = !gModelDragMoved;
            const int clickX = GET_X_LPARAM(lParam);
            const int clickY = GET_Y_LPARAM(lParam);
            gModelRightDrag = false;
            if (!gModelLeftDrag) ReleaseCapture();
            if (wasClick) {
                showRendererContextMenu(hwnd, clickX, clickY);
                return 0;
            }
        }
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
            if (std::abs(x - gModelMouseDownPoint.x) > 3 || std::abs(y - gModelMouseDownPoint.y) > 3) gModelDragMoved = true;
            StorylandVec3 currentPoint = gModelViewCubeDrag ?
                viewCubePointFromMouse(hwnd, x, y) :
                arcballPointFromMouse(hwnd, x, y);
            StorylandQuat dragDelta = quatFromVectors(gModelDragStartPoint, currentPoint);
            gModelViewRotation = quatMul(dragDelta, gModelDragStartRotation);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (gModelRightDrag) {
            if (std::abs(x - gModelMouseDownPoint.x) > 3 || std::abs(y - gModelMouseDownPoint.y) > 3) gModelDragMoved = true;
            float panScale = std::max(0.0025f, gModelDistance * 0.0045f);
            if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) panScale *= 2.5f;
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
        fitModelViewportCloser();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void updateActionBar() {
    if (!gActionBar || !gActionPrimary || !gActionSecondary || !gActionTertiary || !gActionQuaternary) return;
    ShowWindow(gActionPrimary, SW_HIDE);
    ShowWindow(gActionSecondary, SW_HIDE);
    if (gMediaTimeline) ShowWindow(gMediaTimeline, SW_HIDE);
    ShowWindow(gActionTertiary, SW_HIDE);
    ShowWindow(gActionQuaternary, SW_HIDE);
    EnableWindow(gActionSecondary, TRUE);
    EnableWindow(gActionTertiary, TRUE);

    if (gMode == StorylandMode::ModelFile ||
        (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile)) {
        SetWindowTextW(gActionPrimary, (gMode == StorylandMode::ModelFile && gModelFile.isEmptyDraft()) ? L"Import MDL Data..." : L"Test Model");
        SetWindowTextW(gActionSecondary, L"Apply Animation...");
        ShowWindow(gActionBar, SW_SHOW);
        ShowWindow(gActionPrimary, SW_SHOW);
        if (!(gMode == StorylandMode::ModelFile && gModelFile.isEmptyDraft()) && currentModelCanUsePedCutsceneAnimation())
            ShowWindow(gActionSecondary, SW_SHOW);
        if (gMode == StorylandMode::ModelFile && gModelAnimLoaded) {
            SetWindowTextW(gActionTertiary, L"Previous Frame");
            SetWindowTextW(gActionQuaternary, L"Next Frame");
            ShowWindow(gActionTertiary, SW_SHOW);
            ShowWindow(gActionQuaternary, SW_SHOW);
        }
    } else if (gMode == StorylandMode::AnimFile) {
        SetWindowTextW(gActionPrimary, gAnimPlaying ? L"Pause" : L"Play");
        SetWindowTextW(gActionSecondary, L"Stop");
        SetWindowTextW(gActionTertiary, L"Previous Frame");
        SetWindowTextW(gActionQuaternary, L"Next Frame");
        ShowWindow(gActionBar, SW_SHOW);
        ShowWindow(gActionPrimary, SW_SHOW);
        ShowWindow(gActionSecondary, SW_SHOW);
        ShowWindow(gActionTertiary, SW_SHOW);
        ShowWindow(gActionQuaternary, SW_SHOW);
    } else if (gMode == StorylandMode::TextureArchive) {
        SetWindowTextW(gActionPrimary, L"Test Texture");
        SetWindowTextW(gActionSecondary, L"Add Material...");
        SetWindowTextW(gActionTertiary, L"Edit Material...");
        SetWindowTextW(gActionQuaternary, L"Remove Material");
        ShowWindow(gActionBar, SW_SHOW);
        ShowWindow(gActionPrimary, SW_SHOW);
        ShowWindow(gActionSecondary, SW_SHOW);
        ShowWindow(gActionTertiary, SW_SHOW);
        ShowWindow(gActionQuaternary, SW_SHOW);

        const BOOL haveMaterialSelection =
            (gSelectedIndex >= 0 &&
             size_t(gSelectedIndex) < gTextureArchive.textures().size()) ? TRUE : FALSE;
        EnableWindow(gActionPrimary, TRUE);
        EnableWindow(gActionSecondary, TRUE);
        EnableWindow(gActionTertiary, haveMaterialSelection);
        EnableWindow(gActionQuaternary, haveMaterialSelection);
    } else if (gMode == StorylandMode::DtzArchive &&
               gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::TextureArchive) {
        SetWindowTextW(gActionPrimary, L"Test Texture");
        SetWindowTextW(gActionSecondary, L"Back to GAME.DTZ");
        ShowWindow(gActionBar, SW_SHOW);
        ShowWindow(gActionPrimary, SW_SHOW);
        ShowWindow(gActionSecondary, SW_SHOW);
    } else if (gMode == StorylandMode::ArchiveFile && gArchiveBrowser.hasLvzContext()) {
        SetWindowTextW(gActionPrimary, L"Add Resource...");
        SetWindowTextW(gActionSecondary, L"Replace Resource...");
        SetWindowTextW(gActionTertiary, L"Test LVZ/IMG");
        SetWindowTextW(gActionQuaternary, L"Save LVZ + IMG");
        ShowWindow(gActionBar, SW_SHOW);
        ShowWindow(gActionPrimary, SW_SHOW);
        ShowWindow(gActionSecondary, SW_SHOW);
        ShowWindow(gActionTertiary, SW_SHOW);
        ShowWindow(gActionQuaternary, SW_SHOW);
        const bool haveEntry = gSelectedKind == StorylandTreeKind::ArchiveEntry &&
            gSelectedIndex >= 0 && size_t(gSelectedIndex) < gArchiveBrowser.entries().size();
        EnableWindow(gActionSecondary, haveEntry);
    } else if (gMode == StorylandMode::MediaFile) {
        SetWindowTextW(gActionPrimary, L"Play");
        SetWindowTextW(gActionSecondary, L"Stop");
        ShowWindow(gActionBar, SW_SHOW);
        ShowWindow(gActionPrimary, SW_SHOW);
        ShowWindow(gActionSecondary, SW_SHOW);
        if (gMediaFile.kind() == StorylandMediaKind::Video) {
            SetWindowTextW(gActionTertiary, L"Previous Frame");
            SetWindowTextW(gActionQuaternary, L"Next Frame");
            ShowWindow(gActionTertiary, SW_SHOW);
            ShowWindow(gActionQuaternary, SW_SHOW);
            updateMediaTimelineFromDecoder();
        }
    } else {
        ShowWindow(gActionBar, SW_HIDE);
    }
}

static void refreshModeUi() {
    rebuildFileMenu();
    rebuildViewMenu();
    updateActionBar();
    if (gMainWindow) layoutChildren(gMainWindow);

    // Presentation flushing is automatic. Any operation that refreshes the
    // active mode now finishes pending GL work and repaints the UI without
    // exposing a manual File > Flush command.
    flushStoryland(false);
}


struct StorylandUiRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

static int storylandRectWidth(const StorylandUiRect& rect) {
    return std::max(0, rect.right - rect.left);
}

static int storylandRectHeight(const StorylandUiRect& rect) {
    return std::max(0, rect.bottom - rect.top);
}

static StorylandUiRect storylandInsetRect(const StorylandUiRect& rect, int amount) {
    StorylandUiRect result = rect;
    result.left += amount;
    result.top += amount;
    result.right -= amount;
    result.bottom -= amount;
    if (result.right < result.left) result.right = result.left;
    if (result.bottom < result.top) result.bottom = result.top;
    return result;
}

static void storylandMoveWindowToRect(HWND hwnd, const StorylandUiRect& rect, BOOL repaint = FALSE) {
    if (!hwnd) return;
    MoveWindow(
        hwnd,
        rect.left,
        rect.top,
        storylandRectWidth(rect),
        storylandRectHeight(rect),
        repaint);
}

static void storylandInvalidatePaneFrame(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    RedrawWindow(
        hwnd,
        nullptr,
        nullptr,
        RDW_INVALIDATE | RDW_FRAME);
}

static void storylandInvalidateAllPaneFrames() {
    const HWND panes[] = {gTree, gPreview, gDetails, gActionBar};
    for (HWND pane : panes) storylandInvalidatePaneFrame(pane);
}

static void layoutChildren(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);

    constexpr int menuHeight = 28;
    constexpr int statusHeight = 22;
    constexpr int gutter = 6;
    constexpr int paneBorder = 1;
    constexpr int actionPadding = 7;

    const int actionHeight =
        (gActionBar && IsWindowVisible(gActionBar)) ? 42 : 0;
    const int clientWidth = std::max(0, int(rc.right - rc.left));
    const int clientHeight = std::max(0, int(rc.bottom - rc.top));

    SendMessageW(gStatus, WM_SIZE, 0, 0);
    storylandMoveWindowToRect(
        gStatus,
        {0, std::max(0, clientHeight - statusHeight), clientWidth, clientHeight});
    storylandMoveWindowToRect(
        gMenuStrip,
        {0, 0, clientWidth, menuHeight});

    const int contentTop = menuHeight + gutter;
    const int contentBottom =
        std::max(contentTop, clientHeight - statusHeight - gutter);
    const int contentHeight = std::max(0, contentBottom - contentTop);

    if (gAnalyzeGraphActive) {
        ShowWindow(gPreview, SW_HIDE);
        ShowWindow(gActionBar, SW_HIDE);

        const int graphAvailableW =
            std::max(0, clientWidth - gutter * 2);
        const int graphDesiredW =
            clientWidth >= 720
                ? std::clamp(
                    clientWidth * 72 / 100,
                    420,
                    std::max(420, clientWidth - 300))
                : graphAvailableW;
        const int graphTreeW =
            std::clamp(graphDesiredW, 0, graphAvailableW);
        const int graphRightX =
            std::min(clientWidth, graphTreeW + gutter * 2);
        const int graphRightW =
            std::max(0, clientWidth - graphRightX - gutter);

        const StorylandUiRect treeRect{
            gutter,
            contentTop,
            std::max(gutter, graphTreeW),
            contentBottom
        };
        const StorylandUiRect detailsRect{
            graphRightX,
            contentTop,
            graphRightX + graphRightW,
            contentBottom
        };

        storylandMoveWindowToRect(gTree, treeRect);
        storylandMoveWindowToRect(gDetails, detailsRect);

        storylandInvalidatePaneFrame(gTree);
        storylandInvalidatePaneFrame(gDetails);
        return;
    }

    if (gPreview) ShowWindow(gPreview, SW_SHOW);

    // One canonical set of pane rectangles drives both child placement and
    // border painting. No control is allowed to extend beyond its pane.
    const int leftW = gModelDffStructureTreeActive
        ? std::clamp(clientWidth * 34 / 100, 350, 540)
        : std::clamp(clientWidth * 28 / 100, 280, 420);

    const int treeRight = std::clamp(leftW, gutter, std::max(gutter, clientWidth - gutter));
    const int rightX = std::clamp(treeRight + gutter, gutter, std::max(gutter, clientWidth - gutter));
    const int rightW = std::max(0, clientWidth - rightX - gutter);

    const StorylandUiRect treeRect{
        gutter,
        contentTop,
        treeRight,
        contentBottom
    };
    storylandMoveWindowToRect(gTree, treeRect);

    int usableRightH =
        contentHeight - (actionHeight > 0 ? actionHeight + gutter : 0);
    usableRightH = std::max(0, usableRightH);

    const bool videoFrameViewer = gMode == StorylandMode::MediaFile &&
        gMediaFile.kind() == StorylandMediaKind::Video;
    const int detailsH = videoFrameViewer && usableRightH > 240
        ? std::clamp(usableRightH / 5, 100, 180)
        : usableRightH > 240
            ? std::clamp(usableRightH * 28 / 100, 140, 260)
            : usableRightH / 2;
    const int previewH =
        std::max(0, usableRightH - detailsH - gutter);

    const StorylandUiRect previewRect{
        rightX,
        contentTop,
        rightX + rightW,
        contentTop + previewH
    };
    storylandMoveWindowToRect(gPreview, previewRect);
    if (gMediaTimeline) {
        if (videoFrameViewer) {
            const int timelineHeight = 30;
            const StorylandUiRect timelineRect{
                previewRect.left + 8,
                std::max(previewRect.top + 4, previewRect.bottom - timelineHeight - 4),
                previewRect.right - 8,
                previewRect.bottom - 4
            };
            storylandMoveWindowToRect(gMediaTimeline, timelineRect);
            SetWindowPos(gMediaTimeline, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        } else {
            ShowWindow(gMediaTimeline, SW_HIDE);
        }
    }

    int cursorY = previewRect.bottom + gutter;

    if (gActionBar) {
        const StorylandUiRect actionRect{
            rightX,
            cursorY,
            rightX + rightW,
            cursorY + actionHeight
        };
        storylandMoveWindowToRect(gActionBar, actionRect);

        if (actionHeight > 0) {
            const HWND buttons[] = {
                gActionPrimary,
                gActionSecondary,
                gActionTertiary,
                gActionQuaternary
            };

            int visibleCount = 0;
            for (HWND button : buttons) {
                if (button && IsWindowVisible(button)) ++visibleCount;
            }

            if (visibleCount > 0) {
                StorylandUiRect actionInner =
                    storylandInsetRect(actionRect, paneBorder + actionPadding);

                const int buttonGap = 6;
                const int innerWidth = storylandRectWidth(actionInner);
                const int availableButtons =
                    std::max(0, innerWidth - buttonGap * (visibleCount - 1));
                const int buttonW = visibleCount > 0
                    ? std::max(1, std::min(142, availableButtons / visibleCount))
                    : 0;
                const int buttonH =
                    std::max(1, std::min(26, storylandRectHeight(actionInner)));
                int x = actionInner.left;
                const int y =
                    actionInner.top +
                    std::max(0, (storylandRectHeight(actionInner) - buttonH) / 2);

                for (HWND button : buttons) {
                    if (!button || !IsWindowVisible(button)) continue;

                    const int maxRight = actionInner.right;
                    const int remaining = std::max(0, maxRight - x);
                    const int actualWidth = std::min(buttonW, remaining);
                    MoveWindow(
                        button,
                        x,
                        y,
                        actualWidth,
                        buttonH,
                        FALSE);
                    x += actualWidth + buttonGap;
                    if (x > maxRight) x = maxRight;
                }
            }

            cursorY = actionRect.bottom + gutter;
        }
    }

    const int detailsBottom =
        std::max(cursorY, clientHeight - statusHeight - gutter);
    const StorylandUiRect detailsRect{
        rightX,
        cursorY,
        rightX + rightW,
        detailsBottom
    };
    storylandMoveWindowToRect(gDetails, detailsRect);

    // Child controls are repositioned without repainting one-by-one. Repaint the
    // settled layout once, which avoids button/tree fragments while resizing.
    if (hwnd && IsWindow(hwnd)) {
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}
static void selectPayloadForCurrentMode(const StorylandTreePayload& payload) {
    if (gAnalyzeGraphActive) {
        if (payload.kind == StorylandTreeKind::AnalyzeGraphBack) {
            exitAnalyzeGraph();
        } else if (payload.kind == StorylandTreeKind::AnalyzeGraphNode) {
            selectAnalyzeGraphNode(payload.index);
        }
        return;
    }

    if (gMode == StorylandMode::ScmFile &&
        (gSelectedKind == StorylandTreeKind::ScmSource || gSelectedKind == StorylandTreeKind::ScmMission) &&
        (payload.kind != gSelectedKind || payload.index != gSelectedIndex)) {
        commitScmSourceEditor(false);
    }

    if (gMode == StorylandMode::TextureArchive && payload.kind == StorylandTreeKind::Texture) selectTexture(payload.index);
    else if (gMode == StorylandMode::ModelFile && payload.kind == StorylandTreeKind::ModelDffGraphNode) selectModelDffGraphNode(payload.index);
    else if (gMode == StorylandMode::DtzArchive) selectDtzPayload(payload);
    else if (gMode == StorylandMode::ModelFile && payload.kind == StorylandTreeKind::ModelField) selectModelField(payload.index);
    else if (gMode == StorylandMode::ModelFile && payload.kind == StorylandTreeKind::ModelBone) selectModelBone(payload.index);
    else if (gMode == StorylandMode::ModelFile && payload.kind == StorylandTreeKind::ModelPrelight) selectModelPrelight(payload.index);
    else if (gMode == StorylandMode::ModelFile && (payload.kind == StorylandTreeKind::AnimOverview || payload.kind == StorylandTreeKind::AnimClip || payload.kind == StorylandTreeKind::AnimTrack || payload.kind == StorylandTreeKind::AnimField || payload.kind == StorylandTreeKind::AnimString)) selectAnimPayload(payload);
    else if (gMode == StorylandMode::ArchiveFile) selectArchivePayload(payload);
    else if (gMode == StorylandMode::WblFile) selectWblPayload(payload);
    else if (gMode == StorylandMode::AnimFile) selectAnimPayload(payload);
    else if (gMode == StorylandMode::ScmFile) selectScmPayload(payload);
    else if (gMode == StorylandMode::MediaFile) selectMediaPayload(payload);
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

static bool resourceNameSupportsAnalyzeGraph(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    const auto endsWith = [&](const char* suffix) {
        const size_t length = std::strlen(suffix);
        return lower.size() >= length && lower.compare(lower.size() - length, length, suffix) == 0;
    };
    return endsWith(".dff") || endsWith(".txd") || endsWith(".mdl") ||
           endsWith(".xtx") || endsWith(".chk") || endsWith(".anim");
}

static std::string analyzeExtensionLower(const std::wstring& name) {
    std::wstring extension = std::filesystem::path(name).extension().wstring();
    std::string result;
    result.reserve(extension.size());
    for (wchar_t ch : extension) {
        if (ch >= 0 && ch <= 127) result.push_back(char(std::tolower(static_cast<unsigned char>(ch))));
    }
    return result;
}

static bool collectCurrentAnalyzeResource(
    std::wstring& displayName,
    std::vector<uint8_t>& bytes,
    std::string& errorMessage) {
    displayName.clear();
    bytes.clear();

    if (gAnalyzeGraphActive && !gAnalysisGraphBytes.empty()) {
        displayName = gAnalysisGraphName;
        bytes = gAnalysisGraphBytes;
        return true;
    }

    if (gMode == StorylandMode::ModelFile) {
        displayName = gModelFile.sourcePath();
        if (displayName.empty()) displayName = L"model.mdl";
        if (!readBinaryFileForUi(gModelFile.sourcePath(), bytes, errorMessage)) {
            errorMessage = "Could not read the current model bytes for analysis: " + errorMessage;
            return false;
        }
        return true;
    }

    if (gMode == StorylandMode::TextureArchive) {
        displayName = gTextureArchive.sourcePath();
        if (displayName.empty()) displayName = L"texture.xtx";
        bytes = gTextureArchive.rawBytes();
        if (bytes.empty()) {
            errorMessage = "Current texture archive has no bytes to analyze.";
            return false;
        }
        return true;
    }

    if (gMode == StorylandMode::AnimFile) {
        displayName = gAnimFile.sourcePath();
        if (displayName.empty()) displayName = L"animation.anim";
        bytes = gAnimFile.rawBytes();
        if (bytes.empty()) {
            errorMessage = "Current ANIM has no bytes to analyze.";
            return false;
        }
        return true;
    }

    if (gMode == StorylandMode::ArchiveFile &&
        gSelectedKind == StorylandTreeKind::ArchiveEntry &&
        gSelectedIndex >= 0 &&
        size_t(gSelectedIndex) < gArchiveBrowser.entries().size()) {
        const auto& entry = gArchiveBrowser.entries()[size_t(gSelectedIndex)];
        displayName = widenResourceName(entry.name);
        return gArchiveBrowser.extractEntryBytes(size_t(gSelectedIndex), bytes, errorMessage);
    }

    if (gMode == StorylandMode::DtzArchive &&
        gSelectedKind == StorylandTreeKind::DtzDirEntry &&
        gSelectedIndex >= 0 &&
        size_t(gSelectedIndex) < gDtzArchive.dirEntries().size()) {
        const auto& entry = gDtzArchive.dirEntries()[size_t(gSelectedIndex)];
        displayName = widenResourceName(entry.name);
        return gDtzArchive.extractDirEntryBytes(size_t(gSelectedIndex), bytes, errorMessage);
    }

    errorMessage = "Select a resource before using Analyze.";
    return false;
}

static void setAnalyzeGraphNodeDetails(int index) {
    const auto& nodes = gAnalysisGraph.nodes();
    if (index < 0 || size_t(index) >= nodes.size()) return;
    const StorylandAnalysisGraphNode& node = nodes[size_t(index)];

    std::wostringstream ss;
    ss << L"STRUCTURE NODE\r\n\r\n"
       << L"Resource: " << gAnalysisGraphName << L"\r\n"
       << L"Node: " << widen(node.label) << L"\r\n"
       << L"Offset: 0x" << std::uppercase << std::hex << node.offset << std::dec << L"\r\n"
       << L"Size: " << node.size << L" bytes";
    if (node.size != 0u) {
        ss << L" (0x" << std::uppercase << std::hex << node.size << std::dec << L")";
    }
    ss << L"\r\n";

    if (node.type != 0u) {
        ss << L"Type/tag: 0x" << std::uppercase << std::hex << node.type << std::dec << L"\r\n";
    }
    if (node.version != 0u) {
        ss << L"Version/build: 0x" << std::uppercase << std::hex << node.version << std::dec << L"\r\n";
    }
    if (!node.description.empty()) {
        ss << L"Description: " << widen(node.description) << L"\r\n";
    }

    if (node.offset < gAnalysisGraphBytes.size() && node.size != 0u) {
        const uint64_t available =
            std::min<uint64_t>(node.size, gAnalysisGraphBytes.size() - size_t(node.offset));
        const size_t dump = size_t(std::min<uint64_t>(available, 512u));
        ss << L"\r\nHEX / ASCII (" << dump;
        if (available > dump) ss << L" of " << available;
        ss << L" bytes)\r\n";

        for (size_t row = 0u; row < dump; row += 16u) {
            ss << std::setw(8) << std::setfill(L'0') << std::hex
               << (node.offset + row) << L"  ";
            for (size_t column = 0u; column < 16u; ++column) {
                if (row + column < dump) {
                    ss << std::setw(2) << unsigned(gAnalysisGraphBytes[size_t(node.offset) + row + column]) << L" ";
                } else {
                    ss << L"   ";
                }
            }
            ss << L" ";
            for (size_t column = 0u; column < 16u && row + column < dump; ++column) {
                const unsigned char ch = gAnalysisGraphBytes[size_t(node.offset) + row + column];
                ss << wchar_t(ch >= 32u && ch < 127u ? ch : '.');
            }
            ss << L"\r\n";
        }
        ss << std::dec << std::setfill(L' ');
    }

    setDetails(ss.str());
    setStatus(L"Analyze Graph | " + widen(node.label));
}

static void selectAnalyzeGraphNode(int index) {
    gSelectedKind = StorylandTreeKind::AnalyzeGraphNode;
    gSelectedIndex = index;
    setAnalyzeGraphNodeDetails(index);
}

static void restoreTreeAfterAnalyzeGraph() {
    switch (gMode) {
    case StorylandMode::TextureArchive: populateTextureList(); break;
    case StorylandMode::ModelFile: populateModelList(); break;
    case StorylandMode::AnimFile: populateAnimList(); break;
    case StorylandMode::ArchiveFile: populateArchiveList(); break;
    case StorylandMode::DtzArchive: populateDtzList(); break;
    case StorylandMode::WblFile: populateWblList(); break;
    case StorylandMode::ScmFile: populateScmList(); break;
    case StorylandMode::MediaFile: populateMediaList(); break;
    default: break;
    }
}

static void exitAnalyzeGraph() {
    if (!gAnalyzeGraphActive) return;
    gAnalyzeGraphActive = false;
    gAnalysisGraphBytes.clear();
    gAnalysisGraphName.clear();

    ShowWindow(gPreview, SW_SHOW);
    restoreTreeAfterAnalyzeGraph();
    refreshModeUi();
    layoutChildren(gMainWindow);
    setStatus(L"Returned to resource view.");
}

static void populateAnalyzeGraphTree() {
    if (!gTree) return;

    replaceResourceTreeWithoutDeletingItems();
    gSelectedKind = StorylandTreeKind::None;
    gSelectedIndex = -1;
    setDetails(L"");

    HTREEITEM back = addTreeItem(
        TVI_ROOT,
        L"\x2190 Back to resource view",
        StorylandTreeKind::AnalyzeGraphBack,
        0);

    const auto& nodes = gAnalysisGraph.nodes();
    std::vector<HTREEITEM> treeItems(nodes.size(), nullptr);

    for (size_t i = 0u; i < nodes.size(); ++i) {
        const StorylandAnalysisGraphNode& node = nodes[i];
        HTREEITEM parent = TVI_ROOT;
        if (node.parent >= 0 && size_t(node.parent) < treeItems.size() && treeItems[size_t(node.parent)]) {
            parent = treeItems[size_t(node.parent)];
        }
        treeItems[i] = addTreeItem(
            parent,
            widen(node.label),
            StorylandTreeKind::AnalyzeGraphNode,
            int(i));
    }

    if (!nodes.empty() && treeItems[0]) {
        expandTreeItem(treeItems[0]);
        HTREEITEM child = TreeView_GetChild(gTree, treeItems[0]);
        while (child) {
            TreeView_Expand(gTree, child, TVE_EXPAND);
            child = TreeView_GetNextSibling(gTree, child);
        }
        TreeView_SelectItem(gTree, treeItems[0]);
        selectAnalyzeGraphNode(0);
    } else {
        TreeView_SelectItem(gTree, back);
    }

    layoutChildren(gMainWindow);
}

static void analyzeCurrentResourceGraph() {
    try {
        std::wstring displayName;
        std::vector<uint8_t> bytes;
        std::string error;
        if (!collectCurrentAnalyzeResource(displayName, bytes, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Analyze Graph", MB_OK | MB_ICONERROR);
            return;
        }

        const std::string extension = analyzeExtensionLower(displayName);
        const std::string narrowName = narrow(displayName);
        if (!resourceNameSupportsAnalyzeGraph(narrowName)) {
            MessageBoxW(
                gMainWindow,
                L"Analyze Graph currently supports only DFF, TXD, MDL, XTX, CHK and ANIM.",
                L"Analyze Graph",
                MB_OK | MB_ICONINFORMATION);
            return;
        }

        if (!gAnalysisGraph.build(bytes, extension, narrowName, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Analyze Graph failed", MB_OK | MB_ICONERROR);
            return;
        }

        gAnalysisGraphBytes = std::move(bytes);
        gAnalysisGraphName = displayName;
        gAnalyzeGraphActive = true;

        ShowWindow(gPreview, SW_HIDE);
        ShowWindow(gActionBar, SW_HIDE);
        populateAnalyzeGraphTree();

        SetWindowTextW(
            gMainWindow,
            (L"Storyland - Analyze Graph - " + std::filesystem::path(displayName).filename().wstring()).c_str());
        setStatus(L"Analyze Graph | " + widen(gAnalysisGraph.summary()));
    } catch (const std::exception& e) {
        MessageBoxW(gMainWindow, (L"Analyze Graph failed safely:\r\n\r\n" + widen(e.what())).c_str(),
                    L"Analyze Graph", MB_OK | MB_ICONERROR);
    } catch (...) {
        MessageBoxW(gMainWindow, L"Analyze Graph failed safely because the selected resource is malformed.",
                    L"Analyze Graph", MB_OK | MB_ICONERROR);
    }
}

static void analyzeCurrentResourceData() {
    try {
        std::wstring displayName;
        std::vector<uint8_t> bytes;
        std::string error;
        if (!collectCurrentAnalyzeResource(displayName, bytes, error)) {
            MessageBoxW(gMainWindow, widen(error).c_str(), L"Analyze Data", MB_OK | MB_ICONERROR);
            return;
        }

        uint64_t histogram[256] = {};
        size_t printable = 0u;
        size_t zeroBytes = 0u;
        size_t ffBytes = 0u;
        size_t longestZeroRun = 0u;
        size_t currentZeroRun = 0u;
        uint64_t fnv1a = 1469598103934665603ull;

        for (uint8_t byte : bytes) {
            ++histogram[byte];
            if (byte >= 32u && byte < 127u) ++printable;
            if (byte == 0u) {
                ++zeroBytes;
                ++currentZeroRun;
                longestZeroRun = std::max(longestZeroRun, currentZeroRun);
            } else {
                currentZeroRun = 0u;
            }
            if (byte == 0xFFu) ++ffBytes;
            fnv1a ^= byte;
            fnv1a *= 1099511628211ull;
        }

        double entropy = 0.0;
        if (!bytes.empty()) {
            for (uint64_t count : histogram) {
                if (count == 0u) continue;
                const double probability = double(count) / double(bytes.size());
                entropy -= probability * std::log2(probability);
            }
        }

        size_t alignedDwords = bytes.size() / 4u;
        size_t zeroDwords = 0u;
        size_t plausibleFileOffsets = 0u;
        for (size_t offset = 0u; offset + 4u <= bytes.size(); offset += 4u) {
            const uint32_t value =
                uint32_t(bytes[offset]) |
                (uint32_t(bytes[offset + 1u]) << 8u) |
                (uint32_t(bytes[offset + 2u]) << 16u) |
                (uint32_t(bytes[offset + 3u]) << 24u);
            if (value == 0u) ++zeroDwords;
            if (value >= 4u && value < bytes.size() && (value & 3u) == 0u) ++plausibleFileOffsets;
        }

        std::wostringstream ss;
        ss << L"BINARY DATA ANALYSIS\r\n\r\n"
           << L"Resource: " << displayName << L"\r\n"
           << L"Extension: " << widen(analyzeExtensionLower(displayName)) << L"\r\n"
           << L"Size: " << bytes.size() << L" bytes (0x"
           << std::uppercase << std::hex << bytes.size() << std::dec << L")\r\n"
           << L"FNV-1a 64: 0x" << std::uppercase << std::hex << fnv1a << std::dec << L"\r\n"
           << L"Shannon entropy: " << std::fixed << std::setprecision(4) << entropy << L" bits/byte\r\n"
           << L"Printable ASCII: " << printable << L" / " << bytes.size()
           << L" (" << (bytes.empty() ? 0.0 : (100.0 * double(printable) / double(bytes.size()))) << L"%)\r\n"
           << L"Zero bytes: " << zeroBytes
           << L" (" << (bytes.empty() ? 0.0 : (100.0 * double(zeroBytes) / double(bytes.size()))) << L"%)\r\n"
           << L"0xFF bytes: " << ffBytes << L"\r\n"
           << L"Longest zero run: " << longestZeroRun << L" bytes\r\n"
           << L"Aligned DWORDs: " << alignedDwords << L"\r\n"
           << L"Zero DWORDs: " << zeroDwords << L"\r\n"
           << L"Aligned in-file offset candidates: " << plausibleFileOffsets << L"\r\n\r\n";

        if (bytes.size() >= 4u) {
            const uint32_t magic =
                uint32_t(bytes[0]) |
                (uint32_t(bytes[1]) << 8u) |
                (uint32_t(bytes[2]) << 16u) |
                (uint32_t(bytes[3]) << 24u);
            ss << L"First DWORD: 0x" << std::uppercase << std::hex << magic << std::dec << L"\r\n";
            if (magic == 0x006D646Cu) ss << L"Signature: Leeds MDL\r\n";
            else if (magic == 0x616E696Du) ss << L"Signature: Leeds ANIM ('mina')\r\n";
            else if (magic == 0x10u) ss << L"Signature: RenderWare Clump / DFF\r\n";
            else if (magic == 0x16u) ss << L"Signature: RenderWare Texture Dictionary / TXD\r\n";
            else if (bytes.size() >= 3u && bytes[0] == 'x' && bytes[1] == 'e' && bytes[2] == 't')
                ss << L"Signature: Leeds XTX/XET texture archive\r\n";
        }

        ss << L"\r\nFIRST 256 BYTES\r\n";
        const size_t dump = std::min<size_t>(256u, bytes.size());
        for (size_t row = 0u; row < dump; row += 16u) {
            ss << std::setw(8) << std::setfill(L'0') << std::hex << row << L"  ";
            for (size_t column = 0u; column < 16u; ++column) {
                if (row + column < dump) ss << std::setw(2) << unsigned(bytes[row + column]) << L" ";
                else ss << L"   ";
            }
            ss << L" ";
            for (size_t column = 0u; column < 16u && row + column < dump; ++column) {
                const unsigned char ch = bytes[row + column];
                ss << wchar_t(ch >= 32u && ch < 127u ? ch : '.');
            }
            ss << L"\r\n";
        }
        ss << std::dec << std::setfill(L' ');

        setDetails(ss.str());
        setStatus(L"Analyze Data complete | " + std::filesystem::path(displayName).filename().wstring());
    } catch (const std::exception& e) {
        MessageBoxW(gMainWindow, (L"Analyze Data failed safely:\r\n\r\n" + widen(e.what())).c_str(),
                    L"Analyze Data", MB_OK | MB_ICONERROR);
    } catch (...) {
        MessageBoxW(gMainWindow, L"Analyze Data failed safely because the selected resource is malformed.",
                    L"Analyze Data", MB_OK | MB_ICONERROR);
    }
}

// Legacy command id maps to the byte-analysis path. Context menus use the new
// Analyze submenu with explicit Graph/Data choices.
static void analyzeCurrentResource() {
    analyzeCurrentResourceData();
}

static bool addAnalyzeSubmenu(HMENU menu, bool graphEnabled) {
    HMENU analyzeMenu = CreatePopupMenu();
    if (!analyzeMenu) return false;

    AppendMenuW(
        analyzeMenu,
        MF_STRING | (graphEnabled ? 0u : MF_GRAYED),
        ID_RESOURCE_ANALYZE_GRAPH,
        L"Analyze Graph");
    AppendMenuW(analyzeMenu, MF_STRING, ID_RESOURCE_ANALYZE_DATA, L"Analyze Data");

    if (!AppendMenuW(
            menu,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(analyzeMenu),
            L"Analyze")) {
        DestroyMenu(analyzeMenu);
        return false;
    }
    return true;
}


static bool buildTreeContextMenu(HMENU menu) {
    bool hasItems = false;

    if (gAnalyzeGraphActive) {
        hasItems = addContextMenuItem(menu, ID_RESOURCE_ANALYZE_BACK, L"Back to resource view") || hasItems;
        hasItems = addContextMenuItem(menu, ID_RESOURCE_ANALYZE_DATA, L"Analyze Data") || hasItems;
        return hasItems;
    }

    if (gMode == StorylandMode::DtzArchive) {
        hasItems = addContextMenuItem(menu, ID_DTZ_FIND, L"Find...") || hasItems;
        bool specificItems = false;

        if (gSelectedKind == StorylandTreeKind::DtzFindResult && gSelectedIndex >= 0 && size_t(gSelectedIndex) < gDtzFindResults.size()) {
            const StorylandDtzFindResult& result = gDtzFindResults[size_t(gSelectedIndex)];
            if (result.dirEntryIndex >= 0) {
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                specificItems = addContextMenuItem(menu, ID_FILE_OPEN_EMBEDDED, L"Open matched internal resource...") || specificItems;
            }
        } else if (gSelectedKind == StorylandTreeKind::DtzDirEntry) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            specificItems = addContextMenuItem(menu, ID_FILE_OPEN_EMBEDDED, L"Open selected internal IMG entry standalone...") || specificItems;
            specificItems = addContextMenuItem(menu, ID_FILE_EXPORT_SELECTED_RESOURCE, L"Export selected internal IMG entry...") || specificItems;
            specificItems = addContextMenuItem(menu, ID_DTZ_REPLACE_SELECTED_ENTRY, L"Replace selected internal IMG entry...") || specificItems;
            specificItems = addContextMenuItem(menu, ID_DTZ_RENAME_RESOURCE, L"Rename...  F2") || specificItems;
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            specificItems = addContextMenuItem(menu, ID_DTZ_PATCH_SELECTED, L"Patch selected sector count...") || specificItems;
        } else if (gSelectedKind == StorylandTreeKind::DtzSectorRecord) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            specificItems = addContextMenuItem(menu, ID_DTZ_PATCH_SELECTED, L"Patch selected sector count...") || specificItems;
        } else if (gSelectedKind == StorylandTreeKind::DtzDataField) {
            const auto& fields = gDtzArchive.dataFields();
            if (gSelectedIndex >= 0 && size_t(gSelectedIndex) < fields.size() && fields[size_t(gSelectedIndex)].editable) {
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                specificItems = addContextMenuItem(menu, ID_DTZ_PATCH_DATA_FIELD, L"Edit...") || specificItems;
            }
        }

        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        hasItems = addContextMenuItem(menu, ID_DTZ_REBUILD_AS, L"Rebuild GAME.DTZ As...") || hasItems;
        if (gSelectedKind == StorylandTreeKind::DtzDirEntry &&
            gSelectedIndex >= 0 &&
            size_t(gSelectedIndex) < gDtzArchive.dirEntries().size()) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            const std::string& resourceName = gDtzArchive.dirEntries()[size_t(gSelectedIndex)].name;
            hasItems = addAnalyzeSubmenu(menu, resourceNameSupportsAnalyzeGraph(resourceName)) || hasItems;
        }
        hasItems = hasItems || specificItems;
    } else if (gMode == StorylandMode::TextureArchive) {
        hasItems = addContextMenuItem(menu, ID_TEXTURE_ADD, L"Add material...") || hasItems;
        hasItems = addContextMenuItem(menu, ID_TEXTURE_VALIDATE, L"Validate texture archive") || hasItems;
        if (gSelectedIndex >= 0) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            hasItems = addContextMenuItem(menu, ID_FILE_REPLACE_TEXTURE, L"Edit material...") || hasItems;
            if (gTextureArchive.textures().size() > 1u) hasItems = addContextMenuItem(menu, ID_TEXTURE_SWAP, L"Swap texture data with...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_TEXTURE_DUPLICATE, L"Duplicate material...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_FILE_RENAME_TEXTURE, L"Rename texture...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_FILE_EXPORT_TEXTURE, L"Export texture PNG...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_TEXTURE_REMOVE, L"Remove material...") || hasItems;
        }
        if (hasItems) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        hasItems = addAnalyzeSubmenu(
            menu,
            resourceNameSupportsAnalyzeGraph(narrow(gTextureArchive.sourcePath()))) || hasItems;
    } else if (gMode == StorylandMode::ModelFile) {
        hasItems = addContextMenuItem(menu, ID_MODEL_IMPORT_DATA, gModelFile.isEmptyDraft() ? L"Import MDL data..." : L"Replace model data from MDL...") || hasItems;
        if (!gModelFile.isEmptyDraft() && currentModelCanUsePedCutsceneAnimation())
            hasItems = addContextMenuItem(menu, ID_ACTION_SECONDARY, L"Apply Animation...") || hasItems;
        if (!gModelFile.isEmptyDraft()) {
            if (hasItems) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            hasItems = addAnalyzeSubmenu(
                menu,
                resourceNameSupportsAnalyzeGraph(narrow(gModelFile.sourcePath()))) || hasItems;
        }
    } else if (gMode == StorylandMode::AnimFile) {
        hasItems = addAnalyzeSubmenu(menu, true) || hasItems;
    } else if (gMode == StorylandMode::ArchiveFile) {
        if (gSelectedKind == StorylandTreeKind::ArchiveEntry &&
            gSelectedIndex >= 0 &&
            size_t(gSelectedIndex) < gArchiveBrowser.entries().size()) {
            hasItems = addContextMenuItem(menu, ID_FILE_OPEN_EMBEDDED, L"Open selected embedded entry...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_FILE_EXPORT_SELECTED_RESOURCE, L"Export selected archive entry...") || hasItems;
            const std::string& resourceName = gArchiveBrowser.entries()[size_t(gSelectedIndex)].name;
            hasItems = addAnalyzeSubmenu(menu, resourceNameSupportsAnalyzeGraph(resourceName)) || hasItems;
            if (gArchiveBrowser.hasLvzContext()) {
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                hasItems = addContextMenuItem(menu, ID_ARCHIVE_REPLACE_SELECTED_RESOURCE, L"Replace selected LVZ+IMG resource...") || hasItems;
            }
        } else if (gSelectedKind == StorylandTreeKind::ArchiveMeshResource) {
            hasItems = addContextMenuItem(menu, ID_FILE_EXPORT_SELECTED_RESOURCE, L"Export selected mesh resource...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_REPLACE_SELECTED_RESOURCE, L"Replace selected mesh resource...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_REPLACE_MESH_WITH_RESOURCE_ID, L"Clone mesh resource from Resource ID...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_CHANGE_SELECTED_MESH_RESOURCE_ID, L"Change mesh Resource ID...") || hasItems;
        }
        if (gArchiveBrowser.hasLvzContext() && gArchiveBrowser.hasImgContext()) {
            if (hasItems) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_ADD_RESOURCE, L"Add Resource...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_TEST_LVZ_IMG_PAIR, L"Test LVZ/IMG Pair") || hasItems;
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_EXPORT_LVZ_IMG_PAIR, L"Rebuild LVZ + IMG As...") || hasItems;
            hasItems = addContextMenuItem(menu, ID_ARCHIVE_OVERWRITE_LVZ_IMG_PAIR, L"Overwrite Current LVZ + IMG...") || hasItems;
        }
    } else if (gMode == StorylandMode::MediaFile && gMediaFile.kind() == StorylandMediaKind::Video) {
        hasItems = addContextMenuItem(menu, ID_MEDIA_REPLACE_CURRENT_FRAME, L"Replace Current Frame Image...") || hasItems;
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

static void paintStorylandAeroGradient(HDC dc, const RECT& bounds, COLORREF upper, COLORREF lower) {
    const int height = std::max(1, int(bounds.bottom - bounds.top));
    for (int row = 0; row < height; ++row) {
        const int weight = row * 255 / height;
        const auto channel = [weight](BYTE top, BYTE bottom) {
            return (int(top) * (255 - weight) + int(bottom) * weight) / 255;
        };
        const COLORREF color = RGB(
            channel(GetRValue(upper), GetRValue(lower)),
            channel(GetGValue(upper), GetGValue(lower)),
            channel(GetBValue(upper), GetBValue(lower)));
        HPEN pen = CreatePen(PS_SOLID, 1, color);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        MoveToEx(dc, bounds.left, bounds.top + row, nullptr);
        LineTo(dc, bounds.right, bounds.top + row);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
}

static LRESULT CALLBACK storylandMenuStripProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT client{};
        GetClientRect(hwnd, &client);
        const COLORREF accent = storylandPaneBorderColor();
        const COLORREF barBase = gEyeFriendlyPaneBackground ? RGB(26, 29, 37) : RGB(248, 252, 255);
        const COLORREF barTop = storylandBlendUiColor(barBase, accent,
            gEyeFriendlyPaneBackground ? 16 : 22);
        const COLORREF barBottom = storylandBlendUiColor(barBase, accent,
            gEyeFriendlyPaneBackground ? 35 : 58);
        const COLORREF hoverTop = storylandBlendUiColor(barBase, accent,
            gEyeFriendlyPaneBackground ? 35 : 40);
        const COLORREF hoverBottom = storylandBlendUiColor(barBase, accent,
            gEyeFriendlyPaneBackground ? 53 : 76);
        const COLORREF text = gEyeFriendlyPaneBackground ? RGB(238, 239, 243) : RGB(27, 55, 88);
        paintStorylandAeroGradient(dc, client, barTop, barBottom);
        HFONT font = gUiFont ? gUiFont : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, text);
        static const wchar_t* labels[] = {L"File", L"View", L"Help"};
        for (int index = 0; index < 3; ++index) {
            RECT item = storylandTopMenuRect(index, client.bottom);
            if (index == gMenuStripHover) {
                paintStorylandAeroGradient(dc, item, hoverTop, hoverBottom);
            }
            DrawTextW(dc, labels[index], -1, &item,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        HPEN edge = CreatePen(PS_SOLID, 1, accent);
        HGDIOBJ oldPen = SelectObject(dc, edge);
        MoveToEx(dc, client.left, client.bottom - 1, nullptr);
        LineTo(dc, client.right, client.bottom - 1);
        SelectObject(dc, oldPen);
        DeleteObject(edge);
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

static void clearMenuItems(HMENU menu) {
    if (!menu) return;
    while (GetMenuItemCount(menu) > 0) RemoveMenu(menu, 0, MF_BYPOSITION);
}

static void destroyMenuHandle(HMENU& menu) {
    if (menu) DestroyMenu(menu);
    menu = nullptr;
}

static void flushStoryland(bool showStatus) {
    // Flush only transient presentation state. Do not discard or reload the
    // currently opened resource, because that would silently throw away edits.
    // The parsed model/archive remains authoritative; cached Win32/OpenGL output
    // is rebuilt from it below.
    if (gOpenGlContext && gPreview && IsWindow(gPreview)) {
        HDC dc = GetDC(gPreview);
        if (dc) {
            if (wglMakeCurrent(dc, gOpenGlContext)) {
                glFinish();
                wglMakeCurrent(nullptr, nullptr);
            }
            ReleaseDC(gPreview, dc);
        }
    }

    // Rebuild the currently visible decoded texture from the parsed archive.
    // This deliberately avoids touching on-disk files or archive edit state.
    if (gMode == StorylandMode::TextureArchive) {
        const int keepIndex = gSelectedIndex;
        deleteTextureBitmap();
        gCurrentImage = {};
        if (keepIndex >= 0 && size_t(keepIndex) < gTextureArchive.textures().size()) {
            selectTexture(keepIndex);
        }
    } else if (gMode == StorylandMode::ModelFile) {
        // Drop only the OpenGL copy. Keep the decoded atlas, companion archive,
        // selected material and unsaved model state intact. The next paint will
        // upload the already-decoded atlas again.
        if (gModelTextureId != 0 && gOpenGlContext && gPreview && IsWindow(gPreview)) {
            HDC dc = GetDC(gPreview);
            if (dc) {
                if (wglMakeCurrent(dc, gOpenGlContext)) {
                    glDeleteTextures(1, &gModelTextureId);
                    glFinish();
                    wglMakeCurrent(nullptr, nullptr);
                    gModelTextureId = 0;
                    gModelTextureUploadNeeded = gModelTextureLoaded && !gModelTextureImage.rgba.empty();
                }
                ReleaseDC(gPreview, dc);
            }
        }
    }

    if (gMainWindow) {
        RedrawWindow(
            gMainWindow,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
        UpdateWindow(gMainWindow);
    }
    if (showStatus) setStatus(L"Refreshed renderer/UI state.");
}

static void rebuildFileMenu() {
    if (!gFileMenu) return;
    clearMenuItems(gFileMenu);
    destroyMenuHandle(gRecentMenu);
    destroyMenuHandle(gScmMenu);

    destroyMenuHandle(gNewMenu);
    gNewMenu = CreatePopupMenu();

    // New > Model > platform > format > model type.
    HMENU newModelMenu = CreatePopupMenu();
    HMENU newModelPs2Menu = CreatePopupMenu();
    HMENU newModelPs2MdlMenu = CreatePopupMenu();
    HMENU newModelPspMenu = CreatePopupMenu();
    HMENU newModelPspMdlMenu = CreatePopupMenu();
    HMENU newModelPspDffMenu = CreatePopupMenu();

    AppendMenuW(newModelPs2MdlMenu, MF_STRING, ID_FILE_NEW_MODEL_PS2_SIMPLE, L"Simple Model");
    AppendMenuW(newModelPs2MdlMenu, MF_STRING, ID_FILE_NEW_MODEL_PS2_PED, L"Ped Model");
    AppendMenuW(newModelPs2MdlMenu, MF_STRING, ID_FILE_NEW_MODEL_PS2_CUTSCENE, L"Cutscene Model");
    AppendMenuW(newModelPs2MdlMenu, MF_STRING, ID_FILE_NEW_MODEL_PS2_VEHICLE, L"Vehicle Model");
    AppendMenuW(newModelPs2MdlMenu, MF_STRING, ID_FILE_NEW_MODEL_PS2_WORLD, L"World Model");
    AppendMenuW(
        newModelPs2Menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(newModelPs2MdlMenu),
        L"MDL");

    AppendMenuW(newModelPspMdlMenu, MF_STRING, ID_FILE_NEW_MODEL_PSP_SIMPLE, L"Simple Model");
    AppendMenuW(newModelPspMdlMenu, MF_STRING, ID_FILE_NEW_MODEL_PSP_PED, L"Ped Model");
    AppendMenuW(newModelPspMdlMenu, MF_STRING, ID_FILE_NEW_MODEL_PSP_CUTSCENE, L"Cutscene Model");
    AppendMenuW(newModelPspMdlMenu, MF_STRING, ID_FILE_NEW_MODEL_PSP_VEHICLE, L"Vehicle Model");
    AppendMenuW(newModelPspMdlMenu, MF_STRING, ID_FILE_NEW_MODEL_PSP_WORLD, L"World Model");
    AppendMenuW(
        newModelPspMenu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(newModelPspMdlMenu),
        L"MDL (Retail)");

    AppendMenuW(newModelPspDffMenu, MF_STRING, ID_FILE_NEW_MODEL_PSP_DFF_SIMPLE, L"Simple Model");
    AppendMenuW(newModelPspDffMenu, MF_STRING, ID_FILE_NEW_MODEL_PSP_DFF_PED, L"Ped Model");
    AppendMenuW(newModelPspDffMenu, MF_STRING, ID_FILE_NEW_MODEL_PSP_DFF_CUTSCENE, L"Cutscene Model");
    AppendMenuW(newModelPspDffMenu, MF_STRING, ID_FILE_NEW_MODEL_PSP_DFF_VEHICLE, L"Vehicle Model");
    AppendMenuW(newModelPspDffMenu, MF_STRING, ID_FILE_NEW_MODEL_PSP_DFF_WORLD, L"World Model");
    AppendMenuW(
        newModelPspMenu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(newModelPspDffMenu),
        L"DFF (LCS Beta)");

    AppendMenuW(
        newModelMenu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(newModelPs2Menu),
        L"PS2");
    AppendMenuW(
        newModelMenu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(newModelPspMenu),
        L"PSP");
    AppendMenuW(
        gNewMenu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(newModelMenu),
        L"Model");

    // New > Texture > platform > texture format.
    HMENU newTextureMenu = CreatePopupMenu();
    HMENU newTexturePs2Menu = CreatePopupMenu();
    HMENU newTexturePspMenu = CreatePopupMenu();

    AppendMenuW(newTexturePs2Menu, MF_STRING, ID_FILE_NEW_TEXTURE_PS2_XTX, L"XTX");
    AppendMenuW(newTexturePs2Menu, MF_STRING, ID_FILE_NEW_TEXTURE_PS2_CHK, L"CHK");

    AppendMenuW(newTexturePspMenu, MF_STRING, ID_FILE_NEW_TEXTURE_PSP_XTX, L"XTX");
    AppendMenuW(newTexturePspMenu, MF_STRING, ID_FILE_NEW_TEXTURE_PSP_CHK, L"CHK");
    AppendMenuW(newTexturePspMenu, MF_STRING, ID_FILE_NEW_TEXTURE_PSP_TXD, L"TXD (LCS Beta)");

    AppendMenuW(
        newTextureMenu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(newTexturePs2Menu),
        L"PS2");
    AppendMenuW(
        newTextureMenu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(newTexturePspMenu),
        L"PSP");
    AppendMenuW(
        gNewMenu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(newTextureMenu),
        L"Texture");

    AppendMenuW(gFileMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gNewMenu), L"New");
    AppendMenuW(gFileMenu, MF_STRING, ID_FILE_OPEN, L"Open...");
    gRecentMenu = CreatePopupMenu();
    rebuildRecentMenu();
    AppendMenuW(gFileMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gRecentMenu), L"Open recent");

    if (gMode == StorylandMode::ScmFile) {
        AppendMenuW(gFileMenu, MF_SEPARATOR, 0, nullptr);
        gScmMenu = CreatePopupMenu();
        AppendMenuW(gScmMenu, MF_STRING, ID_SCM_CONFIGURE_SANNY, L"Configure Sanny Builder...");
        AppendMenuW(gScmMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(gScmMenu, MF_STRING, ID_SCM_DECOMPILE_PS2, L"Decompile as VCS PS2");
        AppendMenuW(gScmMenu, MF_STRING, ID_SCM_DECOMPILE_PSP, L"Decompile as VCS PSP");
        AppendMenuW(gScmMenu, MF_STRING, ID_SCM_REFRESH_MISSIONS, L"Refresh mission list");
        AppendMenuW(gScmMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(gScmMenu, MF_STRING, ID_SCM_EXPORT_SOURCE, L"Export Source...");
        AppendMenuW(gScmMenu, MF_STRING, ID_SCM_COMPILE, L"Compile SCM...");
        AppendMenuW(gFileMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gScmMenu), L"VCS SCM missions");
    }

    if (gMode == StorylandMode::ModelFile || gMode == StorylandMode::TextureArchive) {
        AppendMenuW(gFileMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(gFileMenu, MF_STRING, ID_FILE_EXPORT_CURRENT, L"Export...");
        AppendMenuW(gFileMenu, MF_STRING, ID_FILE_EXPORT_AS, L"Export As...");
    }

    AppendMenuW(gFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(gFileMenu, gMode == StorylandMode::Empty ? (MF_STRING | MF_GRAYED) : MF_STRING, ID_FILE_RELOAD, L"Reload");
    AppendMenuW(gFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(gFileMenu, MF_STRING, ID_FILE_EXPORT_LOG, L"Export Log...");
    AppendMenuW(gFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(gFileMenu, MF_STRING, ID_FILE_EXIT, L"Exit");
    if (gMenuStrip) InvalidateRect(gMenuStrip, nullptr, TRUE);
}


static std::wstring scriptControlText(HWND control) {
    if (!control) return L"";
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) return L"";
    std::wstring text(size_t(length) + 1u, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(size_t(length));
    return text;
}

static void setScriptOutput(const std::wstring& text) {
    if (gScriptOutput) SetWindowTextW(gScriptOutput, text.c_str());
}

static std::wstring trimScriptLine(const std::wstring& line) {
    size_t begin = 0;
    while (begin < line.size() && iswspace(line[begin])) ++begin;
    size_t end = line.size();
    while (end > begin && iswspace(line[end - 1])) --end;
    return line.substr(begin, end - begin);
}

static bool scriptExtractCallArgument(const std::wstring& line, const std::wstring& prefix, std::wstring& argument) {
    if (line.rfind(prefix, 0) != 0 || line.size() < prefix.size() + 1u || line.back() != L')') return false;
    argument = trimScriptLine(line.substr(prefix.size(), line.size() - prefix.size() - 1u));
    return true;
}

static bool scriptParseFloat(const std::wstring& text, float& value) {
    wchar_t* end = nullptr;
    errno = 0;
    const float parsed = std::wcstof(text.c_str(), &end);
    if (errno != 0 || end == text.c_str()) return false;
    while (*end && iswspace(*end)) ++end;
    if (*end) return false;
    value = parsed;
    return std::isfinite(value);
}

static bool scriptParseTwoFloats(const std::wstring& text, float& a, float& b) {
    const size_t comma = text.find(L',');
    if (comma == std::wstring::npos) return false;
    return scriptParseFloat(trimScriptLine(text.substr(0, comma)), a) &&
           scriptParseFloat(trimScriptLine(text.substr(comma + 1)), b);
}

static bool scriptParseFourFloats(const std::wstring& text, float& a, float& b, float& c, float& d) {
    std::array<std::wstring, 4> parts;
    size_t start = 0;
    for (size_t i = 0; i < 3; ++i) {
        const size_t comma = text.find(L',', start);
        if (comma == std::wstring::npos) return false;
        parts[i] = trimScriptLine(text.substr(start, comma - start));
        start = comma + 1;
    }
    parts[3] = trimScriptLine(text.substr(start));
    return scriptParseFloat(parts[0], a) && scriptParseFloat(parts[1], b) &&
           scriptParseFloat(parts[2], c) && scriptParseFloat(parts[3], d);
}

static std::wstring scriptUnquote(std::wstring value) {
    value = trimScriptLine(value);
    if (value.size() >= 2u && ((value.front() == L'"' && value.back() == L'"') ||
                              (value.front() == L'\'' && value.back() == L'\''))) {
        return value.substr(1u, value.size() - 2u);
    }
    return value;
}

static std::wstring storylandScriptTempDirectory() {
    wchar_t tempPath[MAX_PATH] = {};
    DWORD length = GetTempPathW(MAX_PATH, tempPath);
    std::filesystem::path root = length > 0 ? std::filesystem::path(tempPath) : std::filesystem::temp_directory_path();
    root /= L"StorylandScripts";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return root.wstring();
}

static std::wstring findExecutableOnPath(const wchar_t* name) {
    wchar_t found[32768] = {};
    DWORD length = SearchPathW(nullptr, name, nullptr, DWORD(std::size(found)), found, nullptr);
    if (length > 0 && length < std::size(found)) return found;
    return {};
}

static bool runCapturedProcess(const std::wstring& executable,
                               const std::vector<std::wstring>& arguments,
                               const std::wstring& workingDirectory,
                               DWORD& exitCode,
                               std::wstring& output,
                               std::wstring& error) {
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
        error = L"Could not create the script output pipe. Win32 error " + std::to_wstring(GetLastError()) + L".";
        return false;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::wstring commandLine = quoteCommandArgument(executable);
    for (const std::wstring& argument : arguments) {
        commandLine += L" ";
        commandLine += quoteCommandArgument(argument);
    }
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION process{};
    BOOL created = CreateProcessW(
        executable.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr,
        workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
        &startup, &process);
    CloseHandle(writePipe);
    writePipe = nullptr;
    if (!created) {
        error = L"Could not start " + executable + L". Win32 error " + std::to_wstring(GetLastError()) + L".";
        CloseHandle(readPipe);
        return false;
    }

    std::string bytes;
    char buffer[4096];
    for (;;) {
        DWORD available = 0;
        if (!PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr)) break;
        if (available > 0) {
            DWORD read = 0;
            if (ReadFile(readPipe, buffer, std::min<DWORD>(DWORD(sizeof(buffer)), available), &read, nullptr) && read > 0)
                bytes.append(buffer, buffer + read);
        } else {
            if (WaitForSingleObject(process.hProcess, 15) == WAIT_OBJECT_0) break;
            Sleep(1);
        }
    }
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(readPipe, buffer, DWORD(sizeof(buffer)), &read, nullptr) || read == 0) break;
        bytes.append(buffer, buffer + read);
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    exitCode = 0xFFFFFFFFu;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(readPipe);

    if (!bytes.empty()) {
        int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), int(bytes.size()), nullptr, 0);
        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
        if (chars <= 0) {
            codePage = GetOEMCP();
            flags = 0;
            chars = MultiByteToWideChar(codePage, flags, bytes.data(), int(bytes.size()), nullptr, 0);
        }
        if (chars > 0) {
            output.resize(size_t(chars));
            MultiByteToWideChar(codePage, flags, bytes.data(), int(bytes.size()), output.data(), chars);
        }
    }
    return true;
}

static bool sourceIsStorylandCommandsOnly(const std::wstring& source) {
    std::wistringstream stream(source);
    std::wstring line;
    bool any = false;
    while (std::getline(stream, line)) {
        line = trimScriptLine(line);
        if (line.empty() || line.rfind(L"#", 0) == 0 || line.rfind(L"//", 0) == 0) continue;
        any = true;
        if (line.rfind(L"storyland.", 0) != 0) return false;
    }
    return any;
}

static void runStorylandViewportCommands(const std::wstring& source) {
    std::wistringstream stream(source);
    std::wstring line;
    std::wstringstream output;
    int lineNumber = 0;
    int executed = 0;

    while (std::getline(stream, line)) {
        ++lineNumber;
        line = trimScriptLine(line);
        if (line.empty() || line.rfind(L"#", 0) == 0 || line.rfind(L"//", 0) == 0) continue;
        std::wstring argument;
        bool handled = false;
        if (line == L"storyland.reset()") {
            resetModelViewport(); handled = true; output << L"> reset viewport\r\n";
        } else if (scriptExtractCallArgument(line, L"storyland.zoom(", argument)) {
            float scale = 0.0f;
            if (!scriptParseFloat(argument, scale) || scale <= 0.0f) { output << L"Line " << lineNumber << L": zoom() requires a positive number.\r\n"; continue; }
            applyModelViewportZoom(scale); handled = true; output << L"> zoom " << scale << L"\r\n";
        } else if (scriptExtractCallArgument(line, L"storyland.pan(", argument)) {
            float x = 0.0f, y = 0.0f;
            if (!scriptParseTwoFloats(argument, x, y)) { output << L"Line " << lineNumber << L": pan() requires x, y.\r\n"; continue; }
            gModelPanX += x; gModelPanY += y; handled = true; output << L"> pan " << x << L", " << y << L"\r\n";
        } else if (scriptExtractCallArgument(line, L"storyland.rotate(", argument)) {
            float degrees = 0.0f, x = 0.0f, y = 0.0f, z = 0.0f;
            if (!scriptParseFourFloats(argument, degrees, x, y, z)) { output << L"Line " << lineNumber << L": rotate() requires degrees, x, y, z.\r\n"; continue; }
            rotateModelViewport(degrees, x, y, z); handled = true; output << L"> rotate " << degrees << L" degrees\r\n";
        } else if (scriptExtractCallArgument(line, L"storyland.grid(", argument)) {
            const std::wstring value = trimScriptLine(argument);
            if (value == L"True" || value == L"true" || value == L"1") gOpenGlShowGrid = true;
            else if (value == L"False" || value == L"false" || value == L"0") gOpenGlShowGrid = false;
            else { output << L"Line " << lineNumber << L": grid() requires True or False.\r\n"; continue; }
            handled = true; output << L"> grid " << (gOpenGlShowGrid ? L"on" : L"off") << L"\r\n";
        } else if (scriptExtractCallArgument(line, L"storyland.open(", argument)) {
            const std::wstring path = scriptUnquote(argument);
            if (path.empty()) { output << L"Line " << lineNumber << L": open() requires a quoted path.\r\n"; continue; }
            openStorylandFile(path); handled = true; output << L"> opened " << path << L"\r\n";
        }
        if (!handled) { output << L"Line " << lineNumber << L": unsupported Storyland command: " << line << L"\r\n"; continue; }
        ++executed;
    }
    output << L"\r\nExecuted " << executed << L" command" << (executed == 1 ? L"." : L"s.") << L"\r\n";
    setScriptOutput(output.str());
    if (gPreview) InvalidateRect(gPreview, nullptr, FALSE);
}

static void runStorylandScriptEditor() {
    const std::wstring source = scriptControlText(gScriptEditor);
    if (trimScriptLine(source).empty()) { setScriptOutput(L"Nothing to run.\r\n"); return; }
    if (sourceIsStorylandCommandsOnly(source)) { runStorylandViewportCommands(source); return; }

    const bool cFamily = source.find(L"#include") != std::wstring::npos ||
                         source.find(L"int main(") != std::wstring::npos ||
                         source.find(L"int main (") != std::wstring::npos;
    const std::filesystem::path tempRoot(storylandScriptTempDirectory());
    const std::wstring stamp = std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64());
    std::error_code ec;

    if (!cFamily) {
        std::wstring python = findExecutableOnPath(L"py.exe");
        std::vector<std::wstring> args;
        if (!python.empty()) args.push_back(L"-3");
        if (python.empty()) python = findExecutableOnPath(L"python.exe");
        if (python.empty()) {
            setScriptOutput(L"Python was not found. Install Python or make py.exe/python.exe available on PATH.\r\n");
            return;
        }
        std::filesystem::path scriptPath = tempRoot / (L"script_" + stamp + L".py");
        {
            std::ofstream file(scriptPath, std::ios::binary | std::ios::trunc);
            const std::string utf8 = narrow(source);
            file.write(utf8.data(), std::streamsize(utf8.size()));
        }
        args.push_back(scriptPath.wstring());
        DWORD exitCode = 0;
        std::wstring processOutput, processError;
        if (!runCapturedProcess(python, args, tempRoot.wstring(), exitCode, processOutput, processError)) setScriptOutput(processError + L"\r\n");
        else {
            if (processOutput.empty()) processOutput = L"(no output)\r\n";
            processOutput += L"\r\nPython exit code: " + std::to_wstring(exitCode) + L"\r\n";
            setScriptOutput(processOutput);
        }
        std::filesystem::remove(scriptPath, ec);
        return;
    }

    std::wstring compiler = findExecutableOnPath(L"cl.exe");
    enum class CompilerKind { Msvc, Clang, Gnu } kind = CompilerKind::Msvc;
    if (compiler.empty()) { compiler = findExecutableOnPath(L"clang++.exe"); kind = CompilerKind::Clang; }
    if (compiler.empty()) { compiler = findExecutableOnPath(L"g++.exe"); kind = CompilerKind::Gnu; }
    if (compiler.empty()) {
        setScriptOutput(L"No C/C++ compiler was found. Start Storyland from a Visual Studio developer environment or put cl.exe, clang++.exe, or g++.exe on PATH.\r\n");
        return;
    }

    const std::filesystem::path sourcePath = tempRoot / (L"script_" + stamp + L".cpp");
    const std::filesystem::path exePath = tempRoot / (L"script_" + stamp + L".exe");
    {
        std::ofstream file(sourcePath, std::ios::binary | std::ios::trunc);
        const std::string utf8 = narrow(source);
        file.write(utf8.data(), std::streamsize(utf8.size()));
    }
    std::vector<std::wstring> compileArgs;
    if (kind == CompilerKind::Msvc) compileArgs = {L"/nologo", L"/EHsc", L"/std:c++17", sourcePath.wstring(), L"/Fe:" + exePath.wstring()};
    else compileArgs = {L"-std=c++17", sourcePath.wstring(), L"-o", exePath.wstring()};
    DWORD compileExit = 0;
    std::wstring compileOutput, processError;
    if (!runCapturedProcess(compiler, compileArgs, tempRoot.wstring(), compileExit, compileOutput, processError)) {
        setScriptOutput(processError + L"\r\n");
    } else if (compileExit != 0 || !std::filesystem::exists(exePath)) {
        if (compileOutput.empty()) compileOutput = L"Compilation failed without compiler output.\r\n";
        setScriptOutput(compileOutput + L"\r\nCompiler exit code: " + std::to_wstring(compileExit) + L"\r\n");
    } else {
        DWORD runExit = 0;
        std::wstring runOutput, runError;
        if (!runCapturedProcess(exePath.wstring(), {}, tempRoot.wstring(), runExit, runOutput, runError)) setScriptOutput(runError + L"\r\n");
        else {
            std::wstring combined;
            if (!compileOutput.empty()) combined += compileOutput + L"\r\n";
            combined += runOutput.empty() ? L"(no output)\r\n" : runOutput;
            combined += L"\r\nProgram exit code: " + std::to_wstring(runExit) + L"\r\n";
            setScriptOutput(combined);
        }
    }
    std::filesystem::remove(sourcePath, ec);
    std::filesystem::remove(exePath, ec);
}

static void loadStorylandScriptFile() {
    const std::wstring path = openFileDialog(L"Storyland script\0*.py;*.txt\0Python files\0*.py\0Text files\0*.txt\0All files\0*.*\0");
    if (path.empty()) return;
    std::wifstream file{std::filesystem::path(path)};
    if (!file) {
        setScriptOutput(L"Could not open script file:\r\n" + path);
        return;
    }
    std::wstringstream contents;
    contents << file.rdbuf();
    SetWindowTextW(gScriptEditor, contents.str().c_str());
    setScriptOutput(L"Loaded " + path + L"\r\n");
}

static void saveStorylandScriptFile() {
    const std::wstring path = saveFileDialog(L"Script files\0*.py;*.cpp;*.c;*.txt\0Python\0*.py\0C/C++\0*.c;*.cpp\0Text\0*.txt\0All files\0*.*\0", L"py");
    if (path.empty()) return;
    std::wofstream file{std::filesystem::path(path), std::ios::trunc};
    if (!file) {
        setScriptOutput(L"Could not save script file:\r\n" + path);
        return;
    }
    file << scriptControlText(gScriptEditor);
    file.flush();
    setScriptOutput(L"Saved " + path + L"\r\n");
}

static void layoutStorylandScriptWindow(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = std::max(1, static_cast<int>(client.right - client.left));
    const int height = std::max(1, static_cast<int>(client.bottom - client.top));
    const int toolbarHeight = 38;
    const int gap = 6;
    const int outputHeight = std::max(120, height / 4);
    const int editorHeight = std::max(80, height - toolbarHeight - outputHeight - gap * 3);

    if (gScriptToolbar) MoveWindow(gScriptToolbar, 0, 0, width, toolbarHeight, TRUE);
    if (gScriptEditor) MoveWindow(gScriptEditor, gap, toolbarHeight + gap, width - gap * 2, editorHeight, TRUE);
    if (gScriptOutput) MoveWindow(gScriptOutput, gap, toolbarHeight + gap * 2 + editorHeight, width - gap * 2, outputHeight, TRUE);
}

static LRESULT CALLBACK storylandScriptProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        gScriptToolbar = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 100, 38, hwnd, nullptr, gInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"New", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 8, 7, 58, 25, hwnd, reinterpret_cast<HMENU>(ID_SCRIPT_NEW), gInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Open", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 72, 7, 62, 25, hwnd, reinterpret_cast<HMENU>(ID_SCRIPT_OPEN), gInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 140, 7, 62, 25, hwnd, reinterpret_cast<HMENU>(ID_SCRIPT_SAVE), gInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Run Script", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 214, 7, 86, 25, hwnd, reinterpret_cast<HMENU>(ID_SCRIPT_RUN), gInstance, nullptr);
        CreateWindowExW(0, L"STATIC", L"Python / C / C++ / Storyland", WS_CHILD | WS_VISIBLE, 316, 10, 160, 22, hwnd, nullptr, gInstance, nullptr);

        const DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | ES_NOHIDESEL;
        gScriptEditor = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, editStyle, 0, 0, 100, 100,
            hwnd, reinterpret_cast<HMENU>(ID_SCRIPT_EDITOR), gInstance, nullptr);
        gScriptOutput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(ID_SCRIPT_OUTPUT), gInstance, nullptr);

        if (!gScriptFont) {
            gScriptFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                FIXED_PITCH | FF_MODERN, L"Consolas");
        }
        if (!gScriptBackgroundBrush) gScriptBackgroundBrush = CreateSolidBrush(RGB(31, 32, 36));
        if (!gScriptToolbarBrush) gScriptToolbarBrush = CreateSolidBrush(RGB(43, 44, 49));
        if (gScriptFont) {
            SendMessageW(gScriptEditor, WM_SETFONT, reinterpret_cast<WPARAM>(gScriptFont), TRUE);
            SendMessageW(gScriptOutput, WM_SETFONT, reinterpret_cast<WPARAM>(gScriptFont), TRUE);
        }
        SendMessageW(gScriptEditor, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
        SendMessageW(gScriptOutput, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
        SetWindowTextW(gScriptEditor,
            L"# Python example\r\n"
            L"import os\r\n"
            L"print(os.getcwd())\r\n");
        setScriptOutput(L"Output\r\nPython, C and C++ source can be run here. Storyland viewport commands are also supported.\r\n");
        layoutStorylandScriptWindow(hwnd);
        return 0;
    }
    case WM_SIZE:
        layoutStorylandScriptWindow(hwnd);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_SCRIPT_NEW:
            SetWindowTextW(gScriptEditor, L"");
            setScriptOutput(L"New script.\r\n");
            SetFocus(gScriptEditor);
            return 0;
        case ID_SCRIPT_OPEN:
            loadStorylandScriptFile();
            return 0;
        case ID_SCRIPT_SAVE:
            saveStorylandScriptFile();
            return 0;
        case ID_SCRIPT_RUN:
            runStorylandScriptEditor();
            return 0;
        }
        break;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        SetTextColor(dc, RGB(226, 228, 233));
        if (control == gScriptToolbar) {
            SetBkColor(dc, RGB(43, 44, 49));
            return reinterpret_cast<LRESULT>(gScriptToolbarBrush);
        }
        SetBkColor(dc, RGB(31, 32, 36));
        return reinterpret_cast<LRESULT>(gScriptBackgroundBrush);
    }
    case WM_KEYDOWN:
        if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            runStorylandScriptEditor();
            return 0;
        }
        break;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        gScriptWindow = nullptr;
        gScriptToolbar = nullptr;
        gScriptEditor = nullptr;
        gScriptOutput = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static void showStorylandScriptingConsole() {
    if (gScriptWindow && IsWindow(gScriptWindow)) {
        ShowWindow(gScriptWindow, SW_SHOWNORMAL);
        SetForegroundWindow(gScriptWindow);
        return;
    }

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = storylandScriptProc;
        windowClass.hInstance = gInstance;
        windowClass.hCursor = LoadCursor(nullptr, IDC_IBEAM);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        windowClass.lpszClassName = L"StorylandScriptingWindow";
        registered = RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    if (!registered) return;

    gScriptWindow = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        L"StorylandScriptingWindow",
        L"Storyland - Scripting",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 920, 720,
        gMainWindow, nullptr, gInstance, nullptr);
    if (!gScriptWindow) return;
    ShowWindow(gScriptWindow, SW_SHOWNORMAL);
    UpdateWindow(gScriptWindow);
}

static void rebuildViewMenu() {
    if (!gViewMenu) return;
    clearMenuItems(gViewMenu);
    destroyMenuHandle(gTexturePreviewMenu);
    destroyMenuHandle(gOpenGlMenu);
    destroyMenuHandle(gSkyMenu);
    destroyMenuHandle(gBackgroundMenu);

    const bool threeDimensionalMode =
        gMode == StorylandMode::ModelFile ||
        gMode == StorylandMode::WblFile ||
        gMode == StorylandMode::ArchiveFile ||
        gMode == StorylandMode::DtzArchive ||
        gMode == StorylandMode::AnimFile;

    if (threeDimensionalMode) {
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
        AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_PRELIGHT_COMBINED, L"Prelight: combined with timecycle");
        AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_PRELIGHT_RAW, L"Prelight: raw vertex colours");
        AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_PRELIGHT_OFF, L"Prelight: disabled");
        UINT checkedPrelight = gPrelightViewMode == StorylandPrelightViewMode::Raw ? ID_VIEW_PRELIGHT_RAW :
                               gPrelightViewMode == StorylandPrelightViewMode::Off ? ID_VIEW_PRELIGHT_OFF : ID_VIEW_PRELIGHT_COMBINED;
        CheckMenuRadioItem(gOpenGlMenu, ID_VIEW_PRELIGHT_COMBINED, ID_VIEW_PRELIGHT_OFF, checkedPrelight, MF_BYCOMMAND);
        AppendMenuW(gOpenGlMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_SHOW_GRID, L"Grid");
        AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_SHOW_BONES, L"Bones");
        AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_SHOW_BOUNDS, L"Bounds");
        AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_SHOW_2DFX_LIGHTS, L"2DFX lights");
        AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_SHOW_VIEWCUBE, L"View cube");
        if (gMode == StorylandMode::ModelFile ||
            (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile)) {
            AppendMenuW(gOpenGlMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(gOpenGlMenu, MF_STRING, ID_VIEW_FLIP_MODEL_TEXTURE_V, L"Flip V coordinate");
            CheckMenuItem(gOpenGlMenu, ID_VIEW_FLIP_MODEL_TEXTURE_V,
                          MF_BYCOMMAND | (gModelFlipTextureV ? MF_CHECKED : MF_UNCHECKED));
        }
        CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_GRID, MF_BYCOMMAND | (gOpenGlShowGrid ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_BONES, MF_BYCOMMAND | (gOpenGlShowBones ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_BOUNDS, MF_BYCOMMAND | (gOpenGlShowBounds ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_2DFX_LIGHTS, MF_BYCOMMAND | (gOpenGlShow2dfxLights ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_VIEWCUBE, MF_BYCOMMAND | (gOpenGlShowViewCube ? MF_CHECKED : MF_UNCHECKED));
        AppendMenuW(gViewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gOpenGlMenu), L"Viewport options");
    }

    if (gMode == StorylandMode::TextureArchive) {
        gTexturePreviewMenu = CreatePopupMenu();
        AppendMenuW(gTexturePreviewMenu, MF_STRING, ID_VIEW_FLIP_TEXTURE_PREVIEW_V, L"Flip vertically");
        CheckMenuItem(gTexturePreviewMenu, ID_VIEW_FLIP_TEXTURE_PREVIEW_V,
                      MF_BYCOMMAND | (gTexturePreviewFlipV ? MF_CHECKED : MF_UNCHECKED));
        AppendMenuW(gViewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gTexturePreviewMenu), L"Texture");
    }

    if (threeDimensionalMode) {
        gSkyMenu = CreatePopupMenu();
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SHOW_SKY, L"Enabled");
        CheckMenuItem(gSkyMenu, ID_VIEW_SHOW_SKY, MF_BYCOMMAND | (gStoriesSky.isEnabled() ? MF_CHECKED : MF_UNCHECKED));
        AppendMenuW(gSkyMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SKY_LCS, L"Liberty City Stories");
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SKY_VCS, L"Vice City Stories");
        CheckMenuRadioItem(gSkyMenu, ID_VIEW_SKY_LCS, ID_VIEW_SKY_VCS,
                           gStoriesSky.game() == StorylandSkyGame::Lcs ? ID_VIEW_SKY_LCS : ID_VIEW_SKY_VCS, MF_BYCOMMAND);
        AppendMenuW(gSkyMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SKY_MIDNIGHT, L"Midnight");
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SKY_DAWN, L"Dawn");
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SKY_NOON, L"Noon");
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SKY_SUNSET, L"Sunset");
        AppendMenuW(gSkyMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SKY_WEATHER_SUNNY, L"Sunny");
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SKY_WEATHER_CLOUDY, L"Cloudy");
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SKY_WEATHER_RAINY, L"Rainy");
        AppendMenuW(gSkyMenu, MF_STRING, ID_VIEW_SKY_WEATHER_FOGGY, L"Foggy");
        AppendMenuW(gViewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gSkyMenu), L"Sky");
    }

    gBackgroundMenu = CreatePopupMenu();
    AppendMenuW(gBackgroundMenu, MF_STRING, ID_VIEW_BACKGROUND_LIGHT, L"Light");
    AppendMenuW(gBackgroundMenu, MF_STRING, ID_VIEW_BACKGROUND_DARK, L"Dark");
    CheckMenuRadioItem(gBackgroundMenu, ID_VIEW_BACKGROUND_LIGHT, ID_VIEW_BACKGROUND_DARK,
                       gEyeFriendlyPaneBackground ? ID_VIEW_BACKGROUND_DARK : ID_VIEW_BACKGROUND_LIGHT, MF_BYCOMMAND);
    AppendMenuW(gViewMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gBackgroundMenu), L"Background");
    AppendMenuW(gViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(gViewMenu, MF_STRING, ID_VIEW_SCRIPTING_CONSOLE, L"Scripting...");
    if (gMenuStrip) InvalidateRect(gMenuStrip, nullptr, TRUE);
}

static void createMenuBar(HWND hwnd) {
    (void)hwnd;
    gMainMenu = CreateMenu();
    gFileMenu = CreatePopupMenu();
    gViewMenu = CreatePopupMenu();
    gHelpMenu = CreatePopupMenu();

    rebuildFileMenu();
    rebuildViewMenu();

    AppendMenuW(gHelpMenu, MF_STRING, ID_HELP_WIKI, L"Wiki");
    AppendMenuW(gHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(gHelpMenu, MF_STRING, ID_HELP_ABOUT, L"About Storyland...");
    AppendMenuW(gMainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gFileMenu), L"File");
    AppendMenuW(gMainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gViewMenu), L"View");
    AppendMenuW(gMainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(gHelpMenu), L"Help");
}


static constexpr int kStorylandPaneBorderThickness = 1;

static void drawStorylandPaneWindowBorder(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;

    HDC dc = GetWindowDC(hwnd);
    if (!dc) return;

    RECT windowRect{};
    GetWindowRect(hwnd, &windowRect);
    const int width =
        std::max(0, int(windowRect.right - windowRect.left));
    const int height =
        std::max(0, int(windowRect.bottom - windowRect.top));

    if (width > 0 && height > 0) {
        HBRUSH brush = CreateSolidBrush(storylandPaneBorderColor());
        if (brush) {
            RECT top{
                0,
                0,
                width,
                std::min(kStorylandPaneBorderThickness, height)
            };
            RECT bottom{
                0,
                std::max(0, height - kStorylandPaneBorderThickness),
                width,
                height
            };
            RECT left{
                0,
                0,
                std::min(kStorylandPaneBorderThickness, width),
                height
            };
            RECT right{
                std::max(0, width - kStorylandPaneBorderThickness),
                0,
                width,
                height
            };

            FillRect(dc, &top, brush);
            FillRect(dc, &bottom, brush);
            FillRect(dc, &left, brush);
            FillRect(dc, &right, brush);
            DeleteObject(brush);
        }
    }

    ReleaseDC(hwnd, dc);
}

static LRESULT CALLBACK storylandPaneBorderSubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR referenceData
) {
    (void)referenceData;

    if (message == WM_NCCALCSIZE) {
        // Reserve an actual non-client pixel for the accent frame. Previously
        // the frame was painted over the client area, so TreeView/Edit/OpenGL
        // repainting could erase it or appear to draw outside it.
        LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);

        RECT* clientRect = nullptr;
        if (wParam != 0) {
            NCCALCSIZE_PARAMS* params =
                reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
            if (params) clientRect = &params->rgrc[0];
        } else {
            clientRect = reinterpret_cast<RECT*>(lParam);
        }

        if (clientRect) {
            if (clientRect->right - clientRect->left >
                kStorylandPaneBorderThickness * 2) {
                clientRect->left += kStorylandPaneBorderThickness;
                clientRect->right -= kStorylandPaneBorderThickness;
            }
            if (clientRect->bottom - clientRect->top >
                kStorylandPaneBorderThickness * 2) {
                clientRect->top += kStorylandPaneBorderThickness;
                clientRect->bottom -= kStorylandPaneBorderThickness;
            }
        }
        return result;
    }

    if (message == WM_NCPAINT || message == WM_NCACTIVATE) {
        LRESULT result =
            DefSubclassProc(hwnd, message, wParam, lParam);
        drawStorylandPaneWindowBorder(hwnd);
        return result;
    }

    if (message == WM_WINDOWPOSCHANGED) {
        LRESULT result =
            DefSubclassProc(hwnd, message, wParam, lParam);
        drawStorylandPaneWindowBorder(hwnd);
        return result;
    }

    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(
            hwnd,
            storylandPaneBorderSubclassProc,
            subclassId);
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}
static LRESULT CALLBACK mainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    try {
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
            WS_CHILD, 0, 0, 100, 28, hwnd,
            reinterpret_cast<HMENU>(ID_MENU_STRIP), gInstance, nullptr);
        gTree = createStorylandResourceTree(hwnd);
        WNDCLASSW previewClass = {};
        previewClass.lpfnWndProc = previewProc;
        previewClass.hInstance = gInstance;
        previewClass.lpszClassName = L"StorylandPreviewClass";
        previewClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        previewClass.style = CS_OWNDC | CS_DBLCLKS;
        RegisterClassW(&previewClass);
        gPreview = CreateWindowExW(
            0,
            previewClass.lpszClassName,
            nullptr,
            WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, 100, 100,
            hwnd,
            reinterpret_cast<HMENU>(ID_PREVIEW),
            gInstance,
            nullptr);
        initializeOpenGlPreview(gPreview);
        gDetails = CreateWindowExW(0, L"EDIT", nullptr, WS_CHILD | WS_CLIPSIBLINGS | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_NOHIDESEL, 0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(ID_DETAILS), gInstance, nullptr);
        SendMessageW(gDetails, EM_SETLIMITTEXT, 16 * 1024 * 1024, 0);
        gActionBar = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, 100, 36, hwnd, reinterpret_cast<HMENU>(ID_ACTION_BAR), gInstance, nullptr);
        gActionPrimary = CreateWindowExW(0, L"BUTTON", L"Test Model", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 118, 28, hwnd, reinterpret_cast<HMENU>(ID_ACTION_PRIMARY), gInstance, nullptr);
        gActionSecondary = CreateWindowExW(0, L"BUTTON", L"Apply Animation...", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 118, 28, hwnd, reinterpret_cast<HMENU>(ID_ACTION_SECONDARY), gInstance, nullptr);
        gActionTertiary = CreateWindowExW(0, L"BUTTON", L"Remove Material", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 118, 28, hwnd, reinterpret_cast<HMENU>(ID_ACTION_TERTIARY), gInstance, nullptr);
        gActionQuaternary = CreateWindowExW(0, L"BUTTON", L"Validate Archive", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 118, 28, hwnd, reinterpret_cast<HMENU>(ID_ACTION_QUATERNARY), gInstance, nullptr);
        gMediaTimeline = CreateWindowExW(0, TRACKBAR_CLASSW, nullptr,
            WS_CHILD | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS | TBS_ENABLESELRANGE,
            0, 0, 100, 28, hwnd, reinterpret_cast<HMENU>(ID_MEDIA_TIMELINE), gInstance, nullptr);
        SendMessageW(gMediaTimeline, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1));
        // Do not install SetWindowSubclass hooks on the main panes.
        // In particular, subclassing the Win32 TreeView and changing its
        // non-client rectangle from inside WM_NCCALCSIZE can leave COMCTL32
        // with stale internal geometry while the tree is being deleted and
        // repopulated. That can surface as an access violation inside
        // COMCTL32.dll during otherwise-valid XTX loads/tests.
        //
        // Storyland already provides pane separation through the parent
        // background/gutters, so the extra non-client subclass is unnecessary.
        // Keeping the controls native also avoids resize paint corruption.

        gStatus = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(ID_STATUS), gInstance, nullptr);

        // ODFF-style compact Windows UI: one modern font, flat panes, real
        // gutters, and the OpenGL viewport as the dominant workspace.
        gUiFont = CreateFontW(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        if (gUiFont) {
            const HWND controls[] = {
                gTree, gDetails, gStatus,
                gActionPrimary, gActionSecondary, gActionTertiary, gActionQuaternary
            };
            for (HWND control : controls) {
                if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(gUiFont), TRUE);
            }
        }
        SendMessageW(gDetails, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
        applyStorylandPaneBackground(hwnd);
        updateActionBar();
        layoutChildren(hwnd);
        ShowWindow(gMenuStrip, SW_SHOW);
        ShowWindow(gTree, SW_SHOW);
        ShowWindow(gPreview, SW_SHOW);
        ShowWindow(gDetails, SW_SHOW);
        ShowWindow(gStatus, SW_SHOW);
        gStoriesSkyLastTick = GetTickCount();
        SetTimer(hwnd, 2, 33, nullptr);
        SetTimer(hwnd, 3, 10, nullptr);
        setStatus(L"Open models, textures, archives, GAME.DTZ, animations, audio, or video.");
        return 0;
    }
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED && gRenderPieWindow && IsWindow(gRenderPieWindow))
            ShowWindow(gRenderPieWindow, SW_HIDE);
        layoutChildren(hwnd);
        if (wParam != SIZE_MINIMIZED) {
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        }
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
        if (reinterpret_cast<HWND>(lParam) == gActionBar) {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkColor(dc, storylandPaneBackgroundColor());
            return reinterpret_cast<LRESULT>(gPaneBackgroundBrush);
        }
        if (reinterpret_cast<HWND>(lParam) == gDetails) {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, storylandPaneTextColor());
            SetBkColor(dc, storylandPaneBackgroundColor());
            return reinterpret_cast<LRESULT>(gPaneBackgroundBrush);
        }
        break;

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (draw &&
            (draw->CtlID == ID_ACTION_PRIMARY ||
             draw->CtlID == ID_ACTION_SECONDARY ||
             draw->CtlID == ID_ACTION_TERTIARY ||
             draw->CtlID == ID_ACTION_QUATERNARY)) {
            const bool disabled = (draw->itemState & ODS_DISABLED) != 0;
            const bool pressed = (draw->itemState & ODS_SELECTED) != 0;
            const COLORREF accent = storylandPaneBorderColor();
            const COLORREF base = gEyeFriendlyPaneBackground ? RGB(37, 40, 48) : RGB(250, 253, 255);
            const COLORREF top = storylandBlendUiColor(base, accent,
                pressed ? (gEyeFriendlyPaneBackground ? 35 : 40) : (gEyeFriendlyPaneBackground ? 12 : 10));
            const COLORREF bottom = storylandBlendUiColor(base, accent,
                pressed ? (gEyeFriendlyPaneBackground ? 48 : 69) : (gEyeFriendlyPaneBackground ? 27 : 34));
            const COLORREF border = storylandBlendUiColor(base, accent,
                gEyeFriendlyPaneBackground ? 67 : 79);
            COLORREF textColor = gEyeFriendlyPaneBackground
                ? (disabled ? RGB(119, 123, 132) : RGB(238, 239, 243))
                : (disabled ? RGB(130, 150, 170) : RGB(24, 57, 91));

            paintStorylandAeroGradient(draw->hDC, draw->rcItem, top, bottom);

            HPEN pen = CreatePen(PS_SOLID, 1, border);
            HGDIOBJ oldPen = SelectObject(draw->hDC, pen);
            HGDIOBJ oldBrush = SelectObject(draw->hDC, GetStockObject(HOLLOW_BRUSH));
            const int buttonRight =
                std::max(draw->rcItem.left, draw->rcItem.right - 1);
            const int buttonBottom =
                std::max(draw->rcItem.top, draw->rcItem.bottom - 1);
            RoundRect(
                draw->hDC,
                draw->rcItem.left,
                draw->rcItem.top,
                buttonRight,
                buttonBottom,
                8, 8);
            SelectObject(draw->hDC, oldBrush);
            SelectObject(draw->hDC, oldPen);
            DeleteObject(pen);

            wchar_t text[128] = {};
            GetWindowTextW(draw->hwndItem, text, int(sizeof(text) / sizeof(text[0])));
            RECT textRect = draw->rcItem;
            if (pressed) OffsetRect(&textRect, 1, 1);
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, textColor);
            HFONT font = reinterpret_cast<HFONT>(SendMessageW(draw->hwndItem, WM_GETFONT, 0, 0));
            HGDIOBJ oldFont = font ? SelectObject(draw->hDC, font) : nullptr;
            DrawTextW(
                draw->hDC,
                text,
                -1,
                &textRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (oldFont) SelectObject(draw->hDC, oldFont);

            if ((draw->itemState & ODS_FOCUS) != 0 && !disabled) {
                RECT focusRect = draw->rcItem;
                InflateRect(&focusRect, -3, -3);
                DrawFocusRect(draw->hDC, &focusRect);
            }
            return TRUE;
        }
        if (draw && draw->CtlID == ID_STATUS) {
            const COLORREF accent = storylandPaneBorderColor();
            const COLORREF base = gEyeFriendlyPaneBackground ? RGB(25, 26, 31) : RGB(239, 247, 254);
            paintStorylandAeroGradient(draw->hDC, draw->rcItem,
                storylandBlendUiColor(base, accent, 8),
                storylandBlendUiColor(base, accent, 28));
            RECT accentEdge = draw->rcItem;
            accentEdge.bottom = std::min(accentEdge.bottom, accentEdge.top + 2);
            HBRUSH accentBrush = CreateSolidBrush(accent);
            FillRect(draw->hDC, &accentEdge, accentBrush);
            DeleteObject(accentBrush);
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
        if (wParam == 3) {
            if (gMode == StorylandMode::MediaFile && gMediaFile.kind() == StorylandMediaKind::Video && gMediaFile.isPlaying()) {
                std::string mediaError;
                if (!gMediaFile.tickVideo(mediaError)) {
                    gMediaFile.stop();
                    if (!mediaError.empty()) setStatus(L"Video stopped: " + widen(mediaError));
                    updateActionBar();
                }
                updateMediaVideoDetails();
                updateMediaTimelineFromDecoder();
                InvalidateRect(gPreview, nullptr, FALSE);
            }
            return 0;
        }
        if (wParam == 2) {
            DWORD tick = GetTickCount();
            float elapsedSeconds = float(tick - gStoriesSkyLastTick) / 1000.0f;
            gStoriesSkyLastTick = tick;
            gStoriesSky.update(std::min(elapsedSeconds, 0.25f));
            if ((gOpenGlRenderMode == StorylandOpenGlRenderMode::Stories && gStoriesSky.isEnabled()) ||
                (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::None && gOpenGlShow2dfxLights)) {
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
        if (reinterpret_cast<HWND>(wParam) == gPreview &&
            gMode == StorylandMode::MediaFile && gMediaFile.kind() == StorylandMediaKind::Video) {
            POINT pt{};
            if (lParam == -1) GetCursorPos(&pt);
            else { pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam); }
            HMENU menu = CreatePopupMenu();
            if (menu) {
                AppendMenuW(menu, MF_STRING, ID_MEDIA_REPLACE_CURRENT_FRAME, L"Replace Current Frame Image...");
                TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
            }
            return 0;
        }
        break;

    case WM_DWMCOLORIZATIONCOLORCHANGED:
    case WM_THEMECHANGED:
    case WM_SETTINGCHANGE:
        applyStorylandTitleTint(gTitleTint);
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            if (gRenderPieWindow && IsWindow(gRenderPieWindow)) ShowWindow(gRenderPieWindow, SW_HIDE);
        } else {
            applyStorylandTitleTint(gTitleTint);
        }
        break;

    case WM_SYSCHAR:
        switch (towlower(wchar_t(wParam))) {
        case L'f': openStorylandTopMenu(gMenuStrip, 0); return 0;
        case L'v': openStorylandTopMenu(gMenuStrip, 1); return 0;
        case L'h': openStorylandTopMenu(gMenuStrip, 2); return 0;
        default: break;
        }
        break;

    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == gMediaTimeline && !gMediaTimelineInternalUpdate &&
            gMode == StorylandMode::MediaFile && gMediaFile.kind() == StorylandMediaKind::Video) {
            const int code = LOWORD(wParam);
            if (code == TB_THUMBTRACK || code == TB_THUMBPOSITION || code == TB_ENDTRACK ||
                code == TB_LINEDOWN || code == TB_LINEUP || code == TB_PAGEDOWN || code == TB_PAGEUP) {
                const int position = int(SendMessageW(gMediaTimeline, TBM_GETPOS, 0, 0));
                const int maximum = std::max(1, int(SendMessageW(gMediaTimeline, TBM_GETRANGEMAX, 0, 0)));
                const uint64_t totalFrames = gMediaFile.videoFrameCountEstimate();
                uint64_t targetFrame = 0u;
                if (totalFrames > 1u) targetFrame = (uint64_t(std::max(0, position)) * (totalFrames - 1u)) / uint64_t(maximum);
                std::string mediaError;
                if (!gMediaFile.seekVideoFrame(targetFrame, mediaError)) {
                    if (!mediaError.empty()) setStatus(L"Video scrub failed: " + widen(mediaError));
                } else {
                    updateMediaVideoDetails();
                    InvalidateRect(gPreview, nullptr, FALSE);
                    setStatus(L"Video frame " + std::to_wstring(gMediaFile.videoFrameIndex()) + L" | " +
                              std::to_wstring(gMediaFile.videoPositionSeconds()) + L" s");
                }
            }
            return 0;
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
        case ID_FILE_NEW:
        case ID_FILE_NEW_MODEL:
            beginNewModelResource(
                StorylandNewPlatform::Ps2,
                StorylandNewModelContainer::Mdl,
                StorylandModelKind::SimpleModel);
            break;

        case ID_FILE_NEW_MODEL_PS2_SIMPLE:
            beginNewModelResource(StorylandNewPlatform::Ps2, StorylandNewModelContainer::Mdl, StorylandModelKind::SimpleModel);
            break;
        case ID_FILE_NEW_MODEL_PS2_PED:
            beginNewModelResource(StorylandNewPlatform::Ps2, StorylandNewModelContainer::Mdl, StorylandModelKind::PedModel);
            break;
        case ID_FILE_NEW_MODEL_PS2_CUTSCENE:
            beginNewModelResource(StorylandNewPlatform::Ps2, StorylandNewModelContainer::Mdl, StorylandModelKind::CutsceneModel);
            break;
        case ID_FILE_NEW_MODEL_PS2_VEHICLE:
            beginNewModelResource(StorylandNewPlatform::Ps2, StorylandNewModelContainer::Mdl, StorylandModelKind::VehicleModel);
            break;
        case ID_FILE_NEW_MODEL_PS2_WORLD:
            beginNewModelResource(StorylandNewPlatform::Ps2, StorylandNewModelContainer::Mdl, StorylandModelKind::WorldModel);
            break;

        case ID_FILE_NEW_MODEL_PSP_SIMPLE:
            beginNewModelResource(StorylandNewPlatform::Psp, StorylandNewModelContainer::Mdl, StorylandModelKind::SimpleModel);
            break;
        case ID_FILE_NEW_MODEL_PSP_PED:
            beginNewModelResource(StorylandNewPlatform::Psp, StorylandNewModelContainer::Mdl, StorylandModelKind::PedModel);
            break;
        case ID_FILE_NEW_MODEL_PSP_CUTSCENE:
            beginNewModelResource(StorylandNewPlatform::Psp, StorylandNewModelContainer::Mdl, StorylandModelKind::CutsceneModel);
            break;
        case ID_FILE_NEW_MODEL_PSP_VEHICLE:
            beginNewModelResource(StorylandNewPlatform::Psp, StorylandNewModelContainer::Mdl, StorylandModelKind::VehicleModel);
            break;
        case ID_FILE_NEW_MODEL_PSP_WORLD:
            beginNewModelResource(StorylandNewPlatform::Psp, StorylandNewModelContainer::Mdl, StorylandModelKind::WorldModel);
            break;

        case ID_FILE_NEW_MODEL_PSP_DFF_SIMPLE:
            beginNewModelResource(StorylandNewPlatform::Psp, StorylandNewModelContainer::Dff, StorylandModelKind::SimpleModel);
            break;
        case ID_FILE_NEW_MODEL_PSP_DFF_PED:
            beginNewModelResource(StorylandNewPlatform::Psp, StorylandNewModelContainer::Dff, StorylandModelKind::PedModel);
            break;
        case ID_FILE_NEW_MODEL_PSP_DFF_CUTSCENE:
            beginNewModelResource(StorylandNewPlatform::Psp, StorylandNewModelContainer::Dff, StorylandModelKind::CutsceneModel);
            break;
        case ID_FILE_NEW_MODEL_PSP_DFF_VEHICLE:
            beginNewModelResource(StorylandNewPlatform::Psp, StorylandNewModelContainer::Dff, StorylandModelKind::VehicleModel);
            break;
        case ID_FILE_NEW_MODEL_PSP_DFF_WORLD:
            beginNewModelResource(StorylandNewPlatform::Psp, StorylandNewModelContainer::Dff, StorylandModelKind::WorldModel);
            break;

        // Legacy direct IDs map to the matching platform-specific choice.
        case ID_FILE_NEW_TEXTURE:
        case ID_FILE_NEW_TEXTURE_XTX:
        case ID_FILE_NEW_TEXTURE_PS2_XTX:
        case ID_FILE_NEW_TEXTURE_PSP_XTX:
            beginNewTextureResource(StorylandNewTextureContainer::Xtx);
            break;
        case ID_FILE_NEW_TEXTURE_CHK:
        case ID_FILE_NEW_TEXTURE_PS2_CHK:
        case ID_FILE_NEW_TEXTURE_PSP_CHK:
            beginNewTextureResource(StorylandNewTextureContainer::Chk);
            break;
        case ID_FILE_NEW_TEXTURE_TXD:
        case ID_FILE_NEW_TEXTURE_PSP_TXD:
            beginNewTextureResource(StorylandNewTextureContainer::Txd);
            break;
        case ID_FILE_RELOAD: {
            std::wstring reloadPath;
            switch (gMode) {
                case StorylandMode::TextureArchive: reloadPath = gTextureArchive.sourcePath(); break;
                case StorylandMode::DtzArchive: reloadPath = gDtzArchive.sourcePath(); break;
                case StorylandMode::ModelFile: reloadPath = gModelFile.sourcePath(); break;
                case StorylandMode::WblFile: reloadPath = gWblFile.sourcePath(); break;
                case StorylandMode::AnimFile: reloadPath = gAnimFile.sourcePath(); break;
                case StorylandMode::ScmFile: reloadPath = gScmFile.sourcePath(); break;
                case StorylandMode::MediaFile: reloadPath = gMediaFile.sourcePath(); break;
                case StorylandMode::ArchiveFile:
                    reloadPath = !gArchiveBrowser.lvzPath().empty() ? gArchiveBrowser.lvzPath() : gArchiveBrowser.imgPath();
                    break;
                case StorylandMode::Empty: break;
            }
            if (reloadPath.empty()) {
                MessageBoxW(gMainWindow, L"The current resource has no file on disk to reload.", L"Storyland Reload", MB_ICONINFORMATION);
                break;
            }
            if (GetFileAttributesW(reloadPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                std::wstring message = L"The current resource no longer exists on disk:\r\n\r\n" + reloadPath;
                MessageBoxW(gMainWindow, message.c_str(), L"Storyland Reload", MB_ICONERROR);
                break;
            }
            openStorylandFile(reloadPath);
            flushStoryland(false);
            setStatus(L"Reloaded from disk: " + reloadPath);
            break;
        }

        case ID_FILE_OPEN: {
            std::wstring path = openFileDialog(L"Storyland files\0*.scm;*.dtz;*.bin;*.img;*.dir;*.lvz;*.zmg;*.area;*.wbl;*.mdl;*.dff;*.anim;*.chk;*.xtx;*.tex;*.txd;*.sdt;*.raw;*.vag;*.vb;*.wav;*.at3;*.aa3;*.oma;*.mp3;*.ogg;*.flac;*.aac;*.m4a;*.wma;*.ac3;*.aif;*.aiff;*.adx;*.pss;*.pmf;*.mpg;*.mpeg;*.mp4;*.m4v;*.wmv;*.avi;*.mov;*.mkv;*.ts;*.m2ts;*.mts;*.vob;*.3gp;*.3g2;*.webm;*.ogv;*.flv\0Models\0*.mdl;*.dff;*.wbl\0Textures\0*.chk;*.xtx;*.tex;*.txd\0Audio\0*.sdt;*.raw;*.vag;*.vb;*.wav;*.at3;*.aa3;*.oma;*.mp3;*.ogg;*.flac;*.aac;*.m4a;*.wma;*.ac3;*.aif;*.aiff;*.adx\0Video\0*.pss;*.pmf;*.mpg;*.mpeg;*.mp4;*.m4v;*.wmv;*.avi;*.mov;*.mkv;*.ts;*.m2ts;*.mts;*.vob;*.3gp;*.3g2;*.webm;*.ogv;*.flv\0All files\0*.*\0");
            openStorylandFile(path);
            break;
        }
        case ID_FILE_OPEN_EMBEDDED:
            if (gMode == StorylandMode::ArchiveFile && gSelectedKind == StorylandTreeKind::ArchiveEntry && gSelectedIndex >= 0) openSelectedArchiveEntry();
            else if (gMode == StorylandMode::DtzArchive && gSelectedKind == StorylandTreeKind::DtzDirEntry && gSelectedIndex >= 0) openSelectedDtzDirEntryStandalone();
            else if (gMode == StorylandMode::DtzArchive && gSelectedKind == StorylandTreeKind::DtzFindResult && gSelectedIndex >= 0) openDtzFindResult(gSelectedIndex);
            else MessageBoxW(gMainWindow, L"Select an embedded archive entry, GAME.DTZ internal IMG entry, or Find result first.", L"Storyland", MB_ICONINFORMATION);
            break;
        case ID_FILE_RECENT_CLEAR:
            gRecentFiles.clear();
            saveRecentFiles();
            rebuildRecentMenu();
            break;
        case ID_FILE_EXIT:
            SendMessageW(hwnd, WM_CLOSE, 0, 0);
            break;
        case ID_DTZ_REBUILD_AS: rebuildCurrentDtzAs(); break;
        case ID_DTZ_FIND: performDtzFind(); break;
        case ID_SCM_CONFIGURE_SANNY: configureSannyBuilder(); break;
        case ID_SCM_DECOMPILE_PS2: decompileCurrentScm("vcs_ps2", true); break;
        case ID_SCM_DECOMPILE_PSP: decompileCurrentScm("vcs_psp", true); break;
        case ID_SCM_REFRESH_MISSIONS: refreshScmMissionTree(); break;
        case ID_SCM_EXPORT_SOURCE: exportCurrentScmSource(); break;
        case ID_SCM_COMPILE: compileCurrentScm(); break;
        case ID_FILE_EXPORT_LOG: exportCurrentLog(); break;
        case ID_FILE_EXPORT_CURRENT: exportCurrentOpenedFile(false); break;
        case ID_FILE_EXPORT_AS: exportCurrentOpenedFile(true); break;
        case ID_FILE_EXPORT_SELECTED_RESOURCE: exportSelectedResourceBytes(); break;
        case ID_ACTION_PRIMARY:
            if (gMode == StorylandMode::AnimFile) {
                toggleAnimPlayback();
                updateActionBar();
            } else if (gMode == StorylandMode::ModelFile && gModelFile.isEmptyDraft()) {
                importModelDataIntoCurrentDraft();
            } else if (gMode == StorylandMode::ModelFile ||
                (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile)) {
                runCurrentModelTest();
            } else if (gMode == StorylandMode::TextureArchive ||
                       (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::TextureArchive)) {
                runCurrentTextureTest();
            } else if (gMode == StorylandMode::ArchiveFile && gArchiveBrowser.hasLvzContext()) {
                addResourceToCurrentArchive();
            } else if (gMode == StorylandMode::MediaFile) {
                std::string mediaError;
                if (!gMediaFile.play(mediaError)) MessageBoxW(gMainWindow, widen(mediaError).c_str(), L"Storyland media playback", MB_ICONERROR);
                updateActionBar();
                InvalidateRect(gPreview, nullptr, FALSE);
            }
            break;
        case ID_ACTION_SECONDARY:
            if (gMode == StorylandMode::AnimFile) {
                stopAnimPlayback();
                updateActionBar();
            } else if (gMode == StorylandMode::ModelFile ||
                (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::ModelFile)) {
                applyAnimationToCurrentModel();
            } else if (gMode == StorylandMode::TextureArchive) {
                addTextureMaterial();
            } else if (gMode == StorylandMode::DtzArchive && gDtzEmbeddedPreviewKind == DtzEmbeddedPreviewKind::TextureArchive) {
                returnToGameDtz();
            } else if (gMode == StorylandMode::ArchiveFile && gArchiveBrowser.hasLvzContext()) {
                replaceSelectedArchiveResourceFromFile();
            } else if (gMode == StorylandMode::MediaFile) {
                gMediaFile.stop();
                updateActionBar();
                InvalidateRect(gPreview, nullptr, FALSE);
            }
            break;
        case ID_ACTION_TERTIARY:
            if (gMode == StorylandMode::AnimFile ||
                (gMode == StorylandMode::ModelFile && gModelAnimLoaded)) {
                stepAnimPlayback(-1.0f / std::max(1.0f, gAnimFile.framesPerSecond()));
                updateActionBar();
            } else if (gMode == StorylandMode::TextureArchive) {
                editSelectedTextureMaterial();
            } else if (gMode == StorylandMode::ArchiveFile && gArchiveBrowser.hasLvzContext()) {
                testCurrentLvzImgPair();
            } else if (gMode == StorylandMode::MediaFile && gMediaFile.kind() == StorylandMediaKind::Video) {
                std::string mediaError;
                if (!gMediaFile.stepVideoFrame(-1, mediaError))
                    MessageBoxW(gMainWindow, widen(mediaError).c_str(), L"Storyland video frame step", MB_ICONERROR);
                updateMediaVideoDetails();
                updateMediaTimelineFromDecoder();
                InvalidateRect(gPreview, nullptr, FALSE);
                setStatus(L"Video frame " + std::to_wstring(gMediaFile.videoFrameIndex()) + L" | " +
                          formatMediaSeconds(gMediaFile.videoPositionSeconds()));
            }
            break;
        case ID_ACTION_QUATERNARY:
            if (gMode == StorylandMode::AnimFile ||
                (gMode == StorylandMode::ModelFile && gModelAnimLoaded)) {
                stepAnimPlayback(1.0f / std::max(1.0f, gAnimFile.framesPerSecond()));
                updateActionBar();
            } else if (gMode == StorylandMode::TextureArchive) {
                removeSelectedTexture();
            } else if (gMode == StorylandMode::ArchiveFile && gArchiveBrowser.hasLvzContext()) {
                overwriteCurrentLvzImgPair();
            } else if (gMode == StorylandMode::MediaFile && gMediaFile.kind() == StorylandMediaKind::Video) {
                std::string mediaError;
                if (!gMediaFile.stepVideoFrame(1, mediaError))
                    MessageBoxW(gMainWindow, widen(mediaError).c_str(), L"Storyland video frame step", MB_ICONERROR);
                updateMediaVideoDetails();
                updateMediaTimelineFromDecoder();
                InvalidateRect(gPreview, nullptr, FALSE);
                setStatus(L"Video frame " + std::to_wstring(gMediaFile.videoFrameIndex()) + L" | " +
                          formatMediaSeconds(gMediaFile.videoPositionSeconds()));
            }
            break;
        case ID_FILE_EXPORT_TEXTURE: exportSelectedTexture(); break;
        case ID_FILE_REPLACE_TEXTURE: replaceSelectedTexture(); break;
        case ID_FILE_RENAME_TEXTURE: renameSelectedTexture(); break;
        case ID_TEXTURE_ADD: addTextureMaterial(); break;
        case ID_TEXTURE_SWAP: swapSelectedTextureData(); break;
        case ID_TEXTURE_DUPLICATE: duplicateSelectedTexture(); break;
        case ID_TEXTURE_REMOVE: removeSelectedTexture(); break;
        case ID_TEXTURE_VALIDATE: validateCurrentTextureArchive(); break;
        case ID_MODEL_IMPORT_DATA: importModelDataIntoCurrentDraft(); break;
        case ID_DTZ_REPLACE_SELECTED_ENTRY: replaceSelectedDtzDirEntryFromFile(); break;
        case ID_DTZ_RENAME_RESOURCE: renameSelectedDtzDirEntry(); break;
        case ID_ARCHIVE_ADD_RESOURCE: addResourceToCurrentArchive(); break;
        case ID_ARCHIVE_REPLACE_SELECTED_RESOURCE: replaceSelectedArchiveResourceFromFile(); break;
        case ID_ARCHIVE_REPLACE_MESH_WITH_RESOURCE_ID: replaceSelectedMeshResourceWithResourceId(); break;
        case ID_ARCHIVE_CHANGE_SELECTED_MESH_RESOURCE_ID: changeSelectedMeshResourceId(); break;
        case ID_ARCHIVE_EXPORT_LVZ_IMG_PAIR: exportLvzImgPair(); break;
        case ID_ARCHIVE_OVERWRITE_LVZ_IMG_PAIR: overwriteCurrentLvzImgPair(); break;
        case ID_ARCHIVE_TEST_LVZ_IMG_PAIR: testCurrentLvzImgPair(); break;
        case ID_MEDIA_REPLACE_CURRENT_FRAME: replaceCurrentVideoFrameImage(); break;
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
        case ID_VIEW_AUTO_MODEL_TEXTURE_V:
            gModelTextureVAuto = false;
            break;
        case ID_VIEW_FLIP_MODEL_TEXTURE_V: {
            gModelTextureVAuto = false;
            gModelFlipTextureV = !gModelFlipTextureV;
            rebuildViewMenu();
            setStatus(gModelFlipTextureV ? L"Flip V coordinate: on." : L"Flip V coordinate: off.");
            InvalidateRect(gPreview, nullptr, FALSE);
            break;
        }
        case ID_VIEW_SCRIPTING_CONSOLE:
            showStorylandScriptingConsole();
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
        case ID_VIEW_PRELIGHT_COMBINED:
            gPrelightViewMode = StorylandPrelightViewMode::Combined;
            CheckMenuRadioItem(gOpenGlMenu, ID_VIEW_PRELIGHT_COMBINED, ID_VIEW_PRELIGHT_OFF, ID_VIEW_PRELIGHT_COMBINED, MF_BYCOMMAND);
            InvalidateRect(gPreview, nullptr, FALSE);
            setStatus(L"Prelight: source vertex colours combined with Stories timecycle lighting.");
            break;
        case ID_VIEW_PRELIGHT_RAW:
            gPrelightViewMode = StorylandPrelightViewMode::Raw;
            CheckMenuRadioItem(gOpenGlMenu, ID_VIEW_PRELIGHT_COMBINED, ID_VIEW_PRELIGHT_OFF, ID_VIEW_PRELIGHT_RAW, MF_BYCOMMAND);
            InvalidateRect(gPreview, nullptr, FALSE);
            setStatus(L"Prelight: raw source vertex colours, no timecycle light/fog modulation.");
            break;
        case ID_VIEW_PRELIGHT_OFF:
            gPrelightViewMode = StorylandPrelightViewMode::Off;
            CheckMenuRadioItem(gOpenGlMenu, ID_VIEW_PRELIGHT_COMBINED, ID_VIEW_PRELIGHT_OFF, ID_VIEW_PRELIGHT_OFF, MF_BYCOMMAND);
            InvalidateRect(gPreview, nullptr, FALSE);
            setStatus(L"Prelight disabled for viewport inspection.");
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
        case ID_VIEW_SHOW_2DFX_LIGHTS:
            gOpenGlShow2dfxLights = !gOpenGlShow2dfxLights;
            CheckMenuItem(gOpenGlMenu, ID_VIEW_SHOW_2DFX_LIGHTS, MF_BYCOMMAND | (gOpenGlShow2dfxLights ? MF_CHECKED : MF_UNCHECKED));
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
            // VCS' strongest dawn transition sits between the 5AM and 6AM
            // timecycle rows.  Sampling exactly 6AM pushes the preview too far
            // into the cyan morning row and no longer resembles retail dawn.
            // Keep LCS at 6AM; use 05:30 for VCS so both warm horizon colour
            // and the near-horizon sun/moon transition are visible.
            gStoriesSky.setTime(gStoriesSky.game() == StorylandSkyGame::Vcs ? 5.5f : 6.0f);
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
        case ID_RESOURCE_ANALYZE: analyzeCurrentResource(); break;
        case ID_RESOURCE_ANALYZE_GRAPH: analyzeCurrentResourceGraph(); break;
        case ID_RESOURCE_ANALYZE_DATA: analyzeCurrentResourceData(); break;
        case ID_RESOURCE_ANALYZE_BACK: exitAnalyzeGraph(); break;
        case ID_HELP_ABOUT: showAboutDialog(); break;
        case ID_HELP_WIKI:
            ShellExecuteW(hwnd, L"open", L"https://github.com/spicybung/BLeeds/wiki", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        return 0;
    }
    case WM_NOTIFY: {
        if (gTreeMutationDepth > 0) return 0;
        LPNMHDR header = reinterpret_cast<LPNMHDR>(lParam);
        if (header && header->idFrom == ID_TREE && header->code == TVN_SELCHANGEDW) {
            NMTREEVIEWW* changed = reinterpret_cast<NMTREEVIEWW*>(lParam);
            StorylandTreePayload payload = payloadFromLParam(changed->itemNew.lParam);
            selectPayloadForCurrentMode(payload);
            return 0;
        }
        if (header && header->idFrom == ID_TREE && header->code == NM_CLICK) {
            DWORD messagePosition = GetMessagePos();
            POINT screenPoint{GET_X_LPARAM(messagePosition), GET_Y_LPARAM(messagePosition)};
            selectTreeItemAtScreenPoint(screenPoint);
            return 0;
        }
        if (header && header->idFrom == ID_TREE && header->code == NM_DBLCLK) {
            if (gMode == StorylandMode::ModelFile && gSelectedKind == StorylandTreeKind::ModelPrelight) {
                editSelectedModelPrelight();
                return 0;
            }
            if (gMode == StorylandMode::DtzArchive && gSelectedKind == StorylandTreeKind::DtzDataField && gSelectedIndex >= 0) {
                const auto& fields = gDtzArchive.dataFields();
                if (size_t(gSelectedIndex) < fields.size() && fields[size_t(gSelectedIndex)].editable) {
                    patchSelectedDtzDataField();
                    return 0;
                }
            }
            if (gMode == StorylandMode::ArchiveFile && gSelectedKind == StorylandTreeKind::ArchiveEntry && gSelectedIndex >= 0) {
                openSelectedArchiveEntry();
                return 0;
            }
            if (gMode == StorylandMode::DtzArchive && gSelectedKind == StorylandTreeKind::DtzDirEntry && gSelectedIndex >= 0) {
                openSelectedDtzDirEntryStandalone();
                return 0;
            }
            if (gMode == StorylandMode::DtzArchive && gSelectedKind == StorylandTreeKind::DtzFindResult && gSelectedIndex >= 0) {
                openDtzFindResult(gSelectedIndex);
                return 0;
            }
        }
        break;
    }
    case WM_DESTROY:
        KillTimer(hwnd, 2);
        KillTimer(hwnd, 3);
        gMediaFile.close();
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
        if (gScriptWindow && IsWindow(gScriptWindow)) DestroyWindow(gScriptWindow);
        if (gScriptFont) { DeleteObject(gScriptFont); gScriptFont = nullptr; }
        if (gScriptBackgroundBrush) { DeleteObject(gScriptBackgroundBrush); gScriptBackgroundBrush = nullptr; }
        if (gScriptToolbarBrush) { DeleteObject(gScriptToolbarBrush); gScriptToolbarBrush = nullptr; }
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
    } catch (const std::exception& exception) {
        std::wstring message = L"Storyland encountered an error while processing the current file:\r\n\r\n" + widen(exception.what()) +
                               L"\r\n\r\nThe current operation was stopped. Please report repeated crashes on the Storyland GitHub.";
        MessageBoxW(hwnd, message.c_str(), L"Storyland - Error", MB_OK | MB_ICONERROR);
        setStatus(L"Operation stopped because of an unexpected error.");
        return 0;
    } catch (...) {
        MessageBoxW(hwnd,
                    L"Storyland encountered an unknown error while processing the current file.\r\n\r\nThe current operation was stopped. Please report repeated crashes on the Storyland GitHub.",
                    L"Storyland - Error", MB_OK | MB_ICONERROR);
        setStatus(L"Operation stopped because of an unexpected error.");
        return 0;
    }
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
    uint32_t quoteIndex = 0;
};

static std::vector<StorylandSplashDrop> gSplashDrops;
static int gSplashTick = 0;
static int gSplashCell = 16;
static bool gSplashSkipRequested = false;
static uint32_t gSplashQuoteSeed = 0u;
static ULONGLONG gSplashVisibleStartedAt = 0;

struct StorylandSplashMemeFrame {
    HBITMAP bitmap = nullptr;
    int width = 0;
    int height = 0;
    UINT durationMs = 100;
};

static std::vector<StorylandSplashMemeFrame> gSplashMemeFrames;

static void clearSplashMemeFrames() {
    for (StorylandSplashMemeFrame& frame : gSplashMemeFrames) {
        if (frame.bitmap) {
            DeleteObject(frame.bitmap);
            frame.bitmap = nullptr;
        }
    }
    gSplashMemeFrames.clear();
}

static UINT splashGifFrameDelayMs(IWICBitmapFrameDecode* frame) {
    if (!frame) return 100u;
    IWICMetadataQueryReader* metadata = nullptr;
    if (FAILED(frame->GetMetadataQueryReader(&metadata)) || !metadata) return 100u;

    PROPVARIANT value{};
    PropVariantInit(&value);
    UINT delayMs = 100u;
    if (SUCCEEDED(metadata->GetMetadataByName(L"/grctlext/Delay", &value))) {
        unsigned long delayCentiseconds = 0;
        if (value.vt == VT_UI2) delayCentiseconds = value.uiVal;
        else if (value.vt == VT_UI4) delayCentiseconds = value.ulVal;
        if (delayCentiseconds > 0) delayMs = UINT(delayCentiseconds * 10u);
    }
    PropVariantClear(&value);
    metadata->Release();
    return std::clamp(delayMs, 40u, 250u);
}

static bool loadSplashMemeGif() {
    clearSplashMemeFrames();

    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = module ? FindResourceW(
        module,
        MAKEINTRESOURCEW(IDR_REIGNS_SPLASH_MEME_GIF),
        reinterpret_cast<LPCWSTR>(static_cast<ULONG_PTR>(10))) : nullptr;
    if (!resource) return false;

    HGLOBAL loaded = LoadResource(module, resource);
    const BYTE* bytes = loaded ? static_cast<const BYTE*>(LockResource(loaded)) : nullptr;
    const DWORD byteCount = loaded ? SizeofResource(module, resource) : 0u;
    if (!bytes || byteCount == 0u) return false;

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromMemory(
            const_cast<BYTE*>(bytes),
            byteCount);
    }
    if (SUCCEEDED(hr)) {
        hr = factory->CreateDecoderFromStream(
            stream,
            nullptr,
            WICDecodeMetadataCacheOnLoad,
            &decoder);
    }

    UINT frameCount = 0u;
    if (SUCCEEDED(hr)) hr = decoder->GetFrameCount(&frameCount);
    if (SUCCEEDED(hr)) {
        gSplashMemeFrames.reserve(frameCount);
        for (UINT frameIndex = 0u; frameIndex < frameCount; ++frameIndex) {
            IWICBitmapFrameDecode* frame = nullptr;
            IWICFormatConverter* converter = nullptr;
            UINT width = 0u;
            UINT height = 0u;

            hr = decoder->GetFrame(frameIndex, &frame);
            if (SUCCEEDED(hr)) hr = frame->GetSize(&width, &height);
            if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
            if (SUCCEEDED(hr)) {
                hr = converter->Initialize(
                    frame,
                    GUID_WICPixelFormat32bppBGRA,
                    WICBitmapDitherTypeNone,
                    nullptr,
                    0.0,
                    WICBitmapPaletteTypeCustom);
            }

            HBITMAP bitmap = nullptr;
            void* pixels = nullptr;
            if (SUCCEEDED(hr) && width > 0u && height > 0u) {
                BITMAPINFO info{};
                info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                info.bmiHeader.biWidth = LONG(width);
                info.bmiHeader.biHeight = -LONG(height);
                info.bmiHeader.biPlanes = 1;
                info.bmiHeader.biBitCount = 32;
                info.bmiHeader.biCompression = BI_RGB;
                bitmap = CreateDIBSection(
                    nullptr,
                    &info,
                    DIB_RGB_COLORS,
                    &pixels,
                    nullptr,
                    0);
                if (bitmap && pixels) {
                    const UINT stride = width * 4u;
                    const UINT imageBytes = stride * height;
                    hr = converter->CopyPixels(
                        nullptr,
                        stride,
                        imageBytes,
                        static_cast<BYTE*>(pixels));
                } else {
                    hr = E_OUTOFMEMORY;
                }
            }

            if (SUCCEEDED(hr) && bitmap) {
                StorylandSplashMemeFrame out{};
                out.bitmap = bitmap;
                out.width = int(width);
                out.height = int(height);
                out.durationMs = splashGifFrameDelayMs(frame);
                gSplashMemeFrames.push_back(out);
            } else if (bitmap) {
                DeleteObject(bitmap);
            }

            if (converter) converter->Release();
            if (frame) frame->Release();
            if (FAILED(hr)) break;
        }
    }

    if (decoder) decoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();

    if (FAILED(hr) || gSplashMemeFrames.empty()) {
        clearSplashMemeFrames();
        return false;
    }
    return true;
}

static void drawSplashMeme(HDC dc, const RECT& client) {
    if (!dc || gSplashMemeFrames.empty() || gSplashVisibleStartedAt == 0u) return;

    const ULONGLONG elapsed = GetTickCount64() - gSplashVisibleStartedAt;
    // Easter egg: flash the licking portion of the supplied GIF during the
    // final second of the 5.5-second Reigns Studios intro.
    constexpr ULONGLONG showFromMs = 4350u;
    constexpr ULONGLONG showUntilMs = 5350u;
    if (elapsed < showFromMs || elapsed >= showUntilMs) return;

    const size_t firstFrame = std::min<size_t>(13u, gSplashMemeFrames.size() - 1u);
    const size_t available = gSplashMemeFrames.size() - firstFrame;
    if (available == 0u) return;

    ULONGLONG localMs = elapsed - showFromMs;
    size_t frameIndex = firstFrame;
    for (size_t i = 0u; i < available; ++i) {
        const UINT duration = std::max(40u, gSplashMemeFrames[firstFrame + i].durationMs);
        if (localMs < duration) {
            frameIndex = firstFrame + i;
            break;
        }
        localMs -= duration;
        frameIndex = firstFrame + ((i + 1u) % available);
    }

    const StorylandSplashMemeFrame& frame = gSplashMemeFrames[frameIndex];
    if (!frame.bitmap || frame.width <= 0 || frame.height <= 0) return;

    const int targetWidth = std::max(1, frame.width * 3 / 2);
    const int targetHeight = std::max(1, frame.height * 3 / 2);
    const int x = client.left + (client.right - client.left - targetWidth) / 2;
    const int y = client.top + (client.bottom - client.top - targetHeight) / 2 + 34;

    HDC source = CreateCompatibleDC(dc);
    if (!source) return;
    HGDIOBJ old = SelectObject(source, frame.bitmap);
    const int oldMode = SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);
    StretchBlt(
        dc,
        x,
        y,
        targetWidth,
        targetHeight,
        source,
        0,
        0,
        frame.width,
        frame.height,
        SRCCOPY);
    SetStretchBltMode(dc, oldMode);
    SelectObject(source, old);
    DeleteDC(source);
}


static const wchar_t* gSplashRainQuotes[] = {
    // Vice City Stories — deliberately the largest share.
    L"No, you're the asshole who got kicked out of the boy scouts. - Vic",
    L"I am Lance Vance, baby - you can trust me. Lance T Vance - T for 'Trust'... - Lance",
    L"Try to keep up, man, I'm a baaad driver. - Lance",
    L"What the fuck are you talking about? - Vic",
    L"Let me squeeze a fart out of ya. - Phil",
    L"Yeah, fools seldom differ, dipshit. - Forbes",
    L"Marty can do it, and he was nearly inbred. You'll pick it up! - Louise",
    L"I play around all the time - but don't tell my wife. - Gonzalez",
    L"St. Victor of Vance, the holier than thou killer! - Martinez",
    L"Play time is over, bitches! - Lance",

    // San Andreas.
    L"Ah shit, here we go again. - CJ",
    L"Grove Street - Home. At least it was before I fucked everything up. - CJ",
    L"I am the real deal, fool. Oh yeah. A genius. - Ryder",
    L"Keep up, motherfucker! - Ryder",
    L"For Grove Street, baby! - Sweet",
    L"GROVE IS KING!! - Sweet",

    // Vice City.
    L"Vercetti, remember the name! - Tommy",
    L"I run this town now - me! - Tommy",
    L"He killed my brother. What do you expect me to do, mow his lawns? - Lance",
    L"Son, I could shoot a fly off your head at 80 feet. - Phil",
    L"We made it! We're rich! RIIIIICH! - Phil",
    L"I'm too crafty for that, sunshine! - Kent Paul",
    L"Bloody hell... you nutter! - Kent Paul"
};

static constexpr size_t gSplashRainQuoteCount = sizeof(gSplashRainQuotes) / sizeof(gSplashRainQuotes[0]);

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

static void randomizeSplashDropStyle(
    StorylandSplashDrop& drop,
    uint32_t random,
    size_t column
) {
    (void)column;
    drop.quoteIndex =
        gSplashRainQuoteCount > 0u
            ? random % uint32_t(gSplashRainQuoteCount)
            : 0u;
    drop.trail = 7 + int((random >> 17) % 15u);
    drop.speed = 6.0f + float((random >> 9) % 8u);
}
static void resetSplashDrops(int width, int height) {
    int columns = std::max(1, (width + gSplashCell - 1) / gSplashCell);
    gSplashDrops.resize(size_t(columns));
    for (int column = 0; column < columns; ++column) {
        uint32_t random = splashHash(0x52454947u + uint32_t(column) * 0x9E3779B9u);
        StorylandSplashDrop& drop = gSplashDrops[size_t(column)];
        drop.seed = random;
        randomizeSplashDropStyle(drop, random, size_t(column));
        drop.headY =
            -float(random % uint32_t(std::max(1, height + 320)));
    }
}

static void drawSplashQuoteBlock(
    HDC dc,
    const RECT& client,
    const wchar_t* phrase,
    int baseX,
    int baseY,
    int cell,
    COLORREF bright,
    COLORREF body
) {
    if (!phrase || !*phrase) return;

    const size_t phraseLength = wcslen(phrase);
    const int usableHeight = (std::max)(cell * 12, int(client.bottom) - 120);
    const int rowsPerColumn = std::max(12, usableHeight / cell);
    const int columnsNeeded =
        std::max(1, int((phraseLength + size_t(rowsPerColumn) - 1u) /
                       size_t(rowsPerColumn)));

    // Keep the complete attributed quote inside the splash whenever possible.
    const int blockWidth = columnsNeeded * cell;
    baseX = std::clamp(
        baseX,
        8,
        std::max(8, int(client.right) - blockWidth - 8));

    // Quotes drift downward with the Matrix rain, but start already visible.
    const int animatedY =
        baseY + int((gSplashTick * 2) % std::max(1, cell * 4));

    for (size_t index = 0u; index < phraseLength; ++index) {
        const int quoteColumn = int(index / size_t(rowsPerColumn));
        const int quoteRow = int(index % size_t(rowsPerColumn));
        const int x = baseX + quoteColumn * cell;
        const int y = animatedY + quoteRow * cell;
        if (y < -cell || y >= client.bottom - 4) continue;

        const float progress =
            phraseLength > 1u
                ? float(index) / float(phraseLength - 1u)
                : 1.0f;
        const float glow =
            0.72f + 0.28f * (1.0f - progress);
        SetTextColor(
            dc,
            index + 1u == phraseLength
                ? bright
                : splashScale(body, glow));

        const wchar_t glyph = phrase[index];
        TextOutW(dc, x, y, &glyph, 1);
    }
}

static void drawSplashMatrixRain(HDC dc, const RECT& client) {
    static const wchar_t glyphs[] =
        L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ<>[]{}+=*#:/\\|"
        L"ｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓ";
    constexpr size_t glyphCount =
        (sizeof(glyphs) / sizeof(glyphs[0])) - 1u;

    HFONT rainFont = CreateFontW(
        -15, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN,
        L"Consolas");
    HGDIOBJ oldFont = SelectObject(dc, rainFont);
    SetBkMode(dc, TRANSPARENT);

    // First draw classic random Matrix columns.
    for (size_t column = 0; column < gSplashDrops.size(); ++column) {
        const StorylandSplashDrop& drop = gSplashDrops[column];
        const float columnPhase =
            gSplashDrops.size() > 1
                ? (float(column) /
                   float(gSplashDrops.size() - 1u) - 0.5f) * 0.34f
                : 0.0f;
        const float palette = splashPaletteProgress(columnPhase);
        const COLORREF bright =
            splashBlend(
                RGB(214, 238, 255),
                RGB(255, 211, 237),
                palette);
        const COLORREF body =
            splashBlend(
                RGB(30, 132, 255),
                RGB(255, 116, 196),
                palette);

        const int x = int(column) * gSplashCell;
        for (int trailIndex = drop.trail;
             trailIndex >= 0;
             --trailIndex) {
            const int y =
                int(drop.headY) - trailIndex * gSplashCell;
            if (y < -gSplashCell || y >= client.bottom) continue;

            const uint32_t random =
                splashHash(
                    drop.seed ^
                    uint32_t(
                        gSplashTick * 37 -
                        trailIndex * 101));
            const wchar_t glyph =
                glyphs[random % glyphCount];

            if (trailIndex == 0) {
                SetTextColor(dc, bright);
            } else {
                const int strength =
                    28 +
                    (drop.trail - trailIndex) * 155 /
                    std::max(1, drop.trail);
                SetTextColor(
                    dc,
                    splashScale(
                        body,
                        float(strength) / 210.0f));
            }
            TextOutW(dc, x, y, &glyph, 1);
        }
    }

    // Then draw several complete attributed quote blocks over the rain.
    // Each quote still reads downward like Matrix code, but long lines wrap
    // into the next vertical column instead of being truncated to a handful
    // of letters.
    if (gSplashRainQuoteCount > 0u) {
        const float palette = splashPaletteProgress();
        const COLORREF quoteBright =
            splashBlend(
                RGB(235, 248, 255),
                RGB(255, 226, 244),
                palette);
        const COLORREF quoteBody =
            splashBlend(
                RGB(88, 176, 255),
                RGB(255, 136, 204),
                palette);

        const int width =
            std::max(1, int(client.right - client.left));
        const uint32_t quotePhase =
            uint32_t(gSplashTick / 18);
        const size_t baseQuote =
            size_t(
                splashHash(
                    gSplashQuoteSeed ^
                    (quotePhase * 0x9E3779B9u)) %
                uint32_t(gSplashRainQuoteCount));

        const int quoteCell = 15;
        const int y1 = 18;
        const int y2 = 44;
        const int y3 = 70;

        drawSplashQuoteBlock(
            dc, client,
            gSplashRainQuotes[
                (baseQuote + 0u) %
                gSplashRainQuoteCount],
            width / 12,
            y1,
            quoteCell,
            quoteBright,
            quoteBody);

        if (width >= 700) {
            drawSplashQuoteBlock(
                dc, client,
                gSplashRainQuotes[
                    size_t(
                        splashHash(
                            gSplashQuoteSeed ^
                            uint32_t(baseQuote) ^
                            0xA511E9B3u) %
                        uint32_t(gSplashRainQuoteCount))],
                width / 2 - 80,
                y2,
                quoteCell,
                quoteBright,
                quoteBody);
        }

        if (width >= 980) {
            drawSplashQuoteBlock(
                dc, client,
                gSplashRainQuotes[
                    size_t(
                        splashHash(
                            gSplashQuoteSeed ^
                            uint32_t(baseQuote) ^
                            0x63D83595u) %
                        uint32_t(gSplashRainQuoteCount))],
                width * 3 / 4,
                y3,
                quoteCell,
                quoteBright,
                quoteBody);
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
        gSplashVisibleStartedAt = 0u;
        loadSplashMemeGif();

        LARGE_INTEGER performanceCounter{};
        QueryPerformanceCounter(&performanceCounter);
        gSplashQuoteSeed = splashHash(
            uint32_t(GetTickCount64()) ^
            uint32_t(GetCurrentProcessId() * 0x9E37u) ^
            uint32_t(performanceCounter.LowPart) ^
            uint32_t(performanceCounter.HighPart));

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
        for (size_t column = 0u;
             column < gSplashDrops.size();
             ++column) {
            StorylandSplashDrop& drop =
                gSplashDrops[column];
            drop.headY += drop.speed;

            if (drop.headY -
                    float(drop.trail * gSplashCell) >
                float(client.bottom)) {
                const uint32_t random =
                    splashHash(
                        drop.seed ^
                        uint32_t(gSplashTick * 0x9E37));

                randomizeSplashDropStyle(
                    drop,
                    random,
                    column);

                drop.headY =
                    -float(
                        40u +
                        random % 360u);
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
        drawSplashMeme(memoryDc, client);
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
        clearSplashMemeFrames();
        gSplashVisibleStartedAt = 0u;
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
    startedAt = GetTickCount64();
    gSplashVisibleStartedAt = startedAt;
    playStorylandTheme(true);
    for (BYTE alpha = 0; alpha < 238; alpha = BYTE(alpha + 17)) {
        SetLayeredWindowAttributes(splash, 0, alpha, LWA_ALPHA);
        pumpSplashMessages(splash);
        Sleep(8);
    }
    SetLayeredWindowAttributes(splash, 0, 255, LWA_ALPHA);
    return splash;
}

static void finishStorylandSplash(HWND splash, ULONGLONG startedAt) {
    if (!splash) return;
    constexpr ULONGLONG totalIntroMs = 5500u;
    constexpr ULONGLONG fadeStepMs = 8u;
    constexpr int fadeStep = 17;
    constexpr ULONGLONG fadeSteps = 16u;
    constexpr ULONGLONG fadeOutMs = fadeStepMs * fadeSteps;
    const ULONGLONG fadeStartMs = totalIntroMs - fadeOutMs;

    while (!gSplashSkipRequested && GetTickCount64() - startedAt < fadeStartMs) {
        pumpSplashMessages(splash);
        Sleep(5);
    }

    if (!gSplashSkipRequested) {
        for (int alpha = 255; alpha >= 0; alpha -= fadeStep) {
            SetLayeredWindowAttributes(splash, 0, BYTE(alpha), LWA_ALPHA);
            pumpSplashMessages(splash);
            const ULONGLONG elapsed = GetTickCount64() - startedAt;
            if (elapsed >= totalIntroMs) break;
            const ULONGLONG remaining = totalIntroMs - elapsed;
            Sleep(DWORD(std::min<ULONGLONG>(fadeStepMs, remaining)));
        }
    }

    // The sting is timed from the instant playback starts, so fade-in, startup
    // work and fade-out are all included in the same 5.5-second budget.
    stopStorylandTheme();
    DestroyWindow(splash);
}

static void loadEmbeddedVcsTimecycle() {
    HRSRC resource = FindResourceW(
        gInstance,
        MAKEINTRESOURCEW(IDR_VCS_TIMECYC),
        reinterpret_cast<LPCWSTR>(static_cast<ULONG_PTR>(10)) // RT_RCDATA = integer resource type 10
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

static int runStorylandApplication(HINSTANCE hInstance, HINSTANCE, LPWSTR commandLine, int showCommand) {
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

    gMainWindow = CreateWindowExW(0, wc.lpszClassName, L"Storyland", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
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
        if (msg.message == WM_KEYDOWN && msg.hwnd == gScriptEditor && msg.wParam == VK_RETURN &&
            (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            runStorylandScriptEditor();
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_BACK && gDtzReturnAvailable) {
            returnToGameDtz();
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_F2 && msg.hwnd == gTree) {
            if (gMode == StorylandMode::DtzArchive && gSelectedKind == StorylandTreeKind::DtzDirEntry) {
                renameSelectedDtzDirEntry();
                continue;
            }
            if (gMode == StorylandMode::TextureArchive && gSelectedKind == StorylandTreeKind::Texture) {
                renameSelectedTexture();
                continue;
            }
        }
        const bool typingInScriptEditor = gScriptEditor && (msg.hwnd == gScriptEditor || IsChild(gScriptEditor, msg.hwnd));
        const bool typingInEditControl = [] (HWND hwnd) {
            if (!hwnd) return false;
            wchar_t className[32] = {};
            GetClassNameW(hwnd, className, int(std::size(className)));
            return _wcsicmp(className, L"Edit") == 0 || _wcsicmp(className, L"RichEdit20W") == 0 ||
                   _wcsicmp(className, L"RichEdit50W") == 0;
        }(msg.hwnd);
        if (!typingInScriptEditor && !typingInEditControl &&
            msg.message == WM_KEYDOWN && handleModelViewportShortcut(msg.wParam)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    stopStorylandTheme();
    stopStorylandTheme();
    CoUninitialize();
    return int(msg.wParam);
}

static std::wstring storylandCrashBasePath() {
    wchar_t tempDir[MAX_PATH] = {};
    DWORD tempLength = GetTempPathW(MAX_PATH, tempDir);
    std::wstring base = (tempLength > 0 && tempLength < MAX_PATH) ? std::wstring(tempDir) : L".";
    if (!base.empty() && base.back() != L'\\' && base.back() != L'/') base += L"\\";
    base += L"StorylandCrash_" + std::to_wstring(GetCurrentProcessId());
    return base;
}

static std::string narrowForCrashReport(const std::wstring& value) {
    if (value.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), int(value.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string result(size_t(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), int(value.size()), result.data(), needed, nullptr, nullptr);
    return result;
}

static void writeStorylandMiniDump(const std::wstring& dumpPath, EXCEPTION_POINTERS* exceptionPointers) {
    HMODULE dbgHelp = LoadLibraryW(L"Dbghelp.dll");
    if (!dbgHelp) return;
    using MiniDumpWriteDumpFn = BOOL (WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                                PMINIDUMP_EXCEPTION_INFORMATION,
                                                PMINIDUMP_USER_STREAM_INFORMATION,
                                                PMINIDUMP_CALLBACK_INFORMATION);
    auto miniDumpWriteDump = reinterpret_cast<MiniDumpWriteDumpFn>(GetProcAddress(dbgHelp, "MiniDumpWriteDump"));
    if (!miniDumpWriteDump) {
        FreeLibrary(dbgHelp);
        return;
    }

    HANDLE dumpFile = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dumpFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION info{};
        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = exceptionPointers;
        info.ClientPointers = FALSE;
        miniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile,
                          MINIDUMP_TYPE(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory),
                          exceptionPointers ? &info : nullptr, nullptr, nullptr);
        CloseHandle(dumpFile);
    }
    FreeLibrary(dbgHelp);
}

static LONG WINAPI storylandUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers) {
    const std::wstring basePath = storylandCrashBasePath();
    const std::wstring textPath = basePath + L".txt";
    const std::wstring dumpPath = basePath + L".dmp";

    const EXCEPTION_RECORD* record = exceptionPointers ? exceptionPointers->ExceptionRecord : nullptr;
    const uintptr_t faultAddress = record ? reinterpret_cast<uintptr_t>(record->ExceptionAddress) : 0;

    MEMORY_BASIC_INFORMATION memoryInfo{};
    HMODULE faultModule = nullptr;
    std::wstring faultModulePath;
    uintptr_t moduleBase = 0;
    if (faultAddress != 0 && VirtualQuery(reinterpret_cast<LPCVOID>(faultAddress), &memoryInfo, sizeof(memoryInfo))) {
        faultModule = static_cast<HMODULE>(memoryInfo.AllocationBase);
        moduleBase = reinterpret_cast<uintptr_t>(faultModule);
        wchar_t moduleName[MAX_PATH] = {};
        if (faultModule && GetModuleFileNameW(faultModule, moduleName, MAX_PATH) > 0) faultModulePath = moduleName;
    }

    HANDLE file = CreateFileW(textPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        std::ostringstream text;
        text << "Storyland catastrophic error\r\n";
        text << "----------------------------------------\r\n";
        if (record) {
            text << "Exception code: 0x" << std::hex << std::uppercase << record->ExceptionCode << "\r\n";
            text << "Fault address: 0x" << faultAddress << "\r\n";
            if (!faultModulePath.empty()) {
                text << "Fault module: " << narrowForCrashReport(faultModulePath) << "\r\n";
                text << "Module base: 0x" << moduleBase << "\r\n";
                text << "Module offset: 0x" << (faultAddress - moduleBase) << "\r\n";
            }
            if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
                const ULONG_PTR operation = record->ExceptionInformation[0];
                const ULONG_PTR address = record->ExceptionInformation[1];
                const char* operationName = operation == 0 ? "read" : (operation == 1 ? "write" : (operation == 8 ? "execute" : "unknown"));
                text << "Access violation: " << operationName << " at 0x" << address << "\r\n";
            }
        }
        text << std::dec;
        text << "Process ID: " << GetCurrentProcessId() << "\r\n";
        text << "Thread ID: " << GetCurrentThreadId() << "\r\n";
        if (exceptionPointers && exceptionPointers->ContextRecord) {
#ifdef _M_X64
            const CONTEXT* context = exceptionPointers->ContextRecord;
            text << std::hex << std::uppercase;
            text << "RIP: 0x" << context->Rip << "  RSP: 0x" << context->Rsp << "  RBP: 0x" << context->Rbp << "\r\n";
            text << "RAX: 0x" << context->Rax << "  RBX: 0x" << context->Rbx << "  RCX: 0x" << context->Rcx << "\r\n";
            text << "RDX: 0x" << context->Rdx << "  RSI: 0x" << context->Rsi << "  RDI: 0x" << context->Rdi << "\r\n";
            text << std::dec;
#endif
        }
        text << "\r\nA matching .dmp minidump is written beside this report when Windows DbgHelp is available.\r\n";
        text << "Please include both files when reporting a repeated crash.\r\n";
        const std::string bytes = text.str();
        DWORD written = 0;
        WriteFile(file, bytes.data(), DWORD(std::min<size_t>(bytes.size(), size_t((std::numeric_limits<DWORD>::max)()))), &written, nullptr);
        FlushFileBuffers(file);
        CloseHandle(file);
    }

    writeStorylandMiniDump(dumpPath, exceptionPointers);
    stopStorylandTheme();

    std::wstring message = L"Storyland crashed because Windows reported an access violation or another fatal exception.\r\n\r\n"
                           L"Crash report:\r\n" + textPath +
                           L"\r\n\r\nMinidump:\r\n" + dumpPath +
                           L"\r\n\r\nIf the crash repeats, include both files. The module name and exact failing address are now recorded.";
    MessageBoxW(nullptr, message.c_str(), L"Storyland - Catastrophic Error", MB_OK | MB_ICONERROR | MB_TASKMODAL);
    return EXCEPTION_EXECUTE_HANDLER;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE previousInstance, LPWSTR commandLine, int showCommand) {
    SetUnhandledExceptionFilter(storylandUnhandledExceptionFilter);
    try {
        return runStorylandApplication(hInstance, previousInstance, commandLine, showCommand);
    } catch (const std::exception& exception) {
        std::wstring message = L"Storyland stopped because of an unexpected error:\r\n\r\n" + widen(exception.what()) +
                               L"\r\n\r\nPlease report this error on the Storyland GitHub.";
        MessageBoxW(nullptr, message.c_str(), L"Storyland - Catastrophic Error", MB_OK | MB_ICONERROR | MB_TASKMODAL);
        return 1;
    } catch (...) {
        MessageBoxW(nullptr,
                    L"Storyland stopped because of an unexpected error.\r\n\r\nPlease report this error on the Storyland GitHub.",
                    L"Storyland - Catastrophic Error", MB_OK | MB_ICONERROR | MB_TASKMODAL);
        return 1;
    }
}
