#include "app/stage/StageEntryPresentation.h"

#include "app/ui/CombatHud.h"
#include "app/runtime/GameRuntimeMath.h"
#include "app/runtime/GameRuntimeTuning.h"
#include "app/ui/MenuUi.h"
#include "app/stage/StageEntryTiming.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

namespace {
using namespace GameRuntimeMath;
using namespace GameRuntimeTuning;
using namespace StageEntryTiming;

constexpr Math::Vector3 kStagePreviewCamera{-82.0f,140.0f,-84.0f};

struct StageFlightKey {
    float time;
    Math::Vector3 position, velocity;
};

// 直前と次の位置を補間し、飛行経路の途中で滑らかにつながる機体位置を求める。
Math::Vector3 StageFlightSpline(float elapsed, std::span<const StageFlightKey> keys)
{
    if (elapsed <= keys.front().time) { return keys.front().position; }
    if (elapsed >= keys.back().time) { return keys.back().position; }
    size_t index=0;
    while (index+2 < keys.size() && elapsed > keys[index+1].time) { ++index; }
    const auto& a=keys[index]; const auto& b=keys[index+1];
    const float duration=b.time-a.time, t=(elapsed-a.time)/duration, t2=t*t, t3=t2*t;
    const float p=2.0f*t3-3.0f*t2+1.0f, q=t3-2.0f*t2+t;
    const float r=-2.0f*t3+3.0f*t2, s=t3-t2;
    return {p*a.position.x+q*duration*a.velocity.x+r*b.position.x+s*duration*b.velocity.x,
        p*a.position.y+q*duration*a.velocity.y+r*b.position.y+s*duration*b.velocity.y,
        p*a.position.z+q*duration*a.velocity.z+r*b.position.z+s*duration*b.velocity.z};
}

struct StageFlightSample {
    Math::Vector3 position, velocity;
};

// 戻りの飛行で使う位置の曲線を、区間内の時間から計算する。
StageFlightSample StageReturnCurve(const std::array<Math::Vector3,4>& points, float t)
{
    const float s=1.0f-t;
    const auto component=[&](float a,float b,float c,float d) {
        return std::pair{s*s*s*a+3.0f*s*s*t*b+3.0f*s*t*t*c+t*t*t*d,
            3.0f*(s*s*(b-a)+2.0f*s*t*(c-b)+t*t*(d-c))};
    };
    const auto x=component(points[0].x,points[1].x,points[2].x,points[3].x);
    const auto y=component(points[0].y,points[1].y,points[2].y,points[3].y);
    const auto z=component(points[0].z,points[1].z,points[2].z,points[3].z);
    return {{x.first,y.first,z.first},{x.second,y.second,z.second}};
}

// 出撃直前に機体がカメラ前へ入る姿勢に向けて位置を補間する。
StageFlightSample StageFlightApproach(Math::Vector3 origin, Math::Vector3 velocity,
    float duration, float elapsed)
{
    // 始点の速度を引き継ぎ、終点では相対速度・加速度をともに0にする。
    const float t=std::clamp(elapsed/duration,0.0f,1.0f), t2=t*t, t3=t2*t, t4=t3*t, t5=t4*t;
    const float p=1.0f-10.0f*t3+15.0f*t4-6.0f*t5, v=t-6.0f*t3+8.0f*t4-3.0f*t5;
    const float dp=(-30.0f*t2+60.0f*t3-30.0f*t4)/duration;
    const float dv=1.0f-18.0f*t2+32.0f*t3-15.0f*t4;
    return {{origin.x*p+velocity.x*duration*v,origin.y*p+velocity.y*duration*v,
        origin.z*p+velocity.z*duration*v},
        {origin.x*dp+velocity.x*dv,origin.y*dp+velocity.y*dv,origin.z*dp+velocity.z*dv}};
}

Math::Vector3 StageEntryShipView(float elapsed)
{
    // カメラの下で向きを整え、背中を見せたまま入口へ加速する。
    static constexpr std::array<StageFlightKey,9> keys{{
        {0.00f,{0.0f,-3.30f,1.20f},{0.0f,0.0f,18.0f}},
        {StageEntryTiming::kStageFlybyFollowTime,{0.0f,-3.00f,4.80f},{0.0f,1.8f,18.0f}},
        {0.62f,{0.0f,-1.30f,11.50f},{0.0f,2.8f,13.0f}},
        {1.10f,{0.20f,-0.38f,16.30f},{1.0f,0.1f,7.0f}},
        {StageEntryTiming::kStagePortalBoostTime,{0.28f,-0.38f,16.86f},{1.0f,0.0f,7.0f}},
        {1.50f,{0.60f,-0.32f,20.756f},{0.0f,-0.55f,17.35f}},
        {1.70f,{0.55f,-0.45f,24.426f},{-2.7f,-0.5f,19.35f}},
        {StageEntryTiming::kStagePortalTransitDuration,{0.18f,-0.525f,27.75f},{-2.5f,-0.5f,25.0f}},
        {StageEntryTiming::kStagePortalClearTime,{-0.57f,-0.675f,35.25f},{-2.5f,-0.5f,25.0f}}
    }};
    return StageFlightSpline(elapsed,keys);
}

Math::Vector3 StageEntryCameraView(float elapsed)
{
    // 通過した機体の後方へ回り、入口から先は同じ追走位置を引き継ぐ。
    static constexpr std::array<StageFlightKey,8> keys{{
        {StageEntryTiming::kStageFlybyFollowTime,{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f}},
        {0.62f,{0.0f,-0.30f,4.50f},{0.0f,0.8f,14.0f}},
        {1.10f,{0.10f,0.50f,11.50f},{0.5f,0.0f,5.0f}},
        {StageEntryTiming::kStagePortalBoostTime,{0.14f,0.50f,11.90f},{0.5f,0.0f,5.0f}},
        {1.50f,{0.40f,0.38f,15.10f},{0.0f,-0.45f,17.0f}},
        {1.70f,{0.30f,0.21f,18.80f},{-1.5f,-0.5f,22.0f}},
        {StageEntryTiming::kStagePortalTransitDuration,{0.09f,0.13f,22.25f},{-1.0f,-0.4f,25.0f}},
        {StageEntryTiming::kStagePortalClearTime,{-0.21f,0.01f,29.75f},{-1.0f,-0.4f,25.0f}}
    }};
    return StageFlightSpline(elapsed,keys);
}

// 待機中の周回、開口部を通る飛行、次の周回へ戻る動きを、同じ時計でつなぐ。
Math::Vector3 StagePreviewPatrolPosition(float elapsed)
{
    // 周回の間に、入口の表面を機首から横断する二度の切断パスを入れる。
    // 切断中は高さと奥行きを固定し、発光する切断線から機体が外れない。
    constexpr float duration=StageEntryTiming::kStagePatrolDuration;
    const float time=(std::max)(0.0f,elapsed);
    const int variation=static_cast<int>(time/duration)%3;
    std::array<StageFlightKey,14> keys{{
        {0,{1000,-35,15},{-450,-100,12}},
        {1.2f,{240,-150,42},{-160,0,28}},
        {2.4f,{1120,-140,55},{650,80,-30}},
        {3.10f,{1450,0,21.5f},{-400,0,0}},
        {3.35f,{1150,0,21.5f},{-2600,0,0}},
        {3.80f,{-100,0,21.5f},{-2600,0,0}},
        {4.30f,{-220,-100,34},{300,-160,25}},
        {5.8f,{520,-170,76},{600,60,0}},
        {7.4f,{1120,100,42},{200,-80,-20}},
        {8.80f,{1450,0,21.5f},{-400,0,0}},
        {9.05f,{1150,0,21.5f},{-2600,0,0}},
        {9.50f,{-100,0,21.5f},{-2600,0,0}},
        {10.2f,{120,-130,48},{650,30,30}},
        {12.0f,{1000,-35,15},{-450,-100,12}}
    }};
    if (variation == 1) { keys[1].position.y=100; keys[7].position.x=280; keys[8].position.y=-100; }
    if (variation == 2) { keys[1].position.x=150; keys[7].position.x=800; keys[8].position.y=160; }
    const auto canvas=StageFlightSpline(std::fmod(time,duration),keys);
    const float focal=360.0f/std::tan(0.45f);
    const float screenY=500.0f-0.19f*canvas.x+canvas.y;
    return {(canvas.x-640.0f)*canvas.z/focal,(360.0f-screenY)*canvas.z/focal,canvas.z};
}

// 開口部の位置を基準に、周回中の機体をカメラから見た座標へ移す。
Math::Vector3 StagePortalView(Math::Vector3 view, Math::Vector2 seam)
{
    const float slope=-0.19f-seam.y;
    view.y+=(500.0f-seam.x+slope*640.0f)*view.z/(360.0f/std::tan(0.45f))+slope*view.x;
    return view;
}

}

