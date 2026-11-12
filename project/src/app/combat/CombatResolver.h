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

// 弾の接触、ダメージ、撃破の報酬を命中順に反映する
class CombatResolver {
public:
    class Reactions {
    public:
        // keyは効果音の識別名　pitchは再生周波数の倍率
        virtual void PlaySfx(const char* key, float pitch = 1.0f) = 0;
        // powerは揺れの強さ　durationは続けるフレーム数
        virtual void AddCameraShake(float power, int duration) = 0;
        // 命中位置と結果を通知し、HUDに命中の反応を出す
        // positionは命中位置　chargedはチャージ攻撃か　bossはボスに当たったか
        virtual void TriggerHitConfirm(const Math::Vector3& position, bool charged, bool boss, bool destroyed) = 0;
        // 自機の攻撃が当たったときの画面の揺れや時間変化を開始する
        // chargedはチャージ攻撃か　bossはボスに当たったか　destroyedは撃破したか
        virtual void TriggerPlayerImpactMoment(bool charged, bool boss, bool destroyed) = 0;
        // 自機の被弾を通知し、HUDに被弾の反応を出す
        // positionは被弾位置　velocityは当たった弾の速度ベクトル
        virtual void TriggerPlayerDamageFeedback(const Math::Vector3& position, const Math::Vector3& velocity) = 0;
        // bulletPositionは回避した弾の位置
        virtual void RewardJustDodge(const Math::Vector3& bulletPosition) = 0;
        // 自機の撃墜を通知し、敗北処理を開始する
        // incomingVelocityは自機を撃墜した弾の速度ベクトル
        virtual void BeginPlayerDefeat(const Math::Vector3& incomingVelocity) = 0;
        // ボス撃破をゲーム進行側へ通知する
        virtual void OnBossDefeated(const Math::Vector3& position) = 0;

    protected:
        ~Reactions() = default;
    };

    // 各更新の間だけ借りる　勝敗は命中の途中でも変わるので現在値を参照する
    struct Context {
        const std::list<std::unique_ptr<Enemy>>& enemies;
        ProjectileSystem& projectiles;
        CombatProgress& progress;
        CombatEffects& effects; // 命中や撃破の演出を登録する先
        FeverState& fever;
        PlayerSkill& skill;
        const BossAttackController& bossAttack; // ボスに反撃できる隙が開いているかを確認する先
        Player* player;
        TutorialProgress* tutorial;
        const bool& gameOver; // 判定の途中で敗北が確定したかを参照する
        const bool& gameClear; // 判定の途中でクリアが確定したかを参照する
        Reactions& reactions; // 命中や撃破をゲーム進行と演出へ伝える通知先
    };

    struct Totals {
        int directHits = 0;
        int playerDamage = 0; // 自機が受けたダメージの累計
        int defeated = 0; // 撃破した敵の数
    };

    void Reset(int defeated = 0);
    const Totals& Stats() const { return totals_; }

    void ResolvePlayerShots(const Context& battle);
    void ResolveEnemyShots(const Context& battle);
    bool ApplyShotDamage(const Context& battle, Enemy& enemy, const Math::Vector3& impact,
        bool charged, bool fever, bool splash = false);
    // スキルが指定した敵へダメージを適用する　ボスが反撃可能なら倍率をかけ、撃破した場合は共通の得点処理へ渡す
    void ApplySkillDamage(const Context& battle, Enemy& enemy, int damage);
    // 撃破数・チェイン・得点を反映して演出を出す　ボスの場合はシーンにクリアを通知する
    void RegisterEnemyDefeat(const Context& battle, const Enemy& enemy, bool charged, bool fever);
    static void RecoverSkillOnHit(const Context& battle, bool charged, bool destroyed);
    static CombatEffects::ChainStyle HitEffectChainStyle(const CombatProgress& progress);

#ifdef _DEBUG
    void SetPlayerDamageForProbe(int value) { totals_.playerDamage = value; }
    void SetDefeatedForProbe(int value) { totals_.defeated = value; }
#endif

private:
    // 直撃した敵を中心に、範囲内の別の敵へ爆風ダメージを適用する　直撃対象の二重被弾を防ぐ
    void ApplyChargeSplash(const Context& battle, const Enemy& directTarget, const Math::Vector3& center);
    static void AddFeverGauge(const Context& battle, int amount);

    Totals totals_;
};
