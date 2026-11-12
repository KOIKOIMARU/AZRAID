// ===== CombatEffects.cpp =====
// 命中や撃破の種類に応じて、火花や爆発の形・色・出し方を決める

#include "app/combat/CombatEffects.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace {
constexpr float kHalfPi = std::numbers::pi_v<float> * 0.5f;
constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;
}

void CombatEffects::Initialize(Object3dCommon* common, Camera* camera, const Assets& assets)
{
    playback_.Initialize(common, camera);
    assets_ = assets;
}

void CombatEffects::Clear()
{
    playback_.Clear();
    assets_ = {};
}

// 撃破時の演出を通常・フィーバーで切り替える　チェインの段階は発光や破片の見せ方に使う
void CombatEffects::EnemyDestroyed(const Math::Vector3& position, float strength, ChainStyle chain, bool fever)
{
    if (fever) { FeverEnemyDestroyed(position, strength, chain); }
    else { NormalEnemyDestroyed(position, strength, chain); }
}

// 命中時の光を、通常弾とフィーバー弾で選び分ける
void CombatEffects::EnemyImpact(const Math::Vector3& position, float strength, bool fever)
{
    if (fever) { FeverEnemyImpact(position, strength); }
    else { NormalEnemyImpact(position, strength); }
}

// 爆発に使う輪、芯、斬撃などを同じ位置に重ね、描画枠へ登録する
void CombatEffects::AddCombatBurstVisuals(Effect& effect, bool fever, ChainStyle chain)
{
    const auto position = effect.worldPosition;
    const float seed = static_cast<float>(chain.count % 7) * 0.17f;
    // 暗い爆心と短い閃光を重ね、明るい背景でも爆発の中心と方向を見せる
    playback_.AddVisual(effect, assets_.disc, position, {
        .color = { 0.018f, 0.012f, 0.045f, 0.86f },
        .baseSize = 2.1f,
        .growth = -0.62f,
        .spin = 0,
        .popDelay = 0,
        .aspectX = 1.0f,
        .aspectY = 0.85f,
        .velocity = {},
        .additive = false,
        .lifeEnd = 0.09f,
    });
    for (int side = 0; side < 2; ++side) {
        playback_.AddVisual(effect, assets_.blade, position, {
            .color = { 1.70f, 1.55f, 1.90f, 0.96f },
            .baseSize = fever ? 1.3f : 1.0f,
            .growth = -0.60f,
            .spin = 0.06f,
            .popDelay = 0,
            .aspectX = 0.19f,
            .aspectY = fever ? 7.8f : 6.8f,
            .velocity = {},
            .additive = true,
            .lifeEnd = 0.23f,
            .roll = 1.22f + seed + static_cast<float>(side) * kHalfPi * 2.0f,
        });
    }
    playback_.AddVisual(effect, assets_.arc, position, {
        .color = fever ? Math::Vector4{ 1.10f, 0.56f, 1.50f, 0.82f } : Math::Vector4{ 0.94f, 1.05f, 1.35f, 0.72f },
        .baseSize = 0.95f,
        .growth = 2.8f,
        .spin = -0.35f,
        .popDelay = 0.02f,
        .aspectX = 1.7f,
        .aspectY = 0.70f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.32f,
        .roll = 0.32f + seed * 0.25f,
    });
    const int rayCount = (std::min)(4, 2 + chain.tier);
    for (int index = 0; index < rayCount; ++index) {
        const float angle = kTwoPi * static_cast<float>(index) / static_cast<float>(rayCount) + seed + 0.62f;
        const float speed = 0.34f + 0.045f * static_cast<float>(index % 2);
        playback_.AddVisual(effect, assets_.blade, position, {
            .color = fever ? Math::Vector4{ 1.35f, 0.72f, 1.75f, 0.92f } : Math::Vector4{ 1.45f, 1.15f, 0.60f, 0.94f },
            .baseSize = 0.85f,
            .growth = -0.66f,
            .spin = 0.08f,
            .popDelay = 0.02f,
            .aspectX = 0.15f,
            .aspectY = 3.1f,
            .velocity = { std::cos(angle) * speed, std::sin(angle) * speed, -0.01f },
            .additive = true,
            .lifeEnd = 0.68f,
            .roll = 0,
            .drag = 0.035f,
        });
    }
}

