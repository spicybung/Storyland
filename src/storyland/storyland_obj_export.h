#pragma once

#include "src/leeds/leeds_model.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct StorylandObjTextureBinding {
    std::string originalName;
    std::string outputFileName;
};

struct StorylandObjSource {
    const std::vector<StorylandModelPoint>& points;
    const std::vector<StorylandModelTriangle>& triangles;
    const std::vector<StorylandModelTexcoord>& texcoords;
    const std::vector<StorylandModelPrelight>& colors;
    const std::vector<std::string>& materialTextures;
    const std::vector<uint32_t>& materialColors;
    const std::vector<StorylandObjTextureBinding>& availableTextures;
    size_t boneCount = 0;
    size_t skinnedVertexCount = 0;
};

struct StorylandObjResult {
    std::string obj;
    std::string mtl;
    size_t vertices = 0;
    size_t faces = 0;
    size_t materials = 0;
    size_t groups = 0;
    bool omitsSkeleton = false;
};

inline std::string storylandObjToken(const std::string& value) {
    std::string token;
    token.reserve(value.size());
    for (unsigned char character : value) {
        if ((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.') {
            token.push_back(char(character));
        } else {
            token.push_back('_');
        }
    }
    while (!token.empty() && token.front() == '.') token.erase(token.begin());
    if (token.empty()) token = "unnamed";
    if (token.size() > 90) token.resize(90);
    return token;
}

inline std::string storylandObjNameLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return character >= 'A' && character <= 'Z' ? char(character + ('a' - 'A')) : char(character);
    });
    return value;
}

inline bool storylandObjFinite(float value) {
    return std::isfinite(value) && std::fabs(value) <= 10000000.0f;
}

