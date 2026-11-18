// Included by storyland_media.cpp after the FFmpeg and Windows declarations.
class StorylandRadioStream {
public:
    ~StorylandRadioStream() {stop();releaseDecoder();}
    bool open(const std::wstring& path, std::string& error,
        const std::shared_ptr<std::atomic<bool>>& preparationCancelled={}) {
        filePath=path;
        rawVb=std::filesystem::path(path).extension().wstring();
        std::transform(rawVb.begin(),rawVb.end(),rawVb.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});
        if(rawVb==L".vb") {
            if(!vb.open(std::filesystem::path(path),error))return false;
            sampleRate=32000;channels=2;duration=vb.duration();
        } else if(rawVb==L".pss" && pss.open(std::filesystem::path(path),error,preparationCancelled.get())) {
            sonyPss=true;
            sampleRate=pss.rate();channels=pss.channelCount();duration=pss.duration();
            presentationStart=pss.startSeconds();
            error.clear();
        } else {
            if(preparationCancelled&&preparationCancelled->load(std::memory_order_acquire)) {
                error="Audio preparation cancelled.";return false;
            }
            if(preparationCancelled) {
                format=avformat_alloc_context();
                if(!format) {error="Could not allocate the movie audio input.";return false;}
                format->interrupt_callback.callback=[](void* value) -> int {
                    return static_cast<std::atomic<bool>*>(value)->load(std::memory_order_acquire)?1:0;
                };
                format->interrupt_callback.opaque=preparationCancelled.get();
            }
            const auto utf8=std::filesystem::path(path).u8string();
            int result=avformat_open_input(&format,utf8.c_str(),nullptr,nullptr);
            if(result<0) {error="Could not open radio stream: "+ffmpegError(result);return false;}
            result=avformat_find_stream_info(format,nullptr);
            if(result<0) {error="Could not read radio stream metadata: "+ffmpegError(result);return false;}
            streamIndex=av_find_best_stream(format,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);
            if(streamIndex<0) {error="No audio stream in this radio file.";return false;}
            const auto* selectedStream=format->streams[streamIndex];
            if(selectedStream->start_time!=AV_NOPTS_VALUE)
                presentationStart=double(selectedStream->start_time)*av_q2d(selectedStream->time_base);
            const auto* decoder=avcodec_find_decoder(format->streams[streamIndex]->codecpar->codec_id);
            if(!decoder) {error="No decoder for this radio codec.";return false;}
            codec=avcodec_alloc_context3(decoder);
            if(!codec) {error="Could not allocate radio decoder.";return false;}
            result=avcodec_parameters_to_context(codec,format->streams[streamIndex]->codecpar);
            if(result>=0)result=avcodec_open2(codec,decoder,nullptr);
            if(result<0) {error="Could not initialize radio decoder: "+ffmpegError(result);return false;}
            packet=av_packet_alloc();frame=av_frame_alloc();
            if(!packet||!frame) {error="Could not allocate radio decode buffers.";return false;}
            if(format->duration>0)duration=double(format->duration)/AV_TIME_BASE;
        }
        bool eof=false;
        if(!readFrame(primed,eof,error)||eof||primed.empty()) {
            if(error.empty())error="Radio stream has no playable audio.";
            return false;
        }
        return true;
    }
    bool seek(double seconds, std::string& error) {
        if(!std::isfinite(seconds)||seconds<0) {error="Invalid radio start position.";return false;}
        if(duration>0)seconds=std::fmod(seconds,duration);
        if(seconds==0) {sourceStartSeconds=0;return true;}
        if(rawVb==L".vb") {
            if(!vb.seek(seconds,primed,error))return false;
            sourceStartSeconds=seconds;return true;
        }
        if(sonyPss) {
            if(!pss.seek(seconds,primed,error))return false;
            sourceStartSeconds=seconds;return true;
        }
        const auto* stream=format->streams[streamIndex];
        const int64_t start=stream->start_time==AV_NOPTS_VALUE?0:stream->start_time;
        const int64_t target=start+int64_t(seconds*stream->time_base.den/stream->time_base.num);
        const int result=av_seek_frame(format,streamIndex,target,AVSEEK_FLAG_BACKWARD);
        if(result<0) {error="Could not seek radio stream: "+ffmpegError(result);return false;}
        avcodec_flush_buffers(codec);av_packet_unref(packet);av_frame_unref(frame);
        inputEof=flushed=false;primed.clear();
        bool eof=false;
        if(!readFrame(primed,eof,error)||eof||primed.empty())return false;
        sourceStartSeconds=seconds;return true;
    }
    uint32_t rate() const {return sampleRate;}
    uint16_t channelCount() const {return channels;}
    double durationSeconds() const {return duration;}
    double presentationStartSeconds() const {return presentationStart;}
    void setFrontendAmbience(StorylandFrontendAmbience value) {frontendAmbience.store(value);}
    void setGain(float value) {gain.store(value);}
    void setLooping(bool value) {looping.store(value);}
    void setPaused(bool value) {paused.store(value);}
    bool isPlaying() const {return running.load();}
    bool playbackPositionMilliseconds(uint32_t& position) const {
        if(!playbackPositionValid.load())return false;
        position=storylandRadioSourcePosition(sourceStartSeconds.load(),playedMilliseconds.load(),duration,looping.load());
        return true;
    }
    std::string error() const {std::lock_guard<std::mutex> lock(errorMutex);return playbackError;}
    bool play(std::string& error) {
#ifdef _WIN32
        if(worker.joinable()) {
            if(running.load()) {error="Radio stream is already started.";return false;}
            stop(); // Reap the completed worker before replaying.
        }
        if(hasPlayed) {
            // Stop discards queued and decoded-ahead audio. Replay starts at the
            // source origin, while an initial seek still applies to first play.
            cancelled=false;primed.clear();
            if(!rewind(error))return false;
            sourceStartSeconds=0;
            bool eof=false;
            if(!readFrame(primed,eof,error)||eof||primed.empty()) {
                if(error.empty())error="Radio stream has no playable audio after rewind.";
                return false;
            }
        }
        {std::lock_guard<std::mutex> lock(errorMutex);playbackError.clear();}
        if(!output.open(sampleRate,channels,error))return false;
        output.gain(gain.load());
        cancelled=false;running=true;playedMilliseconds=0;playbackPositionValid=false;
        try {
            worker=std::thread([this](){
                SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
                pump();
            });
        }
        catch(const std::exception& exception) {
            running=false;output.close();
            error=exception.what();return false;
        }
        hasPlayed=true;
        error.clear();return true;
#else
        error="Radio playback requires the Windows audio backend.";return false;
#endif
    }
    void stop() {
        cancelled=true;
        if(worker.joinable())worker.join();
        running=false;
    }
