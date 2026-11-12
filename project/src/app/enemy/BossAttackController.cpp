// ===== BossAttackController.cpp =====
// ボスの攻撃選択と溜め・発射・隙の進行を管理し、各段階の切り替わりを通知する

#include "app/enemy/BossAttackController.h"
#include <algorithm>
#include <cmath>

// 新しいボス戦の開始時に、形態、次の攻撃、連射番号、反撃時間を初期状態へ戻す
void BossAttackController::Reset()
{
    phase_ = 1;
    pattern_ = Pattern::Fan;
    action_ = Action::Waiting;
    remaining_ = cooldown_ = shotIndex_ = attackCount_ = counterRemaining_ = 0;
    counterDuration_ = 1;
    attackElapsed_ = 0;
    flightClock_ = 0.0f;
    nextPatternOverride_ = -1;
    aimPoint_ = {};
}

// 攻撃パターンと現在の形態から、予告を見せるフレーム数を返す
int BossAttackController::WindupFrames() const
{
    constexpr std::array<int, 5> normal{ 48, 54, 78, 58, 60 };
    constexpr std::array<int, 5> enraged{ 40, 44, 64, 46, 48 };
    const auto index = static_cast<size_t>(pattern_);
    return phase_ >= 2 ? enraged[index] : normal[index];
}

// 攻撃に合わせたボスの横・縦・前後の移動先を返し、EnemyFlightがそこへ補間する
Math::Vector3 BossAttackController::FlightOffset() const
{
    const float side = attackCount_ % 2 == 0 ? -1.0f : 1.0f;
    const float t = static_cast<float>(attackElapsed_);
    const float windup = static_cast<float>(WindupFrames());
    const float ready = std::clamp(t / windup, 0.0f, 1.0f);
    Math::Vector3 offset{ std::sin(flightClock_ * 0.018f) * 4.8f,
        0.5f + std::sin(flightClock_ * 0.026f) * 1.3f, 0.0f };
    if (!IsAttacking()) { return offset; }
    switch (pattern_) {
    case Pattern::Fan:
        return { side * (2.0f + 3.5f * ready), 1.4f + ready, -3.0f * ready };
    case Pattern::Sweep: {
        const float across = std::clamp((t - windup * 0.35f) / 105.0f, 0.0f, 1.0f);
        return { side * (6.0f - 12.0f * across), 2.2f - across * 1.5f, -4.0f };
    }
    case Pattern::Charge:
        return { side * 1.8f, 1.0f + ready * 1.8f, 3.0f - ready * 6.0f };
    case Pattern::Crossfire:
        return { side * (5.8f - ready * 2.0f), -0.5f + ready * 2.0f, 2.5f };
    case Pattern::Rush: {
        const float across = std::clamp((t - windup * 0.50f) / 80.0f, 0.0f, 1.0f);
        const float dive = std::sin(across * 3.14159265f);
        return { side * (6.8f - 13.6f * across), 2.0f - dive * 1.8f, -dive * 14.0f };
    }
    case Pattern::Count: break;
    }
    return offset;
}

// 進行中の予告や連射を取り消し、構えと反撃時間をリセットする
void BossAttackController::CancelAttack()
{
    action_ = Action::Waiting;
    remaining_ = cooldown_ = shotIndex_ = counterRemaining_ = 0;
}

// 攻撃終了後の反撃可能時間を設定する　攻撃の種類と形態で長さが変わる
void BossAttackController::BeginRecovery()
{
    action_ = Action::Recovery;
    shotIndex_ = 0;
    // 大技の後だけ長めに止める　全攻撃に同じ長い休止を挟んで単調にしない
    const bool heavy = pattern_ == Pattern::Charge || pattern_ == Pattern::Rush;
    counterRemaining_ = counterDuration_ = (phase_ >= 2 ? 84 : 96) + (heavy ? 24 : 0);
    cooldown_ = counterDuration_ + 12;
}

