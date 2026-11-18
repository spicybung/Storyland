#pragma once
#include <imgui.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

class StorylandGallery {
public:
    using ImageLoader = std::function<bool(const std::filesystem::path&, int&, int&,
        std::vector<uint8_t>&, std::string&)>;

    void open(const std::filesystem::path& userDirectory) {
        directory = userDirectory.empty() ? std::filesystem::path{} : userDirectory / "Gallery";
        visible = true;
        selected = 0;
        refresh();
    }

    void refresh() {
        releaseTexture();
        entries.clear();
        status.clear();
        if (directory.empty()) {
            status = "Could not find the user files folder.";
            return;
        }
        std::error_code error;
        if (!std::filesystem::exists(directory, error)) {
            if (error) status = error.message();
            return;
        }
        std::filesystem::directory_iterator iterator(directory, error), end;
        while (!error && iterator != end) {
            const auto& entry = *iterator;
            if (entry.is_regular_file(error) && entry.path().extension() == ".png")
                entries.push_back(entry.path());
            if (!error) iterator.increment(error);
        }
        if (error) status = error.message();
        std::sort(entries.begin(), entries.end());
        if (!entries.empty()) selected = std::min(selected, entries.size() - 1);
    }

    std::filesystem::path nextPath(std::string& error) const {
        if (directory.empty()) {
            error = "Could not find the user files folder.";
            return {};
        }
        std::error_code filesystemError;
        std::filesystem::create_directories(directory, filesystemError);
        if (filesystemError) {
            error = filesystemError.message();
            return {};
        }
        for (unsigned number = 1; number <= 999999; ++number) {
            char name[24];
            std::snprintf(name, sizeof(name), "Photo%06u.png", number);
            const auto candidate = directory / name;
#ifdef _WIN32
            const int handle=::_wopen(candidate.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_S_IREAD|_S_IWRITE);
#else
            const int handle=::open(candidate.c_str(),O_WRONLY|O_CREAT|O_EXCL,0666);
#endif
            if (handle >= 0) {
#ifdef _WIN32
                ::_close(handle);
#else
                ::close(handle);
#endif
                error.clear();
                return candidate;
            }
            if(errno!=EEXIST) {
                error=std::error_code(errno,std::generic_category()).message();
                return {};
            }
        }
        error = "Gallery is full.";
        return {};
    }

    void setDirectory(const std::filesystem::path& userDirectory) {
        const auto next = userDirectory.empty() ? std::filesystem::path{} : userDirectory / "Gallery";
        if (directory != next) {
            directory = next;
            selected = 0;
            releaseTexture();
            entries.clear();
        }
    }

    void draw(const ImageLoader& load) {
        if (!visible) return;
        ImGui::SetNextWindowSize(ImVec2(720, 560), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Gallery", &visible)) {
            if (ImGui::Button("Refresh")) refresh();
            if (entries.empty()) {
                ImGui::TextUnformatted("No photographs yet.");
            } else {
                ImGui::SameLine();
                if (ImGui::Button("Previous")) {
                    selected = (selected + entries.size() - 1) % entries.size();
                    releaseTexture();
                    status.clear();
                }
                ImGui::SameLine();
                if (ImGui::Button("Next")) {
                    selected = (selected + 1) % entries.size();
                    releaseTexture();
                    status.clear();
                }
                ImGui::SameLine();
                ImGui::Text("%zu / %zu", selected + 1, entries.size());
                if (!texture && status.empty()) {
                    int width = 0, height = 0;
                    std::vector<uint8_t> pixels;
                    if (load(entries[selected], width, height, pixels, status)) {
                        if (width <= 0 || height <= 0 || uint64_t(width) * uint64_t(height) > 67108864 ||
                            pixels.size() != size_t(width) * size_t(height) * 4) {
                            status = "Invalid photograph dimensions.";
                        } else {
                            imageWidth = width;
                            imageHeight = height;
                            glGenTextures(1, &texture);
                            glBindTexture(GL_TEXTURE_2D, texture);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                                GL_UNSIGNED_BYTE, pixels.data());
                        }
                    }
                }
                ImGui::TextUnformatted(entries[selected].filename().u8string().c_str());
                if (texture) {
                    const ImVec2 available = ImGui::GetContentRegionAvail();
                    const float scale = std::max(0.01f, std::min(available.x / imageWidth, available.y / imageHeight));
                    ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2(imageWidth * scale, imageHeight * scale));
                }
            }
            if (!status.empty()) ImGui::TextWrapped("%s", status.c_str());
        }
        ImGui::End();
    }

    void releaseTexture() {
        if (texture) glDeleteTextures(1, &texture);
        texture = 0;
        imageWidth = imageHeight = 0;
    }

private:
    std::filesystem::path directory;
    std::vector<std::filesystem::path> entries;
    std::string status;
    size_t selected = 0;
    GLuint texture = 0;
    int imageWidth = 0;
    int imageHeight = 0;
    bool visible = false;
};
