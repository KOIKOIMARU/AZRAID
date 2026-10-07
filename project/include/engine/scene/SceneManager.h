#pragma once
#include <memory>
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
    void FinalizeCurrentScene();

    void SetNextScene(SceneType sceneType);
    void PrepareScene(SceneType sceneType);
    bool IsScenePrepared(SceneType sceneType) const;
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

private:
    std::unique_ptr<BaseScene> scene_;
    std::unique_ptr<BaseScene> preparedScene_;
    SceneType preparedSceneType_{};
    bool hasPreparedScene_ = false;
    SceneType nextSceneType_{};
    bool hasNextScene_ = false;

    AbstractSceneFactory* sceneFactory_ = nullptr;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;
};
