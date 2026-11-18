#ifdef __ANDROID__
#include "src/platform/android/storyland_android_gl.h"
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include "src/platform/linux/storyland_linux_renderer.h"
#include "src/storyland/storyland_bike_controls.h"
#include "src/re/reVCS/runtime/storyland_vcs_retail_quirks.h"
#include "src/storyland/storyland_actor_shadow.h"
#include "src/storyland/storyland_ped_shadow.h"
#include "src/storyland/storyland_shadow_projection.h"
#include "src/graphics/storyland_actor_shadow_state.h"
#include "src/graphics/storyland_actor_clip.h"
#include "src/graphics/storyland_native_marker_render.h"
#include "src/storyland/storyland_world_frame.h"
#include "src/graphics/storyland_world_alpha_gl.h"
#include "src/graphics/storyland_world_submission.h"
#include "src/storyland/storyland_scene_materials.h"
#include "src/graphics/storyland_frame_blur.h"
#include <SDL.h>
#ifndef __ANDROID__
#include "src/storyland/storyland_screen_droplets_gl.h"
#include "src/storyland/storyland_ps2_postfx_gl.h"
#include "src/storyland/storyland_ps2_environment_gl.h"
#include "src/storyland/storyland_underwater_gl.h"
#include "src/storyland/storyland_fxaa_gl.h"
#include "src/storyland/storyland_texture_quality_gl.h"
#include "src/storyland/storyland_screen_water_events.h"
#endif
#include "src/graphics/storyland_box_lines.h"
#include "src/platform/linux/storyland_linux_session.h"
#include "src/re/reVCS/services/storyland_vcs_service_crew.h"
#include "src/platform/linux/storyland_script_shadow_projection.h"
#include "src/storyland/storyland_sky.h"
#include "src/storyland/storyland_time_menu.h"
#include "src/storyland/storyland_c2d_lights.h"
#include "src/storyland/storyland_script_corona_frame.h"
#include "src/re/reVCS/gangs/storyland_empire.h"
#include "src/storyland/storyland_authored_world_selection.h"
#include "src/storyland/storyland_water.h"
#include "src/graphics/storyland_underwater.h"
#include "src/graphics/storyland_water_reaction.h"
#include "src/shaders/storyland_aero_portable.h"
#include "src/graphics/storyland_flow_deformation.h"
#include "src/graphics/storyland_aero_material.h"
#include "src/graphics/storyland_flame_fractal.h"
#include "src/storyland/storyland_shaders.h"
#include "storyland_shader_sources.h"
#include "src/graphics/storyland_particle_render.h"
#include "src/graphics/storyland_native_particle_render.h"
#include "src/graphics/storyland_transparent_render.h"
#include "src/graphics/storyland_bullet_trace_render.h"
#include "src/graphics/storyland_aero_emission_render.h"
#include "src/storyland/storyland_muzzle_flash.h"
#include "src/graphics/storyland_road_skid_render.h"
#include "src/graphics/storyland_rubbish_render.h"
#include "src/graphics/storyland_pickup_render.h"
#include "src/storyland/storyland_water_cannon_math.h"
#include "src/storyland/storyland_world_detail.h"
#include "src/leeds/leeds_world_texture.h"
#include "src/storyland/storyland_vehicle_occupants.h"
#include "src/storyland/storyland_vehicle_wheels.h"
#include "src/storyland/storyland_vehicle_lights.h"
#include "src/graphics/storyland_vehicle_lights_render.h"
#include "src/storyland/storyland_script_vehicle_animation.h"
#include "src/storyland/storyland_vehicle_carjack_animation.h"
#include "src/re/reLCS2/pedestrians/storyland_lcs_ped_look.h"
#include "src/re/reVCS/runtime/storyland_vcs_attachment.h"
#include "src/re/reVCS/camera/storyland_vcs_camera_projection.h"
#include "src/re/reVCS/camera/storyland_vcs_camera_fov.h"
#include "src/storyland/storyland_retail_screen.h"
#include "src/storyland/storyland_play_test_hud.h"
#include "src/storyland/storyland_coop_presentation.h"
#include "src/storyland/storyland_world_names.h"
#include "src/storyland/storyland_mission_radar.h"
#include "src/storyland/storyland_mission_marker_priority.h"
#include "src/storyland/storyland_mission_world_markers.h"
#include "src/storyland/storyland_native_marker_mesh.h"
#include "src/storyland/storyland_intro_world_streaming.h"
#include "src/storyland/storyland_hud_geometry.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/storyland/storyland_weapon_attachment.h"
#include "src/leeds/leeds_native_texture.h"
#include "src/graphics/storyland_texture_upload.h"
#include "src/graphics/storyland_ps2_texture_upload.h"
#include "src/graphics/storyland_ps2_lighting.h"
#include "src/graphics/storyland_ps2_local_lights.h"
#include "src/graphics/storyland_ps2_environment.h"
#include "src/re/reVCS/render/storyland_vcs_psp_lighting.h"
#include "src/re/reVCS/render/storyland_vcs_psp_local_lights.h"
#include "src/re/reVCS/render/storyland_vcs_psp_environment.h"
#include "src/leeds/leeds_model_materials.h"
#include "src/re/reLCS2/world/storyland_lcs_named_texture.h"
#include "src/re/reLCS2/world/storyland_lcs_dynamic_placement.h"
#include "src/storyland/storyland_script_object.h"
#include "src/re/reLCS2/runtime/storyland_lcs_attachment.h"
#include "src/leeds/leeds_native_object_atomic.h"
#include "src/re/reVCS/world/storyland_vcs_object_visibility.h"
#include "src/storyland/storyland_service_population.h"
#include "src/storyland/storyland_vehicle_entry.h"
#include "src/re/reVCS/vehicles/storyland_vcs_helicopter_exit.h"
#include "src/storyland/storyland_pickup_visual.h"
#include "src/re/reLCS2/runtime/storyland_lcs_pickup_presentation.h"
#include "src/storyland/storyland_multiplayer.h"
#include "src/graphics/storyland_vehicle_environment_gl.h"
#include "src/graphics/storyland_vehicle_environment_scene.h"
#include "src/graphics/storyland_vehicle_environment_ui.h"
#include "src/graphics/storyland_imgui_viewport.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <numeric>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {
constexpr float Pi = 3.14159265358979323846f;

int gPortableShaderProfile=-1;
int gPortableEmissionPointCount=0;
std::array<float,24> gPortableEmissionPointPosition{},gPortableEmissionPointColor{};
std::array<float,8> gPortableEmissionPointRange{};
StorylandGraphicsQualitySettings gPortableGraphicsQuality;
#ifndef __ANDROID__
StorylandTextureQualityGl gPortableTextureQuality;
#endif
bool gPortableSkyIsVcs=false;
std::array<float,3> gPortablePspEnvironmentS{1,0,0};
std::array<float,3> gPortablePspEnvironmentT{0,0,-1};
std::array<float,3> gPortablePs2EnvironmentS{1,0,0};
std::array<float,3> gPortablePs2EnvironmentT{0,-1,0};
GLuint gPortableShadowTexture=0;
bool gPortableShadowReady=false;
int gPortableShadowSize=0;
std::array<float,16> gPortableEyeToShadow={
 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1
};

struct PreparedCameraView {
    StorylandViewRectangle rectangle;
    StorylandNativePoint eye{}, target{}, up{0,0,1};
    float fieldOfView = 70;
    float nativeFieldOfView = 70;
    double nearClip = .1, farClip = 200;
    bool nativeCutscene = false, speedCamera = false;
};

PreparedCameraView prepareCameraView(StorylandLinuxSession& session, StorylandSky& sky,
    uint32_t& skyMilliseconds, int width, int height, StorylandAspectRatio aspectRatio) {
    auto& env = session.environment;
    sky.setPspLighting(gPortableShaderProfile==3);
    sky.setTime(env.hour);
    sky.setWindDirection(session.windDirectionDegrees);
    sky.setNativeWeatherState(unsigned(std::max<int16_t>(0,env.weatherState.previous)),
        unsigned(std::max<int16_t>(0,env.weatherState.current)),env.weatherState.interpolation,env.weatherState.rain);
    sky.setExtraColours(env.extraColor,env.extraColorFade);
    if (env.gameMilliseconds >= skyMilliseconds)
        sky.update(float(env.gameMilliseconds-skyMilliseconds)/1000);
    skyMilliseconds = env.gameMilliseconds;

    PreparedCameraView view;
    const int resolvedAspect = StorylandRetailScreen::resolvedAspectMode(
        int(aspectRatio), session.active(), session.archive.isPsp());
    view.rectangle = storylandFitView(width, height, StorylandAspectRatio(resolvedAspect));
    const float aspect = float(view.rectangle.aspect());
    const bool croppedPs2Widescreen = StorylandRetailScreen::hasNativePs2WidescreenCrop(
        int(aspectRatio), session.active(), session.archive.isPsp());
    const bool nativeManual = session.vcs() && !env.camera.active && session.camera.manualAimActive;
    const bool nativeVehicle = !env.camera.active && session.camera.vehicleCameraActive;
    const bool nativeFollow = StorylandVcsCameraProjection::ps2FollowPed(session.vcs(),
        session.archive.isPsp(),env.camera.active,session.camera.manualAimActive,
        session.camera.aimLocked,session.camera.freeAimActive,session.camera.vehicleCameraActive);
    const float nativeHorizontal = storylandVcsCameraFieldOfView(session.camera.fieldOfView,
        session.vcs() ? env.camera.vcsFieldOfViewOverride : 0.0f);
    if (nativeManual || nativeFollow || nativeVehicle) {
        view.nativeFieldOfView=nativeHorizontal;
        if (!StorylandVcsCameraProjection::verticalFieldOfView(nativeHorizontal,aspect,view.fieldOfView)) {
            session.mission.state = StorylandMissionState::Fault;
            session.mission.diagnostic = "Camera has an invalid native FOV or viewport aspect.";
            view.fieldOfView = 0;
            return view;
        }
    } else {
        const float calculated = env.camera.active ? env.camera.fieldOfView : session.camera.fieldOfView;
        const float defaultVertical = std::clamp(storylandVcsCameraFieldOfView(calculated,
            session.vcs() ? env.camera.vcsFieldOfViewOverride : 0.0f), 5.f, 170.f);
        view.nativeFieldOfView=defaultVertical;
        const bool nativeVcsLock = session.vcs() && !env.camera.active &&
            session.camera.nativeLockActive;
        view.fieldOfView = nativeVcsLock ? defaultVertical :
            float(StorylandRetailScreen::ps2VerticalFieldOfView(
                defaultVertical, aspect, croppedPs2Widescreen));
    }
    view.nearClip = std::clamp(double(session.camera.vehicleCameraActive&&!env.camera.active?
        session.camera.vehicleNearClip:env.cameraNearClip), .001, 50.0);
    view.farClip = std::max(200.f,sky.state().worldFarClip);
    auto& eye = view.eye;
    auto& target = view.target;
    auto& up = view.up;
    eye = session.camera.eye;
    target = session.camera.aimLocked && !session.vcs() ?
        session.camera.aimTarget : session.camera.target;
    if(session.camera.vehicleCameraActive&&!env.camera.active)up=session.camera.vehicleUp;
    if (!session.camera.initialized) {
        eye = {session.playerPosition.x-4,session.playerPosition.y-6,session.playerPosition.z+3};
        target = {session.playerPosition.x,session.playerPosition.y,session.playerPosition.z+1};
    }
    if (env.camera.active && env.camera.positionSet && env.camera.targetSet) {
        if (session.camera.initialized) {
            eye = session.camera.eye;
            target = session.camera.target;
        } else {
            eye = {env.camera.x,env.camera.y,env.camera.z};
            target = {env.camera.targetX,env.camera.targetY,env.camera.targetZ};
        }
        up = {env.camera.upX,env.camera.upY,1+env.camera.upZ};
        const float dx=target.x-eye.x,dy=target.y-eye.y,dz=target.z-eye.z;
        const float span=std::hypot(dx,dy,dz);
        if (span>.00001f && std::isfinite(env.camera.roll) && env.camera.roll != 0) {
            const StorylandNativePoint forward{dx/span,dy/span,dz/span};
            StorylandNativePoint right{forward.y*up.z-forward.z*up.y,
                forward.z*up.x-forward.x*up.z,forward.x*up.y-forward.y*up.x};
            const float rightLength=std::hypot(right.x,right.y,right.z);
            if (rightLength>.00001f) {
                right={right.x/rightLength,right.y/rightLength,right.z/rightLength};
                const StorylandNativePoint verticalUp{right.y*forward.z-right.z*forward.y,
                    right.z*forward.x-right.x*forward.z,right.x*forward.y-right.y*forward.x};
                const float angle=env.camera.roll*Pi/180;
                up={verticalUp.x*std::cos(angle)-right.x*std::sin(angle),
                    verticalUp.y*std::cos(angle)-right.y*std::sin(angle),
                    verticalUp.z*std::cos(angle)-right.z*std::sin(angle)};
            }
        }
    }
    view.nativeCutscene=env.cutscene.loaded&&(env.cutscene.playing||env.cutscene.finished)&&session.cutscene.scene().ready();
    const bool gameplayCamera=session.active()&&!session.paused&&!session.runtime.paused&&!session.runtime.pauseMenu&&
        !view.nativeCutscene;
    if(gameplayCamera&&env.cameraShake.sampled&&env.cameraShake.sampledAt==env.gameMilliseconds) {
        const auto& shake=env.cameraShake.offset;
        eye.x+=shake[0];eye.y+=shake[1];eye.z+=shake[2];
        target.x+=shake[0];target.y+=shake[1];target.z+=shake[2];
    }
    view.speedCamera=gameplayCamera&&!env.camera.active&&!env.widescreen&&
        session.camera.vehicleCameraActive&&session.camera.vehicleMode>=0&&
        session.camera.vehicleMode<=3&&session.runtime.occupiedVehicle>=0;
    if(view.speedCamera) {
        const auto& shake=session.camera.speedEffects.shake;
        const auto& aimShift=session.camera.speedEffects.aimShift;
        eye.x+=shake.x;eye.y+=shake.y;eye.z+=shake.z;
        target.x+=shake.x+aimShift.x;target.y+=shake.y+aimShift.y;target.z+=shake.z+aimShift.z;
    }
    StorylandStoriesCamera lens = session.camera;
    lens.eye = eye; lens.target = target; lens.aimLocked = false; lens.fieldOfView = view.fieldOfView;
    const double nearClip = view.nearClip, farClip = view.farClip;
    session.camera.aspectRatio = aspect;
    env.sphereOnScreen = [lens, aspect, nearClip, farClip, up, nativeManual, nativeHorizontal](const std::array<float,3>& point, float radius) {
        if (nativeManual) return StorylandVcsCameraProjection::sphereVisible(lens,nativeHorizontal,
            {point[0],point[1],point[2]},radius,float(nearClip),float(farClip),up);
        return storylandCameraSphereVisible(lens, {point[0], point[1], point[2]}, radius,
            aspect, float(nearClip), float(farClip), up);
    };
    env.screenCoordinates = [lens,aspect,up](const std::array<float,3>& point,std::array<float,2>& screen) {
        return storylandCameraScreenCoordinates(lens,{point[0],point[1],point[2]},aspect,screen,up);
    };
    return view;
}

void multiplyMatrices(const float* a,const float* b,float* out){
 for(int col=0;col<4;col++)for(int row=0;row<4;row++){
  float result=0;
  for(int k=0;k<4;k++)result+=a[k*4+row]*b[col*4+k];
  out[col*4+row]=result;
 }
}

bool inverseMatrix(const float* matrix,float* result){
 double rows[4][8]={};
 for(int row=0;row<4;row++)for(int col=0;col<4;col++){
  rows[row][col]=matrix[col*4+row];rows[row][col+4]=row==col?1.0:0.0;
 }
 for(int col=0;col<4;col++){
  int pivot=col;
  for(int row=col+1;row<4;row++)if(std::abs(rows[row][col])>std::abs(rows[pivot][col]))pivot=row;
  if(std::abs(rows[pivot][col])<1e-15)return false;
  for(int k=0;k<8;k++)std::swap(rows[col][k],rows[pivot][k]);
  const double divisor=rows[col][col];
  for(double& value:rows[col])value/=divisor;
  for(int row=0;row<4;row++)if(row!=col){
   const double f=rows[row][col];
   for(int k=0;k<8;k++)rows[row][k]-=f*rows[col][k];
  }
 }
 for(int row=0;row<4;row++)for(int col=0;col<4;col++)result[col*4+row]=float(rows[row][col+4]);
 return true;
}


bool storylandPortableAeroProfile(int profile) {
    return profile==6||profile==7||profile==10||profile==11||profile==12;
}

bool storylandPortableWaterProfile(int profile) {
    return storylandPortableAeroProfile(profile);
}

std::unordered_map<const void*,std::pair<size_t,float>> gPortableEmissionCache;

float storylandPortableEmission(const StorylandDirectTextureResource* source) {
    if(!source||source->rgba.empty())return 0.0f;
    auto cached=gPortableEmissionCache.find(source->rgba.data());
    if(cached!=gPortableEmissionCache.end()&&cached->second.first==source->rgba.size())
        return cached->second.second;
    const float strength=storylandAeroMaterialEmission(source->name,source->rgba);
    gPortableEmissionCache[source->rgba.data()]={source->rgba.size(),strength};
    return strength;
}


GLuint uploadTexture(int width, int height, const uint8_t* pixels, size_t bytes,
    const std::vector<RgbaMipLevel>& mipLevels = {}) {
    if (width <= 0 || height <= 0 || !pixels ||
        uint64_t(width) * uint64_t(height) * 4u != bytes) return 0;
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (width > maximum || height > maximum) return 0;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLint alignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    storylandUploadRgbaTexture(width, height, pixels, mipLevels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    #ifndef __ANDROID__
    GLint uploadedWidth = 0, uploadedHeight = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &uploadedWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &uploadedHeight);
    if (uploadedWidth != width || uploadedHeight != height) {
        glDeleteTextures(1, &texture);
        return 0;
    }
    #else
    if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&texture);return 0;}
    #endif
    return texture;
}

template<class Image>
GLuint uploadSurfaceTexture(const Image& image) {
    if(gPortableShaderProfile!=0&&gPortableShaderProfile!=2)
        return uploadTexture(image.width,image.height,image.rgba.data(),image.rgba.size(),image.mipLevels);
    if(image.width<=0||image.height<=0||image.rgba.size()!=size_t(image.width)*size_t(image.height)*4u)return 0;
    GLint maximum=0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    if(image.width>maximum||image.height>maximum)return 0;
    GLuint texture=0;
    glGenTextures(1,&texture);
    if(!texture)return 0;
    glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    GLint alignment=4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    storylandUploadStoriesTexture(image,true);
    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);
    return texture;
}

bool compileProgram(bool vcs, StorylandShaderKind kind, GLuint& program, std::string& error, int profile = -1) {
    profile = storylandShaderRenderProfile(profile);
    GLuint stages[2]{};
    const char* sources[] = {storylandStoriesVertexShaderSource(vcs, kind),
        storylandStoriesFragmentShaderSource(vcs, kind)};
    if (profile >= 0 && profile < 4) {
        sources[0] = StorylandShaderSources::console_vert[profile][size_t(kind)];
        sources[1] = StorylandShaderSources::console_frag[profile][size_t(kind)];
    } else if (profile == 5) {
        sources[0]=StorylandShaderSources::ctw_vertex;
        sources[1]=StorylandShaderSources::ctw_fragment;
    } else if (profile == 4) {
        sources[0] = StorylandShaderSources::neo_vertex;
        sources[1] = kind == StorylandShaderKind::Vehicle ? StorylandShaderSources::neo_vehicle : StorylandShaderSources::neo_world;
    } else if (profile == 14 || profile == 15) {
        sources[0] = StorylandShaderSources::neo_vertex;
        sources[1] = profile == 14 ? StorylandShaderSources::cel_fragment :
            StorylandShaderSources::budokai_fragment;
    } else if (storylandPortableAeroProfile(profile)) {
        sources[0] = StorylandPortableAeroVertex;
        sources[1] = StorylandPortableAeroFragment;
    } else if (profile == 9) {
        sources[0] = StorylandPortableAeroVertex;
        sources[1] = StorylandPortableSanAndreasFragment;
    } else if (profile == 13) {
        sources[0] = StorylandPortableShadowVertex;
        sources[1] = StorylandPortableShadowFragment;
    } else if (profile == 8) {
        sources[0] = StorylandPortableWaterVertex;
        sources[1] = StorylandPortableWaterFragment;
    }
    #ifdef __ANDROID__
    const std::string translated[]={storylandAndroidShader(sources[0],true),storylandAndroidShader(sources[1],false)};
    sources[0]=translated[0].c_str();sources[1]=translated[1].c_str();
    #endif
    const GLenum types[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
    for (size_t index = 0; index < 2; ++index) {
        stages[index] = glCreateShader(types[index]);
        if (!stages[index] || !sources[index]) {
            error = "OpenGL could not create the native Stories shader.";
            for (auto shader : stages) if (shader) glDeleteShader(shader);
            return false;
        }
        glShaderSource(stages[index], 1, &sources[index], nullptr);
        glCompileShader(stages[index]);
        GLint okay = GL_FALSE;
        glGetShaderiv(stages[index], GL_COMPILE_STATUS, &okay);
        if (okay != GL_TRUE) {
            char log[4096]{};
            glGetShaderInfoLog(stages[index], sizeof(log) - 1, nullptr, log);
            error = std::string("Native Stories shader compilation failed: ") + log;
            for (auto shader : stages) if (shader) glDeleteShader(shader);
            return false;
        }
    }
    program = glCreateProgram();
    for (auto shader : stages) glAttachShader(program, shader);
    glLinkProgram(program);
    for (auto shader : stages) glDeleteShader(shader);
    GLint okay = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &okay);
    if (okay != GL_TRUE) {
        char log[4096]{};
        glGetProgramInfoLog(program, sizeof(log) - 1, nullptr, log);
        error = std::string("Native Stories shader linking failed: ") + log;
        glDeleteProgram(program);
        program = 0;
        return false;
    }
    return true;
}

void setProgram(GLuint program, const StorylandSkyState& sky, bool vehicle = false, const std::array<float,3>* eyeLight = nullptr,
    const std::array<float,3>* eyeUp = nullptr,float rain = 0) {
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "tex0"), 0);
    glUniform1i(glGetUniformLocation(program, "uRenderMode"), 0);
    glUniform1i(glGetUniformLocation(program, "uPrelightMode"), 0);
    const auto fog=gPortableShaderProfile==3?
        storylandPspFogColor({sky.zenith.r,sky.zenith.g,sky.zenith.b},
            {sky.horizon.r,sky.horizon.g,sky.horizon.b}):
        (gPortableShaderProfile==0||gPortableShaderProfile==2)?
        storylandPs2FogColor({sky.zenith.r,sky.zenith.g,sky.zenith.b},
            {sky.horizon.r,sky.horizon.g,sky.horizon.b}):
        std::array<float,3>{sky.fog.r,sky.fog.g,sky.fog.b};
    glUniform3fv(glGetUniformLocation(program, "uFogColor"),1,fog.data());
    glUniform3f(glGetUniformLocation(program,"uConsoleAmbient"),sky.ambient.r,sky.ambient.g,sky.ambient.b);
    const auto pspLighting=storylandPspLightingColors(
        {sky.ambientBlur.r,sky.ambientBlur.g,sky.ambientBlur.b},
        {sky.objectAmbientBlur.r,sky.objectAmbientBlur.g,sky.objectAmbientBlur.b});
    const auto consoleEmissive=gPortableShaderProfile==3?pspLighting.worldEmissive:
        std::array<float,3>{sky.objectAmbient.r,sky.objectAmbient.g,sky.objectAmbient.b};
    glUniform3fv(glGetUniformLocation(program,"uConsoleEmissive"),1,consoleEmissive.data());
    glUniform1f(glGetUniformLocation(program,"uPs2ColorScale"),1.0f);
    glUniform1f(glGetUniformLocation(program,"uPspTextureScale"),2.0f);
    glUniform1f(glGetUniformLocation(program,"uPspTextureLodBias"),0.0f);
    glUniform1i(glGetUniformLocation(program,"uPspTextureLodMode"),0);
    glUniform1f(glGetUniformLocation(program,"uPspTextureLodSlope"),1.0f);
    glUniform1f(glGetUniformLocation(program,"uPspDepthOffset"),0.0f);
    glUniform1i(glGetUniformLocation(program,"uPspPrelitModel"),0);
    glUniform2f(glGetUniformLocation(program,"uPspEnvironmentSize"),0,0);
    glUniform3fv(glGetUniformLocation(program,"uPspEnvironmentS"),1,gPortablePspEnvironmentS.data());
    glUniform3fv(glGetUniformLocation(program,"uPspEnvironmentT"),1,gPortablePspEnvironmentT.data());
    glUniform3f(glGetUniformLocation(program,"uPspExtraLightColor0"),0,0,0);
    glUniform3f(glGetUniformLocation(program,"uPspExtraLightColor1"),0,0,0);
    if(eyeLight)glUniform3f(glGetUniformLocation(program,"uLightEye"),(*eyeLight)[0],(*eyeLight)[1],(*eyeLight)[2]);
    const auto objectAmbient=gPortableShaderProfile==3?pspLighting.objectAmbient:
        std::array<float,3>{sky.objectAmbient.r,sky.objectAmbient.g,sky.objectAmbient.b};
    glUniform3fv(glGetUniformLocation(program, "uAmbientColor"),1,objectAmbient.data());
    glUniform3f(glGetUniformLocation(program, "uDirectionalColor"),
        sky.directional.r, sky.directional.g, sky.directional.b);
    if(gPortableShaderProfile==0||gPortableShaderProfile==2) {
        const auto selected=storylandPs2SelectLighting(
            {sky.ambient.r,sky.ambient.g,sky.ambient.b},
            {sky.ambientBlur.r,sky.ambientBlur.g,sky.ambientBlur.b},
            {sky.objectAmbient.r,sky.objectAmbient.g,sky.objectAmbient.b},
            {sky.objectAmbientBlur.r,sky.objectAmbientBlur.g,sky.objectAmbientBlur.b},
            {sky.directional.r,sky.directional.g,sky.directional.b});
        const auto lighting=storylandPs2LightingColors(gPortableShaderProfile==2?
            StorylandPs2LightingGame::ViceCityStories:StorylandPs2LightingGame::LibertyCityStories,
            {sky.ambient.r,sky.ambient.g,sky.ambient.b},selected.worldAmbient,
            selected.objectAmbient,selected.directional);
        glUniform4fv(glGetUniformLocation(program,"uPs2WorldMultiplier"),1,lighting.world.multiplier.data());
        glUniform4fv(glGetUniformLocation(program,"uPs2WorldAdditive"),1,lighting.world.additive.data());
        glUniform4fv(glGetUniformLocation(program,"uPs2ColorCeiling"),1,lighting.world.ceiling.data());
        glUniform3fv(glGetUniformLocation(program,"uPs2ActorAmbient"),1,lighting.actorAmbient.data());
        glUniform3fv(glGetUniformLocation(program,"uPs2DirectionalColor"),1,lighting.actorDirectional.data());
        for(const char* name:{"uPs2ExtraLightColor0","uPs2ExtraLightColor1","uPs2ExtraLightColor2"})
            glUniform3f(glGetUniformLocation(program,name),0,0,0);
        const auto fogParameters=storylandPs2FogParameters(sky.worldFogStart,sky.worldFarClip);
        glUniform3f(glGetUniformLocation(program,"uPs2Fog"),fogParameters.slope,
            fogParameters.intercept,fogParameters.enabled?1.0f:0.0f);
        glUniform3fv(glGetUniformLocation(program,"uPs2EnvironmentS"),1,gPortablePs2EnvironmentS.data());
        glUniform3fv(glGetUniformLocation(program,"uPs2EnvironmentT"),1,gPortablePs2EnvironmentT.data());
        glUniform1i(glGetUniformLocation(program,"uPs2TextureFunction"),0);
        glUniform1i(glGetUniformLocation(program,"uPs2TextureAlpha"),1);
        glUniform1i(glGetUniformLocation(program,"uPs2NativeTextureAlpha"),1);
        glUniform1i(glGetUniformLocation(program,"uPs2PrelitModel"),0);
        glUniform1f(glGetUniformLocation(program,"uPs2EnvironmentCoefficient"),0);
    }
    const int lightingModel=gPortableShaderProfile==12?0:gPortableShaderProfile==11?1:2;
    glUniform1i(glGetUniformLocation(program,"uLightingModel"),lightingModel);
    glUniform1f(glGetUniformLocation(program,"uHeightScale"),
        gPortableShaderProfile==7?.022f:gPortableShaderProfile==6?.014f:
        gPortableShaderProfile==11?.009f:0.0f);
    glUniform1f(glGetUniformLocation(program,"uMafia2Tone"),gPortableShaderProfile==10?1.0f:0.0f);
    glUniform1f(glGetUniformLocation(program,"uNightStrength"),
        (gPortableShaderProfile==6||gPortableShaderProfile==7)?
        std::clamp(.18f-sky.sunDirectionZ,0.0f,1.0f):0.0f);
    glUniform1f(glGetUniformLocation(program,"uEmissiveStrength"),0.0f);
    if(storylandPortableAeroProfile(gPortableShaderProfile)){
        const int emissionCount=(gPortableShaderProfile==6||gPortableShaderProfile==7)?gPortableEmissionPointCount:0;
        glUniform1i(glGetUniformLocation(program,"uEmissionPointCount"),emissionCount);
        if(emissionCount) {
            glUniform3fv(glGetUniformLocation(program,"uEmissionPointPosition[0]"),emissionCount,
                gPortableEmissionPointPosition.data());
            glUniform3fv(glGetUniformLocation(program,"uEmissionPointColor[0]"),emissionCount,
                gPortableEmissionPointColor.data());
            glUniform1fv(glGetUniformLocation(program,"uEmissionPointRange[0]"),emissionCount,
                gPortableEmissionPointRange.data());
        }
        glUniformMatrix4fv(glGetUniformLocation(program,"uEyeToShadow"),1,GL_FALSE,
            gPortableEyeToShadow.data());
        glUniform1i(glGetUniformLocation(program,"uShadowEnabled"),gPortableShadowReady?1:0);
        glUniform1f(glGetUniformLocation(program,"uShadowTexel"),
            1.0f/float(std::max(1,gPortableShadowSize)));
        glUniform1i(glGetUniformLocation(program,"uShadowMap"),2);
        if(gPortableShadowReady){
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D,gPortableShadowTexture);
            glActiveTexture(GL_TEXTURE0);
        }
    }
    glUniform3f(glGetUniformLocation(program, "uSunDirection"),
        sky.sunDirectionX, sky.sunDirectionY, sky.sunDirectionZ);
    glUniform1i(glGetUniformLocation(program, "tex1"), 1);
    glUniform1i(glGetUniformLocation(program,"uDualTextureMode"),0);
    glUniform1i(glGetUniformLocation(program,"uDualTextureUvSet"),0);
    glUniform1f(glGetUniformLocation(program,"uDualTextureStrength"),0.0f);
    glUniform3f(glGetUniformLocation(program,"uDualTextureScale"),1.0f,1.0f,0.0f);
    glUniform1f(glGetUniformLocation(program, "uNativeMaterialEffect"), 0);
    glUniform1i(glGetUniformLocation(program,"uPs2BudokaiReflection"),0);
    glUniform1i(glGetUniformLocation(program, "uNeoVehicle"), vehicle ? 1 : 0);
    glUniform1i(glGetUniformLocation(program,"uSkyGfxGame"),gPortableSkyIsVcs?1:0);
    glUniform1f(glGetUniformLocation(program, "uReflection"), 0);
    glUniform4f(glGetUniformLocation(program, "u_lightMap"), 0, 0, 0, 1);
    if (eyeLight) glUniform3f(glGetUniformLocation(program, "uNeoLightDirection"), (*eyeLight)[0], (*eyeLight)[1], (*eyeLight)[2]);
    const GLint ctwSun=glGetUniformLocation(program,"sunEye");
    if(ctwSun>=0&&eyeLight&&eyeUp) {
        glUniform3f(ctwSun,-(*eyeLight)[0],-(*eyeLight)[1],-(*eyeLight)[2]);
        glUniform3fv(glGetUniformLocation(program,"upEye"),1,eyeUp->data());
        glUniform3f(glGetUniformLocation(program,"ambientColour"),sky.ambient.r,sky.ambient.g,sky.ambient.b);
        glUniform3f(glGetUniformLocation(program,"sunColour"),sky.directional.r,sky.directional.g,sky.directional.b);
        glUniform3f(glGetUniformLocation(program,"specularColour"),sky.directional.r,sky.directional.g,sky.directional.b);
        glUniform3f(glGetUniformLocation(program,"fogColour"),sky.fog.r,sky.fog.g,sky.fog.b);
        glUniform1i(glGetUniformLocation(program,"diffuseTexture"),0);
        glUniform1i(glGetUniformLocation(program,"nativeFog"),1);
        glUniform1f(glGetUniformLocation(program,"alphaCutoff"),.08f);
        glUniform1f(glGetUniformLocation(program,"fogStart"),sky.worldFogStart);
        glUniform1f(glGetUniformLocation(program,"fogEnd"),sky.worldFarClip);
        glUniform1f(glGetUniformLocation(program,"fogAmount"),1);
        glUniform1f(glGetUniformLocation(program,"wetness"),rain);
        glUniform1f(glGetUniformLocation(program,"specularStrength"),.35f);
        glUniform1f(glGetUniformLocation(program,"roughness"),.55f);
    }
    // Actors and map geometry share the same world-space fog distance.
    glUniform1f(glGetUniformLocation(program, "uFogStart"), sky.worldFogStart);
    const auto pspFog=storylandPspFogParameters(sky.worldFarClip,sky.worldFogStart);
    glUniform1f(glGetUniformLocation(program, "uFarClip"),
        gPortableShaderProfile==3?pspFog.farClip:sky.worldFarClip);
    glUniform1f(glGetUniformLocation(program,"uPspFogSlope"),pspFog.slope);
}

