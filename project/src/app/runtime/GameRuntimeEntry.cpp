// ===== GameRuntimeEntry.cpp =====
// ステージ選択中のプレビューと出撃・再挑戦の演出を進め、プレイ開始につなぐ

#include "app/runtime/GameRuntime.h"

#include "app/ui/MenuUi.h"
#include "engine/3d/Skybox.h"
#include "engine/base/DirectXCommon.h"

#include <imgui.h>
#include <algorithm>

// ステージ選択の待機時間から、開口部を周回する自機の位置を求める
Math::Vector3 GameRuntime::GetStagePatrolPosition(float elapsed, Math::Vector2 seam)
{
    return StageEntryPresentation::GetStagePatrolPosition(elapsed, seam);
}

// 出撃計算へ渡す現在のレール距離、速度、モードをまとめる
StageEntryPresentation::FlightInput GameRuntime::GetEntryFlightInput() const
{
    return { stageFlight_.Distance(), stageFlight_.Speed(), stageFlight_.SpeedRate(), IsTutorial() };
}

// 演出中の機体とカメラの姿勢に合わせて、噴射・空・背景・照準表示を更新する
void GameRuntime::UpdateEntryWorld(bool updateGameplayOverlays)
{
    UpdatePlayerFlightEffects();
    if (skybox_) { skybox_->Update(camera_.get()); }
    railScenery_.Update(stageFlight_.Distance());
    if (updateGameplayOverlays) { UpdateDepthCueEffects(); }
    sceneObjects_.Update();
    if (updateGameplayOverlays) { UpdatePlayerAim(); }
}

// ステージ選択のカメラを合わせ、待機中の自機の飛行と噴射を進める
void GameRuntime::UpdateStagePreview(float elapsed)
{
    if (IsTutorial() || !camera_ || !player_ || !dxCommon_) { return; }

    gameplayCamera_.SuppressBlur();
    camera_->SetAspectRatio(dxCommon_->GetPresentationAspectRatio());
    gameplayCamera_.AdoptPose(stageEntryPresentation_.GetPreviewCamera(stageFlight_.Distance()));
    ApplyGameplayCameraPose();
    const auto ship = stageEntryPresentation_.UpdatePreview(elapsed, camera_->GetWorldMatrix());
    player_->SetCinematicPose(ship.position, ship.rotation);
    playerFlightEffects_.SetCinematicThrust(ship.thrust);
    gameplayCamera_.SynchronizePlayer(ship.position, elapsed * 60.0f);
    UpdateEntryWorld(false);
}

// 出撃に入る前の機体の戻りを計算し、その姿勢と速度演出を画面へ反映する
void GameRuntime::UpdateStageReturn(float elapsed)
{
    if (IsTutorial() || !camera_ || !player_ || !dxCommon_) { return; }

    const auto ship = stageEntryPresentation_.UpdateReturn(elapsed, player_->GetTranslate());
    player_->SetCinematicPose(ship.position, ship.rotation);
    playerFlightEffects_.SetCinematicThrust(ship.thrust);
    const float delta = std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 0.10f);
    gameplayCamera_.SynchronizePlayer(ship.position, gameplayCamera_.Clock() + delta * 60.0f);
    UpdateEntryWorld(false);
}

// ステージを見せるカメラと飛行のフレームを取得し、自機・背景・レール位置へ反映する
void GameRuntime::UpdateStageOverview(float elapsed)
{
    if (IsTutorial() || !camera_ || !player_ || !dxCommon_) { return; }
    if (!stageEntryPresentation_.IsTitleActive()) { flightMotionBlur_.ResetHistory(); }

    const auto frame = stageEntryPresentation_.UpdateOverview(elapsed, GetEntryFlightInput());
    stageFlight_.SetEntryDistance(frame.railDistance);
    gameplayCamera_.SetCinematicMotion(stageFlight_.Speed(), frame.blurStrength, true);
    playerFlightEffects_.SetCinematicThrust(frame.ship.thrust);
    camera_->SetAspectRatio(dxCommon_->GetPresentationAspectRatio());
    gameplayCamera_.AdoptPose(frame.camera);
    ApplyGameplayCameraPose();
    player_->SetCinematicPose(frame.ship.position, frame.ship.rotation);
    gameplayCamera_.SynchronizePlayer(frame.ship.position, frame.clockFrames);
    UpdateEntryWorld(true);
    if (elapsed >= kStageStartFlightEnd - 0.65f) { UpdateMusic(); }
}

// 指定した経過時間で、出撃時の目標表示を現在の画面領域へ描く
void GameRuntime::DrawStageBriefing(float elapsed)
{
    if (IsTutorial() || elapsed < 0.0f) { return; }

    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    stageEntryPresentation_.DrawBriefing(elapsed, min, size);
}