private:
    bool readFrame(std::vector<int16_t>& pcm, bool& eof, std::string& error) {
        if(rawVb==L".vb")return vb.read(pcm,eof,error);
        if(sonyPss)return pss.read(pcm,eof,error);
        pcm.clear();eof=false;
        while(!cancelled) {
            int result=avcodec_receive_frame(codec,frame);
            if(result==0) {
                const int rate=frame->sample_rate>0?frame->sample_rate:codec->sample_rate;
                const int count=frame->ch_layout.nb_channels;
                if(rate<=0 || rate>384000 || count<1 || count>2 ||
                    (sampleRate && (sampleRate!=uint32_t(rate)||channels!=uint16_t(count)))) {
                    error="Radio audio has unsupported or changing channel/rate metadata.";return false;
                }
                sampleRate=uint32_t(rate);channels=uint16_t(count);
                const bool decoded=storylandAppendAudioFrameS16(frame,pcm,error);
                av_frame_unref(frame);
                if(!decoded)return false;
                if(!pcm.empty())return true;
                continue;
            }
            if(result==AVERROR_EOF) {eof=true;return true;}
            if(result!=AVERROR(EAGAIN)) {error="Radio decode failed: "+ffmpegError(result);return false;}
            if(inputEof) {
                if(flushed) {eof=true;return true;}
                result=avcodec_send_packet(codec,nullptr);flushed=true;
                if(result<0 && result!=AVERROR_EOF) {error="Could not flush radio decoder: "+ffmpegError(result);return false;}
                continue;
            }
            do {
                av_packet_unref(packet);result=av_read_frame(format,packet);
            } while(result>=0 && packet->stream_index!=streamIndex && !cancelled);
            if(cancelled)return false;
            if(result==AVERROR_EOF) {inputEof=true;continue;}
            if(result<0) {error="Could not read radio packet: "+ffmpegError(result);return false;}
            result=avcodec_send_packet(codec,packet);av_packet_unref(packet);
            if(result<0) {error="Could not submit radio packet: "+ffmpegError(result);return false;}
        }
        return false;
    }
    bool rewind(std::string& error) {
        if(rawVb==L".vb")return vb.open(std::filesystem::path(filePath),error);
        if(sonyPss)return pss.open(std::filesystem::path(filePath),error);
        const auto* stream=format->streams[streamIndex];
        const int64_t start=stream->start_time==AV_NOPTS_VALUE?0:stream->start_time;
        const int result=av_seek_frame(format,streamIndex,start,AVSEEK_FLAG_BACKWARD);
        if(result<0) {error="Could not loop radio stream: "+ffmpegError(result);return false;}
        avcodec_flush_buffers(codec);av_packet_unref(packet);av_frame_unref(frame);
        inputEof=flushed=false;return true;
    }
    void fail(const std::string& error) {std::lock_guard<std::mutex> lock(errorMutex);playbackError=error;}
