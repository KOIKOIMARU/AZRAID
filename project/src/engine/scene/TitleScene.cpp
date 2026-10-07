#include "engine/scene/TitleScene.h"
#include "app/CombatHud.h"
#include "app/MenuUi.h"
#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/audio/SoundManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/io/Input.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/SceneManager.h"
#include <dinput.h>
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace {
constexpr const char* kTitle = "AZRAID";
constexpr const char* kTitleJapanese = "アズレイド";
const std::string kLogoSurface = "resources/effects/title_logo_surface.png";
constexpr const char* kShip = "free_models/player_candidates/Omen.gltf";
constexpr const char* kSky = "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";
constexpr float kIdleShipScale = 2.65f;
constexpr float kDepartureDuration = 1.04f;
constexpr float kDepartureCutTime = 0.24f;
constexpr float kDepartureOpenTime = 0.35f;
constexpr float kApproachStart = 1.70f;
constexpr float kOrbitStart = 2.10f;
constexpr float kChargeStart = 2.95f;
constexpr float kFlybyStart = 3.45f;
constexpr float kFlybyDuration = 0.24f;
constexpr float kReturnTurnTime = kFlybyStart + kFlybyDuration + 0.36f;
constexpr float kReturnSettleTime = kReturnTurnTime + 1.12f;
constexpr float kReturnEnd = kReturnSettleTime + 1.00f;
constexpr float kFlybyLoop = 8.00f;
constexpr float kFlightSequence = kFlybyLoop * 3.0f;
constexpr float kClosePassStart = 0.40f;
constexpr float kClosePassTime = 1.05f;
constexpr float kCloseExitTime = 1.35f;
constexpr float kArrivalDuration = 1.60f;
constexpr float kDashStartX = 17.0f;
constexpr float kDashTravel = 1415.0f;
constexpr float kCutLead = 100.0f;
constexpr float kTitleStrikeX = 175.0f;
constexpr float kKanaSize = 16.0f;
constexpr ImVec2 kKanaOrigin{8.0f, 108.0f};


float Smooth(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

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

float TitleDashDirection(int pattern) { return pattern == 1 ? -1.0f : 1.0f; }
float TitleDashOrigin(int pattern) { return pattern == 1 ? 1280.0f - kDashStartX : kDashStartX; }
float TitleContactTime(int pattern, float canvasX)
{
    return kFlybyStart + ((canvasX - TitleDashOrigin(pattern)) * TitleDashDirection(pattern) - kCutLead) /
        kDashTravel * kFlybyDuration;
}
float TitleStrikeTime(int pattern) { return TitleContactTime(pattern,pattern == 1 ? 1110.0f : kTitleStrikeX); }
float TitleCutCompleteTime(int pattern) { return TitleContactTime(pattern,pattern == 1 ? kTitleStrikeX : 1110.0f); }

struct TitleCut { float base; float slope; };
TitleCut GetTitleCut(int pattern) { return pattern == 2 ? TitleCut{12.0f,0.15f} : TitleCut{80.0f,-0.11f}; }
ImVec2 LogoSeparationAxis(int pattern)
{
    return pattern == 2 ? ImVec2{-0.08f,-0.70f} : ImVec2{pattern == 1 ? -0.62f : 0.55f,-0.38f};
}

TitleStrike GetTitleStrike(const TitleFlyby& flyby)
{
    const float time = flyby.phase - TitleStrikeTime(flyby.pattern);
    TitleStrike strike;
    const float dashTime = flyby.phase - kFlybyStart;
    strike.bladeX = TitleDashOrigin(flyby.pattern) + TitleDashDirection(flyby.pattern) *
        (kCutLead + kDashTravel * std::clamp(dashTime / kFlybyDuration, 0.0f, 1.0f));
    strike.charge = Smooth((flyby.phase - kChargeStart) / (kFlybyStart - kChargeStart)) *
        (1.0f - Smooth(dashTime / 0.025f));
    if (time >= 0.0f) {
        strike.flash = Smooth(time / 0.012f) * std::exp(-time / 0.11f);
        strike.afterglow = Smooth(time / 0.015f) * std::exp(-time / 1.00f);
    }
    if (dashTime >= 0.0f) {
        strike.blade = Smooth(dashTime / 0.010f) *
            (1.0f - Smooth((dashTime - kFlybyDuration) / 0.12f));
    }
    return strike;
}

// TitleRift.PS.hlslと同じ時計。110msで切り抜けてから、重い上下の面を放つ。
TitleStrike GetDepartureStrike(float departure)
{
    const float time = departure - kDepartureCutTime;
    TitleStrike strike;
    strike.charge = Smooth(departure / kDepartureCutTime) * (1.0f - Smooth(time / 0.035f));
    strike.bladeX = -120.0f + 1520.0f * Smooth(time / 0.11f);
    if (time >= 0.0f) {
        strike.flash = Smooth(time / 0.014f) * std::exp(-time / 0.14f);
        strike.afterglow = Smooth(time / 0.02f) * std::exp(-time / 0.24f);
        strike.blade = 1.0f - Smooth((time - 0.11f) / 0.12f);
    }
    return strike;
}

ImVec2 DepartureOffset(float departure, int half)
{
    if (half < 0) { return {}; }
    const float time = (std::max)(0.0f, departure - kDepartureOpenTime);
    const float travel = 740.0f * std::pow(std::clamp(time / 0.65f, 0.0f, 1.0f), 1.40f);
    const float recoil = Smooth(time / 0.025f) * std::exp(-time / 0.12f) * 26.0f;
    const float side = half == 0 ? 1.0f : -1.0f;
    return {side * (travel + recoil) * 0.30f, -side * (travel + recoil)};
}

// 文字に機首が届いてから反応する。飛行と切断は同じ位置・速度を使う。
// TitleRift.PS.hlslも同じ時計・係数で背景の断面を動かす。
float LogoImpulse(const TitleFlyby& flyby)
{
    const float time = (std::max)(0.0f, flyby.phase - TitleStrikeTime(flyby.pattern));
    return Smooth(time / 0.035f) * std::exp(-(std::max)(0.0f,time - 0.085f) / 0.60f) *
        (1.0f + std::sin(time * 20.0f) * 0.05f);
}

float LogoCutPresence(const TitleFlyby& flyby, float canvasX)
{
    const float time = flyby.phase - TitleContactTime(flyby.pattern,canvasX);
    return Smooth(time / 0.012f) * (1.0f - Smooth((time - 0.85f) / 0.70f));
}

float LogoSeparation(const TitleFlyby& flyby)
{
    const float time = (std::max)(0.0f, flyby.phase - TitleCutCompleteTime(flyby.pattern));
    const float strength = flyby.pattern == 2 ? 88.0f : flyby.pattern == 1 ? 70.0f : 49.0f;
    // 斬り終えてから面全体を放つ。文字ごとに先に移動すると隣へ食い込む。
    // 面が押されるまでの抵抗と、開き切った重さを短く保ってから戻す。
    return Smooth(time / 0.060f) * std::exp(-(std::max)(0.0f,time - 0.115f) /
        (flyby.pattern == 2 ? 0.72f : 0.60f)) * strength *
        LogoCutPresence(flyby,flyby.pattern == 1 ? 1110.0f : kTitleStrikeX);
}

float LogoZoom(float elapsed, const TitleFlyby& flyby)
{
    return 1.0f + Smooth((elapsed - 0.35f) / 1.20f) *
        (std::sin(elapsed * 0.58f) * 0.012f + GetTitleStrike(flyby).charge * 0.016f + LogoImpulse(flyby) * 0.026f);
}

struct LogoMotion {
    float cosine = 1.0f;
    float sine = 0.0f;
    float zoom = 1.0f;
    ImVec2 drift{};
};

// 各頂点で三角関数・指数を再計算せず、ロゴ全体で同じ姿勢を共有する。
LogoMotion GetLogoMotion(float elapsed, const TitleFlyby& flyby)
{
    const float ready = Smooth((elapsed - 0.35f) / 1.20f);
    const float angle = std::sin(elapsed * 0.54f) * 0.020f * ready;
    const float time = (std::max)(0.0f, flyby.phase - TitleStrikeTime(flyby.pattern));
    const float shock = std::sin(time * 42.0f) * std::exp(-time / 0.16f) * 7.0f;
    return {std::cos(angle), std::sin(angle), LogoZoom(elapsed, flyby),
        {std::sin(elapsed * 0.43f) * 12.0f * ready + shock,
        std::sin(elapsed * 0.67f) * 6.0f * ready - LogoImpulse(flyby) * 10.0f + shock * 0.22f}};
}

ImVec2 MoveLogoCanvas(ImVec2 point, const LogoMotion& motion, bool inverse = false)
{
    const float x = point.x - 641.25f - (inverse ? motion.drift.x : 0.0f);
    const float y = point.y - 334.5f - (inverse ? motion.drift.y : 0.0f);
    const float sine = inverse ? -motion.sine : motion.sine;
    const float zoom = inverse ? 1.0f / motion.zoom : motion.zoom;
    return {641.25f + (x * motion.cosine - y * sine) * zoom + (inverse ? 0.0f : motion.drift.x),
        334.5f + (x * sine + y * motion.cosine) * zoom + (inverse ? 0.0f : motion.drift.y)};
}

float TitleCutY(float screenX, int pattern = 0)
{
    // ProjectLogoCanvasの切断線を逆算する。飛行と反射は同じ傾き・透視を使う。
    constexpr float cosine = 0.9950042f, sine = 0.0998334f;
    const auto cut = GetTitleCut(pattern);
    const float intercept = cut.base + cut.slope * 255.0f - 50.0f;
    const float q = (screenX - 195.0f) / 1.75f - 255.0f;
    const float x = (q - intercept * sine) / (cosine + cut.slope * sine - q * 0.00042f);
    const float y = intercept + cut.slope * x;
    return 247.0f + (50.0f + (-x * sine + y * cosine) / (1.0f + x * 0.00042f)) * 1.75f;
}

ImVec2 TitleCruisePosition(float phase)
{
    const float cycle = phase * 6.2831853f / kFlybyLoop;
    return {916.0f + std::sin(cycle) * 18.0f,143.0f + std::sin(cycle + 0.80f) * 10.0f};
}

Math::Vector3 TitleCruiseDirection(float phase)
{
    // 待機中も翼をゆっくり見せる。加速・帰還の接続点では基準姿勢へ戻す。
    const float idle = phase < kApproachStart ? 1.0f - Smooth((phase - 1.10f) / 0.60f) :
        Smooth((phase - kReturnEnd) / 0.55f);
    const float sway = std::sin(phase * 6.2831853f / kFlybyLoop) * idle;
    return {0.90f + sway * 0.08f,0.22f + sway * 0.04f,0.38f - sway * 0.20f};
}

Math::Vector3 TitleViewPoint(ImVec2 screen, float depth)
{
    const float projection = 360.0f / std::tan(0.31f);
    return {(screen.x - 640.0f) * depth / projection,(360.0f - screen.y) * depth / projection,depth};
}

Math::Vector3 FlightCurve(Math::Vector3 start, Math::Vector3 controlA, Math::Vector3 controlB,
    Math::Vector3 end, float t)
{
    const float u = 1.0f - t;
    return {u * u * u * start.x + 3.0f * u * u * t * controlA.x +
        3.0f * u * t * t * controlB.x + t * t * t * end.x,
        u * u * u * start.y + 3.0f * u * u * t * controlA.y +
        3.0f * u * t * t * controlB.y + t * t * t * end.y,
        u * u * u * start.z + 3.0f * u * u * t * controlA.z +
        3.0f * u * t * t * controlB.z + t * t * t * end.z};
}

Math::Vector3 TitleOrbitPosition(float angle, int pattern = 0)
{
    // 手前の機体から数百単位離れた軌道。点になる距離で一周し、再び切断線へ戻る。
    if (pattern == 2) {
        return {130.0f * std::cos(angle),20.0f + 260.0f * std::sin(angle),520.0f + 160.0f * std::cos(angle)};
    }
    return {TitleDashDirection(pattern) * 340.0f * std::cos(angle),20.0f + 130.0f * std::sin(angle),
        520.0f + 180.0f * std::sin(angle)};
}

Math::Vector3 FlightControlPoint(Math::Vector3 position, Math::Vector3 velocity, float time)
{
    return {position.x + velocity.x * time,position.y + velocity.y * time,position.z + velocity.z * time};
}

Math::Vector3 TitleCruiseVelocity(float phase)
{
    const auto before = TitleViewPoint(TitleCruisePosition(phase - 0.004f),22.0f);
    const auto after = TitleViewPoint(TitleCruisePosition(phase + 0.004f),22.0f);
    return {(after.x - before.x) / 0.008f,(after.y - before.y) / 0.008f,0.0f};
}

Math::Vector3 TitleReturnSettleOffset(float time)
{
    const auto cruise = TitleCruisePosition(kReturnSettleTime);
    const auto target = TitleViewPoint(cruise,22.0f);
    const auto overshoot = TitleViewPoint({cruise.x - 18.0f,cruise.y + 6.0f},21.0f);
    const Math::Vector3 offset{overshoot.x - target.x,overshoot.y - target.y,overshoot.z - target.z};
    constexpr Math::Vector3 velocity{-1.40f,-0.10f,-1.30f};
    constexpr float damping = 5.50f;
    // 戻りの速度を引き継いで少し行き過ぎ、余韻を減衰させて巡航へ繋ぐ。
    const float fade = std::exp(-damping * time) * (1.0f - Smooth((time - 0.65f) / 0.35f));
    return {(offset.x + (velocity.x + damping * offset.x) * time) * fade,
        (offset.y + (velocity.y + damping * offset.y) * time) * fade,
        (offset.z + (velocity.z + damping * offset.z) * time) * fade};
}

Math::Vector3 GetTitleFlightLocalPosition(float phase, int pattern = 0)
{
    const bool closePass = pattern == 2;
    const float dashDirection = TitleDashDirection(pattern);
    const float dashOrigin = TitleDashOrigin(pattern);
    if (closePass && phase >= kClosePassStart && phase < kOrbitStart) {
        // 大きく翼を見せて宙返りし、縦の軌道を経て別角度の一閃へ。
        const auto nearPosition = TitleViewPoint({570.0f,105.0f},15.5f);
        constexpr Math::Vector3 nearVelocity{-20.0f,8.0f,0.0f};
        if (phase < kClosePassTime) {
            const float duration = kClosePassTime - kClosePassStart;
            const auto start = TitleViewPoint(TitleCruisePosition(kClosePassStart),22.0f);
            return FlightCurve(start,FlightControlPoint(start,TitleCruiseVelocity(kClosePassStart),duration / 3.0f),
                FlightControlPoint(nearPosition,nearVelocity,-duration / 3.0f),nearPosition,
                (phase - kClosePassStart) / duration);
        }
        const auto exitPosition = TitleViewPoint({-200.0f,-400.0f},120.0f);
        constexpr Math::Vector3 exitVelocity{-90.0f,60.0f,250.0f};
        if (phase < kCloseExitTime) {
            const float duration = kCloseExitTime - kClosePassTime;
            return FlightCurve(nearPosition,FlightControlPoint(nearPosition,nearVelocity,duration / 3.0f),
                FlightControlPoint(exitPosition,exitVelocity,-duration / 3.0f),exitPosition,
                (phase - kClosePassTime) / duration);
        }
        const float duration = kOrbitStart - kCloseExitTime;
        const auto end = TitleOrbitPosition(3.1415927f,pattern);
        const float angularSpeed = 6.2831853f / (kChargeStart - kOrbitStart);
        const Math::Vector3 endVelocity{0.0f,-260.0f * angularSpeed,0.0f};
        return FlightCurve(exitPosition,FlightControlPoint(exitPosition,exitVelocity,duration / 3.0f),
            FlightControlPoint(end,endVelocity,-duration / 3.0f),end,
            (phase - kCloseExitTime) / duration);
    }
    if (phase < kApproachStart || phase >= kReturnEnd) {
        return TitleViewPoint(TitleCruisePosition(phase),22.0f);
    }
    if (phase < kOrbitStart) {
        // 画面内でUターンせず、右奥へ加速して小さな光になる。
        return FlightCurve(TitleViewPoint(TitleCruisePosition(kApproachStart),22.0f),
            {90.0f,22.0f,82.0f},{-dashDirection * 340.0f,175.0f,740.0f},TitleOrbitPosition(3.1415927f,pattern),
            (phase - kApproachStart) / (kOrbitStart - kApproachStart));
    }
    if (phase < kChargeStart) {
        const float orbit = (phase - kOrbitStart) / (kChargeStart - kOrbitStart);
        return TitleOrbitPosition(3.1415927f + orbit * 6.2831853f,pattern);
    }
    if (phase < kFlybyStart) {
        // 奥の軌道から手前へ突入。終端の速度を一閃と揃え、左端で止めない。
        const float approach = kFlybyStart - kChargeStart;
        const float controlX = dashOrigin - dashDirection * kDashTravel / kFlybyDuration * approach / 3.0f;
        return FlightCurve(TitleOrbitPosition(3.1415927f,pattern),{-dashDirection * 450.0f,-90.0f,330.0f},
            TitleViewPoint({controlX,TitleCutY(controlX,pattern)},22.0f),
            TitleViewPoint({dashOrigin,TitleCutY(dashOrigin,pattern)},22.0f),
            (phase - kChargeStart) / approach);
    }
    if (phase <= kFlybyStart + kFlybyDuration) {
        // 切断中は等速。光の先端もこの機首に追従し、先に文字だけを切らない。
        const float x = dashOrigin + dashDirection * kDashTravel * (phase - kFlybyStart) / kFlybyDuration;
        return TitleViewPoint({x,TitleCutY(x,pattern)},22.0f);
    }
    const auto turnPosition = TitleViewPoint({pattern == 1 ? -80.0f : 1360.0f,85.0f},150.0f);
    const Math::Vector3 turnVelocity{-dashDirection * 70.0f,-18.0f,-80.0f};
    if (phase < kReturnTurnTime) {
        const float startTime = kFlybyStart + kFlybyDuration;
        const float duration = kReturnTurnTime - startTime;
        const float endX = dashOrigin + dashDirection * kDashTravel;
        const auto start = TitleViewPoint({endX,TitleCutY(endX,pattern)},22.0f);
        const float speed = dashDirection * kDashTravel / kFlybyDuration;
        const float slope = (TitleCutY(endX + 1.0f,pattern) - TitleCutY(endX - 1.0f,pattern)) * 0.5f;
        const auto velocity = TitleViewPoint({640.0f + speed,360.0f + slope * speed},22.0f);
        // 一閃の速度のまま画面外へ抜けて奥で旋回。曲線の継ぎ目で停止しない。
        return FlightCurve(start,FlightControlPoint(start,{velocity.x,velocity.y,0.0f},duration / 3.0f),
            FlightControlPoint(turnPosition,turnVelocity,-duration / 3.0f),turnPosition,
            (phase - startTime) / duration);
    }
    const auto target = TitleViewPoint(TitleCruisePosition(kReturnSettleTime),22.0f);
    const auto overshoot = FlightControlPoint(target,TitleReturnSettleOffset(0.0f),1.0f);
    if (phase < kReturnSettleTime) {
        const float duration = kReturnSettleTime - kReturnTurnTime;
        const auto cruiseVelocity = TitleCruiseVelocity(kReturnSettleTime);
        const Math::Vector3 endVelocity{cruiseVelocity.x - 1.40f,cruiseVelocity.y - 0.10f,-1.30f};
        return FlightCurve(turnPosition,FlightControlPoint(turnPosition,turnVelocity,duration / 3.0f),
            FlightControlPoint(overshoot,endVelocity,-duration / 3.0f),overshoot,
            (phase - kReturnTurnTime) / duration);
    }
    return FlightControlPoint(TitleViewPoint(TitleCruisePosition(phase),22.0f),
        TitleReturnSettleOffset(phase - kReturnSettleTime),1.0f);
}

Math::Vector3 MoveFlightWithLogo(Math::Vector3 local, float elapsed, float phase, int pattern = 0)
{
    TitleFlyby flyby;
    flyby.phase = phase;
    flyby.pattern = pattern;
    const float projection = 360.0f / std::tan(0.31f);
    const ImVec2 canvas{640.0f + local.x * projection / local.z,360.0f - local.y * projection / local.z};
    return TitleViewPoint(MoveLogoCanvas(canvas,GetLogoMotion(elapsed,flyby)),local.z);
}

Math::Vector3 GetTitleArrivalLocalPosition(float time)
{
    const float t = 1.0f - std::pow(1.0f - std::clamp(time / kArrivalDuration,0.0f,1.0f),2.30f);
    return FlightCurve(TitleViewPoint({-280.0f,72.0f},360.0f),TitleViewPoint({150.0f,80.0f},220.0f),
        TitleViewPoint({690.0f,100.0f},22.0f),TitleViewPoint(TitleCruisePosition(0.0f),22.0f),t);
}

Math::Vector3 TitleArrivalDirection(float time)
{
    const auto before = GetTitleArrivalLocalPosition(time - 0.004f);
    const auto after = GetTitleArrivalLocalPosition(time + 0.004f);
    const auto flight = Math::Normalize({after.x - before.x,after.y - before.y,after.z - before.z});
    const float settle = Smooth((time - kArrivalDuration + 0.50f) / 0.50f);
    return {flight.x + (0.90f - flight.x) * settle,flight.y + (0.22f - flight.y) * settle,
        flight.z + (0.38f - flight.z) * settle};
}

float TitleFlightWakeStrength(float phase, int pattern = 0)
{
    const bool closePass = pattern == 2;
    const float launch = closePass ? kClosePassStart : kApproachStart;
    const float orbit = Smooth((phase - launch) / (closePass ? 0.22f : 0.09f)) *
        (1.0f - Smooth((phase - kChargeStart) / 0.08f));
    return (std::max)(orbit * 0.80f,GetTitleStrike(TitleFlyby{phase}).blade);
}

bool IsTitleOrbitalWake(float phase, int pattern = 0)
{
    return phase >= (pattern == 2 ? 1.60f : kApproachStart) && phase < kChargeStart + 0.08f;
}

float TitleFlightScale(float phase)
{
    const float attack = Smooth((phase - kApproachStart) / (kChargeStart - kApproachStart)) *
        (1.0f - Smooth((phase - kFlybyStart - kFlybyDuration) / (kReturnSettleTime - kFlybyStart - kFlybyDuration)));
    return kIdleShipScale + (1.50f - kIdleShipScale) * attack;
}

float TitleFlightBank(float phase, int pattern = 0)
{
    const float cruise = -0.70f + std::sin(phase * 6.2831853f / kFlybyLoop + 0.35f) * 0.24f;
    const float roll = pattern == 2 ? 6.2831853f * Smooth((phase - kClosePassStart) / 0.95f) : 0.0f;
    const float cutting = Smooth((phase - kChargeStart + 0.18f) / 0.18f) *
        (1.0f - Smooth((phase - kReturnTurnTime) / (kReturnEnd - kReturnTurnTime)));
    return cruise + (-0.06f - cruise) * cutting + roll;
}

Math::Vector3 TitleFlightDirection(float phase, int pattern = 0)
{
    const bool closePass = pattern == 2;
    const Math::Vector3 dashDirection{TitleDashDirection(pattern) * 0.95f,
        (pattern == 2 ? -0.05f : 0.23f) * TitleDashDirection(pattern),0.30f};
    const float launch = closePass ? kClosePassStart : kApproachStart;
    if (phase < launch || phase >= kReturnEnd) { return TitleCruiseDirection(phase); }
    if (phase >= kFlybyStart && phase <= kFlybyStart + kFlybyDuration) {
        return dashDirection;
    }
    const float alignStart = kReturnSettleTime - 0.70f;
    if (phase >= alignStart) {
        // 小さく戻ってくる段階から機首を整え、着く瞬間だけで姿勢を戻さない。
        const auto before = GetTitleFlightLocalPosition(alignStart - 0.004f,pattern);
        const auto after = GetTitleFlightLocalPosition(alignStart + 0.004f,pattern);
        const auto from = Math::Normalize({after.x - before.x,after.y - before.y,after.z - before.z});
        const auto to = Math::Normalize({0.90f,0.22f,0.38f});
        const float blend = Smooth((phase - alignStart) / (kReturnEnd - alignStart));
        const float cosine = std::clamp(from.x * to.x + from.y * to.y + from.z * to.z,-1.0f,1.0f);
        const float angle = std::acos(cosine);
        const float divisor = std::sin(angle);
        const float a = std::abs(divisor) > 0.001f ? std::sin((1.0f - blend) * angle) / divisor : 1.0f - blend;
        const float b = std::abs(divisor) > 0.001f ? std::sin(blend * angle) / divisor : blend;
        return {from.x * a + to.x * b,from.y * a + to.y * b,from.z * a + to.z * b};
    }
    const auto before = GetTitleFlightLocalPosition((std::max)(launch,phase - 0.004f),pattern);
    const auto after = GetTitleFlightLocalPosition((std::min)(kReturnEnd,phase + 0.004f),pattern);
    const auto flight = Math::Normalize({after.x - before.x,after.y - before.y,after.z - before.z});
    const float turn = Smooth((phase - launch) / (closePass ? 0.28f : 0.18f));
    const auto cruise = TitleCruiseDirection(launch);
    Math::Vector3 direction{cruise.x * (1.0f - turn) + flight.x * turn,
        cruise.y * (1.0f - turn) + flight.y * turn,cruise.z * (1.0f - turn) + flight.z * turn};
    if (phase >= kChargeStart && phase < kFlybyStart) {
        const float align = Smooth((phase - kFlybyStart + 0.07f) / 0.07f);
        direction = {direction.x + (dashDirection.x - direction.x) * align,
            direction.y + (dashDirection.y - direction.y) * align,direction.z + (dashDirection.z - direction.z) * align};
    }
    return direction;
}

Math::Vector3 GetTitleFlightPosition(float phase, float elapsed, int pattern = 0)
{
    return MoveFlightWithLogo(GetTitleFlightLocalPosition(phase,pattern),elapsed,phase,pattern);
}

TitleFlyby GetTitleFlyby(float clock, bool ready)
{
    TitleFlyby motion;
    if (!ready) { return motion; }
    motion.phase = std::fmod(clock, kFlybyLoop);
    motion.pattern = static_cast<int>(std::fmod(clock,kFlightSequence) / kFlybyLoop);
    // 角度の違う断面は実際に斬った瞬間から残す。ループ境界で勝手に切り直さない。
    motion.cutPattern = ((motion.pattern == 2 && motion.phase >= kFlybyStart) ||
        (clock >= kFlightSequence && motion.pattern == 0 && motion.phase < kFlybyStart)) ? 2 : 0;
    motion.flight = std::clamp((motion.phase - kFlybyStart) / kFlybyDuration, 0.0f, 1.0f);
    const auto strike = GetTitleStrike(motion);
    motion.energy = 0.42f + strike.charge * 0.20f + TitleFlightWakeStrength(motion.phase,motion.pattern) * 0.38f;
    motion.lightX = strike.bladeX;
    return motion;
}

ImVec2 ProjectLogoCanvas(ImVec2 local, float separation, int half, float depth, int pattern = 0)
{
    const float side = half < 0 ? 0.0f : half == 0 ? 1.0f : -1.0f;
    const auto axis = LogoSeparationAxis(pattern);
    const float x = local.x - 255.0f + side * separation * axis.x + depth * 3.6f;
    const float y = local.y - 50.0f + side * separation * axis.y + depth * 6.0f;
    const float perspective = 1.0f / (1.0f + x * 0.00042f);
    constexpr float cosine = 0.9950042f, sine = 0.0998334f;
    return { 255.0f + (x * cosine + y * sine) * perspective,
        50.0f + (-x * sine + y * cosine) * perspective };
}

Math::Vector3 RotateVector(const Math::Vector3& value, const Math::Matrix4x4& matrix)
{
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] };
}

