#pragma once

#include "app/combat/ProjectileSystem.h"

#include <array>
#include <cstddef>
#include <optional>

class Enemy;

// 機種ごとの射撃間隔と狙いを決める　発射の要求を返し、弾の生成は呼び出し側が行う
class EnemyAttackPatterns {
public:
    struct Shot {
        ProjectileSystem::EnemyStyle style = ProjectileSystem::EnemyStyle::Standard;
        Math::Vector2 aimOffset{};
    };
    struct Volley {
        Math::Vector3 muzzle;
        Math::Vector3 aimPoint;
        std::array<Shot, 3> shots{};
        size_t count = 1;
    };

    void Reset() { playerAimVelocity_ = {}; }
    void TrackPlayerMovement(const Math::Vector3& before, const Math::Vector3& after,
        float frameStep, bool dodging); // frameStepは60fps換算で進めるフレーム数　dodgingは回避中か
    std::optional<Volley> Update(Enemy& enemy, const Math::Vector3& playerPosition,
        float worldStep, int shotInterval, bool hasSupportDrone, bool visible) const;

private:
    Math::Vector2 playerAimVelocity_{}; // 回避中の急加速は予測照準に使わない
};
