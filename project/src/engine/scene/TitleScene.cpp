// ===== TitleScene.cpp =====
// タイトルとステージ選択の入力・表示を担当し、出撃のタイミングで本編へ切り替える

#include "engine/scene/TitleScene.h"
#include "app/ui/CombatHud.h"
#include "app/runtime/GameResources.h"
#include "app/ui/MenuUi.h"
#include "app/ui/StageRecords.h"
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
#include "engine/scene/TitleMotion.h"
#include "engine/scene/TitleRift.h"
#include "engine/scene/TitleWordmark.h"
#include <dinput.h>
#include <imgui.h>
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
using namespace TitleMotion;

const std::string kLogoSurface = "resources/effects/title_logo_surface.png";

struct StageDestination {
    const char* name;
    SceneType scene;
};
// 実際に遊べる行き先だけを並べる　別ステージを追加するときの選択情報はここへ集約
constexpr std::array<StageDestination, 1> kStageDestinations{{
    {"航空都市", SceneType::Game}
}};

} // namespace

TitleScene::TitleScene(Screen screen, StageRecords* records, bool automaticDeparture) :
    screen_(screen), stageRecords_(records), automaticDeparture_(automaticDeparture) {}
TitleScene::~TitleScene() = default;

// タイトルとステージ選択に共通する時刻、待機飛行、音、画面遷移の状態を初期化する
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
    char fadeMode[8]{}, reviewMode[8]{}, previewTime[32]{};
    whiteFadeEnabled_ = !(GetEnvironmentVariableA("AZRAID_TITLE_FADE", fadeMode, static_cast<DWORD>(sizeof(fadeMode))) == 1 && fadeMode[0] == '0');
    transitionReview_ = GetEnvironmentVariableA("AZRAID_TRANSITION_REVIEW", reviewMode, static_cast<DWORD>(sizeof(reviewMode))) == 1 && reviewMode[0] == '1';
    previewDepartureTime_ = -1.0f;
    const DWORD timeLength = GetEnvironmentVariableA("AZRAID_DEPARTURE_PREVIEW_TIME", previewTime, static_cast<DWORD>(sizeof(previewTime)));
    if (timeLength > 0 && timeLength < sizeof(previewTime)) {
        char* end = nullptr;
        const float time = std::strtof(previewTime, &end);
        if (end != previewTime && *end == '\0' && std::isfinite(time) && time >= 0.0f && time < kDepartureDuration) {
            previewDepartureTime_ = time;
        }
    }
    menuEmphasis_ = { 1.0f, 0.0f, 0.0f };
    showControls_ = startRequested_ = false;
    stageSelection_.Reset(screen_ == Screen::StageSelection);
    selectedStage_ = stageAction_ = 0;
    requestedScene_ = screen_ == Screen::Title ? SceneType::StageSelect : SceneType::Game;
    TextureManager::GetInstance()->LoadTexture(kLogoSurface);
    PrepareTitleComposition();
}

// タイトルで選んだ行き先を予約する　本編はステージ選択へ、練習はチュートリアルへ進む
void TitleScene::RequestStart(bool tutorial)
{
    if (screen_ != Screen::Title || startRequested_) { return; }
    requestedScene_ = tutorial ? SceneType::Tutorial : SceneType::StageSelect;
    showControls_ = false;
    startRequested_ = true;
    if (sound_) { sound_->Play("confirm"); }
}

// 選択画面で出撃を受け付けられる時だけ、選択された行き先を予約して出撃演出を始める
bool TitleScene::ConfirmStageSelection()
{
    if (screen_ != Screen::StageSelection || startRequested_ || !stageSelection_.CanConfirm()) { return false; }
    requestedScene_ = kStageDestinations[static_cast<size_t>(selectedStage_)].scene;
    stageSelection_.BeginDeparture();
    startRequested_ = true;
    if (sound_) { sound_->Play("confirm"); }
    return true;
}

// ステージ紹介へ進む前の選択画面から、タイトルへ戻るよう予約する
void TitleScene::CancelStageSelection()
{
    if (screen_ != Screen::StageSelection || stageSelection_.OverviewTime() >= 0.0f || !sceneManager_) { return; }
    sceneManager_->SetNextScene(SceneType::Title);
}

// 機体と背景の準備を進める　ステージ選択ではステージの先読みも完了してからtrueを返す
bool TitleScene::InitializeStep()
{
    PrepareBackdrop();
    if (!ship_) { return false; }
    if (screen_ != Screen::StageSelection) { UpdateBackdrop(); return true; }
    stageSelection_.PreparePortal(objectCommon_.get());
    // 隙間の街は出撃先の本編から描く　選択画面を開く前に出撃先を準備する
    if (!sceneManager_ || !sceneManager_->PrepareSceneStep(SceneType::Game)) { return false; }
    if (camera_) { stageSelection_.UpdateCamera(*camera_, dxCommon_->GetPresentationAspectRatio()); }
    if (auto* preview = GetDeparturePreview()) { preview->UpdateStagePreview(stageSelection_.Elapsed()); }
    UpdateStagePortal();
    return stageSelection_.PortalReady();
}