Math::Vector4 TitleExhaustColor(int layer, float fade = 1.0f)
{
    if (layer == 2) { return {0.86f,0.94f,1.0f,0.94f * fade}; }
    return layer == 0 ? Math::Vector4{0.16f,0.37f,0.95f,0.68f * fade} :
        Math::Vector4{0.79f,0.91f,1.0f,0.96f * fade};
}

Math::Vector3 FlightRotation(const Math::Vector3& direction, const Math::Vector3& referenceUp, float bank)
{
    const auto cross = [](const Math::Vector3& a, const Math::Vector3& b) {
        return Math::Vector3{a.y * b.z - a.z * b.y,a.z * b.x - a.x * b.z,a.x * b.y - a.y * b.x};
    };
    const auto forward = Math::Normalize(direction);
    const auto right = Math::Normalize(cross(referenceUp,forward));
    const auto up = cross(forward,right);
    const float cosine = std::cos(bank), sine = std::sin(bank);
    const Math::Vector3 bankedRight{right.x * cosine + up.x * sine,
        right.y * cosine + up.y * sine,right.z * cosine + up.z * sine};
    const Math::Vector3 bankedUp{up.x * cosine - right.x * sine,
        up.y * cosine - right.y * sine,up.z * cosine - right.z * sine};
    // 機首を接線へ向けたまま、その軸を中心にバンクする。エンジンも同じ姿勢を使う。
    const float yaw = std::asin(std::clamp(-bankedRight.z,-1.0f,1.0f));
    if (std::abs(std::cos(yaw)) < 0.0001f) {
        return {0.0f,yaw,std::atan2(-bankedUp.x,bankedUp.y)};
    }
    return {std::atan2(bankedUp.z,forward.z),yaw,std::atan2(bankedRight.y,bankedRight.x)};
}

