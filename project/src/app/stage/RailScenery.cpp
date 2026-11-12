// ===== RailScenery.cpp =====
// レールに沿って街並みを配置し、進行位置に合わせて繰り返し使う背景を更新する

#include "app/stage/RailScenery.h"
#include "app/stage/SceneryAssets.h"
#include "app/runtime/GameRuntimeMath.h"
#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3dCommon.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace GameRuntimeMath;
using namespace SceneryAssets;

// 背景の範囲をカメラへ写し、画面に入る可能性があるかを判定する
bool BoundsIntersectCamera(const Math::Vector3& minimum, const Math::Vector3& maximum,
    const Math::Matrix4x4& worldViewProjection)
{
    // 同じクリップ面の外に全頂点がある箱だけを省く　カメラを跨ぐ大きな敷地も消さない
    unsigned int outsideEveryCorner = 0x3fu;
    const auto& m = worldViewProjection.m;
    for (int corner = 0; corner < 8; ++corner) {
        const float x = corner & 1 ? maximum.x : minimum.x;
        const float y = corner & 2 ? maximum.y : minimum.y;
        const float z = corner & 4 ? maximum.z : minimum.z;
        const float cx = x * m[0][0] + y * m[1][0] + z * m[2][0] + m[3][0];
        const float cy = x * m[0][1] + y * m[1][1] + z * m[2][1] + m[3][1];
        const float cz = x * m[0][2] + y * m[1][2] + z * m[2][2] + m[3][2];
        const float cw = x * m[0][3] + y * m[1][3] + z * m[2][3] + m[3][3];
        const float margin = 0.001f * std::abs(cw) + 0.001f;
        unsigned int outside = 0;
        if (cx < -cw - margin) { outside |= 1u; }
        if (cx > cw + margin) { outside |= 2u; }
        if (cy < -cw - margin) { outside |= 4u; }
        if (cy > cw + margin) { outside |= 8u; }
        if (cz < -margin) { outside |= 16u; }
        if (cz > cw + margin) { outside |= 32u; }
        outsideEveryCorner &= outside;
        if (outsideEveryCorner == 0) { return true; }
    }
    return false;
}

// 周回する背景の前後位置を、繰り返し区間の範囲へ戻す
float WrapSceneryLocalZ(float localZ, float loopLength, float nearLocalZ)
{
    const float safeLoopLength = (std::max)(loopLength, 1.0f);
    const float cycle = localZ - nearLocalZ;
    return cycle - std::floor(cycle / safeLoopLength) * safeLoopLength + nearLocalZ;
}
}

// 本編と練習で使う区間の境界を保存し、背景を切り替える計算に使う
void RailScenery::SetDistricts(bool tutorial, float defenseStartZ, float plazaStartZ)
{
    tutorial_ = tutorial;
    defenseStartZ_ = defenseStartZ;
    plazaStartZ_ = plazaStartZ;
}

// 背景の描画オブジェクトと、配置・表示に使った状態を片づける
void RailScenery::Clear()
{
    transparentOrder_.clear();
    objects_.clear();
    boundsCache_.clear();
    visibleCount_ = 0;
    common_ = nullptr;
    camera_ = nullptr;
}

