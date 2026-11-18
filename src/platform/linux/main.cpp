#include "src/platform/linux/storyland_linux_audio.h"
#include "src/storyland/storyland_vehicle_siren.h"
#include "src/platform/linux/storyland_linux_language.h"
#include "src/platform/linux/storyland_linux_renderer.h"
#include "src/platform/linux/storyland_linux_ctw_viewer.h"
#include "src/platform/linux/storyland_loading_presentation.h"
#include "src/storyland/storyland_opening_movies.h"
#include "src/storyland/storyland_movie_player.h"
#include "src/storyland/storyland_game_files.h"
#include "src/re/reVCS/frontend/storyland_vcs_loading_chapter.h"
#include "src/graphics/storyland_mesh_effects_ui.h"
#include "src/platform/linux/storyland_linux_session.h"
#include "src/storyland/storyland_script_log_popup.h"
#include "src/storyland/storyland_modding_ui.h"
#include "src/audio/storyland_mission_voice_preview.h"
#include "src/storyland/storyland_unicode_font.h"
#include "src/storyland/storyland_scm_workbench.h"
#include "src/storyland/storyland_dyom.h"
#include "src/platform/linux/storyland_portable_dtz_editor.h"
#include "src/storyland/storyland_play_test_weapons.h"
#include "src/platform/linux/storyland_portable_coop_panel.h"
#include "src/platform/linux/storyland_portable_chat.h"
#include "imgui.h"
#include "imgui_impl_opengl2.h"
#include "imgui_impl_sdl2.h"
#include <SDL.h>
#include <GL/gl.h>
#include <png.h>
#include "src/storyland/storyland_gallery.h"
#include "src/storyland/storyland_user_directories.h"
#include "src/graphics/storyland_game_menu.h"
#include "src/graphics/storyland_play_options_ui.h"
#include "src/storyland/storyland_stats_menu.h"
#include "src/graphics/storyland_graphics_quality_ui.h"
#include "src/storyland/storyland_time.h"
#include "src/graphics/storyland_imgui_viewport.h"
#include "src/graphics/storyland_frame_pacing.h"
#include "src/storyland/storyland_pc_shoulder_buttons.h"
#include "src/storyland/storyland_pc_input.h"
#include "src/storyland/storyland_bike_controls.h"
#include "src/platform/linux/storyland_linux_gamepad.h"
#include "src/storyland/storyland_retail_pc_camera.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
std::filesystem::path linuxDocumentsDirectory() {
    const char* homeValue=std::getenv("HOME");
    if(!homeValue||!*homeValue)return {};
    const std::filesystem::path home=std::filesystem::u8path(homeValue);
    const char* configValue=std::getenv("XDG_CONFIG_HOME");
    const auto config=(configValue&&*configValue?std::filesystem::u8path(configValue):home/".config")/"user-dirs.dirs";
    std::ifstream file(config);
    std::string line;
    while(std::getline(file,line)) {
        const std::string prefix="XDG_DOCUMENTS_DIR=\"";
        if(line.rfind(prefix,0)!=0||line.size()<=prefix.size()||line.back()!='\"')continue;
        std::string value=line.substr(prefix.size(),line.size()-prefix.size()-1);
        if(value.rfind("$HOME",0)==0)value=home.u8string()+value.substr(5);
        const auto path=std::filesystem::u8path(value);
        if(path.is_absolute())return path;
    }
    return home/"Documents";
}

std::vector<StorylandOpeningMovie> linuxOpeningMovies(
    const StorylandLinuxSession& session,bool pcShader) {
    auto folders=storylandNativeGameFolders(session.gameRoot());
    bool pal=false;
    const size_t nativeFolderCount=folders.size();
    for(size_t index=0;index<nativeFolderCount;++index) {
        std::vector<std::filesystem::path> movieFolders;
        std::error_code error;
        for(std::filesystem::directory_iterator entry(folders[index],error),end;
            !error&&entry!=end;entry.increment(error)) {
            const auto name=storylandNativePathLower(entry->path().filename().u8string());
            if(name.rfind("sles",0)==0||name.rfind("sces",0)==0)pal=true;
            std::error_code kindError;
            if(name=="movies"&&entry->is_directory(kindError)&&!kindError)
                movieFolders.push_back(entry->path());
        }
        std::sort(movieFolders.begin(),movieFolders.end());
        folders.insert(folders.end(),movieFolders.begin(),movieFolders.end());
    }
    const auto resolveRetail=[&](const std::string& name) {
        std::filesystem::path fallback;
        const auto wanted=storylandNativePathLower(name);
        for(const auto& folder:folders) {
            std::vector<std::filesystem::path> matches;
            std::error_code error;
            for(std::filesystem::directory_iterator entry(folder,error),end;
                !error&&entry!=end;entry.increment(error)) {
                if(storylandNativePathLower(entry->path().filename().u8string())!=wanted)continue;
                std::error_code kindError;
                if(entry->is_regular_file(kindError)&&!kindError)matches.push_back(entry->path());
            }
            if(matches.empty())continue;
            std::sort(matches.begin(),matches.end());
            fallback=matches.front();
            break;
        }
        return session.archive.resolveGameFile(name,fallback);
    };
    std::filesystem::path executableDirectory;
    if(char* directory=SDL_GetBasePath()) {
        executableDirectory=std::filesystem::u8path(directory);
        SDL_free(directory);
    }
    const auto resolveBundled=[&](const std::string& name) {
        return storylandFindBundledOpeningMovie(executableDirectory,name);
    };
    return storylandResolveOpeningMovies(session.vcs(),pal,pcShader,resolveRetail,
        resolveBundled,!session.archive.isPsp());
}

bool loadGalleryImage(const std::filesystem::path& path,int& width,int& height,
    std::vector<uint8_t>& pixels,std::string& error) {
    png_image image{};image.version=PNG_IMAGE_VERSION;
    if(!png_image_begin_read_from_file(&image,path.string().c_str())) {error=image.message;return false;}
    if(!image.width||!image.height||uint64_t(image.width)*image.height>67108864) {
        png_image_free(&image);error="Invalid photograph dimensions.";return false;
    }
    image.format=PNG_FORMAT_RGBA;
    pixels.resize(PNG_IMAGE_SIZE(image));
    if(!png_image_finish_read(&image,nullptr,pixels.data(),0,nullptr)) {
        error=image.message;png_image_free(&image);return false;
    }
    width=int(image.width);height=int(image.height);png_image_free(&image);error.clear();return true;
}

struct LaunchOptions {
    struct TimedCapture {
        double seconds = 0;
        std::filesystem::path path;
        bool saved = false;
    };
    std::filesystem::path game;
    std::filesystem::path capture;
    std::filesystem::path report;
    std::filesystem::path trace;
    std::filesystem::path scm;
    std::filesystem::path scmDisassembly;
    std::filesystem::path scmPatch;
    std::filesystem::path scmCopy;
    std::filesystem::path dyom;
    std::filesystem::path editDyom;
    std::filesystem::path validateDyom;
    int scmMission = -2;
    bool dyomEditor = false;
    std::vector<TimedCapture> captures;
    uint64_t frames = 0;
    double duration = 0;
    float fixedStep = 0;
    StorylandPlayFrameMode frameMode = StorylandPlayFrameMode::Fps60;
    int width = 1280;
    int height = 800;
    bool playGame = false;
    bool playTest = false;
    bool zombiePedestrians = false;
    bool audio = true;
    bool hideUi = false;
    bool quitOnFault = false;
    bool ctwViewer = false;
    bool ctwTopView = false;
    int ctwFocusResource = -1;
};

void printHelp() {
    std::cout << "Storyland " STORYLAND_VERSION " for Linux\n"
        "Usage: Storyland [options]\n\n"
        "  --game PATH         Open a game folder, GAME.DTZ, or CTW GAME.PAK\n"
        "  --ctw-pak PATH      Assemble Chinatown Wars PSP Liberty City from GAME.PAK\n"
        "  --ctw-top-view      Start the CTW city viewer looking straight down\n"
        "  --ctw-focus N       Focus a placed CTW world-block resource ID\n"
        "  --play-game         Run retail MAIN.SCM after opening the game\n"
        "  --play-test         Start free exploration after opening the game\n"
        "  --zombie-pedestrians Enable the Brawn of the Dead pedestrian state (VCS)\n"
        "  --scm PATH          Open MAIN.SCM in the native workbench\n"
        "  --scm-mission N     Export one native mission slot; -1 selects MAIN\n"
        "  --scm-disassembly PATH Export decoded instructions without opening a window\n"
        "  --scm-patch PATH    Apply a matching .scmpatch to the working document\n"
        "  --scm-copy PATH     Export the edited SCM copy without opening a window\n"
        "  --dyom PATH         Play a .dyom or .dyomvcs mission using --game data\n"
        "  --dyom-editor       Open the DYOM mission designer\n"
        "  --edit-dyom PATH    Open a mission project for editing\n"
        "  --validate-dyom PATH Validate and compile a project without a window\n"
        "  --no-audio          Disable native cutscene audio and sound effects\n"
        "  --hide-ui           Start with panels hidden; Home toggles them\n"
        "  --width N           Window width (default 1280)\n"
        "  --height N          Window height (default 800)\n"
        "  --frames N          Close after N rendered frames\n"
        "  --duration N        Close after N seconds of the game clock\n"
        "  --fixed-step N      Advance N seconds per frame (0 uses real time)\n"
        "  --frame-mode MODE   30, 60, or 60-120 (60 gameplay / 120 presentation)\n"
        "  --capture PATH      Save the final rendered frame as PNG\n"
        "  --capture-at N PATH Save a PNG at game-clock second N; repeatable\n"
        "  --report PATH       Write final game and renderer state as JSON\n"
        "  --trace PATH        Write frame, script, camera and audio state as JSONL\n"
        "  --quit-on-fault     Exit when a retail script reports a fault\n\n"
        "Controls: W/S/A/D move, I/J/K/L camera, Shift sprint, Left Alt walk.\n"
        "Right mouse free aims; E locks on; G or left mouse fires; R reloads; T chats.\n"
        "Q cycles weapons and B recenters on foot; Q/E look sideways and R cycles radio in vehicles.\n"
        "Space jumps or applies the handbrake. F enters or exits a vehicle.\n"
        "Home shows or hides panels. Tab opens or closes the pause/debug panel.\n"
        "F12 saves a screenshot to the game's Gallery.\n";
}

