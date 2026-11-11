#include "app/combat/ProjectileSystem.h"

#include "app/enemy/Enemy.h"
#include "app/enemy/EnemyFireControl.h"
#include "app/runtime/GameRuntimeMath.h"
#include "engine/3d/Object3dCommon.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <utility>

void ProjectileSystem::Initialize(Object3dCommon* common, const Assets& assets)
{
    common_ = common;
    assets_ = assets;
}

// シーン終了時に弾の実体と参照を解放し、次の初期化に備える。
void ProjectileSystem::Clear()
{
    homingTargets_.clear();
    dodgedBullets_.clear();
    playerPool_.Clear();
    enemyPool_.Clear();
    assets_ = {};
    common_ = nullptr;
}

// 使用中の自機弾と敵弾を終了させ、追尾とジャスト回避に使う参照を消す。
void ProjectileSystem::KillAll()
{
    for (const auto& bullet : playerPool_.Active()) { bullet->Kill(); }
    for (const auto& bullet : enemyPool_.Active()) { bullet->Kill(); }
    homingTargets_.clear();
    dodgedBullets_.clear();
}

// すべての弾を空き枠へ戻し、以前の弾が使っていた記録を解除する。
void ProjectileSystem::RecycleAll()
{
    homingTargets_.clear();
    playerPool_.RecycleAll();
    RecycleEnemyBullets();
}

// 形態移行やボス撃破で残った敵弾をすべて回収し、回避の記録も消す。
void ProjectileSystem::RecycleEnemyBullets()
{
    dodgedBullets_.clear();
    enemyPool_.RecycleAll();
}

// 退場する敵を追尾先と命中済み一覧から外す。敵の参照は削除前に解除する。
void ProjectileSystem::InvalidateTarget(const Enemy* enemy)
{
    std::erase_if(homingTargets_, [enemy](const auto& item) { return item.second == enemy; });
    for (const auto& bullet : playerPool_.Active()) { bullet->ForgetHitTarget(enemy); }
}

// 自機弾の空き枠を1つ用意する。ここでモデルを準備しておき、発射時に再利用する。
void ProjectileSystem::PreparePlayerBullet(float speed)
{
    const Math::Vector3 offscreenPosition{ 0.0f, -1000.0f, -1000.0f };

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(
        common_,
        assets_.body,
        offscreenPosition,
        { 0.0f, 0.0f, speed },
        { 0.70f, 1.0f, 1.0f, 0.88f },
        1,
        { 0.36f, 0.36f, 0.88f },
        0.40f,
        1,
        assets_.bulletGlow ? assets_.bulletGlow : assets_.playerCore,
        { 0.66f, 1.0f, 1.0f, 0.88f },
        { 0.72f, 1.75f, 1.0f },
        assets_.playerTrail,
        { 0.54f, 0.96f, 1.0f, 0.64f },
        { 0.42f, 3.50f, 1.0f },
        1.78f,
        nullptr,
        { 0.70f, 1.0f, 1.0f, 0.0f },
        { 0.075f, 0.075f, 1.0f });
    bullet->Kill();
    playerPool_.Prepare(std::move(bullet));
}

// 敵弾の空き枠を1つ用意する。ここでモデルを準備しておき、攻撃時に再利用する。
void ProjectileSystem::PrepareEnemyBullet(float speed)
{
    const Math::Vector3 offscreenPosition{ 0.0f, -1000.0f, -1000.0f };

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(
        common_,
        assets_.enemyCore ? assets_.enemyCore : assets_.body,
        offscreenPosition,
        { 0.0f, 0.0f, -speed },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        1,
        { 1.20f, 1.20f, 1.0f },
        0.48f,
        1,
        assets_.enemyCore ? assets_.enemyCore : assets_.bulletGlow,
        { 0.60f, 0.35f, 0.26f, 0.22f },
        { 1.20f, 1.20f, 1.0f },
        assets_.enemyTrail,
        { 1.0f, 1.0f, 1.0f, 0.70f },
        { 0.34f, 2.55f, 1.0f },
        1.15f,
        nullptr,
        { 1.0f, 0.30f, 0.70f, 0.0f },
        { 0.14f, 0.14f, 1.0f }, 8, true);
    bullet->Kill();
    enemyPool_.Prepare(std::move(bullet));
}

