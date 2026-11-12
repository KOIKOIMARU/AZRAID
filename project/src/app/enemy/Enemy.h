// 敵1機のHP・シールド・描画・生存状態を持つ　飛行の計算はEnemyFlight、射撃の時計はEnemyFireControlが担当する

#pragma once

#include "engine/3d/Object3d.h"
#include "engine/base/Math.h"
#include "app/enemy/EnemyFlight.h"

#include <memory>

class Model;
class Object3dCommon;

class Enemy {
public:
    using Behavior = EnemyBehavior;
    using EntryStyle = EnemyEntryStyle;

    enum class LifeState {
        Alive,
        Destroyed,
        Escaped,
        Departing,
        Departed
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
    // 自機の死亡演出中は射撃を止め、実時間で飛び去る　通常の逃走成績には加えない
    void UpdateDeparture(float deltaSeconds);
    bool HasDeparted() const { return lifeState_ == LifeState::Departed; }
    // 練習標的だけは入場後に滞在し、操作を試している間に退場しない
    void SetTrainingTarget(bool enabled) { flight_.SetTrainingTarget(enabled); }
    void Draw();
    void DrawShadow(const Math::Matrix4x4& lightViewProjection);

    void Kill();
    bool Damage(int damage);
    bool CanShoot() const;
    Behavior GetBehavior() const { return flight_.Behavior(); }
    bool IsTightFormation() const { return GetBehavior() == Behavior::Formation && flight_.EntryStyle() == EntryStyle::TightFormation; }
    float GetFormationShotDelay() const { return flight_.FormationShotDelay(); } // 編隊の外側ほど遅く撃つ
    EnemyFireControl::Cycle& GetFireControl() { return fireControl_; }
    const EnemyFireControl::Cycle& GetFireControl() const { return fireControl_; }
    void SetAttackTelegraphRate(float rate);
    void SetBossRecoveryRate(float rate) { flight_.SetBossRecoveryRate(rate); }
    void SetBossFlightOffset(const Math::Vector3& offset) { flight_.SetBossTarget(offset); }
    bool IsDead() const { return lifeState_ == LifeState::Destroyed || lifeState_ == LifeState::Escaped || HasDeparted(); }
    bool WasDestroyed() const { return lifeState_ == LifeState::Destroyed; }
    bool HasEscaped() const { return lifeState_ == LifeState::Escaped; }
    bool IsBoss() const { return GetBehavior() == Behavior::Boss; }
    bool IsCrossfire() const { return GetBehavior() == Behavior::Crossfire; }
    bool IsSniper() const { return GetBehavior() == Behavior::Sniper; }
    // 二射後、反動が収まってから姿勢を戻すまでがチャージ直撃の反撃機会
    bool IsChargeCounterOpen() const {
        const float recovery = fireControl_.RecoveryElapsed();
        return IsSniper() && CanShoot() && recovery >= 8.0f && recovery < 66.0f;
    }
    bool IsShield() const { return GetBehavior() == Behavior::Shield; }
    bool IsSupport() const { return GetBehavior() == Behavior::Support; }
    bool HasShield() const { return shieldHp_ > 0; }
    int GetShieldHp() const { return shieldHp_; }
    int GetShieldMaxHp() const { return shieldMaxHp_; }
    bool IsTargetable() const;
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    const Math::Vector3& GetTranslate() const { return translate_; }
    Math::Vector3 GetAimPosition() const;
    Math::Vector3 GetBossMuzzlePosition(float side) const; // -1:左、0:中央、1:右　機体の傾きに追従
    float GetRadius() const { return collisionRadius_; }
    float GetAimRadius() const;

private:
    // 飛行パターンの姿勢に攻撃予告と被弾の見た目を重ねる
    void UpdateAppearance(EnemyFlight::Pose pose);

    EnemyFlight flight_;
    std::unique_ptr<Object3d> object_;
    Math::Vector3 translate_{};
    Math::Vector3 baseScale_{ 1.0f, 1.0f, 1.0f };
    Math::Vector3 aimLocalCenter_{ 0.0f, 0.0f, 0.0f };
    Math::Vector4 baseColor_{ 1.0f, 0.25f, 0.25f, 1.0f };
    Math::Vector3 departureOrigin_{};
    Math::Vector3 departureRotation_{};
    Math::Vector4 departureColor_{};
    float departureElapsed_ = 0.0f;
    int hp_ = 1;
    int maxHp_ = 1;
    int shieldHp_ = 0;
    int shieldMaxHp_ = 0;
    int hitFlashTimer_ = 0;
    int hitFlashDuration_ = 1;
    float hitFlashStrength_ = 1.0f;
    float collisionRadius_ = 0.8f;
    float aimRadius_ = 0.8f;
    float visualScaleRate_ = 1.0f;
    float attackTelegraphRate_ = 0.0f;
    EnemyFireControl::Cycle fireControl_{}; // 共通タイマーではなく、各機の構え・連射・休止を保持
    LifeState lifeState_ = LifeState::Alive;
};
