#include "storyland_psp_validator.h"

#include "storyland_model.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

namespace {

std::string pspHexValue(uint64_t value, int width = 8) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(width) << std::setfill('0') << value;
    return out.str();
}

} // namespace

std::string StorylandPspDmaReport::text() const {
    std::ostringstream out;
    out << "PSP DMA / GE stream test\r\n\r\n"
        << "Target: " << label << "\r\n"
        << "Bytes: " << byteSize << "\r\n"
        << "Geometry blocks: " << geometryBlocks << "\r\n"
        << "Mesh streams: " << meshStreams << "\r\n"
        << "Vertex buffers: " << vertexStreams << "\r\n"
        << "Warnings: " << warnings << "\r\n"
        << "Fatal hazards: " << fatals << "\r\n\r\n"
        << (safe() ? "RESULT: PASS - PSP geometry stream ranges are internally valid."
                   : "RESULT: FAIL - a PSP geometry stream escapes its resource range.")
        << "\r\nNote: PSP uses GE vertex streams rather than the PS2 VIF packet format.\r\n";
    if (!issues.empty()) out << "\r\nIssues\r\n";
    for (const StorylandDmaIssue& issue : issues) {
        const char* severity = issue.severity == StorylandDmaIssueSeverity::Fatal ? "FATAL" :
                               issue.severity == StorylandDmaIssueSeverity::Warning ? "WARN" : "INFO";
        out << severity << " @ file+" << pspHexValue(issue.offset)
            << " addr=" << pspHexValue(issue.address) << " | " << issue.message << "\r\n";
    }
    return out.str();
}

StorylandPspDmaReport storylandValidatePspDmaGe(
    const StorylandModelFile& model,
    const std::vector<uint8_t>& bytes,
    const std::string& label
) {
    StorylandPspDmaReport report;
    report.label = label;
    report.byteSize = bytes.size();
    size_t geometryStart = std::numeric_limits<size_t>::max();
    size_t geometryEnd = 0;
    size_t vertexBase = std::numeric_limits<size_t>::max();

    auto issue = [&](StorylandDmaIssueSeverity severity, size_t offset, uint32_t address,
                     const std::string& message) {
        report.issues.push_back({severity, offset, address, message});
        if (severity == StorylandDmaIssueSeverity::Fatal) report.fatals++;
        else if (severity == StorylandDmaIssueSeverity::Warning) report.warnings++;
    };

    for (const StorylandModelField& field : model.fields()) {
        const bool geometryField = field.group.rfind("sPspGeometry @", 0) == 0;
        const bool meshField = field.group.rfind("sPspGeometryMesh #", 0) == 0;
        if (geometryField && field.name == "size") {
            geometryStart = field.offset;
            vertexBase = std::numeric_limits<size_t>::max();
            report.geometryBlocks++;
            const uint64_t end = uint64_t(geometryStart) + uint64_t(field.value);
            geometryEnd = size_t(std::min<uint64_t>(end, bytes.size()));
            if (field.value < 0x48u || end > bytes.size()) {
                issue(StorylandDmaIssueSeverity::Fatal, field.offset, field.value,
                      "sPspGeometry size extends beyond the MDL resource.");
            }
        } else if (geometryField && field.name == "vertex_buffer_offset") {
            if (field.offset < 0x40u) continue;
            const size_t header = field.offset - 0x40u;
            if (geometryStart != header) geometryStart = header;
            const uint64_t absolute = uint64_t(header) + uint64_t(field.value);
            if (absolute >= bytes.size() || (geometryEnd != 0 && absolute >= geometryEnd)) {
                issue(StorylandDmaIssueSeverity::Fatal, field.offset, uint32_t(absolute),
                      "PSP vertex buffer starts outside its sPspGeometry block.");
                vertexBase = std::numeric_limits<size_t>::max();
            } else {
                vertexBase = size_t(absolute);
                report.vertexStreams++;
            }
        } else if (meshField && field.name == "vertex_stream_offset") {
            report.meshStreams++;
            if (vertexBase == std::numeric_limits<size_t>::max()) {
                issue(StorylandDmaIssueSeverity::Warning, field.offset, field.value,
                      "PSP mesh stream has no resolved vertex buffer base.");
                continue;
            }
            const uint64_t absolute = uint64_t(vertexBase) + uint64_t(field.value);
            if (absolute >= bytes.size() || (geometryEnd != 0 && absolute >= geometryEnd)) {
                issue(StorylandDmaIssueSeverity::Fatal, field.offset, uint32_t(absolute),
                      "PSP mesh stream points outside its geometry block.");
            }
        }
    }

    if (report.geometryBlocks == 0)
        issue(StorylandDmaIssueSeverity::Warning, 0, 0, "No decoded sPspGeometry block was found.");
    if (report.meshStreams == 0)
        issue(StorylandDmaIssueSeverity::Warning, 0, 0, "No decoded PSP mesh stream was found.");
    return report;
}