void drawBoundingBox(StorylandNativePoint low,StorylandNativePoint high) {
    glColor4f(1,.77f,.26f,1);
    glBegin(GL_LINES);
    storylandBoxLines(low,high,[](const auto& point){glVertex3f(point.x,point.y,point.z);});
    glEnd();
}

void setPspTextureSize(GLuint program, int width, int height, size_t mipCount) {
    glUniform2f(glGetUniformLocation(program,"uPspTextureSize"),float(width),float(height));
    glUniform1f(glGetUniformLocation(program,"uPspMaxMipLevel"),float(mipCount));
}

void bindTexture(GLuint program, GLuint texture, GLuint white = 0, const RgbaImage* image = nullptr) {
    glBindTexture(GL_TEXTURE_2D, texture ? texture : white);
#ifndef __ANDROID__
    if(texture)gPortableTextureQuality.applyBoundMaterial(gPortableGraphicsQuality,
        gPortableShaderProfile==0||gPortableShaderProfile==2||gPortableShaderProfile==4||
        storylandPortableAeroProfile(gPortableShaderProfile),SDL_GL_GetCurrentContext());
#endif
    if (texture) glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
    glUniform1i(glGetUniformLocation(program, "uUseTexture"), texture != 0);
    if (gPortableShaderProfile == 3) {
        int width = 0, height = 0, mipCount = 0;
        if (image) {
            width = image->width;
            height = image->height;
            mipCount = int(storylandAuthoredMipCount(width,height,image->mipLevels));
        }
#ifndef __ANDROID__
        else if (texture) {
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
            glGetTexParameteriv(GL_TEXTURE_2D,StorylandTextureMaxLevel,&mipCount);
        }
#endif
        setPspTextureSize(program,width,height,mipCount);
    }
}

struct Vertex {
    float u, v, r, g, b, a, x, y, z;
    float nx = 0, ny = 0, nz = 1;
};

class Batch {
public:
    Batch() { vertices.reserve(12288); }
    ~Batch() { flush(); }
    void add(const Vertex& vertex) {
        vertices.push_back(vertex);
        if (vertices.size() % 3 == 0) {
            auto& a = vertices[vertices.size()-3]; auto& b = vertices[vertices.size()-2]; auto& c = vertices.back();
            const float ax=b.x-a.x, ay=b.y-a.y, az=b.z-a.z, bx=c.x-a.x, by=c.y-a.y, bz=c.z-a.z;
            float nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
            const float length=std::hypot(nx,ny,nz);
            if (length > 1e-12f) { nx/=length;ny/=length;nz/=length; } else { nx=0;ny=0;nz=1; }
            for (auto* point : {&a,&b,&c}) {
                const float normalLength=std::hypot(point->nx,point->ny,point->nz);
                if (!std::isfinite(normalLength) || normalLength < 1e-8f) { point->nx=nx;point->ny=ny;point->nz=nz; }
            }
        }
        if (vertices.size() == 12288) flush();
    }
    void flush() {
        if (vertices.empty()) return;
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, sizeof(Vertex), &vertices.front().nx);
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &vertices.front().u);
        glColorPointer(4, GL_FLOAT, sizeof(Vertex), &vertices.front().r);
        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &vertices.front().x);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
        vertices.clear();
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
    }
private:
    std::vector<Vertex> vertices;
};

bool translucent(const StorylandDirectTextureResource& texture) {
    size_t fractional = 0;
    for (size_t i = 3; i < texture.rgba.size(); i += 4)
        if (texture.rgba[i] > 8 && texture.rgba[i] < 247) ++fractional;
    return fractional >= std::max(size_t(32), texture.rgba.size() / 20);
}

std::string rasterName(std::string name) {
    for (char& c : name) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    const auto separator = name.find_last_of("/\\");
    if (separator != std::string::npos) name.erase(0, separator + 1);
    for (const char* suffix : {".png", ".bmp", ".tga", ".dds"})
        if (name.size() >= 4 && name.compare(name.size() - 4, 4, suffix) == 0) {
            name.resize(name.size() - 4); break;
        }
    return name;
}

struct FrameActorTextures {
    std::unordered_map<const RgbaImage*, GLuint> textures;
    ~FrameActorTextures() {
        for (const auto& item : textures) if (item.second) glDeleteTextures(1, &item.second);
    }
    GLuint get(const RgbaImage& image) {
        const auto found = textures.find(&image);
        if (found != textures.end()) return found->second;
        const auto texture = uploadSurfaceTexture(image);
        textures.emplace(&image, texture);
        return texture;
    }
};

void lookAt(const StorylandNativePoint& eye, const StorylandNativePoint& target,
    StorylandNativePoint up) {
    auto normal = [](StorylandNativePoint p) {
        const float length = std::hypot(p.x, p.y, p.z);
        return length > 0.00001f ? StorylandNativePoint{p.x/length, p.y/length, p.z/length} :
            StorylandNativePoint{0, 1, 0};
    };
    auto cross = [](const StorylandNativePoint& a, const StorylandNativePoint& b) {
        return StorylandNativePoint{a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
    };
    const auto forward = normal({target.x-eye.x, target.y-eye.y, target.z-eye.z});
    auto right = cross(forward, up);
    if (std::hypot(right.x, right.y, right.z) < 0.00001f) right = cross(forward, {0,1,0});
    right = normal(right);
    up = cross(right, forward);
    const float matrix[] = {right.x,up.x,-forward.x,0, right.y,up.y,-forward.y,0,
        right.z,up.z,-forward.z,0, 0,0,0,1};
    glMultMatrixf(matrix);
    glTranslatef(-eye.x, -eye.y, -eye.z);
}

float minimumZ(const StorylandPlayTestAsset& asset) {
    float value = std::numeric_limits<float>::infinity();
    for (const auto& point : asset.model.previewPoints()) value = std::min(value, point.z);
    return std::isfinite(value) ? value : 0;
}

void pedestrianTransform(const StorylandPlayTestAsset& asset,
    const StorylandNativePoint& position, float yaw) {
    const auto bounds = storylandMeshBounds(asset.model.previewPoints());
    glTranslatef(position.x, position.y, position.z);
    glRotatef(yaw - storylandPedModelForwardAngle(asset.model), 0, 0, 1);
    if (!bounds.valid) return;
    const double span = std::max({bounds.maximum[0]-bounds.minimum[0],
        bounds.maximum[1]-bounds.minimum[1], bounds.maximum[2]-bounds.minimum[2]});
    const float scale = float(std::clamp(1.8/std::max(0.001, span), 0.001, 10.0));
    glScalef(scale, scale, scale);
    glTranslatef(float(-(bounds.maximum[0]+bounds.minimum[0])*0.5),
        float(-(bounds.maximum[1]+bounds.minimum[1])*0.5), float(-bounds.minimum[2]));
}

bool characterClothing(const StorylandLinuxSession& session,
    const StorylandMissionEnvironment::Character& character, std::array<uint32_t,6>& colors) {
    if (character.clothingColorsSet) {
        colors = character.clothingColors;
        return true;
    }
    const auto clothing = storylandDecodePedClothing(session.archive.unpackedBytes(),
        uint32_t(character.model), session.vcs());
    return storylandTrySelectNativePedClothing(clothing,character.scriptColorSet,
        character.colorSet,colors);
}

struct AreaFrame : StorylandWorldFrameResources {
    std::unordered_map<const StorylandDirectTextureResource*, bool> fractionalAlpha;
};

std::vector<AreaFrame> areaFrames(const StorylandLinuxSession& session) {
    std::vector<AreaFrame> result(session.areas.size());
    for (size_t area = 0; area < session.areas.size(); ++area)
        result[area].build(session.areas[area]->worldMeshes(), session.areas[area]->directTextures());
    return result;
}
void drawRoadSkids(const StorylandPlayTestRuntime& runtime,GLuint texture) {
    if(runtime.roadSkids.empty())return;
    glUseProgram(0);
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_TEXTURE_BIT);
    glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
    if(texture) {
        glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    }
    glBegin(GL_QUADS);
    for(const auto& skid:runtime.roadSkids)
        storylandVisitRoadSkid(skid,runtime.visibleArea,[](const auto& point,const auto& color,const auto& uv) {
            glColor4fv(color.data());glTexCoord2fv(uv.data());glVertex3f(point.x,point.y,point.z);
        },texture!=0);
    glEnd();glPopAttrib();
}

void drawParticles(const std::vector<StorylandRuntimeParticle>& particles,
    const std::map<std::string,GLuint>& textures,bool vcs,bool aeroFire,float seconds,
    const std::array<StorylandWaterCannon,3>* cannons=nullptr,int visibleArea=0,
    const StorylandPlayTestRuntime* runtime=nullptr,float nativeFieldOfView=70.0f) {
    if(particles.empty()&&(!cannons||!storylandWaterCannonActive(*cannons)))return;
    glUseProgram(0);
    double modelView[16]; glGetDoublev(GL_MODELVIEW_MATRIX,modelView);
    float view[16]; for (size_t index=0; index<16; ++index) view[index]=float(modelView[index]);
    float projection[16];glGetFloatv(GL_PROJECTION_MATRIX,projection);
    float rightAxis[3],upAxis[3];
    if (!storylandParticleBillboardAxes(view,rightAxis,upAxis)) return;
    const StorylandNativePoint right{rightAxis[0],rightAxis[1],rightAxis[2]},up{upAxis[0],upAxis[1],upAxis[2]};
    std::vector<size_t> order(particles.size());
    std::iota(order.begin(),order.end(),size_t(0));
    const auto depth=[&](size_t index) {const auto& p=particles[index].position;return view[2]*p.x+view[6]*p.y+view[10]*p.z+view[14];};
    std::sort(order.begin(),order.end(),[&](size_t a,size_t b){return depth(a)<depth(b);});
    const auto selectTexture=[](GLenum unit){glActiveTexture(unit);};
    const auto renderState=storylandBeginTransparentPass(true,selectTexture);
    const auto textureFor=[&](const char* name,bool nativePixels=false)->GLuint {
        if(!name)return 0;
        const auto found=textures.find(storylandParticleTextureKey(name,nativePixels));
        return found==textures.end()?0:found->second;
    };
#ifndef __ANDROID__
    // The Android compatibility layer does not yet preserve polygon offset
    // enable/factor state in its glPushAttrib/glPopAttrib emulation.
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f,-2.0f);
    bool surfaceOffset=false;
#endif
    for(size_t index:order) {
        glDisable(GL_TEXTURE_2D);
        const auto& particle=particles[index];
        if(particle.kind==StorylandParticleKind::BloodPool&&particle.bloodSurfaceArea>=0&&
            particle.bloodSurfaceArea!=visibleArea)continue;
#ifndef __ANDROID__
        const bool nextSurfaceOffset=storylandParticleUsesSurfaceDepthOffset(particle);
        if(nextSurfaceOffset!=surfaceOffset) {
            if(nextSurfaceOffset)glEnable(GL_POLYGON_OFFSET_FILL);else glDisable(GL_POLYGON_OFFSET_FILL);
            surfaceOffset=nextSurfaceOffset;
        }
#endif
        const float life=std::clamp(particle.remaining/std::max(0.001f,particle.lifetime),0.0f,1.0f);
        if(life<=0)continue;
        const float age=1-life;
        const bool flame=particle.kind==StorylandParticleKind::Flame;
        const bool flash=particle.kind==StorylandParticleKind::Flash||
            particle.kind==StorylandParticleKind::MuzzleFlash||particle.kind==StorylandParticleKind::Explosion;
        const bool spark=particle.kind==StorylandParticleKind::Spark;
        const bool lamp=particle.kind==StorylandParticleKind::Headlight||particle.kind==StorylandParticleKind::Taillight||
            particle.kind==StorylandParticleKind::PickupGlow;
        glBlendFunc(GL_SRC_ALPHA,flame||flash||spark||lamp?GL_ONE:GL_ONE_MINUS_SRC_ALPHA);
        if(particle.kind==StorylandParticleKind::Water)glBlendFunc(GL_SRC_ALPHA,GL_ONE);
        float red=0.3f,green=0.3f,blue=0.32f,alpha=life*0.35f;
        if(flame||flash||spark) {red=1;green=0.25f+life*0.55f;blue=0.04f;alpha=life*0.9f;}
        else if(particle.kind==StorylandParticleKind::Headlight) {red=1;green=0.97f;blue=0.8f;alpha=life;}
        else if(particle.kind==StorylandParticleKind::Taillight) {red=1;green=0.02f;blue=0.01f;alpha=life;}
        else if(particle.kind==StorylandParticleKind::Blood||particle.kind==StorylandParticleKind::BloodPool) {
            red=0.4f;green=0.015f;blue=0.02f;alpha=std::min(0.85f,life*4.0f);
        }
        else if(particle.kind==StorylandParticleKind::Gas) {red=0.68f;green=0.73f;blue=0.58f;alpha=life*0.25f;}
        else if(particle.kind==StorylandParticleKind::Steam) {red=green=blue=0.82f;alpha=life*0.22f;}
        else if(particle.kind==StorylandParticleKind::Water) {
            red=particle.swimmingParticle?.90f:.65f;
            green=particle.swimmingParticle?.95f:.82f;blue=1;alpha=life*.55f;
        }
        else if(particle.kind==StorylandParticleKind::Debris) {red=0.12f;green=0.12f;blue=0.13f;alpha=life;}
        else if(particle.kind==StorylandParticleKind::Dollar) {red=green=blue=1;alpha=std::min(1.0f,life*8);}
        if(particle.nativePhysics) {
            red=particle.nativeColor[0]/255.0f;green=particle.nativeColor[1]/255.0f;
            blue=particle.nativeColor[2]/255.0f;alpha=life*particle.nativeColor[3]/255.0f;
        }
        const auto position=runtime?storylandMuzzleFlashPosition(particle,*runtime):particle.position;
        if(spark) {
            glLineWidth(1.5f);glBegin(GL_LINES);glColor4f(red,green,blue,alpha);
            glVertex3f(position.x,position.y,position.z);glColor4f(red,green,blue,0);
            glVertex3f(position.x-particle.velocity.x*0.04f,position.y-particle.velocity.y*0.04f,position.z-particle.velocity.z*0.04f);
            glEnd();continue;
        }
        const bool blood=particle.kind==StorylandParticleKind::Blood||particle.kind==StorylandParticleKind::BloodPool;
        const bool dollar=particle.kind==StorylandParticleKind::Dollar;
        const float radius=particle.size*(particle.nativePhysics||particle.swimmingParticle||dollar||blood||lamp?1.0f:flame?0.6f+life*0.5f:1+age*1.8f);
        const float height=dollar?radius*.5f:flame&&!particle.nativePhysics?radius*2.3f:radius;
        GLuint texture=0;
        if(particle.kind==StorylandParticleKind::PickupGlow)
            texture=textureFor(storylandCoronaTextureName(vcs?7:5));
        for(const auto* name:storylandParticleTextureCandidates(particle.kind,particle.nativeFrame,
            particle.nativePhysics||particle.swimmingParticle?particle.nativeType:-1,vcs))
            if(texture || (texture=textureFor(name,particle.nativePresentation.enabled))!=0)break;
        if(!texture&&particle.kind==StorylandParticleKind::MuzzleFlash)continue;
        if(particle.nativePresentation.enabled) {
            storylandDrawNativeParticle(particle,texture,view,projection,nativeFieldOfView);
            continue;
        }
        if(texture) {
            glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
            // Native fire/explosion pixels already carry their warm colouring.
            const bool untinted=(flame||flash)&&!particle.nativePhysics;
            glColor4f(untinted?1:red,untinted?1:green,untinted?1:blue,alpha);
            if (flame && aeroFire) {
                // The original flame texture is bent through the same divergence-free field
                // as Curl-Noise Deformation. Six strips let the upper flame move independently.
                glBegin(GL_TRIANGLE_STRIP);
                for (int segment=0;segment<=6;++segment) {
                    const float fraction=float(segment)/6.0f;
                    const float vertical=height*(2.0f*fraction-1.0f);
                    const float curlHeight=std::max(0.0f,fraction);
                    const auto offset=storylandFireCurlOffset(position.x,position.y,position.z,
                        curlHeight,seconds,index);
                    const float upperWidth=radius*(1.0f-.32f*fraction);
                    for (int side=0;side<2;++side) {
                        const float fractal=storylandFlameFractalEdge(fraction,seconds,
                            uint32_t(index*29u+particle.nativeFrame));
                        const float horizontal=(side?upperWidth:-upperWidth)*(1.0f+fractal);
                        glTexCoord2f(float(side),1.0f-fraction);
                        glVertex3f(position.x+right.x*horizontal+up.x*vertical+offset.x*std::max(.3f,radius),
                            position.y+right.y*horizontal+up.y*vertical+offset.y*std::max(.3f,radius),
                            position.z+right.z*horizontal+up.z*vertical+offset.z*std::max(.3f,height));
                    }
                }
                glEnd();
                continue;
            }
            glBegin(GL_QUADS);
            for(const auto corner:std::array<std::array<float,2>,4>{{{-1,-1},{1,-1},{1,1},{-1,1}}}) {
                glTexCoord2f((corner[0]+1)*.5f,(1-corner[1])*.5f);
                const float horizontal=corner[0]*radius,vertical=corner[1]*height;
                if(particle.waterSurface) {
                    const auto vertex=storylandSwimmingSurfaceVertex(particle,horizontal,vertical);
                    glVertex3f(vertex.x,vertex.y,vertex.z);
                } else if(particle.kind==StorylandParticleKind::BloodPool) {
                    const auto vertex=storylandBloodPoolVertex(particle,horizontal,vertical);
                    glVertex3f(vertex.x,vertex.y,vertex.z);
                } else if(particle.nativePhysics&&(particle.nativeType==76||particle.nativeType==77))
                    glVertex3f(position.x+horizontal,position.y+vertical,position.z);
                else glVertex3f(position.x+right.x*horizontal+up.x*vertical,
                    position.y+right.y*horizontal+up.y*vertical,position.z+right.z*horizontal+up.z*vertical);
            }
            glEnd();continue;
        }
        if(dollar) {
            glColor4f(.25f,.62f,.32f,alpha);glBegin(GL_QUADS);
            for(const auto corner:std::array<std::array<float,2>,4>{{{-1,-1},{1,-1},{1,1},{-1,1}}}) {
                const float horizontal=corner[0]*radius,vertical=corner[1]*height;
                glVertex3f(position.x+right.x*horizontal+up.x*vertical,
                    position.y+right.y*horizontal+up.y*vertical,position.z+right.z*horizontal+up.z*vertical);
            }
            glEnd();continue;
        }
        // Missing, malformed or unavailable native sprites retain the previous
        // procedural particles, including their size, lifetime and blend mode.
        const int segments=particle.kind==StorylandParticleKind::Debris?3:12;
        glBegin(GL_TRIANGLE_FAN);glColor4f(red,green,blue,alpha);
        glVertex3f(position.x,position.y,position.z);
        glColor4f(red,green,blue,0);
        for(int segment=0;segment<=segments;++segment) {
            const float angle=float(segment)*6.28318530718f/segments;
            const float horizontal=std::cos(angle)*radius;
            const float vertical=std::sin(angle)*height;
            if(particle.waterSurface) {
                const auto vertex=storylandSwimmingSurfaceVertex(particle,horizontal,vertical);
                glVertex3f(vertex.x,vertex.y,vertex.z);
            } else if(particle.kind==StorylandParticleKind::BloodPool) {
                const float irregular=1.0f+0.15f*std::sin(angle*3.0f+float(index));
                const auto vertex=storylandBloodPoolVertex(particle,horizontal*irregular,vertical*irregular);
                glVertex3f(vertex.x,vertex.y,vertex.z);
            } else glVertex3f(position.x+right.x*horizontal+up.x*vertical,
                position.y+right.y*horizontal+up.y*vertical,position.z+right.z*horizontal+up.z*vertical);
        }
        glEnd();
    }
#ifndef __ANDROID__
    glDisable(GL_POLYGON_OFFSET_FILL);
#endif
    if(cannons) {
        const GLuint texture=textureFor("firehose");
        GLint wrapS=GL_REPEAT,wrapT=GL_REPEAT;
        if(texture) {
            glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
            glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,&wrapS);
            glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,&wrapT);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        } else glDisable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        const std::array<float,3> forward{-view[2],-view[6],-view[10]};
        const float phase=std::fmod(seconds*6.0f,1.0f);
        glBegin(GL_QUADS);
        for(const auto& cannon:*cannons)
            storylandWaterCannonRibbons(cannon,forward,phase,[](const auto& vertices) {
                for(const auto& vertex:vertices) {
                    glColor4ub(255,255,255,vertex.alpha);
                    glTexCoord2fv(vertex.uv.data());
                    glVertex3fv(vertex.position.data());
                }
            });
        glEnd();
        if(texture) {
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrapS);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrapT);
        }
    }
    storylandEndTransparentPass(renderState,selectTexture);
}
void drawMissionWorldMarkers(const StorylandLinuxSession& session,const StorylandNativePoint& eye,
    const std::map<std::string,GLuint>& textures) {
    const auto& world=session.environment;
    const auto& runtime=session.runtime;
    const bool vcs=session.vcs();
    const int32_t remoteVehicle=vcs?world.remoteVehicle:world.remoteControl.vehicle;
    const auto& focus=session.camera.initialized&&(world.camera.active||remoteVehicle>=0)?eye:session.playerPosition;
    const auto nearby=[&](const StorylandNativePoint& point) {
        return storylandRadarFinitePoint(point)&&std::hypot(point.x-focus.x,point.y-focus.y)<90;
    };
    const auto selectTexture=[](GLenum unit){glActiveTexture(unit);};
    const auto renderState=storylandBeginNativeMarkers(true,selectTexture);
    const auto nativeMarkerTexture=[&](const char* name) {
        const auto found=textures.find(name);
        return found==textures.end()?GLuint(0):found->second;
    };
    const auto arrow=[&](const StorylandNativePoint& point,const std::array<uint8_t,4>& color) {
        if(!nearby(point))return;
        const auto model=session.nativeMarkerModels.find(1);
        if(model!=session.nativeMarkerModels.end()) {
            storylandDrawNativeMarker(model->second,1,[&](const auto& drawVertex) {
                storylandVisitNativeDoorArrow(model->second,{point.x,point.y,point.z},
                    world.gameMilliseconds,{focus.x,focus.y,focus.z},color,drawVertex);
            },nativeMarkerTexture);
        }
    };
    if(!world.cutscene.playing&&!world.widescreen) {
        for(const auto& entry:runtime.entryExits) {
            if(runtime.visibleArea==entry.sourceArea&&
               storylandEntryExitOpen(entry,runtime.clockHour))arrow(entry.entrance,{255,209,15,255});
            if(runtime.visibleArea==entry.destinationArea&&
               storylandEntryExitOpen(entry,runtime.clockHour,true))arrow(entry.exit,{255,209,15,255});
        }
        storylandVisitMissionWorldMarkers(world,runtime,session.archive.nativeWorld(),
            session.vehicles,session.playerPosition,vcs,[&](const StorylandMissionWorldMarker& marker) {
                if(!nearby({marker.position[0],marker.position[1],marker.position[2]}))return;
                const auto model=session.nativeMarkerModels.find(marker.type);
                if(model==session.nativeMarkerModels.end())return;
                storylandDrawNativeMarker(model->second,marker.type,[&](const auto& drawVertex) {
                    if(marker.type==1u)
                        storylandVisitNativeWorldArrow(model->second,marker.position,marker.size,marker.color,
                            marker.createdAt,world.gameMilliseconds,{focus.x,focus.y,focus.z},drawVertex);
                    else
                        storylandVisitNativeWorldCylinderSet(model->second,marker.type,marker.position,
                            marker.size,marker.color,marker.createdAt,world.gameMilliseconds,
                            {focus.x,focus.y,focus.z},world.groundHeightAt,drawVertex,0.2f);
                },nativeMarkerTexture);
            });
        for(const auto& marker:world.markers) {
            if(!storylandMissionMarkerVisibleInArea(marker,runtime.visibleArea,vcs)||!nearby({marker.x,marker.y,marker.z})||
                !std::isfinite(marker.radius)||marker.radius<=0)continue;
            if(marker.type==5u||marker.type==6u||marker.type==7u) {
                const auto model=session.nativeMarkerModels.find(marker.type);
                if(model==session.nativeMarkerModels.end())continue;
                storylandDrawNativeMarker(model->second,marker.type,[&](const auto& drawVertex) {
                    storylandVisitNativePersistentMarker(model->second,marker,world,
                        {focus.x,focus.y,focus.z},drawVertex);
                },nativeMarkerTexture);
                continue;
            }
            glDisable(GL_TEXTURE_2D);glDepthMask(GL_FALSE);
            const float opacity=vcs&&marker.persistent?228.f/255.f:.48f;
            glBegin(GL_TRIANGLES);
            for(unsigned side=0;side<24;++side) {
                const float a=float(side)*2*Pi/24,b=float(side+1)*2*Pi/24;
                const float ax=marker.x+std::cos(a)*marker.radius,ay=marker.y+std::sin(a)*marker.radius;
                const float bx=marker.x+std::cos(b)*marker.radius,by=marker.y+std::sin(b)*marker.radius;
                const float red=marker.red/255.f,green=marker.green/255.f,blue=marker.blue/255.f;
                glColor4f(red,green,blue,opacity);glVertex3f(ax,ay,marker.z);glVertex3f(bx,by,marker.z);
                glColor4f(red,green,blue,0);glVertex3f(bx,by,marker.z+2);
                glColor4f(red,green,blue,opacity);glVertex3f(ax,ay,marker.z);
                glColor4f(red,green,blue,0);glVertex3f(bx,by,marker.z+2);glVertex3f(ax,ay,marker.z+2);
            }
            glEnd();
        }
        for(const auto& region:world.locateRegions) {
            if(!nearby({region.center[0],region.center[1],region.center[2]})||
                !std::isfinite(region.extent[0])||!std::isfinite(region.extent[1]))continue;
            const float x=region.center[0],y=region.center[1],z=region.center[2]+.04f;
            const float dx=region.extent[0],dy=region.extent[1];
            glDisable(GL_TEXTURE_2D);glDepthMask(GL_FALSE);
            glColor4f(1,.85f,.1f,.7f);glBegin(GL_LINE_LOOP);
            glVertex3f(x-dx,y-dy,z);glVertex3f(x+dx,y-dy,z);
            glVertex3f(x+dx,y+dy,z);glVertex3f(x-dx,y+dy,z);glEnd();
        }
    }
    const auto checkpoint=[&](const StorylandMissionEnvironment::ScriptCheckpoint& marker) {
        if(marker.area!=runtime.visibleArea)return;
        const auto model=session.nativeMarkerModels.find(marker.type);
        if(model!=session.nativeMarkerModels.end()) {
            storylandDrawNativeMarker(model->second,marker.type,[&](const auto& drawVertex) {
                storylandVisitNativeFrameMarker(model->second,marker,world,
                    {focus.x,focus.y,focus.z},drawVertex);
            },nativeMarkerTexture);
            return;
        }
        glDisable(GL_TEXTURE_2D);glDepthMask(GL_FALSE);
        const auto mesh=storylandCheckpointMesh(marker.position,marker.direction,marker.radius,marker.type);
        glColor4f(marker.color[0]/255.f,marker.color[1]/255.f,marker.color[2]/255.f,marker.color[3]/255.f);
        glBegin(GL_TRIANGLES);
        for(const auto& triangle:mesh)for(const auto& vertex:triangle)glVertex3f(vertex[0],vertex[1],vertex[2]);
        glEnd();
    };
    for(const auto& entry:world.frameScriptCheckpoints)checkpoint(entry.second);
    if(world.pointerArrowActive) {
        StorylandMissionEnvironment::ScriptCheckpoint pointer;
        GLfloat view[16];glGetFloatv(GL_MODELVIEW_MATRIX,view);
        if(storylandNativePointerArrow(world.pointerArrowPosition,{eye.x,eye.y,eye.z},
            {-view[2],-view[6],-view[10]},runtime.visibleArea,pointer)) {
            checkpoint(pointer);
        }
    }
    storylandEndNativeMarkers(renderState,selectTexture);
}
} // namespace

struct StorylandLinuxRenderer::Resources {
    StorylandFrameBlur speedBlur;
#ifndef __ANDROID__
    StorylandScreenDropletsGl screenDroplets;
    StorylandPs2PostEffectsGl ps2PostEffects;
    StorylandUnderwaterGl underwater;
    StorylandFxaaGl fxaa;
#endif
    const StorylandMeshEffects* meshEffects=nullptr;
    float meshTime=0;
    bool showActorBounds=false;

