// ===== Enemy.cpp =====
// 敵1体のHPと生存状態を持ち、飛行計算の結果を機体の姿勢と表示に反映する

#include "app/enemy/Enemy.h"

#include "engine/3d/Model.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.1415926535f;

float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float Lerp(float start, float end, float rate)
{
    return start + (end - start) * rate;
}

float SmoothStep(float value)
{
    const float t = Clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

struct ModelAimBounds {
    Math::Vector3 center{};
    float radius = 0.0f;
    bool isValid = false;
};

// モデルの頂点から中心と範囲を計算し、照準と弾の判定に使う大きさを求める
ModelAimBounds CalculateModelAimBounds(const Model* model)
{
    if (!model || model->GetVertices().empty()) {
        return {};
    }

    const auto& vertices = model->GetVertices();
    Math::Vector3 minPosition{
        vertices.front().position.x,
        vertices.front().position.y,
        vertices.front().position.z
    };
    Math::Vector3 maxPosition = minPosition;

    for (const auto& vertex : vertices) {
        minPosition.x = (std::min)(minPosition.x, vertex.position.x);
        minPosition.y = (std::min)(minPosition.y, vertex.position.y);
        minPosition.z = (std::min)(minPosition.z, vertex.position.z);
        maxPosition.x = (std::max)(maxPosition.x, vertex.position.x);
        maxPosition.y = (std::max)(maxPosition.y, vertex.position.y);
        maxPosition.z = (std::max)(maxPosition.z, vertex.position.z);
    }

    ModelAimBounds bounds{};
    bounds.center = {
        (minPosition.x + maxPosition.x) * 0.5f,
        (minPosition.y + maxPosition.y) * 0.5f,
        (minPosition.z + maxPosition.z) * 0.5f
    };

    float radiusSq = 0.0f;
    for (const auto& vertex : vertices) {
        const float dx = vertex.position.x - bounds.center.x;
        const float dy = vertex.position.y - bounds.center.y;
        const float dz = vertex.position.z - bounds.center.z;
        radiusSq = (std::max)(radiusSq, dx * dx + dy * dy + dz * dz);
    }

    bounds.radius = std::sqrt(radiusSq);
    bounds.isValid = true;
    return bounds;
}

} // namespace

