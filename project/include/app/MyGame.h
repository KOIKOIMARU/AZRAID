#pragma once
#include "engine/base/Framework.h"
#include <chrono>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>

class AbstractSceneFactory;

struct SmokeTestOptions {
    bool enabled = false;
    bool tutorial = false; // スモークテストの入場先。通常起動には影響しない。
    bool tutorialPreview = false; // Debug専用。各練習の実画面で止めて確認する。
    bool playthrough = false; // Debug専用。通常の射撃・被弾ルールでクリアと再挑戦を検証。
    bool phantom = false; // Debug専用。残像連撃の条件・対象消失・ボス撃破を試験配置で検証。
    bool phantomPreview = false; // 映像確認用。代表フレームで止め、NEXTボタンで再開する。
    bool chargeShot = false; // Debug専用。チャージ爆風の境界・重複命中・本編編隊への実射を検証。
    bool chargePreview = false;
    bool rift = false; // Debug専用。照準・実射・裂け目通過・延長・固定プールを検証。
    bool riftPreview = false;
    bool boss = false; // Debug専用。全攻撃・固定照準・反撃時間・音楽切替を検証。
    bool bossPreview = false;
    double gameplaySeconds = 15.0;
    double startupTimeoutSeconds = 120.0;
    std::filesystem::path logPath;
};

class MyGame : public Framework {
public:
    explicit MyGame(SmokeTestOptions smokeTestOptions = {});
    ~MyGame() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    void UpdateSmokeTest();
    void FailSmokeTest(std::string_view reason, int exitCode);
    void WriteSmokeLog(std::string_view message) const;
    void WriteSmokePerformanceSummary() const;
    struct SmokeFrameSample {
        float elapsed = 0.0f, frame = 0.0f, update = 0.0f;
        float draw = 0.0f, present = 0.0f, fence = 0.0f;
        uint32_t buffers = 0;
    };
    // 先頭10秒を固定領域に記録し、終了後に集計。毎フレームのファイル書込を避ける。
    std::array<SmokeFrameSample, 720> smokeFrameSamples_{};
    size_t smokeFrameSampleCount_ = 0;

    std::unique_ptr<AbstractSceneFactory> sceneFactory_;
    SmokeTestOptions smokeTestOptions_;
    std::chrono::steady_clock::time_point smokeTestStartTime_{};
    std::chrono::steady_clock::time_point smokeGameplayStartTime_{};
    bool smokeAutoStartRequested_ = false;
    bool smokeGameplayStarted_ = false;
    bool smokeTestFinished_ = false;
    bool playthroughComplete_ = false;
    uint64_t smokeGameplayFrameCount_ = 0;
};
