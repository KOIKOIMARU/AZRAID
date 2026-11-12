// ===== RuntimeSceneObjects.cpp =====
// シーンに配置したオブジェクトを読み込み、更新・描画・保存する

#include "app/runtime/RuntimeSceneObjects.h"

#include <iterator>
#include <utility>

namespace {
constexpr const char* kRuntimeSceneModelItems[] = {
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
} // namespace

// 配置データから描画オブジェクトを作る　読み込みに成功してから現在の配置と入れ替える
bool RuntimeSceneObjects::Load(const char* path, Object3dCommon& common, SceneSerializer::SceneSettings& settings)
{
    std::vector<SceneSerializer::ObjectRecord> records;
    if (!path || !SceneSerializer::LoadScene(path, records, settings)) {
        return false;
    }

    std::vector<std::unique_ptr<Object3d>> nextObjects;
    std::vector<SceneSerializer::ObjectRecord> nextRecords;
    nextObjects.reserve(records.size());
    nextRecords.reserve(records.size());
    std::string nextPath(path);
    // モデル番号が有効なレコードだけを、描画物体と同じ順序で持つ
    const int modelItemCount = static_cast<int>(std::size(kRuntimeSceneModelItems));
    for (auto& record : records) {
        if (record.modelIndex < 0 || modelItemCount <= record.modelIndex) {
            continue;
        }

        auto sceneObject = std::make_unique<Object3d>();
        sceneObject->Initialize(&common);
        sceneObject->SetModel(kRuntimeSceneModelItems[record.modelIndex]);
        sceneObject->SetTranslate(record.translate);
        sceneObject->SetRotate(record.rotate);
        sceneObject->SetScale(record.scale);
        sceneObject->SetColor(record.color);
        sceneObject->SetAlphaReference(record.alphaReference);
        sceneObject->SetLightingMode(record.lightingMode);
        if (!record.textureFilePath.empty()) {
            sceneObject->SetTextureFilePath(record.textureFilePath);
        }
        sceneObject->SetEnvironmentCoefficient(0.0f);
        nextObjects.push_back(std::move(sceneObject));
        nextRecords.push_back(std::move(record));
    }

    // 読み込みが途中で失敗しても、旧配置と保存先の対応が崩れないよう一緒に置き換える
    objects_ = std::move(nextObjects);
    records_ = std::move(nextRecords);
    path_ = std::move(nextPath);
    return true;
}

// 現在の配置から保存用のレコードを作り、シーン設定と一緒に書き出す
bool RuntimeSceneObjects::Save(const char* path, const SceneSerializer::SceneSettings& settings)
{
    if (!path || path[0] == '\0') { return false; }
    std::string nextPath(path);
    auto records = BuildRecords();
    if (!SceneSerializer::SaveScene(nextPath.c_str(), records, settings)) { return false; }
    records_ = std::move(records);
    path_ = std::move(nextPath);
    return true;
}

// 描画オブジェクトのモデル名や現在の姿勢を保存用データへ写す
std::vector<SceneSerializer::ObjectRecord> RuntimeSceneObjects::BuildRecords() const
{
    std::vector<SceneSerializer::ObjectRecord> records = records_;

    for (size_t index = 0; index < objects_.size(); ++index) {
        const Object3d* object = objects_[index].get();
        if (!object) {
            continue;
        }

        SceneSerializer::ObjectRecord& record = records[index];
        if (record.name.empty()) {
            record.name = "Runtime Object " + std::to_string(index);
        }
        if (record.modelIndex < 0) {
            record.primitive = true;
            record.modelIndex = 0;
        }
        record.translate = object->GetTranslate();
        record.rotate = object->GetRotate();
        record.scale = object->GetScale();
        record.color = object->GetColor();
        record.alphaReference = object->GetAlphaReference();
        record.lightingMode = object->GetLightingMode();
        record.textureFilePath = object->GetTextureFilePath();
    }

    return records;
}

// 配置済みの各物体について、現在の姿勢を描画用の行列へ反映する
void RuntimeSceneObjects::Update()
{
    for (const auto& object : objects_) {
        object->Update();
    }
}

// 配置ファイルから用意した物体を描画する
void RuntimeSceneObjects::Draw() const
{
    for (const auto& object : objects_) {
        object->Draw();
    }
}

void RuntimeSceneObjects::Clear()
{
    objects_.clear();
    records_.clear();
}
