#include "src/platform/linux/storyland_linux_ctw_viewer.h"

#include <SDL.h>
#include <GL/gl.h>
#include <png.h>
#include "imgui.h"
#include "imgui_impl_opengl2.h"
#include "imgui_impl_sdl2.h"
#include "src/graphics/storyland_ctw_city_view.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace storyland_ctw;

struct CityTextureObject {
    GLuint texture = 0;
    bool translucent = false;
};

bool sameOutputPath(const std::filesystem::path& left, const std::filesystem::path& right) {
    if (left.empty() || right.empty()) return false;
    if (isArchiveSource(left, right)) return true;
    std::error_code leftError, rightError;
    const auto absoluteLeft = std::filesystem::absolute(left, leftError);
    const auto absoluteRight = std::filesystem::absolute(right, rightError);
    if (leftError || rightError) return false;
    const auto normalizedLeft = std::filesystem::weakly_canonical(absoluteLeft, leftError);
    const auto normalizedRight = std::filesystem::weakly_canonical(absoluteRight, rightError);
    return !leftError && !rightError && normalizedLeft == normalizedRight;
}

void protectArchive(const std::filesystem::path& archive, const std::filesystem::path& output) {
    if (!output.empty() && (isArchiveSource(archive, output) || sameOutputPath(archive, output)))
        throw std::runtime_error("Cannot write a CTW capture or report over the opened GAME.PAK archive.");
}

class CityDocument {
public:
    PakArchive archive;
    CityScene scene;
    CityViewCamera camera;
    CityDrawStats lastDraw;
    std::map<uint32_t, CityTextureObject> textures;
    int focusedBlock = -1;
    bool showTextures = true;
    std::string error;

    void releaseTextures() {
        for (const auto& entry : textures)
            if (entry.second.texture) glDeleteTextures(1, &entry.second.texture);
        textures.clear();
    }

    bool load(const std::filesystem::path& path) {
        PakArchive nextArchive;
        CityScene nextScene;
        if (!nextArchive.loadFromFile(path, error) ||
            !loadPspCity(nextArchive, nextScene, error)) return false;
        releaseTextures();
        archive = std::move(nextArchive);
        scene = std::move(nextScene);
        for (const auto& image : scene.textures) {
            CityTextureObject object;
            for (size_t alpha = 3; alpha < image.rgba.size(); alpha += 4)
                if (image.rgba[alpha] >= 26 && image.rgba[alpha] < 255) {
                    object.translucent = true;
                    break;
                }
            glGenTextures(1, &object.texture);
            glBindTexture(GL_TEXTURE_2D, object.texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(image.width), GLsizei(image.height),
                0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
            textures.emplace(image.resourceId, object);
        }
        fitCity();
        return true;
    }

    void fitCity() {
        focusedBlock = -1;
        camera.fit(scene.bounds, scene.bounds);
    }

    void focus(size_t index) {
        if (index >= scene.blocks.size()) return;
        focusedBlock = int(index);
        camera.fit(scene.blocks[index].bounds, scene.bounds);
    }

    void render(int left, int width, int height) {
        glViewport(left, 0, width, height);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);
        glDisable(GL_FOG);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GEQUAL, 0.1f);
        glDepthMask(GL_TRUE);
        glClearColor(0.055f, 0.09f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const auto view = makeCityView(camera, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMultMatrixf(view.projection.data());
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glMultMatrixf(view.modelView.data());
        lastDraw = drawCityScene(scene, view, [&](const PakModelPart& part, CityDrawPass pass) {
            if (part.alpha == 0) return false;
            const auto found = textures.find(part.textureId);
            const bool textured = showTextures && found != textures.end();
            const bool translucent = part.alpha < 255 || (textured && found->second.translucent);
            if (translucent != (pass == CityDrawPass::Transparent)) return false;
            const float alpha = part.alpha / 255.0f;
            if (textured) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, found->second.texture);
                glColor4f(1, 1, 1, alpha);
            } else {
                glDisable(GL_TEXTURE_2D);
                glColor4f(0.72f, 0.76f, 0.82f, alpha);
            }
            return true;
        });
    }
};

