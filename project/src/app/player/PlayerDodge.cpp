// ===== PlayerDodge.cpp =====
// 回避入力を受け付けて横移動を進め、ジャスト回避が成立したかを確定する

#include "app/player/PlayerDodge.h"

#include <algorithm>
#include <numbers>

namespace {
constexpr float kCooldown = 28.0f;
constexpr float kDistance = 4.4f;
constexpr float kInputBufferDuration = 5.0f;
}

// 押した方向と入力の猶予を覚え、再使用が可能になった時点で回避を始める
bool PlayerDodge::UpdateInput(bool pressed, int horizontalInput, float controlStep)
{
    cooldown_ = (std::max)(cooldown_ - controlStep, 0.0f);
    inputBuffer_ = (std::max)(inputBuffer_ - controlStep, 0.0f);

    if (horizontalInput != 0) {
        lastHorizontalDirection_ = horizontalInput > 0 ? 1 : -1;
    }
    if (pressed) {
        inputBuffer_ = kInputBufferDuration;
        // 再使用待ちでも、押した時点の方向で回避する
        bufferedDirection_ = lastHorizontalDirection_;
    }
    if (inputBuffer_ <= 0.0f || cooldown_ > 0.0f || IsActive()) {
        return false;
    }

    direction_ = bufferedDirection_;
    inputBuffer_ = 0.0f;
    remaining_ = kDuration;
    confirmed_ = false;
    cooldown_ = kCooldown;
    return true;
}

// 今回の更新で増えた横移動量と機体のロール角を返す　回避が終わっていれば両方0
PlayerDodge::Step PlayerDodge::Advance(float controlStep)
{
    if (!IsActive()) { return {}; }
    const float progress = 1.0f - remaining_ / kDuration;
    remaining_ = (std::max)(remaining_ - controlStep, 0.0f);
    const float nextProgress = 1.0f - remaining_ / kDuration;

    // イージングの差分を足すことで、フレーム間隔にかかわらず総移動距離を保つ
    const auto distanceRate = [](float t) { return t * (2.0f - t); };
    return {
        static_cast<float>(direction_) * kDistance *
            (distanceRate(nextProgress) - distanceRate(progress)),
        static_cast<float>(direction_) *
            (nextProgress * nextProgress * (3.0f - 2.0f * nextProgress)) *
            2.0f * std::numbers::pi_v<float>
    };
}

// 1回の回避につき、ジャスト回避の成立を一度だけ受け付ける
bool PlayerDodge::TryConfirm()
{
    if (!IsActive() || confirmed_) { return false; }
    confirmed_ = true;
    return true;
}
