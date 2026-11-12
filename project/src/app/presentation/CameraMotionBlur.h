#pragma once

#include "engine/base/Math.h"
#include <cmath>

// 深度の現在クリップ座標を、同じワールド位置の前フレームのクリップ座標へ写す
// 色の履歴や物体固有の速度は持たず、カメラ移動だけを扱う
struct CameraMotionBlur {
    Math::Matrix4x4 clipToPreviousClip = Math::MakeIdentity4x4();
    float exposureScale = 0.0f;
    bool ready = false;

    // カメラの履歴を無効にし、シーン切り替え後の最初の画像に古いブラーが残らないようにする
    void Reset() { ready = false; exposureScale = 0.0f; clipToPreviousClip = Math::MakeIdentity4x4(); }

    // 直前の視点と比較して画像が連続しているか確認し、連続する場合だけ前フレームへ写す行列を求める
    void Update(const Math::Matrix4x4& viewProjection, const Math::Vector3& position,
        const Math::Vector3& rotation, float aspect, float deltaTime, bool active)
    {
        const auto rotationDelta = [](float a, float b) {
            return std::abs(std::remainder(a - b, 6.28318530718f));
        };
        const float dx = position.x - previousPosition.x;
        const float dy = position.y - previousPosition.y;
        const float dz = position.z - previousPosition.z;
        const bool continuous = ready && active && deltaTime > 0.0f && deltaTime <= 0.075f &&
            dx * dx + dy * dy + dz * dz < 144.0f && std::abs(aspect - previousAspect) < 0.001f &&
            rotationDelta(rotation.x, previousRotation.x) < 0.35f &&
            rotationDelta(rotation.y, previousRotation.y) < 0.35f &&
            rotationDelta(rotation.z, previousRotation.z) < 0.35f;
        clipToPreviousClip = continuous ?
            Math::Multiply(Math::Inverse(viewProjection), previousViewProjection) : Math::MakeIdentity4x4();
        // 25ms相当　fpsが変わっても同じカメラ速度なら同じ長さになるよう正規化
        exposureScale = continuous ? 0.025f / deltaTime : 0.0f;
        previousViewProjection = viewProjection;
        previousPosition = position;
        previousRotation = rotation;
        previousAspect = aspect;
        ready = active;
    }

private:
    Math::Matrix4x4 previousViewProjection = Math::MakeIdentity4x4();
    Math::Vector3 previousPosition{};
    Math::Vector3 previousRotation{};
    float previousAspect = 0.0f;
};
