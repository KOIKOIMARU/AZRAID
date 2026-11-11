#pragma once

#include "engine/base/Math.h"
#include <array>

// ボスの攻撃順・予備動作・固定照準・反撃時間を管理する。
// 敵や弾を所有せず、この更新で必要になった発射要求だけを返す。
class BossAttackController {
public:
    enum class Pattern { Fan, Sweep, Charge, Crossfire, Rush, Count };
    enum class Action { Waiting, Windup, Burst, Recovery };
    enum class Muzzle { Left, Right, Center };
    static constexpr int kAimLockFrames = 14;
    static constexpr int kMaxShotsPerUpdate = 7;

    struct Input {
        bool present = false;
        bool canAttack = false; // 入場・段階移行の演出中は呼び出し側で止める。
        int hp = 0;
        int maxHp = 1;
        Math::Vector3 playerPosition{};
    };
    struct Shot {
        Muzzle muzzle = Muzzle::Left;
        bool charged = false;
        Math::Vector2 targetOffset{};
    };
    struct Events {
        std::array<Shot, kMaxShotsPerUpdate> shots{};
        int shotCount = 0;
        bool phaseChanged = false;
        bool attackStarted = false;
        bool recoveryStarted = false;
    };

    void Reset();
    Events Update(const Input& input);
    // 勝敗確定時に予約攻撃と反撃判定を閉じる。段階・攻撃回数は結果の確認用に残す。
    void CancelAttack();

    int Phase() const { return phase_; }
    Pattern CurrentPattern() const { return pattern_; }
    Action CurrentAction() const { return action_; }
    bool IsAttacking() const { return action_ == Action::Windup || action_ == Action::Burst; }
    bool IsWindingUp() const { return action_ == Action::Windup; }
    bool IsBursting() const { return action_ == Action::Burst; }
    bool IsAimLocked() const { return IsAttacking() && (IsBursting() || remaining_ <= kAimLockFrames); }
    bool IsCounterOpen() const { return counterRemaining_ > 0; }
    int RemainingFrames() const { return remaining_; }
    int CooldownFrames() const { return cooldown_; }
    int CounterFrames() const { return counterRemaining_; }
    int CounterDuration() const { return counterDuration_; }
    int ShotIndex() const { return shotIndex_; }
    int AttackCount() const { return attackCount_; }
    const Math::Vector3& AimPoint() const { return aimPoint_; }
    int WindupFrames() const;
    // レールに追従する基準位置からの移動先。敵側で補間し、攻撃の切り替えでも瞬間移動しない。
    Math::Vector3 FlightOffset() const;

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
    // 単独の攻撃・ダメージ試験用。通常の進行からは呼ばない。
    void PrepareForDebug(int phase, Pattern nextPattern);
    void RequestNextForDebug();
    void OpenCounterForDebug(int frames);
#endif

private:
    void BeginRecovery();
    int phase_ = 1;
    Pattern pattern_ = Pattern::Fan;
    Action action_ = Action::Waiting;
    int remaining_ = 0;
    int cooldown_ = 0;
    int shotIndex_ = 0;
    int attackCount_ = 0;
    int counterRemaining_ = 0;
    int counterDuration_ = 1;
    int attackElapsed_ = 0;
    float flightClock_ = 0.0f;
    int nextPatternOverride_ = -1;
    Math::Vector3 aimPoint_{}; // 予備動作の最後14更新と連射中は追い直さない。
};
