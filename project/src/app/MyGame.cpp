#include "app/MyGame.h"
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
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace {

float ToMilliseconds(
    const std::chrono::steady_clock::time_point& begin,
    const std::chrono::steady_clock::time_point& end)
{
    return std::chrono::duration<float, std::milli>(end - begin).count();
}

}

MyGame::MyGame(SmokeTestOptions smokeTestOptions)
    : smokeTestOptions_(std::move(smokeTestOptions))
{
    if (smokeTestOptions_.enabled && smokeTestOptions_.logPath.empty()) {
        smokeTestOptions_.logPath = "smoke-test.log";
    }
}
MyGame::~MyGame() = default;

void MyGame::Initialize() {
    if (smokeTestOptions_.enabled) {
        smokeTestStartTime_ = std::chrono::steady_clock::now();

        const auto parentPath = smokeTestOptions_.logPath.parent_path();
        if (!parentPath.empty()) {
            std::error_code error;
            std::filesystem::create_directories(parentPath, error);
        }

        std::ofstream clearLog(
            smokeTestOptions_.logPath,
            std::ios::out | std::ios::trunc);
        clearLog.close();

        std::ostringstream message;
        message << "SMOKE_TEST_START gameplay_seconds="
                << smokeTestOptions_.gameplaySeconds
                << " startup_timeout_seconds="
                << smokeTestOptions_.startupTimeoutSeconds;
        WriteSmokeLog(message.str());
    }

    Framework::Initialize();

    if (smokeTestOptions_.enabled) {
        WriteSmokeLog("SMOKE_TEST_ENGINE_INITIALIZED");
    }

    sceneFactory_ = std::make_unique<SceneFactory>();

    SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
    SceneManager::GetInstance()->SetSystems(
        dxCommon_.get(),
        srvManager_.get(),
        spriteCommon_.get(),
        imguiManager_.get(),
        input_.get()
    );

    SceneManager::GetInstance()->SetNextScene(SceneType::Title);
}

void MyGame::Update() {
    const auto updateBegin = std::chrono::steady_clock::now();
    dxCommon_->EditFrameTiming().bufferCreates = 0;
    Framework::Update();
    if (endRequst_) {
        if (smokeTestOptions_.enabled && !smokeTestFinished_) {
            FailSmokeTest("window_closed_before_completion", 2);
        }
        if (dxCommon_) {
            dxCommon_->EditFrameTiming().updateMs =
                ToMilliseconds(updateBegin, std::chrono::steady_clock::now());
        }
        return;
    }

    imguiManager_->Begin();

#ifdef _DEBUG
    if (smokeTestOptions_.enabled && (smokeTestOptions_.playthrough || smokeTestOptions_.phantom || smokeTestOptions_.chargeShot || smokeTestOptions_.boss || smokeTestOptions_.rift)) {
        if (auto* game = dynamic_cast<GameScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
            try {
                playthroughComplete_ = smokeTestOptions_.rift ?
                    game->RunRiftProbe(smokeTestOptions_.logPath.string(), smokeTestOptions_.riftPreview) :
                    smokeTestOptions_.boss ?
                    game->RunBossProbe(smokeTestOptions_.logPath.string(), smokeTestOptions_.bossPreview) :
                    smokeTestOptions_.chargeShot ?
                    game->RunChargeShotProbe(smokeTestOptions_.logPath.string(), smokeTestOptions_.chargePreview) :
                    smokeTestOptions_.phantom ?
                    game->RunPhantomProbe(smokeTestOptions_.logPath.string(), smokeTestOptions_.phantomPreview) :
                    game->RunPlaythroughProbe(smokeTestOptions_.logPath.string(), smokeTestOptions_.tutorialPreview);
            } catch (const std::exception& error) {
                // 通し試験の失敗理由を保存する。デバッガ未接続でも終了理由を追えるようにする。
                WriteSmokeLog(std::string("PROBE_EXCEPTION ") + error.what());
                throw;
            }
        }
    }
#endif

    SceneManager::GetInstance()->Update();

    imguiManager_->End();

    UpdateSmokeTest();

    dxCommon_->EditFrameTiming().updateMs =
        ToMilliseconds(updateBegin, std::chrono::steady_clock::now());
}

