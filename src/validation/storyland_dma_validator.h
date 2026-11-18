#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class StorylandDmaIssueSeverity {
    Info,
    Warning,
    Fatal
};

struct StorylandDmaIssue {
    StorylandDmaIssueSeverity severity = StorylandDmaIssueSeverity::Info;
    size_t offset = 0;
    uint32_t address = 0;
    std::string message;
    std::string stage;
    uint32_t qwc = 0;
    uint32_t command = 0;
};

struct StorylandDmaTlbReport {
    std::string label;
    size_t byteSize = 0;
    uint32_t dmaTags = 0;
    uint32_t dmaChains = 0;
    uint64_t dmaPayloadBytes = 0;
    uint32_t vifStreams = 0;
    uint32_t vifCommands = 0;
    uint32_t vifUnpacks = 0;
    uint32_t directTransfers = 0;
    uint32_t gifTags = 0;
    uint32_t gifPackets = 0;
    uint64_t gifPayloadBytes = 0;
    uint32_t gifPackedRegisters = 0;
    uint32_t gifReglistRegisters = 0;
    uint32_t gsRegisterWrites = 0;
    uint32_t gsPrimitiveKicks = 0;
    uint32_t vu1DataQwordsWritten = 0;
    uint32_t vu1MicroInstructionsUploaded = 0;
    uint32_t vu1MicroCalls = 0;
    uint32_t vu1MicroCallsWithResidentCode = 0;
    uint32_t vu1MicroCallsExternal = 0;
    uint32_t vu1InstructionsExecuted = 0;
    uint32_t vu1ProgramsCompleted = 0;
    uint32_t vu1UnsupportedInstructions = 0;
    uint32_t vu1Xgkicks = 0;
    uint32_t fifoQwordsTransferred = 0;
    uint32_t incompleteStages = 0;
    uint32_t declaredDmaRoots = 0;
    uint64_t vu1DataDigest = 0;
    uint64_t fifoDigest = 0;
    uint32_t warnings = 0;
    uint32_t fatals = 0;
    std::vector<StorylandDmaIssue> issues;

    bool safe() const { return fatals == 0; }
    bool complete() const { return safe() && incompleteStages == 0 && vifStreams != 0; }
    std::string text() const;
    std::string firstError() const;
};

struct StorylandPs2DmaRegion {
    size_t start = 0;
    size_t end = 0;
    bool transferTagData = false;
};

struct StorylandPs2ValidationOptions {
    // Explicit regions are for packet editors and execution tests. Native MDLs
    // discover their regions from geometry part tables rather than opcode scans.
    std::vector<StorylandPs2DmaRegion> dmaRegions;
    std::vector<StorylandPs2DmaRegion> vifRegions;
    std::vector<uint8_t> residentVu1Microcode;
    uint32_t residentVu1MicroAddress = 0;
    uint32_t maximumVu1Instructions = 65536;
    bool captureVu1Data = false;
};

struct StorylandPs2ExecutionResult {
    StorylandDmaTlbReport report;
    std::vector<uint32_t> vu1Data;
    std::vector<uint8_t> vu1DataKnown;
};

StorylandDmaTlbReport storylandValidatePs2DmaTlb(const std::vector<uint8_t>& bytes, const std::string& label);
StorylandPs2ExecutionResult storylandExecutePs2DmaTlb(const std::vector<uint8_t>& bytes,
    const std::string& label, const StorylandPs2ValidationOptions& options);
StorylandDmaTlbReport storylandValidatePs2WorldResource(const std::vector<uint8_t>& bytes,
    size_t start, size_t end, uint32_t materialRowLength, const std::string& label);
StorylandDmaTlbReport storylandValidatePs2WorldResource(const std::vector<uint8_t>& bytes,
    size_t start, size_t end, uint32_t materialRowLength, const std::string& label, bool computeVu1Digest);
