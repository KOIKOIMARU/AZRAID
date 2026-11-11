#include "app/debug/GameplaySmokeTest.h"

#include "engine/base/DirectXCommon.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/TitleScene.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

GameplaySmokeTest::GameplaySmokeTest(SmokeTestOptions options)
    : options_(std::move(options))
{
    if (options_.enabled && options_.logPath.empty()) {
        options_.logPath = "smoke-test.log";
    }
}

// 自動確認の開始時刻と出力先を準備し、検証条件をログに書く。
void GameplaySmokeTest::Begin()
{
    if (options_.enabled) {
        smokeTestStartTime_ = std::chrono::steady_clock::now();

        const auto parentPath = options_.logPath.parent_path();
        if (!parentPath.empty()) {
            std::error_code error;
            std::filesystem::create_directories(parentPath, error);
        }

        std::ofstream clearLog(
            options_.logPath,
            std::ios::out | std::ios::trunc);
        clearLog.close();

        std::ostringstream message;
        message << "SMOKE_TEST_START gameplay_seconds="
                << options_.gameplaySeconds
                << " startup_timeout_seconds="
                << options_.startupTimeoutSeconds;
        WriteLog(message.str());
    }
}

// テスト完了前にウィンドウが閉じられた場合、早期終了として失敗を記録する。
void GameplaySmokeTest::OnWindowClosed()
{
    if (options_.enabled && !smokeTestFinished_) {
        Fail("window_closed_before_completion", 2);
    }
}

bool GameplaySmokeTest::HasGameplayProbe() const
{
    return options_.playthrough || options_.phantom || options_.chargeShot ||
        options_.boss || options_.rift || options_.defeat;
}

// 起動時に指定した確認内容に応じて、自動プレイや個別機能の検証を進める。
void GameplaySmokeTest::UpdateProbe(SceneManager& scenes)
{
#ifdef _DEBUG
    if (options_.enabled && HasGameplayProbe()) {
        if (auto* game = dynamic_cast<GameScene*>(scenes.GetCurrentScene())) {
            try {
                playthroughComplete_ = options_.defeat ?
                    game->RunDefeatProbe(options_.logPath.string(), options_.defeatPreview) :
                    options_.rift ?
                    game->RunRiftProbe(options_.logPath.string(), options_.riftPreview) :
                    options_.boss ?
                    game->RunBossProbe(options_.logPath.string(), options_.bossPreview) :
                    options_.chargeShot ?
                    game->RunChargeShotProbe(options_.logPath.string(), options_.chargePreview) :
                    options_.phantom ?
                    game->RunPhantomProbe(options_.logPath.string(), options_.phantomPreview) :
                    game->RunPlaythroughProbe(options_.logPath.string(), options_.tutorialPreview,
                        options_.tutorialDeparture);
            } catch (const std::exception& error) {
                // 通し試験の失敗理由を保存する。デバッガ未接続でも終了理由を追えるようにする。
                WriteLog(std::string("PROBE_EXCEPTION ") + error.what());
                throw;
            }
        }
    }
#else
    (void)scenes;
#endif
}

// 本編開始前のロード進捗と作業時間を記録し、準備の順番も確認する。
void GameplaySmokeTest::ObservePreparation(bool startupLoading)
{
    if (options_.enabled && !smokeGameplayStarted_) {
        const auto preparation = GameRuntime::GetInitializationTiming();
        if (!startupLoading && !options_.tutorial &&
            selection_ != Selection::Confirmed && preparation.workMs > 0.0f) {
            Fail("game_preparation_repeated_after_startup", 8);
        }
        if (preparation.workMs > 0.0f) {
            for (auto& sample : smokePreparationWork_) {
                if (sample.label && sample.label != preparation.label) { continue; }
                sample.label = preparation.label;
                ++sample.frames;
                sample.totalMs += preparation.workMs;
                sample.maxMs = (std::max)(sample.maxMs, preparation.workMs);
                break;
            }
        }
        if (preparation.workMs > 0.0f && preparation.label != smokePrepareLabel_) {
            smokePrepareLabel_ = preparation.label;
            std::ostringstream message;
            message << "TITLE_PREPARE label=" << preparation.label << " work_ms=" << preparation.workMs;
            WriteLog(message.str());
        }
        const int step = GameScene::GetResourcePreloadStep();
        if (step != smokeTitleLoadStep_) {
            smokeTitleLoadStep_ = step;
            std::ostringstream message;
            message << "TITLE_LOAD step=" << step << '/' << GameScene::GetResourcePreloadStepCount()
                << " label=" << GameScene::GetResourcePreloadLabel()
                << " work_ms=" << GameScene::GetResourcePreloadLastStepMs();
            WriteLog(message.str());
        }
    }
}

