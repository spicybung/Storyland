#pragma once
#include "src/storyland/storyland_pc_help.h"
#include "src/storyland/storyland_play_test_runtime.h"
#include "src/leeds/leeds_texture.h"
#include "src/leeds/leeds_dtz.h"
#include <map>
#include <memory>
#include <filesystem>
#include "src/leeds/leeds_retail_font.h"
#include <imgui.h>
#include "src/storyland/storyland_mission_vm.h"
struct StorylandStoriesCamera;
class StorylandGxtFile;

struct StorylandRetailHudAssets {
    std::map<std::string,StorylandRetailFont> fonts;
    std::map<std::string, std::shared_ptr<const RgbaImage>> sprites;
    uint64_t loadedFingerprint=0;
    size_t fontSprites=0,menuSprites=0,splashSprites=0;
    std::string source;
    std::map<int,std::string> radarTiles;
    int radarGrid=8;
    bool radarAtlas=false;
    float radarWorldMinimum=-2000.0f,radarTileSize=500.0f;
    uint64_t generation = 0;
    bool pcMode = false;
    std::vector<std::string> pcSpriteBindings;
    std::vector<std::string> pcFontBindings;
    std::string pcAssetsDiagnostic;
    void load(const StorylandDtzArchive& archive, const std::vector<std::wstring>& looseCandidates);
    void loadPcAssets(bool vcs, const std::filesystem::path& artworkDirectory = {});
    void setPcMode(bool enabled);
    bool loadUserTracksIcon(const std::wstring& path, std::string& error);
    bool loadUserTracksIcon(const std::vector<uint8_t>& bytes, std::string& error);
    bool loadUserTracksIcon(const LeedsTextureArchive& archive, std::string& error);
    bool loadUserTracksIconNear(const std::wstring& directory, std::string& error);
};

struct StorylandTextLayout {
    float horizontalScale=1.0f,lineSpacing=1.0f,fixedAdvance=0;
    bool rightAligned=false,justify=false;
    float rasterScaleX=1.0f,rasterScaleY=1.0f;
    bool insetTextureCoordinates=true;
    float cursorAdvanceQuantum=0;
    bool breakLongWords=true;
    const StorylandPcHelpBindings* pcHelpBindings=nullptr;
};
std::string storylandExpandRetailControlText(const std::string& text);
ImVec2 storylandMeasurePlayTestText(const StorylandRetailHudAssets* assets,const char* text,
    float height,float wrapWidth=0,const char* fontName="gametext",const StorylandTextLayout* layout=nullptr);
void storylandDrawPlayTestText(ImDrawList* draw,const StorylandRetailHudAssets* assets,ImVec2 position,
    ImU32 colour,const char* text,float height,float wrapWidth=0,const char* fontName="gametext",bool centered=false,
    const StorylandTextLayout* layout=nullptr);
void storylandDrawLcsScriptTexts(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& world,ImVec2 viewport,
    const std::function<std::string(const std::string&,const std::string&)>& translate);
std::string storylandFormatLcsMessage(const StorylandMissionEnvironment::LcsMessage& message,
    const std::function<std::string(const std::string&,const std::string&)>& translate);
void storylandDrawLcsMessages(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& world,ImVec2 viewport,bool afterFade,
    const std::function<std::string(const std::string&,const std::string&)>& translate);
void storylandDrawLcsNavigationArrow(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& world,const StorylandNativePoint& player,float cameraYaw,ImVec2 viewport);

