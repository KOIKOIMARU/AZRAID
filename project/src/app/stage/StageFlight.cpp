// ===== StageFlight.cpp =====
// 区間ごとの速度や加速に従ってレール上の進行距離を更新し、ステージ進行に反映する。

#include "app/stage/StageFlight.h"

#include "app/runtime/GameRuntimeMath.h"
#include "app/runtime/GameRuntimeTuning.h"

#include <algorithm>
#include <array>

namespace {
using GameRuntimeMath::Lerp;
constexpr float kTutorialCruiseSpeed = 0.145f;
constexpr float kMainFlightSpeedMultiplier = 3.30f;
constexpr float kFeverSpeedMultiplier = 2.15f;
constexpr float kFeverAccelerationResponse = 0.15f;

float InitialSpeed(bool tutorial)
{
    return tutorial ? kTutorialCruiseSpeed : 0.160f * kMainFlightSpeedMultiplier;
}
}

// レールの距離、速度、区間名を初期状態へ戻す。練習と本編で開始速度を切り替える。
void StageFlight::Reset(bool tutorial)
{
    director_.Reset(tutorial);
    distance_ = 0.0f;
    speed_ = targetSpeed_ = InitialSpeed(tutorial);
    previousWorldScale_ = 1.0f;
    releaseKick_ = 0.0f;
    cameraBias_ = {};
    waveIndex_ = 0;
    sectionName_ = "Opening";
    combatBeat_ = "Intro";
}

// 現在区間から目標速度とカメラの傾きを求める。リングとフィーバーの加速もここで合成する。
void StageFlight::PrepareFrame(bool feverActive, float ringBoost)
{
    const float gateSpeed = 1.0f + ringBoost * (feverActive ? 0.22f : 0.65f);
    const float feverSpeed = feverActive ? kFeverSpeedMultiplier : 1.0f;
    if (director_.IsTutorial()) {
        targetSpeed_ = kTutorialCruiseSpeed * feverSpeed * gateSpeed;
        sectionName_ = "Tutorial";
        return;
    }

    const auto& segment = director_.CurrentSegment();
    sectionName_ = segment.name;
    waveIndex_ = director_.WaveIndex();
    targetSpeed_ = segment.targetSpeed * kMainFlightSpeedMultiplier * feverSpeed * gateSpeed;
    cameraBias_.yaw = Lerp(cameraBias_.yaw, segment.yawBias, 0.035f);
    cameraBias_.roll = Lerp(cameraBias_.roll, segment.rollBias, 0.035f);
    cameraBias_.lift = Lerp(cameraBias_.lift, segment.liftBias, 0.035f);
    cameraBias_.fov = Lerp(cameraBias_.fov, segment.fovBoost, 0.035f);
}

// 目標速度に近づけてレール距離を進める。スロー解除時の加速演出と敵配置の進行も更新する。
void StageFlight::Advance(float worldTimeScale, StageDirector::EnemyLoad load, bool feverActive, float ringBoost, bool bossSpawned)
{
    speed_ = Lerp(speed_, targetSpeed_, ringBoost > 0.0f ? 0.20f : feverActive ? kFeverAccelerationResponse : 0.070f);
    releaseKick_ = (std::max)(0.0f, releaseKick_ - 1.0f / 28.0f);
    if (!director_.IsTutorial() && worldTimeScale > previousWorldScale_ + 0.25f) {
        releaseKick_ = 1.0f;
    }
    previousWorldScale_ = worldTimeScale;
    distance_ += speed_ * worldTimeScale;

    // 背景は練習中も流す。配置の進行は敵の出現枠やボスの状態で別に決まる。
    director_.Advance(worldTimeScale, load, feverActive, bossSpawned);
}

// 配置表からリングを取り出し、自機の前方に置く。高速時は前方距離を広げ、接近までの時間を確保する。
std::optional<Math::Vector3> StageFlight::TryConsumeGatePosition()
{
    const auto* gate = director_.TryConsumeGate();
    if (!gate) { return std::nullopt; }

    const float lead = (std::max)(gate->leadDistance, speed_ * 105.0f);
    return Math::Vector3{ gate->x, gate->y, distance_ + lead };
}

// 本編でボス警告を始める進行度に達したか返す。警告を一度に絞る処理は演出側で行う。
bool StageFlight::IsBossWarningDue() const
{
    return !director_.IsTutorial() && director_.Progress() >= MainStage::kBossWarningProgress;
}

void StageFlight::FinishStage()
{
    waveIndex_ = GameRuntimeTuning::kWaveCount - 1;
    combatBeat_ = "Boss destroyed";
}

// 現在の飛行速度を、モードごとの基準から0〜1の演出用の値に換算する。
float StageFlight::SpeedRate() const
{
    const bool tutorial = director_.IsTutorial();
    const float base = tutorial ? 0.13f : 0.30f;
    const float range = tutorial ? 0.10f : 0.60f;
    return std::clamp((speed_ - base) / range, 0.0f, 1.0f);
}

// 目標速度と現在速度の差から、加速演出で使う0〜1の値を求める。
float StageFlight::AccelerationRate() const
{
    return std::clamp((targetSpeed_ - speed_) / 0.045f, 0.0f, 1.0f);
}

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
// 開発用に区間を選び、レールと配置の進行度を確認したい地点へ移す。
StageFlight::DebugPhase StageFlight::SeekPhaseForDebug(int phaseIndex)
{
    struct Phase {
        float progress;
        bool startsAtBoss;
        const char* name;
    };
    constexpr std::array phases{
        Phase{ 0.0f, false, "Wave 1" },
        Phase{ 104.0f, false, "Wave 2" },
        Phase{ 230.0f, false, "Wave 3" },
        Phase{ MainStage::kBossSpawnProgress, true, "Boss" },
        Phase{ 78.0f, false, "Sniper wing" }
    };
    const auto& phase = phases[std::clamp(phaseIndex, 0, static_cast<int>(phases.size()) - 1)];
    const int skippedEnemies = director_.SeekForDebug(phase.progress, phase.startsAtBoss);
    waveIndex_ = director_.WaveIndex();
    combatBeat_ = phase.startsAtBoss ? "Boss" : "Debug phase jump";
    return { skippedEnemies, phase.startsAtBoss, phase.name };
}

void StageFlight::ResetSpeedForDebug()
{
    speed_ = InitialSpeed(director_.IsTutorial());
    targetSpeed_ = kTutorialCruiseSpeed;
}
#endif

#ifdef _DEBUG
void StageFlight::ResetScheduleForProbe(float progress)
{
    director_.Reset();
    director_.SeekForDebug(progress);
}
#endif
