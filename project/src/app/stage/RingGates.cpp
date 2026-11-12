// ===== RingGates.cpp =====
// リングの出現・描画と通過判定を担当し、通過後の加速を管理する

#include "app/stage/RingGates.h"
#include "engine/3d/Object3dCommon.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr float kReleaseFrames = 26.0f;
float Smooth(float value) { value = std::clamp(value, 0.0f, 1.0f); return value * value * (3.0f - 2.0f * value); }
}

// リングの枠と発光部に使う描画オブジェクトを、同時に表示できる数だけ用意する
void RingGates::Initialize(Object3dCommon* common, Model* frame, Model* light, Model* rear, Model* flow)
{
    Clear();
    if (!common || !frame || !light || !rear || !flow) { return; }
    common_ = common;
    const auto prepare = [&](std::unique_ptr<Object3d>& object, Model* model, bool metal) {
        object = std::make_unique<Object3d>();
        object->Initialize(common_);
        object->SetModel(model);
        object->SetLightingMode(metal ? 2 : 0);
        object->SetEnvironmentCoefficient(metal ? 0.025f : 0.0f);
        object->SetMetallic(metal ? 0.65f : 0.0f);
        object->SetRoughness(0.38f);
        object->SetShininess(48.0f);
        object->SetSpecularColor({ 0.20f, 0.24f, 0.28f });
        object->SetShadowReceiveStrength(0.0f);
    };
    for (auto& visual : visuals_) {
        for (auto& object : visual.frames) { prepare(object, frame, true); }
        for (auto& object : visual.lights) { prepare(object, light, false); }
        prepare(visual.rear, rear, false);
        prepare(visual.flow, flow, false);
    }
}

// リングの描画枠と素材参照を解放する
void RingGates::Clear()
{
    Reset();
    for (auto& visual : visuals_) { visual = {}; }
    common_ = nullptr;
}

// リングの表示と加速状態を戻し、通過・出現の回数を初期化する
void RingGates::Reset()
{
    gates_ = {};
    spawnCount_ = passCount_ = 0;
    boostAge_ = -1.0f;
}

// 使用中のリングを閉じ、通過後に残る開放表示も解除する
void RingGates::Deactivate()
{
    for (auto& gate : gates_) { gate.active = false; gate.releaseAge = -1.0f; }
    boostAge_ = -1.0f;
}

// 空いている枠にリングを配置し、前方距離と飛行速度から表示しておく時間を決める
bool RingGates::Spawn(const Math::Vector3& position, float railDistance, float railSpeed)
{
    if (!common_) { return false; }
    for (auto& gate : gates_) {
        if (gate.active || gate.releaseAge >= 0.0f) { continue; }
        gate = {};
        gate.position = position;
        gate.lifetime = std::clamp((position.z - railDistance) / (std::max)(railSpeed, 0.15f) + 120.0f, 120.0f, 600.0f);
        gate.active = true;
        ++spawnCount_;
        return true;
    }
    return false;
}

// リング通過直後の加速を最初のフレームから始める
void RingGates::StartBoost() { boostAge_ = 0.0f; }

// 通過直後に強くなり、その後ゆっくり0へ戻る加速の強さを返す
float RingGates::BoostStrength() const
{
    if (boostAge_ < 0.0f) { return 0.0f; }
    // 通過時に立ち上がり、後半は滑らかに巡航へ戻す
    return (0.35f + 0.65f * Smooth(boostAge_ / 6.0f)) *
        (1.0f - Smooth((boostAge_ - 25.0f) / (kBoostFrames - 25.0f)));
}

// 前回と今回の自機位置を結ぶ線がリング面を横切ったか調べる　高速でも途中の通過を拾える
int RingGates::Update(const Math::Vector3& previous, const Math::Vector3& current, float frameStep)
{
    const float step = (std::max)(frameStep, 0.0f);
    if (boostAge_ >= 0.0f) {
        boostAge_ += step;
        if (boostAge_ >= kBoostFrames) { boostAge_ = -1.0f; }
    }
    int passes = 0;
    const float advance = current.z - previous.z;
    for (auto& gate : gates_) {
        if (gate.releaseAge >= 0.0f) {
            gate.releaseAge += step;
            if (gate.releaseAge >= kReleaseFrames) { gate.releaseAge = -1.0f; }
        }
        if (!gate.active) { continue; }
        gate.age += step;
        if (advance > 0.0001f && previous.z < gate.position.z && current.z >= gate.position.z) {
            const float rate = (gate.position.z - previous.z) / advance;
            const float x = (previous.x + (current.x - previous.x) * rate - gate.position.x) / kRadiusX;
            const float y = (previous.y + (current.y - previous.y) * rate - gate.position.y) / kRadiusY;
            gate.active = false;
            if (x * x + y * y <= 1.0f) { ++passes; ++passCount_; gate.releaseAge = 0.0f; }
        } else if (current.z > gate.position.z + 2.0f || gate.age >= gate.lifetime) { gate.active = false; }
    }
    return passes;
}

