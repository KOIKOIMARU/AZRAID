// ===== GameplayAim.cpp =====
// 照準位置を更新し、画面に投影した敵の位置から狙っている相手を選ぶ

#include "app/player/GameplayAim.h"

#include "app/enemy/Enemy.h"
#include "engine/3d/ScreenProjection.h"

#include <algorithm>
#include <cmath>

// 照準の座標を表示領域内に収める　エディタの小さい画面でも同じ領域を基準にする
void GameplayAim::UpdateReticle(Math::Vector2 cursor, const Math::Vector2& viewportMin,
    const Math::Vector2& viewportSize)
{
    reticle_ = {
        std::clamp(cursor.x, viewportMin.x, viewportMin.x + viewportSize.x),
        std::clamp(cursor.y, viewportMin.y, viewportMin.y + viewportSize.y)
    };
}

// 敵の中心と大きさを画面座標に換算し、照準からの距離で候補を選ぶ
void GameplayAim::SelectTarget(const std::list<std::unique_ptr<Enemy>>& enemies,
    const Math::Matrix4x4& viewProjection, const Math::Vector2& viewportMin, const Math::Vector2& viewportSize)
{
    ClearTarget();
    const auto project = [&](const Math::Vector3& world, Math::Vector2& screen) {
        return ScreenProjection::ToViewport(world, viewProjection, viewportMin, viewportSize, screen);
    };

    // 照準に最も近い敵を候補にする　実際に狙えているかは画面上での機体の大きさで判定する
    const float lockRadius = (std::max)(lockRadius_, 1.0f);
    constexpr float kReticleHitRadius = 24.0f;
    float bestScore = lockRadius * lockRadius;

    for (const auto& enemy : enemies) {
        if (!enemy || enemy->IsDead() || !enemy->IsTargetable()) {
            continue;
        }

        Math::Vector2 screenPosition{};
        const Math::Vector3 enemyPosition = enemy->GetAimPosition();
        if (!project(enemyPosition, screenPosition)) {
            continue;
        }

        float projectedEnemyRadius = 0.0f;
        const float enemyRadius = enemy->GetAimRadius();
        Math::Vector2 edgeScreen{};
        if (project(
                { enemyPosition.x + enemyRadius, enemyPosition.y, enemyPosition.z },
                edgeScreen)) {
            const float dx = edgeScreen.x - screenPosition.x;
            const float dy = edgeScreen.y - screenPosition.y;
            projectedEnemyRadius =
                (std::max)(projectedEnemyRadius, std::sqrt(dx * dx + dy * dy));
        }
        if (project(
                { enemyPosition.x, enemyPosition.y + enemyRadius, enemyPosition.z },
                edgeScreen)) {
            const float dx = edgeScreen.x - screenPosition.x;
            const float dy = edgeScreen.y - screenPosition.y;
            projectedEnemyRadius =
                (std::max)(projectedEnemyRadius, std::sqrt(dx * dx + dy * dy));
        }
        const float targetHitRadius =
            std::clamp(projectedEnemyRadius * 0.82f + 10.0f, kReticleHitRadius, 72.0f);
        const float targetHitRadiusSq = targetHitRadius * targetHitRadius;

        const float dx = screenPosition.x - reticle_.x;
        const float dy = screenPosition.y - reticle_.y;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq < bestScore) {
            bestScore = distanceSq;
            target_ = enemy.get();
            onTarget_ = distanceSq <= targetHitRadiusSq;
        }
    }
}

// 現在選んでいる敵と命中表示用の情報を消し、次の照準判定に備える
void GameplayAim::ClearTarget()
{
    target_ = nullptr;
    onTarget_ = false;
}

// 現在の照準が退場する敵を指していれば解除する
void GameplayAim::InvalidateTarget(const Enemy* removed)
{
    if (target_ == removed) { ClearTarget(); }
}