ImVec2 ProjectTitleCanvas(const Math::Vector3& point, const Camera& camera)
{
    const auto& matrix = camera.GetViewProjectionMatrix();
    const float w = (std::max)(0.001f, point.x * matrix.m[0][3] + point.y * matrix.m[1][3] +
        point.z * matrix.m[2][3] + matrix.m[3][3]);
    const float x = (point.x * matrix.m[0][0] + point.y * matrix.m[1][0] +
        point.z * matrix.m[2][0] + matrix.m[3][0]) / w;
    const float y = (point.x * matrix.m[0][1] + point.y * matrix.m[1][1] +
        point.z * matrix.m[2][1] + matrix.m[3][1]) / w;
    const float fit = (std::min)(camera.GetAspectRatio() / (1280.0f / 720.0f), 1.0f);
    return { 640.0f + x * 360.0f * camera.GetAspectRatio() / fit, 360.0f - y * 360.0f / fit };
}

struct LogoPiece {
    std::array<ImVec2, 16> points{};
    int count = 0;
    int half = 0;
    int glyph = 0;
    size_t edgeStart = 0;
    size_t edgeCount = 0;
    float contactX = 0.0f;
    float contactRightX = 0.0f;
};
struct LogoEdge {
    ImVec2 start{}, end{};
    bool cut = false;
};
struct LogoGeometry {
    std::array<LogoPiece, 64> pieces{};
    size_t count = 0;
    std::array<LogoEdge, 1024> edges{};
    size_t edgeCount = 0;
};

