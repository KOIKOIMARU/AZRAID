#include "app/presentation/FlightSpeedEffects.h"
#include "app/combat/FeverState.h"
#include "app/runtime/GameRuntimeMath.h"

#include "engine/3d/Camera.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/ScreenProjection.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace {
using GameRuntimeMath::Lerp;

constexpr int kCueCount = 18;
constexpr float kNearLocalZ = -10.0f;
constexpr float kFarLocalZ = 148.0f;
constexpr float kLoopLength = 172.0f;
constexpr float kTwoPi = 6.28318530718f;

float PseudoRandom01(int index, float salt)
{
    const float value =
        std::sin((static_cast<float>(index) + 1.0f) * 12.9898f + salt * 78.233f) *
        43758.5453f;
    return value - std::floor(value);
}

bool ProjectToScreen(const FlightSpeedEffects::Frame& frame, const Math::Vector3& worldPosition,
    Math::Vector2& screenPosition)
{
    return frame.camera && ScreenProjection::ToViewport(worldPosition,
        frame.camera->GetViewProjectionMatrix(), frame.viewportMin, frame.viewportSize, screenPosition);
}

// カメラから遠方の基準点を画面へ写し、速度線の集まる位置を求める。
Math::Vector2 VanishingPoint(const FlightSpeedEffects::Frame& frame)
{
    Math::Vector2 point{ frame.viewportMin.x + frame.viewportSize.x * 0.5f,
        frame.viewportMin.y + frame.viewportSize.y * 0.45f };
    ProjectToScreen(frame, { 0.0f, 1.0f, frame.railDistance + 260.0f }, point);
    return point;
}

}

// 背景側に流す光の描画枠と配置を準備する。
void FlightSpeedEffects::Initialize(Object3dCommon* common, const Assets& assets, float railDistance)
{
    Clear();
    common_ = common;
    if (!common_) {
        return;
    }

    Model* sparkleModel = assets.spark ? assets.spark : assets.glow;
    Model* glowModel = assets.glow ? assets.glow : sparkleModel;
    Model* streakModel = assets.streak ? assets.streak : assets.fallbackStreak;
    if (!sparkleModel && !glowModel && !streakModel) {
        return;
    }

    cues_.reserve(kCueCount);
    for (int index = 0; index < kCueCount; ++index) {
        const bool isStreak = (index % 3) != 2 && streakModel;
        const bool isGold = !isStreak && (index % 7) == 0;
        const float side = PseudoRandom01(index, 0.12f) < 0.5f ? -1.0f : 1.0f;
        const float outerBias = std::pow(PseudoRandom01(index, 0.35f), 0.42f);
        const float x = side * Lerp(3.6f, 19.5f, outerBias);
        const float y = Lerp(-5.8f, 7.2f, PseudoRandom01(index, 0.61f));
        const float z = Lerp(10.0f, kFarLocalZ, PseudoRandom01(index, 0.88f));

        Cue cue{};
        cue.object = std::make_unique<Object3d>();
        cue.object->Initialize(common_);
        cue.object->SetModel(isStreak ? streakModel : ((index % 3) == 0 ? glowModel : sparkleModel));
        cue.object->SetLightingMode(0);
        cue.object->SetEnvironmentCoefficient(0.0f);
        cue.object->SetAlphaReference(0.01f);
        cue.anchor = { x, y, z };
        cue.color = isStreak ?
            Math::Vector4{ 0.72f, 0.96f, 1.0f, 0.26f } :
            isGold ?
                Math::Vector4{ 1.0f, 0.86f, 0.42f, 0.40f } :
                Math::Vector4{ 0.78f, 0.96f, 1.0f, 0.34f };
        cue.speedMultiplier = isStreak ?
            Lerp(1.42f, 2.62f, PseudoRandom01(index, 1.12f)) :
            Lerp(0.86f, 1.58f, PseudoRandom01(index, 1.12f));
        cue.lateralDrift = Lerp(0.18f, 1.05f, PseudoRandom01(index, 1.46f));
        cue.verticalDrift = Lerp(0.10f, 0.62f, PseudoRandom01(index, 1.78f));
        cue.driftSpeed = Lerp(0.014f, 0.042f, PseudoRandom01(index, 2.03f));
        cue.phase = PseudoRandom01(index, 2.41f) * kTwoPi;
        cue.baseScale = isStreak ?
            Lerp(0.46f, 0.82f, PseudoRandom01(index, 2.82f)) :
            Lerp(0.09f, 0.26f, PseudoRandom01(index, 2.82f));
        cue.aspectX = isStreak ? 0.16f : 1.0f;
        cue.aspectY = isStreak ? Lerp(4.0f, 7.2f, PseudoRandom01(index, 3.17f)) : 1.0f;
        cue.spinSpeed = isStreak ?
            Lerp(-0.006f, 0.006f, PseudoRandom01(index, 3.59f)) :
            Lerp(-0.018f, 0.018f, PseudoRandom01(index, 3.59f));

        cue.object->SetColor(cue.color);
        cue.object->SetScale({
            cue.baseScale * cue.aspectX,
            cue.baseScale * cue.aspectY,
            1.0f,
        });
        cue.object->SetTranslate({
            cue.anchor.x,
            cue.anchor.y,
            railDistance + cue.anchor.z,
        });
        cue.object->Update();
        cues_.push_back(std::move(cue));
    }
}

