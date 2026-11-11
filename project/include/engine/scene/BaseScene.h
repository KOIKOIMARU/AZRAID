// 各シーンが実装する初期化・更新・描画・終了の入口。描画や入力に使う共通機能も受け渡す。

#pragma once

class SceneManager;
class DirectXCommon;
class SrvManager;
class SpriteCommon;
class ImGuiManager;
class Input;

class BaseScene {
public:
    virtual ~BaseScene() = default;

    virtual void Initialize() = 0;
    virtual void BeginInitialize() { Initialize(); }
    virtual bool InitializeStep() { return true; }
    virtual float GetInitializationProgress() const { return 0.0f; }
    virtual void Finalize() = 0;
    virtual void Update() = 0;
    virtual void UpdateMenuArrival() { Update(); }
    virtual void Draw() = 0;

    virtual void SetSceneManager(SceneManager* sceneManager) {
        sceneManager_ = sceneManager;
    }

    // シーンが描画や入力に使う共通システムの参照を受け取る。これらのシステムの寿命は呼び出し元が管理する。
    virtual void SetSystems(
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

protected:
    SceneManager* sceneManager_ = nullptr;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;
};
