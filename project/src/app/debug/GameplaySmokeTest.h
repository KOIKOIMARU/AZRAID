#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string_view>

class DirectXCommon;
class SceneManager;

struct SmokeTestOptions {
    bool enabled = false;
    bool tutorial = false; // スモークテストの入場先。通常起動には影響しない。
    bool tutorialPreview = false; // Debug専用。各練習の実画面で止めて確認する。
    bool tutorialDeparture = false; // Debug専用。全練習から通常の出撃カット・本編まで通す。
    bool playthrough = false; // Debug専用。通常の射撃・被弾ルールでクリアと再挑戦を検証。
    bool phantom = false; // Debug専用。残像連撃の条件・対象消失・ボス撃破を試験配置で検証。
    bool phantomPreview = false; // 映像確認用。代表フレームで止め、NEXTボタンで再開する。
    bool chargeShot = false; // Debug専用。チャージ爆風の境界・重複命中・本編編隊への実射を検証。
    bool chargePreview = false;
    bool rift = false; // Debug専用。照準・実射・裂け目通過・延長・固定プールを検証。
    bool riftPreview = false;
    bool boss = false; // Debug専用。全攻撃・固定照準・反撃時間・音楽切替を検証。
    bool bossPreview = false;
    bool defeat = false; // Debug専用。致命弾・失速・結果・再挑戦の連続性を検証する。
    bool defeatPreview = false;
    double gameplaySeconds = 15.0;
    double startupTimeoutSeconds = 120.0;
    std::filesystem::path logPath;
};

// 起動オプションで有効になる自動検証。通常プレイとは別に成績と計測結果を扱う。
class GameplaySmokeTest {
public:
    explicit GameplaySmokeTest(SmokeTestOptions options);

    bool IsEnabled() const { return options_.enabled; }
    bool ShouldExit() const { return smokeTestFinished_; }
    int GetExitCode() const { return exitCode_; }
    const std::filesystem::path& GetLogPath() const { return options_.logPath; }
    void Begin();
    void OnWindowClosed();
    void UpdateProbe(SceneManager& scenes);
    void ObservePreparation(bool startupLoading);
    void ObserveFrame(const DirectXCommon& renderer);
    void Update(SceneManager& scenes);
    void WriteLog(std::string_view message) const;
    void WritePerformanceSummary() const;
    void End();

private:
    enum class Selection {
        NotRequested,
        OpeningFirst,
        WaitingToReturn,
        ReturningToTitle,
        OpeningAgain,
        WaitingToConfirm,
        Confirmed
    };

    bool HasGameplayProbe() const;
    void Fail(std::string_view reason, int exitCode);

    struct SmokeFrameSample {
        float elapsed = 0.0f, frame = 0.0f, update = 0.0f;
        float draw = 0.0f, present = 0.0f, fence = 0.0f;
        uint32_t buffers = 0;
    };
    // 先頭10秒を固定領域に記録し、終了後に集計。毎フレームのファイル書込を避ける。
    std::array<SmokeFrameSample, 720> smokeFrameSamples_{};
    size_t smokeFrameSampleCount_ = 0;
    std::array<float, 4096> smokeTitleFrameTimes_{};
    size_t smokeTitleFrameCount_ = 0;
    int smokeTitleLoadStep_ = -1;
    const char* smokePrepareLabel_ = nullptr;
    struct PreparationWorkSample {
        const char* label = nullptr;
        uint32_t frames = 0;
        float totalMs = 0.0f, maxMs = 0.0f;
    };
    std::array<PreparationWorkSample, 32> smokePreparationWork_{};
    SmokeTestOptions options_;
    std::chrono::steady_clock::time_point smokeTestStartTime_{};
    std::chrono::steady_clock::time_point smokeGameplayStartTime_{};
    bool smokeAutoStartRequested_ = false;
    Selection selection_ = Selection::NotRequested;
    std::chrono::steady_clock::time_point smokeStageSelectionTime_{};
    bool smokeGameplayStarted_ = false;
    float smokeTutorialDepartureClock_ = -1.0f;
    bool smokeTestFinished_ = false;
    bool playthroughComplete_ = false;
    uint64_t smokeGameplayFrameCount_ = 0;
    int exitCode_ = 0;
};
