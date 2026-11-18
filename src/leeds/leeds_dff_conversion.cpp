#include "src/leeds/leeds_dff_conversion.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace {

constexpr uint32_t RwSanAndreas = 0x1803FFFFu;
constexpr uint32_t RwMobileLcs = 0x00000310u;
constexpr uint32_t RwMobileGeometry = 0x0000F00Du;
constexpr uint32_t RwStruct = 1u;
constexpr uint32_t RwString = 2u;
constexpr uint32_t RwExtension = 3u;
constexpr uint32_t RwTexture = 6u;
constexpr uint32_t RwMaterial = 7u;
constexpr uint32_t RwMaterialList = 8u;
constexpr uint32_t RwFrameList = 14u;
constexpr uint32_t RwGeometry = 15u;
constexpr uint32_t RwClump = 16u;
constexpr uint32_t RwAtomic = 20u;
constexpr uint32_t RwGeometryList = 26u;
constexpr uint32_t RwHAnim = 0x11Eu;
constexpr uint32_t RwUVAnimation = 0x135u;
constexpr uint32_t RwUVAnimationDictionary = 0x2Bu;
constexpr uint32_t RwAnimation = 0x1Bu;
constexpr uint32_t RwSkin = 0x116u;
constexpr uint32_t RwMatFx = 0x120u;
constexpr uint32_t RwFrameName = 0x0253F2FEu;
constexpr float NativePositionScale = 0.00003051820313464149f;

struct Buffer {
    std::vector<uint8_t> data;

    void byte(uint8_t value) { data.push_back(value); }
    void u16(uint16_t value) { byte(uint8_t(value)); byte(uint8_t(value >> 8)); }
    void u32(uint32_t value) {
        byte(uint8_t(value)); byte(uint8_t(value >> 8));
        byte(uint8_t(value >> 16)); byte(uint8_t(value >> 24));
    }
    void i32(int32_t value) { u32(uint32_t(value)); }
    void f32(float value) {
        uint32_t raw = 0;
        std::memcpy(&raw, &value, sizeof(raw));
        u32(raw);
    }
    void bytes(const std::vector<uint8_t>& value) {
        data.insert(data.end(), value.begin(), value.end());
    }
    void zeros(size_t count) { data.insert(data.end(), count, 0u); }
    void align(size_t size) { zeros((size - (data.size() % size)) % size); }
    void replace32(size_t offset, uint32_t value) {
        for (size_t byteIndex = 0; byteIndex < 4u; ++byteIndex)
            data[offset + byteIndex] = uint8_t(value >> (byteIndex * 8u));
    }
    void chunk(uint32_t type, uint32_t stamp, const Buffer& payload) {
        u32(type);
        u32(uint32_t(payload.data.size()));
        u32(stamp);
        bytes(payload.data);
    }
    void namedChunk(uint32_t type, uint32_t stamp, const std::string& name) {
        Buffer payload;
        for (size_t index = 0; index < name.size() && index < 127u; ++index)
            payload.byte(uint8_t(name[index]));
        payload.byte(0);
        payload.align(4);
        chunk(type, stamp, payload);
    }
};

bool finite(float value) { return std::isfinite(value) && std::fabs(value) < 1000000.0f; }

bool validPoint(const StorylandModelPoint& point) {
    return finite(point.x) && finite(point.y) && finite(point.z);
}

bool isSkinned(const StorylandModelFile& source) {
    return source.modelKind() == StorylandModelKind::PedModel ||
           source.modelKind() == StorylandModelKind::CutsceneModel;
}

std::string exportedName(std::string value) {
    if (value.empty()) return "part";
    for (char& character : value) {
        if (character < 32 || character > 126 || character == '/' || character == '\\')
            character = '_';
    }
    if (value.size() > 100u) value.resize(100u);
    return value;
}

void quatMatrix(const StorylandModelBone& bone, StorylandDffExportFrame& frame) {
    float x = bone.localRotationX;
    float y = bone.localRotationY;
    float z = bone.localRotationZ;
    float w = bone.localRotationW;
    const float length = std::sqrt(x*x + y*y + z*z + w*w);
    if (length > 0.00001f) {
        x /= length; y /= length; z /= length; w /= length;
    } else {
        x = y = z = 0.0f; w = 1.0f;
    }
    frame.matrix[0] = 1.0f - 2.0f*(y*y + z*z);
    frame.matrix[1] = 2.0f*(x*y + z*w);
    frame.matrix[2] = 2.0f*(x*z - y*w);
    frame.matrix[3] = 2.0f*(x*y - z*w);
    frame.matrix[4] = 1.0f - 2.0f*(x*x + z*z);
    frame.matrix[5] = 2.0f*(y*z + x*w);
    frame.matrix[6] = 2.0f*(x*z + y*w);
    frame.matrix[7] = 2.0f*(y*z - x*w);
    frame.matrix[8] = 1.0f - 2.0f*(x*x + y*y);
    frame.matrix[9] = bone.localPosition.x;
    frame.matrix[10] = bone.localPosition.y;
    frame.matrix[11] = bone.localPosition.z;
}

struct FrameMatrix {
    float value[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
};

FrameMatrix compose(const FrameMatrix& first, const FrameMatrix& second) {
    FrameMatrix result;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row) {
            result.value[column*4 + row] = 0.0f;
            for (int index = 0; index < 4; ++index)
                result.value[column*4 + row] += first.value[index*4 + row] * second.value[column*4 + index];
        }
    return result;
}

FrameMatrix localMatrix(const StorylandDffExportFrame& frame) {
    FrameMatrix result;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            result.value[column*4 + row] = frame.matrix[column*3 + row];
    result.value[12] = frame.matrix[9];
    result.value[13] = frame.matrix[10];
    result.value[14] = frame.matrix[11];
    return result;
}

FrameMatrix invertRigid(const FrameMatrix& matrix) {
    FrameMatrix inverse;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            inverse.value[column*4 + row] = matrix.value[row*4 + column];
    for (int row = 0; row < 3; ++row) {
        inverse.value[12 + row] = 0.0f;
        for (int column = 0; column < 3; ++column)
            inverse.value[12 + row] -= inverse.value[column*4 + row] * matrix.value[12 + column];
    }
    return inverse;
}

void writeMatrix(Buffer& result, const FrameMatrix& matrix) {
    for (float value : matrix.value) result.f32(value);
}

bool frameWorldMatrices(const StorylandDffExportScene& scene,
                        std::vector<FrameMatrix>& matrices, std::string& error) {
    matrices.resize(scene.frames.size());
    std::vector<uint8_t> flags(scene.frames.size(), 0);
    auto visit = [&](auto&& self, size_t index) -> bool {
        if (flags[index] == 2) return true;
        if (flags[index] == 1) { error = "Frame hierarchy contains a parent cycle."; return false; }
        flags[index] = 1;
        const auto& frame = scene.frames[index];
        FrameMatrix local = localMatrix(frame);
        if (frame.parent >= 0) {
            if (size_t(frame.parent) >= index && size_t(frame.parent) >= scene.frames.size()) {
                error = "Frame parent index is outside the frame list."; return false;
            }
            if (size_t(frame.parent) >= scene.frames.size() ||
                !self(self, size_t(frame.parent))) return false;
            matrices[index] = compose(matrices[size_t(frame.parent)], local);
        } else {
            matrices[index] = local;
        }
        flags[index] = 2;
        return true;
    };
    for (size_t index = 0; index < scene.frames.size(); ++index)
        if (!visit(visit, index)) return false;
    return true;
}

Buffer frameList(const StorylandDffExportScene& scene, uint32_t stamp) {
    Buffer frames;
    Buffer rows;
    rows.u32(uint32_t(scene.frames.size()));
    for (const auto& frame : scene.frames) {
        for (size_t index = 0; index < 9u; ++index) rows.f32(frame.matrix[index]);
        for (size_t index = 9u; index < 12u; ++index) rows.f32(frame.matrix[index]);
        rows.i32(frame.parent);
        rows.u32(0);
    }
    frames.chunk(RwStruct, stamp, rows);

    for (size_t index = 0; index < scene.frames.size(); ++index) {
        Buffer extensions;
        extensions.namedChunk(RwFrameName, stamp, scene.frames[index].name);
        const auto& frame = scene.frames[index];
        if (scene.hasSkeleton && frame.boneId != 0xFFFFFFFFu) {
            Buffer hanim;
            hanim.u32(0x100u);
            hanim.u32(frame.boneId);
            const bool root = uint32_t(index) == scene.skeletonRoot;
            hanim.u32(root ? uint32_t(scene.skinToFrame.size()) : 0u);
            if (root) {
                hanim.u32(0x3000u);
                hanim.u32(36u);
                for (uint32_t paletteIndex = 0; paletteIndex < scene.skinToFrame.size(); ++paletteIndex) {
                    const uint32_t boneFrame = scene.skinToFrame[paletteIndex];
                    const auto& bone = scene.frames[boneFrame];
                    bool laterSibling = false;
                    for (uint32_t laterIndex = paletteIndex + 1u;
                         laterIndex < scene.skinToFrame.size(); ++laterIndex) {
                        if (scene.frames[scene.skinToFrame[laterIndex]].parent == bone.parent) {
                            laterSibling = true;
                            break;
                        }
                    }
                    const bool nextIsChild = paletteIndex + 1u < scene.skinToFrame.size() &&
                        scene.frames[scene.skinToFrame[paletteIndex + 1u]].parent == int32_t(boneFrame);
                    const uint32_t hierarchyFlags = (laterSibling ? 2u : 0u) |
                        (!nextIsChild && paletteIndex != 0u ? 1u : 0u);
                    hanim.u32(bone.boneId);
                    hanim.u32(paletteIndex);
                    hanim.u32(hierarchyFlags);
                }
            }
            extensions.chunk(RwHAnim, stamp, hanim);
        }
        frames.chunk(RwExtension, stamp, extensions);
    }
    return frames;
}

