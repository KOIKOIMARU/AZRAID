// ===== Camera.cpp =====
// カメラの位置・向き・画角から、ビュー行列と射影行列を更新する

#include "engine/3d/Camera.h"
#include "engine/base/WinApp.h"

using namespace Math;

// 画角・表示比率・手前と奥の距離を初期化し、最初のカメラ行列を作る
Camera::Camera()
{
    transform = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

    fovY = 0.45f;
    aspectRatio = float(WinApp::kClientWidth) / float(WinApp::kClientHeight);
    nearClip = 0.1f;
    farClip = 100.0f;

    Update();
}


// カメラの位置と回転からビューと投影を作り、描画時に使う合成行列を更新する
void Camera::Update()
{
    // カメラ自身の位置と向きから、ワールド上での姿勢を作る
    worldMatrix = MakeAffineMatrix(
        transform.scale,
        transform.rotate,
        transform.translate
    );

    // カメラの姿勢を逆変換して、ワールド座標をカメラから見た座標へ直す
    viewMatrix = Inverse(worldMatrix);

    // 画角・画面比率・手前と奥の限界から透視投影を作る
    projectionMatrix = MakePerspectiveFovMatrix(
        fovY,
        aspectRatio,
        nearClip,
        farClip
    );

    // ビューと投影を合成し、ワールド座標からクリップ座標へ一度で変換できるようにする
    viewProjectionMatrix =
        Multiply(viewMatrix, projectionMatrix);
}



// ===== setter =====
void Camera::SetRotate(const Vector3& rotate) {
    transform.rotate = rotate;
}

void Camera::SetTranslate(const Vector3& translate) {
    transform.translate = translate;
}

void Camera::SetFovY(float value) {
    fovY = value;
}

void Camera::SetAspectRatio(float value) {
    aspectRatio = value;
}

void Camera::SetNearClip(float value) {
    nearClip = value;
}

void Camera::SetFarClip(float value) {
    farClip = value;
}

// ===== getter =====
const Matrix4x4& Camera::GetWorldMatrix() const {
    return worldMatrix;
}

const Matrix4x4& Camera::GetViewMatrix() const {
    return viewMatrix;
}

const Matrix4x4& Camera::GetProjectionMatrix() const {
    return projectionMatrix;
}

const Matrix4x4& Camera::GetViewProjectionMatrix() const {
    return viewProjectionMatrix;
}

const Vector3& Camera::GetRotate() const {
    return transform.rotate;
}

const Vector3& Camera::GetTranslate() const {
    return transform.translate;
}
