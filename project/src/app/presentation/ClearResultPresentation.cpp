#include "app/presentation/ClearResultPresentation.h"
#include "app/runtime/GameRuntimeMath.h"

#include "engine/3d/Camera.h"
#include "engine/3d/Model.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Skybox.h"
#include "engine/base/DirectXCommon.h"

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>

ClearResultPresentation::ClearResultPresentation() = default;
ClearResultPresentation::~ClearResultPresentation() = default;

// クリア専用の機体、カメラ、噴射の描画を解放する。
void ClearResultPresentation::Clear()
{
    exhaust_ = {};
    ship_.reset();
    camera_.reset();
    common_ = nullptr;
    modelCenter_ = {};
}

const Math::Matrix4x4& ClearResultPresentation::GetProjectionMatrix() const
{
    assert(IsReady());
    return camera_->GetProjectionMatrix();
}

// リザルト用の機体、カメラと噴射を準備する。準備済みなら今ある描画枠を使う。
void ClearResultPresentation::Initialize(
    Object3dCommon& common, Model* playerModel, Model* exhaustTrailModel, Model* exhaustCoreModel)
{
    Clear();
    if (!playerModel || playerModel->GetVertices().empty()) { return; }
    common_ = &common;
    Math::Vector3 min{ FLT_MAX, FLT_MAX, FLT_MAX };
    Math::Vector3 max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (const auto& vertex : playerModel->GetVertices()) {
        min.x = (std::min)(min.x, vertex.position.x);
        min.y = (std::min)(min.y, vertex.position.y);
        min.z = (std::min)(min.z, vertex.position.z);
        max.x = (std::max)(max.x, vertex.position.x);
        max.y = (std::max)(max.y, vertex.position.y);
        max.z = (std::max)(max.z, vertex.position.z);
    }
    modelCenter_ = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
    camera_ = std::make_unique<Camera>();
    camera_->SetFovY(0.56f);
    camera_->SetFarClip(200.0f);
    ship_ = std::make_unique<Object3d>();
    ship_->Initialize(common_);
    ship_->SetCamera(camera_.get());
    ship_->SetModel(playerModel);
    ship_->SetLightingMode(1);
    ship_->SetColor({ 0.64f, 0.74f, 0.88f, 1.0f });
    ship_->SetDirectionalLightDirection({ -0.30f, -0.72f, 0.55f });
    ship_->SetDirectionalLightIntensity(1.65f);
    ship_->SetPointLightIntensity(0.0f);
    ship_->SetSpotLightIntensity(0.0f);
    ship_->SetEnvironmentCoefficient(0.07f);
    ship_->SetShininess(160.0f);
    ship_->SetRoughness(0.34f);
    ship_->SetMetallic(0.28f);
    ship_->SetSpecularColor({ 0.40f, 0.44f, 0.50f });
    ship_->SetShadowReceiveStrength(0.0f);
    // 左右の噴射を外炎・内炎・ノズルの光の三層で準備する。
    for (size_t index = 0; index < exhaust_.size(); ++index) {
        auto& exhaust = exhaust_[index];
        exhaust = std::make_unique<Object3d>();
        exhaust->Initialize(common_);
        exhaust->SetCamera(camera_.get());
        exhaust->SetModel(index / 2 == 2 ? exhaustCoreModel : exhaustTrailModel);
        exhaust->SetLightingMode(0);
        exhaust->SetEnvironmentCoefficient(0.0f);
        exhaust->SetAlphaReference(0.003f);
    }
}

