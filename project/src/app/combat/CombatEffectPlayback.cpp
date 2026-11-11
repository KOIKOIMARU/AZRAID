#include "app/combat/CombatEffectPlayback.h"

#include "app/combat/Bullet.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Object3dCommon.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

// 演出の描画に使う共通機能とカメラを受け取り、再生用の一覧を準備する。
void CombatEffectPlayback::Initialize(Object3dCommon* common, Camera* camera)
{
    Clear();
    common_ = common;
    camera_ = camera;
    effects_.reserve(kEffectCapacity);
    available_.reserve(kVisualCapacity);
}

// 再生中と空きの描画枠を解放し、プールの準備状態を最初に戻す。
void CombatEffectPlayback::Clear()
{
    effects_.clear();
    available_.clear();
    common_ = nullptr;
    camera_ = nullptr;
    missCount_ = 0;
}

// 再生中の演出をすべて回収し、確保済みの描画枠を次の演出に使えるようにする。
void CombatEffectPlayback::Reset()
{
    for (auto& effect : effects_) { RecycleVisuals(effect); }
    effects_.clear();
}

// 演出に使い回す描画オブジェクトの枠を1つ準備する。
void CombatEffectPlayback::PrepareOne()
{
    assert(common_ && effects_.empty());
    if (available_.size() >= kVisualCapacity) { return; }
    auto object = std::make_unique<Object3d>();
    object->Initialize(common_);
    object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
    object->SetScale({ 0.01f, 0.01f, 1.0f });
    object->SetLightingMode(0);
    object->SetEnvironmentCoefficient(0.0f);
    object->Update();
    available_.push_back(std::move(object));
}

// 再生中の各演出が使っている描画オブジェクトの数を合計する。
size_t CombatEffectPlayback::ActiveVisualCount() const
{
    size_t count = 0;
    for (const auto& effect : effects_) {
        for (size_t i = 0; i < effect.visualCount; ++i) {
            if (effect.visuals[i].object) { ++count; }
        }
    }
    return count;
}

// 描画枠がある演出を再生一覧へ移す。一覧が満杯なら描画枠を返し、今回の演出は登録しない。
void CombatEffectPlayback::Commit(Effect&& effect)
{
    if (effect.visualCount == 0) { return; }
    // 記録の枠も固定にし、連射中にvectorの容量が増えることを防ぐ。
    // 作成途中の物体も必ず戻すため、表示を省いた後も再利用枠を失わない。
    if (effects_.size() >= kEffectCapacity) {
        RecycleVisuals(effect);
        ++missCount_;
        return;
    }
    effects_.push_back(std::move(effect));
}

// 空いている描画枠を取り出す。空きがなければ割り当て失敗として記録する。
std::unique_ptr<Object3d> CombatEffectPlayback::AcquireVisual()
{
    if (available_.empty()) {
        ++missCount_;
        return nullptr;
    }

    auto object = std::move(available_.back());
    available_.pop_back();
    return object;
}

// 演出で使い終わった描画枠を、次の発生で使える空き一覧へ戻す。
void CombatEffectPlayback::RecycleVisuals(Effect& effect)
{
    for (size_t index = 0; index < effect.visualCount; ++index) {
        Effect::Visual& visual = effect.visuals[index];
        if (!visual.object) {
            continue;
        }
        visual.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        visual.object->SetScale({ 0.01f, 0.01f, 1.0f });
        visual.object->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        visual.object->Update();
        available_.push_back(std::move(visual.object));
    }
    effect.visualCount = 0;
}

// 描画用の空き枠を取得し、エフェクトの位置・色・大きさと変化量を登録する。
void CombatEffectPlayback::AddVisual(Effect& effect, Model* model, const Math::Vector3& worldPosition, const VisualSpec& spec)
{
    if (!common_ || !model) {
        return;
    }
    if (effect.visualCount >= Effect::kMaxVisuals) {
        return;
    }

    Effect::Visual& visual = effect.visuals[effect.visualCount];
    visual = Effect::Visual{};
    visual.object = AcquireVisual();
    if (!visual.object) {
        return;
    }
    visual.object->SetModel(model);
    visual.object->SetTranslate(worldPosition);
    visual.object->SetScale({
        spec.baseSize * spec.aspectX * effect.strength,
        spec.baseSize * spec.aspectY * effect.strength,
        1.0f
    });
    visual.object->SetRotate(camera_ ? camera_->GetRotate() : Math::Vector3{});
    visual.object->SetColor(spec.color);
    visual.object->SetLightingMode(0);
    visual.object->SetEnvironmentCoefficient(0.0f);
    visual.object->Update();
    visual.color = spec.color;
    visual.baseSize = spec.baseSize;
    visual.growth = spec.growth;
    visual.spin = spec.spin;
    visual.popDelay = spec.popDelay;
    visual.lifeEnd = std::clamp(spec.lifeEnd, spec.popDelay + 0.001f, 1.0f);
    visual.roll = spec.roll;
    visual.drag = (std::max)(spec.drag, 0.0f);
    visual.aspectX = spec.aspectX;
    visual.aspectY = spec.aspectY;
    visual.velocity = spec.velocity;
    visual.additive = spec.additive;
    ++effect.visualCount;
}