void TitleScene::UpdateStagePortal()
{
    stageSelection_.UpdatePortal(GetDeparturePreview(), rift_.get(), startRequested_);
}

const Math::Matrix4x4& TitleScene::GetStageProjectionMatrix() const
{
    return camera_->GetProjectionMatrix();
}

// 直前のタイトルのカメラと自機の時計を受け取り、開口部越しのプレビューを同じ演出につなぐ
void TitleScene::BeginStageReveal(const TitleScene& title)
{
    if (!camera_ || !title.camera_ || !title.ship_ ||
        !stageSelection_.CaptureEntry(*title.camera_, *camera_, title.elapsed_, title.flybyElapsed_)) { return; }
    if (auto* preview = GetDeparturePreview()) {
        preview->SetStagePortalSeam(stageSelection_.Seam());
        preview->UpdateStagePreview(stageSelection_.Elapsed());
    }
    UpdateStagePortal();
}

// ステージ選択の演出時刻を進め、カメラと先読み済みのステージ表示を更新する
void TitleScene::AdvanceStagePresentation(float delta)
{
    elapsed_ += delta;
    stageSelection_.AdvanceClock(delta);
    if (camera_) { stageSelection_.UpdateCamera(*camera_, dxCommon_->GetPresentationAspectRatio()); }
    if (auto* preview=GetDeparturePreview()) { preview->UpdateStagePreview(stageSelection_.Elapsed()); }
    UpdateStagePortal();
}

// ステージ選択の準備と表示を進め、操作を受け付けられる状態で決定・戻るを処理する
void TitleScene::UpdateStageSelection(float delta)
{
    const bool gameReady = sceneManager_ && sceneManager_->PrepareSceneStep(requestedScene_);
    if (automaticDeparture_ && !startRequested_ && gameReady) {
        stageSelection_.EnableInputNow();
        ConfirmStageSelection();
    }
    if (input_ && !automaticDeparture_ && !startRequested_ && stageSelection_.OverviewTime() < 0.0f) {
        if (MenuUi::Pressed(input_,DIK_ESCAPE)) { CancelStageSelection(); }
        else if (!startRequested_ && stageSelection_.InputReady()) {
            if (MenuUi::Pressed(input_,DIK_UP) || MenuUi::Pressed(input_,DIK_DOWN) ||
                MenuUi::Pressed(input_,DIK_W) || MenuUi::Pressed(input_,DIK_S) ||
                MenuUi::Pressed(input_,DIK_LEFT) || MenuUi::Pressed(input_,DIK_RIGHT) ||
                MenuUi::Pressed(input_,DIK_A) || MenuUi::Pressed(input_,DIK_D)) { stageAction_ = 1-stageAction_; }
            if (MenuUi::ConfirmPressed(input_)) {
                if (stageAction_ == 0) { ConfirmStageSelection(); }
                else { CancelStageSelection(); }
            }
        }
    }
    if (startRequested_ && gameReady) {
        auto* preview = GetDeparturePreview();
        const bool returning = stageSelection_.AdvanceReturn(preview, delta);
        if (!returning && stageSelection_.AdvanceOverview(delta) && sound_) { sound_->Play("launch"); }
        elapsed_ += delta;
        stageSelection_.AdvanceClock(delta);
        if (!returning && preview) {
            const float time = stageSelection_.OverviewTime();
            preview->UpdateStageOverview(time);
            preview->DrawTitleEntryHud();
            // 練習からの自動出撃でも、目標と黒帯は本編と同じ時計で描く
            preview->DrawStageBriefing(time);
        }
        UpdateStagePortal();
        if (!returning && stageSelection_.OverviewComplete()) {
            sceneManager_->SetNextScene(requestedScene_);
        }
    } else {
        AdvanceStagePresentation(delta);
    }
    DrawMenu(gameReady);
}

