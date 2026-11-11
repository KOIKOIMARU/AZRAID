// ===== StageEnemies.cpp =====
// ステージ内の敵を生成・更新し、退場時には照準などからの参照を解除する。

#include "app/enemy/StageEnemies.h"

#include "app/enemy/BossAttackController.h"
#include "app/enemy/Enemy.h"
#include "app/player/GameplayAim.h"
#include "app/player/PlayerSkill.h"
#include "app/combat/ProjectileSystem.h"

#include <algorithm>
#include <utility>

namespace {
constexpr int kBossMaxHp = 220;
}

StageEnemies::StageEnemies() = default;
StageEnemies::~StageEnemies() = default;

// 敵の生成に使う描画共通機能とモデル群を受け取る。
void StageEnemies::Prepare(Object3dCommon& common, ModelManager& models)
{
    common_ = &common;
    factory_.Prepare(models);
}

// 各敵を追っている照準・弾・スキルの参照を外し、敵とボスの状態を初期化する。
void StageEnemies::Reset(const TargetReferences& targets)
{
    for (const auto& enemy : enemies_) { InvalidateTargets(enemy.get(), targets); }
    enemies_.clear();
    escapedCount_ = 0;
    bossSpawned_ = false;
    bossDefeated_ = false;
}

// 敵の参照と一覧を解除し、シーン終了時には生成用の素材参照も外す。
void StageEnemies::Clear(const TargetReferences& targets)
{
    Reset(targets);
    factory_ = {};
    common_ = nullptr;
}

// 指定した機種を現在の敵リストへ登録する。準備不足などで生成できなければnullptrを返す。
Enemy* StageEnemies::Spawn(const Math::Vector3& position, EnemyBehavior behavior, EnemyEntryStyle entryStyle,
    int hp, float scale)
{
    if (!common_) { return nullptr; }

    auto enemy = factory_.Create(*common_, position, behavior, entryStyle, hp, scale);
    if (!enemy) { return nullptr; }
    enemies_.push_back(std::move(enemy));
    return enemies_.back().get();
}

// 配置表の左右・上下位置と自機の前方距離から、敵の出現座標を決める。
void StageEnemies::SpawnScheduled(const MainStage::EnemySpawn& spawn, float railDistance)
{
    Spawn({ spawn.x, spawn.y, railDistance + spawn.leadDistance }, spawn.behavior,
        spawn.entryStyle, spawn.maxHpOverride, spawn.scaleMultiplier);
}

// ボスが出ていないときだけ生成し、成功後に登場済みとして記録する。
bool StageEnemies::SpawnBoss(float railDistance)
{
    if (bossSpawned_ || bossDefeated_) { return false; }
    if (!Spawn({ 0.0f, 2.05f, railDistance + 98.0f }, EnemyBehavior::Boss,
        EnemyEntryStyle::Direct, kBossMaxHp, 1.0f)) { return false; }
    bossSpawned_ = true;
    return true;
}

// 練習用の標的を中央から左右対称に並べ、課題中にその場へ残る設定にする。
void StageEnemies::SpawnTrainingTargets(float railDistance, int count, int hp, float spacing)
{
    for (int index = 0; index < count; ++index) {
        const float x = (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f) * spacing;
        if (auto* target = Spawn({ x, 1.4f, railDistance + 42.0f }, EnemyBehavior::Formation,
            EnemyEntryStyle::TightFormation, hp, 1.2f)) {
            target->SetTrainingTarget(true);
        }
    }
}

// 管理中の敵を撃破状態にする。リストからの取り除きは通常の更新で行う。
void StageEnemies::KillAll()
{
    for (const auto& enemy : enemies_) { enemy->Kill(); }
}

