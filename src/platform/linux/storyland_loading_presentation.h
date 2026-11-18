#pragma once

#include "src/storyland/storyland_loading_screens.h"
#include "imgui.h"
#include <SDL.h>
#include <cmath>
#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <GL/gl.h>
#endif

class StorylandLoadingPresentation {
public:
    ~StorylandLoadingPresentation() { cancel(); }

    void start(const std::filesystem::path& path, bool vcs, unsigned chapter = 1) {
        const float previousProgress = openingStage && sequence.active() ? progress : 0.0f;
        cancel();
        progress = previousProgress;
        screens = storylandReadLoadingScreens(path, vcs);
        vcsArtwork = vcs;
        currentChapter = chapter >= 1 && chapter <= 3 ? chapter : 1;
        selection = storylandLoadingScreenSelection(screens, vcsArtwork, currentChapter);
        sequence.begin(selection.size());
    }

    void startOpening() {
        cancel();
        openingStage = true;
        sequence.begin(0);
    }

    void setProgress(float value) {
        if (std::isfinite(value)) progress = std::max(progress, std::clamp(value, 0.0f, 1.0f));
    }

    void ready() {
        setProgress(1.0f);
        sequence.ready();
    }

    bool active() const { return sequence.active(); }

    bool beginFrame(uint64_t now = SDL_GetTicks64()) {
        if (!sequence.active()) return false;
        if (sequence.present(now)) return true;
        cancel();
        return false;
    }

    void setChapter(unsigned chapter) {
        chapter = chapter >= 1 && chapter <= 3 ? chapter : 1;
        if (!vcsArtwork || chapter == currentChapter) return;
        const size_t currentScreen = sequence.index() < selection.size() ?
            selection[sequence.index()] : SIZE_MAX;
        currentChapter = chapter;
        selection = storylandLoadingScreenSelection(screens, true, currentChapter);
        const auto retained = std::find(selection.begin(), selection.end(), currentScreen);
        const bool preserveCurrentScreen = retained != selection.end();
        const size_t selectedIndex = preserveCurrentScreen ? size_t(retained - selection.begin()) : 0;
        sequence.reselect(selection.size(), selectedIndex, preserveCurrentScreen);
    }

    void releaseTexture() {
        if (texture) glDeleteTextures(1, &texture);
        texture = 0;
        textureScreen = SIZE_MAX;
    }

    void cancel() {
        sequence.cancel();
        releaseTexture();
        screens.clear();
        selection.clear();
        vcsArtwork = false;
        currentChapter = 1;
        openingStage = false;
        progress = 0.0f;
    }

    void draw(const char* stage = "Loading game data...") {
        if (!sequence.active()) return;
        auto* draw = ImGui::GetForegroundDrawList();
        const auto size = ImGui::GetIO().DisplaySize;
        if (size.x <= 0.0f || size.y <= 0.0f) return;
        draw->AddRectFilled({0, 0}, size, IM_COL32_BLACK);
        if (sequence.index() < selection.size()) {
            const size_t screenIndex = selection[sequence.index()];
            if (textureScreen != screenIndex || !texture) uploadTexture(screenIndex);
        }
        if (texture && textureScreen < screens.size() &&
            std::find(selection.begin(), selection.end(), textureScreen) != selection.end()) {
            const auto& screen = screens[textureScreen];
            const auto& image = screen.image;
            ImVec2 origin{0, 0};
            ImVec2 extent = size;
            if (!screen.fillViewport) {
                const float scale = std::min(size.x / float(image.width), size.y / float(image.height));
                extent = {float(image.width) * scale, float(image.height) * scale};
                origin = {(size.x - extent.x) * .5f, (size.y - extent.y) * .5f};
            }
            draw->AddImage(static_cast<ImTextureID>(texture), origin,
                {origin.x + extent.x, origin.y + extent.y});
        }
        const float fontSize = std::clamp(size.y / 30.0f, 16.0f, 32.0f);
        const float left = std::floor(size.x * .08f);
        const float right = std::floor(size.x * .92f);
        const float barBottom = std::floor(size.y - fontSize);
        const float barHeight = std::clamp(std::floor(size.y / 150.0f), 4.0f, 8.0f);
        const float barTop = barBottom - barHeight;
        const ImVec2 origin{left, std::floor(barTop - fontSize * 1.5f)};
        const char* message = sequence.workFinished() ? "Loading complete" : stage;
        draw->AddRectFilled({0, origin.y - fontSize * .5f}, size, IM_COL32(0, 0, 0, 190));
        draw->AddText(ImGui::GetFont(), fontSize, origin, IM_COL32_WHITE, message);
        draw->AddRectFilled({left, barTop}, {right, barBottom}, IM_COL32(30, 30, 36, 255));
        const float filledRight = std::floor(left + (right - left) * progress);
        const ImU32 accent = vcsArtwork ? IM_COL32(244, 112, 196, 255) : IM_COL32(110, 174, 245, 255);
        if (filledRight > left)
            draw->AddRectFilled({left, barTop}, {filledRight, barBottom}, accent);
    }

private:
    bool uploadTexture(size_t screenIndex) {
        if (screenIndex >= screens.size()) return false;
        const auto& image = screens[screenIndex].image;
        if (image.width <= 0 || image.height <= 0 ||
            image.rgba.size() != size_t(image.width) * size_t(image.height) * 4u) return false;

        using BindBufferFunction = void (*)(GLenum, GLuint);
        const auto bindBuffer = reinterpret_cast<BindBufferFunction>(SDL_GL_GetProcAddress("glBindBuffer"));
        GLint previousTexture = 0, alignment = 0, rowLength = 0, skipPixels = 0, skipRows = 0;
        GLint unpackBuffer = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);
        if (bindBuffer) {
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
            bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        for (unsigned pending = 0; pending < 16 && glGetError() != GL_NO_ERROR; ++pending) {}

        GLuint uploaded = 0;
        glGenTextures(1, &uploaded);
        if (uploaded) {
            glBindTexture(GL_TEXTURE_2D, uploaded);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
        }
        const bool complete = uploaded != 0 && glGetError() == GL_NO_ERROR;
        glBindTexture(GL_TEXTURE_2D, GLuint(previousTexture));
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows);
        if (bindBuffer) bindBuffer(GL_PIXEL_UNPACK_BUFFER, GLuint(unpackBuffer));
        if (!complete) {
            if (uploaded) glDeleteTextures(1, &uploaded);
            return false;
        }
        if (texture) glDeleteTextures(1, &texture);
        texture = uploaded;
        textureScreen = screenIndex;
        return true;
    }

    StorylandLoadingSequence sequence;
    std::vector<StorylandLoadingScreen> screens;
    std::vector<size_t> selection;
    bool vcsArtwork = false;
    bool openingStage = false;
    unsigned currentChapter = 1;
    float progress = 0.0f;
    GLuint texture = 0;
    size_t textureScreen = SIZE_MAX;
};
