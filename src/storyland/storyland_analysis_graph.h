#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct StorylandAnalysisGraphNode {
    int parent = -1;
    uint64_t offset = 0;
    uint64_t size = 0;
    uint32_t type = 0;
    uint32_t version = 0;
    std::string label;
    std::string description;
};

struct StorylandAnalysisGraphEdge {
    size_t source = 0, target = 0;
    uint64_t pointerOffset = 0;
};

class StorylandAnalysisGraph {
public:
    bool build(
        const std::vector<uint8_t>& bytes,
        const std::string& extensionLower,
        const std::string& displayName,
        std::string& errorMessage);

    const std::vector<StorylandAnalysisGraphNode>& nodes() const;
    const std::string& summary() const;
    const std::vector<StorylandAnalysisGraphEdge>& edges() const;

private:
    std::vector<StorylandAnalysisGraphNode> graphNodes;
    std::string graphSummary;
    std::vector<StorylandAnalysisGraphEdge> graphEdges;
    bool buildNativeChunk(const std::vector<uint8_t>& bytes, const std::string& displayName, std::string& errorMessage);

    int addNode(
        int parent,
        uint64_t offset,
        uint64_t size,
        uint32_t type,
        uint32_t version,
        const std::string& label,
        const std::string& description = {});

    bool buildRenderWare(
        const std::vector<uint8_t>& bytes,
        const std::string& displayName,
        std::string& errorMessage);
    bool buildMdl(
        const std::vector<uint8_t>& bytes,
        const std::string& displayName,
        std::string& errorMessage);
    bool buildTextureArchive(
        const std::vector<uint8_t>& bytes,
        const std::string& extensionLower,
        const std::string& displayName,
        std::string& errorMessage);
    bool buildAnim(
        const std::vector<uint8_t>& bytes,
        const std::string& displayName,
        std::string& errorMessage);
};