// 起動時とプレイ中のフレームを記録し、ロード進捗や描画時間を検証ログに残す。
void GameplaySmokeTest::ObserveFrame(const DirectXCommon& renderer)
{
    const auto& timing = renderer.GetFrameTiming();
    if (options_.enabled && !smokeGameplayStarted_ && smokeTitleFrameCount_ < smokeTitleFrameTimes_.size()) {
        smokeTitleFrameTimes_[smokeTitleFrameCount_++] = timing.frameCpuMs;
        if (timing.frameCpuMs > 100.0f) {
            std::ostringstream message;
            message << "TITLE_HITCH frame_ms=" << timing.frameCpuMs << " update_ms=" << timing.updateMs
                << " draw_ms=" << timing.sceneDrawMs << " post_ms=" << timing.postEffectMs
                << " present_ms=" << timing.presentMs << " fence_ms=" << timing.fenceWaitMs
                << " resource_step=" << GameScene::GetResourcePreloadStep()
                << " resource_label=" << GameScene::GetResourcePreloadLabel()
                << " prepare_label=" << GameRuntime::GetInitializationTiming().label
                << " prepare_ms=" << GameRuntime::GetInitializationTiming().workMs;
            WriteLog(message.str());
        }
    }
    if (options_.enabled && smokeGameplayStarted_ &&
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

// テストの終了コードをログに書く。完了の記録がないまま終了した場合は失敗を残す。
void GameplaySmokeTest::End()
{
    if (options_.enabled) {
        if (!smokeTestFinished_) { Fail("finalized_before_completion", 3); }

        std::ostringstream message;
        message << "SMOKE_TEST_END exit_code=" << exitCode_;
        WriteLog(message.str());
    }
}

// タイトル、ステージ選択、本編の順に自動操作を進め、起動タイムアウトと終了条件を確認する。
void GameplaySmokeTest::Update(SceneManager& scenes)
{
    if (!options_.enabled || smokeTestFinished_) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    auto* titleScene = dynamic_cast<TitleScene*>(scenes.GetCurrentScene());

    if (!smokeGameplayStarted_) {
        if (auto* gameScene = dynamic_cast<GameScene*>(scenes.GetCurrentScene())) {
            if (!options_.tutorial && selection_ != Selection::Confirmed) {
                Fail("stage_selection_bypassed",8); return;
            }
            if (gameScene->IsTutorial() != options_.tutorial) {
                Fail("wrong_play_mode", 6);
                return;
            }
            smokeGameplayStarted_ = true;
            smokeGameplayStartTime_ = now;
            WriteLog(options_.tutorial ?
                "SMOKE_TEST_GAMEPLAY_ENTERED mode=tutorial" :
                "SMOKE_TEST_GAMEPLAY_ENTERED mode=game");
        } else if (!smokeAutoStartRequested_ &&
                   titleScene && GameScene::AreResourcesPreloaded()) {
            if (options_.tutorial) {
                titleScene->RequestStart(true);
                smokeAutoStartRequested_ = true;
            } else if (selection_ == Selection::NotRequested) {
                titleScene->RequestStart();
                selection_ = Selection::OpeningFirst;
                WriteLog("TITLE_CUT_TO_STAGE_SELECTION_REQUESTED");
            } else if (selection_ == Selection::OpeningFirst && titleScene->IsStageSelectionOpen()) {
                smokeStageSelectionTime_ = now;
                selection_ = Selection::WaitingToReturn;
                WriteLog("STAGE_SELECTION_SCENE_ENTERED");
            } else if (selection_ == Selection::WaitingToReturn &&
                std::chrono::duration<double>(now-smokeStageSelectionTime_).count() >= 0.65) {
                    if (titleScene->IsDepartureRequested()) {
                        Fail("stage_selection_started_without_confirmation", 8); return;
                    }
                    titleScene->CancelStageSelection();
                    WriteLog("STAGE_SELECTION_RETURN_REQUESTED");
                    selection_ = Selection::ReturningToTitle;
            } else if (selection_ == Selection::ReturningToTitle && !titleScene->IsStageSelectionOpen()) {
                    WriteLog("STAGE_SELECTION_RETURN_TO_TITLE_OK");
                    titleScene->RequestStart();
                    selection_ = Selection::OpeningAgain;
            } else if (selection_ == Selection::OpeningAgain && titleScene->IsStageSelectionOpen()) {
                    smokeStageSelectionTime_ = now;
                    selection_ = Selection::WaitingToConfirm;
                    WriteLog("STAGE_SELECTION_SCENE_REENTERED");
            } else if (selection_ == Selection::WaitingToConfirm &&
                std::chrono::duration<double>(now-smokeStageSelectionTime_).count() >= 0.65 && titleScene->ConfirmStageSelection()) {
                    WriteLog("STAGE_SELECTION_CONFIRM_OK");
                    selection_ = Selection::Confirmed;
                    smokeAutoStartRequested_ = true;
            }
            if (smokeAutoStartRequested_) { WriteLog("SMOKE_TEST_AUTO_START_REQUESTED"); }
        }

        const double startupElapsedSeconds =
            std::chrono::duration<double>(now - smokeTestStartTime_).count();
        if (!smokeGameplayStarted_ &&
            startupElapsedSeconds >= options_.startupTimeoutSeconds) {
            Fail("gameplay_start_timeout", 4);
        }
        return;
    }

    const bool gameplaySceneActive = dynamic_cast<GameScene*>(scenes.GetCurrentScene()) != nullptr;
    const bool tutorialDeparture = options_.tutorialDeparture;
    const bool gameplaySceneLoading = scenes.IsMenuSceneLoading(
        options_.tutorial ? SceneType::Tutorial : SceneType::Game) ||
        (tutorialDeparture && scenes.IsMenuSceneLoading(SceneType::Departure));
    if (tutorialDeparture && titleScene && titleScene->IsAutomaticDeparture()) {
        if (smokeTutorialDepartureClock_ < 0.0f) { WriteLog("TUTORIAL DEPARTURE_SCENE_ENTERED"); }
        smokeTutorialDepartureClock_ = (std::max)(0.0f, titleScene->GetDepartureClock());
        if (std::chrono::duration<double>(now - smokeGameplayStartTime_).count() >= options_.gameplaySeconds) {
            Fail("tutorial_departure_timeout", 7);
        }
        return;
    }
    if (tutorialDeparture && gameplaySceneActive &&
        !static_cast<GameScene*>(scenes.GetCurrentScene())->IsTutorial() &&
        smokeTutorialDepartureClock_ < GameRuntime::kStageStartOverviewDuration - 0.05f) {
        Fail("tutorial_departure_skipped", 7); return;
    }
    if (!gameplaySceneActive && !gameplaySceneLoading) {
        Fail("gameplay_scene_exited_early", 5);
        return;
    }

    if (gameplaySceneActive) { ++smokeGameplayFrameCount_; }
    const double gameplayElapsedSeconds =
        std::chrono::duration<double>(now - smokeGameplayStartTime_).count();
    if (HasGameplayProbe() && !playthroughComplete_ &&
        gameplayElapsedSeconds >= options_.gameplaySeconds) {
        Fail("playthrough_timeout", 7);
        return;
    }
    if (gameplaySceneLoading) { return; }
    if (playthroughComplete_ || gameplayElapsedSeconds >= options_.gameplaySeconds) {
        std::ostringstream message;
        message << "SMOKE_TEST_PASS gameplay_elapsed_seconds="
                << std::fixed << std::setprecision(3)
                << gameplayElapsedSeconds
                << " monitored_frames=" << smokeGameplayFrameCount_;
        WriteLog(message.str());

        smokeTestFinished_ = true;
        exitCode_ = 0;
    }
}

// ロードの工程別時間とフレーム時間の集計をログへ出す。
void GameplaySmokeTest::WritePerformanceSummary() const
{
    if (options_.enabled) {
        for (const auto& sample : smokePreparationWork_) {
            if (!sample.label) { continue; }
            std::ostringstream message;
            message << std::fixed << std::setprecision(3) << "TITLE_PREPARE_STATS label=" << sample.label
                << " frames=" << sample.frames << " max_ms=" << sample.maxMs << " total_ms=" << sample.totalMs;
            WriteLog(message.str());
        }
    }
    if (options_.enabled && smokeTitleFrameCount_ > 0) {
        auto times = smokeTitleFrameTimes_;
        std::sort(times.begin(), times.begin() + smokeTitleFrameCount_);
        float total = 0.0f;
        size_t over33 = 0, over100 = 0;
        for (size_t i = 0; i < smokeTitleFrameCount_; ++i) {
            total += times[i];
            over33 += times[i] > 33.333f ? 1u : 0u;
            over100 += times[i] > 100.0f ? 1u : 0u;
        }
        std::ostringstream message;
        message << std::fixed << std::setprecision(2) << "TITLE_PERF frames=" << smokeTitleFrameCount_
            << " avg_ms=" << total / static_cast<float>(smokeTitleFrameCount_)
            << " p95_ms=" << times[(smokeTitleFrameCount_ - 1) * 95 / 100]
            << " max_ms=" << times[smokeTitleFrameCount_ - 1]
            << " over33=" << over33 << " over100=" << over100;
        WriteLog(message.str());
    }
    if (!options_.enabled || smokeFrameSampleCount_ == 0) {
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
        WriteLog(message.str());
    }
}

// 最初に起きた失敗の理由と終了コードを記録して自動確認を止める。
void GameplaySmokeTest::Fail(std::string_view reason, int exitCode)
{
    if (smokeTestFinished_) {
        return;
    }

    std::ostringstream message;
    message << "SMOKE_TEST_FAIL reason=" << reason;
    WriteLog(message.str());

    smokeTestFinished_ = true;
    exitCode_ = exitCode;
}

// 検証開始からの経過時間を付けて、メッセージをログに追記する。
void GameplaySmokeTest::WriteLog(std::string_view message) const
{
    if (!options_.enabled || options_.logPath.empty()) {
        return;
    }

    double elapsedSeconds = 0.0;
    if (smokeTestStartTime_.time_since_epoch().count() != 0) {
        elapsedSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - smokeTestStartTime_).count();
    }

    std::ofstream log(options_.logPath, std::ios::out | std::ios::app);
    if (!log) {
        return;
    }

    log << '[' << std::fixed << std::setprecision(3)
        << elapsedSeconds << "s] " << message << '\n';
}