void BuildLogoEdges(LogoGeometry& geometry, int pattern)
{
    const auto cut = GetTitleCut(pattern);
    const auto cross = [](ImVec2 a, ImVec2 b) { return a.x * b.y - a.y * b.x; };
    const auto onCut = [&](ImVec2 point) {
        return std::abs(std::abs(point.y - cut.base - point.x * cut.slope) - 0.65f) < 0.005f;
    };
    for (size_t index = 0; index < geometry.count; ++index) {
        auto& piece = geometry.pieces[index];
        piece.edgeStart = geometry.edgeCount;
        for (int vertex = 0; vertex < piece.count; ++vertex) {
            const auto a = piece.points[vertex], b = piece.points[(vertex + 1) % piece.count];
            const ImVec2 edge{b.x - a.x,b.y - a.y};
            const float length = std::hypot(edge.x,edge.y);
            if (length <= 0.001f) { continue; }
            // 同じ文字の別ストロークに埋まる辺を除く。共有境界は外側へ少し出して判定。
            const ImVec2 probe{a.x + edge.y / length * 0.01f,a.y - edge.x / length * 0.01f};
            std::array<std::array<float,2>,64> covered{};
            size_t coveredCount = 0;
            for (size_t otherIndex = 0; otherIndex < geometry.count; ++otherIndex) {
                const auto& other = geometry.pieces[otherIndex];
                if (otherIndex == index || other.glyph != piece.glyph || other.half != piece.half) { continue; }
                float begin = 0.0f, end = 1.0f;
                for (int otherVertex = 0; otherVertex < other.count; ++otherVertex) {
                    const auto p = other.points[otherVertex], q = other.points[(otherVertex + 1) % other.count];
                    const ImVec2 side{q.x - p.x,q.y - p.y};
                    const float startDistance = cross(side,{probe.x - p.x,probe.y - p.y});
                    const float endDistance = startDistance + cross(side,edge);
                    if (startDistance < 0.0f && endDistance < 0.0f) { end = begin; break; }
                    if ((startDistance < 0.0f) != (endDistance < 0.0f)) {
                        const float t = startDistance / (startDistance - endDistance);
                        if (startDistance < 0.0f) { begin = (std::max)(begin,t); }
                        else { end = (std::min)(end,t); }
                    }
                    if (end <= begin) { break; }
                }
                if (end > begin) { covered[coveredCount++] = {begin,end}; }
            }
            std::sort(covered.begin(),covered.begin() + coveredCount,
                [](const auto& aRange, const auto& bRange) { return aRange[0] < bRange[0]; });
            const auto append = [&](float begin, float end) {
                if ((end - begin) * length <= 0.01f) { return; }
                if (geometry.edgeCount >= geometry.edges.size()) { throw std::runtime_error("Title logo edge capacity exceeded"); }
                geometry.edges[geometry.edgeCount++] = {{a.x + edge.x * begin,a.y + edge.y * begin},
                    {a.x + edge.x * end,a.y + edge.y * end},piece.half >= 0 && onCut(a) && onCut(b)};
            };
            float cursor = 0.0f;
            for (size_t range = 0; range < coveredCount; ++range) {
                append(cursor,covered[range][0]);
                cursor = (std::max)(cursor,covered[range][1]);
            }
            append(cursor,1.0f);
        }
        piece.edgeCount = geometry.edgeCount - piece.edgeStart;
    }
}

