// ===== GameScene.cpp =====
// 本編シーンの準備と更新・描画をGameRuntimeにつなぎ、画面の切り替えを受け渡す。

#include "engine/scene/GameScene.h"

#include "app/runtime/GameResources.h"

#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"

namespace {
bool gRetryEntryRequested = false;
}

// 通常プレイの再挑戦要求を初期化へ引き継ぐ。要求はこのシーンで消費する。
GameScene::GameScene(GameRuntime::PlayMode mode, StageRecords* records) : mode_(mode)
{
    runtime_.SetStageRecords(records);
    shortEntry_ = mode == GameRuntime::PlayMode::Game && gRetryEntryRequested;
    gRetryEntryRequested = false;
}
GameScene::~GameScene() = default;

bool GameScene::PreloadResourcesStep(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    return GameResources::PreloadStep(dxCommon, srvManager);
}

bool GameScene::AreResourcesPreloaded()
{
    return GameResources::IsPreloaded();
}

int GameScene::GetResourcePreloadStep()
{
    return GameResources::GetStep();
}

int GameScene::GetResourcePreloadStepCount()
{
    return GameResources::GetStepCount();
}

const char* GameScene::GetResourcePreloadLabel()
{
    return GameResources::GetLabel();
}

float GameScene::GetResourcePreloadLastStepMs()
{
    return GameResources::GetLastStepMs();
}

float GameScene::GetResourcePreloadTotalMs()
{
    return GameResources::GetTotalMs();
}

// ゲーム本体に共通システムを渡して開始する。再挑戦の場合は短い出撃演出を使う。
void GameScene::Initialize()
{
    runtime_.SetSystems(
        dxCommon_,
        srvManager_,
        spriteCommon_,
        imguiManager_,
        input_);
    runtime_.Initialize(mode_);
    if (shortEntry_) { runtime_.BeginRetryEntry(); shortEntry_ = false; }
}

void GameScene::Finalize()
{
    runtime_.Finalize();
}

// 現在のモードでGameRuntimeの初期化を始める。続きはフレームごとの初期化へ渡す。
void GameScene::BeginInitialize()
{
    runtime_.SetSystems(dxCommon_, srvManager_, spriteCommon_, imguiManager_, input_);
    runtime_.BeginInitialize(mode_);
}

// 素材とゲームの初期化を段階的に進め、プレイ可能になったら完了を返す。
bool GameScene::InitializeStep()
{
    if (!runtime_.InitializeStep()) { return false; }
    if (shortEntry_) { runtime_.BeginRetryEntry(); shortEntry_ = false; }
    return true;
}

// 本編を1フレーム進め、リザルトやメニューから届いた再挑戦・シーン切り替えを処理する。
void GameScene::Update()
{
    runtime_.Update();
    if (runtime_.IsMainGameRequested() && sceneManager_) {
        sceneManager_->SetNextScene(SceneType::Departure, runtime_.HasMenuSceneTransition());
    } else if (runtime_.IsRetryRequested() && sceneManager_) {
        gRetryEntryRequested = !IsTutorial();
        sceneManager_->SetNextScene(IsTutorial() ? SceneType::Tutorial : SceneType::Game, runtime_.HasMenuSceneTransition());
    } else if (runtime_.IsExitRequested() && sceneManager_) {
        sceneManager_->SetNextScene(SceneType::Title, runtime_.HasMenuSceneTransition());
    }
}

void GameScene::Draw()
{
    runtime_.Draw();
}

int GameScene::GetPostEffectMode() const
{
    return runtime_.GetPostEffectMode();
}

const Math::Matrix4x4& GameScene::GetProjectionMatrix() const
{
    return runtime_.GetProjectionMatrix();
}