// 機種と入場方法から機体の描画、HP、シールド、当たり判定の初期値を決める
void Enemy::Initialize(
    Object3dCommon* object3dCommon,
    Model* model,
    const Math::Vector3& position,
    Behavior behavior,
    EntryStyle entryStyle,
    int maxHpOverride,
    float scaleMultiplier,
    const char* textureOverride)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(model);
    flight_.Reset(position, behavior, entryStyle);
    lifeState_ = LifeState::Alive;
    departureElapsed_ = 0.0f;
    hitFlashTimer_ = 0;
    hitFlashDuration_ = 1;
    hitFlashStrength_ = 1.0f;
    visualScaleRate_ = 0.42f;
    fireControl_ = {};
    attackTelegraphRate_ = 0.0f;

    switch (behavior) {
    case Behavior::Boss:
        baseScale_ = { 1.15f, 1.15f, 1.15f };
        baseColor_ = { 1.0f, 0.96f, 1.0f, 1.0f };
        maxHp_ = 52;
        collisionRadius_ = 4.2f;
        visualScaleRate_ = 0.34f;
        break;
    case Behavior::Swoop:
        baseScale_ = { 0.78f, 0.78f, 0.78f };
        baseColor_ = { 0.98f, 1.0f, 1.0f, 1.0f };
        maxHp_ = 4;
        collisionRadius_ = 1.35f;
        break;
    case Behavior::StrafeShooter:
        baseScale_ = { 0.46f, 0.46f, 0.46f };
        baseColor_ = { 1.0f, 0.98f, 0.94f, 1.0f };
        maxHp_ = 5;
        collisionRadius_ = 1.65f;
        break;
    case Behavior::DiveBomber:
        baseScale_ = { 0.64f, 0.64f, 0.64f };
        baseColor_ = { 0.94f, 0.98f, 1.0f, 1.0f };
        maxHp_ = 5;
        collisionRadius_ = 1.55f;
        break;
    case Behavior::Crossfire:
        baseScale_ = { 0.52f, 0.52f, 0.52f };
        baseColor_ = { 1.0f, 0.94f, 0.98f, 1.0f };
        maxHp_ = 6;
        collisionRadius_ = 1.55f;
        break;
    case Behavior::Sniper:
        baseScale_ = { 0.54f, 0.54f, 0.54f };
        baseColor_ = { 1.0f, 0.48f, 0.62f, 1.0f };
        maxHp_ = 6;
        collisionRadius_ = 1.55f;
        break;
    case Behavior::Shield:
        baseScale_ = { 0.70f, 0.70f, 0.70f };
        baseColor_ = { 0.48f, 0.88f, 1.0f, 1.0f };
        maxHp_ = 10;
        shieldMaxHp_ = 3;
        collisionRadius_ = 1.90f;
        break;
    case Behavior::Support:
        baseScale_ = { 0.56f, 0.56f, 0.56f };
        baseColor_ = { 0.56f, 1.0f, 0.52f, 1.0f };
        maxHp_ = 7;
        collisionRadius_ = 1.60f;
        break;
    case Behavior::Formation:
    default:
        baseScale_ = { 0.42f, 0.42f, 0.42f };
        baseColor_ = { 1.0f, 0.96f, 0.94f, 1.0f };
        maxHp_ = 3;
        collisionRadius_ = 1.35f;
        break;
    }

    if (maxHpOverride > 0) {
        maxHp_ = maxHpOverride;
    }
    const float safeScaleMultiplier =
        std::clamp(scaleMultiplier, 0.25f, 3.0f);
    baseScale_.x *= safeScaleMultiplier;
    baseScale_.y *= safeScaleMultiplier;
    baseScale_.z *= safeScaleMultiplier;
    collisionRadius_ *= safeScaleMultiplier;
    hp_ = maxHp_;
    shieldHp_ = shieldMaxHp_;

    const ModelAimBounds aimBounds = CalculateModelAimBounds(model);
    if (aimBounds.isValid) {
        const float maxScale =
            (std::max)(baseScale_.x, (std::max)(baseScale_.y, baseScale_.z));
        aimLocalCenter_ = aimBounds.center;
        aimRadius_ = (std::max)(collisionRadius_, aimBounds.radius * maxScale);
    } else {
        aimLocalCenter_ = { 0.0f, 0.0f, 0.0f };
        aimRadius_ = collisionRadius_;
    }

    translate_ = position;
    object_->SetScale(baseScale_);
    object_->SetColor(baseColor_);
    // 遠くの機体は陰側を潰さず、白い反射を抑えて赤・紫などの識別色を残す
    object_->SetLightingMode(2);
    if (behavior == Behavior::Boss) {
        object_->SetEnvironmentCoefficient(0.030f);
        object_->SetShininess(72.0f);
        object_->SetSpecularColor({ 0.24f, 0.19f, 0.30f });
        object_->SetRoughness(0.56f);
        object_->SetMetallic(0.18f);
    } else {
        object_->SetEnvironmentCoefficient(0.020f);
        object_->SetShininess(64.0f);
        object_->SetSpecularColor({ 0.22f, 0.16f, 0.18f });
        object_->SetRoughness(0.64f);
        object_->SetMetallic(0.10f);
    }
    if (textureOverride && textureOverride[0] != '\0') {
        object_->SetTextureFilePath(textureOverride);
    }
    object_->SetTranslate(translate_);
    object_->SetRotate({ 0.0f, 0.0f, 0.0f });
    object_->Update();
}

// 現在のレール位置と時間倍率で飛行を進め、取得した姿勢を機体の描画に反映する
void Enemy::Update(float railDistance, float timeScale)
{
    if (lifeState_ != LifeState::Alive || !object_) {
        return;
    }

    if (hitFlashTimer_ > 0) { --hitFlashTimer_; }
    const auto pose = flight_.Advance(railDistance, timeScale, attackTelegraphRate_, fireControl_);
    translate_ = pose.position;
    if (pose.escaped) {
        lifeState_ = LifeState::Escaped;
        return;
    }
    UpdateAppearance(pose);
}