    void deform(StorylandMeshCategory category,const std::array<float,6>& bounds,size_t seed=0,
        const float* placement=nullptr) const {
        if(!meshEffects||!meshEffects->enabled(category))return;
        const auto effect=storylandMeshEffectTransform(*meshEffects,category,bounds,meshTime,seed,placement);
        glMultMatrixf(effect.data());
    }
    StorylandSky sky;
    StorylandRetailHudAssets hud;
    StorylandCoopPresentation coopPresentation;
    StorylandWorldNameHud worldNames;
    std::array<float,3> lightEye{},upEye{};
    float rain=0;
    bool neo = false;
    std::map<std::string,GLuint> particleTextures;
    std::map<std::string,GLuint> retailParticleTextures,pcParticleTextures;
    std::vector<GLuint> presentationTextureHandles;
    StorylandC2dLightFrame c2dLights;
    StorylandScriptCoronaFrame scriptCoronas;
    std::vector<StorylandC2dPointLight> pspPointLights;
    StorylandPs2PointLights ps2PointLights;
    std::vector<StorylandVcsPickupVisual> pickupVisuals;
    uint32_t pickupVisualRandomState=180371;
    std::array<float,16> pspView{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<float,3> pspExtraDirection{};
    bool pspExtraColourActive=false;
    GLuint pspEnvironmentTexture=0;
    bool pspEnvironmentReady=false;
    float pspEnvironmentStrength=1.0f;
    GLuint ps2EnvironmentTexture=0;
#ifndef __ANDROID__
    StorylandPs2EnvironmentGl ps2EnvironmentRenderer;
#endif
    bool ps2EnvironmentReady=false;
    std::string ps2EnvironmentDiagnostic;
    unsigned pspActorAlphaReference=8;
    std::vector<uint8_t> pspFramebufferPixels;
    std::array<GLuint, 5> programs{};
    // Backend handles belong to the loaded area/image, and are released on reload.
    std::vector<std::vector<GLuint>> worldTextures;
    uint64_t sceneryRevision=0;
    std::map<std::string,StorylandDirectTextureResource> namedWorldTextures;
    std::map<std::string,GLuint> namedWorldHandles;
    GLuint whiteTexture = 0;
    GLuint waterTexture = 0;
    GLuint retailWaterTexture = 0, pcWaterTexture = 0;
    GLuint aeroWaterProgram = 0;
    GLuint reactionTexture = 0;
    StorylandWaterReaction waterReaction;
    uint32_t lastWaterTime = UINT32_MAX;
    StorylandNativePoint lastWaterPosition{};
    bool waterPositionInitialized = false;
    GLuint charredVehicleTexture = 0;
    RgbaImage charredVehicleImage;
    StorylandVehicleEnvironmentGlTarget vehicleEnvironmentTarget;
    StorylandVehicleEnvironmentState* vehicleEnvironment = nullptr;
    GLuint targetTriangle = 0;
    GLuint retailTargetTriangle = 0, pcTargetTriangle = 0;
    std::array<GLuint, 5> opticTextures{};
    std::array<GLuint, 5> retailOpticTextures{},pcOpticTextures{};
    static constexpr std::array<const char*, 5> opticNames{{
        "siterocket", "sitesniper", "sitelaser", "viewfinder_128", "binox_128"}};
    GLuint shadowFramebuffer=0,shadowColor=0,shadowDepth=0,shadowProgram=0;
    int shadowSize=0;
    int shadowFrame=0;
    int lastShadowUpdate=-20;
    bool shadowValid=false;
    std::array<float,3> shadowFocus{},shadowSun{};
    float shadowRadius=0;
    std::array<float,16> shadowProjection{},shadowView{};
    size_t renderedPlacements = 0, renderedActors = 0;
    double ps2EnvironmentCaptureMilliseconds = 0;
    uint32_t skyMilliseconds = 0;
    bool introSceneryPrewarmed = false;
    uint32_t worldLevel = 0;
    uint32_t remoteLife = 0;
    StorylandAnimationBlendState remoteBlend;

    ~Resources() {
#ifndef __ANDROID__
        screenDroplets.release(SDL_GL_GetCurrentContext());
        ps2PostEffects.release(SDL_GL_GetCurrentContext());
        ps2EnvironmentRenderer.release(SDL_GL_GetCurrentContext());
        underwater.release(SDL_GL_GetCurrentContext());
        fxaa.release(SDL_GL_GetCurrentContext());
#endif
        if(pspEnvironmentTexture)glDeleteTextures(1,&pspEnvironmentTexture);
        if(ps2EnvironmentTexture)glDeleteTextures(1,&ps2EnvironmentTexture);
        vehicleEnvironmentTarget.release();
        speedBlur.release();
        for (const auto texture : presentationTextureHandles) glDeleteTextures(1,&texture);
        for (const auto& area : worldTextures) for (auto texture : area)
            if (texture) glDeleteTextures(1, &texture);
        for(const auto& texture:namedWorldHandles)if(texture.second)glDeleteTextures(1,&texture.second);
        if (whiteTexture) glDeleteTextures(1, &whiteTexture);
        if (reactionTexture) glDeleteTextures(1, &reactionTexture);
        if (aeroWaterProgram) glDeleteProgram(aeroWaterProgram);
        if(shadowProgram)glDeleteProgram(shadowProgram);
        if(shadowFramebuffer)glDeleteFramebuffers(1,&shadowFramebuffer);
        if(shadowDepth)glDeleteRenderbuffers(1,&shadowDepth);
        if(shadowColor)glDeleteTextures(1,&shadowColor);
        gPortableShadowTexture=0;gPortableShadowReady=false;gPortableShadowSize=0;
        if (charredVehicleTexture) glDeleteTextures(1, &charredVehicleTexture);
        for (auto program : programs) if (program) glDeleteProgram(program);
    }

    void applyPcArtwork() {
        particleTextures=retailParticleTextures;
        if(hud.pcMode)for(const auto& texture:pcParticleTextures)
            particleTextures[texture.first]=texture.second;
        waterTexture=hud.pcMode&&pcWaterTexture?pcWaterTexture:retailWaterTexture;
        targetTriangle=hud.pcMode&&pcTargetTriangle?pcTargetTriangle:retailTargetTriangle;
        for(size_t index=0;index<opticTextures.size();++index)
            opticTextures[index]=hud.pcMode&&pcOpticTextures[index]?
                pcOpticTextures[index]:retailOpticTextures[index];
    }

    void uploadPcArtwork() {
        std::map<std::pair<const RgbaImage*,bool>,GLuint> uploads;
        const auto upload=[&](const std::shared_ptr<const RgbaImage>& image,const std::string& name,
            bool prepareSmoke=false) {
            const auto key=std::make_pair(image.get(),prepareSmoke);
            const auto found=uploads.find(key);
            if(found!=uploads.end())return found->second;
            RgbaImage adjusted;
            const RgbaImage* raster=image.get();
            if(prepareSmoke) {
                adjusted=*image;
                storylandPrepareSmokeTexture(name,adjusted);
                raster=&adjusted;
            }
            const GLuint handle=uploadTexture(raster->width,raster->height,raster->rgba.data(),
                raster->rgba.size(),raster->mipLevels);
            if(handle) {
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                presentationTextureHandles.push_back(handle);
                uploads.emplace(key,handle);
            }
            return handle;
        };
        for(const auto& entry:hud.sprites) {
            if(entry.first.rfind("pc:particle:",0)!=0||!entry.second)continue;
            const auto name=entry.first.substr(12);
            const bool smoke=storylandParticleSmokeTexture(name);
            if(smoke) {
                const GLuint original=upload(entry.second,name);
                if(original)pcParticleTextures[storylandParticleTextureKey(name.c_str(),true)]=original;
            }
            const GLuint handle=upload(entry.second,name,smoke);
            if(handle)pcParticleTextures[name]=handle;
        }
        const auto sprite=[&](const char* name) {
            const auto found=hud.sprites.find(std::string("pc:")+name);
            return found!=hud.sprites.end()&&found->second?upload(found->second,name):GLuint(0);
        };
        pcTargetTriangle=sprite("triangle16");
        for(size_t index=0;index<opticNames.size();++index)pcOpticTextures[index]=sprite(opticNames[index]);
        const auto water=hud.sprites.find("pc:particle:waterclear256");
        if(water!=hud.sprites.end()&&water->second) {
            const auto& image=*water->second;
            pcWaterTexture=uploadTexture(image.width,image.height,image.rgba.data(),image.rgba.size(),image.mipLevels);
            if(pcWaterTexture)presentationTextureHandles.push_back(pcWaterTexture);
        }
        applyPcArtwork();
    }

    void refreshNativeScenery(const StorylandLinuxSession& session) {
        if(sceneryRevision==session.sceneryRevision)return;
        worldTextures.resize(session.areas.size());
        for(size_t area=0;area<session.areas.size();++area) {
            if(!session.areas[area]->hasNativeScenery())continue;
            for(GLuint texture:worldTextures[area])if(texture)glDeleteTextures(1,&texture);
            worldTextures[area].assign(session.areas[area]->directTextures().size(),0);
        }
        sceneryRevision=session.sceneryRevision;
        if(vehicleEnvironment) {
            vehicleEnvironment->ready=false;
            vehicleEnvironment->completedFaces=0;
            vehicleEnvironment->nextCaptureFace=0;
        }
        introSceneryPrewarmed=false;
        lastShadowUpdate=-20;
    }

    GLuint worldTexture(const StorylandLinuxSession& session, size_t area,
        const StorylandDirectTextureResource* source) {
        if (!source) return 0;
        const auto& textures = session.areas[area]->directTextures();
        const size_t index = size_t(source - textures.data());
        if (index >= textures.size() || area >= worldTextures.size()) return 0;
        auto& handles = worldTextures[area];
        if (handles.size() < textures.size()) handles.resize(textures.size());
        if (!handles[index]) handles[index] = uploadSurfaceTexture(*source);
        return handles[index];
    }

    GLuint namedWorldTexture(const StorylandLinuxSession& session,const StorylandArchiveBrowser& browser,
        uint32_t material,const StorylandDirectTextureResource*& source) {
        if(session.vcs())return 0;
        std::string name;if(!browser.nativeWorldTextureName(material,name))return 0;
        auto found=namedWorldTextures.find(name);
        if(found==namedWorldTextures.end()) {
            StorylandDirectTextureResource native;RgbaImage image;std::string diagnostic;
            if(storylandResolveLcsNamedWorldTexture(session.archive,name,image,diagnostic)) {
                native.width=image.width;native.height=image.height;native.rgba=std::move(image.rgba);
                native.ps2Alpha=std::move(image.ps2Alpha);native.ps2FiveBitColor=image.ps2FiveBitColor;
                native.mipLevels=std::move(image.mipLevels);
                native.name=name;native.materialId=int32_t(material);native.source="exact native dictionary/raster";
            }
            found=namedWorldTextures.emplace(name,std::move(native)).first;
        }
        if(found->second.rgba.empty())return 0;
        source=&found->second;
        auto handle=namedWorldHandles.find(name);
        if(handle==namedWorldHandles.end())handle=namedWorldHandles.emplace(name,
            uploadSurfaceTexture(*source)).first;
        return handle->second;
    }

    void captureVehicleEnvironment(StorylandLinuxSession& session,
        StorylandVehicleEnvironmentState& state, const std::vector<AreaFrame>& frame,
        const std::vector<size_t>& offsets, const std::vector<uint8_t>& selected,
        const StorylandNativePoint& eye, float farClip) {
        vehicleEnvironment=&state;
        glGetFloatv(GL_MODELVIEW_MATRIX,state.cameraView.data());
        if(!state.enabled) {
            state.ready=false;state.completedFaces=0;state.nextCaptureFace=0;
            return;
        }
        if(!storylandVehicleEnvironmentHasConsumers(gPortableShaderProfile,state.viewerOpen,
            !session.vehicleAssets.empty(),!session.weaponAssets.empty()))return;
        if(state.texture!=vehicleEnvironmentTarget.texture) {
            state.ready=false;state.completedFaces=0;state.nextCaptureFace=0;
        }
        const bool progressive=session.isPlayingGame()&&!state.viewerOpen;
        const auto elapsed=std::chrono::steady_clock::now().time_since_epoch();
        const uint64_t nowMs=uint64_t(std::chrono::duration_cast<
            std::chrono::milliseconds>(elapsed).count());
        if(!storylandVehicleEnvironmentProbeMoved(state,{eye.x,eye.y,eye.z},nowMs,
            session.isPlayingGame(),progressive)||
            !storylandVehicleEnvironmentCaptureDue(state,nowMs,session.isPlayingGame(),progressive))return;
        const unsigned firstFace=progressive?state.nextCaptureFace:0u;
        const unsigned endFace=progressive?firstFace+1u:6u;
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        GLint oldProgram=0, oldActive=0, oldClient=0;
        glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&oldActive);
        glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE,&oldClient);
        GLint textureUnits=1;
        glGetIntegerv(0x84E2,&textureUnits);
        for(GLint unit=0;unit<textureUnits;++unit) {
            glActiveTexture(GL_TEXTURE0+unit);
            glDisable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D,0);
#ifndef __ANDROID__
            glDisable(GL_TEXTURE_1D);glDisable(GL_TEXTURE_3D);glDisable(GL_TEXTURE_CUBE_MAP);
#endif
        }
        glActiveTexture(GL_TEXTURE0);glClientActiveTexture(GL_TEXTURE0);
        vehicleEnvironmentTarget.api={glGenFramebuffers,glBindFramebuffer,glFramebufferTexture2D,
            glCheckFramebufferStatus,glDeleteFramebuffers,glGenRenderbuffers,glBindRenderbuffer,
            glRenderbufferStorage,glFramebufferRenderbuffer,glDeleteRenderbuffers};
        if(!vehicleEnvironmentTarget.begin(state.faceSize,state.diagnostic)) {
            glClientActiveTexture(GLenum(oldClient));glActiveTexture(GLenum(oldActive));
            glPopClientAttrib();glPopAttrib();return;
        }
        if(!progressive||firstFace==0u) {
            state.position={eye.x,eye.y,eye.z};
            state.renderedTriangles=0;
        }
        state.faceSize=vehicleEnvironmentTarget.faceSize;
        const auto projection=storylandEnvironmentProjection(std::min(farClip,110.0f));
        glUseProgram(0);
        glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_CULL_FACE);
