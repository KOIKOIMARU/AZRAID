// タイトルの機体とロゴの動き、画面上の切断位置を時刻から求める

#pragma once

#include "app/stage/StageEntryTiming.h"
#include "engine/base/Math.h"

#include <imgui.h>

class Camera;

namespace TitleMotion {

// ロゴ・機体・背景が同じタイミングで切り替わるための時刻と座標
inline constexpr float kIdleShipScale = 2.65f;
inline constexpr float kDepartureDuration = StageEntryTiming::kTitleEntryDuration;
inline constexpr float kDepartureCutTime = 0.24f;
inline constexpr float kDepartureOpenTime = StageEntryTiming::kTitleEntryWorldTime;
inline constexpr float kDepartureImpactTime = 0.565f;
inline constexpr float kChargeStart = 2.95f;
inline constexpr float kFlybyStart = 3.45f;
inline constexpr float kFlybyDuration = 0.24f;
inline constexpr float kFlybyLoop = 8.00f;
inline constexpr float kFlightSequence = kFlybyLoop * 3.0f;
inline constexpr float kClosePassStart = 0.40f;
inline constexpr float kArrivalDuration = 1.60f;
inline constexpr float kCutLead = 100.0f;
inline constexpr ImVec2 kStageDepartureFocus{670.0f,373.0f};

struct StagePortalIdleState {
    float gap, cutAge;
};

struct TitleFlyby {
    float phase = 0.0f;
    float flight = 0.0f;
    float energy = 0.0f;
    float lightX = -50.0f;
    int pattern = 0;
    int cutPattern = 0;
};

struct TitleStrike {
    float charge = 0.0f;
    float flash = 0.0f;
    float afterglow = 0.0f;
    float blade = 0.0f;
    float bladeX = -100.0f;
};

struct TitleCut { float base; float slope; };

struct LogoMotion {
    float cosine = 1.0f;
    float sine = 0.0f;
    float zoom = 1.0f;
    ImVec2 drift{};
};

float Smooth(float value);
StagePortalIdleState StagePortalIdle(float elapsed);
float TitleDashDirection(int pattern);
TitleCut GetTitleCut(int pattern);
TitleStrike GetTitleStrike(const TitleFlyby& flyby);
float DepartureResistance(float departure);
float DepartureFade(float departure);
TitleStrike GetDepartureStrike(float departure);
float DepartureContactTime(float canvasX);
float DepartureImpact(float departure);
ImVec2 DepartureOffset(float departure, int half);
float LogoCutPresence(const TitleFlyby& flyby, float canvasX);
float LogoSeparation(const TitleFlyby& flyby);
LogoMotion GetLogoMotion(float elapsed, const TitleFlyby& flyby, float departure = 0.0f);
ImVec2 MoveLogoCanvas(ImVec2 point, const LogoMotion& motion, bool inverse = false);
float TitleCutY(float screenX, int pattern = 0);
Math::Vector3 TitleViewPoint(ImVec2 screen, float depth);
Math::Vector3 FlightCurve(Math::Vector3 start, Math::Vector3 controlA, Math::Vector3 controlB,
    Math::Vector3 end, float t);
Math::Vector3 FlightControlPoint(Math::Vector3 position, Math::Vector3 velocity, float time);
Math::Vector3 MoveFlightWithLogo(Math::Vector3 local, float elapsed, float phase, int pattern = 0);
Math::Vector3 GetTitleArrivalLocalPosition(float time);
Math::Vector3 TitleArrivalDirection(float time);
float TitleFlightWakeStrength(float phase, int pattern = 0);
bool IsTitleOrbitalWake(float phase, int pattern = 0);
float TitleFlightScale(float phase);
float TitleFlightBank(float phase, int pattern = 0);
Math::Vector3 TitleFlightDirection(float phase, int pattern = 0);
Math::Vector3 GetTitleFlightPosition(float phase, float elapsed, int pattern = 0);
TitleFlyby GetTitleFlyby(float clock, bool ready);
ImVec2 ProjectLogoCanvas(ImVec2 local, float separation, int half, float depth, int pattern = 0);
Math::Vector3 RotateVector(const Math::Vector3& value, const Math::Matrix4x4& matrix);
Math::Vector4 TitleExhaustColor(int layer, float fade = 1.0f);
Math::Vector3 TitleNozzleOffset(size_t nozzle, float shipScale, const Math::Matrix4x4& rotation);
Math::Vector3 FlightRotation(const Math::Vector3& direction, const Math::Vector3& referenceUp, float bank);
ImVec2 ProjectTitleCanvas(const Math::Vector3& point, const Camera& camera);
} // namespace TitleMotion