LaunchOptions parseOptions(int argc, char** argv) {
    LaunchOptions options;
    std::vector<std::string> requestedOptions;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        requestedOptions.push_back(argument);
        const auto value = [&]() -> std::string {
            if (++index >= argc) throw std::runtime_error("Missing value for " + argument);
            return argv[index];
        };
        if (argument == "--game") options.game = std::filesystem::u8path(value());
        else if (argument == "--ctw-pak") {
            options.game = std::filesystem::u8path(value());
            options.ctwViewer = true;
        }
        else if (argument == "--ctw-top-view") options.ctwTopView = true;
        else if (argument == "--ctw-focus") {
            const std::string resource = value();
            size_t consumed = 0;
            options.ctwFocusResource = std::stoi(resource, &consumed);
            if (consumed != resource.size() || options.ctwFocusResource < 0 || options.ctwFocusResource > 65535)
                throw std::runtime_error("CTW focus must be a world-block resource ID between 0 and 65535.");
        }
        else if (argument == "--play-game") options.playGame = true;
        else if (argument == "--play-test") options.playTest = true;
        else if (argument == "--zombie-pedestrians") options.zombiePedestrians = true;
        else if (argument == "--scm") options.scm = std::filesystem::u8path(value());
        else if (argument == "--scm-mission") options.scmMission = std::stoi(value());
        else if (argument == "--scm-disassembly") options.scmDisassembly = std::filesystem::u8path(value());
        else if (argument == "--scm-patch") options.scmPatch = std::filesystem::u8path(value());
        else if (argument == "--scm-copy") options.scmCopy = std::filesystem::u8path(value());
        else if (argument == "--dyom") options.dyom = std::filesystem::u8path(value());
        else if (argument == "--dyom-editor") options.dyomEditor = true;
        else if (argument == "--edit-dyom") options.editDyom = std::filesystem::u8path(value());
        else if (argument == "--validate-dyom") options.validateDyom = std::filesystem::u8path(value());
        else if (argument == "--no-audio") options.audio = false;
        else if (argument == "--hide-ui") options.hideUi = true;
        else if (argument == "--quit-on-fault") options.quitOnFault = true;
        else if (argument == "--capture") options.capture = std::filesystem::u8path(value());
        else if (argument == "--report") options.report = std::filesystem::u8path(value());
        else if (argument == "--trace") options.trace = std::filesystem::u8path(value());
        else if (argument == "--capture-at") {
            LaunchOptions::TimedCapture capture;
            capture.seconds = std::stod(value());
            capture.path = std::filesystem::u8path(value());
            if (!std::isfinite(capture.seconds) || capture.seconds < 0 || capture.seconds > 86400)
                throw std::runtime_error("Capture time must be between 0 and 86400 seconds.");
            options.captures.push_back(std::move(capture));
        }
        else if (argument == "--frames") options.frames = std::stoull(value());
        else if (argument == "--duration") options.duration = std::stod(value());
        else if (argument == "--fixed-step") options.fixedStep = std::stof(value());
        else if (argument == "--frame-mode") {
            const auto mode=value();
            if(mode=="30")options.frameMode=StorylandPlayFrameMode::Fps30;
            else if(mode=="60")options.frameMode=StorylandPlayFrameMode::Fps60;
            else if(mode=="60-120")options.frameMode=StorylandPlayFrameMode::Fps60Hz120;
            else throw std::runtime_error("Frame mode must be 30, 60, or 60-120.");
        }
        else if (argument == "--width") options.width = std::stoi(value());
        else if (argument == "--height") options.height = std::stoi(value());
        else throw std::runtime_error("Unknown option: " + argument);
    }
    if (options.playGame && options.playTest)
        throw std::runtime_error("Choose either --play-game or --play-test.");
    if (options.playGame && !options.dyom.empty())
        throw std::runtime_error("DYOM runs in Play Test. Choose either --dyom or --play-game.");
    if (!options.dyom.empty() && options.game.empty())
        throw std::runtime_error("Provide --game with the original VCS game directory to play a DYOM mission.");
    if (options.scm.empty() && (!options.scmDisassembly.empty() || !options.scmPatch.empty() ||
        !options.scmCopy.empty() || options.scmMission != -2))
        throw std::runtime_error("Provide --scm before selecting a mission, applying a patch, or exporting SCM data.");
    if (options.scmMission < -2)
        throw std::runtime_error("SCM mission slots start at zero; use -1 for MAIN.");
    if ((!options.scmPatch.empty() || options.scmMission != -2) &&
        options.scmDisassembly.empty() && options.scmCopy.empty())
        throw std::runtime_error("Use --scm-disassembly or --scm-copy with command-line patch/mission selection; the workbench also provides these controls.");
    if (!options.validateDyom.empty() && (!options.scmDisassembly.empty() || !options.scmCopy.empty()))
        throw std::runtime_error("Run DYOM validation and SCM export as separate commands.");
    if (options.width < 320 || options.width > 8192 || options.height < 240 || options.height > 8192)
        throw std::runtime_error("Window dimensions are outside 320x240 to 8192x8192.");
    if (!std::isfinite(options.fixedStep) || options.fixedStep < 0 || options.fixedStep > 0.1f)
        throw std::runtime_error("The fixed frame step must be between 0 and 0.1 seconds.");
    if (!std::isfinite(options.duration) || options.duration < 0 || options.duration > 86400)
        throw std::runtime_error("Run duration must be between 0 and 86400 seconds.");
    auto extension = options.game.extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return char(std::tolower(value)); });
    const bool ctwMode = options.ctwViewer || extension == ".pak";
    if (ctwMode) {
        for (const std::string& requested : requestedOptions) {
            for (const char* unsupported : {"--capture-at", "--trace", "--play-game", "--play-test",
                "--zombie-pedestrians", "--scm", "--scm-mission", "--scm-disassembly", "--scm-patch",
                "--scm-copy", "--dyom", "--dyom-editor", "--edit-dyom", "--validate-dyom",
                "--fixed-step", "--frame-mode", "--quit-on-fault"}) {
                if (requested == unsupported)
                    throw std::runtime_error(requested + " is not supported by the CTW city viewer.");
            }
        }
    } else if (options.ctwTopView || options.ctwFocusResource >= 0) {
        throw std::runtime_error("Use --ctw-pak GAME.PAK or --game GAME.PAK with CTW view options.");
    }
    return options;
}

int runModdingCommand(const LaunchOptions& options) {
    std::string error;
    if (!options.validateDyom.empty()) {
        StorylandDyomProject project;
        StorylandDyomCompiled compiled;
        if (!storylandLoadDyomProject(options.validateDyom, project, error) ||
            !storylandCompileDyomVcs(project, compiled, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        std::cout << "DYOM valid: " << project.title << '\n'
            << "Objectives: " << compiled.objectives.size() << ", entities: " << compiled.entities.size()
            << ", native bytecode: " << compiled.bytecode.size() << " bytes\n"
            << "Model and weapon availability is checked against game data when starting Play Test.\n";
        return 0;
    }
    if (options.scmDisassembly.empty() && options.scmCopy.empty()) return -1;
    StorylandScmWorkbench workbench;
    if (!workbench.load(options.scm, error) ||
        (!options.scmPatch.empty() && !workbench.importPatch(options.scmPatch, error))) {
        std::cerr << error << '\n';
        return 1;
    }
    if (options.scmMission != -2) {
        const auto& scopes = workbench.scopes();
        const auto found = std::find_if(scopes.begin(), scopes.end(), [&](const auto& scope) {
            return scope.mission == options.scmMission;
        });
        if (found == scopes.end() || !workbench.selectScope(size_t(found - scopes.begin()), error)) {
            std::cerr << (error.empty() ? "The selected SCM mission slot does not exist." : error) << '\n';
            return 1;
        }
    }
    if ((!options.scmDisassembly.empty() && !workbench.exportDisassembly(options.scmDisassembly,
        options.scmMission == -2, error)) ||
        (!options.scmCopy.empty() && !workbench.saveCopy(options.scmCopy, error))) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "SCM export completed from " << options.scm.u8string() << '\n'
        << "Original SHA-256: " << workbench.originalSha256() << '\n';
    return 0;
}

const char* missionStateName(StorylandMissionState state) {
    switch (state) {
    case StorylandMissionState::Idle: return "idle";
    case StorylandMissionState::Running: return "running";
    case StorylandMissionState::Waiting: return "waiting";
    case StorylandMissionState::Finished: return "finished";
    case StorylandMissionState::Unsupported: return "unsupported";
    case StorylandMissionState::Fault: return "fault";
    }
    return "unknown";
}

std::string jsonString(const std::string& value) {
    std::ostringstream result;
    result << '"';
    for (unsigned char character : value) {
        switch (character) {
        case '"': result << "\\\""; break;
        case '\\': result << "\\\\"; break;
        case '\n': result << "\\n"; break;
        case '\r': result << "\\r"; break;
        case '\t': result << "\\t"; break;
        default:
            if (character < 32)
                result << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(character) << std::dec;
            else result << char(character);
            break;
        }
    }
    result << '"';
    return result.str();
}

bool captureFrame(const std::filesystem::path& path, int width, int height, std::string& error) {
    if (width <= 0 || height <= 0 || uint64_t(width) * uint64_t(height) > 67108864u) {
        error = "Cannot capture an empty or oversized framebuffer.";
        return false;
    }
    std::vector<uint8_t> pixels(size_t(width) * size_t(height) * 4);
    GLint alignment = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    const size_t rowBytes = size_t(width) * 4;
    for (int row = 0; row < height / 2; ++row)
        for (size_t column = 0; column < rowBytes; ++column)
            std::swap(pixels[size_t(row) * rowBytes + column],
                pixels[size_t(height - row - 1) * rowBytes + column]);
    std::error_code filesystemError;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), filesystemError);
    if (filesystemError) { error = filesystemError.message(); return false; }
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = png_uint_32(width);
    image.height = png_uint_32(height);
    image.format = PNG_FORMAT_RGBA;
    if (!png_image_write_to_file(&image, path.string().c_str(), 0, pixels.data(), 0, nullptr)) {
        error = image.message;
        png_image_free(&image);
        return false;
    }
    png_image_free(&image);
    error.clear();
    return true;
}

bool writeReport(const std::filesystem::path& path, const StorylandLinuxSession& session,
    const StorylandLinuxAudio& audio, uint64_t frames, double wallSeconds,
    const std::string& renderer, std::string& error) {
    std::error_code filesystemError;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), filesystemError);
    if (filesystemError) { error = filesystemError.message(); return false; }
    std::ofstream output(path);
    if (!output) { error = "Could not open the report destination."; return false; }
    output << std::boolalpha << std::setprecision(9)
        << "{\n  \"version\": \"" STORYLAND_VERSION "\",\n"
        << "  \"platform\": \"Linux\",\n"
        << "  \"renderer\": " << jsonString(renderer) << ",\n"
        << "  \"rendered_frames\": " << frames << ",\n"
        << "  \"wall_runtime_seconds\": " << wallSeconds << ",\n"
        << "  \"average_render_fps\": " << (wallSeconds > 0 ? frames / wallSeconds : 0) << ",\n"
        << "  \"game_milliseconds\": " << session.environment.gameMilliseconds << ",\n"
        << "  \"simulation_frames\": " << session.environment.nativeFrameCounter << ",\n"
        << "  \"game_loaded\": " << session.loaded() << ",\n"
        << "  \"game_active\": " << session.active() << ",\n"
        << "  \"zombie_pedestrians\": " << session.runtime.zombiePedestrians << ",\n"
        << "  \"zombie_pedestrian_count\": " << std::count_if(session.runtime.pedestrians.begin(),
            session.runtime.pedestrians.end(),[](const auto& ped){return ped.zombie.active&&ped.health>0;}) << ",\n"
        << "  \"paused\": " << session.paused << ",\n"
        << "  \"game\": \"" << (session.vcs() ? "VCS" : "LCS") << "\",\n"
        << "  \"status\": " << jsonString(session.status()) << ",\n"
        << "  \"script_state\": " << jsonString(missionStateName(session.mission.state)) << ",\n"
        << "  \"script_name\": " << jsonString(session.mission.scriptName) << ",\n"
        << "  \"script_diagnostic\": " << jsonString(session.mission.diagnostic) << ",\n"
        << "  \"script_pc\": " << session.mission.pc << ",\n"
        << "  \"script_instructions\": " << session.mission.instructions << ",\n"
        << "  \"program_status\": " << jsonString(session.program.status()) << ",\n"
        << "  \"background_script_errors\": [";
    bool firstError = true;
    for (const auto& diagnostic : session.program.threadErrors()) {
        if (!firstError) output << ',';
        firstError = false;
        output << jsonString(diagnostic);
    }
    output << "],\n";
    if (session.dyomActive()) {
        const auto& compiled = session.dyomCompiled();
        const auto outcome = session.mission.missionOutcome;
        output << "  \"dyom_active\": true,\n"
            << "  \"dyom_title\": " << jsonString(compiled.title) << ",\n"
            << "  \"dyom_objective\": " << compiled.objectiveAt(session.mission.pc) << ",\n"
            << "  \"dyom_objective_count\": " << compiled.objectives.size() << ",\n"
            << "  \"dyom_outcome\": " << jsonString(outcome == StorylandMissionOutcome::Passed ? "passed" :
                outcome == StorylandMissionOutcome::Failed ? "failed" : "running") << ",\n";
    }
    output << "  \"fade_alpha\": " << session.environment.fadeAlpha << ",\n"
        << "  \"cutscene\": " << jsonString(session.environment.cutscene.name) << ",\n"
        << "  \"cutscene_playing\": " << session.environment.cutscene.playing << ",\n"
        << "  \"cutscene_milliseconds\": " << session.environment.cutscene.elapsedMilliseconds << ",\n"
        << "  \"cutscene_actors\": " << session.cutscene.scene().actors().size() << ",\n"
        << "  \"mission_vehicle_count\": " << std::count_if(session.environment.missionVehicles.begin(),
            session.environment.missionVehicles.end(), [](const auto& item) { return !item.second.deleted; }) << ",\n"
        << "  \"vehicle_actor_slots\": " << session.vehicles.size() << ",\n"
        << "  \"mission_character_count\": " << session.environment.missionCharacters.size() << ",\n"
        << "  \"player_position\": [" << session.playerPosition.x << ", "
        << session.playerPosition.y << ", " << session.playerPosition.z << "],\n"
        << "  \"audio_enabled\": " << audio.isEnabled() << ",\n"
        << "  \"audio_playing\": " << audio.isPlaying() << ",\n"
        << "  \"audio_milliseconds\": " << audio.playbackMilliseconds() << ",\n"
        << "  \"radio_channel\": " << audio.currentRadioChannel(session.vcs()) << ",\n"
        << "  \"radio_playing\": " << audio.radioIsPlaying() << ",\n"
        << "  \"radio_milliseconds\": " << audio.radioPlaybackMilliseconds() << ",\n"
        << "  \"audio_underruns\": " << audio.underrunCount() << ",\n"
        << "  \"maximum_audio_queue_bytes\": " << audio.maximumQueuedBytes() << ",\n"
        << "  \"audio_error\": " << jsonString(audio.missionError()) << ",\n"
        << "  \"audio_status\": " << jsonString(audio.status()) << "\n}\n";
    if (!output) { error = "Could not write the complete report."; return false; }
    return true;
}