void FlightSpeedEffects::Clear()
{
    cues_.clear();
    common_ = nullptr;
}

// レール距離と速度に合わせてワールド上の光を進め、必要に応じて機体の前方へ循環させる。
void FlightSpeedEffects::UpdateWorld(const Frame& frame, float frameStep, bool alignStreaks)
{
    const Math::Vector3 cameraRotate = frame.camera ? frame.camera->GetRotate() : Math::Vector3{};
    const float speedRate = frame.speedRate;
    const float feverSpeedRate = frame.feverSpeedRate;
    const auto& viewportSize = frame.viewportSize;
    const Math::Vector2 vanishing = alignStreaks && frame.camera ? VanishingPoint(frame) : Math::Vector2{};
    for (Cue& cue : cues_) {
        float localZ = std::fmod(
            cue.anchor.z - frame.railDistance * cue.speedMultiplier,
            kLoopLength);
        while (localZ < kNearLocalZ) {
            localZ += kLoopLength;
        }

        const float depthRate = std::clamp(
            (localZ - kNearLocalZ) /
                (kFarLocalZ - kNearLocalZ),
            0.0f,
            1.0f);
        const float motion = frame.railDistance * cue.driftSpeed + cue.phase;
        const float sidePull = 1.0f + (1.0f - depthRate) *
            (0.28f + speedRate * 0.18f + feverSpeedRate * 0.42f);
        Math::Vector3 position{};
        position.x = cue.anchor.x * sidePull + std::sin(motion) * cue.lateralDrift;
        position.y = cue.anchor.y + std::cos(motion * 0.77f) * cue.verticalDrift;
        position.z = frame.railDistance + localZ;

        const float nearRate = 1.0f - depthRate;
        const float twinkle =
            0.78f +
            0.22f * std::sin(cue.phase + frame.railDistance * (0.16f + cue.driftSpeed));
        const float scale =
            cue.baseScale *
            Lerp(
                0.66f,
                1.68f + speedRate * 0.28f + feverSpeedRate * 0.70f,
                nearRate) *
            twinkle;
        Math::Vector4 color = cue.color;
        color.w *= Lerp(
            0.44f,
            1.22f + speedRate * 0.22f + feverSpeedRate * 0.58f,
            nearRate) * twinkle;

        Math::Vector3 rotate = cameraRotate;
        rotate.z += cue.phase * 0.34f + frame.railDistance * cue.spinSpeed * frameStep;
        if (alignStreaks && cue.aspectY > 1.0f) {
            Math::Vector2 screen{};
            if (viewportSize.x > 1.0f && viewportSize.y > 1.0f && ProjectToScreen(frame, position, screen)) {
                const float dx = screen.x - vanishing.x;
                const float dy = screen.y - vanishing.y;
                const float edge = (std::max)(std::abs(dx) / (viewportSize.x * 0.5f),
                    std::abs(dy) / (viewportSize.y * 0.5f));
                // 浮遊する縦線ではなく、消失点から外へ抜ける空気の流れに合わせる。
                rotate.z = cameraRotate.z + std::atan2(-dy, dx) - kTwoPi * 0.25f;
                color.w *= std::clamp((edge - 0.52f) / 0.24f, 0.0f, 1.0f);
            } else {
                color.w = 0.0f;
            }
        }

        cue.object->SetTranslate(position);
        cue.object->SetRotate(rotate);
        cue.object->SetScale({
            scale * cue.aspectX,
            scale * cue.aspectY *
                (1.0f + speedRate * nearRate * 0.36f +
                    feverSpeedRate * nearRate * 1.20f),
            1.0f,
        });
        cue.object->SetColor(color);
        cue.object->Update();
    }
}