// 地形と施設の配置を作り、飛行中に表示する区画の描画オブジェクトを準備する
bool RailScenery::Prepare(Object3dCommon* common, Camera* camera, size_t first, size_t count)
{
    common_ = common;
    camera_ = camera;
    if (first == 0) {
        transparentOrder_.clear();
        objects_.clear();
        boundsCache_.clear();
    }
    size_t sceneryIndex = 0;

    if (!common_) {
        return true;
    }

    ModelManager* modelManager = ModelManager::GetInstance();

    const auto addSceneryStyled =
        [&](const char* modelPath,
            const Math::Vector3& anchor,
            const Math::Vector3& scale,
            const Math::Vector3& rotate,
            float loopLength,
            float phase,
            const Math::Vector4& color,
            int lightingMode,
            float environmentCoefficient) {
            const size_t index = sceneryIndex++;
            if (index < first || index >= first + count) { return; }
            // 本編の外周も中央と同じ航空施設の意匠へ揃える　既存の配置・循環は引き継ぐ
            if (!tutorial_) {
                const std::string_view requestedModel = modelPath;
                if (requestedModel == kCityBuildingSmallModelPath) { modelPath = kAviationBuildingSupportModelPath; }
                else if (requestedModel == kCityBuildingMediumModelPath) { modelPath = kAviationBuildingOperationsModelPath; }
                else if (requestedModel == kCityBuildingLargeModelPath) { modelPath = kAviationBuildingHangarModelPath; }
            }
            modelManager->LoadModel(modelPath);
            Model* model = modelManager->FindModel(modelPath);
            if (!model) {
                return;
            }

            const std::string modelPathText = modelPath ? modelPath : "";
            const bool isAviation = modelPathText.find("scenery/aviation_district/") == 0;
            const bool isGlazing = isAviation && modelPathText.find("Glazing") != std::string::npos;
            const bool isPaving = modelPathText == kAviationFlightwayModelPath ||
                modelPathText == kAviationDefenseFlightwayModelPath || modelPathText == kAviationBossApronSurfaceModelPath ||
                modelPathText == kAviationTerrainModelPath || modelPathText == kAviationCampusGroundModelPath;
            Object scenery;
            scenery.object = std::make_unique<Object3d>();
            scenery.object->Initialize(common_);
            scenery.object->SetModel(model);
            scenery.object->SetTranslate(anchor);
            scenery.object->SetRotate(rotate);
            scenery.object->SetScale(scale);
            scenery.object->SetColor(color);
            scenery.object->SetLightingMode(lightingMode);
            scenery.object->SetEnvironmentCoefficient(environmentCoefficient);
            scenery.object->SetShadowReceiveStrength(0.9f);
            if (modelPathText.find("Street") != std::string::npos || isPaving) {
                scenery.object->SetShadowReceiveStrength(0.76f);
                scenery.object->SetShininess(24.0f);
                scenery.object->SetSpecularColor({ 0.035f, 0.038f, 0.042f });
                scenery.object->SetRoughness(0.84f);
                scenery.object->SetMetallic(0.02f);
            } else if (modelPathText.find("Building") != std::string::npos) {
                scenery.object->SetShininess(28.0f);
                scenery.object->SetSpecularColor({ 0.065f, 0.070f, 0.080f });
                scenery.object->SetRoughness(0.76f);
                scenery.object->SetMetallic(0.0f);
            } else if (isGlazing) {
                scenery.object->SetShininess(110.0f);
                scenery.object->SetSpecularColor({ 0.14f, 0.18f, 0.20f });
                scenery.object->SetRoughness(0.22f);
                scenery.object->SetMetallic(0.35f);
            } else {
                scenery.object->SetShininess(32.0f);
                scenery.object->SetSpecularColor({ 0.09f, 0.095f, 0.105f });
                scenery.object->SetRoughness(0.68f);
                scenery.object->SetMetallic(0.05f);
            }
            if ((isAviation && !isGlazing && !isPaving) || modelPathText == kTransitLandmarkModelPath) {
                // 施設だけを既存の柔らかい拡散光へ合わせ、陰側の窓・扉・段差も残す
                scenery.object->SetLightingMode(2);
                scenery.object->SetShadowReceiveStrength(0.70f);
                scenery.object->SetEnvironmentCoefficient(0.055f);
            }
            scenery.object->SetAlphaReference(0.01f);
            scenery.object->Update();
            scenery.anchor = anchor;
            scenery.scale = scale;
            scenery.rotate = rotate;
            scenery.color = color;
            scenery.loopLength = loopLength;
            scenery.speedMultiplier = 1.0f;
            scenery.driftSpeed = 0.0f;
            scenery.phase = phase;
            scenery.currentLocalZ = anchor.z;
            scenery.isBuilding = modelPathText.find("Building") != std::string::npos;
            scenery.isRoad = modelPathText.find("Street") != std::string::npos;
            scenery.isRoadDetail = modelPathText.find("Manhole") != std::string::npos ||
                modelPathText.find("Drain") != std::string::npos;
            scenery.isBackRow = scenery.isBuilding && std::abs(anchor.x) > 40.0f;
            scenery.isLandmark = modelPathText == kTransitLandmarkModelPath ||
                modelPathText == kAviationDistrictModelPath || modelPathText == kAviationGlazingModelPath ||
                modelPathText == kAviationFlightwayModelPath || modelPathText == kAviationCampusGroundModelPath;
            scenery.isTrackside = modelPathText == kTracksideModelPath;
            scenery.isTerrain = modelPathText == kAviationTerrainModelPath;
            scenery.isDefenseDistrict = modelPathText == kAviationDefenseModelPath ||
                modelPathText == kAviationDefenseGlazingModelPath || modelPathText == kAviationDefenseFlightwayModelPath ||
                modelPathText == kAviationDefenseRelayModelPath || modelPathText == kAviationDefenseRelayGlazingModelPath;
            scenery.isBossApron = modelPathText == kAviationBossApronModelPath ||
                modelPathText == kAviationBossApronGlazingModelPath || modelPathText == kAviationBossApronSurfaceModelPath ||
                modelPathText == kAviationBossApronFreightModelPath || modelPathText == kAviationBossApronFreightGlazingModelPath;
            scenery.castsShadow = !isGlazing && !isPaving &&
                (scenery.isBuilding || scenery.isLandmark || scenery.isTrackside ||
                    scenery.isDefenseDistrict || scenery.isBossApron);
            scenery.halfDepth = (modelPathText == kAviationDistrictModelPath ||
                modelPathText == kAviationGlazingModelPath || modelPathText == kAviationFlightwayModelPath) ? 250.0f :
                modelPathText == kAviationCampusGroundModelPath ? 324.0f :
                scenery.isLandmark ? 54.0f : scenery.isTrackside ? 18.0f : 0.0f;
            if (scenery.isDefenseDistrict) { scenery.halfDepth = kDefenseModuleLength * 0.5f; }
            if (scenery.isBossApron) { scenery.halfDepth = kBossApronModuleLength * 0.5f; }
            if (scenery.isTerrain) { scenery.halfDepth = 128.0f; }
            const bool rotatedBounds = scenery.isBuilding || scenery.isRoad || scenery.isRoadDetail;
            const auto same = [](Math::Vector3 a, Math::Vector3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; };
            const auto cached = std::find_if(boundsCache_.begin(), boundsCache_.end(),
                [&](const BoundsCache& entry) {
                    return entry.model == model && (!rotatedBounds || (same(entry.scale, scale) && same(entry.rotate, rotate)));
                });
            if (cached != boundsCache_.end()) {
                scenery.boundsMin = cached->min; scenery.boundsMax = cached->max; scenery.halfDepth = cached->halfDepth;
            } else {
            const auto transform = Math::MakeAffineMatrix(scale, rotate, {});
            for (const auto& vertex : model->GetVertices()) {
                const Math::Vector3 v{ vertex.position.x, vertex.position.y, vertex.position.z };
                scenery.boundsMin = { (std::min)(scenery.boundsMin.x, v.x),
                    (std::min)(scenery.boundsMin.y, v.y), (std::min)(scenery.boundsMin.z, v.z) };
                scenery.boundsMax = { (std::max)(scenery.boundsMax.x, v.x),
                    (std::max)(scenery.boundsMax.y, v.y), (std::max)(scenery.boundsMax.z, v.z) };
                if (scenery.isBuilding || scenery.isRoad || scenery.isRoadDetail) {
                    // 回転後の実際の端を含める　中心だけで判定すると建物の手前側が突然出る
                    const auto position = TransformCoord({ vertex.position.x, vertex.position.y, vertex.position.z }, transform);
                    scenery.halfDepth = (std::max)(scenery.halfDepth, std::abs(position.z));
                }
            }
            boundsCache_.push_back({ model, scale, rotate, scenery.boundsMin, scenery.boundsMax, scenery.halfDepth });
            }
            scenery.drawFarLocalZ = kSceneryFarLocalZ;
            if (tutorial_) {
                scenery.drawFarLocalZ = scenery.isBuilding ? (scenery.isBackRow ? 178.0f : 220.0f) : 300.0f;
            }
            if (scenery.isRoadDetail) {
                scenery.drawFarLocalZ = 140.0f;
            }
            scenery.isVisible = true;
            objects_.push_back(std::move(scenery));
        };

    const auto addScenery =
        [&](const char* modelPath,
            const Math::Vector3& anchor,
            const Math::Vector3& scale,
            const Math::Vector3& rotate,
            float loopLength,
            float phase) {
            addSceneryStyled(
                modelPath,
                anchor,
                scale,
                rotate,
                loopLength,
                phase,
                { 1.0f, 1.0f, 1.0f, 1.0f },
                2,
                0.04f);
        };

    constexpr float kCityLayoutLength = 324.0f;
    const int cityCycleCount = tutorial_ ? 1 : 2;
    const float cityLoopLength = kCityLayoutLength * static_cast<float>(cityCycleCount);
    constexpr float kRoadY = -3.05f;
    constexpr float kSurfaceDetailY = kRoadY - 0.135f;
    constexpr float kBuildingY = kRoadY - 0.15f;
    // 元の道路の最下面もkBuildingYと同じ高さ　基盤は下へ離して面の競合を防ぐ
    constexpr float kTerrainY = kBuildingY - 0.30f;
    constexpr float kHalfPi = 1.57079632679f;

    for (int i = 0; i < 18 * cityCycleCount; ++i) {
        const float segmentZ = 18.0f * static_cast<float>(i);
        addSceneryStyled(
            kCityStreet4LaneModelPath,
            { 0.0f, kRoadY, -9.0f + segmentZ },
            // 元モデルはX方向へ続く6mの道路、Z方向18mに車道と歩道が並ぶ
            // 90度回して進行方向へ18mずつ連結　歩道が横断帯として反復しないようにする
            { 3.0f, 1.0f, 14.0f / 3.0f },
            { 0.0f, kHalfPi, 0.0f },
            cityLoopLength,
            0.0f,
            { 0.78f, 0.82f, 0.86f, 1.0f },
            2,
            0.015f);
    }

    struct CityBuildingPlacement {
        const char* modelPath;
        Math::Vector3 anchor;
        Math::Vector3 scale;
        Math::Vector3 rotate;
        Math::Vector4 color;
    };

    const CityBuildingPlacement cityBuildings[] = {
        { kCityBuildingLargeModelPath,
            { -31.5f, kBuildingY, 4.0f },
            { 0.95f, 1.30f, 0.95f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.92f, 0.96f, 1.0f, 1.0f } },
        { kCityBuildingMediumModelPath,
            { 31.5f, kBuildingY, 10.0f },
            { 1.05f, 1.22f, 1.05f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.95f, 0.98f, 1.0f, 1.0f } },
        { kCityBuildingSmallModelPath,
            { -31.5f, kBuildingY, 24.0f },
            { 1.10f, 1.20f, 1.10f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.98f, 0.97f, 0.94f, 1.0f } },
        { kCityBuildingLargeModelPath,
            { 31.5f, kBuildingY, 31.0f },
            { 1.05f, 1.42f, 1.05f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.93f, 0.96f, 1.0f, 1.0f } },
        { kCityBuildingMediumModelPath,
            { -31.5f, kBuildingY, 48.0f },
            { 1.08f, 1.28f, 1.08f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.96f, 0.98f, 1.0f, 1.0f } },
        { kCityBuildingSmallModelPath,
            { 31.5f, kBuildingY, 56.0f },
            { 1.15f, 1.26f, 1.15f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.98f, 0.96f, 0.93f, 1.0f } },
        { kCityBuildingLargeModelPath,
            { -31.5f, kBuildingY, 73.0f },
            { 1.05f, 1.46f, 1.05f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.92f, 0.95f, 0.99f, 1.0f } },
        { kCityBuildingMediumModelPath,
            { 31.5f, kBuildingY, 82.0f },
            { 1.10f, 1.32f, 1.10f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.96f, 0.98f, 1.0f, 1.0f } },
        { kCityBuildingSmallModelPath,
            { -31.5f, kBuildingY, 101.0f },
            { 1.12f, 1.22f, 1.12f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.98f, 0.97f, 0.94f, 1.0f } },
        { kCityBuildingLargeModelPath,
            { 31.5f, kBuildingY, 111.0f },
            { 1.00f, 1.38f, 1.00f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.93f, 0.96f, 1.0f, 1.0f } },
        { kCityBuildingMediumModelPath,
            { -31.5f, kBuildingY, 132.0f },
            { 1.08f, 1.30f, 1.08f },
            { 0.0f, kHalfPi, 0.0f },
            { 0.96f, 0.98f, 1.0f, 1.0f } },
        { kCityBuildingSmallModelPath,
            { 31.5f, kBuildingY, 145.0f },
            { 1.14f, 1.24f, 1.14f },
            { 0.0f, -kHalfPi, 0.0f },
            { 0.98f, 0.96f, 0.93f, 1.0f } },
    };

    constexpr size_t kCityBuildingCount =
        sizeof(cityBuildings) / sizeof(cityBuildings[0]);
    for (size_t index = 0; index < kCityBuildingCount * cityCycleCount; ++index) {
        const CityBuildingPlacement& building = cityBuildings[index % kCityBuildingCount];
        Math::Vector3 buildingAnchor = building.anchor;
        buildingAnchor.z += kCityLayoutLength * static_cast<float>(index / kCityBuildingCount);
        addSceneryStyled(
            building.modelPath,
            buildingAnchor,
            building.scale,
            building.rotate,
            cityLoopLength,
            0.0f,
            building.color,
            2,
            0.04f);
        const bool keepFarPair = index % 4 == 0 || index % 4 == 1;
        if (!keepFarPair) {
            continue;
        }
        Math::Vector3 farAnchor = buildingAnchor;
        farAnchor.z += kCityLayoutLength * 0.5f;
        addSceneryStyled(
            building.modelPath,
            farAnchor,
            building.scale,
            building.rotate,
            cityLoopLength,
            0.0f,
            building.color,
            2,
            0.04f);
    }

    struct CityBackRowPlacement {
        const char* modelPath;
        float z;
        Math::Vector3 scale;
        Math::Vector4 color;
    };

    constexpr float kBackRowX = 43.5f;
    const CityBackRowPlacement cityBackRows[] = {
        { kCityBuildingLargeModelPath,  -8.0f, { 1.18f, 1.58f, 1.62f }, { 0.86f, 0.90f, 0.96f, 1.0f } },
        { kCityBuildingMediumModelPath,  34.0f, { 1.24f, 1.48f, 1.72f }, { 0.88f, 0.92f, 0.98f, 1.0f } },
        { kCityBuildingLargeModelPath,   76.0f, { 1.20f, 1.62f, 1.66f }, { 0.86f, 0.90f, 0.96f, 1.0f } },
        { kCityBuildingMediumModelPath, 118.0f, { 1.26f, 1.50f, 1.76f }, { 0.88f, 0.92f, 0.98f, 1.0f } },
        { kCityBuildingLargeModelPath,  160.0f, { 1.22f, 1.66f, 1.68f }, { 0.86f, 0.90f, 0.96f, 1.0f } },
        { kCityBuildingMediumModelPath, 202.0f, { 1.24f, 1.50f, 1.74f }, { 0.88f, 0.92f, 0.98f, 1.0f } },
        { kCityBuildingLargeModelPath,  244.0f, { 1.20f, 1.60f, 1.70f }, { 0.86f, 0.90f, 0.96f, 1.0f } },
        { kCityBuildingMediumModelPath, 286.0f, { 1.26f, 1.52f, 1.78f }, { 0.88f, 0.92f, 0.98f, 1.0f } },
    };

    constexpr size_t kCityBackRowCount =
        sizeof(cityBackRows) / sizeof(cityBackRows[0]);
    for (size_t index = 0; index < kCityBackRowCount * cityCycleCount; ++index) {
        const CityBackRowPlacement& row = cityBackRows[index % kCityBackRowCount];
        for (int sideIndex = 0; sideIndex < 2; ++sideIndex) {
            const float side = sideIndex == 0 ? -1.0f : 1.0f;
            const float stagger = side > 0.0f ? 21.0f : 0.0f;
            Math::Vector3 scale = row.scale;
            scale.x *= side > 0.0f ? 1.04f : 1.0f;
            scale.z *= side > 0.0f ? 1.02f : 1.0f;
            addSceneryStyled(
                row.modelPath,
                { side * kBackRowX, kBuildingY - 0.03f, row.z + stagger + kCityLayoutLength * static_cast<float>(index / kCityBackRowCount) },
                scale,
                { 0.0f, side < 0.0f ? kHalfPi : -kHalfPi, 0.0f },
                cityLoopLength,
                0.0f,
                row.color,
                2,
                0.03f);
        }
    }

    for (int i = 0; i < 4 * cityCycleCount; ++i) {
        const float detailZ = 18.0f + 42.0f * static_cast<float>(i % 4) + kCityLayoutLength * static_cast<float>(i / 4);
        addScenery(
            kCityManholeCoverModelPath,
            { -1.6f, kSurfaceDetailY, detailZ },
            { 1.15f, 1.15f, 1.15f },
            { 0.0f, 0.22f * static_cast<float>(i), 0.0f },
            cityLoopLength,
            0.0f);
        addScenery(
            kCityDrainModelPath,
            { 4.2f, kSurfaceDetailY, detailZ + 8.0f },
            { 1.2f, 1.2f, 1.2f },
            { 0.0f, 0.0f, 0.0f },
            cityLoopLength,
            0.0f);
    }
    if (!tutorial_) {
        // 路面と同じ648m周期　全遠景を初期化時に確保し、実行中に増やさない
        // 自機の可動域(±8.9m)の外へ配置し、戦闘の中央には障害物を足さない
        for (int index = 0; index < 18; ++index) {
            addSceneryStyled(kTracksideModelPath,
                { 0.0f, kRoadY, 18.0f + 36.0f * static_cast<float>(index) },
                { 1.0f, 1.0f, 1.0f }, {}, cityLoopLength, 0.0f,
                { 0.94f, 0.96f, 0.97f, 1.0f }, 2, 0.015f);
        }
        // 路面・歩道・建物の敷地の隙間を含む地表　四枚が端で接続し、視界外でだけ循環する
        for (int index = 0; index < 4; ++index) {
            addSceneryStyled(kAviationTerrainModelPath,
                { 0.0f, kTerrainY, 256.0f * static_cast<float>(index) },
                { 1.0f, 1.0f, 1.0f }, {}, 1024.0f, 0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f }, 2, 0.015f);
        }
        // 航空施設と外周の街区を、同じ固定敷地の管理道路・舗装・植栽でつなぐ
        addSceneryStyled(kAviationCampusGroundModelPath,
            {0.0f,kTerrainY,162.0f},{1.0f,1.0f,1.0f},{},0.0f,0.0f,
            {1.0f,1.0f,1.0f,1.0f},2,0.015f);
        // 地上から約15mより上に橋桁、支柱は左右22mより外　敵と自機の飛行域は塞がない
        // 部品はオフラインで8材質へ結合済み　プレイ中にモデルやGPU資源を追加しない
        addSceneryStyled(kTransitLandmarkModelPath,
            { 0.0f, kBuildingY, kTransitLandmarkWorldZ }, { 1.0f, 1.0f, 1.0f }, {},
            0.0f, 0.0f, { 0.86f, 0.90f, 0.93f, 1.0f }, 1, 0.025f);
        // 進入設備、運航棟、管制塔、退出側の整備場を同じ固定敷地へまとめる
        // 元の高架へ接続する形状をオフラインで7材質に結合　飛行域の左右17mは空ける
        addSceneryStyled(kAviationDistrictModelPath,
            { 0.0f, kBuildingY, kTransitLandmarkWorldZ }, { 1.0f, 1.0f, 1.0f }, {},
            0.0f, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 1, 0.025f);
        // 窓だけを別の材質設定にする　既存の不透明パスで描き、追加の描画先やSRVは使わない
        addSceneryStyled(kAviationGlazingModelPath,
            { 0.0f, kBuildingY, kTransitLandmarkWorldZ }, { 1.0f, 1.0f, 1.0f }, {},
            0.0f, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 1, 0.16f);
        // 序盤の飛行進入路だけを敷地へ固定　地表の誘導線は自動車用の車線へ重ねない
        addSceneryStyled(kAviationFlightwayModelPath,
            { 0.0f, kBuildingY, kTransitLandmarkWorldZ }, { 1.0f, 1.0f, 1.0f }, {},
            0.0f, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 2, 0.015f);
        // 近景と遠景を含む六つの共有敷地を起動時に確保　視界外で循環する
        for (int index = 0; index < kDefenseModuleCount; ++index) {
            const Math::Vector3 anchor{ 0.0f, kBuildingY,
                kDefenseModuleLength * (static_cast<float>(index) + 0.5f) };
            const Math::Vector3 rotate{ 0.0f, index % 4 >= 2 ? kHalfPi * 2.0f : 0.0f, 0.0f };
            const bool relay = index % 2 != 0;
            addSceneryStyled(relay ? kAviationDefenseRelayModelPath : kAviationDefenseModelPath,
                anchor, { 1.0f, 1.0f, 1.0f }, rotate,
                kDefenseLoopLength, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 1, 0.025f);
            addSceneryStyled(relay ? kAviationDefenseRelayGlazingModelPath : kAviationDefenseGlazingModelPath,
                anchor, { 1.0f, 1.0f, 1.0f }, rotate,
                kDefenseLoopLength, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 1, 0.16f);
            // 舗装だけは同じ向きへ固定し、継ぎ目でアスファルトの粒を反転させない
            addSceneryStyled(kAviationDefenseFlightwayModelPath, anchor, { 1.0f, 1.0f, 1.0f }, {},
                kDefenseLoopLength, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 2, 0.015f);
        }
        // 低い外周施設と広い舗装面　ボス戦が長引いても同じ固定プールで続ける
        for (int index = 0; index < kBossApronModuleCount; ++index) {
            const Math::Vector3 anchor{ 0.0f, kBuildingY,
                kBossApronModuleLength * (static_cast<float>(index) + 0.5f) };
            const bool freight = index % 3 == 1;
            const Math::Vector3 rotate{ 0.0f, index % 3 == 2 ? kHalfPi * 2.0f : 0.0f, 0.0f };
            addSceneryStyled(freight ? kAviationBossApronFreightModelPath : kAviationBossApronModelPath,
                anchor, { 1.0f, 1.0f, 1.0f }, rotate,
                kBossApronLoopLength, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 1, 0.025f);
            addSceneryStyled(freight ? kAviationBossApronFreightGlazingModelPath : kAviationBossApronGlazingModelPath,
                anchor, { 1.0f, 1.0f, 1.0f }, rotate,
                kBossApronLoopLength, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 1, 0.16f);
            addSceneryStyled(kAviationBossApronSurfaceModelPath, anchor, { 1.0f, 1.0f, 1.0f }, {},
                kBossApronLoopLength, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 2, 0.015f);
        }
    }
    transparentOrder_.reserve(objects_.size());
    const bool ready = sceneryIndex <= first + count;
    if (ready) { boundsCache_.clear(); }
    return ready;
}

