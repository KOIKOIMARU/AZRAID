#include "engine/scene/StageSelectionPresentation.h"

#include "app/stage/StageEntryPresentation.h"
#include "app/stage/StageEntryTiming.h"
#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/TitleMotion.h"
#include "engine/scene/TitleRift.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
using namespace TitleMotion;
constexpr float kStageSelectionInputDelay = 0.45f;
constexpr float kStagePortalPanelHalfHeight = 500.0f;
} // namespace

StageSelectionPresentation::StageSelectionPresentation() = default;
StageSelectionPresentation::~StageSelectionPresentation() = default;

// ステージ選択を開くたびに、待機・出撃・裂け目の開閉に使う時刻と引き継ぎ状態を初期化する。
void StageSelectionPresentation::Reset([[maybe_unused]] bool stageSelection)
{
    stageSelectionElapsed_ = 0.0f;
    stageReturnTime_ = -1.0f;
    stageOverviewTime_ = -1.0f;
    stageDepartureGap_ = 160.0f;
    stageDepartureCutAge_ = stagePortalCutIntensity_ = 0.0f;
    stageEntryCaptured_ = false;
#ifdef _DEBUG
    stageLaunchReviewPoint_ = 0;
#endif
    char launchRate[24]{};
    if (GetEnvironmentVariableA("AZRAID_STAGE_LAUNCH_RATE",launchRate,static_cast<DWORD>(sizeof(launchRate))) > 0) {
        const float rate = std::strtof(launchRate,nullptr);
        if (std::isfinite(rate) && rate > 0.0f) { stageLaunchRate_ = std::clamp(rate,0.03f,1.0f); }
    }
#ifdef _DEBUG
    char launchReview[2]{};
    stageLaunchReview_=GetEnvironmentVariableA("AZRAID_STAGE_LAUNCH_REVIEW",launchReview,2) == 1 && launchReview[0] == '1';
    char patrolReview[2]{};
    stagePatrolReview_=GetEnvironmentVariableA("AZRAID_STAGE_PATROL_REVIEW",patrolReview,2) == 1 && patrolReview[0] == '1';
    char patrolPhase[24]{};
    const DWORD patrolPhaseLength=GetEnvironmentVariableA("AZRAID_STAGE_PATROL_PHASE",patrolPhase,24);
    if (stageSelection && patrolPhaseLength > 0 && patrolPhaseLength < sizeof(patrolPhase)) {
        char* end=nullptr;
        const float phase=std::strtof(patrolPhase,&end);
        if (end != patrolPhase && *end == '\0' && std::isfinite(phase) && phase >= 0.0f && phase < 36.0f) {
            stageSelectionElapsed_=phase;
        }
    }
#endif
}

// 裂け目を構成するパネルと専用カメラを解放する。
void StageSelectionPresentation::Clear()
{
    for (auto& panel : stagePortalPanels_) { panel.reset(); }
    for (auto& cut : stagePortalCut_) { cut.reset(); }
    stagePortalCamera_.reset();
}

bool StageSelectionPresentation::InputReady() const
{
    return stageSelectionElapsed_ >= kStageSelectionInputDelay;
}

// 待機演出が入力を受け付けられる時刻に達したか返す。開発用の周回確認では専用の待ち時間を使う。
bool StageSelectionPresentation::CanConfirm() const
{
#ifdef _DEBUG
    // 映像確認のときだけ3種類の周回を通してから自動出撃する。
    if (stagePatrolReview_ && stageSelectionElapsed_ < 38.0f) { return false; }
#endif
    return InputReady();
}

void StageSelectionPresentation::EnableInputNow()
{
    stageSelectionElapsed_ = (std::max)(stageSelectionElapsed_, kStageSelectionInputDelay);
}