LogoGeometry CreateLogoGeometry(int pattern = 0, bool whole = false)
{
    LogoGeometry geometry;
    const auto cut = GetTitleCut(pattern);
    float pen = 0.0f;
    int glyph = 0;
    const auto polygon = [&](std::initializer_list<ImVec2> shape) {
        std::array<ImVec2, 12> source{};
        int count = 0;
        for (const auto point : shape) {
            source[count++] = { pen + point.x + (100.0f - point.y) * 0.19f, point.y };
        }
        for (int half = 0; half < 2; ++half) {
            LogoPiece piece;
            piece.half = half;
            piece.glyph = glyph;
            const float letterX = pen + 4.0f;
            piece.contactX = 195.0f + ProjectLogoCanvas({letterX,cut.base + letterX * cut.slope},
                0.0f,half,0.0f).x * 1.75f;
            for (int vertex = 0; vertex < count; ++vertex) {
                piece.contactRightX = (std::max)(piece.contactRightX,
                    195.0f + ProjectLogoCanvas(source[vertex],0.0f,half,0.0f).x * 1.75f);
            }
            if (whole) {
                piece.half = -1;
                piece.count = count;
                std::copy_n(source.begin(),count,piece.points.begin());
                geometry.pieces[geometry.count++] = piece;
                break;
            }
            const auto distance = [&](ImVec2 point) {
                const float distanceToCut = point.y - cut.base - point.x * cut.slope;
                return half == 0 ? -distanceToCut - 0.65f : distanceToCut - 0.65f;
            };
            for (int index = 0; index < count; ++index) {
                const auto a = source[index], b = source[(index + 1) % count];
                const float da = distance(a), db = distance(b);
                if (da >= 0.0f) { piece.points[piece.count++] = a; }
                if ((da >= 0.0f) != (db >= 0.0f)) {
                    const float t = da / (da - db);
                    piece.points[piece.count++] = { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
                }
            }
            if (piece.count >= 3) { geometry.pieces[geometry.count++] = piece; }
        }
    };
    for (const char* letter = kTitle; *letter; ++letter) {
        switch (*letter) {
        case 'A':
            polygon({ {0,100}, {35,0}, {55,0}, {26,100} });
            polygon({ {43,0}, {58,0}, {90,100}, {65,100} });
            polygon({ {25,59}, {64,53}, {70,72}, {19,79} });
            pen += 93.0f;
            break;
        case 'Z':
            // 隣のA・Rへ張り出さず、斜線の外郭と上下の棒を同じ輪郭で繋ぐ。
            // 継ぎ目は輪郭の内側で2単位重ね、AAの細い隙間も残さない。
            polygon({ {0,0}, {88,0}, {84,20}, {0,20} });
            polygon({ {61.8667f,18}, {84.4f,18}, {84,20}, {28,80}, {4,80} });
            polygon({ {5.8667f,78}, {84.4f,78}, {80,100}, {4,100}, {4,80} });
            pen += 90.0f;
            break;
        case 'R':
            polygon({ {0,0}, {22,0}, {22,100}, {0,100} });
            polygon({ {21,0}, {65,0}, {82,18}, {21,18} });
            polygon({ {61,17}, {82,17}, {82,43}, {61,43} });
            polygon({ {21,42}, {82,42}, {63,61}, {21,61} });
            polygon({ {36,58}, {62,58}, {90,100}, {64,100} });
            pen += 96.0f;
            break;
        case 'I':
            polygon({ {3,0}, {27,0}, {21,100}, {-3,100} });
            pen += 37.0f;
            break;
        case 'D':
            polygon({ {0,0}, {22,0}, {22,100}, {0,100} });
            polygon({ {21,0}, {62,0}, {84,20}, {63,20}, {21,18} });
            polygon({ {63,19}, {84,20}, {84,78}, {63,82} });
            polygon({ {21,81}, {84,78}, {62,100}, {21,100} });
            pen += 92.0f;
            break;
        }
        ++glyph;
    }
    // 逆方向でも一文字の全ストロークを同時に斬る。棒ごとの右端を使うと、
    // 一部だけ未切断のまま残り、同じ文字内で分離・反動の時計がずれる。
    for (size_t index = 0; index < geometry.count; ++index) {
        auto& piece = geometry.pieces[index];
        for (size_t other = 0; other < geometry.count; ++other) {
            if (geometry.pieces[other].glyph == piece.glyph) {
                piece.contactRightX = (std::max)(piece.contactRightX,geometry.pieces[other].contactRightX);
            }
        }
    }
    BuildLogoEdges(geometry,pattern);
    return geometry;
}

void DrawWordmark(ImDrawList* draw, ImVec2 origin, float scale, float elapsed, const TitleFlyby& flyby,
    float departure, ImTextureID surface)
{
    static const std::array<LogoGeometry,4> geometries{CreateLogoGeometry(),CreateLogoGeometry(2),
        CreateLogoGeometry(0,true),CreateLogoGeometry(2,true)};
    const auto& geometry = geometries[flyby.cutPattern == 2 ? 1 : 0];
    const auto& wholeGeometry = geometries[flyby.cutPattern == 2 ? 3 : 2];
    const auto cutLine = GetTitleCut(flyby.cutPattern);
    const float idleTime = (std::max)(0.0f, elapsed - 2.10f);
    const float idle = Smooth(idleTime / 1.40f);
    const TitleStrike strike = departure > 0.0f ? GetDepartureStrike(departure) : GetTitleStrike(flyby);
    const LogoMotion motion = GetLogoMotion(elapsed, flyby);
    float separation = LogoSeparation(flyby);
    float cutPresence = 0.0f;
    const auto onCut = [&](ImVec2 point) {
        return std::abs(std::abs(point.y - cutLine.base - point.x * cutLine.slope) - 0.65f) < 0.005f;
    };
    const auto position = [&](ImVec2 local, int half, float depth) {
        if (half >= 0 && onCut(local)) {
            const float side = half == 0 ? -1.0f : 1.0f;
            local.y -= side * 0.65f * (1.0f - cutPresence);
        }
        const auto canvas = ProjectLogoCanvas(local, separation, half, depth,flyby.pattern);
        const auto moved = MoveLogoCanvas({195.0f + canvas.x * 1.75f,
            247.0f + canvas.y * 1.75f}, motion);
        const auto offset = DepartureOffset(departure, half);
        return ImVec2{ origin.x + (moved.x + offset.x - 195.0f) / 1.75f * scale,
            origin.y + (moved.y + offset.y - 247.0f) / 1.75f * scale };
    };
    const float lightX = -220.0f + 1720.0f * std::fmod(idleTime / 3.60f + 0.35f, 1.0f);
    const auto cutLight = [&](ImVec2 screen) {
        // 投影後の画面位置を使い、実機の通過と背景・文字の反射を揃える。
        const float canvasX = 195.0f + (screen.x - origin.x) / scale * 1.75f;
        const float distance = (canvasX - lightX) / 130.0f;
        const float passed = (strike.bladeX - canvasX) * (departure > 0.0f ? 1.0f : TitleDashDirection(flyby.pattern));
        const float flybyDistance = passed / 95.0f;
        const float wake = passed > 0.0f ? std::exp(-passed / 410.0f) * 0.50f : 0.0f;
        return std::clamp(std::exp(-distance * distance) * idle * 0.32f +
            (std::exp(-flybyDistance * flybyDistance) + wake) * strike.blade +
            strike.charge * 0.18f + strike.flash * 0.72f + strike.afterglow * 0.16f, 0.0f, 1.0f);
    };
    // 側面を先にまとめて描く。隣り合う文字ストロークの側面が正面へ重ならない。
    for (int pass = 0; pass < 3; ++pass) {
        if (pass == 2) { draw->PushTexture(surface); }
        for (size_t index = 0; index < geometry.count + wholeGeometry.count; ++index) {
            const bool whole = index >= geometry.count;
            const auto& currentGeometry = whole ? wholeGeometry : geometry;
            const auto& piece = currentGeometry.pieces[whole ? index - geometry.count : index];
            const float contactX = flyby.pattern == 1 ? piece.contactRightX : piece.contactX;
            cutPresence = departure > 0.0f ?
                (departure >= kDepartureCutTime ? Smooth((strike.bladeX - contactX) / 45.0f) : 0.0f) :
                LogoCutPresence(flyby,contactX);
            // 正面は一枚だけ描く。閉じ際に未切断の文字を重ねると残像に見える。
            const bool opened = cutPresence > 0.001f;
            if (whole == opened) { continue; }
            separation = whole ? 0.0f : LogoSeparation(flyby);
            float centerX = 0.0f;
            for (int vertex = 0; vertex < piece.count; ++vertex) { centerX += piece.points[vertex].x; }
            centerX /= static_cast<float>(piece.count);
            const float reveal = Smooth((elapsed - 0.28f - centerX * 0.0013f) / 0.25f);
            if (reveal <= 0.001f) { continue; }
            const auto color = [&](ImU32 value) {
                const int alpha = static_cast<int>(static_cast<float>((value >> IM_COL32_A_SHIFT) & 255) * reveal);
                return CombatHud::SurfaceColor((value & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT));
            };
            std::array<ImVec2, 16> front{};
            for (int vertex = 0; vertex < piece.count; ++vertex) {
                front[vertex] = position(piece.points[vertex], piece.half, 0.0f);
            }
            if (pass == 0) {
                std::array<ImVec2, 16> back{};
                for (int vertex = 0; vertex < piece.count; ++vertex) {
                    back[vertex] = position(piece.points[vertex], piece.half, 1.25f);
                }
                draw->AddConvexPolyFilled(back.data(), piece.count, color(IM_COL32(12,21,43,255)));
                for (size_t edge = piece.edgeStart; edge < piece.edgeStart + piece.edgeCount; ++edge) {
                    const auto& contour = currentGeometry.edges[edge];
                    if (contour.cut) { continue; }
                    const auto a = position(contour.start,piece.half,0.0f);
                    const auto b = position(contour.end,piece.half,0.0f);
                    const auto backA = position(contour.start,piece.half,1.25f);
                    const auto backB = position(contour.end,piece.half,1.25f);
                    // 正面の奥に隠れる側面は描かない。可視面の頂点順も時計回りに揃う。
                    if ((backA.x - a.x) * (b.y - a.y) - (backA.y - a.y) * (b.x - a.x) <= 0.0f) { continue; }
                    const std::array<ImVec2, 4> side{a,backA,backB,b};
                    draw->AddConvexPolyFilled(side.data(), 4, color(IM_COL32(29,47,80,255)));
                }
            } else if (pass == 1) {
                if (whole) { continue; }
                for (size_t edge = piece.edgeStart; edge < piece.edgeStart + piece.edgeCount; ++edge) {
                    const auto& contour = currentGeometry.edges[edge];
                    if (!contour.cut) { continue; }
                    const auto a = position(contour.start,piece.half,0.0f);
                    const auto b = position(contour.end,piece.half,0.0f);
                    const std::array<ImVec2, 4> cut{a,position(contour.start,piece.half,0.72f),
                        position(contour.end,piece.half,0.72f),b};
                    const int first = draw->VtxBuffer.Size;
                    draw->AddConvexPolyFilled(cut.data(), 4, IM_COL32_WHITE);
                    for (int cutVertex = first; cutVertex < draw->VtxBuffer.Size; ++cutVertex) {
                        auto& value = draw->VtxBuffer[cutVertex];
                        const ImU32 tint = CombatHud::Mix(IM_COL32(91,125,192,255),
                            IM_COL32(242,248,255,255), cutLight(value.pos));
                        const ImU32 aa = (value.col >> IM_COL32_A_SHIFT) & 255;
                        const ImU32 alpha = static_cast<ImU32>(static_cast<float>(aa) * reveal * cutPresence);
                        value.col = (CombatHud::SurfaceColor(tint) & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
                    }
                    const ImVec2 midpoint{(a.x + b.x) * 0.5f,(a.y + b.y) * 0.5f};
                    draw->AddLine(a, b, color(CombatHud::Mix(
                        IM_COL32(133,155,184,static_cast<int>(255.0f * cutPresence)),
                        IM_COL32(255,255,255,static_cast<int>(255.0f * cutPresence)), cutLight(midpoint))), 1.0f * scale);
                }
            } else {
                const int first = draw->VtxBuffer.Size;
                draw->AddConvexPolyFilled(front.data(), piece.count, IM_COL32_WHITE);
                for (int vertex = first; vertex < draw->VtxBuffer.Size; ++vertex) {
                    auto& value = draw->VtxBuffer[vertex];
                    const auto offset = DepartureOffset(departure, piece.half);
                    const ImVec2 neutralScreen{value.pos.x - offset.x / 1.75f * scale,
                        value.pos.y - offset.y / 1.75f * scale};
                    const float metalX = 195.0f + (neutralScreen.x - origin.x) / scale * 1.75f;
                    // 共通の画面座標で面全体をサンプリング。ストロークごとの頂点補間で明暗を切らない。
                    value.uv = {0.5f + (metalX - lightX) / 4096.0f,
                        (neutralScreen.y - origin.y) / (100.0f * scale)};
                    const int shade = static_cast<int>(255.0f *
                        (1.0f - (!whole && piece.half == 1 ? cutPresence * 0.18f : 0.0f)));
                    const ImU32 aa = (value.col >> IM_COL32_A_SHIFT) & 255;
                    const ImU32 alpha = static_cast<ImU32>(static_cast<float>(aa) * reveal);
                    value.col = IM_COL32(shade,shade,shade,alpha);
                }
            }
        }
        if (pass == 2) { draw->PopTexture(); }
    }

    // 読みは自然な字形を保つ。追加の斜体・立体化はせず、英字の下の面と投影・動きを共有する。
    const float kanaReveal = Smooth((elapsed - 0.85f) / 0.35f);
    if (kanaReveal > 0.001f) {
        cutPresence = 0.0f;
        separation = LogoSeparation(flyby);
        const int first = draw->VtxBuffer.Size;
        draw->AddText(CombatHud::HeadingFont(), kKanaSize * scale, origin,
            CombatHud::SurfaceColor(IM_COL32(211,220,233,static_cast<int>(255.0f * kanaReveal))), kTitleJapanese);
        for (int index = first; index < draw->VtxBuffer.Size; ++index) {
            auto& vertex = draw->VtxBuffer[index];
            const ImVec2 local{kKanaOrigin.x + (vertex.pos.x - origin.x) / scale,
                kKanaOrigin.y + (vertex.pos.y - origin.y) / scale};
            vertex.pos = position(local, 1, 0.0f);
        }
    }

    // 一閃は文字と同じ投影の切断線に重ねる。固定点列で描き、待機中に粒子を増やさない。
    if (strike.blade > 0.001f || strike.afterglow > 0.02f) {
        constexpr int count = 33;
        std::array<ImVec2, count> points{};
        const float tail = departure > 0.0f ? 1500.0f : 280.0f + strike.afterglow * 520.0f;
        const float head = std::clamp(strike.bladeX, 20.0f, 1260.0f);
        const float direction = departure > 0.0f ? 1.0f : TitleDashDirection(flyby.pattern);
        const float start = std::clamp(head - direction * tail,20.0f,1260.0f);
        for (int index = 0; index < count; ++index) {
            const float x = start + (head - start) * static_cast<float>(index) / (count - 1);
            const auto moved = MoveLogoCanvas({x, TitleCutY(x,flyby.cutPattern)}, motion);
            points[index] = {origin.x + (moved.x - 195.0f) / 1.75f * scale,
                origin.y + (moved.y - 247.0f) / 1.75f * scale};
        }
        const float strength = std::clamp(strike.blade + strike.afterglow * 0.14f, 0.0f, 1.0f);
        const auto bladeLayer = [&](ImU32 ink, float width) {
            const int first = draw->VtxBuffer.Size;
            draw->AddPolyline(points.data(), count, CombatHud::SurfaceColor(ink), 0, width * scale);
            const float delta = points.back().x - points.front().x;
            const float span = std::abs(delta) > 1.0f ? delta : direction;
            for (int index = first; index < draw->VtxBuffer.Size; ++index) {
                auto& vertex = draw->VtxBuffer[index];
                const float travel = (vertex.pos.x - points.front().x) / span;
                const float taper = Smooth(travel / 0.22f) * (1.0f - Smooth((travel - 0.91f) / 0.09f));
                const auto alpha = static_cast<ImU32>(static_cast<float>((vertex.col >> IM_COL32_A_SHIFT) & 255) * taper);
                vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
            }
        };
        bladeLayer(IM_COL32(112,158,231,static_cast<int>(64 * strength)), departure > 0.0f ? 22.0f : 19.0f);
        bladeLayer(IM_COL32(204,226,255,static_cast<int>(160 * strength)), departure > 0.0f ? 7.0f : 6.0f);
        bladeLayer(IM_COL32(255,255,255,static_cast<int>(245 * strength)), 2.0f);
    }
}


}

// タイトル専用の断面と航跡。画像・中間描画面・追加SRVを使わず直接描く。
class TitleRift {
public:
    void Initialize(DirectXCommon* dxCommon)
    {
        dxCommon_ = dxCommon;
        D3D12_ROOT_PARAMETER parameter{};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        parameter.Constants.Num32BitValues = 64;
        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 1;
        rootDesc.pParameters = &parameter;
        Microsoft::WRL::ComPtr<ID3DBlob> signature, error;
        if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
            FAILED(dxCommon_->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(),
                signature->GetBufferSize(), IID_PPV_ARGS(&root_)))) {
            throw std::runtime_error("Title rift root signature");
        }
        const auto vs = dxCommon_->CompileShader(L"shaders/Fullscreen.VS.hlsl", L"vs_6_0");
        const auto ps = dxCommon_->CompileShader(L"shaders/TitleRift.PS.hlsl", L"ps_6_0");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = root_.Get();
        desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
        desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline_)))) {
            throw std::runtime_error("Title rift pipeline");
        }
        auto& blend = desc.BlendState.RenderTarget[0];
        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&wakePipeline_)))) {
            throw std::runtime_error("Title flight wake pipeline");
        }
        // 本編の色補正後の描画先には深度を接続しない。
        desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&departurePipeline_)))) {
            throw std::runtime_error("Title departure overlay pipeline");
        }
        flightDsvHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);
    }

    void PrepareFlightDepth()
    {
        DXGI_SWAP_CHAIN_DESC1 swapDesc{};
        if (FAILED(dxCommon_->GetSwapChain()->GetDesc1(&swapDesc))) {
            throw std::runtime_error("Title flight target size");
        }
        if (flightDepth_ && flightWidth_ == swapDesc.Width && flightHeight_ == swapDesc.Height) { return; }
        // Updateは前フレームのPostDrawによるGPU完了待ち後。リサイズ時だけ作り直す。
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = swapDesc.Width;
        desc.Height = swapDesc.Height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        D3D12_CLEAR_VALUE clear{};
        clear.Format = desc.Format;
        clear.DepthStencil.Depth = 1.0f;
        Microsoft::WRL::ComPtr<ID3D12Resource> depth;
        if (FAILED(dxCommon_->GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&depth)))) {
            throw std::runtime_error("Title flight depth");
        }
        D3D12_DEPTH_STENCIL_VIEW_DESC view{};
        view.Format = desc.Format;
        view.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dxCommon_->GetDevice()->CreateDepthStencilView(depth.Get(),&view,
            flightDsvHeap_->GetCPUDescriptorHandleForHeapStart());
        flightDepth_ = std::move(depth);
        flightWidth_ = swapDesc.Width;
        flightHeight_ = swapDesc.Height;
    }

    void BindFlightTarget(bool begin)
    {
        auto rtv = dxCommon_->GetRTVHeap()->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(dxCommon_->GetSwapChain()->GetCurrentBackBufferIndex()) *
            dxCommon_->GetRTVDescriptorSize();
        auto* command = dxCommon_->GetCommandList();
        const auto dsv = flightDsvHeap_->GetCPUDescriptorHandleForHeapStart();
        command->OMSetRenderTargets(1,&rtv,FALSE,begin ? &dsv : nullptr);
        if (begin) {
            // 本編・背景の深度を持ち込まず、機体自身の前後関係だけを解決する。
            command->ClearDepthStencilView(dsv,D3D12_CLEAR_FLAG_DEPTH,1.0f,0,0,nullptr);
            const auto& viewport = dxCommon_->GetPresentationViewport();
            const D3D12_RECT scissor{0,0,static_cast<LONG>(flightWidth_),static_cast<LONG>(flightHeight_)};
            command->RSSetViewports(1,&viewport);
            command->RSSetScissorRects(1,&scissor);
        }
    }

    void Draw(float elapsed, float departure, const TitleFlyby& flyby,
        const std::array<Math::Vector4, 2>& engines,
        const std::array<Math::Vector4, 12>& wakePoints, bool wake = false, bool reveal = false)
    {
        const auto& viewport = reveal ? dxCommon_->GetPresentationViewport() : dxCommon_->GetViewport();
        std::array<float, 64> parameters{ viewport.Width, viewport.Height, elapsed, departure,
            flyby.lightX, flyby.energy, flyby.phase + static_cast<float>(flyby.pattern) * kFlybyLoop +
                (flyby.cutPattern == 2 ? kFlightSequence : 0.0f),
            wake ? (IsTitleOrbitalWake(flyby.phase,flyby.pattern) ? 2.0f : 1.0f) : 0.0f,
            engines[0].x, engines[0].y, engines[0].z, engines[0].w,
            engines[1].x, engines[1].y, engines[1].z, engines[1].w };
        for (size_t index = 0; index < wakePoints.size(); ++index) {
            const auto& point = wakePoints[index];
            const size_t offset = 16 + index * 4;
            parameters[offset] = point.x; parameters[offset + 1] = point.y;
            parameters[offset + 2] = point.z; parameters[offset + 3] = point.w;
        }
        auto* command = dxCommon_->GetCommandList();
        command->SetGraphicsRootSignature(root_.Get());
        command->SetPipelineState(reveal ? departurePipeline_.Get() : wake ? wakePipeline_.Get() : pipeline_.Get());
        command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command->SetGraphicsRoot32BitConstants(0, static_cast<UINT>(parameters.size()), parameters.data(), 0);
        command->DrawInstanced(3, 1, 0, 0);
    }