// 通常時の撃破で、連続撃破の段階と強さに合わせて閃光や破片を広げる
void CombatEffects::NormalEnemyDestroyed(
    const Math::Vector3& worldPosition,
    float strength, ChainStyle chain)
{
    Effect effect{};
    effect.worldPosition = worldPosition;
    const bool isHeavyExplosion = strength >= 1.7f;
    effect.duration = isHeavyExplosion ? 58 : 40;
    const float chainPower = 1.35f + 0.10f * static_cast<float>((std::min)(chain.tier, 3));
    effect.strength = strength * (isHeavyExplosion ? 1.25f : chainPower);
    effect.type = EffectType::EnemyDestroy;

    Model* fireballModel = assets_.fireball ? assets_.fireball : assets_.impactBurst;
    Model* smokeModel = assets_.smoke ? assets_.smoke : assets_.glowCore;
    Model* sparksModel = assets_.sparks ? assets_.sparks : assets_.sparkStar;
    Model* debrisModel = assets_.blade;

    playback_.AddVisual(effect, smokeModel, worldPosition, {
        .color = { 0.32f, 0.31f, 0.29f, 0.46f },
        .baseSize = 0.92f,
        .growth = 1.50f,
        .spin = 0.18f,
        .popDelay = 0.10f,
        .aspectX = 1.12f,
        .aspectY = 0.92f,
        .velocity = { -0.014f, 0.038f, -0.004f },
        .additive = false,
    });
    playback_.AddVisual(effect, smokeModel, worldPosition, {
        .color = { 0.24f, 0.23f, 0.22f, 0.42f },
        .baseSize = 0.82f,
        .growth = 1.75f,
        .spin = -0.14f,
        .popDelay = 0.16f,
        .aspectX = 0.86f,
        .aspectY = 1.05f,
        .velocity = { 0.038f, 0.028f, -0.006f },
        .additive = false,
    });
    playback_.AddVisual(effect, smokeModel, worldPosition, {
        .color = { 0.34f, 0.32f, 0.29f, 0.32f },
        .baseSize = 0.68f,
        .growth = 1.80f,
        .spin = 0.10f,
        .popDelay = 0.22f,
        .aspectX = 1.22f,
        .aspectY = 0.78f,
        .velocity = { -0.052f, 0.012f, -0.002f },
        .additive = false,
    });
    playback_.AddVisual(effect, smokeModel, worldPosition, {
        .color = { 0.20f, 0.20f, 0.19f, 0.30f },
        .baseSize = 0.62f,
        .growth = 1.65f,
        .spin = -0.22f,
        .popDelay = 0.28f,
        .aspectX = 0.90f,
        .aspectY = 0.82f,
        .velocity = { 0.060f, -0.002f, -0.002f },
        .additive = false,
    });

    playback_.AddVisual(effect, fireballModel, worldPosition, {
        .color = { 1.55f, 0.86f, 0.32f, 1.0f },
        .baseSize = 2.05f,
        .growth = 0.94f,
        .spin = 0.34f,
        .popDelay = 0.00f,
        .aspectX = 1.08f,
        .aspectY = 0.92f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.70f,
    });
    playback_.AddVisual(effect, fireballModel, worldPosition, {
        .color = { 1.35f, 0.32f, 0.07f, 0.90f },
        .baseSize = 1.55f,
        .growth = 1.04f,
        .spin = -0.44f,
        .popDelay = 0.04f,
        .aspectX = 0.88f,
        .aspectY = 1.06f,
        .velocity = { 0.018f, 0.010f, 0.0f },
        .additive = true,
        .lifeEnd = 0.55f,
    });
    playback_.AddVisual(effect, assets_.sparkStar, worldPosition, {
        .color = { 1.90f, 1.72f, 1.42f, 1.0f },
        .baseSize = 1.60f,
        .growth = -0.55f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.0f,
        .aspectY = 1.0f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.16f,
    });
    playback_.AddVisual(effect, assets_.shock, worldPosition, {
        .color = { 1.45f, 1.20f, 0.68f, 0.74f },
        .baseSize = 1.15f,
        .growth = 4.50f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.38f,
        .aspectY = 0.70f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.28f,
    });
    playback_.AddVisual(effect, sparksModel, worldPosition, {
        .color = { 1.0f, 0.82f, 0.38f, 0.90f },
        .baseSize = 0.74f,
        .growth = 0.60f,
        .spin = 0.32f,
        .popDelay = 0.00f,
        .aspectX = 1.0f,
        .aspectY = 1.0f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.56f,
    });

    playback_.AddVisual(effect, debrisModel, worldPosition, {
        .color = { 1.50f, 1.18f, 0.60f, 1.0f },
        .baseSize = 0.80f,
        .growth = -0.55f,
        .spin = 0.18f,
        .popDelay = 0.00f,
        .aspectX = 0.13f,
        .aspectY = 3.20f,
        .velocity = { -0.29f, 0.22f, 0.016f },
        .additive = true,
        .lifeEnd = 0.76f,
        .roll = 0,
        .drag = 0.025f,
    });
    playback_.AddVisual(effect, debrisModel, worldPosition, {
        .color = { 1.45f, 0.80f, 0.28f, 0.92f },
        .baseSize = 0.76f,
        .growth = -0.50f,
        .spin = -0.16f,
        .popDelay = 0.02f,
        .aspectX = 0.14f,
        .aspectY = 3.10f,
        .velocity = { 0.32f, 0.15f, 0.014f },
        .additive = true,
        .lifeEnd = 0.72f,
        .roll = 0,
        .drag = 0.028f,
    });
    playback_.AddVisual(effect, debrisModel, worldPosition, {
        .color = { 1.35f, 1.15f, 0.65f, 0.90f },
        .baseSize = 0.68f,
        .growth = -0.62f,
        .spin = 0.12f,
        .popDelay = 0.04f,
        .aspectX = 0.12f,
        .aspectY = 2.80f,
        .velocity = { -0.24f, -0.28f, 0.010f },
        .additive = true,
        .lifeEnd = 0.74f,
        .roll = 0,
        .drag = 0.025f,
    });
    playback_.AddVisual(effect, debrisModel, worldPosition, {
        .color = { 1.50f, 1.30f, 0.80f, 0.86f },
        .baseSize = 0.72f,
        .growth = -0.60f,
        .spin = -0.18f,
        .popDelay = 0.05f,
        .aspectX = 0.12f,
        .aspectY = 3.10f,
        .velocity = { 0.28f, -0.25f, 0.010f },
        .additive = true,
        .lifeEnd = 0.68f,
        .roll = 0,
        .drag = 0.030f,
    });
    playback_.AddVisual(effect, sparksModel, worldPosition, {
        .color = { 1.0f, 0.94f, 0.58f, 0.88f },
        .baseSize = 0.38f,
        .growth = 0.95f,
        .spin = 1.15f,
        .popDelay = 0.02f,
        .aspectX = 1.0f,
        .aspectY = 1.0f,
        .velocity = { -0.104f, 0.154f, 0.006f },
    });
    playback_.AddVisual(effect, sparksModel, worldPosition, {
        .color = { 1.0f, 0.62f, 0.20f, 0.78f },
        .baseSize = 0.34f,
        .growth = 0.90f,
        .spin = -1.30f,
        .popDelay = 0.04f,
        .aspectX = 1.0f,
        .aspectY = 1.0f,
        .velocity = { 0.118f, 0.132f, 0.006f },
    });
    playback_.AddVisual(effect, sparksModel, worldPosition, {
        .color = { 1.0f, 0.42f, 0.14f, 0.64f },
        .baseSize = 0.28f,
        .growth = 0.82f,
        .spin = 0.92f,
        .popDelay = 0.08f,
        .aspectX = 1.0f,
        .aspectY = 1.0f,
        .velocity = { 0.026f, -0.172f, 0.004f },
    });
    AddCombatBurstVisuals(effect, false, chain);
    playback_.Commit(std::move(effect));
}

