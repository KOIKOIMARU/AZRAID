// ===== MyGame.cpp =====
// 起動時の読み込みを進め、アプリ全体の更新・描画・終了を管理する

#include "app/runtime/MyGame.h"
#include "app/ui/MenuUi.h"
#include "engine/base/WinApp.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneFactory.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/base/ImGuiManager.h"
#include "engine/3d/ModelManager.h"
#include "engine/io/Input.h"
#include "engine/scene/EditorScene.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/SceneType.h"
#include "engine/scene/TitleScene.h"

#include <chrono>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <utility>
#include <cstdio>

namespace {

float ToMilliseconds(
    const std::chrono::steady_clock::time_point& begin,
    const std::chrono::steady_clock::time_point& end)
{
    return std::chrono::duration<float, std::milli>(end - begin).count();
}

}

MyGame::MyGame(GameLaunchOptions options)
    : smokeTest_(std::move(options.smokeTest))
{
#ifdef _DEBUG
    debugDirectStart_ = options.debugDirectStart && !smokeTest_.IsEnabled();
#endif
}
MyGame::~MyGame() = default;

// 共通機能を準備し、ゲームが使うシーンと起動時ロードを設定する
void MyGame::Initialize() {
    smokeTest_.Begin();
    Framework::Initialize();

    smokeTest_.WriteLog("SMOKE_TEST_ENGINE_INITIALIZED");

    // 自動検証の成績を、実際に遊んだユーザーの記録へ混ぜない
    StageRecords* records = debugDirectStart_ ? nullptr : &stageRecords_;
    if (records) {
        auto recordPath = StageRecords::DefaultPath();
        if (smokeTest_.IsEnabled()) {
            recordPath = smokeTest_.GetLogPath();
            recordPath += L".record";
        }
        if (!records->Load(recordPath)) { smokeTest_.WriteLog("STAGE_RECORD_LOAD_FAILED"); }
    }
    sceneFactory_ = std::make_unique<SceneFactory>(records);

    SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
    SceneManager::GetInstance()->SetSystems(
        dxCommon_.get(),
        srvManager_.get(),
        spriteCommon_.get(),
        imguiManager_.get(),
        input_.get()
    );

    startupLoadingStart_ = std::chrono::steady_clock::now();
    // 最初のフレームは読み込み表示だけを描画し、その後に重い準備を始める
    startupLoading_ = true;
    startupFramePresented_ = false;
    startupDisplayedProgress_ = 0.0f;
    startupStep_ = 0;
}

// 起動時のロードを1フレームずつ進め、完了した時点で最初のシーンへ切り替える
void MyGame::UpdateStartupLoading()
{
    if (!startupFramePresented_) { return; }
    auto* scenes = SceneManager::GetInstance();
    switch (startupStep_) {
    case 0:
        if (GameScene::PreloadResourcesStep(dxCommon_.get(), srvManager_.get())) {
            startupStep_ = debugDirectStart_ ? 3 : 1;
        }
        break;
    case 1:
        if (scenes->PrepareSceneStep(SceneType::Title)) { ++startupStep_; }
        break;
    case 2:
        if (scenes->PrepareSceneStep(SceneType::StageSelect)) { ++startupStep_; }
        break;
    case 3:
        if (scenes->PrepareSceneStep(SceneType::Game)) { ++startupStep_; }
        break;
    default: {
        std::ostringstream message;
        message << "STARTUP_LOADING_COMPLETE seconds=" << std::chrono::duration<double>(
            std::chrono::steady_clock::now() - startupLoadingStart_).count()
            << " title=" << scenes->IsScenePrepared(SceneType::Title)
            << " stage_select=" << scenes->IsScenePrepared(SceneType::StageSelect)
            << " game=" << scenes->IsScenePrepared(SceneType::Game);
        smokeTest_.WriteLog(message.str());
        startupLoading_ = false;
        scenes->SetNextScene(debugDirectStart_ ? SceneType::Game : SceneType::Title);
        scenes->Update();
        break;
    }
    }
}