private:
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> wakePipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> departurePipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> flightDsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> flightDepth_; // DEPTH_WRITEのまま使う、タイトル専用の深度。
    UINT flightWidth_ = 0;
    UINT flightHeight_ = 0;
};

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize()
{
    elapsed_ = departureTime_ = 0.0f;
    presentationRate_ = 1.0f;
    arrivalElapsed_ = 0.0f;
    flightOpacity_ = 0.0f;
    flybyElapsed_ = 0.0f;
    previewFlybyPhase_ = -1.0f;
    std::array<char, 32> previewRate{};
    const DWORD previewLength = GetEnvironmentVariableA("AZRAID_TITLE_PREVIEW_RATE",
        previewRate.data(), static_cast<DWORD>(previewRate.size()));
    if (previewLength > 0 && previewLength < previewRate.size()) {
        const char* value = previewRate.data();
        char* end = nullptr;
        const float rate = std::strtof(value, &end);
        if (end != value && *end == '\0' && std::isfinite(rate) && rate >= 0.05f && rate <= 1.0f) {
            presentationRate_ = rate;
        }
    }
    std::array<char, 32> previewPhase{};
    const DWORD phaseLength = GetEnvironmentVariableA("AZRAID_TITLE_PREVIEW_PHASE",
        previewPhase.data(), static_cast<DWORD>(previewPhase.size()));
    if (phaseLength > 0 && phaseLength < previewPhase.size()) {
        const char* value = previewPhase.data();
        char* end = nullptr;
        const float phase = std::strtof(value, &end);
        if (end != value && *end == '\0' && std::isfinite(phase) && phase >= 0.0f && phase < kFlightSequence) {
            previewFlybyPhase_ = phase;
        }
    }
    selectedItem_ = 0;
    menuEmphasis_ = { 1.0f, 0.0f, 0.0f };
    showControls_ = startRequested_ = false;
    requestedScene_ = SceneType::Game;
    TextureManager::GetInstance()->LoadTexture(kLogoSurface);
    PrepareTitleComposition();
}

void TitleScene::RequestStart(bool tutorial)
{
    if (startRequested_) { return; }
    requestedScene_ = tutorial ? SceneType::Tutorial : SceneType::Game;
    startRequested_ = true;
    showControls_ = false;
    if (sound_) { sound_->Play("confirm"); }
}

void TitleScene::PrepareBackdrop()
{
    if (objectCommon_ || GameScene::GetResourcePreloadStep() < 8) { return; }
    Model* model = ModelManager::GetInstance()->FindModel(kShip);
    if (!model || model->GetVertices().empty()) { return; }
    // 自機と環境光は本編と共有し、タイトルのカメラ・姿勢は独立させる。
    camera_ = std::make_unique<Camera>();
    camera_->SetFovY(0.56f);
    camera_->SetFarClip(1000.0f);
    objectCommon_ = std::make_unique<Object3dCommon>();
    objectCommon_->Initialize(dxCommon_, srvManager_);
    objectCommon_->SetDefaultCamera(camera_.get());
    objectCommon_->SetEnvironmentTexturePath(kSky);

    Math::Vector3 min{ FLT_MAX, FLT_MAX, FLT_MAX }, max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (const auto& vertex : model->GetVertices()) {
        min.x = (std::min)(min.x, vertex.position.x); max.x = (std::max)(max.x, vertex.position.x);
        min.y = (std::min)(min.y, vertex.position.y); max.y = (std::max)(max.y, vertex.position.y);
        min.z = (std::min)(min.z, vertex.position.z); max.z = (std::max)(max.z, vertex.position.z);
    }
    modelCenter_ = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
    ship_ = std::make_unique<Object3d>();
    ship_->Initialize(objectCommon_.get());
    ship_->SetModel(model);
    ship_->SetLightingMode(1);
    ship_->SetColor({ 0.90f, 0.94f, 1.0f, 1.0f });
    ship_->SetDirectionalLightDirection({ -0.35f, -0.82f, 0.32f });
    // 本編と同じ塗装・材質を残し、カメラと照明でタイトル専用の見せ方を作る。
    ship_->SetDirectionalLightIntensity(1.18f);
    ship_->SetEnvironmentCoefficient(0.045f);
    ship_->SetShininess(112.0f);
    ship_->SetRoughness(0.38f);
    ship_->SetMetallic(0.24f);
    ship_->SetSpecularColor({ 0.30f, 0.32f, 0.35f });
    ship_->SetShadowReceiveStrength(0.0f);

    // タイトル専用の噴流。安定したObject3d描画でノズルへ接続する。
    ModelManager::GetInstance()->CreatePlane("title_engine_plume",1.0f,1.0f,"resources/effects/title_engine_plume.png");
    for (size_t index = 0; index < exhaust_.size(); ++index) {
        auto& effect = exhaust_[index];
        effect = std::make_unique<Object3d>();
        effect->Initialize(objectCommon_.get());
        effect->SetModel(ModelManager::GetInstance()->FindModel(index / 2 == 2 ?
            "effect_player_bullet_core" : "title_engine_plume"));
        effect->SetLightingMode(0);
        effect->SetEnvironmentCoefficient(0.0f);
        effect->SetAlphaReference(0.003f);
    }
    sound_ = std::make_unique<SoundManager>();
    if (sound_->Initialize()) {
        sound_->Load("confirm", "resources/audio/combat/skill_ready.wav", 1, 0.22f);
        sound_->Load("launch", "resources/audio/combat/dodge.wav", 1, 0.48f);
    }
}

