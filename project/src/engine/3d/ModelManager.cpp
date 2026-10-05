#include "engine/3d/ModelManager.h"
#include "engine/3d/Model.h"
#include "engine/3d/ModelCommon.h"
#include "engine/base/DirectXCommon.h"

ModelManager* ModelManager::instance = nullptr;

ModelManager* ModelManager::GetInstance()
{
    if (!instance) {
        instance = new ModelManager();
    }
    return instance;
}

void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    if (modelCommon_) {
        return;
    }

    modelCommon_ = new ModelCommon();
    modelCommon_->Initialize(dxCommon, srvManager);
}

void ModelManager::Finalize()
{
    models_.clear();

    delete modelCommon_;
    modelCommon_ = nullptr;

    delete instance;
    instance = nullptr;
}

void ModelManager::LoadModel(const std::string& filePath)
{
    // すでに読み込み済みなら何もしない
    if (models_.contains(filePath)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_, "resources", filePath);

    models_.insert(std::make_pair(filePath, std::move(model)));
}

void ModelManager::CreateTriangle(
    const std::string& name,
    float width,
    float height,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_,
        Model::CreateTriangleData(width, height, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreatePlane(
    const std::string& name,
    float width,
    float height,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_,
        Model::CreatePlaneData(width, height, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateCircle(
    const std::string& name,
    uint32_t divideCount,
    float radius,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_,
        Model::CreateCircleData(divideCount, radius, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateHeart(
    const std::string& name,
    uint32_t divideCount,
    float radius,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_,
        Model::CreateHeartData(divideCount, radius, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateRing(
    const std::string& name,
    uint32_t divideCount,
    float outerRadius,
    float innerRadius,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_,
        Model::CreateRingData(
            divideCount,
            outerRadius,
            innerRadius,
            textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateArc(const std::string& name, uint32_t divideCount,
    float outerRadius, float width, float arcRadians, const std::string& textureFilePath)
{
    if (models_.contains(name)) { return; }
    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_, Model::CreateArcData(divideCount, outerRadius, width, arcRadians, textureFilePath));
    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateSphere(
    const std::string& name,
    uint32_t latDivideCount,
    uint32_t lonDivideCount,
    float radius,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_,
        Model::CreateSphereData(
            latDivideCount,
            lonDivideCount,
            radius,
            textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateTorus(
    const std::string& name,
    uint32_t majorDivideCount,
    uint32_t minorDivideCount,
    float majorRadius,
    float minorRadius,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_,
        Model::CreateTorusData(
            majorDivideCount,
            minorDivideCount,
            majorRadius,
            minorRadius,
            textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateCylinder(
    const std::string& name,
    uint32_t divideCount,
    float topRadius,
    float bottomRadius,
    float height,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_,
        Model::CreateCylinderData(
            divideCount,
            topRadius,
            bottomRadius,
            height,
            textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateCone(
    const std::string& name,
    uint32_t divideCount,
    float radius,
    float height,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_,
        Model::CreateConeData(divideCount, radius, height, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateBox(
    const std::string& name,
    float width,
    float height,
    float depth,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        modelCommon_,
        Model::CreateBoxData(width, height, depth, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::SetEnvironmentTexturePath(const std::string& texturePath)
{
    if (modelCommon_) {
        modelCommon_->SetEnvironmentTexturePath(texturePath);
    }
}

Model* ModelManager::FindModel(const std::string& filePath)
{
    if (models_.contains(filePath)) {
        return models_.at(filePath).get();
    }
    return nullptr;
}
