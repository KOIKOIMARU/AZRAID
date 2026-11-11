#include "app/presentation/GameplayCamera.h"
#include "app/runtime/GameRuntimeMath.h"
#include "app/player/JustDodgeFeedback.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
using namespace GameRuntimeMath;
constexpr float kRailCameraCurveFrequency = 0.050f;
constexpr float kRailCameraDriftFrequency = 0.027f;
constexpr float kGameplayCameraBaseDistance = 16.10f;
constexpr float kGameplayCameraEdgeRollStart = 5.25f;
constexpr float kGameplayCameraEdgeRollRange = 3.65f;
constexpr float kGameplayCameraEdgeRollMax = 0.055f;
constexpr float kJustDodgeCameraClosePushIn = 1.65f;
constexpr float kJustDodgeCameraPlayerFollowX = 0.115f;
constexpr float kJustDodgeCameraPlayerFollowY = 0.105f;
constexpr float kJustDodgeCameraLowAngle = 0.18f;
constexpr float kJustDodgeCameraFovTighten = 0.065f;
}

// 通常追従と表示用のカメラを初期化し、加速計算の基準になる速度も記録する。
void GameplayCamera::Reset(float railSpeed)
{
    follow_ = display_ = Pose{};
    motion_ = {};
    motion_.previousSpeed = railSpeed;
    previousPlayer_ = {};
    clock_ = 0.0f;
    ClearShake();
}

// 演出中の自機位置と時刻を追従カメラに覚えさせ、通常飛行へ戻った直後の位置差をそろえる。
void GameplayCamera::SynchronizePlayer(Math::Vector3 position, float clockFrames)
{
    previousPlayer_ = position;
    clock_ = clockFrames;
}

// 出撃中の速度とブラーを引き継ぐ。指定があれば、通常追従のばねにも演出終了時の前進速度を渡す。
void GameplayCamera::SetCinematicMotion(float speed, float blur, bool seedSpring)
{
    motion_.previousSpeed = speed;
    motion_.blurStrength = blur;
    if (seedSpring) {
        motion_.translationVelocity = { 0.0f, 0.0f, speed };
        motion_.rotationVelocity = {};
        motion_.fovVelocity = 0.0f;
    }
}

// 揺れの強さとフレーム数を登録する。すでに強い揺れがある場合はその強さを保つ。
void GameplayCamera::AddShake(float power, int duration)
{
    if (power <= 0.0f) { return; }
    shakePower_ = (std::max)(shakePower_, power);
    shakeDuration_ = (std::max)(duration, 1);
    shakeFrames_ = (std::max)(shakeFrames_, shakeDuration_);
}

// 画面揺れの強さと残り時間をまとめて解除する。
void GameplayCamera::ClearShake()
{
    shakePower_ = 0.0f;
    shakeFrames_ = 0;
    shakeDuration_ = 1;
}

// 揺れの残り割合を返して残りフレームを1つ進める。揺れの終了後は0を返す。
float GameplayCamera::ConsumeShake()
{
    if (shakeFrames_ <= 0) { shakePower_ = 0.0f; return 0.0f; }
    const float rate = static_cast<float>(shakeFrames_) / static_cast<float>(shakeDuration_);
    --shakeFrames_;
    return rate;
}

// フィーバー発動、スロー解除、前進加速のうち最も強い値を、加速演出に使える0〜1で返す。
float GameplayCamera::BoostKick(float activationKick, float releaseKick) const
{
    return std::clamp((std::max)(activationKick, (std::max)(releaseKick * 0.55f,
        (std::max)(motion_.acceleration, 0.0f) * 1.8f)), 0.0f, 1.0f);
}

// 練習と本編でカメラの追従設定を選び分ける。
void GameplayCamera::Update(const Frame& frame, Mode mode)
{
    if (mode == Mode::Flight) { UpdateFlight(frame); }
    else { UpdateTutorial(frame); }
}

