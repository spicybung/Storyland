#include "src/leeds/leeds_archive.h"
#include "src/leeds/leeds_dtz.h"
#include "src/leeds/leeds_model.h"
#include "src/leeds/leeds_native_texture.h"
#include "src/leeds/leeds_texture.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <utility>

#include <zlib.h>

namespace {
uint32_t sceneryWord(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) return 0;
    uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

uint16_t sceneryHalf(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) return 0;
    uint16_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

float sceneryFloat(const std::vector<uint8_t>& bytes, size_t offset) {
    const uint32_t word = sceneryWord(bytes, offset);
    float value = 0;
    std::memcpy(&value, &word, sizeof(value));
    return value;
}

std::string sceneryName(std::string name) {
    const auto slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name.erase(0, slash + 1);
    const auto dot = name.find_last_of('.');
    if (dot != std::string::npos) name.resize(dot);
    for (char& character : name)
        if (character >= 'A' && character <= 'Z') character = char(character - 'A' + 'a');
    return name;
}

std::string sceneryString(const std::vector<uint8_t>& bytes, size_t offset, size_t length) {
    if (offset > bytes.size() || length > bytes.size() - offset) return {};
    std::string name;
    for (size_t index = 0; index < length && bytes[offset + index]; ++index) {
        const auto character = bytes[offset + index];
        if (character < 32 || character > 126) return {};
        name.push_back(char(character));
    }
    return name;
}

bool sceneryTextureEntry(const StorylandDtzDirEntry& entry) {
    const std::string& extension = entry.detectedExtension.empty() ? entry.expectedExtension : entry.detectedExtension;
    return extension == ".xtx" || extension == ".chk" || extension == ".tex" || extension == ".txd";
}

struct SceneryDirectory {
    std::map<uint32_t, size_t> streamEntries;
    std::map<uint32_t, size_t> modelEntries;
    std::map<uint32_t, size_t> modelHashes;

    explicit SceneryDirectory(const std::vector<StorylandDtzDirEntry>& entries) {
        for (size_t index = 0; index < entries.size(); ++index) {
            const auto& entry = entries[index];
            if (entry.streamingIndex != UINT32_MAX) streamEntries.emplace(entry.streamingIndex, index);
            if (entry.detectedExtension != ".mdl" && entry.detectedExtension != ".dff") continue;
            modelEntries.emplace(entry.streamingIndex, index);
            std::string stem = std::filesystem::path(entry.name).stem().string();
            for (char& character : stem)
                character = char(std::toupper(static_cast<unsigned char>(character)));
            uLong hash = crc32(0L, Z_NULL, 0);
            hash = crc32(hash, reinterpret_cast<const Bytef*>(stem.data()), static_cast<uInt>(stem.size()));
            const auto found = modelHashes.emplace(uint32_t(hash) ^ 0xFFFFFFFFu, index);
            if (found.second || found.first->second == SIZE_MAX) continue;
            const auto& previous = entries[found.first->second];
            found.first->second = previous.startSector == entry.startSector &&
                previous.sectorCount == entry.sectorCount ? index : SIZE_MAX;
        }
    }