// 準備済みの弾を取り出し、通常・チャージ・フィーバーに応じて速度や描画、命中時の性質を設定する。
Bullet* ProjectileSystem::FirePlayer(const PlayerShot& shot)
{
    if (!assets_.body) { return nullptr; }

    const auto& spawnPosition = shot.origin;
    const Enemy* aimedTarget = shot.target;
    Math::Vector3 aimDirection = shot.aimDirection;
    if (aimedTarget) {
        const Math::Vector3 targetPosition = aimedTarget->GetAimPosition();
        Math::Vector3 targetDirection = Math::Normalize({
            targetPosition.x - spawnPosition.x,
            targetPosition.y - spawnPosition.y,
            targetPosition.z - spawnPosition.z
        });
        if (std::abs(targetDirection.x) + std::abs(targetDirection.y) + std::abs(targetDirection.z) > 0.001f) {
            aimDirection = targetDirection;
        }
    }
    Math::Vector3 velocity = aimDirection * shot.normalSpeed;
    Math::Vector4 color{ 0.26f, 0.80f, 1.22f, 1.0f };
    Math::Vector3 scale{ 0.28f, 0.28f, 3.2f };
    float collisionRadius = 0.42f;
    int lifeTimer = 180;
    int hitLimit = 1;
    const bool isCharged = shot.charged;
    const bool isFeverShot = shot.fever;
    float feverRed = 0.45f;
    float feverGreen = 0.92f;
    float feverBlue = 1.0f;
    if (isFeverShot) {
        float feverHue = std::fmod(
            shot.clock * 0.020f +
                static_cast<float>(playerPool_.Active().size() % 6) * 0.145f,
            1.0f);
        if (feverHue < 0.0f) {
            feverHue += 1.0f;
        }
        ImGui::ColorConvertHSVtoRGB(
            feverHue,
            0.64f,
            1.0f,
            feverRed,
            feverGreen,
            feverBlue);
    }

    if (isCharged) {
        velocity = aimDirection * shot.chargedSpeed * shot.chargeMultiplier;
        color = { 1.42f, 0.85f, 0.24f, 1.0f };
        scale = { 0.70f, 0.70f, 2.35f };
        collisionRadius = 1.00f;
        lifeTimer = 260;
        hitLimit = 1;
    }
    if (isFeverShot) {
        velocity = aimDirection * shot.chargedSpeed * shot.chargeMultiplier * 1.18f;
        color = { 1.18f + feverRed * 0.26f, 1.18f + feverGreen * 0.26f, 1.18f + feverBlue * 0.26f, 1.0f };
        scale = { 0.56f, 0.56f, 6.2f };
        collisionRadius = 1.04f;
        lifeTimer = 280;
        hitLimit = 3;
    }

    // 巡航分を弾速にも引き継ぎ、加速中も敵へ届くまでの手応えを保つ。
    if (!shot.tutorial) {
        velocity = velocity * 1.20f;
        velocity.x += aimDirection.x * shot.railSpeed;
        velocity.y += aimDirection.y * shot.railSpeed;
        velocity.z += aimDirection.z * shot.railSpeed;
    }

    auto bullet = playerPool_.Acquire();
    if (!bullet) {
        return nullptr;
    }
    const bool isGoldCharge = isCharged && !isFeverShot;
    Model* coreModel = isGoldCharge ? assets_.chargeCore : assets_.playerCore;
    if (!coreModel) { coreModel = assets_.fallbackGlow; }
    bullet->Initialize(
        common_,
        isGoldCharge && assets_.chargeBody ? assets_.chargeBody : assets_.body,
        spawnPosition,
        velocity,
        color,
        lifeTimer,
        scale,
        collisionRadius,
        hitLimit,
        coreModel,
        isFeverShot ?
            Math::Vector4{
                0.62f + feverRed * 0.38f,
                0.62f + feverGreen * 0.38f,
                0.62f + feverBlue * 0.38f,
                1.0f } :
        isCharged ?
            Math::Vector4{ 1.90f, 0.72f, 0.12f, 0.90f } :
            Math::Vector4{ 0.18f, 0.64f, 1.18f, 0.70f },
        isFeverShot ?
            Math::Vector3{ 0.85f, 0.85f, 1.0f } :
        isCharged ?
            Math::Vector3{ 1.25f, 1.25f, 1.0f } :
            Math::Vector3{ 0.38f, 0.56f, 1.0f },
        isGoldCharge ? assets_.chargeTrail : assets_.playerTrail,
        isFeverShot ?
            Math::Vector4{ feverRed, feverGreen, feverBlue, 0.94f } :
        isCharged ?
            Math::Vector4{ 1.85f, 0.66f, 0.08f, 0.88f } :
            Math::Vector4{ 0.12f, 0.56f, 1.20f, 0.76f },
        isGoldCharge ?
            Math::Vector3{ 0.86f, 9.0f, 1.0f } :
        isFeverShot ?
            Math::Vector3{ 0.48f, 8.0f, 1.0f } :
            Math::Vector3{ 0.22f, 5.5f, 1.0f },
        isFeverShot ? 8.0f : isCharged ? 9.0f : 5.5f,
        nullptr,
        isFeverShot ?
            Math::Vector4{ feverRed, feverGreen, feverBlue, 0.92f } :
        isCharged ?
            Math::Vector4{ 0.72f, 0.94f, 1.0f, 0.72f } :
            Math::Vector4{ 0.34f, 0.72f, 1.0f, 0.0f },
        isFeverShot ?
            Math::Vector3{ 0.24f, 0.24f, 1.0f } :
        isCharged ?
            Math::Vector3{ 0.17f, 0.17f, 1.0f } :
            Math::Vector3{ 0.092f, 0.092f, 1.0f });
    bullet->SetFeverShot(isFeverShot);
    // フィーバーでも照準を合わせた敵だけ補正する。外れた弾が別の敵を探すことはしない。
    if (aimedTarget) {
        bullet->EnableHoming(
            isFeverShot ? 0.11f :
            (isCharged ? 0.18f : 0.08f));
        bullet->SetHomingTarget(aimedTarget->GetAimPosition());
        homingTargets_[bullet] = aimedTarget;
    }
    return bullet;
}