Buffer textureChunk(const std::string& name, uint32_t stamp) {
    Buffer content;
    Buffer values;
    values.u32(0x1106u);
    content.chunk(RwStruct, stamp, values);
    content.namedChunk(RwString, stamp, name);
    content.namedChunk(RwString, stamp, "");
    content.chunk(RwExtension, stamp, {});
    return content;
}

Buffer materialList(const StorylandDffExportMesh& mesh, uint32_t stamp,
                    const std::vector<std::array<std::string, 2>>& trackNames) {
    Buffer list;
    Buffer header;
    header.u32(uint32_t(mesh.textureNames.size()));
    for (size_t index = 0; index < mesh.textureNames.size(); ++index)
        header.i32(-1);
    list.chunk(RwStruct, stamp, header);
    for (size_t index = 0; index < mesh.textureNames.size(); ++index) {
        Buffer material;
        Buffer structure;
        const uint32_t color = index < mesh.materialColors.size()
            ? mesh.materialColors[index] : 0xFFFFFFFFu;
        structure.u32(0u);
        structure.u32(color);
        structure.u32(0u);
        const std::string& name = mesh.textureNames[index];
        const bool hasTexture = !name.empty() && name[0] != '<';
        structure.u32(hasTexture ? 1u : 0u);
        structure.f32(1.0f);
        structure.f32(1.0f);
        structure.f32(1.0f);
        material.chunk(RwStruct, stamp, structure);
        if (hasTexture) material.chunk(RwTexture, stamp, textureChunk(name, stamp));
        Buffer extensions;
        const bool hasEnvironment = index < mesh.environmentNames.size() &&
            !mesh.environmentNames[index].empty() && mesh.environmentNames[index][0] != '<';
        if (hasEnvironment) {
            Buffer effect;
            effect.u32(2u);
            effect.u32(2u);
            effect.f32(index < mesh.environmentStrengths.size() ?
                std::max(0.0f, std::min(8.0f, mesh.environmentStrengths[index])) : 1.0f);
            effect.u32(0);
            effect.u32(1u);
            effect.chunk(RwTexture, stamp, textureChunk(mesh.environmentNames[index], stamp));
            effect.u32(0u);
            extensions.chunk(RwMatFx, stamp, effect);
        }
        if (index < trackNames.size()) {
            uint32_t mask = 0u;
            Buffer names;
            for (uint32_t channel = 0u; channel < 2u; ++channel) {
                if (trackNames[index][channel].empty()) continue;
                mask |= 1u << channel;
                for (uint32_t letter = 0u; letter < 32u; ++letter)
                    names.byte(letter < trackNames[index][channel].size()
                        ? uint8_t(trackNames[index][channel][letter]) : 0u);
            }
            if (mask) {
                Buffer reference;
                Buffer content;
                content.u32(mask);
                content.bytes(names.data);
                reference.chunk(RwStruct, stamp, content);
                extensions.chunk(RwUVAnimation, stamp, reference);
            }
        }
        material.chunk(RwExtension, stamp, extensions);
        list.chunk(RwMaterial, stamp, material);
    }
    return list;
}

void calculateNormals(StorylandDffExportMesh& mesh) {
    bool needsNormals = false;
    for (const auto& vertex : mesh.vertices) if (!vertex.hasNormal) needsNormals = true;
    if (!needsNormals) return;
    std::vector<std::array<float, 3>> normals(mesh.vertices.size(), {0,0,0});
    for (const auto& face : mesh.faces) {
        const auto& a = mesh.vertices[face.indices[0]];
        const auto& b = mesh.vertices[face.indices[1]];
        const auto& c = mesh.vertices[face.indices[2]];
        const float ax = b.x - a.x, ay = b.y - a.y, az = b.z - a.z;
        const float bx = c.x - a.x, by = c.y - a.y, bz = c.z - a.z;
        const std::array<float,3> normal = {ay*bz-az*by, az*bx-ax*bz, ax*by-ay*bx};
        for (uint32_t index : face.indices)
            for (int axis = 0; axis < 3; ++axis) normals[index][axis] += normal[axis];
    }
    for (size_t index = 0; index < mesh.vertices.size(); ++index) {
        auto& vertex = mesh.vertices[index];
        if (vertex.hasNormal) continue;
        const auto& normal = normals[index];
        const float len = std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
        vertex.nx = len > 1e-10f ? normal[0]/len : 0;
        vertex.ny = len > 1e-10f ? normal[1]/len : 0;
        vertex.nz = len > 1e-10f ? normal[2]/len : 1;
        vertex.hasNormal = true;
    }
}

Buffer skinPlugin(const StorylandDffExportScene& scene,
                  const StorylandDffExportMesh& mesh,
                  const std::vector<FrameMatrix>& frameWorld) {
    Buffer output;
    const size_t count = scene.skinToFrame.size();
    std::set<uint32_t> used;
    uint8_t maximum = 0;
    for (const auto& weights : mesh.weights) {
        maximum = std::max(maximum, uint8_t(weights.influenceCount));
        for (uint32_t index = 0; index < weights.influenceCount; ++index)
            if (weights.influences[index].weight > 0) used.insert(weights.influences[index].boneIndex);
    }
    output.byte(uint8_t(count));
    output.byte(uint8_t(used.size()));
    output.byte(maximum);
    output.byte(0);
    for (uint32_t index : used) output.byte(uint8_t(index));

    for (const auto& weights : mesh.weights) {
        for (uint32_t index = 0; index < 4; ++index)
            output.byte(index < weights.influenceCount ? uint8_t(weights.influences[index].boneIndex) : 0u);
    }
    for (const auto& weights : mesh.weights) {
        for (uint32_t index = 0; index < 4; ++index)
            output.f32(index < weights.influenceCount ? weights.influences[index].weight : 0.0f);
    }
    for (uint32_t frameIndex : scene.skinToFrame)
        writeMatrix(output, invertRigid(frameWorld[frameIndex]));
    output.zeros(12);
    return output;
}

Buffer standardGeometry(const StorylandDffExportScene& scene,
                        StorylandDffExportMesh mesh,
                        const std::vector<FrameMatrix>& frameWorld,
                        const std::vector<std::array<std::string, 2>>& trackNames) {
    calculateNormals(mesh);
    const uint32_t stamp = RwSanAndreas;
    Buffer geometry;
    Buffer structure;
    bool hasColors = false;
    bool secondUV = false;
    for (const auto& color : mesh.colors) if (color.valid) hasColors = true;
    for (const auto& uv : mesh.texcoords) if (uv.hasSecondary) secondUV = true;
    const uint32_t flags = 0x02u | (secondUV ? 0x80u : 0x04u) |
        (hasColors ? 0x08u : 0u) | 0x10u | 0x20u |
        uint32_t(secondUV ? 2u : 1u) << 16u;
    structure.u32(flags);
    structure.u32(uint32_t(mesh.faces.size()));
    structure.u32(uint32_t(mesh.vertices.size()));
    structure.u32(1u);
    if (hasColors) {
        for (const auto& color : mesh.colors) {
            structure.byte(color.valid ? color.red : 255);
            structure.byte(color.valid ? color.green : 255);
            structure.byte(color.valid ? color.blue : 255);
            structure.byte(color.valid ? color.alpha : 255);
        }
    }
    for (const auto& uv : mesh.texcoords) {
        structure.f32(uv.u);
        structure.f32(uv.v);
    }
    if (secondUV) for (const auto& uv : mesh.texcoords) {
        structure.f32(uv.hasSecondary ? uv.secondaryU : uv.u);
        structure.f32(uv.hasSecondary ? uv.secondaryV : uv.v);
    }
    for (const auto& face : mesh.faces) {
        structure.u16(uint16_t(face.indices[2]));
        structure.u16(uint16_t(face.indices[1]));
        structure.u16(uint16_t(face.material));
        structure.u16(uint16_t(face.indices[0]));
    }
    StorylandModelPoint lower = mesh.vertices.front(), upper = lower;
    for (const auto& point : mesh.vertices) {
        lower.x = std::min(lower.x, point.x); upper.x = std::max(upper.x, point.x);
        lower.y = std::min(lower.y, point.y); upper.y = std::max(upper.y, point.y);
        lower.z = std::min(lower.z, point.z); upper.z = std::max(upper.z, point.z);
    }
    const float centerX = (lower.x+upper.x)*0.5f;
    const float centerY = (lower.y+upper.y)*0.5f;
    const float centerZ = (lower.z+upper.z)*0.5f;
    float radiusSquared = 0;
    for (const auto& point : mesh.vertices) {
        const float x=point.x-centerX,y=point.y-centerY,z=point.z-centerZ;
        radiusSquared=std::max(radiusSquared,x*x+y*y+z*z);
    }
    structure.f32(centerX); structure.f32(centerY); structure.f32(centerZ);
    structure.f32(std::sqrt(radiusSquared));
    structure.u32(1u);
    structure.u32(1u);
    for (const auto& point : mesh.vertices) {
        structure.f32(point.x); structure.f32(point.y); structure.f32(point.z);
    }
    for (const auto& point : mesh.vertices) {
        structure.f32(point.nx); structure.f32(point.ny); structure.f32(point.nz);
    }
    geometry.chunk(RwStruct, stamp, structure);
    geometry.chunk(RwMaterialList, stamp, materialList(mesh, stamp, trackNames));
    Buffer extensions;
    if (mesh.skinned) extensions.chunk(RwSkin, stamp, skinPlugin(scene, mesh, frameWorld));
    geometry.chunk(RwExtension, stamp, extensions);
    return geometry;
}

