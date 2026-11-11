#pragma once

#include "app/stage/StageDirector.h"
#include "app/enemy/StageEnemyFactory.h"

#include <list>
#include <memory>

class BossAttackController;
class Enemy;
class GameplayAim;
class ModelManager;
class Object3dCommon;
class PlayerSkill;
class ProjectileSystem;

// ステージ内の敵機を所有し、生成・毎フレームの更新・退場時の参照解除を行う。
class StageEnemies {
public:
    using Actors = std::list<std::unique_ptr<Enemy>>;

    struct TargetReferences {
        PlayerSkill& skill;
        ProjectileSystem& projectiles;
        GameplayAim& aim;
    };
    struct Frame {
        float railDistance;
        float worldTimeScale;
        bool tutorial;
        bool trainingTelegraphActive;
        float trainingChargeRate;
        const BossAttackController& bossAttack;
    };

    StageEnemies();
    ~StageEnemies();
    void Prepare(Object3dCommon& common, ModelManager& models);
    void Reset(const TargetReferences& targets);
    void Clear(const TargetReferences& targets);

    void SpawnScheduled(const MainStage::EnemySpawn& spawn, float railDistance);
    bool SpawnBoss(float railDistance);
    void SpawnTrainingTargets(float railDistance, int count, int hp, float spacing);
    void KillAll();
    // 撤退があったフレームだけtrueを返す。自機撃墜時の演出退場はここに含めない。
    bool Update(const Frame& frame, const TargetReferences& targets);
    void UpdateDeparture(float deltaSeconds);
    void Draw() const;

    const Actors& Active() const { return enemies_; }
    StageDirector::EnemyLoad Load() const;
    bool HasLiving() const;
    bool HasTargetableNormal() const;
    bool HasSupport() const;
    Enemy* FindTargetable(const Enemy* target) const;
    const Enemy* Boss() const;
    bool BossSpawned() const { return bossSpawned_; }
    bool BossDefeated() const { return bossDefeated_; }
    void MarkBossDefeated() { bossDefeated_ = true; }
    int EscapedCount() const { return escapedCount_; }

#ifdef _DEBUG
    Enemy* SpawnForProbe(const Math::Vector3& position, EnemyBehavior behavior, EnemyEntryStyle entryStyle,
        int hp = 0, float scale = 1.0f);
    Model* ShooterModelForProbe() const { return factory_.ShooterModelForProbe(); }
    void ClearBossSpawnedForProbe() { bossSpawned_ = false; }
#endif

private:
    Enemy* Spawn(const Math::Vector3& position, EnemyBehavior behavior, EnemyEntryStyle entryStyle,
        int hp, float scale);
    static void InvalidateTargets(const Enemy* enemy, const TargetReferences& targets);

    Object3dCommon* common_ = nullptr;
    StageEnemyFactory factory_;
    Actors enemies_;
    int escapedCount_ = 0;
    bool bossSpawned_ = false;
    bool bossDefeated_ = false;
};