// 選択画面から引き継いだ値と、出撃・再挑戦・目標表示の時計を初期化する。
void StageEntryPresentation::Reset()
{
    titleEntryTime_ = -1.0f;
    titleEntryFromStage_ = false;
    titleEntryStartRail_ = 0.0f;
    stageOverviewActive_ = false;
    stageReturnTime_ = -1.0f;
    stagePreviewVelocity_ = {};
    stageBriefingTime_ = retryEntryTime_ = -1.0f;
}

Math::Vector3 StageEntryPresentation::GetStagePatrolPosition(float elapsed, Math::Vector2 seam)
{
    return StagePortalView(StagePreviewPatrolPosition(elapsed),seam);
}

// レールの位置に合わせて上空のカメラを置き、ステージの前方へ向ける。
GameplayCamera::Pose StageEntryPresentation::GetPreviewCamera(float railDistance) const
{
    GameplayCamera::Pose pose;
    pose.position = {kStagePreviewCamera.x,kStagePreviewCamera.y,railDistance+kStagePreviewCamera.z};
    const Math::Vector3 look{-pose.position.x,-2.0f-pose.position.y,
        railDistance+180.0f-pose.position.z};
    pose.rotation = {std::atan2(-look.y,std::hypot(look.x,look.z)),
        std::atan2(look.x,look.z),0.0f};
    pose.fov = 0.90f;
    return pose;
}

// ステージ選択中に周回する機体の位置と姿勢を、カメラに対する座標から求める。
StageEntryPresentation::ShipFrame StageEntryPresentation::UpdatePreview(float elapsed, const Math::Matrix4x4& frame)
{
    // 入口を描くカメラの実座標を、周回とその後の出撃で共有する。
    stageOverviewActive_ = true;
    stagePortalFrame_ = frame;
    const auto position=TransformCoord(GetStagePatrolPosition(elapsed,stagePortalSeam_),frame);
    const auto before=TransformCoord(GetStagePatrolPosition(elapsed-0.004f,stagePortalSeam_),frame);
    const auto after=TransformCoord(GetStagePatrolPosition(elapsed+0.004f,stagePortalSeam_),frame);
    const float step=elapsed < 0.004f ? 0.004f+(std::max)(0.0f,elapsed) : 0.008f;
    stagePreviewVelocity_={(after.x-before.x)/step,(after.y-before.y)/step,(after.z-before.z)/step};
    const float phase=std::fmod((std::max)(0.0f,elapsed),12.0f);
    const int variation=static_cast<int>((std::max)(0.0f,elapsed)/12.0f)%3;
    const float rollT=std::clamp((phase-4.0f)/1.20f,0.0f,1.0f);
    const float patrolBank=-0.90f*std::sin(phase*6.2831853f/12.0f)+
        (variation == 2 ? -1.0f : 1.0f)*rollT*rollT*(3.0f-2.0f*rollT)*6.2831853f;
    float cutting=0.0f;
    for (const float contact : kStagePatrolCutTimes) {
        const float a=std::clamp((phase-contact+0.20f)/0.20f,0.0f,1.0f);
        const float b=std::clamp((phase-contact-0.45f)/0.20f,0.0f,1.0f);
        cutting=(std::max)(cutting,a*a*(3.0f-2.0f*a)*(1.0f-b*b*(3.0f-2.0f*b)));
    }
    const float bank=std::lerp(patrolBank,std::atan(-stagePortalSeam_.y),cutting);
    stagePreviewBank_ = bank;
    const auto rotation = StageFlightRotation(stagePreviewVelocity_,
        { frame.m[1][0], frame.m[1][1], frame.m[1][2] }, bank);
    const float thrust = 0.95f+0.30f*std::pow(std::sin(phase*3.1415927f/6.0f),2.0f)+cutting*0.65f;
    return { position, rotation, thrust };
}