bool validateScene(const StorylandDffExportScene& scene,
                   StorylandDffTarget target, std::string& error) {
    if (scene.frames.empty() || scene.frames.size() > 4096u || scene.meshes.empty()) {
        error = "The model has no complete frame/atomic hierarchy.";
        return false;
    }
    if (scene.hasSkeleton &&
        (scene.skinToFrame.empty() || scene.skinToFrame.size() > (target == StorylandDffTarget::MobileLcs ? 64u : 255u))) {
        error = "The bone palette exceeds the target DFF limit.";
        return false;
    }
    for (uint32_t frameIndex : scene.skinToFrame) {
        if (frameIndex >= scene.frames.size() || scene.frames[frameIndex].boneId == 0xFFFFFFFFu) {
            error = "Skin palette references a missing HAnim bone frame.";
            return false;
        }
    }
    for (const auto& mesh : scene.meshes) {
        if (mesh.frameIndex >= scene.frames.size() || mesh.vertices.empty() || mesh.faces.empty() ||
            mesh.vertices.size() > 65535u || mesh.textureNames.empty() || mesh.textureNames.size() > 65535u ||
            mesh.texcoords.size() != mesh.vertices.size() || mesh.colors.size() != mesh.vertices.size() ||
            mesh.rwUVAnimations.size() != mesh.textureNames.size() ||
            mesh.rslUVAnimations.size() != mesh.textureNames.size()) {
            error = "An atomic contains missing or oversized geometry/material data.";
            return false;
        }
        if (mesh.skinned && (!scene.hasSkeleton || mesh.weights.size() != mesh.vertices.size())) {
            error = "The pedestrian atomic has no complete skin and skeleton.";
            return false;
        }
        if (target == StorylandDffTarget::MobileLcs) {
            for (size_t index = 0; index < mesh.texcoords.size(); ++index) {
                const auto& uv = mesh.texcoords[index];
                if (uv.hasSecondary &&
                    (std::fabs(uv.secondaryU - uv.u) > 0.0001f ||
                     std::fabs(uv.secondaryV - uv.v) > 0.0001f)) {
                    error = "Mobile LCS F00D cannot preserve independent secondary UV coordinates in this writer.";
                    return false;
                }
                if (mesh.skinned && mesh.colors[index].valid &&
                    (mesh.colors[index].red != 255u || mesh.colors[index].green != 255u ||
                     mesh.colors[index].blue != 255u || mesh.colors[index].alpha != 255u)) {
                    error = "Mobile LCS skinned geometry has nonwhite vertex colors not encoded by its native skin packet.";
                    return false;
                }
            }
        }
        for (size_t index = 0; index < mesh.vertices.size(); ++index) {
            if (!validPoint(mesh.vertices[index]) || !finite(mesh.texcoords[index].u) ||
                !finite(mesh.texcoords[index].v)) {
                error = "Vertex or UV values contain invalid floating-point data.";
                return false;
            }
            if (mesh.skinned) {
                const auto& weight = mesh.weights[index];
                if (!weight.valid || weight.influenceCount == 0 || weight.influenceCount > 4) {
                    error = "A pedestrian vertex lacks valid native skin weights.";
                    return false;
                }
                float total = 0;
                for (uint32_t entry = 0; entry < weight.influenceCount; ++entry) {
                    if (!finite(weight.influences[entry].weight) || weight.influences[entry].weight <= 0 ||
                        weight.influences[entry].boneIndex >= scene.skinToFrame.size()) {
                        error = "A pedestrian vertex has invalid skin-palette indices.";
                        return false;
                    }
                    total += weight.influences[entry].weight;
                }
                if (std::fabs(total - 1.0f) > 0.02f) {
                    error = "Skin weights do not sum to one.";
                    return false;
                }
            }
        }
        for (const auto& face : mesh.faces) {
            if (face.material >= mesh.textureNames.size() ||
                face.indices[0] >= mesh.vertices.size() ||
                face.indices[1] >= mesh.vertices.size() ||
                face.indices[2] >= mesh.vertices.size()) {
                error = "A triangle has a material or vertex index outside its atomic.";
                return false;
            }
            if (face.indices[0] == face.indices[1] || face.indices[1] == face.indices[2] ||
                face.indices[0] == face.indices[2]) {
                error = "Degenerate source triangles were detected; the geometry needs repair before conversion.";
                return false;
            }
        }
    }
    return true;
}

// Native LCS geometry uses 16-byte DMA packet units. Each atomic is stored
// in its own frame with its original name and material list.
Buffer nativePacket(const StorylandDffExportMesh& mesh,
                    const StorylandDffExportFace& face,
                    const std::array<float, 3>& scale,
                    const std::array<float, 3>& origin,
                    const std::array<float, 2>& uvScale) {
    Buffer packet;
    const bool skinned = mesh.skinned;
    packet.u32(0x6C018000u); packet.u32(0u); packet.u32(0u);
    packet.u32(3u); packet.u32(0x8003u);
    packet.u32(0x20000000u); packet.u32(0x40404040u);
    packet.u32(0x30000000u); packet.zeros(16);
    packet.u32(0x79008001u | (3u << 16u));
    for (uint32_t index : face.indices) {
        const auto& point = mesh.vertices[index];
        const float values[3] = {point.x, point.y, point.z};
        for (int axis = 0; axis < 3; ++axis) {
            const float quantized = (values[axis] - origin[axis]) / (scale[axis] * NativePositionScale);
            packet.u16(uint16_t(int16_t(std::round(quantized))));
        }
    }
    packet.align(4);
    packet.u32(0x20000000u); packet.u32(0x50505050u);
    packet.u32(0x30000000u); packet.u32(0); packet.u32(0);
    packet.u32(255u); packet.u32(0);
    packet.u32((skinned ? 0x7600C055u : 0x7600C08Du) | (3u << 16u));
    for (uint32_t index : face.indices) {
        const auto& uv = mesh.texcoords[index];
        packet.byte(uint8_t(std::clamp(int(std::round(uv.u * 127.5f / uvScale[0])), 0, 255)));
        packet.byte(uint8_t(std::clamp(int(std::round((1.0f-uv.v) * 127.5f / uvScale[1])), 0, 255)));
    }
    packet.align(4);
    if (!skinned) {
        packet.u32(0x6F0080D3u | (3u << 16u));
        for (uint32_t index : face.indices) {
            const auto& color = mesh.colors[index];
            uint32_t red = (uint32_t(color.red) * 31u + 127u) / 255u;
            uint32_t green = (uint32_t(color.green) * 31u + 127u) / 255u;
            uint32_t blue = (uint32_t(color.blue) * 31u + 127u) / 255u;
            packet.u16(uint16_t(red | (green << 5u) | (blue << 10u) |
                (color.alpha >= 128u ? 0x8000u : 0u)));
        }
        packet.align(4);
    }
    packet.u32((skinned ? 0x6A00802Bu : 0x6A008047u) | (3u << 16u));
    for (uint32_t index : face.indices) {
        const auto& point = mesh.vertices[index];
        packet.byte(uint8_t(int8_t(std::clamp(int(std::round(point.nx * 127.0f)), -127, 127))));
        packet.byte(uint8_t(int8_t(std::clamp(int(std::round(point.ny * 127.0f)), -127, 127))));
        packet.byte(uint8_t(int8_t(std::clamp(int(std::round(point.nz * 127.0f)), -127, 127))));
    }
    packet.align(4);
    if (skinned) {
        packet.u32(0x6C00807Fu | (3u << 16u));
        for (uint32_t index : face.indices) {
            const auto& weights = mesh.weights[index];
            for (uint32_t entry = 0; entry < 4; ++entry) {
                uint32_t packed = 0;
                if (entry < weights.influenceCount) {
                    const auto& influence = weights.influences[entry];
                    float weight = influence.weight;
                    std::memcpy(&packed, &weight, sizeof(packed));
                    packed = (packed & 0xFFFFFF00u) | (influence.boneIndex * 4u);
                }
                packet.u32(packed);
            }
        }
    }
    packet.u32(0x14000006u);
    packet.align(16);
    Buffer result;
    result.u32(0x60000000u | uint32_t(packet.data.size() / 16u));
    result.u32(0); result.u32(0); result.u32(0);
    result.bytes(packet.data);
    return result;
}

