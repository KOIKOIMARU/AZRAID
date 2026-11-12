#pragma once

#include "engine/base/Math.h"

#include <list>
#include <memory>

class Enemy;

class GameplayAim {
public:
    static constexpr float kDefaultLockRadius = 118.0f;

    void UpdateReticle(Math::Vector2 cursor, const Math::Vector2& viewportMin, const Math::Vector2& viewportSize);
    void SelectTarget(const std::list<std::unique_ptr<Enemy>>& enemies, const Math::Matrix4x4& viewProjection,
        const Math::Vector2& viewportMin, const Math::Vector2& viewportSize);
    void ClearTarget();
    void InvalidateTarget(const Enemy* removed);
    const Enemy* Target() const { return target_; }
    bool IsReticleOnTarget() const { return onTarget_; }
    const Math::Vector2& Reticle() const { return reticle_; }
    float GetLockRadius() const { return lockRadius_; }
    void SetLockRadius(float radius) { lockRadius_ = radius; }

#ifdef _DEBUG
    void SetReticleForProbe(Math::Vector2 position) { reticle_ = position; }
    void SetTargetForProbe(const Enemy& target) { target_ = &target; onTarget_ = true; }
#endif

private:
    // 敵を所有するStageEnemiesが、敵を破棄する前に参照を外す
    const Enemy* target_ = nullptr;
    Math::Vector2 reticle_{};
    float lockRadius_ = kDefaultLockRadius;
    bool onTarget_ = false;
};