// 敵の攻撃種類から弾のモデル、色、速度、当たり判定を決め、銃口から発射する。
Bullet* ProjectileSystem::FireEnemy(const EnemyShot& shot)
{
    if (!assets_.body) { return nullptr; }

    Math::Vector3 spawnPosition = shot.origin;
    spawnPosition.z -= 1.0f;
    const EnemyStyle style = shot.style;

    // 暗い縁と高熱の芯を一枚の不透明な弾頭で読む。芯の半径は当たり判定に揃える。
    Math::Vector4 bodyColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    Math::Vector3 bodyScale{ 1.20f, 1.20f, 1.0f };
    Math::Vector4 glowColor{ 0.60f, 0.35f, 0.26f, 0.22f };
    Math::Vector3 glowScale{ 1.20f, 1.20f, 1.0f };
    Math::Vector4 trailColor{ 1.0f, 1.0f, 1.0f, 0.70f };
    Math::Vector3 trailScale{ 0.34f, 2.55f, 1.0f };
    float trailOffset = 1.15f;
    float speedScale = shot.tutorial ? 1.0f : 1.25f;
    float collisionRadius = 0.48f;
    int lifeTimer = 260;
    int damage = 8;

    switch (style) {
    case EnemyStyle::Crossfire:
        bodyScale = glowScale = { 1.05f, 1.05f, 1.0f };
        trailScale = { 0.25f, 3.30f, 1.0f };
        trailOffset = 1.65f;
        speedScale = shot.tutorial ? 1.12f : 1.40f;
        collisionRadius = 0.42f;
        damage = 7;
        break;
    case EnemyStyle::Sniper:
        bodyScale = glowScale = { 1.40f, 1.40f, 1.0f };
        trailColor = { 1.0f, 0.92f, 0.86f, 0.94f };
        trailScale = { 0.26f, 6.80f, 1.0f };
        trailOffset = 3.55f;
        speedScale = shot.tutorial ? 1.67f : 1.90f;
        collisionRadius = 0.56f;
        lifeTimer = 210;
        damage = 14;
        break;
    case EnemyStyle::ShieldOrb:
        bodyScale = glowScale = { 1.55f, 1.55f, 1.0f };
        bodyColor = { 0.90f, 0.78f, 0.72f, 1.0f };
        trailScale = { 0.44f, 1.42f, 1.0f };
        trailOffset = 0.78f;
        speedScale = shot.tutorial ? 0.78f : 0.95f;
        collisionRadius = 0.62f;
        lifeTimer = 300;
        damage = 6;
        break;
    case EnemyStyle::BossCannon:
        bodyScale = glowScale = { 1.80f, 1.80f, 1.0f };
        trailScale = { 0.58f, 4.35f, 1.0f };
        trailOffset = 2.15f;
        speedScale = 1.22f;
        collisionRadius = 0.72f;
        lifeTimer = 280;
        damage = 12;
        break;
    case EnemyStyle::BossCharge:
        bodyScale = glowScale = { 2.15f, 2.15f, 1.0f };
        glowColor = { 0.95f, 0.64f, 0.48f, 0.34f };
        trailColor = { 1.05f, 1.0f, 0.92f, 0.92f };
        trailScale = { 0.64f, 5.80f, 1.0f };
        trailOffset = 2.70f;
        speedScale = 1.76f;
        collisionRadius = 0.86f;
        lifeTimer = 220;
        damage = 16;
        break;
    case EnemyStyle::Standard:
    default:
        break;
    }

    if (!shot.tutorial) {
        damage += style == EnemyStyle::BossCharge ? 6 :
            style == EnemyStyle::Sniper || style == EnemyStyle::BossCannon ||
            style == EnemyStyle::ShieldOrb ? 4 : 2;
    }

    Math::Vector3 bulletDirection{ 0.0f, 0.0f, -1.0f };
    if (shot.playerPosition) {
        Math::Vector3 targetPosition = *shot.playerPosition;
        if (shot.fixedAim) {
            targetPosition.x = shot.fixedAim->x;
            targetPosition.y = shot.fixedAim->y;
        }
        targetPosition.x += shot.aimOffset.x;
        targetPosition.y += 0.08f + shot.aimOffset.y;
        targetPosition.z += 0.18f;
        bulletDirection = EnemyFireControl::SolveShotDirection(
            spawnPosition, targetPosition, shot.railSpeed, shot.speed * speedScale);
        if (bulletDirection.z > -0.10f) {
            bulletDirection.z = -0.10f;
            bulletDirection = Math::Normalize(bulletDirection);
        }
    }

    auto bullet = enemyPool_.Acquire();
    if (!bullet) {
        return nullptr;
    }
    bullet->Initialize(
        common_,
        assets_.enemyCore ? assets_.enemyCore : assets_.body,
        spawnPosition,
        bulletDirection * shot.speed * speedScale,
        bodyColor,
        lifeTimer,
        bodyScale,
        collisionRadius,
        1,
        assets_.enemyCore ? assets_.enemyCore : assets_.bulletGlow,
        glowColor,
        glowScale,
        assets_.enemyTrail,
        trailColor,
        trailScale,
        trailOffset,
        nullptr,
        { 1.0f, 0.30f, 0.70f, 0.0f },
        { 0.14f, 0.14f, 1.0f },
        damage, true);
    return bullet;
}