// 出撃に入る前に機体の位置をつなぎ直し、次に使う姿勢と噴射の強さを返す。
StageEntryPresentation::ShipFrame StageEntryPresentation::UpdateReturn(float elapsed, const Math::Vector3& shipPosition)
{
    const auto length=[](Math::Vector3 v) { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); };
    const auto smooth=[](float t) { t=std::clamp(t,0.0f,1.0f); return t*t*(3.0f-2.0f*t); };
    const auto launchVelocity=StagePortalView({0.0f,0.0f,18.0f},stagePortalSeam_);
    const auto launchOrigin=StagePortalView(StageEntryShipView(0.0f),stagePortalSeam_);
    // 周回の勢いで画面端へ抜け、画面外で入口へ向き直ってから登場する。
    if (stageReturnTime_ < 0.0f) {
        const auto inverse=Inverse(stagePortalFrame_);
        const auto origin=TransformCoord(shipPosition,inverse);
        const auto tip=TransformCoord({shipPosition.x+stagePreviewVelocity_.x,
            shipPosition.y+stagePreviewVelocity_.y,
            shipPosition.z+stagePreviewVelocity_.z},inverse);
        const Math::Vector3 velocity{tip.x-origin.x,tip.y-origin.y,tip.z-origin.z};
        stageReturnBank_=std::remainder(stagePreviewBank_,6.2831853f);
        const float screenVelocity=velocity.x*origin.z-origin.x*velocity.z;
        const float side=std::abs(screenVelocity) > 0.01f ? (screenVelocity > 0.0f ? 1.0f : -1.0f) :
            (origin.x >= 0.0f ? 1.0f : -1.0f);
        const float exitZ=std::clamp(origin.z+velocity.z*0.25f,12.0f,40.0f);
        const Math::Vector3 exit{side*1.55f*exitZ,std::clamp(origin.y,-8.0f,8.0f),exitZ};
        const Math::Vector3 exitVelocity{side*22.0f,0.0f,0.0f};
        stageReturnDuration_=0.0f;
        stageReturnLegCount_=0;
        const auto addLeg=[&](Math::Vector3 from,Math::Vector3 to,Math::Vector3 incoming,
            Math::Vector3 outgoing,float rate,float minimum,float maximum) {
            auto& leg=stageReturnLegs_[stageReturnLegCount_++];
            leg.startSpeed=(std::max)(0.01f,length(incoming)); leg.endSpeed=length(outgoing);
            const float lead=(std::min)(0.24f,12.0f/leg.startSpeed);
            leg.curve={from,Math::Vector3{from.x+incoming.x*lead,from.y+incoming.y*lead,from.z+incoming.z*lead},
                Math::Vector3{to.x-outgoing.x*0.22f,to.y-outgoing.y*0.22f,to.z-outgoing.z*0.22f},to};
            leg.arcLength[0]=0.0f;
            auto previous=from;
            for (size_t index=1; index<leg.arcLength.size(); ++index) {
                const auto p=StageReturnCurve(leg.curve,static_cast<float>(index)/
                    static_cast<float>(leg.arcLength.size()-1)).position;
                leg.arcLength[index]=leg.arcLength[index-1]+length({p.x-previous.x,p.y-previous.y,p.z-previous.z});
                previous=p;
            }
            const float distance=leg.arcLength.back();
            leg.duration=(std::min)(std::clamp(distance/rate,minimum,maximum),
                distance/(0.15f*(leg.startSpeed+leg.endSpeed)+0.70f*10.0f));
            leg.cruiseSpeed=(distance/leg.duration-0.15f*(leg.startSpeed+leg.endSpeed))/0.70f;
            stageReturnDuration_+=leg.duration;
        };
        addLeg(origin,exit,velocity,exitVelocity,42.0f,0.35f,1.35f);
        // 左右どちらへ抜けても、背後を通って画角の下で前方へ向く。
        const Math::Vector3 behind{side*12.0f,-12.0f,-14.0f}, behindVelocity{-side*25.0f,-10.0f,0.0f};
        addLeg(exit,behind,exitVelocity,behindVelocity,70.0f,0.35f,0.90f);
        addLeg(behind,launchOrigin,behindVelocity,launchVelocity,55.0f,0.32f,0.70f);
    }
    stageReturnTime_ = elapsed;
    stageOverviewActive_ = true;
    size_t legIndex=0;
    float legTime=elapsed;
    while (legIndex+1 < stageReturnLegCount_ && legTime > stageReturnLegs_[legIndex].duration) {
        legTime-=stageReturnLegs_[legIndex++].duration;
    }
    const auto& leg=stageReturnLegs_[legIndex];
    const float t=std::clamp(legTime/leg.duration,0.0f,1.0f);
    // 速度は前後30%で滑らかに接続。遠くから戻るときだけ必要な距離を飛ぶ時間を取る。
    const auto integral=[](float u) { return u*u*u-0.5f*u*u*u*u; };
    const float entry=std::clamp(t/0.30f,0.0f,1.0f), exit=std::clamp((t-0.70f)/0.30f,0.0f,1.0f);
    const float entryDistance=0.30f*(entry-integral(entry));
    const float exitDistance=0.30f*integral(exit);
    const float distance=leg.duration*(leg.cruiseSpeed*t+(leg.startSpeed-leg.cruiseSpeed)*entryDistance+
        (leg.endSpeed-leg.cruiseSpeed)*exitDistance);
    const float speed=leg.cruiseSpeed+(leg.startSpeed-leg.cruiseSpeed)*(1.0f-smooth(entry))+
        (leg.endSpeed-leg.cruiseSpeed)*smooth(exit);
    const auto found=std::lower_bound(leg.arcLength.begin()+1,leg.arcLength.end(),distance);
    const size_t index=static_cast<size_t>(found-leg.arcLength.begin());
    const size_t segment=(std::min)(index,leg.arcLength.size()-1);
    const float span=leg.arcLength[segment]-leg.arcLength[segment-1];
    const float u=(static_cast<float>(segment-1)+std::clamp((distance-leg.arcLength[segment-1])/
        (std::max)(0.00001f,span),0.0f,1.0f))/static_cast<float>(leg.arcLength.size()-1);
    const auto sample=StageReturnCurve(leg.curve,u);
    const auto position=TransformCoord(sample.position,stagePortalFrame_);
    const auto viewDirection=Math::Normalize(sample.velocity);
    const auto direction=TransformCoord(viewDirection,stagePortalFrame_);
    const Math::Vector3 velocity{(direction.x-stagePortalFrame_.m[3][0])*speed,
        (direction.y-stagePortalFrame_.m[3][1])*speed,(direction.z-stagePortalFrame_.m[3][2])*speed};
    const Math::Vector3 up{stagePortalFrame_.m[1][0],stagePortalFrame_.m[1][1],stagePortalFrame_.m[1][2]};
    const auto ahead=StageReturnCurve(leg.curve,(std::min)(1.0f,u+0.01f)).velocity;
    const float turning=std::atan2(viewDirection.z*ahead.x-viewDirection.x*ahead.z,
        viewDirection.x*ahead.x+viewDirection.z*ahead.z);
    const float curveBank=std::clamp(-turning*12.0f,-0.85f,0.85f);
    const float bank=std::lerp(std::lerp(stageReturnBank_,curveBank,smooth(elapsed/0.45f)),
        0.0f,smooth((elapsed-stageReturnDuration_+0.30f)/0.30f));
    const auto rotation=StageFlightRotation(velocity,up,bank);
    stagePreviewVelocity_ = velocity;
    return { position, rotation, 1.05f+0.50f*std::sin(t*3.1415927f) };
}

