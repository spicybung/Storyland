#include "storyland_model_validator.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

namespace {

uint32_t readU32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) return 0;
    return uint32_t(bytes[offset]) |
           (uint32_t(bytes[offset + 1]) << 8) |
           (uint32_t(bytes[offset + 2]) << 16) |
           (uint32_t(bytes[offset + 3]) << 24);
}

std::string hexOffset(size_t value) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

void addIssue(StorylandModelIntegrityReport& report, StorylandModelIssueSeverity severity,
              size_t offset, const std::string& message) {
    report.issues.push_back({severity, offset, message});
    if (severity == StorylandModelIssueSeverity::Fatal) report.fatals++;
    else report.warnings++;
}

bool isSkinnedKind(StorylandModelKind kind) {
    return kind == StorylandModelKind::PedModel || kind == StorylandModelKind::CutsceneModel;
}

} // namespace

std::string StorylandModelIntegrityReport::text() const {
    std::ostringstream out;
    out << "MDL structural / skinning preflight\r\n\r\n"
        << "Target: " << label << "\r\n"
        << "Bytes inspected: " << byteSize << "\r\n"
        << "Decoded vertices: " << vertices << "\r\n"
        << "Decoded triangles: " << triangles << "\r\n"
        << "Skeleton bones: " << bones << "\r\n"
        << "Skeleton roots: " << rootBones << "\r\n"
        << "Weighted vertices: " << weightedVertices << "/" << vertices << "\r\n"
        << "Skin influences: " << skinInfluences << "\r\n"
        << "Warnings: " << warnings << "\r\n"
        << "Fatal structural hazards: " << fatals << "\r\n\r\n"
        << (safe() ? "RESULT: PASS - decoded MDL structure is internally consistent."
                   : "RESULT: FAIL - the decoded MDL is structurally unsafe or visibly incomplete.")
        << "\r\n";
    if (!issues.empty()) out << "\r\nIssues\r\n";
    for (const StorylandModelIntegrityIssue& issue : issues) {
        out << (issue.severity == StorylandModelIssueSeverity::Fatal ? "FATAL" : "WARN")
            << " @ file+" << hexOffset(issue.offset) << " | " << issue.message << "\r\n";
    }
    return out.str();
}

StorylandModelIntegrityReport storylandValidateModelIntegrity(
    const StorylandModelFile& model,
    const std::vector<uint8_t>& bytes,
    const std::string& label
) {
    StorylandModelIntegrityReport report;
    report.label = label;
    report.byteSize = bytes.size();
    report.vertices = model.previewPoints().size();
    report.triangles = model.previewTriangles().size();
    report.bones = model.armatureBones().size();

    if (bytes.size() < 0x20u || readU32(bytes, 0) != 0x006D646Cu) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 "Missing Leeds MDL chunk identifier 0x006D646C.");
    } else {
        const uint32_t declaredSize = readU32(bytes, 0x08u);
        if (declaredSize < 0x20u || declaredSize > bytes.size()) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 0x08u,
                     "Declared MDL chunk size extends beyond the available file bytes.");
        } else if (bytes.size() - declaredSize >= 2048u) {
            addIssue(report, StorylandModelIssueSeverity::Warning, 0x08u,
                     "More than one IMG sector follows the declared MDL chunk; streaming allocation or padding should be checked.");
        }
    }

    if (report.vertices == 0 || report.triangles == 0) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 "No complete renderable geometry was decoded.");
    }

    size_t invalidTriangles = 0;
    for (const StorylandModelTriangle& triangle : model.previewTriangles()) {
        if (triangle.a >= report.vertices || triangle.b >= report.vertices || triangle.c >= report.vertices)
            invalidTriangles++;
    }
    if (invalidTriangles != 0) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 std::to_string(invalidTriangles) + " decoded triangles reference vertices outside the vertex array.");
    }

    size_t nonFinitePoints = 0;
    for (const StorylandModelPoint& point : model.previewPoints()) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) nonFinitePoints++;
    }
    if (nonFinitePoints != 0) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 std::to_string(nonFinitePoints) + " decoded vertices contain non-finite coordinates.");
    }

    if (!isSkinnedKind(model.modelKind())) return report;

    const auto& bones = model.armatureBones();
    if (bones.empty()) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 "Ped/cutscene model has no decoded RslTAnim skeleton.");
    } else {
        for (const StorylandModelBone& bone : bones) {
            if (bone.parentIndex == 0xFFFFFFFFu) {
                report.rootBones++;
            } else if (bone.parentIndex >= bones.size()) {
                addIssue(report, StorylandModelIssueSeverity::Fatal, bone.offset,
                         "Bone '" + bone.name + "' has an out-of-range parent index.");
            } else if (bone.parentIndex == bone.index) {
                addIssue(report, StorylandModelIssueSeverity::Fatal, bone.offset,
                         "Bone '" + bone.name + "' is parented to itself.");
            }
        }

        if (report.rootBones != 1u) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                     "Skinned hierarchy must have exactly one root, but " +
                     std::to_string(report.rootBones) + " were decoded.");
        }

        std::set<uint32_t> cycleMembers;
        for (uint32_t start = 0; start < bones.size(); ++start) {
            std::set<uint32_t> path;
            uint32_t cursor = start;
            while (cursor < bones.size() && bones[cursor].parentIndex != 0xFFFFFFFFu) {
                if (!path.insert(cursor).second) {
                    cycleMembers.insert(cursor);
                    break;
                }
                cursor = bones[cursor].parentIndex;
            }
        }
        if (!cycleMembers.empty()) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, bones[*cycleMembers.begin()].offset,
                     "RslTAnim parent links contain a cycle; bind transforms cannot be resolved safely.");
        }
    }

    const auto& skin = model.previewSkinWeights();
    if (skin.size() != report.vertices) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 "Skin row count does not match the decoded vertex count (" +
                 std::to_string(skin.size()) + " vs " + std::to_string(report.vertices) + ").");
    }

    size_t invalidBoneInfluences = 0;
    const size_t rows = std::min(skin.size(), report.vertices);
    for (size_t vertex = 0; vertex < rows; ++vertex) {
        const StorylandModelSkinWeights& weights = skin[vertex];
        bool weighted = false;
        const uint32_t count = std::min<uint32_t>(weights.influenceCount, 4u);
        for (uint32_t influence = 0; influence < count; ++influence) {
            const StorylandModelSkinInfluence& item = weights.influences[influence];
            if (!std::isfinite(item.weight) || item.weight <= 0.00001f) continue;
            weighted = true;
            report.skinInfluences++;
            if (item.boneIndex >= bones.size()) invalidBoneInfluences++;
        }
        if (weighted) report.weightedVertices++;
    }

    if (invalidBoneInfluences != 0) {
        addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                 std::to_string(invalidBoneInfluences) + " skin influences reference missing bones.");
    }

    if (report.vertices != 0) {
        const double coverage = double(report.weightedVertices) / double(report.vertices);
        if (coverage < 0.95) {
            addIssue(report, StorylandModelIssueSeverity::Fatal, 0,
                     "Only " + std::to_string(report.weightedVertices) + " of " +
                     std::to_string(report.vertices) + " ped vertices have usable skin weights; detached/exploded parts are expected.");
        } else if (report.weightedVertices != report.vertices) {
            addIssue(report, StorylandModelIssueSeverity::Warning, 0,
                     std::to_string(report.vertices - report.weightedVertices) +
                     " ped vertices have no usable skin influence.");
        }
    }

    return report;
}