#ifndef __ANDROID__
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
#endif
        glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glViewport(0,0,state.faceSize*3,state.faceSize*2);
        glClearColor(sky.state().fog.r,sky.state().fog.g,sky.state().fog.b,1);
        if(!progressive)glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glMultMatrixf(projection.data());
        glMatrixMode(GL_MODELVIEW);glPushMatrix();
        for(unsigned face=firstFace;face<endFace;++face) {
            if(progressive) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(int(face%3)*state.faceSize,int(face/3)*state.faceSize,
                    state.faceSize,state.faceSize);
                glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                glDisable(GL_SCISSOR_TEST);
            }
            const auto view=storylandEnvironmentFaceView(face,state.position);
            glViewport(int(face%3)*state.faceSize,int(face/3)*state.faceSize,state.faceSize,state.faceSize);
            glLoadIdentity();glMultMatrixf(view.data());
            sky.drawBackground({},100.0f,true);
            glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_FOG);
            glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
            std::array<double,16> projectionDouble{}, viewDouble{};
            std::copy(projection.begin(),projection.end(),projectionDouble.begin());
            std::copy(view.begin(),view.end(),viewDouble.begin());
            StorylandViewFrustum frustum;frustum.set(projectionDouble.data(),viewDouble.data());
            for(unsigned phase=0;phase<2;++phase) for(size_t area=0;area<session.areas.size();++area) {
                const auto& browser=*session.areas[area];
                for(size_t index=0;index<browser.placements().size();++index) {
                    if(offsets[area]+index>=selected.size()||!selected[offsets[area]+index])continue;
                    const auto& placement=browser.placements()[index];
                    if(!storylandVehicleEnvironmentInsideProbe(state.position,
                        placement.x,placement.y,placement.z))continue;
                    if(placement.passName=="LIGHTS")continue;
                    const bool transparent=placement.passName=="TRANSPARENT"||placement.passName=="NOZWRITE";
                    if(transparent!=(phase==1))continue;
                    const auto found=frame[area].findMesh(placement);
                    const auto* mesh=browser.placementModelSwap(index);
                    if(!mesh&&found)mesh=found->draw;
                    if(!mesh||!frustum.visible(mesh->bounds,placement.matrix))continue;
                    glPushMatrix();glMultMatrixf(placement.matrix);
                    state.renderedTriangles+=storylandDrawVehicleEnvironmentMesh(*mesh,placement,
                        [&](uint32_t material) {
                            const auto* source=frame[area].resolveTexture(int32_t(material),*mesh,placement);
                            GLuint texture=worldTexture(session,area,source);
                            if(!source)texture=namedWorldTexture(session,browser,material,source);
                            return texture;
                        }, [&](uint32_t material) {
                            const auto* source = frame[area].resolveTexture(int32_t(material), *mesh, placement);
                            const double seconds = session.active() ?
                                double(session.environment.gameMilliseconds) * 0.001 : double(SDL_GetTicks64()) * 0.001;
                            return storylandWorldUvOffset(source, seconds);
                        });
                    glPopMatrix();
                }
            }
        }
        glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
        glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(GL_MODELVIEW);
        vehicleEnvironmentTarget.end();
        glUseProgram(GLuint(oldProgram));
        glClientActiveTexture(GLenum(oldClient));glActiveTexture(GLenum(oldActive));
        glPopClientAttrib();glPopAttrib();
        state.texture=vehicleEnvironmentTarget.texture;
        state.lastCaptureTimeMs=nowMs;
        state.nextCaptureFace=progressive?endFace%6u:0u;
        state.completedFaces=progressive?std::min(6u,state.completedFaces+1u):6u;
        state.ready=state.completedFaces==6u;
        if(!progressive||state.nextCaptureFace==0u)++state.captures;
    }

    void capturePspEnvironment(const std::array<float,3>& cameraForward) {
        pspEnvironmentReady=false;
        if(gPortableShaderProfile!=3)return;
        const auto& corona=sky.nativeFlareRaster(0);
        if(!corona.valid()||corona.width!=64||corona.height!=64)return;
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT,viewport);
        if(viewport[2]<=0||viewport[3]<=0)return;
        pspFramebufferPixels.resize(size_t(viewport[2])*size_t(viewport[3])*4u);
        GLint packAlignment=0,packRowLength=0,packBuffer=0;
        glGetIntegerv(GL_PACK_ALIGNMENT,&packAlignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH,&packRowLength);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&packBuffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
        glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);
        glReadPixels(viewport[0],viewport[1],viewport[2],viewport[3],GL_RGBA,GL_UNSIGNED_BYTE,
            pspFramebufferPixels.data());
        glPixelStorei(GL_PACK_ALIGNMENT,packAlignment);glPixelStorei(GL_PACK_ROW_LENGTH,packRowLength);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,GLuint(packBuffer));
        const StorylandPspEnvironmentImageView framebuffer{pspFramebufferPixels.data(),
            viewport[2],viewport[3],size_t(viewport[2])*4u,true,false};
        const StorylandPspEnvironmentImageView coronastar{corona.rgba.data(),64,64,64u*4u,false,false};
        StorylandPspEnvironmentPixels pixels{};
        std::string error;
        if(!storylandPspComposeEnvironment(framebuffer,coronastar,
            storylandPspEnvironmentHeading(cameraForward),pixels,error))return;
        GLint activeTexture=0,boundTexture=0;
        glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&boundTexture);
        if(!pspEnvironmentTexture) {
            pspEnvironmentTexture=uploadTexture(64,64,pixels.data(),pixels.size());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        } else {
            glBindTexture(GL_TEXTURE_2D,pspEnvironmentTexture);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        }
        glBindTexture(GL_TEXTURE_2D,GLuint(boundTexture));glActiveTexture(GLenum(activeTexture));
        pspEnvironmentReady=pspEnvironmentTexture!=0;
    }

    void capturePs2Environment(const std::array<float,3>& cameraForward) {
        if(gPortableShaderProfile!=0&&gPortableShaderProfile!=2)return;
        ps2EnvironmentReady=false;
        const auto& corona=sky.nativeFlareRaster(0);
        if(!corona.valid()) {
            ps2EnvironmentDiagnostic="The original coronastar raster is unavailable.";
            return;
        }
        const auto coronaPixels=storylandPs2UploadPixels(corona.rgba.data(),corona.rgba.size(),
            corona.ps2Alpha.data(),corona.ps2Alpha.size(),corona.ps2FiveBitColor);
        const StorylandPs2EnvironmentImageView coronastar{coronaPixels.data(),
            int(corona.width),int(corona.height),size_t(corona.width)*4u,false,false};
#ifndef __ANDROID__
        if(ps2EnvironmentRenderer.capture(SDL_GL_GetCurrentContext(),SDL_GL_GetProcAddress,
            coronastar,storylandPs2EnvironmentHeading(cameraForward),ps2EnvironmentTexture,
            ps2EnvironmentDiagnostic)) {
            ps2EnvironmentReady=true;
            return;
        }
#endif
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT,viewport);
        if(viewport[2]<=0||viewport[3]<=0||uint64_t(viewport[2])*uint64_t(viewport[3])>67108864u) {
            ps2EnvironmentDiagnostic="The environment source viewport is invalid.";
            return;
        }
        pspFramebufferPixels.resize(size_t(viewport[2])*size_t(viewport[3])*4u);
        GLint packAlignment=0,packRowLength=0,packBuffer=0;
        GLint packSkipPixels=0,packSkipRows=0;
        glGetIntegerv(GL_PACK_ALIGNMENT,&packAlignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH,&packRowLength);
        glGetIntegerv(GL_PACK_SKIP_PIXELS,&packSkipPixels);
        glGetIntegerv(GL_PACK_SKIP_ROWS,&packSkipRows);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&packBuffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,0);
        glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);
        glPixelStorei(GL_PACK_SKIP_PIXELS,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);
        glReadPixels(viewport[0],viewport[1],viewport[2],viewport[3],GL_RGBA,GL_UNSIGNED_BYTE,
            pspFramebufferPixels.data());
        const GLenum readError=glGetError();
        glPixelStorei(GL_PACK_ALIGNMENT,packAlignment);glPixelStorei(GL_PACK_ROW_LENGTH,packRowLength);
        glPixelStorei(GL_PACK_SKIP_PIXELS,packSkipPixels);glPixelStorei(GL_PACK_SKIP_ROWS,packSkipRows);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,GLuint(packBuffer));
        if(readError!=GL_NO_ERROR) {
            ps2EnvironmentDiagnostic="The environment framebuffer read failed.";
            return;
        }
        const StorylandPs2EnvironmentImageView framebuffer{pspFramebufferPixels.data(),
            viewport[2],viewport[3],size_t(viewport[2])*4u,true,false};
        StorylandPs2EnvironmentPixels pixels{};
        if(!storylandPs2ComposeEnvironment(framebuffer,coronastar,
            storylandPs2EnvironmentHeading(cameraForward),pixels,ps2EnvironmentDiagnostic))return;
        GLint activeTexture=0,boundTexture=0,unpackAlignment=0,unpackRowLength=0,unpackBuffer=0;
        GLint unpackSkipPixels=0,unpackSkipRows=0;
        glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&boundTexture);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpackAlignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&unpackRowLength);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&unpackSkipPixels);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS,&unpackSkipRows);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
        if(!ps2EnvironmentTexture) {
            ps2EnvironmentTexture=uploadTexture(128,128,pixels.data(),pixels.size());
        } else {
            glBindTexture(GL_TEXTURE_2D,ps2EnvironmentTexture);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,128,128,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        }
        const GLenum uploadError=glGetError();
        glPixelStorei(GL_UNPACK_ALIGNMENT,unpackAlignment);glPixelStorei(GL_UNPACK_ROW_LENGTH,unpackRowLength);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,unpackSkipPixels);glPixelStorei(GL_UNPACK_SKIP_ROWS,unpackSkipRows);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(unpackBuffer));
        glBindTexture(GL_TEXTURE_2D,GLuint(boundTexture));glActiveTexture(GLenum(activeTexture));
        ps2EnvironmentReady=ps2EnvironmentTexture!=0&&uploadError==GL_NO_ERROR;
        if(!ps2EnvironmentReady)ps2EnvironmentDiagnostic="The environment texture upload failed.";
    }

    void applyPspLocalLights(GLuint program,const std::array<float,3>* entityPosition=nullptr) {
        if(gPortableShaderProfile!=3)return;
        float modelView[16]{};
        glGetFloatv(GL_MODELVIEW_MATRIX,modelView);
        const auto position=entityPosition?*entityPosition:storylandPspEntityPosition(pspView.data(),modelView);
        const auto lights=storylandPspSelectLocalLights(pspPointLights,position,sky.state().spriteBrightness,
            pspExtraColourActive?&pspExtraDirection:nullptr);
        const char* directionNames[]{"uPspExtraLightEye0","uPspExtraLightEye1"};
        const char* colorNames[]{"uPspExtraLightColor0","uPspExtraLightColor1"};
        for(size_t index=0;index<lights.lights.size();++index) {
            std::array<float,3> direction{};
            for(size_t axis=0;axis<3;++axis)
                direction[axis]=pspView[axis]*lights.lights[index].direction[0]+
                    pspView[4+axis]*lights.lights[index].direction[1]+
                    pspView[8+axis]*lights.lights[index].direction[2];
            glUniform3fv(glGetUniformLocation(program,directionNames[index]),1,direction.data());
            glUniform3fv(glGetUniformLocation(program,colorNames[index]),1,lights.lights[index].color.data());
        }
    }

    void applyPs2LocalLights(GLuint program,const std::array<float,3>* entityPosition=nullptr,uint8_t alpha=255) {
        if(gPortableShaderProfile!=0&&gPortableShaderProfile!=2)return;
        float modelView[16]{};
        glGetFloatv(GL_MODELVIEW_MATRIX,modelView);
        const auto position=entityPosition?*entityPosition:storylandPs2EntityPosition(pspView.data(),modelView);
        const auto lights=storylandPs2SelectLocalLights(ps2PointLights,position);
        const char* directionNames[]{"uPs2ExtraLightEye0","uPs2ExtraLightEye1","uPs2ExtraLightEye2"};
        const char* colorNames[]{"uPs2ExtraLightColor0","uPs2ExtraLightColor1","uPs2ExtraLightColor2"};
        for(size_t index=0;index<lights.colors.size();++index) {
            const auto direction=storylandPs2EyeDirection(pspView.data(),lights.directions[index]);
            const auto color=storylandPs2LocalLightBytes(lights.colors[index]);
            glUniform3fv(glGetUniformLocation(program,directionNames[index]),1,direction.data());
            glUniform3fv(glGetUniformLocation(program,colorNames[index]),1,color.data());
        }
        const auto& state=sky.state();
        const auto selected=storylandPs2SelectLighting(
            {state.ambient.r,state.ambient.g,state.ambient.b},
            {state.ambientBlur.r,state.ambientBlur.g,state.ambientBlur.b},
            {state.objectAmbient.r,state.objectAmbient.g,state.objectAmbient.b},
            {state.objectAmbientBlur.r,state.objectAmbientBlur.g,state.objectAmbientBlur.b},
            {state.directional.r,state.directional.g,state.directional.b});
        auto lighting=storylandPs2EntityLighting(gPortableShaderProfile==2?
            StorylandPs2LightingGame::ViceCityStories:StorylandPs2LightingGame::LibertyCityStories,
            {state.ambient.r,state.ambient.g,state.ambient.b},selected,lights.ambientMultiplier);
        lighting.world.multiplier[3]=float(alpha)*storylandPs2FloatConstant(0x3B808081u);
        glUniform4fv(glGetUniformLocation(program,"uPs2WorldMultiplier"),1,lighting.world.multiplier.data());
        glUniform3fv(glGetUniformLocation(program,"uPs2ActorAmbient"),1,lighting.actorAmbient.data());
        glUniform3fv(glGetUniformLocation(program,"uPs2DirectionalColor"),1,lighting.actorDirectional.data());
    }

    void actor(const StorylandPlayTestAsset& asset, const std::vector<StorylandModelPoint>& pose,
        FrameActorTextures& textures, const std::vector<std::array<uint8_t,3>>& palette,
        uint8_t alpha = 255, const StorylandRuntimeVehicle* vehicle = nullptr,
        const std::array<uint32_t,6>* clothingColors = nullptr,
        StorylandMeshCategory category=StorylandMeshCategory::World,
        const StorylandModelFile* selectedModel = nullptr,
        const std::array<float,3>* entityPosition=nullptr,bool vehicleComponent=false) {
        const auto& model = selectedModel ? *selectedModel : asset.model;
        const auto& nativeMaterialColors = model.previewMaterialColors();
        const auto& materialEffects = model.previewMaterialEffects();
        const auto& materialEnvironments=model.previewMaterialEnvironmentNames();
        const auto& uvAnimations=model.previewMaterialUVAnimations();
        const auto& rslUVAnimations=model.previewRslUVAnimations();
        const auto& points = pose.size() == model.previewPoints().size() ? pose : model.previewPoints();
        if (points.empty() || model.previewTriangles().empty() || alpha == 0) return;
        const auto kind = vehicle || model.modelKind() == StorylandModelKind::VehicleModel ?
            StorylandShaderKind::Vehicle : (model.modelKind() == StorylandModelKind::PedModel ||
            model.modelKind() == StorylandModelKind::CutsceneModel) ? StorylandShaderKind::Pedestrian :
            StorylandShaderKind::Generic;
        const bool budokaiReflection=gPortableShaderProfile==2&&(!vehicle||!vehicle->charred)&&
            (kind==StorylandShaderKind::Vehicle||vehicleComponent||category==StorylandMeshCategory::Weapons);
        if(category==StorylandMeshCategory::World&&kind==StorylandShaderKind::Pedestrian)
            category=StorylandMeshCategory::Characters;
        if(vehicle)category=StorylandMeshCategory::None;
        if(!vehicle&&!(meshEffects&&meshEffects->enabled(category))) {
            GLfloat projection[16]{},modelView[16]{};
            glGetFloatv(GL_PROJECTION_MATRIX,projection);
            glGetFloatv(GL_MODELVIEW_MATRIX,modelView);
            if(storylandActorOutsideClip(points,projection,modelView))return;
        }
        glPushMatrix();
        if(meshEffects&&meshEffects->enabled(category))
            deform(category,storylandMeshEffectBounds(model.previewPoints()));
        const GLuint program = programs[size_t(kind)];
        setProgram(program, sky.state(), kind == StorylandShaderKind::Vehicle, &lightEye,&upEye,rain);
        glUniform1i(glGetUniformLocation(program,"uPs2BudokaiReflection"),budokaiReflection?1:0);
        applyPspLocalLights(program,entityPosition);
        applyPs2LocalLights(program,entityPosition,alpha);
        glUniform1f(glGetUniformLocation(program,"uPspEnvironmentStrength"),pspEnvironmentStrength);
        if(vehicleEnvironment) {
            const auto basis=storylandEnvironmentEyeToWorld(vehicleEnvironment->cameraView.data());
            const char* names[]{"uEnvironmentViewRight","uEnvironmentViewUp","uEnvironmentViewBack"};
            for(size_t axis=0;axis<3;++axis)
                glUniform3fv(glGetUniformLocation(program,names[axis]),1,basis[axis].data());
            glUniform1f(glGetUniformLocation(program,"uEnvironmentFaceSize"),float(vehicleEnvironment->faceSize));
        }
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(gPortableShaderProfile==3?GL_GEQUAL:GL_GREATER,
            gPortableShaderProfile==3?float(pspActorAlphaReference)/255.0f:.02f);
        float projection[16]{};
        glGetFloatv(GL_PROJECTION_MATRIX,projection);
        const float pspModelSlopeScale=std::abs(projection[0])>0.000001f?
            0.6f/(512.0f*std::abs(projection[0])):0.0008f;
        std::vector<GLuint> materialTextures(model.previewMaterialTextureNames().size());
        std::vector<const RgbaImage*> materialImages(materialTextures.size(),nullptr);
        for (size_t material = 0; material < materialTextures.size(); ++material) {
            const auto name = rasterName(storylandMaterialTextureName(model, material));
            size_t image = SIZE_MAX;
            for (size_t i = 0; i < asset.textureNames.size() && i < asset.images.size(); ++i)
                if (rasterName(asset.textureNames[i]) == name) {image = i; break;}
            if (image == SIZE_MAX && (name.empty() || name.front() == '<' ||
                name.rfind("texture", 0) == 0 || name.rfind("material", 0) == 0) &&
                material < asset.images.size()) image = material;
            if (image != SIZE_MAX) {
                materialImages[material] = &asset.images[image];
                materialTextures[material] = textures.get(asset.images[image]);
            }
        }
        std::vector<GLuint> materialEnvironmentsTextures(materialTextures.size(),0);
        std::vector<const RgbaImage*> materialEnvironmentImages(materialTextures.size(),nullptr);
        for(size_t material=0;material<materialEnvironmentsTextures.size()&&material<materialEnvironments.size();++material) {
            const std::string name=rasterName(materialEnvironments[material]);
            if(name.empty())continue;
            for(size_t image=0;image<asset.textureNames.size()&&image<asset.images.size();++image)
                if(rasterName(asset.textureNames[image])==name) {
                    materialEnvironmentImages[material]=&asset.images[image];
                    materialEnvironmentsTextures[material]=textures.get(asset.images[image]);break;
                }
        }
        std::set<std::string> damagedParts;
        for (const auto& triangle : model.previewTriangles())
            if (triangle.damagedPart) damagedParts.insert(triangle.vehiclePart);
        const auto& definition = vehicle ? vehicle->nativeDefinition : asset.vehicleDefinition;
        auto materialColor = [&](uint32_t material) {
            uint32_t color = !selectedModel && material < asset.materialColors.size() ? asset.materialColors[material] :
                material < nativeMaterialColors.size() ? nativeMaterialColors[material] : 0xFFFFFFFFu;
            if (clothingColors) {
                color = storylandNativePedMaterialColor(color,*clothingColors);
            }
            int paint = material < definition.materialPaintSlots.size() ? definition.materialPaintSlots[material] : -1;
            if (paint < 0 && material >= definition.materialPaintSlots.size()) {
                const auto rgb = color & 0xFFFFFFu;
                if (rgb == 0x0000FF3Cu || rgb == 0x00FF00FFu) paint = 0;
                if (rgb == 0x00AF00FFu || rgb == 0x0000FFFFu) paint = 1;
            }
            if (vehicle && paint >= 0) {
                const int selected = paint == 0 ? vehicle->primaryColor : vehicle->secondaryColor;
                std::array<uint8_t,3> rgb{};
                bool valid = false;
                if (definition.vcsModelLayout && selected >= 240 && selected < 256) {
                    rgb = definition.customPaintColors[size_t(selected-240)]; valid = true;
                } else if (selected >= 0 && size_t(selected) < palette.size()) {
                    rgb = palette[size_t(selected)]; valid = true;
                }
                if (valid) color = (color & 0xFF000000u) | uint32_t(rgb[0]) |
                    (uint32_t(rgb[1])<<8) | (uint32_t(rgb[2])<<16);
            }
            if(vehicle&&vehicle->charred)color=(color&0xFF000000u)|0x00303030u;
            return color;
        };
        const bool spatialEffects=meshEffects&&meshEffects->spatial(category);
        const auto spatialBounds=spatialEffects?storylandMeshEffectBounds(points):std::array<float,6>{};
        Batch batch;
        for (unsigned pass = 0; pass < 2; ++pass) {
            if (pass) {glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);}
            else glDisable(GL_BLEND);
            glDepthMask(pass ? GL_FALSE : GL_TRUE);
            uint32_t lastMaterial = UINT32_MAX - 1;
            float lastPspSlope = -2.0f;
            bool lastPspPrelit = false;
            std::array<float,4> color{1,1,1,1};
            for (const auto& triangle : model.previewTriangles()) {
                if (triangle.a >= points.size() || triangle.b >= points.size() || triangle.c >= points.size()) continue;
                bool finite = true;
                for (auto index : {triangle.a, triangle.b, triangle.c}) {
                    const auto& point = points[index];
                    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) finite = false;
                }
                if (!finite) continue;
                if (vehicle) {
                    if (vehicle->explicitComponents &&
                        !storylandVehicleExtraVisible(triangle.vehicleExtraIndex, vehicle->components)) continue;
                    if (vehicle->charred && triangle.vehicleWheel) continue;
                    if (!storylandVehiclePartVisible(triangle.vehiclePart, triangle.damagedPart,
                        damagedParts.count(triangle.vehiclePart) != 0, vehicle->visiblyDamaged,
                        vehicle->doorDamage, &vehicle->panelDamage)) continue;
                    if (asset.sharedWheel && definition.hasNativeSuspension && triangle.vehicleWheel &&
                        definition.domain != StorylandVehicleDomain::Water) continue;
                } else if (triangle.damagedPart) continue;
                const auto rgba = materialColor(triangle.materialIndex);
                if(vehicle&&vehicle->charred&&uint8_t(rgba>>24)<255)continue;
                const float opacity = float(uint8_t(rgba >> 24)) / 255 * float(alpha) / 255;
                if ((opacity < .999f) != (pass == 1) || opacity <= 0) continue;
                const bool pspPrelit=triangle.a<model.previewPoints().size()&&
                    !model.previewPoints()[triangle.a].hasNormal;
                if (triangle.materialIndex != lastMaterial ||
                    (gPortableShaderProfile==3&&(triangle.pspTextureLodSlope!=lastPspSlope||pspPrelit!=lastPspPrelit))||
                    ((gPortableShaderProfile==0||gPortableShaderProfile==2)&&pspPrelit!=lastPspPrelit)) {
                    batch.flush(); lastMaterial = triangle.materialIndex;
                    lastPspSlope=triangle.pspTextureLodSlope;
                    lastPspPrelit=pspPrelit;
                    if(gPortableShaderProfile==0||gPortableShaderProfile==2)
                        glUniform1i(glGetUniformLocation(program,"uPs2PrelitModel"),pspPrelit?1:0);
                    bindTexture(program, vehicle&&vehicle->charred&&charredVehicleTexture?charredVehicleTexture:
                        lastMaterial < materialTextures.size() ? materialTextures[lastMaterial] : 0, whiteTexture,
                        vehicle&&vehicle->charred?nullptr:
                            lastMaterial<materialImages.size()?materialImages[lastMaterial]:nullptr);
                    if(gPortableShaderProfile==3) {
                        glUniform1i(glGetUniformLocation(program,"uPspPrelitModel"),pspPrelit?1:0);
                        const auto* image=lastMaterial<materialImages.size()?materialImages[lastMaterial]:nullptr;
                        const bool slope=lastPspSlope>=0.0f&&image;
                        glUniform1i(glGetUniformLocation(program,"uPspTextureLodMode"),slope?2:0);
                        glUniform1f(glGetUniformLocation(program,"uPspTextureLodSlope"),
                            slope?lastPspSlope*float(image->width)*pspModelSlopeScale:1.0f);
                    }
                    const float materialEffect=(!vehicle || !vehicle->charred) && lastMaterial<materialEffects.size() ?
                        materialEffects[lastMaterial] : 0.0f;
                    glUniform1f(glGetUniformLocation(program,"uNativeMaterialEffect"),materialEffect);
                    const auto reflection=storylandSelectVehicleReflectionMaterial(gPortableShaderProfile,
                        kind==StorylandShaderKind::Vehicle||vehicleComponent,
                        category==StorylandMeshCategory::Weapons,vehicle&&vehicle->charred,materialEffect,
                        lastMaterial<materialEnvironmentsTextures.size()?materialEnvironmentsTextures[lastMaterial]:0,
                        ps2EnvironmentReady?ps2EnvironmentTexture:0,
                        pspEnvironmentReady?pspEnvironmentTexture:0,vehicleEnvironment);
                    const GLuint environmentTexture=reflection.texture;
                    const bool pspEnvironment=gPortableShaderProfile==3&&materialEffect>0.0f&&pspEnvironmentReady;
                    const bool nativeEnvironment=environmentTexture!=0&&materialEffect>0.0f;
                    if(gPortableShaderProfile==3) {
                        const auto* environmentImage=lastMaterial<materialEnvironmentImages.size()?
                            materialEnvironmentImages[lastMaterial]:nullptr;
                        glUniform2f(glGetUniformLocation(program,"uPspEnvironmentSize"),
                            pspEnvironment?64.0f:environmentImage?float(environmentImage->width):0.0f,
                            pspEnvironment?64.0f:environmentImage?float(environmentImage->height):0.0f);
                    }
                    glUniform1i(glGetUniformLocation(program,"uDualTextureMode"),reflection.mode);
                    glUniform1f(glGetUniformLocation(program,"uDualTextureStrength"),materialEffect);
                    if(gPortableShaderProfile==0||gPortableShaderProfile==2)
                        glUniform1f(glGetUniformLocation(program,"uPs2EnvironmentCoefficient"),
                            float(storylandPs2EnvironmentCoefficient(materialEffect,
                                1.0f,
                                gPortableShaderProfile==2?StorylandPs2LightingGame::ViceCityStories:
                                    StorylandPs2LightingGame::LibertyCityStories)));
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D,nativeEnvironment?environmentTexture:0);
                    glActiveTexture(GL_TEXTURE0);
                    const auto materialName=lastMaterial<model.previewMaterialTextureNames().size()?
                        model.previewMaterialTextureNames()[lastMaterial]:std::string();
                    glUniform1f(glGetUniformLocation(program,"uEmissiveStrength"),
                        storylandAeroMaterialHasAuthoredGlow(materialName)?.85f:0.0f);
                    color = {float(uint8_t(rgba))/255, float(uint8_t(rgba>>8))/255,
                        float(uint8_t(rgba>>16))/255, opacity};
                }
                for (auto index : {triangle.a, triangle.b, triangle.c}) {
                    auto point = points[index];
                    if (vehicle) {
                        if (triangle.vehicleWheel) {
                            point = storylandAnimateVehicleWheel(point, triangle.vehiclePivot,
                                triangle.vehicleFrontWheel ? vehicle->wheelRotation : vehicle->rearWheelRotation,
                                StorylandVcsRetailQuirks::wheelSteerDegrees(definition.vcsModelLayout,
                                    definition.handlingFlags,triangle.vehicleFrontWheel,
                                    vehicle->steeringAngle,vehicle->handbrake));
                            const bool left = triangle.vehiclePivot.x < definition.previewRoot[12];
                            point = storylandAnimateBurstTyre(point, triangle.vehiclePivot,
                                vehicle->tyreStatus[storylandVehicleTyreIndex(definition.twoWheels(), triangle.vehicleFrontWheel, left)]);
                            for (size_t wheel = 0; wheel < definition.wheels.size() && wheel < 4; ++wheel) {
                                const auto& pivot = definition.wheels[wheel].previewPivot;
                                if (std::abs(pivot[0]-triangle.vehiclePivot.x)+std::abs(pivot[1]-triangle.vehiclePivot.y)+
                                    std::abs(pivot[2]-triangle.vehiclePivot.z) < .0001f)
                                    point.z += vehicle->wheelPositions[wheel] - definition.wheels[wheel].nativeZ;
                            }
                        }
                        point = storylandAnimateVehiclePanel(point, triangle.vehiclePivot, triangle.vehiclePart,
                            vehicle->doorOpenRatios, vehicle->bonnetOpenRatio, vehicle->bootOpenRatio);
                        if (definition.hasNativeForklift)
                            point = storylandAnimateForklift(point, triangle.vehiclePart,
                                definition.forkliftPart, vehicle->forklift);
                        point = storylandAnimateAircraftRotor(point, triangle.vehiclePivot,
                            triangle.vehiclePart, vehicle->rotorAngle);
                    }
                    if(spatialEffects)point=storylandMeshEffectVertex(
                        *meshEffects,category,spatialBounds,meshTime,0,point);
                    const auto uv = index < model.previewTexcoords().size() ? model.previewTexcoords()[index] : StorylandModelTexcoord{};
                    float animatedU=std::isfinite(uv.u)?uv.u:0.0f;
                    float animatedV=std::isfinite(uv.v)?uv.v:0.0f;
                    if(triangle.materialIndex<rslUVAnimations.size() &&
                       rslUVAnimations[triangle.materialIndex].animated()) {
                        const auto animated=rslUVAnimations[triangle.materialIndex].transform(
                            animatedU,animatedV,meshTime);
                        animatedU=animated[0];animatedV=animated[1];
                    } else if(triangle.materialIndex<uvAnimations.size()) {
                        const auto animated=uvAnimations[triangle.materialIndex].transform(
                            animatedU,animatedV,meshTime);
                        animatedU=animated[0];animatedV=animated[1];
                    }
                    auto vertexColor=color;
                    if(gPortableShaderProfile==3&&index<model.previewPrelights().size()&&
                        model.previewPrelights()[index].valid) {
                        const auto& prelight=model.previewPrelights()[index];
                        vertexColor={float(prelight.red)/255.0f,float(prelight.green)/255.0f,
                            float(prelight.blue)/255.0f,float(prelight.alpha)/255.0f*float(alpha)/255.0f};
                    }
                    if((gPortableShaderProfile==0||gPortableShaderProfile==2)&&pspPrelit&&
                        index<model.previewPrelights().size()&&model.previewPrelights()[index].valid) {
                        const auto prelight=storylandPs2ModelPrelightColor(model.previewPrelights()[index]);
                        vertexColor={prelight[0]/255.0f,prelight[1]/255.0f,prelight[2]/255.0f,
                            prelight[3]/255.0f};
                    }
                    const bool nativeNormal=(gPortableShaderProfile==0||gPortableShaderProfile==2)&&point.hasPs2Normal;
                    batch.add({animatedU,animatedV,
                        vertexColor[0],vertexColor[1],vertexColor[2],vertexColor[3],
                        point.x,point.y,point.z,nativeNormal?point.ps2Nx:point.nx,
                        nativeNormal?point.ps2Ny:point.ny,nativeNormal?point.ps2Nz:point.nz});
                }
            }
            batch.flush();
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D,0);
        glActiveTexture(GL_TEXTURE0);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        if(showActorBounds&&(vehicle||category!=StorylandMeshCategory::None)) {
            glUseProgram(0);
            glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_DEPTH_BUFFER_BIT);
            glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glDepthMask(GL_FALSE);
            const auto bounds=storylandMeshEffectBounds(points);
            drawBoundingBox({bounds[0],bounds[1],bounds[2]},{bounds[3],bounds[4],bounds[5]});
            glPopAttrib();
        }
        glPopMatrix();
        ++renderedActors;
    }

    bool vehicle(StorylandLinuxSession& session, size_t slot, FrameActorTextures& textures,
        const std::vector<std::array<uint8_t,3>>& palette) {
        if (slot >= session.vehicles.size() || slot >= session.runtime.vehicles.size()) return false;
        const auto& state = session.runtime.vehicles[slot];
        if (state.scriptHandle >= 0) {
            const auto script = session.environment.missionVehicles.find(state.scriptHandle);
            if (script != session.environment.missionVehicles.end() && script->second.deleted) return false;
        } else if (session.environment.carDensityMultiplier <= 0 && state.generator<0&&state.savedGarage<0&&!state.coopControlled&&
            int(slot) != session.runtime.occupiedVehicle) return false;
        if (!state.visible || !state.clumpVisible || state.renderAlpha == 0 || state.model >= session.vehicleAssets.size()) return false;
        const bool drawn=vehicleModel(session.vehicleAssets[state.model],session.vehicles[slot],state,textures,palette);
        if(!drawn&&state.nativeDefinition.vcsJetSki())
            session.runtime.vehiclePhysicsError="Native VCS JetSki component pose is unavailable.";
        return drawn;
    }

    bool vehicleModel(const StorylandPlayTestAsset& asset, const StorylandNativeTrafficActor& traffic,
        const StorylandRuntimeVehicle& state, FrameActorTextures& textures,
        const std::vector<std::array<uint8_t,3>>& palette) {
        const auto& definition = state.nativeDefinition;
        const auto* vehiclePoints=&asset.model.previewPoints();
        std::vector<StorylandModelPoint> componentPoints;
        if(definition.vcsJetSki()) {
            if(!storylandVcsJetSkiChassisPose(definition.vcsJetSkiChassis,
                storylandVcsJetSkiNativeAngularZ(state),*vehiclePoints,componentPoints))return false;
            vehiclePoints=&componentPoints;
        }
        const auto matrix = storylandVehicleRenderFrame(definition,
            {traffic.position.x,traffic.position.y,traffic.position.z}, traffic.yaw,
            minimumZ(asset), state.suspension, state.bodyRoll-state.roadRoll,
            state.bodyPitch+state.roadPitch, traffic.water, state.craneCarried ? &state.craneFrame : nullptr,
            state.bikeVisualLean);
        glPushMatrix(); glMultMatrixf(matrix.data());
        if(meshEffects&&meshEffects->enabled(StorylandMeshCategory::Cars))
            deform(StorylandMeshCategory::Cars,storylandMeshEffectBounds(asset.model.previewPoints()),state.model);
        actor(asset,*vehiclePoints,textures,palette,state.renderAlpha,&state);
        if (!state.charred && asset.sharedWheel && definition.hasNativeSuspension && !traffic.water) {
            auto wheel = [&](const StorylandVehicleWheelDefinition& item, size_t contact) {
                if (contact >= state.wheelPositions.size()) return;
                auto attachment = item.previewFrame;
                attachment[14] += state.wheelPositions[contact]-definition.wheels[contact].nativeZ;
                glPushMatrix(); glMultMatrixf(attachment.data());
                glRotatef(StorylandVcsRetailQuirks::wheelSteerDegrees(definition.vcsModelLayout,
                    definition.handlingFlags,item.front,state.steeringAngle,state.handbrake),0,0,1);
                const bool left = item.previewPivot[0] < definition.previewRoot[12];
                if (left) glRotatef(180,0,0,1);
                glRotatef((left ? 1.f : -1.f)*(item.front ? state.wheelRotation : state.rearWheelRotation)*180/Pi,1,0,0);
                const size_t tyre = storylandVehicleTyreIndex(definition.twoWheels(),item.front,left);
                const float scale = item.front ? definition.wheelScale : definition.rearWheelScale;
                const float squash = state.tyreStatus[tyre] ? .7f : 1.f;
                glScalef(scale,scale*squash,scale*squash);
                actor(*asset.sharedWheel,asset.sharedWheel->model.previewPoints(),textures,palette,state.renderAlpha,
                    nullptr,nullptr,StorylandMeshCategory::None,nullptr,nullptr,true);
                glPopMatrix();
            };
            for (size_t index = 0; index < definition.wheels.size() && index < 4; ++index)
                wheel(definition.wheels[index],index);
            for (const auto& auxiliary : definition.auxiliaryWheels) {
                size_t nearest = SIZE_MAX; float distance = std::numeric_limits<float>::infinity();
                for (size_t i=0; i<definition.wheels.size() && i<4; ++i) {
                    if (definition.wheels[i].front) continue;
                    const float gap = std::hypot(auxiliary.previewPivot[0]-definition.wheels[i].previewPivot[0],
                        auxiliary.previewPivot[1]-definition.wheels[i].previewPivot[1]);
                    if (gap < distance) {distance = gap; nearest = i;}
                }
                if (nearest != SIZE_MAX) wheel(auxiliary,nearest);
            }
        }
        glPopMatrix();
        return true;
    }

    void heldWeapon(StorylandLinuxSession& session, const StorylandPlayTestAsset& character,
        const StorylandAnimationBlendState& blend, const StorylandNativePoint& position,
        float heading, size_t weapon, FrameActorTextures& textures,
        const std::vector<std::array<uint8_t,3>>& palette, uint8_t alpha,
        const std::array<float,16>* nativeRoot=nullptr,
        StorylandMuzzleOwner muzzleOwner=StorylandMuzzleOwner::None,int32_t muzzleHandle=-1) {
        if (weapon >= session.runtime.weapons.size()) return;
        const size_t visual = session.runtime.weapons[weapon].visual;
        if (visual >= session.weaponAssets.size()) return;
        std::array<float,16> frame;
        const bool attached=nativeRoot?storylandNativeRootWeaponAttachmentFrame(character.model.armatureBones(),
            blend.boneFrames,*nativeRoot,storylandPedModelForwardAngle(character.model),frame):
            storylandWeaponAttachmentFrame(character.model.armatureBones(),blend.boneFrames,
                character.model.previewPoints(),position,heading,storylandPedModelForwardAngle(character.model),frame);
        if(!attached)return;
        StorylandNativePoint muzzle{};
        const auto& definition=session.runtime.weapons[weapon];
        const bool muzzleValid=definition.fireOffsetValid&&
            storylandWeaponFirePoint(frame,definition.fireOffset,muzzle);
        if(muzzleOwner==StorylandMuzzleOwner::Player) {
            session.runtime.playerMuzzle=muzzle;session.runtime.playerMuzzleValid=muzzleValid;
        } else if(muzzleOwner==StorylandMuzzleOwner::AmbientPed&&muzzleHandle>=0&&
            size_t(muzzleHandle)<session.runtime.pedestrians.size()) {
            auto& ped=session.runtime.pedestrians[size_t(muzzleHandle)];
            ped.weaponMuzzle=muzzle;ped.weaponMuzzleValid=muzzleValid;
        } else if(muzzleOwner==StorylandMuzzleOwner::MissionPed) {
            const auto found=session.runtime.missionCombat.find(muzzleHandle);
            if(found!=session.runtime.missionCombat.end()) {
                found->second.muzzle=muzzle;found->second.muzzleValid=muzzleValid;
            }
        }
        const auto& asset = session.weaponAssets[visual];
        glPushMatrix();
        glMultMatrixf(frame.data());
        actor(asset, asset.model.previewPoints(), textures, palette, alpha,nullptr,nullptr,StorylandMeshCategory::Weapons);
        glPopMatrix();
    }

    void coopPlayer(StorylandLinuxSession& session, FrameActorTextures& textures,
        const std::vector<std::array<uint8_t,3>>& palette) {
        if (!session.coopNetwork().connected()) {
            remoteLife = 0;
            return;
        }
        const auto peer = session.coopDisplay();
        if (!peer.visible() || !session.playerLoaded || peer.area != session.environment.area) return;
        const StorylandNativePoint position{peer.x, peer.y, peer.z};
        const auto& eye = session.camera.eye;
        if (std::hypot(position.x - eye.x, position.y - eye.y, position.z - eye.z) > 180.f) return;
        if (peer.life != remoteLife) {
            remoteBlend = {};
            remoteLife = peer.life;
        }

        const auto* car = session.coopVehicleAsset(peer.vehicleModel);
        StorylandNativeTrafficActor traffic;
        traffic.position = {peer.carX, peer.carY, peer.carZ};
        traffic.yaw = peer.carHeading;
        traffic.water = (peer.carFlags & 1) != 0;
        if (car && (peer.carFlags & 64) && session.runtime.coopRemoteVehicle < 0) {
            StorylandRuntimeVehicle state;
            state.nativeDefinition = car->vehicleDefinition;
            state.primaryColor = peer.primaryColor;
            state.secondaryColor = peer.secondaryColor;
            state.wheelRotation = peer.wheel;
            state.rearWheelRotation = peer.rearWheel;
            state.steeringAngle = peer.steering;
            state.suspension = peer.suspension;
            state.bodyRoll = peer.roll;
            state.bodyPitch = peer.pitch;
            state.wheelPositions = {peer.wheelPosition0, peer.wheelPosition1, peer.wheelPosition2, peer.wheelPosition3};
            state.doorOpenRatios = {peer.door0, peer.door1, peer.door2, peer.door3};
            state.wrecked = state.charred = (peer.carFlags & 32) != 0;
            vehicleModel(*car, traffic, state, textures, palette);
        }

        auto& animation = session.playerAnimation;
        const auto* remoteCharacter = session.coopCharacterAsset();
        if (!remoteCharacter) return;
        const auto& player = *remoteCharacter;
        const auto& model = player.model;
        std::vector<StorylandModelPoint> points;
        std::string error;
        const auto weapon = std::find_if(session.runtime.weapons.begin(), session.runtime.weapons.end(),
            [&](const auto& item) { return item.nativeId == peer.weapon; });
        const bool weaponAvailable = weapon != session.runtime.weapons.end();
        const size_t weaponIndex = weaponAvailable ? size_t(weapon - session.runtime.weapons.begin()) : SIZE_MAX;
        if (remoteBlend.weaponIndex != weaponIndex) {
            remoteBlend.weaponIndex = weaponIndex;
            remoteBlend.weaponAimBlend = remoteBlend.weaponAimTime = 0;
            remoteBlend.weaponAimWasActive = false;
            remoteBlend.weaponAimIk = {};
        }
        remoteBlend.walkSpeed = session.runtime.walkSpeed;
        remoteBlend.runSpeed = session.runtime.runSpeed;
        remoteBlend.locomotionPhase = peer.phase;
        remoteBlend.distanceDrivenPhase = true;
        remoteBlend.movementForward = peer.moveForward;
        remoteBlend.movementRight = peer.moveRight;
        remoteBlend.measuredSpeed = std::hypot(peer.moveForward, peer.moveRight);
        remoteBlend.weaponAiming = weaponAvailable && weapon->fireType != 0 && peer.aiming();
        remoteBlend.weaponReloading = weaponAvailable && peer.reloading();
        remoteBlend.upperActive = weaponAvailable && (peer.actionFlags & 4) != 0;
        remoteBlend.weaponAnimationTime = -1;
        remoteBlend.weaponShotSerial = peer.shotSerial;
        remoteBlend.upperStartedAt = peer.animationTime - peer.actionTime;
        remoteBlend.weaponAimYaw=remoteBlend.weaponAimPitch=0;
        remoteBlend.weaponAimDirectionValid=false;
        if(remoteBlend.weaponAiming) {
            StorylandNativePoint shoulder=(peer.actionFlags&32)?
                StorylandNativePoint{peer.torsoX,peer.torsoY,peer.torsoZ}:
                StorylandNativePoint{peer.x,peer.y,peer.z+1.3f};
            StorylandNativePoint posedShoulder;
            if(storylandPoseBoneWorldPoint(model,remoteBlend.boneFrames,22,position,peer.heading,1.8f,posedShoulder))
                shoulder=posedShoulder;
            const float dx=peer.aimX-shoulder.x,dy=peer.aimY-shoulder.y,dz=peer.aimZ-shoulder.z;
            const float horizontal=std::hypot(dx,dy);
            if(std::isfinite(horizontal)&&std::isfinite(dz)&&horizontal>.0001f) {
                remoteBlend.weaponAimYaw=std::remainder(std::atan2(dy,dx)*57.2957795131f-peer.heading,360.0f);
                remoteBlend.weaponAimPitch=std::atan2(dz,horizontal)*57.2957795131f;
                remoteBlend.weaponAimDirectionValid=true;
            }
        }
        if (!weaponAvailable || !animation.selectWeaponAction(*weapon, peer.reloading() ? "reload" :
                remoteBlend.upperActive ? "fire" : "aim", remoteBlend.measuredSpeed > .15f,
                peer.shotSerial, -1, peer.aiming(), false)) {
            remoteBlend.weaponAiming = false;
            remoteBlend.weaponReloading = false;
            remoteBlend.upperActive = false;
            remoteBlend.weaponAimDirectionValid = false;
            if (!weaponAvailable) {
                remoteBlend.weaponAimBlend = remoteBlend.weaponAimTime = 0;
                remoteBlend.weaponAimWasActive = false;
                remoteBlend.weaponAimIk = {};
            }
        }

        bool seated = false;
        bool sampled = false;
        const auto motion = peer.busted() ? StorylandPlayTestMotion::Idle : StorylandPlayTestMotion(peer.motion);
        if(car&&car->vehicleDefinition.vcsJetSki()&&(peer.seated()||peer.vehicleAction!=0)) {
            session.runtime.vehiclePhysicsError=
                "Native VCS JetSki remote occupant axes and associations are absent from the network payload.";
            return;
        }
        if (car && peer.seated()) {
            auto options = animation.vehicleAnimationOptions(car->vehicleDefinition);
            options.boat = traffic.water;
            options.passenger = peer.seat >= 0;
            options.seatRelative = true;
            options.elapsedSeconds = peer.animationTime;
            options.transition = &remoteBlend.poseTransition;
            options.steering = peer.steering / std::max(1.f, car->vehicleDefinition.steeringLock);
            options.reversing = peer.speed < -.2f;
            seated = sampled = animation.sampleVehicleAction(model, "sit", 0, options, points, error);
        } else if (peer.dead()) {
            sampled = animation.sampleDeathAnimation(model, peer.animation, peer.scriptTime, points, error,
                &remoteBlend.boneFrames, true, &remoteBlend.poseTransition, peer.deathBlendRate, peer.deathSpeed);
        } else if (car && peer.vehicleAction != 0) {
            auto options = animation.vehicleAnimationOptions(car->vehicleDefinition);
            options.boat = traffic.water;
            options.elapsedSeconds = peer.animationTime;
            options.transition = &remoteBlend.poseTransition;
            const auto action = StorylandVehicleAction(peer.vehicleAction);
            const char* name = action == StorylandVehicleAction::Exit ? "exit" :
                action == StorylandVehicleAction::Hijack ? "hijack" : "enter";
            sampled = animation.sampleVehicleAction(model, name, peer.vehicleFraction, options, points, error);
        } else if (peer.animation >= 0) {
            sampled = animation.sampleScriptAnimation(model, peer.animation, peer.animationGroup,
                peer.scriptTime, 1, remoteBlend, points, error);
        }
        if (!sampled)
            sampled = animation.sampleTree(model, motion, peer.animationTime, remoteBlend, points, error);
        if (!sampled) return;

        glPushMatrix();
        if (seated) {
            const auto& definition = car->vehicleDefinition;
            const auto frame = storylandVehicleOccupantFrame(definition,
                {peer.carX, peer.carY, peer.carZ}, peer.carHeading, definition.previewMinimumZ,
                peer.suspension, peer.roll, peer.pitch, unsigned(peer.seat + 1), traffic.water);
            glMultMatrixf(frame.data());
            glRotatef(90 - storylandPedModelForwardAngle(model), 0, 0, 1);
        } else pedestrianTransform(player, position, peer.heading);
        actor(player, points, textures, palette);
        glPopMatrix();
        if (!peer.seated() && !peer.vehicleAction && !peer.dead() && !peer.busted())
            heldWeapon(session, player, remoteBlend, position, peer.heading, weaponIndex, textures, palette, 255);
    }
};

