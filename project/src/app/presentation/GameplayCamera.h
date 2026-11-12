#pragma once

#include "engine/base/Math.h"

// ゲーム中の追従・画角・揺れの計算　Cameraへの反映、入力、戦闘の判断は呼び出し側で行う
class GameplayCamera {
public:
    static constexpr float kFlightDistance = 13.35f;
    static constexpr float kFlightFov = 0.800f;
    static constexpr float kTutorialFov = 0.590f;
    enum class Mode { Flight, Tutorial };
    struct Pose {
        Math::Vector3 position{ 0.0f, 2.65f, -15.8f };
        Math::Vector3 rotation{ 0.18f, 0.0f, 0.0f };
        float fov = kTutorialFov; // ラジアン
    };
    struct Frame {
        Math::Vector3 playerPosition{};
        float deltaSeconds = 1.0f / 60.0f;
        float railDistance = 0.0f, railSpeed = 0.0f, speedRate = 0.0f, worldScale = 1.0f;
        float fever = 0.0f, activationKick = 0.0f, releaseKick = 0.0f;
        float justDodge = 0.0f, impact = 0.0f;
        float gateBoost = 0.0f;
        float yawBias = 0.0f, rollBias = 0.0f, liftBias = 0.0f, fovBoost = 0.0f;
        int dodgeDirection = 1;
        bool dodging = false, gameOver = false, gameClear = false;
    };
    struct Motion {
        Math::Vector3 translationVelocity{}, rotationVelocity{};
        float fovVelocity = 0.0f;
        float acceleration = 0.0f; // スローも含めた前進速度の変化（-1〜1）
        float previousSpeed = 0.235f;
        float blurStrength = 0.0f;
    };

    void Reset(float railSpeed);
    void Update(const Frame& frame, Mode mode);
    // 揺れの強さとフレーム数を登録する　すでに強い揺れがある場合はその強さを保つ
    void AddShake(float power, int duration);
    void ClearShake();

    // 出撃・死亡・エディターで求めた姿勢を受け取る　ばねの速度や時計を勝手に初期化しない
    void AdoptPose(const Pose& pose) { follow_ = display_ = pose; }
    void SynchronizePlayer(Math::Vector3 position, float clockFrames);
    void SetCinematicMotion(float speed, float blur, bool seedSpring = false);
    void SuppressBlur() { motion_.blurStrength = 0.0f; }

    const Pose& FollowPose() const { return follow_; } // 本編では微振動を加える前の追従姿勢
    const Pose& DisplayPose() const { return display_; }
    const Motion& MotionState() const { return motion_; }
    float Clock() const { return clock_; } // 60fps換算　演出とHUDでも同じ時計を読む
    int ShakeFrames() const { return shakeFrames_; }
    float BoostKick(float activationKick, float releaseKick) const;

private:
    void UpdateFlight(const Frame& frame);
    void UpdateTutorial(const Frame& frame);
    float ConsumeShake();

    Pose follow_{}, display_{};
    Motion motion_{};
    Math::Vector3 previousPlayer_{};
    float clock_ = 0.0f;
    float shakePower_ = 0.0f;
    int shakeFrames_ = 0, shakeDuration_ = 1; // shakeFrames_は揺れフレーム数　shakeDuration_は揺れ継続時間
};
