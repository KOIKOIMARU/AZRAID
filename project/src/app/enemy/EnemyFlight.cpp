// ===== EnemyFlight.cpp =====
// 敵の種類に応じて飛行軌道と機体の向きを計算する

#include "app/enemy/EnemyFlight.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.1415926535f;
constexpr float kStandardEnemyActionTimeScale = 0.66f;

float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float Lerp(float start, float end, float rate)
{
    return start + (end - start) * rate;
}

// 最初は速く、終点に近づくほどゆっくり進む補間率を返す
float EaseOutCubic(float value)
{
    const float t = Clamp01(value);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float SmoothStep(float value)
{
    const float t = Clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

float SignNonZero(float value)
{
    return value < 0.0f ? -1.0f : 1.0f;
}
} // namespace

// 初期位置、機種、進入方法を保存し、飛行と攻撃姿勢に使う時計を最初に戻す
void EnemyFlight::Reset(const Math::Vector3& position, EnemyBehavior behavior, EnemyEntryStyle entryStyle)
{
    *this = {};
    baseTranslate_ = position;
    behavior_ = behavior;
    entryStyle_ = entryStyle;
    phase_ = position.x * 0.75f + position.y * 1.35f;
}

// 指定された時間倍率で飛行の時計を進め、機種に合う現在の位置・回転・大きさを返す
EnemyFlight::Pose EnemyFlight::Advance(float railDistance, float timeScale, float telegraphRate, const EnemyFireControl::Cycle& fireControl)
{
    const float motionScale = std::clamp(timeScale, 0.05f, 1.0f);
    const float actionTimeScale = behavior_ == EnemyBehavior::Boss ?
        motionScale : motionScale * kStandardEnemyActionTimeScale;
    ageTimer_ += actionTimeScale;
    // 練習標的は入場後にその場へ残す　揺れの時計は練習中も進める
    if (trainingTarget_) { ageTimer_ = (std::min)(ageTimer_, 110.0f); }
    age_ = static_cast<int>(std::floor(ageTimer_));
    moveTimer_ += actionTimeScale;
    return UpdateFlight(railDistance, motionScale, telegraphRate, fireControl);
}

float EnemyFlight::FormationShotDelay() const
{
    return std::abs(baseTranslate_.x) * 9.0f;
}

// 機種を見て飛行パターンを選び、照準や当たり判定の基準になる姿勢を返す
EnemyFlight::Pose EnemyFlight::UpdateFlight(float railDistance, float motionScale, float telegraphRate, const EnemyFireControl::Cycle& fireControl)
{
    switch (behavior_) {
    case EnemyBehavior::Boss: return UpdateBossFlight(railDistance, motionScale, telegraphRate);
    case EnemyBehavior::Swoop: return UpdateSwoopFlight(railDistance);
    case EnemyBehavior::StrafeShooter: return UpdateStrafeShooterFlight(railDistance);
    case EnemyBehavior::DiveBomber: return UpdateDiveBomberFlight(railDistance);
    case EnemyBehavior::Crossfire: return UpdateCrossfireFlight(railDistance);
    case EnemyBehavior::Sniper: return UpdateSniperFlight(railDistance, fireControl);
    case EnemyBehavior::Shield: return UpdateShieldFlight(railDistance);
    case EnemyBehavior::Support: return UpdateSupportFlight(railDistance);
    case EnemyBehavior::Formation:
    default: return UpdateFormationFlight(railDistance);
    }
}

// 攻撃ごとに指定された位置へ補間し、機体の傾き、構え、発射後の反動を加える
EnemyFlight::Pose EnemyFlight::UpdateBossFlight(float railDistance, float motionScale, float telegraphRate)
{
    Pose pose;
    const float entryRate = EaseOutCubic(static_cast<float>(age_) / 96.0f);
    // 攻撃ごとの飛行先へ滑らかに移動　構え中にも旋回し、反撃時はゆっくり戻る
    const auto previous = bossFlightOffset_;
    const float follow = (bossRecoveryRate_ > 0.0f ? 0.028f : 0.070f) * motionScale;
    bossFlightOffset_.x = Lerp(previous.x, bossFlightTarget_.x, follow);
    bossFlightOffset_.y = Lerp(previous.y, bossFlightTarget_.y, follow);
    bossFlightOffset_.z = Lerp(previous.z, bossFlightTarget_.z, follow);
    const float lateral = (bossFlightOffset_.x - previous.x) / motionScale;
    pose.position.x = bossFlightOffset_.x * entryRate;
    pose.position.y =
        Lerp(baseTranslate_.y + 4.00f, baseTranslate_.y, entryRate) +
        bossFlightOffset_.y * entryRate + std::sin(moveTimer_ * 0.026f) * 0.16f;
    pose.position.z =
        railDistance +
        Lerp(86.0f, 36.0f, entryRate) + bossFlightOffset_.z * entryRate;
    pose.scale = Lerp(0.34f, 1.0f, entryRate);
    pose.opacity = Lerp(0.20f, 1.0f, entryRate);
    pose.rotation.x = std::clamp((bossFlightOffset_.z - previous.z) / motionScale * 0.45f, -0.16f, 0.16f);
    pose.rotation.y = kPi + std::clamp(lateral * 0.8f, -0.26f, 0.26f);
    pose.rotation.z = std::clamp(-lateral * 1.8f, -0.42f, 0.42f);
    if (bossRecoveryRate_ > 0.0f) {
        const float elapsed = 1.0f - Clamp01(bossRecoveryRate_);
        // 発射直後に位置を飛ばさず、短く押し戻されてから戻る
        const float recoil = std::sin(std::clamp(elapsed / 0.18f, 0.0f, 1.0f) * kPi);
        const float settle = std::sin(elapsed * kPi);
        pose.position.z += recoil * 2.0f - settle * 1.8f;
        pose.rotation.x += recoil * 0.14f + settle * 0.05f;
    } else {
        pose.rotation.x -= telegraphRate * 0.08f;
    }
    return pose;
}

// 横切る機体の進入、弧を描く接近、反対側への離脱を時刻から求める
EnemyFlight::Pose EnemyFlight::UpdateSwoopFlight(float railDistance)
{
    Pose pose;
    const float entryRate = EaseOutCubic(static_cast<float>(age_) / 40.0f);
    const float passRate = SmoothStep(static_cast<float>(age_ - 14) / 182.0f);
    const float side =
        entryStyle_ == EnemyEntryStyle::RightSweep ? -1.0f :
        entryStyle_ == EnemyEntryStyle::LeftSweep ? 1.0f :
        SignNonZero(baseTranslate_.x);
    const float curve = std::sin(passRate * kPi);
    const float laneX =
        side * Lerp(10.8f, -11.6f, passRate) -
        side * curve * 2.8f;
    const float laneY =
        baseTranslate_.y +
        curve * 3.2f -
        passRate * 1.55f;
    const float laneZ =
        Lerp(48.0f, 30.0f, curve) +
        passRate * 12.0f;
    const float exitRunRate = SmoothStep(static_cast<float>(age_ - 196) / 72.0f);
    pose.position.x =
        Lerp(side * 4.4f, laneX, entryRate);
    pose.position.y =
        Lerp(baseTranslate_.y + 2.5f, laneY, entryRate) +
        std::sin(moveTimer_ * 0.064f + phase_) * 0.20f;
    pose.position.z =
        railDistance +
        Lerp(70.0f, laneZ, entryRate);
    pose.position.x += -side * exitRunRate * 64.0f;
    pose.position.y += exitRunRate * 34.0f;
    pose.position.z += exitRunRate * 18.0f;
    pose.scale =
        Lerp(0.42f, 1.0f, entryRate) +
        std::sin(entryRate * kPi) * 0.05f;
    pose.opacity = Lerp(0.28f, 1.0f, entryRate);
    pose.rotation.y =
        kPi +
        side * Lerp(0.35f, -0.25f, passRate) -
        side * exitRunRate * 0.78f;
    pose.rotation.z = -side * (0.18f + curve * 0.40f + exitRunRate * 0.56f);
    if (exitRunRate >= 1.0f) {
        pose.escaped = true;
    }
    return pose;
}

// 横から入り、プレイヤー前方を横切る射撃機の軌道と傾きを計算する
EnemyFlight::Pose EnemyFlight::UpdateStrafeShooterFlight(float railDistance)
{
    Pose pose;
    const float attackRate = EaseOutCubic(static_cast<float>(age_) / 72.0f);
    const float holdRate = Clamp01(static_cast<float>(age_ - 72) / 148.0f);
    const float exitRate = SmoothStep(static_cast<float>(age_ - 220) / 96.0f);
    const float exitSide = SignNonZero(std::sin(phase_));
    const float entryArc =
        entryStyle_ == EnemyEntryStyle::PopShooter ?
        std::sin(attackRate * kPi) :
        0.0f;
    const float exitRunRate = SmoothStep(static_cast<float>(age_ - 316) / 72.0f);
    pose.position.x =
        Lerp(
            baseTranslate_.x * 2.2f + SignNonZero(baseTranslate_.x) * 2.2f,
            baseTranslate_.x,
            attackRate) +
        std::sin(moveTimer_ * 0.050f + phase_) * Lerp(1.7f, 4.4f, holdRate) +
        exitSide * exitRate * 11.0f;
    pose.position.y =
        Lerp(baseTranslate_.y + 3.3f, baseTranslate_.y * 0.30f, attackRate) +
        std::cos(moveTimer_ * 0.042f + phase_) * 0.42f +
        entryArc * 1.05f +
        exitRate * 4.8f;
    pose.position.z =
        railDistance +
        Lerp(68.0f, 28.0f, attackRate) +
        std::cos(moveTimer_ * 0.030f + phase_) * 1.3f +
        exitRate * 14.0f;
    pose.position.x += exitSide * exitRunRate * 56.0f;
    pose.position.y += exitRunRate * 30.0f;
    pose.position.z += exitRunRate * 18.0f;
    pose.scale =
        Lerp(0.44f, 1.0f, attackRate) +
        std::sin(attackRate * kPi) * 0.06f;
    pose.opacity = Lerp(0.24f, 1.0f, attackRate);
    pose.rotation.y = kPi - pose.position.x * 0.030f;
    pose.rotation.z =
        std::sin(moveTimer_ * 0.032f + phase_) * 0.18f -
        exitSide * (exitRate * 0.35f + exitRunRate * 0.55f);
    if (exitRunRate >= 1.0f) {
        pose.escaped = true;
    }
    return pose;
}

// 高い位置から接近して急降下する敵の軌道を計算し、離脱が終われば退場を返す
EnemyFlight::Pose EnemyFlight::UpdateDiveBomberFlight(float railDistance)
{
    Pose pose;
    const float entryRate = EaseOutCubic(static_cast<float>(age_) / 66.0f);
    const float attackRate = SmoothStep(static_cast<float>(age_ - 28) / 168.0f);
    const float exitRate = SmoothStep(static_cast<float>(age_ - 224) / 92.0f);
    const float exitRunRate = SmoothStep(static_cast<float>(age_ - 300) / 70.0f);
    const float side =
        entryStyle_ == EnemyEntryStyle::RightSweep ? -1.0f :
        entryStyle_ == EnemyEntryStyle::LeftSweep ? 1.0f :
        SignNonZero(baseTranslate_.x);
    const float attackArc = std::sin(attackRate * kPi);
    const float weave =
        std::sin(moveTimer_ * 0.060f + phase_) *
        Lerp(0.35f, 2.4f, entryRate);

    pose.position.x =
        Lerp(side * 7.8f, baseTranslate_.x, entryRate) +
        weave -
        side * attackArc * 2.9f -
        side * exitRate * 11.0f -
        side * exitRunRate * 56.0f;
    pose.position.y =
        Lerp(baseTranslate_.y + 6.2f, baseTranslate_.y, entryRate) -
        attackArc * 1.55f +
        std::cos(moveTimer_ * 0.044f + phase_) * 0.36f +
        exitRate * 4.4f +
        exitRunRate * 30.0f;
    pose.position.z =
        railDistance +
        Lerp(78.0f, 32.0f, entryRate) -
        attackArc * 6.0f +
        std::sin(moveTimer_ * 0.026f + phase_) * 1.1f +
        exitRate * 13.0f +
        exitRunRate * 22.0f;
    pose.scale =
        Lerp(0.38f, 1.0f, entryRate) +
        attackArc * 0.055f;
    pose.opacity = Lerp(0.20f, 1.0f, entryRate);
    pose.rotation.x =
        -attackArc * 0.32f +
        std::sin(moveTimer_ * 0.035f + phase_) * 0.06f;
    pose.rotation.y =
        kPi +
        side * Lerp(0.42f, -0.24f, attackRate) -
        side * exitRate * 0.44f;
    pose.rotation.z =
        -side * (0.18f + attackArc * 0.46f + exitRunRate * 0.56f);
    if (exitRunRate >= 1.0f) {
        pose.escaped = true;
    }
    return pose;
}

// 挟撃する機体を左右の進入位置から動かし、交差攻撃に合わせて向きを変える
EnemyFlight::Pose EnemyFlight::UpdateCrossfireFlight(float railDistance)
{
    Pose pose;
    const float side = SignNonZero(baseTranslate_.x);
    const float entryRate = EaseOutCubic(static_cast<float>(age_) / 62.0f);
    const float crossRate = SmoothStep(static_cast<float>(age_ - 94) / 118.0f);
    const float exitRate = SmoothStep(static_cast<float>(age_ - 204) / 78.0f);
    const float crossArc = std::sin(crossRate * kPi);

    pose.position.x =
        Lerp(side * 13.5f, side * 4.25f, entryRate) -
        side * crossRate * 13.8f -
        side * exitRate * 60.0f;
    pose.position.y =
        Lerp(baseTranslate_.y + 3.8f, baseTranslate_.y, entryRate) +
        crossArc * 1.35f +
        std::sin(moveTimer_ * 0.070f + phase_) * 0.22f +
        exitRate * 34.0f;
    pose.position.z =
        railDistance +
        Lerp(72.0f, 32.0f, entryRate) -
        crossArc * 3.8f +
        exitRate * 18.0f;
    pose.scale =
        Lerp(0.40f, 1.0f, entryRate) +
        std::sin(entryRate * kPi) * 0.05f;
    pose.opacity = Lerp(0.22f, 1.0f, entryRate);
    pose.rotation.x = -crossArc * 0.12f;
    pose.rotation.y =
        kPi - side * Lerp(0.34f, 0.62f, crossRate) -
        side * exitRate * 0.68f;
    pose.rotation.z =
        -side * (0.24f + crossArc * 0.44f + exitRate * 0.52f);
    if (exitRate >= 1.0f) {
        pose.escaped = true;
    }
    return pose;
}

// 狙撃機の進入と離脱を計算する　構えている間はレールからの相対位置を固定し、発射後に反動を加える
EnemyFlight::Pose EnemyFlight::UpdateSniperFlight(float railDistance, const EnemyFireControl::Cycle& fireControl)
{
    Pose pose;
    const float entryRate = EaseOutCubic(static_cast<float>(age_) / 72.0f);
    const float exitRate = SmoothStep(static_cast<float>(age_ - 304) / 86.0f);
    const float exitRunRate = SmoothStep(static_cast<float>(age_ - 390) / 72.0f);
    const float exitSide = SignNonZero(baseTranslate_.x);
    pose.position.x =
        Lerp(baseTranslate_.x * 1.9f, baseTranslate_.x, entryRate) +
        std::sin(moveTimer_ * 0.022f + phase_) * 0.62f +
        exitSide * exitRate * 8.0f +
        exitSide * exitRunRate * 58.0f;
    pose.position.y =
        Lerp(baseTranslate_.y + 4.6f, baseTranslate_.y + 1.15f, entryRate) +
        std::cos(moveTimer_ * 0.026f + phase_) * 0.30f +
        exitRate * 5.0f +
        exitRunRate * 30.0f;
    pose.position.z =
        railDistance +
        Lerp(82.0f, 42.0f, entryRate) +
        std::sin(moveTimer_ * 0.018f + phase_) * 1.1f +
        exitRunRate * 18.0f;
    pose.scale =
        Lerp(0.40f, 1.0f, entryRate) +
        std::sin(entryRate * kPi) * 0.05f;
    pose.opacity = Lerp(0.20f, 1.0f, entryRate);
    pose.rotation.y =
        kPi - pose.position.x * 0.016f - exitSide * exitRunRate * 0.72f;
    pose.rotation.z =
        std::sin(moveTimer_ * 0.020f + phase_) * 0.08f -
        exitSide * exitRunRate * 0.58f;
    // 構えると横揺れを止める　二射後は前へ沈み、照準を合わせ直せる隙を残す
    // レール相対座標なので高速移動・スロー・停止でも背景に置き去りにならない
    const float recovery = fireControl.RecoveryElapsed();
    const bool attackPose = age_ <= 304 &&
        (fireControl.IsBraced() || (recovery >= 0.0f && recovery < 90.0f));
    if (attackPose) {
        if (!sniperBraced_) {
            sniperBracePosition_ = { pose.position.x, pose.position.y, pose.position.z - railDistance };
            sniperBraced_ = true;
        }
        const float recoveryReturn = recovery >= 66.0f ? SmoothStep((recovery - 66.0f) / 24.0f) : 0.0f;
        const float returnRate = (std::max)(recoveryReturn, SmoothStep(static_cast<float>(age_ - 290) / 14.0f));
        const float settle = recovery >= 0.0f ? SmoothStep(recovery / 20.0f) : 0.0f;
        const float recoil = fireControl.ShotFlash();
        pose.position.x = Lerp(sniperBracePosition_.x, pose.position.x, returnRate);
        pose.position.y = Lerp(sniperBracePosition_.y - settle * 0.45f, pose.position.y, returnRate);
        pose.position.z = Lerp(railDistance + sniperBracePosition_.z + recoil * 1.1f - settle * 3.0f,
            pose.position.z, returnRate);
        pose.rotation.x += (-fireControl.ChargeRate() * 0.12f + recoil * 0.22f + settle * 0.15f) * (1.0f - returnRate);
        pose.rotation.z *= returnRate;
    } else {
        sniperBraced_ = false;
    }
    if (exitRunRate >= 1.0f) {
        pose.escaped = true;
    }
    return pose;
}

// シールド機の進入と前方での動きを計算し、一定時間後に離脱させる
EnemyFlight::Pose EnemyFlight::UpdateShieldFlight(float railDistance)
{
    Pose pose;
    const float entryRate = EaseOutCubic(static_cast<float>(age_) / 82.0f);
    const float exitRate = SmoothStep(static_cast<float>(age_ - 338) / 88.0f);
    const float exitRunRate = SmoothStep(static_cast<float>(age_ - 426) / 72.0f);
    const float exitSide = SignNonZero(baseTranslate_.x);
    pose.position.x =
        Lerp(baseTranslate_.x * 0.22f, baseTranslate_.x, entryRate) +
        std::sin(moveTimer_ * 0.026f + phase_) * 1.10f +
        exitSide * exitRate * 9.0f +
        exitSide * exitRunRate * 58.0f;
    pose.position.y =
        Lerp(baseTranslate_.y + 3.8f, baseTranslate_.y, entryRate) +
        std::cos(moveTimer_ * 0.024f + phase_) * 0.34f +
        exitRate * 5.0f +
        exitRunRate * 30.0f;
    pose.position.z =
        railDistance +
        Lerp(84.0f, 34.0f, entryRate) +
        std::sin(moveTimer_ * 0.016f + phase_) * 1.2f +
        exitRunRate * 18.0f;
    pose.scale =
        Lerp(0.36f, 1.0f, entryRate) +
        std::sin(entryRate * kPi) * 0.05f;
    pose.opacity = Lerp(0.18f, 1.0f, entryRate);
    pose.rotation.y =
        kPi - pose.position.x * 0.014f - exitSide * exitRunRate * 0.68f;
    pose.rotation.z =
        std::sin(moveTimer_ * 0.021f + phase_) * 0.07f -
        exitSide * exitRunRate * 0.54f;
    if (exitRunRate >= 1.0f) {
        pose.escaped = true;
    }
    return pose;
}

// 後方に留まる支援機の姿勢と、支援終了後の離脱を計算する
EnemyFlight::Pose EnemyFlight::UpdateSupportFlight(float railDistance)
{
    Pose pose;
    const float entryRate = EaseOutCubic(static_cast<float>(age_) / 76.0f);
    const float exitRate = SmoothStep(static_cast<float>(age_ - 324) / 84.0f);
    const float exitRunRate = SmoothStep(static_cast<float>(age_ - 408) / 72.0f);
    const float exitSide = SignNonZero(baseTranslate_.x);
    pose.position.x =
        Lerp(baseTranslate_.x * 0.18f, baseTranslate_.x, entryRate) +
        std::sin(moveTimer_ * 0.024f + phase_) * 0.72f +
        exitSide * exitRate * 8.0f +
        exitSide * exitRunRate * 56.0f;
    pose.position.y =
        Lerp(baseTranslate_.y + 4.2f, baseTranslate_.y + 1.8f, entryRate) +
        std::cos(moveTimer_ * 0.028f + phase_) * 0.34f +
        exitRate * 5.0f +
        exitRunRate * 30.0f;
    pose.position.z =
        railDistance +
        Lerp(82.0f, 38.0f, entryRate) +
        std::sin(moveTimer_ * 0.017f + phase_) * 1.0f +
        exitRunRate * 18.0f;
    pose.scale =
        Lerp(0.38f, 1.0f, entryRate) +
        std::sin(moveTimer_ * 0.11f + phase_) * 0.035f;
    pose.opacity = Lerp(0.20f, 1.0f, entryRate);
    pose.rotation.y =
        kPi - pose.position.x * 0.014f - exitSide * exitRunRate * 0.64f;
    pose.rotation.z =
        std::sin(moveTimer_ * 0.026f + phase_) * 0.10f -
        exitSide * exitRunRate * 0.52f;
    if (exitRunRate >= 1.0f) {
        pose.escaped = true;
    }
    return pose;
}

// 編隊の並び方ごとに進入位置と揺れを求め、時間が過ぎたら離脱させる
EnemyFlight::Pose EnemyFlight::UpdateFormationFlight(float railDistance)
{
    Pose pose;
    const float entryRate = EaseOutCubic(static_cast<float>(age_) / 58.0f);
    const float holdRate = Clamp01(static_cast<float>(age_ - 58) / 132.0f);
    const float exitRate = SmoothStep(static_cast<float>(age_ - 206) / 96.0f);
    const float exitSide = SignNonZero(baseTranslate_.x);
    const float formationSpread =
        entryStyle_ == EnemyEntryStyle::VFormation ?
        std::sin(entryRate * kPi) * std::abs(baseTranslate_.x) * 0.56f :
        0.0f;
    const float exitRunRate = SmoothStep(static_cast<float>(age_ - 302) / 74.0f);
    pose.position.x =
        Lerp(baseTranslate_.x * 0.08f, baseTranslate_.x, entryRate) +
        SignNonZero(baseTranslate_.x) * formationSpread +
        std::sin(moveTimer_ * 0.040f + phase_) * Lerp(0.20f, 0.95f, entryRate) +
        std::sin(moveTimer_ * 0.014f + phase_) * 0.42f +
        exitSide * exitRate * 11.5f;
    pose.position.y =
        Lerp(baseTranslate_.y + 2.1f, baseTranslate_.y, entryRate) +
        std::cos(moveTimer_ * 0.034f + phase_) * 0.52f +
        exitRate * 4.1f;
    pose.position.z =
        railDistance +
        Lerp(70.0f, 30.0f, entryRate) -
        holdRate * 0.8f +
        std::sin(moveTimer_ * 0.022f + phase_) * 1.5f;
    pose.position.x += exitSide * exitRunRate * 56.0f;
    pose.position.y += exitRunRate * 30.0f;
    pose.position.z += exitRunRate * 16.0f;
    if (entryStyle_ == EnemyEntryStyle::TightFormation) {
        // 各機固有の位相を使わず、ひとかたまりで入場する　照準を振って順に倒せる間隔
        pose.position.x = baseTranslate_.x * Lerp(0.75f, 1.0f, entryRate) +
            std::sin(moveTimer_ * 0.032f) * 0.38f +
            exitSide * (exitRate * 11.5f + exitRunRate * 56.0f);
        pose.position.y = Lerp(baseTranslate_.y + 2.1f, baseTranslate_.y, entryRate) +
            std::cos(moveTimer_ * 0.032f) * 0.24f + exitRate * 4.1f + exitRunRate * 30.0f;
        pose.position.z = railDistance + Lerp(70.0f, 30.0f, entryRate) - holdRate * 0.8f + exitRunRate * 16.0f;
    }
    pose.scale =
        Lerp(0.42f, 1.0f, entryRate) +
        std::sin(entryRate * kPi) * 0.055f;
    pose.opacity = Lerp(0.26f, 1.0f, entryRate);
    pose.rotation.y = kPi - pose.position.x * 0.018f;
    pose.rotation.z =
        -exitSide * std::sin(entryRate * kPi) * 0.22f +
        std::sin(moveTimer_ * 0.025f + phase_) * 0.10f -
        exitSide * exitRunRate * 0.56f;
    if (exitRunRate >= 1.0f) {
        pose.escaped = true;
    }
    return pose;
}
