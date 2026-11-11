#include "engine/scene/SceneManager.h"
#include "engine/scene/BaseScene.h"
#include "engine/scene/AbstractSceneFactory.h"
#include "app/ui/MenuUi.h"
#include <cassert>

SceneManager* SceneManager::GetInstance() {
    static SceneManager instance;
    return &instance;
}

SceneManager::~SceneManager() {
    FinalizeAllScenes();
}

// 表示中と準備済みのシーンを終了し、遷移演出の状態もリセットする。
void SceneManager::FinalizeAllScenes() {
    FinalizeCurrentScene();
    for (size_t index = 0; index < preparedScenes_.size(); ++index) {
        DiscardPreparedScene(static_cast<SceneType>(index));
    }
    nextSceneMenuWipe_ = false;
    menuRevealStarted_ = -1.0;
}

void SceneManager::SetSceneFactory(AbstractSceneFactory* sceneFactory) {
    sceneFactory_ = sceneFactory;
}

// 新しいシーンを準備するときに渡す、描画や入力の共通システムを保持する。
void SceneManager::SetSystems(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    SpriteCommon* spriteCommon,
    ImGuiManager* imguiManager,
    Input* input
) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteCommon_ = spriteCommon;
    imguiManager_ = imguiManager;
    input_ = input;
}

// 次の画面と切り替え方法を予約する。入れ替えは次回のシーン更新で行う。
void SceneManager::SetNextScene(SceneType sceneType, bool menuWipe) {
    if (hasNextScene_ && nextSceneType_ == sceneType) { return; }
    nextSceneType_ = sceneType;
    hasNextScene_ = true;
    nextSceneMenuWipe_ = menuWipe;
}

// 分割初期化を完了まで繰り返し、呼び出した場で指定のシーンを使える状態にする。
void SceneManager::PrepareScene(SceneType sceneType) {
    while (!PrepareSceneStep(sceneType)) {}
}

// 次に使うシーンを作り、更新をまたいで初期化を進める。準備済みのシーンは切り替え時に引き継ぐ。
bool SceneManager::PrepareSceneStep(SceneType sceneType) {
    assert(static_cast<size_t>(sceneType) < preparedScenes_.size());
    auto& prepared = preparedScenes_[static_cast<size_t>(sceneType)];
    if (prepared.ready) {
        return true;
    }

    // 本編と練習は同名の粒子グループを使うため、二つを同時には準備しない。
    if (sceneType == SceneType::Game) { DiscardPreparedScene(SceneType::Tutorial); }
    if (sceneType == SceneType::Tutorial) { DiscardPreparedScene(SceneType::Game); }

    if (!prepared.scene) {
        assert(sceneFactory_ != nullptr);
        prepared.scene = sceneFactory_->CreateScene(sceneType);
        assert(prepared.scene != nullptr);
        prepared.scene->SetSceneManager(this);
        prepared.scene->SetSystems(
            dxCommon_,
            srvManager_,
            spriteCommon_,
            imguiManager_,
            input_);
        prepared.scene->BeginInitialize();
    }
    prepared.ready = prepared.scene->InitializeStep();
    return prepared.ready;
}

bool SceneManager::IsScenePrepared(SceneType sceneType) const {
    return preparedScenes_[static_cast<size_t>(sceneType)].ready;
}

float SceneManager::GetScenePreparationProgress(SceneType sceneType) const {
    const auto& prepared = preparedScenes_[static_cast<size_t>(sceneType)];
    return prepared.ready ? 1.0f : prepared.scene ? prepared.scene->GetInitializationProgress() : 0.0f;
}

BaseScene* SceneManager::GetPreparedScene(SceneType sceneType) const {
    return IsScenePrepared(sceneType) ? preparedScenes_[static_cast<size_t>(sceneType)].scene.get() : nullptr;
}

// 先に用意していたシーンが不要になったら、リソースを終了処理で片づけてから手放す。
void SceneManager::DiscardPreparedScene(SceneType sceneType) {
    auto& prepared = preparedScenes_[static_cast<size_t>(sceneType)];
    if (prepared.scene) { prepared.scene->Finalize(); prepared.scene.reset(); }
    prepared.ready = false;
}

// 現在のシーンに終了処理を行い、所有権と切り替え予約を解除する。
void SceneManager::FinalizeCurrentScene()
{
    if (scene_) {
        scene_->Finalize();
        scene_.reset();
    }
    hasNextScene_ = false;
}

// 切り替えの予約があれば現在と次のシーンを入れ替える。画面を開く演出中は到着用の更新を使う。
void SceneManager::Update() {
    const auto wipe = [](float coverage) {
        const auto* viewport = ImGui::GetMainViewport();
        MenuUi::Wipe(viewport->Pos, viewport->Size, coverage);
    };
    if (hasNextScene_) {
        const bool menuWipe = nextSceneMenuWipe_;
        if (menuWipe && scene_) {
            // 完全に覆った次フレームで旧ステージを終了する。新旧の同名パーティクル群を重ねない。
            DiscardPreparedScene(SceneType::Game);
            DiscardPreparedScene(SceneType::Tutorial);
            scene_->Finalize();
            scene_.reset();
        }
        if (menuWipe && !PrepareSceneStep(nextSceneType_)) {
            wipe(1.0f);
            return;
        }
        FinalizeCurrentScene();

        auto& prepared = preparedScenes_[static_cast<size_t>(nextSceneType_)];
        if (prepared.ready) {
            scene_ = std::move(prepared.scene);
            prepared.ready = false;
        } else {
            assert(sceneFactory_ != nullptr);

            scene_ = sceneFactory_->CreateScene(nextSceneType_);
            assert(scene_ != nullptr);

            scene_->SetSceneManager(this);
            scene_->SetSystems(
                dxCommon_,
                srvManager_,
                spriteCommon_,
                imguiManager_,
                input_
            );
            scene_->Initialize();
        }

        hasNextScene_ = false;
        nextSceneMenuWipe_ = false;
        menuRevealStarted_ = menuWipe ? ImGui::GetTime() : -1.0;
    }

    if (scene_) {
        if (menuRevealStarted_ >= 0.0) {
            const float t = std::clamp(static_cast<float>(ImGui::GetTime() - menuRevealStarted_) / 0.18f, 0.0f, 1.0f);
            {
                MenuUi::InputBlock block;
                scene_->UpdateMenuArrival();
            }
            wipe(1.0f - t * t * (3.0f - 2.0f * t));
            if (t == 1.0f) { menuRevealStarted_ = -1.0; }
            return;
        }
        scene_->Update();
    }
}

// 画面切り替えで描画を止める区間を除き、現在のシーンに描画を渡す。
void SceneManager::Draw() {
    if (scene_ && !(hasNextScene_ && nextSceneMenuWipe_)) {
        scene_->Draw();
    }
}
