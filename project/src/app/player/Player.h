// 自機の座標、HP、無敵時間を管理する　入力はPlayerControl、横回避の状態はPlayerDodgeが担当する

#pragma once

#include "app/player/PlayerDodge.h"
#include "engine/3d/Object3d.h"
#include "engine/base/Math.h"

#include <memory>

class Model;
class Object3dCommon;

class Player {
public:
    struct MovementInput {
        int horizontal = 0;
        int vertical = 0;
        bool dodgeTriggered = false;
    };

    void Initialize(Object3dCommon* object3dCommon, Model* model);
    // 上下左右の入力と回避を反映し、左右・上下の移動範囲内に座標を収める　被弾後の無敵時間もここで減らす
    void Update(MovementInput input, float timeScale = 1.0f, float frameStep = 1.0f);
    void Draw();
    void DrawShadow(const Math::Matrix4x4& lightViewProjection);
    void SetRailZ(float z);
    void SetCinematicPose(const Math::Vector3& position, const Math::Vector3& rotation);

    const Math::Vector3& GetTranslate() const { return translate_; }
    const Math::Vector3& GetVisualRotate() const { return objectRotate_; }
    Math::Vector3 GetVisualTranslate() const { return object_ ? object_->GetTranslate() : translate_; }
    float GetVisualScale() const { return objectScale_.x; }
    int GetHp() const { return hp_; }
    float GetRadius() const { return collisionRadius_; }
    bool IsDead() const { return hp_ <= 0; }
    bool IsInvincible() const { return invincibleTimer_ > 0; }
    bool IsDodging() const { return dodge_.IsActive(); }
    int GetDodgeDirection() const { return dodge_.Direction(); }
    bool TryConfirmJustDodge(); // 一回の回避につき、成功の通知と報酬は一度だけ
    // 受け取ったダメージをHPから引く　生き残った場合は被弾直後の無敵時間を設定する
    void Damage(int amount);
    void Reset(const Math::Vector3& position); // 描画オブジェクトを保持して、HP・姿勢・回避状態を戻す

private:
    void UpdateObjectTransform();

    std::unique_ptr<Object3d> object_;
    Math::Vector3 translate_{ 0.0f, 0.0f, 0.0f };
    Math::Vector3 objectScale_{ 1.26f, 1.26f, 1.26f };
    Math::Vector3 objectRotate_{ 0.0f, 0.0f, 0.0f };
    Math::Vector3 movementRotate_{ 0.0f, 0.0f, 0.0f };
    Math::Vector3 modelLocalCenterOffset_{ 0.0f, 0.0f, 0.0f };
    float moveSpeed_ = 0.205f;
    float collisionRadius_ = 0.86f;
    float invincibleTimer_ = 0.0f;
    PlayerDodge dodge_;
    int hp_ = 100;
};
