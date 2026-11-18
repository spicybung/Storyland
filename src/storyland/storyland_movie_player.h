#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

class StorylandMoviePlayer {
public:
    StorylandMoviePlayer();
    ~StorylandMoviePlayer();
    StorylandMoviePlayer(const StorylandMoviePlayer&) = delete;
    StorylandMoviePlayer& operator=(const StorylandMoviePlayer&) = delete;

    bool prepareVideoPlayback(const std::wstring& path, std::string& error,
        const std::shared_ptr<std::atomic<bool>>& cancelled);
    bool play(std::string& error);
    void setPlaybackGain(float gain);
    std::string radioPlaybackError() const;
    bool tickVideo(std::string& error);
    bool isPlaying() const;
    void close();

    void open(const std::filesystem::path& path);
    void stop();
    void pause(bool value);
    void update();
    bool active() const;
    bool paused() const;
    bool completed() const;
    std::string status() const;
    unsigned texture() const;
    double position() const;
    float displayAspect() const;
    void resetTexture();

private:
    struct State;
    std::unique_ptr<State> state;
    std::filesystem::path preparedPath;
};