void writeCapture(const std::filesystem::path& path, int width, int height,
    const std::filesystem::path& archive) {
    protectArchive(archive, path);
    std::vector<uint8_t> pixels(size_t(width) * size_t(height) * 4u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Cannot read the CTW city frame.");
    const size_t stride = size_t(width) * 4u;
    for (int y = 0; y < height / 2; ++y)
        std::swap_ranges(pixels.begin() + size_t(y) * stride,
            pixels.begin() + size_t(y + 1) * stride,
            pixels.begin() + size_t(height - 1 - y) * stride);
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = png_uint_32(width);
    image.height = png_uint_32(height);
    image.format = PNG_FORMAT_RGBA;
    if (!png_image_write_to_file(&image, path.string().c_str(), 0, pixels.data(), 0, nullptr)) {
        const std::string message = image.message;
        png_image_free(&image);
        throw std::runtime_error(message);
    }
    png_image_free(&image);
}

void writeReport(const std::filesystem::path& path, const CityDocument& document,
    uint64_t frames, double seconds) {
    if (path.empty()) return;
    protectArchive(document.archive.sourcePath(), path);
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    if (!file) throw std::runtime_error("Cannot create the CTW city report.");
    const auto& scene = document.scene;
    const auto& diagnostics = scene.diagnostics;
    file << "{\n  \"mode\": \"Chinatown Wars PSP city\",\n"
        << "  \"resources\": " << document.archive.resources().size() << ",\n"
        << "  \"blocks\": " << scene.blocks.size() << ",\n"
        << "  \"models\": " << scene.models.size() << ",\n"
        << "  \"instances\": " << scene.instances.size() << ",\n"
        << "  \"textures\": " << scene.textures.size() << ",\n"
        << "  \"source_placements\": " << diagnostics.sourcePlacements << ",\n"
        << "  \"world_sections\": " << diagnostics.worldSections << ",\n"
        << "  \"boundary_sections\": " << diagnostics.boundarySections << ",\n"
        << "  \"duplicate_placements\": " << diagnostics.duplicatePlacements << ",\n"
        << "  \"embedded_resource_references\": " << diagnostics.embeddedResources << ",\n"
        << "  \"missing_models\": " << diagnostics.missingModels << ",\n"
        << "  \"missing_textures\": " << diagnostics.missingTextures << ",\n"
        << "  \"warnings\": " << diagnostics.warnings.size() << ",\n"
        << "  \"bounds_min\": [" << scene.bounds.minimum[0] << ","
        << scene.bounds.minimum[1] << "," << scene.bounds.minimum[2] << "],\n"
        << "  \"bounds_max\": [" << scene.bounds.maximum[0] << ","
        << scene.bounds.maximum[1] << "," << scene.bounds.maximum[2] << "],\n"
        << "  \"visible_blocks\": " << document.lastDraw.blocks << ",\n"
        << "  \"visible_instances\": " << document.lastDraw.instances << ",\n"
        << "  \"draw_calls\": " << document.lastDraw.drawCalls << ",\n"
        << "  \"vertices_drawn\": " << document.lastDraw.vertices << ",\n"
        << "  \"rendered_frames\": " << frames << ",\n"
        << "  \"elapsed_seconds\": " << seconds << "\n}\n";
    file.close();
    if (!file) throw std::runtime_error("Cannot finish the CTW city report.");
}

void drawCityControls(CityDocument& document, std::array<char, 4096>& archivePath,
    float panelWidth, float height) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, height));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("Chinatown Wars", nullptr, flags);
    ImGui::TextUnformatted("Liberty City from GAME.PAK");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##ArchivePath", archivePath.data(), archivePath.size());
    if (ImGui::Button("Open GAME.PAK", ImVec2(-1, 0)))
        document.load(std::filesystem::u8path(archivePath.data()));
    if (!document.error.empty()) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.45f, 1), "Could not open city");
        ImGui::TextWrapped("%s", document.error.c_str());
    }
    ImGui::Separator();
    if (ImGui::Button("Fit city")) document.fitCity();
    ImGui::SameLine();
    if (ImGui::Button("Top view")) {
        document.camera.yaw = 0;
        document.camera.pitch = 90;
    }
    if (ImGui::Button("Angled view")) {
        document.camera.yaw = 35;
        document.camera.pitch = 55;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Textures", &document.showTextures);
    ImGui::TextWrapped("Drag: orbit | Right/middle drag: pan\nWheel: zoom | F: fit | T: top | Home: panels");
    const auto& scene = document.scene;
    ImGui::Separator();
    ImGui::Text("%zu blocks | %zu instances", scene.blocks.size(), scene.instances.size());
    ImGui::Text("%zu models | %zu textures", scene.models.size(), scene.textures.size());
    ImGui::Text("%zu visible | %zu draws", document.lastDraw.instances, document.lastDraw.drawCalls);
    if (!scene.diagnostics.warnings.empty() && ImGui::CollapsingHeader("Archive diagnostics"))
        for (const auto& warning : scene.diagnostics.warnings)
            ImGui::TextWrapped("%s", warning.c_str());
    ImGui::SeparatorText("Placed world blocks");
    if (ImGui::Selectable("Complete Liberty City", document.focusedBlock < 0))
        document.fitCity();
    ImGui::BeginChild("WorldBlocks", ImVec2(0, 0), ImGuiChildFlags_Borders);
    ImGuiListClipper clipper;
    clipper.Begin(int(scene.blocks.size()));
    while (clipper.Step()) {
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            const auto& block = scene.blocks[size_t(index)];
            const std::string label = "#" + std::to_string(block.resourceId) + "  (" +
                std::to_string(block.instanceCount) + " instances)##" + std::to_string(index);
            if (ImGui::Selectable(label.c_str(), document.focusedBlock == index))
                document.focus(size_t(index));
        }
    }
    ImGui::EndChild();
    ImGui::End();
}
}

