// ===== StageEnemyFactory.cpp =====
// 敵の種類に合ったモデルと外見を選び、ステージに出す敵を生成する

#include "app/enemy/StageEnemyFactory.h"

#include "app/enemy/Enemy.h"
#include "app/runtime/GameResources.h"
#include "engine/3d/ModelManager.h"

// 読込済みのモデルから機種ごとの見た目を準備し、モデル欠落時の代替も決める
void StageEnemyFactory::Prepare(ModelManager& models)
{
    using namespace GameResources;
    fallback_.model = models.FindModel(kFreeEnemyModelPath);
    if (!fallback_.model) {
        fallback_.model = models.FindModel("game_enemy");
    }
    formation_ = { models.FindModel(kEnemyFormationModelPath), kEnemyFormationTexturePath };
    swoop_ = { models.FindModel(kEnemySwoopModelPath), kEnemySwoopTexturePath };
    shooter_ = { models.FindModel(kEnemyShooterModelPath), kEnemyShooterTexturePath };
    heavy_ = { models.FindModel(kEnemyHeavyModelPath), kEnemyHeavyTexturePath };
    auto* bossModel = models.FindModel(kBossModelPath);
    boss_ = { bossModel ? bossModel : fallback_.model, kBossTexturePath };
}

// 機種に対応するモデルとテクスチャを選び、指定位置・HP・スケールで敵を初期化する　モデルがなければnullptrを返す
std::unique_ptr<Enemy> StageEnemyFactory::Create(Object3dCommon& common, const Math::Vector3& position,
    EnemyBehavior behavior, EnemyEntryStyle entryStyle, int maxHp, float scale) const
{
    const auto appearance = SelectAppearance(behavior, maxHp);
    if (!appearance.model) { return nullptr; }

    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(&common, appearance.model, position, behavior, entryStyle, maxHp, scale, appearance.texture);
    return enemy;
}

// 敵の機種と設定HPに応じて、モデルとテクスチャの組み合わせを選ぶ
StageEnemyFactory::Appearance StageEnemyFactory::SelectAppearance(EnemyBehavior behavior, int maxHp) const
{
    // 通常機の代替モデルには、そのモデルに対応するテクスチャを使う
    switch (behavior) {
    case EnemyBehavior::Boss:
        return boss_;
    case EnemyBehavior::StrafeShooter:
        if (maxHp >= 7 && heavy_.model) { return heavy_; }
        [[fallthrough]];
    case EnemyBehavior::Sniper:
    case EnemyBehavior::Crossfire:
        return shooter_.model ? shooter_ : fallback_;
    case EnemyBehavior::DiveBomber:
    case EnemyBehavior::Swoop:
        return swoop_.model ? swoop_ : fallback_;
    case EnemyBehavior::Shield:
        if (heavy_.model) { return heavy_; }
        [[fallthrough]];
    case EnemyBehavior::Support:
    case EnemyBehavior::Formation:
    default:
        return formation_.model ? formation_ : fallback_;
    }
}