void TitleScene::PrepareTitleComposition()
{
    rift_ = std::make_unique<TitleRift>();
    rift_->Initialize(dxCommon_);
}

void TitleScene::UpdateBackdrop()
{
    if (!camera_) { return; }
    if (rift_) { rift_->PrepareFlightDepth(); }
    if (departureTime_ > 0.0f) {
        // 常駐する機体は決定直後に消さず、一閃の発光に合わせて抜く。
        // 本編を描く前に透明になるため、追加の深度・描画パスは必要ない。
        const float fade = 1.0f - Smooth((departureTime_ - 0.20f) / 0.15f);
        ship_->SetColor({0.90f,0.94f,1.0f,flightOpacity_ * fade});
        ship_->Update();
        for (size_t index = 0; index < exhaust_.size(); ++index) {
            exhaust_[index]->SetColor(TitleExhaustColor(static_cast<int>(index / 2),fade));
            exhaust_[index]->Update();
        }
        return;
    }
    camera_->SetAspectRatio(dxCommon_->GetPresentationAspectRatio());
    camera_->SetFovY(0.62f);
    camera_->SetTranslate({ 0, 0, -18 });
    camera_->SetRotate({ 0.02f, 0.0f, -0.10f });
    camera_->Update();
    const auto inView = [&](Math::Vector3 local) {
        const auto offset = RotateVector(local, camera_->GetWorldMatrix());
        const auto& position = camera_->GetTranslate();
        return Math::Vector3{ position.x + offset.x, position.y + offset.y, position.z + offset.z };
    };
    if (departureTime_ == 0.0f) {
        const TitleFlyby flyby = GetTitleFlyby(flybyElapsed_, !showControls_);
        const bool arriving = arrivalElapsed_ < kArrivalDuration;
        const float arrivalFade = Smooth(arrivalElapsed_ / 0.18f);
        const float fit = (std::min)(camera_->GetAspectRatio() / (1280.0f / 720.0f), 1.0f);
        const auto local = arriving ? MoveFlightWithLogo(GetTitleArrivalLocalPosition(arrivalElapsed_),elapsed_,0.0f) :
            GetTitleFlightPosition(flyby.phase,elapsed_,flyby.pattern);
        const auto center = inView({local.x * fit,local.y * fit,local.z});
        const auto direction = RotateVector(arriving ? TitleArrivalDirection(arrivalElapsed_) :
            TitleFlightDirection(flyby.phase,flyby.pattern),camera_->GetWorldMatrix());
        const auto rotation = FlightRotation(direction,RotateVector({0,1,0},camera_->GetWorldMatrix()),
            TitleFlightBank(flyby.phase,flyby.pattern));
        const float shipScale = TitleFlightScale(flyby.phase) * fit;
        const auto rotationMatrix = Math::MakeAffineMatrix({ 1,1,1 }, rotation, {});
        const auto matrix = Math::MakeAffineMatrix({shipScale,shipScale,shipScale},rotation,{});
        const auto offset = RotateVector(modelCenter_, matrix);
        ship_->SetScale({shipScale,shipScale,shipScale});
        ship_->SetRotate(rotation);
        ship_->SetTranslate({ center.x - offset.x, center.y - offset.y, center.z - offset.z });
        flightOpacity_ = arrivalFade * (1.0f - Smooth((local.z - 140.0f) / 220.0f));
        ship_->SetColor({0.90f,0.94f,1.0f,flightOpacity_});
        ship_->Update();
        const auto strike = GetTitleStrike(flyby);
        const float wakeStrength = arriving ? 0.0f : TitleFlightWakeStrength(flyby.phase,flyby.pattern);
        UpdateFlightEffects(center,rotationMatrix,1.4f + strike.charge * 1.2f + wakeStrength * 2.2f,
            shipScale,arrivalFade);
        if (wakeStrength <= 0.001f) { wakePoints_.fill({}); return; }
        // 過去の実飛行位置・ノズル姿勢を再評価し、通った道だけに航跡を残す。
        // 遠方では左右が一点に収まるため、同じ固定領域を1本12点の滑らかな周回へ使う。
        const bool orbitalWake = IsTitleOrbitalWake(flyby.phase,flyby.pattern);
        const size_t nozzleCount = orbitalWake ? 1 : 2;
        const size_t pointCount = orbitalWake ? wakePoints_.size() : 6;
        for (size_t nozzle = 0; nozzle < nozzleCount; ++nozzle) {
            for (size_t sample = 0; sample < pointCount; ++sample) {
                const float t = static_cast<float>(sample) / static_cast<float>(pointCount - 1);
                const float age = t * (orbitalWake ? 0.36f : 0.26f);
                const auto past = GetTitleFlyby(std::fmod(flybyElapsed_ - age + kFlightSequence,kFlightSequence),true);
                const float previousPhase = past.phase;
                const auto pastLocal = GetTitleFlightPosition(previousPhase,elapsed_ - age,past.pattern);
                const auto pastCenter = inView({pastLocal.x * fit,pastLocal.y * fit,pastLocal.z});
                const float pastScale = TitleFlightScale(previousPhase) * fit;
                const auto pastRotation = FlightRotation(RotateVector(TitleFlightDirection(previousPhase,past.pattern),
                    camera_->GetWorldMatrix()),RotateVector({0,1,0},camera_->GetWorldMatrix()),
                    TitleFlightBank(previousPhase,past.pattern));
                const auto pastMatrix = Math::MakeAffineMatrix({1,1,1},pastRotation,{});
                const auto nozzleOffset = RotateVector({(orbitalWake ? 0.0f : nozzle == 0 ? -0.48f : 0.48f) * pastScale / 1.26f,
                    -0.28f * pastScale / 1.26f,-1.70f * pastScale / 1.26f},pastMatrix);
                const auto point = ProjectTitleCanvas({pastCenter.x + nozzleOffset.x,
                    pastCenter.y + nozzleOffset.y,pastCenter.z + nozzleOffset.z},*camera_);
                const auto& engine = engines_[nozzle];
                const float headX = orbitalWake ? (engines_[0].x + engines_[1].x) * 0.5f : engine.x;
                const float headY = orbitalWake ? (engines_[0].y + engines_[1].y) * 0.5f : engine.y;
                const float width = orbitalWake ? 0.9f + std::sin(t * 3.1415927f) * 1.1f :
                    1.4f + std::sin(t * 3.1415927f) * (2.0f + strike.blade * 2.4f);
                wakePoints_[nozzle * 6 + sample] = {sample == 0 ? headX : point.x,
                    sample == 0 ? headY : point.y,width,
                    (0.90f - t * 0.36f) * (1.0f - Smooth((t - 0.72f) / 0.28f)) * wakeStrength};
            }
        }
        return;
    }
}

void TitleScene::UpdateFlightEffects(const Math::Vector3& center, const Math::Matrix4x4& rotationMatrix,
    float thrust, float shipScale, float fade)
{
    const auto tail = RotateVector({ 0, 0, -1 }, rotationMatrix);
    const auto tailView = RotateVector(tail, camera_->GetViewMatrix());
    Math::Vector3 billboard = camera_->GetRotate();
    billboard.z += std::atan2(-tailView.x, tailView.y);
    const float pulse = 1.0f + 0.045f * std::sin(elapsed_ * 29.0f) + 0.025f * std::sin(elapsed_ * 43.0f);
    for (size_t index = 0; index < exhaust_.size(); ++index) {
        const int layer = static_cast<int>(index / 2);
        // 本編で確認済みのノズル位置をタイトルの表示倍率へ換算する。
        const float ratio = shipScale / 1.26f;
        const auto nozzle = RotateVector({ (index % 2 == 0 ? -0.48f : 0.48f) * ratio,
            -0.28f * ratio, -1.70f * ratio }, rotationMatrix);
        if (index < engines_.size()) {
            const Math::Vector3 position{ center.x + nozzle.x, center.y + nozzle.y, center.z + nozzle.z };
            const auto head = ProjectTitleCanvas(position, *camera_);
            const auto end = ProjectTitleCanvas({ position.x + tail.x * 8.0f,
                position.y + tail.y * 8.0f, position.z + tail.z * 8.0f }, *camera_);
            const float length = (std::max)(0.001f, std::hypot(end.x - head.x, end.y - head.y));
            engines_[index] = { head.x, head.y, (end.x - head.x) / length, (end.y - head.y) / length };
        }
        const float length = (layer == 0 ? 1.75f : 1.48f) * (0.70f + thrust * 0.45f) * pulse * shipScale / kIdleShipScale;
        const float centerOffset = layer == 2 ? 0.025f : length * 0.48f;
        auto& effect = exhaust_[index];
        effect->SetTranslate({ center.x + nozzle.x + tail.x * centerOffset,
            center.y + nozzle.y + tail.y * centerOffset, center.z + nozzle.z + tail.z * centerOffset });
        effect->SetRotate(billboard);
        if (layer == 2) {
            effect->SetScale({ 0.34f * ratio * pulse, 0.34f * ratio * pulse, 1.0f });
        } else {
            effect->SetScale({ (layer == 0 ? 0.55f : 0.27f) * ratio * pulse * (0.65f + thrust * 0.10f), length, 1.0f });
        }
        effect->SetColor(TitleExhaustColor(layer,fade));
        effect->Update();
    }
}

