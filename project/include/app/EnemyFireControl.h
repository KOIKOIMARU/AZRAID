#pragma once

#include "engine/base/Math.h"
#include <algorithm>
#include <cmath>
#include <limits>

// 敵射撃の時間と照準計算。描画・弾生成から分け、実際に使う処理を独立検証できるようにする。
namespace EnemyFireControl {
struct Pattern {
    float windup = 28.0f; // 構えてから初弾まで。60fps換算。
    float interval = 12.0f; // 同じ連射内の弾間隔。
    float recovery = 86.0f; // 撃ち終わってから次に構えるまで。
    int shots = 3; // 一回の構えから撃つ回数。扇状弾の本数とは別。
    float aimLockLead = 8.0f; // 初弾の何フレーム前に横・縦の照準を固定するか。
};

enum class Event { None, Aim, Fire };

class Cycle {
public:
    Event Advance(float step, bool canShoot, const Pattern& pattern, float recoveryScale = 1.0f)
    {
        const float elapsed = std::clamp(step, 0.0f, 2.0f);
        shotFlash_ = (std::max)(0.0f, shotFlash_ - elapsed);
        if (phase_ == Phase::Recovery) { recoveryElapsed_ += elapsed; }
        if (!canShoot) {
            phase_ = Phase::Idle;
            remaining_ = 0.0f;
            shotIndex_ = 0;
            shotFlash_ = 0.0f;
            recoveryElapsed_ = 0.0f;
            return Event::None; // 撃破・撤退した敵の予約弾を出さない。
        }
        if (phase_ == Phase::Idle) {
            phase_ = Phase::Windup;
            remaining_ = windup_ = pattern.windup;
            aimLockLead_ = std::clamp(pattern.aimLockLead, 0.0f, windup_);
            shotIndex_ = 0;
            ++volleyIndex_;
            return Event::Aim;
        }
        remaining_ -= elapsed;
        if (remaining_ > 0.0f) { return Event::None; }
        if (phase_ == Phase::Recovery) {
            phase_ = Phase::Idle;
            return Event::None;
        }
        if (phase_ == Phase::Burst) { ++shotIndex_; }
        if (shotIndex_ + 1 >= pattern.shots) {
            phase_ = Phase::Recovery;
            recoveryElapsed_ = 0.0f;
            remaining_ += pattern.recovery * recoveryScale;
        } else {
            phase_ = Phase::Burst;
            remaining_ += pattern.interval;
        }
        shotFlash_ = 8.0f;
        return Event::Fire;
    }

    float ChargeRate() const
    {
        return phase_ == Phase::Windup ?
            std::clamp(1.0f - remaining_ / (std::max)(windup_, 1.0f), 0.0f, 1.0f) : 0.0f;
    }
    bool IsTracking() const { return phase_ == Phase::Windup && remaining_ > aimLockLead_; }
    bool IsBraced() const { return phase_ == Phase::Windup || phase_ == Phase::Burst; }
    float ShotFlash() const { return shotFlash_ / 8.0f; }
    float RecoveryElapsed() const { return phase_ == Phase::Recovery ? recoveryElapsed_ : -1.0f; }
    int ShotIndex() const { return shotIndex_; }
    int VolleyIndex() const { return volleyIndex_; }
    Math::Vector3 aimPoint{}; // 構え中だけ更新し、発射直前から連射終了までは固定。

private:
    enum class Phase { Idle, Windup, Burst, Recovery };
    Phase phase_ = Phase::Idle;
    float remaining_ = 0.0f;
    float windup_ = 1.0f;
    float aimLockLead_ = 8.0f;
    float shotFlash_ = 0.0f; // 発射直後の反動と銃口光を同じ時計で動かす。
    float recoveryElapsed_ = 0.0f; // 撃ち終わりの隙の経過時間。
    int shotIndex_ = 0;
    int volleyIndex_ = 0;
};

// 自機のレール前進を含む迎撃方向。横移動は呼び出し側の固定照準で扱い、弾自体は追尾しない。
inline Math::Vector3 SolveShotDirection(const Math::Vector3& origin,
    const Math::Vector3& target, float railSpeed, float bulletSpeed)
{
    const Math::Vector3 r{ target.x - origin.x, target.y - origin.y, target.z - origin.z };
    const float c = r.x * r.x + r.y * r.y + r.z * r.z;
    if (c < 0.0001f || bulletSpeed <= 0.0001f) { return { 0, 0, -1 }; }
    const float a = railSpeed * railSpeed - bulletSpeed * bulletSpeed;
    const float b = 2.0f * r.z * railSpeed;
    float time = (std::numeric_limits<float>::max)();
    if (std::abs(a) < 0.00001f) {
        if (std::abs(b) > 0.00001f && -c / b > 0.0f) { time = -c / b; }
    } else {
        const float discriminant = b * b - 4.0f * a * c;
        if (discriminant >= 0.0f) {
            const float root = std::sqrt(discriminant);
            for (const float candidate : { (-b - root) / (2.0f * a), (-b + root) / (2.0f * a) }) {
                if (candidate > 0.0f) { time = (std::min)(time, candidate); }
            }
        }
    }
    const float leadZ = time < (std::numeric_limits<float>::max)() ? railSpeed * time : 0.0f;
    return Math::Normalize({ r.x, r.y, r.z + leadZ });
}
} // namespace EnemyFireControl