StorylandLinuxRenderer::StorylandLinuxRenderer()
    : frontendArtwork(std::make_unique<StorylandRetailHudAssets>()) {
    frontendArtwork->loadPcAssets(false);
}
StorylandLinuxRenderer::~StorylandLinuxRenderer() = default;
const std::string& StorylandLinuxRenderer::status() const { return statusMessage; }

const StorylandRetailHudAssets* StorylandLinuxRenderer::frontendAssets() const {
    return resources?&resources->hud:frontendArtwork.get();
}
bool StorylandLinuxRenderer::graphicsQualityApplies() const {
    const int profile=storylandShaderRenderProfile(selectedShader<0?nativeShaderProfile:selectedShader);
    return profile==0||profile==2||profile==4||storylandPortableAeroProfile(profile);
}
float StorylandLinuxRenderer::maximumAnisotropy() {
#ifndef __ANDROID__
    return gPortableTextureQuality.maximumAnisotropy(SDL_GL_GetCurrentContext());
#else
    return 1.0f;
#endif
}
std::string StorylandLinuxRenderer::fxaaStatus() const {
#ifndef __ANDROID__
    return resources?resources->fxaa.status():std::string{};
#else
    return "FXAA is unavailable on this renderer.";
#endif
}
double StorylandLinuxRenderer::ps2EnvironmentCaptureMilliseconds() const {
    return resources ? resources->ps2EnvironmentCaptureMilliseconds : 0.0;
}

void StorylandLinuxRenderer::shutdown() {
    storylandRetailSpriteTexture(nullptr, "");
    if(resources)frontendArtwork=std::make_unique<StorylandRetailHudAssets>(std::move(resources->hud));
    resources.reset();
#ifndef __ANDROID__
    gPortableTextureQuality.reset();
#endif
    vehicleEnvironment.ready=false;
    vehicleEnvironment.texture=0;
    vehicleEnvironment.completedFaces=0;
    vehicleEnvironment.nextCaptureFace=0;
    gPortableEmissionCache.clear();
    statusMessage.clear();
}

bool StorylandLinuxRenderer::initialize(StorylandLinuxSession& session, std::string& error) {
    shutdown(); error.clear();
    if (!glGetString(GL_VERSION)) {error = "The SDL OpenGL context is not current."; return false;}
    if (!session.loaded() || session.areas.empty()) {error = "Load GAME.DTZ and its native world areas first."; return false;}
    nativeShaderProfile = (session.vcs() ? 2 : 0) + (session.archive.isPsp() ? 1 : 0);
    const int effectiveShader = storylandShaderRenderProfile(selectedShader < 0 ? nativeShaderProfile : selectedShader);
    auto next = std::make_unique<Resources>();
    for (size_t kind = 0; kind < next->programs.size(); ++kind)
        if (!compileProgram(session.vcs(),StorylandShaderKind(kind),next->programs[kind],error,effectiveShader)) return false;
    if (storylandPortableWaterProfile(selectedShader) && !compileProgram(session.vcs(),StorylandShaderKind::World,
        next->aeroWaterProgram,error,8)) return false;
    if (storylandPortableAeroProfile(selectedShader) && !compileProgram(session.vcs(),StorylandShaderKind::World,
        next->shadowProgram,error,13)) return false;
    const uint8_t whitePixel[] = {255,255,255,255};
    next->neo = selectedShader == 4;
    next->whiteTexture = uploadTexture(1,1,whitePixel,4);
    next->sky.setGame(session.vcs() ? StorylandSkyGame::Vcs : StorylandSkyGame::Lcs);
    gPortableSkyIsVcs=session.vcs();
    next->sky.setSanAndreasStyle(selectedShader==9);
    next->sky.setPspLighting(effectiveShader==3);
    gPortableShaderProfile=effectiveShader;
    next->hud.load(session.archive,{});
    next->hud.setPcMode(pcShaderEnabled());
    if (session.vcs()) {
        std::string iconError;
        char* base = SDL_GetBasePath();
        if (base) {
            next->hud.loadUserTracksIconNear(std::filesystem::path(base).wstring(), iconError);
            SDL_free(base);
        }
#if defined(__ANDROID__)
        if (!next->hud.sprites.count("radio:usertracks")) {
            SDL_RWops* source = SDL_RWFromFile("radio/usertracks.xtx", "rb");
            if (source) {
                const Sint64 size = SDL_RWsize(source);
                if (size > 0 && size <= 16 * 1024 * 1024) {
                    std::vector<uint8_t> bytes(size_t(size), 0);
                    if (SDL_RWread(source, bytes.data(), 1, bytes.size()) == bytes.size())
                        next->hud.loadUserTracksIcon(bytes, iconError);
                }
                SDL_RWclose(source);
            }
        }
#endif
    }
    if (!next->sky.loadNativeTimecycle(session.archive.unpackedBytes(),session.vcs(),error)) return false;
    next->worldTextures.resize(session.areas.size());
    next->sceneryRevision=session.sceneryRevision;
    for (size_t area = 0; area < session.areas.size(); ++area)
        next->worldTextures[area].resize(session.areas[area]->directTextures().size());
    RgbaImage water; std::string waterError;
    if (storylandLoadPlayTestWaterTexture(session.archive,water,waterError))
        next->waterTexture = uploadTexture(water.width,water.height,water.rgba.data(),water.rgba.size(),water.mipLevels);
    next->retailWaterTexture=next->waterTexture;
    if(next->waterTexture)next->presentationTextureHandles.push_back(next->waterTexture);
    const auto nativeSprite = [&](const char* name,bool surface=false) {
        RgbaImage image;
        if (!storylandFindNativeTexture(session.archive.unpackedBytes(), session.vcs(), name, image))
            return GLuint(0);
        if(surface)next->charredVehicleImage=image;
        const GLuint texture = surface?uploadSurfaceTexture(image):
            uploadTexture(image.width, image.height, image.rgba.data(), image.rgba.size(),image.mipLevels);
        if (texture) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        return texture;
    };
    std::set<std::string> auxiliaryNames;
    for(const auto& marker:session.nativeMarkerModels)
        for(const auto& name:marker.second.previewMaterialTextureNames())
            if(!name.empty())auxiliaryNames.insert(name);
    std::set<std::string> particleNames;
    for(int kind=0;kind<4;++kind)
        particleNames.insert(storylandRubbishTextureName(StorylandRubbishKind(kind)));
    particleNames.insert("shad_car");
    particleNames.insert("shad_bike");
    particleNames.insert("shad_ped");
    particleNames.insert("shad_exp");
    particleNames.insert("headlight_single");
    if(session.vcs()) {
        particleNames.insert("firehose");
        particleNames.insert("shad_ped");
        particleNames.insert("fight_ring");
    }
    for (int type = 0; type < 9; ++type) particleNames.insert(storylandCoronaTextureName(type));
    for (const auto& effect : session.archive.leeds2dfxEffects())
        if (effect.isLight && !effect.coronaTextureName.empty()) auxiliaryNames.insert(effect.coronaTextureName);
    for (int kind=0; kind<=int(StorylandParticleKind::Dollar); ++kind)
        for (unsigned frame=0; frame<6; ++frame)
            for (const char* name : storylandParticleTextureCandidates(StorylandParticleKind(kind),frame)) if(name) particleNames.insert(name);
    if(session.vcs())for(unsigned frame=0;frame<5;++frame)
        for(const char* name:storylandParticleTextureCandidates(StorylandParticleKind::Smoke,frame,66,true))
            if(name)particleNames.insert(name);
    for(int nativeType:{5,12,26,34,35,36,37,38,39,44,45,46,47,48,50,75,76,77,78})
        for(unsigned frame=0;frame<5;++frame)
            for(const char* name:storylandParticleTextureCandidates(StorylandParticleKind::Water,frame,nativeType,session.vcs()))
                if(name)particleNames.insert(name);
    for(int nativeType:{13,14,27,33,40,41,42,43,55,56,57,58,65,66,67,74})
        for(unsigned frame=0;frame<6;++frame)
            for(const char* name:storylandParticleTextureCandidates(StorylandParticleKind::Smoke,frame,nativeType,session.vcs()))
                if(name)particleNames.insert(name);
    for(const auto& name:particleNames)auxiliaryNames.erase(name);
    auto sprites=storylandFindNativeDictionaryTextures(session.archive.unpackedBytes(),session.vcs(),"particle",particleNames);
    for(auto& sprite:storylandFindNativeTextures(session.archive.unpackedBytes(),session.vcs(),auxiliaryNames))
        sprites.push_back(std::move(sprite));
    for (auto& sprite : sprites) {
        if(storylandParticleSmokeTexture(sprite.name)) {
            const auto& original=sprite.image;
            const GLuint texture=uploadTexture(original.width,original.height,original.rgba.data(),
                original.rgba.size(),original.mipLevels);
            if(texture) {
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                if(next->particleTextures.emplace(storylandParticleTextureKey(sprite.name.c_str(),true),texture).second)
                    next->presentationTextureHandles.push_back(texture);
                else glDeleteTextures(1,&texture);
            }
        }
        storylandPrepareSmokeTexture(sprite.name,sprite.image);
        const auto& image=sprite.image;
        const GLuint texture=uploadTexture(image.width,image.height,image.rgba.data(),image.rgba.size(),image.mipLevels);
        if(texture) {
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            if(next->particleTextures.emplace(sprite.name,texture).second)
                next->presentationTextureHandles.push_back(texture);
            else glDeleteTextures(1,&texture);
        }
    }
    next->charredVehicleTexture = nativeSprite(session.vcs()?"xv_vehiclegrunge_sl":"vehiclegrunge256",true);
    next->targetTriangle = nativeSprite("triangle16");
    next->retailTargetTriangle=next->targetTriangle;
    if(next->targetTriangle)next->presentationTextureHandles.push_back(next->targetTriangle);
    for (size_t index = 0; index < next->opticTextures.size(); ++index)
        next->opticTextures[index] = nativeSprite(Resources::opticNames[index]);
    next->retailOpticTextures=next->opticTextures;
    for(const auto texture:next->opticTextures)if(texture)next->presentationTextureHandles.push_back(texture);
    next->retailParticleTextures=std::move(next->particleTextures);
    next->uploadPcArtwork();
    resources = std::move(next);
    frontendArtwork.reset();
    statusMessage = "Native Stories OpenGL renderer ready.";
    return true;
}

bool StorylandLinuxRenderer::prepareCredits(StorylandLinuxSession& session,int32_t group,
    std::vector<StorylandMissionEnvironment::CreditsLine>& lines,std::string& error) {
    if(!resources) {error="Credits require the initialized native HUD.";return false;}
    return storylandPrepareNativeCredits(session.missionGxt,resources->hud,group,lines,error,session.vcs());
}

bool StorylandLinuxRenderer::loadScene(StorylandLinuxSession& session,
    const std::array<float,3>& point, std::string& error) {
    error.clear();
    for(auto& area:session.areas)area->synchronizeNativeSceneryTransforms(session.archive.nativeWorld());
    if (!resources || !glGetString(GL_VERSION)) {
        error = "LOAD_SCENE requires the current SDL OpenGL renderer."; return false;
    }
    resources->refreshNativeScenery(session);
    if (!std::all_of(point.begin(),point.end(),[](float x){return std::isfinite(x);})) {
        error = "LOAD_SCENE coordinates are invalid."; return false;
    }
    auto frame = areaFrames(session);
    size_t geometry = 0;
    const uint32_t level=session.archive.nativeWorld().zones.worldLevelAt({point[0],point[1],point[2]},resources->worldLevel);
    for (size_t area = 0; area < session.areas.size(); ++area) {
        const auto& browser = *session.areas[area];
        const uint32_t owner=browser.authoredSectorLevel();
        if(owner&&level&&owner!=level)continue;
        if (!browser.worldStreamingComplete(session.environment.hour)) {
            error = "LOAD_SCENE is missing native area geometry or textures."; return false;
        }
        const auto visible = storylandIntroVisibleSectors(browser,
            {point[0],point[1],point[2]},
            storylandIntroSceneAnchor(session.archive.nativeWorld().zones,
            {session.environment.x,session.environment.y,session.environment.z},session.environment.introMissionProtected),
            session.environment.introMissionProtected,
            session.environment.camera.active||session.environment.cutscene.playing);
        for (size_t index = 0; index < browser.placements().size(); ++index) {
            const auto& placement = browser.placements()[index];
            if (!browser.placementEnabled(placement,session.environment.hour) ||
                !browser.placementVisible(placement,visible,session.environment.hour)||
                !StorylandArchiveBrowser::nativePlacementVisible(placement,
                    std::hypot(placement.x-point[0],placement.y-point[1],placement.z-point[2]),
                    uint8_t(session.environment.area))) continue;
            const float preloadRange=session.environment.introMissionProtected?
                875.0f:600.0f;
            if (std::hypot(placement.x-point[0],placement.y-point[1]) >
                preloadRange+placement.boundRadius) continue;
            const auto found = frame[area].findMesh(placement);
            const auto* mesh = browser.placementModelSwap(index);
            if (!mesh && found) mesh = found->draw;
            if (!mesh || mesh->triangles.empty()) continue;
            ++geometry;
            if(!storylandVisitSceneMaterialRuns(*mesh,[&](uint32_t material) {
                const auto* source = frame[area].resolveTexture(int32_t(material),*mesh,placement);
                if (source && !resources->worldTexture(session,area,source)) {
                    error = "LOAD_SCENE could not upload a decoded native texture."; return false;
                }
                return true;
            }))return false;
        }
    }
    if (!geometry) {error = "LOAD_SCENE found no decoded geometry at the requested location."; return false;}
    glFlush();
    statusMessage = "Native scene ready: " + std::to_string(geometry) + " placements.";
    return true;
}

void StorylandLinuxRenderer::drawEnvironmentControls(StorylandLinuxSession& session) {
    if(!resources)return;
    auto& sky=resources->sky;
    if(ImGui::CollapsingHeader("Sky")) {
        bool enabled=sky.isEnabled();
        if(ImGui::Checkbox("Enabled##Sky",&enabled))sky.setEnabled(enabled);
        int style=int(sky.game());
        if(ImGui::Combo("Sky style",&style,"Liberty City Stories\0Vice City Stories\0Manhunt 2\0"))
            sky.setGame(StorylandSkyGame(style));
    }
    storylandDrawTimeMenu(session.runtime,session.environment,sky);
    if(ImGui::CollapsingHeader("Weather")) {
        int weather=int(sky.nativeWeather());
        const char* names[8];
        for(unsigned index=0;index<8;++index)
            names[index]=storylandNativeWeatherName(index,session.vcs());
        if(ImGui::Combo("Preset##Weather",&weather,names,8)) {
            sky.setNativeWeather(unsigned(weather));
            auto& state=session.environment.weatherState;
            state.previous=state.current=int16_t(weather);
            state.interpolation=0;
            session.environment.forcedWeather=weather;
            session.environment.weatherChanged=true;
        }
        float wind=sky.windDirection();
        if(ImGui::SliderFloat("Wind direction",&wind,-180,180,"%.0f deg")) {
            session.windDirectionDegrees=wind;
            sky.setWindDirection(wind);
        }
    }
}

void StorylandLinuxRenderer::prepareCameraVisibility(StorylandLinuxSession& session, int width, int height) {
    if (!resources || width <= 0 || height <= 0) return;
    prepareCameraView(session, resources->sky, resources->skyMilliseconds, width, height, aspectRatio);
}

