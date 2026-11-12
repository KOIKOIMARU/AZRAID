#pragma once

#include "app/stage/StageDirector.h"
#include "engine/base/Math.h"

#include <optional>

// レール上の移動と、敵配置に使うステージ進行を管理する
class StageFlight {
public:
    struct CameraBias {
        float yaw = 0.0f;
        float roll = 0.0f;
        float lift = 0.0f;
        float fov = 0.0f;
    };

    void Reset(bool tutorial);
    // 今いる区間の速度とカメラ設定を反映する　区間イベントの消化はAdvanceの前に行う
    void PrepareFrame(bool feverActive, float ringBoost);
    void Advance(float worldTimeScale, StageDirector::EnemyLoad load, bool feverActive, float ringBoost, bool bossSpawned);
    // 出撃・再挑戦の間は演出側の位置だけを引き継ぐ
    void SetEntryDistance(float distance) { distance_ = distance; }

    const MainStage::RailEvent* TryConsumeRailEvent() { return director_.TryConsumeRailEvent(); }
    std::optional<Math::Vector3> TryConsumeGatePosition();
    const MainStage::EnemySpawn* TryConsumeEnemy(StageDirector::EnemyLoad load) { return director_.TryConsumeEnemy(load); }
    // 本編でボス警告を始める進行度に達したか返す　警告を一度に絞る処理は演出側で行う
    bool IsBossWarningDue() const;
    void FinishStage();
    void SetCombatBeat(const char* name) { combatBeat_ = name; }

    const StageDirector& Schedule() const { return director_; }
    float Distance() const { return distance_; }
    float Speed() const { return speed_; }
    float SpeedRate() const;
    float AccelerationRate() const;
    float ReleaseKick() const { return releaseKick_; }
    const CameraBias& Bias() const { return cameraBias_; }
    int WaveIndex() const { return waveIndex_; }
    const char* SectionName() const { return sectionName_; }
    const char* CombatBeat() const { return combatBeat_; }

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
    struct DebugPhase {
        int skippedEnemies;
        bool startsAtBoss;
        const char* name;
    };
    DebugPhase SeekPhaseForDebug(int phaseIndex);
    void ResetSpeedForDebug();
    void SetSpeedForDebug(float speed) { speed_ = speed; }
#endif

#ifdef _DEBUG
    float TargetSpeedForProbe() const { return targetSpeed_; }
    float PreviousWorldScaleForProbe() const { return previousWorldScale_; }
    void HoldProgressForProbe(float progress) { director_.HoldForDebug(progress); }
    void DisableSpawnsForProbe() { director_.DisableSpawnsForDebug(); }
    void ResetScheduleForProbe(float progress);
#endif

private:
    StageDirector director_; // 敵やイベントを出すタイミングを決める処理
    float distance_ = 0.0f; // 次の敵の出現を待つ間も進む、自機のレール上の距離
    float speed_ = 0.115f;
    float targetSpeed_ = 0.115f;
    float previousWorldScale_ = 1.0f;
    float releaseKick_ = 0.0f;
    CameraBias cameraBias_{};
    int waveIndex_ = 0;
    const char* sectionName_ = "Opening";
    const char* combatBeat_ = "Intro";
};