bool nativeGeometry(const StorylandDffExportMesh& input,
                    const std::vector<std::array<std::string, 2>>& trackNames,
                    Buffer& geometry, std::string& error) {
    StorylandDffExportMesh mesh = input;
    calculateNormals(mesh);
    if (mesh.faces.size() > 1200u) {
        error = "Mobile LCS native geometry exceeds 1200 material parts per atomic.";
        return false;
    }
    if (mesh.skinned) {
        for (const auto& weights : mesh.weights)
            for (uint32_t index = 0; index < weights.influenceCount; ++index)
                if (weights.influences[index].boneIndex >= 64u) {
                    error = "Mobile LCS skin palettes permit at most 64 bones.";
                    return false;
                }
    }
    std::array<float, 3> lower = {mesh.vertices.front().x, mesh.vertices.front().y, mesh.vertices.front().z};
    std::array<float, 3> upper = lower;
    for (const auto& point : mesh.vertices) {
        lower[0] = std::min(lower[0], point.x); upper[0] = std::max(upper[0], point.x);
        lower[1] = std::min(lower[1], point.y); upper[1] = std::max(upper[1], point.y);
        lower[2] = std::min(lower[2], point.z); upper[2] = std::max(upper[2], point.z);
    }
    std::array<float, 3> origin{};
    std::array<float, 3> scale{};
    float radiusSquared = 0;
    for (int axis = 0; axis < 3; ++axis) {
        origin[axis] = (lower[axis] + upper[axis]) * 0.5f;
        scale[axis] = std::max((upper[axis] - lower[axis]) * 0.5f, 0.000001f) /
            (28672.0f * NativePositionScale);
    }
    for (const auto& point : mesh.vertices) {
        const float dx = point.x - origin[0], dy = point.y - origin[1], dz = point.z - origin[2];
        radiusSquared = std::max(radiusSquared, dx*dx + dy*dy + dz*dz);
    }
    auto bound = [&](float value, int axis) -> int16_t {
        return int16_t(std::round((value - origin[axis]) / (scale[axis] * NativePositionScale)));
    };
    std::vector<Buffer> packets;
    std::vector<std::array<float, 2>> scales;
    packets.reserve(mesh.faces.size());
    scales.reserve(mesh.faces.size());
    for (const auto& face : mesh.faces) {
        std::array<float, 2> uvScale{1,1};
        for (int axis = 0; axis < 2; ++axis) {
            bool positive = false, negative = false;
            float magnitude = 0;
            for (uint32_t index : face.indices) {
                float value = axis == 0 ? mesh.texcoords[index].u : 1.0f - mesh.texcoords[index].v;
                if (value > 0.000001f) positive = true;
                if (value < -0.000001f) negative = true;
                magnitude = std::max(magnitude, std::fabs(value));
            }
            if (positive && negative) {
                error = "Mobile LCS native UVs cross the signed-texture boundary in one triangle.";
                return false;
            }
            uvScale[axis] = (negative ? -1.0f : 1.0f) * std::max(1.0f, magnitude / 2.0f);
            for (uint32_t index : face.indices) {
                float uv = axis == 0 ? mesh.texcoords[index].u : 1.0f - mesh.texcoords[index].v;
                if (uv * 127.5f / uvScale[axis] > 255.0001f ||
                    uv * 127.5f / uvScale[axis] < -0.0001f) {
                    error = "UV values exceed the Mobile LCS quantization range.";
                    return false;
                }
            }
        }
        scales.push_back(uvScale);
        packets.push_back(nativePacket(mesh, face, scale, origin, uvScale));
    }
    const size_t headerSize = 64u + packets.size() * 48u;
    size_t packetBytes = 0;
    for (const auto& packet : packets) packetBytes += packet.data.size();
    if (headerSize > 65535u || headerSize + packetBytes >= (1u << 20u) ||
        mesh.faces.size() * 3u > 65535u) {
        error = "Mobile native geometry exceeds its packet or 16-bit vertex header limits.";
        return false;
    }
    Buffer structure;
    for (float value : origin) structure.f32(value);
    structure.f32(std::sqrt(radiusSquared));
    structure.u32(uint32_t(headerSize + packetBytes) | (uint32_t(packets.size()) << 20u));
    structure.u32(mesh.skinned ? 0x17u : 0x0Fu);
    structure.u16(uint16_t(mesh.faces.size() * 3u));
    structure.u16(uint16_t(headerSize));
    for (int axis = 0; axis < 3; ++axis) structure.u16(uint16_t(bound(lower[axis],axis)));
    for (int axis = 0; axis < 3; ++axis) structure.u16(uint16_t(bound(upper[axis],axis)));
    for (float value : scale) structure.f32(value);
    for (float value : origin) structure.f32(value);
    uint32_t packetOffset = 0;
    for (size_t index = 0; index < mesh.faces.size(); ++index) {
        const auto& face = mesh.faces[index];
        StorylandModelPoint center{};
        for (uint32_t vertex : face.indices) {
            center.x += mesh.vertices[vertex].x / 3.0f;
            center.y += mesh.vertices[vertex].y / 3.0f;
            center.z += mesh.vertices[vertex].z / 3.0f;
        }
        float radius = 0;
        std::array<float,3> triMin{mesh.vertices[face.indices[0]].x, mesh.vertices[face.indices[0]].y, mesh.vertices[face.indices[0]].z};
        auto triMax = triMin;
        for (uint32_t vertex : face.indices) {
            const auto& point = mesh.vertices[vertex];
            const float deltaX=point.x-center.x,deltaY=point.y-center.y,deltaZ=point.z-center.z;
            radius=std::max(radius,std::sqrt(deltaX*deltaX+deltaY*deltaY+deltaZ*deltaZ));
            const float coords[3] = {point.x,point.y,point.z};
            for (int axis = 0; axis < 3; ++axis) {
                triMin[axis]=std::min(triMin[axis],coords[axis]);
                triMax[axis]=std::max(triMax[axis],coords[axis]);
            }
        }
        structure.f32(center.x); structure.f32(center.y); structure.f32(center.z);
        structure.f32(radius);
        structure.f32(scales[index][0]); structure.f32(scales[index][1]);
        structure.u32(mesh.skinned ? 0x07u : 0x19u);
        structure.u32(packetOffset);
        structure.u16(1u);
        structure.u16(uint16_t(face.material));
        for (int axis=0;axis<3;++axis)structure.u16(uint16_t(bound(triMin[axis],axis)));
        for (int axis=0;axis<3;++axis)structure.u16(uint16_t(bound(triMax[axis],axis)));
        packetOffset += uint32_t(packets[index].data.size());
    }
    for (const auto& packet : packets) structure.bytes(packet.data);
    // Mobile geometry deliberately embeds its native packet header in the
    // F00D payload, followed by the material list and the extension.
    Buffer result = structure;
    result.chunk(RwMaterialList, RwMobileLcs, materialList(mesh, RwMobileLcs, trackNames));
    result.chunk(RwExtension, RwMobileLcs, {});
    if (result.data.size() < 0x100u) result.zeros(0x100u - result.data.size());
    geometry = std::move(result);
    return true;
}


struct MobileVertex {
    StorylandModelPoint point;
    StorylandModelTexcoord uv;
    StorylandModelPrelight color;
    StorylandModelSkinWeights skin;
};