// 出撃決定時点の開口幅と最後に切り開いた時刻を控え、選択中の背景の状態を引き継ぐ。
void StageSelectionPresentation::BeginDeparture()
{
    // 決定した瞬間の隙間を起点にして閉じる。周回中の切断の途中でも幅を飛ばさない。
    const auto idle = StagePortalIdle(stageSelectionElapsed_);
    stageDepartureGap_ = idle.gap;
    stageDepartureCutAge_ = idle.cutAge;
}

// 選択画面を飛んでいた機体を出撃開始の位置へ戻す。戻りの時計を進め、完了した時点を返す。
bool StageSelectionPresentation::AdvanceReturn(GameScene* preview, float delta)
{
    const float launchDelta = delta * stageLaunchRate_;
    if (stageReturnTime_ < 0.0f && preview) {
        preview->UpdateStagePreview(stageSelectionElapsed_);
        preview->UpdateStageReturn(0.0f);
        stageReturnTime_ = 0.0f;
    }
    const float returnDuration=preview ? preview->GetStageReturnDuration() : 0.0f;
    if (stageReturnTime_ < returnDuration) {
        stageReturnTime_ = (std::min)(returnDuration,
            (std::max)(0.0f,stageReturnTime_)+launchDelta);
        if (preview) { preview->UpdateStageReturn(stageReturnTime_); }
        return true;
    }
    return false;
}

// ステージ紹介と出撃の時計を進め、次の演出へ移れる時点を返す。
bool StageSelectionPresentation::AdvanceOverview(float delta)
{
    float launchDelta = delta * stageLaunchRate_;
#ifdef _DEBUG
    // 明示した映像試験だけで各見せ場を停止。F8で進み、最後は通常の本編まで通す。
    if (stageLaunchReview_) {
        constexpr std::array<float,11> stops{{0.0f,0.20f,0.65f,1.70f,2.15f,2.50f,3.30f,4.20f,5.00f,5.35f,5.99f}};
        const float clock=(std::max)(0.0f,stageOverviewTime_);
        const float remaining=stops[stageLaunchReviewPoint_]-clock;
        if (remaining <= 0.00001f) {
            launchDelta=0.0f;
            if (ImGui::IsKeyPressed(ImGuiKey_F8,false)) {
                if (++stageLaunchReviewPoint_ == stops.size()) { stageLaunchReview_=false; }
            }
        } else { launchDelta=(std::min)(launchDelta,remaining); }
    }
#endif
    const float previous = stageOverviewTime_;
    stageOverviewTime_ = (std::min)(StageEntryTiming::kStageStartOverviewDuration,
        (std::max)(0.0f, stageOverviewTime_) + launchDelta);
    return previous < StageEntryTiming::kStagePortalBoostTime &&
        stageOverviewTime_ >= StageEntryTiming::kStagePortalBoostTime;
}

bool StageSelectionPresentation::OverviewComplete() const
{
    return stageOverviewTime_ >= StageEntryTiming::kStageStartOverviewDuration;
}

// 出撃開始時点のカメラと機体の時刻を取り込み、ステージ選択と飛行演出をつなぐ。
bool StageSelectionPresentation::CaptureEntry(const Camera& source, Camera& target, float titleElapsed, float flybyClock)
{
    if (stageEntryCaptured_) { return false; }
    target.SetTranslate(source.GetTranslate());
    target.SetRotate(source.GetRotate());
    const auto flyby=GetTitleFlyby(flybyClock,true);
    const auto motion=GetLogoMotion(titleElapsed,flyby,kDepartureOpenTime);
    const auto a=MoveLogoCanvas({0,TitleCutY(0,flyby.cutPattern)},motion);
    const auto b=MoveLogoCanvas({1280,TitleCutY(1280,flyby.cutPattern)},motion);
    const float slope=(b.y-a.y)/(b.x-a.x);
    stageEntrySeam_={a.y-a.x*slope,slope};
    stageEntryCaptured_=true;
    return true;
}