// 追尾先が有効か確認し、自機弾を進める。寿命が切れた弾を回収する。
void ProjectileSystem::UpdatePlayerBullets()
{
    for (auto iterator = playerPool_.Active().begin();
        iterator != playerPool_.Active().end();) {
        if ((*iterator)->CanHome()) {
            Bullet* bullet = iterator->get();
            auto targetIterator = homingTargets_.find(bullet);
            const Enemy* target =
                targetIterator != homingTargets_.end() ?
                targetIterator->second :
                nullptr;
            const Math::Vector3 targetPosition =
                target ? target->GetAimPosition() : Math::Vector3{};
            const bool hasPassedHomingTarget =
                target &&
                targetPosition.z + 1.25f < bullet->GetTranslate().z;
            if (target && !target->IsDead() && !hasPassedHomingTarget) {
                (*iterator)->SetHomingTarget(targetPosition);
            } else {
                (*iterator)->ClearHomingTarget();
                homingTargets_.erase(bullet);
            }
        }
        (*iterator)->Update();
        if ((*iterator)->IsDead()) {
            homingTargets_.erase(iterator->get());
            iterator = playerPool_.Recycle(iterator);
        } else {
            ++iterator;
        }
    }
}

// 敵弾をワールドの時間倍率で進め、終了した弾とその回避記録を回収する。
void ProjectileSystem::UpdateEnemyBullets(float worldTimeScale)
{
    for (auto iterator = enemyPool_.Active().begin();
        iterator != enemyPool_.Active().end();) {
        Bullet* bullet = iterator->get();
        bullet->Update(worldTimeScale);
        if (bullet->IsDead()) {
            dodgedBullets_.erase(bullet);
            iterator = enemyPool_.Recycle(iterator);
        } else {
            ++iterator;
        }
    }
}