// 待機、予告、連射、反撃の隙を順に進める　発射と形態変更はイベントで呼び出し元へ渡す
BossAttackController::Events BossAttackController::Update(const Input& input)
{
    Events events;
    if (!input.present) {
        action_ = Action::Waiting;
        remaining_ = shotIndex_ = counterRemaining_ = 0;
        return events;
    }
    if (counterRemaining_ > 0) { --counterRemaining_; }
    if (action_ == Action::Recovery && counterRemaining_ == 0) { action_ = Action::Waiting; }

    // 段階移行の演出は呼び出し側が扱う　HP条件は一度だけ判定して進行を切り替える
    if (phase_ == 1 && input.hp * 2 <= input.maxHp) {
        phase_ = 2;
        CancelAttack();
        events.phaseChanged = true;
        return events;
    }
    if (!input.canAttack) { return events; }
    flightClock_ += 1.0f;

    if (!IsAttacking()) {
        if (cooldown_ > 0) { --cooldown_; return events; }
        constexpr std::array<Pattern, 5> normal{ Pattern::Fan, Pattern::Sweep, Pattern::Crossfire, Pattern::Charge, Pattern::Rush };
        constexpr std::array<Pattern, 5> enragedOrder{ Pattern::Rush, Pattern::Crossfire, Pattern::Sweep, Pattern::Charge, Pattern::Fan };
        const auto index = static_cast<size_t>(attackCount_ % 5);
        pattern_ = nextPatternOverride_ >= 0 ? static_cast<Pattern>(nextPatternOverride_) :
            phase_ >= 2 ? enragedOrder[index] : normal[index];
        nextPatternOverride_ = -1;
        ++attackCount_;
        action_ = Action::Windup;
        shotIndex_ = 0;
        aimPoint_ = input.playerPosition;
        remaining_ = WindupFrames();
        attackElapsed_ = 0;
        events.attackStarted = true;
        return events;
    }
    ++attackElapsed_;
    if (remaining_ > 0) {
        if (IsWindingUp() && remaining_ > kAimLockFrames) { aimPoint_ = input.playerPosition; }
        --remaining_;
        return events; // 0に達した次の更新で発射する　従来の発射間隔を保つ
    }

    const bool enraged = phase_ >= 2;
    // 弾の指示に銃口・弾種・固定照準からのずれを記録する　弾の生成はGameRuntimeが受け持つ
    const auto addShot = [&](Muzzle muzzle, bool charged, float x, float y) {
        events.shots[events.shotCount++] = { muzzle, charged, { x, y } };
    };
    switch (pattern_) {
    case Pattern::Fan: {
        const int count = enraged ? 7 : 5;
        // 扇状攻撃は1フレームにまとめて放つ　第2形態では弾数と広がりを増やす
        const float spread = enraged ? 5.4f : 4.4f;
        for (int index = 0; index < count; ++index) {
            const float rate = static_cast<float>(index) / static_cast<float>(count - 1);
            addShot(index % 2 == 0 ? Muzzle::Left : Muzzle::Right, false,
                -spread + spread * 2.0f * rate, 0.50f - std::abs(rate - 0.5f) * 1.4f);
        }
        BeginRecovery();
        events.recoveryStarted = true;
        break;
    }
    case Pattern::Sweep: {
        const int count = enraged ? 9 : 7;
        const float rate = static_cast<float>(shotIndex_) / static_cast<float>(count - 1);
        addShot(shotIndex_ % 2 == 0 ? Muzzle::Left : Muzzle::Right, false,
            -5.8f + 11.6f * rate, shotIndex_ % 2 == 0 ? -0.7f : 0.7f);
        ++shotIndex_;
        if (shotIndex_ >= count) {
            BeginRecovery();
            events.recoveryStarted = true;
        } else {
            action_ = Action::Burst;
            remaining_ = enraged ? 5 : 7;
        }
        break;
    }
    case Pattern::Charge:
        addShot(Muzzle::Center, true, 0.0f, 0.0f);
        if (enraged) {
            addShot(Muzzle::Left, false, -2.4f, 0.0f);
            addShot(Muzzle::Right, false, 2.4f, 0.0f);
        }
        BeginRecovery();
        events.recoveryStarted = true;
        break;
    case Pattern::Crossfire: {
        // 左右を交差させる三連射　上下に抜け道を残し、固定照準のまま撃つ
        const float spread = 3.6f - static_cast<float>(shotIndex_) * 1.2f;
        addShot(Muzzle::Left, false, spread, -0.9f);
        addShot(Muzzle::Right, false, -spread, 0.9f);
        if (enraged) { addShot(Muzzle::Center, false, 0.0f, shotIndex_ % 2 == 0 ? 2.3f : -2.3f); }
        if (++shotIndex_ >= 3) { BeginRecovery(); events.recoveryStarted = true; }
        else { action_ = Action::Burst; remaining_ = enraged ? 10 : 14; }
        break;
    }
    case Pattern::Rush: {
        const int count = enraged ? 9 : 7;
        const float rate = static_cast<float>(shotIndex_) / static_cast<float>(count - 1);
        addShot(shotIndex_ % 2 == 0 ? Muzzle::Left : Muzzle::Right, false,
            (attackCount_ % 2 == 0 ? -1.0f : 1.0f) * (4.5f - rate * 9.0f),
            std::sin(rate * 3.14159265f) * 1.3f);
        if (++shotIndex_ >= count) { BeginRecovery(); events.recoveryStarted = true; }
        else { action_ = Action::Burst; remaining_ = enraged ? 5 : 7; }
        break;
    }
    case Pattern::Count: break;
    }
    return events;
}

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
// 開発用のボス戦を指定形態から試せるよう、HP段階と攻撃状態をそろえる
void BossAttackController::PrepareForDebug(int phase, Pattern nextPattern)
{
    Reset();
    phase_ = std::clamp(phase, 1, 2);
    nextPatternOverride_ = std::clamp(static_cast<int>(nextPattern), 0, static_cast<int>(Pattern::Count) - 1);
}

void BossAttackController::RequestNextForDebug()
{
    CancelAttack();
}

// 確認用に反撃の隙を開き、残り時間を指定してダメージ処理を試せるようにする
void BossAttackController::OpenCounterForDebug(int frames)
{
    CancelAttack();
    counterRemaining_ = (std::max)(0, frames);
    counterDuration_ = (std::max)(1, frames);
    cooldown_ = counterRemaining_ + 24;
    action_ = counterRemaining_ > 0 ? Action::Recovery : Action::Waiting;
}
#endif
