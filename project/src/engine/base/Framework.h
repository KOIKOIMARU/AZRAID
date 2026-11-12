// アプリ共通の初期化・更新・描画・終了を管理する　ゲーム固有の処理は派生クラスが担当する

#pragma once
#include <memory>

#include "engine/io/Input.h"
#include "engine/2d/SpriteCommon.h"

class WinApp;
class DirectXCommon;
class SrvManager;
class ImGuiManager;

class Framework {
public:
    virtual ~Framework();

    int Run();

    virtual void Initialize();
    virtual void Finalize();
    // ウィンドウの終了要求を先に処理し、継続する場合は画面サイズと入力を更新する
    virtual void Update();
    virtual void Draw() = 0;

    virtual bool IsEndRequst() { return endRequst_; }

protected:
    bool endRequst_ = false;
    int exitCode_ = 0;

    std::unique_ptr<WinApp> winApp_;
    std::unique_ptr<DirectXCommon> dxCommon_;
    std::unique_ptr<SrvManager> srvManager_;
    std::unique_ptr<ImGuiManager> imguiManager_;
    std::unique_ptr<Input> input_;
    std::unique_ptr<SpriteCommon> spriteCommon_;
};
