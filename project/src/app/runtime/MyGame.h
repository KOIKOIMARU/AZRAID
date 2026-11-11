#pragma once
#include "app/debug/GameplaySmokeTest.h"
#include "engine/base/Framework.h"
#include "app/ui/StageRecords.h"
#include <chrono>
#include <memory>

class AbstractSceneFactory;

struct GameLaunchOptions {
    SmokeTestOptions smokeTest;
#ifdef _DEBUG
    bool debugDirectStart = false;
#endif
};

class MyGame : public Framework {
public:
    explicit MyGame(GameLaunchOptions options = {});
    ~MyGame() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    void UpdateStartupLoading();
    void DrawStartupLoading();
    StageRecords stageRecords_; // アプリが所有し、タイトルと本編へ同じ記録を渡す。
    std::unique_ptr<AbstractSceneFactory> sceneFactory_;
    bool startupLoading_ = true;
    bool debugDirectStart_ = false;
    bool startupFramePresented_ = false;
    float startupDisplayedProgress_ = 0.0f;
    int startupStep_ = 0;
    std::chrono::steady_clock::time_point startupLoadingStart_{};
    GameplaySmokeTest smokeTest_;
};