// 飛行姿勢に攻撃予告と被弾の色・大きさの変化を重ね、描画オブジェクトに渡す
void Enemy::UpdateAppearance(EnemyFlight::Pose pose)
{
    const float motionClock = flight_.MotionClock();
    const float phase = flight_.Phase();
    const bool hitReacting = hitFlashTimer_ > 0;
    const float hitRate = hitReacting ?
        Clamp01(
            static_cast<float>(hitFlashTimer_) /
            static_cast<float>((std::max)(hitFlashDuration_, 1))) :
        0.0f;
    const float hitElapsed = 1.0f - hitRate;
    const float hitOscillation =
        std::sin(hitElapsed * kPi * 3.0f) * hitRate;
    const float hitPunch = hitReacting ?
        (0.070f * hitRate + 0.038f * hitOscillation) * hitFlashStrength_ :
        0.0f;
    const float flashScale = 1.0f + hitPunch;
    Math::Vector4 color = baseColor_;
    color.w *= std::clamp(pose.opacity, 0.18f, 1.0f);
    if (attackTelegraphRate_ > 0.0f) {
        const float telegraphRate = Clamp01(attackTelegraphRate_);
        const float chargePulse =
            0.5f + 0.5f * std::sin(motionClock * (0.42f + telegraphRate * 0.38f));
        const float flashStrength = GetBehavior() == Behavior::Sniper ?
            0.08f + telegraphRate * 0.12f :
            std::clamp(0.12f + telegraphRate * 0.42f + chargePulse * 0.12f, 0.0f, 0.72f);
        color.x = Lerp(color.x, 1.86f, flashStrength);
        color.y = Lerp(color.y, 0.34f + telegraphRate * 0.36f, flashStrength);
        color.z = Lerp(color.z, 0.20f, flashStrength * 0.82f);
        if (GetBehavior() != Behavior::Sniper) {
            pose.scale += (0.015f + chargePulse * 0.020f) * telegraphRate;
            pose.rotation.x -= telegraphRate * 0.045f;
        }
    }
    if (GetBehavior() == Behavior::Shield && shieldHp_ > 0) {
        const float shieldPulse =
            0.5f + 0.5f * std::sin(motionClock * 0.18f + phase);
        color.x = Lerp(color.x, 0.34f, 0.28f + shieldPulse * 0.10f);
        color.y = Lerp(color.y, 1.16f, 0.28f + shieldPulse * 0.12f);
        color.z = Lerp(color.z, 1.72f, 0.34f + shieldPulse * 0.14f);
        pose.scale += shieldPulse * 0.025f;
    }
    if (GetBehavior() == Behavior::Support) {
        const float supportPulse =
            0.5f + 0.5f * std::sin(motionClock * 0.20f + phase);
        color.x = Lerp(color.x, 0.76f, 0.18f + supportPulse * 0.10f);
        color.y = Lerp(color.y, 1.58f, 0.30f + supportPulse * 0.14f);
        color.z = Lerp(color.z, 0.38f, 0.16f + supportPulse * 0.08f);
        pose.scale += supportPulse * 0.035f;
    }
    if (GetBehavior() == Behavior::Crossfire && 62 <= flight_.Age() && flight_.Age() < 94) {
        const float telegraphPulse =
            0.24f + 0.18f * (std::sin(motionClock * 0.42f) * 0.5f + 0.5f);
        color.x = Lerp(color.x, 1.65f, telegraphPulse);
        color.y = Lerp(color.y, 0.18f, telegraphPulse);
        color.z = Lerp(color.z, 0.72f, telegraphPulse);
    }
    if (hitReacting) {
        const float flash =
            std::clamp(hitRate * hitRate * hitRate * hitFlashStrength_, 0.0f, 1.0f);
        const Math::Vector4 flashColor =
            GetBehavior() == Behavior::Boss ?
            Math::Vector4{ 2.02f, 1.52f, 2.18f, 1.0f } :
            Math::Vector4{ 2.10f, 1.92f, 1.48f, 1.0f };
        color.x = Lerp(color.x, flashColor.x, flash);
        color.y = Lerp(color.y, flashColor.y, flash);
        color.z = Lerp(color.z, flashColor.z, flash);
        pose.rotation.z += hitOscillation * 0.055f * hitFlashStrength_;
    }
    visualScaleRate_ = std::clamp(pose.scale, 0.34f, 1.12f);
    object_->SetScale({
        baseScale_.x * visualScaleRate_ * flashScale,
        baseScale_.y * visualScaleRate_ * flashScale,
        baseScale_.z * visualScaleRate_ * flashScale
    });
    object_->SetColor(color);
    object_->SetTranslate(translate_);
    object_->SetRotate(pose.rotation);
    object_->Update();
}