// フィーバー中の撃破に使う光と破片を作り、チェイン段階に応じて強調する
void CombatEffects::FeverEnemyDestroyed(
    const Math::Vector3& worldPosition,
    float strength, ChainStyle chain)
{
    Effect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 46;
    effect.strength = strength * (1.50f + 0.10f * static_cast<float>((std::min)(chain.tier, 3)));
    effect.type = EffectType::EnemyDestroy;

    Model* ringModel =
        assets_.shock ? assets_.shock : assets_.glowRing;
    Model* coreModel =
        assets_.impactBurst ? assets_.impactBurst : assets_.glowCore;
    Model* shardModel =
        assets_.blade;
    Model* starModel =
        assets_.sparkStar ? assets_.sparkStar : assets_.glowCore;
    const std::array<Math::Vector4, 4> rainbowColors = {
        Math::Vector4{ 1.35f, 0.56f, 1.55f, 0.96f },
        Math::Vector4{ 1.55f, 1.20f, 0.55f, 0.96f },
        Math::Vector4{ 0.72f, 1.20f, 1.65f, 0.96f },
        Math::Vector4{ 1.60f, 1.52f, 1.80f, 0.96f }
    };

    playback_.AddVisual(effect, ringModel, worldPosition, {
        .color = { 1.45f, 1.35f, 1.75f, 0.94f },
        .baseSize = 0.72f,
        .growth = 4.50f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.26f,
        .aspectY = 0.72f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.30f,
    });
    playback_.AddVisual(effect, ringModel, worldPosition, {
        .color = { 1.20f, 0.34f, 1.45f, 0.78f },
        .baseSize = 0.50f,
        .growth = 3.45f,
        .spin = 1.25f,
        .popDelay = 0.02f,
        .aspectX = 0.82f,
        .aspectY = 1.22f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.42f,
    });
    playback_.AddVisual(effect, coreModel, worldPosition, {
        .color = { 1.80f, 1.65f, 1.90f, 1.0f },
        .baseSize = 1.60f,
        .growth = -0.45f,
        .spin = -0.72f,
        .popDelay = 0.0f,
        .aspectX = 1.12f,
        .aspectY = 0.90f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.24f,
    });

    constexpr int kShardCount = 8;
    for (int index = 0; index < kShardCount; ++index) {
        const float angle =
            kTwoPi * static_cast<float>(index) /
            static_cast<float>(kShardCount) + 0.18f;
        const float speed = 0.25f + 0.03f * static_cast<float>(index % 3);
        const Math::Vector3 velocity{
            std::cos(angle) * speed,
            std::sin(angle) * speed,
            0.008f * static_cast<float>((index % 2) * 2 - 1)
        };
        playback_.AddVisual(effect, shardModel, worldPosition, {
            .color = rainbowColors[static_cast<size_t>(index) % rainbowColors.size()],
            .baseSize = 0.68f + 0.10f * static_cast<float>(index % 2),
            .growth = -0.55f,
            .spin = (index % 2 == 0 ? 0.25f : -0.25f),
            .popDelay = 0.01f * static_cast<float>(index % 3),
            .aspectX = 0.12f,
            .aspectY = 2.8f,
            .velocity = velocity,
            .additive = true,
            .lifeEnd = 0.72f,
            .roll = 0,
            .drag = 0.032f,
        });
    }
    playback_.AddVisual(effect, starModel, worldPosition, {
        .color = { 0.34f, 1.0f, 0.88f, 0.90f },
        .baseSize = 0.50f,
        .growth = 1.15f,
        .spin = 1.8f,
        .popDelay = 0.02f,
        .aspectX = 1.0f,
        .aspectY = 1.0f,
        .velocity = { -0.052f, 0.096f, 0.0f },
    });
    playback_.AddVisual(effect, starModel, worldPosition, {
        .color = { 1.0f, 0.38f, 0.82f, 0.86f },
        .baseSize = 0.42f,
        .growth = 1.32f,
        .spin = -2.1f,
        .popDelay = 0.05f,
        .aspectX = 1.0f,
        .aspectY = 1.0f,
        .velocity = { 0.066f, -0.076f, 0.0f },
    });

    AddCombatBurstVisuals(effect, true, chain);
    playback_.Commit(std::move(effect));
}