// 各機の攻撃予告と飛行を更新する。撃破・逃走した敵は参照を解除してから所有リストから外す。
bool StageEnemies::Update(const Frame& frame, const TargetReferences& targets)
{
    const int escapedBefore = escapedCount_;
    const auto& bossAttack = frame.bossAttack;
    const float bossRecoveryRate = static_cast<float>(bossAttack.CounterFrames()) /
        static_cast<float>((std::max)(bossAttack.CounterDuration(), 1));
    bool trainingTelegraphAssigned = false;
    for (auto iterator = enemies_.begin(); iterator != enemies_.end();) {
        auto& enemy = **iterator;
        float attackTelegraphRate = frame.tutorial ? 0.0f : enemy.GetFireControl().ChargeRate();
        if (enemy.IsBoss() && bossAttack.IsWindingUp() && bossAttack.RemainingFrames() > 0) {
            attackTelegraphRate = 1.0f - std::clamp(
                static_cast<float>(bossAttack.RemainingFrames()) /
                static_cast<float>((std::max)(bossAttack.WindupFrames(), 1)), 0.0f, 1.0f);
        } else if (frame.tutorial && !trainingTelegraphAssigned && frame.trainingTelegraphActive &&
            enemy.IsSniper() && enemy.CanShoot()) {
            attackTelegraphRate = frame.trainingChargeRate;
            trainingTelegraphAssigned = true;
        }
        enemy.SetAttackTelegraphRate(attackTelegraphRate);
        enemy.SetBossRecoveryRate(bossRecoveryRate);
        if (enemy.IsBoss()) { enemy.SetBossFlightOffset(bossAttack.FlightOffset()); }
        enemy.Update(frame.railDistance, frame.worldTimeScale);
        if (!enemy.IsDead()) {
            ++iterator;
            continue;
        }

        if (enemy.HasEscaped()) { ++escapedCount_; }
        InvalidateTargets(&enemy, targets);
        iterator = enemies_.erase(iterator);
    }
    return escapedCount_ != escapedBefore;
}

// 敵を破棄する前に、スキル・追尾弾・照準が持つ参照をすべて解除する。
void StageEnemies::InvalidateTargets(const Enemy* enemy, const TargetReferences& targets)
{
    targets.skill.InvalidateTarget(enemy);
    targets.projectiles.InvalidateTarget(enemy);
    targets.aim.InvalidateTarget(enemy);
}

// 自機撃墜時は全機の射撃を止めて飛び去らせる。リザルトまでは所有を保つ。
void StageEnemies::UpdateDeparture(float deltaSeconds)
{
    for (const auto& enemy : enemies_) { enemy->UpdateDeparture(deltaSeconds); }
}

void StageEnemies::Draw() const
{
    for (const auto& enemy : enemies_) { enemy->Draw(); }
}

// 生存中の通常敵の数と攻撃の負荷を集計し、次の編隊を出せるか判断する材料を返す。
StageDirector::EnemyLoad StageEnemies::Load() const
{
    StageDirector::EnemyLoad load;
    for (const auto& enemy : enemies_) {
        if (!enemy->IsDead() && !enemy->IsBoss()) {
            ++load.count;
            load.pressure += StageDirector::EnemyPressure(enemy->GetBehavior());
        }
    }
    return load;
}

bool StageEnemies::HasLiving() const
{
    return std::any_of(enemies_.begin(), enemies_.end(), [](const auto& enemy) { return !enemy->IsDead(); });
}

bool StageEnemies::HasTargetableNormal() const
{
    return std::any_of(enemies_.begin(), enemies_.end(), [](const auto& enemy) {
        return !enemy->IsDead() && !enemy->IsBoss() && enemy->IsTargetable();
    });
}

bool StageEnemies::HasSupport() const
{
    return std::any_of(enemies_.begin(), enemies_.end(), [](const auto& enemy) {
        return !enemy->IsDead() && enemy->IsSupport();
    });
}

// 渡された敵がまだ一覧にいて狙えるかを確認し、有効なときだけそのポインタを返す。
Enemy* StageEnemies::FindTargetable(const Enemy* target) const
{
    for (const auto& enemy : enemies_) {
        if (enemy.get() == target && !enemy->IsDead() && enemy->IsTargetable()) {
            return enemy.get();
        }
    }
    return nullptr;
}

// 現在生存しているボスを返す。登場前や撃破後はnullptr。
const Enemy* StageEnemies::Boss() const
{
    for (const auto& enemy : enemies_) {
        if (enemy->IsBoss() && !enemy->IsDead()) { return enemy.get(); }
    }
    return nullptr;
}

#ifdef _DEBUG
Enemy* StageEnemies::SpawnForProbe(const Math::Vector3& position, EnemyBehavior behavior, EnemyEntryStyle entryStyle,
    int hp, float scale)
{
    return Spawn(position, behavior, entryStyle, hp, scale);
}
#endif