// Native 480 x 272 script text geometry, shared by the renderer and asset checks.
struct StorylandScriptGlyph {
    StorylandRetailFontGlyph glyph;
    uint16_t code=0;
    bool special=false;
    ImVec2 position{},size{};
    ImU32 color=IM_COL32_WHITE;
    bool unicodeFallback=false;
};
struct StorylandScriptTextLayout {
    std::vector<StorylandScriptGlyph> glyphs;
    std::vector<int32_t> lineWidths;
    int32_t lineHeight=0,originX=0,originY=0;
};
struct StorylandOpticQuad {
    std::string texture;
    std::array<ImVec2,4> corners,uv; // Clockwise: top left, top right, bottom right, bottom left.
};
std::vector<StorylandOpticQuad> storylandManualAimOverlay(int32_t weapon,int32_t mode,bool widescreen);
ImU32 storylandTargetHealthColor(float health,float maximumHealth);
ImU32 storylandTargetIndicatorColor(bool vcs,bool character,float healthRatio);
std::vector<StorylandOpticQuad> storylandTargetIndicatorQuads(ImVec2 center,float depth,float aspect);
struct StorylandTargetIndicator {
    StorylandNativePoint origin{},position{};
    float health=0,maximumHealth=0;
    bool character=false,positionValid=false;
};
bool storylandResolveTargetIndicator(const StorylandPlayTestRuntime& runtime,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,StorylandTargetIndicator& indicator);
void storylandDrawTargetIndicator(ImDrawList* draw,ImTextureID texture,ImTextureID firearmSight,
    const StorylandPlayTestRuntime& runtime,const std::vector<StorylandNativeTrafficActor>& pedestrians,
    const StorylandStoriesCamera& camera,ImVec2 displaySize,float nearClip=.1f);
std::string storylandOnscreenTimerText(int32_t milliseconds);
std::string storylandOnscreenCounterText(int32_t value,int32_t maximum,bool money);
bool storylandHudObjectVisible(StorylandMissionEnvironment& mission,int16_t object,float meterValue=0);
std::u16string storylandMissionAreaName(const StorylandMissionEnvironment& mission);
void storylandDrawVcsAreaCaption(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& mission,ImVec2 displaySize);
void storylandDrawVcsVehicleCaption(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    const StorylandMissionEnvironment& mission,ImVec2 displaySize);
bool storylandPrepareNativeCredits(const StorylandGxtFile& gxt,const StorylandRetailHudAssets& assets,
    int32_t group,std::vector<StorylandMissionEnvironment::CreditsLine>& lines,std::string& error,bool vcs=true);
StorylandScriptTextLayout storylandLayoutNativeCreditLine(const StorylandRetailFont& font,
    const StorylandMissionEnvironment::CreditsLine& line,uint32_t scroll,bool vcs=true);
int storylandCompletionStage(const StorylandMissionEnvironment& mission);
StorylandScriptTextLayout storylandLayoutScriptText(const StorylandRetailFont& font,
    const std::u16string& text,float scale,bool widescreen,bool proportional,int32_t x,int32_t y,
    const ImVec4& clip,int32_t alignment,ImU32 color,bool correctRightEdge=false,bool wordWrap=true,
    bool preciseGlyphGeometry=false);
void storylandDrawMissionPresentation(ImDrawList* draw,const StorylandRetailHudAssets* assets,
    StorylandMissionEnvironment& mission,ImVec2 displaySize,const StorylandStoriesCamera* camera=nullptr,
    const StorylandPlayTestRuntime* runtime=nullptr,bool vcs=true);

using StorylandRadarOverlay = void (*)(ImDrawList* draw,ImVec2 center,ImVec2 radius,
    const StorylandNativePoint& origin,float cameraYaw,float range,bool invertedMap);

void storylandDrawPlayTestHud(const StorylandPlayTestRuntime& runtime,const StorylandNativePaths& paths,
    const std::vector<StorylandNativeTrafficActor>& pedestrians,const std::vector<StorylandNativeTrafficActor>& cars,
    const StorylandNativePoint& player,float heading,float cameraYaw,bool vcs,bool debugMenuOpen,bool cutsceneActive,
    const StorylandRetailHudAssets* retailHud = nullptr,
    StorylandMissionEnvironment* mission = nullptr,
    const StorylandStoriesCamera* camera = nullptr, float cinematicViewportTop = 0.0f,
    bool invertedMap = false, StorylandRadarOverlay radarOverlay = nullptr,
    const StorylandPcHelpBindings* helpBindings = nullptr);

ImTextureID storylandRetailSpriteTexture(const StorylandRetailHudAssets* assets,const std::string& name);
