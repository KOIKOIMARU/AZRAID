#include "app/Enemy.h"

#include "engine/3d/Model.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.1415926535f;
constexpr float kStandardEnemyActionTimeScale = 0.66f;

float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float Lerp(float start, float end, float rate)
{
    return start + (end - start) * rate;
}

float EaseOutCubic(float value)
{
    const float t = Clamp01(value);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float SmoothStep(float value)
{
    const float t = Clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

float SignNonZero(float value)
{
    return value < 0.0f ? -1.0f : 1.0f;
}

struct ModelAimBounds {
    Math::Vector3 center{};
    float radius = 0.0f;
    bool isValid = false;
};

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
    behavior_ = behavior;
    entryStyle_ = entryStyle;
    lifeState_ = LifeState::Alive;
    age_ = 0;
    ageTimer_ = 0.0f;
    hitFlashTimer_ = 0;
    hitFlashDuration_ = 1;
    hitFlashStrength_ = 1.0f;
    moveTimer_ = 0.0f;
    visualScaleRate_ = 0.42f;
    fireControl_ = {};
    sniperBracePosition_ = {};
    sniperBraced_ = false;
    trainingTarget_ = false;
    riftCarrier_ = false;
    attackTelegraphRate_ = 0.0f;
    bossRecoveryRate_ = 0.0f;

    switch (behavior_) {
    case Behavior::Boss:
        baseScale_ = { 1.15f, 1.15f, 1.15f };
        baseColor_ = { 1.0f, 0.96f, 1.0f, 1.0f };
        maxHp_ = 52;
        horizontalAmplitude_ = 2.2f;
        verticalAmplitude_ = 0.55f;
        collisionRadius_ = 4.2f;
        visualScaleRate_ = 0.34f;
        break;
    case Behavior::Swoop:
        baseScale_ = { 0.78f, 0.78f, 0.78f };
        baseColor_ = { 0.98f, 1.0f, 1.0f, 1.0f };
        maxHp_ = 4;
        horizontalAmplitude_ = 0.75f;
        verticalAmplitude_ = 0.35f;
        collisionRadius_ = 1.35f;
        break;
    case Behavior::StrafeShooter:
        baseScale_ = { 0.46f, 0.46f, 0.46f };
        baseColor_ = { 1.0f, 0.98f, 0.94f, 1.0f };
        maxHp_ = 5;
        horizontalAmplitude_ = 1.9f;
        verticalAmplitude_ = 0.25f;
        collisionRadius_ = 1.65f;
        break;
    case Behavior::DiveBomber:
        baseScale_ = { 0.64f, 0.64f, 0.64f };
        baseColor_ = { 0.94f, 0.98f, 1.0f, 1.0f };
        maxHp_ = 5;
        horizontalAmplitude_ = 2.4f;
        verticalAmplitude_ = 1.25f;
        collisionRadius_ = 1.55f;
        break;
    case Behavior::Crossfire:
        baseScale_ = { 0.52f, 0.52f, 0.52f };
        baseColor_ = { 1.0f, 0.94f, 0.98f, 1.0f };
        maxHp_ = 6;
        horizontalAmplitude_ = 1.2f;
        verticalAmplitude_ = 0.45f;
        collisionRadius_ = 1.55f;
        break;
    case Behavior::Sniper:
        baseScale_ = { 0.54f, 0.54f, 0.54f };
        baseColor_ = { 1.0f, 0.48f, 0.62f, 1.0f };
        maxHp_ = 6;
        horizontalAmplitude_ = 0.65f;
        verticalAmplitude_ = 0.28f;
        collisionRadius_ = 1.55f;
        break;
    case Behavior::Shield:
        baseScale_ = { 0.70f, 0.70f, 0.70f };
        baseColor_ = { 0.48f, 0.88f, 1.0f, 1.0f };
        maxHp_ = 10;
        shieldMaxHp_ = 3;
        horizontalAmplitude_ = 1.1f;
        verticalAmplitude_ = 0.32f;
        collisionRadius_ = 1.90f;
        break;
    case Behavior::Support:
        baseScale_ = { 0.56f, 0.56f, 0.56f };
        baseColor_ = { 0.56f, 1.0f, 0.52f, 1.0f };
        maxHp_ = 7;
        horizontalAmplitude_ = 0.72f;
        verticalAmplitude_ = 0.34f;
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

    baseTranslate_ = position;
    translate_ = position;
    phase_ = position.x * 0.75f + position.y * 1.35f;
    object_->SetScale(baseScale_);
    object_->SetColor(baseColor_);
    // 遠くの機体は陰側を潰さず、白い反射を抑えて赤・紫などの識別色を残す。
    object_->SetLightingMode(2);
    if (behavior_ == Behavior::Boss) {
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

void Enemy::Update(float railDistance, float timeScale)
{
    if (IsDead() || !object_) {
        return;
    }

    const float motionScale = std::clamp(timeScale, 0.05f, 1.0f);
    const float actionTimeScale =
        behavior_ == Behavior::Boss ?
            motionScale : motionScale * kStandardEnemyActionTimeScale;
    ageTimer_ += actionTimeScale;
    if (trainingTarget_) { ageTimer_ = (std::min)(ageTimer_, 110.0f); }
    age_ = static_cast<int>(std::floor(ageTimer_));
    // ボスは構え・反動中に不規則な横揺れを止め、攻撃と反撃の狙いを読みやすくする。
    if (behavior_ != Behavior::Boss || (attackTelegraphRate_ <= 0.0f && bossRecoveryRate_ <= 0.0f)) {
        moveTimer_ += actionTimeScale;
    }
    if (hitFlashTimer_ > 0) {
        --hitFlashTimer_;
    }

    Math::Vector3 objectRotate{};
    float visualScaleTarget = 1.0f;
    float colorAlphaRate = 1.0f;

    switch (behavior_) {
    case Behavior::Boss: {
        const float entryRate = EaseOutCubic(static_cast<float>(age_) / 96.0f);
        const float fightRate = Clamp01((moveTimer_ - 96.0f) / 180.0f);
        const float fightTime = (std::max)(0.0f, moveTimer_ - 96.0f);
        const float lungeWave =
            std::pow((std::max)(0.0f, std::sin(fightTime * 0.034f + 1.10f)), 4.0f);
        const float swayA =
            std::sin(moveTimer_ * 0.022f + phase_) *
            Lerp(0.45f, 4.60f, fightRate);
        const float swayB =
            std::sin(moveTimer_ * 0.009f + phase_ * 0.37f) *
            Lerp(0.0f, 1.60f, fightRate);
        translate_.x = swayA + swayB;
        translate_.y =
            Lerp(baseTranslate_.y + 4.00f, baseTranslate_.y, entryRate) +
            std::sin(moveTimer_ * 0.026f + phase_) *
                Lerp(0.20f, 1.05f, fightRate) +
            lungeWave * 0.45f;
        translate_.z =
            railDistance +
            Lerp(86.0f, 36.0f, entryRate) +
            std::cos(moveTimer_ * 0.016f + phase_) * Lerp(0.0f, 3.80f, fightRate) -
            lungeWave * 8.50f;
        visualScaleTarget = Lerp(0.34f, 1.0f, entryRate);
        colorAlphaRate = Lerp(0.20f, 1.0f, entryRate);
        objectRotate.x =
            std::sin(moveTimer_ * 0.020f + phase_) * 0.070f -
            lungeWave * 0.080f;
        objectRotate.y =
            kPi +
            std::sin(moveTimer_ * 0.016f + phase_) * 0.130f +
            translate_.x * 0.010f;
        objectRotate.z =
            -translate_.x * 0.026f +
            std::sin(moveTimer_ * 0.024f + phase_) * 0.090f;
        if (bossRecoveryRate_ > 0.0f) {
            const float elapsed = 1.0f - Clamp01(bossRecoveryRate_);
            const float recoil = (std::max)(0.0f, 1.0f - elapsed / 0.12f);
            const float settle = std::sin(elapsed * kPi);
            translate_.z += recoil * 2.0f - settle * 1.8f;
            objectRotate.x += recoil * 0.14f + settle * 0.05f;
        } else {
            objectRotate.x -= attackTelegraphRate_ * 0.08f;
        }
        break;
    }
    case Behavior::Swoop: {
        const float entryRate = EaseOutCubic(static_cast<float>(age_) / 40.0f);
        const float passRate = SmoothStep(static_cast<float>(age_ - 14) / 182.0f);
        const float side =
            entryStyle_ == EntryStyle::RightSweep ? -1.0f :
            entryStyle_ == EntryStyle::LeftSweep ? 1.0f :
            SignNonZero(baseTranslate_.x);
        const float curve = std::sin(passRate * kPi);
        const float laneX =
            side * Lerp(10.8f, -11.6f, passRate) -
            side * curve * 2.8f;
        const float laneY =
            baseTranslate_.y +
            curve * 3.2f -
            passRate * 1.55f;
        const float laneZ =
            Lerp(48.0f, 30.0f, curve) +
            passRate * 12.0f;
        const float exitRunRate = SmoothStep(static_cast<float>(age_ - 196) / 72.0f);
        translate_.x =
            Lerp(side * 4.4f, laneX, entryRate);
        translate_.y =
            Lerp(baseTranslate_.y + 2.5f, laneY, entryRate) +
            std::sin(moveTimer_ * 0.064f + phase_) * 0.20f;
        translate_.z =
            railDistance +
            Lerp(70.0f, laneZ, entryRate);
        translate_.x += -side * exitRunRate * 64.0f;
        translate_.y += exitRunRate * 34.0f;
        translate_.z += exitRunRate * 18.0f;
        visualScaleTarget =
            Lerp(0.42f, 1.0f, entryRate) +
            std::sin(entryRate * kPi) * 0.05f;
        colorAlphaRate = Lerp(0.28f, 1.0f, entryRate);
        objectRotate.y =
            kPi +
            side * Lerp(0.35f, -0.25f, passRate) -
            side * exitRunRate * 0.78f;
        objectRotate.z = -side * (0.18f + curve * 0.40f + exitRunRate * 0.56f);
        if (exitRunRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::StrafeShooter: {
        const float attackRate = EaseOutCubic(static_cast<float>(age_) / 72.0f);
        const float holdRate = Clamp01(static_cast<float>(age_ - 72) / 148.0f);
        const float exitRate = SmoothStep(static_cast<float>(age_ - 220) / 96.0f);
        const float exitSide = SignNonZero(std::sin(phase_));
        const float entryArc =
            entryStyle_ == EntryStyle::PopShooter ?
            std::sin(attackRate * kPi) :
            0.0f;
        const float exitRunRate = SmoothStep(static_cast<float>(age_ - 316) / 72.0f);
        translate_.x =
            Lerp(
                baseTranslate_.x * 2.2f + SignNonZero(baseTranslate_.x) * 2.2f,
                baseTranslate_.x,
                attackRate) +
            std::sin(moveTimer_ * 0.050f + phase_) * Lerp(1.7f, 4.4f, holdRate) +
            exitSide * exitRate * 11.0f;
        translate_.y =
            Lerp(baseTranslate_.y + 3.3f, baseTranslate_.y * 0.30f, attackRate) +
            std::cos(moveTimer_ * 0.042f + phase_) * 0.42f +
            entryArc * 1.05f +
            exitRate * 4.8f;
        translate_.z =
            railDistance +
            Lerp(68.0f, 28.0f, attackRate) +
            std::cos(moveTimer_ * 0.030f + phase_) * 1.3f +
            exitRate * 14.0f;
        translate_.x += exitSide * exitRunRate * 56.0f;
        translate_.y += exitRunRate * 30.0f;
        translate_.z += exitRunRate * 18.0f;
        visualScaleTarget =
            Lerp(0.44f, 1.0f, attackRate) +
            std::sin(attackRate * kPi) * 0.06f;
        colorAlphaRate = Lerp(0.24f, 1.0f, attackRate);
        objectRotate.y = kPi - translate_.x * 0.030f;
        objectRotate.z =
            std::sin(moveTimer_ * 0.032f + phase_) * 0.18f -
            exitSide * (exitRate * 0.35f + exitRunRate * 0.55f);
        if (exitRunRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::DiveBomber: {
        const float entryRate = EaseOutCubic(static_cast<float>(age_) / 66.0f);
        const float attackRate = SmoothStep(static_cast<float>(age_ - 28) / 168.0f);
        const float exitRate = SmoothStep(static_cast<float>(age_ - 224) / 92.0f);
        const float exitRunRate = SmoothStep(static_cast<float>(age_ - 300) / 70.0f);
        const float side =
            entryStyle_ == EntryStyle::RightSweep ? -1.0f :
            entryStyle_ == EntryStyle::LeftSweep ? 1.0f :
            SignNonZero(baseTranslate_.x);
        const float attackArc = std::sin(attackRate * kPi);
        const float weave =
            std::sin(moveTimer_ * 0.060f + phase_) *
            Lerp(0.35f, horizontalAmplitude_, entryRate);

        translate_.x =
            Lerp(side * 7.8f, baseTranslate_.x, entryRate) +
            weave -
            side * attackArc * 2.9f -
            side * exitRate * 11.0f -
            side * exitRunRate * 56.0f;
        translate_.y =
            Lerp(baseTranslate_.y + 6.2f, baseTranslate_.y, entryRate) -
            attackArc * 1.55f +
            std::cos(moveTimer_ * 0.044f + phase_) * 0.36f +
            exitRate * 4.4f +
            exitRunRate * 30.0f;
        translate_.z =
            railDistance +
            Lerp(78.0f, 32.0f, entryRate) -
            attackArc * 6.0f +
            std::sin(moveTimer_ * 0.026f + phase_) * 1.1f +
            exitRate * 13.0f +
            exitRunRate * 22.0f;
        visualScaleTarget =
            Lerp(0.38f, 1.0f, entryRate) +
            attackArc * 0.055f;
        colorAlphaRate = Lerp(0.20f, 1.0f, entryRate);
        objectRotate.x =
            -attackArc * 0.32f +
            std::sin(moveTimer_ * 0.035f + phase_) * 0.06f;
        objectRotate.y =
            kPi +
            side * Lerp(0.42f, -0.24f, attackRate) -
            side * exitRate * 0.44f;
        objectRotate.z =
            -side * (0.18f + attackArc * 0.46f + exitRunRate * 0.56f);
        if (exitRunRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::Crossfire: {
        const float side = SignNonZero(baseTranslate_.x);
        const float entryRate = EaseOutCubic(static_cast<float>(age_) / 62.0f);
        const float crossRate = SmoothStep(static_cast<float>(age_ - 94) / 118.0f);
        const float exitRate = SmoothStep(static_cast<float>(age_ - 204) / 78.0f);
        const float crossArc = std::sin(crossRate * kPi);

        translate_.x =
            Lerp(side * 13.5f, side * 4.25f, entryRate) -
            side * crossRate * 13.8f -
            side * exitRate * 60.0f;
        translate_.y =
            Lerp(baseTranslate_.y + 3.8f, baseTranslate_.y, entryRate) +
            crossArc * 1.35f +
            std::sin(moveTimer_ * 0.070f + phase_) * 0.22f +
            exitRate * 34.0f;
        translate_.z =
            railDistance +
            Lerp(72.0f, 32.0f, entryRate) -
            crossArc * 3.8f +
            exitRate * 18.0f;
        visualScaleTarget =
            Lerp(0.40f, 1.0f, entryRate) +
            std::sin(entryRate * kPi) * 0.05f;
        colorAlphaRate = Lerp(0.22f, 1.0f, entryRate);
        objectRotate.x = -crossArc * 0.12f;
        objectRotate.y =
            kPi - side * Lerp(0.34f, 0.62f, crossRate) -
            side * exitRate * 0.68f;
        objectRotate.z =
            -side * (0.24f + crossArc * 0.44f + exitRate * 0.52f);
        if (exitRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::Sniper: {
        const float entryRate = EaseOutCubic(static_cast<float>(age_) / 72.0f);
        const float exitRate = SmoothStep(static_cast<float>(age_ - 304) / 86.0f);
        const float exitRunRate = SmoothStep(static_cast<float>(age_ - 390) / 72.0f);
        const float exitSide = SignNonZero(baseTranslate_.x);
        translate_.x =
            Lerp(baseTranslate_.x * 1.9f, baseTranslate_.x, entryRate) +
            std::sin(moveTimer_ * 0.022f + phase_) * 0.62f +
            exitSide * exitRate * 8.0f +
            exitSide * exitRunRate * 58.0f;
        translate_.y =
            Lerp(baseTranslate_.y + 4.6f, baseTranslate_.y + 1.15f, entryRate) +
            std::cos(moveTimer_ * 0.026f + phase_) * 0.30f +
            exitRate * 5.0f +
            exitRunRate * 30.0f;
        translate_.z =
            railDistance +
            Lerp(82.0f, 42.0f, entryRate) +
            std::sin(moveTimer_ * 0.018f + phase_) * 1.1f +
            exitRunRate * 18.0f;
        visualScaleTarget =
            Lerp(0.40f, 1.0f, entryRate) +
            std::sin(entryRate * kPi) * 0.05f;
        colorAlphaRate = Lerp(0.20f, 1.0f, entryRate);
        objectRotate.y =
            kPi - translate_.x * 0.016f - exitSide * exitRunRate * 0.72f;
        objectRotate.z =
            std::sin(moveTimer_ * 0.020f + phase_) * 0.08f -
            exitSide * exitRunRate * 0.58f;
        // 構えると横揺れを止める。二射後は前へ沈み、照準を合わせ直せる隙を残す。
        // レール相対座標なので高速移動・スロー・停止でも背景に置き去りにならない。
        const float recovery = fireControl_.RecoveryElapsed();
        const bool attackPose = age_ <= 304 &&
            (fireControl_.IsBraced() || (recovery >= 0.0f && recovery < 90.0f));
        if (attackPose) {
            if (!sniperBraced_) {
                sniperBracePosition_ = { translate_.x, translate_.y, translate_.z - railDistance };
                sniperBraced_ = true;
            }
            const float recoveryReturn = recovery >= 66.0f ? SmoothStep((recovery - 66.0f) / 24.0f) : 0.0f;
            const float returnRate = (std::max)(recoveryReturn, SmoothStep(static_cast<float>(age_ - 290) / 14.0f));
            const float settle = recovery >= 0.0f ? SmoothStep(recovery / 20.0f) : 0.0f;
            const float recoil = fireControl_.ShotFlash();
            translate_.x = Lerp(sniperBracePosition_.x, translate_.x, returnRate);
            translate_.y = Lerp(sniperBracePosition_.y - settle * 0.45f, translate_.y, returnRate);
            translate_.z = Lerp(railDistance + sniperBracePosition_.z + recoil * 1.1f - settle * 3.0f,
                translate_.z, returnRate);
            objectRotate.x += (-fireControl_.ChargeRate() * 0.12f + recoil * 0.22f + settle * 0.15f) * (1.0f - returnRate);
            objectRotate.z *= returnRate;
        } else {
            sniperBraced_ = false;
        }
        if (exitRunRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::Shield: {
        const float entryRate = EaseOutCubic(static_cast<float>(age_) / 82.0f);
        const float exitRate = SmoothStep(static_cast<float>(age_ - 338) / 88.0f);
        const float exitRunRate = SmoothStep(static_cast<float>(age_ - 426) / 72.0f);
        const float exitSide = SignNonZero(baseTranslate_.x);
        translate_.x =
            Lerp(baseTranslate_.x * 0.22f, baseTranslate_.x, entryRate) +
            std::sin(moveTimer_ * 0.026f + phase_) * 1.10f +
            exitSide * exitRate * 9.0f +
            exitSide * exitRunRate * 58.0f;
        translate_.y =
            Lerp(baseTranslate_.y + 3.8f, baseTranslate_.y, entryRate) +
            std::cos(moveTimer_ * 0.024f + phase_) * 0.34f +
            exitRate * 5.0f +
            exitRunRate * 30.0f;
        translate_.z =
            railDistance +
            Lerp(84.0f, 34.0f, entryRate) +
            std::sin(moveTimer_ * 0.016f + phase_) * 1.2f +
            exitRunRate * 18.0f;
        visualScaleTarget =
            Lerp(0.36f, 1.0f, entryRate) +
            std::sin(entryRate * kPi) * 0.05f;
        colorAlphaRate = Lerp(0.18f, 1.0f, entryRate);
        objectRotate.y =
            kPi - translate_.x * 0.014f - exitSide * exitRunRate * 0.68f;
        objectRotate.z =
            std::sin(moveTimer_ * 0.021f + phase_) * 0.07f -
            exitSide * exitRunRate * 0.54f;
        if (exitRunRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::Support: {
        const float entryRate = EaseOutCubic(static_cast<float>(age_) / 76.0f);
        const float exitRate = SmoothStep(static_cast<float>(age_ - 324) / 84.0f);
        const float exitRunRate = SmoothStep(static_cast<float>(age_ - 408) / 72.0f);
        const float exitSide = SignNonZero(baseTranslate_.x);
        translate_.x =
            Lerp(baseTranslate_.x * 0.18f, baseTranslate_.x, entryRate) +
            std::sin(moveTimer_ * 0.024f + phase_) * 0.72f +
            exitSide * exitRate * 8.0f +
            exitSide * exitRunRate * 56.0f;
        translate_.y =
            Lerp(baseTranslate_.y + 4.2f, baseTranslate_.y + 1.8f, entryRate) +
            std::cos(moveTimer_ * 0.028f + phase_) * 0.34f +
            exitRate * 5.0f +
            exitRunRate * 30.0f;
        translate_.z =
            railDistance +
            Lerp(82.0f, 38.0f, entryRate) +
            std::sin(moveTimer_ * 0.017f + phase_) * 1.0f +
            exitRunRate * 18.0f;
        visualScaleTarget =
            Lerp(0.38f, 1.0f, entryRate) +
            std::sin(moveTimer_ * 0.11f + phase_) * 0.035f;
        colorAlphaRate = Lerp(0.20f, 1.0f, entryRate);
        objectRotate.y =
            kPi - translate_.x * 0.014f - exitSide * exitRunRate * 0.64f;
        objectRotate.z =
            std::sin(moveTimer_ * 0.026f + phase_) * 0.10f -
            exitSide * exitRunRate * 0.52f;
        if (exitRunRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::Formation:
    default:
    {
        const float entryRate = EaseOutCubic(static_cast<float>(age_) / 58.0f);
        const float holdRate = Clamp01(static_cast<float>(age_ - 58) / 132.0f);
        const float exitRate = SmoothStep(static_cast<float>(age_ - 206) / 96.0f);
        const float exitSide = SignNonZero(baseTranslate_.x);
        const float formationSpread =
            entryStyle_ == EntryStyle::VFormation ?
            std::sin(entryRate * kPi) * std::abs(baseTranslate_.x) * 0.56f :
            0.0f;
        const float exitRunRate = SmoothStep(static_cast<float>(age_ - 302) / 74.0f);
        translate_.x =
            Lerp(baseTranslate_.x * 0.08f, baseTranslate_.x, entryRate) +
            SignNonZero(baseTranslate_.x) * formationSpread +
            std::sin(moveTimer_ * 0.040f + phase_) * Lerp(0.20f, 0.95f, entryRate) +
            std::sin(moveTimer_ * 0.014f + phase_) * 0.42f +
            exitSide * exitRate * 11.5f;
        translate_.y =
            Lerp(baseTranslate_.y + 2.1f, baseTranslate_.y, entryRate) +
            std::cos(moveTimer_ * 0.034f + phase_) * 0.52f +
            exitRate * 4.1f;
        translate_.z =
            railDistance +
            Lerp(70.0f, 30.0f, entryRate) -
            holdRate * 0.8f +
            std::sin(moveTimer_ * 0.022f + phase_) * 1.5f;
        translate_.x += exitSide * exitRunRate * 56.0f;
        translate_.y += exitRunRate * 30.0f;
        translate_.z += exitRunRate * 16.0f;
        if (entryStyle_ == EntryStyle::TightFormation) {
            // 各機固有の位相を使わず、ひとかたまりで入場する。照準を振って順に倒せる間隔。
            translate_.x = baseTranslate_.x * Lerp(0.75f, 1.0f, entryRate) +
                std::sin(moveTimer_ * 0.032f) * 0.38f +
                exitSide * (exitRate * 11.5f + exitRunRate * 56.0f);
            translate_.y = Lerp(baseTranslate_.y + 2.1f, baseTranslate_.y, entryRate) +
                std::cos(moveTimer_ * 0.032f) * 0.24f + exitRate * 4.1f + exitRunRate * 30.0f;
            translate_.z = railDistance + Lerp(70.0f, 30.0f, entryRate) - holdRate * 0.8f + exitRunRate * 16.0f;
        }
        visualScaleTarget =
            Lerp(0.42f, 1.0f, entryRate) +
            std::sin(entryRate * kPi) * 0.055f;
        colorAlphaRate = Lerp(0.26f, 1.0f, entryRate);
        objectRotate.y = kPi - translate_.x * 0.018f;
        objectRotate.z =
            -exitSide * std::sin(entryRate * kPi) * 0.22f +
            std::sin(moveTimer_ * 0.025f + phase_) * 0.10f -
            exitSide * exitRunRate * 0.56f;
        if (exitRunRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    }

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
    color.w *= std::clamp(colorAlphaRate, 0.18f, 1.0f);
    if (attackTelegraphRate_ > 0.0f) {
        const float telegraphRate = Clamp01(attackTelegraphRate_);
        const float chargePulse =
            0.5f + 0.5f * std::sin(moveTimer_ * (0.42f + telegraphRate * 0.38f));
        const float flashStrength = behavior_ == Behavior::Sniper ?
            0.08f + telegraphRate * 0.12f :
            std::clamp(0.12f + telegraphRate * 0.42f + chargePulse * 0.12f, 0.0f, 0.72f);
        color.x = Lerp(color.x, 1.86f, flashStrength);
        color.y = Lerp(color.y, 0.34f + telegraphRate * 0.36f, flashStrength);
        color.z = Lerp(color.z, 0.20f, flashStrength * 0.82f);
        if (behavior_ != Behavior::Sniper) {
            visualScaleTarget += (0.015f + chargePulse * 0.020f) * telegraphRate;
            objectRotate.x -= telegraphRate * 0.045f;
        }
    }
    if (behavior_ == Behavior::Shield && shieldHp_ > 0) {
        const float shieldPulse =
            0.5f + 0.5f * std::sin(moveTimer_ * 0.18f + phase_);
        color.x = Lerp(color.x, 0.34f, 0.28f + shieldPulse * 0.10f);
        color.y = Lerp(color.y, 1.16f, 0.28f + shieldPulse * 0.12f);
        color.z = Lerp(color.z, 1.72f, 0.34f + shieldPulse * 0.14f);
        visualScaleTarget += shieldPulse * 0.025f;
    }
    if (behavior_ == Behavior::Support) {
        const float supportPulse =
            0.5f + 0.5f * std::sin(moveTimer_ * 0.20f + phase_);
        color.x = Lerp(color.x, 0.76f, 0.18f + supportPulse * 0.10f);
        color.y = Lerp(color.y, 1.58f, 0.30f + supportPulse * 0.14f);
        color.z = Lerp(color.z, 0.38f, 0.16f + supportPulse * 0.08f);
        visualScaleTarget += supportPulse * 0.035f;
    }
    if (behavior_ == Behavior::Crossfire && 62 <= age_ && age_ < 94) {
        const float telegraphPulse =
            0.24f + 0.18f * (std::sin(moveTimer_ * 0.42f) * 0.5f + 0.5f);
        color.x = Lerp(color.x, 1.65f, telegraphPulse);
        color.y = Lerp(color.y, 0.18f, telegraphPulse);
        color.z = Lerp(color.z, 0.72f, telegraphPulse);
    }
    if (hitReacting) {
        const float flash =
            std::clamp(hitRate * hitRate * hitRate * hitFlashStrength_, 0.0f, 1.0f);
        const Math::Vector4 flashColor =
            behavior_ == Behavior::Boss ?
            Math::Vector4{ 2.02f, 1.52f, 2.18f, 1.0f } :
            Math::Vector4{ 2.10f, 1.92f, 1.48f, 1.0f };
        color.x = Lerp(color.x, flashColor.x, flash);
        color.y = Lerp(color.y, flashColor.y, flash);
        color.z = Lerp(color.z, flashColor.z, flash);
        objectRotate.z += hitOscillation * 0.055f * hitFlashStrength_;
    }
    visualScaleRate_ = std::clamp(visualScaleTarget, 0.34f, 1.12f);
    object_->SetScale({
        baseScale_.x * visualScaleRate_ * flashScale,
        baseScale_.y * visualScaleRate_ * flashScale,
        baseScale_.z * visualScaleRate_ * flashScale
    });
    object_->SetColor(color);
    object_->SetTranslate(translate_);
    object_->SetRotate(objectRotate);
    object_->Update();
}

void Enemy::Draw()
{
    if (lifeState_ != LifeState::Destroyed &&
        lifeState_ != LifeState::Escaped &&
        object_) {
        object_->Draw();
    }
}

void Enemy::DrawShadow(const Math::Matrix4x4& lightViewProjection)
{
    if (lifeState_ != LifeState::Destroyed &&
        lifeState_ != LifeState::Escaped &&
        object_) {
        object_->DrawShadow(lightViewProjection);
    }
}

void Enemy::Kill()
{
    if (lifeState_ != LifeState::Alive) {
        return;
    }

    lifeState_ = LifeState::Destroyed;
}

bool Enemy::Damage(int damage)
{
    if (lifeState_ != LifeState::Alive) {
        return false;
    }

    const int requestedDamage = (std::max)(damage, 0);
    int appliedDamage = requestedDamage;
    if (behavior_ == Behavior::Shield && shieldHp_ > 0 && appliedDamage > 0) {
        const int absorbedDamage = (std::min)(shieldHp_, appliedDamage);
        shieldHp_ -= absorbedDamage;
        appliedDamage -= absorbedDamage;
    }
    hp_ = (std::max)(0, hp_ - appliedDamage);
    hitFlashDuration_ = behavior_ == Behavior::Boss ? 14 :
        (requestedDamage >= 3 ? 12 : 10);
    hitFlashTimer_ = hitFlashDuration_;
    hitFlashStrength_ = std::clamp(
        0.84f + static_cast<float>(requestedDamage) * 0.10f,
        0.90f,
        behavior_ == Behavior::Boss ? 1.18f : 1.28f);
    if (hp_ <= 0) {
        Kill();
        return true;
    }
    return false;
}

bool Enemy::IsTargetable() const
{
    // 大型機が遠方から拡大表示されている途中はロック・被弾対象にしない。
    // 戦闘位置へ到着してから攻防を開始する。戦闘中の無敵やHP増加は追加しない。
    return lifeState_ == LifeState::Alive && visualScaleRate_ >= 0.46f &&
        (behavior_ != Behavior::Boss || age_ >= 96);
}

Math::Vector3 Enemy::GetAimPosition() const
{
    const float scaleRate = (std::max)(visualScaleRate_, 0.30f);
    return {
        translate_.x + aimLocalCenter_.x * baseScale_.x * scaleRate,
        translate_.y + aimLocalCenter_.y * baseScale_.y * scaleRate,
        translate_.z + aimLocalCenter_.z * baseScale_.z * scaleRate
    };
}

float Enemy::GetAimRadius() const
{
    return aimRadius_ * (std::max)(visualScaleRate_, 0.48f);
}

bool Enemy::CanShoot() const
{
    if (!IsTargetable()) {
        return false;
    }

    switch (behavior_) {
    case Behavior::Boss:
        return age_ >= 106;
    case Behavior::StrafeShooter:
        return 48 <= age_ && age_ <= 250;
    case Behavior::DiveBomber:
        return 58 <= age_ && age_ <= 218;
    case Behavior::Crossfire:
        return 82 <= age_ && age_ <= 182;
    case Behavior::Sniper:
        return 72 <= age_ && age_ <= 304;
    case Behavior::Shield:
        return 88 <= age_ && age_ <= 318;
    case Behavior::Support:
        return false;
    case Behavior::Swoop:
        return 46 <= age_ && age_ <= 160;
    case Behavior::Formation:
    default:
        return 54 <= age_ && age_ <= 210;
    }
}

void Enemy::SetAttackTelegraphRate(float rate)
{
    attackTelegraphRate_ = std::clamp(rate, 0.0f, 1.0f);
}