void MyGame::Draw() {
    auto& timing = dxCommon_->EditFrameTiming();
    const auto drawBegin = std::chrono::steady_clock::now();
    auto sectionBegin = drawBegin;

    dxCommon_->PreDraw();
    srvManager_->PreDraw();
    timing.preDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    sectionBegin = std::chrono::steady_clock::now();
    // タイトル・編集画面・再入場へ飛行中のブラー設定を持ち越さない。
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
        }
    }

    sectionBegin = std::chrono::steady_clock::now();
    dxCommon_->DrawRenderTextureToSwapChain(postEffectMode);
    if (auto* titleScene = dynamic_cast<TitleScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        titleScene->DrawDepartureOverlay();
    }
    timing.postEffectMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    sectionBegin = std::chrono::steady_clock::now();
    imguiManager_->Draw();
    if (auto* titleScene = dynamic_cast<TitleScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        titleScene->DrawFlightOverlay();
    }
    timing.imguiDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    dxCommon_->PostDraw();
    timing.frameCpuMs =
        timing.updateMs +
        ToMilliseconds(drawBegin, std::chrono::steady_clock::now());
    if (smokeTestOptions_.enabled && smokeGameplayStarted_ &&
        smokeFrameSampleCount_ < smokeFrameSamples_.size()) {
        const float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - smokeGameplayStartTime_).count();
        if (elapsed < 10.0f) {
            smokeFrameSamples_[smokeFrameSampleCount_++] = { elapsed, timing.frameCpuMs,
                timing.updateMs, timing.sceneDrawMs,
                timing.presentMs, timing.fenceWaitMs, timing.bufferCreates };
        }
    }
}

void MyGame::Finalize() {
    WriteSmokePerformanceSummary();
    SceneManager::GetInstance()->FinalizeCurrentScene();
    ModelManager::GetInstance()->Finalize();
    sceneFactory_.reset();
    Framework::Finalize();

    if (smokeTestOptions_.enabled) {
        if (!smokeTestFinished_) {
            WriteSmokeLog("SMOKE_TEST_FAIL reason=finalized_before_completion");
            exitCode_ = 3;
        }

        std::ostringstream message;
        message << "SMOKE_TEST_END exit_code=" << exitCode_;
        WriteSmokeLog(message.str());
    }
}

void MyGame::UpdateSmokeTest()
{
    if (!smokeTestOptions_.enabled || smokeTestFinished_) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    auto* sceneManager = SceneManager::GetInstance();
    auto* titleScene = dynamic_cast<TitleScene*>(sceneManager->GetCurrentScene());

    if (!smokeGameplayStarted_) {
        if (auto* gameScene = dynamic_cast<GameScene*>(sceneManager->GetCurrentScene())) {
            if (gameScene->IsTutorial() != smokeTestOptions_.tutorial) {
                FailSmokeTest("wrong_play_mode", 6);
                return;
            }
            smokeGameplayStarted_ = true;
            smokeGameplayStartTime_ = now;
            WriteSmokeLog(smokeTestOptions_.tutorial ?
                "SMOKE_TEST_GAMEPLAY_ENTERED mode=tutorial" :
                "SMOKE_TEST_GAMEPLAY_ENTERED mode=game");
        } else if (!smokeAutoStartRequested_ &&
                   titleScene && GameScene::AreResourcesPreloaded()) {
            smokeAutoStartRequested_ = true;
            titleScene->RequestStart(smokeTestOptions_.tutorial);
            WriteSmokeLog("SMOKE_TEST_AUTO_START_REQUESTED");
        }

        const double startupElapsedSeconds =
            std::chrono::duration<double>(now - smokeTestStartTime_).count();
        if (!smokeGameplayStarted_ &&
            startupElapsedSeconds >= smokeTestOptions_.startupTimeoutSeconds) {
            FailSmokeTest("gameplay_start_timeout", 4);
        }
        return;
    }

    if (!dynamic_cast<GameScene*>(sceneManager->GetCurrentScene())) {
        FailSmokeTest("gameplay_scene_exited_early", 5);
        return;
    }

    ++smokeGameplayFrameCount_;
    const double gameplayElapsedSeconds =
        std::chrono::duration<double>(now - smokeGameplayStartTime_).count();
    if ((smokeTestOptions_.playthrough || smokeTestOptions_.phantom || smokeTestOptions_.chargeShot || smokeTestOptions_.boss || smokeTestOptions_.rift) && !playthroughComplete_ &&
        gameplayElapsedSeconds >= smokeTestOptions_.gameplaySeconds) {
        FailSmokeTest("playthrough_timeout", 7);
        return;
    }
    if (playthroughComplete_ || gameplayElapsedSeconds >= smokeTestOptions_.gameplaySeconds) {
        std::ostringstream message;
        message << "SMOKE_TEST_PASS gameplay_elapsed_seconds="
                << std::fixed << std::setprecision(3)
                << gameplayElapsedSeconds
                << " monitored_frames=" << smokeGameplayFrameCount_;
        WriteSmokeLog(message.str());

        smokeTestFinished_ = true;
        exitCode_ = 0;
        endRequst_ = true;
    }
}

