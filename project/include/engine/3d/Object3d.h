// 配置された3Dオブジェクト1個分の姿勢、色、アニメーション状態とGPUへの描画を担当する。

#pragma once
#include <cstddef>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"
#include "engine/3d/Model.h"

using Microsoft::WRL::ComPtr;
using namespace Math;

class Object3dCommon;
class Camera;

struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 WorldInverseTranspose;
};

struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
    Matrix4x4 lightViewProjection;
    float shadowStrength;
    float shadowBias;
    float shadowNormalBias;
    float shadowMapEnabled;
};

struct CameraForGPU {
    Vector3 worldPosition;
    float padding;
};

struct PointLight {
    Vector4 color;
    Vector3 position;
    float intensity;
    float radius;
    float decay;
    float padding[2];
};

struct SpotLight {
    Vector4 color;
    Vector3 position;
    float intensity;
    Vector3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
    float padding;
};

struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix;
    Matrix4x4 skeletonSpaceInverseTransposeMatrix;
};

constexpr uint32_t kNumMaxSkeletonJoints = 128;

struct SkinningPaletteForGPU {
    int32_t enableSkinning;
    float padding[3];
    WellForGPU palette[kNumMaxSkeletonJoints];
};

struct SkinningVertexForCompute {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

struct VertexInfluenceForCompute {
    Vector4 weight;
    std::array<int32_t, kNumMaxInfluence> jointIndices;
};

struct SkinningInformationForCompute {
    uint32_t numVertices;
    float padding[3];
};

class Object3d {
public:
    void Initialize(Object3dCommon* object3dCommon);
    void Update();
    void Draw(ModelDrawPass drawPass = ModelDrawPass::All);
    bool HasTransparentMaterials() const { return model_ && model_->HasTransparentMaterials(); }
    void DrawShadow(const Matrix4x4& lightViewProjection);
    void UpdateAnimation(float deltaTime);

    void SetModel(Model* model);

    // setter
    void SetScale(const Vector3& scale);
    void SetRotate(const Vector3& rotate);
    void SetQuaternionRotate(const Quaternion& rotate);
    void SetTranslate(const Vector3& translate);
    void SetDirectionalLightDirection(const Vector3& direction);
    void SetDirectionalLightIntensity(float intensity);
    void SetPointLightPosition(const Vector3& position);
    void SetPointLightIntensity(float intensity);
    void SetPointLightColor(const Vector3& color);
    void SetSpotLightPosition(const Vector3& position);
    void SetSpotLightDirection(const Vector3& direction);
    void SetSpotLightIntensity(float intensity);
    void SetEnvironmentCoefficient(float coefficient);
    void SetColor(const Vector4& color);
    void SetShininess(float shininess);
    void SetSpecularColor(const Vector3& color);
    void SetRoughness(float roughness);
    void SetMetallic(float metallic);
    void SetAlphaReference(float alphaReference);
    void SetShadowReceiveStrength(float strength);
    void SetUVTransform(const Matrix4x4& uvTransform);
    void SetLightingMode(int32_t lightingMode);
    void SetFlameAnimation(float seconds, float seed);
    void SetTextureFilePath(const std::string& textureFilePath);
    bool HasSkeleton() const { return hasSkeleton_; }
    const Skeleton& GetSkeleton() const { return skeleton_; }
    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

    // getter
    Vector3 GetScale() const;
    Vector3 GetRotate() const;
    Vector3 GetTranslate() const;
    float GetEnvironmentCoefficient() const;
    Vector4 GetColor() const;
    float GetShininess() const;
    Vector3 GetSpecularColor() const;
    float GetRoughness() const;
    float GetMetallic() const;
    float GetAlphaReference() const;
    float GetShadowReceiveStrength() const { return shadowReceiveStrength_; }
    int32_t GetLightingMode() const;
    const std::string& GetTextureFilePath() const;

    void SetModel(const std::string& filePath);

