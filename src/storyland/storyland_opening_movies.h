#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class StorylandOpeningMovieSkipInput {
public:
    void reset() { held=false;waitingForRelease=false; }
    void consume() { held=true; }
    void finish() { waitingForRelease=true; }
    bool controllerPressed(bool pressed) {
        if(!pressed)waitingForRelease=false;
        const bool edge=pressed&&!held;
        held=pressed;
        return edge;
    }
    bool blocksMenu() const { return waitingForRelease; }
private:
    bool held=false,waitingForRelease=false;
};

enum class StorylandOpeningMovieRole { Sting, GameIntro, Studio };

struct StorylandOpeningMovieSource {
    std::filesystem::path path;
    float displayAspect=4.0f/3.0f;
};

struct StorylandOpeningMovie {
    StorylandOpeningMovieRole role=StorylandOpeningMovieRole::GameIntro;
    std::vector<StorylandOpeningMovieSource> sources;
};

struct StorylandOpeningMovieCrop {
    float left=0,top=0,right=1,bottom=1;
};

inline StorylandOpeningMovieCrop storylandOpeningMovieCrop(
    float viewportWidth,float viewportHeight,float sourceAspect) {
    StorylandOpeningMovieCrop crop;
    if(!std::isfinite(viewportWidth)||!std::isfinite(viewportHeight)||
        !std::isfinite(sourceAspect)||viewportWidth<=0||viewportHeight<=0||sourceAspect<=0)
        return crop;
    const double viewportAspect=double(viewportWidth)/double(viewportHeight);
    if(double(sourceAspect)>viewportAspect) {
        const float span=float(viewportAspect/double(sourceAspect));
        crop.left=(1.0f-span)*.5f;
        crop.right=1.0f-crop.left;
    } else {
        const float span=float(double(sourceAspect)/viewportAspect);
        crop.top=(1.0f-span)*.5f;
        crop.bottom=1.0f-crop.top;
    }
    return crop;
}

struct StorylandOpeningMoviePresentation {
    StorylandOpeningMovieCrop background;
    StorylandOpeningMovieCrop foreground;
    bool margins=false;
};

inline StorylandOpeningMoviePresentation storylandOpeningMoviePresentation(
    float viewportWidth,float viewportHeight,float sourceAspect) {
    StorylandOpeningMoviePresentation presentation;
    if(!std::isfinite(viewportWidth)||!std::isfinite(viewportHeight)||
        !std::isfinite(sourceAspect)||viewportWidth<=0||viewportHeight<=0||sourceAspect<=0)
        return presentation;
    presentation.background=storylandOpeningMovieCrop(viewportWidth,viewportHeight,sourceAspect);
    const double viewportAspect=double(viewportWidth)/double(viewportHeight);
    if(double(sourceAspect)>viewportAspect) {
        const float span=float(viewportAspect/double(sourceAspect));
        presentation.foreground.top=(1.0f-span)*.5f;
        presentation.foreground.bottom=1.0f-presentation.foreground.top;
    } else {
        const float span=float(double(sourceAspect)/viewportAspect);
        presentation.foreground.left=(1.0f-span)*.5f;
        presentation.foreground.right=1.0f-presentation.foreground.left;
    }
    presentation.margins=presentation.foreground.left>0||presentation.foreground.top>0;
    return presentation;
}

inline std::filesystem::path storylandFindBundledOpeningMovie(
    const std::filesystem::path& executablePath,const std::string& filename) {
    if(filename.empty()||filename.find_first_of("/\\:")!=std::string::npos||
        filename=="."||filename==".."||filename.find('\0')!=std::string::npos)return {};
    const auto lower=[](std::string name) {
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char character) {
            return char(std::tolower(character));
        });
        return name;
    };
    const auto child=[&](const std::filesystem::path& directory,const std::string& name,
        bool folder) {
        std::error_code error;
        const auto direct=directory/name;
        if(folder?std::filesystem::is_directory(direct,error):
            std::filesystem::is_regular_file(direct,error))return direct;
        std::vector<std::filesystem::path> matches;
        error.clear();
        for(std::filesystem::directory_iterator entry(directory,error),end;
            !error&&entry!=end;entry.increment(error)) {
            if(lower(entry->path().filename().u8string())!=lower(name))continue;
            std::error_code kindError;
            if(folder?entry->is_directory(kindError):entry->is_regular_file(kindError))
                matches.push_back(entry->path());
        }
        std::sort(matches.begin(),matches.end());
        return matches.empty()?std::filesystem::path{}:matches.front();
    };
    std::vector<std::filesystem::path> roots;
    std::error_code error;
    if(!executablePath.empty()) {
        auto directory=std::filesystem::absolute(executablePath,error);
        if(!error) {
            if(!std::filesystem::is_directory(directory,error))directory=directory.parent_path();
            roots.push_back(std::move(directory));
        }
    }
    error.clear();
    const auto workingDirectory=std::filesystem::current_path(error);
    if(!error)roots.push_back(workingDirectory);
    std::vector<std::filesystem::path> visited;
    for(auto directory:roots) {
        for(unsigned depth=0;depth<5&&!directory.empty();++depth) {
            directory=directory.lexically_normal();
            if(std::find(visited.begin(),visited.end(),directory)==visited.end()) {
                visited.push_back(directory);
                const auto assets=child(directory,"assets",true);
                const auto movies=assets.empty()?std::filesystem::path{}:child(assets,"movies",true);
                const auto movie=movies.empty()?std::filesystem::path{}:child(movies,filename,false);
                if(!movie.empty())return movie;
            }
            const auto parent=directory.parent_path();
            if(parent==directory||parent==parent.root_path())break;
            directory=parent;
        }
    }
    return {};
}