// 画角と画面比率を更新し、ステージの視点を引き継ぐ前は待機用のカメラ位置を使う。
void StageSelectionPresentation::UpdateCamera(Camera& camera, float aspectRatio) const
{
    camera.SetAspectRatio(aspectRatio);
    camera.SetFovY(0.62f);
    if (!stageEntryCaptured_) {
        camera.SetTranslate({ 0.0f, 0.0f, -18.0f });
        camera.SetRotate({ 0.02f, 0.0f, -0.10f });
    }
    camera.Update();
}

// 裂け目の両側のパネルと断面に使うモデルを、初回だけ用意する。
void StageSelectionPresentation::PreparePortal(Object3dCommon* common)
{
    if (!common || stagePortalPanels_.back()) { return; }
    stagePortalCamera_ = std::make_unique<Camera>();
    stagePortalCamera_->SetFarClip(180.0f);
    auto* models = ModelManager::GetInstance();
    models->CreateBox("title_stage_portal_panel",1.0f,1.0f,1.0f,"resources/human/white.png");
    for (size_t index=0; index<stagePortalPanels_.size(); ++index) {
        auto& panel = stagePortalPanels_[index];
        const bool edge = index >= 2;
        panel = std::make_unique<Object3d>();
        panel->Initialize(common);
        panel->SetCamera(stagePortalCamera_.get());
        panel->SetModel(models->FindModel("title_stage_portal_panel"));
        panel->SetScale(edge ? Math::Vector3{1200.0f,0.06f,0.16f} :
            Math::Vector3{1200.0f,kStagePortalPanelHalfHeight*2.0f,3.2f});
        panel->SetColor(edge ? Math::Vector4{0.13f,0.17f,0.21f,1.0f} : Math::Vector4{0.026f,0.034f,0.048f,1.0f});
        panel->SetLightingMode(2);
        panel->SetDirectionalLightDirection({-0.35f,-0.82f,0.32f});
        panel->SetDirectionalLightIntensity(1.05f);
        panel->SetEnvironmentCoefficient(edge ? 0.045f : 0.025f);
        panel->SetRoughness(edge ? 0.30f : 0.64f);
        panel->SetMetallic(edge ? 0.60f : 0.35f);
        panel->SetSpecularColor({0.15f,0.19f,0.24f});
        panel->SetShadowReceiveStrength(0.0f);
    }
    for (auto& cut : stagePortalCut_) {
        cut=std::make_unique<Object3d>();
        cut->Initialize(common);
        cut->SetCamera(stagePortalCamera_.get());
        cut->SetModel(models->FindModel("title_stage_portal_panel"));
        cut->SetLightingMode(0);
        cut->SetShadowReceiveStrength(0.0f);
    }
}