// 現在のロード工程と進捗を起動画面へ描く
void MyGame::DrawStartupLoading()
{
    const auto* viewport = ImGui::GetMainViewport();
    auto* draw = ImGui::GetForegroundDrawList();
    const float scale = (std::max)(0.1f, (std::min)(viewport->Size.x / 1280.0f, viewport->Size.y / 720.0f));
    const ImVec2 origin{ viewport->Pos.x + (viewport->Size.x - 1280.0f * scale) * 0.5f,
        viewport->Pos.y + (viewport->Size.y - 720.0f * scale) * 0.5f };
    const auto p = [&](float x, float y) { return ImVec2{ origin.x + x * scale, origin.y + y * scale }; };
    const auto color = [](ImU32 srgb) { return CombatHud::SurfaceColor(srgb); };
    const auto paper = MenuUi::Paper;
    const auto quiet = IM_COL32(134, 151, 173, 255);
    auto* scenes = SceneManager::GetInstance();
    const int resourceStep = GameScene::GetResourcePreloadStep();
    const float resources = static_cast<float>(resourceStep) / static_cast<float>(GameScene::GetResourcePreloadStepCount());
    const float game = scenes->GetScenePreparationProgress(SceneType::Game);
    // 完了した読み込みジョブと準備段階を合成する　経過時間では進めない
    const float progress = debugDirectStart_ ? resources * 0.78f + game * 0.22f :
        resources * 0.72f + game * 0.20f +
        (scenes->IsScenePrepared(SceneType::Title) ? 0.06f : 0.0f) +
        (scenes->IsScenePrepared(SceneType::StageSelect) ? 0.02f : 0.0f);
    startupDisplayedProgress_ = (std::max)(startupDisplayedProgress_, std::clamp(progress, 0.0f, 1.0f));
    if (startupStep_ >= 4) { startupDisplayedProgress_ = 1.0f; }
    const char* status = "描画を準備中";
    if (!GameScene::AreResourcesPreloaded()) {
        if (resourceStep >= 58) { status = "ステージデータを読み込み中"; }
        else if (resourceStep >= 48) { status = "敵機データを読み込み中"; }
        else if (resourceStep >= 12) { status = "機体とエフェクトを読み込み中"; }
        else if (resourceStep >= 7) { status = "機体データを読み込み中"; }
    } else if (game < 14.0f / 16.0f) { status = "ステージを準備中"; }
    else if (game < 15.0f / 16.0f) { status = "音声を読み込み中"; }
    else { status = "出撃準備中"; }
    if (startupStep_ >= 4) { status = "準備完了"; }

    const ImVec2 end{ viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y };
    draw->PushClipRect(viewport->Pos, end, true);
    draw->AddRectFilled(viewport->Pos, end, color(IM_COL32(6, 7, 9, 255)));
    // タイトルと同じロゴだけを残し、読み込みの情報は右下にまとめる
    MenuUi::Brand(draw, p(520, 300), 48 * scale, paper);
    char percent[16]{};
    const int completed = startupStep_ >= 4 ? 100 : (std::min)(99, static_cast<int>(startupDisplayedProgress_ * 100.0f));
    std::snprintf(percent, sizeof(percent), "%d%%", completed);
    MenuUi::Text(draw, p(896, 616), 15 * scale, quiet, status);
    MenuUi::Text(draw, p(1184, 616), 15 * scale, paper, percent, true);
    draw->AddRectFilled(p(896, 646), p(1184, 648), color(IM_COL32(52, 55, 60, 255)));
    if (startupDisplayedProgress_ > 0.0f) {
        draw->AddRectFilled(p(896, 646), p(896 + 288 * startupDisplayedProgress_, 648), color(paper));
    }
    draw->PopClipRect();
}

