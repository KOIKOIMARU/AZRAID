// ===== EditorPlayController.cpp =====
// エディタからの試遊を開始・終了し、エディタ内でゲームの更新と描画を行う

#include "engine/editor/EditorPlayController.h"

#include "app/runtime/GameRuntime.h"

EditorPlayController::EditorPlayController() = default;
EditorPlayController::~EditorPlayController() = default;

// 試遊用のゲームが初期化時に使う描画・入力システムへの参照を受け取る
void EditorPlayController::SetSystems(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    SpriteCommon* spriteCommon,
    ImGuiManager* imguiManager,
    Input* input)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteCommon_ = spriteCommon;
    imguiManager_ = imguiManager;
    input_ = input;
}

// 編集画面内でのプレイ確認に使うGameRuntimeを作り、共通の描画・入力機能を接続する
bool EditorPlayController::Start()
{
    if (runtime_) {
        return false;
    }

    runtime_ = std::make_unique<GameRuntime>();
    runtime_->SetSystems(
        dxCommon_,
        srvManager_,
        spriteCommon_,
        imguiManager_,
        input_);
    runtime_->Initialize();
    return true;
}

// 試遊中のゲームを終了処理してから、保持していた実行状態を破棄する
void EditorPlayController::Stop()
{
    if (!runtime_) {
        return;
    }

    runtime_->Finalize();
    runtime_.reset();
}

// 試遊中だけ、エディタの空や画面効果の設定を渡してゲームを1フレーム進める
void EditorPlayController::Update(
    bool isEditorGuiVisible,
    bool showSkybox,
    int postEffectMode)
{
    if (!runtime_) {
        return;
    }

    (void)isEditorGuiVisible;
    runtime_->SetRenderingOptions(showSkybox, postEffectMode);
    runtime_->Update();
}

void EditorPlayController::SetHudViewportRect(
    bool isEnabled,
    const Math::Vector2& min,
    const Math::Vector2& size)
{
    if (runtime_) {
        runtime_->SetHudViewportRect(isEnabled, min, size);
    }
}

void EditorPlayController::Draw()
{
    if (runtime_) {
        runtime_->Draw();
    }
}

int EditorPlayController::GetPostEffectMode() const
{
    return runtime_ ? runtime_->GetPostEffectMode() : 0;
}

const Math::Matrix4x4* EditorPlayController::GetProjectionMatrix() const
{
    return runtime_ ? &runtime_->GetProjectionMatrix() : nullptr;
}

bool EditorPlayController::IsRunning() const
{
    return runtime_ != nullptr;
}

bool EditorPlayController::IsExitRequested() const
{
    return runtime_ && runtime_->IsExitRequested();
}
