#include "src/storyland/storyland_movie_player.h"
#include "src/storyland/storyland_pss_audio.h"
#if defined(__ANDROID__)
#include <GLES3/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <SDL.h>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <limits>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace {
std::string decoderError(int value) {
  char text[AV_ERROR_MAX_STRING_SIZE]{};
  av_strerror(value, text, sizeof(text));
  return text;
}
int movieColorspace(AVColorSpace colorspace) {
  switch (colorspace) {
    case AVCOL_SPC_BT709: return SWS_CS_ITU709;
    case AVCOL_SPC_FCC: return SWS_CS_FCC;
    case AVCOL_SPC_SMPTE240M: return SWS_CS_SMPTE240M;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL: return SWS_CS_BT2020;
    default: return SWS_CS_DEFAULT;
  }
}
struct Decoder {
  SDL_RWops* videoInput = nullptr;
  SDL_RWops* audioInput = nullptr;
  AVIOContext* io = nullptr;
  const std::atomic<bool>* cancelled = nullptr;
  AVFormatContext *format = nullptr;
  AVCodecContext *codec = nullptr;
  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();
  SwsContext *scaler = nullptr;
  SDL_AudioDeviceID device = 0;
  static int read(void* opaque, uint8_t* destination, int size) {
    auto& decoder = *static_cast<Decoder*>(opaque);
    if (decoder.cancelled && decoder.cancelled->load(std::memory_order_acquire))
      return AVERROR_EXIT;
    SDL_ClearError();
    const auto bytes = SDL_RWread(decoder.videoInput, destination, 1, size_t(size));
    if (bytes) return int(bytes);
    return *SDL_GetError() ? AVERROR(EIO) : AVERROR_EOF;
  }
  static int64_t seek(void* opaque, int64_t offset, int origin) {
    auto& decoder = *static_cast<Decoder*>(opaque);
    if (decoder.cancelled && decoder.cancelled->load(std::memory_order_acquire))
      return AVERROR_EXIT;
    const auto result = origin == AVSEEK_SIZE ? SDL_RWsize(decoder.videoInput) :
        SDL_RWseek(decoder.videoInput, offset, origin & ~AVSEEK_FORCE);
    return result < 0 ? AVERROR(EIO) : result;
  }
  static int interrupt(void* opaque) {
    const auto& decoder = *static_cast<Decoder*>(opaque);
    return decoder.cancelled && decoder.cancelled->load(std::memory_order_acquire);
  }
  ~Decoder() {
    if (device)
      SDL_CloseAudioDevice(device);
    sws_freeContext(scaler);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec);
    avformat_close_input(&format);
    if (io) {
      av_freep(&io->buffer);
      avio_context_free(&io);
    }
    if (videoInput) SDL_RWclose(videoInput);
    if (audioInput) SDL_RWclose(audioInput);
  }
};
} // namespace