// 通常弾の命中点に短い閃光と破片を配置し、当たった場所を見せる
void CombatEffects::NormalEnemyImpact(
    const Math::Vector3& worldPosition,
    float strength)
{
    Effect effect{};
    effect.worldPosition = worldPosition;
    const bool isHeavyImpact = strength >= 1.35f;
    effect.duration = isHeavyImpact ? 20 : 14;
    effect.strength = strength * (isHeavyImpact ? 1.08f : 0.96f);
    effect.type = EffectType::EnemyImpact;

    Model* burstModel =
        assets_.impactBurst ? assets_.impactBurst : assets_.glowCore;
    Model* shardModel =
        assets_.blade;

    playback_.AddVisual(effect, burstModel, worldPosition, {
        .color = { 1.0f, 0.97f, 0.86f, 0.96f },
        .baseSize = isHeavyImpact ? 1.05f : 0.72f,
        .growth = -0.22f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.45f,
        .aspectY = 0.62f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.36f,
    });
    playback_.AddVisual(effect, burstModel, worldPosition, {
        .color = { 1.05f, 0.88f, 0.62f, 0.62f },
        .baseSize = isHeavyImpact ? 0.85f : 0.55f,
        .growth = isHeavyImpact ? 0.82f : 0.62f,
        .spin = -0.4f,
        .popDelay = 0.0f,
        .aspectX = 1.62f,
        .aspectY = 0.34f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.55f,
    });

    for (int side = 0; side < 2; ++side) {
        playback_.AddVisual(effect, assets_.blade, worldPosition, {
            .color = { 1.55f, 1.35f, 1.62f, 0.94f },
            .baseSize = isHeavyImpact ? 1.0f : 0.65f,
            .growth = -0.70f,
            .spin = 0,
            .popDelay = 0,
            .aspectX = 0.13f,
            .aspectY = 4.2f,
            .velocity = {},
            .additive = true,
            .lifeEnd = 0.42f,
            .roll = 1.1f + static_cast<float>(side) * kHalfPi * 2.0f,
        });
    }
    constexpr int kImpactShardCount = 6;
    for (int index = 0; index < kImpactShardCount; ++index) {
        const float angle =
            kTwoPi * static_cast<float>(index) /
            static_cast<float>(kImpactShardCount) + 0.28f;
        const float speed =
            (isHeavyImpact ? 0.26f : 0.20f) + 0.018f * static_cast<float>(index % 2);
        playback_.AddVisual(effect, shardModel, worldPosition, {
            .color = index % 2 == 0 ? Math::Vector4{ 1.55f, 1.20f, 0.60f, 0.96f } : Math::Vector4{ 1.20f, 1.30f, 1.60f, 0.90f },
            .baseSize = isHeavyImpact ? 0.45f : 0.30f,
            .growth = -0.55f,
            .spin = index % 2 == 0 ? 0.18f : -0.18f,
            .popDelay = 0.0f,
            .aspectX = 0.12f,
            .aspectY = 2.4f,
            .velocity = { std::cos(angle) * speed, std::sin(angle) * speed, 0.004f },
            .additive = true,
            .lifeEnd = 0.88f,
            .roll = 0,
            .drag = 0.04f,
        });
    }

    playback_.Commit(std::move(effect));
}

