#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"

using Microsoft::WRL::ComPtr;
using namespace Math;

constexpr uint32_t kNumMaxInfluence = 4;
// Object3dの共通RootSignatureで、メッシュごとの材質色を渡すスロット。
constexpr uint32_t kModelMaterialColorRootParameter = 11;

// ===========================
//  モデル用データ構造
// ===========================
struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
    Vector4 weight;
    std::array<uint32_t, kNumMaxInfluence> jointIndices;
};

struct MaterialData {
    std::string textureFilePath;
    std::string normalTextureFilePath;
    float roughness = 0.66f;
    float metallic = 0.0f;
    float normalStrength = 0.0f;
    // glTFのbaseColorFactor。Object3dの演出用カラーとは別に保持する。
    Vector4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool alphaBlend = false; // glTFのBLEND材質。壁などの不透明面とは別のパスで描く。
};

enum class ModelDrawPass { All, Opaque, Transparent };

struct ModelDrawRange {
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

template <typename T>
struct Keyframe {
    float time;
    T value;
};

struct NodeAnimation {
    std::vector<Keyframe<Vector3>> translate;
    std::vector<Keyframe<Quaternion>> rotate;
    std::vector<Keyframe<Vector3>> scale;
};

struct Animation {
    float duration = 0.0f;
    float ticksPerSecond = 1.0f;
    std::map<std::string, NodeAnimation> nodeAnimations;
};

struct Node {
    QuaternionTransform transform;
    Matrix4x4 localMatrix;
    std::string name;
    std::vector<Node> children;
};

struct JointWeightData {
    Matrix4x4 inverseBindPoseMatrix;
};

struct SkinClusterData {
    std::map<std::string, JointWeightData> jointWeights;
};

struct Joint {
    QuaternionTransform transform;
    QuaternionTransform bindPoseTransform;
    Matrix4x4 localMatrix;
    Matrix4x4 skeletonSpaceMatrix;
    std::string name;
    std::vector<int32_t> children;
    std::optional<int32_t> parent;
    int32_t index;
};

struct Skeleton {
    int32_t root = -1;
    std::map<std::string, int32_t> jointMap;
    std::vector<Joint> joints;
};

struct ModelData {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    MaterialData material;
    std::vector<MaterialData> materials;
    std::vector<ModelDrawRange> drawRanges;
    Node rootNode;
    Animation animation;
    SkinClusterData skinClusterData;
};

struct Material {
    Vector4 color;
    int32_t lightingMode;
    float shininess;
    float environmentCoefficient;
    float alphaReference;
    Vector3 specularColor;
    float roughness;
    float metallic;
    float normalStrength;
    float padding1[2];
    Matrix4x4 uvTransform;
};

class ModelCommon;

class Model {
public:
    void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);
    void Initialize(ModelCommon* modelCommon, const ModelData& modelData);
    void Draw(
        const D3D12_VERTEX_BUFFER_VIEW* overrideVertexBufferView = nullptr,
        ID3D12Resource* overrideMaterialResource = nullptr,
        const std::string* overrideTextureFilePath = nullptr,
        ModelDrawPass drawPass = ModelDrawPass::All);
    bool HasTransparentMaterials() const;
    void SetEnvironmentCoefficient(float coefficient);
    float GetEnvironmentCoefficient() const;
    void SetColor(const Vector4& color);
    void SetAlphaReference(float alphaReference);
    void SetUVTransform(const Matrix4x4& uvTransform);
    void SetLightingMode(int32_t lightingMode);
    void SetTextureFilePath(const std::string& textureFilePath);
    Vector4 GetColor() const;
    float GetAlphaReference() const;
    int32_t GetLightingMode() const;
    const std::string& GetTextureFilePath() const;
    const Material& GetMaterial() const { return *materialData_; }
    const Animation& GetAnimation() const { return modelData_.animation; }
    const Node& GetRootNode() const { return modelData_.rootNode; }
    const SkinClusterData& GetSkinClusterData() const { return modelData_.skinClusterData; }
    bool HasAnimation() const
    {
        return !modelData_.animation.nodeAnimations.empty();
    }
    bool HasSkinCluster() const
    {
        return !modelData_.skinClusterData.jointWeights.empty();
    }

    // Object3d が必要になったら使う用（次段階用）
    const D3D12_VERTEX_BUFFER_VIEW& GetVBV() const { return vertexBufferView_; }
    const D3D12_INDEX_BUFFER_VIEW& GetIBV() const { return indexBufferView_; }
    ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }
    size_t GetVertexCount() const { return modelData_.vertices.size(); }
    size_t GetIndexCount() const { return modelData_.indices.size(); }
    const std::vector<VertexData>& GetVertices() const { return modelData_.vertices; }

    static MaterialData LoadMaterialTemplate(const std::string& directoryPath, const std::string& filename);
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
    static ModelData LoadAssimpFile(const std::string& directoryPath, const std::string& filename);
    static QuaternionTransform CalculateValue(const NodeAnimation& nodeAnimation, float time);
    static Skeleton CreateSkeleton(const Node& rootNode);
    static void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float time);
    static void UpdateSkeleton(Skeleton& skeleton);
    static ModelData CreateTriangleData(float width, float height, const std::string& textureFilePath);
    static ModelData CreatePlaneData(float width, float height, const std::string& textureFilePath);
    static ModelData CreateCircleData(
        uint32_t divideCount,
        float radius,
        const std::string& textureFilePath);
    static ModelData CreateHeartData(
        uint32_t divideCount,
        float radius,
        const std::string& textureFilePath);
    static ModelData CreateRingData(
        uint32_t divideCount,
        float outerRadius,
        float innerRadius,
        const std::string& textureFilePath);
    static ModelData CreateArcData(
        uint32_t divideCount,
        float outerRadius,
        float width,
        float arcRadians,
        const std::string& textureFilePath);
    static ModelData CreateSphereData(
        uint32_t latDivideCount,
        uint32_t lonDivideCount,
        float radius,
        const std::string& textureFilePath);
    static ModelData CreateTorusData(
        uint32_t majorDivideCount,
        uint32_t minorDivideCount,
        float majorRadius,
        float minorRadius,
        const std::string& textureFilePath);
    static ModelData CreateCylinderData(
        uint32_t divideCount,
        float topRadius,
        float bottomRadius,
        float height,
        const std::string& textureFilePath);
    static ModelData CreateConeData(
        uint32_t divideCount,
        float radius,
        float height,
        const std::string& textureFilePath);
    static ModelData CreateBoxData(
        float width,
        float height,
        float depth,
        const std::string& textureFilePath);

private:
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateMaterial();

private:
    ModelCommon* modelCommon_ = nullptr;

    ModelData modelData_;

    ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

    ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};