// 自機の敗北後に使う離脱飛行　通常攻撃を終わらせ、実時間で画面の奥へ飛び去らせる
void Enemy::UpdateDeparture(float deltaSeconds)
{
    if (!object_ || IsDead()) { return; }
    if (lifeState_ == LifeState::Alive) {
        lifeState_ = LifeState::Departing;
        departureElapsed_ = 0.0f;
        departureOrigin_ = translate_;
        departureRotation_ = object_->GetRotate();
        departureColor_ = object_->GetColor();
        fireControl_ = {};
        attackTelegraphRate_ = 0.0f;
        flight_.SetBossRecoveryRate(0.0f);
    }
    constexpr float duration = 0.90f;
    departureElapsed_ = (std::min)(duration, departureElapsed_ + std::clamp(deltaSeconds, 0.0f, 0.10f));
    const float t = departureElapsed_ / duration;
    const float turn = SmoothStep(t / 0.35f);
    const float side = departureOrigin_.x < 0.0f ? -1.0f : 1.0f;
    const float spread = IsBoss() ? 0.65f : 1.0f;
    translate_ = { departureOrigin_.x + side * spread * (6.0f * t + 9.0f * t * t),
        departureOrigin_.y + spread * 4.0f * t * t,
        departureOrigin_.z + 8.0f * t + 72.0f * t * t };
    const Math::Vector3 rotation{ departureRotation_.x * (1.0f - turn),
        departureRotation_.y + std::remainder(-departureRotation_.y, 2.0f * kPi) * turn,
        Lerp(departureRotation_.z, -side * 0.38f * spread, turn) };
    auto color = departureColor_;
    color.w *= 1.0f - SmoothStep((t - 0.60f) / 0.40f);
    object_->SetTranslate(translate_);
    object_->SetRotate(rotation);
    object_->SetColor(color);
    object_->Update();
    if (departureElapsed_ >= duration) { lifeState_ = LifeState::Departed; }
}

// 有効な描画オブジェクトがあれば機体を描く　離脱中は飛び去る姿を表示し、撃破・逃走後は描画を止める
void Enemy::Draw()
{
    if (!IsDead() && object_) {
        object_->Draw();
    }
}

// 光源の行列で機体を描き、影用の深度を書き込む　描画対象の条件は通常描画と同じ
void Enemy::DrawShadow(const Math::Matrix4x4& lightViewProjection)
{
    if (!IsDead() && object_) {
        object_->DrawShadow(lightViewProjection);
    }
}

// 生存中の敵を撃破済みにする　次の敵更新で参照を解除して一覧から外せる状態になる
void Enemy::Kill()
{
    if (lifeState_ != LifeState::Alive) {
        return;
    }

    lifeState_ = LifeState::Destroyed;
}