// フィーバー弾の命中点に強い発光と衝撃の輪を重ねる
void CombatEffects::FeverEnemyImpact(
    const Math::Vector3& worldPosition,
    float strength)
{
    Effect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 20;
    effect.strength = strength * 1.10f;
    effect.type = EffectType::EnemyImpact;

    Model* ringModel =
        assets_.shock ? assets_.shock : assets_.glowRing;
    Model* starModel =
        assets_.sparkStar ? assets_.sparkStar : assets_.glowCore;
    Model* shardModel =
        assets_.blade;
    playback_.AddVisual(effect, ringModel, worldPosition, {
        .color = { 1.25f, 0.62f, 1.60f, 0.90f },
        .baseSize = 0.65f,
        .growth = 2.75f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.20f,
        .aspectY = 0.72f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.62f,
    });
    playback_.AddVisual(effect, assets_.glowCore, worldPosition, {
        .color = { 1.70f, 1.55f, 1.80f, 0.96f },
        .baseSize = 0.78f,
        .growth = 0.34f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.0f,
        .aspectY = 1.0f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.40f,
    });
    playback_.AddVisual(effect, starModel, worldPosition, {
        .color = { 1.40f, 0.56f, 1.70f, 0.92f },
        .baseSize = 0.29f,
        .growth = 0.58f,
        .spin = 1.5f,
        .popDelay = 0.0f,
        .aspectX = 1.0f,
        .aspectY = 1.0f,
        .velocity = { -0.060f, 0.036f, 0.0f },
    });
    playback_.AddVisual(effect, shardModel, worldPosition, {
        .color = { 0.32f, 0.72f, 1.0f, 0.82f },
        .baseSize = 0.25f,
        .growth = 0.42f,
        .spin = -1.9f,
        .popDelay = 0.02f,
        .aspectX = 0.38f,
        .aspectY = 1.45f,
        .velocity = { 0.072f, -0.026f, 0.0f },
    });
    playback_.AddVisual(effect, shardModel, worldPosition, {
        .color = { 0.72f, 1.0f, 0.32f, 0.78f },
        .baseSize = 0.23f,
        .growth = 0.48f,
        .spin = 2.1f,
        .popDelay = 0.03f,
        .aspectX = 0.38f,
        .aspectY = 1.45f,
        .velocity = { -0.068f, -0.040f, 0.0f },
    });
    for (int index = 0; index < 5; ++index) {
        const float angle = kTwoPi * static_cast<float>(index) / 5.0f + 0.32f;
        playback_.AddVisual(effect, shardModel, worldPosition, {
            .color = { 1.45f, 1.15f, 1.70f, 0.94f },
            .baseSize = 0.48f,
            .growth = -0.60f,
            .spin = 0.12f,
            .popDelay = 0,
            .aspectX = 0.12f,
            .aspectY = 2.8f,
            .velocity = { std::cos(angle) * 0.27f, std::sin(angle) * 0.27f, 0 },
            .additive = true,
            .lifeEnd = 0.84f,
            .roll = 0,
            .drag = 0.035f,
        });
    }
    playback_.Commit(std::move(effect));
}

