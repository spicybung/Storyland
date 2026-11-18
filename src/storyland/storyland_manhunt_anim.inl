bool StorylandAnimFile::parseManhuntIfp(std::string &errorMessage) {
    try {
        struct Reader {
            const std::vector<uint8_t> &bytes;
            size_t cursor = 0;
            void require(size_t count) {
                if (cursor > bytes.size() || count > bytes.size() - cursor)
                    throw std::runtime_error("Truncated Manhunt animation data.");
            }
            uint32_t word() {
                require(4);
                const uint32_t value = animReadU32(bytes, cursor);
                cursor += 4;
                return value;
            }
            uint16_t half() {
                require(2);
                const uint16_t value = animReadU16(bytes, cursor);
                cursor += 2;
                return value;
            }
            float number() {
                require(4);
                const float value = animReadF32(bytes, cursor);
                cursor += 4;
                if (!std::isfinite(value))
                    throw std::runtime_error("Invalid Manhunt animation number.");
                return value;
            }
            std::string name() {
                const uint32_t count = word();
                if (count > 4096)
                    throw std::runtime_error("Invalid Manhunt animation name length.");
                require(count);
                const std::string value = animReadFixedString(bytes, cursor, count);
                cursor += count;
                return value;
            }
            void tag(uint32_t value) {
                if (word() != value)
                    throw std::runtime_error("Unexpected Manhunt animation record.");
            }
        } input{data};
        std::function<void(Reader &, const std::string &)> pack;
        pack = [&](Reader &reader, const std::string &blockName) {
            reader.tag(0x4B504E41u);
            const uint32_t count = reader.word();
            if (count > 10000 || clipRows.size() + count > 10000)
                throw std::runtime_error("Too many Manhunt animation clips.");
            for (uint32_t i = 0; i < count; ++i) {
                const size_t entryOffset = reader.cursor;
                reader.tag(0x454D414Eu);
                StorylandAnimClip clip;
                clip.index = uint32_t(clipRows.size());
                clip.entryOffset = uint32_t(entryOffset);
                clip.name = blockName + " / " + reader.name();
                const int32_t encodedCount = int32_t(reader.word());
                if (encodedCount == INT32_MIN)
                    throw std::runtime_error("Invalid Manhunt track count.");
                const uint32_t tracks = uint32_t(encodedCount < 0 ? -encodedCount : encodedCount);
                if (tracks > 4096)
                    throw std::runtime_error("Too many Manhunt animation tracks.");
                std::vector<uint8_t> unpacked;
                if (encodedCount < 0) {
                    const uint32_t packedSize = reader.word(), unpackedSize = reader.word();
                    reader.require(packedSize);
                    if (!unpackedSize || unpackedSize > 64u * 1024u * 1024u)
                        throw std::runtime_error("Invalid compressed animation size.");
                    unpacked.resize(unpackedSize);
                    uLongf actual = unpackedSize;
                    if (uncompress(unpacked.data(), &actual, reader.bytes.data() + reader.cursor,
                                   packedSize) != Z_OK ||
                        actual != unpackedSize)
                        throw std::runtime_error("Corrupt compressed Manhunt animation.");
                    reader.cursor += packedSize;
                }
                Reader payload{encodedCount < 0 ? unpacked : reader.bytes,
                               encodedCount < 0 ? 0 : reader.cursor};
                payload.word();
                clip.duration = payload.number();
                clip.channelCount = tracks;
                std::vector<StorylandAnimTrack> decodedTracks;
                for (uint32_t j = 0; j < tracks; ++j) {
                    const uint32_t tag = payload.word();
                    if (tag != 0x54514553u && tag != 0x55514553u)
                        throw std::runtime_error("Expected a Manhunt SEQT/SEQU track.");
                    StorylandAnimTrack track;
                    track.index = j;
                    track.clipIndex = clip.index;
                    track.channelIndex = j;
                    track.boneId = uint32_t(int32_t(int16_t(payload.half())));
                    payload.require(1);
                    const unsigned type = payload.bytes[payload.cursor++];
                    const uint32_t framesInTrack = payload.half();
                    if (type < 1 || type > 3)
                        throw std::runtime_error("Unsupported Manhunt key type.");
                    track.channelFlags = type == 1 ? 1 : 3;
                    track.source = "Manhunt 2";
                    track.name = "Bone_" + std::to_string(int32_t(track.boneId));
                    const size_t startOffset = payload.cursor;
                    const uint16_t start = payload.half();
                    if (type < 3 && !start)
                        payload.cursor = startOffset;
                    auto rotation = [&](StorylandAnimKey &key) {
                        key.qx = int16_t(payload.half()) / 4096.f;
                        key.qy = int16_t(payload.half()) / 4096.f;
                        key.qz = int16_t(payload.half()) / 4096.f;
                        key.qw = int16_t(payload.half()) / 4096.f;
                        const float length =
                            std::sqrt(key.qx * key.qx + key.qy * key.qy + key.qz * key.qz + key.qw * key.qw);
                        if (length > 1e-6f) {
                            key.qx /= length;
                            key.qy /= length;
                            key.qz /= length;
                            key.qw /= length;
                        } else {
                            key.qx = key.qy = key.qz = 0;
                            key.qw = 1;
                        }
                    };
                    StorylandAnimKey initial;
                    if (type == 3)
                        rotation(initial);
                    uint32_t ticks = 0;
                    for (uint32_t frame = 0; frame < framesInTrack; ++frame) {
                        StorylandAnimKey key = initial;
                        if (!start) {
                            if (type != 3 || frame)
                                ticks += payload.half();
                            key.time = ticks / 2048.f;
                        } else
                            key.time = std::max(0.f, start / 2048.f - 1.f / 30.f + frame / 30.f);
                        if (type < 3)
                            rotation(key);
                        if (type > 1) {
                            key.tx = int16_t(payload.half()) / 2048.f;
                            key.ty = int16_t(payload.half()) / 2048.f;
                            key.tz = int16_t(payload.half()) / 2048.f;
                        }
                        clip.duration = std::max(clip.duration, key.time);
                        track.keys.push_back(key);
                    }
                    if (tag == 0x54514553u)
                        payload.number();
                    decodedTracks.push_back(std::move(track));
                }
                if (encodedCount < 0) {
                    if (payload.cursor != unpacked.size())
                        throw std::runtime_error("Unexpected bytes in compressed Manhunt tracks.");
                } else
                    reader.cursor = payload.cursor;
                reader.word();
                reader.number();
                const uint32_t effectSize = reader.word(), effects = reader.word();
                const uint64_t effectBytes = uint64_t(effectSize) * effects;
                if (effectBytes > reader.bytes.size())
                    throw std::runtime_error("Invalid Manhunt effect table.");
                reader.require(size_t(effectBytes));
                reader.cursor += size_t(effectBytes);
                clipRows.push_back(std::move(clip));
                ifpClipTracks.push_back(std::move(decodedTracks));
            }
        };
        if (animReadU32(data, 0) == 0x54434E41u) {
            input.tag(0x54434E41u);
            const uint32_t blocks = input.word();
            if (blocks > 10000)
                throw std::runtime_error("Too many Manhunt animation blocks.");
            for (uint32_t i = 0; i < blocks; ++i) {
                input.tag(0x434F4C42u);
                const std::string block = input.name();
                pack(input, block);
            }
        } else if (animReadU32(data, 0) == 1u) {
            input.word();
            const uint32_t executions = input.word(), environmental = input.word();
            const uint64_t tableEnd = 12ull + uint64_t(executions) * 36ull + uint64_t(environmental) * 12ull;
            if (executions > 65536 || environmental > 65536 || tableEnd > data.size())
                throw std::runtime_error("Invalid Manhunt execution table.");
            auto executionPack = [&](const std::string &name, uint32_t offset, uint32_t length) {
                if (!offset && !length)
                    return;
                if (offset < tableEnd || length < 8 || uint64_t(offset) + length > data.size())
                    throw std::runtime_error("Invalid Manhunt execution animation range.");
                const std::vector<uint8_t> bytes(data.begin() + offset, data.begin() + offset + length);
                Reader reader{bytes};
                pack(reader, name);
            };
            static const char *stages[] = {"Jump", "White", "Yellow", "Red"};
            for (uint32_t i = 0; i < executions; ++i) {
                const uint32_t id = input.word();
                for (const char *stage : stages) {
                    const uint32_t offset = input.word(), length = input.word();
                    executionPack("Execution " + std::to_string(id) + " / " + stage, offset, length);
                }
            }
            for (uint32_t i = 0; i < environmental; ++i) {
                const uint32_t id = input.word(), offset = input.word(), length = input.word();
                executionPack("Environmental " + std::to_string(id), offset, length);
            }
        } else {
            pack(input, "Animations");
        }
        if (clipRows.empty())
            throw std::runtime_error("Manhunt animation library is empty.");
        manhuntIfp = true;
        ifpFormat = true;
        activeClip = 0;
        rebuildActiveClipTracks();
        errorMessage.clear();
        return true;
    } catch (const std::exception &exception) {
        fieldRows.clear();
        trackRows.clear();
        clipRows.clear();
        ifpClipTracks.clear();
        ifpFormat = false;
        hasCandidates = false;
        errorMessage = exception.what();
        return false;
    }
}
