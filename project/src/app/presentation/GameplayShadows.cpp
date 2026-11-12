// ===== GameplayShadows.cpp =====
// 影を落とす機体を選んでシャドウマップを描き、接地感を出す影も表示する

#include "app/presentation/GameplayShadows.h"

#include "app/enemy/Enemy.h"
#include "app/player/Player.h"
#include "app/stage/RailScenery.h"
#include "app/stage/SceneryAssets.h"

#include "engine/3d/Object3dCommon.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kContactShadowY = -2.82f;
constexpr float kContactShadowHalfPi = 1.57079632679f;

// 接地影を描く物体の位置、大きさ、透明度を更新して描画する
void DrawContactShadowObject(
    Object3d& object,
    const Math::Vector3& position,
    float scaleX,
    float scaleZ,
    float alpha,
    float yaw)
{
    const float clampedAlpha = std::clamp(alpha, 0.0f, 0.55f);
    if (clampedAlpha <= 0.001f) {
        return;
    }

    object.SetTranslate({ position.x, kContactShadowY, position.z });
    object.SetRotate({ kContactShadowHalfPi, yaw, 0.0f });
    object.SetScale({
        (std::max)(scaleX, 0.01f),
        (std::max)(scaleZ, 0.01f),
        1.0f
    });
    object.SetColor({ 0.0f, 0.0f, 0.0f, clampedAlpha });
    object.Update();
    object.Draw();
}
}

// 影を落とすステージ・機体・破片を光源側から描く　ここで作った深度は通常描画で参照する
void GameplayShadows::RenderMap(Object3dCommon& common, const Scene& scene) const
{
    const Math::Vector3 shadowFocusCenter{
        0.0f,
        10.0f,
        scene.railDistance + 105.0f
    };
    if (!common.BeginShadowPass(shadowFocusCenter)) {
        return;
    }

    const Math::Matrix4x4& lightViewProjection =
        common.GetShadowLightViewProjection();

    for (const auto& sceneObject : scene.objects) {
        if (sceneObject) {
            sceneObject->DrawShadow(lightViewProjection);
        }
    }

    // 画面外の建物も道路へ影を落とすため、色描画のisVisibleとは別に範囲を判定する
    // 路面・窓ガラスは投影しない　描画数は初期化済みの街モデル数で上限が決まる
    for (const RailScenery::Object& scenery : scene.scenery.Objects()) {
        if ((scenery.isDefenseDistrict || scenery.isBossApron || scenery.isTrackside) && !scenery.isVisible) { continue; }
        if (scenery.object && scenery.castsShadow &&
            scenery.currentLocalZ + scenery.halfDepth >= -45.0f &&
            scenery.currentLocalZ - scenery.halfDepth <= 240.0f) {
            scenery.object->DrawShadow(lightViewProjection);
        }
    }

    if (scene.player && !scene.player->IsDead()) {
        scene.player->DrawShadow(lightViewProjection);
    }
    for (Object3d* debris : scene.wreckage) {
        debris->DrawShadow(lightViewProjection);
    }

    for (const auto& enemy : scene.enemies) {
        if (enemy && !enemy->IsDead()) {
            enemy->DrawShadow(lightViewProjection);
        }
    }

    common.EndShadowPass();
}

// 機体の下に表示する接地影の描画枠を事前に用意する
void GameplayShadows::PrepareContacts(Object3dCommon& common, Model* model)
{
    if (!model) {
        return;
    }

    for (auto& shadow : contacts_) {
        shadow = std::make_unique<Object3d>();
        shadow->Initialize(&common);
        shadow->SetModel(model);
        shadow->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        shadow->SetRotate({ kContactShadowHalfPi, 0.0f, 0.0f });
        shadow->SetScale({ 0.01f, 0.01f, 1.0f });
        shadow->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        shadow->SetLightingMode(0);
        shadow->SetEnvironmentCoefficient(0.0f);
        shadow->SetAlphaReference(0.01f);
        shadow->Update();
    }
}

