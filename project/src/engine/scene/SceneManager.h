// 現在のシーンを所有し、次のシーンの準備と切り替えを管理する

#pragma once
#include <memory>
#include <array>
#include "engine/scene/SceneType.h"

class BaseScene;
class AbstractSceneFactory;
class DirectXCommon;
class SrvManager;
class SpriteCommon;
class ImGuiManager;
class Input;

class SceneManager {
public:
    static SceneManager* GetInstance();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    ~SceneManager();

    void Update();
    void Draw();
    BaseScene* GetCurrentScene() const { return scene_.get(); }
    bool IsMenuSceneLoading(SceneType sceneType) const {
        return hasNextScene_ && nextSceneMenuWipe_ && nextSceneType_ == sceneType;
    }
    // 現在のシーンに終了処理を行い、所有権と切り替え予約を解除する
    void FinalizeCurrentScene();
    void FinalizeAllScenes();

    void SetNextScene(SceneType sceneType, bool menuWipe = false);
    void PrepareScene(SceneType sceneType);
    bool PrepareSceneStep(SceneType sceneType);
    bool IsScenePrepared(SceneType sceneType) const;
    float GetScenePreparationProgress(SceneType sceneType) const;
    BaseScene* GetPreparedScene(SceneType sceneType) const;
    void SetSceneFactory(AbstractSceneFactory* sceneFactory);

    void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input
    );

private:
    SceneManager() = default;
    void DiscardPreparedScene(SceneType sceneType);

private:
    std::unique_ptr<BaseScene> scene_;
    struct PreparedScene {
        std::unique_ptr<BaseScene> scene;
        bool ready = false;
    };
    // 起動時に準備した本編を、タイトル・ステージ選択の準備で捨てない
    std::array<PreparedScene, static_cast<size_t>(SceneType::Count)> preparedScenes_{};
    SceneType nextSceneType_{};
    bool hasNextScene_ = false;
    bool nextSceneMenuWipe_ = false;
    double menuRevealStarted_ = -1.0;

    AbstractSceneFactory* sceneFactory_ = nullptr;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;
};
