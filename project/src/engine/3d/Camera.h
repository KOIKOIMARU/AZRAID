// カメラの位置・回転・画角から、3D座標を画面へ写す行列を作る。

#pragma once
#include "engine/base/Math.h"

class Camera {
public:
    Camera();
    void Update();

    // ===== setter =====
    void SetRotate(const Math::Vector3& rotate);
    void SetTranslate(const Math::Vector3& translate);
    void SetFovY(float fovY);
    void SetAspectRatio(float aspectRatio);
    void SetNearClip(float nearClip);
    void SetFarClip(float farClip);

    // ===== getter =====
    const Math::Matrix4x4& GetWorldMatrix() const;
    const Math::Matrix4x4& GetViewMatrix() const;
    const Math::Matrix4x4& GetProjectionMatrix() const;
    const Math::Matrix4x4& GetViewProjectionMatrix() const;
    const Math::Vector3& GetRotate() const;
    const Math::Vector3& GetTranslate() const;
    float GetAspectRatio() const { return aspectRatio; }

private:
    // transform
    Math::Transform transform;

    // matrices
    Math::Matrix4x4 worldMatrix;
    Math::Matrix4x4 viewMatrix;
    Math::Matrix4x4 projectionMatrix;
    Math::Matrix4x4 viewProjectionMatrix;

    // projection parameters
    float fovY;
    float aspectRatio;
    float nearClip;
    float farClip;
};
