// ===== Framework.cpp =====
// ウィンドウのメッセージ処理とゲームループを回し、初期化から終了までの順序を管理する。

#include "engine/base/Framework.h"

#include "engine/base/WinApp.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/base/ImGuiManager.h"
#include "engine/io/Input.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/3d/ParticleManager.h"

Framework::~Framework() = default;

// アプリを準備し、終了要求が来るまで1フレームずつ更新と描画を繰り返す。
int Framework::Run() {
    Initialize();

    try {
        while (true) {
            Update();
            if (IsEndRequst()) { break; }
            Draw();
        }
    } catch (...) {
        Finalize();
        throw;
    }

    Finalize();
    return exitCode_;
}

// ウィンドウを開き、DirectXと入力、UIや素材管理を依存する順に初期化する。
void Framework::Initialize() {
    winApp_ = std::make_unique<WinApp>();
    winApp_->Initialize();

    dxCommon_ = std::make_unique<DirectXCommon>();
    dxCommon_->Initialize(winApp_.get());

    srvManager_ = std::make_unique<SrvManager>();
    srvManager_->Initialize(dxCommon_.get());
    dxCommon_->InitializeRenderTexture(srvManager_.get());

    imguiManager_ = std::make_unique<ImGuiManager>();
    imguiManager_->Initialize(winApp_.get(), dxCommon_.get(), srvManager_.get());

    TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

    input_ = std::make_unique<Input>();
    input_->Initialize(winApp_.get());

    spriteCommon_ = std::make_unique<SpriteCommon>();
    spriteCommon_->Initialize(dxCommon_.get(), srvManager_.get());

    ParticleManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());
}

// ウィンドウの終了要求を先に処理し、継続する場合は画面サイズと入力を更新する。
void Framework::Update() {
    // Windowsメッセージ処理（終了フラグに変換）
    if (winApp_->ProcessMessage()) {
        endRequst_ = true;
        return;
    }

    dxCommon_->ResizePresentation();
    input_->Update();
}

// ゲームから使う素材やUIの管理を終了し、共通システムを解放する。
void Framework::Finalize() {
    ParticleManager::GetInstance()->Finalize();
    TextureManager::GetInstance()->Destroy();

    if (imguiManager_) {
        imguiManager_->Finalize();
    }

    if (winApp_) {
        winApp_->Finalize();
    }

    // unique_ptr なので delete 不要
    spriteCommon_.reset();
    input_.reset();
    imguiManager_.reset();
    srvManager_.reset();
    dxCommon_.reset();
    winApp_.reset();
}