void MyGame::WriteSmokePerformanceSummary() const
{
    if (!smokeTestOptions_.enabled || smokeFrameSampleCount_ == 0) {
        return;
    }
    constexpr std::array<float, 4> bounds{ 0.0f, 2.0f, 5.0f, 10.0f };
    for (size_t range = 0; range + 1 < bounds.size(); ++range) {
        std::array<float, 720> frameTimes{};
        size_t count = 0, over33 = 0;
        uint32_t buffers = 0;
        float frameTotal = 0.0f, updateTotal = 0.0f;
        float drawTotal = 0.0f, presentTotal = 0.0f, fenceTotal = 0.0f;
        for (size_t index = 0; index < smokeFrameSampleCount_; ++index) {
            const auto& sample = smokeFrameSamples_[index];
            if (sample.elapsed < bounds[range] || sample.elapsed >= bounds[range + 1]) { continue; }
            frameTimes[count++] = sample.frame;
            over33 += sample.frame > 33.333f ? 1u : 0u;
            frameTotal += sample.frame;
            updateTotal += sample.update;
            drawTotal += sample.draw;
            presentTotal += sample.present;
            fenceTotal += sample.fence;
            buffers += sample.buffers;
        }
        if (count == 0) { continue; }
        std::sort(frameTimes.begin(), frameTimes.begin() + count);
        const float divisor = static_cast<float>(count);
        std::ostringstream message;
        message << std::fixed << std::setprecision(2)
            << "STARTUP_PERF seconds=" << bounds[range] << '-' << bounds[range + 1]
            << " frames=" << count << " avg_ms=" << frameTotal / divisor
            << " p95_ms=" << frameTimes[(count - 1) * 95 / 100]
            << " max_ms=" << frameTimes[count - 1] << " over33=" << over33
            << " update_avg_ms=" << updateTotal / divisor
            << " draw_avg_ms=" << drawTotal / divisor
            << " present_avg_ms=" << presentTotal / divisor << " fence_avg_ms=" << fenceTotal / divisor
            << " buffer_creates=" << buffers;
        WriteSmokeLog(message.str());
    }
}

void MyGame::FailSmokeTest(std::string_view reason, int exitCode)
{
    if (smokeTestFinished_) {
        return;
    }

    std::ostringstream message;
    message << "SMOKE_TEST_FAIL reason=" << reason;
    WriteSmokeLog(message.str());

    smokeTestFinished_ = true;
    exitCode_ = exitCode;
    endRequst_ = true;
}

void MyGame::WriteSmokeLog(std::string_view message) const
{
    if (!smokeTestOptions_.enabled || smokeTestOptions_.logPath.empty()) {
        return;
    }

    double elapsedSeconds = 0.0;
    if (smokeTestStartTime_.time_since_epoch().count() != 0) {
        elapsedSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - smokeTestStartTime_).count();
    }

    std::ofstream log(smokeTestOptions_.logPath, std::ios::out | std::ios::app);
    if (!log) {
        return;
    }

    log << '[' << std::fixed << std::setprecision(3)
        << elapsedSeconds << "s] " << message << '\n';
}
