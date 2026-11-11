// ===== CombatResolver.cpp =====
// 弾や機体の当たり判定を行い、命中・撃破に応じてダメージや報酬を反映する。

#include "app/combat/CombatResolver.h"

#include "app/enemy/BossAttackController.h"
#include "app/combat/CombatProgress.h"
#include "app/enemy/Enemy.h"
#include "app/combat/FeverState.h"
#include "app/runtime/GameRuntimeMath.h"
#include "app/runtime/GameRuntimeTuning.h"
#include "app/player/Player.h"
#include "app/player/PlayerSkill.h"
#include "app/combat/ProjectileSystem.h"
#include "app/stage/TutorialProgress.h"

#include <algorithm>
#include <limits>

namespace {
using namespace GameRuntimeMath;
using namespace GameRuntimeTuning;
constexpr float kChargedShotRadius = 0.8f;
}

// 命中・被弾・撃破の集計を初期化し、次のプレイの計測に備える。
void CombatResolver::Reset(int defeated)
{
    totals_ = { .defeated = defeated };
}

// 弾と敵を球として扱い、中心距離の二乗を半径の和の二乗と比較する。平方根を求めずに重なりを判定できる。
void CombatResolver::ResolvePlayerShots(const Context& battle)
{
    for (auto& bullet : battle.projectiles.PlayerPool().Active()) {
        if (bullet->IsDead()) {
            continue;
        }

        // 大きいチャージ弾が複数機に同時接触したとき、リストの先頭ではなく
        // 弾の中心に最も近い機体で爆発させる。編隊中央を狙う操作を尊重する。
        const Enemy* chargeContact = nullptr;
        if (bullet->GetRadius() >= kChargedShotRadius && !bullet->IsFeverShot()) {
            float closestDistance = (std::numeric_limits<float>::max)();
            for (const auto& candidate : battle.enemies) {
                if (candidate->IsDead() || !candidate->IsTargetable()) { continue; }
                const float distance = DistanceSquared(bullet->GetTranslate(), candidate->GetAimPosition());
                const float contactRadius = bullet->GetRadius() + candidate->GetAimRadius();
                if (distance <= contactRadius * contactRadius && distance < closestDistance) {
                    closestDistance = distance;
                    chargeContact = candidate.get();
                }
            }
        }
        for (auto& enemy : battle.enemies) {
            if (chargeContact && enemy.get() != chargeContact) { continue; }
            if (enemy->IsDead() || !enemy->IsTargetable()) {
                continue;
            }

            const Math::Vector3 enemyAimPosition = enemy->GetAimPosition();
            const float radius = bullet->GetRadius() + enemy->GetAimRadius();
            if (DistanceSquared(
                bullet->GetTranslate(),
                enemyAimPosition) <= radius * radius) {
                if (!bullet->TryRegisterHitTarget(enemy.get())) {
                    continue;
                }
                const bool isChargedHit = bullet->GetRadius() >= kChargedShotRadius;
                const bool isFeverHit = bullet->IsFeverShot();
                const bool isBossHit = enemy->IsBoss();
                const Math::Vector3 bulletImpactPosition = bullet->GetTranslate();
                const float centerEffectRate = isBossHit ? 0.68f : 0.42f;
                const Math::Vector3 visibleImpactPosition{
                    Lerp(bulletImpactPosition.x, enemyAimPosition.x, centerEffectRate),
                    Lerp(bulletImpactPosition.y, enemyAimPosition.y, centerEffectRate),
                    Lerp(bulletImpactPosition.z, enemyAimPosition.z, centerEffectRate)
                };
                bullet->RegisterHit();
                ++totals_.directHits;
                ApplyShotDamage(battle, *enemy, visibleImpactPosition, isChargedHit, isFeverHit);
                if (isChargedHit && !isFeverHit) {
                    // 弾の手前側ではなく命中した機体を中心に一度だけ広げる。
                    ApplyChargeSplash(battle, *enemy, enemyAimPosition);
                }
                if (bullet->IsDead()) {
                    break;
                }
            }
        }
    }
}

