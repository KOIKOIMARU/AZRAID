// 出撃と再挑戦の演出で、機体・カメラ・目標表示の姿勢や時刻を計算する

#pragma once

#include "app/presentation/GameplayCamera.h"

#include <array>
#include <cstddef>

// 出撃・再挑戦の飛行姿勢とHUDの時刻を計算する　本編の機体やカメラは呼び出し側が所有する
class StageEntryPresentation {
public:
    struct FlightInput {
        float railDistance;
        float railSpeed;
        float speedRate;
        bool tutorial;
    };
    struct ShipFrame {
        Math::Vector3 position;
        Math::Vector3 rotation;
        float thrust;
    };
    struct FlightFrame {
        ShipFrame ship;
        GameplayCamera::Pose camera;
        float railDistance;
        float blurStrength;
        float clockFrames;
    };

    void Reset();
    static Math::Vector3 GetStagePatrolPosition(float elapsed, Math::Vector2 seam);
    void SetPortalSeam(Math::Vector2 seam) { stagePortalSeam_ = seam; }
    const Math::Matrix4x4& PortalFrame() const { return stagePortalFrame_; }
    GameplayCamera::Pose GetPreviewCamera(float railDistance) const;
    ShipFrame UpdatePreview(float elapsed, const Math::Matrix4x4& cameraFrame);
    ShipFrame UpdateReturn(float elapsed, const Math::Vector3& shipPosition);
    float ReturnDuration() const { return stageReturnDuration_; }
    FlightFrame UpdateOverview(float elapsed, const FlightInput& input);
    void BeginTitle(const Math::Vector3& viewPosition, const Math::Vector3& rotation,
        const Math::Vector3& cameraRotation, float titleScale, float shipScale, const FlightInput& input);
    FlightFrame UpdateTitle(float time, const FlightInput& input);
    void BeginRetry(const Math::Vector3& shipPosition, float railDistance);
    FlightFrame UpdateRetry(float deltaSeconds, const FlightInput& input);
    void FinishRetryFrame();
    void FinishTitleEntry();
    bool IsOverviewActive() const { return stageOverviewActive_; }
    bool IsTitleActive() const { return titleEntryTime_ >= 0.0f; }
    bool IsRetryActive() const { return retryEntryTime_ >= 0.0f; }
    bool IsForeground() const;

    void AdvanceBriefing(float deltaSeconds);
    void HideBriefing() { stageBriefingTime_ = -1.0f; }
    float BriefingTime() const { return stageBriefingTime_; }
    void DrawBriefing(float elapsed, const Math::Vector2& min, const Math::Vector2& size) const;
    float TitleHudOpacity() const;
    float RetryHudOpacity() const;

#ifdef _DEBUG
    bool HasStageDepartureForProbe() const { return IsStageCinematic() && !stageOverviewActive_; }
    float RetryTimeForProbe() const { return retryEntryTime_; }
    float RetryStartRailForProbe() const { return retryEntryStartRail_; }
#endif

private:
    bool IsStageCinematic() const { return titleEntryFromStage_ && stageReturnTime_ >= 0.0f; }
    float BriefingEnd() const;

    float titleEntryTime_ = -1.0f;
    bool titleEntryFromStage_ = false;
    bool stageOverviewActive_ = false;
    float stageReturnTime_ = -1.0f;
    float stageReturnDuration_ = 0.90f;
    float stagePreviewBank_ = 0.0f;
    float stageReturnBank_ = 0.0f;
    struct StageReturnLeg {
        std::array<Math::Vector3,4> curve{};
        std::array<float,65> arcLength{};
        float duration = 0.0f, startSpeed = 0.0f, cruiseSpeed = 0.0f, endSpeed = 0.0f;
    };
    std::array<StageReturnLeg,3> stageReturnLegs_{};
    size_t stageReturnLegCount_ = 0;
    Math::Matrix4x4 stagePortalFrame_{};
    Math::Vector2 stagePortalSeam_{500.0f,-0.19f};
    Math::Vector3 stagePreviewVelocity_{};
    float stageBriefingTime_ = -1.0f;
    float retryEntryTime_ = -1.0f;
    float retryEntryStartRail_ = 0.0f;
    Math::Vector3 retryEntryOrigin_{};
    float titleEntryStartRail_ = 0.0f;
    Math::Vector3 titleEntryOrigin_{};
    Math::Vector3 titleEntryRotation_{};
    Math::Vector3 titleEntryCameraRotation_{ 0.02f, 0.0f, -0.10f };
};