// 切断の時刻と機体の通過に合わせ、ステージが見える開口部の形を更新する。
void StageSelectionPresentation::UpdatePortal(GameScene* preview, TitleRift* rift, bool departing)
{
    if (!stagePortalCamera_) { return; }
    if (!preview || !preview->GetStageCamera()) { return; }
    if (rift) { rift->PrepareFlightDepth(); }
    const auto idle=StagePortalIdle(stageSelectionElapsed_);
    const float seamY = stageEntrySeam_.x;
    const float slope = stageEntrySeam_.y;
    const float angle = std::atan(-slope);
    // 機体と同じカメラ・深度で、選択時の24m先に厚みのある入口を固定する。
    constexpr float unitsPerPixel=2.0f*22.4f*0.4830551f/720.0f; // FOV 0.90、面の手前から投影。
    const float centerY = (360.0f-seamY-640.0f*slope)*unitsPerPixel;
    const float departure=(std::max)(0.0f,stageReturnTime_)+(std::max)(0.0f,stageOverviewTime_);
    const float aperture=departing ? stageDepartureGap_+(46.0f-stageDepartureGap_)*Smooth(departure/1.05f) : idle.gap;
    stagePortalGap_=aperture;
    const float gap=aperture*unitsPerPixel;
    const auto& frame=preview->GetStagePortalFrame();
    const auto rotation=FlightRotation({frame.m[2][0],frame.m[2][1],frame.m[2][2]},
        {frame.m[1][0],frame.m[1][1],frame.m[1][2]},angle);
    const float cutAge=departing ? stageDepartureCutAge_+departure : idle.cutAge;
    stagePortalCutIntensity_=Smooth(cutAge/0.025f)*(1.0f-Smooth((cutAge-0.44f)/0.20f))*
        (departing ? 1.0f-Smooth(departure/0.20f) : 1.0f);
    const float edgeLight=stageOverviewTime_ >= 0.0f ?
        std::sin(std::clamp((stageOverviewTime_-1.40f)/0.40f,0.0f,1.0f)*3.1415927f) :
        stagePortalCutIntensity_*0.40f;
    for (size_t index=0; index<stagePortalPanels_.size(); ++index) {
        const bool edge = index >= 2;
        const float side = index%2 == 0 ? 1.0f : -1.0f;
        const float distance = side*(gap+(edge ? 0.035f : kStagePortalPanelHalfHeight));
        auto& panel = stagePortalPanels_[index];
        panel->SetCamera(preview->GetStageCamera());
        const auto position=RotateVector({-std::sin(angle)*distance,
            centerY+std::cos(angle)*distance,24.0f+(edge ? -1.64f : 0.0f)},frame);
        panel->SetTranslate({position.x+frame.m[3][0],position.y+frame.m[3][1],position.z+frame.m[3][2]});
        panel->SetRotate(rotation);
        // 本編側の黒レベル補正を通しても面の厚みが残る値へ合わせる。
        const bool inScene=stageOverviewTime_ >= 0.0f;
        if (edge) { panel->SetColor({(inScene ? 0.27f : 0.13f)+edgeLight*0.26f,
            (inScene ? 0.27f : 0.17f)+edgeLight*0.35f,(inScene ? 0.27f : 0.21f)+edgeLight*0.40f,1.0f}); }
        else { panel->SetColor(inScene ? Math::Vector4{0.18f,0.18f,0.18f,1.0f} :
            Math::Vector4{0.026f,0.034f,0.048f,1.0f}); }
        panel->Update();
    }
    if (stagePortalCutIntensity_ > 0.0f) {
        const auto shipView=StageEntryPresentation::GetStagePatrolPosition(stageSelectionElapsed_,stageEntrySeam_);
        const float shipX=640.0f+(360.0f/std::tan(0.45f))*shipView.x/shipView.z;
        const float length=60.0f+1200.0f*Smooth(cutAge/0.45f);
        const float centerX=shipX+length*0.50f;
        const auto position=RotateVector({(centerX-640.0f)*unitsPerPixel,
            (360.0f-seamY-centerX*slope)*unitsPerPixel,22.25f},frame);
        for (size_t index=0; index<stagePortalCut_.size(); ++index) {
            auto& cut=stagePortalCut_[index];
            cut->SetCamera(preview->GetStageCamera());
            cut->SetTranslate({position.x+frame.m[3][0],position.y+frame.m[3][1],position.z+frame.m[3][2]});
            cut->SetRotate(rotation);
            cut->SetScale({length*unitsPerPixel,index == 0 ? 0.14f : 0.035f,0.04f});
            const float alpha=stagePortalCutIntensity_*(index == 0 ? 0.26f : 0.85f);
            cut->SetColor(index == 0 ? Math::Vector4{0.20f,0.65f,0.85f,alpha} :
                Math::Vector4{0.85f,0.95f,1.0f,alpha});
            cut->Update();
        }
    }
}

