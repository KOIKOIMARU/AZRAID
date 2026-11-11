#include "app/player/PlayerShooting.h"

#include <algorithm>

// 次の戦闘に向けて発射待ち、先行入力、チャージを戻す。設定済みの射撃間隔はそのまま使う。
void PlayerShooting::Reset()
{
    chargeFrames_ = cooldownFrames_ = bufferedFrames_ = 0;
}

// 先行入力と発射間隔を更新して撃てるか返す。射撃を止めている間はチャージがたまる。
bool PlayerShooting::Update(InputState input, bool feverActive)
{
    if (cooldownFrames_ > 0) { --cooldownFrames_; }
    if (bufferedFrames_ > 0) { --bufferedFrames_; }
    if (input.triggered && cooldownFrames_ > 0) { bufferedFrames_ = kInputBufferFrames; }

    if ((input.held || bufferedFrames_ > 0) && cooldownFrames_ == 0) {
        bufferedFrames_ = 0;
        cooldownFrames_ = feverActive ? kFeverIntervalFrames :
            (IsChargeReady() ? tuning_.chargedIntervalFrames : tuning_.normalIntervalFrames);
        return true;
    }
    if (!input.held && bufferedFrames_ == 0) { AddCharge(1); }
    return false;
}

// 弾を発射できたときにチャージを消費する。フィーバー中は最大のまま次の射撃に移る。
void PlayerShooting::OnShotFired(bool feverActive)
{
    chargeFrames_ = feverActive ? kMaxChargeFrames : 0;
}

// リングやフィーバー発動などの報酬でチャージを最大まで補充する。
void PlayerShooting::FillCharge()
{
    chargeFrames_ = kMaxChargeFrames;
}

void PlayerShooting::AddCharge(int frames)
{
    // 加算前に範囲を絞り、大きな補充値でも整数があふれないようにする。
    chargeFrames_ = (std::min)(chargeFrames_ + std::clamp(frames, 0, kMaxChargeFrames), kMaxChargeFrames);
}

// 発射までの待ち時間が指定値より長い場合に切り詰める。すでに撃てる状態は維持する。
void PlayerShooting::CapCooldown(int maximumFrames)
{
    cooldownFrames_ = (std::min)(cooldownFrames_, (std::max)(maximumFrames, 0));
}

// チャージと連射間隔を設定し、発射タイマーが成立する範囲に調整値を収める。
void PlayerShooting::SetTuning(Tuning tuning)
{
    tuning.chargeFrames = std::clamp(tuning.chargeFrames, 1, kMaxChargeFrames);
    tuning.normalIntervalFrames = (std::max)(tuning.normalIntervalFrames, 1);
    tuning.chargedIntervalFrames = (std::max)(tuning.chargedIntervalFrames, 1);
    tuning_ = tuning;
}

// チャージ完了までの進み具合を0〜1で返す。HUDのゲージや発光に使う。
float PlayerShooting::ChargeRate() const
{
    return std::clamp(static_cast<float>(chargeFrames_) / static_cast<float>(tuning_.chargeFrames), 0.0f, 1.0f);
}

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
void PlayerShooting::SetChargeForDebug(int frames)
{
    chargeFrames_ = std::clamp(frames, 0, kMaxChargeFrames);
}

void PlayerShooting::SetCooldownForDebug(int frames)
{
    cooldownFrames_ = (std::max)(frames, 0);
}
#endif
