// ===== TitleMotion.cpp =====
// タイトルの自機の飛行軌道と、ロゴや背景を切り裂く位置・タイミングを計算する。

#include "engine/scene/TitleMotion.h"

#include "engine/3d/Camera.h"

#include <algorithm>
#include <cmath>

namespace TitleMotion {
namespace {
constexpr float kDepartureBiteTime = 0.29f;
constexpr float kDepartureBreakTime = 0.49f;
constexpr float kDepartureCutHold = 0.16f;
constexpr float kApproachStart = 1.70f;
constexpr float kOrbitStart = 2.10f;
constexpr float kReturnTurnTime = kFlybyStart + kFlybyDuration + 0.36f;
constexpr float kReturnSettleTime = kReturnTurnTime + 1.12f;
constexpr float kReturnEnd = kReturnSettleTime + 1.00f;
constexpr float kClosePassTime = 1.05f;
constexpr float kCloseExitTime = 1.35f;
constexpr float kDashStartX = 17.0f;
constexpr float kDashTravel = 1415.0f;
constexpr float kTitleStrikeX = 175.0f;
} // namespace

float Smooth(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// 待機中の周回と切断時刻から、開口部の閉じ方と次に開くタイミングを求める。
StagePortalIdleState StagePortalIdle(float elapsed)
{
    constexpr auto cuts=StageEntryTiming::kStagePatrolCutTimes;
    constexpr float duration=StageEntryTiming::kStagePatrolDuration;
    const float time=(std::max)(0.0f,elapsed), phase=std::fmod(time,duration);
    const float previous=phase < cuts[0] ? cuts[1]-duration : (phase < cuts[1] ? cuts[0] : cuts[1]);
    const float next=phase < cuts[0] ? cuts[0] : (phase < cuts[1] ? cuts[1] : cuts[0]+duration);
    const float age=phase-previous, interval=next-previous;
    // 最初はタイトルで開いた幅を維持。以後は切断パスの機首が通ってから開く。
    constexpr float handoff=kDepartureDuration-kDepartureOpenTime;
    if (time < cuts[0]) { return {12.0f+148.0f*(1.0f-Smooth((time-handoff)/(cuts[0]-0.20f-handoff))),100.0f}; }
    const float aperture=12.0f+148.0f*Smooth(age/0.45f)*(1.0f-Smooth((age-0.70f)/(interval-0.90f)));
    return {aperture,age};
}

float TitleDashDirection(int pattern) { return pattern == 1 ? -1.0f : 1.0f; }
static float TitleDashOrigin(int pattern) { return pattern == 1 ? 1280.0f - kDashStartX : kDashStartX; }
static float TitleContactTime(int pattern, float canvasX)
{
    return kFlybyStart + ((canvasX - TitleDashOrigin(pattern)) * TitleDashDirection(pattern) - kCutLead) /
        kDashTravel * kFlybyDuration;
}
static float TitleStrikeTime(int pattern) { return TitleContactTime(pattern,pattern == 1 ? 1110.0f : kTitleStrikeX); }
static float TitleCutCompleteTime(int pattern) { return TitleContactTime(pattern,pattern == 1 ? kTitleStrikeX : 1110.0f); }

TitleCut GetTitleCut(int pattern) { return pattern == 2 ? TitleCut{12.0f,0.15f} : TitleCut{80.0f,-0.11f}; }
static ImVec2 LogoSeparationAxis(int pattern)
{
    return pattern == 2 ? ImVec2{-0.08f,-0.70f} : ImVec2{pattern == 1 ? -0.62f : 0.55f,-0.38f};
}

// 待機中の飛行パターンと斬撃時刻から、刃の位置や衝撃の強さを求める。
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

float DepartureResistance(float departure)
{
    return Smooth((departure - 0.27f) / 0.025f) *
        (1.0f - Smooth((departure - kDepartureBreakTime) / 0.025f));
}

// 出撃時の斬撃位置を求める。裂け目に食い込む区間と抜ける区間で進み方を変える。
static float DepartureBladeX(float departure)
{
    // 接触までは速く、AZRへ食い込む200msは抵抗し、残りを一気に断ち切る。
    if (departure < kDepartureBiteTime) {
        return -120.0f + 450.0f * Smooth((departure - kDepartureCutTime) /
            (kDepartureBiteTime - kDepartureCutTime));
    }
    if (departure < kDepartureBreakTime) {
        return 330.0f + 320.0f * Smooth((departure - kDepartureBiteTime) /
            (kDepartureBreakTime - kDepartureBiteTime));
    }
    return 650.0f + 750.0f * Smooth((departure - kDepartureBreakTime) /
        (kDepartureOpenTime - kDepartureBreakTime));
}

float DepartureFade(float departure)
{
    // 食い込みを覆わず、断ち切った衝撃だけ白へ。戻ってから赤熱を見せて開く。
    return Smooth((departure - 0.56f) / 0.038f) *
        (1.0f - Smooth((departure - 0.62f) / 0.070f));
}

// 出撃時刻から斬撃の位置・溜め・衝撃の値を作り、飛行と画面演出で共有する。
TitleStrike GetDepartureStrike(float departure)
{
    const float time = departure - kDepartureCutTime;
    TitleStrike strike;
    strike.charge = Smooth(departure / kDepartureCutTime) * (1.0f - Smooth(time / 0.035f)) +
        DepartureResistance(departure) * 0.65f;
    strike.bladeX = DepartureBladeX(departure);
    if (time >= 0.0f) {
        strike.blade = Smooth(time / 0.014f) *
            (1.0f - Smooth((departure - kDepartureOpenTime - 0.015f) / 0.070f));
    }
    const float impactTime = departure - kDepartureImpactTime;
    if (impactTime >= 0.0f) {
        strike.flash = Smooth(impactTime / 0.014f) * std::exp(-impactTime / 0.14f);
        strike.afterglow = Smooth(impactTime / 0.02f) * std::exp(-impactTime / 0.24f);
    }
    return strike;
}

// 刃の補間を逆算し、画面上の指定した横位置に斬撃が届く時刻を求める。
float DepartureContactTime(float canvasX)
{
    // 自機と同じ3段階の軌道を逆算。火花・加熱だけ先走らせない。
    const auto inverse = [](float progress) {
        progress = std::clamp(progress,0.0f,1.0f);
        return 0.5f - std::sin(std::asin(1.0f - 2.0f * progress) / 3.0f);
    };
    if (canvasX < 330.0f) {
        return kDepartureCutTime + (kDepartureBiteTime - kDepartureCutTime) * inverse((canvasX + 120.0f) / 450.0f);
    }
    if (canvasX < 650.0f) {
        return kDepartureBiteTime + (kDepartureBreakTime - kDepartureBiteTime) * inverse((canvasX - 330.0f) / 320.0f);
    }
    return kDepartureBreakTime + (kDepartureOpenTime - kDepartureBreakTime) * inverse((canvasX - 650.0f) / 750.0f);
}

float DepartureImpact(float departure)
{
    const float time = departure - kDepartureImpactTime;
    return time >= 0.0f ? Smooth(time / 0.012f) * std::exp(-time / 0.080f) : 0.0f;
}

// 切断後の経過時間から、左右の断片がずれて離れていく距離を求める。
ImVec2 DepartureOffset(float departure, int half)
{
    if (half < 0) { return {}; }
    // 断ち切った面を160ms受け止め、赤熱が読めてから重い上下の面を放つ。
    const float time = (std::max)(0.0f, departure - kDepartureOpenTime - kDepartureCutHold);
    const float travel = 740.0f * std::pow(std::clamp(time / 0.90f, 0.0f, 1.0f), 1.40f);
    const float recoil = Smooth(time / 0.025f) * std::exp(-time / 0.12f) * 26.0f;
    const float release = Smooth((departure - kDepartureOpenTime) / 0.045f) * 5.0f;
    const float side = half == 0 ? 1.0f : -1.0f;
    return {side * (travel + recoil + release) * 0.30f, -side * (travel + recoil + release)};
}

// 文字に機首が届いてから反応する。飛行と切断は同じ位置・速度を使う。
// TitleRift.PS.hlslも同じ時計・係数で背景の断面を動かす。
static float LogoImpulse(const TitleFlyby& flyby)
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

// 待機中の斬撃が終わった後、ロゴが一時的に上下へ離れる量を求める。
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

static float LogoZoom(float elapsed, const TitleFlyby& flyby)
{
    return 1.0f + Smooth((elapsed - 0.35f) / 1.20f) *
        (std::sin(elapsed * 0.58f) * 0.012f + GetTitleStrike(flyby).charge * 0.016f + LogoImpulse(flyby) * 0.026f);
}

// 各頂点で三角関数・指数を再計算せず、ロゴ全体で同じ姿勢を共有する。
LogoMotion GetLogoMotion(float elapsed, const TitleFlyby& flyby, float departure)
{
    const float ready = Smooth((elapsed - 0.35f) / 1.20f);
    float angle = std::sin(elapsed * 0.54f) * 0.020f * ready;
    const float time = (std::max)(0.0f, flyby.phase - TitleStrikeTime(flyby.pattern));
    const float shock = std::sin(time * 42.0f) * std::exp(-time / 0.16f) * 7.0f;
    float zoom = LogoZoom(elapsed, flyby);
    ImVec2 drift{std::sin(elapsed * 0.43f) * 12.0f * ready + shock,
        std::sin(elapsed * 0.67f) * 6.0f * ready - LogoImpulse(flyby) * 10.0f + shock * 0.22f};
    if (departure > 0.0f) {
        const float impact = DepartureImpact(departure);
        const float impactTime = (std::max)(0.0f,departure - kDepartureImpactTime);
        const float kick = std::sin(impactTime * 65.0f) * std::exp(-impactTime / 0.10f) * 16.0f;
        const float resistance = DepartureResistance(departure);
        const float strain = std::sin(departure * 130.0f) * resistance;
        zoom *= 1.0f - GetDepartureStrike(departure).charge * 0.035f + impact * 0.085f;
        angle += impact * 0.025f;
        drift.x += kick + resistance * 3.5f + strain;
        drift.y -= impact * 9.0f + kick * 0.18f + strain * 0.18f;
    }
    return {std::cos(angle),std::sin(angle),zoom,drift};
}

// ロゴの中心を基準に拡縮・回転・移動を適用する。逆変換では元の座標へ戻す。
ImVec2 MoveLogoCanvas(ImVec2 point, const LogoMotion& motion, bool inverse)
{
    const float x = point.x - 641.25f - (inverse ? motion.drift.x : 0.0f);
    const float y = point.y - 334.5f - (inverse ? motion.drift.y : 0.0f);
    const float sine = inverse ? -motion.sine : motion.sine;
    const float zoom = inverse ? 1.0f / motion.zoom : motion.zoom;
    return {641.25f + (x * motion.cosine - y * sine) * zoom + (inverse ? 0.0f : motion.drift.x),
        334.5f + (x * sine + y * motion.cosine) * zoom + (inverse ? 0.0f : motion.drift.y)};
}

// ロゴ内の切断線をタイトル画面の座標へ変換し、指定した横位置での高さを求める。
float TitleCutY(float screenX, int pattern)
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

static ImVec2 TitleCruisePosition(float phase)
{
    const float cycle = phase * 6.2831853f / kFlybyLoop;
    return {916.0f + std::sin(cycle) * 18.0f,143.0f + std::sin(cycle + 0.80f) * 10.0f};
}

// 巡航時の機首方向を求める。急接近の前後は、ゆっくりした揺れを弱める。
static Math::Vector3 TitleCruiseDirection(float phase)
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

// 機体が旋回する軌道の位置を求める。接近パターンによって旋回の縦横幅を変える。
static Math::Vector3 TitleOrbitPosition(float angle, int pattern = 0)
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

// 巡航軌道上の前後2点から、カメラ空間での速度を近似する。
static Math::Vector3 TitleCruiseVelocity(float phase)
{
    const auto before = TitleViewPoint(TitleCruisePosition(phase - 0.004f),22.0f);
    const auto after = TitleViewPoint(TitleCruisePosition(phase + 0.004f),22.0f);
    return {(after.x - before.x) / 0.008f,(after.y - before.y) / 0.008f,0.0f};
}

// 巡航軌道に戻る機体が少し行き過ぎてから定位置へ収まるよう、位置の補正を求める。
static Math::Vector3 TitleReturnSettleOffset(float time)
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

// 時刻を巡航・接近・周回・離脱の区間に分け、ロゴを基準にした自機の位置を求める。
static Math::Vector3 GetTitleFlightLocalPosition(float phase, int pattern = 0)
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

// 機体をいったん画面座標へ移し、ロゴと同じ揺れを加えて元の奥行きへ戻す。
Math::Vector3 MoveFlightWithLogo(Math::Vector3 local, float elapsed, float phase, int pattern)
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

// 登場軌道の前後2点から進行方向を求め、登場時の機首の向きに使う。
Math::Vector3 TitleArrivalDirection(float time)
{
    const auto before = GetTitleArrivalLocalPosition(time - 0.004f);
    const auto after = GetTitleArrivalLocalPosition(time + 0.004f);
    const auto flight = Math::Normalize({after.x - before.x,after.y - before.y,after.z - before.z});
    const float settle = Smooth((time - kArrivalDuration + 0.50f) / 0.50f);
    return {flight.x + (0.90f - flight.x) * settle,flight.y + (0.22f - flight.y) * settle,
        flight.z + (0.38f - flight.z) * settle};
}

// 急接近や斬撃に合わせて、機体後方に出す航跡の強さを変える。
float TitleFlightWakeStrength(float phase, int pattern)
{
    const bool closePass = pattern == 2;
    const float launch = closePass ? kClosePassStart : kApproachStart;
    const float orbit = Smooth((phase - launch) / (closePass ? 0.22f : 0.09f)) *
        (1.0f - Smooth((phase - kChargeStart) / 0.08f));
    return (std::max)(orbit * 0.80f,GetTitleStrike(TitleFlyby{phase}).blade);
}

bool IsTitleOrbitalWake(float phase, int pattern)
{
    return phase >= (pattern == 2 ? 1.60f : kApproachStart) && phase < kChargeStart + 0.08f;
}

float TitleFlightScale(float phase)
{
    const float attack = Smooth((phase - kApproachStart) / (kChargeStart - kApproachStart)) *
        (1.0f - Smooth((phase - kFlybyStart - kFlybyDuration) / (kReturnSettleTime - kFlybyStart - kFlybyDuration)));
    return kIdleShipScale + (1.50f - kIdleShipScale) * attack;
}

// 巡航の揺れ、急接近時のロール、斬撃時の傾きから機体の横傾きを求める。
float TitleFlightBank(float phase, int pattern)
{
    const float cruise = -0.70f + std::sin(phase * 6.2831853f / kFlybyLoop + 0.35f) * 0.24f;
    const float roll = pattern == 2 ? 6.2831853f * Smooth((phase - kClosePassStart) / 0.95f) : 0.0f;
    const float cutting = Smooth((phase - kChargeStart + 0.18f) / 0.18f) *
        (1.0f - Smooth((phase - kReturnTurnTime) / (kReturnEnd - kReturnTurnTime)));
    return cruise + (-0.06f - cruise) * cutting + roll;
}

// 接近・斬撃・離脱・巡航の各区間に合わせ、機首の向きをつなぐ。
Math::Vector3 TitleFlightDirection(float phase, int pattern)
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

Math::Vector3 GetTitleFlightPosition(float phase, float elapsed, int pattern)
{
    return MoveFlightWithLogo(GetTitleFlightLocalPosition(phase,pattern),elapsed,phase,pattern);
}

// ループ内の経過時間から、待機飛行の位相と今回使う斬撃パターンを選ぶ。
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

// 切断線に沿った分離と断片の奥行きを加えて、ロゴ上の点を画面へ投影する。
ImVec2 ProjectLogoCanvas(ImVec2 local, float separation, int half, float depth, int pattern)
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

// 排気エフェクトの層ごとに色を選び、フェードに合わせて透明度を変える。
Math::Vector4 TitleExhaustColor(int layer, float fade)
{
    if (layer == 2) { return {0.86f,0.94f,1.0f,0.94f * fade}; }
    return layer == 0 ? Math::Vector4{0.16f,0.37f,0.95f,0.68f * fade} :
        Math::Vector4{0.79f,0.91f,1.0f,0.96f * fade};
}

Math::Vector3 TitleNozzleOffset(size_t nozzle, float shipScale, const Math::Matrix4x4& rotation)
{
    const float ratio=shipScale/1.26f;
    return RotateVector({(nozzle == 0 ? -0.48f : 0.48f)*ratio,-0.28f*ratio,-1.70f*ratio},rotation);
}

// 飛行方向と上方向から機体の姿勢を作り、旋回中の傾きを重ねる。
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

// ワールド上の位置を投影し、タイトルの基準画面で使う2D座標へ変換する。
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
} // namespace TitleMotion
