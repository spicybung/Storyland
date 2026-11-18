#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

struct StorylandWorldIndexRange {
    uint32_t materialId = UINT32_MAX;
    uint32_t pspStripIndex = UINT32_MAX;
    size_t firstIndex = 0;
    size_t indexCount = 0;
    size_t triangleCount = 0;
};

struct StorylandWorldIndexBatches {
    std::vector<uint32_t> indices;
    std::vector<StorylandWorldIndexRange> ranges;

    template<class WorldMesh>
    void build(const WorldMesh& mesh, bool pspMaterialStrips) {
        indices.clear();
        ranges.clear();
        if (mesh.vertices.empty() || mesh.triangles.empty()) return;

        indices.reserve(mesh.triangles.size() * 3u);
        for (const auto& triangle : mesh.triangles) {
            if (triangle.a >= mesh.vertices.size() ||
                triangle.b >= mesh.vertices.size() ||
                triangle.c >= mesh.vertices.size()) continue;

            const uint32_t strip = pspMaterialStrips ? triangle.pspStripIndex : UINT32_MAX;
            if (ranges.empty() || ranges.back().materialId != triangle.textureId ||
                ranges.back().pspStripIndex != strip) {
                StorylandWorldIndexRange range;
                range.materialId = triangle.textureId;
                range.pspStripIndex = strip;
                range.firstIndex = indices.size();
                ranges.push_back(range);
            }

            indices.push_back(triangle.a);
            indices.push_back(triangle.b);
            indices.push_back(triangle.c);
            ranges.back().indexCount += 3u;
            ++ranges.back().triangleCount;
        }
    }

    size_t storageBytes() const {
        return indices.capacity() * sizeof(uint32_t) +
            ranges.capacity() * sizeof(StorylandWorldIndexRange);
    }
};

// The decoded geometry is immutable in Play Game. Reuse its authored material
// runs instead of reconstructing every triangle index on every frame.
template<class WorldMesh>
class StorylandWorldIndexStore {
    struct Entry {
        StorylandWorldIndexBatches draw;
        const void* triangleData = nullptr;
        size_t triangleCount = 0;
        size_t vertexCount = 0;
        size_t bytes = 0;
        uint64_t lastFrame = 0;
        bool pspMaterialStrips = false;
    };

    std::unordered_map<const WorldMesh*, Entry> entries;
    size_t residentBytes = 0;
    size_t capacityBytes = 0;
    uint64_t frame = 0;

public:
    explicit StorylandWorldIndexStore(size_t maximumBytes = 64u * 1024u * 1024u)
        : capacityBytes(maximumBytes) {}

    void clear() {
        entries.clear();
        residentBytes = 0;
        frame = 0;
    }

    void beginFrame() {
        if (frame == UINT64_MAX) {
            clear();
        }
        ++frame;
    }

    size_t bytes() const { return residentBytes; }
    size_t size() const { return entries.size(); }

    const StorylandWorldIndexBatches& get(const WorldMesh& mesh,
        bool pspMaterialStrips, StorylandWorldIndexBatches& scratch) {
        auto found = entries.find(&mesh);
        if (found != entries.end()) {
            if (found->second.triangleData == mesh.triangles.data() &&
                found->second.triangleCount == mesh.triangles.size() &&
                found->second.vertexCount == mesh.vertices.size() &&
                found->second.pspMaterialStrips == pspMaterialStrips) {
                found->second.lastFrame = frame;
                return found->second.draw;
            }
            residentBytes -= found->second.bytes;
            entries.erase(found);
        }

        scratch.build(mesh, pspMaterialStrips);
        const size_t required = scratch.storageBytes();
        if (required > capacityBytes) return scratch;

        while (required > capacityBytes - residentBytes) {
            auto oldest = entries.end();
            for (auto item = entries.begin(); item != entries.end(); ++item) {
                if (item->second.lastFrame == frame) continue;
                if (oldest == entries.end() ||
                    item->second.lastFrame < oldest->second.lastFrame) oldest = item;
            }
            if (oldest == entries.end()) return scratch;
            residentBytes -= oldest->second.bytes;
            entries.erase(oldest);
        }

        Entry entry;
        entry.draw = std::move(scratch);
        entry.triangleData = mesh.triangles.data();
        entry.triangleCount = mesh.triangles.size();
        entry.vertexCount = mesh.vertices.size();
        entry.bytes = required;
        entry.lastFrame = frame;
        entry.pspMaterialStrips = pspMaterialStrips;
        residentBytes += required;
        const auto inserted = entries.emplace(&mesh, std::move(entry));
        return inserted.first->second.draw;
    }
};