// 必要な機体モデルがそろった時点で、タイトルの機体、カメラ、排気の描画物を用意する
void TitleScene::PrepareBackdrop()
{
    if (ship_) { return; }
    Model* model = ModelManager::GetInstance()->FindModel(GameResources::kFreePlayerModelPath);
    if (!model || model->GetVertices().empty() || !ModelManager::GetInstance()->FindModel("effect_player_bullet_core")) { return; }
    // 自機と環境光は本編と共有し、タイトルのカメラ・姿勢は独立させる
    if (!objectCommon_) {
        camera_ = std::make_unique<Camera>();
        camera_->SetFovY(0.56f);
        camera_->SetFarClip(1000.0f);
        objectCommon_ = std::make_unique<Object3dCommon>();
        objectCommon_->BeginInitialize(dxCommon_, srvManager_);
        return;
    }
    if (!objectCommon_->InitializeStep()) { return; }
    objectCommon_->SetDefaultCamera(camera_.get());
    objectCommon_->SetEnvironmentTexturePath(GameResources::kGameEnvironmentTexturePath);

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
    // 本編と同じ塗装・材質を残し、カメラと照明でタイトル専用の見せ方を作る
    ship_->SetDirectionalLightIntensity(1.18f);
    ship_->SetEnvironmentCoefficient(0.045f);
    ship_->SetShininess(112.0f);
    ship_->SetRoughness(0.38f);
    ship_->SetMetallic(0.24f);
    ship_->SetSpecularColor({ 0.30f, 0.32f, 0.35f });
    ship_->SetShadowReceiveStrength(0.0f);

    // タイトル専用の噴流　安定したObject3d描画でノズルへ接続する
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
        sound_->Load("launch", "resources/audio/combat/gate_boost.wav", 1, 0.40f);
    }
}

void TitleScene::PrepareTitleComposition()
{
    rift_ = std::make_unique<TitleRift>();
    rift_->Initialize(dxCommon_);
}

// タイトルから続く出撃の時刻に合わせて、画面に映る機体と航跡を更新する
void TitleScene::UpdateDepartureFlight(float time)
{
    if (!camera_ || !ship_) { return; }
    const auto flyby = GetTitleFlyby(flybyElapsed_,true);
    const float fit = (std::min)(camera_->GetAspectRatio() / (1280.0f / 720.0f),1.0f);
    const auto flight = [&](float time) {
        const auto motion = GetLogoMotion(elapsed_,flyby,time);
        const float head = GetDepartureStrike(time).bladeX;
        const float x = head - kCutLead;
        const auto canvas = MoveLogoCanvas({x,TitleCutY(x,flyby.cutPattern)},motion);
        auto target = TitleViewPoint(canvas,22.0f);
        target.x *= fit; target.y *= fit;
        if (time < kDepartureCutTime) {
            const float t = Smooth(time / kDepartureCutTime);
            return Math::Vector3{
                departureOrigin_.x + (target.x - departureOrigin_.x) * t,
                departureOrigin_.y + (target.y - departureOrigin_.y) * t,
                departureOrigin_.z + (target.z - departureOrigin_.z) * t };
        }
        return target;
    };
    const auto local = flight(time);
    const auto before = flight((std::max)(0.0f,time - 0.001f));
    const auto after = flight((std::min)(kDepartureOpenTime,time + 0.001f));
    const auto direction = RotateVector({after.x - before.x,after.y - before.y,after.z - before.z},
        camera_->GetWorldMatrix());
    const auto rotation = FlightRotation(direction,RotateVector({0,1,0},camera_->GetWorldMatrix()),
        -0.30f * std::sin(Smooth(time / kDepartureCutTime) * 3.1415927f));
    const float scale = departureOriginScale_ +
        (kIdleShipScale * fit - departureOriginScale_) * Smooth(time / kDepartureCutTime);
    const auto offset = RotateVector(modelCenter_,Math::MakeAffineMatrix({scale,scale,scale},rotation,{}));
    const auto world = RotateVector(local,camera_->GetWorldMatrix());
    const auto eye = camera_->GetTranslate();
    const Math::Vector3 center{world.x + eye.x,world.y + eye.y,world.z + eye.z};
    ship_->SetScale({scale,scale,scale});
    ship_->SetRotate(rotation);
    ship_->SetTranslate({center.x - offset.x,center.y - offset.y,center.z - offset.z});
    const float fade = flightOpacity_ + (1.0f - flightOpacity_) * Smooth(time / 0.18f);
    ship_->SetColor({0.90f,0.94f,1.0f,fade});
    ship_->Update();
    UpdateFlightEffects(center,Math::MakeAffineMatrix({1,1,1},rotation,{}),
        1.6f + Smooth(time / kDepartureCutTime) * 3.0f,scale,fade);
    wakePoints_.fill({});
}