// 起動時ロード、シーン更新、検証を現在の状態に合わせて進める
void MyGame::Update() {
    const auto updateBegin = std::chrono::steady_clock::now();
    GameRuntime::ResetInitializationTiming();
    dxCommon_->EditFrameTiming().bufferCreates = 0;
    Framework::Update();
    if (endRequst_) {
        smokeTest_.OnWindowClosed();
        if (dxCommon_) {
            dxCommon_->EditFrameTiming().updateMs =
                ToMilliseconds(updateBegin, std::chrono::steady_clock::now());
        }
        return;
    }

    imguiManager_->Begin();

    auto& scenes = *SceneManager::GetInstance();
    smokeTest_.UpdateProbe(scenes);
    if (startupLoading_) {
        UpdateStartupLoading();
        if (startupLoading_) { DrawStartupLoading(); }
    } else {
        scenes.Update();
    }

    smokeTest_.ObservePreparation(startupLoading_);
    imguiManager_->End();

    smokeTest_.Update(scenes);
    if (smokeTest_.ShouldExit()) {
        exitCode_ = smokeTest_.GetExitCode();
        endRequst_ = true;
    }

    dxCommon_->EditFrameTiming().updateMs =
        ToMilliseconds(updateBegin, std::chrono::steady_clock::now());
}

// ロード画面、通常シーン、描画結果の合成をフレームの状態に応じて切り替える
void MyGame::Draw() {
    auto& timing = dxCommon_->EditFrameTiming();
    const auto drawBegin = std::chrono::steady_clock::now();
    auto sectionBegin = drawBegin;

    dxCommon_->PreDraw();
    srvManager_->PreDraw();
    timing.preDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    sectionBegin = std::chrono::steady_clock::now();
    // タイトル・編集画面・再入場へ飛行中のブラー設定を持ち越さない
    dxCommon_->SetFlightBlur();
    SceneManager::GetInstance()->Draw();
    timing.sceneDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    int postEffectMode = 0;
    if (auto* editorScene =
            dynamic_cast<EditorScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        postEffectMode = editorScene->GetPostEffectMode();
        dxCommon_->SetPostEffectProjectionMatrix(editorScene->GetProjectionMatrix());
    } else if (auto* gameScene =
            dynamic_cast<GameScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        postEffectMode = gameScene->GetPostEffectMode();
        dxCommon_->SetPostEffectProjectionMatrix(gameScene->GetProjectionMatrix());
    } else if (auto* titleScene =
            dynamic_cast<TitleScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        if (auto* preview = titleScene->GetDeparturePreview()) {
            postEffectMode = preview->GetPostEffectMode();
            dxCommon_->SetPostEffectProjectionMatrix(preview->GetProjectionMatrix());
        } else if (auto* stage = titleScene->IsStageSelectionOpen() ? titleScene : titleScene->GetStageReveal()) {
            postEffectMode = 12;
            dxCommon_->SetPostEffectProjectionMatrix(stage->GetStageProjectionMatrix());
        }
    }

    sectionBegin = std::chrono::steady_clock::now();
    dxCommon_->DrawRenderTextureToSwapChain(postEffectMode);
    // 出撃先の景色を色補正してから、手前にあるタイトルの切断面を重ねる
    if (auto* titleScene = dynamic_cast<TitleScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        titleScene->DrawDepartureOverlay();
    }
    timing.postEffectMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    sectionBegin = std::chrono::steady_clock::now();
    if (auto* titleScene = dynamic_cast<TitleScene*>(SceneManager::GetInstance()->GetCurrentScene());
        titleScene && titleScene->IsStageSelectionOpen()) {
        titleScene->DrawFlightOverlay();
    }
    imguiManager_->Draw();
    if (auto* titleScene = dynamic_cast<TitleScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        if (!titleScene->IsStageSelectionOpen()) { titleScene->DrawFlightOverlay(); }
        titleScene->DrawTransitionFade();
    }
    timing.imguiDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    dxCommon_->PostDraw();
    startupFramePresented_ = true;
    winApp_->MarkFramePresented();
    timing.frameCpuMs =
        timing.updateMs +
        ToMilliseconds(drawBegin, std::chrono::steady_clock::now());
    smokeTest_.ObserveFrame(*dxCommon_);
}

// シーンとゲーム固有の処理を終えてから、共通の描画・ウィンドウ機能を終了する
void MyGame::Finalize() {
    smokeTest_.WritePerformanceSummary();
    SceneManager::GetInstance()->FinalizeAllScenes();
    ModelManager::GetInstance()->Finalize();
    sceneFactory_.reset();
    Framework::Finalize();

    smokeTest_.End();
    if (smokeTest_.IsEnabled()) { exitCode_ = smokeTest_.GetExitCode(); }
}
