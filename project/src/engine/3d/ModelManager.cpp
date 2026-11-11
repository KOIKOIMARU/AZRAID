#include "engine/3d/ModelManager.h"
#include "engine/3d/Model.h"
#include "engine/3d/ModelCommon.h"
#include "engine/base/DirectXCommon.h"
#include <cassert>
#include "engine/3d/TextureManager.h"

ModelManager* ModelManager::instance = nullptr;

ModelManager* ModelManager::GetInstance()
{
    if (!instance) {
        instance = new ModelManager();
    }
    return instance;
}

// モデルをGPUへ渡すために、描画とテクスチャの共通機能を受け取る。
void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    if (modelCommon_) {
        return;
    }

    modelCommon_ = new ModelCommon();
    modelCommon_->Initialize(dxCommon, srvManager);
}

// モデルの読込作業と保管しているモデルを片づけ、共通機能を解放する。
void ModelManager::Finalize()
{
    // workerはCPUのローカルデータだけを所有。rendererを破棄する前にjoinする。
    if (pendingModel_.valid()) { pendingModel_.wait(); }
    pendingModel_ = {};
    pendingModelPath_.clear();
    pendingGpuModel_.reset();
    models_.clear();

    delete modelCommon_;
    modelCommon_ = nullptr;

    delete instance;
    instance = nullptr;
}

// 指定のモデルが準備済みになるまで、段階的な読込を進める。
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

// 頂点などの読み込みを別スレッドで進め、受け取った結果から描画用モデルを準備する。
bool ModelManager::PreloadModelStep(const std::string& filePath)
{
    if (models_.contains(filePath)) { return true; }
    if (pendingGpuModel_) {
        assert(pendingModelPath_ == filePath);
        const auto& materials = pendingGpuModel_->GetMaterials();
        while (pendingMaterialTexture_ < materials.size() * 2) {
            const auto& material = materials[pendingMaterialTexture_ / 2];
            const bool color = pendingMaterialTexture_ % 2 == 0;
            auto* textures = TextureManager::GetInstance();
            const auto& path = color ? material.textureFilePath : material.normalTextureFilePath;
            const bool cached = textures->IsTextureLoaded(path);
            if (!textures->PreloadTextureStep(path, color)) { return false; }
            ++pendingMaterialTexture_;
            // 同じ材質画像の重複はその場で飛ばす。新しいGPU画像は1フレーム1枚まで。
            if (!cached) { return false; }
        }
        models_.emplace(filePath, std::move(pendingGpuModel_));
        pendingModelPath_.clear();
        return true;
    }
    if (!pendingModel_.valid()) {
        pendingModelPath_ = filePath;
        pendingModel_ = std::async(std::launch::async, [filePath]() {
            return Model::LoadCpuData("resources", filePath);
        });
        return false;
    }
    assert(pendingModelPath_ == filePath);
    if (pendingModelPath_ != filePath || pendingModel_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return false;
    }
    auto data = pendingModel_.get(); // 完了後だけ受け取る。未完了のfutureを待たない。
    pendingGpuModel_ = std::make_unique<Model>();
    pendingGpuModel_->InitializeGeometry(modelCommon_, std::move(data));
    pendingMaterialTexture_ = 0;
    return false;
}

// 三角形モデルを生成し、名前で共有できるよう登録する。
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

// 平面モデルを生成し、同じ名前で使い回せるよう登録する。
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

// 円盤モデルを生成し、指定名で取り出せるよう登録する。
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

// ハート型のモデルを生成し、指定名で取り出せるよう登録する。
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

// リングの幅や分割数を使ってモデルを生成し、指定名で登録する。
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

// 呼び出し側で作った頂点・材質データをモデルとして登録する。
void ModelManager::CreateMesh(const std::string& name, ModelData&& data)
{
    if (models_.contains(name)) { return; }
    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_, std::move(data));
    models_.insert(std::make_pair(name, std::move(model)));
}

// 指定された範囲の円弧をモデル化し、名前で再利用できるようにする。
void ModelManager::CreateArc(const std::string& name, uint32_t divideCount,
    float outerRadius, float width, float arcRadians, const std::string& textureFilePath)
{
    if (models_.contains(name)) { return; }
    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_, Model::CreateArcData(divideCount, outerRadius, width, arcRadians, textureFilePath));
    models_.insert(std::make_pair(name, std::move(model)));
}

// 球の分割数とサイズを使ってモデルを作り、名前で再利用できるようにする。
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

// 輪と断面の寸法からトーラスを作り、指定名で登録する。
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

// 上下の半径と長さから円柱を作り、指定名で登録する。
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

// 円すいを指定寸法で作り、名前で取り出せるようにする。
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

// 箱の幅・高さ・奥行きからモデルを作り、名前で登録する。
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

// モデルの共通設定が初期化済みなら、環境反射に使う画像のパスを渡す。
void ModelManager::SetEnvironmentTexturePath(const std::string& texturePath)
{
    if (modelCommon_) {
        modelCommon_->SetEnvironmentTexturePath(texturePath);
    }
}

// 指定名で登録済みのモデルを探す。見つからない場合はnullptrを返す。
Model* ModelManager::FindModel(const std::string& filePath)
{
    if (models_.contains(filePath)) {
        return models_.at(filePath).get();
    }
    return nullptr;
}