// 自機の動き、区間の傾き、加速・被弾の演出を合成し、ばね補間で追従の姿勢を求める。
void GameplayCamera::UpdateFlight(const Frame& frame)
{
    const float frameStep = std::clamp(frame.deltaSeconds * 60.0f, 0.1f, 3.0f);
    clock_ += frameStep;
    const auto position = frame.playerPosition;
    const float moveX = std::clamp(
        (position.x - previousPlayer_.x) / (0.205f * frameStep), -1.0f, 1.0f);
    const float moveY = std::clamp(
        (position.y - previousPlayer_.y) / (0.205f * frameStep), -1.0f, 1.0f);
    // レール移動と慣性を分離。高速時でもカメラが際限なく取り残されない。
    follow_.position.z += position.z - previousPlayer_.z;
    previousPlayer_ = position;
    auto& motion = motion_;
    const float worldScale = frame.worldScale;
    const float effectiveSpeed = frame.railSpeed * worldScale;
    const float force = std::clamp(
        (effectiveSpeed - motion.previousSpeed) * 18.0f / frameStep, -1.0f, 1.0f);
    motion.previousSpeed = effectiveSpeed;
    motion.acceleration = Lerp(motion.acceleration, force, 1.0f - std::pow(0.82f, frameStep));
    const bool isFlying = !frame.gameOver && !frame.gameClear;
    const float speed = frame.speedRate;
    const float fever = frame.fever;
    const float dodge = frame.dodging && isFlying ?
        static_cast<float>(frame.dodgeDirection) : 0.0f;
    const auto dodgePulse = JustDodgeFeedback::Evaluate(frame.justDodge);
    const float justDodgePush = dodgePulse.focus;
    const float burst = isFlying ? (std::max)(frame.gateBoost, BoostKick(frame.activationKick, frame.releaseKick)) : 0.0f;
    const float controlX = isFlying ? moveX : 0.0f;
    const float controlY = isFlying ? moveY : 0.0f;

    // ばね＋減衰で追従。一定周期の揺れではなく、操作の開始・反転・停止で余韻が生まれる。
    // 小分けに積分して低fps時も安定させる（固定回数、確保なし）。
    const auto spring = [frameStep](float& value, float& velocity, float target, float frequency, float damping) {
        const int steps = (std::max)(1, static_cast<int>(std::ceil(frameStep / 0.5f)));
        const float step = frameStep / static_cast<float>(steps);
        for (int index = 0; index < steps; ++index) {
            velocity += ((target - value) * frequency * frequency -
                velocity * (2.0f * damping * frequency)) * step;
            value += velocity * step;
        }
    };
    const Math::Vector3 targetPosition{
        position.x * 0.28f - controlX * 0.30f,
        1.80f + position.y * 0.32f - controlY * 0.18f - motion.acceleration * 0.20f,
        frame.railDistance - (kFlightDistance + speed * 0.65f + fever * 1.05f +
            motion.acceleration * 1.40f + burst * 0.60f + dodgePulse.release * 0.65f) + justDodgePush * 1.10f,
    };
    spring(follow_.position.x, motion.translationVelocity.x, targetPosition.x, 0.16f, 0.86f);
    spring(follow_.position.y, motion.translationVelocity.y, targetPosition.y, 0.15f, 0.84f);
    spring(follow_.position.z, motion.translationVelocity.z, targetPosition.z, 0.18f, 0.82f);
    const Math::Vector3 targetRotation{
        0.060f - position.y * 0.025f - controlY * 0.048f + motion.acceleration * 0.030f,
        -position.x * 0.006f + controlX * 0.012f + frame.yawBias * 0.35f,
        std::clamp(-controlX * 0.075f - dodge * 0.045f - position.x * 0.0025f +
            frame.rollBias * 0.55f, -0.15f, 0.15f),
    };
    spring(follow_.rotation.x, motion.rotationVelocity.x, targetRotation.x, 0.13f, 0.82f);
    spring(follow_.rotation.y, motion.rotationVelocity.y, targetRotation.y, 0.14f, 0.88f);
    spring(follow_.rotation.z, motion.rotationVelocity.z, targetRotation.z, 0.13f, 0.76f);
    const float targetFov = kFlightFov + speed * 0.035f + fever * 0.110f +
        burst * 0.050f + std::abs(dodge) * 0.020f - justDodgePush * 0.055f +
        dodgePulse.release * 0.045f + frame.fovBoost;
    spring(follow_.fov, motion.fovVelocity, targetFov, 0.16f, 0.88f);

    const float targetBlur = isFlying ? std::clamp(
        (speed * 0.22f + fever * 0.35f + burst * 0.48f + std::abs(dodge) * 0.20f +
            dodgePulse.release * 0.20f) * worldScale * (1.0f - dodgePulse.focus * 0.75f),
        0.0f, 0.85f) : 0.0f;
    motion.blurStrength = Lerp(motion.blurStrength, targetBlur,
        1.0f - std::pow(targetBlur > motion.blurStrength ? 0.70f : 0.88f, frameStep));

    const float shakeRate = ConsumeShake();
    const float impact = shakePower_ * shakeRate;
    // 微振動は最終表示だけへ加える。ばね状態に蓄積させず、通常飛行は1px前後に抑える。
    const float wind = isFlying ? (speed * 0.35f + fever * 0.45f + burst * 0.30f) * worldScale : 0.0f;
    Math::Vector3 displayPosition = follow_.position;
    Math::Vector3 displayRotation = follow_.rotation;
    // 短い加速の衝撃は表示だけへ加え、ばねや飛行位置に蓄積させない。
    displayPosition.z -= burst * 0.85f;
    displayPosition.x -= static_cast<float>(frame.dodgeDirection) * dodgePulse.contact * 0.085f;
    displayRotation.z += static_cast<float>(frame.dodgeDirection) * dodgePulse.contact * 0.012f;
    displayRotation.x += burst * 0.012f;
    displayPosition.x += std::sin(clock_ * 1.9f) * (impact + wind * 0.012f);
    displayPosition.y += std::cos(clock_ * 2.3f) * (impact + wind * 0.008f);
    displayRotation.z += std::sin(clock_ * 1.37f) * wind * 0.0009f;
    display_ = { displayPosition, displayRotation, follow_.fov + burst * 0.075f };
}

