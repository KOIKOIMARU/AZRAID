#pragma once

#include "engine/3d/Object3d.h"
#include "engine/base/Math.h"
#include "app/EnemyFireControl.h"

#include <memory>
#include <cmath>

class Model;
class Object3dCommon;

class Enemy {
public:
    enum class Behavior {
        Formation,
        Swoop,
        StrafeShooter,
        DiveBomber,
        Crossfire,
        Sniper,
        Shield,
        Support,
        Boss
    };

    enum class EntryStyle {
        Direct,
        VFormation,
        TightFormation, // 同時に入場し、同じ揺れで小型編隊を保つ。
        LeftSweep,
        RightSweep,
        PopShooter
    };

    enum class LifeState {
        Alive,
        Destroyed,
        Escaped
    };

    void Initialize(
        Object3dCommon* object3dCommon,
        Model* model,
        const Math::Vector3& position,
        Behavior behavior,
        EntryStyle entryStyle = EntryStyle::Direct,
        int maxHpOverride = 0,
        float scaleMultiplier = 1.0f,
        const char* textureOverride = nullptr);
    void Update(float railDistance, float timeScale = 1.0f);
    // 練習標的だけは入場後に滞在し、操作を試している間に退場しない。
    void SetTrainingTarget(bool enabled) { trainingTarget_ = enabled; }
    void SetRiftCarrier(bool enabled) { riftCarrier_ = enabled; }
    bool IsRiftCarrier() const { return riftCarrier_; }
    void Draw();
    void DrawShadow(const Math::Matrix4x4& lightViewProjection);

    void Kill();
    bool Damage(int damage);
    bool CanShoot() const;
    Behavior GetBehavior() const { return behavior_; }
    bool IsTightFormation() const { return behavior_ == Behavior::Formation && entryStyle_ == EntryStyle::TightFormation; }
    float GetFormationShotDelay() const { return std::abs(baseTranslate_.x) * 9.0f; } // 編隊の外側ほど遅く撃つ。
    EnemyFireControl::Cycle& GetFireControl() { return fireControl_; }
    const EnemyFireControl::Cycle& GetFireControl() const { return fireControl_; }
    void SetAttackTelegraphRate(float rate);
    void SetBossRecoveryRate(float rate) { bossRecoveryRate_ = rate; }
    bool IsDead() const { return lifeState_ == LifeState::Destroyed || lifeState_ == LifeState::Escaped; }
    bool WasDestroyed() const { return lifeState_ == LifeState::Destroyed; }
    bool HasEscaped() const { return lifeState_ == LifeState::Escaped; }
    bool IsBoss() const { return behavior_ == Behavior::Boss; }
    bool IsCrossfire() const { return behavior_ == Behavior::Crossfire; }
    bool IsSniper() const { return behavior_ == Behavior::Sniper; }
    // 二射後、反動が収まってから姿勢を戻すまでがチャージ直撃の反撃機会。
    bool IsChargeCounterOpen() const {
        const float recovery = fireControl_.RecoveryElapsed();
        return IsSniper() && CanShoot() && recovery >= 8.0f && recovery < 66.0f;
    }
    bool IsShield() const { return behavior_ == Behavior::Shield; }
    bool IsSupport() const { return behavior_ == Behavior::Support; }
    bool HasShield() const { return shieldHp_ > 0; }
    int GetShieldHp() const { return shieldHp_; }
    int GetShieldMaxHp() const { return shieldMaxHp_; }
    bool IsTargetable() const;
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    const Math::Vector3& GetTranslate() const { return translate_; }
    Math::Vector3 GetAimPosition() const;
    float GetRadius() const { return collisionRadius_; }
    float GetAimRadius() const;

private:
    void Escape() { lifeState_ = LifeState::Escaped; }

    std::unique_ptr<Object3d> object_;
    Math::Vector3 baseTranslate_{};
    Math::Vector3 translate_{};
    Math::Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };
    Math::Vector3 aimLocalCenter_{ 0.0f, 0.0f, 0.0f };
    Math::Vector4 baseColor_{ 1.0f, 0.25f, 0.25f, 1.0f };
    int age_ = 0;
    float ageTimer_ = 0.0f;
    int hp_ = 1;
    int maxHp_ = 1;
    int shieldHp_ = 0;
    int shieldMaxHp_ = 0;
    int hitFlashTimer_ = 0;
    int hitFlashDuration_ = 1;
    float hitFlashStrength_ = 1.0f;
    float moveTimer_ = 0.0f;
    float phase_ = 0.0f;
    float horizontalAmplitude_ = 1.2f;
    float verticalAmplitude_ = 0.55f;
    float collisionRadius_ = 0.8f;
    float aimRadius_ = 0.8f;
    float visualScaleRate_ = 1.0f;
    float attackTelegraphRate_ = 0.0f;
    float bossRecoveryRate_ = 0.0f; // 反撃可能時間の残り割合。構えと発射反動に使用する。
    EnemyFireControl::Cycle fireControl_{}; // 共通タイマーではなく、各機の構え・連射・休止を保持。
    Math::Vector3 sniperBracePosition_{}; // 構え始めの位置。zはレールからの距離。
    bool sniperBraced_ = false;
    bool trainingTarget_ = false;
    bool riftCarrier_ = false;
    Behavior behavior_ = Behavior::Formation;
    EntryStyle entryStyle_ = EntryStyle::Direct;
    LifeState lifeState_ = LifeState::Alive;
};