// 自機弾と敵弾の本体を描く。カメラがあれば視点に合わせた見え方を使う。
void ProjectileSystem::DrawBodies(const Math::Matrix4x4* cameraWorld)
{
    for (const auto& bullet : playerPool_.Active()) { bullet->Draw(); }
    for (const auto& bullet : enemyPool_.Active()) { bullet->Draw(cameraWorld); }
}

// 弾本体の後に、カメラの向きに合わせた光や軌跡を重ねる。
void ProjectileSystem::DrawGlows(const Math::Matrix4x4& cameraWorld)
{
    if (!common_) { return; }

    const BlendMode previousBlendMode = common_->GetBlendMode();
    const DepthDrawMode previousDepthMode = common_->GetDepthDrawMode();

    common_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    common_->SetBlendMode(BlendMode::Add);
    common_->CommonDrawSetting();

    for (const auto& bullet : playerPool_.Active()) {
        if (bullet) {
            bullet->DrawGlow(cameraWorld);
        }
    }
    for (const auto& bullet : enemyPool_.Active()) {
        if (bullet) {
            bullet->DrawGlow(cameraWorld);
        }
    }

    common_->SetBlendMode(previousBlendMode);
    common_->SetDepthDrawMode(previousDepthMode);
    common_->CommonDrawSetting();
}

// 画面上の照準を3Dの射線に直し、自機の銃口から狙う方向を求める。
Math::Vector3 ProjectileSystem::AimDirection(const Math::Vector3& origin, const Math::Matrix4x4& viewProjection,
    const Math::Vector2& reticle, const Math::Vector2& viewportMin, const Math::Vector2& viewportSize, float aimDepth)
{
    if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) {
        return { 0.0f, 0.0f, 1.0f };
    }

    const float ndcX =
        ((reticle.x - viewportMin.x) / viewportSize.x) * 2.0f - 1.0f;
    const float ndcY =
        1.0f - ((reticle.y - viewportMin.y) / viewportSize.y) * 2.0f;
    const Math::Matrix4x4 inverseViewProjection =
        Math::Inverse(viewProjection);
    const Math::Vector3 nearPoint =
        GameRuntimeMath::TransformCoord({ ndcX, ndcY, 0.0f }, inverseViewProjection);
    const Math::Vector3 farPoint =
        GameRuntimeMath::TransformCoord({ ndcX, ndcY, 1.0f }, inverseViewProjection);
    const Math::Vector3 cameraRay = Math::Normalize({
        farPoint.x - nearPoint.x,
        farPoint.y - nearPoint.y,
        farPoint.z - nearPoint.z
    });
    if (cameraRay.z <= 0.001f) {
        return { 0.0f, 0.0f, 1.0f };
    }
    // カメラの視線と平行に撃つと、横移動した銃口からの弾道が照準を外れる。
    // 敵が近くにいる場合はその深度、空中は45m先で照準へ収束させる。
    const float rayDistance = (aimDepth - nearPoint.z) / cameraRay.z;
    const Math::Vector3 aimPoint{
        nearPoint.x + cameraRay.x * rayDistance,
        nearPoint.y + cameraRay.y * rayDistance,
        aimDepth };
    return Math::Normalize({ aimPoint.x - origin.x, aimPoint.y - origin.y, aimPoint.z - origin.z });
}