// 上空からステージを見せる導入を進め、機体とカメラを通常飛行へつなぐための姿勢を返す。
StageEntryPresentation::FlightFrame StageEntryPresentation::UpdateOverview(float elapsed, const FlightInput& input)
{
    FlightFrame frame;
    auto& pose = frame.camera;
    if (!IsTitleActive()) {
        titleEntryStartRail_ = input.railDistance;
    }
    const auto smooth = [](float value) {
        const float u=std::clamp(value,0.0f,1.0f); return u*u*(3.0f-2.0f*u);
    };
    // 入口の位置は選択画面から固定。近距離の旋回はこの座標で撮影する。
    const auto portalPoint = [&](Math::Vector3 view) {
        auto p=TransformCoord(StagePortalView(view,stagePortalSeam_),stagePortalFrame_); p.z-=titleEntryStartRail_; return p;
    };
    const auto portalVector = [&](Math::Vector3 view) {
        const auto p=TransformCoord(StagePortalView(view,stagePortalSeam_),stagePortalFrame_);
        return Math::Vector3{p.x-stagePortalFrame_.m[3][0],p.y-stagePortalFrame_.m[3][1],
            p.z-stagePortalFrame_.m[3][2]};
    };
    constexpr float clear=kStagePortalClearTime;
    constexpr float arrivalDuration=kStageStartFlightEnd-clear;
    const bool throughPortal=elapsed >= clear;
    const float arrivalTime=(std::max)(0.0f,elapsed-clear);
    const float t=std::clamp(arrivalTime/arrivalDuration,0.0f,1.0f), ease=smooth(t);
    const float surge=std::sin(t*3.1415927f);
    const float anticipation=smooth((elapsed-0.70f)/0.30f)*
        (1.0f-smooth((elapsed-kStagePortalBoostTime)/0.15f));
    const float passKick=smooth((elapsed-kStagePortalBoostTime)/0.12f)*
        (1.0f-smooth((elapsed-clear-0.10f)/0.60f));
    const float speed=input.speedRate, distance=kRushCameraDistance+speed*0.65f;
    // レールの時計だけを穏やかに巡航へ合わせる。機体とカメラの位置は飛ばさない。
    constexpr float accelerationDuration=0.45f;
    const float acceleration=std::clamp(arrivalTime/accelerationDuration,0.0f,1.0f);
    const float cruiseTravel=arrivalTime < accelerationDuration ? accelerationDuration*
        (acceleration*acceleration*acceleration-0.5f*std::pow(acceleration,4.0f)) :
        arrivalTime-accelerationDuration*0.5f;
    const float boost=0.20f*surge*surge;
    const float travel=cruiseTravel+0.20f*((std::min)(arrivalTime,arrivalDuration)*0.5f-
        arrivalDuration*std::sin(t*6.2831853f)/12.5663706f);
    frame.railDistance=titleEntryStartRail_+input.railSpeed*60.0f*travel;
    const float railVelocity=input.railSpeed*60.0f*(smooth(acceleration)+boost);
    const auto portalForward=portalVector({0,0,1}), portalUp=portalVector({0,1,0});
    Math::Vector3 position{}, rotation{};
    if (!throughPortal) {
        const auto view=StageEntryShipView(elapsed);
        position=portalPoint(view); position.z+=frame.railDistance;
        const auto before=StageEntryShipView((std::max)(0.0f,elapsed-0.001f));
        const auto after=StageEntryShipView((std::min)(clear,elapsed+0.001f));
        const auto velocity=portalVector({after.x-before.x,after.y-before.y,after.z-before.z});
        const float turn=smooth((elapsed-kStageFlybyFollowTime)/0.64f);
        const float bank=Lerp(0.0f,std::atan(-stagePortalSeam_.y),smooth(elapsed/1.47f))-
            anticipation*0.26f;
        rotation=StageFlightRotation(velocity,portalUp,bank);
        const float follow=smooth((elapsed-0.05f)/0.80f);
        auto eye=StageEntryCameraView(elapsed);
        eye={eye.x*follow,eye.y*follow,eye.z*follow};
        pose.position=portalPoint(eye); pose.position.z+=frame.railDistance;
        const Math::Vector3 look{position.x-pose.position.x,
            position.y+0.30f-pose.position.y,position.z-pose.position.z};
        const auto initial=StageFlightRotation(portalForward,portalUp,0.0f);
        const auto tracking=StageFlightRotation(look,portalUp,-0.045f*std::sin(turn*3.1415927f));
        pose.rotation={initial.x+std::remainder(tracking.x-initial.x,6.2831853f)*follow,
            initial.y+std::remainder(tracking.y-initial.y,6.2831853f)*follow,
            initial.z+std::remainder(tracking.z-initial.z,6.2831853f)*follow};
        pose.fov=Lerp(0.90f,0.98f+0.05f*smooth((elapsed-kStagePortalBoostTime)/0.30f),follow)+
            passKick*0.025f-anticipation*0.018f;
    } else {
        const auto exit=portalPoint(StageEntryShipView(clear));
        const auto exitVelocity=portalVector({-2.5f,-0.5f,25.0f});
        const auto flight=StageFlightApproach(exit,exitVelocity,arrivalDuration,arrivalTime);
        // 通過後は翼を傾けて一度だけ斜めに切り込み、街の進行方向へ収束させる。
        // 追加の変位と速度は両端で0。入口の接続と操作開始時の位置を保つ。
        const float carve=surge*surge*(1.0f-t);
        const float carveVelocity=(2.0f*surge*std::cos(t*3.1415927f)*3.1415927f*(1.0f-t)-
            surge*surge)/arrivalDuration;
        position={flight.position.x+2.6f*carve,flight.position.y-1.4f*surge*surge,
            frame.railDistance+flight.position.z};
        const Math::Vector3 velocity{flight.velocity.x+2.6f*carveVelocity,
            flight.velocity.y-2.8f*surge*std::cos(t*3.1415927f)*3.1415927f/arrivalDuration,
            flight.velocity.z+railVelocity};
        const Math::Vector3 up{portalUp.x*(1.0f-ease),Lerp(portalUp.y,1.0f,ease),portalUp.z*(1.0f-ease)};
        const float bank=std::atan(-stagePortalSeam_.y)*(1.0f-smooth(arrivalTime/0.65f))-
            0.30f*surge*surge-0.48f*carve;
        rotation=StageFlightRotation(velocity,up,bank);
        // 飛行方向の少し後ろを追い、地面へ近づいてから通常の距離まで引く。
        const auto followFrame=Math::MakeAffineMatrix({1,1,1},StageFlightRotation(velocity,up,0.0f),{});
        const auto startFrame=Math::MakeAffineMatrix({1,1,1},StageFlightRotation(exitVelocity,portalUp,0.0f),{});
        const auto exitCamera=portalPoint(StageEntryCameraView(clear));
        const auto initialOffset=TransformCoord({0.0f,0.685f,-5.50f},startFrame);
        const Math::Vector3 correction{exitCamera.x-exit.x-initialOffset.x,
            exitCamera.y-exit.y-initialOffset.y,exitCamera.z-exit.z-initialOffset.z};
        const auto followCorrection=StageFlightApproach(correction,portalVector({1.5f,0.1f,0.0f}),
            arrivalDuration,arrivalTime).position;
        const float pullback=smooth((t-0.48f)/0.52f);
        const auto offset=TransformCoord({-2.4f*carve,Lerp(0.685f,1.80f,ease)+carve*0.30f,
            -Lerp(5.50f,distance,pullback)},followFrame);
        pose.position={position.x+offset.x+followCorrection.x,
            position.y+offset.y+followCorrection.y,position.z+offset.z+followCorrection.z};
        const float aimHeight=Lerp(0.30f,1.80f-std::tan(0.060f)*distance,ease);
        const Math::Vector3 look{position.x-pose.position.x,
            position.y+aimHeight-pose.position.y,position.z-pose.position.z};
        pose.rotation=StageFlightRotation(look,up,-0.070f*surge*surge);
        pose.fov=Lerp(1.03f,kRushCameraBaseFovY+speed*0.035f,pullback)+
            passKick*0.025f+carve*0.025f;
    }
    stageOverviewActive_=elapsed < kStageStartOverviewDuration;
    titleEntryFromStage_=true;
    titleEntryTime_=Lerp(kTitleEntryWorldTime,kTitleEntryDuration,
        std::clamp(elapsed/kStageStartOverviewDuration,0.0f,1.0f));
    stageBriefingTime_=elapsed;
    frame.blurStrength = (throughPortal ? speed*0.14f+surge*surge*0.22f :
        0.08f*smooth((elapsed-kStagePortalBoostTime)/0.32f))+passKick*0.06f;
    frame.ship = { position, rotation, 1.10f-anticipation*0.22f+passKick*0.95f+surge*surge*0.55f };
    frame.clockFrames = elapsed*60.0f;
    return frame;
}