// 遷移中は出撃の動き、待機中はタイトルの周回飛行に合わせて機体とカメラを更新する
void TitleScene::UpdateBackdrop()
{
    if (!camera_ || !ship_) { return; }
    if (rift_) { rift_->PrepareFlightDepth(); }
    if (departureTime_ > 0.0f) {
        if (departureTime_ <= kDepartureOpenTime) { UpdateDepartureFlight(departureTime_); }
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
        // 過去の実飛行位置・ノズル姿勢を再評価し、通った道だけに航跡を残す
        // 遠方では左右が一点に収まるため、同じ固定領域を1本12点の滑らかな周回へ使う
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
                const auto nozzleOffset = orbitalWake ? RotateVector({0,-0.28f*pastScale/1.26f,-1.70f*pastScale/1.26f},pastMatrix) :
                    TitleNozzleOffset(nozzle,pastScale,pastMatrix);
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

// 機体の位置と姿勢に合わせ、排気の向き・長さ・色と飛行エフェクトを更新する
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
        // 本編で確認済みのノズル位置をタイトルの表示倍率へ換算する
        const float ratio = shipScale / 1.26f;
        const auto nozzle = TitleNozzleOffset(index%2,shipScale,rotationMatrix);
        if (index < engines_.size()) {
            const Math::Vector3 position{ center.x + nozzle.x, center.y + nozzle.y, center.z + nozzle.z };
            const auto head = ProjectTitleCanvas(position, *camera_);
            const auto end = ProjectTitleCanvas({ position.x + tail.x * 8.0f,
                position.y + tail.y * 8.0f, position.z + tail.z * 8.0f }, *camera_);
            const float length = (std::max)(0.001f, std::hypot(end.x - head.x, end.y - head.y));
            engines_[index] = { head.x, head.y, (end.x - head.x) / length, (end.y - head.y) / length };
        }
        const float length = (layer == 0 ? 1.75f : 1.48f) * (0.70f + thrust * 0.45f) * pulse * shipScale / kIdleShipScale;
        float visibleLength=length;
        float plumeFade=1.0f;
        auto plumeAxis=tail;
        auto plumeRotate=billboard;
        if (screen_ == Screen::StageSelection) {
            const Math::Vector3 position{center.x+nozzle.x,center.y+nozzle.y,center.z+nozzle.z};
            const auto head=ProjectTitleCanvas(position,*camera_);
            const auto end=ProjectTitleCanvas({position.x+tail.x*length,position.y+tail.y*length,position.z+tail.z*length},*camera_);
            const float pixels=std::hypot(end.x-head.x,end.y-head.y);
            const auto eye=camera_->GetTranslate();
            const auto view=RotateVector({position.x-eye.x,position.y-eye.y,position.z-eye.z},camera_->GetViewMatrix());
            const float fit=(std::min)(camera_->GetAspectRatio()/(1280.0f/720.0f),1.0f);
            visibleLength=pixels*(std::max)(view.z,0.001f)*fit/(360.0f*camera_->GetProjectionMatrix().m[1][1]);
            const auto axis=pixels > 0.001f ? Math::Vector3{(end.x-head.x)/pixels,-(end.y-head.y)/pixels,0} : Math::Vector3{0,1,0};
            plumeAxis=RotateVector(axis,camera_->GetWorldMatrix());
            plumeRotate=FlightRotation(RotateVector({0,0,1},camera_->GetWorldMatrix()),plumeAxis,0.0f);
            if (layer != 2) { plumeFade=Smooth(visibleLength/(length*0.15f)); }
        }
        // UVの根元は下端から2%　板の向きと同じ軸で移し、ノズルへ固定する
        const float centerOffset=layer == 2 ? (screen_ == Screen::StageSelection ? 0.0f : 0.025f) : visibleLength*0.48f;
        auto& effect = exhaust_[index];
        effect->SetTranslate({ center.x + nozzle.x + plumeAxis.x * centerOffset,
            center.y + nozzle.y + plumeAxis.y * centerOffset, center.z + nozzle.z + plumeAxis.z * centerOffset });
        effect->SetRotate(plumeRotate);
        if (layer == 2) {
            effect->SetScale({ 0.34f * ratio * pulse, 0.34f * ratio * pulse, 1.0f });
        } else {
            effect->SetScale({ (layer == 0 ? 0.55f : 0.27f) * ratio * pulse * (0.65f + thrust * 0.10f), (std::max)(visibleLength,0.001f), 1.0f });
        }
        effect->SetColor(TitleExhaustColor(layer,fade*plumeFade));
        effect->Update();
#ifdef _DEBUG
        if (screen_ == Screen::StageSelection && layer != 2) {
            const auto& matrix=effect->GetWorldMatrix();
            const auto root=RotateVector({0,-0.48f,0},matrix);
            const auto rootCanvas=ProjectTitleCanvas({root.x+matrix.m[3][0],root.y+matrix.m[3][1],root.z+matrix.m[3][2]},*camera_);
            const auto nozzleCanvas=ProjectTitleCanvas({center.x+nozzle.x,center.y+nozzle.y,center.z+nozzle.z},*camera_);
            assert(std::hypot(rootCanvas.x-nozzleCanvas.x,rootCanvas.y-nozzleCanvas.y) < 0.25f);
        }
#endif
    }
}

