// ===== PlayerSkillPresentation.cpp =====
// スキルの連撃と最後の一撃に合わせて、斬撃や画面エフェクトを表示する。

#include "app/player/PlayerSkillPresentation.h"

#include "app/combat/Bullet.h"
#include "app/ui/CombatHud.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Model.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/ScreenProjection.h"

#include <algorithm>
#include <cmath>
#include <limits>

void PlayerSkillPresentation::Clear()
{
    slashes_ = {};
    finisherVisuals_ = {};
}

// スキルに重ねる画面効果と連撃表示を初期化する。
void PlayerSkillPresentation::Reset()
{
    for (auto& slash : slashes_) {
        slash.age = -1.0f;
    }
}

// 斬撃やフィニッシュの表示時間を進め、寿命が終わった表示を片づける。
void PlayerSkillPresentation::Advance(float frameStep)
{
    for (auto& slash : slashes_) {
        if (slash.age >= 0.0f) {
            slash.age += frameStep;
            if (slash.age >= 22.0f) {
                slash.age = -1.0f;
            }
        }
    }
}

// 連撃の位置を受け取り、その一撃に対応する斬撃と残像を表示する。
void PlayerSkillPresentation::ShowStrike(int strike, const Math::Vector3& playerPosition,
    const Math::Vector3& targetPosition)
{
    auto& slash = slashes_[strike];
    slash.from = strike > 0 ? slashes_[strike - 1].position : playerPosition;
    slash.position = targetPosition;
    slash.angle = strike % 2 == 0 ? 0.55f : -0.65f;
    slash.age = 0.0f;
}

// 最後の一撃に合わせて画面効果と照準の強調を始める。
void PlayerSkillPresentation::ShowFinisher(const PlayerSkill& skill)
{
    finisherPosition_ = {};
    const float count = static_cast<float>(skill.TargetCount());
    for (const auto& target : skill.Targets()) {
        finisherPosition_.x += target.position.x / count;
        finisherPosition_.y += target.position.y / count;
        finisherPosition_.z += target.position.z / count;
    }
}

// 連撃中に描く機体の残像や斬撃のオブジェクトを事前に準備する。
void PlayerSkillPresentation::Initialize(Object3dCommon& common, Model& ship)
{
    Reset();
    shipCenter_ = {};
    if (!ship.GetVertices().empty()) {
        Math::Vector3 min{ (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)() };
        Math::Vector3 max{ -min.x, -min.y, -min.z };
        for (const auto& vertex : ship.GetVertices()) {
            min.x = (std::min)(min.x, vertex.position.x);
            min.y = (std::min)(min.y, vertex.position.y);
            min.z = (std::min)(min.z, vertex.position.z);
            max.x = (std::max)(max.x, vertex.position.x);
            max.y = (std::max)(max.y, vertex.position.y);
            max.z = (std::max)(max.z, vertex.position.z);
        }
        shipCenter_ = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
    }
    Model* bladeModel = ModelManager::GetInstance()->FindModel("phantom_slash_blade");
    const auto setupBlade = [&](std::unique_ptr<Object3d>& visual) {
        visual = std::make_unique<Object3d>();
        visual->Initialize(&common);
        visual->SetModel(bladeModel);
        visual->SetLightingMode(0);
        visual->SetEnvironmentCoefficient(0.0f);
        visual->SetShadowReceiveStrength(0.0f);
        visual->SetAlphaReference(0.005f);
        visual->SetColor({ 1, 1, 1, 0 });
        visual->Update();
    };
    for (auto& slash : slashes_) {
        slash.ghost = std::make_unique<Object3d>();
        slash.ghost->Initialize(&common);
        slash.ghost->SetModel(&ship);
        slash.ghost->SetTextureFilePath("resources/human/white.png");
        slash.ghost->SetLightingMode(0);
        slash.ghost->SetEnvironmentCoefficient(0.0f);
        slash.ghost->SetShadowReceiveStrength(0.0f);
        slash.ghost->SetColor({ 0.25f, 0.80f, 1.0f, 0.0f });
        slash.ghost->Update();
        setupBlade(slash.blade);
        slash.blade->SetTextureFilePath("resources/effects/glow_core.png");
        setupBlade(slash.wake);
    }
    for (size_t index = 0; index < finisherVisuals_.size(); ++index) {
        setupBlade(finisherVisuals_[index]);
        if (index % 2 != 0) { finisherVisuals_[index]->SetTextureFilePath("resources/effects/glow_core.png"); }
    }
}