// クリア後に映す機体とカメラを、リザルトを開いてからの経過時間で動かす。
void ClearResultPresentation::Update(float elapsed, float aspectRatio)
{
    if (!IsReady()) { return; }
    const auto rotate = [](const Math::Vector3& v, const Math::Matrix4x4& m) {
        return Math::Vector3{ v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
            v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
            v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] };
    };
    const float arrival = 1.0f - std::pow(1.0f - std::clamp(elapsed / 1.1f, 0.0f, 1.0f), 3.0f);
    const float fit = (std::min)(aspectRatio / (1280.0f / 720.0f), 1.0f);
    camera_->SetAspectRatio(aspectRatio);
    camera_->SetTranslate({ 0, 0, -17 });
    camera_->SetRotate({ -0.10f, 0.12f, -0.10f });
    camera_->Update();
    const Math::Vector3 local{ (2.9f + (1 - arrival) * 6.0f) * fit,
        2.25f + std::sin(elapsed * 0.65f) * 0.12f, 17.0f };
    const auto worldOffset = rotate(local, camera_->GetWorldMatrix());
    const auto eye = camera_->GetTranslate();
    const Math::Vector3 center{ eye.x + worldOffset.x, eye.y + worldOffset.y, eye.z + worldOffset.z };
    // 機首の方向を保ったまま、機体のバンクを加える。
    const auto forward = rotate({ -0.92f, 0.10f,
        0.38f + (1 - arrival) * 0.15f + std::sin(elapsed * 0.42f) * 0.065f }, camera_->GetWorldMatrix());
    const auto referenceUp = rotate({ 0, 1, 0 }, camera_->GetWorldMatrix());
    const float bank = 0.58f + std::sin(elapsed * 0.65f) * 0.035f;
    const auto rotation = GameRuntimeMath::StageFlightRotation(forward, referenceUp, bank);
    const float shipScale = 3.2f * fit;
    const auto rotationMatrix = Math::MakeAffineMatrix({ 1, 1, 1 }, rotation, {});
    const auto matrix = Math::MakeAffineMatrix({ shipScale, shipScale, shipScale }, rotation, {});
    const auto offset = rotate(modelCenter_, matrix);
    ship_->SetScale({ shipScale, shipScale, shipScale });
    ship_->SetRotate(rotation);
    ship_->SetTranslate({ center.x - offset.x, center.y - offset.y, center.z - offset.z });
    ship_->SetColor({ 0.90f, 0.94f, 1.0f, 1.0f });
    ship_->Update();

    const auto tail = rotate({ 0, 0, -1 }, rotationMatrix);
    const auto tailView = rotate(tail, camera_->GetViewMatrix());
    Math::Vector3 billboard = camera_->GetRotate();
    billboard.z += std::atan2(-tailView.x, tailView.y);
    const float pulse = 1.0f + 0.045f * std::sin(elapsed * 29.0f) + 0.025f * std::sin(elapsed * 43.0f);
    for (size_t index = 0; index < exhaust_.size(); ++index) {
        const int layer = static_cast<int>(index / 2);
        const float ratio = shipScale / 1.26f;
        const auto nozzle = rotate({ (index % 2 == 0 ? -0.48f : 0.48f) * ratio,
            -0.28f * ratio, -1.70f * ratio }, rotationMatrix);
        const float length = (layer == 0 ? 2.0f : 1.3f) * pulse;
        const float centerOffset = layer == 2 ? 0.025f : length * 0.78f;
        auto& exhaust = exhaust_[index];
        exhaust->SetTranslate({ center.x + nozzle.x + tail.x * centerOffset,
            center.y + nozzle.y + tail.y * centerOffset, center.z + nozzle.z + tail.z * centerOffset });
        exhaust->SetRotate(billboard);
        exhaust->SetScale(layer == 2 ? Math::Vector3{ 0.20f * pulse, 0.20f * pulse, 1 } :
            Math::Vector3{ (layer == 0 ? 0.27f : 0.12f) * pulse, length, 1 });
        exhaust->SetColor(layer == 2 ? Math::Vector4{ 0.72f, 0.84f, 1.0f, 0.52f } :
            (layer == 0 ? Math::Vector4{ 0.22f, 0.43f, 0.86f, 0.48f } : Math::Vector4{ 0.91f, 0.96f, 1.0f, 0.85f }));
        exhaust->Update();
    }
}

// クリア画面専用のカメラで空と機体を描き、その飛行に合わせた噴射を表示する。
void ClearResultPresentation::Draw(DirectXCommon& dxCommon, Skybox* skybox) const
{
    assert(IsReady());
    dxCommon.SetFlightBlur(0.0f, { 0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, 0.5f }, camera_->GetAspectRatio());
    // このフレームでは本編の空・機体を描かず、各定数バッファを一つのカメラでだけ使う。
    if (skybox) {
        skybox->Update(camera_.get());
        skybox->Draw();
    }
    const auto depth = common_->GetDepthDrawMode();
    const auto blend = common_->GetBlendMode();
    common_->SetDepthDrawMode(DepthDrawMode::Normal);
    common_->SetBlendMode(BlendMode::Normal);
    common_->CommonDrawSetting();
    ship_->Draw();
    common_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    common_->SetBlendMode(BlendMode::Add);
    common_->CommonDrawSetting();
    for (const auto& exhaust : exhaust_) { exhaust->Draw(); }
    common_->SetDepthDrawMode(depth);
    common_->SetBlendMode(blend);
    common_->CommonDrawSetting();
}