void StorylandLinuxRenderer::draw(StorylandLinuxSession& session, int width, int height) {
    if (!resources || width <= 0 || height <= 0) return;
    StorylandRenderPerformanceFrame performanceFrame(performance);
    gPortableGraphicsQuality=graphicsQuality;
    for(auto& area:session.areas)area->synchronizeNativeSceneryTransforms(session.archive.nativeWorld());
    auto& r = *resources;
    auto& env = session.environment;
    const double uvSeconds = session.active() ? double(env.gameMilliseconds) * 0.001 :
        double(SDL_GetTicks64()) * 0.001;
    r.refreshNativeScenery(session);
    // Upload the area sectors and placed gta3PS models before the first shot.
    if(!session.active()||!env.introMissionProtected)r.introSceneryPrewarmed=false;
    if(session.active()&&session.vcs()&&env.introMissionProtected&&
       !r.introSceneryPrewarmed) {
        const auto anchor=storylandIntroSceneAnchor(session.archive.nativeWorld().zones,
            {env.x,env.y,env.z},true);
        std::string error;
        r.introSceneryPrewarmed=loadScene(session,{anchor.x,anchor.y,anchor.z},error);
        if(!r.introSceneryPrewarmed)
            statusMessage="VCS Intro scenery unavailable: "+error;
    }
    r.renderedPlacements = r.renderedActors = 0;
    r.meshEffects=session.active()?&meshEffects:nullptr;
    r.meshTime=session.animationSeconds();
    r.showActorBounds=showBounds;
    gPortableShadowReady=false;
    ++r.shadowFrame;
    const auto cameraView = prepareCameraView(session, r.sky, r.skyMilliseconds, width, height, aspectRatio);
    if (cameraView.fieldOfView <= 0) return;
    const auto& sky = r.sky.state();
    r.rain=env.weatherState.rain;
    const auto& viewRectangle = cameraView.rectangle;
    const auto& eye = cameraView.eye;
    const auto& target = cameraView.target;
    const auto& up = cameraView.up;
    const float fov = cameraView.fieldOfView;
    const double nearClip = cameraView.nearClip, farClip = cameraView.farClip;
    const bool nativeCutscene = cameraView.nativeCutscene;
    if(session.active()&&session.vcs()&&session.runtime.extendedAndSaFeatures&&!nativeCutscene&&
        !env.cutscene.playing)
        session.runtime.screenBloodView=storylandMakeScreenBloodView(
            {eye.x,eye.y,eye.z},{target.x,target.y,target.z});
    else session.runtime.screenBloodView={};
    const bool speedCamera = cameraView.speedCamera;
    const auto displaySize = ImGui::GetIO().DisplaySize;
    const int hudAspectMode = StorylandRetailScreen::resolvedAspectMode(
        int(aspectRatio), session.active(), session.archive.isPsp());
    const auto hudRectangle = storylandFitView(
        int(displaySize.x), int(displaySize.y), StorylandAspectRatio(hudAspectMode));
    glViewport(viewRectangle.x, viewRectangle.openGlY(height), viewRectangle.width, viewRectangle.height);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(viewRectangle.x, viewRectangle.openGlY(height), viewRectangle.width, viewRectangle.height);
    glClearColor(sky.fog.r,sky.fog.g,sky.fog.b,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0); glClientActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ARRAY_BUFFER,0); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    const double vertical = nearClip*std::tan(fov*Pi/360);
    const double horizontal = vertical*viewRectangle.aspect();
    glFrustum(-horizontal,horizontal,-vertical,vertical,nearClip,farClip);
    if (env.invertedMap) glScalef(-1.0f,1.0f,1.0f);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    lookAt(eye,target,up);
    r.sky.drawBackground({},100.0f,true,cameraView.nativeFieldOfView);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDisable(GL_CULL_FACE);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    double projection[16],view[16]; glGetDoublev(GL_PROJECTION_MATRIX,projection); glGetDoublev(GL_MODELVIEW_MATRIX,view);
    StorylandViewFrustum frustum; frustum.set(projection,view);
    performanceFrame.next(StorylandRenderPerformance::Selection);
    auto frame = areaFrames(session);
    std::vector<StorylandWorldDetailSource> sources;
    for (const auto& area : session.areas) sources.push_back({&area->placements(),&area->worldMeshes(),area->hasAuthoredSectorLod()||area->hasNativeScenery()});
    const auto sourceOffsets = storylandWorldDetailSourceOffsets(sources);
    std::vector<uint8_t> visible(sourceOffsets.back());
    const auto worldLightClock=storylandC2dClock(env.gameMilliseconds,env.hour,
        env.weatherState.rain,session.vcs());
    const StorylandC2dWorldLightState worldLightState(session.archive.leeds2dfxEffects(),
        session.archive.leeds2dfxWorldInstances(),worldLightClock);
    const bool fallbackNightLights=worldLightClock.hour<6||worldLightClock.hour>=20;
    r.worldLevel=storylandIntroWorldLevel(session.archive.nativeWorld().zones,eye,
        storylandIntroSceneAnchor(session.archive.nativeWorld().zones,
            {env.x,env.y,env.z},env.introMissionProtected),env.introMissionProtected,r.worldLevel);
    for (size_t area=0; area<session.areas.size(); ++area) {
        const auto& browser=*session.areas[area];
        const uint32_t owner=browser.authoredSectorLevel();
        if(owner&&r.worldLevel&&owner!=r.worldLevel)continue;
        const auto sectors=storylandIntroVisibleSectors(browser,eye,
            storylandIntroSceneAnchor(session.archive.nativeWorld().zones,
            {env.x,env.y,env.z},env.introMissionProtected),env.introMissionProtected,
            env.camera.active||env.cutscene.playing);
        for (size_t index=0; index<browser.placements().size(); ++index) {
            const auto& placement=browser.placements()[index];
            const auto found=frame[area].findMesh(placement);
            const auto* mesh=browser.placementModelSwap(index);
            if(!mesh&&found)mesh=found->draw;
            const auto lightState=worldLightState.object(mesh?mesh->nativeModelInfoId:-1,
                placement.nativePoolHeader,placement.nativePoolIndex);
            visible[sourceOffsets[area]+index]=browser.placementEnabled(placement,env.hour)&&
                browser.placementVisible(placement,sectors,env.hour)&&
                lightState.placementVisible(placement.passName=="LIGHTS",
                    placement.nativeSectorSelection||fallbackNightLights)&&
                (placement.nativeInstanceIndex==UINT32_MAX || StorylandArchiveBrowser::nativePlacementVisible(placement,
                    std::hypot(placement.x-eye.x,placement.y-eye.y,placement.z-eye.z),
                    uint8_t(session.runtime.visibleArea)));
        }
    }
    std::vector<uint8_t> selected;
    const auto findDetailMesh = [&](size_t area, const StorylandWorldPlacement& placement) {
        const auto found = frame[area].findMesh(placement);
        return found ? found->detail : nullptr;
    };
    if (!storylandSelectAuthoredWorldDetails(sources, {eye.x,eye.y,eye.z}, visible,
        findDetailMesh, selected, std::min(300.0, farClip * 0.45), farClip)) {
        StorylandWorldDetailPlan detail;
        detail.build(sources,session.archive.unpackedBytes());
        selected=detail.select({eye.x,eye.y,eye.z},300,farClip,visible);
    }
    performanceFrame.next(StorylandRenderPerformance::Lights);
    double lightView[16]; glGetDoublev(GL_MODELVIEW_MATRIX,lightView);
    const auto& lightSky=r.sky.state();
    std::array<float,3> sunlight{lightSky.sunDirectionX,lightSky.sunDirectionY,lightSky.sunDirectionZ};
    r.pspExtraColourActive=gPortableShaderProfile==3&&r.sky.extraColourInterpolation()>0;
    if(gPortableShaderProfile==3&&r.sky.extraColourInterpolation()>0)
        sunlight=env.extraLightDirectionSet?storylandPspExtraLightDirection(env.extraLightDirection):
            storylandPspExtraLightDirection();
    else if(!session.vcs()&&env.extraColor>=0&&env.extraLightDirectionSet)
        sunlight=env.extraLightDirection;
    r.pspExtraDirection=sunlight;
    for(size_t index=0;index<r.pspView.size();++index)r.pspView[index]=float(lightView[index]);
    const auto ps2EnvironmentBasis=storylandPs2EnvironmentBasis(
        {target.x-eye.x,target.y-eye.y,target.z-eye.z});
    gPortablePs2EnvironmentS=storylandPs2EyeDirection(r.pspView.data(),ps2EnvironmentBasis.s);
    gPortablePs2EnvironmentT=storylandPs2EyeDirection(r.pspView.data(),ps2EnvironmentBasis.t);
    r.pspPointLights.clear();
    r.ps2PointLights={};
    gPortableEmissionPointCount=0;
    if((selectedShader==6||selectedShader==7)&&session.active()) {
        std::vector<StorylandPs2PointLight> emissionLights;
        storylandForEachC2dPointLight(session.archive.leeds2dfxEffects(),
            session.archive.leeds2dfxWorldInstances(),session.archive.nativeWorld(),worldLightClock,eye,
            [&](const StorylandC2dPointLight& source) {
                StorylandPs2PointLight light;
                light.position={source.position.x,source.position.y,source.position.z};
                light.color=source.color;
                light.radius=source.radius;
                emissionLights.push_back(light);
            });
        storylandVisitAeroEmissionLights(session.runtime,session.vehicles,{eye.x,eye.y,eye.z},
            float(env.gameMilliseconds)*.001f,env.carDensityMultiplier,
            [&](const auto& light){emissionLights.push_back(light);});
        const auto score=[&](const StorylandPs2PointLight& light) {
            const float x=light.position[0]-eye.x,y=light.position[1]-eye.y,z=light.position[2]-eye.z;
            const float radiusSquared=light.radius*light.radius;
            const float brightness=.299f*light.color[0]+.587f*light.color[1]+.114f*light.color[2];
            return brightness*radiusSquared/std::max(radiusSquared*.16f,x*x+y*y+z*z);
        };
        const size_t count=std::min<size_t>(8,emissionLights.size());
        std::partial_sort(emissionLights.begin(),emissionLights.begin()+count,emissionLights.end(),
            [&](const auto& a,const auto& b){return score(a)>score(b);});
        gPortableEmissionPointCount=int(count);
        for(size_t index=0;index<count;++index) {
            const auto& light=emissionLights[index];
            for(size_t axis=0;axis<3;++axis) {
                gPortableEmissionPointPosition[index*3+axis]=float(lightView[axis]*light.position[0]+
                    lightView[4+axis]*light.position[1]+lightView[8+axis]*light.position[2]+lightView[12+axis]);
                gPortableEmissionPointColor[index*3+axis]=light.color[axis];
            }
            gPortableEmissionPointRange[index]=light.radius;
        }
    }
    if(gPortableShaderProfile==3) {
        const auto& clock=worldLightClock;
        r.pspPointLights=storylandCollectPspPointLights(session.archive.leeds2dfxEffects(),
            session.archive.leeds2dfxWorldInstances(),session.archive.nativeWorld(),clock,eye);
    }
    if(gPortableShaderProfile==0||gPortableShaderProfile==2) {
        const auto& clock=worldLightClock;
        r.ps2PointLights=storylandCollectPs2PointLights(session.archive.leeds2dfxEffects(),
            session.archive.leeds2dfxWorldInstances(),session.archive.nativeWorld(),clock,eye,
            lightSky.spriteBrightness);
    }
    r.pickupVisuals.clear();
    if(gPortableShaderProfile==2&&session.active())
        storylandRegisterVehiclePointLights(r.ps2PointLights,session.runtime,session.vehicles,
            {eye.x,eye.y,eye.z},env.carDensityMultiplier,
            session.runtime.occupiedVehicle,float(env.gameMilliseconds)*.001f);
    if(session.active()&&session.vcs()) {
        r.pickupVisuals=storylandBuildVcsPickupVisualFrame(session.archive.unpackedBytes(),env,
            session.runtime,{eye.x,eye.y,eye.z},r.pickupVisualRandomState);
        for(const auto& visual:r.pickupVisuals)if(visual.pointLightRadius>0) {
            StorylandPs2PointLight light;
            light.position=visual.pointLightPosition;
            light.color=visual.pointLightColor;
            light.radius=visual.pointLightRadius;
            storylandPs2RegisterPointLight(r.ps2PointLights,light,{eye.x,eye.y,eye.z});
        }
    }
    for(size_t axis=0;axis<3;++axis) {
        r.lightEye[axis]=float(lightView[axis]*sunlight[0]+lightView[4+axis]*sunlight[1]+lightView[8+axis]*sunlight[2]);
        gPortablePspEnvironmentS[axis]=float(lightView[axis]);
        gPortablePspEnvironmentT[axis]=-float(lightView[8+axis]);
        r.upEye[axis]=float(lightView[8+axis]);
    }
    if(gPortableShaderProfile==0||gPortableShaderProfile==2)
        r.lightEye=storylandPs2NormalizeLightDirection(r.lightEye);
    performanceFrame.next(StorylandRenderPerformance::Reflections);
    if(gPortableShaderProfile!=0&&gPortableShaderProfile!=3)
        r.captureVehicleEnvironment(session,vehicleEnvironment,frame,sourceOffsets,selected,eye,float(farClip));
    else r.vehicleEnvironment=nullptr;
    performanceFrame.next(StorylandRenderPerformance::ShadowMap);
    if(storylandPortableAeroProfile(selectedShader)&&r.shadowProgram){
        std::array<float,3> sun={sky.sunDirectionX,sky.sunDirectionY,sky.sunDirectionZ};
        bool activeSun=sun[2]>.07f;
        if(!activeSun&&selectedShader==7&&sky.moonDirectionZ>.07f){
            sun={sky.moonDirectionX,sky.moonDirectionY,sky.moonDirectionZ};
            activeSun=true;
        }
        if(activeSun){
            const float radius=selectedShader==7?128.0f:
                selectedShader==6?94.0f:selectedShader==11?68.0f:48.0f;
            const int resolution=selectedShader==7?1024:selectedShader==6?768:
                selectedShader==11?512:256;
            if(r.shadowSize!=resolution){
                if(r.shadowColor)glDeleteTextures(1,&r.shadowColor);
                if(r.shadowDepth)glDeleteRenderbuffers(1,&r.shadowDepth);
                if(r.shadowFramebuffer)glDeleteFramebuffers(1,&r.shadowFramebuffer);
                r.shadowColor=r.shadowDepth=r.shadowFramebuffer=0;
                r.shadowSize=resolution;r.shadowValid=false;
            }
            if(!r.shadowFramebuffer){
                glGenTextures(1,&r.shadowColor);
                glBindTexture(GL_TEXTURE_2D,r.shadowColor);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,resolution,resolution,0,
                    GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
                glGenRenderbuffers(1,&r.shadowDepth);
                glBindRenderbuffer(GL_RENDERBUFFER,r.shadowDepth);
                glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,resolution,resolution);
                glGenFramebuffers(1,&r.shadowFramebuffer);
                glBindFramebuffer(GL_FRAMEBUFFER,r.shadowFramebuffer);
                glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,r.shadowColor,0);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,r.shadowDepth);
                r.shadowValid=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
                glBindFramebuffer(GL_FRAMEBUFFER,0);
                glBindRenderbuffer(GL_RENDERBUFFER,0);
            }
            if(r.shadowValid){
                const float moved=std::hypot(std::hypot(eye.x-r.shadowFocus[0],eye.y-r.shadowFocus[1]),
                    eye.z-r.shadowFocus[2]);
                const float changed=std::abs(sun[0]-r.shadowSun[0])+
                    std::abs(sun[1]-r.shadowSun[1])+std::abs(sun[2]-r.shadowSun[2]);
                const bool update=r.lastShadowUpdate<0||r.shadowFrame-r.lastShadowUpdate>=2||
                    moved>1.5f||changed>.002f;
                if(update){
                    const auto savedFocus=std::array<float,3>{eye.x,eye.y,eye.z};
                    GLint oldViewport[4]={},oldDepthFunction=GL_LEQUAL;
                    GLfloat oldClearColour[4]={};
                    GLboolean oldColourMask[4]={};
                    glGetIntegerv(GL_VIEWPORT,oldViewport);
                    glGetIntegerv(GL_DEPTH_FUNC,&oldDepthFunction);
                    glGetFloatv(GL_COLOR_CLEAR_VALUE,oldClearColour);
                    glGetBooleanv(GL_COLOR_WRITEMASK,oldColourMask);
                    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_VIEWPORT_BIT);
                    GLint oldFramebuffer=0;
                    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&oldFramebuffer);
                    glBindFramebuffer(GL_FRAMEBUFFER,r.shadowFramebuffer);
                    glViewport(0,0,resolution,resolution);
                    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
                    glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
                    glDisable(GL_ALPHA_TEST);glEnable(GL_DEPTH_TEST);
                    glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
                    glClearColor(1,1,1,1);
                    #ifdef __ANDROID__
                    glClearDepthf(1.0f);
                    #else
                    glClearDepth(1.0);
                    #endif
                    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
                    glOrtho(-radius,radius,-radius,radius,-radius*4,radius*4);
                    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
                    const StorylandNativePoint shadowEye{
                        eye.x+sun[0]*radius*1.5f,eye.y+sun[1]*radius*1.5f,
                        eye.z+sun[2]*radius*1.5f};
                    const StorylandNativePoint shadowUp=std::abs(sun[2])>.94f?
                        StorylandNativePoint{0,1,0}:StorylandNativePoint{0,0,1};
                    lookAt(shadowEye,eye,shadowUp);
                    double shadowProj[16],shadowCamera[16];
                    glGetDoublev(GL_PROJECTION_MATRIX,shadowProj);
                    glGetDoublev(GL_MODELVIEW_MATRIX,shadowCamera);
                    for(int k=0;k<16;++k){
                        r.shadowProjection[k]=float(shadowProj[k]);
                        r.shadowView[k]=float(shadowCamera[k]);
                    }
                    StorylandViewFrustum shadowFrustum;
                    shadowFrustum.set(shadowProj,shadowCamera);
                    glUseProgram(r.shadowProgram);
                    glUniform1i(glGetUniformLocation(r.shadowProgram,"tex0"),0);
                    const GLint shadowTextured=glGetUniformLocation(r.shadowProgram,"uUseTexture");
                    glActiveTexture(GL_TEXTURE0);
                    glClientActiveTexture(GL_TEXTURE0);
                    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
                    glDisableClientState(GL_NORMAL_ARRAY);
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glEnableClientState(GL_COLOR_ARRAY);
                    glEnableClientState(GL_VERTEX_ARRAY);
                    StorylandWorldDrawVertices shadowVertices;
                    StorylandWorldIndexBatches shadowIndices;
                    StorylandNativeUvCoordinates shadowCoordinates;
                    std::vector<std::array<float,4>> shadowColors;
                    for(size_t area=0;area<session.areas.size();++area){
                        const auto& browser=*session.areas[area];
                        for(size_t index=0;index<browser.placements().size();++index){
                            if(!selected[sourceOffsets[area]+index])continue;
                            const auto& placement=browser.placements()[index];
                            if(!storylandWorldCastsShadow(placement))continue;
                            const auto found=frame[area].findMesh(placement);
                            const auto* mesh=browser.placementModelSwap(index);
                            if(!mesh&&found)mesh=found->draw;
                            if(!mesh||mesh->vertices.empty()||mesh->triangles.empty()||
                                !shadowFrustum.visible(mesh->bounds,placement.matrix))continue;
                            shadowVertices.prepare(*mesh,r.meshEffects,r.meshTime,placement.matrix);
                            shadowIndices.build(*mesh,false);
                            if(shadowIndices.indices.empty())continue;
                            shadowColors.resize(shadowVertices.count);
                            for(size_t vertex=0;vertex<shadowVertices.count;++vertex)
                                shadowColors[vertex]={1.0f,1.0f,1.0f,
                                    storylandWorldShadowVertexAlpha(shadowVertices.vertices[vertex])};
                            glPushMatrix();glMultMatrixf(placement.matrix);
                            r.deform(StorylandMeshCategory::World,storylandMeshEffectBounds(mesh->bounds),
                                mesh->resourceIndex,placement.matrix);
                            glVertexPointer(3,GL_FLOAT,sizeof(StorylandWorldMeshVertex),
                                &shadowVertices.vertices->x);
                            glTexCoordPointer(2,GL_FLOAT,sizeof(StorylandWorldMeshVertex),
                                &shadowVertices.vertices->u);
                            glColorPointer(4,GL_FLOAT,sizeof(shadowColors.front()),shadowColors.data());
                            for(const auto& range:shadowIndices.ranges) {
                                const auto* source=frame[area].resolveTexture(int32_t(range.materialId),*mesh,placement);
                                GLuint texture=r.worldTexture(session,area,source);
                                if(!source)texture=r.namedWorldTexture(session,browser,range.materialId,source);
                                shadowCoordinates.prepare(&shadowVertices.vertices->u, shadowVertices.count,
                                    sizeof(StorylandWorldMeshVertex),
                                    storylandWorldUvOffset(texture ? source : nullptr, uvSeconds));
                                glTexCoordPointer(2, GL_FLOAT, GLsizei(shadowCoordinates.stride),
                                    shadowCoordinates.data);
                                glBindTexture(GL_TEXTURE_2D,texture?texture:r.whiteTexture);
                                glUniform1i(shadowTextured,texture?1:0);
                                glDrawElements(GL_TRIANGLES,GLsizei(range.indexCount),GL_UNSIGNED_INT,
                                    shadowIndices.indices.data()+range.firstIndex);
                            }
                            glPopMatrix();
                        }
                    }
                    glPopClientAttrib();
                    glUseProgram(0);
                    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();
                    glMatrixMode(GL_MODELVIEW);
                    glBindFramebuffer(GL_FRAMEBUFFER,GLuint(oldFramebuffer));
                    glPopAttrib();
                    glViewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);
                    glDepthFunc(GLenum(oldDepthFunction));
                    glColorMask(oldColourMask[0],oldColourMask[1],oldColourMask[2],oldColourMask[3]);
                    glClearColor(oldClearColour[0],oldClearColour[1],oldClearColour[2],oldClearColour[3]);
                    r.shadowFocus=savedFocus;r.shadowSun=sun;r.shadowRadius=radius;
                    r.lastShadowUpdate=r.shadowFrame;
                }
                double currentView[16];glGetDoublev(GL_MODELVIEW_MATRIX,currentView);
                float viewFloat[16],inverse[16],combined[16];
                for(int k=0;k<16;++k)viewFloat[k]=float(currentView[k]);
                if(inverseMatrix(viewFloat,inverse)){
                    multiplyMatrices(r.shadowView.data(),inverse,combined);
                    multiplyMatrices(r.shadowProjection.data(),combined,gPortableEyeToShadow.data());
                    gPortableShadowReady=true;
                    gPortableShadowTexture=r.shadowColor;
                    gPortableShadowSize=resolution;
                }
            }
        }
    }

    const GLuint worldProgram=r.programs[size_t(StorylandShaderKind::World)];
    r.pspEnvironmentReady=false;
    r.pspEnvironmentStrength=storylandPspEarlyEnvironmentStrength(env.hour);
    setProgram(worldProgram,sky,false,&r.lightEye,&r.upEye,r.rain);
    // Submit the authored vertices once per mesh. Expanding every triangle
    // repeats both CPU vertex copies and GPU transforms at shared corners.
    performanceFrame.next(StorylandRenderPerformance::World);
    constexpr size_t indexCapacity = 12288;
    std::unique_ptr<uint32_t[]> indices(new uint32_t[indexCapacity]);
    size_t indexCount = 0;
    std::vector<std::array<uint8_t,4>> mixedPrelights;
    StorylandNativeUvCoordinates animatedCoordinates;
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    GLuint activeWorldProgram=worldProgram;
    GLint useTextureLocation=glGetUniformLocation(worldProgram,"uUseTexture");
    int texturedState=-1;
    const bool pspWorld=gPortableShaderProfile==3;
    const auto phases=storylandWorldRenderPhases(sources,sourceOffsets,selected);
    for(unsigned phase=0;phase<4;++phase) {
        glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDepthMask(phase==0?GL_TRUE:GL_FALSE);
        if (phase==1) {glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}
        if (phase==3) {glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);}
        for (const auto& submitted : phases[phase]) {
            const size_t area=submitted.area;
            const size_t index=submitted.index;
            const auto& browser=*session.areas[area];
            const auto& placement=browser.placements()[index];
            const bool nativeScenery=placement.nativeInstanceIndex!=UINT32_MAX;
            const auto found=frame[area].findMesh(placement);
            const auto* mesh=browser.placementModelSwap(index);
            if (!mesh&&found) mesh=found->draw;
            if (!mesh||mesh->vertices.empty()||mesh->triangles.empty()||!frustum.visible(mesh->bounds,placement.matrix)) continue;
            const auto objectLights=worldLightState.object(mesh->nativeModelInfoId,
                placement.nativePoolHeader,placement.nativePoolIndex);
            glPushMatrix(); glMultMatrixf(placement.matrix);
            r.deform(StorylandMeshCategory::World,storylandMeshEffectBounds(mesh->bounds),mesh->resourceIndex,placement.matrix);
            std::vector<StorylandWorldMeshVertex> deformedVertices;
            if(r.meshEffects&&r.meshEffects->spatial(StorylandMeshCategory::World)) {
                deformedVertices=mesh->vertices;
                const auto bounds=storylandMeshEffectBounds(mesh->bounds);
                for(auto& point:deformedVertices)point=storylandMeshEffectVertex(*r.meshEffects,
                    StorylandMeshCategory::World,bounds,r.meshTime,mesh->resourceIndex,point,placement.matrix);
            }
            const auto* vertices=deformedVertices.empty()?mesh->vertices.data():deformedVertices.data();
            glTexCoordPointer(2,GL_FLOAT,sizeof(StorylandWorldMeshVertex),&vertices->u);
            glVertexPointer(3,GL_FLOAT,sizeof(StorylandWorldMeshVertex),&vertices->x);
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT,sizeof(StorylandWorldMeshVertex),
                (gPortableShaderProfile==0||gPortableShaderProfile==2)?&vertices->ps2Nx:&vertices->nx);
            const bool anyPrelight=std::any_of(mesh->vertices.begin(),mesh->vertices.end(),
                [](const auto& vertex){return vertex.hasPrelight;});
            const bool ps2Colors=(gPortableShaderProfile==0||gPortableShaderProfile==2)&&
                std::all_of(mesh->vertices.begin(),mesh->vertices.end(),
                    [](const auto& vertex){return vertex.hasPs2Color;});
            if(ps2Colors) {
                glEnableClientState(GL_COLOR_ARRAY);
                glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(StorylandWorldMeshVertex),vertices->ps2Color.data());
            } else if(!anyPrelight) {
                glDisableClientState(GL_COLOR_ARRAY);glColor4f(1,1,1,1);
            } else {
                glEnableClientState(GL_COLOR_ARRAY);
                const bool allPrelit=std::all_of(mesh->vertices.begin(),mesh->vertices.end(),
                    [](const auto& vertex){return vertex.hasPrelight;});
                if(allPrelit)glColorPointer(nativeScenery?4:3,GL_UNSIGNED_BYTE,sizeof(StorylandWorldMeshVertex),&vertices->red);
                else {
                    mixedPrelights.resize(mesh->vertices.size());
                    for(size_t vertex=0;vertex<mesh->vertices.size();++vertex) {
                        const auto& point=mesh->vertices[vertex];
                        mixedPrelights[vertex]=point.hasPrelight?std::array<uint8_t,4>{point.red,point.green,point.blue,nativeScenery?point.alpha:uint8_t(255)}:
                            std::array<uint8_t,4>{255,255,255,255};
                    }
                    glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(mixedPrelights.front()),mixedPrelights.data());
                }
            }
            const auto flushIndices=[&](){
                if(indexCount==0)return;
                glDrawElements(GL_TRIANGLES,GLsizei(indexCount),GL_UNSIGNED_INT,indices.get());
                indexCount=0;
            };
            const unsigned transparentPasses=storylandWorldAlphaPassCount(
                gPortableShaderProfile,phase,nativeScenery);
            for(unsigned transparentPass=0;transparentPass<transparentPasses;++transparentPass) {
                const bool transparentReplay=transparentPass==1;
                uint32_t lastMaterial=UINT32_MAX-1;
                uint32_t lastPspStrip=UINT32_MAX-1;
                for (const auto& triangle:mesh->triangles) {
                    if (triangle.a>=mesh->vertices.size()||triangle.b>=mesh->vertices.size()||triangle.c>=mesh->vertices.size()) continue;
                    if (lastMaterial!=triangle.textureId ||
                        (gPortableShaderProfile==3&&lastPspStrip!=triangle.pspStripIndex)) {
                        flushIndices();lastMaterial=triangle.textureId;
                        lastPspStrip=triangle.pspStripIndex;
                        const auto* source=frame[area].resolveTexture(int32_t(lastMaterial),*mesh,placement);
                        GLuint texture=r.worldTexture(session,area,source);
                        if(!source)texture=r.namedWorldTexture(session,browser,lastMaterial,source);
                        animatedCoordinates.prepare(&vertices->u, mesh->vertices.size(),
                            sizeof(StorylandWorldMeshVertex),
                            storylandWorldUvOffset(texture ? source : nullptr, uvSeconds));
                        glTexCoordPointer(2, GL_FLOAT, GLsizei(animatedCoordinates.stride),
                            animatedCoordinates.data);
                        glBindTexture(GL_TEXTURE_2D,texture?texture:r.whiteTexture);
#ifndef __ANDROID__
                        if(texture)gPortableTextureQuality.applyBoundMaterial(gPortableGraphicsQuality,
                            gPortableShaderProfile==0||gPortableShaderProfile==2||gPortableShaderProfile==4||
                            storylandPortableAeroProfile(gPortableShaderProfile),SDL_GL_GetCurrentContext());
#endif
                        const int textured=texture!=0;
                        bool fractional=nativeScenery&&mesh->nativeAlphaBlend;
                        if(phase==2&&source&&!nativeScenery) {
                            auto alpha=frame[area].fractionalAlpha.find(source);
                            if(alpha==frame[area].fractionalAlpha.end())
                                alpha=frame[area].fractionalAlpha.emplace(source,translucent(*source)).first;
                            fractional=alpha->second;
                        }
                        const bool vegetation=nativeScenery?mesh->nativeVegetation:phase==2&&textured&&!fractional;
                        const bool ps2Profile=gPortableShaderProfile==0||gPortableShaderProfile==2;
                        const auto worldKind=(gPortableShaderProfile==3||ps2Profile)&&nativeScenery&&mesh->nativeHasNormals?
                            StorylandShaderKind::Generic:vegetation?StorylandShaderKind::Vegetation:StorylandShaderKind::World;
                        const GLuint materialProgram=r.programs[size_t(worldKind)];
                        if(materialProgram!=activeWorldProgram) {
                            activeWorldProgram=materialProgram;
                            setProgram(activeWorldProgram,sky,false,&r.lightEye,&r.upEye,r.rain);
                            useTextureLocation=glGetUniformLocation(activeWorldProgram,"uUseTexture");
                            texturedState=-1;
                        }
                        glUniform1f(glGetUniformLocation(activeWorldProgram,"uEmissiveStrength"),
                            objectLights.emissionEnabled()?storylandPortableEmission(source):0.0f);
                        if(textured!=texturedState) {
                            if(textured)glEnable(GL_TEXTURE_2D);else glDisable(GL_TEXTURE_2D);
                            glUniform1i(useTextureLocation,textured);
                            texturedState=textured;
                        }
                        glUniform1f(glGetUniformLocation(activeWorldProgram,"uPs2ColorScale"),
                            textured||nativeScenery?255.0f/128.0f:1.0f);
                        if(ps2Profile) {
                            glUniform1i(glGetUniformLocation(activeWorldProgram,"uPs2BudokaiReflection"),0);
                            const bool litNativeModel=nativeScenery&&mesh->nativeHasNormals;
                            if(litNativeModel)r.applyPs2LocalLights(activeWorldProgram);
                            const float intensity=litNativeModel?mesh->nativeMaterialEffect:0.0f;
                            const GLuint reflection=intensity>0.0f&&r.ps2EnvironmentReady?r.ps2EnvironmentTexture:0;
                            glUniform1i(glGetUniformLocation(activeWorldProgram,"uDualTextureMode"),reflection?2:0);
                            glUniform1f(glGetUniformLocation(activeWorldProgram,"uPs2EnvironmentCoefficient"),
                                float(storylandPs2EnvironmentCoefficient(intensity,1.0f,gPortableShaderProfile==2?
                                    StorylandPs2LightingGame::ViceCityStories:StorylandPs2LightingGame::LibertyCityStories)));
                            glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,reflection);
                            glActiveTexture(GL_TEXTURE0);
                        }
                        if(gPortableShaderProfile==3) {
                            if(nativeScenery&&mesh->nativeHasNormals)r.applyPspLocalLights(activeWorldProgram);
                            glUniform1i(glGetUniformLocation(activeWorldProgram,"uPspPrelitModel"),
                                nativeScenery&&!mesh->nativeHasNormals?1:0);
                            setPspTextureSize(activeWorldProgram,source?source->width:0,source?source->height:0,
                                source?storylandAuthoredMipCount(source->width,source->height,source->mipLevels):0);
                            const auto* strip=lastPspStrip<mesh->pspStrips.size()?&mesh->pspStrips[lastPspStrip]:nullptr;
                            glUniform1i(glGetUniformLocation(activeWorldProgram,"uPspTextureLodMode"),strip?2:0);
                            float slopeScale=0.0008f;
                            if(strip&&strip->modelSlope&&source) {
                                float projectionMatrix[16]{};
                                glGetFloatv(GL_PROJECTION_MATRIX,projectionMatrix);
                                if(std::abs(projectionMatrix[0])>0.000001f)
                                    slopeScale=float(source->width)*0.6f/(512.0f*std::abs(projectionMatrix[0]));
                            }
                            glUniform1f(glGetUniformLocation(activeWorldProgram,"uPspTextureLodSlope"),
                                strip?strip->textureLodSlope*slopeScale:1.0f);
                            glUniform1f(glGetUniformLocation(activeWorldProgram,"uPspDepthOffset"),
                                strip&&strip->depthCenterOffset?-4.0f/65535.0f:0.0f);
                            if(strip&&strip->cullBackFaces){glEnable(GL_CULL_FACE);glCullFace(GL_BACK);}
                            else glDisable(GL_CULL_FACE);
                        }
                        const bool pspTransparent=pspWorld&&phase==2&&!nativeScenery;
                        const bool vcsCompiled=storylandVcsPs2CompiledWorld(gPortableShaderProfile,nativeScenery);
                        if(vcsCompiled) {
                            storylandBindWorldAlphaPass(gPortableShaderProfile,phase,transparentPass);
                        } else if(pspTransparent) {
                            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
                            glDepthMask(transparentReplay?GL_FALSE:GL_TRUE);
                        } else if(phase==3) {
                            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDepthMask(GL_FALSE);
                        } else if(phase==2&&fractional) {
                            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
                            glDepthMask(nativeScenery&&mesh->nativeAlphaTest?GL_TRUE:GL_FALSE);
                        } else if(phase==1) {
                            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
                            glDepthMask(GL_FALSE);
                        } else {
                            glDisable(GL_BLEND);glDepthMask(GL_TRUE);
                        }
                        if(pspTransparent) {
                            glEnable(GL_ALPHA_TEST);
                            glAlphaFunc(transparentReplay?GL_LESS:GL_GREATER,240.0f/255.0f);
                        } else if(!vcsCompiled&&(gPortableShaderProfile==3||
                            (nativeScenery?mesh->nativeAlphaTest:textured&&!fractional))) {
                            glEnable(GL_ALPHA_TEST);
                            glAlphaFunc(gPortableShaderProfile==3?GL_GEQUAL:GL_GREATER,
                                gPortableShaderProfile==3?8.0f/255.0f:.08f);
                        }
                        else if(!vcsCompiled)glDisable(GL_ALPHA_TEST);
                    }
                    indices[indexCount++]=triangle.a;
                    indices[indexCount++]=triangle.b;
                    indices[indexCount++]=triangle.c;
                    if(indexCount==indexCapacity)flushIndices();
                }
                flushIndices();
            }
            glPopMatrix();++r.renderedPlacements;
        }
    }
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    if(gPortableShaderProfile==3)glDisable(GL_CULL_FACE);
    performanceFrame.next(StorylandRenderPerformance::Water);
    FrameActorTextures actorTextures;
    const auto palette=storylandNativeVehiclePalette(session.archive.unpackedBytes(),session.vcs());
    const auto drawCutsceneActors=[&]() {
        glPushMatrix();glTranslatef(env.cutscene.offsetX,env.cutscene.offsetY,env.cutscene.offsetZ);
        for(const auto& actor:session.cutscene.scene().actors()) {
            const std::array<float,3> position{env.cutscene.offsetX+actor.entityMatrix[12],
                env.cutscene.offsetY+actor.entityMatrix[13],env.cutscene.offsetZ+actor.entityMatrix[14]};
            r.actor(actor.asset,actor.posed,actorTextures,palette,255,nullptr,nullptr,
                StorylandMeshCategory::World,nullptr,&position);
        }
        glPopMatrix();
    };
    const bool earlyCutscene=nativeCutscene&&gPortableShaderProfile==3;
    if(earlyCutscene) {
        r.pspActorAlphaReference=16;
        drawCutsceneActors();
    }
    const float seconds=float(env.gameMilliseconds)/1000.0f;
    const bool aeroWater=storylandPortableWaterProfile(selectedShader)&&r.aeroWaterProgram!=0;
    const bool ultraWater=selectedShader==7&&aeroWater;
    std::array<float,16> waterProjection{},waterView{};
    std::transform(projection,projection+16,waterProjection.begin(),[](double component){return float(component);});
    std::transform(view,view+16,waterView.begin(),[](double component){return float(component);});
    const auto immersion=storylandUnderwaterFrame(session.archive.nativeWorld().water,
        {eye.x,eye.y,eye.z},seconds,session.environment.vcsSwimmingBoundary.waterWind(sky.windStrength),aeroWater,
        session.runtime.visibleArea,{sky.ambient.r,sky.ambient.g,sky.ambient.b},waterProjection.data(),waterView.data(),r.sky.time());
    const auto drawWaterSurface=[&]() {
        glUseProgram(0);
        const auto selectTexture=[](GLenum unit){glActiveTexture(unit);};
        const auto renderState=storylandBeginTransparentPass(true,selectTexture);
        if (ultraWater) {
            bool contact=false;
            if (session.active()) {
                const auto& player=session.playerPosition;
                for (const auto& region:session.archive.nativeWorld().water) {
                    if (player.x>=region.west && player.x<=region.east &&
                        player.y>=region.south && player.y<=region.north &&
                        player.z>=region.height-2.2f && player.z<=region.height+1.6f) {
                        contact=true;
                        break;
                    }
                }
            }
            float delta=0.0f;
            if (r.lastWaterTime!=UINT32_MAX && env.gameMilliseconds>=r.lastWaterTime)
                delta=std::min(.1f,float(env.gameMilliseconds-r.lastWaterTime)*.001f);
            r.lastWaterTime=env.gameMilliseconds;
            float movement=0.0f;
            if (r.waterPositionInitialized && delta>.0001f)
                movement=std::hypot(session.playerPosition.x-r.lastWaterPosition.x,
                    session.playerPosition.y-r.lastWaterPosition.y)/delta;
            r.lastWaterPosition=session.playerPosition;
            r.waterPositionInitialized=true;
            r.waterReaction.advance(delta,session.playerPosition.x,session.playerPosition.y,
                contact, movement);
            if (!r.reactionTexture) {
                glGenTextures(1,&r.reactionTexture);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D,r.reactionTexture);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,StorylandWaterReaction::Width,
                    StorylandWaterReaction::Height,0,GL_RGBA,GL_UNSIGNED_BYTE,
                    r.waterReaction.image().data());
            } else {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D,r.reactionTexture);
                glTexSubImage2D(GL_TEXTURE_2D,0,0,0,StorylandWaterReaction::Width,
                    StorylandWaterReaction::Height,GL_RGBA,GL_UNSIGNED_BYTE,
                    r.waterReaction.image().data());
            }
            glActiveTexture(GL_TEXTURE0);
        }
        if (r.waterTexture) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D,r.waterTexture);
        } else glDisable(GL_TEXTURE_2D);
        const bool underside=session.vcs()&&immersion.immersion>.535f;
        glNormal3f(0,0,underside?-1.0f:1.0f);
        if(underside) {
            const auto& tint=immersion.waterColor;
            glColor4f(tint[0]*.55f,tint[1]*.68f,tint[2]*.72f,.82f);
        } else glColor4f(.12f,.34f,.43f,.78f);
        if (aeroWater) {
            const GLuint program=r.aeroWaterProgram;
            glUseProgram(program);
            const auto uniform1f=[&](const char* name,float value) {
                const GLint location=glGetUniformLocation(program,name);
                if(location>=0) glUniform1f(location,value);
            };
            const auto uniform1i=[&](const char* name,int value) {
                const GLint location=glGetUniformLocation(program,name);
                if(location>=0) glUniform1i(location,value);
            };
            const auto uniform3=[&](const char* name,float x,float y,float z) {
                const GLint location=glGetUniformLocation(program,name);
                if(location>=0) glUniform3f(location,x,y,z);
            };
            uniform1i("waterTexture",0);
            uniform1i("reactionTexture",1);
            uniform1i("textured",r.waterTexture?1:0);
            uniform1i("reactionEnabled",ultraWater&&r.reactionTexture?1:0);
            uniform1f("time",seconds);
            uniform1f("wind", session.vcs() ? session.environment.vcsSwimmingBoundary.waterWind(sky.windStrength) :
                sky.windStrength);
            uniform1f("rain",sky.rainIntensity);
            uniform1f("fogStart",sky.worldFogStart);
            uniform1f("fogEnd",sky.worldFarClip);
            uniform1f("sunVisibility",std::clamp(sky.sunDirectionZ*2.8f,0.0f,1.0f));
            uniform3("skyHorizon",sky.horizon.r,sky.horizon.g,sky.horizon.b);
            uniform3("skyZenith",sky.zenith.r,sky.zenith.g,sky.zenith.b);
            uniform3("skyLower",sky.lower.r,sky.lower.g,sky.lower.b);
            uniform3("sunDirection",sky.sunDirectionX,sky.sunDirectionY,sky.sunDirectionZ);
            uniform3("sunColor",sky.sunCore.r,sky.sunCore.g,sky.sunCore.b);
            uniform3("fogColor",sky.fog.r,sky.fog.g,sky.fog.b);
            uniform3("cameraPosition",eye.x,eye.y,eye.z);
            uniform3("reactionOrigin",r.waterReaction.west(),r.waterReaction.south(),0.0f);
            const float invExtent=1.0f/StorylandWaterReaction::WorldSize;
            uniform3("reactionInvExtent",invExtent,invExtent,0.0f);
            uniform3("reactionTexel",1.0f/StorylandWaterReaction::Width,
                1.0f/StorylandWaterReaction::Height,0.0f);
        }
        for (const auto& region:session.archive.nativeWorld().water) {
            auto grid=storylandWaterGrid(region.west,region.east,region.south,region.north);
            if (!grid.columns||!grid.rows)continue;
            if (!std::isfinite(region.height)) continue;
            if (aeroWater && session.playerPosition.x >= region.west-48.0f &&
                session.playerPosition.x <= region.east+48.0f &&
                session.playerPosition.y >= region.south-48.0f &&
                session.playerPosition.y <= region.north+48.0f) {
                // Refine only the native water region containing/near the player.
                // Avoid full-map tessellation, particularly on mobile hardware.
                grid.columns=std::clamp(int(std::ceil((region.east-region.west)/3.0f)),grid.columns,192);
                grid.rows=std::clamp(int(std::ceil((region.north-region.south)/3.0f)),grid.rows,192);
            }
            const auto range=storylandVisibleWaterGrid(grid,eye.x,eye.y,float(farClip)+40.0f);
            if(range.firstColumn>=range.lastColumn||range.firstRow>=range.lastRow)continue;
            for(int row=range.firstRow;row<range.lastRow;++row) {
                glBegin(GL_TRIANGLE_STRIP);
                for(int column=range.firstColumn;column<=range.lastColumn;++column) {
                    const float x=grid.x(column);
                    for(const float py:{grid.y(row),grid.y(row+1)}) {
                        const float ripple=aeroWater?0.0f:storylandWaterVisualWave(x,py,seconds,0,false);
                        const float reaction=ultraWater?r.waterReaction.sample(x,py)*.11f:0.0f;
                        glTexCoord2f(x/32.0f+seconds*.015f,py/32.0f+seconds*.01f);
                        glVertex3f(x,py,region.height+ripple+reaction);
                    }
                }
                glEnd();
            }
        }
        if (aeroWater) glUseProgram(0);
        if (ultraWater) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D,0);
            glActiveTexture(GL_TEXTURE0);
        }
        storylandEndTransparentPass(renderState,selectTexture);
    };
    drawWaterSurface();
    r.capturePspEnvironment({target.x-eye.x,target.y-eye.y,target.z-eye.z});
    performanceFrame.next(StorylandRenderPerformance::Actors);
    r.pspEnvironmentStrength=1.0f;
    r.pspActorAlphaReference=8;
    if (nativeCutscene) {
        if(!earlyCutscene)drawCutsceneActors();
    } else {
        session.vehicleDriverBlends.resize(session.vehicles.size());
        for(size_t index=0;index<session.vehicles.size();++index) {
            if(!r.vehicle(session,index,actorTextures,palette))continue;
            if(index>=session.runtime.vehicles.size())continue;
            const auto& vehicle=session.runtime.vehicles[index];
            const auto& traffic=session.vehicles[index];
            if(!traffic.water&&vehicle.health>0&&!vehicle.charred&&vehicle.visible&&
                vehicle.clumpVisible&&vehicle.renderAlpha) {
                const auto& definition=vehicle.nativeDefinition;
                const auto meshFrame=storylandVehicleRenderFrame(definition,
                    {traffic.position.x,traffic.position.y,traffic.position.z},traffic.yaw,
                    definition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
                    vehicle.bodyPitch+vehicle.roadPitch,false,vehicle.craneCarried?&vehicle.craneFrame:nullptr,
                    vehicle.bikeVisualLean);
                const auto nativeFrame=storylandVehicleMatrixProduct(meshFrame,definition.previewRoot);
                const auto lights=storylandVehicleLights(definition,nativeFrame,
                    {vehicle.headlightsOn,vehicle.braking,vehicle.throttle<0,vehicle.engineOn||
                (vehicle.alarmMilliseconds!=0&&vehicle.alarmMilliseconds!=UINT16_MAX),
                    vehicle.lightDamage,vehicle.siren,storylandVehicleSirenRole(session.runtime,vehicle),
                    float(env.gameMilliseconds)*.001f,vehicle.steeringAngle},
                    {eye.x,eye.y,eye.z});
                glUseProgram(0);
                storylandDrawVehicleLights(lights,session.archive.nativeWorld(),{eye.x,eye.y,eye.z},
                    [&](const char* name) {
                        const auto found=r.particleTextures.find(name);
                        return found==r.particleTextures.end()?GLuint(0):found->second;
                    },sky.spriteSize,r.sky.previousNativeWeather()==3u?1.0f:0.0f,
                    selectedShader==6||selectedShader==7);
            }
            const auto scripted=env.missionVehicles.find(vehicle.scriptHandle);
            const bool seatedDriver=vehicle.hasDriver&&!vehicle.missionDriver&&vehicle.health>0&&
                !(int(index)==session.runtime.occupiedVehicle&&session.runtime.playerVehicleSeat<0)&&
                !(scripted!=env.missionVehicles.end()&&scripted->second.driver>=0);
            if(!seatedDriver&&vehicle.displacedDriverTime<=0)continue;
            const size_t driver=storylandVehicleDriverModel(session.runtime,vehicle,uint32_t(index));
            if(driver>=session.pedestrianAssets.size())continue;
            const auto& asset=session.pedestrianAssets[driver];
            auto& blend=session.vehicleDriverBlends[index];
            if(vehicle.displacedDriverTime>0) {
                std::vector<StorylandModelPoint> posed;std::string error;
                if(!session.pedestrianAnimation.sampleTree(asset.model,StorylandPlayTestMotion::Idle,
                    session.animationSeconds(),blend,posed,error))continue;
                glPushMatrix();pedestrianTransform(asset,vehicle.displacedDriverPosition,traffic.yaw);
                StorylandMissionEnvironment::Character driverCharacter;
                driverCharacter.model=int32_t(asset.pedStats.modelId);
                std::array<uint32_t,6> clothing{};
                const bool hasClothing=session.vcs()&&characterClothing(session,driverCharacter,clothing);
                r.actor(asset,posed,actorTextures,palette,vehicle.renderAlpha,nullptr,hasClothing?&clothing:nullptr);
                glPopMatrix();
            }
            if(!seatedDriver)continue;
            auto options=session.pedestrianAnimation.vehicleAnimationOptions(vehicle.nativeDefinition,&vehicle.vcsJetSkiPose.sample,&session.runtime.vehiclePhysicsError);
            options.seatRelative=true;options.boat=traffic.water;
            options.elapsedSeconds=session.animationSeconds();options.transition=&blend.poseTransition;
            options.steering=vehicle.steeringAngle/std::max(1.f,vehicle.handlingSteerLock);
            options.reversing=vehicle.speed<-.2f;
            std::vector<StorylandModelPoint> posed;std::string error;
            const auto matrix=storylandVehicleOccupantFrame(vehicle.nativeDefinition,
                {traffic.position.x,traffic.position.y,traffic.position.z},traffic.yaw,
                vehicle.nativeDefinition.previewMinimumZ,vehicle.suspension,vehicle.bodyRoll-vehicle.roadRoll,
                vehicle.bodyPitch+vehicle.roadPitch,0,traffic.water,nullptr,storylandVcsJetSkiNativeAngularZ(vehicle),
                vehicle.bikeVisualLean);
            std::array<float,3> anchor{};
            const auto carjack=storylandVehicleCarjackSample(session.runtime,index);
            const bool sampled=carjack.active?storylandSampleVehicleCarjackVictim(session.pedestrianAnimation,
                asset.model,vehicle.nativeDefinition,carjack,matrix,session.animationSeconds(),posed,anchor,error,
                &blend.boneFrames,&blend.poseTransition):
                session.pedestrianAnimation.sampleVehicleAction(asset.model,"sit",0,options,posed,error);
            if(!sampled)continue;
            glPushMatrix();glMultMatrixf(matrix.data());
            glTranslatef(anchor[0],anchor[1],anchor[2]);
            glRotatef(90-storylandPedModelForwardAngle(asset.model),0,0,1);
            StorylandMissionEnvironment::Character driverCharacter;
            driverCharacter.model=int32_t(asset.pedStats.modelId);
            std::array<uint32_t,6> clothing{};
            const bool hasClothing=session.vcs()&&characterClothing(session,driverCharacter,clothing);
            r.actor(asset,posed,actorTextures,palette,vehicle.renderAlpha,nullptr,hasClothing?&clothing:nullptr);
            glPopMatrix();
        }
        const bool playerNativeAttached=session.vcs()&&env.activeVcsCharacterAttachment(0)&&
            session.runtime.occupiedVehicle<0&&session.runtime.vehicleAction==StorylandVehicleAction::None;
        std::array<float,16> playerNativeRoot{};
        const bool playerNativeFrameReady=!playerNativeAttached||
            storylandVcsCharacterEntityFrame(env,0,playerNativeRoot);
        if(!playerNativeFrameReady)statusMessage="Attached player has no native entity frame.";
        if (session.playerLoaded&&env.playerVisible&&playerNativeFrameReady&&
            !(session.camera.vehicleCameraActive&&session.camera.vehicleMode==0&&!env.camera.active)&&
            (!session.runtime.manualAimActive||
            (session.runtime.manualAimMode==11&&session.runtime.occupiedVehicle>=0))) {
            glPushMatrix();
            const int occupied=session.runtime.occupiedVehicle;
            std::vector<StorylandModelPoint> seated;
            const std::vector<StorylandModelPoint>* points=&session.playerPoints;
            bool transitionPose=false;
            bool seatedWeapon=false;
            std::array<float,16> seatedWeaponFrame{};
            const int transitioning=session.runtime.transitionVehicle;
            if (session.runtime.health>0&&session.runtime.vehicleAction!=StorylandVehicleAction::None&&transitioning>=0&&
                size_t(transitioning)<session.vehicles.size()&&size_t(transitioning)<session.runtime.vehicles.size()&&
                session.runtime.vehicleTransitionPrepared&&session.runtime.vehiclePhase.phase!=StorylandVehiclePhase::Approach) {
                const auto& vehicle=session.runtime.vehicles[size_t(transitioning)];
                const auto& traffic=session.vehicles[size_t(transitioning)];
                if (vehicle.nativeDefinition.retailTransitions) {
                    auto options=session.playerAnimation.vehicleAnimationOptions(vehicle.nativeDefinition,&vehicle.vcsJetSkiPose.sample,&session.runtime.vehiclePhysicsError);
                    options.rootMotion=true;options.seatRelative=true;options.boat=traffic.water;
                    options.elapsedSeconds=session.animationSeconds();
                    options.transition=&session.playerBlend.poseTransition;
                    options.rightHandSide=session.runtime.vehicleExitRight;
                    options.rearExit=session.runtime.vehicleExitRear;
                    options.variant=session.runtime.vehicleEntryVariant;
                    options.strongWaterContact=session.runtime.vehicleJetSkiWaterContact;
                    options.highAlign=session.runtime.vehicleJetSkiHighAlign;
                    options.rearDoor=session.runtime.playerVehicleSeat>=1;
                    options.passenger=session.runtime.playerVehicleSeat>=0;
                    const unsigned selectedDoor=storylandVehicleSeatDoor(
                        unsigned(session.runtime.playerVehicleSeat+1),options.rightHandSide);
                    options.doorMissing=selectedDoor+2<vehicle.doorDamage.size()&&vehicle.doorDamage[selectedDoor+2]==3;
                    StorylandVehicleRootMotion root;std::string error;
                    const auto phase=session.runtime.vehiclePhase.phase;
                    const bool helicopterBailout=storylandVcsHelicopterBailout(vehicle.nativeDefinition,options.variant);
                    if (session.playerAnimation.sampleVehicleAction(session.player.model,storylandVehiclePhaseAction(phase),
                        session.runtime.vehiclePhase.fraction,options,seated,error,&root,&session.playerBlend.boneFrames)) {
                        const bool pickupUpright=(options.variant==StorylandVehicleEntryVariant::BikePickup||
                            options.variant==StorylandVehicleEntryVariant::BikePullup)&&
                            (phase==StorylandVehiclePhase::Align||phase==StorylandVehiclePhase::Open||
                                phase==StorylandVehiclePhase::PullOut);
                        auto frame=storylandVehicleOccupantFrame(vehicle.nativeDefinition,
                            {traffic.position.x,traffic.position.y,traffic.position.z},traffic.yaw,
                            vehicle.nativeDefinition.previewMinimumZ,vehicle.suspension,
                            pickupUpright?0:vehicle.bodyRoll-vehicle.roadRoll,
                            pickupUpright?0:vehicle.bodyPitch+vehicle.roadPitch,
                            unsigned(session.runtime.playerVehicleSeat+1),traffic.water,
                            vehicle.craneCarried?&vehicle.craneFrame:nullptr,storylandVcsJetSkiNativeAngularZ(vehicle),
                            pickupUpright?0:vehicle.bikeVisualLean);
                        if(helicopterBailout&&!storylandVcsHelicopterBailoutFrame(vehicle.nativeDefinition,
                            {traffic.position.x,traffic.position.y,traffic.position.z},traffic.yaw,vehicle.suspension,
                            vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,frame)) {
                            session.runtime.vehiclePhysicsError="Native VCS helicopter bailout draw frame is unavailable.";
                            glPopMatrix();return;
                        }
                        const auto bounds=storylandMeshBounds(session.player.model.previewPoints());
                        float hipHeight=float(bounds.maximum[2]-bounds.minimum[2])*.56f;
                        for(const auto& bone:session.player.model.armatureBones())
                            if(bone.boneId==1&&bone.hasPreviewPosition) {hipHeight=bone.previewPosition.z-float(bounds.minimum[2]);break;}
                        const auto& door=session.runtime.vehicleActionDoor;
                        const auto doorLocal=storylandVehicleInversePoint(frame,{door.x,door.y,door.z+hipHeight});
                        auto anchor=doorLocal;
                        const auto seatTarget=storylandVehicleEntrySeatTarget(vehicle.nativeDefinition,
                            session.runtime.vehicleEntryVariant,phase,session.runtime.vehiclePhase.elapsed,options.doorMissing);
                        if(phase==StorylandVehiclePhase::GetIn)
                            anchor=storylandVehicleWarpMotion(root.start,root.current,root.end,doorLocal,seatTarget,session.runtime.vehiclePhase.fraction,
                            90.0f-storylandPedModelForwardAngle(session.player.model));
                        else if(phase==StorylandVehiclePhase::GetOut)
                            anchor=storylandVehicleWarpMotion(root.start,root.current,root.end,{0,0,0},doorLocal,session.runtime.vehiclePhase.fraction,
                            90.0f-storylandPedModelForwardAngle(session.player.model));
                        else if(phase==StorylandVehiclePhase::CloseInside)anchor=seatTarget;
                        else anchor=storylandVehicleWarpMotion(root.start,root.current,root.end,doorLocal,doorLocal,session.runtime.vehiclePhase.fraction,
                            90.0f-storylandPedModelForwardAngle(session.player.model));
                        if(helicopterBailout)anchor={};
                        if(!session.vcs()&&!storylandApplyLcsScriptedLook(env,1,session.player.model,
                            storylandLcsLookSeatFrame(session.player.model,frame,anchor),
                            {frame[12],frame[13],frame[14]},session.playerBlend,seated,error))seated.clear();
                        glMultMatrixf(frame.data());glTranslatef(anchor[0],anchor[1],anchor[2]);
                        glRotatef(90-storylandPedModelForwardAngle(session.player.model),0,0,1);
                        points=&seated;transitionPose=true;
                    } else if(helicopterBailout) {
                        session.runtime.vehiclePhysicsError="Native VCS helicopter bailout draw pose: "+error;
                        glPopMatrix();return;
                    }
                }
            }
            if (!transitionPose&&occupied>=0&&size_t(occupied)<session.vehicles.size()&&size_t(occupied)<session.runtime.vehicles.size()) {
                const auto& vehicle=session.runtime.vehicles[size_t(occupied)];const auto& traffic=session.vehicles[size_t(occupied)];
                if (vehicle.model<session.vehicleAssets.size()) {
                    const auto& asset=session.vehicleAssets[vehicle.model];
                    const auto matrix=storylandVehicleOccupantFrame(vehicle.nativeDefinition,
                        {traffic.position.x,traffic.position.y,traffic.position.z},traffic.yaw,minimumZ(asset),vehicle.suspension,
                        vehicle.bodyRoll-vehicle.roadRoll,vehicle.bodyPitch+vehicle.roadPitch,
                        unsigned(session.runtime.playerVehicleSeat+1),traffic.water,vehicle.craneCarried?&vehicle.craneFrame:nullptr,
                        storylandVcsJetSkiNativeAngularZ(vehicle),vehicle.bikeVisualLean);
                    std::string error;auto options=session.playerAnimation.vehicleAnimationOptions(vehicle.nativeDefinition,&vehicle.vcsJetSkiPose.sample,&session.runtime.vehiclePhysicsError);
                    options.seatRelative=true;options.boat=traffic.water;
                    options.elapsedSeconds=session.animationSeconds();
                    options.transition=&session.playerBlend.poseTransition;
                    options.passenger=session.runtime.playerVehicleSeat>=0;
                    options.rightHandSide=options.passenger&&session.runtime.playerVehicleSeat%2==0;
                    options.steering=vehicle.steeringAngle/std::max(1.f,vehicle.handlingSteerLock);
                    options.reversing=vehicle.speed<-.2f;
                    std::array<float,16> driveByFrame{};
                    std::array<float,3> anchor{};
                    const auto carjack=session.runtime.playerVehicleSeat<0?
                        storylandVehicleCarjackSample(session.runtime,size_t(occupied)):StorylandVehicleCarjackSample{};
                    const bool driveBy=session.vcs()&&session.runtime.health>0&&
                        session.runtime.equippedWeapon<session.runtime.weapons.size()&&
                        storylandPlayerSeatedDriveByVehicle(env,session.runtime.weapons[session.runtime.equippedWeapon].nativeId,&driveByFrame);
                    if(carjack.active) {
                        if(storylandSampleVehicleCarjackVictim(session.playerAnimation,session.player.model,
                            vehicle.nativeDefinition,carjack,matrix,session.animationSeconds(),seated,anchor,error,
                            &session.playerBlend.boneFrames,&session.playerBlend.poseTransition))points=&seated;
                    } else if(driveBy) {
                        if(session.playerAnimation.sampleSeatedDriveBy(session.player.model,env.occupiedMissionVehicle,
                            session.runtime.playerVehicleSeat,storylandSeatedDriveByLocalTarget(env,driveByFrame,session.runtime.manualAimActive),
                            session.animationSeconds(),session.playerBlend,seated,error)) {
                            points=&seated;seatedWeapon=true;seatedWeaponFrame=matrix;
                        }
                    } else if(session.playerAnimation.sampleVehicleAction(session.player.model,"sit",0,options,
                        seated,error,nullptr,&session.playerBlend.boneFrames)&&
                        (session.vcs()||storylandApplyLcsScriptedLook(env,1,session.player.model,
                            storylandLcsLookSeatFrame(session.player.model,matrix),
                            {matrix[12],matrix[13],matrix[14]},session.playerBlend,seated,error,true,true)))points=&seated;
                    glMultMatrixf(matrix.data());glTranslatef(anchor[0],anchor[1],anchor[2]);
                    glRotatef(90-storylandPedModelForwardAngle(session.player.model),0,0,1);
                }
            } else if(!transitionPose&&playerNativeAttached) {
                glMultMatrixf(playerNativeRoot.data());
                glRotatef(90-storylandPedModelForwardAngle(session.player.model),0,0,1);
            } else if(!transitionPose) pedestrianTransform(session.player,session.playerPosition,session.playerHeading);
            r.actor(session.player,*points,actorTextures,palette,env.playerRenderAlpha);glPopMatrix();
            if(seatedWeapon)
                r.heldWeapon(session,session.player,session.playerBlend,session.playerPosition,
                    session.playerHeading,session.runtime.equippedWeapon,actorTextures,palette,
                    env.playerRenderAlpha,&seatedWeaponFrame,StorylandMuzzleOwner::Player);
            if (!transitionPose && occupied < 0 && !session.runtime.manualAimActive)
                r.heldWeapon(session, session.player, session.playerBlend, session.playerPosition,
                    session.playerHeading, session.runtime.equippedWeapon, actorTextures,
                    palette, env.playerRenderAlpha,playerNativeAttached?&playerNativeRoot:nullptr,
                    StorylandMuzzleOwner::Player);
        }
        r.coopPlayer(session, actorTextures, palette);
        session.pedestrianBlends.resize(session.pedestrians.size());
        for(size_t index=0;index<session.pedestrians.size()&&index<session.runtime.pedestrians.size();++index) {
            const auto& actor=session.pedestrians[index];
            const auto& ped=session.runtime.pedestrians[index];
            if(ped.scriptHandle>=0||ped.model>=session.pedestrianAssets.size()||
                ped.renderAlpha==0)continue;
            if(env.sphereOnScreen&&!env.sphereOnScreen({actor.position.x,actor.position.y,actor.position.z+1},3))continue;
            const auto& asset=session.pedestrianAssets[ped.model];
            auto& blend=session.pedestrianBlends[index];
            std::vector<StorylandModelPoint> posed;std::string error;
            const bool held=storylandPlayerReactionHoldsTarget(session.runtime.playerReaction,
                StorylandReactionTargetKind::Ambient,int32_t(index),ped.stats.modelId);
            if(!session.samplePedestrian(index,posed,error))continue;
            if(ped.serviceVehicleIdentity) {
                StorylandVehicleMatrix seat;
                if(!storylandVcsServicePassengerFrame(session.runtime,session.vehicles,ped,seat))continue;
                glPushMatrix();glMultMatrixf(seat.data());
                glRotatef(90-storylandPedModelForwardAngle(asset.model),0,0,1);
                StorylandMissionEnvironment::Character seatedCharacter;
                seatedCharacter.model=int32_t(asset.pedStats.modelId);
                std::array<uint32_t,6> clothing{};
                const bool hasClothing=session.vcs()&&characterClothing(session,seatedCharacter,clothing);
                r.actor(asset,posed,actorTextures,palette,ped.renderAlpha,nullptr,hasClothing?&clothing:nullptr);
                glPopMatrix();
                continue;
            }
            glPushMatrix();pedestrianTransform(asset,actor.position,actor.yaw);
            StorylandMissionEnvironment::Character clothingCharacter;
            clothingCharacter.model=int32_t(asset.pedStats.modelId);
            std::array<uint32_t,6> clothing{};
            const bool hasClothing=session.vcs()&&characterClothing(session,clothingCharacter,clothing);
            r.actor(asset,posed,actorTextures,palette,ped.renderAlpha,nullptr,hasClothing?&clothing:nullptr);
            glPopMatrix();
            if(ped.health>0&&!ped.water.active&&!held)
                r.heldWeapon(session,asset,blend,actor.position,actor.yaw,ped.weapon,
                    actorTextures,palette,ped.renderAlpha,nullptr,StorylandMuzzleOwner::AmbientPed,int32_t(index));
        }
        for (const auto& item:env.missionCharacters) {
            const auto& character=item.second;
            if (!character.visible||character.removeFromWorld) continue;
            if(character.vehicle<0&&!character.attached&&env.sphereOnScreen&&
                !env.sphereOnScreen({character.x,character.y,character.z+1},3))continue;
            const auto found=session.missionAssets.find(character.model);
            if (found==session.missionAssets.end()) continue;
            const auto& asset=found->second;
            std::vector<StorylandModelPoint> posed;std::string ignored;
            bool seated=false;
            const bool nativeAttached=session.vcs()?character.attached:
                (character.attached&&env.characterAttachments.count(item.first)>0&&character.vehicle < -1);
            std::array<float,16> nativeRoot{};
            std::array<float,16> parentFrame{};
            const bool frameReady=!nativeAttached || (session.vcs()?
                storylandVcsCharacterEntityFrame(env,item.first,nativeRoot):
                (storylandLcsAttachmentParentFrame(env,character,parentFrame)&&
                 storylandLcsAttachedCharacterFrame(parentFrame,character.attachment,character.heading,nativeRoot)));
            if(nativeAttached&&(!frameReady||!session.sampleMissionCharacter(item.first,posed,ignored))) {
                statusMessage="Attached character "+std::to_string(item.first)+
                    " has no native entity frame or animation pose: "+ignored;
                continue;
            }
            glPushMatrix();
            if(nativeAttached) {
                glMultMatrixf(nativeRoot.data());
                glRotatef(90-storylandPedModelForwardAngle(asset.model),0,0,1);
            }
            const auto task=env.characterVehicleTasks.find(item.first);
            const int32_t vehicleHandle=task!=env.characterVehicleTasks.end()&&task->second.animating?
                task->second.vehicle:character.vehicle;
            const auto missionVehicle=env.missionVehicles.find(vehicleHandle);
            if (!nativeAttached&&missionVehicle!=env.missionVehicles.end()) {
                const int slot=missionVehicle->second.spawnedSlot;
                if(slot>=0&&size_t(slot)<session.vehicles.size()&&size_t(slot)<session.runtime.vehicles.size()) {
                    const auto& state=session.runtime.vehicles[size_t(slot)];
                    const auto& traffic=session.vehicles[size_t(slot)];
                    const int seat=task!=env.characterVehicleTasks.end()?task->second.seat:character.passengerSeat;
                    const auto frame=storylandVehicleOccupantFrame(state.nativeDefinition,
                        {traffic.position.x,traffic.position.y,traffic.position.z},traffic.yaw,
                        state.nativeDefinition.previewMinimumZ,state.suspension,state.bodyRoll-state.roadRoll,
                        state.bodyPitch+state.roadPitch,unsigned(seat+1),traffic.water,state.craneCarried?&state.craneFrame:nullptr,
                        storylandVcsJetSkiNativeAngularZ(state),state.bikeVisualLean);
                    std::array<float,3> anchor{};
                    if(task!=env.characterVehicleTasks.end()&&task->second.animating)
                        seated=storylandSampleScriptVehicleAnimation(session.pedestrianAnimation,asset.model,task->second,
                            env.gameMilliseconds,frame,posed,anchor,ignored,&session.missionBlends[item.first].boneFrames,
                            &session.missionBlends[item.first].poseTransition,&state.nativeDefinition);
                    else if(character.vehicle>=0) {
                        const auto carjack=character.passengerSeat<0||character.passengerSideCarjack?
                            storylandVehicleCarjackSample(session.runtime,size_t(slot)):StorylandVehicleCarjackSample{};
                        auto options=session.pedestrianAnimation.vehicleAnimationOptions(state.nativeDefinition,&state.vcsJetSkiPose.sample,&session.runtime.vehiclePhysicsError);
                        options.seatRelative=true;options.passenger=seat>=0;options.rightHandSide=seat>=0&&seat%2==0;
                        options.elapsedSeconds=session.animationSeconds();
                        options.transition=&session.missionBlends[item.first].poseTransition;
                        options.boat=traffic.water;options.steering=state.steeringAngle/std::max(1.f,state.handlingSteerLock);
                        options.reversing=state.speed<-.2f;
                        seated=carjack.active?storylandSampleVehicleCarjackVictim(session.pedestrianAnimation,
                            asset.model,state.nativeDefinition,carjack,frame,session.animationSeconds(),posed,anchor,ignored,
                            &session.missionBlends[item.first].boneFrames,&session.missionBlends[item.first].poseTransition):
                            session.pedestrianAnimation.sampleVehicleAction(asset.model,"sit",0,options,
                                posed,ignored,nullptr,&session.missionBlends[item.first].boneFrames);
                    }
                    if(seated&&!session.vcs())seated=storylandApplyLcsScriptedLook(env,item.first,asset.model,
                        storylandLcsLookSeatFrame(asset.model,frame,anchor),{frame[12],frame[13],frame[14]},
                        session.missionBlends[item.first],posed,ignored,true,
                        task==env.characterVehicleTasks.end()||!task->second.animating);
                    if(seated) {
                        glMultMatrixf(frame.data());glTranslatef(anchor[0],anchor[1],anchor[2]);
                        glRotatef(90-storylandPedModelForwardAngle(asset.model),0,0,1);
                    }
                }
            }
            if(!seated&&!nativeAttached) {
                if(task!=env.characterVehicleTasks.end()&&task->second.animating) {
                    statusMessage="Mission ped "+std::to_string(item.first)+
                        " missing vehicle transition animation: "+ignored;
                    glPopMatrix();continue;
                }
                session.sampleMissionCharacter(item.first, posed, ignored);
                pedestrianTransform(asset,{character.x,character.y,character.z},character.heading+90);
            }
            std::array<uint32_t,6> clothing{};
            const bool hasClothing=session.vcs()&&characterClothing(session,character,clothing);
            r.actor(asset,posed,actorTextures,palette,character.renderAlpha,nullptr,
                hasClothing?&clothing:nullptr);glPopMatrix();
            const auto combat = session.runtime.missionCombat.find(item.first);
            const auto blend = session.missionBlends.find(item.first);
            if (!seated && character.alive && combat != session.runtime.missionCombat.end() &&
                blend != session.missionBlends.end())
                r.heldWeapon(session, asset, blend->second, {character.x, character.y, character.z},
                    character.heading + 90, combat->second.weapon, actorTextures, palette, character.renderAlpha,
                    nativeAttached?&nativeRoot:nullptr,StorylandMuzzleOwner::MissionPed,item.first);
        }
    }
    // Authored scene props remain part of the world while the CUT replaces its
    // characters and camera; they are not members of the cutscene actor list.
    std::vector<const StorylandMissionEnvironment::Object*> scriptObjectOrder,drawLastObjects;
    std::array<StorylandMissionEnvironment::Object,20> empireObjects;
    scriptObjectOrder.reserve(env.objects.size()+empireObjects.size());drawLastObjects.reserve(env.objects.size());
    if(session.vcs() && env.area==0)for(size_t slot=0;slot<env.animatedEmpireEntities.size();++slot) {
        const auto& entity=env.animatedEmpireEntities[slot];
        if(entity.site<0 || entity.model<0)continue;
        const auto& pose=StorylandVcsEmpireSites[size_t(entity.site)];
        auto& object=empireObjects[slot];
        object.handle=int32_t(0x800000u|uint32_t(slot));object.model=entity.model;
        object.x=pose[0];object.y=pose[1];object.z=pose[2];object.heading=pose[3]*57.2957763671875f;
        scriptObjectOrder.push_back(&object);
    }
    for(const auto& object:env.objects) {
        if(object.deferredRender)continue;
        if(object.drawLast)drawLastObjects.push_back(&object);
        else scriptObjectOrder.push_back(&object);
    }
    std::stable_sort(drawLastObjects.begin(),drawLastObjects.end(),[&](const auto* left,const auto* right) {
        return std::hypot(left->x-eye.x,left->y-eye.y,left->z-eye.z)>
            std::hypot(right->x-eye.x,right->y-eye.y,right->z-eye.z);
    });
    scriptObjectOrder.insert(scriptObjectOrder.end(),drawLastObjects.begin(),drawLastObjects.end());
    for(const auto& object:env.objects)if(object.deferredRender)scriptObjectOrder.push_back(&object);
    for(const auto* pointer:scriptObjectOrder) {
        const auto& object=*pointer;
        if(!object.damage.visible||env.invisibleScriptObjects.count(object.handle)||
            (session.vcs()&&!storylandVcsScriptObjectAreaVisible(object.area,env.area)))continue;
        const auto compiled=session.compiledWorldAssets.find(object.model);
        if(compiled!=session.compiledWorldAssets.end()) {
            const auto& asset=compiled->second;
            if(asset.sourceArea>=session.areas.size())continue;
            const auto& browser=*session.areas[asset.sourceArea];
            const auto matrix=storylandLcsNormalizedPlacement(storylandScriptObjectMatrix(
                object.x,object.y,object.z,object.rotationX,object.rotationY,object.heading),
                asset.geometry.scale,asset.geometry.offset);
            glPushMatrix();glMultMatrixf(matrix.data());
            glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);
            StorylandWorldPlacement owner;owner.sectorIndex=UINT32_MAX;
            for(unsigned pass=0;pass<2;++pass)for(const auto& part:asset.geometry.parts) {
                if(part.transparent!=(pass!=0))continue;
                const auto& mesh=part.mesh;
                glPushMatrix();
                r.deform(StorylandMeshCategory::World,storylandMeshEffectBounds(mesh.bounds),mesh.resourceIndex,matrix.data());
                uint32_t material=UINT32_MAX-1;bool drawing=false;
                std::array<float, 2> uvOffset{};
                for(const auto& triangle:mesh.triangles) {
                    if(triangle.a>=mesh.vertices.size()||triangle.b>=mesh.vertices.size()||triangle.c>=mesh.vertices.size())continue;
                    if(material!=triangle.textureId) {
                        if(drawing)glEnd();material=triangle.textureId;
                        const auto* source=frame[asset.sourceArea].resolveTexture(int32_t(material),mesh,owner);
                        GLuint texture=r.worldTexture(session,asset.sourceArea,source);
                        if(!source)texture=r.namedWorldTexture(session,browser,material,source);
                        uvOffset = storylandWorldUvOffset(texture ? source : nullptr, uvSeconds);
                        bool fractional=false;
                        if(source&&part.transparent) {
                            auto& known=frame[asset.sourceArea].fractionalAlpha;
                            auto alpha=known.find(source);
                            if(alpha==known.end()) {
                                size_t count=0;const size_t pixels=source->rgba.size()/4;
                                for(size_t pixel=0;pixel<pixels;++pixel)
                                    if(source->rgba[pixel*4+3]>8&&source->rgba[pixel*4+3]<247)++count;
                                alpha=known.emplace(source,count>=std::max<size_t>(32,pixels/5)).first;
                            }
                            fractional=alpha->second;
                        }
                        const GLuint program=r.programs[size_t(part.transparent&&texture&&!fractional?
                            StorylandShaderKind::Vegetation:StorylandShaderKind::World)];
                        setProgram(program,sky,texture!=0,&r.lightEye,&r.upEye,r.rain);
                        const GLint scale=glGetUniformLocation(program,"uPs2ColorScale");
                        if(scale>=0)glUniform1f(scale,texture?255.0f/128.0f:1.0f);
                        bindTexture(program,texture,r.whiteTexture);
                        const float emission=storylandPortableEmission(source);
                        glUniform1f(glGetUniformLocation(program,"uEmissiveStrength"),emission);
                        if(fractional) {
                            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
                            glDepthMask(GL_FALSE);glDisable(GL_ALPHA_TEST);
                        } else {
                            glDisable(GL_BLEND);glDepthMask(GL_TRUE);
                            if(texture) {glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.08f);}
                            else glDisable(GL_ALPHA_TEST);
                        }
                        glBegin(GL_TRIANGLES);drawing=true;
                    }
                    for(uint32_t index:{triangle.a,triangle.b,triangle.c}) {
                        const auto& vertex=mesh.vertices[index];
                        if((gPortableShaderProfile==0||gPortableShaderProfile==2)&&vertex.hasPs2Color)
                            glColor4ubv(vertex.ps2Color.data());
                        else if(vertex.hasPrelight)glColor4ub(vertex.red,vertex.green,vertex.blue,vertex.alpha);
                        else glColor4f(1,1,1,1);
                        if(gPortableShaderProfile==0||gPortableShaderProfile==2)
                            glNormal3f(vertex.ps2Nx,vertex.ps2Ny,vertex.ps2Nz);
                        else glNormal3f(vertex.nx,vertex.ny,vertex.nz);
                        glTexCoord2f(vertex.u + uvOffset[0], vertex.v + uvOffset[1]);
                        glVertex3f(vertex.x,vertex.y,vertex.z);
                    }
                }
                if(drawing)glEnd();
                glPopMatrix();
            }
            glPopMatrix();continue;
        }
        const auto found=session.missionAssets.find(object.model);
        if(found==session.missionAssets.end())continue;
        StorylandModelFile selectedAtomic;
        const StorylandModelFile* selectedModel = nullptr;
        if (found->second.model.modelKind() == StorylandModelKind::SimpleModel) {
            size_t atomic = 0;
            std::string error;
            if (!storylandNativeObjectAtomicIndex(session.archive.unpackedBytes(),
                    session.archive.gameVariant(), object.model, object.damage.renderDamaged, atomic, error)) {
                session.mission.state = StorylandMissionState::Unsupported;
                session.mission.diagnostic = "Script object " + std::to_string(object.handle) + ": " + error;
                statusMessage = session.mission.diagnostic;
                return;
            }
            selectedAtomic = found->second.model;
            if (!selectedAtomic.selectSimpleModelAtomic(atomic, error)) {
                session.mission.state = StorylandMissionState::Unsupported;
                session.mission.diagnostic = "Script object " + std::to_string(object.handle) +
                    " atomic " + std::to_string(atomic) + ": " + error;
                statusMessage = session.mission.diagnostic;
                return;
            }
            selectedModel = &selectedAtomic;
        }
        const auto& points = selectedModel ? selectedModel->previewPoints() : found->second.model.previewPoints();
        const int weaponId=gPortableShaderProfile==2?storylandPickupWeaponNativeId(object.model,found->second.name,
            session.runtime.weapons,[&](const std::string& name) {
                return env.modelIdByName?env.modelIdByName(name):-1;
            }):-1;
        glPushMatrix();glTranslatef(object.x,object.y,object.z);glRotatef(object.heading,0,0,1);
        glRotatef(object.rotationX,1,0,0);glRotatef(object.rotationY,0,1,0);
        r.actor(found->second,points,actorTextures,palette,255,nullptr,nullptr,
            weaponId>=0?StorylandMeshCategory::Weapons:StorylandMeshCategory::World,selectedModel);glPopMatrix();
    }
    if(session.escalatorStepAsset) {
        const auto& asset=*session.escalatorStepAsset;
        for(const auto& escalator:session.runtime.escalators.escalators)if(escalator.active)
            for(const auto& step:escalator.steps) {
                if(step.worldIndex>=session.archive.nativeWorld().instances.size())continue;
                const auto& instance=session.archive.nativeWorld().instances[step.worldIndex];
                glPushMatrix();glMultMatrixf(instance.matrix.data());
                r.actor(asset,asset.model.previewPoints(),actorTextures,palette);glPopMatrix();
            }
    }
    if(session.active())for(const auto& projectile:session.runtime.projectiles) {
        if(projectile.kind!=StorylandProjectileKind::SeaMine||projectile.weapon>=session.runtime.weapons.size())continue;
        const size_t visual=session.runtime.weapons[projectile.weapon].projectileVisual;
        if(visual>=session.weaponAssets.size())continue;
        const auto& asset=session.weaponAssets[visual];
        glPushMatrix();
        glTranslatef(projectile.position.x,projectile.position.y,projectile.position.z);
        glRotatef(projectile.yaw,0,0,1);
        r.actor(asset,asset.model.previewPoints(),actorTextures,palette,255,nullptr,nullptr,
            StorylandMeshCategory::Weapons);
        glPopMatrix();
    }
    std::vector<StorylandRuntimeParticle> pickupGlows;
    if(session.active()) {
        const float time=float(env.gameMilliseconds)*.001f;
        const auto renderPickup=[&](const StorylandPlayTestAsset& asset,StorylandNativePoint position,
            int weaponId,int pickupType,const StorylandVcsPickupVisual* nativeVisual) {
            const float dx=position.x-eye.x,dy=position.y-eye.y;
            if(dx*dx+dy*dy>140.0f*140.0f)return;
            if(session.vcs()&&(!nativeVisual||!nativeVisual->valid))return;
            glPushMatrix();
            if(session.vcs())glMultMatrixf(nativeVisual->matrix.data());
            else {
                glTranslatef(position.x,position.y,position.z);
                glRotatef(storylandLcsPickupHeading(env.gameMilliseconds,pickupType,asset.name),0,0,1);
            }
            r.actor(asset,asset.model.previewPoints(),actorTextures,palette,255,nullptr,nullptr,
                weaponId>=0?StorylandMeshCategory::Weapons:StorylandMeshCategory::World);glPopMatrix();
            if(session.vcs())return;
            StorylandRuntimeParticle glow;
            glow.position=position;glow.kind=StorylandParticleKind::PickupGlow;
            glow.size=.5f+.05f*std::sin(time*4.0f);glow.nativePhysics=true;
            glow.nativeColor=storylandPickupGlowColor(session.vcs(),weaponId,asset.name);
            pickupGlows.push_back(glow);
        };
        if(session.runtime.pickupsEnabled)for(size_t index=0;index<session.runtime.pickups.size();++index) {
            const auto& pickup=session.runtime.pickups[index];
            if(pickup.respawn>0||pickup.ammunition<=0||pickup.weapon>=session.runtime.weapons.size())continue;
            const auto& weapon=session.runtime.weapons[pickup.weapon];
            if(weapon.visual>=session.weaponAssets.size())continue;
            auto position=pickup.position;position.z+=.5f;
            const auto* nativeVisual=index<r.pickupVisuals.size()?&r.pickupVisuals[index]:nullptr;
            renderPickup(session.weaponAssets[weapon.visual],position,weapon.nativeId,2,nativeVisual);
        }
        for(size_t index=0;index<env.pickups.size();++index) {
            const auto& pickup=env.pickups[index];
            if(pickup.unavailable || pickup.removed || pickup.type==0 ||
                (!session.vcs() && pickup.area!=session.runtime.visibleArea))continue;
            const int32_t assetKey=pickup.modelNameOverride.empty()?pickup.model:INT32_MIN+std::max(0,pickup.handle);
            const auto asset=session.missionAssets.find(assetKey);
            if(asset==session.missionAssets.end())continue;
            if(!session.vcs()&&!storylandLcsPickupVisible(asset->second.name,
                env.lcsMissionFlagSet&&env.lcsMissionFlagSet(),session.runtime.killFrenzy.active()))continue;
            const int weaponId=storylandPickupWeaponNativeId(pickup.model,asset->second.name,
                session.runtime.weapons,[&](const std::string& name) {
                    return env.modelIdByName?env.modelIdByName(name):-1;
                });
            const size_t nativeIndex=session.runtime.pickups.size()+index;
            const auto* nativeVisual=nativeIndex<r.pickupVisuals.size()?&r.pickupVisuals[nativeIndex]:nullptr;
            renderPickup(asset->second,{pickup.x,pickup.y,pickup.z},weaponId,pickup.type,nativeVisual);
        }
    }
    glUseProgram(0);glDepthMask(GL_TRUE);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_TEXTURE_2D);
    performanceFrame.next(StorylandRenderPerformance::ProjectedShadows);
    if(session.active() && !nativeCutscene && env.groundHeightAt) {
        glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_CURRENT_BIT|GL_TEXTURE_BIT|GL_POLYGON_BIT);
        const auto selectTexture=[](GLenum unit) {glActiveTexture(unit);};
        const auto shadowTextureState=storylandBeginActorShadowTextureState(true,selectTexture);
        glUseProgram(0);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);
        glEnable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
