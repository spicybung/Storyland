#pragma once
#include <cstdint>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include "src/storyland/storyland_rw_uvanim.h"
#include "src/leeds/leeds_rsl_uvanim.h"

struct StorylandModelLine {
    std::string text;
};

struct StorylandModelField {
    std::string group;
    std::string name;
    uint32_t offset = 0;
    uint32_t value = 0;
    std::string note;
};

enum class StorylandModelKind {
    Unknown,
    SimpleModel,
    PedModel,
    CutsceneModel,
    VehicleModel,
    WorldModel
};

struct StorylandModelPoint {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    bool hasNormal = false;
    float ps2Nx = 0.0f, ps2Ny = 0.0f, ps2Nz = 0.0f;
    bool hasPs2Normal = false;
};

inline void storylandStorePs2ModelNormal(StorylandModelPoint& point, int8_t x, int8_t y, int8_t z) {
    point.ps2Nx = float(x) / 128.0f;
    point.ps2Ny = float(y) / 128.0f;
    point.ps2Nz = float(z) / 128.0f;
    point.hasPs2Normal = true;
}

template<class Number>
inline void storylandTransformPs2ModelNormal(StorylandModelPoint& output,
    const StorylandModelPoint& input, const Number* matrix, bool normalizeAxes = true) {
    const bool present = input.hasPs2Normal;
    const std::array<Number, 3> normal{Number(input.ps2Nx), Number(input.ps2Ny), Number(input.ps2Nz)};
    output.hasPs2Normal = false;
    if (!present) return;
    std::array<Number, 3> result{};
    for (size_t axis = 0; axis < 3; ++axis) {
        const Number x = matrix[axis * 4], y = matrix[axis * 4 + 1], z = matrix[axis * 4 + 2];
        const Number length = normalizeAxes ? std::sqrt(x*x + y*y + z*z) : Number(1);
        if (!std::isfinite(length) || length <= Number(1e-12)) return;
        const Number component = normal[axis] / length;
        result[0] += x * component;
        result[1] += y * component;
        result[2] += z * component;
    }
    if (!std::isfinite(result[0]) || !std::isfinite(result[1]) || !std::isfinite(result[2])) return;
    output.ps2Nx = float(result[0]);
    output.ps2Ny = float(result[1]);
    output.ps2Nz = float(result[2]);
    output.hasPs2Normal = true;
}

struct StorylandModelTriangle {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t materialIndex = 0xFFFFFFFFu;
    std::string vehiclePart;
    int8_t vehicleExtraIndex = -1;
    bool damagedPart = false;
    bool vehicleWheel = false,vehicleFrontWheel = false;
    StorylandModelPoint vehiclePivot;
    float pspTextureLodSlope = -1.0f;
};

inline int storylandNativeVehicleExtraCount(const std::vector<uint8_t>& bytes) {
    const auto word = [&](size_t offset) {
        return uint32_t(bytes[offset]) | (uint32_t(bytes[offset+1]) << 8) |
            (uint32_t(bytes[offset+2]) << 16) | (uint32_t(bytes[offset+3]) << 24);
    };
    if (bytes.size() < 0x2C || word(0) != 0x006D646Cu) return -1;
    const uint32_t count = word(0x24), table = word(0x28);
    if (count > 8 || (count && (!table || uint64_t(table) + uint64_t(count)*4 > bytes.size()))) return -1;
    return int(count);
}

inline bool storylandVehicleExtraVisible(int8_t extraIndex, const std::array<int32_t, 2>& selected) {
    return extraIndex < 0 || selected[0] == extraIndex || selected[1] == extraIndex;
}

struct StorylandModelTexcoord {
    float u = 0.0f;
    float v = 0.0f;
    float secondaryU = 0.0f;
    float secondaryV = 0.0f;
    bool hasSecondary = false;
};

enum class StorylandPrelightEncoding {
    None,
    Rgba8888,
    Ps2Vif5551,
    Psp5551,
    Ps2VifRgba8888
};

struct StorylandModelPrelight {
    uint8_t red = 255;
    uint8_t green = 255;
    uint8_t blue = 255;
    uint8_t alpha = 255;
    uint32_t fileOffset = 0xFFFFFFFFu;
    StorylandPrelightEncoding encoding = StorylandPrelightEncoding::None;
    bool valid = false;
};

inline std::array<uint8_t, 4> storylandPs2ModelPrelightColor(const StorylandModelPrelight& color) {
    if (color.encoding == StorylandPrelightEncoding::Ps2Vif5551)
        return {uint8_t(color.red & 0xF8u), uint8_t(color.green & 0xF8u),
            uint8_t(color.blue & 0xF8u), uint8_t(color.alpha ? 128u : 0u)};
    return {color.red, color.green, color.blue, color.alpha};
}