// 出撃中の目標表示を、時間に応じた位置と透明度で描く。
void StageEntryPresentation::DrawBriefing(float elapsed, const Math::Vector2& min, const Math::Vector2& size) const
{
    if (elapsed < 0.0f || elapsed >= BriefingEnd()) { return; }
    const bool cinematic = IsStageCinematic();
    const auto smooth = [](float t) { t=std::clamp(t,0.0f,1.0f); return t*t*(3.0f-2.0f*t); };
    const float scale = CombatHud::Scale(size);
    const ImVec2 origin{min.x+(size.x-1280.0f*scale)*0.5f,min.y+(size.y-720.0f*scale)*0.5f};
    auto* draw = ImGui::GetForegroundDrawList();
    draw->PushClipRect({min.x,min.y},{min.x+size.x,min.y+size.y},true);
    const auto text = [&](const char* value,float y,float fontSize,ImU32 color,float alpha) {
        if (alpha <= 0.0f) { return; }
        const int first=draw->VtxBuffer.Size;
        const float x = cinematic ? 640.0f-MenuUi::Width(value,fontSize,true)*0.5f : 72.0f;
        const float slide = cinematic ? (1.0f-smooth(elapsed/0.16f))*-16.0f : 0.0f;
        const ImVec2 at{origin.x+x*scale,origin.y+(y+slide)*scale};
        // 明るい街並みでも読めるよう、文字の輪郭だけを暗くする。
        for (const auto offset : std::array<ImVec2,4>{{{-1,-1},{1,-1},{-1,1},{1,1}}}) {
            MenuUi::Heading(draw,{at.x+offset.x*2.0f*scale,at.y+offset.y*2.0f*scale},
                fontSize*scale,IM_COL32(7,13,24,220),value);
        }
        MenuUi::Heading(draw,at,fontSize*scale,color,value);
        MenuUi::Opacity(draw,first,alpha);
    };
    if (cinematic) {
        const float closing=1.0f-smooth((elapsed-(kStageStartFlightEnd+0.10f))/0.50f);
        const float bars = smooth(elapsed/0.24f)*closing;
        const float height = 54.0f*scale*bars;
        draw->AddRectFilled({min.x,min.y},{min.x+size.x,min.y+height},IM_COL32(0,0,0,255));
        draw->AddRectFilled({min.x,min.y+size.y-height},{min.x+size.x,min.y+size.y},IM_COL32(0,0,0,255));
        const float wind=smooth((elapsed-kStagePortalBoostTime)/0.16f)*
            (1.0f-smooth((elapsed-3.25f)/0.70f));
        // 風切りの線は画面端だけへ短く流し、機体と目標の周りを空ける。
        draw->PushClipRect({min.x,min.y+height},{min.x+size.x,min.y+size.y-height},true);
        const ImVec2 vanishing{min.x+size.x*0.50f,min.y+size.y*0.44f};
        for (int index=0;index<6;++index) {
            const float phase=std::fmod(elapsed*2.8f+static_cast<float>(index)*0.37f,1.0f);
            const float side=index%2 == 0 ? -1.0f : 1.0f;
            const ImVec2 edge{vanishing.x+side*size.x*(0.53f-phase*0.035f),
                min.y+size.y*(0.16f+static_cast<float>(index/2)*0.28f)};
            const float tail=0.07f+0.04f*phase;
            const ImVec2 inner{edge.x+(vanishing.x-edge.x)*tail,edge.y+(vanishing.y-edge.y)*tail};
            const float pulse=std::sin(phase*3.1415927f);
            draw->AddLine(inner,edge,IM_COL32(210,236,255,static_cast<int>(58.0f*wind*pulse*pulse)),scale);
        }
        draw->PopClipRect();
        const float objective = smooth((elapsed-kStagePortalTransitDuration)/0.25f)*closing;
        text("目標",66.0f,34.0f,MenuUi::Paper,objective);
        text("敵編隊を突破し、ボスを撃破",112.0f,58.0f,MenuUi::Paper,objective);
    } else {
        text("航空都市",454.0f,48.0f,MenuUi::Paper,
            smooth(elapsed/0.22f)*(1.0f-smooth((elapsed-1.70f)/0.35f)));
        text("目標：敵編隊を突破し、ボスを撃破",518.0f,23.0f,MenuUi::Paper,
            smooth((elapsed-0.42f)/0.20f)*(1.0f-smooth((elapsed-4.10f)/0.50f)));
    }
    draw->PopClipRect();
}