    void SetCamera(Camera* camera) { this->camera_ = camera; }


private:
    void CreateConstantBuffer();
    void CreateTransformationMatrix();
    void CreateDirectionalLight();
    void CreateCameraResource();
    void CreatePointLight();
    void CreateSpotLight();
    void CreateMaterialOverride();
    void CreateSkinningPalette();
    void CreateComputeSkinningPipeline();
    void InitializeSkinning();
    void UpdateSkinningPalette();
    void InitializeComputeSkinningResources();
    void ReleaseComputeSkinningResources();
    void DispatchComputeSkinning();

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Model* model_ = nullptr;

    Transform transform_;
    Quaternion quaternionRotate_{ 0.0f, 0.0f, 0.0f, 1.0f };
    bool useQuaternionRotate_ = false;
    Camera* camera_ = nullptr;
    float lastCameraAspectRatio_ = -1.0f; // 転送済み行列の縦横比。停止中のリサイズ検出用。
    Matrix4x4 worldMatrix_ = MakeIdentity4x4();

    // 物体ごとに一度だけ確保する。各CBVの先頭は256バイト境界、材質はModel::Draw用に先頭。
    static constexpr size_t kTransformOffset = (sizeof(Material) + 255u) & ~size_t(255u);
    TransformationMatrix* transformationMatrixData_ = nullptr;
    // 同じUpload Buffer内の別領域。影描画後に通常行列を書いてもGPUの参照先を壊さない。
    static constexpr size_t kTransformBufferStride =
        (sizeof(TransformationMatrix) + 255u) & ~size_t(255u);
    static constexpr size_t kDirectionalLightOffset = kTransformOffset + kTransformBufferStride * 2;
    static constexpr size_t kCameraOffset = kDirectionalLightOffset + ((sizeof(DirectionalLight) + 255u) & ~size_t(255u));
    static constexpr size_t kPointLightOffset = kCameraOffset + ((sizeof(CameraForGPU) + 255u) & ~size_t(255u));
    static constexpr size_t kSpotLightOffset = kPointLightOffset + ((sizeof(PointLight) + 255u) & ~size_t(255u));
    static constexpr size_t kSkinningOffset = kSpotLightOffset + ((sizeof(SpotLight) + 255u) & ~size_t(255u));
    TransformationMatrix* shadowTransformationMatrixData_ = nullptr;

    DirectionalLight* directionalLightData_ = nullptr;
    float shadowReceiveStrength_ = 1.0f;

    CameraForGPU* cameraData_ = nullptr;

    PointLight* pointLightData_ = nullptr;

    SpotLight* spotLightData_ = nullptr;

    ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
    std::string textureFilePath_;
    bool hasTextureOverride_ = false;

    SkinningPaletteForGPU* skinningPaletteData_ = nullptr;
    ComPtr<ID3D12RootSignature> computeRootSignature_;
    ComPtr<ID3D12PipelineState> computePipelineState_;
    ComPtr<ID3D12Resource> computeInputVertexResource_;
    ComPtr<ID3D12Resource> computeInfluenceResource_;
    ComPtr<ID3D12Resource> computeMatrixPaletteResource_;
    ComPtr<ID3D12Resource> computeOutputVertexResource_;
    ComPtr<ID3D12Resource> computeSkinningInfoResource_;
    WellForGPU* computeMatrixPaletteData_ = nullptr;
    SkinningInformationForCompute* computeSkinningInfoData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW computeOutputVertexBufferView_{};
    D3D12_RESOURCE_STATES computeOutputVertexState_ = D3D12_RESOURCE_STATE_COMMON;
    uint32_t computePaletteSrvIndex_ = UINT32_MAX;
    uint32_t computeInputVertexSrvIndex_ = UINT32_MAX;
    uint32_t computeInfluenceSrvIndex_ = UINT32_MAX;
    uint32_t computeOutputVertexUavIndex_ = UINT32_MAX;
    bool enableComputeSkinning_ = false;
    Skeleton skeleton_{};
    std::vector<Matrix4x4> inverseBindPoseMatrices_;
    bool hasSkeleton_ = false;
    float animationTime_ = 0.0f;
};
