#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

class StorylandPssAudio {
public:
  bool open(const std::filesystem::path &path, std::string &error,
            const std::atomic<bool>* cancelled = nullptr) {
    reset();
    readSource = {};
    input.close();
    input.clear();
    if (cancelled && cancelled->load(std::memory_order_acquire))
      return fail(error, "PSS audio preparation cancelled.");
    input.open(path, std::ios::binary | std::ios::ate);
    if (!input)
      return fail(error, "Could not open PSS audio source.");
    const auto end = input.tellg();
    if (end < 0)
      return fail(error, "Could not determine PSS size.");
    return openStream(uint64_t(end),
        [this](uint64_t offset, uint8_t* bytes, size_t count) {
          if (offset > uint64_t(std::numeric_limits<std::streamoff>::max()) ||
              count > size_t(std::numeric_limits<std::streamsize>::max()))
            return false;
          input.clear();
          input.seekg(std::streamoff(offset));
          input.read(reinterpret_cast<char*>(bytes), std::streamsize(count));
          return size_t(input.gcount()) == count;
        }, error, cancelled);
  }

  bool openStream(uint64_t size,
                  std::function<bool(uint64_t, uint8_t*, size_t)> reader,
                  std::string& error,
                  const std::atomic<bool>* cancelled = nullptr) {
    const bool result = openSource(size, std::move(reader), error, cancelled, true);
    scanCancelled = nullptr;
    return result;
  }

  // The reader and cancellation token remain valid until playback ends.
  bool openStreaming(uint64_t size,
                     std::function<bool(uint64_t, uint8_t*, size_t)> reader,
                     std::string& error,
                     const std::atomic<bool>* cancelled = nullptr) {
    return openSource(size, std::move(reader), error, cancelled, false);
  }

  bool read(std::vector<int16_t> &pcm, bool &eof, std::string &error) {
    pcm.clear();
    error.clear();
    eof = position >= dataBytes;
    if (eof)
      return true;
    const size_t blockBytes = size_t(interleave) * channels;
    const uint64_t blockStart = position / blockBytes * blockBytes;
    const size_t skipFrames = size_t(position - blockStart) / (channels * 2);
    std::vector<uint8_t> block(blockBytes);
    if (!readStream(dataOffset + blockStart, block.data(), block.size(), error))
      return false;
    pcm.reserve((interleave / 2 - skipFrames) * channels);
    for (size_t frame = skipFrames; frame < interleave / 2; ++frame) {
      for (size_t channel = 0; channel < channels; ++channel) {
        const size_t offset = channel * interleave + frame * 2;
        const uint16_t value =
            uint16_t(block[offset]) | (uint16_t(block[offset + 1]) << 8);
        pcm.push_back(
            int16_t(value < 0x8000 ? int(value) : int(value) - 65536));
      }
    }
    position = blockStart + blockBytes;
    return true;
  }

