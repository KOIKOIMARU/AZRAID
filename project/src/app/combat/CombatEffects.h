#pragma once

#include "app/combat/CombatEffectPlayback.h"

class Camera;
class Model;
class Object3dCommon;

// 命中・撃破・発射・被弾の見た目を担当する　再生と枠の管理はCombatEffectPlaybackに渡す
class CombatEffects {
public:
    static constexpr size_t kVisualCapacity = CombatEffectPlayback::kVisualCapacity;
    static constexpr size_t kEffectCapacity = CombatEffectPlayback::kEffectCapacity;

    // いずれも借りる参照　モデルはModelManager、カメラと共通描画設定はGameRuntimeが所有する
    struct Assets {
        Model* glowCore = nullptr;
        Model* glowRing = nullptr;
        Model* sparkStar = nullptr;
        Model* impactBurst = nullptr;
        Model* fireball = nullptr;
        Model* smoke = nullptr;
        Model* sparks = nullptr;
        Model* blade = nullptr;
        Model* shock = nullptr;
        Model* arc = nullptr;
        Model* disc = nullptr;
        Model* magicShard = nullptr;
    };
    // 戦闘ルールから渡す演出用の値　スコア計算クラスを直接参照しない
    struct ChainStyle { int count = 0; int tier = 0; };

    void Initialize(Object3dCommon* common, Camera* camera, const Assets& assets);
    void PrepareOne() { playback_.PrepareOne(); } // 起動時の分割読み込みから呼ぶ　プレイ中には描画物体を生成しない
    void Clear();      // 所有する描画物体を破棄し、借りた参照を解除する
    void Reset() { playback_.Reset(); } // デモ周回・検証用　描画物体を保持したまま再利用枠へ戻す
    void Update(float deltaSeconds) { playback_.Update(deltaSeconds); }
    void Draw() { playback_.Draw(); }

    void EnemyDestroyed(const Math::Vector3& position, float strength, ChainStyle chain, bool fever = false);
    void EnemyImpact(const Math::Vector3& position, float strength, bool fever = false);
    void EnemyShot(const Math::Vector3& worldPosition);
    void PlayerShot(const Math::Vector3& worldPosition, bool isCharged, bool fever);
    void RingCollected(const Math::Vector3& worldPosition);
    void PlayerDamaged(const Math::Vector3& worldPosition);
    void DodgeGraze(const Math::Vector3& worldPosition);
    void JustDodged(const Math::Vector3& playerPosition, const Math::Vector3& bulletPosition, int direction);
    void ChargeSplash(const Math::Vector3& center, float radius);

    size_t ActiveCount() const { return playback_.ActiveCount(); }
    size_t ActiveVisualCount() const { return playback_.ActiveVisualCount(); }
    size_t AvailableCount() const { return playback_.AvailableCount(); }
    size_t MissCount() const { return playback_.MissCount(); }
#ifdef _DEBUG
    float LatestStrength() const { return playback_.LatestStrength(); }
    size_t LatestVisualCount() const { return playback_.LatestVisualCount(); }
#endif

private:
    using Effect = CombatEffectPlayback::Effect;
    using EffectType = CombatEffectPlayback::EffectType;

    void AddCombatBurstVisuals(Effect& effect, bool fever, ChainStyle chain);
    void NormalEnemyDestroyed(const Math::Vector3& worldPosition, float strength, ChainStyle chain);
    void FeverEnemyDestroyed(const Math::Vector3& worldPosition, float strength, ChainStyle chain);
    void NormalEnemyImpact(const Math::Vector3& worldPosition, float strength);
    void FeverEnemyImpact(const Math::Vector3& worldPosition, float strength);

    Assets assets_{};
    CombatEffectPlayback playback_;
};
