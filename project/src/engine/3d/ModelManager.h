// モデルを名前で保管し、同じモデルの再利用と段階的な読み込みを担当する

#pragma once
#include <map>
#include <memory>
#include <string>
#include <future>
#include "engine/3d/Model.h"

class Model;
class ModelCommon;
class DirectXCommon;
class SrvManager;

class ModelManager {
public:
    static ModelManager* GetInstance();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Finalize();

    // モデル読み込み
    void LoadModel(const std::string& filePath);
    // 一度に解析するモデルは1つ　readyになったデータのGPU生成は呼出し側で行う
    bool PreloadModelStep(const std::string& filePath);
    void CreateMesh(const std::string& name, ModelData&& data);
    void CreateTriangle(
        const std::string& name,
        float width,
        float height,
        const std::string& textureFilePath);
    void CreatePlane(
        const std::string& name,
        float width,
        float height,
        const std::string& textureFilePath);
    void CreateCircle(
        const std::string& name,
        uint32_t divideCount,
        float radius,
        const std::string& textureFilePath);
    void CreateHeart(
        const std::string& name,
        uint32_t divideCount,
        float radius,
        const std::string& textureFilePath);
    void CreateRing(
        const std::string& name,
        uint32_t divideCount,
        float outerRadius,
        float innerRadius,
        const std::string& textureFilePath);
    void CreateArc(
        const std::string& name,
        uint32_t divideCount,
        float outerRadius,
        float width,
        float arcRadians,
        const std::string& textureFilePath);
    void CreateSphere(
        const std::string& name,
        uint32_t latDivideCount,
        uint32_t lonDivideCount,
        float radius,
        const std::string& textureFilePath);
    void CreateTorus(
        const std::string& name,
        uint32_t majorDivideCount,
        uint32_t minorDivideCount,
        float majorRadius,
        float minorRadius,
        const std::string& textureFilePath);
    void CreateCylinder(
        const std::string& name,
        uint32_t divideCount,
        float topRadius,
        float bottomRadius,
        float height,
        const std::string& textureFilePath);
    void CreateCone(
        const std::string& name,
        uint32_t divideCount,
        float radius,
        float height,
        const std::string& textureFilePath);
    void CreateBox(
        const std::string& name,
        float width,
        float height,
        float depth,
        const std::string& textureFilePath);
    void SetEnvironmentTexturePath(const std::string& texturePath);

    // モデル取得
    Model* FindModel(const std::string& filePath);

private:
    ModelManager() = default;
    ~ModelManager() = default;
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

private:
    static ModelManager* instance;

    std::map<std::string, std::unique_ptr<Model>> models_;
    ModelCommon* modelCommon_ = nullptr;
    std::future<ModelData> pendingModel_;
    std::string pendingModelPath_;
    std::unique_ptr<Model> pendingGpuModel_;
    size_t pendingMaterialTexture_ = 0;
};