MobileVertex blendMobileVertex(const MobileVertex& first, const MobileVertex& second, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    MobileVertex result;
    result.point.x = first.point.x * inv + second.point.x * t;
    result.point.y = first.point.y * inv + second.point.y * t;
    result.point.z = first.point.z * inv + second.point.z * t;
    result.point.nx = first.point.nx * inv + second.point.nx * t;
    result.point.ny = first.point.ny * inv + second.point.ny * t;
    result.point.nz = first.point.nz * inv + second.point.nz * t;
    const float normalLength = std::sqrt(result.point.nx * result.point.nx +
        result.point.ny * result.point.ny + result.point.nz * result.point.nz);
    if (normalLength > 0.000001f) {
        result.point.nx /= normalLength;
        result.point.ny /= normalLength;
        result.point.nz /= normalLength;
    }
    result.point.hasNormal = first.point.hasNormal && second.point.hasNormal;
    result.uv.u = first.uv.u * inv + second.uv.u * t;
    result.uv.v = first.uv.v * inv + second.uv.v * t;
    result.uv.secondaryU = first.uv.secondaryU * inv + second.uv.secondaryU * t;
    result.uv.secondaryV = first.uv.secondaryV * inv + second.uv.secondaryV * t;
    result.uv.hasSecondary = first.uv.hasSecondary || second.uv.hasSecondary;
    result.color.valid = first.color.valid || second.color.valid;
    result.color.red = uint8_t(std::round(first.color.red * inv + second.color.red * t));
    result.color.green = uint8_t(std::round(first.color.green * inv + second.color.green * t));
    result.color.blue = uint8_t(std::round(first.color.blue * inv + second.color.blue * t));
    result.color.alpha = uint8_t(std::round(first.color.alpha * inv + second.color.alpha * t));
    result.color.encoding = StorylandPrelightEncoding::None;
    result.color.fileOffset = 0xFFFFFFFFu;
    result.skin = {};
    if (first.skin.valid || second.skin.valid) {
        std::map<uint32_t, float> influences;
        if (first.skin.valid) for (uint32_t i = 0; i < first.skin.influenceCount; ++i)
            influences[first.skin.influences[i].boneIndex] += first.skin.influences[i].weight * inv;
        if (second.skin.valid) for (uint32_t i = 0; i < second.skin.influenceCount; ++i)
            influences[second.skin.influences[i].boneIndex] += second.skin.influences[i].weight * t;
        std::vector<std::pair<uint32_t, float>> ranked(influences.begin(), influences.end());
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
        float total = 0;
        for (size_t i = 0; i < ranked.size() && i < 4u; ++i)
            if (ranked[i].second > 0.000001f) total += ranked[i].second;
        if (total > 0.000001f) {
            result.skin.valid = true;
            for (size_t i = 0; i < ranked.size() && i < 4u; ++i) {
                if (ranked[i].second <= 0.000001f) break;
                auto& influence = result.skin.influences[result.skin.influenceCount++];
                influence.boneIndex = ranked[i].first;
                influence.rawMatrixIndex = ranked[i].first;
                influence.weight = ranked[i].second / total;
            }
        }
    }
    return result;
}

float mobileUvAxis(const MobileVertex& vertex, int axis) {
    return axis == 0 ? vertex.uv.u : (1.0f - vertex.uv.v);
}

std::vector<MobileVertex> clipMobileUv(const std::vector<MobileVertex>& polygon, int axis, int side) {
    std::vector<MobileVertex> clipped;
    if (polygon.empty()) return clipped;
    clipped.reserve(polygon.size() + 2);
    MobileVertex previous = polygon.back();
    float previousValue = mobileUvAxis(previous, axis) * float(side);
    for (const auto& current : polygon) {
        const float currentValue = mobileUvAxis(current, axis) * float(side);
        if ((previousValue >= 0.0f) != (currentValue >= 0.0f)) {
            const float t = previousValue / (previousValue - currentValue);
            MobileVertex vertex = blendMobileVertex(previous, current, t);
            if (axis == 0) vertex.uv.u = 0.0f;
            else vertex.uv.v = 1.0f;
            clipped.push_back(std::move(vertex));
        }
        if (currentValue >= 0.0f) clipped.push_back(current);
        previous = current;
        previousValue = currentValue;
    }
    return clipped;
}

bool mobileTriangleHasArea(const MobileVertex& a, const MobileVertex& b,
                           const MobileVertex& c) {
    const float ux = b.point.x - a.point.x, uy = b.point.y - a.point.y, uz = b.point.z - a.point.z;
    const float vx = c.point.x - a.point.x, vy = c.point.y - a.point.y, vz = c.point.z - a.point.z;
    const float x = uy * vz - uz * vy, y = uz * vx - ux * vz, z = ux * vy - uy * vx;
    return (x*x + y*y + z*z) > 1.0e-16f;
}

bool splitMobileGeometry(const StorylandDffExportMesh& source,
                         std::vector<StorylandDffExportMesh>& result,
                         std::string& error) {
    result.clear();
    const size_t maxFaces = 300u;
    const auto newMesh = [&]() {
        StorylandDffExportMesh mesh;
        mesh.name = source.name;
        mesh.frameIndex = source.frameIndex;
        mesh.skinned = source.skinned;
        mesh.textureNames = source.textureNames;
        mesh.materialColors = source.materialColors;
        mesh.environmentNames = source.environmentNames;
        mesh.environmentStrengths = source.environmentStrengths;
        mesh.rwUVAnimations = source.rwUVAnimations;
        mesh.rslUVAnimations = source.rslUVAnimations;
        result.push_back(std::move(mesh));
    };
    newMesh();
    for (const auto& face : source.faces) {
        std::vector<MobileVertex> original;
        original.reserve(3);
        for (uint32_t vertexIndex : face.indices) {
            MobileVertex vertex;
            vertex.point = source.vertices[vertexIndex];
            vertex.uv = source.texcoords[vertexIndex];
            vertex.color = source.colors[vertexIndex];
            if (source.skinned) vertex.skin = source.weights[vertexIndex];
            original.push_back(std::move(vertex));
        }
        std::vector<std::vector<MobileVertex>> polygons{original};
        for (int axis = 0; axis < 2; ++axis) {
            std::vector<std::vector<MobileVertex>> next;
            for (const auto& polygon : polygons) {
                float minimum = mobileUvAxis(polygon.front(), axis);
                float maximum = minimum;
                for (const auto& vertex : polygon) {
                    const float value = mobileUvAxis(vertex, axis);
                    minimum = std::min(minimum, value);
                    maximum = std::max(maximum, value);
                }
                if (minimum >= 0.0f || maximum <= 0.0f) {
                    next.push_back(polygon);
                } else {
                    for (int side : {1, -1}) {
                        auto clipped = clipMobileUv(polygon, axis, side);
                        if (clipped.size() >= 3u) next.push_back(std::move(clipped));
                    }
                }
            }
            polygons = std::move(next);
        }
        for (const auto& polygon : polygons) {
            for (size_t index = 1u; index + 1u < polygon.size(); ++index) {
                if (!mobileTriangleHasArea(polygon.front(), polygon[index], polygon[index+1])) continue;
                if (result.back().faces.size() >= maxFaces) newMesh();
                StorylandDffExportMesh& mesh = result.back();
                const uint32_t base = uint32_t(mesh.vertices.size());
                const MobileVertex* corners[3] = {&polygon.front(), &polygon[index], &polygon[index+1]};
                for (const auto* corner : corners) {
                    mesh.vertices.push_back(corner->point);
                    mesh.texcoords.push_back(corner->uv);
                    mesh.colors.push_back(corner->color);
                    if (mesh.skinned) mesh.weights.push_back(corner->skin);
                }
                mesh.faces.push_back({{base, base+1u, base+2u}, face.material});
            }
        }
    }
    result.erase(std::remove_if(result.begin(), result.end(),
                [](const StorylandDffExportMesh& mesh) { return mesh.faces.empty(); }), result.end());
    if (result.empty()) { error = "No valid Mobile LCS triangles after native UV segmentation."; return false; }
    return true;
}

