#include "src/platform/android/storyland_android_gl.h"
#include <GLES3/gl3.h>
#include <SDL.h>
#include <jni.h>
#undef glUseProgram
#undef glEnable
#undef glDisable
#undef glDrawArrays
#undef glDrawElements
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "src/platform/android/storyland_android_movie.h"
#include "src/storyland/storyland_opening_movies.h"
#include "src/storyland/storyland_game_files.h"
#include "src/platform/android/storyland_android_resources.h"
#include "src/platform/android/storyland_android_layout.h"
#include "src/platform/android/storyland_android_presentation.h"
#include <fstream>
#include "src/platform/linux/storyland_linux_audio.h"
#include "src/storyland/storyland_vehicle_siren.h"
#include "src/platform/linux/storyland_linux_renderer.h"
#include "src/platform/linux/storyland_loading_presentation.h"
#include "src/graphics/storyland_game_menu.h"
#include "src/re/reVCS/frontend/storyland_vcs_loading_chapter.h"
#include "src/graphics/storyland_mesh_effects_ui.h"
#include "src/graphics/storyland_frontend_widgets.h"
#include "src/graphics/storyland_play_options_ui.h"
#include "src/platform/linux/storyland_linux_session.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/storyland/storyland_pc_input.h"
#include "src/storyland/storyland_bike_controls.h"
#include "src/platform/linux/storyland_portable_coop_panel.h"
#include "src/platform/linux/storyland_portable_chat.h"
#include "src/storyland/storyland_unicode_font.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <map>
#include <mutex>

namespace {
std::mutex requestMutex;
std::string requestedGame, importedResource, importedResourceName, requestedArchive, requestedCompanion;
int importedToken = 0;
struct PhoneMetrics { float density = 1; ImVec4 safe{0,0,0,0}; };
PhoneMetrics phoneMetrics() {
    PhoneMetrics result;
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) return result;
    jclass type = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(type,"getLayoutMetrics","()[F");
    auto values = method ? static_cast<jfloatArray>(env->CallObjectMethod(activity,method)) : nullptr;
    if (values && env->GetArrayLength(values) == 5) {
        float numbers[5]; env->GetFloatArrayRegion(values,0,5,numbers);
        result.density = numbers[0]; result.safe = {numbers[1],numbers[2],numbers[3],numbers[4]};
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (values) env->DeleteLocalRef(values);
    env->DeleteLocalRef(type); env->DeleteLocalRef(activity);
    return result;
}
void setProgramMusic(int track) {
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) return;
    jclass type = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(type,"setProgramMusic","(I)V");
    if (method) env->CallVoidMethod(activity,method,track);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
}
std::string programMusicError() {
    std::string result;
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) return result;
    jclass type = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(type,"getProgramMusicError","()Ljava/lang/String;");
    auto value = method ? static_cast<jstring>(env->CallObjectMethod(activity,method)) : nullptr;
    if (value) {
        const char* text = env->GetStringUTFChars(value,nullptr);
        if (text) { result=text;env->ReleaseStringUTFChars(value,text); }
        env->DeleteLocalRef(value);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
    return result;
}
void chooseResource(int token) {
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) return;
    jclass type = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(type,"chooseResourceFile","(I)V");
    if (method) env->CallVoidMethod(activity,method,token);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(type); env->DeleteLocalRef(activity);
}
void chooseGame() {
  auto *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
  jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
  if (!env || !activity)
    return;
  jclass type = env->GetObjectClass(activity);
  jmethodID method = env->GetMethodID(type, "chooseGame", "()V");
  if (method)
    env->CallVoidMethod(activity, method);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  env->DeleteLocalRef(type);
  env->DeleteLocalRef(activity);
}
std::vector<std::filesystem::path>
movieFiles(const std::filesystem::path &root) {
  std::vector<std::filesystem::path> result;
  std::error_code error;
  std::filesystem::recursive_directory_iterator iterator(
      root, std::filesystem::directory_options::skip_permission_denied, error),
      end;
  for (; iterator != end && !error; iterator.increment(error)) {
    if (iterator.depth() >= 3)
      iterator.disable_recursion_pending();
    if (!iterator->is_regular_file(error))
      continue;
    auto extension = iterator->path().extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (extension == ".pss")
      result.push_back(iterator->path());
    if (result.size() >= 256)
      break;
  }
  std::sort(result.begin(), result.end());
  return result;
}
std::filesystem::path bundledOpeningMovie(const std::string& filename) {
  const auto installed = storylandFindBundledOpeningMovie({}, filename);
  if (!installed.empty()) return installed;
  const auto asset = std::filesystem::path("movies") / filename;
  auto* input = SDL_RWFromFile(asset.u8string().c_str(), "rb");
  if (!input) return {};
  const auto size = SDL_RWsize(input);
  SDL_RWclose(input);
  return size > 0 ? asset : std::filesystem::path{};
}
void exportFiles(const std::filesystem::path &path) {
  auto *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
  jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
  if (!env || !activity)
    return;
  jclass type = env->GetObjectClass(activity);
  jmethodID method =
      env->GetMethodID(type, "exportFiles", "(Ljava/lang/String;)V");
  jstring value = env->NewStringUTF(path.u8string().c_str());
  if (method && value)
    env->CallVoidMethod(activity, method, value);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  if (value)
    env->DeleteLocalRef(value);
  env->DeleteLocalRef(type);
  env->DeleteLocalRef(activity);
}
struct LoadResult {
  std::unique_ptr<StorylandLinuxSession> session;
  std::string error;
};
struct Finger {
  float x = 0, y = 0, startX = 0, startY = 0;
  int action = -1;
};
struct Button {
  const char *label;
  float x, y;
};
const Button buttons[] = {{"Aim", .73f, .58f},  {"Fire", .88f, .58f},
                          {"Jump", .73f, .75f}, {"Enter", .88f, .75f},
                          {"Run", .58f, .88f},  {"Reload", .88f, .91f},
                          {"Radio", .58f, .58f}, {"Horn", .58f, .75f}};