// タイトルの飛行、ロードの状態、決定・戻るの入力をフレームごとに処理する
void TitleScene::Update()
{
    if (transitionReview_) {
        if ((!startRequested_ || previewDepartureTime_ >= 0.0f) && ImGui::IsKeyPressed(ImGuiKey_F9, false)) { whiteFadeEnabled_ = !whiteFadeEnabled_; }
        ImGui::SetNextWindowPos({ 18, 64 }, ImGuiCond_FirstUseEver);
        ImGui::Begin("Transition review", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::BeginDisabled(startRequested_ && previewDepartureTime_ < 0.0f);
        ImGui::Checkbox("White fade / F9", &whiteFadeEnabled_);
        ImGui::EndDisabled();
        ImGui::End();
    }
    const float delta = (dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 0.05f) :
        1.0f / 60.0f) * presentationRate_;
    if (screen_ == Screen::StageSelection) { UpdateStageSelection(delta); return; }
    elapsed_ += delta;
    if (input_ && !startRequested_ && !MenuUi::InputBlocked) {
        if (showControls_) {
            if (MenuUi::Pressed(input_, DIK_ESCAPE) || MenuUi::Pressed(input_, DIK_H) ||
                MenuUi::ConfirmPressed(input_)) { showControls_ = false; }
        } else {
            if (MenuUi::Pressed(input_, DIK_UP) || MenuUi::Pressed(input_, DIK_W) || MenuUi::Pressed(input_, DIK_LEFT) || MenuUi::Pressed(input_, DIK_A)) { selectedItem_ = (selectedItem_ + 2) % 3; }
            if (MenuUi::Pressed(input_, DIK_DOWN) || MenuUi::Pressed(input_, DIK_S) || MenuUi::Pressed(input_, DIK_RIGHT) || MenuUi::Pressed(input_, DIK_D)) { selectedItem_ = (selectedItem_ + 1) % 3; }
            if (MenuUi::ConfirmPressed(input_)) {
                if (selectedItem_ == 2) { showControls_ = true; }
                else { RequestStart(selectedItem_ == 1); }
            } else if (input_->TriggerKey(DIK_T)) { RequestStart(true); }
            else if (input_->TriggerKey(DIK_H)) { showControls_ = true; }
        }
    }
    // 導入後にモデルを準備　登場の時計は準備後から進め、読み込み時間で姿を飛ばさない
    const bool resourcesReady = elapsed_ >= 0.10f && GameScene::PreloadResourcesStep(dxCommon_, srvManager_);
    PrepareBackdrop();
    if (resourcesReady && sceneManager_ && !sceneManager_->IsScenePrepared(requestedScene_)) {
        sceneManager_->PrepareSceneStep(requestedScene_);
    }
    if (resourcesReady && sceneManager_ && requestedScene_ == SceneType::StageSelect &&
        sceneManager_->IsScenePrepared(SceneType::StageSelect) && !sceneManager_->IsScenePrepared(SceneType::Game)) {
        sceneManager_->PrepareSceneStep(SceneType::Game);
    }
    const bool gameReady = sceneManager_ && sceneManager_->IsScenePrepared(requestedScene_) &&
        (requestedScene_ != SceneType::StageSelect || sceneManager_->IsScenePrepared(SceneType::Game));
    if (ship_ && departureTime_ <= 0.0f && !showControls_) {
        arrivalElapsed_ = (std::min)(kArrivalDuration,arrivalElapsed_ + delta);
        if (arrivalElapsed_ >= kArrivalDuration) {
            flybyElapsed_ = previewFlybyPhase_ >= 0.0f ? previewFlybyPhase_ :
                flybyElapsed_ + delta;
        }
    }
    if (startRequested_ && gameReady && sceneManager_) {
        if (departureTime_ == 0.0f) {
            if (sound_) { sound_->Play("launch"); }
            if (ship_ && camera_) {
                const auto offset = RotateVector(modelCenter_,
                    Math::MakeAffineMatrix(ship_->GetScale(),ship_->GetRotate(),{}));
                const auto position = ship_->GetTranslate();
                const auto eye = camera_->GetTranslate();
                departureOrigin_ = RotateVector({position.x + offset.x - eye.x,
                    position.y + offset.y - eye.y,position.z + offset.z - eye.z},camera_->GetViewMatrix());
                departureOriginScale_ = ship_->GetScale().x;
            }
        }
        const float previousDeparture = departureTime_;
        departureTime_ = (std::min)(kDepartureDuration, departureTime_ + delta);
        if (previewDepartureTime_ >= 0.0f) { departureTime_ = previewDepartureTime_; }
        if (auto* preview = requestedScene_ != SceneType::StageSelect ? GetDeparturePreview() : nullptr) {
            if (previousDeparture <= kDepartureOpenTime && ship_ && camera_) {
                // 最後の切断姿勢を正確に評価　表示サイズとカメラ内の位置を本編へ渡す
                UpdateDepartureFlight(kDepartureOpenTime);
                const auto offset = RotateVector(modelCenter_,
                    Math::MakeAffineMatrix(ship_->GetScale(),ship_->GetRotate(),{}));
                const auto position = ship_->GetTranslate();
                const auto eye = camera_->GetTranslate();
                const auto viewPosition = RotateVector({position.x + offset.x - eye.x,
                    position.y + offset.y - eye.y,position.z + offset.z - eye.z},camera_->GetViewMatrix());
                preview->BeginTitleEntry(viewPosition,ship_->GetRotate(),camera_->GetRotate(),ship_->GetScale().x);
            }
            preview->UpdateTitleEntry(departureTime_);
        }
        if (auto* selection = GetStageReveal()) {
            if (previousDeparture <= kDepartureOpenTime) { UpdateDepartureFlight(kDepartureOpenTime); selection->BeginStageReveal(*this); }
            // 面が開く間も機体は飛び続ける　開口幅は引き継ぎの完了まで保持する
            selection->AdvanceStagePresentation(delta);
        }
        if (departureTime_ >= kDepartureDuration) { sceneManager_->SetNextScene(requestedScene_); }
    }
    UpdateBackdrop();
    DrawMenu(gameReady);
    if (auto* preview = requestedScene_ != SceneType::StageSelect ? GetDeparturePreview() : nullptr) {
        preview->DrawTitleEntryHud();
    }
}