// シールドがあれば先に消費し、残りをHPから引く　HPが尽きたときに撃破を返す
bool Enemy::Damage(int damage)
{
    if (lifeState_ != LifeState::Alive) {
        return false;
    }

    const int requestedDamage = (std::max)(damage, 0);
    int appliedDamage = requestedDamage;
    if (GetBehavior() == Behavior::Shield && shieldHp_ > 0 && appliedDamage > 0) {
        const int absorbedDamage = (std::min)(shieldHp_, appliedDamage);
        shieldHp_ -= absorbedDamage;
        appliedDamage -= absorbedDamage;
    }
    hp_ = (std::max)(0, hp_ - appliedDamage);
    hitFlashDuration_ = GetBehavior() == Behavior::Boss ? 14 :
        (requestedDamage >= 3 ? 12 : 10);
    hitFlashTimer_ = hitFlashDuration_;
    hitFlashStrength_ = std::clamp(
        0.84f + static_cast<float>(requestedDamage) * 0.10f,
        0.90f,
        GetBehavior() == Behavior::Boss ? 1.18f : 1.28f);
    if (hp_ <= 0) {
        Kill();
        return true;
    }
    return false;
}

// 生存中で、入場時の見た目が十分な大きさになっていれば狙える　ボスには登場中の猶予もある
bool Enemy::IsTargetable() const
{
    // 大型機が遠方から拡大表示されている途中はロック・被弾対象にしない
    // 戦闘位置へ到着してから攻防を開始する　戦闘中の無敵やHP増加は追加しない
    return lifeState_ == LifeState::Alive && visualScaleRate_ >= 0.46f &&
        (GetBehavior() != Behavior::Boss || flight_.Age() >= 96);
}

// モデルから求めた中心を現在の拡大率で調整し、射撃や衝突判定の基準になる位置を返す
Math::Vector3 Enemy::GetAimPosition() const
{
    const float scaleRate = (std::max)(visualScaleRate_, 0.30f);
    return {
        translate_.x + aimLocalCenter_.x * baseScale_.x * scaleRate,
        translate_.y + aimLocalCenter_.y * baseScale_.y * scaleRate,
        translate_.z + aimLocalCenter_.z * baseScale_.z * scaleRate
    };
}

// ボスのローカルな銃口位置を機体の姿勢に合わせて変換し、ワールド上の発射位置を得る
Math::Vector3 Enemy::GetBossMuzzlePosition(float side) const
{
    const auto center = GetAimPosition();
    if (!object_ || !IsBoss()) { return center; }
    const auto rotation = Math::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, object_->GetRotate(), {});
    const Math::Vector3 local{ -side * 2.2f, side == 0.0f ? -0.45f : 0.25f, 0.0f };
    return { center.x + local.x * rotation.m[0][0] + local.y * rotation.m[1][0],
        center.y + local.x * rotation.m[0][1] + local.y * rotation.m[1][1],
        center.z + local.x * rotation.m[0][2] + local.y * rotation.m[1][2] };
}

// モデルに合わせて決めた半径に、入場などで変化する現在の表示スケールを反映する
float Enemy::GetAimRadius() const
{
    return aimRadius_ * (std::max)(visualScaleRate_, 0.48f);
}

// 狙える状態の敵に対し、出現からの経過と機種ごとの射撃可能な区間を調べる
bool Enemy::CanShoot() const
{
    if (!IsTargetable()) {
        return false;
    }

    const int age = flight_.Age();
    switch (GetBehavior()) {
    case Behavior::Boss:
        return age >= 106;
    case Behavior::StrafeShooter:
        return 34 <= age && age <= 250;
    case Behavior::DiveBomber:
        return 38 <= age && age <= 218;
    case Behavior::Crossfire:
        return 52 <= age && age <= 212;
    case Behavior::Sniper:
        return 72 <= age && age <= 304;
    case Behavior::Shield:
        return 64 <= age && age <= 318;
    case Behavior::Support:
        return false;
    case Behavior::Swoop:
        return 30 <= age && age <= 180;
    case Behavior::Formation:
    default:
        return 30 <= age && age <= 236;
    }
}

// 攻撃予告の進み具合を0〜1に収め、機体を光らせる強さとして保持する
void Enemy::SetAttackTelegraphRate(float rate)
{
    attackTelegraphRate_ = std::clamp(rate, 0.0f, 1.0f);
}