void traceFrame(std::ostream& output, const StorylandLinuxSession& session,
    const StorylandLinuxAudio& audio, const StorylandLinuxRenderer& renderer,
    uint64_t frame, float elapsed) {
    const auto& world = session.environment;
    const StorylandNativePoint camera = world.camera.active ?
        StorylandNativePoint{world.camera.x, world.camera.y, world.camera.z} : session.camera.eye;
    const StorylandNativePoint target = world.camera.active ?
        StorylandNativePoint{world.camera.targetX, world.camera.targetY, world.camera.targetZ} :
        session.camera.aimLocked && !session.vcs() ? session.camera.aimTarget : session.camera.target;
    output << std::boolalpha << std::setprecision(9)
        << "{\"frame\":" << frame << ",\"game_ms\":" << world.gameMilliseconds
        << ",\"simulation_frame\":" << world.nativeFrameCounter
        << ",\"wall_frame_seconds\":" << elapsed
        << ",\"ps2_reflection_capture_ms\":" << renderer.ps2EnvironmentCaptureMilliseconds()
        << ",\"script\":" << jsonString(missionStateName(session.mission.state))
        << ",\"background_errors\":" << session.program.threadErrors().size()
        << ",\"pc\":" << session.mission.pc << ",\"mission\":" << session.program.activeMission()
        << ",\"fade\":" << world.fadeAlpha << ",\"cutscene\":" << jsonString(world.cutscene.name)
        << ",\"cutscene_playing\":" << world.cutscene.playing
        << ",\"cutscene_ms\":" << world.cutscene.elapsedMilliseconds
        << ",\"audio_ms\":" << audio.playbackMilliseconds()
        << ",\"audio_playing\":" << audio.isPlaying() << ",\"audio_underruns\":" << audio.underrunCount()
        << ",\"paused\":" << (session.paused || session.runtime.paused || session.runtime.pauseMenu)
        << ",\"audio_status\":" << jsonString(audio.status())
        << ",\"audio_error\":" << jsonString(audio.missionError())
        << ",\"radio_channel\":" << world.radioChannel << ",\"radio_position\":" << world.radioPosition
        << ",\"radio_playing\":" << audio.radioIsPlaying() << ",\"radio_ms\":" << audio.radioPlaybackMilliseconds()
        << ",\"player\":[" << session.playerPosition.x << ',' << session.playerPosition.y << ',' << session.playerPosition.z << ']'
        << ",\"player_control\":" << world.control << ",\"player_vehicle\":" << session.runtime.occupiedVehicle
        << ",\"player_aiming\":" << session.runtime.playerAiming
        << ",\"target_locked\":" << storylandHasPlayTestTarget(session.runtime)
        << ",\"target_ped\":" << session.runtime.targetedPed
        << ",\"target_mission_kind\":" << int(world.playerTarget.kind)
        << ",\"target_mission_handle\":" << world.playerTarget.handle
        << ",\"manual_aim_mode\":" << session.runtime.manualAimMode
        << ",\"aim_direction_valid\":" << session.playerBlend.weaponAimDirectionValid
        << ",\"aim_yaw\":" << session.playerBlend.weaponAimYaw
        << ",\"aim_pitch\":" << session.playerBlend.weaponAimPitch
        << ",\"movement_speed\":" << session.playerBlend.measuredSpeed
        << ",\"movement_forward\":" << session.playerBlend.movementForward
        << ",\"movement_right\":" << session.playerBlend.movementRight
        << ",\"camera_active\":" << world.camera.active
        << ",\"camera\":[" << camera.x << ',' << camera.y << ',' << camera.z << ']'
        << ",\"target\":[" << target.x << ',' << target.y << ',' << target.z << ']'
        << ",\"status\":" << jsonString(session.status()) << ",\"vehicles\":[";
    bool first = true;
    for (const auto& [handle, vehicle] : world.missionVehicles) {
        if (!first) output << ',';
        first = false;
        output << "{\"handle\":" << handle << ",\"model\":" << vehicle.model
            << ",\"deleted\":" << vehicle.deleted << ",\"slot\":" << vehicle.spawnedSlot
            << ",\"position\":[" << vehicle.x << ',' << vehicle.y << ',' << vehicle.z << "]}";
    }
    output << "],\"render_stages_ms\":{";
    for(size_t stage=0;stage<StorylandRenderPerformance::Count;++stage) {
        if(stage)output<<',';
        output<<jsonString(StorylandRenderPerformance::names[stage])<<':'<<renderer.performance.milliseconds[stage];
    }
    output << "},\"mission_audio\":[";
    first = true;
    for (const auto& slot : world.audioSlots) {
        if (!first) output << ',';
        first = false;
        output << "{\"name\":" << jsonString(slot.name) << ",\"serial\":" << slot.serial
            << ",\"loaded\":" << slot.loaded << ",\"play\":" << slot.play
            << ",\"started\":" << slot.playbackStarted << ",\"finished\":" << slot.finished
            << ",\"duration_ms\":" << slot.durationMilliseconds << '}';
    }
    output << "]}\n" << std::flush;
}

void drawPresentation(const StorylandLinuxSession& session, bool scriptFault) {
    if (!session.active() && !scriptFault) return;
    const auto& world = session.environment;
    const auto size = ImGui::GetIO().DisplaySize;
    // Presentation overlays belong above the scene, below all diagnostic/UI
    // windows. A native black fade must never cover a stopped-script message.
    auto* draw = ImGui::GetBackgroundDrawList();
    if (world.widescreen) {
        const float bar = size.y * 0.12f;
        draw->AddRectFilled(ImVec2(0, 0), ImVec2(size.x, bar), IM_COL32_BLACK);
        draw->AddRectFilled(ImVec2(0, size.y - bar), size, IM_COL32_BLACK);
    }
    if (world.cutscene.playing) {
        if (const auto* subtitle = session.cutscene.scene().subtitleAt(world.cutscene.elapsedMilliseconds)) {
            float fontSize = std::max(16.0f, size.y / 36.0f);
            const float width = size.x * 0.82f;
            auto extent = ImGui::GetFont()->CalcTextSizeA(fontSize, width, width,
                subtitle->text.c_str());
            float top = size.y * 0.87f - extent.y;
            if (world.widescreen) {
                const float barHeight = size.y * 0.12f;
                const float padding = std::max(2.0f, size.y * 0.006f);
                const float availableHeight = std::max(1.0f, barHeight - padding * 2);
                for (unsigned pass = 0; pass < 4 && extent.y > availableHeight; ++pass) {
                    fontSize *= availableHeight / extent.y;
                    extent = ImGui::GetFont()->CalcTextSizeA(fontSize, width, width,
                        subtitle->text.c_str());
                }
                top = size.y - barHeight + (barHeight - extent.y) * 0.5f;
                draw->PushClipRect(ImVec2(0, size.y - barHeight), size, true);
            }
            const ImVec2 origin((size.x - extent.x) * 0.5f, top);
            draw->AddText(ImGui::GetFont(), fontSize, ImVec2(origin.x + 1, origin.y + 1),
                IM_COL32_BLACK, subtitle->text.c_str(), nullptr, width);
            draw->AddText(ImGui::GetFont(), fontSize, origin, IM_COL32_WHITE,
                subtitle->text.c_str(), nullptr, width);
            if (world.widescreen) draw->PopClipRect();
        }
    }
    if(!session.runtime.entryError.empty() &&
        session.runtime.elapsed < session.runtime.entryErrorUntil) {
        const auto size=ImGui::GetIO().DisplaySize;
        const auto extent=ImGui::CalcTextSize(session.runtime.entryError.c_str());
        const ImVec2 position((size.x-extent.x)*0.5f,size.y*0.78f);
        draw->AddText(ImVec2(position.x+1,position.y+1),IM_COL32_BLACK,session.runtime.entryError.c_str());
        draw->AddText(position,IM_COL32(255,190,190,255),session.runtime.entryError.c_str());
    }
    const float entryFade = std::clamp(session.runtime.entryFade, 0.0f, 1.0f);
    if (entryFade > 0) draw->AddRectFilled(ImVec2(0, 0), size, IM_COL32(0, 0, 0, int(entryFade * 255)));
    const float fade = std::clamp(world.fadeAlpha, 0.0f, 1.0f);
    if (fade > 0) draw->AddRectFilled(ImVec2(0, 0), size, IM_COL32(world.fadingColor[0],
        world.fadingColor[1], world.fadingColor[2], int(fade * 255)));
}

void drawScriptFailure(const StorylandLinuxSession& session, const std::string& audioError = {}) {
    const std::string message = !audioError.empty() ? audioError : session.mission.state == StorylandMissionState::Fault ||
        session.mission.state == StorylandMissionState::Unsupported || session.program.threadErrors().empty() ?
        session.status() : *session.program.threadErrors().begin();
    const auto size = ImGui::GetIO().DisplaySize;
    const float width = std::min(660.0f, std::max(220.0f, size.x - 40.0f));
    const float fontSize = std::max(16.0f, size.y / 45.0f);
    const float inset = 20.0f;
    const float textWidth = width - inset * 2;
    const auto extent = ImGui::GetFont()->CalcTextSizeA(fontSize, textWidth, textWidth,
        message.c_str());
    const float height = extent.y + fontSize * 5.0f + inset * 2;
    const ImVec2 origin((size.x - width) * 0.5f, std::max(20.0f, (size.y - height) * 0.5f));
    auto* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(28, 20, 27, 250), 8);
    draw->AddRect(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(231, 82, 151, 255), 8, 0, 2);
    draw->AddText(ImGui::GetFont(), fontSize * 1.2f, ImVec2(origin.x + inset, origin.y + inset),
        IM_COL32(255, 137, 184, 255), audioError.empty() ? "Storyland script stopped" : "Storyland audio stopped");
    draw->AddText(ImGui::GetFont(), fontSize, ImVec2(origin.x + inset, origin.y + inset + fontSize * 2),
        IM_COL32_WHITE, message.c_str(), nullptr, textWidth);
    std::ostringstream detail;
    detail << "Script " << session.mission.scriptName << " | PC 0x" << std::hex << session.mission.pc
        << "\nHome: show game controls";
    draw->AddText(ImGui::GetFont(), fontSize * 0.85f,
        ImVec2(origin.x + inset, origin.y + inset + fontSize * 2.8f + extent.y),
        IM_COL32(204, 198, 207, 255), detail.str().c_str(), nullptr, textWidth);
}
}