// 再挑戦用の開始位置とレール距離を控え、短い出撃演出の時計を始める。
void StageEntryPresentation::BeginRetry(const Math::Vector3& shipPosition, float railDistance)
{
    retryEntryTime_ = 0.0f;
    retryEntryStartRail_ = railDistance;
    retryEntryOrigin_ = shipPosition;
    stageBriefingTime_ = 2.20f;
    stageOverviewActive_ = false;
    stageReturnTime_ = -1.0f;
    titleEntryTime_ = -1.0f;
}

// 再挑戦用の機体とカメラを動かし、操作に戻る時点を返す。
StageEntryPresentation::FlightFrame StageEntryPresentation::UpdateRetry(float deltaSeconds, const FlightInput& input)
{
    FlightFrame frame;
    auto& pose = frame.camera;
    constexpr float duration = StageEntryTiming::kRetryEntryDuration;
    const float delta=std::clamp(deltaSeconds,0.0f,0.10f);
    retryEntryTime_=(std::min)(duration,retryEntryTime_+delta);
    stageBriefingTime_+=delta;
    const float t=retryEntryTime_/duration;
    const float ease=t*t*(3.0f-2.0f*t), remaining=1.0f-ease;
    const float surge=std::sin(t*3.1415927f);
    frame.railDistance=retryEntryStartRail_+input.railSpeed*60.0f*retryEntryTime_;
    const Math::Vector3 position{retryEntryOrigin_.x,retryEntryOrigin_.y,frame.railDistance};
    const float speed=input.speedRate;
    pose.position={position.x*0.10f+1.8f*remaining,
        2.10f+position.y*0.05f+1.10f*remaining,
        frame.railDistance-kRushCameraDistance-speed*0.65f-3.0f*remaining};
    pose.rotation={0.060f+0.035f*remaining,-0.075f*remaining,-0.045f*remaining};
    pose.fov=kRushCameraBaseFovY+speed*0.035f-0.09f*remaining+0.035f*surge;
    frame.ship = { position, { 0.0f, 0.0f, -0.24f*remaining }, 0.80f+surge*0.60f };
    frame.blurStrength = speed*0.14f+surge*0.26f;
    frame.clockFrames = retryEntryTime_*60.0f;
    return frame;
}