bool dffSceneFromModel(const StorylandModelFile& source,
                       StorylandDffExportScene& scene, std::string& error) {
    scene = {};
    const auto& points = source.previewPoints();
    const auto& triangles = source.previewTriangles();
    const auto& textures = source.previewMaterialTextureNames();
    const auto& uvs = source.previewTexcoords();
    const auto& colors = source.previewPrelights();
    const auto& weights = source.previewSkinWeights();
    const auto& bones = source.armatureBones();
    const auto& environments = source.previewMaterialEnvironmentNames();
    const auto& strengths = source.previewMaterialEffects();
    const auto& rwUVAnimations = source.previewMaterialUVAnimations();
    const auto& rslUVAnimations = source.previewRslUVAnimations();
    const auto materialColors = source.previewMaterialColors();
    if (points.empty() || triangles.empty()) {
        error = "No decoded MDL geometry is available for conversion.";
        return false;
    }
    if (uvs.size() != points.size()) {
        error = "The source MDL has no complete vertex UV stream.";
        return false;
    }
    const bool vehicle = source.modelKind() == StorylandModelKind::VehicleModel;
    const bool pedestrian = isSkinned(source);
    if (source.isManhuntModel()) {
        error = "Manhunt MDLs require their own skeleton/geometry conversion; this writer is for GTA Stories.";
        return false;
    }
    StorylandDffExportFrame root;
    root.name = vehicle ? "chassis_dummy" : pedestrian ? "Root" : "Root";
    scene.frames.push_back(root);
    scene.hasSkeleton = pedestrian;
    std::vector<uint32_t> sourceBoneToPalette(bones.size(), 0xFFFFFFFFu);
    if (pedestrian) {
        if (bones.empty() || bones.size() > 512u || weights.size() != points.size()) {
            error = "The clump has no complete native skeleton and skin-weight data.";
            return false;
        }
        std::map<uint32_t, uint32_t> boneIdToFrame;
        std::vector<uint32_t> sourceBoneToFrame(bones.size(), 0xFFFFFFFFu);
        std::vector<size_t> uniqueBones;
        for (size_t index = 0; index < bones.size(); ++index) {
            const auto& bone = bones[index];
            if (!bone.hasLocalPosition || !bone.hasLocalRotation || !validPoint(bone.localPosition)) {
                error = "The native skeleton is missing a verified local bind transform.";
                return false;
            }
            if (bone.boneId != 0xFFFFFFFFu) {
                const auto existing = boneIdToFrame.find(bone.boneId);
                if (existing != boneIdToFrame.end()) {
                    // Repeated native skeleton records must describe the very same bone.
                    const auto& previous = bones[uniqueBones[size_t(existing->second - 1u)]];
                    auto similar = [](float a, float b) { return std::fabs(a-b) < 0.0001f; };
                    if (previous.name != bone.name || previous.parentIndex != bone.parentIndex ||
                        !similar(previous.localPosition.x, bone.localPosition.x) ||
                        !similar(previous.localPosition.y, bone.localPosition.y) ||
                        !similar(previous.localPosition.z, bone.localPosition.z) ||
                        !similar(previous.localRotationX, bone.localRotationX) ||
                        !similar(previous.localRotationY, bone.localRotationY) ||
                        !similar(previous.localRotationZ, bone.localRotationZ) ||
                        !similar(previous.localRotationW, bone.localRotationW)) {
                        error = "Native skeleton has conflicting duplicate HAnim IDs; exporting would damage the rig.";
                        return false;
                    }
                    sourceBoneToFrame[index] = existing->second;
                    continue;
                }
            }
            StorylandDffExportFrame frame;
            frame.name = exportedName(bone.name);
            frame.boneId = bone.boneId;
            quatMatrix(bone, frame);
            const uint32_t frameIndex = uint32_t(scene.frames.size());
            sourceBoneToFrame[index] = frameIndex;
            uniqueBones.push_back(index);
            scene.frames.push_back(frame);
            if (frame.boneId != 0xFFFFFFFFu) boneIdToFrame.emplace(frame.boneId, frameIndex);
        }
        for (size_t index = 0; index < bones.size(); ++index) {
            const uint32_t frameIndex = sourceBoneToFrame[index];
            if (frameIndex == 0xFFFFFFFFu) {
                error = "Native skeleton has a missing parent frame.";
                return false;
            }
            const auto& bone = bones[index];
            if (bone.parentIndex != 0xFFFFFFFFu && bone.parentIndex >= bones.size()) {
                error = "Native skeleton contains a missing parent.";
                return false;
            }
            if (bone.parentIndex == 0xFFFFFFFFu) scene.frames[frameIndex].parent = 0;
            else {
                const auto parentFrame = sourceBoneToFrame[bone.parentIndex];
                if (parentFrame == frameIndex) {
                    error = "Native skeleton has a self-parenting bone.";
                    return false;
                }
                scene.frames[frameIndex].parent = int32_t(parentFrame);
            }
        }
        // SA's HAnim hierarchy must use preorder bone indices, even when the
        // native Leeds frame array is not ordered by parent. Rebuild the
        // physical frame list and its skin palette as one coherent tree.
        const std::vector<StorylandDffExportFrame> unorderedFrames = scene.frames;
        std::vector<uint32_t> orderedFrames;
        std::vector<uint8_t> visited(unorderedFrames.size(), 0u);
        auto walk = [&](auto&& self, uint32_t frameIndex) -> bool {
            if (frameIndex >= unorderedFrames.size() || visited[frameIndex] != 0u) {
                error = "Native skeleton contains a cycle or a shared parent frame.";
                return false;
            }
            visited[frameIndex] = 1u;
            orderedFrames.push_back(frameIndex);
            for (uint32_t child = 0u; child < unorderedFrames.size(); ++child) {
                if (unorderedFrames[child].parent == int32_t(frameIndex) &&
                    !self(self, child)) return false;
            }
            return true;
        };
        if (!walk(walk, 0u) || orderedFrames.size() != unorderedFrames.size()) {
            error = "Native skeleton is disconnected from the root frame.";
            return false;
        }
        std::vector<uint32_t> orderedIndex(unorderedFrames.size(), 0xFFFFFFFFu);
        for (uint32_t newIndex = 0u; newIndex < orderedFrames.size(); ++newIndex)
            orderedIndex[orderedFrames[newIndex]] = newIndex;
        scene.frames.clear();
        scene.frames.reserve(unorderedFrames.size());
        for (uint32_t originalIndex : orderedFrames) {
            StorylandDffExportFrame frame = unorderedFrames[originalIndex];
            if (frame.parent >= 0) frame.parent = int32_t(orderedIndex[size_t(frame.parent)]);
            scene.frames.push_back(std::move(frame));
        }
        for (uint32_t& sourceFrame : sourceBoneToFrame)
            if (sourceFrame != 0xFFFFFFFFu) sourceFrame = orderedIndex[sourceFrame];
        uint32_t roots = 0u;
        for (uint32_t frameIndex = 0u; frameIndex < scene.frames.size(); ++frameIndex) {
            const auto& frame = scene.frames[frameIndex];
            if (frame.boneId == 0xFFFFFFFFu) continue;
            if (frame.parent < 0 || scene.frames[size_t(frame.parent)].boneId == 0xFFFFFFFFu) {
                scene.skeletonRoot = frameIndex;
                ++roots;
            }
            scene.skinToFrame.push_back(frameIndex);
        }
        if (roots != 1u || scene.skinToFrame.empty()) {
            error = "The native pedestrian contains multiple HAnim roots or no HAnim root.";
            return false;
        }
        // A real skin palette starts at the tagged skeleton root and traverses
        // its children depth-first. Untagged intermediate frames are rejected.
        if (scene.skinToFrame.front() != scene.skeletonRoot) {
            error = "Pedestrian HAnim bones contain an untagged intermediary frame.";
            return false;
        }
        for (size_t index = 1u; index < scene.skinToFrame.size(); ++index) {
            const int32_t parent = scene.frames[scene.skinToFrame[index]].parent;
            if (parent < 0 || scene.frames[size_t(parent)].boneId == 0xFFFFFFFFu) {
                error = "Pedestrian skin contains a disconnected HAnim frame.";
                return false;
            }
        }
        for (size_t oldIndex = 0; oldIndex < bones.size(); ++oldIndex) {
            const uint32_t frameIndex = sourceBoneToFrame[oldIndex];
            if (frameIndex == 0xFFFFFFFFu) continue;
            auto palette = std::find(scene.skinToFrame.begin(), scene.skinToFrame.end(), frameIndex);
            if (palette != scene.skinToFrame.end())
                sourceBoneToPalette[oldIndex] = uint32_t(palette - scene.skinToFrame.begin());
        }
    }
    // Keep each rigid vehicle atomic on a separate, named frame. A part name
    // and pivot are required: a flattened preview is not a valid vehicle clump.
    struct Group {
        std::string name;
        StorylandModelPoint pivot;
        std::vector<size_t> faces;
        uint32_t frame = 0;
    };
    std::vector<Group> groups;
    std::map<std::string, size_t> groupIndices;
    for (size_t index = 0; index < triangles.size(); ++index) {
        const auto& face = triangles[index];
        if (face.a >= points.size() || face.b >= points.size() || face.c >= points.size()) {
            error = "A source MDL triangle references a missing vertex.";
            return false;
        }
        std::string name = vehicle ? face.vehiclePart : pedestrian ? "ped_geometry" : "geometry";
        if (vehicle && name.empty()) {
            error = "Vehicle atomic frame names are missing. Cannot export a flattened vehicle.";
            return false;
        }
        if (vehicle && face.damagedPart) name += "_dam";
        const StorylandModelPoint pivot = vehicle ? face.vehiclePivot : StorylandModelPoint{};
        std::ostringstream key;
        key << name;
        if (vehicle) {
            // Distinct wheel/body instances may use the same name in different
            // hierarchy branches, so their pivots are part of the identity.
            key << ':' << std::round(pivot.x*1000.0f) << ':'
                << std::round(pivot.y*1000.0f) << ':' << std::round(pivot.z*1000.0f);
        }
        const auto found = groupIndices.find(key.str());
        if (found == groupIndices.end()) {
            groupIndices[key.str()] = groups.size();
            Group group;
            group.name = exportedName(name);
            group.pivot = pivot;
            groups.push_back(group);
        }
        groups[groupIndices[key.str()]].faces.push_back(index);
    }
    for (auto& group : groups) {
        if (vehicle) {
            StorylandDffExportFrame frame;
            frame.name = group.name;
            frame.parent = 0;
            frame.matrix[9] = group.pivot.x;
            frame.matrix[10] = group.pivot.y;
            frame.matrix[11] = group.pivot.z;
            group.frame = uint32_t(scene.frames.size());
            scene.frames.push_back(frame);
        }
        // A geometry can only address 65535 vertices; large atomics become
        // multiple geometry chunks on the same native frame.
        size_t cursor = 0;
        while (cursor < group.faces.size()) {
            StorylandDffExportMesh mesh;
            mesh.name = group.name;
            mesh.frameIndex = group.frame;
            mesh.skinned = pedestrian;
            std::map<uint32_t, uint32_t> indices;
            std::map<uint32_t, uint32_t> materials;
            const auto appendVertex = [&](uint32_t oldIndex, auto& destination) -> uint32_t {
                auto found = indices.find(oldIndex);
                if (found != indices.end()) return found->second;
                const uint32_t index = uint32_t(destination.vertices.size());
                indices[oldIndex] = index;
                StorylandModelPoint point = points[oldIndex];
                if (vehicle) {
                    point.x -= group.pivot.x;
                    point.y -= group.pivot.y;
                    point.z -= group.pivot.z;
                }
                destination.vertices.push_back(point);
                destination.texcoords.push_back(uvs[oldIndex]);
                destination.colors.push_back(oldIndex < colors.size() ? colors[oldIndex] : StorylandModelPrelight{});
                if (pedestrian) {
                    StorylandModelSkinWeights copy = weights[oldIndex];
                    if (copy.valid) {
                        for (uint32_t influence=0; influence<copy.influenceCount; ++influence) {
                            auto& item = copy.influences[influence];
                            const uint32_t oldIndex = item.boneIndex;
                            if (oldIndex >= sourceBoneToPalette.size() ||
                                sourceBoneToPalette[oldIndex] == 0xFFFFFFFFu) {
                                error = "A native skin weight references an untagged or missing HAnim bone.";
                                return 0xFFFFFFFFu;
                            }
                            item.boneIndex = sourceBoneToPalette[oldIndex];
                        }
                    }
                    destination.weights.push_back(copy);
                }
                return index;
            };
            while (cursor < group.faces.size()) {
                const auto& face = triangles[group.faces[cursor]];
                const size_t needed = (indices.count(face.a) ? 0u : 1u) +
                    (indices.count(face.b) ? 0u : 1u) + (indices.count(face.c) ? 0u : 1u);
                if (mesh.vertices.size() + needed > 65535u) break;
                if (face.materialIndex >= textures.size()) {
                    error = "A source material index is missing from the native material list.";
                    return false;
                }
                uint32_t material = 0;
                auto materialIt = materials.find(face.materialIndex);
                if (materialIt == materials.end()) {
                    material = uint32_t(mesh.textureNames.size());
                    materials[face.materialIndex] = material;
                    mesh.textureNames.push_back(textures[face.materialIndex]);
                    mesh.materialColors.push_back(face.materialIndex < materialColors.size() ?
                        materialColors[face.materialIndex] : 0xFFFFFFFFu);
                    mesh.environmentNames.push_back(face.materialIndex < environments.size() ?
                        environments[face.materialIndex] : std::string{});
                    mesh.environmentStrengths.push_back(face.materialIndex < strengths.size() ?
                        strengths[face.materialIndex] : 0.0f);
                    mesh.rwUVAnimations.push_back(face.materialIndex < rwUVAnimations.size() ?
                        rwUVAnimations[face.materialIndex] : StorylandUVMaterialAnimation{});
                    mesh.rslUVAnimations.push_back(face.materialIndex < rslUVAnimations.size() ?
                        rslUVAnimations[face.materialIndex] : StorylandRslUV::Material{});
                } else material = materialIt->second;
                const uint32_t a = appendVertex(face.a, mesh);
                const uint32_t b = appendVertex(face.b, mesh);
                const uint32_t c = appendVertex(face.c, mesh);
                if (a == 0xFFFFFFFFu || b == 0xFFFFFFFFu || c == 0xFFFFFFFFu) return false;
                if (a == b || a == c || b == c) { error = "Degenerate MDL triangle."; return false; }
                mesh.faces.push_back({{a,b,c}, material});
                ++cursor;
            }
            if (mesh.faces.empty()) {
                error = "An atomic cannot be split into a valid 16-bit geometry.";
                return false;
            }
            scene.meshes.push_back(std::move(mesh));
        }
    }
    if (vehicle) {
        const auto wheels = source.vehicleWheelFrames(true);
        std::set<std::string> known;
        for (const auto& frame : scene.frames) known.insert(frame.name);
        for (const auto& wheel : wheels) {
            if (known.find(wheel.name) != known.end()) continue;
            StorylandDffExportFrame frame;
            frame.name = exportedName(wheel.name);
            frame.parent = 0;
            frame.matrix[9] = wheel.pivot.x;
            frame.matrix[10] = wheel.pivot.y;
            frame.matrix[11] = wheel.pivot.z;
            scene.frames.push_back(frame);
            known.insert(frame.name);
        }
        // Some native Leeds cars store only three wheel pivots. The fourth
        // front wheel is mirrored across the chassis centreline by the retail
        // vehicle system; write its actual derived pivot for San Andreas.
        if (known.count("wheel_rf_dummy") == 0u &&
            known.count("wheel_lf_dummy") && known.count("wheel_lb_dummy") &&
            known.count("wheel_rb_dummy")) {
            auto findWheel = [&](const char* name) -> const StorylandVehicleWheelFrame* {
                for (const auto& wheel : wheels) if (wheel.name == name) return &wheel;
                return nullptr;
            };
            const auto* leftFront = findWheel("wheel_lf_dummy");
            const auto* leftRear = findWheel("wheel_lb_dummy");
            const auto* rightRear = findWheel("wheel_rb_dummy");
            if (leftFront && leftRear && rightRear &&
                std::fabs(leftRear->pivot.x + rightRear->pivot.x) < 0.025f &&
                std::fabs(leftRear->pivot.y - rightRear->pivot.y) < 0.025f &&
                std::fabs(leftRear->pivot.z - rightRear->pivot.z) < 0.025f) {
                StorylandDffExportFrame frame;
                frame.name = "wheel_rf_dummy";
                frame.parent = 0;
                frame.matrix[9] = -leftFront->pivot.x;
                frame.matrix[10] = leftFront->pivot.y;
                frame.matrix[11] = leftFront->pivot.z;
                scene.frames.push_back(frame);
            }
        }
    }
    return true;
}



