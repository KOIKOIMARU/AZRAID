// ===== EditorScene.cpp =====
// ステージ配置の編集、ファイルの読み書き、エディタ内での試遊を取りまとめる

#include "engine/scene/EditorScene.h"

#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Model.h"
#include "engine/3d/Camera.h"
#include "engine/3d/TextureManager.h"
#include "engine/3d/ParticleManager.h"
#include "engine/3d/ParticleEmitter.h"
#include "engine/3d/Skybox.h"
#include "engine/base/ImGuiManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/scene/SceneSerializer.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"
#include "engine/3d/ModelManager.h"
#include "engine/io/Input.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace {

constexpr const char* kInspectorModelItems[] = {
    "primitive_plane",
    "primitive_ring",
    "primitive_cylinder",
    "primitive_sphere",
    "primitive_triangle",
    "primitive_circle",
    "primitive_box",
    "primitive_torus",
    "primitive_cone",
    "AnimatedCube/AnimatedCube.gltf",
    "simpleSkin/simpleSkin.gltf",
    "human/sneakWalk.gltf",
    "human/walk.gltf"
};

constexpr const char* kSceneFilePath =
"resources/game_scene.json";
constexpr const char* kEditorSettingsPath =
"resources/game_editor_settings.json";
constexpr size_t kMaxTransformHistoryCount = 64;
constexpr float kTransformHistoryEpsilon = 0.0001f;
constexpr Math::Vector3 kGameCameraRotate = { 0.15f, 0.0f, 0.0f };
constexpr Math::Vector3 kGameCameraTranslate = { 0.0f, 2.5f, -13.0f };
constexpr float kGameCameraFovY = 0.5f;

bool IsSameVector3(const Math::Vector3& lhs, const Math::Vector3& rhs)
{
    return std::fabs(lhs.x - rhs.x) <= kTransformHistoryEpsilon &&
        std::fabs(lhs.y - rhs.y) <= kTransformHistoryEpsilon &&
        std::fabs(lhs.z - rhs.z) <= kTransformHistoryEpsilon;
}

// 設定ファイルから指定キーの整数を探す　見つからなければ出力値を保ったままfalseを返す
bool ExtractSettingsInt(const std::string& source, const char* key, int& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*(-?[0-9]+)"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = std::stoi(match[1].str());
    return true;
}

// 設定ファイルから指定キーの小数を探す　見つからなければ出力値を保ったままfalseを返す
bool ExtractSettingsFloat(
    const std::string& source,
    const char* key,
    float& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*([-+0-9.eE]+)"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = std::stof(match[1].str());
    return true;
}

// 設定ファイルから指定キーのtrueまたはfalseを読み取る
bool ExtractSettingsBool(const std::string& source, const char* key, bool& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*(true|false)"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = match[1].str() == "true";
    return true;
}