int storylandRunLinuxCtwViewer(const StorylandLinuxCtwOptions& options) {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    bool imguiContext = false, imguiPlatform = false, imguiRenderer = false;
    CityDocument document;
    const auto cleanup = [&]() {
        if (context) document.releaseTextures();
        if (imguiRenderer) ImGui_ImplOpenGL2_Shutdown();
        if (imguiPlatform) ImGui_ImplSDL2_Shutdown();
        if (imguiContext) ImGui::DestroyContext();
        if (context) SDL_GL_DeleteContext(context);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    };
    try {
        protectArchive(options.archive, options.capture);
        protectArchive(options.archive, options.report);
        if (sameOutputPath(options.capture, options.report))
            throw std::runtime_error("Choose different output files for the CTW capture and report.");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
            throw std::runtime_error(SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        window = SDL_CreateWindow("Storyland | Chinatown Wars Liberty City",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            options.width, options.height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window) throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window, 640, 360);
        context = SDL_GL_CreateContext(window);
        if (!context || SDL_GL_MakeCurrent(window, context) != 0)
            throw std::runtime_error(SDL_GetError());
        SDL_GL_SetSwapInterval(options.frames ? 0 : 1);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        imguiContext = true;
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().LogFilename = nullptr;
        ImGui::StyleColorsDark();
        if (!ImGui_ImplSDL2_InitForOpenGL(window, context))
            throw std::runtime_error("Cannot initialize the CTW window controls.");
        imguiPlatform = true;
        if (!ImGui_ImplOpenGL2_Init())
            throw std::runtime_error("Cannot initialize the CTW control renderer.");
        imguiRenderer = true;
        if (!document.load(options.archive)) throw std::runtime_error(document.error);
        if (options.focusResource >= 0) {
            const auto found = std::find_if(document.scene.blocks.begin(), document.scene.blocks.end(),
                [&](const CityBlock& block) { return block.resourceId == uint32_t(options.focusResource); });
            if (found == document.scene.blocks.end())
                throw std::runtime_error("The selected resource is not a placed city block.");
            document.focus(size_t(found - document.scene.blocks.begin()));
        }
        if (options.topView) {
            document.camera.yaw = 0;
            document.camera.pitch = 90;
        }
        std::array<char, 4096> archivePath{};
        const auto path = options.archive.u8string();
        std::copy_n(path.begin(), std::min(path.size(), archivePath.size() - 1), archivePath.begin());
        std::cout << "CTW city: " << document.scene.blocks.size() << " blocks, "
            << document.scene.instances.size() << " instances, "
            << document.scene.models.size() << " models\n";

        bool running = true, hideUi = options.hideUi;
        uint64_t frames = 0;
        const auto start = std::chrono::steady_clock::now();
        double elapsed = 0.0;
        while (running) {
            int width = 0, height = 0, windowWidth = 0, windowHeight = 0;
            SDL_GetWindowSize(window, &windowWidth, &windowHeight);
            const float inputPanelWidth = hideUi ? 0.0f : std::min(300.0f, float(windowWidth) * 0.32f);
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT) running = false;
                if (event.type == SDL_KEYDOWN && !ImGui::GetIO().WantTextInput) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                    if (event.key.keysym.sym == SDLK_HOME) hideUi = !hideUi;
                    if (event.key.keysym.sym == SDLK_f) document.fitCity();
                    if (event.key.keysym.sym == SDLK_t) {
                        document.camera.yaw = 0;
                        document.camera.pitch = 90;
                    }
                }
                int mouseX = 0, mouseY = 0;
                SDL_GetMouseState(&mouseX, &mouseY);
                if (mouseX < inputPanelWidth || ImGui::GetIO().WantCaptureMouse) continue;
                if (event.type == SDL_MOUSEWHEEL)
                    document.camera.zoom = std::clamp(document.camera.zoom *
                        std::exp(float(event.wheel.y) * 0.14f), 0.02f, 512.0f);
                if (event.type == SDL_MOUSEMOTION) {
                    if (event.motion.state & (SDL_BUTTON_RMASK | SDL_BUTTON_MMASK))
                        document.camera.panPixels(float(event.motion.xrel), float(event.motion.yrel),
                            std::max(1, windowWidth - int(inputPanelWidth)), windowHeight);
                    else if (event.motion.state & SDL_BUTTON_LMASK) {
                        document.camera.yaw += event.motion.xrel * 0.35f;
                        document.camera.pitch = std::clamp(document.camera.pitch +
                            event.motion.yrel * 0.35f, 5.0f, 90.0f);
                    }
                }
            }
            SDL_GL_GetDrawableSize(window, &width, &height);
            SDL_GetWindowSize(window, &windowWidth, &windowHeight);
            elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            if (width <= 0 || height <= 0) {
                if (!running || (options.duration > 0.0 && elapsed >= options.duration)) {
                    if (!options.capture.empty())
                        throw std::runtime_error("Cannot capture the final CTW frame while the window has no drawable surface.");
                    break;
                }
                SDL_Delay(10);
                continue;
            }
            const float panelWidth = hideUi ? 0.0f : std::min(300.0f, float(windowWidth) * 0.32f);
            const int left = int(panelWidth * width / std::max(1, windowWidth));
            const int viewWidth = std::max(1, width - left);
            ImGui_ImplOpenGL2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
            if (!hideUi) drawCityControls(document, archivePath, panelWidth, float(windowHeight));
            ImGui::Render();
            document.render(left, viewWidth, height);
            glViewport(0, 0, width, height);
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
            ++frames;
            elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const bool finalFrame = !running || (options.frames && frames >= options.frames) ||
                (options.duration > 0.0 && elapsed >= options.duration);
            if (finalFrame && !options.capture.empty())
                writeCapture(options.capture, width, height, document.archive.sourcePath());
            SDL_GL_SwapWindow(window);
            if (finalFrame) break;
        }
        writeReport(options.report, document, frames, elapsed);
        cleanup();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "CTW city viewer: " << exception.what() << '\n';
        cleanup();
        return 1;
    }
}