// スキルの対象位置と時刻に合わせ、3Dの残像と斬撃を描く。
void PlayerSkillPresentation::DrawObjects(Object3dCommon& common, const Camera& camera, const PlayerSkill& skill)
{
    if (!(skill.IsActive() && skill.IsFinished()) &&
        std::none_of(slashes_.begin(), slashes_.end(),
            [](const Slash& slash) { return slash.age >= 0 && slash.age < 18; })) { return; }
    const auto previousBlend = common.GetBlendMode();
    const auto previousDepth = common.GetDepthDrawMode();
    const auto& cameraWorld = camera.GetWorldMatrix();
    common.SetDepthDrawMode(DepthDrawMode::ReadOnly);
    common.SetBlendMode(BlendMode::Add);
    common.CommonDrawSetting();
    for (auto& slash : slashes_) {
        if (slash.age < 0 || slash.age >= 18 || !slash.ghost) { continue; }
        const float rate = std::clamp(slash.age / 18.0f, 0.0f, 1.0f);
        const float travel = 1.0f - std::pow(1.0f - std::clamp(slash.age / 6.0f, 0.0f, 1.0f), 3.0f);
        const float scale = skill.IsEmpowered() ? 1.75f : 1.50f;
        const float roll = -slash.angle;
        const Math::Vector3 end{ slash.position.x + std::cos(slash.angle) * 3.0f,
            slash.position.y + std::sin(slash.angle) * 3.0f, slash.position.z + 2.0f };
        const float cx = shipCenter_.x * scale, cy = shipCenter_.y * scale;
        slash.ghost->SetTranslate({
            std::lerp(slash.from.x, end.x, travel) - (cx * std::cos(roll) - cy * std::sin(roll)),
            std::lerp(slash.from.y, end.y, travel) - (cx * std::sin(roll) + cy * std::cos(roll)),
            std::lerp(slash.from.z, end.z, travel) - shipCenter_.z * scale });
        slash.ghost->SetRotate({ 0, 0, roll });
        slash.ghost->SetScale({ scale, scale, scale });
        const float alpha = 0.46f * (1.0f - rate) * (1.0f - rate);
        slash.ghost->SetColor(skill.IsEmpowered() ? Math::Vector4{ 1.0f, 0.62f, 0.18f, alpha } :
            Math::Vector4{ 0.16f, 0.64f, 1.20f, alpha });
        slash.ghost->Update();
        slash.ghost->Draw();
    }

    // 暗い輪郭を持つ実体のある刃。敵の手前へ置き、手前の建物には隠れる。
    common.SetBlendMode(BlendMode::Normal);
    common.CommonDrawSetting();
    const auto blade = [&](Object3d* visual, Math::Vector3 position, float angle,
        float width, float thickness, Math::Vector4 color) {
        if (!visual || color.w <= 0.001f || width <= 0.001f) { return; }
        position.x -= cameraWorld.m[2][0] * 2.0f;
        position.y -= cameraWorld.m[2][1] * 2.0f;
        position.z -= cameraWorld.m[2][2] * 2.0f;
        visual->SetTranslate(position);
        visual->SetRotate(Bullet::CalculateBillboardRotation(cameraWorld, angle));
        visual->SetScale({ width, thickness, 1 });
        visual->SetColor(color);
        visual->Update();
        visual->Draw();
    };
    for (auto& slash : slashes_) {
        if (slash.age < 0 || slash.age >= 18) { continue; }
        const float rate = std::clamp(slash.age / 18.0f, 0.0f, 1.0f);
        const float open = std::clamp(slash.age / 2.0f, 0.0f, 1.0f);
        const float fade = (1.0f - rate) * (1.0f - rate);
        const float width = (skill.IsEmpowered() ? 4.8f : 3.9f) * open;
        const float thickness = (skill.IsEmpowered() ? 3.1f : 2.5f) * (1.0f - rate * 0.65f);
        blade(slash.wake.get(), slash.position, slash.angle, width * 1.03f, thickness * 1.30f,
            skill.IsEmpowered() ? Math::Vector4{ 0.045f, 0.012f, 0.004f, fade * 0.50f } :
                Math::Vector4{ 0.005f, 0.016f, 0.04f, fade * 0.50f });
        blade(slash.blade.get(), slash.position, slash.angle, width, thickness,
            skill.IsEmpowered() ? Math::Vector4{ 1.65f, 1.05f, 0.35f, fade * 0.96f } :
                Math::Vector4{ 0.42f, 1.05f, 1.75f, fade * 0.96f });
    }
    if (skill.IsActive() && skill.IsFinished()) {
        const float age = skill.Clock() - skill.FinisherTime();
        const float fade = std::pow(1.0f - std::clamp(age / 18.0f, 0.0f, 1.0f), 2.0f);
        float span = 4.5f;
        for (int index = 0; index < skill.TargetCount(); ++index) {
            const auto& p = skill.Targets()[index].position;
            span = (std::max)(span, std::abs(p.x - finisherPosition_.x) + 3.8f);
        }
        span = std::clamp(span, 4.5f, 10.0f) * std::clamp((age + 1.0f) / 2.0f, 0.0f, 1.0f);
        for (size_t index = 0; index < finisherVisuals_.size(); ++index) {
            const bool rim = index % 2 == 0;
            const float angle = index < 2 ? 0.42f : -0.48f;
            const Math::Vector4 color = rim ?
                (skill.IsEmpowered() ? Math::Vector4{ 0.045f, 0.012f, 0.004f, fade * 0.55f } :
                    Math::Vector4{ 0.005f, 0.016f, 0.04f, fade * 0.55f }) :
                (skill.IsEmpowered() ? Math::Vector4{ 1.85f, 1.32f, 0.54f, fade } :
                    Math::Vector4{ 0.80f, 1.36f, 1.85f, fade });
            blade(finisherVisuals_[index].get(), finisherPosition_, angle,
                span * (rim ? 1.02f : 1.0f), (rim ? 4.8f : 3.8f) * (1.0f + age * 0.018f), color);
        }
    }
    common.SetBlendMode(previousBlend);
    common.SetDepthDrawMode(previousDepth);
    common.CommonDrawSetting();
}