struct StorylandModelSkinInfluence {
    uint32_t boneIndex = 0xFFFFFFFFu;
    uint32_t rawMatrixIndex = 0xFFFFFFFFu;
    uint32_t rawPackedWord = 0;
    float weight = 0.0f;
};

struct StorylandModelSkinWeights {
    bool valid = false;
    uint32_t influenceCount = 0;
    StorylandModelSkinInfluence influences[4];
};

struct StorylandModelLight2dfx {
    StorylandModelPoint position;
    uint8_t red = 255;
    uint8_t green = 255;
    uint8_t blue = 255;
    uint8_t alpha = 255;
    float coronaFarClip = 0.0f;
    float pointLightRange = 0.0f;
    float coronaSize = 1.0f;
    float shadowSize = 0.0f;
    uint8_t coronaShowMode = 0;
    uint8_t coronaEnableReflection = 0;
    uint8_t coronaFlareType = 0;
    uint8_t shadowColorMultiplier = 0;
    uint8_t flags1 = 0;
    uint8_t flags2 = 0;
    int8_t lookDirectionX = 0;
    int8_t lookDirectionY = 0;
    int8_t lookDirectionZ = 0;
    bool hasLookDirection = false;
    std::string coronaTextureName;
    std::string shadowTextureName;
};

struct StorylandVehicleWheelFrame {
    std::string name;
    StorylandModelPoint pivot;
    bool front=false;
    std::array<float,16> matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
};
struct StorylandModelBone {
    uint32_t index = 0;
    uint32_t offset = 0;
    uint32_t parentIndex = 0xFFFFFFFFu;
    uint32_t parentOffset = 0;
    uint32_t nodeId = 0xFFFFFFFFu;
    uint32_t boneId = 0xFFFFFFFFu;
    uint32_t nameOffset = 0xFFFFFFFFu;
    std::string name;
    std::string sectionKind;
    bool hasLocalPosition = false;
    bool hasLocalRotation = false;
    bool hasWorldPosition = false;
    bool hasWorldRotation = false;
    bool hasComposedPosition = false;
    bool hasPreviewPosition = false;
    StorylandModelPoint localPosition;
    float localRotationX = 0.0f;
    float localRotationY = 0.0f;
    float localRotationZ = 0.0f;
    float localRotationW = 1.0f;
    float worldRotationX = 0.0f;
    float worldRotationY = 0.0f;
    float worldRotationZ = 0.0f;
    float worldRotationW = 1.0f;
    StorylandModelPoint worldPosition;
    StorylandModelPoint composedPosition;
    StorylandModelPoint previewPosition;
    std::string previewPositionSource;
};

class StorylandModelFile {
public:
    // Inspect only the geometry needed to validate a recovered native allocation.
    static bool hasNativePs2PreviewGeometry(const std::vector<uint8_t>& bytes);
    bool loadFromFile(const std::wstring& filePath, std::string& errorMessage);
    bool loadFromMemory(const std::vector<uint8_t>& bytes, const std::wstring& displayPath, std::string& errorMessage);
    // The caller must establish the vehicle class from native model-info metadata.
    bool loadNativeVehicleFromMemory(const std::vector<uint8_t>& bytes,
        const std::wstring& displayPath, std::string& errorMessage);
    const std::vector<StorylandModelLine>& lines() const;
    const std::vector<StorylandModelPoint>& previewPoints() const;
    const std::vector<StorylandModelTriangle>& previewTriangles() const;
    const std::vector<StorylandModelTexcoord>& previewTexcoords() const;
    const std::vector<StorylandModelPrelight>& previewPrelights() const;
    bool setPreviewPrelightColor(size_t vertexIndex, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, std::string& errorMessage);
    const std::vector<StorylandModelSkinWeights>& previewSkinWeights() const;
    const std::vector<uint32_t>& previewFrameIndices() const;
    const std::vector<uint32_t>& previewNativeFrameOffsets() const { return nativePointFrames; }
    bool previewPointsUseNativeFrames() const { return nativePointFramesApplied; }
    bool skinWeightsUseBoneIndices() const;
    const std::vector<StorylandModelBone>& armatureBones() const;
    const std::vector<StorylandModelLight2dfx>& preview2dfxLights() const;
    const std::vector<std::string>& previewMaterialTextureNames() const;
    std::vector<uint32_t> previewMaterialColors() const;
    const std::vector<float>& previewMaterialEffects() const;
    const std::vector<std::string>& previewMaterialEnvironmentNames() const;
    const std::vector<StorylandUVMaterialAnimation>& previewMaterialUVAnimations() const;
    const std::vector<StorylandRslUV::Material>& previewRslUVAnimations() const;
    bool bindRslUVAnimation(size_t materialIndex,size_t channel,const StorylandRslUV::Track& animation,std::string& error);
    const std::vector<std::string>& textureNameHints() const;
    const std::vector<StorylandModelField>& fields() const;
    StorylandModelKind modelKind() const;
    std::string modelKindName() const;
    const std::wstring& sourcePath() const;
    size_t fileSize() const;
    const std::vector<uint8_t>& rawBytes() const;
    std::array<float,16> vehiclePreviewRootMatrix() const;
    // Only a caller with authoritative native boat metadata may override a
    // standalone SimpleModel classification; ordinary previews retain their guard.
    std::array<float,16> vehiclePreviewRootMatrix(bool authoritativeNativeBoat) const;
    std::vector<StorylandVehicleWheelFrame> vehicleWheelFrames(bool includeEmbedded=false) const;
    bool vehicleComponentFrame(uint32_t componentId, std::array<float,16>& localMatrix,
        std::string& name, std::array<float,16>* previewParent=nullptr,
        std::vector<uint8_t>* pointMask=nullptr, bool vcs=true) const;
    std::vector<int8_t> vehicleMaterialPaintSlots(bool vcs) const;
    bool isMobileLcsDff() const;
    bool isPmlcMdl() const;
    bool isManhuntModel() const { return manhuntPsp || manhuntPc; }
    bool isManhuntPsp() const { return manhuntPsp && !manhuntPs2; }
    bool isManhuntPs2() const { return manhuntPs2; }
    bool isManhuntPc() const { return manhuntPc; }
    bool isPspNativeDff() const;
    bool isGtaSaDff() const;
    bool isDesktopRwDff() const;
    bool saveToFile(const std::wstring& outputPath, std::string& errorMessage) const;
    bool exportMobileLcsDffLossless(const std::wstring& outputPath, std::string& errorMessage) const;
    void createEmptyDraft(const std::wstring& displayPath);
    void createEmptyDraft(
        const std::wstring& displayPath,
        StorylandModelKind desiredKind,
        bool pspTarget,
        bool lcsTarget = false);
    void createEmptyDffDraft(const std::wstring& displayPath);
    void createEmptyDffDraft(
        const std::wstring& displayPath,
        StorylandModelKind desiredKind,
        bool mobileTarget = false);
    bool importMdlDataFromFile(const std::wstring& filePath, std::string& errorMessage);
    bool isEmptyDraft() const;

