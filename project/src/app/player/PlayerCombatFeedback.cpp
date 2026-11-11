#include "app/player/PlayerCombatFeedback.h"

#include "app/player/JustDodgeFeedback.h"

#include <algorithm>

// ジャスト回避が成立したフレームから、時間の減速と画面表示の時計を始める。
void PlayerCombatFeedback::StartJustDodge()
{
    dodgeFlashFrames_ = JustDodgeFeedback::kDuration;
    dodgeSlowFrames_ = JustDodgeFeedback::kSlowDuration;
}

// 命中の種類から演出の強さを選び、スローと画面揺れに使う値を決める。
PlayerCombatFeedback::CameraShake PlayerCombatFeedback::StartImpact(bool charged, bool bossHit, bool destroyed)
{
    int flashDuration = 4;
    CameraShake shake{};
    if (bossHit && destroyed) {
        flashDuration = 28;
        shake = { 0.18f, 24 };
        impactSlowFrames_ = 10;
    } else if (bossHit) {
        flashDuration = 15;
        shake = { 0.035f, 4 };
    } else if (destroyed) {
        flashDuration = charged ? 13 : 7;
        shake = charged ? CameraShake{ 0.055f, 6 } : CameraShake{ 0.035f, 4 };
    } else if (charged) {
        flashDuration = 10;
        shake = { 0.018f, 3 };
    }
    if (flashDuration >= impactFlashFrames_) {
        impactFlashFrames_ = flashDuration;
        impactFlashDuration_ = flashDuration;
    }
    return shake;
}

// 回避や命中で使う演出の残りフレームを進める。
void PlayerCombatFeedback::Tick()
{
    dodgeFlashFrames_ = (std::max)(dodgeFlashFrames_ - 1, 0);
    dodgeSlowFrames_ = (std::max)(dodgeSlowFrames_ - 1, 0);
    impactFlashFrames_ = (std::max)(impactFlashFrames_ - 1, 0);
    impactSlowFrames_ = (std::max)(impactSlowFrames_ - 1, 0);
}

// 命中による一時的なスローと画面の光を消し、次の場面に持ち越さないようにする。
void PlayerCombatFeedback::ClearImpact()
{
    impactFlashFrames_ = 0;
    impactFlashDuration_ = 1;
    impactSlowFrames_ = 0;
}

float PlayerCombatFeedback::DodgeRate() const
{
    return static_cast<float>(dodgeFlashFrames_) / JustDodgeFeedback::kDuration;
}

float PlayerCombatFeedback::DodgeRelease() const
{
    return JustDodgeFeedback::Evaluate(DodgeRate()).release;
}

float PlayerCombatFeedback::ImpactRate() const
{
    return static_cast<float>(impactFlashFrames_) / impactFlashDuration_;
}

// 回避・命中・スキルの状態から、敵や弾に適用する時間倍率を求める。
float PlayerCombatFeedback::WorldTimeScale(float skillScale) const
{
    if (IsDodgeSlowActive()) {
        skillScale = (std::min)(skillScale, JustDodgeFeedback::WorldTimeScale(dodgeSlowFrames_));
    }
    if (impactSlowFrames_ > 0) {
        skillScale *= 0.35f;
    }
    return std::clamp(skillScale, 0.05f, 1.0f);
}