// 基準画面の比率を保ってロゴとメニューを配置し、遷移と読込の進み具合を表示に反映する
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

    if (screen_ == Screen::StageSelection) {
        DrawStageSelection(draw,origin,scale,gameReady);
    } else if (!showControls_) {
        const float exit = 1.0f - Smooth(departureTime_ / 0.20f);
        if (auto* selection = GetStageReveal()) {
            selection->DrawStageSelection(draw,origin,scale,false,Smooth((departureTime_ - 1.25f)/0.45f));
        }
        const auto surface = srvManager_->GetGPUDescriptorHandle(TextureManager::GetInstance()->GetSrvIndex(kLogoSurface));
        DrawTitleWordmark(draw, p(195, 247), 1.75f * scale, elapsed_,
            GetTitleFlyby(flybyElapsed_, ship_ != nullptr), departureTime_,static_cast<ImTextureID>(surface.ptr));
        const float menuAlpha = Smooth((elapsed_ - 1.35f) / 0.45f) * exit;
        const int firstMenuVertex = draw->VtxBuffer.Size;
        constexpr std::array<float, 3> positions{ 277.0f, 541.0f, 805.0f };
        const char* labels[] = { "スタート", "チュートリアル", "操作方法" };
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

// ステージ選択の項目を描き、出撃演出の開始に合わせて表示を引っ込める
void TitleScene::DrawStageSelection(ImDrawList* draw, ImVec2 origin, float scale, bool gameReady, float reveal)
{
    if (automaticDeparture_) { return; }
    const float entrance = Smooth((stageSelection_.Elapsed()-0.10f)/0.35f)*reveal;
    const float overviewExit = stageSelection_.ReturnTime() >= 0.0f ? 1.0f-Smooth(stageSelection_.ReturnTime()/0.18f) : 1.0f;
    const auto p = [&](float x, float y) { return ImVec2{origin.x + x*scale,origin.y + y*scale}; };
    const auto opacity = [&](int first, float alpha) {
        for (int index=first;index<draw->VtxBuffer.Size;++index) {
            auto& color=draw->VtxBuffer[index].col;
            const auto value=static_cast<ImU32>(static_cast<float>((color>>IM_COL32_A_SHIFT)&255)*alpha);
            color=(color&~IM_COL32_A_MASK)|(value<<IM_COL32_A_SHIFT);
        }
    };
    const int first = draw->VtxBuffer.Size;
    const auto& stage = kStageDestinations[static_cast<size_t>(selectedStage_)];
    // ステージ名は切断面に残す　都市は斜めの開口部の奥にあり、メニューの背景と分ける
    MenuUi::Heading(draw,p(86,72),108.0f*scale,IM_COL32(5,9,17,180),stage.name);
    MenuUi::Heading(draw,p(84,68),108.0f*scale,MenuUi::Paper,stage.name);
    for (int index=first;index<draw->VtxBuffer.Size;++index) {
        auto& position=draw->VtxBuffer[index].pos;
        position.x-=(position.y-p(0,68).y)*0.09f;
    }
    MenuUi::Heading(draw, p(88, 206), 26 * scale, MenuUi::Paper, "目標：敵編隊を突破し、ボスを撃破");
    if (stageRecords_) {
        const auto& record = stageRecords_->Get();
        char text[128]{};
        if (record.clearCount > 0) {
            std::snprintf(text, sizeof(text), "クリア済み   ベスト %06d   最高ランク %c", record.bestScore, record.bestRank);
        } else { std::snprintf(text, sizeof(text), "未クリア"); }
        MenuUi::Text(draw, p(88, 249), 22 * scale, IM_COL32(187, 199, 212, 255), text);
    }
    char rankTarget[96]{};
    std::snprintf(rankTarget, sizeof(rankTarget), "ランク目標   A %d点 / S %d点", StageRecords::kAScore, StageRecords::kSScore);
    MenuUi::Text(draw, p(88, 286), 22 * scale, IM_COL32(187, 199, 212, 255), rankTarget);
    const bool currentPage = sceneManager_ && sceneManager_->GetCurrentScene() == this;
    const bool canChoose = currentPage && !startRequested_ && stageSelection_.InputReady() && reveal >= 0.99f;
    const auto action=[&](const char* id, const char* label, int index, float x, float width) {
        ImGui::SetCursorScreenPos(p(x,576));
        ImGui::BeginDisabled(!canChoose);
        const bool clicked=ImGui::InvisibleButton(id,{width*scale,76*scale});
        MenuUi::SelectHovered(stageAction_,index);
        ImGui::EndDisabled();
        const bool selected=stageAction_ == index;
        const float size=(selected ? 44.0f : 34.0f)*scale;
        const float left=p(x+width*0.5f,0).x-MenuUi::Width(label,size,true)*0.5f;
        const ImVec2 at{ left, p(0,590).y };
        MenuUi::Heading(draw,{at.x+2*scale,at.y+2*scale},size,IM_COL32(5,9,17,200),label);
        MenuUi::Heading(draw,at,size,selected ? MenuUi::Paper : IM_COL32(132,144,161,255),label);
        return clicked;
    };
    const bool launch=action("##StageDeparture",startRequested_ && !gameReady ? "準備中" : "出撃",0,830,190);
    const bool back=action("##StageBack","戻る",1,1060,160);
    if (launch) { ConfirmStageSelection(); }
    else if (back) { CancelStageSelection(); }
    opacity(first,entrance * overviewExit);
}

// タイトル背景、機体、ステージ選択の開口部を画面の状態に合わせて合成する
void TitleScene::Draw()
{
    GameScene* preview = GetDeparturePreview();
    // 選択中から同じ本編を描き、決定後も街とカメラを引き継ぐ
    if (preview) {
        // 不透明な入口を先に置き、自機・噴射・街は通常の深度で重ねる
        stageSelection_.DrawPortal(rift_.get(), srvManager_, objectCommon_.get(), elapsed_, true);
        preview->Draw();
        return;
    }
    if (auto* selection = GetStageReveal()) { selection->Draw(); return; }
    TitleFlyby flyby = GetTitleFlyby(flybyElapsed_, ship_ && !showControls_);
    if (screen_ == Screen::StageSelection) { flyby.energy=0.55f; flyby.phase=0.0f; }
    flyby.energy *= Smooth(arrivalElapsed_ / 0.18f) * (1.0f - Smooth(departureTime_ / 0.20f));
    if (screen_ == Screen::StageSelection) { flyby.energy=0.55f*flightOpacity_; }
    if (rift_) {
        const auto mode = screen_ == Screen::StageSelection ?
            TitleRift::DrawMode::StageBackground : TitleRift::DrawMode::Background;
        rift_->Draw(elapsed_, departureTime_, flyby, engines_, wakePoints_, mode);
    }
    if (screen_ == Screen::StageSelection && objectCommon_) {
        const Math::Vector2 focus{ kStageDepartureFocus.x / 1280.0f, kStageDepartureFocus.y / 720.0f };
        dxCommon_->SetFlightBlur(0.0f, focus, focus, focus, dxCommon_->GetPresentationAspectRatio());
        dxCommon_->SetCameraMotionBlur(Math::MakeIdentity4x4(),0.0f,1);
        objectCommon_->SetDepthDrawMode(DepthDrawMode::Normal);
        objectCommon_->CommonDrawSetting();
    }
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
    if (rift_ && screen_ != Screen::StageSelection && flyby.energy > 0.001f) {
        rift_->Draw(elapsed_, departureTime_, flyby, engines_, wakePoints_, TitleRift::DrawMode::Wake);
    }
}

// 機体を裂け目より手前に重ねるか、タイトルとステージ選択それぞれの飛行段階から判定する
bool TitleScene::IsForegroundFlight() const
{
    if (auto* selection=GetStageReveal()) { return selection->IsForegroundFlight(); }
    if (screen_ == Screen::StageSelection) {
        if (auto* preview=GetDeparturePreview()) { return preview->IsTitleEntryForeground(); }
        return false;
    }
    if (departureTime_ > 0.0f) {
        if (auto* preview = GetDeparturePreview()) { return preview->IsTitleEntryForeground(); }
        return ship_ && departureTime_ <= kDepartureOpenTime;
    }
    const auto flyby = GetTitleFlyby(flybyElapsed_,true);
    const bool passing = flyby.pattern == 2 && flyby.phase >= kClosePassStart && flyby.phase < 1.60f;
    const bool cutting = flyby.phase >= kChargeStart && flyby.phase < kFlybyStart + kFlybyDuration + 0.18f;
    return ship_ && arrivalElapsed_ >= kArrivalDuration && departureTime_ <= kDepartureOpenTime && !showControls_ &&
        (passing || cutting);
}

// 手前に出る飛行区間で機体と排気を専用深度に描き、裂け目との重なりを作る
void TitleScene::DrawFlightOverlay()
{
    if (auto* selection=GetStageReveal()) { selection->DrawFlightOverlay(); return; }
    if (!rift_ || !IsForegroundFlight()) { return; }
    rift_->BindFlightTarget(true);
    srvManager_->PreDraw();
    if (auto* preview = GetDeparturePreview()) {
        preview->DrawTitleEntryFlight();
        rift_->BindFlightTarget(false);
        return;
    }
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
    if (screen_ == Screen::StageSelection) {
        TitleFlyby wake{};
        wake.energy=0.55f*flightOpacity_;
        rift_->Draw(elapsed_, 0.0f, wake, engines_, wakePoints_, TitleRift::DrawMode::StageWake);
    }
    rift_->BindFlightTarget(false);
}

// 裂け目越しに表示できる段階なら、先読みしたゲームシーンを取得する
GameScene* TitleScene::GetDeparturePreview() const
{
    if (auto* selection=GetStageReveal()) { return selection->GetDeparturePreview(); }
    if (!sceneManager_ || (screen_ != Screen::StageSelection && departureTime_ <= kDepartureOpenTime)) { return nullptr; }
    return dynamic_cast<GameScene*>(sceneManager_->GetPreparedScene(requestedScene_));
}

// タイトルからステージ選択が見える段階になったら、先読み済みの選択シーンを取得する
TitleScene* TitleScene::GetStageReveal() const
{
    if (screen_ != Screen::Title || requestedScene_ != SceneType::StageSelect ||
        departureTime_ <= kDepartureOpenTime || !sceneManager_) { return nullptr; }
    return dynamic_cast<TitleScene*>(sceneManager_->GetPreparedScene(SceneType::StageSelect));
}

// 切断や出撃の前景を描く　ステージ選択側の演出はStageSelectionPresentationへ渡す
void TitleScene::DrawDepartureOverlay()
{
    if (!rift_) { return; }
    if (screen_ == Screen::StageSelection) {
        stageSelection_.DrawOverlay(GetDeparturePreview(), rift_.get(), srvManager_, objectCommon_.get(), elapsed_);
        return;
    }
    auto* selection=GetStageReveal();
    if (!GetDeparturePreview() && !selection) { return; }
    TitleFlyby flyby = GetTitleFlyby(flybyElapsed_, ship_ != nullptr);
    flyby.energy = 0.0f;
    // 本編だけに色補正・輪郭を適用し、タイトルの面へ奥の深度を写し込まない
    rift_->Draw(elapsed_, departureTime_, flyby, engines_, wakePoints_, TitleRift::DrawMode::Reveal,
        selection ? &selection->stageSelection_.Seam() : nullptr);
}

// タイトル出発時に白フェードを使う設定の場合だけ、フェードの進行に合わせて画面を重ねる
void TitleScene::DrawTransitionFade()
{
    if (!rift_) { return; }
    if (screen_ == Screen::StageSelection) { return; }
    if (!whiteFadeEnabled_ || DepartureFade(departureTime_) <= 0.001f) { return; }
    // ロゴ・自機・HUDより最後に合成し、画面全体を白へフェードさせてから開く
    const TitleFlyby noWake{};
    rift_->Draw(elapsed_, departureTime_, noWake, engines_, wakePoints_, TitleRift::DrawMode::Fade);
}

// 機体と演出のリソースを破棄し、使っていたシャドウマップのディスクリプタを返却する
void TitleScene::Finalize()
{
    // フレーム終端のGPU完了待ち後にシーンが切り替わる　共有モデル・空は解放しない
    ship_.reset();
    stageSelection_.Clear();
    rift_.reset();
    for (auto& effect : exhaust_) { effect.reset(); }
    sound_.reset();
    if (objectCommon_ && srvManager_ && objectCommon_->GetShadowMapSrvIndex() != UINT32_MAX) { srvManager_->Free(objectCommon_->GetShadowMapSrvIndex()); }
    objectCommon_.reset();
    camera_.reset();
}
