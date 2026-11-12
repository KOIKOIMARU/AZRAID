#pragma once

#include "app/enemy/EnemyFireControl.h"
#include "app/enemy/EnemyTypes.h"
#include "engine/base/Math.h"

// 進入・攻撃中・離脱の飛行経路を計算する　機体のHPや描画オブジェクトはEnemyが持つ
class EnemyFlight {
public:
    struct Pose {
        Math::Vector3 position{};
        Math::Vector3 rotation{};
        float scale = 1.0f;
        float opacity = 1.0f;
        bool escaped = false;
    };

    void Reset(const Math::Vector3& position, EnemyBehavior behavior, EnemyEntryStyle entryStyle);
    Pose Advance(float railDistance, float timeScale, float telegraphRate, const EnemyFireControl::Cycle& fireControl);
    EnemyBehavior Behavior() const { return behavior_; }
    EnemyEntryStyle EntryStyle() const { return entryStyle_; }
    int Age() const { return age_; }
    float MotionClock() const { return moveTimer_; }
    float Phase() const { return phase_; }
    float FormationShotDelay() const;
    void SetTrainingTarget(bool enabled) { trainingTarget_ = enabled; }
    void SetBossRecoveryRate(float rate) { bossRecoveryRate_ = rate; }
    void SetBossTarget(const Math::Vector3& offset) { bossFlightTarget_ = offset; }

private:
    Pose UpdateFlight(float railDistance, float motionScale, float telegraphRate, const EnemyFireControl::Cycle& fireControl);
    Pose UpdateBossFlight(float railDistance, float motionScale, float telegraphRate);
    Pose UpdateSwoopFlight(float railDistance);
    Pose UpdateStrafeShooterFlight(float railDistance);
    Pose UpdateDiveBomberFlight(float railDistance);
    Pose UpdateCrossfireFlight(float railDistance);
    Pose UpdateSniperFlight(float railDistance, const EnemyFireControl::Cycle& fireControl);
    Pose UpdateShieldFlight(float railDistance);
    Pose UpdateSupportFlight(float railDistance);
    Pose UpdateFormationFlight(float railDistance);

    EnemyBehavior behavior_ = EnemyBehavior::Formation;
    EnemyEntryStyle entryStyle_ = EnemyEntryStyle::Direct;
    Math::Vector3 baseTranslate_{};
    float ageTimer_ = 0.0f;
    int age_ = 0;
    float moveTimer_ = 0.0f;
    float phase_ = 0.0f;
    float bossRecoveryRate_ = 0.0f;
    Math::Vector3 bossFlightTarget_{};
    Math::Vector3 bossFlightOffset_{};
    Math::Vector3 sniperBracePosition_{}; // zは構え始めに記録したレールからの距離
    bool sniperBraced_ = false;
    bool trainingTarget_ = false;
};