// Native RSL UV material transforms are converted into the RenderWare UVAnim
// format during DFF export. No material indices are shared between atomics.
StorylandUVAnimationTrack convertRslTrack(const StorylandRslUV::Track& source) {
    StorylandUVAnimationTrack converted;
    converted.interpolator = 0x1C0u;
    converted.duration = source.duration;
    converted.nodeToChannel.fill(2u);
    auto addFrame = [&](const StorylandRslUV::Keyframe& key) {
        const float cs = std::cos(key.rotation);
        const float sn = std::sin(key.rotation);
        const float a = cs * key.scaleU;
        const float b = sn * key.scaleU;
        const float c = cs * key.shear - sn * key.scaleV;
        const float d = sn * key.shear + cs * key.scaleV;
        StorylandUVKeyframe frame;
        frame.time = key.seconds;
        frame.parameters = {{a, b, c, d,
            key.offsetU + 0.5f - 0.5f * (a + c),
            key.offsetV + 0.5f - 0.5f * (b + d)}};
        converted.frames.push_back(frame);
    };
    if (source.frames.empty()) return converted;
    addFrame(source.frames.front());
    for (size_t frameIndex = 1; frameIndex < source.frames.size(); ++frameIndex) {
        const auto& previous = source.frames[frameIndex - 1u];
        const auto& following = source.frames[frameIndex];
        float delta = following.rotation - previous.rotation;
        constexpr float pi = 3.14159265358979323846f;
        while (delta > pi) delta -= 2.0f * pi;
        while (delta < -pi) delta += 2.0f * pi;
        const size_t steps = size_t(std::clamp(int(std::ceil(std::fabs(delta) / 0.08f)), 1, 64));
        for (size_t step = 1u; step < steps; ++step) {
            const float factor = float(step) / float(steps);
            StorylandRslUV::Keyframe intermediate;
            const auto lerp = [factor](float a, float b) { return a + (b - a) * factor; };
            intermediate.seconds = lerp(previous.seconds, following.seconds);
            intermediate.offsetU = lerp(previous.offsetU, following.offsetU);
            intermediate.offsetV = lerp(previous.offsetV, following.offsetV);
            intermediate.scaleU = lerp(previous.scaleU, following.scaleU);
            intermediate.scaleV = lerp(previous.scaleV, following.scaleV);
            intermediate.rotation = previous.rotation + delta * factor;
            intermediate.shear = lerp(previous.shear, following.shear);
            addFrame(intermediate);
        }
        addFrame(following);
    }
    return converted;
}

