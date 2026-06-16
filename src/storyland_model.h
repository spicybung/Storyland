#pragma once
#include <cstdint>
#include <string>
#include <vector>

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
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct StorylandModelTriangle {
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t materialIndex = 0xFFFFFFFFu;
};

struct StorylandModelTexcoord {
    float u = 0.0f;
    float v = 0.0f;
};

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
    bool loadFromFile(const std::wstring& filePath, std::string& errorMessage);
    const std::vector<StorylandModelLine>& lines() const;
    const std::vector<StorylandModelPoint>& previewPoints() const;
    const std::vector<StorylandModelTriangle>& previewTriangles() const;
    const std::vector<StorylandModelTexcoord>& previewTexcoords() const;
    const std::vector<StorylandModelSkinWeights>& previewSkinWeights() const;
    const std::vector<StorylandModelBone>& armatureBones() const;
    const std::vector<std::string>& previewMaterialTextureNames() const;
    const std::vector<std::string>& textureNameHints() const;
    const std::vector<StorylandModelField>& fields() const;
    StorylandModelKind modelKind() const;
    std::string modelKindName() const;
    const std::wstring& sourcePath() const;
    size_t fileSize() const;

private:
    std::wstring path;
    std::vector<uint8_t> data;
    std::vector<StorylandModelLine> outputLines;
    std::vector<StorylandModelPoint> points;
    std::vector<StorylandModelTriangle> triangles;
    std::vector<StorylandModelTexcoord> texcoords;
    std::vector<StorylandModelSkinWeights> skinWeights;
    std::vector<StorylandModelBone> bones;
    std::vector<std::string> materialTextureNames;
    std::vector<std::string> textureHints;
    std::vector<StorylandModelField> fieldRows;
    StorylandModelKind kind = StorylandModelKind::Unknown;

    void parse();
    void collectPreviewPoints();
    void collectArmatureBones();
    void collectTextureNameHints();
    void detectModelKind();
};