// 敵弾が出た位置に短い発射光を出し、どこから撃たれたか見えるようにする
void CombatEffects::EnemyShot(const Math::Vector3& worldPosition)
{
    Effect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 8;
    effect.strength = 0.76f;
    effect.type = EffectType::EnemyImpact;

    playback_.AddVisual(effect, assets_.glowCore, worldPosition, {
        .color = { 1.12f, 0.10f, 0.08f, 0.70f },
        .baseSize = 0.42f,
        .growth = 0.18f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.20f,
        .aspectY = 0.86f,
        .velocity = {},
    });
    playback_.AddVisual(effect, assets_.sparkStar, worldPosition, {
        .color = { 1.25f, 0.84f, 0.68f, 0.68f },
        .baseSize = 0.24f,
        .growth = 0.42f,
        .spin = 0.0f,
        .popDelay = 0.02f,
        .aspectX = 0.76f,
        .aspectY = 1.24f,
        .velocity = { 0.0f, 0.0f, -0.018f },
    });
    playback_.Commit(std::move(effect));
}

// 自機弾の発射位置に、通常・チャージ・フィーバーに応じた光を出す
void CombatEffects::PlayerShot(
    const Math::Vector3& worldPosition,
    bool isCharged, bool fever)
{
    Effect effect{};
    effect.worldPosition = worldPosition;
    const bool isGoldCharge = isCharged && !fever;
    effect.duration = isGoldCharge ? 10 : 5;
    effect.strength = isCharged ? 1.30f : 0.90f;
    effect.type = EffectType::EnemyImpact;

    playback_.AddVisual(effect, assets_.impactBurst ? assets_.impactBurst : assets_.glowCore, worldPosition, {
        .color = isGoldCharge ? Math::Vector4{ 1.45f, 0.86f, 0.28f, 0.80f } : Math::Vector4{ 0.28f, 0.78f, 1.30f, 0.62f },
        .baseSize = isGoldCharge ? 0.58f : 0.26f,
        .growth = isGoldCharge ? 0.22f : 0.14f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.25f,
        .aspectY = 0.72f,
        .velocity = {},
    });
    playback_.AddVisual(effect, assets_.sparkStar, worldPosition, {
        .color = isGoldCharge ? Math::Vector4{ 1.30f, 0.75f, 0.16f, 0.76f } : Math::Vector4{ 0.30f, 0.76f, 1.0f, 0.50f },
        .baseSize = isCharged ? 0.24f : 0.16f,
        .growth = 0.36f,
        .spin = isCharged ? 1.4f : -1.0f,
        .popDelay = 0.02f,
        .aspectX = 1.0f,
        .aspectY = 0.58f,
        .velocity = {},
    });

    if (isGoldCharge) {
        playback_.AddVisual(effect, assets_.shock, worldPosition, {
            .color = { 1.40f, 0.82f, 0.20f, 0.68f },
            .baseSize = 0.30f,
            .growth = 2.2f,
            .spin = 0,
            .popDelay = 0,
            .aspectX = 1,
            .aspectY = 1,
            .velocity = {},
            .additive = true,
            .lifeEnd = 0.65f,
        });
    }
    for (int side = 0; side < 2; ++side) {
        playback_.AddVisual(effect, assets_.blade, worldPosition, {
            .color = isGoldCharge ? Math::Vector4{ 1.50f, 0.94f, 0.34f, 0.82f } : Math::Vector4{ 0.38f, 0.84f, 1.30f, 0.64f },
            .baseSize = isGoldCharge ? 0.65f : 0.34f,
            .growth = -0.76f,
            .spin = 0,
            .popDelay = 0,
            .aspectX = 0.11f,
            .aspectY = isGoldCharge ? 4.5f : 2.8f,
            .velocity = {},
            .additive = true,
            .lifeEnd = 0.55f,
            .roll = kHalfPi + static_cast<float>(side) * kHalfPi * 2.0f,
        });
    }
    playback_.Commit(std::move(effect));
}