void TitleScene::Update()
{
    const float delta = (dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 0.05f) :
        1.0f / 60.0f) * presentationRate_;
    elapsed_ += delta;
    if (input_ && !startRequested_) {
        if (showControls_) {
            if (MenuUi::Pressed(input_, DIK_ESCAPE) || MenuUi::Pressed(input_, DIK_H) ||
                MenuUi::Pressed(input_, DIK_RETURN)) { showControls_ = false; }
        } else {
            if (MenuUi::Pressed(input_, DIK_UP) || MenuUi::Pressed(input_, DIK_W) || MenuUi::Pressed(input_, DIK_LEFT) || MenuUi::Pressed(input_, DIK_A)) { selectedItem_ = (selectedItem_ + 2) % 3; }
            if (MenuUi::Pressed(input_, DIK_DOWN) || MenuUi::Pressed(input_, DIK_S) || MenuUi::Pressed(input_, DIK_RIGHT) || MenuUi::Pressed(input_, DIK_D)) { selectedItem_ = (selectedItem_ + 1) % 3; }
            if (MenuUi::Pressed(input_, DIK_RETURN)) {
                if (selectedItem_ == 2) { showControls_ = true; }
                else { RequestStart(selectedItem_ == 1); }
            } else if (input_->TriggerKey(DIK_T)) { RequestStart(true); }
            else if (input_->TriggerKey(DIK_H)) { showControls_ = true; }
        }
    }
    // 導入後にモデルを準備。登場の時計は準備後から進め、読み込み時間で姿を飛ばさない。
    const bool resourcesReady = elapsed_ >= 2.10f && GameScene::PreloadResourcesStep(dxCommon_, srvManager_);
    PrepareBackdrop();
    if (resourcesReady && sceneManager_ && !sceneManager_->IsScenePrepared(requestedScene_)) {
        sceneManager_->PrepareScene(requestedScene_);
    }
    const bool gameReady = sceneManager_ && sceneManager_->IsScenePrepared(requestedScene_);
    if (ship_ && !startRequested_ && !showControls_) {
        arrivalElapsed_ = (std::min)(kArrivalDuration,arrivalElapsed_ + delta);
        if (gameReady && arrivalElapsed_ >= kArrivalDuration) {
            flybyElapsed_ = previewFlybyPhase_ >= 0.0f ? previewFlybyPhase_ :
                flybyElapsed_ + delta;
        }
    }
    if (startRequested_ && gameReady && sceneManager_) {
        if (departureTime_ == 0.0f && sound_) { sound_->Play("launch"); }
        departureTime_ = (std::min)(kDepartureDuration, departureTime_ + delta);
        if (departureTime_ >= kDepartureDuration) { sceneManager_->SetNextScene(requestedScene_); }
    }
    UpdateBackdrop();
    DrawMenu(gameReady);
}

void TitleScene::DrawMenu(bool gameReady)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = (std::max)(0.1f, (std::min)(viewport->Size.x / 1280.0f, viewport->Size.y / 720.0f));
    const ImVec2 origin(viewport->Pos.x + (viewport->Size.x - 1280.0f * scale) * 0.5f,
        viewport->Pos.y + (viewport->Size.y - 720.0f * scale) * 0.5f);
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    const ImVec2 end(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##Title", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (!rift_) { draw->AddRectFilled(viewport->Pos, end, CombatHud::SurfaceColor(IM_COL32(9,17,31,255))); }

    if (!showControls_) {
        const float exit = 1.0f - Smooth(departureTime_ / 0.20f);
        const auto surface = srvManager_->GetGPUDescriptorHandle(TextureManager::GetInstance()->GetSrvIndex(kLogoSurface));
        DrawWordmark(draw, p(195, 247), 1.75f * scale, elapsed_,
            GetTitleFlyby(flybyElapsed_, ship_ != nullptr), departureTime_,static_cast<ImTextureID>(surface.ptr));
        const float menuAlpha = Smooth((elapsed_ - 1.35f) / 0.45f) * exit;
        const int firstMenuVertex = draw->VtxBuffer.Size;
        constexpr std::array<float, 3> positions{ 277.0f, 541.0f, 805.0f };
        const char* labels[] = { "出撃", "チュートリアル", "操作方法" };
        const float blend = 1.0f - std::exp(-12.0f * std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f));
        for (int index = 0; index < 3; ++index) {
            const float x = positions[index], y = 608.0f;
            ImGui::SetCursorScreenPos(p(x, y));
            ImGui::BeginDisabled(startRequested_ || menuAlpha < 0.05f);
            const bool clicked = ImGui::InvisibleButton(labels[index], { 236.0f * scale, 48.0f * scale });
            if (ImGui::IsItemHovered() && ImGui::IsMousePosValid() &&
                (ImGui::GetIO().MouseDelta.x != 0.0f || ImGui::GetIO().MouseDelta.y != 0.0f)) { selectedItem_ = index; }
            ImGui::EndDisabled();
            if (clicked) {
                selectedItem_ = index;
                if (index == 2) { showControls_ = true; }
                else { RequestStart(index == 1); }
            }
            const float target = selectedItem_ == index ? 1.0f : 0.0f;
            menuEmphasis_[index] += (target - menuEmphasis_[index]) * blend;
            const float emphasis = menuEmphasis_[index];
            const std::array<ImVec2, 4> button{ p(x + 6,y), p(x + 236,y), p(x + 230,y + 48), p(x,y + 48) };
            draw->AddConvexPolyFilled(button.data(), 4,
                CombatHud::SurfaceColor(IM_COL32(244,245,247,static_cast<int>(245.0f * emphasis))));
            const float fontSize = 22.0f * scale;
            const float textX = p(x + 118, y + 11).x - MenuUi::Width(labels[index], fontSize, true) * 0.5f;
            MenuUi::Heading(draw, { textX, p(x,y + 11).y }, fontSize,
                CombatHud::Mix(IM_COL32(184,200,219,255), MenuUi::Ink, emphasis), labels[index]);
        }
        if (!gameReady) {
            MenuUi::Text(draw, p(1195,680), 16.0f * scale, IM_COL32(113,133,159,255), "読み込み中", true);
        }
        for (int index = firstMenuVertex; index < draw->VtxBuffer.Size; ++index) {
            ImU32& color = draw->VtxBuffer[index].col;
            const auto alpha = static_cast<ImU32>(static_cast<float>((color >> IM_COL32_A_SHIFT) & 255) * menuAlpha);
            color = (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    if (showControls_ && MenuUi::Controls(viewport->Pos, viewport->Size)) { showControls_ = false; }
}

void TitleScene::Draw()
{
    GameScene* preview = GetDeparturePreview();
    // 初期化済みの同じシーンを描き、切替時にも再生成しない。Updateは呼ばない。
    if (preview) { preview->Draw(); return; }
    TitleFlyby flyby = GetTitleFlyby(flybyElapsed_, ship_ && !showControls_);
    flyby.energy *= Smooth(arrivalElapsed_ / 0.18f) * (1.0f - Smooth(departureTime_ / 0.20f));
    if (rift_) { rift_->Draw(elapsed_, departureTime_, flyby, engines_, wakePoints_); }
    const bool idleFlight = departureTime_ <= kDepartureOpenTime && !showControls_;
    if (ship_ && idleFlight && !IsForegroundFlight()) {
        objectCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
        objectCommon_->CommonDrawSetting();
        ship_->Draw();
        objectCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
        objectCommon_->CommonDrawSetting();
        objectCommon_->SetBlendMode(BlendMode::Add);
        objectCommon_->CommonDrawSetting();
        for (auto& effect : exhaust_) { effect->Draw(); }
        objectCommon_->SetBlendMode(BlendMode::Normal);
        objectCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
    }
    if (rift_ && flyby.energy > 0.001f) { rift_->Draw(elapsed_, departureTime_, flyby, engines_, wakePoints_, true); }
}

bool TitleScene::IsForegroundFlight() const
{
    const auto flyby = GetTitleFlyby(flybyElapsed_,true);
    const bool passing = flyby.pattern == 2 && flyby.phase >= kClosePassStart && flyby.phase < 1.60f;
    const bool cutting = flyby.phase >= kChargeStart && flyby.phase < kFlybyStart + kFlybyDuration + 0.18f;
    return ship_ && arrivalElapsed_ >= kArrivalDuration && departureTime_ <= kDepartureOpenTime && !showControls_ &&
        (passing || cutting);
}

void TitleScene::DrawFlightOverlay()
{
    if (!rift_ || !IsForegroundFlight()) { return; }
    rift_->BindFlightTarget(true);
    srvManager_->PreDraw();
    objectCommon_->SetBlendMode(BlendMode::Normal);
    objectCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
    objectCommon_->CommonDrawSetting();
    ship_->Draw();
    objectCommon_->SetBlendMode(BlendMode::Add);
    objectCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    objectCommon_->CommonDrawSetting();
    for (auto& effect : exhaust_) { effect->Draw(); }
    objectCommon_->SetBlendMode(BlendMode::Normal);
    objectCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
    rift_->BindFlightTarget(false);
}

GameScene* TitleScene::GetDeparturePreview() const
{
    if (departureTime_ <= kDepartureOpenTime || !sceneManager_) { return nullptr; }
    return dynamic_cast<GameScene*>(sceneManager_->GetPreparedScene(requestedScene_));
}

void TitleScene::DrawDepartureOverlay()
{
    if (!rift_ || !GetDeparturePreview()) { return; }
    TitleFlyby flyby = GetTitleFlyby(flybyElapsed_, ship_ != nullptr);
    flyby.energy = 0.0f;
    // 本編だけに色補正・輪郭を適用し、タイトルの面へ奥の深度を写し込まない。
    rift_->Draw(elapsed_, departureTime_, flyby, engines_, wakePoints_, false, true);
}

void TitleScene::Finalize()
{
    // フレーム終端のGPU完了待ち後にシーンが切り替わる。共有モデル・空は解放しない。
    ship_.reset();
    rift_.reset();
    for (auto& effect : exhaust_) { effect.reset(); }
    sound_.reset();
    if (objectCommon_ && srvManager_) { srvManager_->Free(objectCommon_->GetShadowMapSrvIndex()); }
    objectCommon_.reset();
    camera_.reset();
}
