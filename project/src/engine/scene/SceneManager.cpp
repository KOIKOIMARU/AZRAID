#include "engine/scene/SceneManager.h"
#include "engine/scene/BaseScene.h"
#include "engine/scene/AbstractSceneFactory.h"
#include <cassert>

SceneManager* SceneManager::GetInstance() {
    static SceneManager instance;
    return &instance;
}

SceneManager::~SceneManager() {
    FinalizeCurrentScene();
    if (preparedScene_) {
        preparedScene_->Finalize();
        preparedScene_.reset();
    }
}

void SceneManager::SetSceneFactory(AbstractSceneFactory* sceneFactory) {
    sceneFactory_ = sceneFactory;
}

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

void SceneManager::SetNextScene(SceneType sceneType) {
    nextSceneType_ = sceneType;
    hasNextScene_ = true;
}

void SceneManager::PrepareScene(SceneType sceneType) {
    if (hasPreparedScene_ && preparedSceneType_ == sceneType) {
        return;
    }

    if (preparedScene_) {
        preparedScene_->Finalize();
        preparedScene_.reset();
    }

    assert(sceneFactory_ != nullptr);
    preparedScene_ = sceneFactory_->CreateScene(sceneType);
    assert(preparedScene_ != nullptr);
    preparedScene_->SetSceneManager(this);
    preparedScene_->SetSystems(
        dxCommon_,
        srvManager_,
        spriteCommon_,
        imguiManager_,
        input_);
    preparedScene_->Initialize();
    preparedSceneType_ = sceneType;
    hasPreparedScene_ = true;
}

bool SceneManager::IsScenePrepared(SceneType sceneType) const {
    return hasPreparedScene_ && preparedSceneType_ == sceneType;
}

BaseScene* SceneManager::GetPreparedScene(SceneType sceneType) const {
    return IsScenePrepared(sceneType) ? preparedScene_.get() : nullptr;
}

void SceneManager::FinalizeCurrentScene()
{
    if (scene_) {
        scene_->Finalize();
        scene_.reset();
    }
    hasNextScene_ = false;
}

void SceneManager::Update() {
    if (hasNextScene_) {
        FinalizeCurrentScene();

        if (hasPreparedScene_ && preparedSceneType_ == nextSceneType_) {
            scene_ = std::move(preparedScene_);
            hasPreparedScene_ = false;
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
    }

    if (scene_) {
        scene_->Update();
    }
}

void SceneManager::Draw() {
    if (scene_) {
        scene_->Draw();
    }
}
