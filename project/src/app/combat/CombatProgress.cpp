// ===== CombatProgress.cpp =====
// 撃破チェーンの継続時間と倍率を管理し、戦闘中のスコアを集計する

#include "app/combat/CombatProgress.h"

#include <algorithm>

void CombatProgress::Reset()
{
    score_ = 0;
    ResetChain();
}

// チェイン数、残り時間、警告表示、今回の最高チェインを初期状態へ戻す
void CombatProgress::ResetChain()
{
    chainCount_ = 0;
    chainRemainingFrames_ = 0;
    chainBreakFlashFrames_ = 0;
    maxChainCount_ = 0;
}

// 正の基本点にフィーバーなどの倍率と現在のチェイン倍率を掛けて加える
void CombatProgress::AddScore(int baseScore, int bonusMultiplier)
{
    if (baseScore <= 0) { return; }
    score_ += baseScore * bonusMultiplier * ChainMultiplier();
}

// 通常戦闘で次に狙える敵がいる間だけ、チェインの残り時間を減らす　時間切れでチェインを終了する
void CombatProgress::Update(bool hasTarget, bool gameplayRunning)
{
    if (chainBreakFlashFrames_ > 0) { --chainBreakFlashFrames_; }
    // 移動だけの区間では猶予を消費しない
    if (!gameplayRunning || !hasTarget || chainCount_ <= 0) { return; }
    if (chainRemainingFrames_ > 0) { --chainRemainingFrames_; }
    if (chainRemainingFrames_ <= 0) { BreakChain(); }
}

// 敵を倒した時点で撃破チェインを加算し、次の撃破までの猶予を更新する
void CombatProgress::RegisterDefeat()
{
    chainCount_ = (std::min)(chainCount_ + 1, kChainMaxCount);
    chainRemainingFrames_ = kChainDurationFrames;
    chainBreakFlashFrames_ = 0;
    maxChainCount_ = (std::max)(maxChainCount_, chainCount_);
}

// 現在のチェインを終了し、連続撃破していた場合は途切れたことをHUDへ知らせる
void CombatProgress::BreakChain()
{
    if (chainCount_ >= 2) { chainBreakFlashFrames_ = kChainBreakFlashFrames; }
    chainCount_ = 0;
    chainRemainingFrames_ = 0;
}

// すでにチェイン中なら、残り時間を指定フレーム以上に保つ
void CombatProgress::ExtendChain(int minimumFrames)
{
    if (chainCount_ > 0) {
        chainRemainingFrames_ = (std::max)(chainRemainingFrames_, minimumFrames);
    }
}

// チェイン中にリングを通過したときなど、残り時間を既定値に戻す
void CombatProgress::RefreshChain()
{
    if (chainCount_ > 0) { chainRemainingFrames_ = kChainDurationFrames; }
}

// 現在のチェイン数から得点倍率を計算し、最大倍率までに収める
int CombatProgress::ChainMultiplier() const
{
    return (std::min)(1 + chainCount_ / kChainScoreTierSize, kChainMaxMultiplier);
}