// 配置中のリングと通過後の開く演出を、自機からの距離やフィーバー状態に応じて描く
void RingGates::Draw(const Math::Vector3& playerPosition, bool fever)
{
    if (!common_) { return; }
    const auto previousBlend = common_->GetBlendMode();
    const auto previousDepth = common_->GetDepthDrawMode();
    common_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    const Math::Vector4 light = fever ? Math::Vector4{ 1.0f, 0.64f, 0.18f, 1.0f } : Math::Vector4{ 0.18f, 0.78f, 1.0f, 1.0f };
    const auto place = [](Object3d& object, Math::Vector3 position, Math::Vector3 scale, float roll, Math::Vector4 color) {
        object.SetTranslate(position); object.SetScale(scale); object.SetRotate({ 0.0f, 0.0f, roll });
        object.SetColor(color); object.Update(); object.Draw();
    };
    for (size_t index = 0; index < gates_.size(); ++index) {
        const auto& gate = gates_[index];
        if (!gate.active && gate.releaseAge < 0.0f) { continue; }
        auto& visual = visuals_[index];
        const float birth = Smooth(gate.age / 22.0f);
        const float release = gate.releaseAge >= 0.0f ? gate.releaseAge / kReleaseFrames : 0.0f;
        const float opening = Smooth(release);
        const float fade = gate.releaseAge >= 0.0f ? 1.0f - opening :
            std::clamp((gate.position.z - playerPosition.z + 1.2f) / 1.2f, 0.0f, 1.0f);
        const float scale = 0.90f + birth * 0.10f;
        const float glow = (0.72f + 0.08f * std::sin(gate.age * 0.08f)) * birth * fade;
        common_->SetBlendMode(BlendMode::Normal);
        common_->CommonDrawSetting();
        for (size_t part = 0; part < 4; ++part) {
            const float angle = static_cast<float>(part) * std::numbers::pi_v<float> * 0.5f;
            const float spread = (1.0f - birth) * 0.35f + opening * 1.5f;
            const Math::Vector3 position{ gate.position.x + std::cos(angle + 0.785398f) * spread,
                gate.position.y + std::sin(angle + 0.785398f) * spread,
                gate.position.z + opening * 9.0f };
            const Math::Vector3 dimensions{ (part % 2 == 0 ? kRadiusX : kRadiusY) * scale,
                (part % 2 == 0 ? kRadiusY : kRadiusX) * scale, 1.0f };
            const float roll = angle + opening * (part % 2 == 0 ? 0.18f : -0.18f);
            place(*visual.frames[part], position, dimensions, roll, { 0.34f, 0.43f, 0.53f, birth * fade });
        }
        common_->SetBlendMode(BlendMode::Add);
        common_->CommonDrawSetting();
        for (size_t part = 0; part < 4; ++part) {
            const float angle = static_cast<float>(part) * std::numbers::pi_v<float> * 0.5f;
            const float spread = (1.0f - birth) * 0.35f + opening * 1.5f;
            auto color = light; color.w = glow;
            place(*visual.lights[part], { gate.position.x + std::cos(angle + 0.785398f) * spread,
                gate.position.y + std::sin(angle + 0.785398f) * spread, gate.position.z - 0.32f + opening * 9.0f },
                { (part % 2 == 0 ? kRadiusX : kRadiusY) * scale,
                    (part % 2 == 0 ? kRadiusY : kRadiusX) * scale, 1.0f },
                angle + opening * (part % 2 == 0 ? 0.18f : -0.18f), color);
        }
        auto color = light; color.w = glow * 0.42f;
        place(*visual.rear, { gate.position.x, gate.position.y, gate.position.z + 1.0f + opening * 9.0f },
            { kRadiusX * scale * 0.98f, kRadiusY * scale * 0.98f, 1.0f }, 0.0f, color);
        color.w = glow * (gate.releaseAge >= 0.0f ? 0.8f : 0.30f);
        const float flow = std::fmod(gate.age / 40.0f, 1.0f);
        place(*visual.flow, { gate.position.x, gate.position.y, gate.position.z - 0.4f + flow * 2.8f + opening * 10.0f },
            { kRadiusX * scale, kRadiusY * scale, 1.0f }, 0.0f, color);
    }
    common_->SetBlendMode(previousBlend);
    common_->SetDepthDrawMode(previousDepth);
    common_->CommonDrawSetting();
}
