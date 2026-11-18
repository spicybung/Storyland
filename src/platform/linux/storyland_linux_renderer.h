#pragma once

#include "src/graphics/storyland_mesh_effects.h"
#include "src/graphics/storyland_render_performance.h"
#include "src/graphics/storyland_shader_profile.h"
#include "src/graphics/storyland_vehicle_environment.h"
#include "src/storyland/storyland_display_settings.h"
#include "src/storyland/storyland_pc_help.h"
#include "src/storyland/storyland_mission_vm.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

class StorylandLinuxSession;
struct StorylandRetailHudAssets;

// All methods run on the SDL window thread with its OpenGL context current.
class StorylandLinuxRenderer {
public:
    StorylandLinuxRenderer();
    ~StorylandLinuxRenderer();
    StorylandLinuxRenderer(const StorylandLinuxRenderer&) = delete;
    StorylandLinuxRenderer& operator=(const StorylandLinuxRenderer&) = delete;

    bool initialize(StorylandLinuxSession& session, std::string& error);
    void prepareCameraVisibility(StorylandLinuxSession& session, int width, int height);
    void draw(StorylandLinuxSession& session, int width, int height);
    void drawEnvironmentControls(StorylandLinuxSession& session);
    bool loadScene(StorylandLinuxSession& session,
        const std::array<float, 3>& point, std::string& error);
    bool prepareCredits(StorylandLinuxSession& session,int32_t group,
        std::vector<StorylandMissionEnvironment::CreditsLine>& lines,std::string& error);
    void shutdown();
    const std::string& status() const;
    const StorylandRetailHudAssets* frontendAssets() const;
    double ps2EnvironmentCaptureMilliseconds() const;
    bool setShaderProfile(int profile, std::string& error);
    int shaderProfile() const { return selectedShader; }
    bool pcShaderEnabled() const { return storylandPcShaderProfile(selectedShader); }
    bool graphicsQualityApplies() const;
    float maximumAnisotropy();
    std::string fxaaStatus() const;
    StorylandGraphicsQualitySettings graphicsQuality;
    StorylandRenderPerformance performance;
    bool trails=true;
    StorylandMeshEffects meshEffects;
    StorylandVehicleEnvironmentState vehicleEnvironment;
    StorylandAspectRatio aspectRatio = StorylandAspectRatio::Auto;
    bool showBounds=false;
    StorylandPcHelpBindings helpBindings;

private:
    struct Resources;
    std::unique_ptr<Resources> resources;
    std::unique_ptr<StorylandRetailHudAssets> frontendArtwork;
    std::string statusMessage;
    int selectedShader = -1;
    int nativeShaderProfile = 2;
};