inline bool storylandWriteObj(const StorylandObjSource& source,
                             const std::string& mtlFileName,
                             StorylandObjResult& result,
                             std::string& error) {
    result = {};
    error.clear();
    if (source.points.empty() || source.triangles.empty()) {
        error = "The model has no decoded vertices or triangles to export.";
        return false;
    }
    if (source.points.size() > 4000000u || source.triangles.size() > 12000000u) {
        error = "The model exceeds the OBJ export geometry limit.";
        return false;
    }
    const size_t vertexCount = source.points.size();
    const bool useUvs = source.texcoords.size() == vertexCount;
    if (!source.texcoords.empty() && !useUvs) {
        error = "The model has an incomplete texture coordinate stream.";
        return false;
    }
    const bool useColors = source.colors.size() == vertexCount &&
        std::any_of(source.colors.begin(), source.colors.end(),
            [](const StorylandModelPrelight& prelight) { return prelight.valid; });
    if (!source.colors.empty() && source.colors.size() != vertexCount) {
        error = "The model has an incomplete vertex colour stream.";
        return false;
    }
    if (mtlFileName.empty() || mtlFileName.find('/') != std::string::npos ||
        mtlFileName.find('\\') != std::string::npos ||
        mtlFileName.find('\n') != std::string::npos ||
        mtlFileName.find('\r') != std::string::npos) {
        error = "Invalid destination material filename.";
        return false;
    }
    const auto& points = source.points;
    const auto& triangles = source.triangles;
    std::vector<std::array<double, 3>> vertexNormals(vertexCount, {0.0, 0.0, 0.0});
    for (const auto& face : triangles) {
        if (face.a >= vertexCount || face.b >= vertexCount || face.c >= vertexCount) {
            error = "A triangle references a vertex outside the model.";
            return false;
        }
        if (face.a == face.b || face.a == face.c || face.b == face.c) {
            error = "A triangle has duplicate vertex indices.";
            return false;
        }
        const auto& a = points[face.a];
        const auto& b = points[face.b];
        const auto& c = points[face.c];
        const double ax = double(b.x) - double(a.x);
        const double ay = double(b.y) - double(a.y);
        const double az = double(b.z) - double(a.z);
        const double bx = double(c.x) - double(a.x);
        const double by = double(c.y) - double(a.y);
        const double bz = double(c.z) - double(a.z);
        const std::array<double,3> cross = { ay*bz-az*by, az*bx-ax*bz, ax*by-ay*bx };
        for (uint32_t vertex : {face.a,face.b,face.c}) {
            for (int component = 0; component < 3; ++component)
                vertexNormals[vertex][component] += cross[component];
        }
        if (face.materialIndex != UINT32_MAX &&
            (source.materialTextures.empty() ? face.materialIndex != 0u :
             face.materialIndex >= source.materialTextures.size())) {
            error = "A triangle references a material outside the material list.";
            return false;
        }
    }
    for (size_t index = 0; index < vertexCount; ++index) {
        const auto& point = points[index];
        if (!storylandObjFinite(point.x) || !storylandObjFinite(point.y) ||
            !storylandObjFinite(point.z) ||
            (point.hasNormal && (!storylandObjFinite(point.nx) ||
             !storylandObjFinite(point.ny) || !storylandObjFinite(point.nz)))) {
            error = "The model has invalid vertex coordinates or normals.";
            return false;
        }
        if (useUvs && (!storylandObjFinite(source.texcoords[index].u) ||
            !storylandObjFinite(source.texcoords[index].v))) {
            error = "The model has invalid UV coordinates.";
            return false;
        }
    }

    std::ostringstream obj;
    std::ostringstream mtl;
    obj.imbue(std::locale::classic());
    mtl.imbue(std::locale::classic());
    obj << std::setprecision(9);
    mtl << std::setprecision(9);
    obj << "# Storyland Wavefront OBJ\n";
    obj << "# Vertex positions and face winding are exported in source-model coordinates.\n";
    obj << "# Vehicle parts are OBJ objects/groups; OBJ has no wheel-pivot or bone-weight format.\n";
    if (source.boneCount || source.skinnedVertexCount) {
        obj << "# Original skeleton: " << source.boneCount << " bones, "
            << source.skinnedVertexCount << " weighted vertices.\n";
        obj << "# OBJ DOES NOT PRESERVE SKIN WEIGHTS OR SKELETAL ANIMATION.\n";
        result.omitsSkeleton = true;
    }
    obj << "mtllib " << mtlFileName << "\n";
    obj << "s 1\n";

    for (size_t index = 0; index < vertexCount; ++index) {
        const auto& point = points[index];
        obj << "v " << point.x << ' ' << point.y << ' ' << point.z;
        if (useColors) {
            const auto& color = source.colors[index];
            obj << ' ' << double(color.valid ? color.red : 255) / 255.0
                << ' ' << double(color.valid ? color.green : 255) / 255.0
                << ' ' << double(color.valid ? color.blue : 255) / 255.0;
        }
        obj << '\n';
    }
    if (useUvs) {
        for (const auto& uv : source.texcoords) {
            // Leeds/RenderWare texture V grows downwards; Wavefront OBJ grows upwards.
            // PNGs are exported from the original texture pixels without changing their rows.
            obj << "vt " << uv.u << ' ' << (1.0f - uv.v) << '\n';
        }
    }
    for (size_t index = 0; index < vertexCount; ++index) {
        const auto& point = source.points[index];
        double nx = point.nx, ny = point.ny, nz = point.nz;
        double length = point.hasNormal ? std::sqrt(nx*nx + ny*ny + nz*nz) : 0.0;
        if (length < 1e-12) {
            nx = vertexNormals[index][0];
            ny = vertexNormals[index][1];
            nz = vertexNormals[index][2];
            length = std::sqrt(nx*nx + ny*ny + nz*nz);
        }
        if (length < 1e-12) { nx = 0.0; ny = 0.0; nz = 1.0; length = 1.0; }
        obj << "vn " << nx / length << ' ' << ny / length << ' ' << nz / length << '\n';
    }
    const size_t materialCount = std::max<size_t>(1, source.materialTextures.size());
    std::vector<std::string> materialIdentifiers;
    materialIdentifiers.reserve(materialCount);
    mtl << "# Storyland material list\n";
    for (size_t index = 0; index < materialCount; ++index) {
        const std::string original = index < source.materialTextures.size() ?
            source.materialTextures[index] : std::string{};
        const std::string identifier = "mat_" + std::to_string(index) + "_" + storylandObjToken(original);
        materialIdentifiers.push_back(identifier);
        const uint32_t packed = index < source.materialColors.size() ?
            source.materialColors[index] : 0xFFFFFFFFu;
        const double red = double(packed & 255u) / 255.0;
        const double green = double((packed >> 8u) & 255u) / 255.0;
        const double blue = double((packed >> 16u) & 255u) / 255.0;
        const double opacity = double((packed >> 24u) & 255u) / 255.0;
        mtl << "\nnewmtl " << identifier << '\n';
        mtl << "Ka 0.2 0.2 0.2\n";
        mtl << "Kd " << red << ' ' << green << ' ' << blue << '\n';
        mtl << "Ks 0 0 0\nNs 0\n";
        mtl << "d " << opacity << "\nillum 2\n";
        if (!original.empty()) mtl << "# Original texture: " << storylandObjToken(original) << '\n';
        for (const auto& reference : source.availableTextures) {
            if (storylandObjNameLower(reference.originalName) != storylandObjNameLower(original)) continue;
            if (reference.outputFileName.empty() ||
                reference.outputFileName.find('/') != std::string::npos ||
                reference.outputFileName.find('\\') != std::string::npos ||
                reference.outputFileName.find('\n') != std::string::npos) continue;
            mtl << "map_Kd " << reference.outputFileName << '\n';
            break;
        }
    }
    std::string previousGroup;
    uint32_t previousMaterial = UINT32_MAX;
    size_t objects = 0;
    for (size_t faceIndex = 0; faceIndex < triangles.size(); ++faceIndex) {
        const auto& face = triangles[faceIndex];
        const std::string partName = face.vehiclePart.empty() ? "mesh" : face.vehiclePart;
        const std::string group = storylandObjToken(partName + (face.damagedPart ? "_dam" : ""));
        if (group != previousGroup) {
            obj << "\no " << group << "_" << objects++ << '\n';
            obj << "g " << group << '\n';
            previousGroup = group;
            previousMaterial = UINT32_MAX;
        }
        const uint32_t material = face.materialIndex < materialCount ? face.materialIndex : 0u;
        if (previousMaterial != material) {
            obj << "usemtl " << materialIdentifiers[material] << '\n';
            previousMaterial = material;
        }
        obj << "f ";
        const uint32_t vertices[3] = {face.a + 1u, face.b + 1u, face.c + 1u};
        for (size_t corner = 0; corner < 3; ++corner) {
            if (corner) obj << ' ';
            obj << vertices[corner];
            if (useUvs) obj << '/' << vertices[corner] << '/' << vertices[corner];
            else obj << "//" << vertices[corner];
        }
        obj << '\n';
    }
    result.obj = obj.str();
    result.mtl = mtl.str();
    result.vertices = vertexCount;
    result.faces = triangles.size();
    result.groups = objects;
    result.materials = materialCount;
    return true;
}

inline bool storylandWriteObj(const StorylandModelFile& model,
                             const std::string& mtlFileName,
                             const std::vector<StorylandObjTextureBinding>& availableTextures,
                             StorylandObjResult& result,
                             std::string& error) {
    size_t skinned = 0;
    for (const auto& weights : model.previewSkinWeights())
        if (weights.valid && weights.influenceCount > 0) ++skinned;
    const StorylandObjSource source = {
        model.previewPoints(), model.previewTriangles(), model.previewTexcoords(),
        model.previewPrelights(), model.previewMaterialTextureNames(),
        model.previewMaterialColors(), availableTextures,
        model.armatureBones().size(), skinned
    };
    return storylandWriteObj(source, mtlFileName, result, error);
}
