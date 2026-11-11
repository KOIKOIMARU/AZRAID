#include "app/player/PlayerFlightEffects.h"
#include "app/runtime/GameRuntimeMath.h"
#include "app/player/JustDodgeFeedback.h"
#include "app/combat/Bullet.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Object3dCommon.h"
#include <algorithm>
#include <cassert>
#include <cmath>

namespace {
using namespace GameRuntimeMath;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kContactShadowHalfPi = 1.57079632679f;
constexpr float kPlayerExhaustRiseResponse = 0.24f;
constexpr float kPlayerExhaustFallResponse = 0.075f;
constexpr float kAfterimageDuration = 22.0f;
constexpr float kHullAfterimageDuration = 15.0f;
// Omenの機体中心から右ノズルまでの座標。左はXを反転。ワールド換算済み。
constexpr Math::Vector3 kPlayerExhaustNozzle{ 0.48f, -0.28f, -1.70f };
}

// 描画に必要な共通機能、カメラ、機体モデルを受け取る。
void PlayerFlightEffects::Initialize(Object3dCommon* common, Camera* camera, const Assets& assets)
{
    Clear();
    common_ = common;
    camera_ = camera;
    assets_ = assets;
}

// 噴射やチャージの表示状態と、前の飛行で残った残像の寿命を戻す。
void PlayerFlightEffects::Reset()
{
    for (auto& afterimage : afterimages_) { afterimage.active = false; afterimage.age = 0.0f; }
    for (auto& plume : plumes_) { plume.visible = false; }
    nextAfterimage_ = 0;
    afterimageTimer_ = 0;
    wasDodging_ = chargeVisible_ = false;
    thrust_ = 0.0f;
}

// 描画の枠と素材への参照を解放し、シーンの終了に備える。
void PlayerFlightEffects::Clear()
{
    Reset();
    afterimages_ = {};
    plumes_ = {};
    chargeLights_ = {};
    assets_ = {};
    camera_ = nullptr;
    common_ = nullptr;
}

// 機体モデルを使う残像の描画枠を用意し、通常描画と違う材質に設定する。
void PlayerFlightEffects::PrepareAfterimages()
{
    if (!common_) { return; }
    Model* model = assets_.trail ? assets_.trail : assets_.glow;
    for (auto& afterimage : afterimages_) {
        afterimage = {};
        afterimage.object = std::make_unique<Object3d>();
        afterimage.object->Initialize(common_);
        if (model) { afterimage.object->SetModel(model); }
        afterimage.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        afterimage.object->SetScale({ 0.01f, 0.01f, 1.0f });
        afterimage.object->SetColor({ 0.40f, 0.96f, 1.0f, 0.0f });
        afterimage.object->SetLightingMode(0);
        afterimage.object->SetEnvironmentCoefficient(0.0f);
        afterimage.object->Update();
    }
}

// 噴射を構成する複数の描画部品を事前に用意し、飛行中に使い回す。
void PlayerFlightEffects::PrepareExhaust()
{
    if (!common_ || !assets_.glow) { return; }
    Model* trail = assets_.enginePlume ? assets_.enginePlume : assets_.glow;
    Model* core = assets_.core ? assets_.core : assets_.glow;
    constexpr std::array<Math::Vector4, 3> colors{{
        { 0.08f, 0.48f, 1.0f, 0.46f }, { 0.55f, 0.85f, 1.0f, 0.66f }, { 0.65f, 0.90f, 1.0f, 0.70f }
    }};
    constexpr std::array<float, 6> phases{ 0.0f, 1.8f, 3.6f, 5.4f, 2.7f, 4.5f };
    // 左右それぞれに外炎・内炎・ノズルの光。使われていなかった位置や回転の設定は持たない。
    for (size_t index = 0; index < plumes_.size(); ++index) {
        auto& plume = plumes_[index];
        plume = {};
        plume.model = index < 4 ? trail : core;
        plume.color = colors[index / 2];
        plume.pulseOffset = phases[index];
        plume.object = std::make_unique<Object3d>();
        plume.object->Initialize(common_);
        plume.object->SetModel(plume.model);
        plume.object->SetLightingMode(0);
        plume.object->SetEnvironmentCoefficient(0.0f);
        plume.object->SetAlphaReference(0.01f);
        plume.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        plume.object->SetScale({ 0.01f, 0.01f, 1.0f });
        plume.object->SetColor({ plume.color.x, plume.color.y, plume.color.z, 0.0f });
        plume.object->Update();
    }
}