// リング通過位置に光を出し、通過したことを画面で示す
void CombatEffects::RingCollected(const Math::Vector3& worldPosition)
{
    Effect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 20;
    effect.strength = 0.48f;
    effect.type = EffectType::RewardCollect;

    // 左右へ抜ける風の弧　視界の中央に取得アイコンや大きな閃光を置かない
    for (float side : { -1.0f, 1.0f }) {
        playback_.AddVisual(effect, assets_.arc, worldPosition, {
            .color = { 0.42f, 0.86f, 1.0f, 0.42f },
            .baseSize = 1.1f, .growth = 2.2f, .spin = 0.0f, .popDelay = 0.0f,
            .aspectX = 1.3f, .aspectY = 0.65f, .velocity = { side * 0.10f, 0.01f, 0.42f },
            .additive = true, .lifeEnd = 0.85f, .roll = side * 1.570796f,
        });
    }
    playback_.Commit(std::move(effect));
}

// 自機の被弾位置を中心に、ダメージを受けた瞬間の閃光と火花を表示する
void CombatEffects::PlayerDamaged(const Math::Vector3& worldPosition)
{
    Effect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 20;
    effect.strength = 0.92f;
    effect.type = EffectType::PlayerDamage;

    Model* burstModel =
        assets_.impactBurst ? assets_.impactBurst : assets_.glowCore;
    Model* shardModel =
        assets_.magicShard ? assets_.magicShard : assets_.sparkStar;
    playback_.AddVisual(effect, burstModel, worldPosition, {
        .color = { 1.0f, 0.92f, 0.78f, 0.92f },
        .baseSize = 0.48f,
        .growth = -0.16f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.38f,
        .aspectY = 0.62f,
        .velocity = {},
    });
    playback_.AddVisual(effect, burstModel, worldPosition, {
        .color = { 1.0f, 0.24f, 0.12f, 0.74f },
        .baseSize = 0.44f,
        .growth = 0.58f,
        .spin = -2.8f,
        .popDelay = 0.0f,
        .aspectX = 1.68f,
        .aspectY = 0.30f,
        .velocity = {},
    });
    playback_.AddVisual(effect, burstModel, worldPosition, {
        .color = { 1.0f, 0.58f, 0.22f, 0.58f },
        .baseSize = 0.34f,
        .growth = 0.48f,
        .spin = 2.4f,
        .popDelay = 0.0f,
        .aspectX = 0.30f,
        .aspectY = 1.58f,
        .velocity = {},
    });

    constexpr std::array<Math::Vector3, 4> kDamageShardVelocities{
        Math::Vector3{ -0.115f, 0.070f, 0.006f },
        Math::Vector3{ 0.120f, 0.052f, 0.006f },
        Math::Vector3{ -0.070f, -0.104f, 0.004f },
        Math::Vector3{ 0.082f, -0.096f, 0.004f }
    };
    for (size_t index = 0; index < kDamageShardVelocities.size(); ++index) {
        playback_.AddVisual(effect, shardModel, worldPosition, {
            .color = index % 2 == 0 ? Math::Vector4{ 1.0f, 0.42f, 0.20f, 0.88f } : Math::Vector4{ 1.0f, 0.78f, 0.36f, 0.84f },
            .baseSize = 0.20f,
            .growth = 0.16f,
            .spin = index % 2 == 0 ? 2.2f : -2.2f,
            .popDelay = 0.0f,
            .aspectX = 0.28f,
            .aspectY = 1.68f,
            .velocity = kDamageShardVelocities[index],
        });
    }
    playback_.Commit(std::move(effect));
}

