#pragma once

#include "app/combat/CombatEffects.h"
#include "engine/base/Math.h"

#include <list>
#include <memory>

class BossAttackController;
class CombatProgress;
class Enemy;
class FeverState;
class Player;
class PlayerSkill;
class ProjectileSystem;
class TutorialProgress;

// 弾の接触、ダメージ、撃破の報酬を命中順に反映する。
class CombatResolver {
public:
    class Reactions {
    public:
        virtual void PlaySfx(const char* key, float pitch = 1.0f) = 0;
        virtual void AddCameraShake(float power, int duration) = 0;
        virtual void TriggerHitConfirm(const Math::Vector3& position, bool charged, bool boss, bool destroyed) = 0;
        virtual void TriggerPlayerImpactMoment(bool charged, bool boss, bool destroyed) = 0;
        virtual void TriggerPlayerDamageFeedback(const Math::Vector3& position, const Math::Vector3& velocity) = 0;
        virtual void RewardJustDodge(const Math::Vector3& bulletPosition) = 0;
        virtual void BeginPlayerDefeat(const Math::Vector3& incomingVelocity) = 0;
        virtual void OnBossDefeated(const Math::Vector3& position) = 0;

    protected:
        ~Reactions() = default;
    };

    // 各更新の間だけ借りる。勝敗は命中の途中でも変わるので現在値を参照する。
    struct Context {
        const std::list<std::unique_ptr<Enemy>>& enemies;
        ProjectileSystem& projectiles;
        CombatProgress& progress;
        CombatEffects& effects;
        FeverState& fever;
        PlayerSkill& skill;
        const BossAttackController& bossAttack;
        Player* player;
        TutorialProgress* tutorial;
        const bool& gameOver;
        const bool& gameClear;
        Reactions& reactions;
    };

    struct Totals {
        int directHits = 0;
        int playerDamage = 0;
        int defeated = 0;
    };

    void Reset(int defeated = 0);
    const Totals& Stats() const { return totals_; }

    void ResolvePlayerShots(const Context& battle);
    void ResolveEnemyShots(const Context& battle);
    bool ApplyShotDamage(const Context& battle, Enemy& enemy, const Math::Vector3& impact,
        bool charged, bool fever, bool splash = false);
    void ApplySkillDamage(const Context& battle, Enemy& enemy, int damage);
    void RegisterEnemyDefeat(const Context& battle, const Enemy& enemy, bool charged, bool fever);
    static void RecoverSkillOnHit(const Context& battle, bool charged, bool destroyed);
    static CombatEffects::ChainStyle HitEffectChainStyle(const CombatProgress& progress);

#ifdef _DEBUG
    void SetPlayerDamageForProbe(int value) { totals_.playerDamage = value; }
    void SetDefeatedForProbe(int value) { totals_.defeated = value; }
#endif

private:
    void ApplyChargeSplash(const Context& battle, const Enemy& directTarget, const Math::Vector3& center);
    static void AddFeverGauge(const Context& battle, int amount);

    Totals totals_;
};
