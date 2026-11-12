// ===== FlightMotionBlur.cpp =====
// カメラの動きや飛行速度から、モーションブラーに渡す値を更新する

#include "app/presentation/FlightMotionBlur.h"

#include "engine/3d/Camera.h"
#include "engine/3d/ScreenProjection.h"
#include "engine/base/DirectXCommon.h"

#include <imgui.h>

#include <algorithm>
#include <string_view>

// ブラーの履歴と表示確認の設定を初期化する
void FlightMotionBlur::Initialize()
{
    history_.Reset();
    mode_ = 1;
    frozen_ = false;
    char blurReview[8]{};
    review_ = GetEnvironmentVariableA("AZRAID_BLUR_REVIEW", blurReview, static_cast<DWORD>(sizeof(blurReview))) == 1 &&
        blurReview[0] == '1';
    char blurMode[16]{};
    const DWORD blurModeLength = GetEnvironmentVariableA("AZRAID_FLIGHT_BLUR", blurMode, static_cast<DWORD>(sizeof(blurMode)));
    if (blurModeLength > 0 && blurModeLength < sizeof(blurMode)) {
        const std::string_view modeName(blurMode);
        if (modeName == "off") { mode_ = 0; }
        else if (modeName == "camera") { mode_ = 2; }
        else if (modeName == "vectors") { mode_ = 3; }
    }
}

// ブラーの比較表示で方式を切り替える　停止できる場面では同じフレームで比較できる
void FlightMotionBlur::UpdateReview(const DirectXCommon& renderer, bool canFreeze)
{
    // 比較表示は明示した比較起動かDebugのF6だけ　通常のHUDに診断を混ぜない
#ifdef ENABLE_DEBUG_GUI
    if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) { review_ = true; mode_ = (mode_ + 1) % 4; }
#else
    if (review_ && ImGui::IsKeyPressed(ImGuiKey_F6, false)) { mode_ = (mode_ + 1) % 4; }
#endif
    if (review_) {
        if (!canFreeze) { frozen_ = false; }
        if (canFreeze && ImGui::IsKeyPressed(ImGuiKey_F7, false)) { frozen_ = !frozen_; }
        ImGui::SetNextWindowPos({ 850.0f, 150.0f }, ImGuiCond_FirstUseEver);
        ImGui::Begin("Blur comparison", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Combo("Method / F6", &mode_, "Off\0Radial (existing)\0Camera motion\0Motion vectors\0");
        ImGui::BeginDisabled(!canFreeze);
        ImGui::Checkbox("Freeze same frame / F7", &frozen_);
        ImGui::EndDisabled();
        ImGui::TextUnformatted("Camera motion only; ship / aim remain protected.");
        if (renderer.IsBlurGpuTimingEnabled()) {
            static constexpr const char* names[] = { "Off", "Radial", "Camera", "Vectors" };
            const auto& timings = renderer.GetBlurGpuTiming();
            for (size_t i = 0; i < timings.size(); ++i) {
                if (timings[i].samples > 0) {
                    ImGui::Text("GPU post effect: %s %.3f ms (%u frames)", names[i],
                        timings[i].totalMs / timings[i].samples, timings[i].samples);
                }
            }
        }
        ImGui::End();
    }
}

// 現在と直前のカメラを使ってブラー用の行列を作り、描画側へ渡す
void FlightMotionBlur::Apply(DirectXCommon& renderer, const Camera& camera,
    const Math::Vector2& viewportMin, const Math::Vector2& viewportSize,
    const Math::Vector3& vanishingPoint, const Math::Vector3& playerPosition,
    const Math::Vector2& reticle, float strength, bool active, bool paused)
{
    const auto screenUv = [&](const Math::Vector2& screen) {
        return Math::Vector2{
            (screen.x - viewportMin.x) / (std::max)(viewportSize.x, 1.0f),
            (screen.y - viewportMin.y) / (std::max)(viewportSize.y, 1.0f) };
    };
    Math::Vector2 vanishing{ viewportMin.x + viewportSize.x * 0.5f, viewportMin.y + viewportSize.y * 0.45f };
    Math::Vector2 playerScreen{ viewportMin.x + viewportSize.x * 0.5f, viewportMin.y + viewportSize.y * 0.7f };
    const auto& viewProjection = camera.GetViewProjectionMatrix();
    ScreenProjection::ToViewport(vanishingPoint, viewProjection, viewportMin, viewportSize, vanishing);
    ScreenProjection::ToViewport(playerPosition, viewProjection, viewportMin, viewportSize, playerScreen);
    renderer.SetFlightBlur(strength, screenUv(vanishing), screenUv(playerScreen), screenUv(reticle), camera.GetAspectRatio());
    if (!paused && !frozen_) {
        history_.Update(viewProjection, camera.GetTranslate(), camera.GetRotate(),
            camera.GetAspectRatio(), renderer.GetDeltaTime(), active);
    }
    renderer.SetCameraMotionBlur(history_.clipToPreviousClip, history_.exposureScale, mode_);
}
