#pragma once

#include "engine/base/Math.h"

#include <array>
#include <memory>

class Camera;
class DirectXCommon;
class Model;
class Object3d;
class Object3dCommon;
class Skybox;

// クリア演出専用のカメラ・機体・噴射を所有する。モデルと描画設定は本編から借りる。
class ClearResultPresentation {
public:
    ClearResultPresentation();
    ~ClearResultPresentation();

    void Initialize(Object3dCommon& common, Model* playerModel, Model* exhaustTrailModel, Model* exhaustCoreModel);
    void Clear();
    bool IsReady() const { return ship_ && camera_; }
    const Math::Matrix4x4& GetProjectionMatrix() const;
    void Update(float elapsed, float aspectRatio);
    void Draw(DirectXCommon& dxCommon, Skybox* skybox) const;

private:
    Object3dCommon* common_ = nullptr;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> ship_;
    std::array<std::unique_ptr<Object3d>, 6> exhaust_;
    Math::Vector3 modelCenter_{};
};
