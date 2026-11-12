#pragma once

#include "app/presentation/CameraMotionBlur.h"

class Camera;
class DirectXCommon;

class FlightMotionBlur {
public:
    void Initialize();
    void UpdateReview(const DirectXCommon& renderer, bool canFreeze);
    bool IsFrozen() const { return frozen_; }

    // 画角が切り替わると、前フレームの位置を比較に使えなくなる
    void ResetHistory() { history_.Reset(); }
    void Apply(DirectXCommon& renderer, const Camera& camera,
        const Math::Vector2& viewportMin, const Math::Vector2& viewportSize,
        const Math::Vector3& vanishingPoint, const Math::Vector3& playerPosition,
        const Math::Vector2& reticle, float strength, bool active, bool paused);

private:
    CameraMotionBlur history_;
    int mode_ = 1; // 0:なし、1:放射状、2:カメラ移動、3:速度の可視化
    bool review_ = false;
    bool frozen_ = false;
};
