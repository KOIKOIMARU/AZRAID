#pragma once

#include <array>

namespace StageEntryTiming {
// タイトルから本編に制御を渡すときに、両方が同じ時計を読む
inline constexpr float kTitleEntryDuration = 1.80f;
inline constexpr float kTitleEntryWorldTime = 0.60f;
inline constexpr float kTitleEntryDiveTime = kTitleEntryWorldTime + 0.47f;
inline constexpr float kStageFlybyFollowTime = 0.20f;
inline constexpr float kStagePortalBoostTime = 1.18f;
inline constexpr float kStagePortalTransitDuration = 1.85f;
inline constexpr float kStagePortalClearTime = 2.15f;
inline constexpr float kStageStartFlightEnd = 5.35f;
inline constexpr float kStageStartOverviewDuration = 6.00f;
inline constexpr float kStageStartLaunchDuration = 2.15f;
inline constexpr float kStagePatrolDuration = 12.0f;
inline constexpr float kRetryEntryDuration = 0.85f;
inline constexpr std::array<float,2> kStagePatrolCutTimes{{3.35f,9.05f}};
} // namespace StageEntryTiming
