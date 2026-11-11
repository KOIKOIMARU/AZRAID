#include "app/enemy/EnemyAttackPatterns.h"

#include "app/enemy/Enemy.h"
#include "app/enemy/EnemyFireControl.h"
#include "app/runtime/GameRuntimeMath.h"

#include <algorithm>

// 自機の位置差から移動の傾向を記録し、射撃の照準補正に使う。
void EnemyAttackPatterns::TrackPlayerMovement(const Math::Vector3& before, const Math::Vector3& after,
    float frameStep, bool dodging)
{
    const float step = (std::max)(frameStep, 0.1f);
    const Math::Vector2 velocity{
        std::clamp((after.x - before.x) / step, -0.205f, 0.205f),
        std::clamp((after.y - before.y) / step, -0.205f, 0.205f)
    };
    playerAimVelocity_ = dodging ? Math::Vector2{} : Math::Vector2{
        GameRuntimeMath::Lerp(playerAimVelocity_.x, velocity.x, 0.3f),
        GameRuntimeMath::Lerp(playerAimVelocity_.y, velocity.y, 0.3f)
    };
}

// 通常敵の機種に合う射撃周期を進める。発射時に固定照準と弾の並びを返す。
std::optional<EnemyAttackPatterns::Volley> EnemyAttackPatterns::Update(Enemy& enemy,
    const Math::Vector3& playerPosition, float worldStep, int shotInterval, bool hasSupportDrone, bool visible) const
{
    EnemyFireControl::Pattern pattern;
    pattern.recovery = static_cast<float>(shotInterval) * 1.4f;
    float leadFrames = 12.0f;
    switch (enemy.GetBehavior()) {
    case Enemy::Behavior::Formation:
        pattern = { 24.0f, 14.0f, shotInterval * 1.1f, 2 };
        if (enemy.IsTightFormation()) {
            // 小型三機の斉射を一度に重ねない。内側から順に撃ち、狙撃機が主な脅威になる。
            pattern = { 24.0f + enemy.GetFormationShotDelay(), 14.0f, shotInterval * 1.35f, 2 };
        }
        break;
    case Enemy::Behavior::Sniper:
        pattern = { 60.0f, 18.0f, shotInterval * 2.0f, 2, 22.0f };
        leadFrames = 22.0f;
        break;
    case Enemy::Behavior::Shield:
        pattern = { 32.0f, 18.0f, shotInterval * 1.45f, 2 };
        leadFrames = 6.0f;
        break;
    case Enemy::Behavior::Crossfire:
        pattern = { 32.0f, 11.0f, shotInterval * 1.25f, 3 };
        break;
    case Enemy::Behavior::StrafeShooter:
        pattern = { 26.0f, 10.0f, shotInterval * 1.2f, 3 };
        leadFrames = 18.0f;
        break;
    case Enemy::Behavior::DiveBomber:
        pattern = { 22.0f, 10.0f, shotInterval * 1.2f, 3 };
        break;
    case Enemy::Behavior::Swoop:
        pattern = { 20.0f, 10.0f, shotInterval * 1.1f, 2 };
        break;
    default:
        break;
    }

    auto& control = enemy.GetFireControl();
    const auto event = control.Advance(worldStep, enemy.CanShoot() && visible,
        pattern, hasSupportDrone ? 0.72f : 1.0f);
    if (event == EnemyFireControl::Event::Aim || control.IsTracking()) {
        control.aimPoint = playerPosition;
        // 入力の未来を読むのではなく、観測した移動だけを少し先読みする。
        control.aimPoint.x += std::clamp(playerAimVelocity_.x * leadFrames, -2.6f, 2.6f);
        control.aimPoint.y += std::clamp(playerAimVelocity_.y * leadFrames, -1.3f, 1.3f);
    }
    if (event != EnemyFireControl::Event::Fire) { return std::nullopt; }

    Volley volley{ enemy.GetAimPosition(), control.aimPoint };
    using Style = ProjectileSystem::EnemyStyle;
    const int shot = control.ShotIndex();
    switch (enemy.GetBehavior()) {
    case Enemy::Behavior::Sniper:
        volley.shots[0].style = Style::Sniper;
        break;
    case Enemy::Behavior::Shield: {
        // 二段の扇。中央は狙うが、外側と上下には逃げ道を残す。
        const float y = shot == 0 ? 0.0f : (control.aimPoint.y > 2.4f ? -1.8f : 1.8f);
        for (size_t index = 0; index < volley.shots.size(); ++index) {
            volley.shots[index] = { Style::ShieldOrb, { (static_cast<float>(index) - 1.0f) * 3.4f, y } };
        }
        volley.count = volley.shots.size();
        break;
    }
    case Enemy::Behavior::Crossfire: {
        // 左右の敵が内側へ射線を寄せる。端か上下へ抜ければ回避できる。
        const float side = volley.muzzle.x < 0.0f ? -1.0f : 1.0f;
        volley.shots[0] = { Style::Crossfire, { side * (2.6f - static_cast<float>(shot) * 1.3f), 0.0f } };
        break;
    }
    case Enemy::Behavior::StrafeShooter: {
        const float sweep = control.VolleyIndex() % 2 == 0 ? -1.0f : 1.0f;
        volley.shots[0] = { Style::Crossfire, { sweep * (static_cast<float>(shot) - 1.0f) * 2.8f, 0.0f } };
        break;
    }
    case Enemy::Behavior::DiveBomber:
        volley.shots[0].aimOffset.y = (1.0f - static_cast<float>(shot)) * 1.5f;
        break;
    default:
        break;
    }
    return volley;
}