// 設定ファイルから指定キーの3成分を読み、XYZの順で出力する
bool ExtractSettingsVector3(
    const std::string& source,
    const char* key,
    Math::Vector3& out)
{
    const std::regex pattern(
        std::string("\"") + key +
        "\"\\s*:\\s*\\[\\s*([-+0-9.eE]+)\\s*,\\s*([-+0-9.eE]+)\\s*,\\s*([-+0-9.eE]+)\\s*\\]"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out.x = std::stof(match[1].str());
    out.y = std::stof(match[2].str());
    out.z = std::stof(match[3].str());
    return true;
}

Math::Vector3 Add(const Math::Vector3& a, const Math::Vector3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Math::Vector3 Subtract(const Math::Vector3& a, const Math::Vector3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

// カメラの上下角と左右角から、前進操作に使う単位ベクトルを求める
Math::Vector3 MakeForwardVector(const Math::Vector3& rotate)
{
    const float cosPitch = std::cos(rotate.x);
    const float sinPitch = std::sin(rotate.x);
    const float cosYaw = std::cos(rotate.y);
    const float sinYaw = std::sin(rotate.y);

    return Math::Normalize({
        sinYaw * cosPitch,
        -sinPitch,
        cosYaw * cosPitch
    });
}

Math::Vector3 MakeRightVector(const Math::Vector3& rotate)
{
    const float yaw = rotate.y + (3.14159265358979323846f * 0.5f);
    return Math::Normalize({
        std::sin(yaw),
        0.0f,
        std::cos(yaw)
    });
}

Math::Vector3 ExtractTranslation(const Math::Matrix4x4& matrix)
{
    return {
        matrix.m[3][0],
        matrix.m[3][1],
        matrix.m[3][2]
    };
}

Math::Vector3 MakeBoneRotate(const Math::Vector3& direction)
{
    const float horizontalLength =
        std::sqrt(direction.x * direction.x + direction.z * direction.z);

    return {
        std::atan2(-direction.y, horizontalLength),
        std::atan2(direction.x, direction.z),
        0.0f
    };
}

} // namespace

EditorScene::EditorScene() = default;
EditorScene::~EditorScene() = default;

// 試遊中はゲーム側、編集時はエディタ側の画面効果を返す
int EditorScene::GetPostEffectMode() const
{
    if (editorManager_.IsPlayMode() && playController_.IsRunning()) {
        return playController_.GetPostEffectMode();
    }
    return postEffectMode_;
}

// 試遊中の投影行列を優先し、取得できなければ編集用カメラの行列を使う
const Math::Matrix4x4& EditorScene::GetProjectionMatrix() const
{
    static const Math::Matrix4x4 kIdentity = Math::MakeIdentity4x4();
    if (editorManager_.IsPlayMode() && playController_.IsRunning()) {
        if (const Math::Matrix4x4* projection =
                playController_.GetProjectionMatrix()) {
            return *projection;
        }
    }
    return camera_ ? camera_->GetProjectionMatrix() : kIdentity;
}

// 配置編集で使う描画と入力を準備し、編集対象を画面に表示できる状態にする
void EditorScene::Initialize() {
    const std::string environmentTexturePath =
        "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";

    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);
    object3dCommon_->SetEnvironmentTexturePath(environmentTexturePath);

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate(kGameCameraRotate);
    camera_->SetTranslate(kGameCameraTranslate);
    camera_->SetFovY(kGameCameraFovY);
    object3dCommon_->SetDefaultCamera(camera_.get());

    playController_.SetSystems(
        dxCommon_,
        srvManager_,
        spriteCommon_,
        imguiManager_,
        input_);

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(
        dxCommon_,
        srvManager_,
        environmentTexturePath
    );

    ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);
    ModelManager::GetInstance()->SetEnvironmentTexturePath(
        environmentTexturePath
    );
    ModelManager::GetInstance()->CreatePlane(
        "primitive_plane",
        8.0f,
        8.0f,
        "resources/checkerBoard.png"
    );
    ModelManager::GetInstance()->CreateTriangle(
        "primitive_triangle",
        1.6f,
        1.6f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateCircle(
        "primitive_circle",
        32,
        0.9f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateRing(
        "primitive_ring",
        32,
        2.0f,
        1.0f,
        "resources/gradationLine.png"
    );
    ModelManager::GetInstance()->CreateSphere(
        "primitive_sphere",
        16,
        32,
        1.0f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateSphere(
        "debug_joint_sphere",
        16,
        32,
        1.0f,
        "resources/human/white.png"
    );
    ModelManager::GetInstance()->CreateTorus(
        "primitive_torus",
        32,
        16,
        0.8f,
        0.3f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateCylinder(
        "primitive_cylinder",
        32,
        1.2f,
        1.2f,
        2.5f,
        "resources/gradationLine.png"
    );
    ModelManager::GetInstance()->CreateCone(
        "primitive_cone",
        32,
        0.8f,
        1.6f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateBox(
        "primitive_box",
        1.5f,
        1.5f,
        1.5f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateBox(
        "game_player",
        1.0f,
        0.45f,
        1.3f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateSphere(
        "game_enemy",
        16,
        32,
        0.9f,
        "resources/uvChecker.png"
    );
    ModelManager::GetInstance()->CreateSphere(
        "game_bullet",
        12,
        24,
        0.35f,
        "resources/gradationLine.png"
    );
    ModelManager::GetInstance()->CreateBox(
        "debug_bone_box",
        1.0f,
        1.0f,
        1.0f,
        "resources/human/white.png"
    );
    ModelManager::GetInstance()->LoadModel("AnimatedCube/AnimatedCube.gltf");
    ModelManager::GetInstance()->LoadModel("simpleSkin/simpleSkin.gltf");
    ModelManager::GetInstance()->LoadModel("human/sneakWalk.gltf");
    ModelManager::GetInstance()->LoadModel("human/walk.gltf");

    editorManager_.SetSceneFilePath(kSceneFilePath);
    LoadEditorSettings();
    LoadPrimitiveObjectsFromSceneFile(editorManager_.GetSceneFilePath());
    camera_->SetRotate(kGameCameraRotate);
    camera_->SetTranslate(kGameCameraTranslate);
    camera_->SetFovY(kGameCameraFovY);

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");
    TextureManager::GetInstance()->LoadTexture("resources/gradationLine.png");
    TextureManager::GetInstance()->LoadTexture("resources/circle2.png");

    showSkinningSamples_ = false;
    showSkeletonDebug_ = false;

    if (editorManager_.ConsumePlayOnNextEditorOpenRequest()) {
        editorManager_.SetMode(EditorManager::EditorMode::Play);
    }
}

// シーンの読込に成功したら設定と配置物を復元し、旧配置物を参照していた編集履歴を消す
bool EditorScene::LoadPrimitiveObjectsFromSceneFile(const char* path) {
    std::vector<SceneSerializer::ObjectRecord> records;
    SceneSerializer::SceneSettings settings{};
    if (!SceneSerializer::LoadScene(path, records, settings)) {
        return false;
    }

    ApplySceneSettings(settings);
    ClearTransformHistory();

    const int modelItemCount =
        static_cast<int>(
            sizeof(kInspectorModelItems) / sizeof(kInspectorModelItems[0]));

    editorObjects_.clear();

    for (const SceneSerializer::ObjectRecord& record : records) {
        if (record.modelIndex < 0 || modelItemCount <= record.modelIndex) {
            continue;
        }

        auto primitiveObject = std::make_unique<Object3d>();
        primitiveObject->Initialize(object3dCommon_.get());
        primitiveObject->SetModel(kInspectorModelItems[record.modelIndex]);
        primitiveObject->SetTranslate(record.translate);
        primitiveObject->SetRotate(record.rotate);
        primitiveObject->SetScale(record.scale);
        primitiveObject->SetColor(record.color);
        primitiveObject->SetAlphaReference(record.alphaReference);
        primitiveObject->SetLightingMode(record.lightingMode);
        if (!record.textureFilePath.empty()) {
            primitiveObject->SetTextureFilePath(record.textureFilePath);
        }
        primitiveObject->SetEnvironmentCoefficient(0.0f);

        EditorObject editorObject{};
        editorObject.name = record.name.empty() ?
            kInspectorModelItems[record.modelIndex] :
            record.name;
        editorObject.modelIndex = record.modelIndex;
        editorObject.object = std::move(primitiveObject);
        editorObjects_.push_back(std::move(editorObject));
    }

    AddGameplayPreviewObjects();
    editorManager_.ResetSelectedObjectIndex();
    return true;
}

// 編集画面で距離感を確認できるよう、プレイヤーと敵の見本を決まった位置に置く
void EditorScene::AddGameplayPreviewObjects()
{
    AddGameplayPreviewObject(
        "Player Preview",
        "game_player",
        { 0.0f, 0.0f, 0.0f },
        { 0.8f, 0.35f, 1.0f },
        { 0.25f, 0.65f, 1.0f, 1.0f });

    const Math::Vector3 enemyPositions[] = {
        { -4.0f, -1.5f, 18.0f },
        { 0.0f, 0.0f, 22.0f },
        { 4.0f, 1.5f, 26.0f }
    };
    for (int index = 0; index < 3; ++index) {
        AddGameplayPreviewObject(
            ("Enemy Preview " + std::to_string(index + 1)).c_str(),
            "game_enemy",
            enemyPositions[index],
            { 0.9f, 0.9f, 0.9f },
            { 1.0f, 0.25f, 0.25f, 1.0f });
    }
}

// プレビュー専用の物体を生成し、通常の配置物と区別できるように登録する
void EditorScene::AddGameplayPreviewObject(
    const char* name,
    const char* modelName,
    const Math::Vector3& translate,
    const Math::Vector3& scale,
    const Math::Vector4& color)
{
    auto previewObject = std::make_unique<Object3d>();
    previewObject->Initialize(object3dCommon_.get());
    previewObject->SetModel(modelName);
    previewObject->SetTranslate(translate);
    previewObject->SetScale(scale);
    previewObject->SetColor(color);
    previewObject->SetEnvironmentCoefficient(0.0f);

    EditorObject editorObject{};
    editorObject.name = name ? name : "Gameplay Preview";
    editorObject.modelIndex = 0;
    editorObject.visible = true;
    editorObject.runtimePreview = true;
    editorObject.object = std::move(previewObject);
    editorObjects_.push_back(std::move(editorObject));
}

bool EditorScene::SaveSceneToFile(const char* path) const
{
    return SceneSerializer::SaveScene(
        path,
        BuildSceneObjectRecords(),
        BuildSceneSettings());
}

// モデル一覧から配置物を生成し、追加した物体をインスペクターの選択対象にする
void EditorScene::AddEditorPrimitive(int modelIndex)
{
    const int modelItemCount =
        static_cast<int>(
            sizeof(kInspectorModelItems) / sizeof(kInspectorModelItems[0]));

    if (modelIndex < 0 || modelIndex >= modelItemCount) {
        return;
    }

    auto primitiveObject = std::make_unique<Object3d>();
    primitiveObject->Initialize(object3dCommon_.get());
    primitiveObject->SetModel(kInspectorModelItems[modelIndex]);
    primitiveObject->SetTranslate({
        0.0f,
        1.0f,
        static_cast<float>(editorObjects_.size()) + 5.0f
        });
    primitiveObject->SetEnvironmentCoefficient(0.0f);

    EditorObject editorObject{};
    editorObject.name =
        "Added Primitive " + std::to_string(editorObjects_.size() + 1);
    editorObject.modelIndex = modelIndex;
    editorObject.object = std::move(primitiveObject);

    editorObjects_.push_back(std::move(editorObject));
    ClearTransformHistory();
}

// 選択中の配置物を削除する　プレビュー専用の物体は削除対象から外す
void EditorScene::RemoveSelectedEditorObject()
{
    const int objectIndex = editorManager_.GetSelectedObjectIndex();

    if (objectIndex < 0 ||
        objectIndex >= static_cast<int>(editorObjects_.size())) {
        return;
    }

    if (editorObjects_[objectIndex].runtimePreview) {
        return;
    }

    editorObjects_.erase(editorObjects_.begin() + objectIndex);
    editorManager_.ResetSelectedObjectIndex();
    ClearTransformHistory();
}

// 選択中の配置物の見た目と変形を引き継いだ複製を追加する
void EditorScene::DuplicateSelectedEditorObject()
{
    const int objectIndex = editorManager_.GetSelectedObjectIndex();

    if (objectIndex < 0 ||
        objectIndex >= static_cast<int>(editorObjects_.size())) {
        return;
    }

    const EditorObject& source = editorObjects_[objectIndex];
    if (source.runtimePreview) {
        return;
    }

    auto duplicatedObject = std::make_unique<Object3d>();
    duplicatedObject->Initialize(object3dCommon_.get());
    duplicatedObject->SetModel(kInspectorModelItems[source.modelIndex]);

    Math::Vector3 translate = source.object->GetTranslate();
    translate.x += 1.0f;
    translate.z += 1.0f;
    duplicatedObject->SetTranslate(translate);
    duplicatedObject->SetRotate(source.object->GetRotate());
    duplicatedObject->SetScale(source.object->GetScale());
    duplicatedObject->SetColor(source.object->GetColor());
    duplicatedObject->SetAlphaReference(source.object->GetAlphaReference());
    duplicatedObject->SetLightingMode(source.object->GetLightingMode());
    duplicatedObject->SetTextureFilePath(source.object->GetTextureFilePath());
    duplicatedObject->SetEnvironmentCoefficient(
        source.object->GetEnvironmentCoefficient());

    EditorObject duplicatedEditorObject{};
    duplicatedEditorObject.name = source.name + " Copy";
    duplicatedEditorObject.modelIndex = source.modelIndex;
    duplicatedEditorObject.visible = source.visible;
    duplicatedEditorObject.object = std::move(duplicatedObject);
    editorObjects_.push_back(std::move(duplicatedEditorObject));
    ClearTransformHistory();
}

// 選択中の配置物のモデルと変形を保存用データに写し、選択中のプレハブファイルへ書く
void EditorScene::SaveSelectedEditorObjectAsPrefab()
{
    const int objectIndex = editorManager_.GetSelectedObjectIndex();

    if (objectIndex < 0 ||
        objectIndex >= static_cast<int>(editorObjects_.size())) {
        return;
    }

    const EditorObject& source = editorObjects_[objectIndex];
    if (source.runtimePreview) {
        return;
    }

    SceneSerializer::ObjectRecord record{};
    record.name = source.name;
    record.primitive = true;
    record.modelIndex = source.modelIndex;
    record.translate = source.object->GetTranslate();
    record.rotate = source.object->GetRotate();
    record.scale = source.object->GetScale();
    record.color = source.object->GetColor();
    record.alphaReference = source.object->GetAlphaReference();
    record.lightingMode = source.object->GetLightingMode();
    record.textureFilePath = source.object->GetTextureFilePath();

    SceneSerializer::SaveObjects(editorManager_.GetPrefabFilePath(), { record });
}

// プレハブから最初の配置データを読み、現在のシーンに追加する
void EditorScene::InstantiatePrefab()
{
    std::vector<SceneSerializer::ObjectRecord> records;
    if (!SceneSerializer::LoadObjects(editorManager_.GetPrefabFilePath(), records) ||
        records.empty()) {
        return;
    }

    const SceneSerializer::ObjectRecord& record = records.front();
    const int modelItemCount =
        static_cast<int>(
            sizeof(kInspectorModelItems) / sizeof(kInspectorModelItems[0]));
    if (record.modelIndex < 0 || modelItemCount <= record.modelIndex) {
        return;
    }

    auto object = std::make_unique<Object3d>();
    object->Initialize(object3dCommon_.get());
    object->SetModel(kInspectorModelItems[record.modelIndex]);
    Math::Vector3 translate = record.translate;
    translate.x += 1.5f;
    translate.z += 1.5f;
    object->SetTranslate(translate);
    object->SetRotate(record.rotate);
    object->SetScale(record.scale);
    object->SetColor(record.color);
    object->SetAlphaReference(record.alphaReference);
    object->SetLightingMode(record.lightingMode);
    if (!record.textureFilePath.empty()) {
        object->SetTextureFilePath(record.textureFilePath);
    }
    object->SetEnvironmentCoefficient(0.0f);

    EditorObject editorObject{};
    editorObject.name = record.name.empty() ?
        "Prefab Instance" :
        record.name + " Instance";
    editorObject.modelIndex = record.modelIndex;
    editorObject.object = std::move(object);
    editorObjects_.push_back(std::move(editorObject));
    ClearTransformHistory();
}

// インスペクターから変形やモデルを編集できるよう、配置物への参照を一覧にする
void EditorScene::BuildInspectableObjects(
    std::vector<ImGuiManager::InspectableObject>& inspectObjects)
{
    inspectObjects.clear();

    for (auto& editorObject : editorObjects_) {
        inspectObjects.push_back({
     editorObject.name.c_str(),
     ImGuiManager::InspectableType::Object3d,
     editorObject.object.get(),
     &editorObject.modelIndex,
     &editorObject.name,
     &editorObject.visible,
     editorObject.runtimePreview
            });
    }
}

// エディタの表示中に設定や操作ボタンを描き、UIで変更した値をシーンに反映する
void EditorScene::ShowEditorGui(
    std::vector<ImGuiManager::InspectableObject>& inspectObjects)
{
    if (!editorManager_.IsEditorGuiVisible()) {
        return;
    }

    ImGuiManager::EditorDebugSettings debugSettings{
        {
            blendModeIndex_,
            environmentCoefficient_,
            showSkybox_,
            postEffectMode_
        },
        {
            lightDirection_,
            lightIntensity_,
            pointLightPosition_,
            pointLightIntensity_,
            spotLightPosition_,
            spotLightDirection_,
            spotLightIntensity_
        },
        {
            renderingPanelOpen_,
            objectsPanelOpen_,
            inspectorPanelOpen_,
            materialPanelOpen_,
            lightingPanelOpen_,
            viewportPanelOpen_
        },
        editorManager_.CreateInspectorSettings(
            inspectObjects.data(),
            static_cast<int>(inspectObjects.size()))
    };

    viewportPanelOpen_ = true;

    imguiManager_->ShowEditorController(debugSettings);
    imguiManager_->ShowViewport(
        dxCommon_->GetRenderTextureGpuDescriptorHandle(),
        dxCommon_->GetRenderTextureSize(),
        camera_->GetViewMatrix(),
        camera_->GetProjectionMatrix(),
        viewportPanelOpen_,
        editorManager_.IsPlayMode(),
        debugSettings.inspector.requestStartPlayMode,
        debugSettings.inspector.requestStopPlayMode,
        debugSettings.inspector);
}

// シーンに置かれた物体から保存用の配置データを作る　試遊用の見本は除く
std::vector<SceneSerializer::ObjectRecord> EditorScene::BuildSceneObjectRecords() const
{
    std::vector<SceneSerializer::ObjectRecord> records;

    auto appendObject =
        [&records](
            const char* name,
            const Object3d* object,
            bool primitive,
            int modelIndex) {
            if (!object) {
                return;
            }

            SceneSerializer::ObjectRecord record{};
            record.name = name ? name : "";
            record.primitive = primitive;
            record.modelIndex = modelIndex;
            record.translate = object->GetTranslate();
            record.rotate = object->GetRotate();
            record.scale = object->GetScale();
            record.color = object->GetColor();
            record.alphaReference = object->GetAlphaReference();
            record.lightingMode = object->GetLightingMode();
            record.textureFilePath = object->GetTextureFilePath();
            records.push_back(record);
        };

    for (const EditorObject& editorObject : editorObjects_) {
        if (editorObject.runtimePreview) {
            continue;
        }

        appendObject(
            editorObject.name.c_str(),
            editorObject.object.get(),
            true,
            editorObject.modelIndex);
    }

    return records;
}

// カメラと照明の現在値を、シーンに保存する設定へ詰める
SceneSerializer::SceneSettings EditorScene::BuildSceneSettings() const
{
    SceneSerializer::SceneSettings settings{};
    if (camera_) {
        settings.hasCamera = true;
        settings.cameraTranslate = camera_->GetTranslate();
        settings.cameraRotate = camera_->GetRotate();
    }

    settings.hasLighting = true;
    settings.lightDirection = lightDirection_;
    settings.lightIntensity = lightIntensity_;
    settings.pointLightPosition = pointLightPosition_;
    settings.pointLightIntensity = pointLightIntensity_;
    settings.spotLightPosition = spotLightPosition_;
    settings.spotLightDirection = spotLightDirection_;
    settings.spotLightIntensity = spotLightIntensity_;
    return settings;
}

// 読み込んだカメラ・照明の設定のうち、ファイルに含まれていた項目を反映する
void EditorScene::ApplySceneSettings(
    const SceneSerializer::SceneSettings& settings)
{
    if (settings.hasCamera && camera_) {
        camera_->SetTranslate(settings.cameraTranslate);
        camera_->SetRotate(settings.cameraRotate);
    }

    if (settings.hasLighting) {
        lightDirection_ = settings.lightDirection;
        lightIntensity_ = settings.lightIntensity;
        pointLightPosition_ = settings.pointLightPosition;
        pointLightIntensity_ = settings.pointLightIntensity;
        spotLightPosition_ = settings.spotLightPosition;
        spotLightDirection_ = settings.spotLightDirection;
        spotLightIntensity_ = settings.spotLightIntensity;
    }
}

// 試遊中を除き、UIから受け取った配置変更・履歴・ファイル操作を実行する
void EditorScene::ProcessEditorRequests()
{
    if (editorManager_.IsPlayMode()) {
        return;
    }

    if (editorManager_.ConsumeUndoRequest() && !undoStack_.empty()) {
        const TransformHistoryRecord record = undoStack_.back();
        undoStack_.pop_back();
        ApplyTransformHistory(record, false);
        redoStack_.push_back(record);
        lastTransformHistoryObjectIndex_ = -1;
    }

    if (editorManager_.ConsumeRedoRequest() && !redoStack_.empty()) {
        const TransformHistoryRecord record = redoStack_.back();
        redoStack_.pop_back();
        ApplyTransformHistory(record, true);
        undoStack_.push_back(record);
        lastTransformHistoryObjectIndex_ = -1;
    }

    if (editorManager_.ConsumeSaveObjectsRequest()) {
        SaveSceneToFile(editorManager_.GetSceneFilePath());
    }

    if (editorManager_.ConsumeLoadObjectsRequest()) {
        LoadPrimitiveObjectsFromSceneFile(editorManager_.GetSceneFilePath());
    }

    if (editorManager_.ConsumeRemoveObjectRequest()) {
        RemoveSelectedEditorObject();
    }

    if (editorManager_.ConsumeDuplicateObjectRequest()) {
        DuplicateSelectedEditorObject();
    }

    if (editorManager_.ConsumeSavePrefabRequest()) {
        SaveSelectedEditorObjectAsPrefab();
    }

    if (editorManager_.ConsumeInstantiatePrefabRequest()) {
        InstantiatePrefab();
    }

    if (editorManager_.ConsumeAddObjectRequest()) {
        AddEditorPrimitive(editorManager_.GetAddModelIndex());
    }
}

// 選択中の変形が変わったときに前後の値を記録し、新しい編集でやり直し履歴を消す
void EditorScene::TrackTransformHistory(
    const std::vector<ImGuiManager::InspectableObject>& inspectObjects)
{
    if (editorManager_.IsPlayMode()) {
        return;
    }

    const int selectedIndex = editorManager_.GetSelectedObjectIndex();
    if (selectedIndex < 0 ||
        selectedIndex >= static_cast<int>(inspectObjects.size())) {
        lastTransformHistoryObjectIndex_ = -1;
        return;
    }

    Object3d* object = inspectObjects[selectedIndex].object;
    if (!object || inspectObjects[selectedIndex].readOnly) {
        lastTransformHistoryObjectIndex_ = -1;
        return;
    }

    const Math::Vector3 translate = object->GetTranslate();
    const Math::Vector3 rotate = object->GetRotate();
    const Math::Vector3 scale = object->GetScale();

    if (lastTransformHistoryObjectIndex_ != selectedIndex) {
        lastTransformHistoryObjectIndex_ = selectedIndex;
        lastObservedTranslate_ = translate;
        lastObservedRotate_ = rotate;
        lastObservedScale_ = scale;
        return;
    }

    if (IsSameVector3(lastObservedTranslate_, translate) &&
        IsSameVector3(lastObservedRotate_, rotate) &&
        IsSameVector3(lastObservedScale_, scale)) {
        return;
    }

    TransformHistoryRecord record{};
    record.object = object;
    record.beforeTranslate = lastObservedTranslate_;
    record.beforeRotate = lastObservedRotate_;
    record.beforeScale = lastObservedScale_;
    record.afterTranslate = translate;
    record.afterRotate = rotate;
    record.afterScale = scale;
    undoStack_.push_back(record);
    if (undoStack_.size() > kMaxTransformHistoryCount) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();

    lastObservedTranslate_ = translate;
    lastObservedRotate_ = rotate;
    lastObservedScale_ = scale;
}

// シーンを読み直すなど、配置物への参照が変わるときに変形履歴と直前の選択をリセットする
void EditorScene::ClearTransformHistory()
{
    undoStack_.clear();
    redoStack_.clear();
    lastTransformHistoryObjectIndex_ = -1;
}

// 元に戻すときは変更前、やり直すときは変更後の位置・回転・拡縮を適用する
void EditorScene::ApplyTransformHistory(
    const TransformHistoryRecord& record,
    bool useAfter)
{
    if (!record.object) {
        return;
    }

    record.object->SetTranslate(
        useAfter ? record.afterTranslate : record.beforeTranslate);
    record.object->SetRotate(
        useAfter ? record.afterRotate : record.beforeRotate);
    record.object->SetScale(
        useAfter ? record.afterScale : record.beforeScale);
}

// エディタで調整した平行光と点光源の値を、指定のモデルへ反映する
void EditorScene::ApplyLightingToObject(Object3d* object)
{
    if (!object) {
        return;
    }

    object->SetDirectionalLightDirection(lightDirection_);
    object->SetDirectionalLightIntensity(lightIntensity_);
    object->SetPointLightPosition(pointLightPosition_);
    object->SetPointLightIntensity(pointLightIntensity_);
    object->SetSpotLightPosition(spotLightPosition_);
    object->SetSpotLightDirection(spotLightDirection_);
    object->SetSpotLightIntensity(spotLightIntensity_);
}

// エディタのアニメーション確認用の処理をまとめる　現在は条件付きコンパイルで無効
void EditorScene::UpdateAnimations(float deltaTime)
{
    (void)deltaTime;
#if 0
    if (Model* animatedCubeModel =
        ModelManager::GetInstance()->FindModel("AnimatedCube/AnimatedCube.gltf")) {
        const Animation& animation = animatedCubeModel->GetAnimation();

        if (animation.duration > 0.0f &&
            animatedCubeModel->HasAnimation()) {
            animationTime_ += deltaTime * animation.ticksPerSecond;

            while (animationTime_ > animation.duration) {
                animationTime_ -= animation.duration;
            }

            const std::string& rootNodeName =
                animatedCubeModel->GetRootNode().name;
            auto it = animation.nodeAnimations.find(rootNodeName);

            if (it != animation.nodeAnimations.end()) {
                const QuaternionTransform transform =
                    Model::CalculateValue(it->second, animationTime_);

                animatedCubeObject_->SetScale(transform.scale);
                animatedCubeObject_->SetQuaternionRotate(transform.rotate);
                animatedCubeObject_->SetTranslate({
                    transform.translate.x,
                    transform.translate.y + 1.5f,
                    transform.translate.z - 3.5f
                    });
            }
        }
    }

    ApplyLightingToObject(animatedCubeObject_.get());

    if (simpleSkinObject_) {
        simpleSkinObject_->UpdateAnimation(deltaTime);
        ApplyLightingToObject(simpleSkinObject_.get());
    }

    if (humanSneakObject_) {
        humanSneakObject_->UpdateAnimation(deltaTime);
        ApplyLightingToObject(humanSneakObject_.get());
    }

    if (humanWalkObject_) {
        humanWalkObject_->UpdateAnimation(deltaTime);
        ApplyLightingToObject(humanWalkObject_.get());
    }
#endif
}

// 編集中のシーンを保存してから試遊を始め、編集用の自由カメラ操作を切る
void EditorScene::EnterPlayMode()
{
    isDebugCameraEnabled_ = false;
    SaveSceneToFile(editorManager_.GetSceneFilePath());
    playController_.Start();
}

// 試遊を終了し、編集用カメラの位置・回転・画角を初期状態へ戻す
void EditorScene::ExitPlayMode()
{
    playController_.Stop();
    editorManager_.SetPlayMode(false);
    wasPlayMode_ = false;
    camera_->SetRotate(kGameCameraRotate);
    camera_->SetTranslate(kGameCameraTranslate);
    camera_->SetFovY(kGameCameraFovY);
}

// エディタの表示やカメラ操作などのショートカットを処理する
void EditorScene::HandleEditorShortcuts()
{
#ifdef ENABLE_DEBUG_GUI
    if (input_ && input_->TriggerKey(DIK_F1)) {
        editorManager_.ToggleEditorGuiVisible();
    }

    if (!editorManager_.IsPlayMode() && input_ && input_->TriggerKey(DIK_F3)) {
        isDebugCameraEnabled_ = !isDebugCameraEnabled_;
    }
#endif

    if (!editorManager_.IsPlayMode() && input_) {
        if (input_->TriggerKey(DIK_1)) {
            editorManager_.SetGizmoMode(0);
        }
        if (input_->TriggerKey(DIK_2)) {
            editorManager_.SetGizmoMode(1);
        }
        if (input_->TriggerKey(DIK_3)) {
            editorManager_.SetGizmoMode(2);
        }
    }
}

// 再生モードが切り替わったフレームを検出し、試遊の開始または終了を処理する
void EditorScene::ApplyPlayModeRequests(bool& startedPlayModeThisFrame)
{
    if (editorManager_.ConsumeStartPlayModeRequest()) {
        editorManager_.SetMode(EditorManager::EditorMode::Play);
    }
    editorManager_.ConsumeStopPlayModeRequest();

    const bool isPlayMode = editorManager_.IsPlayMode();
    if (isPlayMode && !wasPlayMode_) {
        EnterPlayMode();
        startedPlayModeThisFrame = true;
    } else if (!isPlayMode && wasPlayMode_) {
        ExitPlayMode();
    }
    wasPlayMode_ = editorManager_.IsPlayMode();
}

// 入力と編集UIを処理した後、そのフレームで試遊中かどうかに応じて更新先を切り替える
void EditorScene::Update() {
    const float deltaTime = dxCommon_->GetDeltaTime();

    HandleEditorShortcuts();

    if (!editorManager_.IsPlayMode() && isDebugCameraEnabled_) {
        UpdateDebugCamera(deltaTime);
    }

    std::vector<ImGuiManager::InspectableObject> inspectObjects;
    BuildInspectableObjects(inspectObjects);

    const int inspectObjectCount = static_cast<int>(inspectObjects.size());
    editorManager_.ValidateSelectedObjectIndex(inspectObjectCount);

#ifdef ENABLE_DEBUG_GUI
    ShowEditorGui(inspectObjects);
#endif

    Math::Vector2 hudViewportMin{};
    Math::Vector2 hudViewportSize{};
    const bool hasHudViewport =
        imguiManager_ &&
        imguiManager_->GetLastViewportImageRect(hudViewportMin, hudViewportSize);
    playController_.SetHudViewportRect(hasHudViewport, hudViewportMin, hudViewportSize);

    bool startedPlayModeThisFrame = false;
    ApplyPlayModeRequests(startedPlayModeThisFrame);

    if (editorManager_.IsPlayMode()) {
        if (playController_.IsRunning() && !startedPlayModeThisFrame) {
            playController_.Update(
                editorManager_.IsEditorGuiVisible(),
                showSkybox_,
                postEffectMode_);
            if (playController_.IsExitRequested()) {
                ExitPlayMode();
                if (sceneManager_) {
                    sceneManager_->SetNextScene(SceneType::Title);
                }
            }
        }
        return;
    }

    TrackTransformHistory(inspectObjects);
    ProcessEditorRequests();

    UpdateAnimations(deltaTime);

    for (auto& editorObject : editorObjects_) {
        ApplyLightingToObject(editorObject.object.get());
    }

    object3dCommon_->SetBlendMode(
        static_cast<BlendMode>(blendModeIndex_));

    camera_->Update();
    skybox_->Update(camera_.get());
    for (auto& editorObject : editorObjects_) {
        editorObject.object->Update();
    }

}

// 前回保存した表示・照明・ファイル選択などを復元する　設定ファイルがなければ初期値で始める
void EditorScene::LoadEditorSettings()
{
    std::ifstream file(kEditorSettingsPath);
    if (!file) {
        return;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();

    int sceneFileIndex = 0;
    int prefabFileIndex = 0;
    int gizmoMode = 0;
    ExtractSettingsInt(json, "postEffectMode", postEffectMode_);
    ExtractSettingsBool(json, "showSkybox", showSkybox_);
    ExtractSettingsFloat(json, "environmentCoefficient", environmentCoefficient_);
    ExtractSettingsVector3(json, "lightDirection", lightDirection_);
    ExtractSettingsFloat(json, "lightIntensity", lightIntensity_);
    ExtractSettingsVector3(json, "pointLightPosition", pointLightPosition_);
    ExtractSettingsFloat(json, "pointLightIntensity", pointLightIntensity_);
    ExtractSettingsVector3(json, "spotLightPosition", spotLightPosition_);
    ExtractSettingsVector3(json, "spotLightDirection", spotLightDirection_);
    ExtractSettingsFloat(json, "spotLightIntensity", spotLightIntensity_);
    ExtractSettingsBool(json, "renderingPanelOpen", renderingPanelOpen_);
    ExtractSettingsBool(json, "objectsPanelOpen", objectsPanelOpen_);
    ExtractSettingsBool(json, "inspectorPanelOpen", inspectorPanelOpen_);
    ExtractSettingsBool(json, "materialPanelOpen", materialPanelOpen_);
    ExtractSettingsBool(json, "lightingPanelOpen", lightingPanelOpen_);

    if (ExtractSettingsInt(json, "sceneFileIndex", sceneFileIndex)) {
        editorManager_.SetSceneFileIndex(sceneFileIndex);
    }
    if (ExtractSettingsInt(json, "prefabFileIndex", prefabFileIndex)) {
        editorManager_.SetPrefabFileIndex(prefabFileIndex);
    }
    if (ExtractSettingsInt(json, "gizmoMode", gizmoMode)) {
        editorManager_.SetGizmoMode(gizmoMode);
    }
}

// 表示設定やファイルの選択状態を、次回エディタを開いたときに復元できるよう保存する
void EditorScene::SaveEditorSettings() const
{
    std::filesystem::path path(kEditorSettingsPath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(path);
    if (!file) {
        return;
    }

    file << "{\n";
    file << "  \"postEffectMode\": " << postEffectMode_ << ",\n";
    file << "  \"showSkybox\": " << (showSkybox_ ? "true" : "false") << ",\n";
    file << "  \"environmentCoefficient\": " << environmentCoefficient_ << ",\n";
    file << "  \"lightDirection\": [" << lightDirection_.x << ", "
        << lightDirection_.y << ", " << lightDirection_.z << "],\n";
    file << "  \"lightIntensity\": " << lightIntensity_ << ",\n";
    file << "  \"pointLightPosition\": [" << pointLightPosition_.x << ", "
        << pointLightPosition_.y << ", " << pointLightPosition_.z << "],\n";
    file << "  \"pointLightIntensity\": " << pointLightIntensity_ << ",\n";
    file << "  \"spotLightPosition\": [" << spotLightPosition_.x << ", "
        << spotLightPosition_.y << ", " << spotLightPosition_.z << "],\n";
    file << "  \"spotLightDirection\": [" << spotLightDirection_.x << ", "
        << spotLightDirection_.y << ", " << spotLightDirection_.z << "],\n";
    file << "  \"spotLightIntensity\": " << spotLightIntensity_ << ",\n";
    file << "  \"renderingPanelOpen\": "
        << (renderingPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"objectsPanelOpen\": "
        << (objectsPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"inspectorPanelOpen\": "
        << (inspectorPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"materialPanelOpen\": "
        << (materialPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"lightingPanelOpen\": "
        << (lightingPanelOpen_ ? "true" : "false") << ",\n";
    file << "  \"sceneFileIndex\": " << editorManager_.GetSceneFileIndex() << ",\n";
    file << "  \"prefabFileIndex\": " << editorManager_.GetPrefabFileIndex() << ",\n";
    file << "  \"gizmoMode\": " << editorManager_.GetGizmoMode() << "\n";
    file << "}\n";
}

// キー入力と経過時間から、編集カメラを回転・移動させる
void EditorScene::UpdateDebugCamera(float deltaTime)
{
    if (!input_ || !camera_) {
        return;
    }

    Math::Vector3 rotate = camera_->GetRotate();
    Math::Vector3 translate = camera_->GetTranslate();

    const float rotateStep = debugCameraRotateSpeed_ * deltaTime;
    if (input_->PushKey(DIK_LEFT)) {
        rotate.y -= rotateStep;
    }
    if (input_->PushKey(DIK_RIGHT)) {
        rotate.y += rotateStep;
    }
    if (input_->PushKey(DIK_UP)) {
        rotate.x -= rotateStep;
    }
    if (input_->PushKey(DIK_DOWN)) {
        rotate.x += rotateStep;
    }

    rotate.x = std::clamp(rotate.x, -1.4f, 1.4f);

    const Math::Vector3 forward = MakeForwardVector(rotate);
    const Math::Vector3 right = MakeRightVector(rotate);
    const Math::Vector3 up = { 0.0f, 1.0f, 0.0f };

    Math::Vector3 move = { 0.0f, 0.0f, 0.0f };
    if (input_->PushKey(DIK_W)) {
        move = Add(move, forward);
    }
    if (input_->PushKey(DIK_S)) {
        move = Subtract(move, forward);
    }
    if (input_->PushKey(DIK_D)) {
        move = Add(move, right);
    }
    if (input_->PushKey(DIK_A)) {
        move = Subtract(move, right);
    }
    if (input_->PushKey(DIK_SPACE)) {
        move = Add(move, up);
    }
    if (input_->PushKey(DIK_LSHIFT) || input_->PushKey(DIK_RSHIFT)) {
        move = Subtract(move, up);
    }

    if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f) {
        move = Math::Normalize(move) * (debugCameraMoveSpeed_ * deltaTime);
        translate = Add(translate, move);
    }

    camera_->SetRotate(rotate);
    camera_->SetTranslate(translate);
}

// 試遊中はビューポートに合わせてゲームを描き、編集時はエディタの配置物を描く
void EditorScene::Draw() {
    if (editorManager_.IsPlayMode() && playController_.IsRunning()) {
        Math::Vector2 hudViewportMin{};
        Math::Vector2 hudViewportSize{};
        const bool hasHudViewport =
            imguiManager_ &&
            imguiManager_->GetLastViewportImageRect(hudViewportMin, hudViewportSize);
        playController_.SetHudViewportRect(hasHudViewport, hudViewportMin, hudViewportSize);
        playController_.Draw();
        return;
    }

    if (showSkybox_) {
        skybox_->Draw();
    }

    object3dCommon_->CommonDrawSetting();
    for (auto& editorObject : editorObjects_) {
        if (editorObject.visible) {
            editorObject.object->Draw();
        }
    }

}

// 試遊を止めて設定を保存し、エディタの配置物や描画用リソースを破棄する
void EditorScene::Finalize() {
    ExitPlayMode();
    SaveEditorSettings();

    emitter_.reset();
    cylinderObject_.reset();
    ringObject_.reset();
    sphereObject_.reset();
    animatedCubeObject_.reset();
    simpleSkinObject_.reset();
    humanSneakObject_.reset();
    humanWalkObject_.reset();
    simpleSkinDebug_.joints.clear();
    simpleSkinDebug_.bones.clear();
    humanSneakDebug_.joints.clear();
    humanSneakDebug_.bones.clear();
    humanWalkDebug_.joints.clear();
    humanWalkDebug_.bones.clear();
    editorObjects_.clear();
    skybox_.reset();
    camera_.reset();
    object3dCommon_.reset();

}

// スケルトンの関節と親子を結ぶ骨を可視化するためのオブジェクトを用意する
void EditorScene::InitializeSkeletonDebugSet(
    SkeletonDebugSet& debugSet,
    Object3d* source,
    const Math::Vector4& color)
{
    debugSet.source = source;
    debugSet.color = color;
    debugSet.joints.clear();
    debugSet.bones.clear();

    if (!source || !source->HasSkeleton()) {
        return;
    }

    const Skeleton& skeleton = source->GetSkeleton();
    debugSet.joints.reserve(skeleton.joints.size());
    debugSet.bones.reserve(skeleton.joints.size());

    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        auto jointObject = std::make_unique<Object3d>();
        jointObject->Initialize(object3dCommon_.get());
        jointObject->SetModel("debug_joint_sphere");
        jointObject->SetScale({ 0.006f, 0.006f, 0.006f });
        jointObject->SetLightingMode(0);
        jointObject->SetEnvironmentCoefficient(0.0f);
        jointObject->SetColor(color);
        debugSet.joints.push_back(std::move(jointObject));

        if (skeleton.joints[jointIndex].parent.has_value()) {
            auto boneObject = std::make_unique<Object3d>();
            boneObject->Initialize(object3dCommon_.get());
            boneObject->SetModel("debug_bone_box");
            boneObject->SetLightingMode(0);
            boneObject->SetEnvironmentCoefficient(0.0f);
            boneObject->SetColor(color);
            debugSet.bones.push_back(std::move(boneObject));
        }
    }
}

// モデルの現在の関節座標をワールド座標に移し、表示用の点と骨の位置・向きを更新する
void EditorScene::UpdateSkeletonDebugSet(SkeletonDebugSet& debugSet)
{
    if (!debugSet.source || !debugSet.source->HasSkeleton()) {
        return;
    }

    const bool isSimpleSkin = debugSet.source == simpleSkinObject_.get();
    const Skeleton& skeleton = debugSet.source->GetSkeleton();
    const Matrix4x4& worldMatrix = debugSet.source->GetWorldMatrix();

    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        const Matrix4x4 jointWorldMatrix = Multiply(
            skeleton.joints[jointIndex].skeletonSpaceMatrix,
            worldMatrix
        );
        const Math::Vector3 jointPosition = ExtractTranslation(jointWorldMatrix);

        debugSet.joints[jointIndex]->SetTranslate(jointPosition);
        if (isSimpleSkin) {
            debugSet.joints[jointIndex]->SetScale({ 0.02f, 0.02f, 0.02f });
            if (jointIndex == 0) {
                debugSet.joints[jointIndex]->SetColor({ 1.0f, 0.55f, 0.75f, 1.0f });
            } else {
                debugSet.joints[jointIndex]->SetColor({ 0.55f, 0.9f, 1.0f, 1.0f });
            }
        } else {
            debugSet.joints[jointIndex]->SetScale({ 0.006f, 0.006f, 0.006f });
            debugSet.joints[jointIndex]->SetColor(debugSet.color);
        }
    }

    size_t boneIndex = 0;
    for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
        const auto& joint = skeleton.joints[jointIndex];
        if (!joint.parent.has_value()) {
            continue;
        }

        const Matrix4x4 jointWorldMatrix = Multiply(
            joint.skeletonSpaceMatrix,
            worldMatrix
        );
        const Matrix4x4 parentWorldMatrix = Multiply(
            skeleton.joints[*joint.parent].skeletonSpaceMatrix,
            worldMatrix
        );

        const Math::Vector3 jointPosition = ExtractTranslation(jointWorldMatrix);
        const Math::Vector3 parentPosition = ExtractTranslation(parentWorldMatrix);
        const Math::Vector3 diff = Subtract(jointPosition, parentPosition);
        const float length =
            std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        if (length <= 0.0001f) {
            if (isSimpleSkin) {
                debugSet.bones[boneIndex]->SetScale({ 0.0015f, 0.0015f, 0.0015f });
            } else {
                debugSet.bones[boneIndex]->SetScale({ 0.002f, 0.002f, 0.002f });
            }
            debugSet.bones[boneIndex]->SetTranslate(parentPosition);
            debugSet.bones[boneIndex]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            ++boneIndex;
            continue;
        }

        const Math::Vector3 direction = {
            diff.x / length,
            diff.y / length,
            diff.z / length
        };
        const Math::Vector3 center = {
            (jointPosition.x + parentPosition.x) * 0.5f,
            (jointPosition.y + parentPosition.y) * 0.5f,
            (jointPosition.z + parentPosition.z) * 0.5f
        };

        if (isSimpleSkin) {
            debugSet.bones[boneIndex]->SetScale({ 0.0025f, 0.0025f, length });
        } else {
            debugSet.bones[boneIndex]->SetScale({ 0.006f, 0.006f, length });
        }
        debugSet.bones[boneIndex]->SetRotate(MakeBoneRotate(direction));
        debugSet.bones[boneIndex]->SetTranslate(center);
        debugSet.bones[boneIndex]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        ++boneIndex;
    }
}