// チャージ中の発光を描くオブジェクトを事前に用意する。
void PlayerFlightEffects::PrepareCharge()
{
    if (!common_ || !assets_.glow) { return; }
    const std::array<Model*, 2> models{ assets_.chargeArc, assets_.chargeStar };
    for (size_t index = 0; index < chargeLights_.size(); ++index) {
        auto& visual = chargeLights_[index];
        visual = std::make_unique<Object3d>();
        visual->Initialize(common_);
        visual->SetModel(models[index] ? models[index] : assets_.glow);
        visual->SetLightingMode(0);
        visual->SetEnvironmentCoefficient(0.0f);
        visual->SetShadowReceiveStrength(0.0f);
        visual->SetTranslate({ 0, -1000, -1000 });
        visual->SetScale({ 0.01f, 0.01f, 1 });
        visual->SetColor({ 1, 0.65f, 0.12f, 0 });
        visual->Update();
    }
}

// 現在の機体姿勢を残像用の空き枠へ写し、短い寿命を設定する。
void PlayerFlightEffects::SpawnAfterimage(Math::Vector3 position, int direction)
{
    if (!(assets_.trail || assets_.glow)) { return; }
    auto& afterimage = afterimages_[nextAfterimage_];
    if (!afterimage.object) { return; }
    afterimage.position = { position.x, position.y + 0.06f, position.z + 0.06f };
    afterimage.direction = direction >= 0 ? 1 : -1;
    afterimage.age = 0.0f;
    afterimage.precise = false;
    afterimage.active = true;
    nextAfterimage_ = (nextAfterimage_ + 1) % afterimages_.size();
}

void PlayerFlightEffects::FlashDodgeTrail(Math::Vector3 position, int direction)
{
    SpawnAfterimage(position, direction);
    afterimageTimer_ = 0;
}

// 自機の見た目を写した残像を発生させ、表示の種類に応じて色と寿命を決める。
void PlayerFlightEffects::SpawnHullAfterimage(const DodgeFrame& frame)
{
    if (!assets_.hull) { SpawnAfterimage(frame.position, frame.direction); return; }
    auto& afterimage = afterimages_[nextAfterimage_];
    if (!afterimage.object) { return; }
    afterimage.position = frame.visualPosition;
    afterimage.rotation = frame.rotation;
    afterimage.scale = frame.scale;
    afterimage.direction = frame.direction >= 0 ? 1 : -1;
    afterimage.precise = afterimage.active = true;
    afterimage.age = 0.0f;
    nextAfterimage_ = (nextAfterimage_ + 1) % afterimages_.size();
}

void PlayerFlightEffects::FlashJustDodge(const DodgeFrame& frame)
{
    SpawnHullAfterimage(frame);
    afterimageTimer_ = 3;
}

// 新しい残像の発生と、すでに出ている残像の色や消える時刻を更新する。
bool PlayerFlightEffects::UpdateAfterimages(const DodgeFrame& frame)
{
    const float step = std::clamp(frame.deltaSeconds * 60.0f, 0.5f, 4.0f);
    const bool dodging = frame.alive && frame.dodging;
    const bool justDodge = frame.alive && frame.justDodge;
    const bool began = dodging && !wasDodging_;
    if (afterimageTimer_ > 0) { --afterimageTimer_; }
    if ((dodging && (began || afterimageTimer_ <= 0)) || (justDodge && afterimageTimer_ <= 0)) {
        if (justDodge) { SpawnHullAfterimage(frame); }
        else { SpawnAfterimage(frame.position, frame.direction); }
        afterimageTimer_ = justDodge ? 3 : 2;
    }
    wasDodging_ = dodging;
    for (auto& afterimage : afterimages_) {
        if (!afterimage.active) { continue; }
        afterimage.age += step;
        const float duration = afterimage.precise ? kHullAfterimageDuration : kAfterimageDuration;
        if (afterimage.age >= duration) { afterimage.active = false; }
    }
    return began;
}