    bool findModel(const std::vector<uint8_t>& data, uint32_t modelId, size_t& entryIndex) const {
        const auto streamed = modelEntries.find(modelId);
        if (streamed != modelEntries.end()) {
            entryIndex = streamed->second;
            return true;
        }
        const uint32_t count = sceneryWord(data, 0x38), table = sceneryWord(data, 0x3C);
        if (modelId >= count || table < 0x20 || uint64_t(table) + (uint64_t(modelId) + 1) * 4 > data.size())
            return false;
        const uint32_t info = sceneryWord(data, size_t(table) + size_t(modelId) * 4);
        if (info < 0x20 || uint64_t(info) + 12 > data.size()) return false;
        const auto named = modelHashes.find(sceneryWord(data, size_t(info) + 8));
        if (named == modelHashes.end() || named->second == SIZE_MAX) return false;
        entryIndex = named->second;
        return true;
    }
};

struct SceneryDictionary {
    uint16_t slot = 0;
    uint32_t residentPointer = 0;
    uint32_t baseOffset = 0;
    size_t entryIndex = SIZE_MAX;
    std::string name;
    std::string source;
    std::set<std::string> wantedNames;
    std::map<std::string, uint32_t> textureIds;
};

struct SceneryAtomic {
    uint8_t index = 0;
    float minimumDistance = 0;
    float maximumDistance = 0;
    std::vector<size_t> meshes;
};

struct SceneryModel {
    uint32_t id = 0;
    uint32_t infoOffset = 0;
    uint16_t flags = 0;
    uint16_t textureSlot = 0;
    size_t entryIndex = SIZE_MAX;
    std::string name;
    StorylandModelFile geometry;
    std::vector<SceneryAtomic> atomics;
};

struct SceneryAlpha {
    bool zero = false;
    bool fractional = false;
    bool opaque = false;
};

SceneryAlpha sceneryTextureAlpha(const StorylandDirectTextureResource& texture) {
    SceneryAlpha alpha;
    for (size_t index = 3; index < texture.rgba.size(); index += 4) {
        const uint8_t value = texture.rgba[index];
        alpha.zero = alpha.zero || value == 0;
        alpha.opaque = alpha.opaque || value == 255;
        alpha.fractional = alpha.fractional || (value > 0 && value < 255);
    }
    return alpha;
}

uint8_t sceneryAlphaByte(const StorylandModelPrelight& color) {
    if (color.encoding == StorylandPrelightEncoding::Ps2VifRgba8888)
        return uint8_t(std::min(255u, unsigned(color.alpha) * 255u / 128u));
    return color.alpha;
}

void sceneryPlacementBounds(StorylandWorldPlacement& placement, const StorylandWorldMesh& mesh) {
    std::array<double, 3> center{}, extent{};
    for (size_t row = 0; row < 3; ++row) {
        center[row] = placement.matrix[12 + row];
        for (size_t column = 0; column < 3; ++column) {
            const double coefficient = placement.matrix[column * 4 + row];
            center[row] += coefficient * (mesh.bounds.minimum[column] + mesh.bounds.maximum[column]) * .5;
            extent[row] += std::abs(coefficient) * (mesh.bounds.maximum[column] - mesh.bounds.minimum[column]) * .5;
        }
    }
    placement.boundX = float(center[0]);
    placement.boundY = float(center[1]);
    placement.boundZ = float(center[2]);
    placement.boundRadius = float(std::max(.01, std::hypot(extent[0], extent[1], extent[2])));
}
}

std::string StorylandNativeSceneryReport::summary() const {
    std::ostringstream text;
    text << "Original IMG scenery: " << modelCount << " models, " << placementCount
         << " material/LOD placements, " << textureCount << " textures; "
         << emptyModelCount << " empty models, " << skippedModels.size()
         << " skipped resources, " << missingTextures.size() << " missing textures.";
    return text.str();
}

bool StorylandArchiveBrowser::nativePlacementVisible(const StorylandWorldPlacement& placement,
    float distance, uint8_t area, float lodMultiplier) {
    if (placement.nativeInstanceIndex == UINT32_MAX) return true;
    if (!placement.nativeVisible) return false;
    if (placement.nativeAreaRestricted && placement.nativeArea != area && placement.nativeArea != 13) return false;
    if (!std::isfinite(distance) || distance < 0 || !std::isfinite(lodMultiplier) || lodMultiplier <= 0) return false;
    return distance >= placement.nativeMinDrawDistance * lodMultiplier &&
        distance < placement.nativeDrawDistance * lodMultiplier;
}

void StorylandArchiveBrowser::synchronizeNativeSceneryTransforms(const StorylandNativeWorld& world) {
    if (!nativeScenery) return;
    for (size_t index = 0; index < worldPlacements.size(); ++index) {
        auto& placement = worldPlacements[index];
        if (placement.nativeInstanceIndex >= world.instances.size() || placement.resourceIndex >= worldMeshCache.size()) {
            placement.nativeVisible = false;
            continue;
        }
        const auto& instance = world.instances[placement.nativeInstanceIndex];
        const auto& mesh = worldMeshCache[placement.resourceIndex];
        placement.nativeVisible = (placement.nativeEntityFlags & 0x00080000u) != 0 &&
            instance.poolHeader == placement.nativePoolHeader && instance.poolIndex == placement.nativePoolIndex &&
            instance.modelId == uint32_t(mesh.nativeModelInfoId);
        if (!std::equal(instance.matrix.begin(), instance.matrix.end(), placement.matrix))
            setPlacementTransform(index, instance.matrix);
    }
}