// 回避中に弾が触れた位置へ、短いかすりの光を出す
void CombatEffects::DodgeGraze(const Math::Vector3& worldPosition)
{
    Effect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 12;
    effect.strength = 0.58f;
    effect.type = EffectType::EnemyImpact;

    playback_.AddVisual(effect, assets_.glowCore, worldPosition, {
        .color = { 0.38f, 1.0f, 0.92f, 0.48f },
        .baseSize = 0.38f,
        .growth = 0.12f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.34f,
        .aspectY = 0.42f,
        .velocity = {},
    });
    playback_.AddVisual(effect, assets_.sparkStar, worldPosition, {
        .color = { 0.84f, 1.0f, 0.96f, 0.62f },
        .baseSize = 0.17f,
        .growth = 0.36f,
        .spin = 1.25f,
        .popDelay = 0.02f,
        .aspectX = 0.72f,
        .aspectY = 1.08f,
        .velocity = { 0.018f, 0.012f, 0.0f },
    });

    playback_.Commit(std::move(effect));
}

// ジャスト回避時の自機と敵弾の位置を基準に、回避方向に沿う光を重ねる
void CombatEffects::JustDodged(const Math::Vector3& playerPosition, const Math::Vector3& bulletPosition, int direction)
{
    Effect effect{};
    effect.worldPosition = playerPosition;
    effect.duration = 18;
    effect.type = EffectType::EnemyImpact;
    const float side = direction >= 0 ? 1.0f : -1.0f;
    playback_.AddVisual(effect, assets_.arc, playerPosition, {
        .color = { 0.34f, 0.90f, 1.0f, 0.70f }, .baseSize = 1.05f, .growth = 1.6f,
        .spin = side * 0.25f, .aspectX = 1.7f, .aspectY = 0.50f,
        .velocity = { -side * 0.025f, 0.0f, -0.035f }, .lifeEnd = 0.78f, .roll = side * 0.32f });
    // すれ違った弾の場所に細い閃光を残し、何を避けたのか読めるようにする
    playback_.AddVisual(effect, assets_.blade, bulletPosition, {
        .color = { 0.85f, 1.10f, 1.18f, 0.88f }, .baseSize = 0.80f, .growth = 0.8f,
        .aspectX = 0.075f, .aspectY = 3.2f, .lifeEnd = 0.30f, .roll = side * 1.15f });
    for (int index = 0; index < 4; ++index) {
        const float spread = static_cast<float>(index) - 1.5f;
        playback_.AddVisual(effect, assets_.sparks, bulletPosition, {
            .color = { 0.30f, 0.82f, 1.0f, 0.65f }, .baseSize = 0.16f, .growth = -0.5f,
            .popDelay = 0.04f, .aspectX = 0.24f, .aspectY = 2.8f,
            .velocity = { -side * (0.10f + static_cast<float>(index) * 0.025f), spread * 0.035f, -0.045f },
            .lifeEnd = 0.85f, .roll = side * 1.15f, .drag = 0.025f });
    }
    playback_.Commit(std::move(effect));
}

// チャージ弾の爆風が届く範囲を画面に示す　ダメージ判定はCombatResolverで行う
void CombatEffects::ChargeSplash(const Math::Vector3& center, float radius)
{
    Effect effect{};
    effect.worldPosition = center;
    effect.duration = 22;
    effect.type = EffectType::EnemyImpact;
    // 薄い衝撃波だけを広げる　白い面で敵弾や後ろの狙撃機を隠さない
    playback_.AddVisual(effect, assets_.shock, center, {
        .color = { 1.50f, 0.86f, 0.20f, 0.78f },
        .baseSize = 0.85f,
        .growth = radius / 0.85f - 1.0f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 1.4f,
        .aspectY = 0.78f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.64f,
        .roll = -0.15f,
    });
    playback_.AddVisual(effect, assets_.impactBurst, center, {
        .color = { 1.60f, 1.16f, 0.42f, 0.88f },
        .baseSize = 1.20f,
        .growth = -0.55f,
        .spin = 0.0f,
        .popDelay = 0.0f,
        .aspectX = 3.20f,
        .aspectY = 0.25f,
        .velocity = {},
        .additive = true,
        .lifeEnd = 0.40f,
    });
    playback_.Commit(std::move(effect));
}