void GameplayShadows::Clear()
{
    contacts_ = {};
}

// 上空から見ても機体と地面の距離が分かるよう、地面に接地影を重ねる
void GameplayShadows::DrawContacts(Object3dCommon& common, const Scene& scene, float cameraClock)
{
    if (!contacts_.front()) {
        return;
    }

    const BlendMode previousBlendMode = common.GetBlendMode();
    const DepthDrawMode previousDepthMode = common.GetDepthDrawMode();
    common.SetDepthDrawMode(DepthDrawMode::ReadOnly);
    common.SetBlendMode(BlendMode::Normal);
    common.CommonDrawSetting();

    size_t shadowIndex = 0;
    const auto drawNextShadow =
        [&](const Math::Vector3& position,
            float scaleX,
            float scaleZ,
            float alpha,
            float yaw) {
            if (shadowIndex >= contacts_.size()) {
                return;
            }
            const auto& shadow = contacts_[shadowIndex++];
            if (!shadow) {
                return;
            }
            DrawContactShadowObject(
                *shadow,
                position,
                scaleX,
                scaleZ,
                alpha,
                yaw);
        };

    if (scene.player && !scene.player->IsDead()) {
        const Math::Vector3 playerPosition = scene.player->GetTranslate();
        const float height =
            (std::max)(playerPosition.y - kContactShadowY, 0.0f);
        const float heightFade =
            std::clamp(1.0f - height / 8.0f, 0.42f, 1.0f);
        const float dodgeStretch = scene.player->IsDodging() ? 0.26f : 0.0f;
        drawNextShadow(
            playerPosition,
            1.65f + height * 0.10f + dodgeStretch,
            1.00f + height * 0.055f,
            0.19f * heightFade,
            scene.player->IsDodging() ?
                static_cast<float>(scene.player->GetDodgeDirection()) * 0.20f :
                0.0f);
    }

    for (const auto& enemy : scene.enemies) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }
        const Math::Vector3 enemyPosition = enemy->GetTranslate();
        const float localZ = enemyPosition.z - scene.railDistance;
        if (localZ < SceneryAssets::kSceneryNearLocalZ || localZ > 210.0f) {
            continue;
        }

        const float height =
            (std::max)(enemyPosition.y - kContactShadowY, 0.0f);
        const float heightFade =
            std::clamp(1.0f - height / 12.0f, 0.32f, 0.88f);
        const float distanceFade =
            std::clamp(1.0f - (localZ - 36.0f) / 210.0f, 0.42f, 1.0f);
        if (enemy->IsBoss()) {
            drawNextShadow(
                enemyPosition,
                6.2f + height * 0.10f,
                3.3f + height * 0.055f,
                0.20f * heightFade * distanceFade,
                std::sin(cameraClock * 0.013f) * 0.20f);
        } else {
            const float radius = enemy->GetRadius();
            drawNextShadow(
                enemyPosition,
                1.25f + radius * 0.55f + height * 0.075f,
                0.78f + radius * 0.30f + height * 0.040f,
                0.15f * heightFade * distanceFade,
                std::sin(cameraClock * 0.018f + enemyPosition.x) * 0.18f);
        }
    }

    // 道路沿いの実際の建物に接地影を合わせる　広場の中央へ旧配置の影を残さない
    for (const RailScenery::Object& scenery : scene.scenery.Objects()) {
        if (!scenery.isBuilding || scenery.isBackRow || !scenery.isVisible) { continue; }
        const auto p = scenery.object->GetTranslate();
        const float distanceFade = std::clamp(1.0f - (scenery.currentLocalZ - 28.0f) / 230.0f, 0.36f, 1.0f);
        drawNextShadow({ p.x + (p.x < 0.0f ? 3.0f : -3.0f), kContactShadowY, p.z },
            6.0f, 11.0f * scenery.scale.x, 0.050f * distanceFade, 0.0f);
    }

    common.SetBlendMode(previousBlendMode);
    common.SetDepthDrawMode(previousDepthMode);
    common.CommonDrawSetting();
}