struct StorylandMoviePlayer::State {
  struct Frame {
    int width = 0, height = 0;
    double time = 0;
    std::vector<uint8_t> rgba;
  };
  mutable std::mutex mutex;
  std::deque<Frame> frames;
  std::string message;
  std::thread worker;
  std::atomic<bool> cancel{false}, playing{false}, pauseRequested{false},
      decoded{false}, failed{false};
  std::atomic<double> clock{0};
  std::atomic<float> gain{1}, aspect{4.0f / 3.0f};
  GLuint texture = 0;
  int textureWidth = 0, textureHeight = 0;
  void setMessage(const std::string &text) {
    std::lock_guard<std::mutex> lock(mutex);
    message = text;
  }
  void decode(const std::filesystem::path &path) {
    try {
      Decoder decoder;
      decoder.cancelled = &cancel;
      if (!decoder.packet || !decoder.frame)
        throw std::bad_alloc();
      StorylandPssAudio audio;
      std::string audioError;
      const auto filename = path.u8string();
      decoder.audioInput = SDL_RWFromFile(filename.c_str(), "rb");
      if (!decoder.audioInput)
        throw std::runtime_error(std::string("Movie audio source: ") + SDL_GetError());
      const Sint64 audioSize = SDL_RWsize(decoder.audioInput);
      if (audioSize < 0)
        throw std::runtime_error("Could not determine PSS audio source size.");
      if (!audio.openStreaming(uint64_t(audioSize),
          [&](uint64_t offset, uint8_t* bytes, size_t count) {
            if (cancel || offset > uint64_t(std::numeric_limits<Sint64>::max()))
              return false;
            return SDL_RWseek(decoder.audioInput, Sint64(offset), RW_SEEK_SET) >= 0 &&
                SDL_RWread(decoder.audioInput, bytes, 1, count) == count;
          }, audioError, &cancel))
        throw std::runtime_error(audioError);
      if (cancel)
        return;
      SDL_AudioSpec desired{}, obtained{};
      desired.freq = int(audio.rate());
      desired.channels = Uint8(audio.channelCount());
      desired.format = AUDIO_S16SYS;
      desired.samples = 1024;
      decoder.device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
      decoder.videoInput = SDL_RWFromFile(filename.c_str(), "rb");
      if (!decoder.videoInput)
        throw std::runtime_error(std::string("Movie video source: ") + SDL_GetError());
      auto* buffer = static_cast<uint8_t*>(av_malloc(65536));
      if (!buffer) throw std::bad_alloc();
      decoder.io = avio_alloc_context(buffer, 65536, 0, &decoder,
          &Decoder::read, nullptr, &Decoder::seek);
      if (!decoder.io) {
        av_free(buffer);
        throw std::bad_alloc();
      }
      decoder.format = avformat_alloc_context();
      if (!decoder.format) throw std::bad_alloc();
      decoder.format->pb = decoder.io;
      decoder.format->flags |= AVFMT_FLAG_CUSTOM_IO;
      decoder.format->video_codec_id = AV_CODEC_ID_MPEG2VIDEO;
      decoder.format->skip_estimate_duration_from_pts = 1;
      decoder.format->interrupt_callback = {&Decoder::interrupt, &decoder};
      int result = avformat_open_input(&decoder.format, filename.c_str(),
                                       nullptr, nullptr);
      if (result < 0)
        throw std::runtime_error("Movie open: " + decoderError(result));
      result = avformat_find_stream_info(decoder.format, nullptr);
      if (result < 0)
        throw std::runtime_error("Movie streams: " + decoderError(result));
      const int streamIndex = av_find_best_stream(
          decoder.format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
      if (streamIndex < 0)
        throw std::runtime_error("No MPEG video stream in this PSS.");
      auto *stream = decoder.format->streams[streamIndex];
      const auto *codec = avcodec_find_decoder(stream->codecpar->codec_id);
      if (!codec)
        throw std::runtime_error("Unsupported PSS video codec.");
      decoder.codec = avcodec_alloc_context3(codec);
      if (!decoder.codec)
        throw std::bad_alloc();
      decoder.codec->thread_count = 2;
      result = avcodec_parameters_to_context(decoder.codec, stream->codecpar);
      if (result >= 0)
        result = avcodec_open2(decoder.codec, codec, nullptr);
      if (result < 0)
        throw std::runtime_error("Movie decoder: " + decoderError(result));
      const double frameDuration =
          stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0
              ? av_q2d(av_inv_q(stream->avg_frame_rate))
              : 1.0 / 30;
      double firstTimestamp = 0, nextTimestamp = 0, previousTimestamp = -1;
      int decodedWidth = 0, decodedHeight = 0, decodedFormat = -1;
      bool haveTimestamp = false, inputEnd = false, flushSent = false,
           audioEnd = decoder.device == 0, started = false;
      bool devicePaused = true;
      uint64_t submitted = 0;
      const double bytesPerSecond =
          double(audio.rate()) * audio.channelCount() * sizeof(int16_t);
      const Uint32 queueLimit = Uint32(bytesPerSecond * .4);
      uint64_t lastTick = SDL_GetTicks64();
      std::vector<int16_t> samples;
      const auto service = [&]() {
        while (decoder.device && !audioEnd && !cancel &&
               SDL_GetQueuedAudioSize(decoder.device) < queueLimit) {
          if (!audio.read(samples, audioEnd, audioError))
            throw std::runtime_error(audioError);
          if (!samples.empty()) {
            const float volume = gain.load(std::memory_order_relaxed);
            if (volume < 1) {
              for (auto& sample : samples)
                sample = int16_t(std::lround(float(sample) * volume));
            }
            const auto bytes = Uint32(samples.size() * sizeof(int16_t));
            if (SDL_QueueAudio(decoder.device, samples.data(), bytes) != 0)
              throw std::runtime_error(std::string("Movie audio queue: ") +
                                       SDL_GetError());
            submitted += bytes;
          }
        }
        const bool shouldPause = pauseRequested || !started;
        if (decoder.device && shouldPause != devicePaused) {
          SDL_PauseAudioDevice(decoder.device, shouldPause ? 1 : 0);
          devicePaused = shouldPause;
        }
        const auto now = SDL_GetTicks64();
        if (!shouldPause) {
          const Uint32 queued = decoder.device ? SDL_GetQueuedAudioSize(decoder.device) : 0;
          const double consumed =
              double(submitted - std::min<uint64_t>(submitted, queued)) /
              bytesPerSecond;
          if (audioEnd && queued == 0)
            clock = std::max(clock.load(), consumed) +
                    double(now - lastTick) / 1000;
          else
            clock = consumed;
        }
        lastTick = now;
      };
      setMessage(path.filename().u8string() +
          (decoder.device ? "" : " (audio output unavailable)"));
      while (!cancel) {
        service();
        bool full;
        {
          std::lock_guard<std::mutex> lock(mutex);
          full = frames.size() >= 4;
        }
        if (full || pauseRequested) {
          SDL_Delay(5);
          continue;
        }
        result = avcodec_receive_frame(decoder.codec, decoder.frame);
        if (result == AVERROR_EOF)
          break;
        if (result == AVERROR(EAGAIN)) {
          if (inputEnd) {
            if (flushSent)
              throw std::runtime_error(
                  "Movie decoder did not finish after its last packet.");
            result = avcodec_send_packet(decoder.codec, nullptr);
            flushSent = true;
            if (result < 0 && result != AVERROR_EOF)
              throw std::runtime_error(decoderError(result));
            continue;
          }
          result = av_read_frame(decoder.format, decoder.packet);
          if (result == AVERROR_EOF) {
            inputEnd = true;
            continue;
          }
          if (result < 0)
            throw std::runtime_error("Movie read: " + decoderError(result));
          if (decoder.packet->stream_index == streamIndex) {
            result = avcodec_send_packet(decoder.codec, decoder.packet);
            av_packet_unref(decoder.packet);
            if (result < 0)
              throw std::runtime_error("Movie packet: " + decoderError(result));
          } else
            av_packet_unref(decoder.packet);
          continue;
        }
        if (result < 0)
          throw std::runtime_error("Movie frame: " + decoderError(result));
        auto *source = decoder.frame;
        if (source->width < 1 || source->height < 1 || source->width > 2048 ||
            source->height > 2048)
          throw std::runtime_error("PSS movie dimensions exceed 2048 pixels.");
        if (decodedWidth &&
            (decodedWidth != source->width || decodedHeight != source->height ||
             decodedFormat != source->format))
          throw std::runtime_error(
              "The PSS changes video format during playback.");
        decodedWidth = source->width;
        decodedHeight = source->height;
        decodedFormat = source->format;
        const AVRational sampleAspect = av_guess_sample_aspect_ratio(decoder.format, stream, source);
        const double pixelAspect = sampleAspect.num > 0 && sampleAspect.den > 0 ?
            av_q2d(sampleAspect) : 1.0;
        aspect = float(double(source->width) * pixelAspect / source->height);
        Frame output;
        output.width = source->width;
        output.height = source->height;
        if (source->best_effort_timestamp != AV_NOPTS_VALUE) {
          const double timestamp =
              source->best_effort_timestamp * av_q2d(stream->time_base);
          if (!haveTimestamp) {
            firstTimestamp =
                audio.startSeconds() >= 0 ? audio.startSeconds() : timestamp;
            haveTimestamp = true;
          }
          output.time = std::max(0.0, timestamp - firstTimestamp);
        } else
          output.time = nextTimestamp;
        if (output.time <= previousTimestamp + 0.000001)
          output.time = nextTimestamp;
        previousTimestamp = output.time;
        nextTimestamp = output.time + frameDuration;
        if (!decoder.scaler)
          decoder.scaler = sws_getContext(
              source->width, source->height, AVPixelFormat(source->format),
              source->width, source->height, AV_PIX_FMT_RGBA, SWS_BILINEAR,
              nullptr, nullptr, nullptr);
        if (!decoder.scaler)
          throw std::runtime_error(
              "Could not create the movie colour converter.");
        const AVColorSpace colorspace = source->colorspace != AVCOL_SPC_UNSPECIFIED ?
            source->colorspace : decoder.codec->colorspace;
        const AVColorRange range = source->color_range != AVCOL_RANGE_UNSPECIFIED ?
            source->color_range : decoder.codec->color_range;
        const int* coefficients = sws_getCoefficients(movieColorspace(colorspace));
        if (sws_setColorspaceDetails(decoder.scaler, coefficients,
            range == AVCOL_RANGE_JPEG, coefficients, 1, 0, 1 << 16, 1 << 16) < 0)
          throw std::runtime_error("Could not apply the movie colour metadata.");
        output.rgba.resize(size_t(output.width) * output.height * 4);
        uint8_t *pixels[] = {output.rgba.data()};
        const int stride[] = {output.width * 4};
        if (sws_scale(decoder.scaler, source->data, source->linesize, 0,
                      output.height, pixels, stride) != output.height)
          throw std::runtime_error("Incomplete movie frame conversion.");
        {
          std::lock_guard<std::mutex> lock(mutex);
          frames.push_back(std::move(output));
        }
        av_frame_unref(source);
        started = true;
      }
      decoded = true;
      while (!cancel) {
        service();
        bool empty;
        {
          std::lock_guard<std::mutex> lock(mutex);
          empty = frames.empty();
        }
        if (empty && audioEnd &&
            (!decoder.device || SDL_GetQueuedAudioSize(decoder.device) == 0) &&
            clock >= nextTimestamp)
          break;
        SDL_Delay(5);
      }
    } catch (const std::bad_alloc &) {
      setMessage("Storyland failed to allocate enough memory for the movie.");
      failed = true;
    } catch (const std::exception &error) {
      setMessage(error.what());
      failed = !cancel.load();
    }
    playing = false;
  }
};
StorylandMoviePlayer::StorylandMoviePlayer()
    : state(std::make_unique<State>()) {}
StorylandMoviePlayer::~StorylandMoviePlayer() { stop(); }
bool StorylandMoviePlayer::prepareVideoPlayback(const std::wstring& path,
    std::string& error, const std::shared_ptr<std::atomic<bool>>& cancelled) {
  close();
  if (cancelled && cancelled->load(std::memory_order_acquire)) {
    error = "Movie preparation cancelled.";
    return false;
  }
  const std::filesystem::path filename(path);
  auto* input = SDL_RWFromFile(filename.u8string().c_str(), "rb");
  if (!input) {
    error = std::string("Could not open movie: ") + SDL_GetError();
    return false;
  }
  const auto size = SDL_RWsize(input);
  SDL_RWclose(input);
  if (size <= 0) {
    error = "Movie source is empty or unreadable.";
    return false;
  }
  preparedPath = filename;
  error.clear();
  return true;
}
bool StorylandMoviePlayer::play(std::string& error) {
  if (preparedPath.empty()) {
    error = "No movie source was prepared.";
    return false;
  }
  open(preparedPath);
  error.clear();
  return true;
}
void StorylandMoviePlayer::setPlaybackGain(float value) {
  state->gain = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}
std::string StorylandMoviePlayer::radioPlaybackError() const {
  return state->failed ? status() : std::string{};
}
bool StorylandMoviePlayer::tickVideo(std::string& error) {
  update();
  error = radioPlaybackError();
  return active();
}
bool StorylandMoviePlayer::isPlaying() const { return active(); }
void StorylandMoviePlayer::close() {
  stop();
  preparedPath.clear();
}
void StorylandMoviePlayer::open(const std::filesystem::path &path) {
  stop();
  state->cancel = false;
  state->playing = true;
  state->decoded = false;
  state->failed = false;
  state->pauseRequested = false;
  state->clock = 0;
  state->aspect = 4.0f / 3.0f;
  state->setMessage("Opening movie...");
  state->worker = std::thread([this, path]() { state->decode(path); });
}
void StorylandMoviePlayer::stop() {
  state->cancel = true;
  if (state->worker.joinable())
    state->worker.join();
  state->playing = false;
  state->frames.clear();
  resetTexture();
}
void StorylandMoviePlayer::pause(bool value) { state->pauseRequested = value; }
bool StorylandMoviePlayer::active() const { return state->playing; }
bool StorylandMoviePlayer::completed() const {
  return state->decoded && !state->playing && !state->failed;
}
bool StorylandMoviePlayer::paused() const { return state->pauseRequested; }
std::string StorylandMoviePlayer::status() const {
  std::lock_guard<std::mutex> lock(state->mutex);
  return state->message;
}
unsigned StorylandMoviePlayer::texture() const { return state->texture; }
double StorylandMoviePlayer::position() const { return state->clock; }
float StorylandMoviePlayer::displayAspect() const { return state->aspect; }
void StorylandMoviePlayer::resetTexture() {
  if (state->texture)
    glDeleteTextures(1, &state->texture);
  state->texture = 0;
  state->textureWidth = state->textureHeight = 0;
}
void StorylandMoviePlayer::update() {
  State::Frame frame;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    while (!state->frames.empty() &&
           state->frames.front().time <= state->clock + .005) {
      frame = std::move(state->frames.front());
      state->frames.pop_front();
    }
  }
  if (frame.rgba.empty())
    return;
  using BindBufferFunction = void (*)(GLenum, GLuint);
  const auto bindBuffer = reinterpret_cast<BindBufferFunction>(SDL_GL_GetProcAddress("glBindBuffer"));
  constexpr GLenum pixelUnpackBuffer = 0x88EC, pixelUnpackBinding = 0x88EF;
  GLint oldTexture = 0, oldAlignment = 0, oldBuffer = 0;
  GLint oldRowLength = 0, oldSkipPixels = 0, oldSkipRows = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldAlignment);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &oldRowLength);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &oldSkipPixels);
  glGetIntegerv(GL_UNPACK_SKIP_ROWS, &oldSkipRows);
  if (bindBuffer) {
    glGetIntegerv(pixelUnpackBinding, &oldBuffer);
    bindBuffer(pixelUnpackBuffer, 0);
  }
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  if (!state->texture)
    glGenTextures(1, &state->texture);
  glBindTexture(GL_TEXTURE_2D, state->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (state->textureWidth != frame.width ||
      state->textureHeight != frame.height) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame.width, frame.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, frame.rgba.data());
    state->textureWidth = frame.width;
    state->textureHeight = frame.height;
  } else
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width, frame.height, GL_RGBA,
                    GL_UNSIGNED_BYTE, frame.rgba.data());
  glPixelStorei(GL_UNPACK_ALIGNMENT, oldAlignment);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, oldRowLength);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, oldSkipPixels);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, oldSkipRows);
  if (bindBuffer) bindBuffer(pixelUnpackBuffer, GLuint(oldBuffer));
  glBindTexture(GL_TEXTURE_2D, GLuint(oldTexture));
}