template<class ResolveRetail,class ResolveBundled>
std::vector<StorylandOpeningMovie> storylandResolveOpeningMovies(
    bool vcs,bool pal,bool pcShader,ResolveRetail resolveRetail,ResolveBundled resolveBundled,
    bool retailPs2=true) {
    std::vector<StorylandOpeningMovie> movies;
    const auto addSource=[](StorylandOpeningMovie& movie,std::filesystem::path path,float aspect) {
        if(path.empty())return;
        path=path.lexically_normal();
        for(const auto& source:movie.sources)if(source.path==path)return;
        movie.sources.push_back({std::move(path),aspect});
    };
    const auto addMovie=[&](StorylandOpeningMovie movie) {
        if(!movie.sources.empty())movies.push_back(std::move(movie));
    };
    const auto nativeVcs=[&](StorylandOpeningMovie& movie,const char* name) {
        if(!retailPs2)return;
        addSource(movie,resolveRetail(std::string(name)+(pal?"P.pss":".pss")),4.0f/3.0f);
        addSource(movie,resolveRetail(std::string(name)+(pal?".pss":"P.pss")),4.0f/3.0f);
    };
    StorylandOpeningMovie intro{StorylandOpeningMovieRole::GameIntro,{}};
    if(vcs) {
        StorylandOpeningMovie sting{StorylandOpeningMovieRole::Sting,{}};
        nativeVcs(sting,"stings");
        addMovie(std::move(sting));
        if(pcShader)addSource(intro,resolveBundled("intro_pc.pss"),8.0f/7.0f);
        nativeVcs(intro,"intro");
    } else if(retailPs2) {
        for(const char* name:{"RSNY.PSS","RSLEEDS.PSS","RSNORTH.PSS"}) {
            StorylandOpeningMovie movie{StorylandOpeningMovieRole::Sting,{}};
            addSource(movie,resolveRetail(name),4.0f/3.0f);
            addMovie(std::move(movie));
        }
        addSource(intro,resolveRetail("DM.PSS"),4.0f/3.0f);
    }
    StorylandOpeningMovie studio{StorylandOpeningMovieRole::Studio,{}};
    addSource(studio,resolveBundled("reigns_studio.pss"),16.0f/9.0f);
    addMovie(std::move(studio));
    addMovie(std::move(intro));
    return movies;
}

template<class Resolve>
std::vector<std::filesystem::path> storylandResolveVcsOpeningMovies(bool pal,Resolve resolve) {
    std::vector<std::filesystem::path> paths;
    for(const auto* base:{"stings","intro"}) {
        auto path=resolve(std::string(base)+(pal?"P.pss":".pss"));
        if(path.empty())path=resolve(std::string(base)+(pal?".pss":"P.pss"));
        if(!path.empty())paths.push_back(std::move(path));
    }
    return paths;
}

template<class Media>
class StorylandOpeningMovies {
public:
    StorylandOpeningMovies()=default;
    StorylandOpeningMovies(const StorylandOpeningMovies&)=delete;
    StorylandOpeningMovies& operator=(const StorylandOpeningMovies&)=delete;
    ~StorylandOpeningMovies() { stop(); }

    void begin(std::vector<StorylandOpeningMovie> movies) {
        stop();
        for(auto& movie:movies) {
            movie.sources.erase(std::remove_if(movie.sources.begin(),movie.sources.end(),
                [](const auto& source){return source.path.empty();}),movie.sources.end());
            if(!movie.sources.empty())sequence.push_back(std::move(movie));
        }
        index=0;
        sourceIndex=0;
        diagnostic.clear();
        prepareCurrent();
    }

