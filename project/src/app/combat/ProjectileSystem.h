// 自機弾と敵弾の準備・発射・更新・描画をまとめる。命中結果はCombatResolverが処理する。

#pragma once

#include "app/combat/BulletPool.h"

#include <unordered_map>
#include <unordered_set>

class Enemy;
class Model;
class Object3dCommon;

class ProjectileSystem {
public:
    enum class EnemyStyle { Standard, Crossfire, Sniper, ShieldOrb, BossCannon, BossCharge };

    struct Assets {
        Model* body = nullptr;
        Model* chargeBody = nullptr;
        Model* bulletGlow = nullptr;
        Model* fallbackGlow = nullptr;
        Model* playerCore = nullptr;
        Model* playerTrail = nullptr;
        Model* chargeCore = nullptr;
        Model* chargeTrail = nullptr;
        Model* enemyCore = nullptr;
        Model* enemyTrail = nullptr;
    };

    struct PlayerShot {
        Math::Vector3 origin;
        Math::Vector3 aimDirection;
        const Enemy* target;
        float normalSpeed;
        float chargedSpeed;
        float chargeMultiplier;
        float railSpeed;
        float clock;
        bool tutorial;
        bool charged;
        bool fever;
    };

    struct EnemyShot {
        Math::Vector3 origin;
        EnemyStyle style;
        Math::Vector2 aimOffset;
        const Math::Vector3* fixedAim;
        const Math::Vector3* playerPosition;
        float speed;
        float railSpeed;
        bool tutorial;
    };

    void Initialize(Object3dCommon* common, const Assets& assets);
    void Clear();
    void PreparePlayerBullet(float speed);
    void PrepareEnemyBullet(float speed);

    Bullet* FirePlayer(const PlayerShot& shot);
    Bullet* FireEnemy(const EnemyShot& shot);
    void UpdatePlayerBullets();
    void UpdateEnemyBullets(float worldTimeScale);
    void InvalidateTarget(const Enemy* enemy);
    bool WasDodged(const Bullet& bullet) const { return dodgedBullets_.contains(&bullet); }
    void MarkDodged(const Bullet& bullet) { dodgedBullets_.insert(&bullet); }

    void KillAll();
    void RecycleAll();
    void RecycleEnemyBullets();
    void DrawBodies(const Math::Matrix4x4* cameraWorld);
    void DrawGlows(const Math::Matrix4x4& cameraWorld);

    static Math::Vector3 AimDirection(const Math::Vector3& origin, const Math::Matrix4x4& viewProjection,
        const Math::Vector2& reticle, const Math::Vector2& viewportMin, const Math::Vector2& viewportSize,
        float aimDepth);

    const BulletPool& PlayerPool() const { return playerPool_; }
    const BulletPool& EnemyPool() const { return enemyPool_; }

#ifdef _DEBUG
    Bullet* AcquirePlayerForDebug() { return playerPool_.Acquire(); }
    Bullet* AcquireEnemyForDebug() { return enemyPool_.Acquire(); }
    Model* BodyModelForDebug() const { return assets_.body; }
    void SetHomingTargetForDebug(const Bullet& bullet, const Enemy* enemy) { homingTargets_[&bullet] = enemy; }
    bool HasHomingTargetForProbe(const Bullet& bullet) const { return homingTargets_.contains(&bullet); }
    bool HasTrackingForDebug() const { return !homingTargets_.empty() || !dodgedBullets_.empty(); }
#endif

private:
    Object3dCommon* common_ = nullptr;
    Assets assets_{};
    BulletPool playerPool_;
    BulletPool enemyPool_;
    std::unordered_map<const Bullet*, const Enemy*> homingTargets_;
    std::unordered_set<const Bullet*> dodgedBullets_;
};