    bool loadNativeSimpleModel(const std::vector<uint8_t>& source,uint32_t modelId,std::string& error,
                               bool vcs=true);
    bool selectSimpleModelAtomic(size_t index,std::string& errorMessage);
    bool loadNativeClump(const std::vector<uint8_t>& source, uint32_t clumpOffset,
        bool vcs, std::string& errorMessage);
    bool loadNativeMarker(const std::vector<uint8_t>& source, bool vcs, uint8_t type,
        std::string& errorMessage);

private:
    std::wstring path;
    std::vector<uint8_t> data;
    std::vector<StorylandModelLine> outputLines;
    std::vector<StorylandModelPoint> points;
    std::vector<StorylandModelTriangle> triangles;
    std::vector<StorylandModelTexcoord> texcoords;
    std::vector<StorylandModelPrelight> prelights;
    std::vector<StorylandModelSkinWeights> skinWeights;
    std::vector<StorylandModelBone> bones;
    std::vector<uint32_t> pointFrameIndices;
    std::vector<uint32_t> nativePointFrames;
    bool nativePointFramesApplied = false;
    std::vector<StorylandModelLight2dfx> lights2dfx;
    std::vector<std::string> materialTextureNames;
    std::vector<float> materialEffects;
    std::vector<std::string> materialEnvironmentNames;
    std::vector<StorylandUVMaterialAnimation> materialUVAnimations;
    std::vector<StorylandRslUV::Material> rslMaterialUVAnimations;
    std::vector<uint32_t> residentMaterialColors;
    std::vector<std::string> textureHints;
    std::vector<StorylandModelField> fieldRows;
    StorylandModelKind kind = StorylandModelKind::Unknown;
    bool previewUsesPspNativeSkinPalette = false;
    bool mobileLcsDff = false;
    bool pmlcMdl = false;
    bool manhuntPsp = false;
    bool manhuntPs2 = false;
    bool manhuntPc = false;
    bool manhuntCompressed = false;
    bool pspNativeDff = false;
    bool desktopRwDff = false;
    bool emptyDraft = false;

    void parse(StorylandModelKind nativeKind = StorylandModelKind::Unknown);
    void parseContents(StorylandModelKind nativeKind);
    bool parseMobileLcsDff();
    bool parsePmlcMdl();
    bool parseManhuntPsp();
    bool parsePspStandardDff();
    bool parseDesktopRwDff();
    bool parsePspNativeDff();
    void collectRenderWare2dfxLights();
    void collectPreviewPoints();
    void collectPreviewPrelights();
    void collectArmatureBones();
    void collectTextureNameHints();
    void detectModelKind();
};