// 操作を確認しやすい練習用カメラに更新する。通常飛行とカメラの追従・画角の設定を切り替える。
void GameplayCamera::UpdateTutorial(const Frame& frame)
{
    const float frameStep =
        std::clamp(frame.deltaSeconds * 60.0f, 0.5f, 2.0f);
    clock_ += frameStep;

    const Math::Vector3 playerTranslate = frame.playerPosition;
    const Math::Vector3 playerVelocity = {
        playerTranslate.x - previousPlayer_.x,
        playerTranslate.y - previousPlayer_.y,
        0.0f,
    };
    previousPlayer_ = playerTranslate;

    const float shakeRate = ConsumeShake();

    const float bob = std::sin(clock_ * 0.045f) * 0.08f;
    const float railCurve =
        std::sin(frame.railDistance * kRailCameraCurveFrequency);
    const float railDrift =
        std::sin(frame.railDistance * kRailCameraDriftFrequency + 1.35f);
    const float railLift =
        std::sin(frame.railDistance * 0.034f + 0.45f) * 0.18f;
    const float railWideCurve =
        railCurve * 0.32f +
        railDrift * 0.10f +
        std::sin(frame.railDistance * 0.014f + 2.20f) * 0.14f;
    const float turnRate = std::clamp(railWideCurve, -0.65f, 0.65f);
    const float speedPulse =
        0.5f + 0.5f * std::sin(frame.railDistance * 0.021f + 0.80f);
    const float railSpeedRate = frame.speedRate;
    const float feverSpeedRate = frame.fever;
    const float feverAccelerationKick = frame.activationKick;
    const bool isPlayerDodging = frame.dodging;
    const float justDodgeCameraRate = frame.justDodge;
    const auto dodgePulse = JustDodgeFeedback::Evaluate(justDodgeCameraRate);
    const float justDodgeWhip = dodgePulse.focus;
    const float playerImpactCameraRate = frame.impact;
    const float edgeRollRate = std::clamp(
        (std::abs(playerTranslate.x) - kGameplayCameraEdgeRollStart) /
            kGameplayCameraEdgeRollRange,
        0.0f,
        1.0f);
    const float smoothEdgeRollRate =
        edgeRollRate * edgeRollRate * (3.0f - 2.0f * edgeRollRate);
    const float edgeDirection =
        playerTranslate.x > 0.0f ? 1.0f :
        playerTranslate.x < 0.0f ? -1.0f :
        0.0f;
    const float playerEdgeRoll =
        edgeDirection * smoothEdgeRollRate * kGameplayCameraEdgeRollMax *
        (1.0f - justDodgeWhip * 0.92f);
    const float dodgeLean =
        isPlayerDodging ?
        static_cast<float>(frame.dodgeDirection >= 0 ? 1 : -1) :
        0.0f;
    const float inputSpeed = std::clamp(
        std::abs(playerVelocity.x) * 4.2f +
            std::abs(playerVelocity.y) * 2.8f,
        0.0f,
        1.0f);
    const float shakeX = std::sin(clock_ * 1.9f) * shakePower_ * shakeRate;
    const float shakeY = std::cos(clock_ * 2.3f) * shakePower_ * shakeRate;
    const float feverWindBuffetX = feverSpeedRate * (
        std::sin(clock_ * 0.21f) * 0.040f +
        std::sin(clock_ * 0.53f + 1.20f) * 0.016f);
    const float feverWindBuffetY = feverSpeedRate * (
        std::sin(clock_ * 0.27f + 0.70f) * 0.024f +
        std::sin(clock_ * 0.61f) * 0.010f);
    const float resultZoom =
        frame.gameClear ? 0.9f :
        frame.gameOver ? -0.8f :
        0.0f;

    const Math::Vector3 targetTranslate = {
            playerTranslate.x * (0.08f + justDodgeWhip * kJustDodgeCameraPlayerFollowX) +
            playerVelocity.x * 0.55f +
            dodgeLean * 0.18f +
            dodgeLean * justDodgeWhip * 0.32f +
            railWideCurve * 0.22f +
            railDrift * 0.04f +
            feverWindBuffetX +
            shakeX,
        2.65f +
            playerTranslate.y * (0.04f + justDodgeWhip * kJustDodgeCameraPlayerFollowY) +
            playerVelocity.y * 0.38f +
            railLift +
            frame.liftBias +
            std::sin(frame.railDistance * 0.017f + 0.35f) * 0.16f +
            justDodgeWhip * (0.08f - kJustDodgeCameraLowAngle) +
            feverSpeedRate * -0.12f +
            bob +
            feverWindBuffetY +
            shakeY,
            frame.railDistance -
            (kGameplayCameraBaseDistance +
                speedPulse * 0.55f +
                railSpeedRate * 0.46f +
                feverSpeedRate * (1.15f + speedPulse * 0.25f) +
                feverAccelerationKick * 0.78f + frame.gateBoost * 1.40f +
                inputSpeed * 0.22f +
            std::abs(turnRate) * 0.18f) +
            resultZoom +
            justDodgeWhip * kJustDodgeCameraClosePushIn +
            -dodgePulse.release * 0.65f +
            playerImpactCameraRate * playerImpactCameraRate * 0.42f,
    };

    const float cameraTranslateRate =
        0.065f +
        justDodgeCameraRate * 0.105f +
        playerImpactCameraRate * 0.020f +
        feverSpeedRate * 0.035f +
        feverAccelerationKick * 0.025f;
    follow_.position = Lerp(follow_.position, targetTranslate, cameraTranslateRate);

    const Math::Vector3 targetRotate = {
        0.065f +
            playerTranslate.y * 0.003f +
            railLift * 0.005f +
            speedPulse * 0.004f +
            bob * 0.006f +
            feverWindBuffetY * 0.012f +
            shakeY * 0.008f,
        -playerTranslate.x * 0.0045f +
            railWideCurve * 0.018f +
            railDrift * 0.004f +
            frame.yawBias * 0.35f +
            playerVelocity.x * 0.006f,
        frame.rollBias * 0.55f -
            turnRate * 0.024f +
            playerEdgeRoll +
            playerImpactCameraRate * 0.010f +
            feverWindBuffetX * 0.020f +
            shakeX * 0.006f,
    };

    const float targetFov =
        kTutorialFov +
        speedPulse * 0.040f +
        railSpeedRate * 0.035f +
        std::abs(turnRate) * 0.020f +
        inputSpeed * 0.025f +
        (isPlayerDodging ? 0.024f : 0.0f) +
        playerImpactCameraRate * 0.030f -
        justDodgeWhip * kJustDodgeCameraFovTighten +
        dodgePulse.release * 0.045f +
        feverSpeedRate *
            (0.115f + std::sin(clock_ * 0.11f) * 0.012f) +
        feverAccelerationKick * 0.060f +
        frame.releaseKick * 0.065f + frame.gateBoost * 0.10f +
        frame.fovBoost +
        (frame.gameClear ? -0.035f : 0.0f) +
        (frame.gameOver ? 0.025f : 0.0f) +
        shakePower_ * shakeRate * 0.04f;

    follow_.rotation = Lerp(follow_.rotation, targetRotate, 0.075f + justDodgeCameraRate * 0.082f);
    follow_.fov = Lerp(
        follow_.fov,
        targetFov,
        0.060f + justDodgeCameraRate * 0.080f + feverSpeedRate * 0.040f);

    display_ = follow_;
}
