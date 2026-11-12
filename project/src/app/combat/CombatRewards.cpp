// ===== CombatRewards.cpp =====
// リング通過やジャスト回避の報酬を反映し、フィーバーの発動と終了を扱う

#include "app/combat/CombatRewards.h"

#include "app/combat/CombatProgress.h"
#include "app/runtime/GameRuntimeTuning.h"
#include "app/player/PlayerShooting.h"

// 今回のフィーバー、リング報酬の通知、ジャスト回避の回数を初期化する
void CombatRewards::Reset()
{
    fever_.Reset();
    justDodgeCount_ = 0;
    ResetRingNotice();
}

// 発動中はチャージを補充し続ける　通常時は発動条件を確認し、新たに発動したフレームだけtrueを返す
bool CombatRewards::UpdateFever(PlayerShooting& shooting, bool canActivate)
{
    if (fever_.Tick()) {
        shooting.FillCharge();
        return false;
    }
    if (!canActivate || !fever_.IsReady()) { return false; }
    ActivateFever(shooting);
    return true;
}

void CombatRewards::ActivateFever(PlayerShooting& shooting)
{
    fever_.Activate();
    shooting.FillCharge();
}

// リングで得点とチャージを補充する　通常時は条件に応じてゲージ、発動中はフィーバー時間を増やす
void CombatRewards::ApplyRingPass(CombatProgress& progress, PlayerShooting& shooting, bool canEarnFever)
{
    ringRecoveredFrames_ = 0;
    if (fever_.IsActive()) {
        ringRecoveredFrames_ = fever_.Extend(kRingFeverRecoveryFrames);
    } else if (canEarnFever) {
        fever_.AddGauge(kRingGaugeReward);
    }
    shooting.FillCharge();
    shooting.CapCooldown(4); // 通過直後の攻撃へつなげる
    progress.RefreshChain();
    progress.AddScore(GameRuntimeTuning::kRiftPassScore, fever_.ScoreMultiplier());
    ringNoticeFrames_ = 54;
}

// ジャスト回避に対して得点、チャージ、チェイン猶予を与える　ゲージが許可された場面ではそれも加える
void CombatRewards::ApplyJustDodge(CombatProgress& progress, PlayerShooting& shooting, bool canEarnFever)
{
    ++justDodgeCount_;
    progress.AddScore(50, fever_.ScoreMultiplier());
    if (canEarnFever) { fever_.AddGauge(8); }
    progress.ExtendChain(90);
    shooting.AddCharge(24);
}

// リング通過の通知が表示中なら、残り表示フレームを1つ減らす
void CombatRewards::TickRingNotice()
{
    if (ringNoticeFrames_ > 0) { --ringNoticeFrames_; }
}