#ifndef __ANDROID__
        glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(0.0f,-2.0f);
#endif
        const float strength=r.sky.hasNativeTimecycle()?r.sky.state().shadowStrength:128.0f;
        const auto drawActorShadow=[&](const StorylandActorShadow& shadow) {
            if(env.sphereOnScreen&&!env.sphereOnScreen(shadow.position,
                2.0f+std::hypot(shadow.halfWidth,shadow.halfLength)))return;
            const auto texture=r.particleTextures.find(shadow.texture);
            if(texture==r.particleTextures.end()||!texture->second)return;
            glBindTexture(GL_TEXTURE_2D,texture->second);
            glColor4f(0,0,0,std::clamp(shadow.opacity,0.0f,1.0f));
            glBegin(GL_TRIANGLES);
            storylandVisitNativeActorShadow(shadow,session.archive.nativeWorld(),[](const auto& triangle) {
                for(const auto& vertex:triangle) {
                    glTexCoord2fv(vertex.uv.data());glVertex3fv(vertex.position.data());
                }
            });
            glEnd();
        };
        storylandVisitVehicleActorShadows(session.runtime,env,session.vehicles,session.vehicleAssets.size(),eye,strength,drawActorShadow);
        storylandVisitPedActorShadows(session.runtime,env,session.pedestrians,session.playerPosition,
            session.playerHeading,eye,strength,drawActorShadow,
            {r.sky.state().sunDirectionX,r.sky.state().sunDirectionY,r.sky.state().sunDirectionZ});
        if(session.coopNetwork().connected()) {
            const auto peer=session.coopDisplay();
            if(peer.visible()&&!peer.seated()&&peer.area==env.area) {
                StorylandActorShadow shadow{{peer.x,peer.y,peer.z},peer.heading,.6f,.6f,
                    std::clamp(strength/255.0f,0.0f,1.0f),"shad_ped"};
                storylandSetPedShadowBasis(shadow,storylandPedShadowBasis({r.sky.state().sunDirectionX,
                    r.sky.state().sunDirectionY,r.sky.state().sunDirectionZ}));
                drawActorShadow(shadow);
            }
        }
        storylandEndActorShadowTextureState(shadowTextureState,selectTexture);
    }
    if (session.active() && session.vcs() && env.groundHeightAt && !env.frameScriptShadows.empty()) {
        glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_CURRENT_BIT | GL_TEXTURE_BIT | GL_POLYGON_BIT);
        glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);
        glEnable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#ifndef __ANDROID__
        glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(0.0f,-2.0f);