// 演出の経過時間に合わせて見た目を変える。寿命が尽きた描画枠は次の演出で再利用する。
void CombatEffectPlayback::Update(float deltaSeconds)
{
    // 演出は実時間で進める。戦闘スローを掛けて煙や閃光を長引かせない。
    const float effectFrameStep = std::clamp(deltaSeconds * 60.0f, 0.5f, 4.0f);

    for (auto iterator = effects_.begin(); iterator != effects_.end();) {
        iterator->age += effectFrameStep;
        // 短い閃光の枠は煙の終了を待たず返す。連続撃破時の枠不足を防ぐ。
        for (size_t index = 0; index < iterator->visualCount; ++index) {
            auto& visual = iterator->visuals[index];
            if (visual.object && iterator->age >= visual.lifeEnd * static_cast<float>(iterator->duration)) {
                available_.push_back(std::move(visual.object));
            }
        }
        if (iterator->age >= static_cast<float>(iterator->duration)) {
            RecycleVisuals(*iterator);
            iterator = effects_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

// 再生中の演出に属する描画オブジェクトをまとめて描く。
void CombatEffectPlayback::Draw()
{
    if (effects_.empty() || !common_) {
        return;
    }

    const BlendMode previousBlendMode = common_->GetBlendMode();
    const DepthDrawMode previousDepthMode = common_->GetDepthDrawMode();

    // 煙や閃光は深度を書き込まず、後ろのエフェクトを消してしまうことを防ぐ。
    common_->SetDepthDrawMode(DepthDrawMode::ReadOnly);

    auto drawVisualPass = [&](bool additivePass) {
        common_->SetBlendMode(additivePass ? BlendMode::Add : BlendMode::Normal);
        common_->CommonDrawSetting();

        for (const Effect& effect : effects_) {
            if (effect.visualCount == 0) {
                continue;
            }

            const float rate =
                effect.age /
                static_cast<float>((std::max)(effect.duration, 1));
            for (size_t index = 0; index < effect.visualCount; ++index) {
                const Effect::Visual& visual = effect.visuals[index];
                if (!visual.object || visual.additive != additivePass) {
                    continue;
                }
                if (rate < visual.popDelay || rate >= visual.lifeEnd) {
                    continue;
                }
                const float localRate = (std::clamp)(
                    (rate - visual.popDelay) / (std::max)(visual.lifeEnd - visual.popDelay, 0.001f),
                    0.0f,
                    1.0f);
                const float fade = 1.0f - localRate;
                const float easeOut = 1.0f - std::pow(1.0f - localRate, 3.0f);
                const float size =
                    (visual.baseSize + visual.baseSize * visual.growth * easeOut) *
                    effect.strength;
                Math::Vector4 color = visual.color;
                const float alphaCurve =
                    effect.type == EffectType::RewardCollect ? fade :
                    effect.type == EffectType::PlayerDamage ? fade * fade * fade :
                    effect.type == EffectType::EnemyImpact ? fade :
                    fade * (0.78f + 0.22f * fade);
                color.w *= alphaCurve;

                float roll = visual.roll + localRate * visual.spin;
                if (camera_ && visual.aspectY > visual.aspectX * 1.8f &&
                    std::abs(visual.velocity.x) + std::abs(visual.velocity.y) > 0.001f) {
                    const auto& cameraWorld = camera_->GetWorldMatrix();
                    const float x = visual.velocity.x * cameraWorld.m[0][0] +
                        visual.velocity.y * cameraWorld.m[0][1] + visual.velocity.z * cameraWorld.m[0][2];
                    const float y = visual.velocity.x * cameraWorld.m[1][0] +
                        visual.velocity.y * cameraWorld.m[1][1] + visual.velocity.z * cameraWorld.m[1][2];
                    roll += std::atan2(-x, y);
                }
                const Math::Vector3 rotate = camera_ ?
                    Bullet::CalculateBillboardRotation(camera_->GetWorldMatrix(), roll) : Math::Vector3{ 0, 0, roll };
                const float activeAge = (std::max)(0.0f, effect.age - visual.popDelay * static_cast<float>(effect.duration));
                const float travel = visual.drag > 0.001f ?
                    (1.0f - std::exp(-visual.drag * activeAge)) / visual.drag : activeAge;
                Math::Vector3 translate = effect.worldPosition;
                translate.x += visual.velocity.x * travel;
                translate.y += visual.velocity.y * travel;
                translate.z += visual.velocity.z * travel;

                visual.object->SetTranslate(translate);
                visual.object->SetScale({
                    size * visual.aspectX,
                    size * visual.aspectY,
                    1.0f
                });
                visual.object->SetRotate(rotate);
                visual.object->SetColor(color);
                visual.object->Update();
                visual.object->Draw();
            }
        }
    };

    drawVisualPass(false);
    drawVisualPass(true);

    // 共通描画設定は他の描画も使うため、呼び出し前の状態へ戻す。
    common_->SetBlendMode(previousBlendMode);
    common_->SetDepthDrawMode(previousDepthMode);
    common_->CommonDrawSetting();
}
