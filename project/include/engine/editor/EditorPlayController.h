// エディタから試しに遊ぶためのGameRuntimeを所有し、開始・終了・描画をつなぐ。

#pragma once

#include <memory>

#include "engine/base/Math.h"

class DirectXCommon;
class GameRuntime;
class ImGuiManager;
class Input;
class SpriteCommon;
class SrvManager;

class EditorPlayController {
public:
    EditorPlayController();
    ~EditorPlayController();

    void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input);
    bool Start();
    void Stop();
    void Update(bool isEditorGuiVisible, bool showSkybox, int postEffectMode);
    void SetHudViewportRect(bool isEnabled, const Math::Vector2& min, const Math::Vector2& size);
    void Draw();
    int GetPostEffectMode() const;
    const Math::Matrix4x4* GetProjectionMatrix() const;
    bool IsRunning() const;
    bool IsExitRequested() const;

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;
    std::unique_ptr<GameRuntime> runtime_;
};
