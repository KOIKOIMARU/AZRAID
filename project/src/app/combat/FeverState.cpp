#include "app/combat/FeverState.h"

#include <algorithm>

// ゲージ、発動時間、発動回数、加速演出をまとめて初期状態に戻す。
void FeverState::Reset()
{
    gauge_ = 0;
    remainingFrames_ = 0;
    activationFlashFrames_ = 0;
    activationCount_ = 0;
    speedEffectRate_ = 0.0f;
}

// 発動中の残り時間を進め、時間切れで通常状態へ戻す。
bool FeverState::Tick()
{
    const bool wasActive = IsActive();
    const float target = wasActive ? 1.0f : 0.0f;
    const float response = target > speedEffectRate_ ? 0.16f : 0.045f;
    speedEffectRate_ += (target - speedEffectRate_) * response;
    if (!wasActive && speedEffectRate_ < 0.001f) { speedEffectRate_ = 0.0f; }
    if (activationFlashFrames_ > 0) { --activationFlashFrames_; }
    if (wasActive) { --remainingFrames_; }
    return wasActive;
}

// ゲージを消費し、フィーバーの残り時間と発動時の光をセットする。発動条件は呼び出し側が判定する。
void FeverState::Activate()
{
    gauge_ = 0;
    remainingFrames_ = kDurationFrames;
    activationFlashFrames_ = kActivationFlashFrames;
    ++activationCount_;
}

// フィーバーの効果と発動表示を終える。指定された場合は加速演出の残りも消す。
void FeverState::Stop(bool resetSpeedEffect)
{
    remainingFrames_ = 0;
    activationFlashFrames_ = 0;
    if (resetSpeedEffect) { speedEffectRate_ = 0.0f; }
}

void FeverState::SetGauge(int amount)
{
    gauge_ = std::clamp(amount, 0, kGaugeMax);
}

// 通常状態で正の加算を受け付け、ゲージを上限まで増やす。
void FeverState::AddGauge(int amount)
{
    if (amount > 0 && !IsActive()) { SetGauge(gauge_ + amount); }
}

// 通常状態の被弾などでゲージを減らし、0未満にならないようにする。
void FeverState::RemoveGauge(int amount)
{
    if (amount > 0 && !IsActive()) { SetGauge(gauge_ - amount); }
}

// 発動中の残り時間を通常の最大時間まで伸ばし、実際に増えたフレーム数を返す。
int FeverState::Extend(int frames)
{
    if (!IsActive() || frames <= 0) { return 0; }
    const int previous = remainingFrames_;
    const int room = (std::max)(0, kDurationFrames - remainingFrames_);
    remainingFrames_ += (std::min)(frames, room);
    return remainingFrames_ - previous;
}