#ifdef _WIN32
    void pump() {
        struct Buffer {std::vector<int16_t> pcm;};
        // Decoders can produce frames of only a few milliseconds. Feeding those directly
        // to the device causes queue starvation and audible discontinuities.
        // Prebuffer two seconds. Decoder and texture-upload spikes on the
        // same machine must not starve the playback queue.
        std::array<Buffer,12> buffers;
        std::vector<int16_t> decoded;
        size_t decodedCursor=0;
        bool exhausted=false;
        bool devicePaused=false;
        StorylandFrontendEcho echo(sampleRate,channels,frontendAmbience.load());
        auto fillBuffer=[&](std::vector<int16_t>& destination,std::string& message)->bool {
            // Keep stereo frames intact even at rates such as 44,100 Hz.
            const size_t targetSamples=std::max<size_t>(1,size_t(sampleRate)/6)*channels;
            destination.clear();
            if(exhausted)return false;
            destination.reserve(targetSamples);
            while(destination.size()<targetSamples&&!cancelled) {
                if(decodedCursor>=decoded.size()) {
                    decoded.clear();decodedCursor=0;
                    if(!primed.empty()) {decoded=std::move(primed);primed.clear();}
                    else {
                        bool eof=false;
                        if(!readFrame(decoded,eof,message))return false;
                        if(eof) {
                            if(!looping.load()) {
                                exhausted=true;
                                break;
                            }
                            if(!rewind(message)||!readFrame(decoded,eof,message)||eof) {
                                if(message.empty())message="Radio stream could not loop.";
                                return false;
                            }
                        }
                    }
                    if(decoded.empty())continue;
                }
                const size_t count=std::min(targetSamples-destination.size(),decoded.size()-decodedCursor);
                destination.insert(destination.end(),decoded.begin()+decodedCursor,decoded.begin()+decodedCursor+count);
                decodedCursor+=count;
            }
            if(!destination.empty())echo.process(destination.data(),destination.size());
            return !destination.empty();
        };
        auto submitBuffer=[&](Buffer& buffer)->bool {
            if(!output.submit(buffer.pcm.data(),uint32_t(buffer.pcm.size()*sizeof(int16_t)))) {
                fail("Could not queue streaming audio.");return false;
            }
            return true;
        };
        try {
            // Decode ahead before starting the voice so loading a model cannot
            // starve the beginning of the dialogue.
            for(auto& buffer:buffers) {
                if(cancelled)break;
                std::string message;
                if(!fillBuffer(buffer.pcm,message)) {
                    if(!cancelled&&!message.empty()) {fail(message);cancelled=true;}
                    break;
                }
            }
            if(!cancelled) {
                if(!output.pause(true)) {
                    fail("Could not pause streaming audio before playback.");
                    cancelled=true;
                } else devicePaused=true;
            }
            if(!cancelled)for(auto& buffer:buffers) {
                if(cancelled)break;
                if(buffer.pcm.empty())continue;
                if(!submitBuffer(buffer))cancelled=true;
            }
            size_t nextBuffer=0;
            uint32_t submittedBuffers=output.queued();
            while(!cancelled) {
                const bool pauseRequested=paused.load();
                if(pauseRequested!=devicePaused) {
                    if(!output.pause(pauseRequested)) {fail("Could not change audio pause state.");break;}
                    devicePaused=pauseRequested;
                }
                output.gain(gain.load());
                playedMilliseconds=std::max(playedMilliseconds.load(),output.milliseconds());
                playbackPositionValid=true;
                const uint32_t queued=output.queued();
                uint32_t completed=submittedBuffers-queued;
                submittedBuffers=queued;
                while(completed--&&!cancelled) {
                    auto& buffer=buffers[nextBuffer];
                    nextBuffer=(nextBuffer+1)%buffers.size();
                    if(exhausted)continue;
                    std::string message;
                    if(!fillBuffer(buffer.pcm,message)) {
                        if(!message.empty()) {fail(message);cancelled=true;}
                        if(!exhausted)cancelled=true;
                        continue;
                    }
                    if(!submitBuffer(buffer)) {cancelled=true;break;}
                    ++submittedBuffers;
                }
                if(exhausted&&submittedBuffers==0)break;
                if(!cancelled)std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } catch(const std::exception& exception) {fail(exception.what());}
        output.close();running=false;
    }
    StorylandAudioOutput output;
#endif
    std::atomic<StorylandFrontendAmbience> frontendAmbience{StorylandFrontendAmbience::None};
    void releaseDecoder() {
        if(frame)av_frame_free(&frame);
        if(packet)av_packet_free(&packet);
        if(codec)avcodec_free_context(&codec);
        if(format)avformat_close_input(&format);
    }
    StorylandRadioVb vb;
    StorylandPssAudio pss;
    double presentationStart=-1;
    bool sonyPss=false;
    std::wstring filePath,rawVb;
    AVFormatContext* format=nullptr;
    AVCodecContext* codec=nullptr;
    AVPacket* packet=nullptr;
    AVFrame* frame=nullptr;
    int streamIndex=-1;
    bool inputEof=false,flushed=false,hasPlayed=false;
    uint32_t sampleRate=0;
    uint16_t channels=0;
    double duration=0;
    std::vector<int16_t> primed;
    std::thread worker;
    std::atomic<bool> running{false},cancelled{false},looping{true},paused{false};
    std::atomic<bool> playbackPositionValid{false};
    std::atomic<uint32_t> playedMilliseconds{0};
    std::atomic<double> sourceStartSeconds{0};
    std::atomic<float> gain{1.0f};
    mutable std::mutex errorMutex;
    std::string playbackError;
};
