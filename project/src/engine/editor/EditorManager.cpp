// ===== EditorManager.cpp =====
// エディタで選んだシーンやオブジェクト、インスペクターの設定を管理する

#include "engine/editor/EditorManager.h"

bool EditorManager::playOnNextEditorOpen_ = false;

void EditorManager::SetSceneFilePath(const char* path)
{
    sceneFilePaths_[0] = path;
}

const char* EditorManager::GetSceneFilePath() const
{
    return sceneFilePaths_[sceneFileIndex_];
}

const char* EditorManager::GetPrefabFilePath() const
{
    return prefabFilePaths_[prefabFileIndex_];
}

int EditorManager::GetSceneFileIndex() const
{
    return sceneFileIndex_;
}

int EditorManager::GetPrefabFileIndex() const
{
    return prefabFileIndex_;
}

// 一覧の範囲内にある番号だけをシーンの選択に反映する
void EditorManager::SetSceneFileIndex(int index)
{
    if (0 <= index && index < static_cast<int>(sceneFilePaths_.size())) {
        sceneFileIndex_ = index;
    }
}

// 一覧の範囲内にある番号だけをプレハブの選択に反映する
void EditorManager::SetPrefabFileIndex(int index)
{
    if (0 <= index && index < static_cast<int>(prefabFilePaths_.size())) {
        prefabFileIndex_ = index;
    }
}

// 選択中のファイル・モデル・編集モードを、インスペクタが使う設定にまとめる
ImGuiManager::ObjectInspectorSettings EditorManager::CreateInspectorSettings(
    ImGuiManager::InspectableObject* objects,
    int objectCount)
{
    return {
        objects,
        objectCount,
        selectedObjectIndex_,
        addModelIndex_,
        gizmoMode_,
        mode_ == EditorMode::Play,
        requestStartPlayMode_,
        requestStopPlayMode_,
        requestAddObject_,
        requestRemoveObject_,
        requestDuplicateObject_,
        requestSavePrefab_,
        requestInstantiatePrefab_,
        requestSaveObjects_,
        requestLoadObjects_,
        requestUndo_,
        requestRedo_,
        GetSceneFilePath(),
        sceneFilePaths_.data(),
        static_cast<int>(sceneFilePaths_.size()),
        sceneFileIndex_,
        prefabFilePaths_.data(),
        static_cast<int>(prefabFilePaths_.size()),
        prefabFileIndex_
    };
}

// 削除や読込で物体の数が変わった後に、範囲外を指していた選択を解除する
void EditorManager::ValidateSelectedObjectIndex(int objectCount)
{
    if (selectedObjectIndex_ < 0 || selectedObjectIndex_ >= objectCount) {
        selectedObjectIndex_ = 0;
    }
}

int EditorManager::GetSelectedObjectIndex() const
{
    return selectedObjectIndex_;
}

void EditorManager::ResetSelectedObjectIndex()
{
    selectedObjectIndex_ = 0;
}

bool EditorManager::IsPlayMode() const
{
    return mode_ == EditorMode::Play;
}

void EditorManager::SetPlayMode(bool isPlayMode)
{
    mode_ = isPlayMode ? EditorMode::Play : EditorMode::Edit;
}

EditorManager::EditorMode EditorManager::GetMode() const
{
    return mode_;
}

void EditorManager::SetMode(EditorMode mode)
{
    mode_ = mode;
}

bool EditorManager::IsEditorGuiVisible() const
{
    return isEditorGuiVisible_;
}

void EditorManager::SetEditorGuiVisible(bool isVisible)
{
    isEditorGuiVisible_ = isVisible;
}

void EditorManager::ToggleEditorGuiVisible()
{
    isEditorGuiVisible_ = !isEditorGuiVisible_;
}

void EditorManager::RequestPlayOnNextEditorOpen()
{
    playOnNextEditorOpen_ = true;
}

// エディタを開き直したときの自動再生要求を取り出し、同じ要求を繰り返さないように解除する
bool EditorManager::ConsumePlayOnNextEditorOpenRequest()
{
    const bool requested = playOnNextEditorOpen_;
    playOnNextEditorOpen_ = false;
    return requested;
}

// 編集画面からのプレイ開始要求を一度だけ取り出す　次のフレームでは消費済みになる
bool EditorManager::ConsumeStartPlayModeRequest()
{
    const bool requested = requestStartPlayMode_;
    requestStartPlayMode_ = false;
    return requested;
}

// 再生停止の要求を一度だけ取り出す
bool EditorManager::ConsumeStopPlayModeRequest()
{
    const bool requested = requestStopPlayMode_;
    requestStopPlayMode_ = false;
    return requested;
}

int EditorManager::GetAddModelIndex() const
{
    return addModelIndex_;
}

int EditorManager::GetGizmoMode() const
{
    return gizmoMode_;
}

// 移動・回転・拡縮に対応する番号だけをギズモに設定する
void EditorManager::SetGizmoMode(int mode)
{
    constexpr int kGizmoModeCount = 3;
    if (0 <= mode && mode < kGizmoModeCount) {
        gizmoMode_ = mode;
    }
}

// 追加ボタンから届いた要求を取り出して解除する
bool EditorManager::ConsumeAddObjectRequest()
{
    const bool requested = requestAddObject_;
    requestAddObject_ = false;
    return requested;
}

// 削除ボタンから届いた要求を取り出して解除する
bool EditorManager::ConsumeRemoveObjectRequest()
{
    const bool requested = requestRemoveObject_;
    requestRemoveObject_ = false;
    return requested;
}

// 複製ボタンから届いた要求を取り出して解除する
bool EditorManager::ConsumeDuplicateObjectRequest()
{
    const bool requested = requestDuplicateObject_;
    requestDuplicateObject_ = false;
    return requested;
}

// 選択物をプレハブとして保存する要求を取り出して解除する
bool EditorManager::ConsumeSavePrefabRequest()
{
    const bool requested = requestSavePrefab_;
    requestSavePrefab_ = false;
    return requested;
}

// プレハブをシーンに配置する要求を取り出して解除する
bool EditorManager::ConsumeInstantiatePrefabRequest()
{
    const bool requested = requestInstantiatePrefab_;
    requestInstantiatePrefab_ = false;
    return requested;
}

// 現在のシーンを保存する要求を取り出して解除する
bool EditorManager::ConsumeSaveObjectsRequest()
{
    const bool requested = requestSaveObjects_;
    requestSaveObjects_ = false;
    return requested;
}

// 選択中のシーンを読み直す要求を取り出して解除する
bool EditorManager::ConsumeLoadObjectsRequest()
{
    const bool requested = requestLoadObjects_;
    requestLoadObjects_ = false;
    return requested;
}

// 元に戻す操作を一度だけ取り出す
bool EditorManager::ConsumeUndoRequest()
{
    const bool requested = requestUndo_;
    requestUndo_ = false;
    return requested;
}

// やり直す操作を一度だけ取り出す
bool EditorManager::ConsumeRedoRequest()
{
    const bool requested = requestRedo_;
    requestRedo_ = false;
    return requested;
}