// 再挑戦用の短い出撃演出を準備し、その後に通常操作へ戻す
void GameRuntime::BeginRetryEntry()
{
    if (IsTutorial() || !player_ || !camera_) { return; }

    stageEntryPresentation_.BeginRetry(player_->GetTranslate(), stageFlight_.Distance());
    flightMotionBlur_.ResetHistory();
}

// 再挑戦時の機体とカメラを進める　trueの間は通常戦闘の更新を待つ
bool GameRuntime::UpdateRetryEntry()
{
    if (!stageEntryPresentation_.IsRetryActive() || !player_ || !camera_ || !dxCommon_) { return false; }

    const auto frame = stageEntryPresentation_.UpdateRetry(dxCommon_->GetDeltaTime(), GetEntryFlightInput());
    stageFlight_.SetEntryDistance(frame.railDistance);
    player_->SetCinematicPose(frame.ship.position, frame.ship.rotation);
    gameplayCamera_.AdoptPose(frame.camera);
    ApplyGameplayCameraPose();
    gameplayCamera_.SetCinematicMotion(stageFlight_.Speed(), frame.blurStrength);
    playerFlightEffects_.SetCinematicThrust(frame.ship.thrust);
    gameplayCamera_.SynchronizePlayer(frame.ship.position, frame.clockFrames);
    UpdateEntryWorld(true);

    auto* draw = ImGui::GetForegroundDrawList();
    const int first = draw->VtxBuffer.Size;
    DrawHud();
    MenuUi::Opacity(draw, first, stageEntryPresentation_.RetryHudOpacity());
    DrawStageBriefing(stageEntryPresentation_.BriefingTime());
    stageEntryPresentation_.FinishRetryFrame();
    return true;
}

// 画面上の機体の位置・向き・大きさを受け取り、出撃演出の初期値にする
void GameRuntime::BeginTitleEntry(const Math::Vector3& viewPosition, const Math::Vector3& rotation,
    const Math::Vector3& cameraRotation, float titleScale)
{
    if (!player_) { return; }

    stageEntryPresentation_.BeginTitle(viewPosition, rotation, cameraRotation,
        titleScale, player_->GetVisualScale(), GetEntryFlightInput());
}

// 出撃の経過時間で飛行を進め、機体とカメラの姿勢をゲームシーンへ反映する
void GameRuntime::UpdateTitleEntry(float time)
{
    if (!camera_ || !player_ || !dxCommon_) { return; }

    const auto frame = stageEntryPresentation_.UpdateTitle(time, GetEntryFlightInput());
    stageFlight_.SetEntryDistance(frame.railDistance);
    player_->SetCinematicPose(frame.ship.position, frame.ship.rotation);
    camera_->SetAspectRatio(dxCommon_->GetPresentationAspectRatio());
    gameplayCamera_.AdoptPose(frame.camera);
    ApplyGameplayCameraPose();
    gameplayCamera_.SetCinematicMotion(stageFlight_.Speed(), frame.blurStrength);
    playerFlightEffects_.SetCinematicThrust(frame.ship.thrust);
    gameplayCamera_.SynchronizePlayer(frame.ship.position, frame.clockFrames);
    UpdateEntryWorld(true);
}

bool GameRuntime::IsTitleEntryForeground() const
{
    return stageEntryPresentation_.IsForeground();
}

// 出撃中、前景に飛び出す機体と噴射を別の描画段階で表示する
void GameRuntime::DrawTitleEntryFlight()
{
    if (!IsTitleEntryForeground() || !player_) { return; }
    object3dCommon_->SetBlendMode(BlendMode::Normal);
    object3dCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
    object3dCommon_->CommonDrawSetting();
    player_->Draw();
    DrawPlayerFlightEffects();
}

// 出撃中のHUDを演出の時間に合わせて表示し、通常戦闘の画面につなぐ
void GameRuntime::DrawTitleEntryHud()
{
    const float opacity = stageEntryPresentation_.TitleHudOpacity();
    if (opacity <= 0.0f) { return; }
    ImDrawList* list = ImGui::GetForegroundDrawList();
    const int begin = list->VtxBuffer.Size;
    DrawHud();
    // 生の頂点色を使うHUDも一緒に立ち上げる　追加の画像・描画パスは不要
    for (int index = begin; index < list->VtxBuffer.Size; ++index) {
        auto& color = list->VtxBuffer[index].col;
        const auto alpha = static_cast<ImU32>(static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xffu) * opacity);
        color = (color & ~(0xffu << IM_COL32_A_SHIFT)) | (alpha << IM_COL32_A_SHIFT);
    }
}
