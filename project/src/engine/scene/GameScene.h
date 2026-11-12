// 本編と練習のシーン窓口　ゲーム中の処理をGameRuntimeへ渡し、出撃や結果からの切り替えを扱う

#pragma once

#include "app/runtime/GameRuntime.h"
#include "engine/scene/BaseScene.h"

class GameScene : public BaseScene {
public:
    explicit GameScene(GameRuntime::PlayMode mode = GameRuntime::PlayMode::Game, StageRecords* records = nullptr);
    ~GameScene() override;

    static bool PreloadResourcesStep(DirectXCommon* dxCommon, SrvManager* srvManager);
    static bool AreResourcesPreloaded();
    static int GetResourcePreloadStep();
    static int GetResourcePreloadStepCount();
    static const char* GetResourcePreloadLabel();
    // リソース先読み直前の段階ミリ秒を返す
    static float GetResourcePreloadLastStepMs();
    static float GetResourcePreloadTotalMs();

    // ゲーム本体に共通システムを渡して開始する　再挑戦の場合は短い出撃演出を使う
    void Initialize() override;
    void BeginInitialize() override;
    bool InitializeStep() override;
    float GetInitializationProgress() const override { return runtime_.GetInitializationProgress(); }
    void Finalize() override;
    void Update() override;
    void UpdateMenuArrival() override { runtime_.DrawMenuArrivalHud(); }
    void Draw() override;
    void UpdateTitleEntry(float time) { runtime_.UpdateTitleEntry(time); }
    void BeginTitleEntry(const Math::Vector3& position, const Math::Vector3& rotation,
        const Math::Vector3& cameraRotation, float scale) {
        runtime_.BeginTitleEntry(position, rotation, cameraRotation, scale);
    }
    void DrawTitleEntryFlight() { runtime_.DrawTitleEntryFlight(); }
    bool IsTitleEntryForeground() const { return runtime_.IsTitleEntryForeground(); }
    void DrawTitleEntryHud() { runtime_.DrawTitleEntryHud(); }
    void UpdateStagePreview(float elapsed) { runtime_.UpdateStagePreview(elapsed); }
    void UpdateStageReturn(float elapsed) { runtime_.UpdateStageReturn(elapsed); }
    void SetStagePortalSeam(Math::Vector2 seam) { runtime_.SetStagePortalSeam(seam); }
    float GetStageReturnDuration() const { return runtime_.GetStageReturnDuration(); }
    void UpdateStageOverview(float elapsed) { runtime_.UpdateStageOverview(elapsed); }
    void DrawStageBriefing(float elapsed) { runtime_.DrawStageBriefing(elapsed); }
    Camera* GetStageCamera() const { return runtime_.GetStageCamera(); }
    const Math::Matrix4x4& GetStagePortalFrame() const { return runtime_.GetStagePortalFrame(); }

    int GetPostEffectMode() const;
    const Math::Matrix4x4& GetProjectionMatrix() const;
    bool IsTutorial() const { return mode_ == GameRuntime::PlayMode::Tutorial; }
#ifdef _DEBUG
    bool RunPlaythroughProbe(const std::string& logPath, bool tutorialPreview = false, bool tutorialDeparture = false) {
        return runtime_.RunPlaythroughProbe(logPath, tutorialPreview, tutorialDeparture);
    }
    bool RunPhantomProbe(const std::string& logPath, bool preview) { return runtime_.RunPhantomProbe(logPath, preview); }
    bool RunChargeShotProbe(const std::string& logPath, bool preview) { return runtime_.RunChargeShotProbe(logPath, preview); }
    bool RunRiftProbe(const std::string& logPath, bool preview) { return runtime_.RunRiftProbe(logPath, preview); }
    bool RunBossProbe(const std::string& logPath, bool preview) { return runtime_.RunBossProbe(logPath, preview); }
    bool RunDefeatProbe(const std::string& logPath, bool preview) { return runtime_.RunDefeatProbe(logPath, preview); }
#endif

private:
    GameRuntime::PlayMode mode_; // SceneFactoryで指定する入場先　途中では切り替えない
    bool shortEntry_ = false;
    GameRuntime runtime_;
};