constexpr int RadioAction = 6, HornAction = 7, ButtonCount = 8;
int actionAt(float x, float y, bool driving) {
  for (int i = 0; i < (driving ? ButtonCount : RadioAction); ++i)
    if (std::fabs(x - buttons[i].x) < .065f &&
        std::fabs(y - buttons[i].y) < .065f)
      return i;
  return -1;
}
void controls(const std::map<SDL_FingerID, Finger> &fingers,
              StorylandPlayTestInput &input,
              StorylandPlayTestCameraInput &camera, bool *held) {
  for (const auto &entry : fingers) {
    const auto &finger = entry.second;
    if (finger.action >= 0) {
      held[finger.action] = true;
      continue;
    }
    const float dx = std::clamp((finger.x - finger.startX) * 7, -1.0f, 1.0f);
    const float dy = std::clamp((finger.startY - finger.y) * 7, -1.0f, 1.0f);
    if (finger.startX < .4f && finger.startY > .4f) {
      input.analogMovement = true;
      input.analogHorizontal = dx;
      input.analogForward = dy;
      input.left = dx < -.1f;
      input.right = dx > .1f;
      input.forward = dy > .1f;
      input.backward = dy < -.1f;
    } else {
      camera.left = dx < -.15f;
      camera.right = dx > .15f;
      camera.up = dy > .15f;
      camera.down = dy < -.15f;
    }
  }
  input.running = true;
  input.sprinting = held[4];
  input.handbrake = held[2];
}
void drawControls(const std::map<SDL_FingerID, Finger> &fingers, const StorylandAndroidLayout& layout,
                  bool driving, bool siren) {
  auto *draw = ImGui::GetForegroundDrawList();
  const auto size = layout.size;
  const auto origin = layout.origin;
  const ImVec2 pad{origin.x+size.x * .16f, origin.y+size.y * .76f};
  const float radius = size.y * .13f;
  draw->AddCircleFilled(pad, radius, IM_COL32(30, 15, 30, 100));
  draw->AddCircle(pad, radius, IM_COL32(240, 90, 160, 160), 48, 2);
  for (const auto &entry : fingers)
    if (entry.second.action < 0 && entry.second.startX < .4f &&
        entry.second.startY > .4f)
      draw->AddCircleFilled({origin.x+entry.second.x * size.x, origin.y+entry.second.y * size.y},
                            radius * .3f, IM_COL32(240, 90, 160, 200));
  for (int index = 0; index < (driving ? ButtonCount : RadioAction); ++index) {
    const auto& button = buttons[index];
    ImVec2 center{origin.x+size.x * button.x, origin.y+size.y * button.y};
    draw->AddCircleFilled(center, size.y * .06f, IM_COL32(70, 20, 50, 180));
    draw->AddCircle(center, size.y * .06f, IM_COL32(240, 90, 160, 180), 32, 2);
    const char* label = driving && index == 2 ? "Brake" : driving && index == 3 ? "Exit" :
        siren && index == HornAction ? "Siren" : button.label;
    const auto text = ImGui::CalcTextSize(label);
    draw->AddText({center.x - text.x * .5f, center.y - text.y * .5f},
                  IM_COL32_WHITE, label);
  }
}
} // namespace
extern "C" JNIEXPORT void JNICALL
Java_org_reignsstudio_storyland_StorylandActivity_openGame(JNIEnv *env, jclass,
                                                           jstring path) {
  const char *value = env->GetStringUTFChars(path, nullptr);
  if (!value)
    return;
  {
    std::lock_guard<std::mutex> lock(requestMutex);
    requestedGame = value;
  }
  env->ReleaseStringUTFChars(path, value);
}

extern "C" JNIEXPORT void JNICALL
Java_org_reignsstudio_storyland_StorylandActivity_resourceImported(JNIEnv* env,jclass,jstring path,jint token,jstring name) {
    const char* value = env->GetStringUTFChars(path,nullptr);
    if (!value) return;
    const char* displayName = env->GetStringUTFChars(name,nullptr);
    { std::lock_guard<std::mutex> lock(requestMutex); importedResource=value; importedToken=token;importedResourceName=displayName?displayName:""; }
    if(displayName)env->ReleaseStringUTFChars(name,displayName);
    env->ReleaseStringUTFChars(path,value);
}

extern "C" JNIEXPORT void JNICALL
Java_org_reignsstudio_storyland_StorylandActivity_archiveImported(JNIEnv* env,jclass,jstring primary,jstring companion) {
    const char* input=env->GetStringUTFChars(primary,nullptr);
    if(!input)return;
    const char* attachment=companion?env->GetStringUTFChars(companion,nullptr):nullptr;
    {
        std::lock_guard<std::mutex> lock(requestMutex);
        requestedArchive=input;
        requestedCompanion=attachment?attachment:"";
    }
    if(attachment) env->ReleaseStringUTFChars(companion,attachment);
    env->ReleaseStringUTFChars(primary,input);
}

