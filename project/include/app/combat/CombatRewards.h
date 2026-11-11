#pragma once

#include "app/combat/FeverState.h"

class CombatProgress;
class PlayerShooting;

// リング通過とジャスト回避の報酬、フィーバー中のチャージ補充を管理する。
class CombatRewards {
public:
    static constexpr int kRingGaugeReward = 14;
    static constexpr int kRingFeverRecoveryFrames = 90;

    void Reset();
    // 発動したフレームだけtrue。終了フレームにもチャージは補充する。
    bool UpdateFever(PlayerShooting& shooting, bool canActivate);
    void ActivateFever(PlayerShooting& shooting);
    void ApplyRingPass(CombatProgress& progress, PlayerShooting& shooting, bool canEarnFever);
    void ApplyJustDodge(CombatProgress& progress, PlayerShooting& shooting, bool canEarnFever);

    void ResetRingNotice() { ringNoticeFrames_ = ringRecoveredFrames_ = 0; }
    void HideRingNotice() { ringNoticeFrames_ = 0; }
    void TickRingNotice();

    FeverState& Fever() { return fever_; }
    const FeverState& Fever() const { return fever_; }
    int JustDodgeCount() const { return justDodgeCount_; }
    int RingNoticeFrames() const { return ringNoticeFrames_; }
    int RingRecoveredFrames() const { return ringRecoveredFrames_; }

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
    void ResetJustDodgesForDebug() { justDodgeCount_ = 0; }
#endif

private:
    FeverState fever_;
    int justDodgeCount_ = 0;
    int ringNoticeFrames_ = 0;
    int ringRecoveredFrames_ = 0;
};
