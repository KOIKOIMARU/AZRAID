// ステージ選択のプレビュー、背景の開口部、出撃時のカメラの受け渡しを管理する

#pragma once

#include "engine/base/Math.h"

#include <array>
#include <cstddef>
#include <memory>

class Camera;
class GameScene;
class Object3d;
class Object3dCommon;
class SrvManager;
class TitleRift;

// タイトルの切断線を引き継ぎ、選択画面の入口と本編へ渡す出撃の時計を管理する
class StageSelectionPresentation {
public:
    StageSelectionPresentation();
    ~StageSelectionPresentation();

    void Reset(bool stageSelection);
    void Clear();
    void PreparePortal(Object3dCommon* common);
    bool PortalReady() const { return stagePortalPanels_.back() != nullptr; }
    void UpdatePortal(GameScene* preview, TitleRift* rift, bool departing);
    void DrawPortal(TitleRift* rift, SrvManager* srv, Object3dCommon* common, float titleElapsed, bool inScene);
    void DrawOverlay(GameScene* preview, TitleRift* rift, SrvManager* srv, Object3dCommon* common, float titleElapsed);
    bool CaptureEntry(const Camera& source, Camera& target, float titleElapsed, float flybyClock);
    void UpdateCamera(Camera& camera, float aspectRatio) const;

    bool InputReady() const;
    bool CanConfirm() const;
    void EnableInputNow();
    void BeginDeparture();
    void AdvanceClock(float delta) { stageSelectionElapsed_ += delta; }
    // 機体を入口へ戻している間はtrue　戻り切った次のフレームから出撃へ進む
    bool AdvanceReturn(GameScene* preview, float delta);
    // 機体が入口を通過する時刻をまたいだフレームだけtrue
    bool AdvanceOverview(float delta);
    bool OverviewComplete() const;

    float Elapsed() const { return stageSelectionElapsed_; }
    float ReturnTime() const { return stageReturnTime_; }
    float OverviewTime() const { return stageOverviewTime_; }
    const Math::Vector2& Seam() const { return stageEntrySeam_; }

private:
    std::unique_ptr<Camera> stagePortalCamera_;
    std::array<std::unique_ptr<Object3d>, 4> stagePortalPanels_{};
    std::array<std::unique_ptr<Object3d>, 2> stagePortalCut_{};
    Math::Vector2 stageEntrySeam_{ 500.0f, -0.19f };
    bool stageEntryCaptured_ = false;
    float stagePortalCutIntensity_ = 0.0f;
    float stagePortalGap_ = 160.0f;
    float stageDepartureGap_ = 160.0f;
    float stageDepartureCutAge_ = 0.0f;
    float stageSelectionElapsed_ = 0.0f;
    float stageReturnTime_ = -1.0f;
    float stageOverviewTime_ = -1.0f;
    float stageLaunchRate_ = 1.0f;
#ifdef _DEBUG
    bool stageLaunchReview_ = false;
    bool stagePatrolReview_ = false;
    std::size_t stageLaunchReviewPoint_ = 0;
#endif
};