// 自機の姿勢と速度に合わせて、噴射の向き・強さとチャージの光を更新する。
void PlayerFlightEffects::UpdateFlight(const FlightFrame& frame)
{
    if (!common_ || !camera_) {
        return;
    }

    bool hasAuraObject = false;
    for (const Plume& aura : plumes_) {
        if (aura.object && aura.model) {
            hasAuraObject = true;
            break;
        }
    }
    if (!hasAuraObject) {
        return;
    }

    const Math::Vector3 playerPosition = frame.position;
    const Math::Vector3 playerRotate = frame.rotation;
    const Math::Vector3 cameraRotate = camera_->GetRotate();
    const float speedRate = frame.speed;
    const float accelerationRate = frame.acceleration;
    const float feverThrustBoost = frame.fever;
    const float boostKick = frame.boost;
    const float dodgeBoost = frame.dodging ? 1.0f : 0.0f;
    const auto dodgePulse = JustDodgeFeedback::Evaluate(frame.justDodge);
    const float justDodgeBoost = dodgePulse.contact * 0.30f + dodgePulse.release * 1.5f;
    const float targetThrust = std::clamp(
        0.10f +
            speedRate * 0.42f +
            accelerationRate * 0.20f +
            dodgeBoost * 0.82f +
            justDodgeBoost * 0.34f +
            frame.releaseKick * 0.35f +
            boostKick * 0.55f +
            feverThrustBoost * 0.95f,
        0.0f,
        1.75f);
    const float effectFrameStep = std::clamp(frame.deltaSeconds * 60.0f, 0.25f, 3.0f);
    const float thrustResponse =
        targetThrust > thrust_ ?
        kPlayerExhaustRiseResponse :
        kPlayerExhaustFallResponse;
    const float thrustBlend =
        1.0f - std::pow(1.0f - thrustResponse, effectFrameStep);
    thrust_ +=
        (targetThrust - thrust_) * thrustBlend;
    // 球状の粒を連ねず、ノズルから後方へ伸びる炎と根元の発光だけを描く。
    const auto tail = RotateLocalOffset({0,0,-1},playerRotate);
    const auto& view = camera_->GetViewMatrix();
    const auto& cameraWorld = camera_->GetWorldMatrix();

    for (size_t auraIndex = 0; auraIndex < plumes_.size(); ++auraIndex) {
        Plume& aura = plumes_[auraIndex];
        aura.visible = false;
        if (!aura.object || !aura.model) {
            continue;
        }

        const float pulse = 1.0f+std::sin(frame.clockFrames*0.29f+aura.pulseOffset)*0.025f;
        const bool isOuterFlame = auraIndex < 2;
        const bool isInnerFlame = auraIndex >= 2 && auraIndex < 4;
        const Math::Vector3 localOffset{(auraIndex%2 == 0 ? -1.0f : 1.0f)*kPlayerExhaustNozzle.x,
            kPlayerExhaustNozzle.y,kPlayerExhaustNozzle.z};
        const Math::Vector3 rotatedOffset = RotateLocalOffset(localOffset, playerRotate);
        Math::Vector3 translate{
            playerPosition.x + rotatedOffset.x,
            playerPosition.y + rotatedOffset.y,
            playerPosition.z + rotatedOffset.z
        };
        Math::Vector3 rotate = cameraRotate;
        Math::Vector3 scale{0.24f*pulse,0.24f*pulse,1.0f};
        float plumeOpacity = 1.0f;
        if (isOuterFlame || isInnerFlame) {
            const float length = (isOuterFlame ? 1.45f : 0.95f)*
                (1.0f+thrust_*0.65f+feverThrustBoost*0.30f+boostKick*0.22f+frame.gateBoost*0.65f)*pulse;
            const auto head = TransformCoord(translate,view);
            const auto end = TransformCoord({translate.x+tail.x*length,
                translate.y+tail.y*length,translate.z+tail.z*length},view);
            if (head.z <= 0.10f || end.z <= 0.10f) { continue; }
            const float dx=end.x/end.z-head.x/head.z, dy=end.y/end.z-head.y/head.z;
            const float projected=std::hypot(dx,dy);
            const float visibleLength=projected*head.z;
            const float x=projected > 0.00001f ? dx/projected : 0.0f;
            const float y=projected > 0.00001f ? dy/projected : 1.0f;
            const Math::Vector3 axis{x*cameraWorld.m[0][0]+y*cameraWorld.m[1][0],
                x*cameraWorld.m[0][1]+y*cameraWorld.m[1][1],x*cameraWorld.m[0][2]+y*cameraWorld.m[1][2]};
            rotate=StageFlightRotation({cameraWorld.m[2][0],cameraWorld.m[2][1],cameraWorld.m[2][2]},axis,0.0f);
            // UV下端の根元をノズルへ固定。背面では短く、側面では長く見える。
            translate={translate.x+axis.x*visibleLength*0.48f,
                translate.y+axis.y*visibleLength*0.48f,translate.z+axis.z*visibleLength*0.48f};
            scale={(isOuterFlame ? 0.42f : 0.22f)*(1.0f+thrust_*0.12f)*pulse,
                (std::max)(visibleLength,0.001f),1.0f};
            plumeOpacity=std::clamp(visibleLength/(length*0.16f),0.0f,1.0f);
        }
        Math::Vector4 color = aura.color;
        if (isOuterFlame) {
            color.x = Lerp(color.x, 0.46f, feverThrustBoost);
            color.y = Lerp(color.y, 0.90f, feverThrustBoost);
            color.z = Lerp(color.z, 1.0f, feverThrustBoost);
        } else {
            color.x = Lerp(color.x, 1.0f, feverThrustBoost);
            color.y = Lerp(color.y, 1.0f, feverThrustBoost);
            color.z = Lerp(color.z, 0.92f, feverThrustBoost);
        }
        color.w *= plumeOpacity*pulse*(0.90f+thrust_*0.10f);
        color.w = std::clamp(color.w, 0.0f, 1.0f);

        aura.object->SetModel(aura.model);
        aura.object->SetTranslate(translate);
        aura.object->SetRotate(rotate);
        aura.object->SetScale(scale);
        aura.object->SetColor(color);
        aura.object->Update();
        aura.visible = true;
    }

    // 機首へ収束する半円と芯。完了後は大きく点滅させず、小さな金色の光を保つ。
    const float charge = frame.charge;
    const bool chargeReady = charge >= 1.0f;
    chargeVisible_ = !frame.feverActive && charge > 0.08f;
    if (chargeVisible_) {
        const float opacity = std::clamp((charge - 0.08f) / 0.45f, 0.0f, 1.0f);
        const Math::Vector3 muzzle{ playerPosition.x, playerPosition.y + 0.08f, playerPosition.z + 2.05f };
        for (size_t index = 0; index < chargeLights_.size(); ++index) {
            auto& visual = chargeLights_[index];
            if (!visual) { continue; }
            const float size = index == 0 ? Lerp(1.40f, 0.78f, charge) :
                Lerp(0.06f, 0.24f, charge) * (chargeReady ? 1.10f : 1.0f);
            visual->SetTranslate(muzzle);
            visual->SetRotate(Bullet::CalculateBillboardRotation(camera_->GetWorldMatrix(),
                index == 0 ? frame.clockFrames * 0.040f : kContactShadowHalfPi * 0.5f));
            visual->SetScale({ size, size, 1 });
            visual->SetColor({ 1.35f, 0.78f, 0.18f,
                opacity * (index == 0 ? (chargeReady ? 0.62f : 0.34f) : 0.80f) });
            visual->Update();

        }
    }

}