#endif
        for (const auto& entry : env.frameScriptShadows) {
            const auto& shadow = entry.second;
            if (shadow.intensity <= 0 || shadow.radius <= 0 ||
                std::hypot(shadow.position[0] - eye.x, shadow.position[1] - eye.y) >
                    float(shadow.pedestrian ? 240 : 200)) continue;
            const auto texture = r.particleTextures.find(shadow.pedestrian ? "shad_ped" : "fight_ring");
            if (texture == r.particleTextures.end() || !texture->second) continue;
            glBindTexture(GL_TEXTURE_2D, texture->second);
            glColor4ub(shadow.color[0], shadow.color[1], shadow.color[2],
                uint8_t(std::clamp(int(shadow.intensity), 0, 255)));
            storylandProjectScriptShadow(shadow.position, shadow.radius, env.groundHeightAt,
                [](const std::array<StorylandScriptShadowVertex, 4>& tile) {
                    glBegin(GL_QUADS);
                    for (const auto& vertex : tile) {
                        glTexCoord2fv(vertex.uv.data());
                        glVertex3fv(vertex.position.data());
                    }
                    glEnd();
                });
        }
        glPopAttrib();
    }
    glUseProgram(0);
#ifndef __ANDROID__
    if(session.active()&&session.vcs())
        r.underwater.draw(immersion,SDL_GL_GetCurrentContext(),&SDL_GL_GetProcAddress);
#endif
    if(session.active()&&!session.vcs()) {
        glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_LINE_BIT|GL_COLOR_BUFFER_BIT);
        glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(1);glColor4ub(128,128,128,100);glBegin(GL_LINES);
        for(const auto& rope:env.lcsExtendedWorld.swatRopes)if(rope.active)
            for(size_t index=1;index<rope.points.size();++index) {
                glVertex3fv(rope.points[index-1].data());glVertex3fv(rope.points[index].data());
            }
        glEnd();glPopAttrib();
    }
    performanceFrame.next(StorylandRenderPerformance::CoronasMarkers);
    if (session.active()) {
        const auto& sun=r.sky.state();
        const StorylandNativePoint sunPosition{eye.x+sun.sunDirectionX*150.0f,
            eye.y+sun.sunDirectionY*150.0f,eye.z+sun.sunDirectionZ*150.0f};
        const bool clearSun=storylandTraceNativeBuildings(session.archive.nativeWorld(),sunPosition,eye)>=.999f;
        r.sky.drawNativeSunCoronas(clearSun,env.gameMilliseconds,env.nativeScriptTimeStep,
            session.runtime.visibleArea!=0);
        const auto& clock=worldLightClock;
        storylandUpdateC2dLights(r.c2dLights, session.archive.leeds2dfxEffects(),
            session.archive.leeds2dfxWorldInstances(), session.archive.nativeWorld(), eye,
            clock, sky.spriteSize,
            [&](const StorylandNativePoint& point, float radius) {
                return !env.sphereOnScreen || env.sphereOnScreen({point.x, point.y, point.z}, radius);
            }, [&](const StorylandNativePoint& point, const StorylandNativePoint& camera) {
                return storylandTraceNativeBuildings(session.archive.nativeWorld(), point, camera) >= .999f;
            });
        glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT);
        glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_ALPHA_TEST); glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE); glDepthMask(GL_FALSE);
        const StorylandNativePoint right{float(view[0]), float(view[4]), float(view[8])};
        const StorylandNativePoint upDirection{float(view[1]), float(view[5]), float(view[9])};
        std::array<float,16> coronaView, coronaProjection;
        std::copy(view, view+16, coronaView.begin());
        std::copy(projection, projection+16, coronaProjection.begin());
        glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();glMatrixMode(GL_MODELVIEW);
        storylandForEachC2dCoronaSprite(r.c2dLights, coronaView.data(), coronaProjection.data(), height,
            [&](const StorylandC2dCoronaSprite& sprite) {
            const auto& center = sprite.position;
            const auto texture = r.particleTextures.find(sprite.texture);
            const GLuint handle = texture == r.particleTextures.end() ? 0 : texture->second;
            if (sprite.depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (handle) {glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, handle);}
            else glDisable(GL_TEXTURE_2D);
            glColor4f(sprite.color[0]*sprite.alpha, sprite.color[1]*sprite.alpha, sprite.color[2]*sprite.alpha, 1);
            if (handle) {
                glBegin(GL_QUADS);
                for (const auto& corner : std::array<std::array<float,2>,4>{{{-1,-1},{1,-1},{1,1},{-1,1}}}) {
                    const float x=sprite.radius*(corner[0]*std::cos(sprite.rotation)-corner[1]*std::sin(sprite.rotation));
                    const float y=sprite.radius*(corner[0]*std::sin(sprite.rotation)+corner[1]*std::cos(sprite.rotation));
                    glTexCoord2f((corner[0]+1)*.5f, (1-corner[1])*.5f);
                    glVertex3f(center.x+right.x*x+upDirection.x*y,
                        center.y+right.y*x+upDirection.y*y, center.z+right.z*x+upDirection.z*y);
                }
                glEnd();
            }
        },r.sky.previousNativeWeather()==3u?1.0f:0.0f);
        storylandUpdateScriptCoronaFrame(r.scriptCoronas,env.scriptCoronas,env.frameScriptCoronas,
            session.vcs(),env.gameMilliseconds,env.nativeScriptTimeStep,coronaView.data(),coronaProjection.data());
        glEnable(GL_DEPTH_TEST);
        storylandForEachScriptCoronaSprite(r.scriptCoronas,coronaView.data(),coronaProjection.data(),height,
            sky.spriteSize,r.sky.previousNativeWeather()==3u?1.0f:0.0f,[&](const StorylandScriptCoronaSprite& sprite) {
            const char* name=storylandCoronaTextureName(sprite.texture);
            if(!name)return;
            const auto texture=r.particleTextures.find(name);
            if(texture==r.particleTextures.end()||!texture->second)return;
            glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture->second);
            glColor4f(sprite.color[0],sprite.color[1],sprite.color[2],1);
            const float cosine=std::cos(sprite.rotation),sine=std::sin(sprite.rotation);
            glBegin(GL_QUADS);
            for(const auto& corner:std::array<std::array<float,2>,4>{{{-1,-1},{1,-1},{1,1},{-1,1}}}) {
                const float x=sprite.halfWidth*(corner[0]*cosine-corner[1]*sine);
                const float y=sprite.halfHeight*(corner[0]*sine+corner[1]*cosine);
                glTexCoord2f((corner[0]+1)*.5f,(1-corner[1])*.5f);
                glVertex3f(sprite.position[0]+right.x*x+upDirection.x*y,
                    sprite.position[1]+right.y*x+upDirection.y*y,
                    sprite.position[2]+right.z*x+upDirection.z*y);
            }
            glEnd();
        });
        glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(GL_MODELVIEW);
        glPopAttrib();
        drawMissionWorldMarkers(session,eye,r.particleTextures);
    }
    performanceFrame.next(StorylandRenderPerformance::Hud);
    if (session.active() && !session.paused && !nativeCutscene && !env.widescreen && !env.camera.active) {
        StorylandImGuiViewport hudViewport(hudRectangle);
        auto* draw = ImGui::GetBackgroundDrawList();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (!session.vcs() && session.runtime.manualAimActive && session.runtime.equippedWeapon < session.runtime.weapons.size()) {
            const int nativeWeapon = session.runtime.weapons[session.runtime.equippedWeapon].nativeId;
            for (const auto& quad : storylandManualAimOverlay(nativeWeapon, session.runtime.manualAimMode,
                display.x / std::max(1.f, display.y) > 1.5f)) {
                const auto name = std::find(Resources::opticNames.begin(), Resources::opticNames.end(), quad.texture);
                if (name == Resources::opticNames.end()) continue;
                const GLuint texture = r.opticTextures[size_t(name - Resources::opticNames.begin())];
                if (!texture) continue;
                std::array<ImVec2,4> corners;
                for (size_t corner = 0; corner < corners.size(); ++corner)
                    corners[corner] = {quad.corners[corner].x * display.x / 480,
                        quad.corners[corner].y * display.y / 272};
                draw->AddImageQuad(static_cast<ImTextureID>(texture), corners[0], corners[1], corners[2], corners[3],
                    quad.uv[0], quad.uv[1], quad.uv[2], quad.uv[3], IM_COL32_WHITE);
            }
        }
    }
    if(session.active() && (!session.paused || session.runtime.pauseMap)) {
        StorylandImGuiViewport hudViewport(hudRectangle);
        r.coopPresentation.update(session.coopNetwork(), session.coopTime());
        r.worldNames.update(session.archive.unpackedBytes(), session.archive.nativeWorld().zones,
            session.missionGxt, session.runtime, session.environment, session.playerPosition, session.vcs());
        helpBindings.driving=session.runtime.occupiedVehicle>=0||session.environment.remoteVehicle>=0||
            session.environment.remoteControl.vehicle>=0;
        helpBindings.motorcycle=storylandControlledVehicleIsMotorcycle(session.runtime);
        storylandDrawPlayTestHud(session.runtime,session.archive.nativeWorld().paths,session.pedestrians,
            session.vehicles,session.playerPosition,session.playerHeading,session.camera.yaw,session.vcs(),
            false,session.environment.cutscene.playing,&r.hud,&session.environment,&session.camera,0,
            session.environment.invertedMap,nullptr,&helpBindings);
        storylandDrawCoopOverhead(ImGui::GetForegroundDrawList(), &r.hud, session.camera,
            session.archive.nativeWorld(), session.coopNetwork(), r.coopPresentation,
            session.runtime, session.playerPosition, session.coopTime(), session.environment.invertedMap);
        r.worldNames.draw(ImGui::GetForegroundDrawList(), &r.hud, session.runtime, session.environment, session.vcs());
        if (!session.vcs() && !session.runtime.pauseMenu && !session.runtime.paused) {
            const auto translate = [&session](const std::string& key, const std::string& table) {
                return session.translateMissionText(key, table);
            };
            auto* draw = ImGui::GetForegroundDrawList();
            const auto display = ImGui::GetIO().DisplaySize;
            storylandDrawLcsMessages(draw, &r.hud, session.environment, display, false, translate);
            storylandDrawLcsScriptTexts(draw, &r.hud, session.environment, display, translate);
            storylandDrawLcsMessages(draw, &r.hud, session.environment, display, true, translate);
        }
    }
    performanceFrame.next(StorylandRenderPerformance::Particles);
    if(session.active()) {
        if(session.environment.rubbishEnabled && session.runtime.visibleArea==0 &&
           !session.runtime.playerWater.active && !session.environment.cutscene.playing) {
            glUseProgram(0);
            const auto rubbishFlag=session.environment.statistics.find("LcsRubbishFourthTexture");
            const bool fourthTexture=session.vcs() ||
                (rubbishFlag!=session.environment.statistics.end()&&rubbishFlag->second!=0);
            const auto selectTexture=[](GLenum unit){glActiveTexture(unit);};
            const auto renderState=storylandBeginTransparentPass(true,selectTexture);
            storylandDrawRubbish(session.runtime.rubbish,[&](const char* name) {
                const auto found=r.particleTextures.find(name);
                return found==r.particleTextures.end()?GLuint(0):found->second;
            },fourthTexture);
            storylandEndTransparentPass(renderState,selectTexture);
        }
        const bool aeroFire=storylandPortableAeroProfile(selectedShader);
        const auto skidTexture=r.particleTextures.find("skid");
        drawRoadSkids(session.runtime,skidTexture==r.particleTextures.end()?0:skidTexture->second);
        drawParticles(session.runtime.particles,r.particleTextures,session.vcs(),
            aeroFire,session.animationSeconds(),&session.runtime.waterCannons,session.runtime.visibleArea,&session.runtime,
            cameraView.nativeFieldOfView);
        glUseProgram(0);
        const bool emission=selectedShader==6||selectedShader==7;
        storylandDrawWeaponTraces(session.runtime.shotTraces,emission);
        if(emission)
            storylandDrawMuzzleEmission(session.runtime,session.archive.nativeWorld(),{eye.x,eye.y,eye.z},
                [&](const char* name) {
                    const auto found=r.particleTextures.find(name);
                    return found==r.particleTextures.end()?GLuint(0):found->second;
                });
        if(session.vcs()) {
            glUseProgram(0);
            storylandDrawVcsPickupVisuals(r.pickupVisuals,session.archive.nativeWorld(),
                {eye.x,eye.y,eye.z},[&](const char* name) {
                    const auto found=r.particleTextures.find(name);
                    return found==r.particleTextures.end()?GLuint(0):found->second;
                });
        } else drawParticles(pickupGlows,r.particleTextures,false,aeroFire,session.animationSeconds());
    }
    performanceFrame.next(StorylandRenderPerformance::PostEffects);
    glUseProgram(0);
#ifndef __ANDROID__
    if(gPortableShaderProfile==2 && r.sky.game()==StorylandSkyGame::Vcs && r.sky.hasNativeTimecycle()) {
        const auto& sky=r.sky.state();
        const auto effects=storylandPs2PostEffects(trails?sky.radiosityIntensity:0.0f,
            trails?sky.radiosityLimit:0.0f,sky.frameGlowIntensity,sky.frameGlowRadius,
            {sky.frameFilter.r,sky.frameFilter.g,sky.frameFilter.b});
        r.ps2PostEffects.draw(effects,SDL_GL_GetCurrentContext(),&SDL_GL_GetProcAddress);
    }
#endif
    const auto ps2CaptureStarted=std::chrono::steady_clock::now();
    r.capturePs2Environment({target.x-eye.x,target.y-eye.y,target.z-eye.z});
    r.ps2EnvironmentCaptureMilliseconds=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-ps2CaptureStarted).count();
#ifndef __ANDROID__
    performanceFrame.next(StorylandRenderPerformance::ScreenEffects);
    StorylandScreenDropletCamera dropletCamera;
    dropletCamera.position={eye.x,eye.y,eye.z};
    dropletCamera.forward={target.x-eye.x,target.y-eye.y,target.z-eye.z};
    const float forwardLength=std::hypot(dropletCamera.forward[0],dropletCamera.forward[1],
        dropletCamera.forward[2]);
    if(forwardLength>.00001f)for(auto& axis:dropletCamera.forward)axis/=forwardLength;
    dropletCamera.right={dropletCamera.forward[1]*up.z-dropletCamera.forward[2]*up.y,
        dropletCamera.forward[2]*up.x-dropletCamera.forward[0]*up.z,
        dropletCamera.forward[0]*up.y-dropletCamera.forward[1]*up.x};
    const float rightLength=std::hypot(dropletCamera.right[0],dropletCamera.right[1],dropletCamera.right[2]);
    if(rightLength>.00001f)for(auto& axis:dropletCamera.right)axis/=rightLength;
    dropletCamera.up={dropletCamera.right[1]*dropletCamera.forward[2]-dropletCamera.right[2]*dropletCamera.forward[1],
        dropletCamera.right[2]*dropletCamera.forward[0]-dropletCamera.right[0]*dropletCamera.forward[2],
        dropletCamera.right[0]*dropletCamera.forward[1]-dropletCamera.right[1]*dropletCamera.forward[0]};
    dropletCamera.rain=std::clamp(env.weatherState.rain,0.0f,1.0f);
    dropletCamera.rainBlocked=session.runtime.visibleArea!=0;
    if(session.runtime.extendedAndSaFeatures&&dropletCamera.rain>0&&!dropletCamera.rainBlocked)
        dropletCamera.rainBlocked=storylandTraceNativeSegment(session.archive.nativeWorld(),eye,
            {eye.x,eye.y,eye.z+20.0f})<.999f;
    dropletCamera.firstPerson=session.runtime.occupiedVehicle>=0&&session.camera.vehicleCameraActive&&
        session.camera.vehicleMode==0;
    dropletCamera.movingEnabled=!session.paused&&!session.runtime.paused&&!session.runtime.pauseMenu;
    dropletCamera.splashPosition={session.runtime.playerPosition.x,session.runtime.playerPosition.y,
        session.runtime.playerPosition.z};
    dropletCamera.splashPositionValid=session.active();
    if(session.runtime.extendedAndSaFeatures)
        dropletCamera.sprayStrength=storylandScreenWaterSpray(dropletCamera,session.runtime.waterCannons);
    r.screenDroplets.draw(session.active()&&session.vcs()&&session.runtime.extendedAndSaFeatures&&
        !nativeCutscene&&!env.cutscene.playing,session.runtime.elapsed,session.runtime.playerCameraWaterSerial,
        session.runtime.playerCameraBloodSerial,session.runtime.playerCameraWaterDroplets,
        session.runtime.playerCameraBloodDroplets,dropletCamera,SDL_GL_GetCurrentContext(),
        &glActiveTexture,&glMultiTexCoord2f);
#endif
    r.speedBlur.draw(viewRectangle.width,viewRectangle.height,speedCamera?session.camera.speedEffects.blur:0,
        viewRectangle.x,viewRectangle.openGlY(height));
#ifndef __ANDROID__
    if(graphicsQuality.fxaa&&graphicsQualityApplies()&&!nativeCutscene)
        r.fxaa.draw(SDL_GL_GetCurrentContext(),&SDL_GL_GetProcAddress);
#endif
    // Draw the world boxes after the blur so the debug outlines stay readable.
    if(showBounds) {
        const auto& world=session.archive.nativeWorld();
        glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_TEXTURE_2D);
        for(const auto& instance:world.instances) {
            const auto found=world.collisionModels.find(instance.modelId);
            if(found==world.collisionModels.end()||!world.collisionModelEnabled(found->second))continue;
            const auto& model=found->second;
            const StorylandMeshBounds bounds{{model.minimum.x,model.minimum.y,model.minimum.z},
                {model.maximum.x,model.maximum.y,model.maximum.z},true};
            if(!frustum.visible(bounds,instance.matrix.data()))continue;
            glPushMatrix();glMultMatrixf(instance.matrix.data());
            drawBoundingBox(model.minimum,model.maximum);
            glPopMatrix();
        }
        glPopAttrib();
    }
    storylandDrawVehicleEnvironmentViewer(vehicleEnvironment);
    if(performance.enabled) { performance.placements=r.renderedPlacements; performance.actors=r.renderedActors; }
    statusMessage="Native scene: "+std::to_string(r.renderedPlacements)+" placements, "+std::to_string(r.renderedActors)+" actors.";
    if((gPortableShaderProfile==0||gPortableShaderProfile==2)&&!r.ps2EnvironmentDiagnostic.empty())
        statusMessage+=" PS2 reflection: "+r.ps2EnvironmentDiagnostic;
}

bool StorylandLinuxRenderer::setShaderProfile(int profile, std::string& error) {
    if (!storylandSelectableShaderProfile(profile)) {
        error = "Invalid shader selection."; return false;
    }
    if (!resources) {
        selectedShader = profile;
        gPortableShaderProfile=storylandShaderRenderProfile(profile < 0 ? nativeShaderProfile : profile);
        if(frontendArtwork)frontendArtwork->setPcMode(pcShaderEnabled());
        error.clear();
        return true;
    }
    const int effectiveShader = storylandShaderRenderProfile(profile < 0 ? nativeShaderProfile : profile);
    std::array<GLuint,5> programs{};
    for (size_t kind = 0; kind < programs.size(); ++kind) {
        if (!compileProgram(resources->sky.game()==StorylandSkyGame::Vcs, StorylandShaderKind(kind), programs[kind], error, effectiveShader)) {
            for (auto program : programs) if (program) glDeleteProgram(program);
            return false;
        }
    }
    if (storylandPortableAeroProfile(profile) && !resources->shadowProgram) {
        if(!compileProgram(resources->sky.game()==StorylandSkyGame::Vcs,
            StorylandShaderKind::World,resources->shadowProgram,error,13)){
            for(const auto program:programs)if(program)glDeleteProgram(program);
            return false;
        }
    }
    if (storylandPortableWaterProfile(profile) && !resources->aeroWaterProgram) {
        GLuint waterProgram=0;
        if (!compileProgram(resources->sky.game()==StorylandSkyGame::Vcs,
            StorylandShaderKind::World,waterProgram,error,8)) {
            for (const auto program:programs) if(program)glDeleteProgram(program);
            return false;
        }
        resources->aeroWaterProgram=waterProgram;
    }
    for (auto program : resources->programs) if (program) glDeleteProgram(program);
    resources->programs = programs;
    resources->neo = profile == 4;
    resources->sky.setSanAndreasStyle(profile==9);
    resources->sky.setPspLighting(effectiveShader==3);
    resources->ps2EnvironmentReady=false;
    resources->ps2EnvironmentDiagnostic.clear();
    if (profile!=7) {
        resources->waterReaction.clear();
        resources->lastWaterTime=UINT32_MAX;
        resources->waterPositionInitialized=false;
    }
    const bool changedPs2=(gPortableShaderProfile==0||gPortableShaderProfile==2)!=
        (effectiveShader==0||effectiveShader==2);
    selectedShader = profile;
    gPortableShaderProfile=effectiveShader;
    resources->hud.setPcMode(pcShaderEnabled());
    resources->applyPcArtwork();
    if(changedPs2) {
        for(auto& area:resources->worldTextures)for(auto& texture:area) {
            if(texture)glDeleteTextures(1,&texture);
            texture=0;
        }
        for(const auto& texture:resources->namedWorldHandles)
            if(texture.second)glDeleteTextures(1,&texture.second);
        resources->namedWorldHandles.clear();
        if(resources->charredVehicleTexture)glDeleteTextures(1,&resources->charredVehicleTexture);
        resources->charredVehicleTexture=resources->charredVehicleImage.rgba.empty()?0:
            uploadSurfaceTexture(resources->charredVehicleImage);
    }
    error.clear();
    return true;
}