// 直撃・爆風・フィーバーでダメージを選び、反撃の隙が開いていれば倍率を適用する。
bool CombatResolver::ApplyShotDamage(const Context& battle, Enemy& enemy, const Math::Vector3& impact,
    bool charged, bool fever, bool splash)
{
    if (enemy.IsDead() || !enemy.IsTargetable()) { return false; }
    const bool boss = enemy.IsBoss();
    int baseDamage = kNormalShotDamage;
    if (splash) {
        baseDamage = kChargeSplashDamage;
    } else if (fever) {
        baseDamage = kFeverShotDamage;
    } else if (charged) {
        baseDamage = boss ? kChargeBossDamage : kChargeDirectDamage;
    }
    const bool sniperCounter = !splash && charged && !fever && enemy.IsChargeCounterOpen();
    const int damage = (boss && battle.bossAttack.IsCounterOpen()) || sniperCounter ? baseDamage * 2 : baseDamage;
    if (fever) {
        battle.effects.EnemyImpact(impact, boss ? 1.75f : 1.38f, true);
    } else {
        battle.effects.EnemyImpact(impact, boss ? 1.85f : (charged ? 1.42f : 1.20f));
    }
    const bool destroyed = enemy.Damage(damage);
    if (battle.tutorial && charged && !splash) { battle.tutorial->RecordChargedHit(); }

    // 巻き込みの人数やフィーバーの貫通連射で、スキルを即座に再使用させない。
    if (!splash && !fever) { RecoverSkillOnHit(battle, charged, destroyed); }
    if (destroyed) { AddFeverGauge(battle, boss ? 25 : splash ? 4 : charged ? 9 : 7); }

    // 音・照準表示・画面揺れは直撃で一回。巻き込み数ぶん画面を揺らさない。
    if (!splash) {
        battle.reactions.PlaySfx(destroyed ? "destroy" : "hit");
        battle.reactions.TriggerHitConfirm(impact, charged, boss, destroyed);
        if (!fever || boss || destroyed) {
            battle.reactions.TriggerPlayerImpactMoment(charged, boss, destroyed);
        }
    }
    if (destroyed) { RegisterEnemyDefeat(battle, enemy, charged, fever); }
    return destroyed;
}

// スキルが指定した敵へダメージを適用する。ボスが反撃可能なら倍率をかけ、撃破した場合は共通の得点処理へ渡す。
void CombatResolver::ApplySkillDamage(const Context& battle, Enemy& enemy, int damage)
{
    if (enemy.IsDead()) { return; }
    const bool boss = enemy.IsBoss();
    const auto position = enemy.GetAimPosition();
    const bool destroyed = enemy.Damage(boss && battle.bossAttack.IsCounterOpen() ? damage * 2 : damage);
    battle.reactions.TriggerHitConfirm(position, true, boss, destroyed);
    if (destroyed) {
        AddFeverGauge(battle, 6);
        battle.skill.RecordDefeat();
        battle.reactions.PlaySfx("destroy");
        RegisterEnemyDefeat(battle, enemy, true, battle.skill.IsEmpowered());
    } else {
        battle.effects.EnemyImpact(position, boss ? 1.35f : 0.80f);
    }
}

// 生存中の自機のスキル待ち時間を射撃の命中で短縮し、再使用可能になった瞬間に効果音を出す。
void CombatResolver::RecoverSkillOnHit(const Context& battle, bool charged, bool destroyed)
{
    if (!battle.player || battle.player->IsDead() || battle.gameOver || battle.gameClear) { return; }
    battle.skill.RecoverOnHit(charged, destroyed);
    if (battle.skill.TryRecover()) { battle.reactions.PlaySfx("skill_ready"); }
}

// 勝敗がつく前の戦闘だけ、命中や撃破によるゲージを加算する。
void CombatResolver::AddFeverGauge(const Context& battle, int amount)
{
    if (!battle.gameOver && !battle.gameClear) { battle.fever.AddGauge(amount); }
}

// 現在のチェイン数と、何段階目の撃破演出を使うかをまとめて返す。
CombatEffects::ChainStyle CombatResolver::HitEffectChainStyle(const CombatProgress& progress)
{
    const int count = progress.ChainCount();
    return { count, count / CombatProgress::kChainScoreTierSize };
}