// 速度演出のうち、ワールド空間に置いた光を描く。
void FlightSpeedEffects::DrawWorld()
{
    if (cues_.empty()) {
        return;
    }

    const BlendMode previousBlendMode = common_->GetBlendMode();
    const DepthDrawMode previousDepthMode = common_->GetDepthDrawMode();

    common_->SetDepthDrawMode(DepthDrawMode::Overlay);
    common_->SetBlendMode(BlendMode::Add);
    common_->CommonDrawSetting();

    for (const Cue& cue : cues_) {
        cue.object->Draw();
    }

    common_->SetBlendMode(previousBlendMode);
    common_->SetDepthDrawMode(previousDepthMode);
    common_->CommonDrawSetting();
}


// 画面上の流れる線を描く。照準の近くを空け、加速やスローで濃さと伸びを変える。
void FlightSpeedEffects::DrawScreenStreaks(const Frame& frame, const ScreenStreaks& streaks, ImDrawList& drawList) const
{
    const auto& viewportMin = frame.viewportMin;
    const auto& viewportSize = frame.viewportSize;
    if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) { return; }
    const float worldScale = streaks.worldTimeScale;
    const float speed = frame.speedRate;
    const float fever = frame.feverSpeedRate;
    const float kick = streaks.boostKick;
    const float intensity = (0.35f + speed * 0.40f + fever * 0.45f + kick * 0.30f) * worldScale;
    const float exposureTravel = streaks.railSpeed * worldScale * (0.075f + kick * 0.065f);
    const float pixelScale = viewportSize.y / 720.0f;
    Math::Vector2 vanishing = VanishingPoint(frame);
    // 操作で消失点が動いても、各方向で実際に画面端へ達する距離を使う。
    vanishing.x = std::clamp(vanishing.x, viewportMin.x + viewportSize.x * 0.25f,
        viewportMin.x + viewportSize.x * 0.75f);
    vanishing.y = std::clamp(vanishing.y, viewportMin.y + viewportSize.y * 0.25f,
        viewportMin.y + viewportSize.y * 0.75f);
    Math::Vector2 playerScreen{ vanishing.x, viewportMin.y + viewportSize.y * 0.65f };
    if (streaks.playerPosition) {
        ProjectToScreen(frame, *streaks.playerPosition, playerScreen);
    }
    drawList.PushClipRect({ viewportMin.x, viewportMin.y },
        { viewportMin.x + viewportSize.x, viewportMin.y + viewportSize.y }, true);
    for (int index = 0; index < 56; ++index) {
        const float seed = PseudoRandom01(index, 8.71f);
        const float angle = (static_cast<float>(index) + seed * 0.7f) * kTwoPi / 56.0f;
        const float directionX = std::cos(angle);
        const float directionY = std::sin(angle);
        const float boundaryX = directionX >= 0.0f ?
            viewportMin.x + viewportSize.x - vanishing.x : vanishing.x - viewportMin.x;
        const float boundaryY = directionY >= 0.0f ?
            viewportMin.y + viewportSize.y - vanishing.y : vanishing.y - viewportMin.y;
        const float boundary = (std::min)(boundaryX / (std::max)(std::abs(directionX), 0.001f),
            boundaryY / (std::max)(std::abs(directionY), 0.001f));
        const float phase = std::fmod(frame.railDistance * (0.045f + seed * 0.020f) + seed, 1.0f);
        // 奥行きの逆数で投影。手前ほど速く広がり、実際の前進が止まれば流れも止まる。
        const float depth = 1.0f - phase * 0.88f;
        const float front = 0.18f / depth;
        const float back = 0.18f / (depth + exposureTravel);
        const float visibility = std::clamp((back - 0.50f) / 0.18f, 0.0f, 1.0f) *
            std::clamp((1.18f - front) / 0.15f, 0.0f, 1.0f);
        if (visibility <= 0.0f) { continue; }
        const ImVec2 tail{ vanishing.x + directionX * boundary * back,
            vanishing.y + directionY * boundary * back };
        const ImVec2 head{ vanishing.x + directionX * boundary * front,
            vanishing.y + directionY * boundary * front };
        const auto clearOf = [&](const Math::Vector2& point, float radiusX, float radiusY) {
            const float dx = (head.x - tail.x) / (viewportSize.x * radiusX);
            const float dy = (head.y - tail.y) / (viewportSize.y * radiusY);
            const float px = (point.x - tail.x) / (viewportSize.x * radiusX);
            const float py = (point.y - tail.y) / (viewportSize.y * radiusY);
            const float t = std::clamp((px * dx + py * dy) / (std::max)(dx * dx + dy * dy, 0.0001f), 0.0f, 1.0f);
            const float distance = std::sqrt((px - dx * t) * (px - dx * t) + (py - dy * t) * (py - dy * t));
            return std::clamp(distance - 1.0f, 0.0f, 1.0f);
        };
        const int alpha = (std::min)(180, static_cast<int>((54.0f + fever * 54.0f + kick * 46.0f) *
            intensity * visibility * clearOf(streaks.reticleScreen, 0.08f, 0.11f) *
            clearOf(playerScreen, 0.10f, 0.14f)));
        const float width = pixelScale * (1.2f + fever * 1.1f + kick * 1.25f);
        // 両端が細く消える風の帯。光点を付けず、敵弾と見分けられる形にする。
        constexpr float sections[] = { 0.0f, 0.32f, 0.76f, 1.0f };
        constexpr float opacity[] = { 0.22f, 0.85f, 0.38f };
        for (int section = 0; section < 3; ++section) {
            const ImVec2 a{ Lerp(tail.x, head.x, sections[section]), Lerp(tail.y, head.y, sections[section]) };
            const ImVec2 b{ Lerp(tail.x, head.x, sections[section + 1]), Lerp(tail.y, head.y, sections[section + 1]) };
            const int segmentAlpha = static_cast<int>(static_cast<float>(alpha) * opacity[section]);
            const float thickness = width * (section == 1 ? 1.0f : 0.45f);
            drawList.AddLine(a, b, IM_COL32(121, 213, 244, segmentAlpha / 5), thickness * 3.0f);
            drawList.AddLine(a, b, IM_COL32(225, 246, 252, segmentAlpha), thickness);
        }
    }
    // 発動時だけ左右へ風圧が抜ける。中央を白く覆わず、前進の立ち上がりを見せる。
    const float onset = static_cast<float>(FeverState::kActivationFlashFrames - streaks.feverFlashFrames) / 24.0f;
    if (streaks.feverFlashFrames > 0 && onset > 0.0f && onset < 1.0f) {
        const float radiusX = viewportSize.x * (0.22f + onset * 0.90f);
        const float radiusY = viewportSize.y * (0.28f + onset * 1.10f);
        const int alpha = static_cast<int>(105.0f * std::sin(onset * std::numbers::pi_v<float>) * (1.0f - onset));
        for (int side = 0; side < 2; ++side) {
            for (int segment = 0; segment < 12; ++segment) {
                const float angle = static_cast<float>(side) * std::numbers::pi_v<float> - 0.65f +
                    static_cast<float>(segment) * (1.30f / 12.0f);
                const float nextAngle = angle + 1.30f / 12.0f;
                const ImVec2 a{ vanishing.x + std::cos(angle) * radiusX, vanishing.y + std::sin(angle) * radiusY };
                const ImVec2 b{ vanishing.x + std::cos(nextAngle) * radiusX, vanishing.y + std::sin(nextAngle) * radiusY };
                drawList.AddLine(a, b, IM_COL32(126, 219, 255, alpha / 4), pixelScale * 9.0f);
                drawList.AddLine(a, b, IM_COL32(227, 248, 255, alpha), pixelScale * 1.5f);
            }
        }
    }
    drawList.PopClipRect();
}
