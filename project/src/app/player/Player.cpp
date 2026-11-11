#include "app/player/Player.h"
#include "app/runtime/GameRuntimeMath.h"

#include "engine/3d/Model.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kDamageProtectionDuration = 24.0f; // 同じ斉射を連続で受け切らず、立て直す猶予を残す。
// 成功演出で世界が止まっても、移動入力の手応えは残す。
constexpr float kPlayerSlowMinScale = 0.75f;
constexpr float kPlayerHorizontalLimit = 8.9f;
constexpr float kPlayerLowerLimitY = -0.80f;
constexpr float kPlayerUpperLimitY = 5.55f;
constexpr float kPlayerMoveBankAngle = 0.42f;
constexpr float kPlayerMoveYawAngle = 0.055f;
constexpr float kPlayerMovePitchAngle = 0.14f;
constexpr float kPlayerPoseResponse = 0.17f;
// 塗装色を残すため、通常時は薄い青みだけを掛ける。発光はエンジン側で表現する。
constexpr Math::Vector4 kPlayerHullColor{ 0.90f, 0.94f, 1.0f, 1.0f };
constexpr Math::Vector4 kPlayerInvincibleColor{ 0.52f, 0.85f, 1.0f, 1.0f };

// 頂点の最小・最大座標から機体中心の補正値を求め、姿勢を変えたときの回転軸に使う。
Math::Vector3 CalculateModelLocalCenterOffset(const Model* model)
{
    if (!model || model->GetVertices().empty()) {
        return { 0.0f, 0.0f, 0.0f };
    }

    Math::Vector3 minPosition{
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
    };
    Math::Vector3 maxPosition{
        -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)(),
    };

    for (const VertexData& vertex : model->GetVertices()) {
        minPosition.x = (std::min)(minPosition.x, vertex.position.x);
        minPosition.y = (std::min)(minPosition.y, vertex.position.y);
        minPosition.z = (std::min)(minPosition.z, vertex.position.z);
        maxPosition.x = (std::max)(maxPosition.x, vertex.position.x);
        maxPosition.y = (std::max)(maxPosition.y, vertex.position.y);
        maxPosition.z = (std::max)(maxPosition.z, vertex.position.z);
    }

    return {
        (minPosition.x + maxPosition.x) * 0.5f,
        (minPosition.y + maxPosition.y) * 0.5f,
        (minPosition.z + maxPosition.z) * 0.5f,
    };
}

} // namespace

// 自機の描画オブジェクトを用意し、モデルの中心と当たり判定の基準を機体に合わせる。
void Player::Initialize(Object3dCommon* object3dCommon, Model* model)
{
    objectRotate_ = {};
    movementRotate_ = {};
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(model);
    object_->SetScale(objectScale_);
    object_->SetColor(kPlayerHullColor);
    // 面の向きで明暗を分け、空の映り込みで暗い部品まで白くならないようにする。
    object_->SetLightingMode(1);
    object_->SetEnvironmentCoefficient(0.045f);
    object_->SetShininess(112.0f);
    object_->SetSpecularColor({ 0.30f, 0.32f, 0.35f });
    object_->SetRoughness(0.38f);
    object_->SetMetallic(0.24f);

    modelLocalCenterOffset_ = CalculateModelLocalCenterOffset(model);
    UpdateObjectTransform();
}