// スキル中の画面の色、照準付近の光、最後の一撃の見せ方をHUDへ重ねる。
void PlayerSkillPresentation::DrawOverlay(ImDrawList& draw, const Math::Vector2& min, const Math::Vector2& size,
    const Math::Matrix4x4* viewProjection, const PlayerSkill& skill) const
{
    const bool active = skill.IsActive();
    if (!active && std::none_of(slashes_.begin(), slashes_.end(),
        [](const Slash& slash) { return slash.age >= 0 && slash.age < 6; })) { return; }
    draw.PushClipRect({ min.x, min.y }, { min.x + size.x, min.y + size.y }, true);
    const float scale = CombatHud::Scale(size);
    const auto ink = [&](int alpha) {
        return CombatHud::SurfaceColor(skill.IsEmpowered() ?
            IM_COL32(255, 206, 105, alpha) : IM_COL32(116, 229, 255, alpha));
    };
    if (active) {
        const float fade = 1.0f - std::clamp((skill.Clock() - skill.FinisherTime() - 4.0f) / 16.0f, 0.0f, 1.0f);
        draw.AddRectFilled({ min.x, min.y }, { min.x + size.x, min.y + size.y },
            IM_COL32(2, 8, 24, static_cast<int>(26 * fade)));
        // 発動の一瞬だけ、実際に選ばれた相手へ四隅を収束させる。
        if (skill.Clock() < 12) {
            for (int index = 0; index < skill.TargetCount(); ++index) {
                Math::Vector2 point{};
                if (!viewProjection || !ScreenProjection::ToViewport(skill.Targets()[index].position,
                    *viewProjection, min, size, point)) { continue; }
                const float r = (38.0f - skill.Clock() * 1.2f) * scale;
                for (const float x : { -1.0f, 1.0f }) {
                    for (const float y : { -1.0f, 1.0f }) {
                        const ImVec2 corner{ point.x + r * x, point.y + r * y };
                        draw.AddLine(corner, { corner.x - x * 12 * scale, corner.y }, ink(230), 2 * scale);
                        draw.AddLine(corner, { corner.x, corner.y - y * 12 * scale }, ink(230), 2 * scale);
                    }
                }
            }
        }
        if (skill.IsFinished()) {
            const float age = skill.Clock() - skill.FinisherTime();
            if (age < 2) {
                draw.AddRectFilled({ min.x, min.y }, { min.x + size.x, min.y + size.y },
                    IM_COL32(213, 244, 255, static_cast<int>(18 * (1.0f - age / 2.0f))));
            }
        }
    }
    for (const auto& slash : slashes_) {
        if (slash.age < 0 || slash.age >= 6) { continue; }
        Math::Vector2 point{};
        if (!viewProjection || !ScreenProjection::ToViewport(slash.position,
            *viewProjection, min, size, point)) { continue; }
        const float fade = 1.0f - slash.age / 6.0f;
        const float radius = (18.0f + slash.age * 4.0f) * scale;
        for (int ray = 0; ray < 4; ++ray) {
            const float angle = slash.angle + static_cast<float>(ray) * 1.57079632679f;
            const ImVec2 a{ point.x + std::cos(angle) * radius * 0.28f, point.y + std::sin(angle) * radius * 0.28f };
            const ImVec2 b{ point.x + std::cos(angle) * radius, point.y + std::sin(angle) * radius };
            draw.AddLine(a, b, IM_COL32(4, 12, 28, static_cast<int>(160 * fade)), 5 * scale);
            draw.AddLine(a, b, ink(static_cast<int>(230 * fade)), 2 * scale);
        }
    }
    draw.PopClipRect();
}