    void begin(std::vector<std::filesystem::path> movies) {
        std::vector<StorylandOpeningMovie> sequence;
        for(auto& path:movies)sequence.push_back({StorylandOpeningMovieRole::GameIntro,
            {{std::move(path),4.0f/3.0f}}});
        begin(std::move(sequence));
    }

    void stop() {
        cancelCurrent();
        sequence.clear();
        index=0;
        sourceIndex=0;
    }

    bool active() const { return index<sequence.size(); }
    Media* current() { return playing.get(); }
    const Media* current() const { return playing.get(); }
    const std::string& status() const { return diagnostic; }
    const StorylandOpeningMovie* currentMovie() const {
        return active()?&sequence[index]:nullptr;
    }
    const StorylandOpeningMovieSource* currentSource() const {
        return active()?&sequence[index].sources[sourceIndex]:nullptr;
    }
    float currentDisplayAspect(float decodedAspect=4.0f/3.0f) const {
        const auto* source=currentSource();
        return source&&std::isfinite(source->displayAspect)&&source->displayAspect>0?
            source->displayAspect:decodedAspect;
    }

    bool skip() {
        if(!active())return false;
        advance();
        return true;
    }

    void tick(float gain) {
        if(!active())return;
        try {
            if(!playing&&ticket&&ticket->ready.load(std::memory_order_acquire)) {
                if(!ticket->prepared) {
                    failCurrent(ticket->error);
                    return;
                }
                playing=std::move(ticket->media);
                playing->setPlaybackGain(gain);
                std::string error;
                if(!playing->play(error)) {
                    failCurrent(error);
                    return;
                }
            }
            if(playing) {
                playing->setPlaybackGain(gain);
                const auto audioError=playing->radioPlaybackError();
                if(!audioError.empty()) {
                    failCurrent(audioError);
                    return;
                }
                std::string error;
                const bool decoded=playing->tickVideo(error);
                if(!error.empty())failCurrent(error);
                else if(!decoded||!playing->isPlaying())advance();
            }
        } catch(const std::exception& exception) {
            failCurrent(exception.what());
        } catch(...) {
            failCurrent("Movie playback failed.");
        }
    }

private:
    struct Preparation {
        std::shared_ptr<std::atomic<bool>> cancelled=std::make_shared<std::atomic<bool>>(false);
        std::atomic<bool> ready{false};
        std::shared_ptr<Media> media;
        bool prepared=false;
        std::string error;
        ~Preparation() { if(media)media->close(); }
    };

    void cancelCurrent() {
        if(ticket)ticket->cancelled->store(true,std::memory_order_release);
        if(playing)playing->close();
        playing.reset();
        ticket.reset();
    }

    void advance() {
        cancelCurrent();
        ++index;
        sourceIndex=0;
        prepareCurrent();
    }

    void failCurrent(const std::string& error) {
        if(!active())return;
        if(!diagnostic.empty())diagnostic+='\n';
        diagnostic+=currentSource()->path.filename().u8string()+": "+
            (error.empty()?"Movie playback failed.":error);
        cancelCurrent();
        ++sourceIndex;
        if(sourceIndex>=sequence[index].sources.size()) {
            ++index;
            sourceIndex=0;
        }
        prepareCurrent();
    }

    void prepareCurrent() {
        if(!active())return;
        ticket=std::make_shared<Preparation>();
        const auto pending=ticket;
        const auto path=currentSource()->path.wstring();
        try {
            std::thread([pending,path]() {
                try {
                    auto media=std::make_shared<Media>();
                    pending->prepared=media->prepareVideoPlayback(path,pending->error,pending->cancelled);
                    if(pending->cancelled->load(std::memory_order_acquire)) {
                        media->close();
                        return;
                    }
                    pending->media=std::move(media);
                } catch(const std::exception& exception) {
                    pending->error=exception.what();
                    pending->prepared=false;
                } catch(...) {
                    pending->error="Movie preparation failed.";
                    pending->prepared=false;
                }
                pending->ready.store(true,std::memory_order_release);
            }).detach();
        } catch(const std::exception& exception) {
            ticket->error=exception.what();
            ticket->ready.store(true,std::memory_order_release);
        }
    }

    std::vector<StorylandOpeningMovie> sequence;
    size_t index=0;
    size_t sourceIndex=0;
    std::shared_ptr<Preparation> ticket;
    std::shared_ptr<Media> playing;
    std::string diagnostic;
};