// 裂け目が残っている間だけ、ステージの表示段階に対応する幕とパネルを描く。
void StageSelectionPresentation::DrawPortal(TitleRift* rift, SrvManager* srv,
    Object3dCommon* common, float titleElapsed, bool inScene)
{
    if (!rift || !srv || !common || !stagePortalPanels_.back() ||
        stageOverviewTime_ >= StageEntryTiming::kStagePortalClearTime) { return; }
    if (inScene != (stageOverviewTime_ >= 0.0f)) { return; }
    // 出撃後は本編と同じ描画先・深度を使う。機体の色調や縁の前後関係を途中で変えない。
    // 本編の深度はPreDrawで用意済み。ここでは描画先の変更・クリアをしない。
    if (!inScene) {
        // タイトルと同じ色・同じ切断線。面を描き直す際に位置や色を変更しない。
        rift->DrawStageCurtain(titleElapsed,stageEntrySeam_,stagePortalGap_,0.0f);
        rift->BindFlightTarget(true);
    }
    srv->PreDraw();
    common->SetBlendMode(BlendMode::Normal);
    common->SetDepthDrawMode(DepthDrawMode::Normal);
    common->CommonDrawSetting();
    for (size_t index=inScene ? 0 : 2; index<stagePortalPanels_.size(); ++index) { stagePortalPanels_[index]->Draw(); }
    if (stagePortalCutIntensity_ > 0.0f && stagePortalCut_.back()) {
        common->SetBlendMode(BlendMode::Add);
        common->SetDepthDrawMode(DepthDrawMode::ReadOnly);
        common->CommonDrawSetting();
        for (auto& cut : stagePortalCut_) { cut->Draw(); }
        common->SetBlendMode(BlendMode::Normal);
        common->SetDepthDrawMode(DepthDrawMode::Normal);
        common->CommonDrawSetting();
    }
    if (!inScene) { rift->BindFlightTarget(false); }
}

// 選択中は手前に裂け目を描き、出撃時はステージのカメラに合わせて画面をつなぐ。
void StageSelectionPresentation::DrawOverlay(GameScene* preview, TitleRift* rift, SrvManager* srv,
    Object3dCommon* common, float titleElapsed)
{
    if (!rift) { return; }
    if (stageOverviewTime_ < 0.0f) {
        DrawPortal(rift, srv, common, titleElapsed, false);
    } else if (stageOverviewTime_ < StageEntryTiming::kStagePortalClearTime) {
        if (!preview || !preview->GetStageCamera()) { return; }
        auto* camera=preview->GetStageCamera();
        const auto& frame=preview->GetStagePortalFrame();
        const auto position=camera->GetTranslate();
        const auto eye=RotateVector({position.x-frame.m[3][0],position.y-frame.m[3][1],
            position.z-frame.m[3][2]},Math::Inverse(frame));
        if (eye.z >= 22.30f) { return; }
        constexpr float unitsPerPixel=2.0f*22.4f*0.4830551f/720.0f;
        const float angle=std::atan(-stageEntrySeam_.y), gap=stagePortalGap_*unitsPerPixel;
        const float center=(360.0f-stageEntrySeam_.x-640.0f*stageEntrySeam_.y)*unitsPerPixel;
        const auto line=[&](float side) {
            const auto point=[&](float along) {
                const auto local=RotateVector({std::cos(angle)*along-std::sin(angle)*side*gap,
                    center+std::sin(angle)*along+std::cos(angle)*side*gap,22.4f},frame);
                return ProjectTitleCanvas({local.x+frame.m[3][0],local.y+frame.m[3][1],
                    local.z+frame.m[3][2]},*camera);
            };
            const auto a=point(-20.0f), b=point(20.0f);
            const float slope=(b.y-a.y)/(b.x-a.x);
            return Math::Vector2{a.y-a.x*slope,slope};
        };
        const auto upper=line(1.0f), lower=line(-1.0f);
        const Math::Vector4 edges{upper.x,upper.y,lower.x,lower.y};
        // 深度は実際の立体面を使い、色だけはタイトルと同じ合成先で統一する。
        rift->DrawStageCurtain(titleElapsed,stageEntrySeam_,stagePortalGap_,0.0f,1.0f,0.0f,0.0f,&edges);
    }
}