int main(int argc, char **argv) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
    return 1;
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_Window *window = SDL_CreateWindow(
      "Storyland", 0, 0, 1280, 720,
      SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) {
    SDL_Quit();
    return 1;
  }
  SDL_GLContext gl = SDL_GL_CreateContext(window);
  if (!gl) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_GL_SetSwapInterval(1);
  std::string error;
  if (!storylandAndroidGlInitialize(error)) {
    SDL_Log("%s", error.c_str());
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  int width = 0, height = 0;
  SDL_GetWindowSize(window, &width, &height);
  const float initialDensity = std::clamp(phoneMetrics().density,1.f,4.f);
  ImFontConfig font; font.SizePixels=17*initialDensity;
  const auto fontPath = argc>1 ? std::filesystem::path(argv[1])/"StorylandUI.ttf" : std::filesystem::path("StorylandUI.ttf");
  std::error_code fontError;
  if (std::filesystem::is_regular_file(fontPath, fontError))
    io.FontDefault = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(),font.SizePixels,&font,storylandUnicodeGlyphRanges());
  if (!io.FontDefault) io.FontDefault = storylandAddUnicodeFont(font.SizePixels);
  ImFontConfig pixelConfig;
  pixelConfig.SizePixels=13;
  pixelConfig.OversampleH=pixelConfig.OversampleV=1;
  pixelConfig.PixelSnapH=true;
  ImFont* pixelFont=io.Fonts->AddFontDefault(&pixelConfig);
  float uiZoom = 1;
  ImGui::GetStyle() = storylandAndroidStyle(initialDensity);
  ImGui_ImplSDL2_InitForOpenGL(window, gl);
  ImGui_ImplOpenGL3_Init("#version 300 es");
  auto session = std::make_unique<StorylandLinuxSession>();
  StorylandLinuxRenderer renderer;
  StorylandLinuxAudio audio;
  StorylandLoadingPresentation loadingPresentation;
  StorylandGameMenu gameMenu;
  std::filesystem::path audioModePath;
  if(char* folder=SDL_GetPrefPath("Reigns Studios","Storyland")) {
    audioModePath=std::filesystem::u8path(folder)/"audio_mode.cfg";
    SDL_free(folder);
  }
  if(!audioModePath.empty()) {
    std::ifstream saved(audioModePath);
    int selected=int(StorylandAudioMode::Ps2);
    if(saved>>selected)audio.setMode(storylandAudioModeFromInt(selected));
  }
  StorylandAndroidMovie movie;
  StorylandOpeningMovies<StorylandMoviePlayer> openingSequence;
  StorylandOpeningMovieSkipInput openingSkipInput;
  StorylandAndroidResources resources;
  StorylandAndroidPresentation studio;
  studio.showIntro(uint32_t(SDL_GetPerformanceCounter()));
  int programTrack=0;
  std::vector<std::filesystem::path> movies;
  bool movieVisible = false, startAfterMovie = false, moviePaused = false;
  std::future<LoadResult> loading;
  std::map<SDL_FingerID, Finger> fingers;
  StorylandCoopPanel coopPanel;
  StorylandPortableChat coopChat;
  bool quit = false, panels = true, background = false, previous[ButtonCount]{};
  StorylandPcKeyPress weaponSwitch, cameraCentre;
  StorylandVehicleSirenControls sirenControls;
  renderer.helpBindings.keyboard=false;
  SDL_GameController *controller = nullptr;
  for (int i = 0; i < SDL_NumJoysticks(); ++i)
    if (SDL_IsGameController(i)) {
      controller = SDL_GameControllerOpen(i);
      break;
    }
  if (argc > 1) {
    std::error_code ec;
    std::filesystem::current_path(argv[1], ec);
  }
  if (argc > 2) {
    std::lock_guard<std::mutex> lock(requestMutex);
    requestedGame = argv[2];
  }
  {std::ifstream preferences("storyland-ui.cfg"); if(preferences)preferences>>uiZoom;}
  const auto bind = [&]() {
    session->preloadWorldScene = [&](const auto &p, std::string &e) {
      return renderer.loadScene(*session, p, e);
    };
    session->cutsceneElapsedMilliseconds = [&](uint32_t ms) {
      audio.synchronize(*session);
      return audio.advance(ms);
    };
    session->currentRadioChannel = [&]() {
      return audio.currentRadioChannel(session->vcs());
    };
  };
  const auto refreshLoadingChapter = [&]() {
    if(!loadingPresentation.active()||!session->active()||!session->vcs())return;
    const auto& globals=session->program.active()?
        session->program.persistentSaveGlobals():session->mission.globals;
    loadingPresentation.setChapter(
        storylandReadVcsLoadingChapter(session->script.rawBytes(),globals).value_or(1));
  };
  const auto startPlay = [&](bool game) -> bool {
    bool started=false;
    gameMenu.close();
    audio.stop();
    error.clear();
    try {
      audio.setEnabled(false);
      loadingPresentation.start(session->gameRoot(),session->vcs());
      if (game ? session->startGame(error) : session->startTest(error)) {
        refreshLoadingChapter();
        loadingPresentation.ready();
        started=true;
        panels = false;
        fingers.clear();
      } else {
        loadingPresentation.cancel();
        panels=true;
      }
    } catch (const std::bad_alloc &) {
      error = "Storyland failed to allocate enough memory. Play was stopped.";
      loadingPresentation.cancel();
      session->stopGame();
      panels=true;
    } catch (const std::exception &exception) {
      error = exception.what();
      loadingPresentation.cancel();
      session->stopGame();
      panels=true;
    }
    return started;
  };
  const auto currentMovie = [&]() -> StorylandMoviePlayer* {
    return startAfterMovie ? openingSequence.current() : &movie;
  };
  const auto beginMovie = [&](const std::filesystem::path &path) {
    openingSequence.stop();
    openingSkipInput.reset();
    openingSkipInput.consume();
    startAfterMovie = false;
    studio.close();
    gameMenu.close();
    audio.stop();
    audio.setEnabled(false);
    audio.releaseOutput();
    session->paused = true;
    movie.open(path);
    movieVisible = true;
    moviePaused = false;
    fingers.clear();
  };
  const auto finishOpening = [&]() {
    gameMenu.showMain();
    panels=false;
    session->paused=true;
    fingers.clear();
    ImGui::SetWindowFocus(nullptr);
  };
  const auto finishMovie = [&]() {
    if (startAfterMovie && openingSequence.active()) {
      openingSequence.skip();
      openingSkipInput.consume();
      moviePaused = false;
      if (openingSequence.active()) return;
    }
    const bool finishedOpening = startAfterMovie;
    openingSequence.stop();
    movie.stop();
    movieVisible = startAfterMovie = moviePaused = false;
    openingSkipInput.finish();
    audio.setEnabled(true);
    fingers.clear();
    if (finishedOpening) finishOpening();
    else {
      panels = true;
      session->paused = session->active();
    }
  };
  const auto beginOpening = [&]() {
    gameMenu.close();
    movie.stop();
    openingSequence.stop();
    openingSkipInput.reset();
    openingSkipInput.consume();
    studio.close();
    setProgramMusic(0);
    programTrack = 0;
    session->stopGame();
    audio.stop();
    audio.setEnabled(false);
    audio.releaseOutput();
    bool pal = false;
    for (const auto& folder : storylandNativeGameFolders(session->gameRoot())) {
      std::error_code scanError;
      for (std::filesystem::directory_iterator entry(folder, scanError), end;
           !scanError && entry != end; entry.increment(scanError)) {
        auto name = entry->path().filename().u8string();
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char value) { return char(std::toupper(value)); });
        if (name.rfind("SLES", 0) == 0 || name.rfind("SCES", 0) == 0) pal = true;
      }
    }
    const auto resolveRetail = [&](std::string wanted) {
      const auto retailName = wanted;
      std::transform(wanted.begin(), wanted.end(), wanted.begin(),
          [](unsigned char value) { return char(std::tolower(value)); });
      std::filesystem::path fallback;
      for (const auto& file : movies) {
        auto name = file.filename().u8string();
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char value) { return char(std::tolower(value)); });
        if (name == wanted) { fallback = file; break; }
      }
      return session->archive.resolveGameFile(retailName, fallback);
    };
    openingSequence.begin(storylandResolveOpeningMovies(session->vcs(), pal,
        storylandPcShaderProfile(renderer.shaderProfile()), resolveRetail,
        bundledOpeningMovie, !session->archive.isPsp()));
    startAfterMovie = movieVisible = openingSequence.active();
    moviePaused = false;
    session->paused = true;
    panels = false;
    fingers.clear();
    if (!movieVisible) {
      audio.setEnabled(true);
      finishOpening();
    }
  };
  bind();
  auto inputLayout=storylandAndroidLayout({float(width),float(height)},initialDensity,uiZoom,{0,0,0,0});
  auto previousTime = std::chrono::steady_clock::now();
  const auto startedAt = previousTime;
  int workspacePage = 0;
  const auto saveDirectory=audioModePath.parent_path();
  const auto menuServices = [&]() {
    StorylandGameMenuServices services;
    services.artwork=renderer.frontendAssets();
    services.loaded=session->loaded();
    services.active=session->active()&&session->isPlayingGame();
    services.vcsVisualGlow=services.loaded&&session->vcs();
    services.title=session->vcs()?"Grand Theft Auto: Vice City Stories":"Grand Theft Auto: Liberty City Stories";
    services.canSave=session->canSaveGame(&services.saveRestriction);
    services.frontendSound=[&](uint16_t event) {audio.playFrontendSound(*session,event);};
    services.newGame=[&](std::string& failure) {
      const bool started=startPlay(true);
      failure=error;
      if(!started)gameMenu.showMain();
      return started;
    };
    services.resume=[&]() {
      gameMenu.close();
      panels=false;
      session->paused=false;
      session->runtime.paused=false;
      session->runtime.pauseMenu=false;
      session->runtime.pauseMap=false;
      session->runtime.pauseOptions=false;
      fingers.clear();
      ImGui::SetWindowFocus(nullptr);
    };
    services.returnToMain=[&]() {
      audio.stop();
      session->stopGame();
      panels=false;
      fingers.clear();
    };
    services.quit=[&]() {quit=true;};
    services.debug=[&]() {
      gameMenu.close();
      panels=true;
      session->paused=session->active();
      fingers.clear();
    };
    services.slotExists=[&](int slot) {return session->hasGameSaveSlot(saveDirectory,slot);};
    services.slotLabel=[](int) {return std::string("Saved game");};
    services.loadGame=[&](int slot,std::string& failure) {
      audio.stop();
      audio.setEnabled(false);
      loadingPresentation.start(session->gameRoot(),session->vcs());
      if(!session->loadGameSlot(saveDirectory,slot,failure)) {
        loadingPresentation.cancel();
        audio.setEnabled(true);
        return false;
      }
      refreshLoadingChapter();
      loadingPresentation.ready();
      panels=false;
      fingers.clear();
      return true;
    };
    services.saveGame=[&](int slot,std::string& failure) {
      return session->saveGameSlot(saveDirectory,slot,failure);
    };
    services.deleteGame=[&](int slot,std::string& failure) {
      return session->deleteGameSlot(saveDirectory,slot,failure);
    };
    services.finishSaveMenu=[&]() {
      session->environment.saveMenuRequested=false;
      session->environment.saveGameFinished=true;
    };
    services.drawSettings=[&]() {
      storylandFrontendText(renderer.frontendAssets(),"Game settings are available in the Storyland workspace.");
      if(storylandFrontendButton(renderer.frontendAssets(),"Open settings",ImVec2(-1,36))) {
        gameMenu.close();
        panels=true;
        workspacePage=3;
        session->paused=session->active();
        fingers.clear();
      }
    };
    return services;
  };
  while (!quit) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if(event.type==SDL_KEYDOWN||
          (event.type==SDL_MOUSEBUTTONDOWN&&event.button.which!=SDL_TOUCH_MOUSEID))
        renderer.helpBindings.keyboard=true;
      else if(event.type==SDL_FINGERDOWN||event.type==SDL_CONTROLLERBUTTONDOWN||
          (event.type==SDL_CONTROLLERAXISMOTION&&std::abs(int(event.caxis.value))>12000))
        renderer.helpBindings.keyboard=false;
      if (event.type == SDL_KEYDOWN && !event.key.repeat &&
          event.key.keysym.sym == SDLK_t && !coopChat.open && !coopPanel.open &&
          !panels && !movieVisible && !studio.active() && !loadingPresentation.active() && session->active() &&
          session->coopNetwork().connected() && !ImGui::GetIO().WantTextInput) {
        coopChat.show();
        fingers.clear();
        continue;
      }
      if (event.type == SDL_KEYDOWN && coopChat.open &&
          (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_AC_BACK)) {
        coopChat.close(event.key.keysym.scancode);
        fingers.clear();
        continue;
      }
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
        quit = true;
      if (event.type == SDL_APP_WILLENTERBACKGROUND) {
        background = true;
        if (auto* playing = currentMovie()) playing->pause(true);
        audio.setEnabled(false);
        fingers.clear();
      }
      if (event.type == SDL_APP_DIDENTERFOREGROUND) {
        background = false;
        audio.setEnabled(!movieVisible&&!studio.active()&&!loadingPresentation.active());
        if (auto* playing = currentMovie()) playing->pause(moviePaused);
        previousTime = std::chrono::steady_clock::now();
      }
      if (event.type == SDL_RENDER_DEVICE_RESET) {
        loadingPresentation.releaseTexture();
        renderer.shutdown();
        resources.releasePreview();
        if (auto* playing = currentMovie()) playing->resetTexture();
        ImGui_ImplOpenGL3_DestroyDeviceObjects();
        storylandAndroidGlShutdown();
        if (!storylandAndroidGlInitialize(error))
          quit = true;
        else if (session->loaded())
          renderer.initialize(*session, error);
      }
      if (event.type == SDL_CONTROLLERDEVICEADDED && !controller &&
          SDL_IsGameController(event.cdevice.which))
        controller = SDL_GameControllerOpen(event.cdevice.which);
      if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller &&
          !SDL_GameControllerGetAttached(controller)) {
        SDL_GameControllerClose(controller);
        controller = nullptr;
      }
      if(loadingPresentation.active()) {
        fingers.clear();
        continue;
      }
      if (event.type == SDL_KEYDOWN && !event.key.repeat &&
          (event.key.keysym.sym == SDLK_AC_BACK || event.key.keysym.sym == SDLK_ESCAPE) &&
          !io.WantTextInput && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)) {
        if(gameMenu.open()) {
          gameMenu.back(menuServices());
          fingers.clear();
          continue;
        }
        if (coopPanel.open) {coopPanel.open=false;fingers.clear();continue;}
        if (studio.active()) {studio.close();fingers.clear();continue;}
        if (movieVisible) {
          if(event.key.keysym.sym==SDLK_AC_BACK) {
            moviePaused = !moviePaused;
            if (auto* playing = currentMovie()) playing->pause(moviePaused);
          }
          continue;
        }
        if(session->active()) {
          if(!session->environment.pauseMenuDisabled&&
              (!session->environment.widescreen||session->environment.allowPauseInWidescreen)) {
            panels=false;
            gameMenu.showPause();
            session->paused=true;
            session->runtime.pauseMenu=true;
          }
        } else panels=!panels;
        fingers.clear();
      }
      if(event.type==SDL_CONTROLLERBUTTONDOWN&&!movieVisible&&!studio.active()&&
          !io.WantTextInput&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)) {
        if(event.cbutton.button==SDL_CONTROLLER_BUTTON_START&&session->active()) {
          if(gameMenu.open())gameMenu.back(menuServices());
          else if(!session->environment.pauseMenuDisabled&&
              (!session->environment.widescreen||session->environment.allowPauseInWidescreen)) {
            panels=false;
            gameMenu.showPause();
            session->paused=true;
            session->runtime.pauseMenu=true;
            fingers.clear();
          }
        } else if(event.cbutton.button==SDL_CONTROLLER_BUTTON_B&&gameMenu.open())
          gameMenu.back(menuServices());
      }
      if(studio.intro()&&event.type==SDL_KEYDOWN&&
          (event.key.keysym.sym==SDLK_ESCAPE||event.key.keysym.sym==SDLK_RETURN||event.key.keysym.sym==SDLK_SPACE))studio.close();
      if(event.type==SDL_FINGERDOWN||event.type==SDL_FINGERMOTION) {
        const auto screen=io.DisplaySize;
        const float x=(event.tfinger.x*screen.x-inputLayout.origin.x)/inputLayout.size.x;
        const float y=(event.tfinger.y*screen.y-inputLayout.origin.y)/inputLayout.size.y;
        if(event.type==SDL_FINGERDOWN&&!studio.active()&&!gameMenu.open()&&!panels&&!coopPanel.open&&!coopChat.open&&!movieVisible&&x>=0&&x<=1&&y>=inputLayout.toolbarHeight/inputLayout.size.y&&y<=1&&
            !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel))
            fingers[event.tfinger.fingerId]={x,y,x,y,actionAt(x,y,session->runtime.occupiedVehicle>=0)};
        if(event.type==SDL_FINGERMOTION) {
            const auto found=fingers.find(event.tfinger.fingerId);
            if(found!=fingers.end()){found->second.x=x;found->second.y=y;}
        }
      }
      if (event.type == SDL_FINGERUP)
        fingers.erase(event.tfinger.fingerId);
    }
    if (background) {
      SDL_Delay(25);
      continue;
    }
    std::string path;
    resources.poll(exportFiles);
    std::string replacement,replacementName; int replacementToken = 0;
    {std::lock_guard<std::mutex> lock(requestMutex);replacement.swap(importedResource);replacementName.swap(importedResourceName);replacementToken=importedToken;}
    if (!replacement.empty()) {
        if(replacementToken==-1) {
          openingSequence.stop();
          movie.stop();
          openingSkipInput.reset();
          movieVisible=startAfterMovie=moviePaused=false;
          gameMenu.close();
          audio.stop();session->stopGame();panels=true;workspacePage=0;
        }
        resources.importFile(std::filesystem::u8path(replacement),replacementToken,replacementName);
    }
    std::string archivePath, companionPath;
    if (!studio.active() && !loading.valid() && !loadingPresentation.active() && !resources.busy()) {
      std::lock_guard<std::mutex> lock(requestMutex);
      path.swap(requestedGame);
      archivePath.swap(requestedArchive);
      companionPath.swap(requestedCompanion);
    }
    if (!archivePath.empty()) {
      openingSequence.stop();
      movie.stop();
      openingSkipInput.reset();
      movieVisible=startAfterMovie=moviePaused=false;
      gameMenu.close();
      audio.stop();
      session->stopGame();
      panels=true;
      workspacePage=0;
      fingers.clear();
      resources.openArchive(std::filesystem::u8path(archivePath),std::filesystem::u8path(companionPath));
    }
    if (!path.empty()) {
      gameMenu.close();
      openingSequence.stop();
      openingSkipInput.reset();
      movie.stop();
      movieVisible = startAfterMovie = moviePaused = false;
      movies.clear();
      resources.reset();
      workspacePage=0;
      audio.setEnabled(false);
      audio.stop();
      loadingPresentation.startOpening();
      renderer.shutdown();
      session = std::make_unique<StorylandLinuxSession>();
      bind();
      error.clear();
      panels = true;
      loading = std::async(std::launch::async, [path]() {
        LoadResult result;
        try {
          result.session = std::make_unique<StorylandLinuxSession>();
          if (!result.session->loadGame(std::filesystem::u8path(path),
                                        result.error))
            result.session.reset();
        } catch (const std::bad_alloc &) {
          result.error = "Storyland failed to allocate enough memory. The game "
                         "was not opened.";
          result.session.reset();
        } catch (const std::exception &e) {
          result.error = e.what();
          result.session.reset();
        }
        return result;
      });
    }
    if (loading.valid() && loading.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready) {
      auto result = loading.get();
      error = result.error;
      if (result.session) {
        session = std::move(result.session);
        movies = movieFiles(session->gameRoot());
        bind();
        try {
          loadingPresentation.setProgress(.5f);
          loadingPresentation.start(session->gameRoot(),session->vcs());
          if(renderer.initialize(*session,error))loadingPresentation.ready();
          else loadingPresentation.cancel();
        } catch (const std::exception &e) {
          error=e.what();
          loadingPresentation.cancel();
        }
      } else loadingPresentation.cancel();
    }
    const auto now = std::chrono::steady_clock::now();
    const float seconds = std::clamp(
        std::chrono::duration<float>(now - previousTime).count(), 0.f, .05f);
    previousTime = now;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    const auto metrics = phoneMetrics();
    const auto layout = storylandAndroidLayout(io.DisplaySize,metrics.density,uiZoom,metrics.safe);
    inputLayout=layout;
    const float unit = layout.unit;
    const auto frontendButton=[&](const char* label,float width=0) {
      return storylandFrontendButton(renderer.frontendAssets(),label,
          ImVec2(width>0?width*unit:0,38*unit));
    };
    ImGui::GetStyle() = storylandAndroidStyle(unit);
    io.FontGlobalScale = unit/initialDensity;
    ImGui::NewFrame();
    ImGui::BeginDisabled(loadingPresentation.active());
    studio.advance(seconds);
    const float toolbarBottom = layout.origin.y+layout.toolbarHeight;
    const auto* movieKeys = SDL_GetKeyboardState(nullptr);
    const bool skipHeld = movieKeys[SDL_SCANCODE_RETURN] || movieKeys[SDL_SCANCODE_SPACE] ||
        movieKeys[SDL_SCANCODE_ESCAPE] || (controller &&
        (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START) ||
         SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A)));
    const bool skipPressed = openingSkipInput.controllerPressed(skipHeld);
    if (movieVisible) {
      if (skipPressed && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
        finishMovie();
      if (movieVisible && startAfterMovie) {
        if (auto* playing = currentMovie()) playing->pause(moviePaused);
        if (!moviePaused) openingSequence.tick(1);
        if (!openingSequence.active()) finishMovie();
      } else if (movieVisible) {
        movie.update();
        if (!movie.active()) {
          if (!movie.completed()) error = movie.status();
          finishMovie();
        }
      }
    }
    if (!studio.active()) {
    const ImGuiWindowFlags workspaceFlags = ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::SetNextWindowPos(layout.origin);
    ImGui::SetNextWindowSize({layout.size.x,panels&&!movieVisible?layout.size.y:layout.toolbarHeight});
    ImGui::SetNextWindowScroll({0,0});
    ImGui::Begin("Storyland",nullptr,workspaceFlags);
    if(panels&&!movieVisible)storylandDrawFrontendBackground(ImGui::GetWindowDrawList(),renderer.frontendAssets(),
        layout.origin,ImVec2(layout.origin.x+layout.size.x,layout.origin.y+layout.size.y),IM_COL32(255,255,255,90));
    if(layout.size.x/unit>650) {
      ImGui::AlignTextToFramePadding();
      storylandFrontendText(renderer.frontendAssets(),"STORYLAND",0,"fancytext",IM_COL32(255,110,179,255));
      ImGui::SameLine();
    }
    const bool toolbarWorkspace=layout.size.x/unit>=360;
    const bool toolbarStop=layout.size.x/unit>=560;
    const bool shortWorkspace=layout.size.y/unit<340;
    bool messageRequested=false;
    ImGui::BeginDisabled(movieVisible||resources.busy());
    static int pendingOpen = 0;
    if(frontendButton("Open",64))ImGui::OpenPopup("Open files");
    if(ImGui::BeginPopup("Open files")) {
        if(ImGui::Selectable("Game folder",false,0,{240*unit,40*unit}))pendingOpen=1;
        if(ImGui::Selectable("File",false,0,{240*unit,40*unit}))pendingOpen=2;
        ImGui::EndPopup();
    }
    if(pendingOpen) {
        if(resources.modified())ImGui::OpenPopup("Unsaved files");
        else {if(pendingOpen==1)chooseGame();else chooseResource(-1);pendingOpen=0;}
    }
    ImGui::SetNextWindowSize({std::min(360*unit,layout.size.x-24*unit),0},ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Unsaved files",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Save your changes before opening another file?");
        if(frontendButton("Keep editing")){pendingOpen=0;ImGui::CloseCurrentPopup();}
        if(frontendButton("Discard and open")){ImGui::CloseCurrentPopup();if(pendingOpen==1)chooseGame();else chooseResource(-1);pendingOpen=0;}
        ImGui::EndPopup();
    }
    ImGui::EndDisabled(); ImGui::SameLine();
    if(toolbarWorkspace && frontendButton("Tools",layout.size.x/unit<560?64:74)) {
        if(!movieVisible){gameMenu.close();panels=!panels;session->paused=panels&&session->active();fingers.clear();}
    }
    if(toolbarWorkspace)ImGui::SameLine();
    if(frontendButton("Play",62))ImGui::OpenPopup("Play actions");
    if(ImGui::BeginPopup("Play actions")) {
        ImGui::BeginDisabled(!session->loaded()||loading.valid()||resources.busy()||movieVisible);
        if(ImGui::Selectable("Play Test",false,0,{180*unit,44*unit}))startPlay(false);
        if(ImGui::Selectable("Play Game",false,0,{180*unit,44*unit})) {
            beginOpening();
        }
        if(session->active()&&ImGui::Selectable("Resume",false,0,{180*unit,44*unit}))menuServices().resume();
        if(session->active()&&!gameMenu.open()&&!session->environment.pauseMenuDisabled&&
            (!session->environment.widescreen||session->environment.allowPauseInWidescreen)&&
            ImGui::Selectable("Pause",false,0,{180*unit,44*unit})) {
            gameMenu.showPause();panels=false;session->paused=true;session->runtime.pauseMenu=true;fingers.clear();
        }
        ImGui::EndDisabled();ImGui::EndPopup();
    }
    if(toolbarStop) {ImGui::SameLine(); ImGui::BeginDisabled(!session->active()&&!movieVisible&&!gameMenu.open());
    if(frontendButton("Stop",58)) {
        gameMenu.close();
        if(movieVisible){openingSequence.stop();movie.stop();movieVisible=startAfterMovie=moviePaused=false;openingSkipInput.finish();audio.setEnabled(true);}
        audio.stop();session->stopGame();panels=true;
    }
    ImGui::EndDisabled();} ImGui::SameLine();
    if(frontendButton("More",62))ImGui::OpenPopup("More actions");
    if(ImGui::BeginPopup("More actions")) {
        if(shortWorkspace&&!movieVisible) {
            const char* pages[]={"Resources","Play options","Movies","Settings"};
            for(int page=0;page<4;++page)if(ImGui::Selectable(pages[page],workspacePage==page,0,{200*unit,44*unit})) {
                workspacePage=page;panels=true;session->paused=session->active();
            }
            ImGui::Separator();
        }
        if(!movieVisible&&!error.empty()&&ImGui::Selectable("Message details",false,0,{200*unit,44*unit})){panels=true;session->paused=session->active();messageRequested=true;}
        if(!toolbarStop&&(session->active()||movieVisible||gameMenu.open())&&ImGui::Selectable("Stop",false,0,{200*unit,44*unit})) {
            gameMenu.close();
            if(movieVisible){openingSequence.stop();movie.stop();movieVisible=startAfterMovie=moviePaused=false;openingSkipInput.finish();audio.setEnabled(true);}
            audio.stop();session->stopGame();panels=true;
        }
        if(movieVisible) {
            if(ImGui::Selectable(moviePaused?"Resume movie":"Pause movie",false,0,{200*unit,44*unit})){moviePaused=!moviePaused;if(auto* playing=currentMovie())playing->pause(moviePaused);}
            if(ImGui::Selectable("Skip movie",false,0,{200*unit,44*unit}))finishMovie();
        } else if(session->active()&&session->environment.cutscene.playing&&ImGui::Selectable("Skip scene",false,0,{200*unit,44*unit})) {
            session->requestCutsceneSkip();audio.synchronize(*session);
        }
        if(!movieVisible&&ImGui::Selectable("Co-op",false,0,{200*unit,44*unit})) {
            coopPanel.open=true;
            fingers.clear();
        }
        if(!movieVisible&&session->coopNetwork().connected()&&
            ImGui::Selectable("Chat",false,0,{200*unit,44*unit})) {
            coopChat.show();
            fingers.clear();
        }
        if(ImGui::Selectable("Show files",false,0,{200*unit,44*unit})){panels=true;session->paused=session->active();}
        ImGui::Separator();
        if (ImGui::Selectable("Open file",false,0,{200*unit,40*unit})) chooseResource(-1);
        ImGui::Separator();
        ImGui::BeginDisabled(movieVisible);
        if(ImGui::Selectable("About Storyland",false,0,{200*unit,44*unit})){studio.showAbout();panels=true;session->paused=session->active();fingers.clear();}
        if(ImGui::Selectable("Special Thanks",false,0,{200*unit,44*unit})){studio.showThanks();panels=true;session->paused=session->active();fingers.clear();}
        if(ImGui::Selectable("Reigns Studio intro",false,0,{200*unit,44*unit})) {
            const auto studioMovie=bundledOpeningMovie("reigns_studio.pss");
            if(!studioMovie.empty())beginMovie(studioMovie);
            else {studio.showIntro(uint32_t(SDL_GetPerformanceCounter()));panels=true;session->paused=session->active();fingers.clear();}
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    if(panels&&!movieVisible) {
        if(messageRequested)ImGui::OpenPopup("Storyland message");
        ImGui::Separator();
        const float footerHeight=shortWorkspace?0:30*unit;
        const auto beginPage=[&](const char* label,int page){
            if(shortWorkspace)return workspacePage==page;
            const bool open=ImGui::BeginTabItem(label);if(open)workspacePage=page;return open;
        };
        const auto endPage=[&](){if(!shortWorkspace)ImGui::EndTabItem();};
        if(shortWorkspace||ImGui::BeginTabBar("Pages",ImGuiTabBarFlags_FittingPolicyScroll)) {
            if(beginPage("Resources",0)) {
                if((!session->loaded()&&!resources.standalone())||loading.valid()) {
                    ImGui::BeginChild("Welcome",{0,-footerHeight});
                    storylandFrontendText(renderer.frontendAssets(),"Open a game folder or file.");
                    if (!loading.valid() && !resources.status().empty())
                        ImGui::TextWrapped("%s", resources.status().c_str());
                    if (loading.valid()) ImGui::TextUnformatted("Opening game files...");
                    if (!loading.valid() && frontendButton("Open game folder")) chooseGame();
                    if (!loading.valid() && frontendButton("Open file")) chooseResource(-1);
                    ImGui::EndChild();
                } else {
                    ImGui::BeginChild("Files",{0,-footerHeight});
                    resources.draw(session->archive,session->active(),std::filesystem::current_path(),layout,chooseResource);
                    ImGui::EndChild();
                }
                endPage();
            }
            if(beginPage("Play",1)) {
                ImGui::BeginChild("Play settings",{0,-footerHeight});
                ImGui::TextUnformatted("PLAY");
                if(session->active()&&frontendButton("Resume game"))menuServices().resume();
                if(StorylandPlayOptionsSection section{"World",false,true})
                    ImGui::Checkbox("Inverted Map",&session->environment.invertedMap);
                if(StorylandPlayOptionsSection section{"Pedestrians",false,true}) {
                    ImGui::Checkbox("Aggressive Pedestrians",&session->runtime.aggressivePedestrians);
                    bool zombiePedestrians=session->runtime.zombiePedestrians;
                    ImGui::BeginDisabled(!session->loaded()||!session->vcs());
                    if(ImGui::Checkbox("Zombie Pedestrians",&zombiePedestrians)) {
                        std::string optionError;
                        session->setZombiePedestrians(zombiePedestrians,optionError);
                    }
                    ImGui::EndDisabled();
                }
                if(StorylandPlayOptionsSection section{"Vehicles",false,true})
                    ImGui::Checkbox("Mafia 2 Vehicle Collision",&session->runtime.collisionOccupantDamage);
                if(StorylandPlayOptionsSection section{"Co-op",false}) {
                    storylandCoopFriendlyFireMenu(session->coopNetwork(), session->environment.coopFriendlyFire);
                    if(frontendButton("Host or join"))coopPanel.open=true;
                    ImGui::BeginDisabled(!session->coopNetwork().connected());
                    if(frontendButton("Chat"))coopChat.show();
                    ImGui::EndDisabled();
                }
                if(StorylandPlayOptionsSection section{"Controls",false}) {
                    ImGui::TextWrapped("Choose Play Game or Play Test. Your files stay open when you return.");
                    ImGui::TextWrapped("Use the left pad to move or drive. Drag to turn the camera. Use the buttons on the right to aim, fire, jump, enter, run or reload.");
                    ImGui::TextWrapped("Press Back to return. Keyboard and controller are supported.");
                }
                if(session->active()&&!session->mission.diagnostic.empty())ImGui::TextWrapped("%s",session->mission.diagnostic.c_str());
                ImGui::EndChild();endPage();
            }
            if(beginPage("Movies",2)) {
                ImGui::BeginChild("Movie browser",{0,-footerHeight});
                ImGui::TextUnformatted("GAME MOVIES");
                if(movies.empty())ImGui::TextWrapped("No PSS movies found.");
                for(const auto& file:movies)if(ImGui::Selectable(file.filename().string().c_str(),false,0,{0,44*unit})){startAfterMovie=false;beginMovie(file);break;}
                ImGui::EndChild();endPage();
            }
            if(beginPage("Settings",3)) {
                ImGui::BeginChild("Settings",{0,-footerHeight});
                ImGui::TextUnformatted("DISPLAY");
                ImGui::TextUnformatted("Shader");
                ImGui::SetNextItemWidth(std::min(300*unit,ImGui::GetContentRegionAvail().x));
                int shaderSelection=storylandShaderSelectionIndex(renderer.shaderProfile());
                if(ImGui::Combo("##Shader",&shaderSelection,StorylandPortableShaderNames.data(),
                    int(StorylandPortableShaderNames.size())))
                    renderer.setShaderProfile(StorylandPortableShaderProfiles[size_t(shaderSelection)],error);
                ImGui::Checkbox("Bounding boxes",&renderer.showBounds);
                if(ImGui::BeginMenu("Mesh effects")) {
                    storylandMeshEffectMenu(renderer.meshEffects,session->animationSeconds());ImGui::EndMenu();
                }
                ImGui::Separator();
                ImGui::TextUnformatted("AUDIO");
                int audioProfile=int(audio.mode());
                ImGui::SetNextItemWidth(std::min(300*unit,ImGui::GetContentRegionAvail().x));
                if(ImGui::Combo("Audio Mode",&audioProfile,"PC (Enhanced)\0PS2\0PSP\0")) {
                    audio.setMode(storylandAudioModeFromInt(audioProfile));
                    if(!audioModePath.empty()) {
                        std::ofstream saved(audioModePath,std::ios::trunc);
                        saved<<audioProfile<<"\n";
                        saved.close();
                        if(!saved)error="Could not save the audio mode.";
                    } else error="The audio settings directory is unavailable.";
                }
                ImGui::TextWrapped("PC (Enhanced): extra clarity. PS2: original sound. "
                    "PSP: softer treble and narrower stereo.");
                ImGui::TextUnformatted("Interface size");
                ImGui::SetNextItemWidth(std::min(300*unit,ImGui::GetContentRegionAvail().x));
                ImGui::SliderFloat("##Interface size",&uiZoom,.8f,1.3f,"%.2fx");
                if(ImGui::IsItemDeactivatedAfterEdit()){std::ofstream preferences("storyland-ui.cfg");preferences<<uiZoom;}

                ImGui::Separator();ImGui::TextUnformatted("Storyland " STORYLAND_VERSION " - Android");
                if(frontendButton("About Storyland"))studio.showAbout();
                if(frontendButton("Special Thanks"))studio.showThanks();

                ImGui::EndChild();endPage();
            }
            if(!shortWorkspace)ImGui::EndTabBar();
        }
        const std::string status=!error.empty()?error:!resources.status().empty()?resources.status():session->status();
        if(!shortWorkspace&&!error.empty()) {
            if(ImGui::SmallButton("Details"))ImGui::OpenPopup("Storyland message");ImGui::SameLine();
        }
        if (!shortWorkspace && (session->loaded() || resources.standalone()))
            ImGui::TextDisabled("%s", status.empty() ? "Ready" : status.c_str());
        if(ImGui::BeginPopupModal("Storyland message",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::min(layout.size.x*.75f,480*unit));ImGui::TextUnformatted(error.c_str());ImGui::PopTextWrapPos();
            if(frontendButton("Close"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
        }
    }
    ImGui::End();
    }
    if(!movieVisible&&!studio.active()&&!gameMenu.open())coopPanel.draw(*session,unit);
    if(!movieVisible&&!studio.active()&&!gameMenu.open())coopChat.draw(session->coopNetwork(),unit);
    ImGui::EndDisabled();
    if(!movieVisible&&!studio.active()&&!loadingPresentation.active()) {
      if(session->active()&&session->environment.saveMenuRequested&&!gameMenu.scriptedSave()) {
        gameMenu.showSave(true);
        session->paused=true;
        panels=false;
        fingers.clear();
      }
      ImGui::BeginDisabled(openingSkipInput.blocksMenu());
      gameMenu.draw(menuServices());
      ImGui::EndDisabled();
    }
    refreshLoadingChapter();
    const bool loadingFrame=loadingPresentation.beginFrame();
    audio.setEnabled(!movieVisible&&!studio.active()&&!loadingFrame);
    StorylandPlayTestInput input;
    StorylandPlayTestCameraInput camera;
    bool held[ButtonCount]{};
    const auto* keys=SDL_GetKeyboardState(nullptr);
    const bool inputEnabled=!loadingFrame&&!studio.active()&&!panels&&!coopPanel.open&&!coopChat.blocksGameInput()&&
        !ImGui::GetIO().WantTextInput&&!movieVisible&&!openingSkipInput.blocksMenu()&&!gameMenu.open()&&session->active()&&!session->paused&&
        !background&&(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0;
    const bool keyboardEnabled=inputEnabled&&!ImGui::GetIO().WantCaptureKeyboard;
    if(weaponSwitch.update(keys[SDL_SCANCODE_Q]!=0,keyboardEnabled))
      storylandCyclePlayerWeapon(session->runtime);
    camera.refocus=cameraCentre.update(keys[SDL_SCANCODE_B]!=0,keyboardEnabled&&
        session->environment.cameraControlEnabled&&session->runtime.occupiedVehicle<0&&
        session->environment.remoteVehicle<0&&session->environment.remoteControl.vehicle<0);
    if (inputEnabled) {
      const bool driving = session->runtime.occupiedVehicle >= 0;
      const bool motorcycle = storylandControlledVehicleIsMotorcycle(session->runtime);
      controls(fingers, input, camera, held);
      drawControls(fingers,layout,driving,storylandPlayerVehicleHasSiren(session->runtime));
      const auto touchPedals = input.vehiclePedalPressures();
      const auto movementKeys = storylandPcMovementKeys(driving, motorcycle,
          keyboardEnabled && keys[SDL_SCANCODE_W], keyboardEnabled && keys[SDL_SCANCODE_S],
          keyboardEnabled && keys[SDL_SCANCODE_UP], keyboardEnabled && keys[SDL_SCANCODE_DOWN],
          keyboardEnabled && keys[SDL_SCANCODE_KP_9], keyboardEnabled && keys[SDL_SCANCODE_KP_6]);
      input.forward |= movementKeys.forward;
      input.backward |= movementKeys.backward;
      input.bikeLean = movementKeys.bikeLean;
      if (motorcycle) {
        input.acceleratorPressure = std::max(int(touchPedals[0]), movementKeys.forward ? 255 : 0);
        input.brakePressure = std::max(int(touchPedals[1]), movementKeys.backward ? 255 : 0);
      }
      if (keyboardEnabled) {
        input.left |= keys[SDL_SCANCODE_A];
        input.right |= keys[SDL_SCANCODE_D];
        input.sprinting |= keys[SDL_SCANCODE_LSHIFT];
        held[0] |= keys[SDL_SCANCODE_E];
        held[1] |= keys[SDL_SCANCODE_G];
        held[2] |= keys[SDL_SCANCODE_SPACE];
        held[3] |= keys[SDL_SCANCODE_F];
        held[5] |= !driving && keys[SDL_SCANCODE_R];
        held[RadioAction] |= driving && keys[SDL_SCANCODE_R];
        held[HornAction] |= driving && keys[SDL_SCANCODE_H];
      }
      if (controller) {
        float x =
            SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) /
            32767.f;
        float y =
            -SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) /
            32767.f;
        if (std::hypot(x, y) > .18f) {
          input.analogMovement = true;
          input.analogHorizontal = x;
          input.left = x < -.1f;
          input.right = x > .1f;
          if (motorcycle) {
            const float lean = std::fabs(y) > .18f ? std::clamp(y, -1.0f, 1.0f) : 0;
            if (std::fabs(lean) > std::fabs(input.bikeLean)) input.bikeLean = lean;
          } else {
            input.analogForward = y;
            input.forward = y > .1f;
            input.backward = y < -.1f;
          }
        }
        int rx = SDL_GameControllerGetAxis(controller,
                                           SDL_CONTROLLER_AXIS_RIGHTX),
            ry = SDL_GameControllerGetAxis(controller,
                                           SDL_CONTROLLER_AXIS_RIGHTY);
        camera.left = rx < -5000;
        camera.right = rx > 5000;
        camera.up = ry < -5000;
        camera.down = ry > 5000;
        const int leftTrigger = std::max(0, int(SDL_GameControllerGetAxis(
            controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT)));
        const int rightTrigger = std::max(0, int(SDL_GameControllerGetAxis(
            controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT)));
        if (motorcycle) {
          input.acceleratorPressure = std::max(input.acceleratorPressure,
              int(std::lround(float(rightTrigger) * 255.0f / 32767.0f)));
          input.brakePressure = std::max(input.brakePressure,
              int(std::lround(float(leftTrigger) * 255.0f / 32767.0f)));
          input.forward = input.acceleratorPressure > 0;
          input.backward = input.brakePressure > 0;
          input.analogForward = float(input.forward) - float(input.backward);
        } else {
          held[0] |= leftTrigger > 5000;
          held[1] |= rightTrigger > 5000;
        }
        held[2] |=
            SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X);
        held[3] |=
            SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y);
        input.sprinting |=
            SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
        held[5] |=
            SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);
        held[RadioAction] |= driving && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        held[HornAction] |= driving && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
      }
      input.handbrake = held[2];
    }
    const bool hornHeld=held[HornAction]||keys[SDL_SCANCODE_H]||
        (controller&&SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))||
        std::any_of(fingers.begin(),fingers.end(),[](const auto& entry){return entry.second.action==HornAction;});
    try {
      audio.setVehicleHornHeld(sirenControls.update(session->runtime,hornHeld,
          inputEnabled&&session->active(),seconds));
      if(!loadingFrame&&held[RadioAction]&&!previous[RadioAction])audio.cycleRadio(*session,1);
      const double coopTime=std::chrono::duration<double>(now-startedAt).count();
      if(!loadingFrame)session->updateCoop(coopTime);
      if (!loadingFrame&&!movieVisible&&!studio.active()) {
        audio.synchronize(*session);
        audio.advanceSilently(seconds);
      }
      session->camera.aspectRatio =
          io.DisplaySize.x / std::max(1.f, io.DisplaySize.y);
      const bool wasActive = session->active();
      if (session->environment.invertedMap) storylandInvertPlayControls(input,camera);
      if (!loadingFrame&&!movieVisible&&!studio.active())
        session->tick(seconds, input, camera, held[2] && !previous[2],
                      held[3] && !previous[3], held[0], held[1],
                      held[5] && !previous[5]);
      if(!loadingFrame)session->publishCoop(coopTime);
      if (wasActive && !session->active()) {
        panels = true;
        fingers.clear();
      }
      if (!loadingFrame&&!movieVisible&&!studio.active())
        audio.synchronize(*session);
      SDL_GL_GetDrawableSize(window, &width, &height);
      glViewport(0, 0, width, height);
      glClearColor(.06f, .035f, .055f, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      if (session->loaded() && !loading.valid() && !loadingFrame && !movieVisible&&!studio.active())
        renderer.draw(*session, width, height);
    } catch (const std::bad_alloc &) {
      error = "Storyland failed to allocate enough memory. Play was stopped.";
      gameMenu.close();
      audio.stop();
      session->stopGame();
      panels = true;
    } catch (const std::exception &e) {
      error = e.what();
      gameMenu.close();
      audio.stop();
      session->stopGame();
      panels = true;
    }
    std::copy(held, held + ButtonCount, previous);
    auto *presentation = ImGui::GetBackgroundDrawList();
    const auto size = io.DisplaySize;
    if (session->environment.widescreen) {
      presentation->AddRectFilled({0, toolbarBottom},
                                  {size.x, size.y * .12f}, IM_COL32_BLACK);
      presentation->AddRectFilled({0, size.y * .88f}, size, IM_COL32_BLACK);
    }
    if (session->environment.cutscene.playing) {
      if (const auto *subtitle = session->cutscene.scene().subtitleAt(
              session->environment.cutscene.elapsedMilliseconds)) {
        const float fontSize = std::max(18.f, size.y / 32),
                    textWidth = size.x * .82f;
        const auto extent = ImGui::GetFont()->CalcTextSizeA(
            fontSize, textWidth, textWidth, subtitle->text.c_str());
        const ImVec2 origin{(size.x - extent.x) * .5f,
                            size.y * .87f - extent.y};
        presentation->AddText(ImGui::GetFont(), fontSize,
                              {origin.x + 2, origin.y + 2}, IM_COL32_BLACK,
                              subtitle->text.c_str(), nullptr, textWidth);
        presentation->AddText(ImGui::GetFont(), fontSize, origin,
                              IM_COL32_WHITE, subtitle->text.c_str(), nullptr,
                              textWidth);
      }
    }
    if (!session->runtime.entryError.empty() &&
        session->runtime.elapsed < session->runtime.entryErrorUntil) {
      const float width = std::max(40.f, size.x - 32.f);
      auto *font = ImGui::GetFont();
      const float fontSize = std::max(17.f, size.y / 33.f);
      const auto extent = font->CalcTextSizeA(fontSize, width, width,
          session->runtime.entryError.c_str());
      const ImVec2 position{(size.x - extent.x) * .5f,
          size.y * .78f};
      ImGui::GetForegroundDrawList()->AddText(font, fontSize,
          {position.x + 1, position.y + 1}, IM_COL32_BLACK,
          session->runtime.entryError.c_str(), nullptr, width);
      ImGui::GetForegroundDrawList()->AddText(font, fontSize,
          position, IM_COL32(255,190,190,255),
          session->runtime.entryError.c_str(), nullptr, width);
    }
    if (session->runtime.entryFade > 0)
      presentation->AddRectFilled(
          {0, toolbarBottom}, size,
          IM_COL32(
              0, 0, 0,
              int(std::clamp(session->runtime.entryFade, 0.f, 1.f) * 255)));
    if (session->environment.fadeAlpha > 0)
      presentation->AddRectFilled(
          {0, toolbarBottom}, io.DisplaySize,
          IM_COL32(
              session->environment.fadingColor[0],
              session->environment.fadingColor[1],
              session->environment.fadingColor[2],
              int(std::clamp(session->environment.fadeAlpha, 0.f, 1.f) * 255)));
    if (movieVisible) {
      const float top = toolbarBottom;
      presentation->AddRectFilled({0, top}, size, IM_COL32_BLACK);
      const auto* playing = currentMovie();
      if (playing && playing->texture()) {
        const float aspect = startAfterMovie ?
            openingSequence.currentDisplayAspect(playing->displayAspect()) : playing->displayAspect();
        const auto frame = storylandOpeningMoviePresentation(size.x, size.y - top, aspect);
        if (frame.margins)
          presentation->AddImage(ImTextureID(playing->texture()), {0, top}, size,
              {frame.background.left, frame.background.top},
              {frame.background.right, frame.background.bottom}, IM_COL32(56, 56, 56, 255));
        const auto& bounds = frame.foreground;
        presentation->AddImage(ImTextureID(playing->texture()),
            {size.x * bounds.left, top + (size.y - top) * bounds.top},
            {size.x * bounds.right, top + (size.y - top) * bounds.bottom});
      } else {
        const auto status = startAfterMovie ? openingSequence.status() : movie.status();
        presentation->AddText({20, top + 20}, IM_COL32_WHITE,
            status.empty() ? "Preparing movie..." : status.c_str());
      }
    }
    if(studio.active())studio.draw(layout,pixelFont,programMusicError());
    if(programTrack!=studio.musicTrack()) {
        programTrack=studio.musicTrack();
        setProgramMusic(programTrack);
    }
    if(loadingFrame) {
      loadingPresentation.draw(loading.valid()?"Opening game data...":"Preparing game...");
    }
    ImGui::Render();
    if(io.WantTextInput&&!SDL_IsTextInputActive())SDL_StartTextInput();
    else if(!io.WantTextInput&&SDL_IsTextInputActive())SDL_StopTextInput();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }
  if (loading.valid())
    loading.wait();
  loadingPresentation.cancel();
  resources.reset();
  setProgramMusic(0);
  openingSequence.stop();
  movie.stop();
  audio.stop();
  renderer.shutdown();
  session.reset();
  if (controller)
    SDL_GameControllerClose(controller);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  storylandAndroidGlShutdown();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