// 出撃へ引き継ぐ機体・カメラの姿勢とモデルの大きさを控え、演出の開始状態を作る。
void StageEntryPresentation::BeginTitle(const Math::Vector3& viewPosition, const Math::Vector3& rotation,
    const Math::Vector3& cameraRotation, float titleScale, float shipScale, const FlightInput& input)
{
    titleEntryFromStage_ = false;
    titleEntryStartRail_ = input.railDistance;
    titleEntryRotation_ = rotation;
    titleEntryCameraRotation_ = cameraRotation;
    // 同じ機体を同じ画角で見せる。モデル倍率の違いは距離で相殺し、切替で縮ませない。
    const auto offset = RotateLocalOffset(viewPosition *
        (shipScale / (std::max)(titleScale,0.01f)),cameraRotation);
    const float distance = input.tutorial ? kGameplayCameraInitialDistance : kRushCameraDistance + input.speedRate * 0.65f;
    titleEntryOrigin_ = {offset.x,offset.y + (input.tutorial ? 2.70f : 2.10f),offset.z - distance};
    titleEntryTime_ = kTitleEntryWorldTime;
}

// タイトル側から引き継いだ出撃演出を進め、画面内の自機とカメラを同じ時刻から求める。
StageEntryPresentation::FlightFrame StageEntryPresentation::UpdateTitle(float time, const FlightInput& input)
{
    FlightFrame frame;
    auto& pose = frame.camera;
    if (titleEntryTime_ < 0.0f) { titleEntryStartRail_ = input.railDistance; }
    titleEntryTime_ = std::clamp(time, kTitleEntryWorldTime, kTitleEntryDuration);
    const auto ease = [](float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    const float flight = titleEntryTime_ - kTitleEntryWorldTime;
    if (titleEntryFromStage_) {
        constexpr float stagePassTime=0.58f;
        stageBriefingTime_=kStageStartOverviewDuration+stagePassTime+
            flight/(kTitleEntryDuration-kTitleEntryWorldTime)*(kStageStartLaunchDuration-stagePassTime);
    }
    const float chase = titleEntryFromStage_ ? ease(flight/1.02f) : ease((flight - 0.30f) / 0.90f);
    const float remaining = 1.0f - chase;
    const float speed = input.speedRate;
    const float distance = input.tutorial ? kGameplayCameraInitialDistance : kRushCameraDistance + speed * 0.65f;
    const auto curve = [](Math::Vector3 a, Math::Vector3 b, Math::Vector3 c, Math::Vector3 d, float t) {
        t = std::clamp(t,0.0f,1.0f);
        const float u = 1.0f - t;
        return Math::Vector3{
            a.x*u*u*u + 3.0f*b.x*u*u*t + 3.0f*c.x*u*t*t + d.x*t*t*t,
            a.y*u*u*u + 3.0f*b.y*u*u*t + 3.0f*c.y*u*t*t + d.y*t*t*t,
            a.z*u*u*u + 3.0f*b.z*u*u*t + 3.0f*c.z*u*t*t + d.z*t*t*t };
    };
    const Math::Vector3 nearPass{1.8f,2.3f,-distance + 4.8f};
    const auto path = [&](float t) {
        if (titleEntryFromStage_) {
            // 入ってきた機首方向へ一度抜けてから旋回。切替直後に軌道を中央へ折り返さない。
            const Math::Vector3 lead{titleEntryOrigin_.x,titleEntryOrigin_.y,
                titleEntryOrigin_.z-input.railSpeed*60.0f*0.34f};
            return curve(titleEntryOrigin_,lead,{},{},t/1.02f);
        }
        if (t < 0.30f) {
            const auto exit = RotateLocalOffset({3.4f,0,0},titleEntryCameraRotation_);
            return curve(titleEntryOrigin_,{titleEntryOrigin_.x + exit.x,titleEntryOrigin_.y + exit.y,
                titleEntryOrigin_.z + exit.z},{nearPass.x + 3.4f,nearPass.y + 1.0f,nearPass.z + 1.0f},
                nearPass,t / 0.30f);
        }
        // 接線を揃えて近距離の旋回から奥へ突き抜ける。終端は通常レール速度へ滑らかに戻る。
        return curve(nearPass,{nearPass.x - 3.4f * (0.52f / 0.30f),
            nearPass.y - (0.52f / 0.30f),nearPass.z - (0.52f / 0.30f)}, {}, {},
            (t - 0.30f) / 0.52f);
    };
    // レールだけを前進。敵の出現時計・スコア・入力は動かさない。
    frame.railDistance = titleEntryStartRail_ + input.railSpeed * 60.0f * flight;
    const auto local = path(flight);
    const Math::Vector3 position{local.x,local.y,local.z + frame.railDistance};
    const auto before = path((std::max)(0.0f,flight - 0.001f));
    const auto after = path(flight + 0.001f);
    const float step = flight < 0.001f ? 0.001f + flight : 0.002f;
    const auto direction = Math::Normalize({after.x - before.x,after.y - before.y,
        after.z - before.z + input.railSpeed * 60.0f * step});
    const auto cross = [](Math::Vector3 a, Math::Vector3 b) {
        return Math::Vector3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
    };
    const auto right = Math::Normalize(cross({0,1,0},direction));
    const auto up = cross(direction,right);
    const float bank = (titleEntryFromStage_ ? 0.72f : -0.86f) *
        std::sin(std::clamp(flight / (titleEntryFromStage_ ? 1.02f : 0.82f),0.0f,1.0f) * 3.1415927f);
    const float cosine = std::cos(bank), sine = std::sin(bank);
    const Math::Vector3 bankedRight{right.x*cosine+up.x*sine,right.y*cosine+up.y*sine,right.z*cosine+up.z*sine};
    const Math::Vector3 bankedUp{up.x*cosine-right.x*sine,up.y*cosine-right.y*sine,up.z*cosine-right.z*sine};
    const float yaw = std::asin(std::clamp(-bankedRight.z,-1.0f,1.0f));
    const Math::Vector3 rotation{std::atan2(bankedUp.z,direction.z),yaw,
        std::atan2(bankedRight.y,bankedRight.x)};
    const float poseBlend = ease(flight / (titleEntryFromStage_ ? 0.30f : 0.03f));
    const auto blendPose = [&](float start, float end) {
        return titleEntryFromStage_ ? start+std::remainder(end-start,6.2831853f)*poseBlend : Lerp(start,end,poseBlend);
    };
    frame.ship.position = position;
    frame.ship.rotation = {
        blendPose(titleEntryRotation_.x,rotation.x),
        blendPose(titleEntryRotation_.y,rotation.y),
        blendPose(titleEntryRotation_.z,rotation.z)};
    const float surge = std::sin(chase * 3.1415927f);
    pose.position = { -0.45f * surge,
        (input.tutorial ? 2.70f : 2.10f) - 0.60f * surge,
        frame.railDistance - distance - 2.2f * surge };
    if (titleEntryFromStage_) {
        pose.position.x-=2.3f*surge;
        pose.position.y+=2.70f*remaining;
        pose.position.z-=16.0f*remaining;
    }
    pose.rotation = { Lerp(titleEntryCameraRotation_.x,0.060f,chase),
        titleEntryCameraRotation_.y * remaining,titleEntryCameraRotation_.z * remaining +
            (titleEntryFromStage_ ? 0.24f : -0.14f)*surge };
    if (titleEntryFromStage_) {
        // 至近距離を横切る自機へ振り向く。機体を追う角度から前方の通常カメラへ戻す。
        const float aimYaw=std::atan2(position.x-pose.position.x,position.z-pose.position.z);
        const float follow=ease(flight/0.20f)*(1.0f-ease((flight-0.72f)/0.30f));
        pose.rotation.y=Lerp(pose.rotation.y,aimYaw,follow);
    }
    const float normalFov = input.tutorial ? kGameplayCameraBaseFovY : kRushCameraBaseFovY + speed * 0.035f;
    pose.fov = Lerp(0.62f,normalFov,chase) + 0.10f * surge;
    frame.blurStrength = speed * 0.14f + surge * (titleEntryFromStage_ ? 0.68f : 0.38f);
    frame.ship.thrust = 0.80f + surge * (titleEntryFromStage_ ? 1.45f : 0.95f);
    frame.clockFrames = flight * 60.0f;
    return frame;
}

bool StageEntryPresentation::IsForeground() const
{
    // 入口の手前では機体を面より前へ置き、通り抜けてから本編の深度へ渡す。
    return (stageOverviewActive_ && titleEntryTime_ < 0.0f) ||
        (titleEntryFromStage_ && stageReturnTime_ >= 0.0f && titleEntryTime_ >= 0.0f &&
            stageBriefingTime_ < kStagePortalTransitDuration) ||
        (!titleEntryFromStage_ && titleEntryTime_ >= kTitleEntryWorldTime && titleEntryTime_ < kTitleEntryDiveTime);
}

// 出撃の進行に応じて、通常の戦闘HUDを重ねる透明度を返す。
float StageEntryPresentation::TitleHudOpacity() const
{
    constexpr float hudBegin = kTitleEntryDuration - 0.25f;
    const float t = titleEntryFromStage_ && stageReturnTime_ >= 0.0f ?
        std::clamp((stageBriefingTime_-kStageStartFlightEnd-0.40f)/0.20f,0.0f,1.0f) :
        std::clamp((titleEntryTime_-hudBegin)/0.25f,0.0f,1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// 通常戦闘の更新に入った時点で出撃演出の状態を終了する。
void StageEntryPresentation::FinishTitleEntry()
{
    if (titleEntryTime_ >= kTitleEntryDuration) { titleEntryTime_ = -1.0f; }
}

// 再挑戦の演出を描き切った後で状態を終了し、次の更新から通常戦闘へ戻す。
void StageEntryPresentation::FinishRetryFrame()
{
    // HUDの最後のフレームを描いてから、次の更新で戦闘へ戻る。
    if (retryEntryTime_ >= kRetryEntryDuration) { retryEntryTime_ = -1.0f; }
}

float StageEntryPresentation::RetryHudOpacity() const
{
    return std::clamp((retryEntryTime_-(kRetryEntryDuration-0.25f))/0.25f,0.0f,1.0f);
}

float StageEntryPresentation::BriefingEnd() const
{
    return IsStageCinematic() ? kStageStartOverviewDuration + 0.35f : 4.8f;
}

// 表示中の目標案内の時計を進め、規定時間を過ぎたら表示を終える。
void StageEntryPresentation::AdvanceBriefing(float deltaSeconds)
{
    if (stageBriefingTime_ < 0.0f) { return; }
    stageBriefingTime_ += std::clamp(deltaSeconds, 0.0f, 0.10f);
    if (stageBriefingTime_ >= BriefingEnd()) { HideBriefing(); }
}