// 現在のレール距離から、区間ごとの背景をなじませるための比率を求める
Math::Vector2 RailScenery::DistrictWeights(float worldZ) const
{
    if (tutorial_) { return {}; }
    const auto blend = [worldZ](float start, float length) {
        if (start == -1.0f) { return 0.0f; }
        const float t = std::clamp((worldZ - start) / length, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    return { blend(defenseStartZ_, 96.0f), blend(plazaStartZ_, 144.0f) };
}

// ボス戦の広場が始まる前後位置を、本編の背景配置から求める
float RailScenery::BossApronStartZ() const
{
    if (tutorial_ || plazaStartZ_ == -1.0f) { return -1.0f; }
    const float defenseStartZ = defenseStartZ_ >= 0.0f ?
        (std::max)(defenseStartZ_, 450.0f) : defenseStartZ_;
    // 中盤の128m敷地の端へ接続　境界の確定時には全ての変更対象が描画範囲より先にある
    return defenseStartZ + std::ceil((plazaStartZ_ + 96.0f - defenseStartZ) /
        kDefenseModuleLength) * kDefenseModuleLength;
}

// 進行距離に応じて背景の表示範囲と使う区画を更新する
void RailScenery::Update(float railDistance)
{
    // 序盤の敷地（Z=396まで）と路面（Z=450まで）を終えてから中盤へ入る
    // 負の原点は明示的なDebugフェーズ移動だけで使用する
    const float defenseStartZ = defenseStartZ_ >= 0.0f ?
        (std::max)(defenseStartZ_, 450.0f) : defenseStartZ_;
    const float bossApronStartZ = BossApronStartZ();
    const float nearLocalZ = tutorial_ ? kSceneryNearLocalZ : kSceneryRecycleNearLocalZ;
    visibleCount_ = 0;
    for (Object& scenery : objects_) {
        if (!scenery.object) {
            continue;
        }

        float localZ = scenery.isLandmark ? scenery.anchor.z - railDistance : WrapSceneryLocalZ(
            scenery.anchor.z -
                railDistance * scenery.speedMultiplier +
                scenery.phase + scenery.halfDepth,
            scenery.loopLength, nearLocalZ) - scenery.halfDepth;
        if (scenery.isDefenseDistrict || scenery.isBossApron) {
            const float startZ = scenery.isBossApron ? bossApronStartZ : defenseStartZ;
            if (startZ == -1.0f) {
                scenery.isVisible = false;
                continue;
            }
            // 原点は区画の境界へ固定する　周期全体を保持し、近・遠クリップの外でだけ循環
            const float cycle = startZ + scenery.anchor.z - railDistance +
                scenery.halfDepth - nearLocalZ;
            localZ = cycle - std::floor(cycle / scenery.loopLength) * scenery.loopLength +
                nearLocalZ - scenery.halfDepth;
        }
        scenery.currentLocalZ = localZ;
        scenery.isVisible =
            localZ + scenery.halfDepth >= nearLocalZ &&
            localZ - scenery.halfDepth <= scenery.drawFarLocalZ;
        if (scenery.isDefenseDistrict) {
            const float worldZ = railDistance + localZ;
            // 広場の舗装面へ隙間なく接続し、建物の敷地同士は重ねない
            scenery.isVisible &= worldZ - scenery.halfDepth >= defenseStartZ - 0.01f &&
                (bossApronStartZ == -1.0f || worldZ + scenery.halfDepth <= bossApronStartZ + 0.01f);
        } else if (scenery.isBossApron) {
            scenery.isVisible &= railDistance + localZ - scenery.halfDepth >= bossApronStartZ - 0.01f;
        }
        if (bossApronStartZ != -1.0f && (scenery.isRoad || scenery.isTrackside ||
            scenery.isRoadDetail)) {
            // 舗装は専用面で繋がっている　18m道路片の端まで判定し、最後の歩道が広場へ突き出さない
            const float halfDepth = scenery.isRoad ? 9.0f : scenery.halfDepth;
            scenery.isVisible &= railDistance + localZ + halfDepth < bossApronStartZ;
        }
        if (scenery.isVisible) {
            ++visibleCount_;
        }
        const float driftPhase =
            railDistance * scenery.driftSpeed + scenery.phase * 0.031f;
        const float railCurve =
            std::sin((railDistance + localZ) * 0.018f) * 1.25f;
        const float nearRate = std::clamp(
            1.0f -
                (localZ - kSceneryNearLocalZ) /
                    (kSceneryFarLocalZ - kSceneryNearLocalZ),
            0.0f,
            1.0f);

        Math::Vector3 position{
            scenery.anchor.x +
                std::sin(driftPhase) * scenery.lateralDrift +
                railCurve * scenery.curveInfluence *
                    (0.25f + nearRate * 0.75f),
            scenery.anchor.y +
                std::cos(driftPhase * 0.82f) * scenery.verticalDrift,
            railDistance + localZ
        };
        const float yawWobble =
            std::abs(scenery.driftSpeed) > 0.0001f ?
            std::sin(driftPhase * 0.55f) * 0.08f :
            0.0f;
        const Math::Vector3 rotate{
            scenery.rotate.x,
            scenery.rotate.y + yawWobble,
            scenery.rotate.z + railDistance * scenery.rollSpeed
        };

        Math::Vector3 scale = scenery.scale;
        if (!tutorial_) {
            const auto district = DistrictWeights(position.z);
            const auto value = [district](float avenue, float canyon, float plaza) {
                return Lerp(Lerp(avenue, canyon, district.x), plaza, district.y);
            };
            if (scenery.isBuilding) {
                const float side = scenery.anchor.x < 0.0f ? -1.0f : 1.0f;
                // 最も狭い街区でも旧配置の31.5mを下回らず、既存の退場ルートを塞がない
                const float facadeX = scenery.isBackRow ? value(54.0f, 46.0f, 89.0f) :
                    value(38.0f, 31.5f, 64.0f);
                position.x = side * facadeX;
                // 施設の接続部に建物がめり込まないよう、左右の敷地を空ける
                // ワールド座標から決めるため、近づいた途端に建物が動くことはない
                const float facilityDistance = std::abs(position.z - kTransitLandmarkWorldZ);
                const float setback = std::clamp((78.0f - facilityDistance) / 30.0f, 0.0f, 1.0f);
                position.x += side * setback * (scenery.isBackRow ? 8.0f : 13.0f);
                // 建物ごとの元の高さの差は残す　ボス広場は低い街並みと空でシルエットを抜く
                scale.y *= scenery.isBackRow ? value(0.95f, 1.65f, 0.90f) : value(0.80f, 1.65f, 0.72f);
                // 序盤だけを運航区画として構成する　ワールド位置に固定し、表示中には動かさない
                const float openingT = std::clamp((position.z - 390.0f) / 90.0f, 0.0f, 1.0f);
                const float opening = 1.0f - openingT * openingT * (3.0f - 2.0f * openingT);
                const float defenseEndT = bossApronStartZ == -1.0f ? 0.0f :
                    std::clamp((position.z - bossApronStartZ) / 128.0f, 0.0f, 1.0f);
                const float defense = district.x * (1.0f - defenseEndT * defenseEndT * (3.0f - 2.0f * defenseEndT));
                const float bossApronT = bossApronStartZ == -1.0f ? 0.0f :
                    std::clamp((position.z - bossApronStartZ + 96.0f) / 96.0f, 0.0f, 1.0f);
                const float bossApron = bossApronT * bossApronT * (3.0f - 2.0f * bossApronT);
                const float campus = (std::max)(opening, (std::max)(defense, bossApron));
                const float cluster = 0.5f + 0.5f * std::sin(position.z * 0.045f + side * 1.1f);
                const float hubT = std::clamp(1.0f - facilityDistance / 116.0f, 0.0f, 1.0f);
                const float hubSetback = hubT * hubT * (3.0f - 2.0f * hubT);
                const float apronT = std::clamp(1.0f - std::abs(position.z - 351.0f) / 74.0f, 0.0f, 1.0f);
                const float openingX = scenery.isBackRow ?
                    128.0f + cluster * 18.0f + hubSetback * 12.0f + apronT * 18.0f :
                    94.0f + cluster * 16.0f + hubSetback * 13.0f + apronT * 22.0f;
                const float defenseX = scenery.isBackRow ? 138.0f + cluster * 18.0f : 106.0f + cluster * 12.0f;
                const float bossApronX = scenery.isBackRow ? 151.0f + cluster * 18.0f : 123.0f + cluster * 12.0f;
                const float campusX = Lerp(defenseX, bossApronX, bossApron);
                position.x = Lerp(position.x, side * Lerp(campusX, openingX, opening), campus);
                // 元の階高・窓の縦横比を戻し、管制塔の輪郭を一般の街並みより上へ抜く
                const float openingScale = scenery.scale.x * (scenery.isBackRow ? 1.02f : 1.0f);
                scale.y = Lerp(scale.y, openingScale * Lerp(1.0f, 0.72f, bossApron), campus);
                scale.z = Lerp(scale.z, scenery.scale.x, campus);
                const Math::Vector4 districtColor = scenery.isBackRow ?
                    Math::Vector4{ 0.79f, 0.83f, 0.88f, 1.0f } : Math::Vector4{ 0.91f, 0.93f, 0.94f, 1.0f };
                scenery.object->SetColor({ Lerp(scenery.color.x, districtColor.x, campus),
                    Lerp(scenery.color.y, districtColor.y, campus),
                    Lerp(scenery.color.z, districtColor.z, campus), 1.0f });
            } else if (scenery.isRoad) {
                // 回転後の道幅はローカルZ　長さXを変えると連結部に隙間ができる
                scale.z *= value(14.8f, 12.0f, 24.0f) / 14.0f;
            }
        }
        scenery.object->SetTranslate(position);
        scenery.object->SetRotate(rotate);
        scenery.object->SetScale(scale);
        scenery.object->Update();
        scenery.isInView = !camera_ || BoundsIntersectCamera(scenery.boundsMin, scenery.boundsMax,
            Math::Multiply(scenery.object->GetWorldMatrix(), camera_->GetViewProjectionMatrix()));
    }
}

// 表示中の地面や建物を描く　ガラスなどの透明部は指定された描画パスで重ねる
void RailScenery::Draw(ModelDrawPass drawPass)
{
    if (objects_.empty() || !common_) {
        return;
    }

    const BlendMode previousBlendMode = common_->GetBlendMode();
    const DepthDrawMode previousDepthMode = common_->GetDepthDrawMode();
    const bool transparent = drawPass == ModelDrawPass::Transparent;
    common_->SetDepthDrawMode(transparent ? DepthDrawMode::ReadOnly : DepthDrawMode::Normal);
    common_->SetBlendMode(transparent ? BlendMode::Normal : BlendMode::None);
    common_->CommonDrawSetting();

    if (transparent) {
        transparentOrder_.clear();
        for (const Object& scenery : objects_) {
            if (scenery.object && scenery.isVisible && scenery.isInView && scenery.object->HasTransparentMaterials()) {
                transparentOrder_.push_back(&scenery);
            }
        }
        // カメラを動かしても手前のガラスが奥の室内を消さないよう、深度は読み取りだけにする
        const Math::Matrix4x4 view = camera_ ? camera_->GetViewMatrix() : Math::MakeIdentity4x4();
        std::sort(transparentOrder_.begin(), transparentOrder_.end(),
            [&view](const Object* a, const Object* b) {
                return TransformCoord(a->object->GetTranslate(), view).z >
                    TransformCoord(b->object->GetTranslate(), view).z;
            });
        for (const Object* scenery : transparentOrder_) {
            scenery->object->Draw(drawPass);
        }
    } else {
        for (const Object& scenery : objects_) {
            if (scenery.object && scenery.isVisible && scenery.isInView) {
                scenery.object->Draw(drawPass);
            }
        }
    }

    common_->SetBlendMode(previousBlendMode);
    common_->SetDepthDrawMode(previousDepthMode);
    common_->CommonDrawSetting();
}