  bool seek(double seconds, std::vector<int16_t> &pcm, std::string &error) {
    if (!std::isfinite(seconds) || seconds < 0 || !sampleRate || !channels)
      return fail(error, "Invalid PSS audio seek.");
    const double frame =
        std::min(seconds * sampleRate, double(dataBytes / (channels * 2)));
    position = uint64_t(frame) * channels * 2;
    bool eof = false;
    return read(pcm, eof, error);
  }
  double startSeconds() const { return firstTimestamp; }
  uint32_t rate() const { return sampleRate; }
  uint16_t channelCount() const { return uint16_t(channels); }
  double duration() const {
    return sampleRate && channels ?
        double(dataBytes) / (double(sampleRate) * channels * 2) : 0;
  }

private:
  struct Segment {
    uint64_t offset, size, start;
  };
  static uint32_t little32(const uint8_t *bytes) {
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
           (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
  }
  static bool fail(std::string &error, const char *message) {
    error = message;
    return false;
  }
  bool openSource(uint64_t size,
                  std::function<bool(uint64_t, uint8_t*, size_t)> reader,
                  std::string& error, const std::atomic<bool>* cancelled,
                  bool indexAll) {
    reset();
    readSource = std::move(reader);
    sourceBytes = size;
    scanCancelled = cancelled;
    if (!readSource || size < 4)
      return fail(error, "PSS audio source is empty or unreadable.");
    if (indexAll && !indexPackets(std::numeric_limits<uint64_t>::max(), error))
      return false;
    std::array<uint8_t, 40> header{};
    if (!readStream(0, header.data(), header.size(), error))
      return false;
    if (std::memcmp(header.data(), "SShd", 4))
      return fail(error, "No Sony SShd audio stream in this PSS.");
    const uint32_t headerSize = little32(header.data() + 4);
    const uint32_t codec = little32(header.data() + 8);
    sampleRate = little32(header.data() + 12);
    channels = little32(header.data() + 16);
    interleave = little32(header.data() + 20);
    if (headerSize < 24 || headerSize > 4096 || codec != 1)
      return fail(error,
                  "Unsupported PSS audio encoding (expected Sony PCM16).");
    if (sampleRate < 8000 || sampleRate > 192000 || channels < 1 ||
        channels > 2 || interleave < 2 || interleave > 65536 || interleave % 2)
      return fail(error,
                  "Invalid PSS PCM channel, rate or interleave metadata.");
    std::array<uint8_t, 8> body{};
    const uint64_t bodyOffset = 8 + uint64_t(headerSize);
    if (!readStream(bodyOffset, body.data(), body.size(), error))
      return false;
    if (std::memcmp(body.data(), "SSbd", 4))
      return fail(error, "PSS audio is missing its SSbd header.");
    dataOffset = bodyOffset + 8;
    dataBytes = little32(body.data() + 4);
    const uint64_t available = indexAll ? streamBytes : sourceBytes;
    if (!dataBytes || dataOffset > available ||
        dataBytes > available - dataOffset ||
        dataBytes % (channels * interleave))
      return fail(error, "Truncated or misaligned PSS PCM payload.");
    error.clear();
    return true;
  }
  bool indexPackets(uint64_t minimumBytes, std::string& error) {
    if (scanCancelled && scanCancelled->load(std::memory_order_acquire))
      return fail(error, "PSS audio preparation cancelled.");
    while (!scanComplete && streamBytes < minimumBytes) {
      if (scanCancelled && scanCancelled->load(std::memory_order_acquire))
        return fail(error, "PSS audio preparation cancelled.");
      if (scanOffset > sourceBytes || sourceBytes - scanOffset < 4) {
        scanComplete = true;
        break;
      }
      std::array<uint8_t, 14> header{};
      if (!readAt(scanOffset, header.data(), 4))
        return fail(error, "Truncated PSS start code.");
      if (header[0] || header[1] || header[2] != 1) {
        ++scanOffset;
        continue;
      }
      const uint8_t code = header[3];
      if (code == 0xB9) {
        scanOffset += 4;
        scanComplete = true;
        break;
      }
      if (code == 0xBA) {
        if (!readAt(scanOffset + 4, header.data(), 10))
          return fail(error, "Truncated PSS pack header.");
        uint64_t length = 0;
        if ((header[0] & 0xC0) == 0x40)
          length = 14 + (header[9] & 7);
        else if ((header[0] & 0xF0) == 0x20)
          length = 12;
        else
          return fail(error, "Invalid PSS pack header.");
        if (length > sourceBytes - scanOffset)
          return fail(error, "Truncated PSS pack header.");
        scanOffset += length;
        continue;
      }
      if (code < 0xBB) {
        scanOffset += 4;
        continue;
      }
      if (!readAt(scanOffset + 4, header.data(), 2))
        return fail(error, "Truncated PSS packet length.");
      const uint64_t length = (uint64_t(header[0]) << 8) | header[1];
      if (length > sourceBytes - scanOffset - 6)
        return fail(error, "PSS packet extends beyond the file.");
      const uint64_t packetEnd = scanOffset + 6 + length;
      if (code == 0xBD && length >= 7) {
        if (!readAt(scanOffset + 6, header.data(), 3))
          return fail(error, "Truncated PSS PES header.");
        if ((header[0] & 0xC0) != 0x80)
          return fail(error, "Unsupported PSS PES header.");
        const uint64_t payload = scanOffset + 9 + header[2];
        if (payload + 4 > packetEnd)
          return fail(error, "Invalid PSS private packet size.");
        double packetTimestamp = -1;
        if (header[1] & 0x80) {
          if (header[2] < 5)
            return fail(error, "Truncated PSS audio timestamp.");
          std::array<uint8_t, 5> timestamp{};
          if (!readAt(scanOffset + 9, timestamp.data(), timestamp.size()))
            return fail(error, "Truncated PSS audio timestamp.");
          if (!(timestamp[0] & 1) || !(timestamp[2] & 1) || !(timestamp[4] & 1))
            return fail(error, "Invalid PSS audio timestamp.");
          const uint64_t ticks = (uint64_t((timestamp[0] >> 1) & 7) << 30) |
                                 (uint64_t(timestamp[1]) << 22) |
                                 (uint64_t(timestamp[2] >> 1) << 15) |
                                 (uint64_t(timestamp[3]) << 7) |
                                 (timestamp[4] >> 1);
          packetTimestamp = double(ticks) / 90000;
        }
        if (!readAt(payload, header.data(), 4))
          return fail(error, "Truncated PSS private header.");
        if (header[0] == 0xFF && (header[1] & 0xF0) == 0xA0) {
          if (selectedChannel < 0)
            selectedChannel = header[1];
          if (selectedChannel == header[1] && payload + 4 < packetEnd) {
            if (segments.size() >= 2000000)
              return fail(error, "Too many PSS audio packets.");
            if (segments.empty())
              firstTimestamp = packetTimestamp;
            segments.push_back(
                {payload + 4, packetEnd - payload - 4, streamBytes});
            streamBytes += packetEnd - payload - 4;
          }
        }
      }
      scanOffset = packetEnd;
    }
    if (scanCancelled && scanCancelled->load(std::memory_order_acquire))
      return fail(error, "PSS audio preparation cancelled.");
    return true;
  }
  bool readAt(uint64_t offset, uint8_t *bytes, size_t count) {
    return readSource && offset <= sourceBytes && count <= sourceBytes - offset &&
           readSource(offset, bytes, count);
  }
  void reset() {
    segments.clear();
    sourceBytes = streamBytes = dataOffset = dataBytes = position = 0;
    scanOffset = 0;
    selectedChannel = -1;
    scanComplete = false;
    scanCancelled = nullptr;
    sampleRate = channels = interleave = 0;
    firstTimestamp = -1;
  }
  bool readStream(uint64_t offset, uint8_t *bytes, size_t count,
                  std::string& error) {
    if (offset > sourceBytes || count > sourceBytes - offset)
      return fail(error, "Truncated PSS audio stream.");
    while (count) {
      if (scanCancelled && scanCancelled->load(std::memory_order_acquire))
        return fail(error, "PSS audio preparation cancelled.");
      if (offset >= streamBytes) {
        if (!indexPackets(offset + 1, error))
          return false;
        if (offset >= streamBytes)
          return fail(error, "Truncated PSS audio stream.");
      }
      auto segment = std::upper_bound(segments.begin(), segments.end(), offset,
                                      [](uint64_t value, const Segment &entry) {
                                        return value < entry.start;
                                      });
      if (segment == segments.begin())
        return fail(error, "Invalid PSS audio segment.");
      --segment;
      const uint64_t within = offset - segment->start;
      if (within >= segment->size)
        return fail(error, "Invalid PSS audio segment.");
      const size_t amount =
          size_t(std::min<uint64_t>(count, segment->size - within));
      if (!readAt(segment->offset + within, bytes, amount))
        return fail(error, "Could not read PSS audio bytes.");
      bytes += amount;
      offset += amount;
      count -= amount;
    }
    return true;
  }
  std::ifstream input;
  std::function<bool(uint64_t, uint8_t*, size_t)> readSource;
  uint64_t sourceBytes = 0;
  uint64_t scanOffset = 0;
  int selectedChannel = -1;
  bool scanComplete = false;
  const std::atomic<bool>* scanCancelled = nullptr;
  std::vector<Segment> segments;
  uint64_t streamBytes = 0, dataOffset = 0, dataBytes = 0, position = 0;
  double firstTimestamp = -1;
  uint32_t sampleRate = 0, channels = 0, interleave = 0;
};