int main(int argc, char** argv) {
    std::cout.setf(std::ios::unitbuf);
    for (int index = 1; index < argc; ++index)
        if (std::string(argv[index]) == "--help" || std::string(argv[index]) == "-h") {
            printHelp();
            return 0;
        }
    LaunchOptions options;
    try { options = parseOptions(argc, argv); }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 2;
    }
    try {
        auto extension = options.game.extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char value) { return char(std::tolower(value)); });
        if (options.ctwViewer || extension == ".pak") {
            StorylandLinuxCtwOptions city;
            city.archive = options.game;
            city.capture = options.capture;
            city.report = options.report;
            city.width = options.width;
            city.height = options.height;
            city.frames = options.frames;
            city.duration = options.duration;
            city.hideUi = options.hideUi;
            city.topView = options.ctwTopView;
            city.focusResource = options.ctwFocusResource;
            return storylandRunLinuxCtwViewer(city);
        }
        const int result = runModdingCommand(options);
        if (result >= 0) return result;
    } catch (const std::exception& exception) {
        std::cerr << "Modding command failed: " << exception.what() << '\n';
        return 1;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        return 1;
    }
    if (options.audio && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        std::cerr << "Audio initialization failed: " << SDL_GetError() << '\n';
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_Window* window = SDL_CreateWindow("Storyland " STORYLAND_VERSION " | Linux",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, options.width, options.height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 320, 240);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context || SDL_GL_MakeCurrent(window, context) != 0) {
        std::cerr << "OpenGL context creation failed: " << SDL_GetError() << '\n';
        if (context) SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(0);
    const auto* rendererBytes = glGetString(GL_RENDERER);
    const std::string rendererName = rendererBytes ? reinterpret_cast<const char*>(rendererBytes) : "Unknown OpenGL renderer";
    std::cout << "Storyland " STORYLAND_VERSION " | " << rendererName << '\n';

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    storylandInitializeUnicodeFont();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();
    auto& colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Button] = ImVec4(0.51f, 0.10f, 0.30f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.70f, 0.16f, 0.42f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.20f, 0.48f, 1.0f);
    ImGui_ImplSDL2_InitForOpenGL(window, context);
    ImGui_ImplOpenGL2_Init();

    int exitCode = 0;
    {
        StorylandLinuxSession session;
        StorylandScriptLogPopup scriptLogPopup;
        StorylandController gamepad;
        gamepad.select(StorylandControllerMode::Gamepad);
        std::array<int,20> previousGamepad{};
        StorylandLinuxRenderer renderer;
        StorylandLinuxAudio audio;
        StorylandMissionVoicePreview missionVoicePreview(
            [&](const StorylandMissionVoicePcm& pcm, std::string& failure) {
                return audio.playGeneratedVoice(pcm.samples, pcm.sampleRate, failure);
            },
            [&] { return audio.generatedVoicePlaying(); },
            [&] { audio.stopGeneratedVoice(); });
        StorylandLoadingPresentation loadingPresentation;
        StorylandOpeningMovies<StorylandMoviePlayer> openingMovies;
        StorylandOpeningMovieSkipInput openingSkipInput;
        std::vector<StorylandOpeningMovie> pendingOpeningMovies;
        bool openingPending=false;
        std::string openingDiagnostic;
        bool loadingAudioEnabled=options.audio;
        std::filesystem::path pendingGamePath;
        StorylandCoopPanel coopPanel;
        StorylandPortableChat coopChat;
        StorylandGallery gallery;
        StorylandGameMenu menu;
        StorylandModdingUi modding;
        StorylandDisplaySettings displaySettings;
        const auto documentsDirectory = linuxDocumentsDirectory();
        std::string gamepadBindingsError;
        gamepad.loadBindings(storylandGameUserDirectory(documentsDirectory,true),gamepadBindingsError);
        const auto languagePath=storylandGameUserDirectory(documentsDirectory,true)/"storyland_ui_language.cfg";
        storylandLoadLinuxUiLanguage(languagePath);
        uint32_t lastGalleryPhoto=0;
        bool galleryCaptureRequested=false;
        std::string error;
        audio.setEnabled(options.audio);
        const auto audioModePath=storylandGameUserDirectory(documentsDirectory,true)/"storyland_audio_mode.cfg";
        {
            std::ifstream saved(audioModePath);
            int selected=int(StorylandAudioMode::Ps2);
            if(saved>>selected)audio.setMode(storylandAudioModeFromInt(selected));
        }
        const auto saveAudioMode=[&](StorylandAudioMode selected) {
            std::error_code directoryError;
            std::filesystem::create_directories(audioModePath.parent_path(),directoryError);
            if(directoryError) {error="Audio setting directory: "+directoryError.message();return;}
            std::ofstream saved(audioModePath,std::ios::trunc);
            saved<<int(selected)<<"\n";
            saved.close();
            if(!saved)error="Could not save the selected audio mode.";
        };
        session.preloadWorldScene = [&](const std::array<float,3>& point, std::string& error) {
            return renderer.loadScene(session, point, error);
        };
        session.prepareCredits = [&](int32_t group,
            std::vector<StorylandMissionEnvironment::CreditsLine>& lines,std::string& error) {
            return renderer.prepareCredits(session,group,lines,error);
        };
        session.cutsceneElapsedMilliseconds = [&](uint32_t milliseconds) {
            audio.synchronize(session);
            return audio.advance(milliseconds, session.paused || session.runtime.paused || session.runtime.pauseMenu);
        };
        session.currentRadioChannel = [&]() { return audio.currentRadioChannel(session.vcs()); };
        session.musicStreamPlaying = [&](int32_t stream) { return audio.musicStreamIsPlaying(stream); };
        session.musicStreamPositionMilliseconds = [&]() { return audio.scriptMusicPositionMilliseconds(); };
        session.playMissionTune = [&](int32_t stream,bool looping,std::string& error) {
            return audio.playMissionTune(session,stream,looping,error);
        };
        session.stopMissionTune = [&]() { audio.stopMissionTune(); };
        std::array<char, 4096> gamePath{};
        if (!options.game.empty())
            std::snprintf(gamePath.data(), gamePath.size(), "%s", options.game.u8string().c_str());
        displaySettings.load(storylandGameUserDirectory(documentsDirectory,true),error);
        renderer.aspectRatio = displaySettings.aspectRatio;
        renderer.graphicsQuality=displaySettings.graphicsQuality;
        renderer.trails=displaySettings.trails;
        std::string lastStatus;
        std::ofstream trace;
        if (!options.trace.empty()) {
            std::error_code pathError;
            if (!options.trace.parent_path().empty())
                std::filesystem::create_directories(options.trace.parent_path(), pathError);
            if (!pathError) trace.open(options.trace);
            if (pathError || !trace) {
                std::cerr << "Could not open the frame trace destination.\n";
                exitCode = 1;
            }
        }
        renderer.performance.enabled=trace.is_open();
        bool panelsVisible = false;
        bool debugVisible = false;
        StorylandPortableDtzEditor dtzEditor;
        int selectedWeapon = 0;
        bool quit = false;
        bool quitRequested = false;
        const auto requestQuit = [&]() {
            if (modding.hasUnsavedChanges()) quitRequested = true;
            else quit = true;
        };
        uint64_t renderedFrames = 0;
        bool presentationDirty=true,presentationSynchronized=false;
        int presentationDisplay=-1;
        StorylandPlayFrameMode presentationMode=StorylandPlayFrameMode::Fps60;
        StorylandPlayFrameMode simulationMode=options.frameMode;
        StorylandSimulationCadence simulationCadence;
        bool pendingJump=false,pendingInteract=false,pendingReload=false,pendingFire=false,pendingAim=false;
        bool cameraCentreHeld=false,pendingCameraCentre=false;
        StorylandPcKeyPress weaponSwitch;
        StorylandVehicleSirenControls sirenControls;
        float pendingYaw=0,pendingPitch=0;
        std::array<int16_t,20> pendingButtons{};
        const auto finishOpeningMovies=[&]() {
            openingSkipInput.finish();
            audio.setEnabled(loadingAudioEnabled);
            menu.showMain();
            simulationCadence.reset();
            ImGui::SetWindowFocus(nullptr);
        };
        const auto prepareLoading = [&]() {
            if(!loadingPresentation.active())loadingAudioEnabled=audio.isEnabled();
            audio.setEnabled(false);
            simulationCadence.reset();
            SDL_SetRelativeMouseMode(SDL_FALSE);
        };
        const auto beginLoading = [&](const std::filesystem::path& path, bool vcs) {
            prepareLoading();
            loadingPresentation.start(path,vcs);
        };
        const auto beginOpening = [&]() {
            prepareLoading();
            loadingPresentation.startOpening();
        };
        const auto refreshLoadingChapter = [&]() {
            if(!loadingPresentation.active()||!session.active()||!session.vcs())return;
            const auto& globals=session.program.active()?
                session.program.persistentSaveGlobals():session.mission.globals;
            loadingPresentation.setChapter(
                storylandReadVcsLoadingChapter(session.script.rawBytes(),globals).value_or(1));
        };
        const auto cancelLoading = [&]() {
            loadingPresentation.cancel();
            audio.setEnabled(loadingAudioEnabled);
        };
        const auto drawLoadingFrame = [&](const char* stage) {
            SDL_Event loadingEvent;
            while(SDL_PollEvent(&loadingEvent)) {
                ImGui_ImplSDL2_ProcessEvent(&loadingEvent);
                if(loadingEvent.type==SDL_QUIT||(loadingEvent.type==SDL_WINDOWEVENT&&
                    loadingEvent.window.event==SDL_WINDOWEVENT_CLOSE))requestQuit();
                if(loadingEvent.type==SDL_DROPFILE)SDL_free(loadingEvent.drop.file);
            }
            int width=0,height=0;
            SDL_GL_GetDrawableSize(window,&width,&height);
            if(width<=0||height<=0) {SDL_Delay(20);return;}
            if(!loadingPresentation.beginFrame())return;
            ImGui_ImplOpenGL2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            glViewport(0,0,width,height);glDisable(GL_SCISSOR_TEST);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
            glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            loadingPresentation.draw(stage);
            ImGui::Render();
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(window);
            SDL_Delay(8);
        };
        const auto openGame = [&](const std::filesystem::path& path,bool openingFrontend=true) {
            if(openingPending||openingMovies.active())audio.setEnabled(loadingAudioEnabled);
            openingMovies.stop();
            pendingOpeningMovies.clear();
            openingPending=false;
            openingSkipInput.reset();
            openingDiagnostic.clear();
            audio.stop();
            renderer.shutdown();
            error.clear();
            beginOpening();
            try {
                auto opening=std::async(std::launch::async,[&,path]() {return session.loadGame(path,error);});
                do {drawLoadingFrame("Opening game data...");}
                while(opening.wait_for(std::chrono::seconds(0))!=std::future_status::ready);
                if(!opening.get()||quit) {cancelLoading();return false;}
                loadingPresentation.setProgress(.5f);
                loadingPresentation.start(session.gameRoot(),session.vcs());
                dtzEditor.reset();
                if(!session.archive.companionStatus().empty())
                    std::cout<<"Archive: "<<session.archive.companionStatus()<<'\n';
                drawLoadingFrame("Preparing graphics...");
                if(!renderer.initialize(session,error)||
                    !displaySettings.load(storylandGameUserDirectory(documentsDirectory,session.vcs()),error)) {
                    cancelLoading();return false;
                }
                renderer.aspectRatio=displaySettings.aspectRatio;
                renderer.graphicsQuality=displaySettings.graphicsQuality;
                renderer.trails=displaySettings.trails;
                if(openingFrontend&&!options.hideUi) {
                    pendingOpeningMovies=linuxOpeningMovies(session,renderer.pcShaderEnabled());
                    openingPending=true;
                    menu.close();
                }
                loadingPresentation.ready();
                return true;
            } catch(const std::exception& exception) {
                error=exception.what();cancelLoading();return false;
            }
        };
        if (!options.game.empty()) {
            const bool openingFrontend=!options.playGame&&!options.playTest&&options.dyom.empty()&&
                !options.dyomEditor&&options.editDyom.empty()&&options.scm.empty();
            if (!openGame(options.game,openingFrontend)) { std::cerr << error << '\n'; exitCode = 1; }
            else if (!options.dyom.empty()) {
                StorylandDyomProject project;
                if (!storylandLoadDyomProject(options.dyom, project, error) || !session.startDyom(project, error)) {
                    std::cerr << error << '\n'; exitCode = 1;
                }
            }
            else if (options.playGame && !session.startGame(error)) { std::cerr << error << '\n'; exitCode = 1; }
            else if (options.playTest && !session.startTest(error)) { std::cerr << error << '\n'; exitCode = 1; }
            if(!exitCode&&options.zombiePedestrians&&!session.setZombiePedestrians(true,error)) {
                std::cerr<<error<<'\n';exitCode=1;
            }
        }
        if (!options.scm.empty()) modding.showScm(options.scm);
        refreshLoadingChapter();
        if (options.dyomEditor || !options.editDyom.empty()) modding.showDyom(options.editDyom);
        if (!options.hideUi && !session.active() && !modding.visible() && !openingPending) menu.showMain();
        const auto menuServices = [&]() {
            StorylandGameMenuServices services;
            services.artwork = renderer.frontendAssets();
            services.loaded = session.loaded();
            services.frontendSound = [&](uint16_t event) {audio.playFrontendSound(session,event);};
            services.vcsVisualGlow = services.loaded && session.vcs();
            services.active = session.active() && session.isPlayingGame();
            services.title = session.loaded() ? (session.vcs() ? "Grand Theft Auto: Vice City Stories" :
                "Grand Theft Auto: Liberty City Stories") : "Storyland " STORYLAND_VERSION;
            services.canSave = session.canSaveGame(&services.saveRestriction);
            services.newGame = [&](std::string& failure) {
                audio.stop();
                if (session.active()) session.stopGame();
                beginLoading(session.gameRoot(),session.vcs());
                if (!session.startGame(failure)) {cancelLoading();return false;}
                refreshLoadingChapter();
                loadingPresentation.ready();
                session.paused = false;
                session.runtime.pauseMenu = false;
                panelsVisible = debugVisible = false;
                ImGui::SetWindowFocus(nullptr);
                return true;
            };
            services.resume = [&]() {
                menu.close();
                session.paused = false;
                session.runtime.paused = false;
                session.runtime.pauseMenu = false;
                session.runtime.pauseMap = false;
                session.runtime.pauseOptions = false;
                ImGui::SetWindowFocus(nullptr);
            };
            services.returnToMain = [&]() {
                audio.stop(); session.stopGame();
                panelsVisible = debugVisible = false;
            };
            services.quit = requestQuit;
            services.gallery = [&]() {
                gallery.open(storylandGameUserDirectory(documentsDirectory,session.vcs()));
            };
            services.debug = [&]() {
                panelsVisible = true;
                debugVisible = session.active();
                session.paused = session.active();
                session.runtime.pauseMenu = false;
                menu.close();
            };
            services.slotExists = [&](int slot) { return session.hasGameSaveSlot(documentsDirectory,slot); };
            services.slotLabel = [](int) { return std::string("Saved game"); };
            services.loadGame = [&](int slot,std::string& failure) {
                audio.stop();
                beginLoading(session.gameRoot(),session.vcs());
                if (!session.loadGameSlot(documentsDirectory,slot,failure)) {cancelLoading();return false;}
                refreshLoadingChapter();
                loadingPresentation.ready();
                panelsVisible = debugVisible = false;
                return true;
            };
            services.saveGame = [&](int slot,std::string& failure) {
                return session.saveGameSlot(documentsDirectory,slot,failure);
            };
            services.deleteGame = [&](int slot,std::string& failure) {
                return session.deleteGameSlot(documentsDirectory,slot,failure);
            };
            services.finishSaveMenu = [&]() {
                session.environment.saveMenuRequested = false;
                session.environment.saveGameFinished = true;
            };
            services.drawGameSource = [&]() {
                ImGui::TextWrapped("%s",session.loaded() ? session.gameRoot().string().c_str() : "Open your game folder or GAME.DTZ to begin.");
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##GameSource",gamePath.data(),gamePath.size());
                if (ImGui::Button("Open game data",ImVec2(-1,32))) pendingGamePath=std::filesystem::u8path(gamePath.data());
                if (!error.empty()) ImGui::TextWrapped("%s",error.c_str());
                ImGui::Spacing();
            };
            services.drawStats = [&]() {
                const auto rows=storylandStatsMenuRows(session.environment,session.vcs());
                if(ImGui::BeginTable("##StorylandStats",2,ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Statistic",ImGuiTableColumnFlags_WidthStretch,3.0f);
                    ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch,1.0f);
                    for(const auto& row:rows) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        const auto translated=session.translateMissionText(row.labelKey,"MAIN");
                        ImGui::TextUnformatted(translated==row.labelKey?row.fallback:translated.c_str());
                        ImGui::TableSetColumnIndex(1);
                        const float width=ImGui::CalcTextSize(row.value.c_str()).x;
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.0f,ImGui::GetContentRegionAvail().x-width));
                        ImGui::TextUnformatted(row.value.c_str());
                    }
                    ImGui::EndTable();
                }
            };
            services.drawSettings = [&]() {
                int shader = storylandShaderSelectionIndex(renderer.shaderProfile());
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("Shader", &shader, StorylandPortableShaderNames.data(),
                    int(StorylandPortableShaderNames.size())))
                    renderer.setShaderProfile(StorylandPortableShaderProfiles[size_t(shader)], error);
                int aspect = int(displaySettings.aspectRatio);
                ImGui::TextUnformatted("Aspect ratio");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##AspectRatio",&aspect,StorylandAspectRatioNames,5)) {
                    displaySettings.aspectRatio = StorylandAspectRatio(aspect);
                    renderer.aspectRatio = displaySettings.aspectRatio;
                    displaySettings.save(storylandGameUserDirectory(documentsDirectory,session.loaded()?session.vcs():true),error);
                }
                ImGui::TextWrapped("The view fits inside the window. Black bars preserve the selected shape.");
                bool graphicsChanged=ImGui::Checkbox("Trails",&displaySettings.trails);
                graphicsChanged|=storylandDrawGraphicsAdvancedOptions(displaySettings.graphicsQuality,
                    renderer.maximumAnisotropy(),renderer.graphicsQualityApplies(),renderer.fxaaStatus());
                if(graphicsChanged) {
                    renderer.graphicsQuality=displaySettings.graphicsQuality;
                    renderer.trails=displaySettings.trails;
                    displaySettings.save(storylandGameUserDirectory(documentsDirectory,session.loaded()?session.vcs():true),error);
                }
                const char* frameModes[]={"30 FPS","60 FPS","60 FPS + 120 Hz"};
                int frameMode=int(options.frameMode);
                ImGui::SetNextItemWidth(-1);
                if(ImGui::Combo("Frame rate",&frameMode,frameModes,3))
                    options.frameMode=StorylandPlayFrameMode(frameMode);
                bool sound = audio.isEnabled();
                if (ImGui::Checkbox("Game audio",&sound)) {
                    if (sound && !(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO)!=0)
                        error = SDL_GetError();
                    else audio.setEnabled(sound);
                }
                int audioProfile=int(audio.mode());
                if(ImGui::Combo("Audio Mode",&audioProfile,"PC (Enhanced)\0PS2\0PSP\0")) {
                    audio.setMode(storylandAudioModeFromInt(audioProfile));
                    saveAudioMode(audio.mode());
                }
                ImGui::TextWrapped("PC (Enhanced): extra clarity. PS2: original sound. "
                    "PSP: softer treble and narrower stereo.");
                ImGui::Checkbox("HUD",&session.runtime.hud);
                ImGui::Checkbox("Subtitles",&session.environment.missionAudioSubtitleEnabled);
                if (!error.empty()) ImGui::TextWrapped("%s",error.c_str());
            };
            return services;
        };
        if (exitCode && options.frames) quit = true;
        auto previousTime = std::chrono::steady_clock::now();
        const auto startedAt = previousTime;
        double nextPresentation=0;
        while (!quit) {
            missionVoicePreview.update();
            bool jumpPressed = false;
            bool interactPressed = false;
            bool reloadPressed = false;
            bool clearUiFocus = false;
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if(event.type==SDL_KEYDOWN||
                    (event.type==SDL_MOUSEBUTTONDOWN&&event.button.which!=SDL_TOUCH_MOUSEID))
                    renderer.helpBindings.keyboard=true;
                else if(event.type==SDL_CONTROLLERBUTTONDOWN||
                    (event.type==SDL_CONTROLLERAXISMOTION&&std::abs(int(event.caxis.value))>12000))
                    renderer.helpBindings.keyboard=false;
                if(event.type==SDL_DISPLAYEVENT||(event.type==SDL_WINDOWEVENT&&
                    event.window.event==SDL_WINDOWEVENT_DISPLAY_CHANGED))presentationDirty=true;
                if(loadingPresentation.active()||openingPending||
                    openingMovies.active()||openingSkipInput.blocksMenu()) {
                    if(event.type==SDL_QUIT||(event.type==SDL_WINDOWEVENT&&
                        event.window.event==SDL_WINDOWEVENT_CLOSE))requestQuit();
                    if(event.type==SDL_DROPFILE)SDL_free(event.drop.file);
                    continue;
                }
                if (event.type == SDL_KEYDOWN && coopChat.open) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) coopChat.close(SDL_SCANCODE_ESCAPE);
                    continue;
                }
                if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym == SDLK_t &&
                    session.coopNetwork().connected() && session.active() && !session.paused &&
                    !menu.open() && !io.WantTextInput && !io.WantCaptureKeyboard) {
                    coopChat.show();
                    continue;
                }
                if (event.type == SDL_QUIT) requestQuit();
                if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
                    requestQuit();
                if (event.type == SDL_KEYDOWN && !event.key.repeat && !io.WantTextInput) {
                    if(event.key.keysym.sym==SDLK_F8&&session.loaded()) {scriptLogPopup.request();continue;}
                    if(event.key.keysym.sym==SDLK_F12&&session.active())galleryCaptureRequested=true;
                    if (event.key.keysym.sym == SDLK_HOME) {
                        panelsVisible = !panelsVisible;
                        clearUiFocus = !panelsVisible;
                    }
                    if (event.key.keysym.sym == SDLK_TAB && session.active() && !menu.open() &&
                            (!io.WantCaptureKeyboard || debugVisible)) {
                        debugVisible = !debugVisible;
                        session.paused = debugVisible;
                        session.runtime.pauseMenu = false;
                        clearUiFocus = !debugVisible;
                    }
                    if(session.active()&&!menu.open()&&!session.paused&&session.environment.cutscene.playing&&!io.WantCaptureKeyboard&&
                        (event.key.keysym.sym==SDLK_ESCAPE||event.key.keysym.sym==SDLK_RETURN||event.key.keysym.sym==SDLK_SPACE)) {
                        session.requestCutsceneSkip();
                        audio.synchronize(session);
                        continue;
                    }
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        if (session.active() && !session.vcs() && session.environment.credits.active) continue;
                        if (ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)) continue;
                        if (menu.open()) menu.back(menuServices());
                        else if (session.active() && !session.environment.pauseMenuDisabled &&
                            (!session.environment.widescreen || session.environment.allowPauseInWidescreen)) {
                            panelsVisible = debugVisible = false;
                            menu.showPause(); session.paused = true; session.runtime.pauseMenu = true;
                        } else if (!session.active()) menu.showMain();
                        continue;
                    }
                    if (!io.WantCaptureKeyboard && !session.paused &&
                            (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) {
                        if (event.key.keysym.sym == SDLK_SPACE && session.runtime.occupiedVehicle < 0)
                            jumpPressed = true;
                        if (event.key.keysym.sym == SDLK_f) interactPressed = true;
                        if (event.key.keysym.sym == SDLK_r && session.runtime.occupiedVehicle < 0) reloadPressed = true;
                        if ((event.key.keysym.sym == SDLK_r && session.runtime.occupiedVehicle >= 0) || event.key.keysym.sym == SDLK_RIGHTBRACKET)
                            audio.cycleRadio(session, 1);
                        if (event.key.keysym.sym == SDLK_LEFTBRACKET) audio.cycleRadio(session, -1);
                    }
                }
                if (event.type == SDL_DROPFILE) {
                    const std::filesystem::path path = std::filesystem::u8path(event.drop.file);
                    SDL_free(event.drop.file);
                    if (modding.requestOpen(path)) menu.close();
                    else {
                        std::snprintf(gamePath.data(), gamePath.size(), "%s", path.u8string().c_str());
                        if (session.active()) error = "Return to the main menu before opening different game data.";
                        else if (!openGame(path)) std::cerr << error << '\n';
                    }
                }
                if (event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.button == SDL_CONTROLLER_BUTTON_START && session.active()) {
                    if (menu.open()) menu.back(menuServices());
                    else if (!session.environment.pauseMenuDisabled &&
                        (!session.environment.widescreen || session.environment.allowPauseInWidescreen)) {
                        menu.showPause(); session.paused = true; session.runtime.pauseMenu = true;
                    }
                }
                if (event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.button == SDL_CONTROLLER_BUTTON_B && menu.open() &&
                    !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))
                    menu.back(menuServices());
            }
            const auto now = std::chrono::steady_clock::now();
            const float elapsed = std::chrono::duration<float>(now - previousTime).count();
            previousTime = now;
            const float seconds = options.fixedStep > 0 ? options.fixedStep : std::clamp(elapsed, 0.0f, 0.1f);
            ImGui_ImplOpenGL2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            ImGui::BeginDisabled(loadingPresentation.active()||openingPending||
                openingMovies.active()||openingSkipInput.blocksMenu());
            if (clearUiFocus) ImGui::SetWindowFocus(nullptr);
            if (session.active() && session.environment.saveMenuRequested) {
                session.environment.saveMenuRequested = false;
                session.paused = true; session.runtime.pauseMenu = true;
                panelsVisible = debugVisible = false;
                menu.showSave(true);
            }
            if (menu.open() && session.active()) { session.paused = true; session.runtime.pauseMenu = true; }
            if (!menu.open() && session.active() && session.paused && !debugVisible && error.empty() && audio.missionError().empty())
                menu.showPause();

            if (panelsVisible || debugVisible || !error.empty()) {
                ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(std::min(480.0f,io.DisplaySize.x-24),std::max(100.0f,io.DisplaySize.y-32)), ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.91f);
                if (ImGui::Begin("Storyland")) {
                    ImGui::InputText("Game folder or file", gamePath.data(), gamePath.size());
                    if (ImGui::Button("Open")) pendingGamePath=std::filesystem::u8path(gamePath.data());
                    if (session.loaded()) {
                        ImGui::BeginDisabled(!session.loaded() || session.active());
                        if (ImGui::Button("Play Game")) {
                            audio.stop();
                            error.clear();
                            beginLoading(session.gameRoot(),session.vcs());
                            if (session.startGame(error)) {
                                refreshLoadingChapter();
                                loadingPresentation.ready();
                                panelsVisible = debugVisible = false;
                                ImGui::SetWindowFocus(nullptr);
                            } else cancelLoading();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(storylandLinuxUiText(L"Play Test").c_str())) {
                            audio.stop();
                            error.clear();
                            beginLoading(session.gameRoot(),session.vcs());
                            if (session.startTest(error)) {
                                refreshLoadingChapter();
                                loadingPresentation.ready();
                                panelsVisible = debugVisible = false;
                                ImGui::SetWindowFocus(nullptr);
                            } else cancelLoading();
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        ImGui::BeginDisabled(!session.active());
                        if (ImGui::Button("Stop")) { audio.stop(); session.stopGame(); }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        if (ImGui::Button("MAIN.SCM")) modding.showScm(session.script.sourcePath());
                        ImGui::SameLine();
                        if (ImGui::Button("DYOM")) modding.showDyom();
                    }
                    ImGui::BeginDisabled(!session.active());
                    if (ImGui::Button("Pause menu")) {
                        menu.showPause(); session.paused = true; session.runtime.pauseMenu = true;
                        panelsVisible = debugVisible = false;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::BeginDisabled(!session.loaded());
                    if (ImGui::Button("Gallery")) gallery.open(storylandGameUserDirectory(linuxDocumentsDirectory(),session.vcs()));
                    ImGui::SameLine();
                    if (ImGui::Button("Co-op")) coopPanel.open = true;
                    ImGui::EndDisabled();
                    bool soundEnabled = audio.isEnabled();
                    if (ImGui::Checkbox("Game audio", &soundEnabled)) audio.setEnabled(soundEnabled);
                    ImGui::BeginDisabled(!session.loaded());
                    if (ImGui::Button("Edit car handling / pedestrian stats")) {
                        dtzEditor.open = true;
                    }
                    ImGui::EndDisabled();
                    if(ImGui::BeginMenu(storylandLinuxUiText(L"Options").c_str())) {
                        if(ImGui::BeginMenu(storylandLinuxUiText(L"Language").c_str())) {
                            const char* names[]={"English",u8"Français (Québec)",u8"Italiano",u8"Русский"};
                            for(unsigned index=0;index<4;++index) {
                                if(ImGui::MenuItem(names[index],nullptr,
                                    static_cast<unsigned>(gStorylandUiLanguage)==index)) {
                                    gStorylandUiLanguage=static_cast<StorylandUiLanguage>(index);
                                    if(!storylandSaveLinuxUiLanguage(languagePath))
                                        error="Could not save the selected interface language.";
                                }
                            }
                            ImGui::EndMenu();
                        }
                        if(ImGui::BeginMenu("Audio")) {
                            for(int index=0;index<3;++index) {
                                const auto selected=storylandAudioModeFromInt(index);
                                if(ImGui::MenuItem(storylandAudioModeName(selected),nullptr,
                                    audio.mode()==selected)) {
                                    audio.setMode(selected);
                                    saveAudioMode(selected);
                                }
                            }
                            ImGui::EndMenu();
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu(storylandLinuxUiText(L"Play Options").c_str())) {
                        if(StorylandPlayOptionsSection section{"World",true}) {
                            ImGui::MenuItem("Inverted Map",nullptr,&session.environment.invertedMap);
                            ImGui::Separator();
                            renderer.drawEnvironmentControls(session);
                        }
                        if(StorylandPlayOptionsSection section{"Player movement",true}) {
                            if(session.vcs())
                                ImGui::MenuItem("Extended + SA Features",nullptr,&session.runtime.extendedAndSaFeatures,
                                    session.loaded()&&session.extendedCrouchAvailable);
                            else
                                ImGui::MenuItem("Extended + SA Crouch",nullptr,&session.runtime.extendedLcsCrouch,
                                    session.loaded()&&session.extendedCrouchAvailable);
                        }
                        if(StorylandPlayOptionsSection section{"Pedestrians",true}) {
                            ImGui::MenuItem("Aggressive Pedestrians",nullptr,&session.runtime.aggressivePedestrians);
                            bool zombiePedestrians=session.runtime.zombiePedestrians;
                            if(ImGui::MenuItem("Zombie Pedestrians",nullptr,&zombiePedestrians,session.loaded()&&session.vcs())) {
                                std::string optionError;
                                session.setZombiePedestrians(zombiePedestrians,optionError);
                            }
                            if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                storylandPlayOptionTooltip("Use the Brawn of the Dead zombie model, walk animation and walking behavior. Requires Vice City Stories.");
                        }
                        if(StorylandPlayOptionsSection section{"Vehicles",true})
                            ImGui::MenuItem("Mafia 2 Vehicle Collision",nullptr,&session.runtime.collisionOccupantDamage);
                        if(StorylandPlayOptionsSection section{"Visual effects",true}) {
                            int shader=storylandShaderSelectionIndex(renderer.shaderProfile());
                            if(ImGui::Combo("Shader",&shader,StorylandPortableShaderNames.data(),
                                int(StorylandPortableShaderNames.size())))
                                renderer.setShaderProfile(StorylandPortableShaderProfiles[size_t(shader)],error);
                            ImGui::MenuItem("Bounding boxes",nullptr,&renderer.showBounds);
                            if(ImGui::MenuItem("Cube-map viewer"))renderer.vehicleEnvironment.viewerOpen=true;
                            ImGui::SeparatorText("Mesh effects");
                            storylandMeshEffectMenu(renderer.meshEffects,session.animationSeconds());
                        }
                        if(StorylandPlayOptionsSection section{"Performance",true}) {
                            const char* frameModes[]={"30 FPS","60 FPS","60 FPS + 120 Hz"};
                            for(int mode=0;mode<3;++mode)
                                if(ImGui::MenuItem(frameModes[mode],nullptr,int(options.frameMode)==mode))
                                    options.frameMode=StorylandPlayFrameMode(mode);
                        }
                        if(StorylandPlayOptionsSection section{"Co-op",true}) {
                            storylandCoopFriendlyFireMenu(session.coopNetwork(), session.environment.coopFriendlyFire);
                            if (ImGui::MenuItem("Host / Join")) coopPanel.open = true;
                            ImGui::BeginDisabled(!session.coopNetwork().connected());
                            if (ImGui::MenuItem("Chat", "T")) coopChat.show();
                            ImGui::EndDisabled();
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::TextWrapped("%s", session.status().c_str());
                    if (!session.archive.companionStatus().empty())
                        ImGui::TextWrapped("%s", session.archive.companionStatus().c_str());
                    if (!audio.status().empty()) ImGui::TextWrapped("%s", audio.status().c_str());
                    if (!error.empty()) {
                        ImGui::TextColored(ImVec4(1, 0.45f, 0.45f, 1), "%s", error.c_str());
                        if (ImGui::Button("Dismiss message")) error.clear();
                    }
                    ImGui::Separator();
                    if(ImGui::CollapsingHeader("Controls")) {
                        ImGui::TextWrapped("W/S/A/D move | I/J/K/L camera | Shift sprint | Left Alt walk");
                        ImGui::TextWrapped("Space jump/handbrake | F enter/exit vehicle");
                        ImGui::TextWrapped("Right mouse free aim | E lock on | G/left mouse fire | R reload | T chat");
                        ImGui::TextWrapped("Q next weapon | B recenter | Vehicle: Q/E look sideways, R radio");
                        ImGui::TextWrapped("Home panels | Tab debug/pause | Escape pause");
                        ImGui::TextWrapped("F8 script log | F12 saves a screenshot to Gallery");
                    }
                    if (session.active() && !session.runtime.weapons.empty() &&
                        (debugVisible || ImGui::CollapsingHeader("Weapons"))) {
                        selectedWeapon = std::clamp(selectedWeapon, 0,
                            int(session.runtime.weapons.size()) - 1);
                        if (ImGui::BeginCombo("Weapon", session.runtime.weapons[size_t(selectedWeapon)].name.c_str())) {
                            for (size_t index = 0; index < session.runtime.weapons.size(); ++index)
                                if (ImGui::Selectable(session.runtime.weapons[index].name.c_str(),
                                    selectedWeapon == int(index))) selectedWeapon = int(index);
                            ImGui::EndCombo();
                        }
                        if (ImGui::Button("Give weapon")) {
                            const auto& definition = session.runtime.weapons[size_t(selectedWeapon)];
                            storylandGiveWeapon(session.runtime, size_t(selectedWeapon),
                                std::max(1, definition.clipSize * 3));
                        }
                    }
                    if (debugVisible || ImGui::CollapsingHeader("Script and scene")) {
                        if (ImGui::Button("Save script debug log...")) scriptLogPopup.request();
                        ImGui::Text("%.1f FPS", io.Framerate);
                        ImGui::Text("%s | PC 0x%zX | %zu instructions", missionStateName(session.mission.state),
                            session.mission.pc, session.mission.instructions);
                        ImGui::TextWrapped("%s", session.program.status().c_str());
                        if (!session.mission.diagnostic.empty()) ImGui::TextWrapped("%s", session.mission.diagnostic.c_str());
                        for (const auto& diagnostic : session.program.threadErrors())
                            ImGui::TextWrapped("%s", diagnostic.c_str());
                        ImGui::Text("Fade %.3f | %s | %.3f seconds", session.environment.fadeAlpha,
                            session.environment.cutscene.name.c_str(), session.environment.cutscene.elapsedMilliseconds / 1000.0);
                        ImGui::Text("Player %.2f, %.2f, %.2f", session.playerPosition.x,
                            session.playerPosition.y, session.playerPosition.z);
                    }
                    if (session.loaded() && ImGui::CollapsingHeader("Mission selector")) {
                        static ImGuiTextFilter missionFilter;
                        static bool showInternalScripts=false;
                        static int selectedMission=-1;
                        missionFilter.Draw("Find mission");
                        ImGui::Checkbox("Show internal scripts",&showInternalScripts);
                        const auto& missions=session.script.missions();
                        ImGui::BeginChild("##NativeMissionList",ImVec2(0,190),ImGuiChildFlags_Borders);
                        for(size_t index=0;index<missions.size();++index) {
                            const auto& mission=missions[index];
                            if(!showInternalScripts&&mission.category!="Mission"&&mission.category!="Activity")continue;
                            const std::string search=mission.name+" "+mission.scriptName+" "+std::to_string(mission.id);
                            if(!missionFilter.PassFilter(search.c_str()))continue;
                            const std::string label=mission.name+"##MissionSlot"+std::to_string(mission.id);
                            if(ImGui::Selectable(label.c_str(),selectedMission==int(index)))selectedMission=int(index);
                        }
                        ImGui::EndChild();
                        if(selectedMission>=0&&size_t(selectedMission)<missions.size()) {
                            const auto& selected=missions[size_t(selectedMission)];
                            ImGui::TextWrapped("%s",selected.name.c_str());
                            ImGui::Text("Native slot: %d",selected.id);
                            ImGui::TextWrapped("%s | Script: %s",selected.category.c_str(),selected.scriptName.c_str());
                            const bool publicMission=selected.category=="Mission"||selected.category=="Activity";
                            ImGui::BeginDisabled(!session.program.active()||!selected.hasBinaryRange||!publicMission);
                            if(ImGui::Button("Start selected mission")) {
                                audio.stop();error.clear();
                                bool started=false;
                                if(selectedMission==session.environment.introMissionSlot)
                                    started=session.restartIntroMission(error);
                                else {
                                    started=session.program.restartMission(selectedMission,session.mission);
                                    if(!started)error=session.program.status().empty()?"The native mission could not start.":session.program.status();
                                    else {
                                        session.cutscene.discard(session.environment);
                                        auto& world=session.environment;
                                        storylandResetMissionPresentation(world);
                                        world.introMissionProtected=false;world.introMissionRestartQueued=false;
                                        world.control=true;world.cameraControlEnabled=true;
                                        world.camera.active=false;world.camera.cutsceneOwned=false;world.camera.restoreRequested=true;
                                        world.widescreen=false;world.timeFrozen=false;world.playerPositionFrozen=false;
                                        world.playerAttachedVehicle=-1;world.applyPlayerCarBrakes=false;world.playerVisible=true;
                                        world.vcsCharacterObjectParents.erase(0);world.vcsCharacterAttachments.erase(0);
                                        world.playerCollisionEnabled=true;
                                        world.fadeAlpha=0;world.fading=false;
                                    }
                                }
                                if(started) {
                                    session.paused=false;session.runtime.pauseMenu=false;
                                    panelsVisible=debugVisible=false;menu.close();ImGui::SetWindowFocus(nullptr);
                                }
                            }
                            ImGui::EndDisabled();
                            if(!session.program.active())ImGui::TextWrapped("Start Game mode to initialize the original MAIN script before selecting a mission.");
                            else if(!publicMission)ImGui::TextWrapped("Internal scripts are listed for inspection.");
                        }
                    }
                }
                ImGui::End();
            }

            if(!openingPending&&!openingMovies.active()&&
                !openingSkipInput.blocksMenu())menu.draw(menuServices());

            if (modding.visible()) {
                StorylandModdingServices moddingServices;
                moddingServices.previewMissionVoice = [&](const StorylandPreparedMissionVoice& prepared) {
                    return missionVoicePreview.request(prepared);
                };
                moddingServices.missionVoiceBackendName = missionVoicePreview.backendName();
                moddingServices.missionVoicePlaybackStatus = missionVoicePreview.status();
                moddingServices.missionVoiceBusy = missionVoicePreview.busy();
                moddingServices.missionVoiceFailed = missionVoicePreview.failed();
                moddingServices.stopMissionVoice = [&] { missionVoicePreview.stop(); };
                moddingServices.scmPath = session.script.sourcePath();
                moddingServices.gameDirectory = session.gameRoot();
                moddingServices.userDirectory = storylandGameUserDirectory(documentsDirectory, true);
                moddingServices.gameLoaded = session.loaded();
                moddingServices.vcs = session.loaded() && session.vcs();
                moddingServices.playerAvailable = session.active();
                moddingServices.playerPlacement = {session.playerPosition.x, session.playerPosition.y,
                    session.playerPosition.z, session.playerHeading - 90.0f};
                moddingServices.canStartDyom = session.canStartDyom(&moddingServices.playRestriction);
                moddingServices.dyomActive = session.dyomActive();
                moddingServices.dyomFinished = session.dyomActive() && session.mission.state == StorylandMissionState::Finished;
                moddingServices.dyomFailed = session.dyomActive() &&
                    (session.mission.missionOutcome == StorylandMissionOutcome::Failed ||
                     session.mission.state == StorylandMissionState::Fault || session.mission.state == StorylandMissionState::Unsupported);
                moddingServices.dyomPc = session.mission.pc;
                if (session.dyomActive()) {
                    const auto& compiled = session.dyomCompiled();
                    moddingServices.dyomObjective = compiled.objectiveAt(session.mission.pc);
                    moddingServices.dyomObjectiveCount = compiled.objectives.size();
                    if (moddingServices.dyomObjective >= 0 &&
                        size_t(moddingServices.dyomObjective) < compiled.objectives.size()) {
                        const auto& objective = compiled.objectives[size_t(moddingServices.dyomObjective)];
                        const auto text = compiled.text.find(objective.textKey);
                        if (text != compiled.text.end()) moddingServices.dyomObjectiveText = text->second;
                    }
                }
                moddingServices.dyomStatus = session.dyomStatus();
                moddingServices.playDyom = [&](const StorylandDyomProject& project, std::string& failure) {
                    missionVoicePreview.stop();
                    audio.stop();
                    beginLoading(session.gameRoot(),session.vcs());
                    if (!session.startDyom(project, failure)) {cancelLoading();return false;}
                    refreshLoadingChapter();
                    loadingPresentation.ready();
                    menu.close();
                    panelsVisible = debugVisible = false;
                    return true;
                };
                moddingServices.stopDyom = [&]() { audio.stop(); session.stopDyom(); };
                moddingServices.modelName = [&](int32_t model) {
                    if (!session.loaded() || model < 0) return std::string{};
                    size_t index = 0;
                    std::string name;
                    if (session.archive.findModelDirEntryByModelId(uint32_t(model), index, name)) return name;
                    return storylandNativeModelAvailable(session.archive, model) ? std::string("Native world model") :
                        std::string("Unavailable in loaded game");
                };
                modding.draw(moddingServices);
            }
            if (quitRequested) {
                if (!ImGui::IsPopupOpen("Unsaved modding documents"))
                    ImGui::OpenPopup("Unsaved modding documents");
                if (ImGui::BeginPopupModal("Unsaved modding documents", nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextUnformatted("The SCM workbench or DYOM project has unsaved edits.");
                    ImGui::TextUnformatted("Keep Storyland open to save them, or discard them and quit.");
                    if (ImGui::Button("Keep Storyland open")) {
                        quitRequested = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Discard and quit")) {
                        quitRequested = false;
                        quit = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }
            if (session.loaded()) dtzEditor.draw(session);
            gallery.draw(loadGalleryImage);
            coopPanel.draw(session);
            coopChat.draw(session.coopNetwork());
            ImGui::EndDisabled();
            refreshLoadingChapter();
            const bool wasLoading=loadingPresentation.active();
            const bool loadingFrame=loadingPresentation.beginFrame();
            if(wasLoading&&!loadingFrame&&!openingPending)audio.setEnabled(loadingAudioEnabled);
            if(openingPending&&!loadingFrame) {
                openingPending=false;
                openingSkipInput.reset();
                openingSkipInput.consume();
                audio.stop();
                audio.setEnabled(false);
                audio.releaseOutput();
                openingMovies.begin(std::move(pendingOpeningMovies));
                pendingOpeningMovies.clear();
                if(!openingMovies.active())finishOpeningMovies();
            }

            StorylandPlayTestInput playerInput;
            StorylandPlayTestCameraInput cameraInput;
            bool aimHeld = false;
            bool fireHeld = false;
            bool captureAimMouse = false;
            const bool mouseWasCaptured=SDL_GetRelativeMouseMode()==SDL_TRUE;
            std::array<int16_t,20> missionPad{};
            const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
            const bool inputEnabled = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) &&
                !io.WantTextInput && !io.WantCaptureKeyboard && !coopChat.blocksGameInput() &&
                !session.paused&&!loadingFrame&&!openingPending&&
                !openingMovies.active()&&!openingSkipInput.blocksMenu();
            const bool gamepadGameplayEnabled=inputEnabled&&!menu.open()&&
                !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId);
            if(weaponSwitch.update(keyboard[SDL_SCANCODE_Q]!=0,gamepadGameplayEnabled&&session.active()))
                storylandCyclePlayerWeapon(session.runtime);
            const Uint32 movieWindowFlags=SDL_GetWindowFlags(window);
            const bool movieFocused=(movieWindowFlags&SDL_WINDOW_INPUT_FOCUS)!=0&&
                (movieWindowFlags&SDL_WINDOW_MINIMIZED)==0;
            const bool movieInput=(openingMovies.active()||openingSkipInput.blocksMenu())&&movieFocused;
            gamepad.update(gamepadGameplayEnabled||movieInput,false,false);
            const bool openingSkipHeld=keyboard[SDL_SCANCODE_RETURN]||keyboard[SDL_SCANCODE_SPACE]||
                keyboard[SDL_SCANCODE_ESCAPE]||gamepad.state.down(12)||gamepad.state.down(16)||
                gamepad.state.down(17);
            const bool openingSkipPressed=movieFocused&&openingSkipInput.controllerPressed(openingSkipHeld);
            if(!movieFocused)openingSkipInput.consume();
            if(openingMovies.active()) {
                if(auto* movie=openingMovies.current())movie->pause(!movieFocused);
                if(movieFocused) {
                    if(openingSkipPressed)openingMovies.skip();
                    if(openingMovies.active())openingMovies.tick(loadingAudioEnabled?1.0f:0.0f);
                }
                if(openingDiagnostic!=openingMovies.status()) {
                    openingDiagnostic=openingMovies.status();
                    if(!openingDiagnostic.empty())std::cerr<<"Opening movies: "<<openingDiagnostic<<'\n';
                }
                if(!openingMovies.active())finishOpeningMovies();
            }
            session.runtime.extendedMovement.crouchPressed=false;
            if (inputEnabled) {
                const bool driving=session.runtime.occupiedVehicle>=0||session.environment.remoteVehicle>=0;
                const auto movementKeys=storylandPcMovementKeys(driving,
                    storylandControlledVehicleIsMotorcycle(session.runtime),
                    keyboard[SDL_SCANCODE_W],keyboard[SDL_SCANCODE_S],
                    keyboard[SDL_SCANCODE_UP],keyboard[SDL_SCANCODE_DOWN],
                    keyboard[SDL_SCANCODE_KP_9],keyboard[SDL_SCANCODE_KP_6]);
                session.runtime.extendedMovement.crouchPressed=!driving&&keyboard[SDL_SCANCODE_C];
                playerInput.forward = movementKeys.forward;
                playerInput.backward = movementKeys.backward;
                playerInput.left = keyboard[SDL_SCANCODE_A]||(driving&&keyboard[SDL_SCANCODE_LEFT]);
                playerInput.right = keyboard[SDL_SCANCODE_D]||(driving&&keyboard[SDL_SCANCODE_RIGHT]);
                playerInput.running = !keyboard[SDL_SCANCODE_LALT];
                playerInput.sprinting = keyboard[SDL_SCANCODE_LSHIFT] && playerInput.running;
                playerInput.handbrake = keyboard[SDL_SCANCODE_SPACE];
                playerInput.bikeLean=movementKeys.bikeLean;
                cameraInput.up = keyboard[SDL_SCANCODE_I];
                cameraInput.down = keyboard[SDL_SCANCODE_K];
                cameraInput.left = keyboard[SDL_SCANCODE_J];
                cameraInput.right = keyboard[SDL_SCANCODE_L];
                int mouseX = 0, mouseY = 0;
                const Uint32 mouse = SDL_GetMouseState(&mouseX, &mouseY);
                const auto inputView = storylandFitView(int(io.DisplaySize.x),int(io.DisplaySize.y),renderer.aspectRatio);
                const bool mouseInView = mouseWasCaptured||inputView.contains(float(mouseX),float(mouseY));
                const bool gameMouse=mouseInView&&(!io.WantCaptureMouse||mouseWasCaptured);
                aimHeld = !driving&&keyboard[SDL_SCANCODE_E];
                fireHeld = keyboard[SDL_SCANCODE_G] ||
                    (gameMouse && (mouse & SDL_BUTTON(SDL_BUTTON_LEFT)));
#if !defined(__ANDROID__)
                cameraInput.freeAim=!driving&&(aimHeld||(gameMouse&&(mouse&SDL_BUTTON(SDL_BUTTON_RIGHT))));
                captureAimMouse=cameraInput.freeAim&&session.active()&&session.environment.control&&
                    session.environment.cameraControlEnabled&&!session.environment.camera.active&&
                    !session.environment.cutscene.playing&&session.runtime.health>0;
                if(captureAimMouse) {
                    int relativeX=0,relativeY=0;
                    if(!mouseWasCaptured) {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        SDL_GetRelativeMouseState(&relativeX,&relativeY);
                    } else {
                        SDL_GetRelativeMouseState(&relativeX,&relativeY);
                        cameraInput.yawDelta=-float(relativeX)*.16f;
                        cameraInput.pitchDelta=float(relativeY)*.16f;
                    }
                }
#endif
                const auto axis=[](bool negative,bool positive) {
                    return int16_t(128*(int(positive)-int(negative)));
                };
                missionPad[0]=axis(keyboard[SDL_SCANCODE_A]||(driving&&keyboard[SDL_SCANCODE_LEFT]),
                    keyboard[SDL_SCANCODE_D]||(driving&&keyboard[SDL_SCANCODE_RIGHT]));
                missionPad[1]=movementKeys.nativeVertical;
                missionPad[2]=axis(cameraInput.left,cameraInput.right);
                missionPad[3]=axis(cameraInput.up,cameraInput.down);
                const auto shoulders=storylandPcShoulderButtons(driving,keyboard[SDL_SCANCODE_Q],
                    keyboard[SDL_SCANCODE_E],keyboard[SDL_SCANCODE_R],keyboard[SDL_SCANCODE_Z],
                    keyboard[SDL_SCANCODE_C],keyboard[SDL_SCANCODE_SPACE],aimHeld||cameraInput.freeAim,
                    keyboard[SDL_SCANCODE_B]);
                const auto button=[&](size_t index,bool down) {missionPad[index]=down?255:0;};
                button(4,shoulders.l1);button(5,shoulders.l2);
                button(6,shoulders.r1);button(7,shoulders.r2);
                button(8,keyboard[SDL_SCANCODE_UP]);button(9,keyboard[SDL_SCANCODE_DOWN]);
                button(10,keyboard[SDL_SCANCODE_LEFT]);button(11,keyboard[SDL_SCANCODE_RIGHT]);
                button(12,keyboard[SDL_SCANCODE_ESCAPE]);button(13,keyboard[SDL_SCANCODE_V]);
                button(14,driving?movementKeys.backward:keyboard[SDL_SCANCODE_SPACE]);
                button(15,keyboard[SDL_SCANCODE_F]);
                button(16,keyboard[SDL_SCANCODE_RETURN]||
                    (driving?movementKeys.forward:keyboard[SDL_SCANCODE_LSHIFT]));
                button(17,fireHeld);button(18,keyboard[SDL_SCANCODE_H]);
                button(19,keyboard[SDL_SCANCODE_LCTRL]||keyboard[SDL_SCANCODE_RCTRL]);
                storylandMergeLinuxGamepad(gamepad.state,driving,playerInput,missionPad);
                const auto gamepadPressed=[&](size_t button) {
                    return gamepad.state.down(button) && previousGamepad[button]==0;
                };
                if (!driving) {
                    jumpPressed = jumpPressed || gamepadPressed(14);
                    aimHeld = aimHeld || gamepad.state.down(6);
                } else if (gamepadPressed(4)) audio.cycleRadio(session,1);
                interactPressed = interactPressed || gamepadPressed(15);
                fireHeld = fireHeld || gamepad.state.down(17);
                cameraInput.yawDelta += -float(gamepad.state.pad[2])/128.0f*100.0f*seconds;
                cameraInput.pitchDelta += float(gamepad.state.pad[3])/128.0f*70.0f*seconds;
            }
            const bool centreCamera=inputEnabled&&session.active()&&session.environment.cameraControlEnabled&&
                session.runtime.occupiedVehicle<0&&session.environment.remoteVehicle<0&&
                !aimHeld&&!cameraInput.freeAim&&(keyboard[SDL_SCANCODE_B]||gamepad.state.down(4));
            cameraInput.refocus=storylandRetailCameraCentrePressed(centreCamera,cameraCentreHeld);
            previousGamepad=gamepad.state.pad;
            if(!captureAimMouse&&mouseWasCaptured)SDL_SetRelativeMouseMode(SDL_FALSE);
            const auto key=[&](SDL_Scancode code) {return inputEnabled&&keyboard[code]!=0;};
            if(session.vcs())session.environment.creditsFastForward=key(SDL_SCANCODE_TAB);
            audio.setVehicleHornHeld(sirenControls.update(session.runtime,
                keyboard[SDL_SCANCODE_H]!=0||gamepad.state.down(18),
                gamepadGameplayEnabled&&session.active(),seconds));
            if(!loadingFrame&&!openingPending&&!openingMovies.active()) {
            audio.synchronize(session);
            audio.advanceSilently(seconds);
            session.camera.aspectRatio = float(storylandFitView(int(io.DisplaySize.x),int(io.DisplaySize.y),renderer.aspectRatio).aspect());
            const double coopTime = std::chrono::duration<double>(now - startedAt).count();
            session.updateCoop(coopTime);
            if(session.environment.invertedMap) storylandInvertPlayControls(playerInput,cameraInput);
            if(simulationMode!=options.frameMode) {
                simulationMode=options.frameMode;
                simulationCadence.reset();
            }
            if(!inputEnabled) {
                pendingJump=pendingInteract=pendingReload=pendingFire=pendingAim=false;
                pendingCameraCentre=false;
                pendingYaw=pendingPitch=0;
                pendingButtons.fill(0);
            } else {
                pendingJump|=jumpPressed;pendingInteract|=interactPressed;pendingReload|=reloadPressed;
                pendingFire|=fireHeld;pendingAim|=aimHeld;
                pendingCameraCentre|=cameraInput.refocus;
                pendingYaw+=cameraInput.yawDelta;pendingPitch+=cameraInput.pitchDelta;
                for(size_t button=4;button<pendingButtons.size();++button)
                    pendingButtons[button]=std::max(pendingButtons[button],missionPad[button]);
            }
            const bool advancing=session.active()&&!session.paused&&!session.runtime.pauseMenu;
            const float simulationSeconds=advancing?
                float(simulationCadence.advance(seconds,options.frameMode)):seconds;
            if(!advancing)simulationCadence.reset();
            if(simulationSeconds>0) {
                playerInput.pcControls=gamepad.mode==StorylandControllerMode::Current;
                playerInput.grab=key(SDL_SCANCODE_LCTRL)||key(SDL_SCANCODE_RCTRL);
                playerInput.scopeToggle=key(SDL_SCANCODE_N);
                playerInput.cancelAim=key(SDL_SCANCODE_X);
                session.runtime.playerDiveDown=key(SDL_SCANCODE_LCTRL)||key(SDL_SCANCODE_RCTRL);
                session.runtime.playerDiveUp=key(SDL_SCANCODE_SPACE);
                cameraInput.yawDelta=pendingYaw;cameraInput.pitchDelta=pendingPitch;
                cameraInput.refocus=pendingCameraCentre;
                for(size_t button=4;button<pendingButtons.size();++button)
                    missionPad[button]=std::max(missionPad[button],pendingButtons[button]);
                session.tick(simulationSeconds,playerInput,cameraInput,pendingJump,pendingInteract,
                    pendingAim,pendingFire,pendingReload,&missionPad);
                pendingJump=pendingInteract=pendingReload=pendingFire=pendingAim=false;
                pendingCameraCentre=false;
                pendingYaw=pendingPitch=0;
                pendingButtons.fill(0);
            }
            if(session.loaded()&&!session.active())
                storylandAdvancePreviewClock(session.runtime,session.environment,seconds);
            session.publishCoop(coopTime);
            if(simulationSeconds>0||!advancing)audio.serviceNativeVcsAudio(session);
            audio.synchronize(session);
            if (options.audio && !audio.missionError().empty()) session.paused = true;
            } else {
                simulationCadence.reset();
                pendingJump=pendingInteract=pendingReload=pendingFire=pendingAim=false;
                pendingCameraCentre=false;
                pendingYaw=pendingPitch=0;
                pendingButtons.fill(0);
            }
            if (session.status() != lastStatus) {
                lastStatus = session.status();
                std::cout << "Storyland: " << lastStatus << '\n';
            }
            const bool scriptFault = session.mission.state == StorylandMissionState::Fault ||
                session.mission.state == StorylandMissionState::Unsupported ||
                !session.program.threadErrors().empty() || (options.audio && !audio.missionError().empty());
            int width = 0;
            int height = 0;
            SDL_GL_GetDrawableSize(window, &width, &height);
            if (width > 0 && height > 0) {
                const bool movieFrame=openingPending||openingMovies.active();
                if(!session.loaded()||loadingFrame||movieFrame) {
                    glViewport(0,0,width,height);glDisable(GL_SCISSOR_TEST);
                    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
                    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                }
                if(!loadingFrame&&!movieFrame)renderer.draw(session, width, height);
                if(loadingFrame||movieFrame)galleryCaptureRequested=false;
                else if(!session.active()){lastGalleryPhoto=0;galleryCaptureRequested=false;}
                else if(session.runtime.photographSerial!=lastGalleryPhoto||galleryCaptureRequested) {
                    lastGalleryPhoto=session.runtime.photographSerial;
                    const bool requested=galleryCaptureRequested;galleryCaptureRequested=false;
                    if(lastGalleryPhoto||requested) {
                        gallery.setDirectory(storylandGameUserDirectory(linuxDocumentsDirectory(),session.vcs()));
                        const auto photo=gallery.nextPath(error);
                        if(photo.empty()||!captureFrame(photo,width,height,error)) {
                            if(!photo.empty()){std::error_code ignored;std::filesystem::remove(photo,ignored);}
                            std::cerr<<"Gallery: "<<error<<'\n';
                        }
                    }
                }
                if (!menu.open()&&!loadingFrame&&!movieFrame) {
                    StorylandImGuiViewport viewport(storylandFitView(int(io.DisplaySize.x),int(io.DisplaySize.y),renderer.aspectRatio));
                    drawPresentation(session, scriptFault);
                }
                if (scriptFault&&!loadingFrame&&!movieFrame) drawScriptFailure(session, audio.missionError());
                if(!loadingFrame&&!movieFrame)scriptLogPopup.draw(session.mission,session.program,session.environment,
                    storylandGameUserDirectory(linuxDocumentsDirectory(),session.vcs()));
                if(loadingFrame) {
                    loadingPresentation.draw();
                }
                if(movieFrame&&!loadingFrame) {
                    auto* draw=ImGui::GetForegroundDrawList();
                    draw->AddRectFilled(ImVec2(0,0),io.DisplaySize,IM_COL32_BLACK);
                    const auto* movie=openingMovies.current();
                    if(movie&&movie->texture()) {
                        const auto view=storylandFitView(int(io.DisplaySize.x),int(io.DisplaySize.y),
                            renderer.aspectRatio);
                        const auto presentation=storylandOpeningMoviePresentation(float(view.width),
                            float(view.height),openingMovies.currentDisplayAspect(movie->displayAspect()));
                        const auto& background=presentation.background;
                        const auto& foreground=presentation.foreground;
                        if(presentation.margins)draw->AddImage(ImTextureID(movie->texture()),
                            ImVec2(float(view.x),float(view.y)),
                            ImVec2(float(view.x+view.width),float(view.y+view.height)),
                            ImVec2(background.left,background.top),ImVec2(background.right,background.bottom),
                            IM_COL32(56,56,56,255));
                        draw->AddImage(ImTextureID(movie->texture()),
                            ImVec2(float(view.x)+foreground.left*view.width,
                                float(view.y)+foreground.top*view.height),
                            ImVec2(float(view.x)+foreground.right*view.width,
                                float(view.y)+foreground.bottom*view.height));
                    }
                }
                ImGui::Render();
                ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
                if(!loadingFrame)++renderedFrames;
                if (!loadingFrame&&trace.is_open()) traceFrame(trace, session, audio, renderer, renderedFrames, elapsed);
                const double gameSeconds = session.environment.gameMilliseconds / 1000.0;
                for (auto& capture : options.captures) {
                    if (loadingFrame||capture.saved || gameSeconds < capture.seconds) continue;
                    capture.saved = true;
                    if (!captureFrame(capture.path, width, height, error)) {
                        std::cerr << "Timed capture failed: " << error << '\n';
                        exitCode = 1;
                    } else std::cout << "Captured game second " << gameSeconds << ": " << capture.path << '\n';
                }
                const bool lastFrame = quit || (!loadingFrame&&((options.frames && renderedFrames >= options.frames) ||
                    (options.duration > 0 && gameSeconds >= options.duration) ||
                    (options.quitOnFault && scriptFault)));
                if (lastFrame && !options.capture.empty() && !captureFrame(options.capture, width, height, error)) {
                    std::cerr << "Capture failed: " << error << '\n';
                    exitCode = 1;
                }
                const int display=SDL_GetWindowDisplayIndex(window);
                const auto mode=session.active()?options.frameMode:StorylandPlayFrameMode::Fps60;
                if(presentationDirty||display!=presentationDisplay||mode!=presentationMode) {
                    presentationDirty=false;presentationDisplay=display;presentationMode=mode;
                    SDL_DisplayMode displayMode{};
                    const int refresh=SDL_GetCurrentDisplayMode(display,&displayMode)==0?displayMode.refresh_rate:0;
                    const int interval=options.frames?0:storylandPresentationSwapInterval(mode,refresh);
                    const int requested=options.frames?0:(interval>0?interval:1);
                    const bool accepted=SDL_GL_SetSwapInterval(requested)==0;
                    presentationSynchronized=interval>0&&accepted&&SDL_GL_GetSwapInterval()==interval;
                    if(!accepted&&!options.frames)SDL_GL_SetSwapInterval(1);
                    nextPresentation=std::chrono::duration<double>(std::chrono::steady_clock::now()-startedAt).count();
                }
                SDL_GL_SwapWindow(window);
                if (lastFrame) quit = true;
                if (!loadingFrame&&scriptFault && options.quitOnFault) exitCode = 1;
                if(loadingFrame&&options.frames)SDL_Delay(8);
                if(!quit&&!options.frames) {
                    const double completed=std::chrono::duration<double>(std::chrono::steady_clock::now()-startedAt).count();
                    nextPresentation=storylandNextPresentationTime(nextPresentation,completed,
                        storylandPresentationFrameSeconds(mode),presentationSynchronized);
                    if(!presentationSynchronized)
                        std::this_thread::sleep_until(startedAt+std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                            std::chrono::duration<double>(nextPresentation)));
                }
            } else {
                ImGui::EndFrame();
                SDL_Delay(20);
            }
            if(!pendingGamePath.empty()&&!quit) {
                const auto path=std::move(pendingGamePath);
                pendingGamePath.clear();
                if(!openGame(path))std::cerr<<error<<'\n';
                previousTime=std::chrono::steady_clock::now();
            }
        }
        const double wallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - startedAt).count();
        if (trace.is_open()) {
            trace.flush();
            if (!trace) { std::cerr << "Could not write the complete frame trace.\n"; exitCode = 1; }
            trace.close();
        }
        if (!options.report.empty() && !writeReport(options.report, session, audio, renderedFrames, wallSeconds, rendererName, error)) {
            std::cerr << "Report failed: " << error << '\n';
            exitCode = 1;
        }
        openingMovies.stop();
        pendingOpeningMovies.clear();
        loadingPresentation.cancel();
        missionVoicePreview.shutdown();
        audio.stop();
        gallery.releaseTexture();
        renderer.shutdown();
    }
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