// 現在の噴射とチャージの状態から、描画する光の部品を選ぶ。
void PlayerFlightEffects::DrawFlight()
{
    if (!common_ || !camera_) { return; }
#ifdef _DEBUG
    const Snapshot beforeDraw = Inspect();
#endif
    const auto blend = common_->GetBlendMode();
    const auto depth = common_->GetDepthDrawMode();
    common_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    common_->SetBlendMode(BlendMode::Add);
    common_->CommonDrawSetting();
    for (const auto& plume : plumes_) { if (plume.visible && plume.object) { plume.object->Draw(); } }
    if (chargeVisible_) { for (const auto& light : chargeLights_) { if (light) { light->Draw(); } } }
    common_->SetBlendMode(blend);
    common_->SetDepthDrawMode(depth);
    common_->CommonDrawSetting();
#ifdef _DEBUG
    assert(Inspect() == beforeDraw); // 描画回数で噴射や残像の時計を進めない。
    assert(common_->GetBlendMode() == blend && common_->GetDepthDrawMode() == depth);
#endif
}

// 生きている残像を寿命に合わせた透明度で描き、回避の光を画面へ重ねる。
void PlayerFlightEffects::DrawAfterimages()
{
    Model* afterimageModel =
        assets_.trail ? assets_.trail : assets_.glow;
    if (!common_ || !camera_ || !afterimageModel) {
        return;
    }

    bool hasActiveAfterimage = false;
    for (const Afterimage& afterimage : afterimages_) {
        if (afterimage.active && afterimage.object) {
            hasActiveAfterimage = true;
            break;
        }
    }
    if (!hasActiveAfterimage) {
        return;
    }

    const BlendMode previousBlendMode = common_->GetBlendMode();
    const DepthDrawMode previousDepthMode = common_->GetDepthDrawMode();

    common_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    common_->SetBlendMode(BlendMode::Add);
    common_->CommonDrawSetting();

    for (Afterimage& afterimage : afterimages_) {
        if (!afterimage.active || !afterimage.object) {
            continue;
        }

        const float duration = afterimage.precise ? kHullAfterimageDuration : kAfterimageDuration;
        const float rate = std::clamp(
            afterimage.age / duration,
            0.0f,
            1.0f);
        const float fade = 1.0f - rate;
        if (afterimage.precise && assets_.hull) {
            // 成功した姿勢と位置を固定して残し、現在の機体だけがそこから抜ける。
            afterimage.object->SetModel(assets_.hull);
            afterimage.object->SetTranslate(afterimage.position);
            afterimage.object->SetRotate(afterimage.rotation);
            afterimage.object->SetScale({ afterimage.scale, afterimage.scale, afterimage.scale });
            afterimage.object->SetColor({ 0.26f, 0.80f, 1.0f, 0.21f * fade * fade });
            afterimage.object->Update();
            afterimage.object->Draw();
            continue;
        }
        const float direction = static_cast<float>(afterimage.direction);
        Math::Vector3 translate = afterimage.position;
        translate.x -= direction * (0.24f + 1.55f * rate);
        translate.y += std::sin(rate * kTwoPi) * 0.045f;
        translate.z -= 0.28f * rate;

        Math::Vector3 rotate = camera_->GetRotate();
        rotate.z += direction * (0.26f + 0.18f * rate);

        const float length = 2.75f + 2.25f * rate;
        const float thickness = 0.56f + 0.18f * rate;
        Math::Vector4 color{
            0.46f,
            0.96f,
            1.0f,
            0.74f * fade * fade
        };

        afterimage.object->SetModel(afterimageModel);
        afterimage.object->SetTranslate(translate);
        afterimage.object->SetScale({ length, thickness, 1.0f });
        afterimage.object->SetRotate(rotate);
        afterimage.object->SetColor(color);
        afterimage.object->Update();
        afterimage.object->Draw();
    }

    common_->SetBlendMode(previousBlendMode);
    common_->SetDepthDrawMode(previousDepthMode);
    common_->CommonDrawSetting();
}

#ifdef _DEBUG
// 開発確認で、準備した描画枠の数や現在の噴射状態を取得する。
PlayerFlightEffects::Snapshot PlayerFlightEffects::Inspect() const
{
    Snapshot snapshot{};
    for (const auto& afterimage : afterimages_) {
        snapshot.objects += afterimage.object ? 1 : 0;
        snapshot.afterimages += afterimage.active ? 1 : 0;
        snapshot.afterimageAge += afterimage.age;
    }
    for (const auto& plume : plumes_) {
        snapshot.objects += plume.object ? 1 : 0;
        snapshot.plumes += plume.visible && plume.object ? 1 : 0;
    }
    for (const auto& light : chargeLights_) {
        snapshot.objects += light ? 1 : 0;
        snapshot.chargeLights += chargeVisible_ && light ? 1 : 0;
    }
    snapshot.thrust = thrust_;
    return snapshot;
}
#endif