bool StorylandArchiveBrowser::loadNativeScenery(const StorylandDtzArchive& dtz,
    StorylandNativeSceneryReport& report, std::string& errorMessage) {
    clear();
    report = {};
    errorMessage.clear();
    const auto& data = dtz.unpackedBytes();
    const auto& entries = dtz.dirEntries();
    const bool vcs = dtz.gameVariant() == StorylandDtzGameVariant::Vcs;
    if (dtz.gameVariant() != StorylandDtzGameVariant::Vcs && dtz.gameVariant() != StorylandDtzGameVariant::Lcs) {
        errorMessage = "Native scenery requires an identified LCS or VCS GAME.DTZ.";
        return false;
    }
    if (!dtz.hasCompanionImg()) {
        errorMessage = "Original gta3ps*.img is unavailable; native scenery geometry was not substituted.";
        return false;
    }
    const uint32_t count = sceneryWord(data, 0x38), table = sceneryWord(data, 0x3C);
    if (data.size() < 0x80 || !table || !count || count > 100000 || uint64_t(table) + uint64_t(count) * 4 > data.size()) {
        errorMessage = "Native scenery model-info table is outside GAME.DTZ.";
        return false;
    }
    const uint32_t texList = sceneryWord(data, 0x60);
    const uint32_t texRows = sceneryWord(data, texList), texCount = sceneryWord(data, size_t(texList) + 8);
    const bool validTexList = texList && texRows && texCount <= 4096 &&
        uint64_t(texRows) + uint64_t(texCount) * 28 <= data.size();
    const uint32_t streaming = sceneryWord(data, 0x7C);
    const uint32_t textureStreamBase = vcs ? sceneryWord(data, size_t(streaming) + 8) : 4900u;
    const size_t distanceOffset = vcs ? 0x2C : 0x28;
    const size_t countOffset = vcs ? 0x38 : 0x34;
    const size_t flagsOffset = vcs ? 0x3A : 0x36;
    const SceneryDirectory directory(entries);
    const auto& streamEntries = directory.streamEntries;
    std::set<uint32_t> placedModels;
    for (const auto& instance : dtz.nativeWorld().instances) placedModels.insert(instance.modelId);
    std::map<uint32_t, SceneryModel> models;
    std::map<uint16_t, SceneryDictionary> dictionaries;
    for (uint32_t id : placedModels) {
        if (id >= count) continue;
        const uint32_t info = sceneryWord(data, size_t(table) + size_t(id) * 4);
        if (!info || uint64_t(info) + countOffset + 4 > data.size()) continue;
        if (data[size_t(info) + 0x10] != 1 && data[size_t(info) + 0x10] != 3) continue;
        size_t entryIndex = SIZE_MAX;
        std::string error;
        if (!directory.findModel(data, id, entryIndex)) {
            const auto declared = streamEntries.find(id);
            if (declared != streamEntries.end() && id < textureStreamBase) {
                const auto& entry = entries[declared->second];
                if (entry.expectedExtension == ".mdl" || entry.detectedExtension == ".mdl" || entry.detectedExtension == ".dff")
                    report.skippedModels.push_back("Model " + std::to_string(id) + " " + entry.name +
                        ": unavailable original geometry" + (error.empty() ? std::string() : ": " + error));
            }
            continue;
        }
        const auto& entry = entries[entryIndex];
        const std::string label = "Model " + std::to_string(id) + " " + entry.name;
        std::vector<uint8_t> bytes;
        if (!dtz.extractDirEntryBytes(entryIndex, bytes, error)) {
            report.skippedModels.push_back(label + ": unavailable original geometry: " + error);
            continue;
        }
        SceneryModel model;
        model.id = id;
        model.infoOffset = info;
        model.entryIndex = entryIndex;
        model.name = entry.name;
        model.flags = sceneryHalf(data, size_t(info) + flagsOffset);
        model.textureSlot = sceneryHalf(data, size_t(info) + 0x1E);
        if (!model.geometry.loadFromMemory(bytes, std::filesystem::path(entry.name).wstring(), error) ||
            model.geometry.modelKind() != StorylandModelKind::SimpleModel) {
            report.skippedModels.push_back(label + ": original geometry could not be decoded (" +
                model.geometry.modelKindName() + ")" + (error.empty() ? std::string() : ": " + error));
            continue;
        }
        const uint8_t atomicCount = data[size_t(info) + countOffset];
        if (!atomicCount || atomicCount > 3) {
            report.skippedModels.push_back(label + ": invalid authored simple-model atomic count " + std::to_string(atomicCount));
            continue;
        }
        const uint8_t firstAtomic = (model.flags & 8) ? uint8_t(model.flags & 3) : 0;
        float previousDistance = 0;
        for (uint8_t atomic = firstAtomic; atomic < atomicCount; ++atomic) {
            const float maximum = sceneryFloat(data, size_t(info) + distanceOffset + size_t(atomic) * 4);
            if (!std::isfinite(maximum) || maximum <= 0 || maximum > 10000) {
                report.skippedModels.push_back(label + " atomic " + std::to_string(atomic) + ": invalid authored draw distance");
                continue;
            }
            // The retail search returns its first matching distance. Shorter
            // later ranges are damaged variants, not additional visible LODs.
            if (maximum <= previousDistance) continue;
            if (!model.geometry.selectSimpleModelAtomic(atomic, error)) {
                report.skippedModels.push_back(label + " atomic " + std::to_string(atomic) + ": original geometry could not be decoded: " + error);
                previousDistance = maximum;
                continue;
            }
            if (model.geometry.previewPoints().empty() || model.geometry.previewTriangles().empty()) {
                report.skippedModels.push_back(label + " atomic " + std::to_string(atomic) + ": decoded empty original geometry");
                previousDistance = maximum;
                continue;
            }
            auto& dictionary = dictionaries[model.textureSlot];
            dictionary.slot = model.textureSlot;
            for (const auto& name : model.geometry.previewMaterialTextureNames())
                if (!name.empty() && name.front() != '<') dictionary.wantedNames.insert(sceneryName(name));
            model.atomics.push_back({atomic, previousDistance, maximum, {}});
            previousDistance = maximum;
        }
        if (model.atomics.empty()) {
            if (model.geometry.previewPoints().empty() || model.geometry.previewTriangles().empty()) ++report.emptyModelCount;
            continue;
        }
        models.emplace(id, std::move(model));
    }

    for (auto& item : dictionaries) {
        auto& dictionary = item.second;
        if (validTexList && dictionary.slot < texCount) {
            const size_t row = size_t(texRows) + size_t(dictionary.slot) * 28;
            dictionary.residentPointer = sceneryWord(data, row);
            dictionary.name = sceneryString(data, row + 8, 20);
        }
        const auto stream = streamEntries.find(textureStreamBase + dictionary.slot);
        if (stream != streamEntries.end() && sceneryTextureEntry(entries[stream->second])) dictionary.entryIndex = stream->second;
        if (dictionary.entryIndex == SIZE_MAX && !dictionary.name.empty()) {
            const std::wstring name(dictionary.name.begin(), dictionary.name.end());
            dtz.findDirEntryByStemAndExtension(name, {L".xtx", L".chk", L".tex", L".txd"}, dictionary.entryIndex);
        }
        const auto appendTexture = [&](const std::string& name, RgbaImage image, uint32_t headerOffset,
            uint32_t dataOffset, uint32_t flags, unsigned bpp, bool storedInImg) {
            const std::string key = sceneryName(name);
            if (!dictionary.wantedNames.count(key) || dictionary.textureIds.count(key) || image.width <= 0 ||
                image.height <= 0 || image.rgba.size() != size_t(image.width) * size_t(image.height) * 4) return;
            StorylandDirectTextureResource texture;
            texture.index = uint32_t(directTextureCache.size());
            texture.materialId = int32_t(texture.index);
            texture.unambiguousMaterialBinding = true;
            texture.headerOffset = headerOffset;
            texture.dataOffset = dataOffset;
            texture.rasterFlags = flags;
            texture.width = image.width;
            texture.height = image.height;
            texture.bpp = int(bpp);
            texture.pspRaster = dtz.isPsp();
            texture.storedInImg = storedInImg;
            texture.baseOffset = dictionary.baseOffset;
            texture.storageBytes = uint32_t(rgbaImageDecodedBytes(image));
            texture.name = key;
            texture.source = dictionary.source;
            texture.rgba = std::move(image.rgba);
            texture.mipLevels = std::move(image.mipLevels);
            texture.ps2Alpha = std::move(image.ps2Alpha);
            texture.ps2FiveBitColor = image.ps2FiveBitColor;
            dictionary.textureIds.emplace(key, texture.index);
            directTextureCache.push_back(std::move(texture));
        };
        std::string error;
        if (dictionary.entryIndex != SIZE_MAX && dictionary.entryIndex < entries.size()) {
            const auto& entry = entries[dictionary.entryIndex];
            dictionary.baseOffset = uint32_t(entry.byteOffset);
            dictionary.source = entry.name;
            std::vector<uint8_t> bytes;
            LeedsTextureArchive textures;
            if (dtz.extractDirEntryBytes(dictionary.entryIndex, bytes, error) &&
                textures.loadFromMemory(bytes, LeedsPlatform::Auto, error, std::filesystem::path(entry.name).wstring())) {
                for (size_t index = 0; index < textures.textures().size(); ++index) {
                    const auto& texture = textures.textures()[index];
                    if (!dictionary.wantedNames.count(sceneryName(texture.name))) continue;
                    RgbaImage image;
                    if (textures.decodeTexture(index, image, error))
                        appendTexture(texture.name, std::move(image), texture.textureHeaderOffset,
                            texture.rasterOffset, texture.flags, texture.bpp, true);
                }
            }
        } else if (dictionary.residentPointer) {
            dictionary.baseOffset = dictionary.residentPointer;
            dictionary.source = "GAME.DTZ/" + dictionary.name;
            for (auto& texture : storylandDecodeNativeTextureDictionary(data, dictionary.residentPointer, vcs, &dictionary.wantedNames))
                appendTexture(texture.name, std::move(texture.image), dictionary.residentPointer, 0, 0, 32, false);
        }
        for (const auto& name : dictionary.wantedNames) {
            if (dictionary.textureIds.count(name)) continue;
            std::string modelsUsing;
            for (const auto& model : models)
                if (model.second.textureSlot == dictionary.slot) {
                    if (!modelsUsing.empty()) modelsUsing += ", ";
                    modelsUsing += std::to_string(model.first) + " " + model.second.name;
                }
            report.missingTextures.push_back("TXD " + std::to_string(dictionary.slot) + " " + dictionary.name +
                ", raster " + name + ", models " + modelsUsing + ": unavailable original texture" +
                (error.empty() ? std::string() : " (" + error + ")"));
        }
    }

    for (auto& item : models) {
        auto& model = item.second;
        const auto& dictionary = dictionaries[model.textureSlot];
        std::string error;
        for (auto& atomic : model.atomics) {
            if (!model.geometry.selectSimpleModelAtomic(atomic.index, error)) continue;
            const auto& points = model.geometry.previewPoints();
            const auto& texcoords = model.geometry.previewTexcoords();
            const auto& prelights = model.geometry.previewPrelights();
            const auto& textureNames = model.geometry.previewMaterialTextureNames();
            const auto colors = model.geometry.previewMaterialColors();
            const auto& materialEffects = model.geometry.previewMaterialEffects();
            std::map<uint32_t, std::vector<const StorylandModelTriangle*>> materialTriangles;
            for (const auto& triangle : model.geometry.previewTriangles())
                if (triangle.a < points.size() && triangle.b < points.size() && triangle.c < points.size())
                    materialTriangles[triangle.materialIndex].push_back(&triangle);
            for (const auto& material : materialTriangles) {
                StorylandWorldMesh mesh;
                mesh.sectorIndex = UINT32_MAX;
                mesh.resourceIndex = uint32_t(worldMeshCache.size());
                mesh.nativeModelInfoId = int32_t(model.id);
                mesh.rawOffset = entries[model.entryIndex].byteOffset;
                mesh.textureScopeOffset = dictionary.baseOffset;
                mesh.textureScopeInImg = dictionary.entryIndex != SIZE_MAX;
                mesh.materialCount = 1;
                mesh.nativeMaterialEffect=material.first<materialEffects.size()?materialEffects[material.first]:0.0f;
                mesh.nativeVegetation = sceneryName(model.name).compare(0, 4, "veg_") == 0;
                const uint32_t color = material.first < colors.size() ? colors[material.first] : 0xFFFFFFFFu;
                const uint8_t materialAlpha = uint8_t(color >> 24);
                uint32_t textureId = UINT32_MAX;
                if (material.first < textureNames.size()) {
                    const auto found = dictionary.textureIds.find(sceneryName(textureNames[material.first]));
                    if (found != dictionary.textureIds.end()) textureId = found->second;
                }
                SceneryAlpha textureAlpha;
                if (textureId != UINT32_MAX) textureAlpha = sceneryTextureAlpha(directTextureCache[textureId]);
                bool vertexAlpha = false;
                std::map<uint32_t, uint32_t> remap;
                std::map<float, uint32_t> pspSlopes;
                for (const auto* source : material.second) {
                    StorylandWorldMeshTriangle triangle;
                    triangle.textureId = textureId;
                    if (dtz.isPsp() && std::isfinite(source->pspTextureLodSlope) && source->pspTextureLodSlope >= 0.0f) {
                        const auto inserted = pspSlopes.emplace(source->pspTextureLodSlope, uint32_t(mesh.pspStrips.size()));
                        if (inserted.second) {
                            StorylandWorldPspStripState state;
                            state.textureLodSlope = source->pspTextureLodSlope;
                            state.modelSlope = true;
                            mesh.pspStrips.push_back(state);
                        }
                        triangle.pspStripIndex = inserted.first->second;
                    }
                    std::array<uint32_t, 3> output{};
                    const std::array<uint32_t, 3> input{source->a, source->b, source->c};
                    for (size_t corner = 0; corner < 3; ++corner) {
                        const uint32_t index = input[corner];
                        auto found = remap.find(index);
                        if (found == remap.end()) {
                            const auto& point = points[index];
                            StorylandWorldMeshVertex vertex;
                            vertex.x = point.x; vertex.y = point.y; vertex.z = point.z;
                            vertex.nx = point.nx; vertex.ny = point.ny; vertex.nz = point.nz;
                            vertex.ps2Nx=point.hasPs2Normal?point.ps2Nx:point.nx;
                            vertex.ps2Ny=point.hasPs2Normal?point.ps2Ny:point.ny;
                            vertex.ps2Nz=point.hasPs2Normal?point.ps2Nz:point.nz;
                            mesh.nativeHasNormals = mesh.nativeHasNormals || point.hasNormal;
                            if (index < texcoords.size()) { vertex.u = texcoords[index].u; vertex.v = texcoords[index].v; }
                            vertex.red = dtz.isPsp() ? uint8_t(color) : 128u;
                            vertex.green = dtz.isPsp() ? uint8_t(color >> 8u) : 128u;
                            vertex.blue = dtz.isPsp() ? uint8_t(color >> 16u) : 128u;
                            vertex.alpha = dtz.isPsp() ? materialAlpha : 255u;
                            if(!dtz.isPsp()) {
                                vertex.ps2Color={128,128,128,128};
                                vertex.hasPs2Color=true;
                            }
                            if (index < prelights.size() && prelights[index].valid) {
                                const auto& prelight = prelights[index];
                                vertex.red = prelight.red; vertex.green = prelight.green; vertex.blue = prelight.blue;
                                vertex.alpha = sceneryAlphaByte(prelight);
                                if(!dtz.isPsp())vertex.ps2Color=storylandPs2ModelPrelightColor(prelight);
                            }
                            if(!dtz.isPsp()&&point.hasNormal)
                                vertex.ps2Color={uint8_t(color),uint8_t(color>>8),uint8_t(color>>16),materialAlpha};
                            if (!dtz.isPsp()) {
                                vertex.red = uint8_t(unsigned(vertex.red) * (color & 255) / 255u);
                                vertex.green = uint8_t(unsigned(vertex.green) * ((color >> 8) & 255) / 255u);
                                vertex.blue = uint8_t(unsigned(vertex.blue) * ((color >> 16) & 255) / 255u);
                                vertex.alpha = uint8_t(unsigned(vertex.alpha) * materialAlpha / 255u);
                            }
                            vertex.hasPrelight = true;
                            vertexAlpha = vertexAlpha || vertex.alpha != 255;
                            const uint32_t destination = uint32_t(mesh.vertices.size());
                            mesh.vertices.push_back(vertex);
                            found = remap.emplace(index, destination).first;
                        }
                        output[corner] = found->second;
                    }
                    triangle.a = output[0]; triangle.b = output[1]; triangle.c = output[2];
                    mesh.triangles.push_back(triangle);
                }
                mesh.nativeAlphaTest = textureAlpha.zero && !vertexAlpha;
                mesh.nativeAlphaBlend = textureAlpha.fractional || vertexAlpha;
                mesh.bounds = storylandMeshBounds(mesh.vertices);
                if (!mesh.bounds.valid || mesh.vertices.empty() || mesh.triangles.empty()) continue;
                atomic.meshes.push_back(worldMeshCache.size());
                decodedWorldMeshKeys.insert((uint64_t(mesh.sectorIndex) << 32) | mesh.resourceIndex);
                worldMeshCache.push_back(std::move(mesh));
            }
        }
        bool nonempty = false;
        for (const auto& atomic : model.atomics) nonempty = nonempty || !atomic.meshes.empty();
        if (nonempty) ++report.modelCount;
    }

    const auto& instances = dtz.nativeWorld().instances;
    for (size_t index = 0; index < instances.size(); ++index) {
        const auto& instance = instances[index];
        const auto model = models.find(instance.modelId);
        if (model == models.end()) continue;
        const uint32_t pool = sceneryWord(data, instance.poolHeader);
        const uint32_t items = sceneryWord(data, pool);
        const size_t row = size_t(items) + size_t(instance.poolIndex) * 96;
        if (row + 96 > data.size()) continue;
        const size_t flags = row + (vcs ? 0x48 : 0x44);
        for (const auto& atomic : model->second.atomics) for (size_t meshIndex : atomic.meshes) {
            const auto& mesh = worldMeshCache[meshIndex];
            StorylandWorldPlacement placement;
            placement.resourceIndex = mesh.resourceIndex;
            placement.sectorIndex = UINT32_MAX;
            placement.iplId = uint32_t(worldPlacements.size());
            placement.iplRaw = placement.iplId;
            placement.imgOffset = row;
            placement.nativeInstanceIndex = uint32_t(index);
            placement.nativePoolHeader = instance.poolHeader;
            placement.nativePoolIndex = instance.poolIndex;
            placement.nativeEntityFlags = sceneryWord(data, flags);
            placement.nativeEntityFlags2 = sceneryWord(data, flags + 4);
            placement.nativeModelFlags = model->second.flags;
            placement.nativeLevel = data[row + 0x5A];
            // VCS has no LCS area byte; +0x5B is allocation padding.
            placement.nativeArea = vcs ? 0 : data[row + 0x5B];
            placement.nativeAreaRestricted = !vcs;
            placement.nativeVisible = (placement.nativeEntityFlags & 0x00080000u) != 0;
            placement.nativeAtomicIndex = atomic.index;
            placement.nativeMinDrawDistance = atomic.minimumDistance;
            placement.nativeDrawDistance = atomic.maximumDistance;
            const bool transparent = mesh.nativeAlphaTest || mesh.nativeAlphaBlend;
            placement.passIndex = transparent ? 6 : 3;
            placement.passName = transparent ? "TRANSPARENT" : "NORMAL";
            std::copy(instance.matrix.begin(), instance.matrix.end(), placement.matrix);
            placement.x = instance.matrix[12]; placement.y = instance.matrix[13]; placement.z = instance.matrix[14];
            placement.scaleX = std::hypot(instance.matrix[0], instance.matrix[1], instance.matrix[2]);
            placement.scaleY = std::hypot(instance.matrix[4], instance.matrix[5], instance.matrix[6]);
            placement.scaleZ = std::hypot(instance.matrix[8], instance.matrix[9], instance.matrix[10]);
            sceneryPlacementBounds(placement, mesh);
            worldPlacements.push_back(std::move(placement));
        }
    }
    nativeScenery = true;
    currentImgPath = dtz.companionImgPath();
    currentImgSize = dtz.companionImgSize();
    currentImgKind = StorylandImgKind::ClassicDir;
    report.placementCount = worldPlacements.size();
    report.textureCount = directTextureCache.size();
    currentLevelSummary = report.summary();
    return true;
}