// 上下左右の入力と回避を反映し、左右・上下の移動範囲内に座標を収める。被弾後の無敵時間もここで減らす。
void Player::Update(MovementInput input, float timeScale, float frameStep)
{
    if (!object_ || IsDead()) {
        return;
    }

    const float controlStep = std::clamp(frameStep, 0.0f, 3.0f);
    const float motionScale = std::clamp(timeScale, kPlayerSlowMinScale, 1.0f) * controlStep;
    invincibleTimer_ = (std::max)(invincibleTimer_ - controlStep, 0.0f);

    float moveX = static_cast<float>(input.horizontal) * moveSpeed_;
    const float moveY = static_cast<float>(input.vertical) * moveSpeed_;
    if (dodge_.UpdateInput(input.dodgeTriggered, input.horizontal, controlStep)) {
        invincibleTimer_ = PlayerDodge::kDuration;
    }

    const bool dodging = dodge_.IsActive();
    const int poseDirection = dodging ? dodge_.Direction() : input.horizontal;
    const Math::Vector3 targetMovementRotate{
        -static_cast<float>(input.vertical) * kPlayerMovePitchAngle,
        static_cast<float>(poseDirection) * kPlayerMoveYawAngle,
        -static_cast<float>(poseDirection) * kPlayerMoveBankAngle,
    };
    const float poseBlend =
        1.0f - std::pow(1.0f - kPlayerPoseResponse, motionScale);
    movementRotate_.x +=
        (targetMovementRotate.x - movementRotate_.x) * poseBlend;
    movementRotate_.y +=
        (targetMovementRotate.y - movementRotate_.y) * poseBlend;
    movementRotate_.z +=
        (targetMovementRotate.z - movementRotate_.z) * poseBlend;

    const auto dodgeStep = dodge_.Advance(controlStep);
    if (dodging) { moveX = 0.0f; }
    Math::Vector3 rotate = movementRotate_;
    rotate.z += dodgeStep.roll;

    translate_.x = std::clamp(translate_.x + moveX * motionScale + dodgeStep.distance,
        -kPlayerHorizontalLimit, kPlayerHorizontalLimit);
    translate_.y = std::clamp(translate_.y + moveY * motionScale,
        kPlayerLowerLimitY, kPlayerUpperLimitY);

    objectRotate_ = rotate;
    object_->SetColor(
        invincibleTimer_ > 0 ?
        kPlayerInvincibleColor : kPlayerHullColor);
    UpdateObjectTransform();
}

// 自機の描画オブジェクトがあれば表示する。墜落中に機体を描くかどうかは呼び出し元が決める。
void Player::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

// 自機が生きていて、今回の回避でまだ成立していなければtrueを返す。
bool Player::TryConfirmJustDodge()
{
    return !IsDead() && dodge_.TryConfirm();
}

// 光源から見た自機の輪郭を影用の深度画像へ書く。
void Player::DrawShadow(const Math::Matrix4x4& lightViewProjection)
{
    if (object_ && !IsDead()) {
        object_->DrawShadow(lightViewProjection);
    }
}

// レールで進んだ距離を自機の前後位置に反映する。左右と上下の位置はそのまま使う。
void Player::SetRailZ(float z)
{
    translate_.z = z;
    if (object_) {
        UpdateObjectTransform();
    }
}

// 出撃などの演出が決めた位置と回転を設定し、画面上の機体もその場で更新する。
void Player::SetCinematicPose(const Math::Vector3& position, const Math::Vector3& rotation)
{
    translate_ = position;
    objectRotate_ = movementRotate_ = rotation;
    UpdateObjectTransform();
}

// モデルの中心のずれを機体の向きに合わせて補正し、自機座標にモデルが合うように配置する。
void Player::UpdateObjectTransform()
{
    if (!object_) {
        return;
    }

    const Math::Vector3 scaledCenterOffset{
        modelLocalCenterOffset_.x * objectScale_.x,
        modelLocalCenterOffset_.y * objectScale_.y,
        modelLocalCenterOffset_.z * objectScale_.z,
    };
    // 回転中心をモデルの原点から機体の中心へ補正する。
    const Math::Vector3 rotatedCenterOffset =
        GameRuntimeMath::RotateLocalOffset(scaledCenterOffset, objectRotate_);
    object_->SetTranslate({
        translate_.x - rotatedCenterOffset.x,
        translate_.y - rotatedCenterOffset.y,
        translate_.z - rotatedCenterOffset.z,
    });
    object_->SetRotate(objectRotate_);
    object_->Update();
}

// 受け取ったダメージをHPから引く。生き残った場合は被弾直後の無敵時間を設定する。
void Player::Damage(int amount)
{
    hp_ -= amount;
    if (hp_ < 0) {
        hp_ = 0;
    }
    if (amount > 0 && hp_ > 0) {
        invincibleTimer_ = (std::max)(invincibleTimer_, kDamageProtectionDuration);
    }
    if (object_) {
        object_->SetColor(
            IsDead() ?
            Math::Vector4{ 0.35f, 0.35f, 0.4f, 1.0f } :
            Math::Vector4{ 1.0f, 0.35f, 0.35f, 1.0f });
    }
}

// 指定位置で再開できるようHP、無敵時間、回避、姿勢を初期状態へ戻す。
void Player::Reset(const Math::Vector3& position)
{
    hp_ = 100;
    translate_ = position;
    objectRotate_ = movementRotate_ = {};
    invincibleTimer_ = 0.0f;
    dodge_ = {};
    if (object_) { object_->SetColor(kPlayerHullColor); }
    UpdateObjectTransform();
}
