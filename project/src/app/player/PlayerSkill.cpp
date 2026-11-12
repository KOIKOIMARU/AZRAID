// ===== PlayerSkill.cpp =====
// スキルの対象選びと発動条件、連撃の進行、再発動までの状態を管理する

#include "app/player/PlayerSkill.h"
#include <algorithm>
#include <limits>

// 候補の敵を照準からの距離順に入れる　同じ敵は重複させず、発動時の上限に入る候補を残す
void PlayerSkill::Selection::Consider(const Enemy* enemy, Math::Vector3 position, float distanceSquared, bool boss)
{
    if (!enemy || !(distanceSquared < (std::numeric_limits<float>::max)())) { return; }
    for (int index = 0; index < count_; ++index) { if (entries_[index].target.enemy == enemy) { return; } }
    const int capacity = empowered_ ? 5 : 3;
    int slot = 0;
    while (slot < count_ && !(distanceSquared < entries_[slot].distance)) { ++slot; }
    if (slot >= capacity) { return; }
    for (int index = (std::min)(count_, capacity - 1); index > slot; --index) { entries_[index] = entries_[index - 1]; }
    entries_[slot] = { { enemy, position, 0 }, distanceSquared, boss };
    count_ = (std::min)(count_ + 1, capacity);
}

// 再使用までの待ち時間、対象、連撃の進行、発動回数を初期状態に戻す
void PlayerSkill::Reset()
{
    CancelSequence();
    ready_ = true;
    empowered_ = finished_ = false;
    cooldown_ = readyFlash_ = noTargetNotice_ = 0.0f;
    strikeCount_ = 3;
    nextStrike_ = activationCount_ = defeatCount_ = 0;
}

// 再使用可能で対象がいるときに連撃を開始する　対象がいない場合はその通知を出す時間を設定する
bool PlayerSkill::TryActivate(const Selection& selection)
{
    if (!ready_ || IsActive()) { return false; }
    if (selection.count_ == 0) { noTargetNotice_ = 60.0f; return false; }
    targets_ = {};
    targetCount_ = 0;
    for (int index = 0; index < selection.count_; ++index) {
        targets_[targetCount_++] = selection.entries_[index].target;
        if (selection.entries_[index].boss) { break; }
    }
    empowered_ = selection.empowered_;
    strikeCount_ = empowered_ ? 5 : 3;
    nextStrike_ = 0;
    finished_ = ready_ = false;
    readyFlash_ = noTargetNotice_ = 0.0f;
    clock_ = 0.0f;
    cooldown_ = kCooldownFrames;
    ++activationCount_;
    return true;
}

// 再使用までの時間とHUD通知の時間を減らす　1回に進める量には上限を設ける
void PlayerSkill::TickTimers(float frameStep)
{
    const float step = std::clamp(frameStep, 0.0f, 4.0f);
    cooldown_ = (std::max)(0.0f, cooldown_ - step);
    readyFlash_ = (std::max)(0.0f, readyFlash_ - step);
    noTargetNotice_ = (std::max)(0.0f, noTargetNotice_ - step);
}

// 再使用の待ち時間が終わったときだけtrueを返し、発動可能表示を始める
bool PlayerSkill::TryRecover()
{
    if (ready_ || IsActive() || cooldown_ > 0.0f) { return false; }
    ready_ = true;
    readyFlash_ = 72.0f;
    return true;
}

// 射撃が直撃した報酬としてスキルの再使用を早める　チャージ命中と撃破では短縮量が大きい
void PlayerSkill::RecoverOnHit(bool charged, bool destroyed)
{
    if (ready_ || IsActive()) { return; }
    // 連撃自身や爆風では呼ばず、通常の直撃だけを回復へつなげる
    const float recovery = (charged ? 6.0f : 1.5f) + (destroyed ? 3.0f : 0.0f);
    cooldown_ = (std::max)(0.0f, cooldown_ - recovery);
}

// 練習では待ち時間を0にしてスキルを試せるようにする　実行中は終了を待つ
void PlayerSkill::MakeReadyForPractice()
{
    if (IsActive()) { return; }
    ready_ = true;
    cooldown_ = 0.0f;
}

// 発動中のスキルの時計を進める　一度に進める量を制限して連撃時刻が急に飛ばないようにする
void PlayerSkill::AdvanceSequence(float frameStep)
{
    if (IsActive()) { clock_ += std::clamp(frameStep, 0.0f, 4.0f); }
}

// 演出の時刻に達した連撃の番号を一度だけ返す　呼び出し元がその番号で攻撃対象を選ぶ
int PlayerSkill::ConsumeStrike()
{
    if (!IsActive() || nextStrike_ >= strikeCount_ ||
        clock_ < kWindupFrames + static_cast<float>(nextStrike_) * kStrikeInterval) { return -1; }
    return nextStrike_++;
}

// 攻撃が入った位置と回数を対象ごとに覚え、最後の一撃へ渡す
void PlayerSkill::MarkStrike(int strikeIndex, Math::Vector3 position)
{
    if (!IsActive() || targetCount_ == 0 || strikeIndex < 0 || strikeIndex >= nextStrike_) { return; }
    auto& target = targets_[strikeIndex % targetCount_];
    if (!target.enemy) { return; }
    target.position = position;
    ++target.marks;
}

// 連撃後の時刻に達したら、最後の一撃の実行を一度だけ許可する
bool PlayerSkill::ConsumeFinisher()
{
    if (!IsActive() || finished_ || clock_ < FinisherTime()) { return false; }
    finished_ = true;
    return true;
}

// 最後の一撃から余韻の時間が経ったら、スキル演出を終了する
void PlayerSkill::EndSequenceIfElapsed()
{
    if (IsActive() && clock_ >= FinisherTime() + 20.0f) { CancelSequence(); }
}

// 連撃を終了し、対象への参照と選択数を解除する
void PlayerSkill::CancelSequence()
{
    clock_ = -1.0f;
    targets_ = {};
    targetCount_ = 0;
}

// 選択済みの敵が動いたとき、連撃と最後の一撃に使う位置を更新する
void PlayerSkill::UpdateTargetPosition(const Enemy* enemy, Math::Vector3 position)
{
    if (!enemy) { return; }
    for (int index = 0; index < targetCount_; ++index) {
        if (targets_[index].enemy == enemy) { targets_[index].position = position; }
    }
}

// 退場する敵の参照を解除する　連撃の記録には直前までの位置が残る
void PlayerSkill::InvalidateTarget(const Enemy* enemy)
{
    for (int index = 0; index < targetCount_; ++index) {
        if (targets_[index].enemy == enemy) { targets_[index].enemy = nullptr; }
    }
}

// 構え、連撃、最後の一撃の時刻に応じて、敵や弾に使う時間倍率を返す
float PlayerSkill::WorldTimeScale() const
{
    if (!IsActive()) { return 1.0f; }
    return clock_ < kWindupFrames ? 0.55f : clock_ < FinisherTime() ? 0.75f :
        clock_ < FinisherTime() + 5.0f ? 0.08f : 1.0f;
}

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
void PlayerSkill::SetRechargeForDebug(float frames, bool ready)
{
    ready_ = ready;
    cooldown_ = ready ? 0.0f : (std::max)(0.0f, frames);
}
#endif