// 撃破数・チェイン・得点を反映して演出を出す。ボスの場合はシーンにクリアを通知する。
void CombatResolver::RegisterEnemyDefeat(const Context& battle, const Enemy& enemy, bool charged, bool fever)
{
    const bool boss = enemy.IsBoss();
    const auto position = enemy.GetAimPosition();
    battle.progress.RegisterDefeat();
    const float strength = fever ? (boss ? 1.72f : 1.08f) : (boss ? 2.0f : (charged ? 1.32f : 1.0f));
    battle.effects.EnemyDestroyed(position, strength, HitEffectChainStyle(battle.progress), fever);

    // 弾とスキルで撃破報酬を共有し、ボスの撃破だけをシーンへ知らせる。
    battle.progress.AddScore(boss ? 1850 : (charged ? 250 : 200), battle.fever.ScoreMultiplier());
    ++totals_.defeated;
    if (boss) { battle.reactions.OnBossDefeated(position); }
}

// 直撃した敵を中心に、範囲内の別の敵へ爆風ダメージを適用する。直撃対象の二重被弾を防ぐ。
void CombatResolver::ApplyChargeSplash(const Context& battle, const Enemy& directTarget, const Math::Vector3& center)
{
    battle.effects.ChargeSplash(center, kChargeSplashRadius);

    bool destroyedAny = false;
    for (auto& nearby : battle.enemies) {
        if (nearby.get() == &directTarget || nearby->IsDead() || !nearby->IsTargetable()) { continue; }
        const auto position = nearby->GetAimPosition();
        // ワールド空間の球判定。画面上で重なるだけの遠い敵には届かない。
        if (DistanceSquared(center, position) > kChargeSplashRadius * kChargeSplashRadius) { continue; }
        destroyedAny |= ApplyShotDamage(battle, *nearby, position, true, false, true);
    }
    if (destroyedAny) { battle.reactions.TriggerPlayerImpactMoment(true, false, true); }
}

// 回避中は弾との距離でジャスト回避と接触を区別する。通常の被弾ではHP・チェイン・敗北状態を更新する。
void CombatResolver::ResolveEnemyShots(const Context& battle)
{
    Player* player = battle.player;
    if (!player || player->IsDead()) {
        return;
    }
    // 技の攻撃区間だけ保護。ここではジャスト回避を発生させず、自己再充填を防ぐ。
    if (battle.skill.ProtectsPlayer()) {
        return;
    }

    for (auto& bullet : battle.projectiles.EnemyPool().Active()) {
        if (bullet->IsDead()) {
            continue;
        }

        const float radius = bullet->GetRadius() + player->GetRadius();
        const Math::Vector3 bulletPosition = bullet->GetTranslate();
        const float distanceSq = DistanceSquared(
            bulletPosition,
            player->GetTranslate());
        if (player->IsDodging()) {
            const float justDodgeRadius = radius + kJustDodgeGrazePadding;
            if (distanceSq > radius * radius &&
                distanceSq <= justDodgeRadius * justDodgeRadius) {
                if (!battle.projectiles.WasDodged(*bullet) && player->TryConfirmJustDodge()) {
                    battle.projectiles.MarkDodged(*bullet);
                    battle.reactions.RewardJustDodge(bulletPosition);
                }
                continue;
            }
        }
        if (distanceSq <= radius * radius) {
            if (player->IsDodging()) {
                bullet->Kill();
                battle.effects.DodgeGraze(bulletPosition);
                battle.reactions.AddCameraShake(0.018f, 4);
                continue;
            }
            // Player側の無敵フラグも尊重し、保護中の弾でチェインを切らない。
            if (player->IsInvincible()) {
                bullet->Kill();
                continue;
            }
            const Math::Vector3 incomingVelocity = bullet->GetVelocity();
            const int incomingDamage = bullet->GetDamage();
            bullet->Kill();
            ++totals_.playerDamage;
            battle.reactions.PlaySfx("damage");
            battle.progress.BreakChain();
            const int damage = battle.fever.IsActive() ? (std::max)(1, (incomingDamage * 3 + 3) / 4) : incomingDamage;
            // 練習では被弾の反応は残し、HP1で止めて何度でも試せるようにする。
            player->Damage(battle.tutorial ? (std::min)(damage, (std::max)(0, player->GetHp() - 1)) : damage);
            if (!battle.fever.IsActive()) {
                battle.fever.RemoveGauge(20);
            }
            battle.effects.PlayerDamaged(player->GetTranslate());
            battle.reactions.TriggerPlayerDamageFeedback(
                player->GetTranslate(),
                incomingVelocity);
            battle.reactions.AddCameraShake(0.2f, 18);
            if (player->IsDead() && !battle.gameOver) {
                battle.reactions.BeginPlayerDefeat(incomingVelocity);
            }
            break;
        }
    }
}