bool buildUVAnimationDictionary(const StorylandDffExportScene& scene,
    StorylandDffTarget target, std::vector<std::vector<std::array<std::string, 2>>>& names,
    Buffer& dictionary, std::string& error) {
    names.clear();
    uint32_t count = 0u;
    Buffer animations;
    for (size_t meshIndex = 0; meshIndex < scene.meshes.size(); ++meshIndex) {
        const auto& mesh = scene.meshes[meshIndex];
        names.emplace_back(mesh.textureNames.size());
        for (size_t materialIndex = 0; materialIndex < mesh.textureNames.size(); ++materialIndex) {
            for (size_t channel = 0u; channel < 2u; ++channel) {
                const bool hasRsl = mesh.rslUVAnimations[materialIndex].animated(channel);
                const bool hasRw = mesh.rwUVAnimations[materialIndex].animated(channel);
                if (!hasRsl && !hasRw) continue;
                if (target == StorylandDffTarget::MobileLcs) {
                    error = "Animated RSL/RW UV materials need an LCS Mobile animation dictionary loader. No animation data was discarded.";
                    return false;
                }
                if (hasRsl && hasRw) {
                    error = "Both native RSL and RenderWare UV animations target the same material channel.";
                    return false;
                }
                if (hasRw && !mesh.rwUVAnimations[materialIndex].additionalChannels[channel].empty()) {
                    error = "This material combines several RenderWare UV tracks. A single-track export would discard their composition.";
                    return false;
                }
                StorylandUVAnimationTrack track = hasRsl ?
                    convertRslTrack(mesh.rslUVAnimations[materialIndex].channels[channel]) :
                    mesh.rwUVAnimations[materialIndex].channels[channel];
                if (!track.nodes.empty()) {
                    track.frames = track.selectedFrames();
                    track.nodes.clear();
                    track.selectedNode = 0u;
                }
                if (track.frames.empty() || track.frames.size() > 262144u ||
                    !std::isfinite(track.duration) || track.duration < 0.0f ||
                    (track.interpolator != 0x1C0u && track.interpolator != 0x1C1u)) {
                    error = "A material contains an invalid UV animation track.";
                    return false;
                }
                std::string name = "SL" + std::to_string(meshIndex) + "M" +
                    std::to_string(materialIndex) + "C" + std::to_string(channel);
                if (name.size() > 31u) { error = "Too many UV-animated materials for RenderWare track names."; return false; }
                names.back()[materialIndex][channel] = name;
                track.name = name;
                track.nodeToChannel.fill(2u);
                track.nodeToChannel[0] = uint32_t(channel);
                Buffer animation;
                animation.u32(0x100u);
                animation.u32(track.interpolator);
                animation.u32(uint32_t(track.frames.size()));
                animation.u32(0u);
                animation.f32(track.duration);
                animation.u32(0u);
                for (size_t letter = 0u; letter < 32u; ++letter)
                    animation.byte(letter < name.size() ? uint8_t(name[letter]) : 0u);
                for (uint32_t nodeChannel : track.nodeToChannel) animation.u32(nodeChannel);
                float previousTime = -1.0f;
                for (size_t keyIndex = 0; keyIndex < track.frames.size(); ++keyIndex) {
                    const auto& key = track.frames[keyIndex];
                    if (!std::isfinite(key.time) || key.time < previousTime ||
                        key.time < 0.0f || key.time > track.duration) {
                        error = "UV animation frame times are invalid or unordered.";
                        return false;
                    }
                    previousTime = key.time;
                    animation.f32(key.time);
                    for (float value : key.parameters) {
                        if (!std::isfinite(value)) {
                            error = "UV animation contains non-finite transform parameters.";
                            return false;
                        }
                        animation.f32(value);
                    }
                    animation.u32(keyIndex ? uint32_t(keyIndex - 1u) : 0xFFFFFFFFu);
                }
                animations.chunk(RwAnimation, RwSanAndreas, animation);
                ++count;
            }
        }
    }
    if (count) {
        Buffer payload;
        Buffer structure;
        structure.u32(count);
        payload.chunk(RwStruct, RwSanAndreas, structure);
        payload.bytes(animations.data);
        dictionary.chunk(RwUVAnimationDictionary, RwSanAndreas, payload);
    }
    return true;
}
} // namespace

bool storylandBuildDffExportScene(const StorylandModelFile& source,
                                  StorylandDffExportScene& scene, std::string& error) {
    error.clear();
    return dffSceneFromModel(source, scene, error);
}

bool storylandWriteDff(const StorylandDffExportScene& scene,
                       StorylandDffTarget target,
                       std::vector<uint8_t>& output,
                       std::string& error) {
    output.clear(); error.clear();
    if (!validateScene(scene, target, error)) return false;
    StorylandDffExportScene expandedScene = scene;
    if (target == StorylandDffTarget::MobileLcs) {
        expandedScene.meshes.clear();
        for (const auto& mesh : scene.meshes) {
            std::vector<StorylandDffExportMesh> pieces;
            if (!splitMobileGeometry(mesh, pieces, error)) return false;
            expandedScene.meshes.insert(expandedScene.meshes.end(),
                std::make_move_iterator(pieces.begin()), std::make_move_iterator(pieces.end()));
        }
    }
    const auto& source = expandedScene;
    std::vector<std::vector<std::array<std::string, 2>>> trackNames;
    Buffer uvDictionary;
    if (!buildUVAnimationDictionary(source, target, trackNames, uvDictionary, error)) return false;
    std::vector<FrameMatrix> frameWorld;
    if (!frameWorldMatrices(source, frameWorld, error)) return false;
    const uint32_t stamp = target == StorylandDffTarget::MobileLcs ? RwMobileLcs : RwSanAndreas;
    Buffer clump;
    Buffer structure;
    structure.u32(uint32_t(source.meshes.size()));
    if (target == StorylandDffTarget::SanAndreas) { structure.u32(0); structure.u32(0); }
    clump.chunk(RwStruct, stamp, structure);
    clump.chunk(RwFrameList, stamp, frameList(source, stamp));
    if (target == StorylandDffTarget::SanAndreas) {
        Buffer geometries;
        Buffer count;
        count.u32(uint32_t(source.meshes.size()));
        geometries.chunk(RwStruct, stamp, count);
        for (const auto& mesh : source.meshes)
            geometries.chunk(RwGeometry, stamp, standardGeometry(
                source, mesh, frameWorld, trackNames[size_t(&mesh - source.meshes.data())]));
        clump.chunk(RwGeometryList, stamp, geometries);
    }
    for (size_t index = 0; index < source.meshes.size(); ++index) {
        const auto& mesh = source.meshes[index];
        Buffer atomic;
        Buffer atom;
        atom.u32(mesh.frameIndex);
        if (target == StorylandDffTarget::SanAndreas) atom.u32(uint32_t(index));
        atom.u32(5u);
        atom.u32(0u);
        atomic.chunk(RwStruct, stamp, atom);
        if (target == StorylandDffTarget::MobileLcs) {
            Buffer geometry;
            if (!nativeGeometry(mesh, trackNames[index], geometry, error)) { output.clear(); return false; }
            atomic.chunk(RwGeometry, RwMobileGeometry, geometry);
        }
        Buffer extension;
        if (target == StorylandDffTarget::SanAndreas && mesh.skinned) {
            Buffer effect;
            effect.u32(1u);
            extension.chunk(RwMatFx, stamp, effect);
        }
        atomic.chunk(RwExtension, stamp, extension);
        clump.chunk(RwAtomic, stamp, atomic);
    }
    clump.chunk(RwExtension, stamp, {});
    Buffer file;
    file.bytes(uvDictionary.data);
    file.chunk(RwClump, stamp, clump);
    output = std::move(file.data);
    return true;
}

bool storylandConvertModelToDff(const StorylandModelFile& source,
                                StorylandDffTarget target,
                                std::vector<uint8_t>& output,
                                std::string& error) {
    if ((target == StorylandDffTarget::SanAndreas && source.isGtaSaDff()) ||
        (target == StorylandDffTarget::MobileLcs && source.isMobileLcsDff())) {
        output = source.rawBytes();
        error.clear();
        return !output.empty();
    }
    StorylandDffExportScene scene;
    if (!storylandBuildDffExportScene(source, scene, error)) return false;
    return storylandWriteDff(scene, target, output, error);
}
