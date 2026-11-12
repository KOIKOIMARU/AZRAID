#pragma once

#include <algorithm>

// 成功時の「接触・見切り・抜け」の時間を、カメラ・HUD・噴射で共有する
// 入力や判定は変更せず、残り時間だけから演出の強さを求める
namespace JustDodgeFeedback {
inline constexpr int kDuration = 38;
inline constexpr int kSlowDuration = 18;

inline float Smooth(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

struct Pulse { float contact = 0.0f, focus = 0.0f, release = 0.0f; };

// ジャスト回避の経過から、演出の立ち上がりと解除の強さを分けて求める
inline Pulse Evaluate(float remainingRate)
{
    if (remainingRate <= 0.0f) { return {}; }
    const float age = (1.0f - std::clamp(remainingRate, 0.0f, 1.0f)) * kDuration;
    return { 1.0f - Smooth(age / 5.0f), 1.0f - Smooth((age - 4.0f) / 12.0f),
        Smooth((age - 9.0f) / 6.0f) * (1.0f - Smooth((age - 19.0f) / 16.0f)) };
}

// 回避の終わりに向けて世界の時間倍率を通常速度へ戻す
inline float WorldTimeScale(int remainingFrames)
{
    if (remainingFrames <= 0) { return 1.0f; }
    const float age = static_cast<float>(kSlowDuration - remainingFrames);
    if (age < 3.0f) { return 0.13f; }
    return 0.24f + 0.76f * Smooth((age - 3.0f) / 15.0f);
}
}
