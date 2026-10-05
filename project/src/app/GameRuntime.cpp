#include "app/GameRuntime.h"
#include "app/MenuUi.h"
#include "app/CombatHud.h"
#include "app/EnemyFireControl.h"

#include "engine/3d/ModelManager.h"
#include "engine/3d/ParticleManager.h"
#include "engine/3d/Skybox.h"
#include "engine/3d/TextureManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Logger.h"
#include "engine/io/Input.h"
#include "engine/audio/SoundManager.h"
#include "engine/scene/SceneSerializer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <IconsFontAwesome6.h>
#include <ImGuiFileDialog.h>
#include <implot.h>
#include <imgui_node_editor.h>
#include <TextEditor.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <span>
#include <string>
#include <string_view>

namespace {

constexpr const char* kGameSceneFilePath = "resources/game_scene.json";
constexpr const char* kGameEnvironmentTexturePath =
    "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";
constexpr const char* kFreePlayerModelPath = "free_models/player_candidates/Omen.gltf";
// Omenの機体中心から右ノズルまでの座標。左はXを反転。ワールド換算済み。
constexpr Math::Vector3 kPlayerExhaustNozzle{ 0.48f, -0.28f, -1.70f };
constexpr const char* kFreeEnemyModelPath =
    "free_models/quaternius_sci_fi_essentials/Enemy_EyeDrone_Static.gltf";
constexpr const char* kEnemyFormationModelPath =
    "free_models/Spitfire-20260707T202640Z-3-001/Spitfire/glTF/Spitfire.gltf";
constexpr const char* kEnemyFormationTexturePath =
    "resources/free_models/Spitfire-20260707T202640Z-3-001/Spitfire/Textures/Spitfire_Red.dds";
constexpr const char* kEnemySwoopModelPath =
    "free_models/Striker-20260707T202647Z-3-001/Striker/glTF/Striker.gltf";
constexpr const char* kEnemySwoopTexturePath =
    "resources/free_models/Striker-20260707T202647Z-3-001/Striker/Textures/Striker_Purple.dds";
constexpr const char* kEnemyShooterModelPath =
    "free_models/Pancake-20260707T202659Z-3-001/Pancake/glTF/Pancake.gltf";
constexpr const char* kEnemyShooterTexturePath =
    "resources/free_models/Pancake-20260707T202659Z-3-001/Pancake/Textures/Pancake_Orange.dds";
constexpr const char* kEnemyHeavyModelPath =
    "free_models/Challenger-20260707T202702Z-3-001/Challenger/glTF/Challenger.gltf";
constexpr const char* kEnemyHeavyTexturePath =
    "resources/free_models/Challenger-20260707T202702Z-3-001/Challenger/Textures/Challenger_Blue.dds";
constexpr const char* kBossModelPath =
    "free_models/Imperial-20260707T180457Z-3-001/Imperial/glTF/Imperial.gltf";
constexpr const char* kBossTexturePath =
    "resources/free_models/Imperial-20260707T180457Z-3-001/Imperial/Textures/Imperial_Purple.dds";
constexpr const char* kCityStreet4LaneModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Street_4Lane.gltf";
constexpr const char* kCityManholeCoverModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Prop_ManholeCover.gltf";
constexpr const char* kCityDrainModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Prop_Drain.gltf";
constexpr const char* kCityBuildingSmallModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Building_Small_1.gltf";
constexpr const char* kCityBuildingMediumModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Building_Medium_2_001.gltf";
constexpr const char* kCityBuildingLargeModelPath =
    "free_models/Downtown City MegaKit[Standard]/"
    "Exports/glTF (Godot)/Building_Large_2.gltf";
constexpr const char* kTransitLandmarkModelPath = "free_models/transit_landmark/TransitLandmark.gltf";
constexpr const char* kTracksideModelPath = "scenery/FlightTrackside.gltf";
constexpr const char* kAviationDistrictModelPath = "scenery/aviation_district/AviationDistrict.gltf";
constexpr const char* kAviationGlazingModelPath = "scenery/aviation_district/AviationGlazing.gltf";
constexpr const char* kAviationFlightwayModelPath = "scenery/aviation_district/AviationFlightway.gltf";
constexpr const char* kAviationDefenseModelPath = "scenery/aviation_district/AviationDefense.gltf";
constexpr const char* kAviationDefenseGlazingModelPath = "scenery/aviation_district/AviationDefenseGlazing.gltf";
constexpr const char* kAviationDefenseFlightwayModelPath = "scenery/aviation_district/AviationDefenseFlightway.gltf";
constexpr const char* kAviationDefenseRelayModelPath = "scenery/aviation_district/AviationDefenseRelay.gltf";
constexpr const char* kAviationDefenseRelayGlazingModelPath = "scenery/aviation_district/AviationDefenseRelayGlazing.gltf";
constexpr const char* kAviationBossApronModelPath = "scenery/aviation_district/AviationBossApron.gltf";
constexpr const char* kAviationBossApronGlazingModelPath = "scenery/aviation_district/AviationBossApronGlazing.gltf";
constexpr const char* kAviationBossApronSurfaceModelPath = "scenery/aviation_district/AviationBossApronSurface.gltf";
constexpr const char* kAviationBossApronFreightModelPath = "scenery/aviation_district/AviationBossApronFreight.gltf";
constexpr const char* kAviationBossApronFreightGlazingModelPath = "scenery/aviation_district/AviationBossApronFreightGlazing.gltf";
constexpr const char* kAviationTerrainModelPath = "scenery/aviation_district/AviationTerrain.gltf";
constexpr float kDefenseStartWorldZ = 450.0f;
constexpr float kPlazaStartWorldZ = 866.0f;
constexpr float kDefenseModuleLength = 128.0f;
constexpr int kDefenseModuleCount = 6;
constexpr float kDefenseLoopLength = kDefenseModuleLength * kDefenseModuleCount;
constexpr float kBossApronModuleLength = 256.0f;
constexpr int kBossApronModuleCount = 4;
constexpr float kBossApronLoopLength = kBossApronModuleLength * kBossApronModuleCount;
constexpr float kTransitLandmarkWorldZ = 240.0f; // 市街地の途中に固定。撃破速度では位置を変えない。
constexpr int kTargetPlayerBulletPoolCount = 40; // 8f間隔・寿命280fのフィーバー空撃ちも起動時の固定枠で賄う。
constexpr int kTargetEnemyBulletPoolCount = 64; // 連射・扇状弾も起動時の固定プールだけで賄う。
// TitleSceneのPrepareScene中に通常の予備数まで作る。開始直後の大量GPU確保を避ける。
constexpr int kInitialPlayerBulletPoolCount = kTargetPlayerBulletPoolCount;
constexpr int kInitialEnemyBulletPoolCount = kTargetEnemyBulletPoolCount;
constexpr int kTargetHitEffectObjectPoolCount = 320; // 編隊同時撃破＋着弾＋回収まで起動時の固定枠で賄う。
constexpr int kInitialHitEffectObjectPoolCount = kTargetHitEffectObjectPoolCount;
constexpr float kChargeSplashRadius = 3.4f; // 敵の中心間の距離。編隊中央なら左右へ届き、端なら反対端へ届かない。
constexpr int kChargeSplashDamage = 3; // 小型機一機分。直撃対象には重ねず、爆風からの再誘爆もしない。
constexpr int kRewardHeartPoolCount = 40;
constexpr int kRewardHeartScoreValue = 25;
constexpr int kDepthCueEffectCount = 18;
constexpr float kDepthCueNearLocalZ = -10.0f;
constexpr float kDepthCueFarLocalZ = 148.0f;
constexpr float kDepthCueLoopLength = 172.0f;
constexpr float kContactShadowY = -2.82f;
constexpr float kContactShadowHalfPi = 1.57079632679f;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kRailCameraCurveFrequency = 0.050f;
constexpr float kRailCameraDriftFrequency = 0.027f;
constexpr float kGameplayCameraBaseFovY = 0.590f;
constexpr float kRushCameraBaseFovY = 0.710f; // 通常飛行は広角化し、手前の路面を大きく流す。
constexpr float kRushCameraDistance = 14.6f; // 広角化しても自機が小さくなりすぎない距離。
constexpr float kMainFlightSpeedMultiplier = 1.65f;
constexpr float kGameplayCameraInitialDistance = 15.8f;
constexpr float kGameplayCameraBaseDistance = 16.10f;
constexpr float kGameplayCameraEdgeRollStart = 5.25f;
constexpr float kGameplayCameraEdgeRollRange = 3.65f;
constexpr float kGameplayCameraEdgeRollMax = 0.055f;
constexpr float kPlayerExhaustRiseResponse = 0.24f;
constexpr float kPlayerExhaustFallResponse = 0.075f;
constexpr float kPlayerExhaustParticleInterval = 1.0f / 38.0f;
constexpr const char* kPlayerExhaustGpuLeftOuterGroup =
    "player_exhaust_gpu_left_outer";
constexpr const char* kPlayerExhaustGpuRightOuterGroup =
    "player_exhaust_gpu_right_outer";
constexpr const char* kPlayerExhaustGpuLeftCoreGroup =
    "player_exhaust_gpu_left_core";
constexpr const char* kPlayerExhaustGpuRightCoreGroup =
    "player_exhaust_gpu_right_core";
constexpr float kJustDodgeCameraClosePushIn = 3.35f;
constexpr float kJustDodgeCameraPlayerFollowX = 0.115f;
constexpr float kJustDodgeCameraPlayerFollowY = 0.105f;
constexpr float kJustDodgeCameraLowAngle = 0.18f;
constexpr float kJustDodgeCameraFovTighten = 0.140f;
constexpr int kPlayerDodgeAfterimageIntervalFrames = 2;
constexpr float kPlayerDodgeAfterimageDuration = 34.0f;
constexpr int kSharedResourcePreloadStepCount = 23;
constexpr float kSceneryNearLocalZ = -24.0f;
// カメラの後方まで地表を残し、520mの遠クリップより外でだけ背景を循環させる。
constexpr float kSceneryRecycleNearLocalZ = -64.0f;
constexpr float kSceneryFarLocalZ = 540.0f;
constexpr float kStageClearDistance = 365.0f;
constexpr float kBossWarningDistance = 300.0f;
constexpr float kBossSpawnDistance = 330.0f;
constexpr float kStageTimelineCruiseSpeed = 0.090f;
constexpr int kStageEncounterBreatherFrames = 84;
constexpr float kEmptyEncounterMaxFrames = 60.0f;
constexpr float kEmptyBossApproachMaxFrames = 120.0f;
constexpr float kStageEncounterGroupingDistance = 10.5f;
constexpr int kBossWarningDuration = 150;
constexpr int kBossIntroDuration = 72;
constexpr int kBossDefeatFlashDuration = 44;
constexpr int kBossPhaseTransitionDuration = 36;
constexpr int kBossAimLockFrames = 14;
int BossWindupDuration(int pattern, int phase)
{
    constexpr int normal[] = { 48, 54, 78 };
    constexpr int enraged[] = { 38, 42, 62 };
    return phase >= 2 ? enraged[std::clamp(pattern, 0, 2)] : normal[std::clamp(pattern, 0, 2)];
}
constexpr int kBossMaxHp = 52;
constexpr int kMaxActiveStageEnemiesBeforeBoss = 7;
constexpr int kMaxStageEnemyEventsPerFrame = 2;
constexpr float kJustDodgeGrazePadding = 1.25f;
constexpr float kJustDodgeRailSlowScale = 0.28f;
constexpr int kJustDodgeChargeBonus = 24;
constexpr int kJustDodgeScoreBonus = 50;
constexpr int kJustDodgeFlashDuration = 46;
constexpr int kJustDodgeSlowDuration = 14; // 成功を見せる瞬間だけ減速し、飛行の勢いを切らない。
constexpr int kPlayerImpactFlashDuration = 10;
constexpr int kPlayerImpactSlowDuration = 8;
constexpr float kPlayerImpactRailSlowScale = 0.42f;
constexpr int kSniperTelegraphLeadFrames = 34;
constexpr int kFeverGaugeMax = 100;
constexpr int kFeverDurationFrames = 600;
constexpr int kFeverActivationFlashFrames = 90;
constexpr int kFeverRapidShotCooldown = 8;
constexpr int kFeverScoreMultiplier = 3;
constexpr int kFeverEncounterBreatherFrames = 18;
constexpr float kFeverRailSpeedMultiplier = 2.15f;
constexpr float kFeverRailAccelerationResponse = 0.15f;
constexpr int kDefeatChainDurationFrames = 210;
constexpr int kDefeatChainBreakFlashFrames = 42;
constexpr int kDefeatChainMaxCount = 99;
constexpr int kDefeatChainScoreTierSize = 4;
constexpr int kDefeatChainMaxScoreMultiplier = 4;
int gSharedResourcePreloadStep = 0;
bool gSharedResourcesPreloaded = false;
const char* gSharedResourcePreloadLabel = "Waiting";
float gSharedResourceLastStepMs = 0.0f;
float gSharedResourceTotalMs = 0.0f;

Math::Vector3 RotateLocalOffset(
    const Math::Vector3& offset,
    const Math::Vector3& rotate)
{
    const Math::Matrix4x4 rotateMatrix =
        Math::Multiply(
            Math::MakeRotateXMatrix(rotate.x),
            Math::Multiply(
                Math::MakeRotateYMatrix(rotate.y),
                Math::MakeRotateZMatrix(rotate.z)));
    return {
        offset.x * rotateMatrix.m[0][0] +
            offset.y * rotateMatrix.m[1][0] +
            offset.z * rotateMatrix.m[2][0],
        offset.x * rotateMatrix.m[0][1] +
            offset.y * rotateMatrix.m[1][1] +
            offset.z * rotateMatrix.m[2][1],
        offset.x * rotateMatrix.m[0][2] +
            offset.y * rotateMatrix.m[1][2] +
            offset.z * rotateMatrix.m[2][2],
    };
}

struct EnemySpawnPattern {
    float x;
    float y;
    Enemy::Behavior behavior;
    Enemy::EntryStyle entryStyle;
};

struct StageSegment {
    float startDistance;
    float endDistance;
    float targetSpeed;
    float yawBias;
    float rollBias;
    float liftBias;
    float fovBoost;
    const char* name;
};

struct StageRailEvent {
    float distance;
    int spawnTimerCap;
    float shakePower;
    int shakeDuration;
};

struct StageEnemySpawnEvent {
    float distance;
    float x;
    float y;
    float leadDistance;
    Enemy::Behavior behavior;
    Enemy::EntryStyle entryStyle;
    float shakePower;
    int shakeDuration;
    int maxHpOverride;
    float scaleMultiplier;
    const char* beatName;
};

constexpr int kWaveCount = 3;

constexpr StageSegment kStageSegments[] = {
    {   0.0f,  42.0f, 0.160f,  0.000f,  0.000f,  0.00f, 0.048f, "Opening" },
    {  42.0f, 104.0f, 0.205f, -0.022f,  0.042f,  0.16f, 0.068f, "Side sweep" },
    { 104.0f, 170.0f, 0.185f,  0.018f, -0.036f,  0.34f, 0.062f, "Climb and dodge" },
    { 170.0f, 230.0f, 0.235f,  0.000f, -0.055f, -0.16f, 0.102f, "Fast dive" },
    { 230.0f, 999.0f, 0.198f,  0.018f,  0.034f,  0.08f, 0.068f, "Final approach" }
};

constexpr StageRailEvent kStageRailEvents[] = {
    {  12.0f,  0, 0.045f,  8 },
    {  54.0f,  3, 0.065f, 10 },
    { 112.0f,  2, 0.058f,  8 },
    { 176.0f,  1, 0.080f, 12 },
    { 246.0f,  0, 0.066f, 10 }
};


// 本編の配置。チュートリアルの案内・練習用HP・反撃制限は適用しない。
constexpr StageEnemySpawnEvent kStageEnemySpawnEvents[] = {
    {  10.0f, -2.7f, -0.5f, 42.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.020f,  4,  4, 1.08f, "Opening pair" },
    {  16.0f,  2.7f,  0.2f, 42.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.020f,  4,  4, 1.08f, "Opening pair" },
    {  28.0f,  0.0f,  1.4f, 50.0f, Enemy::Behavior::DiveBomber,    Enemy::EntryStyle::Direct,     0.030f,  7,  5, 1.08f, "Crossing assault" },
    {  32.0f,  5.1f,  1.2f, 48.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::RightSweep, 0.032f,  6,  5, 1.08f, "Crossing assault" },
    {  58.0f, -5.1f,  0.7f, 48.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::LeftSweep,  0.032f,  6,  5, 1.08f, "Cross sweep" },
    // 手前の三機を一掃するか、奥の狙撃機を先に処理するかを選べる組み合わせ。
    {  78.0f, -4.6f, -0.6f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 0.022f, 5, 3, 1.00f, "Sniper wing" },
    {  78.0f, -2.0f,  0.5f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 0.000f, 0, 3, 1.00f, "Sniper wing" },
    {  78.0f,  0.6f, -0.6f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 0.000f, 0, 3, 1.00f, "Sniper wing" },
    {  78.0f,  5.3f,  2.0f, 48.0f, Enemy::Behavior::Sniper,    Enemy::EntryStyle::PopShooter,    0.000f, 0, 6, 1.18f, "Sniper wing" },
    { 112.0f,  4.6f,  1.6f, 54.0f, Enemy::Behavior::DiveBomber,    Enemy::EntryStyle::RightSweep, 0.036f,  8,  5, 1.12f, "High dive" },
    { 124.0f, -2.9f, -0.4f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.024f,  5,  5, 1.12f, "Guard pair" },
    { 128.0f,  2.9f, -0.2f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.024f,  5,  5, 1.12f, "Guard pair" },
    { 148.0f, -4.6f,  1.5f, 54.0f, Enemy::Behavior::DiveBomber,    Enemy::EntryStyle::LeftSweep,  0.036f,  8,  5, 1.12f, "High dive" },
    { 160.0f,  0.0f,  1.45f, 54.0f, Enemy::Behavior::Shield,        Enemy::EntryStyle::Direct,     0.070f, 14, 11, 1.24f, "Shield wall" },
    { 174.0f, -5.4f,  0.55f, 52.0f, Enemy::Behavior::Crossfire,     Enemy::EntryStyle::LeftSweep,  0.048f,  9,  6, 1.12f, "Crossfire pair" },
    { 174.0f,  5.4f,  1.15f, 52.0f, Enemy::Behavior::Crossfire,     Enemy::EntryStyle::RightSweep, 0.048f,  9,  6, 1.12f, "Crossfire pair" },
    { 188.0f, -4.9f,  1.0f, 52.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::LeftSweep,  0.038f,  7,  5, 1.14f, "High-speed pass" },
    { 202.0f,  4.9f,  1.0f, 52.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::RightSweep, 0.038f,  7,  5, 1.14f, "High-speed pass" },
    { 214.0f,  0.0f,  1.0f, 50.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.030f,  6,  5, 1.18f, "Break formation" },
    { 228.0f, -3.8f,  0.6f, 48.0f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter, 0.040f,  8,  7, 1.18f, "Final crossfire" },
    { 234.0f,  3.8f,  0.6f, 48.0f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter, 0.040f,  8,  7, 1.18f, "Final crossfire" },
    { 258.0f,  0.0f,  1.9f, 48.0f, Enemy::Behavior::Support,       Enemy::EntryStyle::Direct,     0.050f, 10,  8, 1.16f, "Support gate" },
    { 258.0f, -2.9f, -0.5f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.030f,  6,  5, 1.16f, "Support gate" },
    { 258.0f,  2.9f, -0.5f, 44.0f, Enemy::Behavior::Formation,     Enemy::EntryStyle::VFormation, 0.030f,  6,  5, 1.16f, "Support gate" },
    // 終盤は左右を入れ替えて再登場。初見と同じ構え・攻撃なので学んだ対処を使える。
    { 274.0f,  4.6f, -0.6f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 0.030f, 6, 3, 1.00f, "Sniper wing reprise" },
    { 274.0f,  2.0f,  0.5f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 0.000f, 0, 3, 1.00f, "Sniper wing reprise" },
    { 274.0f, -0.6f, -0.6f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 0.000f, 0, 3, 1.00f, "Sniper wing reprise" },
    { 274.0f, -5.3f,  2.0f, 48.0f, Enemy::Behavior::Sniper,    Enemy::EntryStyle::PopShooter,    0.000f, 0, 6, 1.18f, "Sniper wing reprise" },
    { 286.0f, -5.2f,  1.2f, 54.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::LeftSweep,  0.034f,  7,  5, 1.12f, "Boss screen" },
    { 296.0f,  5.2f,  1.2f, 54.0f, Enemy::Behavior::Swoop,         Enemy::EntryStyle::RightSweep, 0.034f,  7,  5, 1.12f, "Boss screen" }
};

template <size_t Count>
constexpr bool IsStageEnemyScheduleValid(const StageEnemySpawnEvent (&events)[Count])
{
    for (size_t index = 0; index < Count; ++index) {
        const StageEnemySpawnEvent& event = events[index];
        if (event.distance < 0.0f || event.distance >= kBossSpawnDistance ||
            event.maxHpOverride <= 0 || event.scaleMultiplier <= 0.0f ||
            (index > 0 && event.distance < events[index - 1].distance)) {
            return false;
        }
    }
    return Count > 0;
}
static_assert(IsStageEnemyScheduleValid(kStageEnemySpawnEvents));

std::span<const StageEnemySpawnEvent> GetEnemySchedule(bool isTutorial)
{
    return isTutorial ? std::span<const StageEnemySpawnEvent>{} :
        std::span<const StageEnemySpawnEvent>(kStageEnemySpawnEvents);
}

constexpr EnemySpawnPattern kWaveOnePatterns[] = {
    { -2.8f, -0.7f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  0.0f,  0.3f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  2.8f, -0.7f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    { -4.2f,  1.0f, Enemy::Behavior::Swoop, Enemy::EntryStyle::LeftSweep },
    {  4.2f,  1.0f, Enemy::Behavior::Swoop, Enemy::EntryStyle::RightSweep },
    {  0.0f, -1.1f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter }
};

constexpr EnemySpawnPattern kWaveTwoPatterns[] = {
    { -5.2f,  0.8f, Enemy::Behavior::Swoop, Enemy::EntryStyle::LeftSweep },
    {  5.2f, -0.4f, Enemy::Behavior::Swoop, Enemy::EntryStyle::RightSweep },
    { -2.2f,  1.2f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    {  2.2f,  1.2f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    { -3.4f, -1.1f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  0.0f, -0.2f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  3.4f, -1.1f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  0.0f,  1.8f, Enemy::Behavior::Swoop, Enemy::EntryStyle::RightSweep }
};

constexpr EnemySpawnPattern kWaveThreePatterns[] = {
    { -4.6f,  1.4f, Enemy::Behavior::Swoop, Enemy::EntryStyle::LeftSweep },
    {  4.6f,  1.4f, Enemy::Behavior::Swoop, Enemy::EntryStyle::RightSweep },
    { -2.8f,  0.4f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    {  2.8f,  0.4f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    {  0.0f, -1.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    { -3.8f, -0.7f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    {  3.8f, -0.7f, Enemy::Behavior::Formation, Enemy::EntryStyle::VFormation },
    { -1.4f,  1.8f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    {  1.4f,  1.8f, Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::PopShooter },
    {  0.0f,  0.6f, Enemy::Behavior::Swoop, Enemy::EntryStyle::LeftSweep }
};

EnemySpawnPattern GetEnemySpawnPattern(int waveIndex, int sequenceIndex)
{
    if (waveIndex == 0) {
        return kWaveOnePatterns[
            sequenceIndex %
            (sizeof(kWaveOnePatterns) / sizeof(kWaveOnePatterns[0]))];
    }
    if (waveIndex == 1) {
        return kWaveTwoPatterns[
            sequenceIndex %
            (sizeof(kWaveTwoPatterns) / sizeof(kWaveTwoPatterns[0]))];
    }
    return kWaveThreePatterns[
        sequenceIndex %
        (sizeof(kWaveThreePatterns) / sizeof(kWaveThreePatterns[0]))];
}

constexpr int kChargeShotMax = 100;

constexpr const char* kRuntimeSceneModelItems[] = {
    "primitive_plane",
    "primitive_ring",
    "primitive_cylinder",
    "primitive_sphere",
    "primitive_triangle",
    "primitive_circle",
    "primitive_box",
    "primitive_torus",
    "primitive_cone",
    "AnimatedCube/AnimatedCube.gltf",
    "simpleSkin/simpleSkin.gltf",
    "human/sneakWalk.gltf",
    "human/walk.gltf"
};

float DistanceSquared(const Math::Vector3& a, const Math::Vector3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

float Lerp(float start, float end, float rate)
{
    return start + (end - start) * rate;
}

float PseudoRandom01(int index, float salt)
{
    const float value =
        std::sin((static_cast<float>(index) + 1.0f) * 12.9898f + salt * 78.233f) *
        43758.5453f;
    return value - std::floor(value);
}

bool BoundsIntersectCamera(const Math::Vector3& minimum, const Math::Vector3& maximum,
    const Math::Matrix4x4& worldViewProjection)
{
    // 同じクリップ面の外に全頂点がある箱だけを省く。カメラを跨ぐ大きな敷地も消さない。
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

float WrapSceneryLocalZ(float localZ, float loopLength, float nearLocalZ)
{
    const float safeLoopLength = (std::max)(loopLength, 1.0f);
    const float cycle = localZ - nearLocalZ;
    return cycle - std::floor(cycle / safeLoopLength) * safeLoopLength + nearLocalZ;
}

Math::Vector3 Lerp(const Math::Vector3& start, const Math::Vector3& end, float rate)
{
    return {
        Lerp(start.x, end.x, rate),
        Lerp(start.y, end.y, rate),
        Lerp(start.z, end.z, rate),
    };
}

Math::Vector3 TransformCoord(const Math::Vector3& v, const Math::Matrix4x4& m)
{
    const float x =
        v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
    const float y =
        v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
    const float z =
        v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
    const float w =
        v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
    if (std::abs(w) <= 0.0001f) {
        return { x, y, z };
    }
    return { x / w, y / w, z / w };
}

// 1280x720を基準に、ゲーム表示領域に合わせて文字と余白を一緒に拡縮する。
float GetCombatHudScale(const Math::Vector2& viewportSize)
{
    return CombatHud::Scale(viewportSize);
}

void DrawCombatHudText(
    ImDrawList* drawList,
    ImVec2 position,
    float fontSize,
    ImU32 color,
    const char* text,
    bool alignRight = false,
    bool number = false)
{
    (void)number; // 戦闘中は数字も和文と同じ書体にする。
    CombatHud::Readout(drawList, position, fontSize, color, text, alignRight);
}

// 枠を使わず、明るい背景でも数字を読める程度の局所的な下地だけを敷く。
void DrawCombatHudShade(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, bool alignRight)
{
    const ImU32 dark = IM_COL32(12, 13, 14, 164);
    const ImU32 clear = IM_COL32(12, 13, 14, 0);
    drawList->AddRectFilledMultiColor(min, max,
        alignRight ? clear : dark, alignRight ? dark : clear,
        alignRight ? IM_COL32(12, 13, 14, 40) : clear,
        alignRight ? clear : IM_COL32(12, 13, 14, 40));
}


} // namespace

GameRuntime::GameRuntime() = default;
GameRuntime::~GameRuntime() = default;

void GameRuntime::PlaySfx(const char* key, float pitch)
{
    if (sound_) {
        const std::string_view cue(key);
        const bool skill = IsPhantomRaidActive();
        const bool accent = gameplayElapsedSeconds_ < sfxAccentUntil_;
        float gain = 1.0f;
        // 主役となる一音に場所を空ける。被弾通知と技本体は抑えない。
        if (cue == "shot") { gain = skill ? 0.38f : accent ? 0.58f : 1.0f; }
        else if (cue == "hit") { gain = skill ? 0.45f : accent ? 0.68f : 1.0f; }
        else if (cue == "destroy") { gain = skill ? 0.24f : accent ? 0.56f : 1.0f; }
        if (sound_->Play(key, gain, pitch)) {
#ifdef _DEBUG
            if (cue == "slash") { ++phantomAudioPlays_[0]; }
            else if (cue == "slash_finish") { ++phantomAudioPlays_[1]; }
#endif
            const float hold = cue == "slash_finish" ? 0.55f : cue == "fever" ? 0.48f :
                cue == "skill_start" ? 0.25f : cue == "damage" ? 0.22f : cue == "charge" ? 0.18f : 0.0f;
            if (hold > 0.0f) {
                sfxAccentUntil_ = (std::max)(sfxAccentUntil_, gameplayElapsedSeconds_ + hold);
            }
        }
    }
}

void GameRuntime::UpdateMusic()
{
    if (!sound_) { return; }
    constexpr const char* keys[] = { "music_stage", "music_boss", "music_fever" };
    constexpr float volumes[] = { 0.28f, 0.32f, 0.30f };
    const int desired = (isGameOver_ || isGameClear_) ? -1 : (feverTimer_ > 0 ? 2 : (bossSpawned_ ? 1 : 0));
    if (desired != musicTrack_) {
        if (desired >= 0 && !sound_->PlayLoop(keys[desired])) {
            Logger::Log("Music loop unavailable; gameplay continues.\n");
        }
        musicTrack_ = desired;
    }
    const float dt = dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 0.1f) : 1.0f / 60.0f;
    const float duck = IsPhantomRaidActive() || bossPhaseTransitionTimer_ > 0 ? 0.42f : 1.0f;
    for (int index = 0; index < 3; ++index) {
        const float target = index == desired ? volumes[index] * duck * (IsTutorial() ? 0.70f : 1.0f) : 0.0f;
        auto& level = musicLevels_[index];
        const float speed = volumes[index] / (target > level ? 0.70f : 0.35f);
        level += std::clamp(target - level, -speed * dt, speed * dt);
        sound_->SetVolume(keys[index], level);
        if (index != desired && level <= 0.0f) { sound_->Stop(keys[index]); }
    }
}

void GameRuntime::SetSystems(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    SpriteCommon* spriteCommon,
    ImGuiManager* imguiManager,
    Input* input)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteCommon_ = spriteCommon;
    imguiManager_ = imguiManager;
    input_ = input;
}

bool GameRuntime::PreloadSharedResourceStep(
    DirectXCommon* dxCommon,
    SrvManager* srvManager)
{
    if (gSharedResourcesPreloaded) {
        gSharedResourcePreloadLabel = "Ready";
        return true;
    }
    if (!dxCommon || !srvManager) {
        gSharedResourcePreloadLabel = "Waiting for renderer";
        return false;
    }

    ModelManager* modelManager = ModelManager::GetInstance();
    const auto stepBegin = std::chrono::steady_clock::now();
    dxCommon->BeginTextureUploadBatch();
    switch (gSharedResourcePreloadStep) {
    case 0:
        gSharedResourcePreloadLabel = "Renderer resources";
        modelManager->Initialize(dxCommon, srvManager);
        modelManager->SetEnvironmentTexturePath(kGameEnvironmentTexturePath);
        break;
    case 1:
        gSharedResourcePreloadLabel = "Core game meshes";
        modelManager->CreateBox("game_player", 1.0f, 0.45f, 1.3f, "resources/uvChecker.png");
        modelManager->CreateSphere("game_bullet", 12, 24, 0.35f, "resources/human/white.png");
        modelManager->CreateSphere("game_enemy", 16, 32, 0.9f, "resources/uvChecker.png");
        modelManager->CreatePlane("primitive_plane", 8.0f, 8.0f, "resources/checkerBoard.png");
        modelManager->CreateTriangle("primitive_triangle", 1.6f, 1.6f, "resources/uvChecker.png");
        modelManager->CreateCircle("primitive_circle", 32, 0.9f, "resources/uvChecker.png");
        break;
    case 2:
        gSharedResourcePreloadLabel = "Reward and glow meshes";
        modelManager->CreateHeart("reward_heart", 48, 1.0f, "resources/human/white.png");
        modelManager->CreateSphere("reward_energy_shard", 2, 4, 0.5f, "resources/human/white.png");
        modelManager->CreateCircle("effect_glow_core", 48, 1.0f, "resources/effects/glow_core.png");
        modelManager->CreateCircle("effect_glow_ring", 64, 1.0f, "resources/effects/glow_ring.png");
        modelManager->CreateRing("rift_rim", 64, 1.0f, 0.94f, "resources/human/white.png");
        modelManager->CreateCircle("effect_spark_star", 48, 1.0f, "resources/effects/pal_star_spark.png");
        modelManager->CreateCircle("effect_bullet_glow", 48, 1.0f, "resources/effects/glow_core.png");
        modelManager->CreateCircle("effect_contact_shadow", 64, 1.0f, "resources/effects/glow_core.png");
        break;
    case 3:
        gSharedResourcePreloadLabel = "Projectile effect meshes";
        modelManager->CreatePlane("effect_bullet_trail", 1.0f, 1.0f, "resources/effects/bullet_trail.png");
        modelManager->CreatePlane("effect_player_bullet_core", 1.0f, 1.0f, "resources/effects/rail_player_bolt_core.png");
        modelManager->CreatePlane("effect_player_bullet_trail", 1.0f, 1.0f, "resources/effects/rail_player_bolt_trail.png");
        modelManager->CreatePlane("effect_player_charge_core", 1.0f, 1.0f, "resources/effects/rail_charge_lance_core.png");
        modelManager->CreatePlane("effect_player_charge_trail", 1.0f, 1.0f, "resources/effects/rail_charge_lance_trail.png");
        modelManager->CreateRing("effect_enemy_bullet_core", 48, 0.5f, 0.39f, "resources/human/white.png");
        modelManager->CreateRing("effect_combat_shock", 64, 0.5f, 0.492f, "resources/human/white.png");
        modelManager->CreateTriangle("effect_combat_blade", 1.0f, 1.0f, "resources/human/white.png");
        modelManager->CreateCircle("effect_combat_disc", 48, 0.5f, "resources/human/white.png");
        modelManager->CreateArc("effect_combat_arc", 40, 0.5f, 0.038f, 3.2f, "resources/human/white.png");
        modelManager->CreatePlane("effect_enemy_bullet_tail", 1.0f, 1.0f, "resources/effects/rail_enemy_tail.png");
        modelManager->CreatePlane("effect_impact_burst", 1.0f, 1.0f, "resources/effects/pal_impact_burst.png");
        modelManager->CreatePlane("effect_magic_shard", 1.0f, 1.0f, "resources/effects/pal_magic_shard.png");
        modelManager->CreatePlane("effect_explosion_fireball", 1.0f, 1.0f, "resources/effects/rail_explosion_fireball.png");
        modelManager->CreatePlane("effect_explosion_smoke", 1.0f, 1.0f, "resources/effects/rail_explosion_smoke.png");
        modelManager->CreatePlane("effect_explosion_sparks", 1.0f, 1.0f, "resources/effects/rail_explosion_sparks.png");
        break;
    case 4:
        gSharedResourcePreloadLabel = "Primitive scene meshes";
        modelManager->CreateRing("primitive_ring", 32, 2.0f, 1.0f, "resources/gradationLine.png");
        modelManager->CreateSphere("primitive_sphere", 16, 32, 1.0f, "resources/uvChecker.png");
        modelManager->CreateTorus("primitive_torus", 32, 16, 0.8f, 0.3f, "resources/uvChecker.png");
        modelManager->CreateCylinder("primitive_cylinder", 32, 1.2f, 1.2f, 2.5f, "resources/gradationLine.png");
        modelManager->CreateCone("primitive_cone", 32, 0.8f, 1.6f, "resources/uvChecker.png");
        modelManager->CreateBox("primitive_box", 1.5f, 1.5f, 1.5f, "resources/uvChecker.png");
        break;
    case 5:
        gSharedResourcePreloadLabel = "Environment texture";
        TextureManager::GetInstance()->LoadTexture(kGameEnvironmentTexturePath);
        break;
    case 6:
        gSharedResourcePreloadLabel = "Gameplay shaders";
        dxCommon->CompileShader(L"shaders/Object3D.VS.hlsl", L"vs_6_0");
        dxCommon->CompileShader(L"shaders/Object3D.PS.hlsl", L"ps_6_0");
        dxCommon->CompileShader(L"shaders/Object3DShadow.VS.hlsl", L"vs_6_0");
        dxCommon->CompileShader(L"shaders/Skybox.VS.hlsl", L"vs_6_0");
        dxCommon->CompileShader(L"shaders/Skybox.PS.hlsl", L"ps_6_0");
        break;
    case 7:
        gSharedResourcePreloadLabel = "Player ship model";
        modelManager->LoadModel(kFreePlayerModelPath);
        break;
    case 8:
        gSharedResourcePreloadLabel = "Enemy ship models";
        modelManager->LoadModel(kEnemyFormationModelPath);
        modelManager->LoadModel(kEnemySwoopModelPath);
        modelManager->LoadModel(kEnemyShooterModelPath);
        modelManager->LoadModel(kEnemyHeavyModelPath);
        modelManager->LoadModel(kFreeEnemyModelPath);
        TextureManager::GetInstance()->LoadTexture(kEnemyFormationTexturePath);
        TextureManager::GetInstance()->LoadTexture(kEnemySwoopTexturePath);
        TextureManager::GetInstance()->LoadTexture(kEnemyShooterTexturePath);
        TextureManager::GetInstance()->LoadTexture(kEnemyHeavyTexturePath);
        TextureManager::GetInstance()->LoadTexture(kBossTexturePath);
        break;
    case 9:
        gSharedResourcePreloadLabel = "City road model";
        modelManager->LoadModel(kCityStreet4LaneModelPath);
        break;
    case 10:
        gSharedResourcePreloadLabel = "City street detail models";
        modelManager->LoadModel(kCityManholeCoverModelPath);
        modelManager->LoadModel(kCityDrainModelPath);
        break;
    case 11:
        gSharedResourcePreloadLabel = "City building small model";
        modelManager->LoadModel(kCityBuildingSmallModelPath);
        break;
    case 12:
        gSharedResourcePreloadLabel = "City building medium model";
        modelManager->LoadModel(kCityBuildingMediumModelPath);
        break;
    case 13:
        gSharedResourcePreloadLabel = "City building large model";
        modelManager->LoadModel(kCityBuildingLargeModelPath);
        break;
    case 14:
        gSharedResourcePreloadLabel = "Boss spaceship model";
        modelManager->LoadModel(kBossModelPath);
        break;
    case 15:
        gSharedResourcePreloadLabel = "Transit facility model";
        modelManager->LoadModel(kTransitLandmarkModelPath);
        break;
    case 16:
        gSharedResourcePreloadLabel = "Trackside model";
        modelManager->LoadModel(kTracksideModelPath);
        break;
    case 17:
        gSharedResourcePreloadLabel = "Aviation district model";
        modelManager->LoadModel(kAviationDistrictModelPath);
        modelManager->LoadModel(kAviationGlazingModelPath);
        modelManager->LoadModel(kAviationFlightwayModelPath);
        break;
    case 18:
        gSharedResourcePreloadLabel = "Defense district model";
        modelManager->LoadModel(kAviationDefenseModelPath);
        modelManager->LoadModel(kAviationDefenseGlazingModelPath);
        modelManager->LoadModel(kAviationDefenseFlightwayModelPath);
        break;
    case 19:
        gSharedResourcePreloadLabel = "Boss apron model";
        modelManager->LoadModel(kAviationBossApronModelPath);
        modelManager->LoadModel(kAviationBossApronGlazingModelPath);
        modelManager->LoadModel(kAviationBossApronSurfaceModelPath);
        break;
    case 20:
        gSharedResourcePreloadLabel = "Communications court model";
        modelManager->LoadModel(kAviationDefenseRelayModelPath);
        modelManager->LoadModel(kAviationDefenseRelayGlazingModelPath);
        break;
    case 21:
        gSharedResourcePreloadLabel = "Freight apron model";
        modelManager->LoadModel(kAviationBossApronFreightModelPath);
        modelManager->LoadModel(kAviationBossApronFreightGlazingModelPath);
        break;
    case 22:
        gSharedResourcePreloadLabel = "Continuous terrain model";
        modelManager->LoadModel(kAviationTerrainModelPath);
        break;
    default:
        gSharedResourcesPreloaded = true;
        gSharedResourcePreloadLabel = "Ready";
        return true;
    }
    dxCommon->EndTextureUploadBatch();

    const auto stepEnd = std::chrono::steady_clock::now();
    gSharedResourceLastStepMs =
        std::chrono::duration<float, std::milli>(stepEnd - stepBegin).count();
    gSharedResourceTotalMs += gSharedResourceLastStepMs;

    ++gSharedResourcePreloadStep;
    if (gSharedResourcePreloadStep >= kSharedResourcePreloadStepCount) {
        gSharedResourcesPreloaded = true;
        gSharedResourcePreloadLabel = "Ready";
    }
    return gSharedResourcesPreloaded;
}

bool GameRuntime::AreSharedResourcesPreloaded()
{
    return gSharedResourcesPreloaded;
}

int GameRuntime::GetSharedResourcePreloadStep()
{
    return std::clamp(
        gSharedResourcePreloadStep,
        0,
        kSharedResourcePreloadStepCount);
}

int GameRuntime::GetSharedResourcePreloadStepCount()
{
    return kSharedResourcePreloadStepCount;
}

const char* GameRuntime::GetSharedResourcePreloadLabel()
{
    return gSharedResourcePreloadLabel;
}

float GameRuntime::GetSharedResourceLastStepMs()
{
    return gSharedResourceLastStepMs;
}

float GameRuntime::GetSharedResourceTotalMs()
{
    return gSharedResourceTotalMs;
}

void GameRuntime::Initialize(PlayMode mode)
{
    playMode_ = mode;
    isExitRequested_ = false;
    isRetryRequested_ = false;
    isMainGameRequested_ = false;
    tutorial_ = {};
    showControlsHelp_ = false;
    isPaused_ = false;
    pauseSelectedItem_ = resultSelectedItem_ = 0;
    resultSoundPlayed_ = false;
    isGameClear_ = false;
    isGameOver_ = false;
    bossWarningTriggered_ = false;
    bossSpawned_ = false;
    bossDefeated_ = false;
    isPerformanceOverlayVisible_ = false;
    showSkybox_ = true;
    currentWaveIndex_ = 0;
    spawnedEnemyCountInWave_ = 0;
    defeatedEnemyCountInWave_ = 0;
    spawnSequenceIndex_ = 0;
    defeatedEnemyCount_ = 0;
    escapedEnemyCount_ = 0;
    playerShotsFired_ = 0;
    playerHitCount_ = 0;
    playerDamageCount_ = 0;
    feverActivationCount_ = 0;
    justDodgeCount_ = 0;
    defeatChainCount_ = 0;
    defeatChainTimer_ = 0;
    defeatChainBreakFlashTimer_ = 0;
    maxDefeatChainCount_ = 0;
    score_ = 0;
    gameplayElapsedSeconds_ = 0.0f;
    enemySpawnTimer_ = 0;
    enemyShotTimer_ = 32;
    enemyAimVelocity_ = {};
    bossWarningTimer_ = 0;
    bossIntroTimer_ = 0;
    bossDefeatFlashTimer_ = 0;
    bossPhaseTransitionTimer_ = 0;
    bossAttackCooldown_ = 0;
    bossAttackStepTimer_ = 0;
    bossAttackStep_ = -1;
    bossAttackPattern_ = 0;
    bossAttackSequence_ = 0;
    bossShotsFired_ = 0;
    bossPhase_ = 1;
    bossCounterTimer_ = 0;
    bossCounterDuration_ = 1;
    bossAimPoint_ = {};
    bossDefeatPosition_ = {};
    resultTransitionTimer_ = -1;
    railDistance_ = 0.0f;
    // 地形は開始時から固定する。撃破速度による区間確定で視界内の施設を出現・変形させない。
    sceneryCanyonStartZ_ = IsTutorial() ? -1.0f : kDefenseStartWorldZ;
    sceneryPlazaStartZ_ = IsTutorial() ? -1.0f : kPlazaStartWorldZ;
    railSpeed_ = IsTutorial() ? 0.145f : 0.235f;
    targetRailSpeed_ = railSpeed_;
    previousFlightTimeScale_ = 1.0f;
    flightReleaseKick_ = 0.0f;
    flightCameraMotion_ = {};
    flightCameraMotion_.previousSpeed = railSpeed_;
    stageProgress_ = 0.0f;
    stageTimelineSpeed_ = 0.0f;
    stageTimelineWasBlocked_ = false;
    stageEncounterBreatherTimer_ = 0;
    stageEmptyFrames_ = 0.0f;
    stageCameraYawBias_ = 0.0f;
    stageCameraRollBias_ = 0.0f;
    stageCameraLiftBias_ = 0.0f;
    stageCameraFovBoost_ = 0.0f;
    playerExhaustThrust_ = 0.0f;
    playerExhaustParticleTimer_ = 0.0f;
    nextPlayerExhaustParticleIndex_ = 0;
    stageSectionName_ = "Opening";
    stageCombatBeatName_ = "Intro";
    stageRailEventTriggered_.fill(false);
    stageEnemyEventTriggered_.assign(GetEnemySchedule(IsTutorial()).size(), false);
    shootCooldown_ = 0;
    shootBufferTimer_ = 0;
    chargeTimer_ = 0;
    chargeFlashTimer_ = 0;
    feverGauge_ = 0;
    feverTimer_ = 0;
    feverActivationFlashTimer_ = 0;
    feverSpeedEffectRate_ = 0.0f;
    playerDodgeAfterimageTimer_ = 0;
    justDodgeFlashTimer_ = 0;
    justDodgeSlowTimer_ = 0;
    playerImpactFlashTimer_ = 0;
    playerImpactFlashDuration_ = 1;
    playerImpactSlowTimer_ = 0;
    playerImpactSlowDuration_ = 1;
    playerImpactSlowScale_ = 1.0f;
    hitConfirmTimer_ = 0;
    hitConfirmDuration_ = 1;
    hitConfirmComboTimer_ = 0;
    hitConfirmComboCount_ = 0;
    hitConfirmStrength_ = 1.0f;
    hitConfirmCharged_ = false;
    hitConfirmBoss_ = false;
    hitConfirmDestroyed_ = false;
    playerDamageHudTimer_ = 0;
    playerDamageHudDuration_ = 1;
    playerDamageScreen_ = {};
    playerDamageDirection_ = { 0.0f, -1.0f };
    nextPlayerDodgeAfterimageIndex_ = 0;
    wasPlayerDodging_ = false;
    cameraShakeTimer_ = 0;
    playerBulletPoolMisses_ = 0;
    enemyBulletPoolMisses_ = 0;
    hitEffectObjectPoolMisses_ = 0;
    rewardHeartPoolMisses_ = 0;
    maxActivePlayerBullets_ = 0;
    maxActiveEnemyBullets_ = 0;
    justDodgedEnemyBullets_.clear();
    hitEffects_.clear();
    hitEffects_.reserve(32);
    hitEffectObjectPool_.clear();

    object3dCommon_ = std::make_unique<Object3dCommon>();
    object3dCommon_->Initialize(dxCommon_, srvManager_);
    object3dCommon_->SetEnvironmentTexturePath(kGameEnvironmentTexturePath);

    while (!PreloadSharedResourceStep(dxCommon_, srvManager_)) {
    }

    playerModel_ = ModelManager::GetInstance()->FindModel(kFreePlayerModelPath);
    if (!playerModel_) {
        playerModel_ = ModelManager::GetInstance()->FindModel("game_player");
    }
    bulletModel_ = ModelManager::GetInstance()->FindModel("game_bullet");
    enemyModel_ = ModelManager::GetInstance()->FindModel(kFreeEnemyModelPath);
    if (!enemyModel_) {
        enemyModel_ = ModelManager::GetInstance()->FindModel("game_enemy");
    }
    enemyFormationModel_ = ModelManager::GetInstance()->FindModel(kEnemyFormationModelPath);
    enemySwoopModel_ = ModelManager::GetInstance()->FindModel(kEnemySwoopModelPath);
    enemyShooterModel_ = ModelManager::GetInstance()->FindModel(kEnemyShooterModelPath);
    enemyHeavyModel_ = ModelManager::GetInstance()->FindModel(kEnemyHeavyModelPath);
    bossModel_ = ModelManager::GetInstance()->FindModel(kBossModelPath);
    if (!bossModel_) {
        bossModel_ = enemyModel_;
    }
    effectGlowCoreModel_ = ModelManager::GetInstance()->FindModel("effect_glow_core");
    effectGlowRingModel_ = ModelManager::GetInstance()->FindModel("effect_glow_ring");
    effectSparkStarModel_ = ModelManager::GetInstance()->FindModel("effect_spark_star");
    effectBulletGlowModel_ = ModelManager::GetInstance()->FindModel("effect_bullet_glow");
    effectBulletTrailModel_ = ModelManager::GetInstance()->FindModel("effect_bullet_trail");
    effectPlayerBulletCoreModel_ = ModelManager::GetInstance()->FindModel("effect_player_bullet_core");
    effectPlayerBulletTrailModel_ = ModelManager::GetInstance()->FindModel("effect_player_bullet_trail");
    effectPlayerChargeCoreModel_ = ModelManager::GetInstance()->FindModel("effect_player_charge_core");
    effectPlayerChargeTrailModel_ = ModelManager::GetInstance()->FindModel("effect_player_charge_trail");
    effectEnemyBulletCoreModel_ = ModelManager::GetInstance()->FindModel("effect_enemy_bullet_core");
    effectEnemyBulletTailModel_ = ModelManager::GetInstance()->FindModel("effect_enemy_bullet_tail");
    effectImpactBurstModel_ = ModelManager::GetInstance()->FindModel("effect_impact_burst");
    effectCombatShockModel_ = ModelManager::GetInstance()->FindModel("effect_combat_shock");
    effectCombatBladeModel_ = ModelManager::GetInstance()->FindModel("effect_combat_blade");
    effectCombatDiscModel_ = ModelManager::GetInstance()->FindModel("effect_combat_disc");
    effectCombatArcModel_ = ModelManager::GetInstance()->FindModel("effect_combat_arc");
    effectMagicShardModel_ = ModelManager::GetInstance()->FindModel("effect_magic_shard");
    effectExplosionFireballModel_ = ModelManager::GetInstance()->FindModel("effect_explosion_fireball");
    effectExplosionSmokeModel_ = ModelManager::GetInstance()->FindModel("effect_explosion_smoke");
    effectExplosionSparksModel_ = ModelManager::GetInstance()->FindModel("effect_explosion_sparks");
    effectContactShadowModel_ = ModelManager::GetInstance()->FindModel("effect_contact_shadow");

    camera_ = std::make_unique<Camera>();
    camera_->SetRotate({ 0.06f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 2.65f, -kGameplayCameraInitialDistance });
    camera_->SetFovY(kGameplayCameraBaseFovY);
    camera_->SetAspectRatio(dxCommon_->GetPresentationAspectRatio());
    camera_->SetFarClip(520.0f);
    camera_->Update();
    object3dCommon_->SetDefaultCamera(camera_.get());

    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(dxCommon_, srvManager_, kGameEnvironmentTexturePath);
    skybox_->Update(camera_.get());

    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon_.get(), playerModel_);
    player_->SetRailZ(railDistance_);
    previousPlayerTranslate_ = player_->GetTranslate();
    const Math::Vector3 initialPlayerTranslate = player_->GetTranslate();
    cameraTranslate_ = {
        initialPlayerTranslate.x * 0.10f,
        (IsTutorial() ? 2.70f : 2.10f) + initialPlayerTranslate.y * 0.05f,
        railDistance_ - (IsTutorial() ? kGameplayCameraInitialDistance : kRushCameraDistance),
    };
    cameraRotate_ = {
        0.065f + initialPlayerTranslate.y * 0.003f,
        -initialPlayerTranslate.x * 0.004f,
        0.0f,
    };
    cameraFovY_ = IsTutorial() ? kGameplayCameraBaseFovY : kRushCameraBaseFovY;
    camera_->SetTranslate(cameraTranslate_);
    camera_->SetRotate(cameraRotate_);
    camera_->SetFovY(cameraFovY_);
    camera_->Update();
    if (skybox_) {
        skybox_->Update(camera_.get());
    }

    PrewarmBulletPools();
    PrewarmHitEffectObjectPool();
    InitializeRailScenery();
    InitializeDepthCueEffects();

    LoadSceneObjects(kGameSceneFilePath);
    InitializeRewardHearts();
    InitializePlayerDodgeAfterimages();
    InitializePlayerFlightAura();
    InitializePlayerExhaustParticles();
    InitializeContactShadows();
    InitializePhantomRaid();
    InitializeRifts();

    musicLevels_.fill(0.0f);
    musicTrack_ = -1;
    sfxAccentUntil_ = 0.0f;
    sound_ = std::make_unique<SoundManager>();
    if (sound_->Initialize()) {
        // 効果音25枠を事前確保。チャージ2枠＋技の発動1枠へ分離し、合計数は増やさない。
        const auto varied = [this](const char* key, uint32_t voices, float volume, float interval, float pitch) {
            const std::string base = std::string("resources/audio/combat/") + key;
            sound_->LoadVariations(key, { base + ".wav", base + "_02.wav", base + "_03.wav" },
                voices, volume, interval, pitch, 0.05f);
        };
        varied("shot", 4, 0.38f, 0.065f, 0.012f);
        sound_->Load("charge", "resources/audio/combat/charge.wav", 2, 0.51f, 0.16f, 0.012f, 0.025f);
        sound_->Load("skill_start", "resources/audio/combat/skill_start.wav", 1, 0.52f, 0.4f);
        varied("hit", 3, 0.35f, 0.075f, 0.025f);
        varied("destroy", 3, 0.49f, 0.12f, 0.025f);
        sound_->Load("damage", "resources/audio/combat/damage.wav", 2, 0.60f, 0.15f, 0.012f);
        sound_->Load("dodge", "resources/audio/combat/dodge.wav", 2, 0.40f, 0.12f, 0.02f);
        sound_->Load("fever", "resources/audio/combat/fever.wav", 1, 0.59f, 0.5f);
        sound_->Load("clear", "resources/audio/combat/clear.wav", 1, 0.54f, 1.0f);
        sound_->Load("fail", "resources/audio/combat/fail.wav", 1, 0.48f, 1.0f);
        sound_->Load("skill_ready", "resources/audio/combat/skill_ready.wav", 1, 0.29f, 0.5f);
        InitializePhantomAudio();
        // BGM専用3枠を入場時に確保。状態切替では再ロード・ボイス生成を行わない。
        sound_->Load("music_stage", "resources/audio/music/stage.wav", 1, 0.0f, 0.0f);
        sound_->Load("music_boss", "resources/audio/music/boss.wav", 1, 0.0f, 0.0f);
        sound_->Load("music_fever", "resources/audio/music/fever.wav", 1, 0.0f, 0.0f);
    }

#ifdef ENABLE_DEBUG_GUI
    char* debugStartPhase = nullptr;
    size_t debugStartPhaseLength = 0;
    if (_dupenv_s(
            &debugStartPhase,
            &debugStartPhaseLength,
            "CG2_DEBUG_START_PHASE") == 0 &&
        debugStartPhase) {
        const std::string_view startPhase(debugStartPhase);
        if (startPhase == "Wave2") {
            DebugJumpToStagePhase(1);
        } else if (startPhase == "Wave3") {
            DebugJumpToStagePhase(2);
        } else if (startPhase == "Boss") {
            DebugJumpToStagePhase(3);
        } else if (startPhase == "SniperWing") {
            DebugJumpToStagePhase(4);
        }
    }
    std::free(debugStartPhase);
#endif
}

void GameRuntime::Finalize()
{
    ResetRifts();
    for (auto& rift : rifts_) {
        rift.rim.reset();
        rift.glow.reset();
    }
    for (auto& slash : phantomSlashes_) {
        slash.ghost.reset();
    }
    ResetPhantomRaid();
    sound_.reset(); // 再生を止めてボイスを破棄してから波形を解放する。
    sceneObjects_.clear();
    rewardHearts_.clear();
    for (PlayerDodgeAfterimage& afterimage : playerDodgeAfterimages_) {
        afterimage.object.reset();
        afterimage.isActive = false;
    }
    for (PlayerFlightAura& aura : playerFlightAuras_) {
        aura.object.reset();
    }
    for (PlayerExhaustParticle& particle : playerExhaustParticles_) {
        particle.object.reset();
        particle.isActive = false;
    }
    for (ContactShadow& shadow : contactShadows_) {
        shadow.object.reset();
    }
    playerBullets_.clear();
    enemyBullets_.clear();
    playerBulletPool_.clear();
    enemyBulletPool_.clear();
    homingBulletTargets_.clear();
    justDodgedEnemyBullets_.clear();
    enemies_.clear();
    hitEffects_.clear();
    hitEffectObjectPool_.clear();
    transparentSceneryDrawOrder_.clear();
    railSceneryObjects_.clear();
    depthCueEffects_.clear();
    player_.reset();
    camera_.reset();
    skybox_.reset();
    object3dCommon_.reset();

    playerModel_ = nullptr;
    bulletModel_ = nullptr;
    enemyModel_ = nullptr;
    enemyFormationModel_ = nullptr;
    enemySwoopModel_ = nullptr;
    enemyShooterModel_ = nullptr;
    enemyHeavyModel_ = nullptr;
    bossModel_ = nullptr;
    effectGlowCoreModel_ = nullptr;
    effectGlowRingModel_ = nullptr;
    effectSparkStarModel_ = nullptr;
    effectBulletGlowModel_ = nullptr;
    effectBulletTrailModel_ = nullptr;
    effectPlayerBulletCoreModel_ = nullptr;
    effectPlayerBulletTrailModel_ = nullptr;
    effectPlayerChargeCoreModel_ = nullptr;
    effectPlayerChargeTrailModel_ = nullptr;
    effectEnemyBulletCoreModel_ = nullptr;
    effectEnemyBulletTailModel_ = nullptr;
    effectImpactBurstModel_ = nullptr;
    effectCombatShockModel_ = nullptr;
    effectCombatBladeModel_ = nullptr;
    effectCombatDiscModel_ = nullptr;
    effectCombatArcModel_ = nullptr;
    effectMagicShardModel_ = nullptr;
    effectExplosionFireballModel_ = nullptr;
    effectExplosionSmokeModel_ = nullptr;
    effectExplosionSparksModel_ = nullptr;
    effectContactShadowModel_ = nullptr;

}

void GameRuntime::Update()
{
    // ゲーム進行を止めたままでも、表示先の変更だけは反映する。
    if (camera_) {
        Math::Vector2 viewportMin{}, viewportSize{};
        GetEffectiveHudViewportRect(viewportMin, viewportSize);
        const float aspect = viewportSize.x / (std::max)(viewportSize.y, 1.0f);
        if (aspect > 0.0f && camera_->GetAspectRatio() != aspect) {
            camera_->SetAspectRatio(aspect);
            camera_->Update();
            if (skybox_) { skybox_->Update(camera_.get()); }
            UpdateLockOnTarget();
        }
    }
#ifdef _DEBUG
    if (phantomPreviewPaused_) {
        DrawHud();
        DrawResultOverlay();
        DrawPauseOverlay();
        DrawControlsHelp();
        return;
    }
#endif
    if (HandleRuntimeShortcuts()) {
        return;
    }

    if (isPaused_) {
        // カメラ・敵・弾・衝突・ゲージを更新せず、最後のゲーム画面を描き続ける。
        DrawHud();
        DrawPauseOverlay();
        DrawControlsHelp();
        return;
    }

    if (!isGameOver_ && !isGameClear_ && dxCommon_) {
        gameplayElapsedSeconds_ +=
            std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 0.10f);
    }

    const Math::Vector3 previousRiftPlayerPosition = player_ ? player_->GetTranslate() : Math::Vector3{};
    UpdateRailProgress();
    UpdatePlayerAndCamera();
    UpdateFever();
    UpdatePhantomRaid();

    if (!isGameOver_ && !isGameClear_) {
        if (!IsPhantomRaidActive() && TutorialAllowsShooting()) {
            UpdatePlayerShooting();
        }
        UpdateEnemyActions();
    }

    UpdateWorldEntities();
    UpdateGameplayCollisions();
    if (player_) { UpdateRifts(previousRiftPlayerPosition, player_->GetTranslate()); }
    UpdateTutorialLesson();
    UpdateDefeatChain();
    AdvanceEnemyWaveIfCleared();
    UpdateLockOnTarget();
    DrawHud();
#ifdef ENABLE_DEBUG_GUI
    DrawPerformanceOverlay();
#endif
    DrawResultOverlay();
    DrawControlsHelp();
#ifdef ENABLE_DEBUG_GUI
    DrawEditorOverlayGuiRich();
#endif

    UpdateResultAndSceneObjects();
    UpdateMusic();
}

bool GameRuntime::HandleRuntimeShortcuts()
{
    menuInputConsumed_ = false;
    if (!isGameOver_ && !isGameClear_) {
        if (MenuUi::Pressed(input_, DIK_ESCAPE)) {
            if (showControlsHelp_) { showControlsHelp_ = false; }
            else { isPaused_ = !isPaused_; pauseSelectedItem_ = 0; }
        } else if (MenuUi::Pressed(input_, DIK_H)) {
            showControlsHelp_ = !showControlsHelp_;
            isPaused_ = showControlsHelp_;
            pauseSelectedItem_ = 0;
        } else if (showControlsHelp_ && MenuUi::Pressed(input_, DIK_RETURN)) {
            showControlsHelp_ = false;
            menuInputConsumed_ = true;
        }
    }
    if ((isGameOver_ || isGameClear_) && resultTransitionTimer_ <= 0 &&
        input_ && input_->TriggerKey(DIK_R)) {
        isRetryRequested_ = true;
        return true;
    }
#ifdef ENABLE_DEBUG_GUI
    // 提出版では診断表示・描画切り替え・強制シーン移動を受け付けない。
    if (input_ && input_->TriggerKey(DIK_F1)) {
        isEditorOverlayVisible_ = !isEditorOverlayVisible_;
        return true;
    }
    if (input_ && input_->TriggerKey(DIK_F3)) {
        isPerformanceOverlayVisible_ = !isPerformanceOverlayVisible_;
        return true;
    }
    if (input_ && input_->TriggerKey(DIK_F4)) {
        isPostEffectBypassEnabled_ = !isPostEffectBypassEnabled_;
        return true;
    }
    if (input_ && input_->TriggerKey(DIK_F5)) {
        showSkybox_ = !showSkybox_;
        return true;
    }
    if (input_ && input_->TriggerKey(DIK_F2)) {
        isExitRequested_ = true;
        return true;
    }
#endif
    return false;
}

void GameRuntime::UpdateStageDirector()
{
    if (IsTutorial()) {
        targetRailSpeed_ = 0.145f * (feverTimer_ > 0 ? kFeverRailSpeedMultiplier : 1.0f);
        stageSectionName_ = "Tutorial";
        return;
    }
    const StageSegment* activeSegment = &kStageSegments[0];
    for (const StageSegment& segment : kStageSegments) {
        if (stageProgress_ >= segment.startDistance &&
            stageProgress_ < segment.endDistance) {
            activeSegment = &segment;
            break;
        }
    }

    stageSectionName_ = activeSegment->name;
    currentWaveIndex_ =
        stageProgress_ < 104.0f ? 0 :
        stageProgress_ < 230.0f ? 1 :
        2;
    targetRailSpeed_ = activeSegment->targetSpeed * kMainFlightSpeedMultiplier *
        (feverTimer_ > 0 ? kFeverRailSpeedMultiplier : 1.0f);
    stageCameraYawBias_ =
        Lerp(stageCameraYawBias_, activeSegment->yawBias, 0.035f);
    stageCameraRollBias_ =
        Lerp(stageCameraRollBias_, activeSegment->rollBias, 0.035f);
    stageCameraLiftBias_ =
        Lerp(stageCameraLiftBias_, activeSegment->liftBias, 0.035f);
    stageCameraFovBoost_ =
        Lerp(stageCameraFovBoost_, activeSegment->fovBoost, 0.035f);

    const size_t eventCount =
        (std::min)(
            stageRailEventTriggered_.size(),
            sizeof(kStageRailEvents) / sizeof(kStageRailEvents[0]));
    for (size_t index = 0; index < eventCount; ++index) {
        const StageRailEvent& event = kStageRailEvents[index];
        if (stageRailEventTriggered_[index] ||
            stageProgress_ < event.distance) {
            continue;
        }

        stageRailEventTriggered_[index] = true;
        if (!isGameOver_ && !isGameClear_ && currentWaveIndex_ < kWaveCount) {
            enemySpawnTimer_ = (std::min)(enemySpawnTimer_, event.spawnTimerCap);
        }
        AddCameraShake(event.shakePower, event.shakeDuration);
    }

    if (!IsTutorial() && !bossWarningTriggered_ &&
        !bossSpawned_ &&
        !isGameOver_ &&
        !isGameClear_ &&
        stageProgress_ >= kBossWarningDistance) {
        bossWarningTriggered_ = true;
        bossWarningTimer_ = kBossWarningDuration;
        stageCombatBeatName_ = "WARNING";
        AddCameraShake(0.13f, 28);
    }
}

void GameRuntime::UpdateRailProgress()
{
    if (!isGameOver_ && !isGameClear_) {
        UpdateStageDirector();
        railSpeed_ = Lerp(
            railSpeed_,
            targetRailSpeed_,
            feverTimer_ > 0 ? kFeverRailAccelerationResponse : 0.070f);
        const float worldTimeScale = GetCinematicWorldTimeScale();
        flightReleaseKick_ = (std::max)(0.0f, flightReleaseKick_ - 1.0f / 28.0f);
        if (!IsTutorial() && worldTimeScale > previousFlightTimeScale_ + 0.25f) {
            flightReleaseKick_ = 1.0f;
        }
        previousFlightTimeScale_ = worldTimeScale;
        railDistance_ += railSpeed_ * worldTimeScale;
        // 背景は流し続けるが、練習の進行は距離ではなく操作の達成で決める。
        if (IsTutorial()) { return; }

        int activeStageEnemyCount = 0;
        for (const auto& enemy : enemies_) {
            if (enemy && !enemy->IsDead() && !enemy->IsBoss()) {
                ++activeStageEnemyCount;
            }
        }

        const StageEnemySpawnEvent* nextEnemyEvent = nullptr;
        const auto enemyEvents = GetEnemySchedule(IsTutorial());
        const size_t eventCount =
            (std::min)(stageEnemyEventTriggered_.size(), enemyEvents.size());
        for (size_t index = 0; index < eventCount; ++index) {
            if (!stageEnemyEventTriggered_[index]) {
                nextEnemyEvent = &enemyEvents[index];
                break;
            }
        }

        const bool nextEventContinuesCurrentEncounter =
            nextEnemyEvent != nullptr &&
            std::string_view(nextEnemyEvent->beatName) ==
                std::string_view(stageCombatBeatName_ ? stageCombatBeatName_ : "Intro") &&
            nextEnemyEvent->distance - stageProgress_ <=
                kStageEncounterGroupingDistance;
        const bool currentEncounterFullyScheduled =
            !nextEventContinuesCurrentEncounter;
        const bool timelineBlocked =
            activeStageEnemyCount > 0 && currentEncounterFullyScheduled;

        if (activeStageEnemyCount > 0 || bossSpawned_) { stageEmptyFrames_ = 0.0f; }
        else { stageEmptyFrames_ += worldTimeScale; }

        if (stageTimelineWasBlocked_ && !timelineBlocked &&
            activeStageEnemyCount == 0) {
            stageEncounterBreatherTimer_ = feverTimer_ > 0 ?
                kFeverEncounterBreatherFrames : (IsTutorial() ? kStageEncounterBreatherFrames : 36);
        }
        stageTimelineWasBlocked_ = timelineBlocked;

        if (bossSpawned_ || timelineBlocked) {
            stageTimelineSpeed_ = 0.0f;
        } else {
            // 編隊間の移動と息継ぎを並行させ、撃破後の二重待ちをなくす。
            // フィーバーでも敵が残っている間は進行を止め、倒す機会を奪わない。
            stageTimelineSpeed_ = kStageTimelineCruiseSpeed *
                (feverTimer_ > 0 ? kFeverRailSpeedMultiplier : 1.0f);
            if (!IsTutorial() && activeStageEnemyCount == 0 &&
                std::any_of(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), [](bool v) { return v; })) {
                // 背景や自機の速度は変えず、敵の配置タイムラインだけを詰める。
                // 次の一件までしか進めないので、編隊の順序・数・戦闘中の滞在時間は維持する。
                const float destination = nextEnemyEvent ? nextEnemyEvent->distance : kBossSpawnDistance;
                const float deadline = nextEnemyEvent ? kEmptyEncounterMaxFrames : kEmptyBossApproachMaxFrames;
                const float remainingFrames = (std::max)(1.0f, deadline - stageEmptyFrames_);
                stageTimelineSpeed_ = (std::max)(stageTimelineSpeed_,
                    (destination - stageProgress_) / remainingFrames);
            }
            if (stageEncounterBreatherTimer_ > 0) {
                --stageEncounterBreatherTimer_;
            }
        }
        float nextProgress = stageProgress_ + stageTimelineSpeed_ * worldTimeScale;
        if (!IsTutorial() && activeStageEnemyCount == 0 && nextEnemyEvent) {
            nextProgress = (std::min)(nextProgress, nextEnemyEvent->distance);
        }
        if (stageEncounterBreatherTimer_ > 0 && nextEnemyEvent) {
            // 最短の息継ぎ時間を確保する。出現境界を越えないだけで配置は飛ばさない。
            nextProgress = (std::min)(nextProgress,
                (std::max)(stageProgress_, nextEnemyEvent->distance - 0.001f));
        }
        stageProgress_ = nextProgress;
    }
}

void GameRuntime::UpdatePlayerAndCamera()
{
    const float controlStep = dxCommon_ ? dxCommon_->GetDeltaTime() * 60.0f : 1.0f;
    const Math::Vector3 before = player_->GetTranslate();
    player_->Update(input_, GetCinematicWorldTimeScale(), controlStep);
    player_->SetRailZ(railDistance_);
    const Math::Vector3 after = player_->GetTranslate();
    const float step = (std::max)(controlStep, 0.1f);
    const Math::Vector3 velocity{
        std::clamp((after.x - before.x) / step, -0.205f, 0.205f),
        std::clamp((after.y - before.y) / step, -0.205f, 0.205f), 0.0f };
    enemyAimVelocity_ = player_->IsDodging() ? Math::Vector3{} : Math::Vector3{
        Lerp(enemyAimVelocity_.x, velocity.x, 0.3f),
        Lerp(enemyAimVelocity_.y, velocity.y, 0.3f), 0.0f };
    UpdateGameCamera();
    if (skybox_) {
        skybox_->Update(camera_.get());
    }
    UpdateLockOnTarget();
}

void GameRuntime::UpdatePlayerShooting()
{
    if (shootCooldown_ > 0) {
        --shootCooldown_;
    }
    if (shootBufferTimer_ > 0) {
        --shootBufferTimer_;
    }

    const bool isShootPressed = input_ && input_->PushKey(DIK_SPACE);
    const bool isShootTriggered = input_ && input_->TriggerKey(DIK_SPACE);
    if (isShootTriggered && shootCooldown_ > 0) {
        shootBufferTimer_ = 8;
    }

    const bool shouldShoot = isShootPressed || shootBufferTimer_ > 0;
    if (shouldShoot && shootCooldown_ <= 0) {
        const bool isCharged = chargeTimer_ >= chargeShotThreshold_;
        const bool hasAssistShot = isCharged && isReticleOnTarget_ && lockedEnemy_;
        FirePlayerBullet();
        shootBufferTimer_ = 0;
        AddCameraShake(
            isCharged ? (hasAssistShot ? 0.105f : 0.085f) : 0.026f,
            isCharged ? (hasAssistShot ? 16 : 14) : 5);
        shootCooldown_ = feverTimer_ > 0 ?
            kFeverRapidShotCooldown :
            (isCharged ? chargedShootCooldown_ : normalShootCooldown_);
    } else if (!isShootPressed && shootBufferTimer_ <= 0) {
        chargeTimer_ = (std::min)(chargeTimer_ + 1, kChargeShotMax);
    }

    if (chargeFlashTimer_ > 0) {
        --chargeFlashTimer_;
    }
}

void GameRuntime::UpdateFever()
{
    const float targetSpeedEffectRate = feverTimer_ > 0 ? 1.0f : 0.0f;
    const float speedEffectResponse =
        targetSpeedEffectRate > feverSpeedEffectRate_ ? 0.16f : 0.045f;
    feverSpeedEffectRate_ = Lerp(
        feverSpeedEffectRate_,
        targetSpeedEffectRate,
        speedEffectResponse);
    if (targetSpeedEffectRate <= 0.0f && feverSpeedEffectRate_ < 0.001f) {
        feverSpeedEffectRate_ = 0.0f;
    }

    if (feverActivationFlashTimer_ > 0) {
        --feverActivationFlashTimer_;
    }

    if (feverTimer_ > 0) {
        --feverTimer_;
        chargeTimer_ = kChargeShotMax;
        return;
    }

    if (isGameOver_ || isGameClear_ || !player_ || player_->IsDead() ||
        feverGauge_ < kFeverGaugeMax) {
        return;
    }

    // 命中・撃破などで満タンになった次の更新で自動発動。入力待ちは挟まない。
    ActivateFever();
}

void GameRuntime::ActivateFever()
{
    if (isGameOver_ || isGameClear_) {
        return;
    }

    feverGauge_ = 0;
    PlaySfx("fever");
    ++feverActivationCount_;
    feverTimer_ = kFeverDurationFrames;
    feverActivationFlashTimer_ = kFeverActivationFlashFrames;
    chargeTimer_ = kChargeShotMax;
    chargeFlashTimer_ = (std::max)(chargeFlashTimer_, 30);
    stageEncounterBreatherTimer_ =
        (std::min)(stageEncounterBreatherTimer_, kFeverEncounterBreatherFrames);
    if (player_) {
        const Math::Vector3 playerPosition = player_->GetTranslate();
        AddPlayerDodgeGrazeEffect(playerPosition);
        AddPlayerDodgeGrazeEffect({
            playerPosition.x,
            playerPosition.y + 0.18f,
            playerPosition.z + 0.70f
        });
    }
    AddCameraShake(0.16f, 24);
}

void GameRuntime::AddFeverGauge(int amount)
{
    if (IsTutorial() && tutorial_.lesson != TutorialLesson::Fever) { return; }
    if (amount <= 0 || feverTimer_ > 0 || isGameOver_ || isGameClear_) {
        return;
    }
    feverGauge_ = (std::clamp)(feverGauge_ + amount, 0, kFeverGaugeMax);
}

void GameRuntime::AddScore(int baseScore)
{
    if (baseScore <= 0) {
        return;
    }
    const int feverMultiplier = feverTimer_ > 0 ? kFeverScoreMultiplier : 1;
    score_ += baseScore * feverMultiplier * GetDefeatChainScoreMultiplier();
}

void GameRuntime::UpdateDefeatChain()
{
    if (defeatChainBreakFlashTimer_ > 0) {
        --defeatChainBreakFlashTimer_;
    }
    if (isGameOver_ || isGameClear_ || defeatChainCount_ <= 0) {
        return;
    }

    // 敵がいない移動区間では猶予を消費せず、プレイヤーの腕前以外でチェインを切らない。
    const bool hasChainTarget = std::any_of(
        enemies_.begin(),
        enemies_.end(),
        [](const std::unique_ptr<Enemy>& enemy) {
            return enemy && !enemy->IsDead() && !enemy->IsBoss() && enemy->IsTargetable();
        });
    if (!hasChainTarget) {
        return;
    }

    if (defeatChainTimer_ > 0) {
        --defeatChainTimer_;
    }
    if (defeatChainTimer_ <= 0) {
        BreakEnemyDefeatChain();
    }
}

void GameRuntime::RegisterEnemyDefeatChain()
{
    defeatChainCount_ = (std::min)(defeatChainCount_ + 1, kDefeatChainMaxCount);
    defeatChainTimer_ = kDefeatChainDurationFrames;
    defeatChainBreakFlashTimer_ = 0;
    maxDefeatChainCount_ = (std::max)(maxDefeatChainCount_, defeatChainCount_);
}

void GameRuntime::BreakEnemyDefeatChain()
{
    if (defeatChainCount_ >= 2) {
        defeatChainBreakFlashTimer_ = kDefeatChainBreakFlashFrames;
    }
    defeatChainCount_ = 0;
    defeatChainTimer_ = 0;
}

int GameRuntime::GetDefeatChainScoreMultiplier() const
{
    const int multiplier = 1 + defeatChainCount_ / kDefeatChainScoreTierSize;
    return (std::min)(multiplier, kDefeatChainMaxScoreMultiplier);
}

#ifdef ENABLE_DEBUG_GUI
void GameRuntime::DebugJumpToStagePhase(int phaseIndex)
{
    if (IsTutorial()) {
        editorStatusMessage_ = "本編のフェーズ移動はチュートリアルでは使用できません。";
        return;
    }
    if (isGameOver_ || isGameClear_) {
        editorStatusMessage_ = "フェーズ移動はゲーム進行中のみ使用できます。";
        return;
    }

    constexpr std::array<float, 5> kDebugPhaseStartDistances{
        0.0f,
        104.0f,
        230.0f,
        kBossSpawnDistance,
        78.0f
    };
    const int clampedPhase =
        (std::clamp)(phaseIndex, 0, static_cast<int>(kDebugPhaseStartDistances.size()) - 1);
    const float targetProgress = kDebugPhaseStartDistances[clampedPhase];

    lockedEnemy_ = nullptr;
    hasLockTarget_ = false;
    isReticleOnTarget_ = false;
    homingBulletTargets_.clear();
    justDodgedEnemyBullets_.clear();
    enemies_.clear();
    ResetPhantomRaid();
    ResetRifts();

    while (!playerBullets_.empty()) {
        playerBulletPool_.push_back(std::move(playerBullets_.front()));
        playerBullets_.pop_front();
    }
    while (!enemyBullets_.empty()) {
        enemyBulletPool_.push_back(std::move(enemyBullets_.front()));
        enemyBullets_.pop_front();
    }
    for (RewardHeart& heart : rewardHearts_) {
        heart.isActive = false;
    }

    stageProgress_ = targetProgress;
    // 明示的なデバッグ移動だけは、対象街区の中から始める。通常の遷移は前方から行う。
    sceneryCanyonStartZ_ = targetProgress >= 104.0f ? railDistance_ - 600.0f : kDefenseStartWorldZ;
    sceneryPlazaStartZ_ = targetProgress >= 230.0f ? railDistance_ - 400.0f : kPlazaStartWorldZ;
    stageTimelineSpeed_ = 0.0f;
    stageTimelineWasBlocked_ = false;
    stageEncounterBreatherTimer_ = 0;
    stageEmptyFrames_ = 0.0f;
    currentWaveIndex_ = targetProgress < 104.0f ? 0 : (targetProgress < 230.0f ? 1 : 2);
    spawnedEnemyCountInWave_ = 0;
    defeatedEnemyCountInWave_ = 0;
    spawnSequenceIndex_ = 0;
    defeatedEnemyCount_ = 0;
    escapedEnemyCount_ = 0;
    playerShotsFired_ = 0;
    playerHitCount_ = 0;
    playerDamageCount_ = 0;
    feverActivationCount_ = 0;
    justDodgeCount_ = 0;
    defeatChainCount_ = 0;
    defeatChainTimer_ = 0;
    defeatChainBreakFlashTimer_ = 0;
    maxDefeatChainCount_ = 0;
    gameplayElapsedSeconds_ = 0.0f;
    enemySpawnTimer_ = 0;
    enemyShotTimer_ = 32;
    enemyAimVelocity_ = {};
    bossSpawned_ = false;
    bossDefeated_ = false;
    bossWarningTriggered_ = clampedPhase == 3;
    bossWarningTimer_ = clampedPhase == 3 ? 42 : 0;
    bossIntroTimer_ = 0;
    bossDefeatFlashTimer_ = 0;
    bossPhaseTransitionTimer_ = 0;
    bossAttackCooldown_ = 0;
    bossAttackStepTimer_ = 0;
    bossAttackStep_ = -1;
    bossAttackPattern_ = 0;
    bossAttackSequence_ = 0;
    bossShotsFired_ = 0;
    bossPhase_ = 1;
    bossCounterTimer_ = 0;
    bossCounterDuration_ = 1;
    bossAimPoint_ = {};
    bossDefeatPosition_ = {};
    resultTransitionTimer_ = -1;

    const size_t railEventCount =
        (std::min)(
            stageRailEventTriggered_.size(),
            sizeof(kStageRailEvents) / sizeof(kStageRailEvents[0]));
    for (size_t index = 0; index < railEventCount; ++index) {
        stageRailEventTriggered_[index] =
            kStageRailEvents[index].distance < targetProgress;
    }

    const size_t enemyEventCount =
        (std::min)(stageEnemyEventTriggered_.size(), std::size(kStageEnemySpawnEvents));
    for (size_t index = 0; index < enemyEventCount; ++index) {
        const bool skippedEvent =
            clampedPhase == 3 ||
            kStageEnemySpawnEvents[index].distance < targetProgress;
        stageEnemyEventTriggered_[index] = skippedEvent;
        if (skippedEvent) {
            ++spawnSequenceIndex_;
            ++defeatedEnemyCount_;
        }
    }

    stageCombatBeatName_ = clampedPhase == 3 ? "Boss" : "Debug phase jump";
    UpdateStageDirector();

    constexpr const char* kDebugPhaseNames[] = {
        "Wave 1",
        "Wave 2",
        "Wave 3",
        "Boss",
        "Sniper wing"
    };
    editorStatusMessage_ =
        std::string("デバッグ移動: ") + kDebugPhaseNames[clampedPhase];
}
#endif

void GameRuntime::UpdateEnemyActions()
{
    if (IsTutorial()) { UpdateTutorialAttack(); return; }
    if (justDodgeSlowTimer_ > 0) { return; }
    UpdateEnemyWave();
    UpdateBossActions();
    const bool hasSupportDrone = std::any_of(enemies_.begin(), enemies_.end(),
        [](const auto& enemy) { return enemy && !enemy->IsDead() && enemy->IsSupport(); });
    UpdateEnemyAttackPatterns(hasSupportDrone);
}

void GameRuntime::UpdateEnemyAttackPatterns(bool hasSupportDrone)
{
    Math::Vector2 viewportMin{}, viewportSize{};
    GetEffectiveHudViewportRect(viewportMin, viewportSize);
    const float worldStep = GetCinematicWorldTimeScale();
    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsBoss() || enemy->IsSupport()) { continue; }
        EnemyFireControl::Pattern pattern;
        pattern.recovery = static_cast<float>(enemyShotInterval_) * 1.4f;
        float leadFrames = 12.0f;
        switch (enemy->GetBehavior()) {
        case Enemy::Behavior::Formation:
            if (enemy->IsTightFormation()) {
                // 小型三機の斉射を一度に重ねない。内側から順に撃ち、狙撃機が主な脅威になる。
                pattern = { 36.0f + enemy->GetFormationShotDelay(), 16.0f, enemyShotInterval_ * 1.65f, 2 };
            }
            break;
        case Enemy::Behavior::Sniper:
            pattern = { 60.0f, 18.0f, enemyShotInterval_ * 2.0f, 2 };
            leadFrames = 22.0f;
            break;
        case Enemy::Behavior::Shield:
            pattern = { 36.0f, 18.0f, enemyShotInterval_ * 1.7f, 2 };
            leadFrames = 6.0f;
            break;
        case Enemy::Behavior::Crossfire:
            pattern = { 32.0f, 11.0f, enemyShotInterval_ * 1.25f, 3 };
            break;
        case Enemy::Behavior::StrafeShooter:
            pattern = { 26.0f, 10.0f, enemyShotInterval_ * 1.2f, 3 };
            leadFrames = 18.0f;
            break;
        case Enemy::Behavior::DiveBomber:
            pattern = { 22.0f, 10.0f, enemyShotInterval_ * 1.2f, 3 };
            break;
        case Enemy::Behavior::Swoop:
            pattern = { 20.0f, 10.0f, enemyShotInterval_ * 1.1f, 2 };
            break;
        default:
            break;
        }
        Math::Vector2 screen{};
        const bool visible = TryProjectToScreen(enemy->GetAimPosition(), screen) &&
            screen.x >= viewportMin.x + 12.0f && screen.x <= viewportMin.x + viewportSize.x - 12.0f &&
            screen.y >= viewportMin.y + 12.0f && screen.y <= viewportMin.y + viewportSize.y - 12.0f;
        auto& control = enemy->GetFireControl();
        const auto event = control.Advance(worldStep, enemy->CanShoot() && visible,
            pattern, hasSupportDrone ? 0.72f : 1.0f);
        if (event == EnemyFireControl::Event::Aim || control.IsTracking()) {
            control.aimPoint = player_->GetTranslate();
            // 入力の未来を読むのではなく、観測した移動だけを少し先読みする。
            control.aimPoint.x += std::clamp(enemyAimVelocity_.x * leadFrames, -2.6f, 2.6f);
            control.aimPoint.y += std::clamp(enemyAimVelocity_.y * leadFrames, -1.3f, 1.3f);
        }
        if (event != EnemyFireControl::Event::Fire) { continue; }
        const Math::Vector3 muzzle = enemy->GetAimPosition();
        const int shot = control.ShotIndex();
        const float sweep = control.VolleyIndex() % 2 == 0 ? -1.0f : 1.0f;
        switch (enemy->GetBehavior()) {
        case Enemy::Behavior::Sniper:
            FireEnemyBullet(muzzle, EnemyBulletStyle::Sniper, {}, &control.aimPoint);
            break;
        case Enemy::Behavior::Shield:
            // 二段の扇。中央は狙うが、外側と上下には逃げ道を残す。
            for (int lane = -1; lane <= 1; ++lane) {
                const float y = shot == 0 ? 0.0f : (control.aimPoint.y > 2.4f ? -1.8f : 1.8f);
                FireEnemyBullet(muzzle, EnemyBulletStyle::ShieldOrb,
                    { static_cast<float>(lane) * 3.4f, y }, &control.aimPoint);
            }
            break;
        case Enemy::Behavior::Crossfire: {
            // 左右の敵が内側へ射線を寄せる。端か上下へ抜ければ回避できる。
            const float side = muzzle.x < 0.0f ? -1.0f : 1.0f;
            FireEnemyBullet(muzzle, EnemyBulletStyle::Crossfire,
                { side * (2.6f - static_cast<float>(shot) * 1.3f), 0.0f }, &control.aimPoint);
            break;
        }
        case Enemy::Behavior::StrafeShooter:
            FireEnemyBullet(muzzle, EnemyBulletStyle::Crossfire,
                { sweep * (static_cast<float>(shot) - 1.0f) * 2.8f, 0.0f }, &control.aimPoint);
            break;
        case Enemy::Behavior::DiveBomber:
            FireEnemyBullet(muzzle, EnemyBulletStyle::Standard,
                { 0.0f, (1.0f - static_cast<float>(shot)) * 1.5f }, &control.aimPoint);
            break;
        default:
            FireEnemyBullet(muzzle, EnemyBulletStyle::Standard, {}, &control.aimPoint);
            break;
        }
    }
}

void GameRuntime::UpdateBossActions()
{
    const Enemy* boss = GetBossEnemy();
    if (!boss) {
        bossAttackStep_ = -1;
        bossAttackStepTimer_ = 0;
        bossCounterTimer_ = 0;
        return;
    }

    if (bossCounterTimer_ > 0) {
        --bossCounterTimer_;
    }

    if (bossPhase_ == 1 && boss->GetHp() * 2 <= boss->GetMaxHp()) {
        bossPhase_ = 2;
        bossPhaseTransitionTimer_ = kBossPhaseTransitionDuration;
        bossAttackCooldown_ = 0;
        bossAttackStepTimer_ = 0;
        bossAttackStep_ = -1;
        bossCounterTimer_ = 0;
        stageCombatBeatName_ = "Boss phase 2";

        while (!enemyBullets_.empty()) {
            justDodgedEnemyBullets_.erase(enemyBullets_.front().get());
            enemyBulletPool_.push_back(std::move(enemyBullets_.front()));
            enemyBullets_.pop_front();
        }

        AddEnemyImpactEffect(boss->GetAimPosition(), 2.0f);
        AddCameraShake(0.24f, 42);
        return;
    }

    if (!boss->CanShoot() || bossIntroTimer_ > 0 || bossPhaseTransitionTimer_ > 0) {
        return;
    }

    const bool isPhaseTwo = bossPhase_ >= 2;
    // 自動チャージ(88フレーム)と弾の到達時間を含め、一発を狙って返せる長さ。
    const int counterDuration = isPhaseTwo ? 108 : 120;
    const Math::Vector3 bossPosition = boss->GetAimPosition();
    const Math::Vector3 leftCannon{
        bossPosition.x - 2.2f,
        bossPosition.y + 0.25f,
        bossPosition.z
    };
    const Math::Vector3 rightCannon{
        bossPosition.x + 2.2f,
        bossPosition.y + 0.25f,
        bossPosition.z
    };

    if (bossAttackStep_ < 0) {
        if (bossAttackCooldown_ > 0) {
            --bossAttackCooldown_;
            return;
        }

        bossAttackPattern_ = bossAttackSequence_ % 3;
        ++bossAttackSequence_;
        bossAttackStep_ = 0;
        bossAimPoint_ = player_->GetTranslate();
        bossAttackStepTimer_ = BossWindupDuration(bossAttackPattern_, bossPhase_);
        switch (bossAttackPattern_) {
        case 0:
            stageCombatBeatName_ = "Boss fan volley";
            break;
        case 1:
            stageCombatBeatName_ = "Boss sweep";
            break;
        case 2:
        default:
            stageCombatBeatName_ = "Boss charge cannon";
            AddCameraShake(0.08f, 18);
            break;
        }
        return;
    }

    if (bossAttackStepTimer_ > 0) {
        if (bossAttackStep_ == 0 && bossAttackStepTimer_ > kBossAimLockFrames) {
            bossAimPoint_ = player_->GetTranslate();
        }
        --bossAttackStepTimer_;
        return;
    }

    switch (bossAttackPattern_) {
    case 0: {
        const int bulletCount = isPhaseTwo ? 7 : 5;
        const float spread = isPhaseTwo ? 5.4f : 4.4f;
        for (int index = 0; index < bulletCount; ++index) {
            const float rate = bulletCount > 1 ?
                static_cast<float>(index) / static_cast<float>(bulletCount - 1) : 0.5f;
            const float horizontalOffset = -spread + spread * 2.0f * rate;
            const float verticalOffset = 0.50f - std::abs(rate - 0.5f) * 1.4f;
            FireEnemyBullet(
                index % 2 == 0 ? leftCannon : rightCannon,
                EnemyBulletStyle::BossCannon,
                { horizontalOffset, verticalOffset }, &bossAimPoint_);
        }
        bossAttackStep_ = -1;
        bossAttackCooldown_ = counterDuration + 24;
        bossCounterDuration_ = counterDuration;
        bossCounterTimer_ = counterDuration;
        stageCombatBeatName_ = "Boss exposed";
        AddCameraShake(0.11f, 16);
        break;
    }
    case 1: {
        const int shotCount = isPhaseTwo ? 9 : 7;
        const int shotIndex = bossAttackStep_;
        const float rate = shotCount > 1 ?
            static_cast<float>(shotIndex) / static_cast<float>(shotCount - 1) : 0.5f;
        const float horizontalOffset = -5.8f + 11.6f * rate;
        const float verticalOffset = (shotIndex % 2 == 0 ? -0.7f : 0.7f);
        FireEnemyBullet(
            shotIndex % 2 == 0 ? leftCannon : rightCannon,
            EnemyBulletStyle::BossCannon,
            { horizontalOffset, verticalOffset }, &bossAimPoint_);
        ++bossAttackStep_;
        if (bossAttackStep_ >= shotCount) {
            bossAttackStep_ = -1;
            bossAttackCooldown_ = counterDuration + 24;
            bossCounterDuration_ = counterDuration;
            bossCounterTimer_ = counterDuration;
            stageCombatBeatName_ = "Boss exposed";
        } else {
            bossAttackStepTimer_ = isPhaseTwo ? 5 : 7;
        }
        break;
    }
    case 2:
    default:
        FireEnemyBullet({
            bossPosition.x,
            bossPosition.y - 0.45f,
            bossPosition.z
        }, EnemyBulletStyle::BossCharge, {}, &bossAimPoint_);
        if (isPhaseTwo) {
            FireEnemyBullet(leftCannon, EnemyBulletStyle::BossCannon, { -2.4f, 0.0f }, &bossAimPoint_);
            FireEnemyBullet(rightCannon, EnemyBulletStyle::BossCannon, { 2.4f, 0.0f }, &bossAimPoint_);
        }
        bossAttackStep_ = -1;
        bossAttackCooldown_ = counterDuration + 24;
        bossCounterDuration_ = counterDuration;
        bossCounterTimer_ = counterDuration;
        stageCombatBeatName_ = "Boss exposed";
        AddCameraShake(0.20f, 26);
        break;
    }
}

void GameRuntime::UpdateWorldEntities()
{
    UpdatePlayerBullets();
    UpdateEnemyBullets();
    UpdateEnemies();
    UpdateHitEffects();
    UpdatePlayerDodgeAfterimages();
    UpdateRewardHearts();
    UpdateDepthCueEffects();
    UpdateRailScenery();
}

void GameRuntime::UpdateGameplayCollisions()
{
    if (isGameOver_ || isGameClear_) {
        return;
    }
    CheckBulletEnemyCollisions();
    if (!isGameClear_) {
        CheckEnemyBulletPlayerCollisions();
    }
}

void GameRuntime::UpdateResultAndSceneObjects()
{
    if ((isGameClear_ || isGameOver_) && bossDefeatFlashTimer_ <= 0 && !resultSoundPlayed_) {
        PlaySfx(isGameClear_ ? "clear" : "fail");
        resultSoundPlayed_ = true;
    }
    if (bossWarningTimer_ > 0) {
        --bossWarningTimer_;
    }
    if (bossIntroTimer_ > 0) {
        --bossIntroTimer_;
    }
    if (bossDefeatFlashTimer_ > 0) {
        --bossDefeatFlashTimer_;
        // 同時に白く潰すのでなく、左右→中央と時間差で崩れる余韻を作る。
        if (bossDefeatFlashTimer_ == 32 || bossDefeatFlashTimer_ == 20 || bossDefeatFlashTimer_ == 8) {
            const int index = (32 - bossDefeatFlashTimer_) / 12;
            constexpr Math::Vector3 offsets[] = {
                { -2.2f, 0.5f, -0.8f }, { 2.2f, -0.3f, 0.4f }, { 0.0f, 1.0f, 0.2f }
            };
            const auto& offset = offsets[index];
            AddEnemyHitEffect({ bossDefeatPosition_.x + offset.x, bossDefeatPosition_.y + offset.y,
                bossDefeatPosition_.z + offset.z }, index == 2 ? 2.4f : 1.55f);
            PlaySfx("destroy");
            AddCameraShake(index == 2 ? 0.26f : 0.12f, index == 2 ? 24 : 10);
        }
    }
    if (bossPhaseTransitionTimer_ > 0) {
        --bossPhaseTransitionTimer_;
    }
    if (justDodgeFlashTimer_ > 0) {
        --justDodgeFlashTimer_;
    }
    if (justDodgeSlowTimer_ > 0) {
        --justDodgeSlowTimer_;
    }
    if (playerImpactFlashTimer_ > 0) {
        --playerImpactFlashTimer_;
        if (playerImpactFlashTimer_ <= 0) {
            playerImpactFlashDuration_ = 1;
        }
    }
    if (playerImpactSlowTimer_ > 0) {
        --playerImpactSlowTimer_;
        if (playerImpactSlowTimer_ <= 0) {
            playerImpactSlowDuration_ = 1;
            playerImpactSlowScale_ = 1.0f;
        }
    }
    if (hitConfirmTimer_ > 0) {
        --hitConfirmTimer_;
        if (hitConfirmTimer_ <= 0) {
            hitConfirmDuration_ = 1;
        }
    }
    if (hitConfirmComboTimer_ > 0) {
        --hitConfirmComboTimer_;
        if (hitConfirmComboTimer_ <= 0) {
            hitConfirmComboCount_ = 0;
        }
    }
    if (playerDamageHudTimer_ > 0) {
        --playerDamageHudTimer_;
        if (playerDamageHudTimer_ <= 0) {
            playerDamageHudDuration_ = 1;
        }
    }
    if (resultTransitionTimer_ > 0) {
        --resultTransitionTimer_;
    }

    for (const auto& sceneObject : sceneObjects_) {
        sceneObject->Update();
    }
}


void GameRuntime::SetRenderingOptions(bool showSkybox, int postEffectMode)
{
    showSkybox_ = showSkybox;
    postEffectMode_ = postEffectMode;
}

int GameRuntime::GetPlayerHp() const
{
    return player_ ? player_->GetHp() : 0;
}

const Math::Matrix4x4& GameRuntime::GetProjectionMatrix() const
{
    static const Math::Matrix4x4 kIdentity = Math::MakeIdentity4x4();
    return camera_ ? camera_->GetProjectionMatrix() : kIdentity;
}

void GameRuntime::SetHudViewportRect(
    bool isEnabled,
    const Math::Vector2& min,
    const Math::Vector2& size)
{
    isHudViewportRectEnabled_ = isEnabled && size.x > 1.0f && size.y > 1.0f;
    hudViewportMin_ = min;
    hudViewportSize_ = size;
}

void GameRuntime::GetEffectiveHudViewportRect(
    Math::Vector2& min,
    Math::Vector2& size) const
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    min = { viewport->Pos.x, viewport->Pos.y };
    size = { viewport->Size.x, viewport->Size.y };

    if (isHudViewportRectEnabled_) {
        min = hudViewportMin_;
        size = hudViewportSize_;
        return;
    }

    if (isEditorOverlayVisible_ && hasEditorOverlayViewportRect_) {
        min = editorOverlayViewportMin_;
        size = editorOverlayViewportSize_;
        return;
    }
    if (dxCommon_) {
        const auto& presentation = dxCommon_->GetPresentationViewport();
        min.x += presentation.TopLeftX;
        min.y += presentation.TopLeftY;
        size = { presentation.Width, presentation.Height };
    }
}

void GameRuntime::InitializeRewardHearts()
{
    rewardHearts_.clear();

    Model* heartModel =
        ModelManager::GetInstance()->FindModel("reward_energy_shard");
    if (!heartModel || !object3dCommon_) {
        return;
    }

    rewardHearts_.reserve(kRewardHeartPoolCount);
    for (int index = 0; index < kRewardHeartPoolCount; ++index) {
        RewardHeart heart{};
        heart.object = std::make_unique<Object3d>();
        heart.object->Initialize(object3dCommon_.get());
        heart.object->SetModel(heartModel);
        heart.object->SetTranslate({ 0.0f, -1000.0f, 0.0f });
        heart.object->SetScale({ 0.0f, 0.0f, 1.0f });
        heart.object->SetTextureFilePath("resources/human/white.png");
        heart.object->SetColor({ 1.0f, 0.18f, 0.44f, 0.0f });
        heart.object->SetLightingMode(1);
        heart.object->SetEnvironmentCoefficient(0.0f);
        heart.object->SetAlphaReference(0.01f);
        heart.object->Update();
        rewardHearts_.push_back(std::move(heart));
    }
}

void GameRuntime::SpawnRewardHearts(const Math::Vector3& worldPosition, int count)
{
    if (count <= 0 || rewardHearts_.empty()) {
        return;
    }

    int spawnedCount = 0;
    const float seed =
        static_cast<float>(defeatedEnemyCount_ * 17 + spawnedEnemyCountInWave_ * 5) * 0.137f;
    for (RewardHeart& heart : rewardHearts_) {
        if (spawnedCount >= count) {
            break;
        }
        if (heart.isActive || !heart.object) {
            continue;
        }

        const float t =
            static_cast<float>(spawnedCount) /
            static_cast<float>((std::max)(count, 1));
        const float angle = seed + kTwoPi * t;
        const float ring = 0.085f + 0.032f * static_cast<float>(spawnedCount % 3);
        heart.position = {
            worldPosition.x + std::cos(angle) * 0.20f,
            worldPosition.y + std::sin(angle * 1.7f) * 0.14f,
            worldPosition.z + std::sin(angle) * 0.07f,
        };
        heart.velocity = {
            std::cos(angle) * ring,
            0.054f + std::sin(angle * 1.3f) * 0.030f,
            -0.052f + std::sin(angle) * 0.018f,
        };
        heart.collisionRadius = 0.46f;
        heart.age = 0.0f;
        heart.collectDelay = 8.0f + static_cast<float>(spawnedCount % 3) * 1.5f;
        heart.life = 150.0f;
        heart.phase = angle;
        heart.baseScale = 0.27f + static_cast<float>(spawnedCount % 2) * 0.035f;
        heart.scoreValue = kRewardHeartScoreValue;
        heart.isActive = true;
        ++spawnedCount;
    }

    if (spawnedCount < count) {
        rewardHeartPoolMisses_ += static_cast<size_t>(count - spawnedCount);
    }
}

void GameRuntime::UpdateRewardHearts()
{
    float frameStep = 1.0f;
    if (dxCommon_) {
        frameStep =
            std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 3.0f);
    }

    const bool canCollect = player_ && !player_->IsDead();
    Math::Vector3 targetPosition{};
    if (canCollect) {
        targetPosition = player_->GetTranslate();
        targetPosition.y += 0.12f;
        targetPosition.z += 0.42f;
    }

    for (RewardHeart& heart : rewardHearts_) {
        if (!heart.isActive || !heart.object) {
            continue;
        }

        heart.age += frameStep;
        heart.phase += 0.12f * frameStep;

        if (!canCollect) {
            heart.position.x += heart.velocity.x * frameStep;
            heart.position.y += heart.velocity.y * frameStep;
            heart.position.z += heart.velocity.z * frameStep;
            heart.velocity = heart.velocity * 0.94f;
            if (heart.age >= heart.life) {
                heart.isActive = false;
            }
            continue;
        }

        if (heart.age < heart.collectDelay) {
            heart.position.x += heart.velocity.x * frameStep;
            heart.position.y += heart.velocity.y * frameStep;
            heart.position.z += heart.velocity.z * frameStep;
            heart.velocity = heart.velocity * std::clamp(1.0f - 0.060f * frameStep, 0.78f, 0.96f);
        } else {
            const float dx = targetPosition.x - heart.position.x;
            const float dy = targetPosition.y - heart.position.y;
            const float dz = targetPosition.z - heart.position.z;
            const float distanceSq = dx * dx + dy * dy + dz * dz;
            if (distanceSq <= heart.collisionRadius * heart.collisionRadius ||
                heart.age >= heart.life) {
                heart.isActive = false;
                AddScore(heart.scoreValue);
                AddRewardHeartCollectEffect(targetPosition);
                AddCameraShake(0.004f, 2);
                continue;
            }

            const float pullRate =
                std::clamp(0.070f + (heart.age - heart.collectDelay) * 0.0038f, 0.070f, 0.36f);
            heart.position = Lerp(
                heart.position,
                targetPosition,
                std::clamp(pullRate * frameStep, 0.0f, 0.54f));
        }

        const float popRate = std::clamp(heart.age / 7.0f, 0.0f, 1.0f);
        const float pullRate =
            heart.age >= heart.collectDelay ?
            std::clamp((heart.age - heart.collectDelay) / 28.0f, 0.0f, 1.0f) :
            0.0f;
        const float pulse = 1.0f + std::sin(heart.phase * 2.6f) * 0.075f;
        const float scale = heart.baseScale * (0.55f + popRate * 0.45f) * pulse *
            (1.0f - pullRate * 0.24f);
        Math::Vector3 rotate = camera_ ? camera_->GetRotate() : Math::Vector3{};
        rotate.z += heart.phase * 0.55f;

        heart.object->SetTranslate(heart.position);
        heart.object->SetRotate(rotate);
        heart.object->SetScale({ scale * 1.15f, scale * 2.1f, scale * 1.15f });
        heart.object->SetColor({ 1.12f, 0.65f, 0.16f, 0.95f });
        heart.object->Update();
    }
}

void GameRuntime::InitializeDepthCueEffects()
{
    depthCueEffects_.clear();
    if (!object3dCommon_) {
        return;
    }

    Model* sparkleModel = effectSparkStarModel_ ? effectSparkStarModel_ : effectGlowCoreModel_;
    Model* glowModel = effectGlowCoreModel_ ? effectGlowCoreModel_ : sparkleModel;
    Model* streakModel = effectPlayerBulletTrailModel_ ?
        effectPlayerBulletTrailModel_ :
        effectBulletTrailModel_;
    if (!sparkleModel && !glowModel && !streakModel) {
        return;
    }

    depthCueEffects_.reserve(kDepthCueEffectCount);
    for (int index = 0; index < kDepthCueEffectCount; ++index) {
        const bool isStreak = (index % 3) != 2 && streakModel;
        const bool isGold = !isStreak && (index % 7) == 0;
        const float side = PseudoRandom01(index, 0.12f) < 0.5f ? -1.0f : 1.0f;
        const float outerBias = std::pow(PseudoRandom01(index, 0.35f), 0.42f);
        const float x = side * Lerp(3.6f, 19.5f, outerBias);
        const float y = Lerp(-5.8f, 7.2f, PseudoRandom01(index, 0.61f));
        const float z = Lerp(10.0f, kDepthCueFarLocalZ, PseudoRandom01(index, 0.88f));

        DepthCueEffect cue{};
        cue.object = std::make_unique<Object3d>();
        cue.object->Initialize(object3dCommon_.get());
        cue.model = isStreak ? streakModel : ((index % 3) == 0 ? glowModel : sparkleModel);
        cue.object->SetModel(cue.model);
        cue.object->SetLightingMode(0);
        cue.object->SetEnvironmentCoefficient(0.0f);
        cue.object->SetAlphaReference(0.01f);
        cue.anchor = { x, y, z };
        cue.color = isStreak ?
            Math::Vector4{ 0.72f, 0.96f, 1.0f, 0.26f } :
            isGold ?
                Math::Vector4{ 1.0f, 0.86f, 0.42f, 0.40f } :
                Math::Vector4{ 0.78f, 0.96f, 1.0f, 0.34f };
        cue.loopLength = kDepthCueLoopLength;
        cue.speedMultiplier = isStreak ?
            Lerp(1.42f, 2.62f, PseudoRandom01(index, 1.12f)) :
            Lerp(0.86f, 1.58f, PseudoRandom01(index, 1.12f));
        cue.lateralDrift = Lerp(0.18f, 1.05f, PseudoRandom01(index, 1.46f));
        cue.verticalDrift = Lerp(0.10f, 0.62f, PseudoRandom01(index, 1.78f));
        cue.driftSpeed = Lerp(0.014f, 0.042f, PseudoRandom01(index, 2.03f));
        cue.phase = PseudoRandom01(index, 2.41f) * kTwoPi;
        cue.baseScale = isStreak ?
            Lerp(0.46f, 0.82f, PseudoRandom01(index, 2.82f)) :
            Lerp(0.09f, 0.26f, PseudoRandom01(index, 2.82f));
        cue.aspectX = isStreak ? 0.16f : 1.0f;
        cue.aspectY = isStreak ? Lerp(4.0f, 7.2f, PseudoRandom01(index, 3.17f)) : 1.0f;
        cue.spinSpeed = isStreak ?
            Lerp(-0.006f, 0.006f, PseudoRandom01(index, 3.59f)) :
            Lerp(-0.018f, 0.018f, PseudoRandom01(index, 3.59f));

        cue.object->SetColor(cue.color);
        cue.object->SetScale({
            cue.baseScale * cue.aspectX,
            cue.baseScale * cue.aspectY,
            1.0f,
        });
        cue.object->SetTranslate({
            cue.anchor.x,
            cue.anchor.y,
            railDistance_ + cue.anchor.z,
        });
        cue.object->Update();
        depthCueEffects_.push_back(std::move(cue));
    }
}

void GameRuntime::UpdateDepthCueEffects()
{
    if (depthCueEffects_.empty()) {
        return;
    }

    const Math::Vector3 cameraRotate = camera_ ? camera_->GetRotate() : Math::Vector3{};
    const float frameStep =
        dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 4.0f) : 1.0f;
    const float speedRate = GetFlightSpeedRate();
    const float feverSpeedRate = feverSpeedEffectRate_;
    for (DepthCueEffect& cue : depthCueEffects_) {
        if (!cue.object) {
            continue;
        }

        float localZ = std::fmod(
            cue.anchor.z - railDistance_ * cue.speedMultiplier,
            cue.loopLength);
        while (localZ < kDepthCueNearLocalZ) {
            localZ += cue.loopLength;
        }

        const float depthRate = std::clamp(
            (localZ - kDepthCueNearLocalZ) /
                (kDepthCueFarLocalZ - kDepthCueNearLocalZ),
            0.0f,
            1.0f);
        const float motion = railDistance_ * cue.driftSpeed + cue.phase;
        const float sidePull = 1.0f + (1.0f - depthRate) *
            (0.28f + speedRate * 0.18f + feverSpeedRate * 0.42f);
        Math::Vector3 position{};
        position.x = cue.anchor.x * sidePull + std::sin(motion) * cue.lateralDrift;
        position.y = cue.anchor.y + std::cos(motion * 0.77f) * cue.verticalDrift;
        position.z = railDistance_ + localZ;

        const float nearRate = 1.0f - depthRate;
        const float twinkle =
            0.78f +
            0.22f * std::sin(cue.phase + railDistance_ * (0.16f + cue.driftSpeed));
        const float scale =
            cue.baseScale *
            Lerp(
                0.66f,
                1.68f + speedRate * 0.28f + feverSpeedRate * 0.70f,
                nearRate) *
            twinkle;
        Math::Vector4 color = cue.color;
        color.w *= Lerp(
            0.44f,
            1.22f + speedRate * 0.22f + feverSpeedRate * 0.58f,
            nearRate) * twinkle;

        Math::Vector3 rotate = cameraRotate;
        rotate.z += cue.phase * 0.34f + railDistance_ * cue.spinSpeed * frameStep;

        cue.object->SetTranslate(position);
        cue.object->SetRotate(rotate);
        cue.object->SetScale({
            scale * cue.aspectX,
            scale * cue.aspectY *
                (1.0f + speedRate * nearRate * 0.36f +
                    feverSpeedRate * nearRate * 1.20f),
            1.0f,
        });
        cue.object->SetColor(color);
        cue.object->Update();
    }
}

void GameRuntime::DrawDepthCueEffects()
{
    if (depthCueEffects_.empty() || !object3dCommon_) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();

    object3dCommon_->SetDepthDrawMode(DepthDrawMode::Overlay);
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->CommonDrawSetting();

    for (const DepthCueEffect& cue : depthCueEffects_) {
        if (cue.object) {
            cue.object->Draw();
        }
    }

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::InitializeRailScenery()
{
    transparentSceneryDrawOrder_.clear();
    railSceneryObjects_.clear();

    if (!object3dCommon_) {
        return;
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
                modelPathText == kAviationTerrainModelPath;
            RailSceneryObject scenery;
            scenery.object = std::make_unique<Object3d>();
            scenery.object->Initialize(object3dCommon_.get());
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
                // 施設だけを既存の柔らかい拡散光へ合わせ、陰側の窓・扉・段差も残す。
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
                modelPathText == kAviationFlightwayModelPath;
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
                scenery.isLandmark ? 54.0f : scenery.isTrackside ? 18.0f : 0.0f;
            if (scenery.isDefenseDistrict) { scenery.halfDepth = kDefenseModuleLength * 0.5f; }
            if (scenery.isBossApron) { scenery.halfDepth = kBossApronModuleLength * 0.5f; }
            if (scenery.isTerrain) { scenery.halfDepth = 128.0f; }
            const auto transform = Math::MakeAffineMatrix(scale, rotate, {});
            for (const auto& vertex : model->GetVertices()) {
                const Math::Vector3 v{ vertex.position.x, vertex.position.y, vertex.position.z };
                scenery.boundsMin = { (std::min)(scenery.boundsMin.x, v.x),
                    (std::min)(scenery.boundsMin.y, v.y), (std::min)(scenery.boundsMin.z, v.z) };
                scenery.boundsMax = { (std::max)(scenery.boundsMax.x, v.x),
                    (std::max)(scenery.boundsMax.y, v.y), (std::max)(scenery.boundsMax.z, v.z) };
                if (scenery.isBuilding || scenery.isRoad || scenery.isRoadDetail) {
                    // 回転後の実際の端を含める。中心だけで判定すると建物の手前側が突然出る。
                    const auto position = TransformCoord({ vertex.position.x, vertex.position.y, vertex.position.z }, transform);
                    scenery.halfDepth = (std::max)(scenery.halfDepth, std::abs(position.z));
                }
            }
            scenery.drawFarLocalZ = kSceneryFarLocalZ;
            if (IsTutorial()) {
                scenery.drawFarLocalZ = scenery.isBuilding ? (scenery.isBackRow ? 178.0f : 220.0f) : 300.0f;
            }
            if (scenery.isRoadDetail) {
                scenery.drawFarLocalZ = 140.0f;
            }
            scenery.isVisible = true;
            railSceneryObjects_.push_back(std::move(scenery));
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
    const int cityCycleCount = IsTutorial() ? 1 : 2;
    const float cityLoopLength = kCityLayoutLength * static_cast<float>(cityCycleCount);
    constexpr float kRoadY = -3.05f;
    constexpr float kSurfaceDetailY = kRoadY - 0.135f;
    constexpr float kBuildingY = kRoadY - 0.15f;
    // 元の道路の最下面もkBuildingYと同じ高さ。基盤は下へ離して面の競合を防ぐ。
    constexpr float kTerrainY = kBuildingY - 0.30f;
    constexpr float kHalfPi = 1.57079632679f;

    for (int i = 0; i < 18 * cityCycleCount; ++i) {
        const float segmentZ = 18.0f * static_cast<float>(i);
        addSceneryStyled(
            kCityStreet4LaneModelPath,
            { 0.0f, kRoadY, -9.0f + segmentZ },
            // 元モデルはX方向へ続く6mの道路、Z方向18mに車道と歩道が並ぶ。
            // 90度回して進行方向へ18mずつ連結。歩道が横断帯として反復しないようにする。
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
    if (!IsTutorial()) {
        // 路面と同じ648m周期。全遠景を初期化時に確保し、実行中に増やさない。
        // 自機の可動域(±8.9m)の外へ配置し、戦闘の中央には障害物を足さない。
        for (int index = 0; index < 18; ++index) {
            addSceneryStyled(kTracksideModelPath,
                { 0.0f, kRoadY, 18.0f + 36.0f * static_cast<float>(index) },
                { 1.0f, 1.0f, 1.0f }, {}, cityLoopLength, 0.0f,
                { 0.94f, 0.96f, 0.97f, 1.0f }, 2, 0.015f);
        }
        // 路面・歩道・建物の敷地の隙間を含む地表。四枚が端で接続し、視界外でだけ循環する。
        for (int index = 0; index < 4; ++index) {
            addSceneryStyled(kAviationTerrainModelPath,
                { 0.0f, kTerrainY, 256.0f * static_cast<float>(index) },
                { 1.0f, 1.0f, 1.0f }, {}, 1024.0f, 0.0f,
                { 1.0f, 1.0f, 1.0f, 1.0f }, 2, 0.015f);
        }
        // 地上から約15mより上に橋桁、支柱は左右22mより外。敵と自機の飛行域は塞がない。
        // 部品はオフラインで8材質へ結合済み。プレイ中にモデルやGPU資源を追加しない。
        addSceneryStyled(kTransitLandmarkModelPath,
            { 0.0f, kBuildingY, kTransitLandmarkWorldZ }, { 1.0f, 1.0f, 1.0f }, {},
            0.0f, 0.0f, { 0.86f, 0.90f, 0.93f, 1.0f }, 1, 0.025f);
        // 進入設備、運航棟、管制塔、退出側の整備場を同じ固定敷地へまとめる。
        // 元の高架へ接続する形状をオフラインで7材質に結合。飛行域の左右17mは空ける。
        addSceneryStyled(kAviationDistrictModelPath,
            { 0.0f, kBuildingY, kTransitLandmarkWorldZ }, { 1.0f, 1.0f, 1.0f }, {},
            0.0f, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 1, 0.025f);
        // 窓だけを別の材質設定にする。既存の不透明パスで描き、追加の描画先やSRVは使わない。
        addSceneryStyled(kAviationGlazingModelPath,
            { 0.0f, kBuildingY, kTransitLandmarkWorldZ }, { 1.0f, 1.0f, 1.0f }, {},
            0.0f, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 1, 0.16f);
        // 序盤の飛行進入路だけを敷地へ固定。地表の誘導線は自動車用の車線へ重ねない。
        addSceneryStyled(kAviationFlightwayModelPath,
            { 0.0f, kBuildingY, kTransitLandmarkWorldZ }, { 1.0f, 1.0f, 1.0f }, {},
            0.0f, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 2, 0.015f);
        // 近景と遠景を含む六つの共有敷地を起動時に確保。視界外で循環する。
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
            // 舗装だけは同じ向きへ固定し、継ぎ目でアスファルトの粒を反転させない。
            addSceneryStyled(kAviationDefenseFlightwayModelPath, anchor, { 1.0f, 1.0f, 1.0f }, {},
                kDefenseLoopLength, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, 2, 0.015f);
        }
        // 低い外周施設と広い舗装面。ボス戦が長引いても同じ固定プールで続ける。
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
    transparentSceneryDrawOrder_.reserve(railSceneryObjects_.size());
}

Math::Vector2 GameRuntime::GetSceneryDistrictWeights(float worldZ) const
{
    if (IsTutorial()) { return {}; }
    const auto blend = [worldZ](float start, float length) {
        if (start == -1.0f) { return 0.0f; }
        const float t = std::clamp((worldZ - start) / length, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    return { blend(sceneryCanyonStartZ_, 96.0f), blend(sceneryPlazaStartZ_, 144.0f) };
}

float GameRuntime::GetBossApronStartZ() const
{
    if (IsTutorial() || sceneryPlazaStartZ_ == -1.0f) { return -1.0f; }
    const float defenseStartZ = sceneryCanyonStartZ_ >= 0.0f ?
        (std::max)(sceneryCanyonStartZ_, 450.0f) : sceneryCanyonStartZ_;
    // 中盤の128m敷地の端へ接続。境界の確定時には全ての変更対象が描画範囲より先にある。
    return defenseStartZ + std::ceil((sceneryPlazaStartZ_ + 96.0f - defenseStartZ) /
        kDefenseModuleLength) * kDefenseModuleLength;
}

void GameRuntime::UpdateRailScenery()
{
    // 序盤の敷地（Z=396まで）と路面（Z=450まで）を終えてから中盤へ入る。
    // 負の原点は明示的なDebugフェーズ移動だけで使用する。
    const float defenseStartZ = sceneryCanyonStartZ_ >= 0.0f ?
        (std::max)(sceneryCanyonStartZ_, 450.0f) : sceneryCanyonStartZ_;
    const float bossApronStartZ = GetBossApronStartZ();
    const float nearLocalZ = IsTutorial() ? kSceneryNearLocalZ : kSceneryRecycleNearLocalZ;
    visibleSceneryCount_ = 0;
    for (RailSceneryObject& scenery : railSceneryObjects_) {
        if (!scenery.object) {
            continue;
        }

        float localZ = scenery.isLandmark ? scenery.anchor.z - railDistance_ : WrapSceneryLocalZ(
            scenery.anchor.z -
                railDistance_ * scenery.speedMultiplier +
                scenery.phase + scenery.halfDepth,
            scenery.loopLength, nearLocalZ) - scenery.halfDepth;
        if (scenery.isDefenseDistrict || scenery.isBossApron) {
            const float startZ = scenery.isBossApron ? bossApronStartZ : defenseStartZ;
            if (startZ == -1.0f) {
                scenery.isVisible = false;
                continue;
            }
            // 原点は区画の境界へ固定する。周期全体を保持し、近・遠クリップの外でだけ循環。
            const float cycle = startZ + scenery.anchor.z - railDistance_ +
                scenery.halfDepth - nearLocalZ;
            localZ = cycle - std::floor(cycle / scenery.loopLength) * scenery.loopLength +
                nearLocalZ - scenery.halfDepth;
        }
        scenery.currentLocalZ = localZ;
        scenery.isVisible =
            localZ + scenery.halfDepth >= nearLocalZ &&
            localZ - scenery.halfDepth <= scenery.drawFarLocalZ;
        if (scenery.isDefenseDistrict) {
            const float worldZ = railDistance_ + localZ;
            // 広場の舗装面へ隙間なく接続し、建物の敷地同士は重ねない。
            scenery.isVisible &= worldZ - scenery.halfDepth >= defenseStartZ - 0.01f &&
                (bossApronStartZ == -1.0f || worldZ + scenery.halfDepth <= bossApronStartZ + 0.01f);
        } else if (scenery.isBossApron) {
            scenery.isVisible &= railDistance_ + localZ - scenery.halfDepth >= bossApronStartZ - 0.01f;
        }
        if (bossApronStartZ != -1.0f && (scenery.isRoad || scenery.isTrackside ||
            scenery.isRoadDetail)) {
            // 舗装は専用面で繋がっている。18m道路片の端まで判定し、最後の歩道が広場へ突き出さない。
            const float halfDepth = scenery.isRoad ? 9.0f : scenery.halfDepth;
            scenery.isVisible &= railDistance_ + localZ + halfDepth < bossApronStartZ;
        }
        if (scenery.isVisible) {
            ++visibleSceneryCount_;
        }
        const float driftPhase =
            railDistance_ * scenery.driftSpeed + scenery.phase * 0.031f;
        const float railCurve =
            std::sin((railDistance_ + localZ) * 0.018f) * 1.25f;
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
            railDistance_ + localZ
        };
        const float yawWobble =
            std::abs(scenery.driftSpeed) > 0.0001f ?
            std::sin(driftPhase * 0.55f) * 0.08f :
            0.0f;
        const Math::Vector3 rotate{
            scenery.rotate.x,
            scenery.rotate.y + yawWobble,
            scenery.rotate.z + railDistance_ * scenery.rollSpeed
        };

        Math::Vector3 scale = scenery.scale;
        if (!IsTutorial()) {
            const auto district = GetSceneryDistrictWeights(position.z);
            const auto value = [district](float avenue, float canyon, float plaza) {
                return Lerp(Lerp(avenue, canyon, district.x), plaza, district.y);
            };
            if (scenery.isBuilding) {
                const float side = scenery.anchor.x < 0.0f ? -1.0f : 1.0f;
                // 最も狭い街区でも旧配置の31.5mを下回らず、既存の退場ルートを塞がない。
                const float facadeX = scenery.isBackRow ? value(54.0f, 46.0f, 89.0f) :
                    value(38.0f, 31.5f, 64.0f);
                position.x = side * facadeX;
                // 施設の接続部に建物がめり込まないよう、左右の敷地を空ける。
                // ワールド座標から決めるため、近づいた途端に建物が動くことはない。
                const float facilityDistance = std::abs(position.z - kTransitLandmarkWorldZ);
                const float setback = std::clamp((78.0f - facilityDistance) / 30.0f, 0.0f, 1.0f);
                position.x += side * setback * (scenery.isBackRow ? 8.0f : 13.0f);
                // 建物ごとの元の高さの差は残す。ボス広場は低い街並みと空でシルエットを抜く。
                scale.y *= scenery.isBackRow ? value(0.95f, 1.65f, 0.90f) : value(0.80f, 1.65f, 0.72f);
                // 序盤だけを運航区画として構成する。ワールド位置に固定し、表示中には動かさない。
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
                // 元の階高・窓の縦横比を戻し、管制塔の輪郭を一般の街並みより上へ抜く。
                const float openingScale = scenery.scale.x * (scenery.isBackRow ? 1.02f : 1.0f);
                scale.y = Lerp(scale.y, openingScale * Lerp(1.0f, 0.72f, bossApron), campus);
                scale.z = Lerp(scale.z, scenery.scale.x, campus);
                const Math::Vector4 districtColor = scenery.isBackRow ?
                    Math::Vector4{ 0.79f, 0.83f, 0.88f, 1.0f } : Math::Vector4{ 0.91f, 0.93f, 0.94f, 1.0f };
                scenery.object->SetColor({ Lerp(scenery.color.x, districtColor.x, campus),
                    Lerp(scenery.color.y, districtColor.y, campus),
                    Lerp(scenery.color.z, districtColor.z, campus), 1.0f });
            } else if (scenery.isRoad) {
                // 回転後の道幅はローカルZ。長さXを変えると連結部に隙間ができる。
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

void GameRuntime::DrawRailScenery(ModelDrawPass drawPass)
{
    if (railSceneryObjects_.empty() || !object3dCommon_) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();
    const bool transparent = drawPass == ModelDrawPass::Transparent;
    object3dCommon_->SetDepthDrawMode(transparent ? DepthDrawMode::ReadOnly : DepthDrawMode::Normal);
    object3dCommon_->SetBlendMode(transparent ? BlendMode::Normal : BlendMode::None);
    object3dCommon_->CommonDrawSetting();

    if (transparent) {
        transparentSceneryDrawOrder_.clear();
        for (const RailSceneryObject& scenery : railSceneryObjects_) {
            if (scenery.object && scenery.isVisible && scenery.isInView && scenery.object->HasTransparentMaterials()) {
                transparentSceneryDrawOrder_.push_back(&scenery);
            }
        }
        // カメラを動かしても手前のガラスが奥の室内を消さないよう、深度は読み取りだけにする。
        const Math::Matrix4x4 view = camera_ ? camera_->GetViewMatrix() : Math::MakeIdentity4x4();
        std::sort(transparentSceneryDrawOrder_.begin(), transparentSceneryDrawOrder_.end(),
            [&view](const RailSceneryObject* a, const RailSceneryObject* b) {
                return TransformCoord(a->object->GetTranslate(), view).z >
                    TransformCoord(b->object->GetTranslate(), view).z;
            });
        for (const RailSceneryObject* scenery : transparentSceneryDrawOrder_) {
            scenery->object->Draw(drawPass);
        }
    } else {
        for (const RailSceneryObject& scenery : railSceneryObjects_) {
            if (scenery.object && scenery.isVisible && scenery.isInView) {
                scenery.object->Draw(drawPass);
            }
        }
    }

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::RenderShadowMap()
{
    if (!object3dCommon_) {
        return;
    }

    const Math::Vector3 shadowFocusCenter{
        0.0f,
        10.0f,
        railDistance_ + 105.0f
    };
    if (!object3dCommon_->BeginShadowPass(shadowFocusCenter)) {
        return;
    }

    const Math::Matrix4x4& lightViewProjection =
        object3dCommon_->GetShadowLightViewProjection();

    for (const auto& sceneObject : sceneObjects_) {
        if (sceneObject) {
            sceneObject->DrawShadow(lightViewProjection);
        }
    }

    // 画面外の建物も道路へ影を落とすため、色描画のisVisibleとは別に範囲を判定する。
    // 路面・窓ガラスは投影しない。描画数は初期化済みの街モデル数で上限が決まる。
    for (const RailSceneryObject& scenery : railSceneryObjects_) {
        if ((scenery.isDefenseDistrict || scenery.isBossApron || scenery.isTrackside) && !scenery.isVisible) { continue; }
        if (scenery.object && scenery.castsShadow &&
            scenery.currentLocalZ + scenery.halfDepth >= -45.0f &&
            scenery.currentLocalZ - scenery.halfDepth <= 240.0f) {
            scenery.object->DrawShadow(lightViewProjection);
        }
    }

    if (player_ && !player_->IsDead()) {
        player_->DrawShadow(lightViewProjection);
    }

    for (const auto& enemy : enemies_) {
        if (enemy && !enemy->IsDead()) {
            enemy->DrawShadow(lightViewProjection);
        }
    }

    object3dCommon_->EndShadowPass();
}

void GameRuntime::InitializeContactShadows()
{
    if (!object3dCommon_ || !effectContactShadowModel_) {
        return;
    }

    for (ContactShadow& shadow : contactShadows_) {
        shadow.object = std::make_unique<Object3d>();
        shadow.object->Initialize(object3dCommon_.get());
        shadow.object->SetModel(effectContactShadowModel_);
        shadow.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        shadow.object->SetRotate({ kContactShadowHalfPi, 0.0f, 0.0f });
        shadow.object->SetScale({ 0.01f, 0.01f, 1.0f });
        shadow.object->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        shadow.object->SetLightingMode(0);
        shadow.object->SetEnvironmentCoefficient(0.0f);
        shadow.object->SetAlphaReference(0.01f);
        shadow.object->Update();
    }
}

void GameRuntime::DrawContactShadowObject(
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

void GameRuntime::DrawContactShadows()
{
    if (!object3dCommon_ || !effectContactShadowModel_) {
        return;
    }

    bool hasShadowObject = false;
    for (const ContactShadow& shadow : contactShadows_) {
        if (shadow.object) {
            hasShadowObject = true;
            break;
        }
    }
    if (!hasShadowObject) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();
    object3dCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    object3dCommon_->SetBlendMode(BlendMode::Normal);
    object3dCommon_->CommonDrawSetting();

    size_t shadowIndex = 0;
    const auto drawNextShadow =
        [&](const Math::Vector3& position,
            float scaleX,
            float scaleZ,
            float alpha,
            float yaw) {
            if (shadowIndex >= contactShadows_.size()) {
                return;
            }
            ContactShadow& shadow = contactShadows_[shadowIndex++];
            if (!shadow.object) {
                return;
            }
            DrawContactShadowObject(
                *shadow.object,
                position,
                scaleX,
                scaleZ,
                alpha,
                yaw);
        };

    if (player_ && !player_->IsDead()) {
        const Math::Vector3 playerPosition = player_->GetTranslate();
        const float height =
            (std::max)(playerPosition.y - kContactShadowY, 0.0f);
        const float heightFade =
            std::clamp(1.0f - height / 8.0f, 0.42f, 1.0f);
        const float dodgeStretch = player_->IsDodging() ? 0.26f : 0.0f;
        drawNextShadow(
            playerPosition,
            1.65f + height * 0.10f + dodgeStretch,
            1.00f + height * 0.055f,
            0.19f * heightFade,
            player_->IsDodging() ?
                static_cast<float>(player_->GetDodgeDirection()) * 0.20f :
                0.0f);
    }

    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }
        const Math::Vector3 enemyPosition = enemy->GetTranslate();
        const float localZ = enemyPosition.z - railDistance_;
        if (localZ < kSceneryNearLocalZ || localZ > 210.0f) {
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
                std::sin(cameraTimer_ * 0.013f) * 0.20f);
        } else {
            const float radius = enemy->GetRadius();
            drawNextShadow(
                enemyPosition,
                1.25f + radius * 0.55f + height * 0.075f,
                0.78f + radius * 0.30f + height * 0.040f,
                0.15f * heightFade * distanceFade,
                std::sin(cameraTimer_ * 0.018f + enemyPosition.x) * 0.18f);
        }
    }

    // 道路沿いの実際の建物に接地影を合わせる。広場の中央へ旧配置の影を残さない。
    for (const RailSceneryObject& scenery : railSceneryObjects_) {
        if (!scenery.isBuilding || scenery.isBackRow || !scenery.isVisible) { continue; }
        const auto p = scenery.object->GetTranslate();
        const float distanceFade = std::clamp(1.0f - (scenery.currentLocalZ - 28.0f) / 230.0f, 0.36f, 1.0f);
        drawNextShadow({ p.x + (p.x < 0.0f ? 3.0f : -3.0f), kContactShadowY, p.z },
            6.0f, 11.0f * scenery.scale.x, 0.050f * distanceFade, 0.0f);
    }

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

int GameRuntime::GetPostEffectMode() const
{
    if (isPostEffectBypassEnabled_) {
        return 0;
    }
    if (postEffectMode_ != 12) {
        return postEffectMode_;
    }
    if (isGameOver_) {
        return 15;
    }
    if (isGameClear_) {
        return 14;
    }
    return 12;
}

std::vector<SceneSerializer::ObjectRecord> GameRuntime::BuildRuntimeSceneRecords() const
{
    std::vector<SceneSerializer::ObjectRecord> records = sceneObjectRecords_;
    if (records.size() < sceneObjects_.size()) {
        records.resize(sceneObjects_.size());
    }

    for (size_t index = 0; index < sceneObjects_.size(); ++index) {
        const Object3d* object = sceneObjects_[index].get();
        if (!object) {
            continue;
        }

        SceneSerializer::ObjectRecord& record = records[index];
        if (record.name.empty()) {
            record.name = "Runtime Object " + std::to_string(index);
        }
        if (record.modelIndex < 0) {
            record.primitive = true;
            record.modelIndex = 0;
        }
        record.translate = object->GetTranslate();
        record.rotate = object->GetRotate();
        record.scale = object->GetScale();
        record.color = object->GetColor();
        record.alphaReference = object->GetAlphaReference();
        record.lightingMode = object->GetLightingMode();
        record.textureFilePath = object->GetTextureFilePath();
    }

    records.resize(sceneObjects_.size());
    return records;
}

SceneSerializer::SceneSettings GameRuntime::BuildRuntimeSceneSettings() const
{
    SceneSerializer::SceneSettings settings{};
    if (camera_) {
        settings.hasCamera = true;
        settings.cameraTranslate = camera_->GetTranslate();
        settings.cameraRotate = camera_->GetRotate();
    }
    return settings;
}

bool GameRuntime::SaveSceneObjects(const char* path)
{
    if (!path || path[0] == '\0') {
        editorStatusMessage_ = "保存失敗: パスが無効です。";
        return false;
    }

    const std::vector<SceneSerializer::ObjectRecord> records =
        BuildRuntimeSceneRecords();
    const bool saved =
        SceneSerializer::SaveScene(path, records, BuildRuntimeSceneSettings());
    if (!saved) {
        editorStatusMessage_ = "保存失敗: " + std::string(path);
        return false;
    }

    sceneObjectRecords_ = records;
    currentSceneFilePath_ = path;
    editorStatusMessage_ = "保存しました: " + currentSceneFilePath_;
    return true;
}

bool GameRuntime::LoadSceneObjects(const char* path)
{
    std::vector<SceneSerializer::ObjectRecord> records;
    SceneSerializer::SceneSettings settings{};
    if (!SceneSerializer::LoadScene(path, records, settings)) {
        editorStatusMessage_ = "読み込み失敗: " + std::string(path ? path : "");
        return false;
    }

    sceneObjects_.clear();
    sceneObjectRecords_.clear();

    if (settings.hasCamera && camera_) {
        cameraTranslate_ = settings.cameraTranslate;
        cameraRotate_ = settings.cameraRotate;
        cameraFovY_ = IsTutorial() ? kGameplayCameraBaseFovY : kRushCameraBaseFovY;
        camera_->SetTranslate(settings.cameraTranslate);
        camera_->SetRotate(settings.cameraRotate);
        camera_->SetFovY(cameraFovY_);
    }

    const int modelItemCount =
        static_cast<int>(
            sizeof(kRuntimeSceneModelItems) / sizeof(kRuntimeSceneModelItems[0]));
    for (const SceneSerializer::ObjectRecord& record : records) {
        if (record.modelIndex < 0 || modelItemCount <= record.modelIndex) {
            continue;
        }

        auto sceneObject = std::make_unique<Object3d>();
        sceneObject->Initialize(object3dCommon_.get());
        sceneObject->SetModel(kRuntimeSceneModelItems[record.modelIndex]);
        sceneObject->SetTranslate(record.translate);
        sceneObject->SetRotate(record.rotate);
        sceneObject->SetScale(record.scale);
        sceneObject->SetColor(record.color);
        sceneObject->SetAlphaReference(record.alphaReference);
        sceneObject->SetLightingMode(record.lightingMode);
        if (!record.textureFilePath.empty()) {
            sceneObject->SetTextureFilePath(record.textureFilePath);
        }
        sceneObject->SetEnvironmentCoefficient(0.0f);
        sceneObject->Update();
        sceneObjects_.push_back(std::move(sceneObject));
        sceneObjectRecords_.push_back(record);
    }

    currentSceneFilePath_ = path ? path : "";
    editorStatusMessage_ = "読み込みました: " + currentSceneFilePath_;
    return true;
}

void GameRuntime::Draw()
{
    if (dxCommon_ && camera_ && player_ && !IsTutorial()) {
        Math::Vector2 viewportMin{}, viewportSize{};
        GetEffectiveHudViewportRect(viewportMin, viewportSize);
        const auto screenUv = [&](const Math::Vector2& screen) {
            return Math::Vector2{
                (screen.x - viewportMin.x) / (std::max)(viewportSize.x, 1.0f),
                (screen.y - viewportMin.y) / (std::max)(viewportSize.y, 1.0f) };
        };
        Math::Vector2 vanishing{ viewportMin.x + viewportSize.x * 0.5f, viewportMin.y + viewportSize.y * 0.45f };
        Math::Vector2 playerScreen{ viewportMin.x + viewportSize.x * 0.5f, viewportMin.y + viewportSize.y * 0.7f };
        TryProjectToScreen({ 0.0f, 1.0f, railDistance_ + 260.0f }, vanishing);
        TryProjectToScreen(player_->GetTranslate(), playerScreen);
        dxCommon_->SetFlightBlur(isGameOver_ || isGameClear_ ? 0.0f : flightCameraMotion_.blurStrength,
            screenUv(vanishing), screenUv(playerScreen), screenUv(reticleScreen_), camera_->GetAspectRatio());
    }
    RenderShadowMap();

    if (showSkybox_ && skybox_) {
        skybox_->Draw();
    }

    DrawRailScenery(ModelDrawPass::Opaque);
    DrawContactShadows();
    DrawDepthCueEffects();

    object3dCommon_->CommonDrawSetting();
    for (const auto& sceneObject : sceneObjects_) {
        sceneObject->Draw();
    }
    for (const RewardHeart& heart : rewardHearts_) {
        if (heart.isActive && heart.object) {
            heart.object->Draw();
        }
    }
    if (player_) {
        player_->Draw();
    }
    DrawPlayerFlightAura();
    DrawPlayerDodgeAfterimages();
    for (const auto& bullet : playerBullets_) {
        bullet->Draw();
    }
    for (const auto& bullet : enemyBullets_) {
        bullet->Draw();
    }
    for (const auto& enemy : enemies_) {
        enemy->Draw();
    }
    // 不透明な背景・機体を描き終えてから、窓ガラスだけを合成する。
    DrawRailScenery(ModelDrawPass::Transparent);
    DrawBulletEffectObjects();
    DrawRiftObjects();
    DrawHitEffectObjects();
    DrawPhantomRaidObjects();
    if (gpuPlayerExhaustEnabled_ && camera_) {
        ParticleManager* particleManager = ParticleManager::GetInstance();
        particleManager->Update(
            camera_->GetViewMatrix(),
            camera_->GetProjectionMatrix(), !isPaused_);
        particleManager->Draw(!isPaused_);
    }
}

void GameRuntime::PrewarmBulletPools()
{
    playerBulletPool_.clear();
    enemyBulletPool_.clear();

    if (!object3dCommon_ || !bulletModel_) {
        return;
    }

    playerBulletPool_.reserve(kTargetPlayerBulletPoolCount);
    enemyBulletPool_.reserve(kTargetEnemyBulletPoolCount);
    for (int index = 0; index < kInitialPlayerBulletPoolCount; ++index) {
        playerBulletPool_.push_back(CreatePooledPlayerBullet());
    }
    for (int index = 0; index < kInitialEnemyBulletPoolCount; ++index) {
        enemyBulletPool_.push_back(CreatePooledEnemyBullet());
    }
}

std::unique_ptr<Bullet> GameRuntime::CreatePooledPlayerBullet()
{
    const Math::Vector3 offscreenPosition{ 0.0f, -1000.0f, -1000.0f };

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(
        object3dCommon_.get(),
        bulletModel_,
        offscreenPosition,
        { 0.0f, 0.0f, playerBulletSpeed_ },
        { 0.70f, 1.0f, 1.0f, 0.88f },
        1,
        { 0.36f, 0.36f, 0.88f },
        0.40f,
        1,
        effectBulletGlowModel_ ? effectBulletGlowModel_ : effectPlayerBulletCoreModel_,
        { 0.66f, 1.0f, 1.0f, 0.88f },
        { 0.72f, 1.75f, 1.0f },
        effectPlayerBulletTrailModel_,
        { 0.54f, 0.96f, 1.0f, 0.64f },
        { 0.42f, 3.50f, 1.0f },
        1.78f,
        nullptr,
        { 0.70f, 1.0f, 1.0f, 0.0f },
        { 0.075f, 0.075f, 1.0f });
    bullet->Kill();
    return bullet;
}

std::unique_ptr<Bullet> GameRuntime::CreatePooledEnemyBullet()
{
    const Math::Vector3 offscreenPosition{ 0.0f, -1000.0f, -1000.0f };

    auto bullet = std::make_unique<Bullet>();
    bullet->Initialize(
        object3dCommon_.get(),
        bulletModel_,
        offscreenPosition,
        { 0.0f, 0.0f, -enemyBulletSpeed_ },
        { 1.0f, 0.18f, 0.42f, 1.0f },
        1,
        { 0.34f, 0.34f, 0.68f },
        0.48f,
        1,
        effectEnemyBulletCoreModel_ ? effectEnemyBulletCoreModel_ : effectBulletGlowModel_,
        { 1.0f, 0.12f, 0.50f, 0.80f },
        { 0.86f, 0.86f, 1.0f },
        effectEnemyBulletTailModel_,
        { 1.0f, 0.10f, 0.44f, 0.44f },
        { 0.28f, 2.55f, 1.0f },
        1.50f,
        nullptr,
        { 1.0f, 0.30f, 0.70f, 0.0f },
        { 0.14f, 0.14f, 1.0f });
    bullet->Kill();
    return bullet;
}

std::unique_ptr<Bullet> GameRuntime::AcquireBullet(std::vector<std::unique_ptr<Bullet>>& pool)
{
    if (pool.empty()) {
        return nullptr;
    }

    auto bullet = std::move(pool.back());
    pool.pop_back();
    return bullet;
}

void GameRuntime::PrewarmHitEffectObjectPool()
{
    hitEffectObjectPool_.clear();

    if (!object3dCommon_) {
        return;
    }

    hitEffectObjectPool_.reserve(kTargetHitEffectObjectPoolCount);
    for (int index = 0; index < kInitialHitEffectObjectPoolCount; ++index) {
        hitEffectObjectPool_.push_back(CreatePooledHitEffectObject());
    }
}

std::unique_ptr<Object3d> GameRuntime::CreatePooledHitEffectObject()
{
    auto object = std::make_unique<Object3d>();
    object->Initialize(object3dCommon_.get());
    object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
    object->SetScale({ 0.01f, 0.01f, 1.0f });
    object->SetLightingMode(0);
    object->SetEnvironmentCoefficient(0.0f);
    object->Update();
    return object;
}

std::unique_ptr<Object3d> GameRuntime::AcquireHitEffectObject()
{
    if (hitEffectObjectPool_.empty()) {
        ++hitEffectObjectPoolMisses_;
        return nullptr;
    }

    auto object = std::move(hitEffectObjectPool_.back());
    hitEffectObjectPool_.pop_back();
    return object;
}

void GameRuntime::RecycleHitEffectVisuals(HitEffect& effect)
{
    for (size_t index = 0; index < effect.visualCount; ++index) {
        HitEffect::Visual& visual = effect.visuals[index];
        if (!visual.object) {
            continue;
        }
        visual.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        visual.object->SetScale({ 0.01f, 0.01f, 1.0f });
        visual.object->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        visual.object->Update();
        hitEffectObjectPool_.push_back(std::move(visual.object));
    }
    effect.visualCount = 0;
}

void GameRuntime::FirePlayerBullet()
{
    if (!player_ || !bulletModel_) {
        return;
    }

    Math::Vector3 spawnPosition = player_->GetTranslate();
    spawnPosition.z += 1.75f;
    const Enemy* aimedTarget = (isReticleOnTarget_ && lockedEnemy_ && !lockedEnemy_->IsDead()) ? lockedEnemy_ : nullptr;
    Math::Vector3 aimDirection = CalculateAimDirection(spawnPosition);
    if (aimedTarget) {
        const Math::Vector3 targetPosition = aimedTarget->GetAimPosition();
        Math::Vector3 targetDirection = Math::Normalize({
            targetPosition.x - spawnPosition.x,
            targetPosition.y - spawnPosition.y,
            targetPosition.z - spawnPosition.z
        });
        if (std::abs(targetDirection.x) + std::abs(targetDirection.y) + std::abs(targetDirection.z) > 0.001f) {
            aimDirection = targetDirection;
        }
    }
    Math::Vector3 velocity = aimDirection * playerBulletSpeed_;
    Math::Vector4 color{ 1.10f, 1.24f, 1.40f, 1.0f };
    Math::Vector3 scale{ 0.34f, 0.34f, 4.0f };
    float collisionRadius = 0.42f;
    int lifeTimer = 180;
    int hitLimit = 1;
    const bool isCharged = chargeTimer_ >= chargeShotThreshold_;
    const bool isFeverShot = feverTimer_ > 0;
    float feverRed = 0.45f;
    float feverGreen = 0.92f;
    float feverBlue = 1.0f;
    if (isFeverShot) {
        float feverHue = std::fmod(
            cameraTimer_ * 0.020f +
                static_cast<float>(playerBullets_.size() % 6) * 0.145f,
            1.0f);
        if (feverHue < 0.0f) {
            feverHue += 1.0f;
        }
        ImGui::ColorConvertHSVtoRGB(
            feverHue,
            0.64f,
            1.0f,
            feverRed,
            feverGreen,
            feverBlue);
    }

    if (isCharged) {
        velocity = aimDirection * lockBulletSpeed_ * chargedBulletSpeedMultiplier_;
        color = { 1.38f, 1.48f, 1.62f, 1.0f };
        scale = { 0.72f, 0.72f, 7.5f };
        collisionRadius = 1.00f;
        lifeTimer = 260;
        hitLimit = 1;
    }
    if (isFeverShot) {
        velocity = aimDirection * lockBulletSpeed_ * chargedBulletSpeedMultiplier_ * 1.18f;
        color = { 1.18f + feverRed * 0.26f, 1.18f + feverGreen * 0.26f, 1.18f + feverBlue * 0.26f, 1.0f };
        scale = { 0.56f, 0.56f, 6.2f };
        collisionRadius = 1.04f;
        lifeTimer = 280;
        hitLimit = 3;
    }

    // 前進だけを速めて自弾が置き去りになるのを防ぐ。敵の攻撃周期は変えない。
    if (!IsTutorial()) { velocity = velocity * 1.20f; }

    auto bullet = AcquireBullet(playerBulletPool_);
    if (!bullet) {
        ++playerBulletPoolMisses_;
        return;
    }
    Model* coreModel =
        isCharged && effectSparkStarModel_ ? effectSparkStarModel_ : effectGlowCoreModel_;
    bullet->Initialize(
        object3dCommon_.get(),
        bulletModel_,
        spawnPosition,
        velocity,
        color,
        lifeTimer,
        scale,
        collisionRadius,
        hitLimit,
        coreModel,
        isFeverShot ?
            Math::Vector4{
                0.62f + feverRed * 0.38f,
                0.62f + feverGreen * 0.38f,
                0.62f + feverBlue * 0.38f,
                1.0f } :
        isCharged ?
            Math::Vector4{ 0.66f, 0.72f, 1.40f, 0.92f } :
            Math::Vector4{ 0.28f, 0.62f, 1.20f, 0.76f },
        isFeverShot ?
            Math::Vector3{ 0.85f, 0.85f, 1.0f } :
        isCharged ?
            Math::Vector3{ 1.0f, 1.0f, 1.0f } :
            Math::Vector3{ 0.46f, 0.46f, 1.0f },
        effectPlayerBulletTrailModel_,
        isCharged ?
            Math::Vector4{ 0.64f, 0.74f, 1.40f, 0.94f } :
            Math::Vector4{ 0.22f, 0.60f, 1.25f, 0.82f },
        isCharged ?
            Math::Vector3{ 0.48f, 9.0f, 1.0f } :
            Math::Vector3{ 0.28f, 5.5f, 1.0f },
        isFeverShot ? 8.0f : isCharged ? 9.0f : 5.5f,
        nullptr,
        isFeverShot ?
            Math::Vector4{ feverRed, feverGreen, feverBlue, 0.92f } :
        isCharged ?
            Math::Vector4{ 0.72f, 0.94f, 1.0f, 0.72f } :
            Math::Vector4{ 0.34f, 0.72f, 1.0f, 0.0f },
        isFeverShot ?
            Math::Vector3{ 0.24f, 0.24f, 1.0f } :
        isCharged ?
            Math::Vector3{ 0.17f, 0.17f, 1.0f } :
            Math::Vector3{ 0.092f, 0.092f, 1.0f });
    bullet->SetFeverShot(isFeverShot);
    const Enemy* homingTarget = aimedTarget;
    if (!homingTarget && isFeverShot) {
        homingTarget = FindHomingTargetForBullet(*bullet);
    }
    if (homingTarget) {
        bullet->EnableHoming(
            isFeverShot ? (aimedTarget ? 0.18f : 0.105f) :
            (isCharged ? 0.18f : 0.08f));
        bullet->SetHomingTarget(homingTarget->GetAimPosition());
        homingBulletTargets_[bullet.get()] = homingTarget;
    }
    playerBullets_.push_back(std::move(bullet));
    ++playerShotsFired_;
    // フィーバー連射に長いチャージ音を重ね続けない。単発の溜め撃ちだけ重い発射音にする。
    PlaySfx(isCharged && !isFeverShot ? "charge" : "shot");
    maxActivePlayerBullets_ =
        (std::max)(maxActivePlayerBullets_, playerBullets_.size());
    chargeTimer_ = feverTimer_ > 0 ? kChargeShotMax : 0;
    if (isCharged) {
        AddMuzzleFlashEffect(spawnPosition, true);
        chargeFlashTimer_ = 18;
    } else {
        AddMuzzleFlashEffect(spawnPosition, false);
    }
}

void GameRuntime::FireEnemyBullet(
    const Math::Vector3& position,
    EnemyBulletStyle style,
    Math::Vector2 aimOffset,
    const Math::Vector3* fixedAim)
{
    if (!bulletModel_) {
        return;
    }

    Math::Vector3 spawnPosition = position;
    spawnPosition.z -= 1.0f;

    Math::Vector4 bodyColor{ 1.32f, 1.02f, 0.90f, 1.0f };
    Math::Vector3 bodyScale{ 0.34f, 0.34f, 0.68f };
    Math::Vector4 glowColor{ 1.10f, 0.06f, 0.10f, 0.90f };
    Math::Vector3 glowScale{ 0.96f, 0.96f, 1.0f };
    Math::Vector4 trailColor{ 1.0f, 0.06f, 0.10f, 0.48f };
    Math::Vector3 trailScale{ 0.16f, 2.55f, 1.0f };
    float trailOffset = 1.50f;
    float speedScale = IsTutorial() ? 1.0f : 1.25f;
    float collisionRadius = 0.48f;
    int lifeTimer = 260;
    int damage = 8;

    switch (style) {
    case EnemyBulletStyle::Crossfire:
        bodyColor = { 1.32f, 0.98f, 1.06f, 1.0f };
        bodyScale = { 0.24f, 0.24f, 1.15f };
        glowColor = { 1.06f, 0.06f, 0.20f, 0.92f };
        glowScale = { 0.72f, 1.05f, 1.0f };
        trailColor = { 1.0f, 0.06f, 0.18f, 0.56f };
        trailScale = { 0.20f, 3.30f, 1.0f };
        trailOffset = 1.90f;
        speedScale = IsTutorial() ? 1.12f : 1.40f;
        collisionRadius = 0.42f;
        damage = 7;
        break;
    case EnemyBulletStyle::Sniper:
        bodyColor = { 1.45f, 0.88f, 0.78f, 1.0f };
        bodyScale = { 0.34f, 0.34f, 1.82f };
        glowColor = { 1.0f, 0.08f, 0.18f, 1.0f };
        glowScale = { 1.18f, 1.48f, 1.0f };
        trailColor = { 1.0f, 0.04f, 0.12f, 0.82f };
        trailScale = { 0.24f, 6.80f, 1.0f };
        trailOffset = 3.55f;
        speedScale = IsTutorial() ? 1.67f : 1.90f;
        collisionRadius = 0.56f;
        lifeTimer = 210;
        damage = 14;
        break;
    case EnemyBulletStyle::ShieldOrb:
        bodyColor = { 1.36f, 1.16f, 0.84f, 1.0f };
        bodyScale = { 0.62f, 0.62f, 0.62f };
        glowColor = { 1.12f, 0.36f, 0.08f, 0.94f };
        glowScale = { 1.42f, 1.42f, 1.0f };
        trailColor = { 1.0f, 0.30f, 0.06f, 0.48f };
        trailScale = { 0.34f, 1.42f, 1.0f };
        trailOffset = 0.78f;
        speedScale = IsTutorial() ? 0.78f : 0.95f;
        collisionRadius = 0.62f;
        lifeTimer = 300;
        damage = 6;
        break;
    case EnemyBulletStyle::BossCannon:
        bodyColor = { 1.45f, 0.94f, 0.42f, 1.0f };
        bodyScale = { 0.58f, 0.58f, 1.18f };
        glowColor = { 1.0f, 0.18f, 0.08f, 1.0f };
        glowScale = { 1.38f, 1.58f, 1.0f };
        trailColor = { 1.0f, 0.12f, 0.04f, 0.72f };
        trailScale = { 0.42f, 4.35f, 1.0f };
        trailOffset = 2.45f;
        speedScale = 1.08f;
        collisionRadius = 0.72f;
        lifeTimer = 280;
        damage = 12;
        break;
    case EnemyBulletStyle::BossCharge:
        bodyColor = { 1.65f, 1.18f, 0.52f, 1.0f };
        bodyScale = { 0.82f, 0.82f, 1.95f };
        glowColor = { 1.0f, 0.10f, 0.04f, 1.0f };
        glowScale = { 1.85f, 2.10f, 1.0f };
        trailColor = { 1.0f, 0.08f, 0.02f, 0.82f };
        trailScale = { 0.55f, 5.80f, 1.0f };
        trailOffset = 3.10f;
        speedScale = 1.76f;
        collisionRadius = 0.86f;
        lifeTimer = 220;
        damage = 16;
        break;
    case EnemyBulletStyle::Standard:
    default:
        break;
    }

    Math::Vector3 bulletDirection{ 0.0f, 0.0f, -1.0f };
    if (player_) {
        Math::Vector3 targetPosition = player_->GetTranslate();
        if (fixedAim) {
            targetPosition.x = fixedAim->x;
            targetPosition.y = fixedAim->y;
        }
        targetPosition.x += aimOffset.x;
        targetPosition.y += 0.08f + aimOffset.y;
        targetPosition.z += 0.18f;
        bulletDirection = EnemyFireControl::SolveShotDirection(
            spawnPosition, targetPosition, railSpeed_, enemyBulletSpeed_ * speedScale);
        if (bulletDirection.z > -0.10f) {
            bulletDirection.z = -0.10f;
            bulletDirection = Math::Normalize(bulletDirection);
        }
    }

    auto bullet = AcquireBullet(enemyBulletPool_);
    if (!bullet) {
        ++enemyBulletPoolMisses_;
        return;
    }
    bullet->Initialize(
        object3dCommon_.get(),
        bulletModel_,
        spawnPosition,
        bulletDirection * enemyBulletSpeed_ * speedScale,
        bodyColor,
        lifeTimer,
        bodyScale,
        collisionRadius,
        1,
        effectEnemyBulletCoreModel_ ? effectEnemyBulletCoreModel_ : effectBulletGlowModel_,
        glowColor,
        glowScale,
        effectEnemyBulletTailModel_,
        trailColor,
        trailScale,
        trailOffset,
        nullptr,
        { 1.0f, 0.30f, 0.70f, 0.0f },
        { 0.14f, 0.14f, 1.0f },
        damage);
    AddEnemyMuzzleFlashEffect(spawnPosition);
    enemyBullets_.push_back(std::move(bullet));
    if (style == EnemyBulletStyle::BossCannon || style == EnemyBulletStyle::BossCharge) { ++bossShotsFired_; }
    maxActiveEnemyBullets_ =
        (std::max)(maxActiveEnemyBullets_, enemyBullets_.size());
}

void GameRuntime::SpawnEnemy()
{
    if (!object3dCommon_ || currentWaveIndex_ >= kWaveCount) {
        return;
    }

    const WaveTuning& wave = waveTuning_[currentWaveIndex_];
    const int remainingEnemyCount =
        wave.enemyCount - spawnedEnemyCountInWave_;
    if (remainingEnemyCount <= 0) {
        return;
    }

    const EnemySpawnPattern firstPattern =
        GetEnemySpawnPattern(currentWaveIndex_, spawnSequenceIndex_);
    int spawnBatchCount = 1;
    if (firstPattern.behavior == Enemy::Behavior::Formation &&
        firstPattern.entryStyle == Enemy::EntryStyle::VFormation) {
        while (spawnBatchCount < 3 && spawnBatchCount < remainingEnemyCount) {
            const EnemySpawnPattern nextPattern =
                GetEnemySpawnPattern(
                    currentWaveIndex_,
                    spawnSequenceIndex_ + spawnBatchCount);
            if (nextPattern.behavior != firstPattern.behavior ||
                nextPattern.entryStyle != firstPattern.entryStyle) {
                break;
            }
            ++spawnBatchCount;
        }
    }

    for (int index = 0; index < spawnBatchCount; ++index) {
        const EnemySpawnPattern pattern =
            GetEnemySpawnPattern(currentWaveIndex_, spawnSequenceIndex_);
        ++spawnSequenceIndex_;
        ++spawnedEnemyCountInWave_;

        Model* model = GetEnemyModelForBehavior(pattern.behavior);
        if (!model) {
            continue;
        }
        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize(
            object3dCommon_.get(),
            model,
            { pattern.x, pattern.y, railDistance_ + wave.spawnLeadDistance },
            pattern.behavior,
            pattern.entryStyle,
            0,
            1.0f,
            GetEnemyTextureOverrideForBehavior(pattern.behavior));
        enemies_.push_back(std::move(enemy));
    }
}

void GameRuntime::SpawnStageEnemy(
    float x,
    float y,
    float leadDistance,
    Enemy::Behavior behavior,
    Enemy::EntryStyle entryStyle,
    int maxHpOverride,
    float scaleMultiplier)
{
    if (!object3dCommon_) {
        return;
    }

    Model* model = GetEnemyModelForBehavior(behavior);
    const char* textureOverride = GetEnemyTextureOverrideForBehavior(behavior);
    if ((behavior == Enemy::Behavior::StrafeShooter &&
        maxHpOverride >= 7 &&
        enemyHeavyModel_) ||
        (behavior == Enemy::Behavior::Shield && enemyHeavyModel_)) {
        model = enemyHeavyModel_;
        textureOverride = kEnemyHeavyTexturePath;
    }
    if (!model) {
        return;
    }
    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(
        object3dCommon_.get(),
        model,
        { x, y, railDistance_ + leadDistance },
        behavior,
        entryStyle,
        maxHpOverride,
        scaleMultiplier,
        textureOverride);
    enemy->SetRiftCarrier(!IsTutorial() && behavior == Enemy::Behavior::Formation &&
        (entryStyle != Enemy::EntryStyle::TightFormation || std::abs(x) < 1.0f));
    enemies_.push_back(std::move(enemy));
    ++spawnSequenceIndex_;
    ++spawnedEnemyCountInWave_;
}

Model* GameRuntime::GetEnemyModelForBehavior(Enemy::Behavior behavior) const
{
    switch (behavior) {
    case Enemy::Behavior::Support:
        return enemyFormationModel_ ? enemyFormationModel_ : enemyModel_;
    case Enemy::Behavior::Shield:
        return enemyHeavyModel_ ? enemyHeavyModel_ :
            (enemyFormationModel_ ? enemyFormationModel_ : enemyModel_);
    case Enemy::Behavior::Sniper:
        return enemyShooterModel_ ? enemyShooterModel_ : enemyModel_;
    case Enemy::Behavior::DiveBomber:
        return enemySwoopModel_ ? enemySwoopModel_ : enemyModel_;
    case Enemy::Behavior::Swoop:
        return enemySwoopModel_ ? enemySwoopModel_ : enemyModel_;
    case Enemy::Behavior::StrafeShooter:
        return enemyShooterModel_ ? enemyShooterModel_ : enemyModel_;
    case Enemy::Behavior::Crossfire:
        return enemyShooterModel_ ? enemyShooterModel_ : enemyModel_;
    case Enemy::Behavior::Formation:
    default:
        return enemyFormationModel_ ? enemyFormationModel_ : enemyModel_;
    }
}

const char* GameRuntime::GetEnemyTextureOverrideForBehavior(Enemy::Behavior behavior) const
{
    switch (behavior) {
    case Enemy::Behavior::Support:
        return enemyFormationModel_ ? kEnemyFormationTexturePath : nullptr;
    case Enemy::Behavior::Shield:
        return enemyHeavyModel_ ? kEnemyHeavyTexturePath :
            (enemyFormationModel_ ? kEnemyFormationTexturePath : nullptr);
    case Enemy::Behavior::Sniper:
        return enemyShooterModel_ ? kEnemyShooterTexturePath : nullptr;
    case Enemy::Behavior::DiveBomber:
        return enemySwoopModel_ ? kEnemySwoopTexturePath : nullptr;
    case Enemy::Behavior::Swoop:
        return enemySwoopModel_ ? kEnemySwoopTexturePath : nullptr;
    case Enemy::Behavior::StrafeShooter:
        return enemyShooterModel_ ? kEnemyShooterTexturePath : nullptr;
    case Enemy::Behavior::Crossfire:
        return enemyShooterModel_ ? kEnemyShooterTexturePath : nullptr;
    case Enemy::Behavior::Formation:
    default:
        return enemyFormationModel_ ? kEnemyFormationTexturePath : nullptr;
    }
}

void GameRuntime::SpawnBossEnemy()
{
    if (IsTutorial() || bossSpawned_ ||
        bossDefeated_ ||
        isGameOver_ ||
        isGameClear_ ||
        stageProgress_ < kBossSpawnDistance ||
        !object3dCommon_) {
        return;
    }

    Model* model = bossModel_ ? bossModel_ : enemyModel_;
    if (!model) {
        return;
    }

    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(
        object3dCommon_.get(),
        model,
        { 0.0f, 2.05f, railDistance_ + 98.0f },
        Enemy::Behavior::Boss,
        Enemy::EntryStyle::Direct,
        kBossMaxHp,
        1.0f,
        kBossTexturePath);
    enemies_.push_back(std::move(enemy));
    bossSpawned_ = true;
    bossIntroTimer_ = kBossIntroDuration;
    bossWarningTimer_ = (std::max)(bossWarningTimer_, 42);
    bossPhaseTransitionTimer_ = 0;
    bossAttackCooldown_ = 0;
    bossAttackStepTimer_ = 0;
    bossAttackStep_ = -1;
    bossAttackPattern_ = 0;
    bossAttackSequence_ = 0;
    bossShotsFired_ = 0;
    bossPhase_ = 1;
    bossCounterTimer_ = 0;
    bossCounterDuration_ = 1;
    stageCombatBeatName_ = "Boss";
    AddCameraShake(0.18f, 36);
}

void GameRuntime::UpdateStageEnemyEvents()
{
    if (IsTutorial()) { return; }
    if (isGameOver_ || isGameClear_) {
        return;
    }
    if (bossSpawned_) {
        return;
    }

    const auto enemyEvents = GetEnemySchedule(IsTutorial());
    const size_t eventCount =
        (std::min)(stageEnemyEventTriggered_.size(), enemyEvents.size());
    int activeStageEnemyCount = 0;
    for (const auto& enemy : enemies_) {
        if (enemy && !enemy->IsDead() && !enemy->IsBoss()) {
            ++activeStageEnemyCount;
        }
    }
    int spawnedEventCountThisFrame = 0;

    for (size_t index = 0; index < eventCount; ++index) {
        const StageEnemySpawnEvent& event = enemyEvents[index];
        if (stageEnemyEventTriggered_[index] ||
            stageProgress_ < event.distance) {
            continue;
        }
        const bool isCrossfirePairStart =
            event.behavior == Enemy::Behavior::Crossfire &&
            (index == 0 ||
                enemyEvents[index - 1].behavior != Enemy::Behavior::Crossfire ||
                enemyEvents[index - 1].distance != event.distance);
        const bool isCrossfirePairEnd =
            event.behavior == Enemy::Behavior::Crossfire &&
            index > 0 &&
            enemyEvents[index - 1].behavior == Enemy::Behavior::Crossfire &&
            enemyEvents[index - 1].distance == event.distance &&
            stageEnemyEventTriggered_[index - 1];
        const int requiredSlots = isCrossfirePairStart ? 2 : 1;
        if ((!isCrossfirePairEnd &&
                activeStageEnemyCount + requiredSlots > kMaxActiveStageEnemiesBeforeBoss) ||
            spawnedEventCountThisFrame >= kMaxStageEnemyEventsPerFrame) {
            break;
        }

        stageEnemyEventTriggered_[index] = true;
        stageCombatBeatName_ = event.beatName;
        SpawnStageEnemy(
            event.x,
            event.y,
            event.leadDistance,
            event.behavior,
            event.entryStyle,
            event.maxHpOverride,
            event.scaleMultiplier);
        ++activeStageEnemyCount;
        ++spawnedEventCountThisFrame;
        AddCameraShake(event.shakePower, event.shakeDuration);
    }
}

void GameRuntime::UpdateEnemyWave()
{
    UpdateStageEnemyEvents();
    SpawnBossEnemy();
}

void GameRuntime::AdvanceEnemyWaveIfCleared()
{
    if (isGameOver_ || isGameClear_) {
        return;
    }

    if (IsTutorial()) {
        // 練習はUpdateTutorialLessonで達成を確認して終了する。
        return;
    }

    if (stageProgress_ < kStageClearDistance) {
        return;
    }

    if (!bossSpawned_ || !bossDefeated_) {
        return;
    }

    if (!enemies_.empty()) {
        return;
    }

    currentWaveIndex_ = kWaveCount - 1;
    isGameClear_ = true;
    resultTransitionTimer_ = 90;
}

void GameRuntime::UpdateLockOnTarget()
{
    lockedEnemy_ = nullptr;
    hasLockTarget_ = false;
    isReticleOnTarget_ = false;

    Math::Vector2 viewportMin{};
    Math::Vector2 viewportSize{};
    GetEffectiveHudViewportRect(viewportMin, viewportSize);
    const Math::Vector2 viewportMax{
        viewportMin.x + viewportSize.x,
        viewportMin.y + viewportSize.y
    };
    if (input_) {
        Math::Vector2 mouseScreen{};
        if (isEditorOverlayVisible_ && hasEditorOverlayViewportRect_) {
            const ImVec2 imguiMouse = ImGui::GetMousePos();
            mouseScreen = { imguiMouse.x, imguiMouse.y };
        } else {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const Math::Vector2& mousePosition = input_->GetMousePosition();
            mouseScreen = {
                viewport->Pos.x + mousePosition.x,
                viewport->Pos.y + mousePosition.y
            };
        }
        reticleScreen_ = {
            std::clamp(mouseScreen.x, viewportMin.x, viewportMax.x),
            std::clamp(mouseScreen.y, viewportMin.y, viewportMax.y)
        };
    } else {
        reticleScreen_ = {
            viewportMin.x + viewportSize.x * 0.5f,
            viewportMin.y + viewportSize.y * 0.5f
        };
    }

    if (!camera_ || isGameOver_ || isGameClear_) {
        return;
    }

    const float lockRadius = (std::max)(lockRadius_, 1.0f);
    const float kLockRadiusSq = lockRadius * lockRadius;
    constexpr float kReticleHitRadius = 24.0f;
    float bestScore = kLockRadiusSq;

    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead() || !enemy->IsTargetable()) {
            continue;
        }

        Math::Vector2 screenPosition{};
        const Math::Vector3 enemyPosition = enemy->GetAimPosition();
        if (!TryProjectToScreen(enemyPosition, screenPosition)) {
            continue;
        }

        float projectedEnemyRadius = 0.0f;
        const float enemyRadius = enemy->GetAimRadius();
        Math::Vector2 edgeScreen{};
        if (TryProjectToScreen(
                { enemyPosition.x + enemyRadius, enemyPosition.y, enemyPosition.z },
                edgeScreen)) {
            const float dx = edgeScreen.x - screenPosition.x;
            const float dy = edgeScreen.y - screenPosition.y;
            projectedEnemyRadius =
                (std::max)(projectedEnemyRadius, std::sqrt(dx * dx + dy * dy));
        }
        if (TryProjectToScreen(
                { enemyPosition.x, enemyPosition.y + enemyRadius, enemyPosition.z },
                edgeScreen)) {
            const float dx = edgeScreen.x - screenPosition.x;
            const float dy = edgeScreen.y - screenPosition.y;
            projectedEnemyRadius =
                (std::max)(projectedEnemyRadius, std::sqrt(dx * dx + dy * dy));
        }
        const float targetHitRadius =
            std::clamp(projectedEnemyRadius * 0.82f + 10.0f, kReticleHitRadius, 72.0f);
        const float targetHitRadiusSq = targetHitRadius * targetHitRadius;

        const float dx = screenPosition.x - reticleScreen_.x;
        const float dy = screenPosition.y - reticleScreen_.y;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq < bestScore) {
            bestScore = distanceSq;
            lockedEnemy_ = enemy.get();
            lockedEnemyScreen_ = screenPosition;
            hasLockTarget_ = true;
            isReticleOnTarget_ = distanceSq <= targetHitRadiusSq;
        }
    }
}

bool GameRuntime::TryProjectToScreen(
    const Math::Vector3& worldPosition,
    Math::Vector2& screenPosition) const
{
    if (!camera_) {
        return false;
    }

    const Math::Matrix4x4& viewProjection = camera_->GetViewProjectionMatrix();
    const float clipX =
        worldPosition.x * viewProjection.m[0][0] +
        worldPosition.y * viewProjection.m[1][0] +
        worldPosition.z * viewProjection.m[2][0] +
        viewProjection.m[3][0];
    const float clipY =
        worldPosition.x * viewProjection.m[0][1] +
        worldPosition.y * viewProjection.m[1][1] +
        worldPosition.z * viewProjection.m[2][1] +
        viewProjection.m[3][1];
    const float clipW =
        worldPosition.x * viewProjection.m[0][3] +
        worldPosition.y * viewProjection.m[1][3] +
        worldPosition.z * viewProjection.m[2][3] +
        viewProjection.m[3][3];

    if (clipW <= 0.001f) {
        return false;
    }

    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    if (ndcX < -1.2f || 1.2f < ndcX || ndcY < -1.2f || 1.2f < ndcY) {
        return false;
    }

    Math::Vector2 viewportMin{};
    Math::Vector2 viewportSize{};
    GetEffectiveHudViewportRect(viewportMin, viewportSize);
    screenPosition = {
        viewportMin.x + (ndcX + 1.0f) * 0.5f * viewportSize.x,
        viewportMin.y + (1.0f - ndcY) * 0.5f * viewportSize.y
    };
    return true;
}

void GameRuntime::AddCombatBurstVisuals(HitEffect& effect, bool fever)
{
    const auto position = effect.worldPosition;
    const float seed = static_cast<float>(defeatChainCount_ % 7) * 0.17f;
    // 暗い一拍を白い刃で断つ。明るい空の上でも爆心と方向が読める。
    AddHitEffectVisual(effect, effectCombatDiscModel_, position,
        { 0.018f, 0.012f, 0.045f, 0.86f }, 2.1f, -0.62f, 0, 0, 1.0f, 0.85f, {}, false, 0.09f);
    for (int side = 0; side < 2; ++side) {
        AddHitEffectVisual(effect, effectCombatBladeModel_, position,
            { 1.70f, 1.55f, 1.90f, 0.96f }, fever ? 1.3f : 1.0f, -0.60f,
            0.06f, 0, 0.19f, fever ? 7.8f : 6.8f, {}, true, 0.23f,
            1.22f + seed + static_cast<float>(side) * kContactShadowHalfPi * 2.0f);
    }
    AddHitEffectVisual(effect, effectCombatArcModel_, position,
        fever ? Math::Vector4{ 1.10f, 0.56f, 1.50f, 0.82f } : Math::Vector4{ 0.94f, 1.05f, 1.35f, 0.72f },
        0.95f, 2.8f, -0.35f, 0.02f, 1.7f, 0.70f, {}, true, 0.32f, 0.32f + seed * 0.25f);
    const int rayCount = (std::min)(4, 2 + defeatChainCount_ / kDefeatChainScoreTierSize);
    for (int index = 0; index < rayCount; ++index) {
        const float angle = kTwoPi * static_cast<float>(index) / static_cast<float>(rayCount) + seed + 0.62f;
        const float speed = 0.34f + 0.045f * static_cast<float>(index % 2);
        AddHitEffectVisual(effect, effectCombatBladeModel_, position,
            fever ? Math::Vector4{ 1.35f, 0.72f, 1.75f, 0.92f } : Math::Vector4{ 1.45f, 1.15f, 0.60f, 0.94f },
            0.85f, -0.66f, 0.08f, 0.02f, 0.15f, 3.1f,
            { std::cos(angle) * speed, std::sin(angle) * speed, -0.01f }, true, 0.68f, 0, 0.035f);
    }
}

void GameRuntime::AddEnemyHitEffect(
    const Math::Vector3& worldPosition,
    float strength)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    const bool isHeavyExplosion = strength >= 1.7f;
    effect.duration = isHeavyExplosion ? 58 : 40;
    const float chainPower = 1.35f + 0.10f * static_cast<float>((std::min)(defeatChainCount_ / 4, 3));
    effect.strength = strength * (isHeavyExplosion ? 1.25f : chainPower);
    effect.scoreValue = 100;
    effect.type = HitEffectType::EnemyDestroy;

    Model* fireballModel = effectExplosionFireballModel_ ? effectExplosionFireballModel_ : effectImpactBurstModel_;
    Model* smokeModel = effectExplosionSmokeModel_ ? effectExplosionSmokeModel_ : effectGlowCoreModel_;
    Model* sparksModel = effectExplosionSparksModel_ ? effectExplosionSparksModel_ : effectSparkStarModel_;
    Model* debrisModel = effectCombatBladeModel_;

    AddHitEffectVisual(effect, smokeModel, worldPosition,
        { 0.32f, 0.31f, 0.29f, 0.46f }, 0.92f, 1.50f, 0.18f, 0.10f, 1.12f, 0.92f,
        { -0.014f, 0.038f, -0.004f }, false);
    AddHitEffectVisual(effect, smokeModel, worldPosition,
        { 0.24f, 0.23f, 0.22f, 0.42f }, 0.82f, 1.75f, -0.14f, 0.16f, 0.86f, 1.05f,
        { 0.038f, 0.028f, -0.006f }, false);
    AddHitEffectVisual(effect, smokeModel, worldPosition,
        { 0.34f, 0.32f, 0.29f, 0.32f }, 0.68f, 1.80f, 0.10f, 0.22f, 1.22f, 0.78f,
        { -0.052f, 0.012f, -0.002f }, false);
    AddHitEffectVisual(effect, smokeModel, worldPosition,
        { 0.20f, 0.20f, 0.19f, 0.30f }, 0.62f, 1.65f, -0.22f, 0.28f, 0.90f, 0.82f,
        { 0.060f, -0.002f, -0.002f }, false);

    AddHitEffectVisual(effect, fireballModel, worldPosition,
        { 1.55f, 0.86f, 0.32f, 1.0f }, 2.05f, 0.94f, 0.34f, 0.00f, 1.08f, 0.92f, {}, true, 0.70f);
    AddHitEffectVisual(effect, fireballModel, worldPosition,
        { 1.35f, 0.32f, 0.07f, 0.90f }, 1.55f, 1.04f, -0.44f, 0.04f, 0.88f, 1.06f,
        { 0.018f, 0.010f, 0.0f }, true, 0.55f);
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.90f, 1.72f, 1.42f, 1.0f }, 1.60f, -0.55f, 0.0f, 0.0f, 1.0f, 1.0f, {}, true, 0.16f);
    AddHitEffectVisual(effect, effectCombatShockModel_, worldPosition,
        { 1.45f, 1.20f, 0.68f, 0.74f }, 1.15f, 4.50f, 0.0f, 0.0f, 1.38f, 0.70f, {}, true, 0.28f);
    AddHitEffectVisual(effect, sparksModel, worldPosition,
        { 1.0f, 0.82f, 0.38f, 0.90f }, 0.74f, 0.60f, 0.32f, 0.00f, 1.0f, 1.0f, {}, true, 0.56f);

    AddHitEffectVisual(effect, debrisModel, worldPosition,
        { 1.50f, 1.18f, 0.60f, 1.0f }, 0.80f, -0.55f, 0.18f, 0.00f, 0.13f, 3.20f,
        { -0.29f, 0.22f, 0.016f }, true, 0.76f, 0, 0.025f);
    AddHitEffectVisual(effect, debrisModel, worldPosition,
        { 1.45f, 0.80f, 0.28f, 0.92f }, 0.76f, -0.50f, -0.16f, 0.02f, 0.14f, 3.10f,
        { 0.32f, 0.15f, 0.014f }, true, 0.72f, 0, 0.028f);
    AddHitEffectVisual(effect, debrisModel, worldPosition,
        { 1.35f, 1.15f, 0.65f, 0.90f }, 0.68f, -0.62f, 0.12f, 0.04f, 0.12f, 2.80f,
        { -0.24f, -0.28f, 0.010f }, true, 0.74f, 0, 0.025f);
    AddHitEffectVisual(effect, debrisModel, worldPosition,
        { 1.50f, 1.30f, 0.80f, 0.86f }, 0.72f, -0.60f, -0.18f, 0.05f, 0.12f, 3.10f,
        { 0.28f, -0.25f, 0.010f }, true, 0.68f, 0, 0.030f);
    AddHitEffectVisual(effect, sparksModel, worldPosition,
        { 1.0f, 0.94f, 0.58f, 0.88f }, 0.38f, 0.95f, 1.15f, 0.02f, 1.0f, 1.0f,
        { -0.104f, 0.154f, 0.006f });
    AddHitEffectVisual(effect, sparksModel, worldPosition,
        { 1.0f, 0.62f, 0.20f, 0.78f }, 0.34f, 0.90f, -1.30f, 0.04f, 1.0f, 1.0f,
        { 0.118f, 0.132f, 0.006f });
    AddHitEffectVisual(effect, sparksModel, worldPosition,
        { 1.0f, 0.42f, 0.14f, 0.64f }, 0.28f, 0.82f, 0.92f, 0.08f, 1.0f, 1.0f,
        { 0.026f, -0.172f, 0.004f });
    AddCombatBurstVisuals(effect, false);
    if (effect.visualCount > 0) {
        hitEffects_.push_back(std::move(effect));
    }
}

void GameRuntime::AddFeverEnemyHitEffect(
    const Math::Vector3& worldPosition,
    float strength)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 46;
    effect.strength = strength * (1.50f + 0.10f * static_cast<float>((std::min)(defeatChainCount_ / 4, 3)));
    effect.scoreValue = 100;
    effect.type = HitEffectType::EnemyDestroy;

    Model* ringModel =
        effectCombatShockModel_ ? effectCombatShockModel_ : effectGlowRingModel_;
    Model* coreModel =
        effectImpactBurstModel_ ? effectImpactBurstModel_ : effectGlowCoreModel_;
    Model* shardModel =
        effectCombatBladeModel_;
    Model* starModel =
        effectSparkStarModel_ ? effectSparkStarModel_ : effectGlowCoreModel_;
    const std::array<Math::Vector4, 4> rainbowColors = {
        Math::Vector4{ 1.35f, 0.56f, 1.55f, 0.96f },
        Math::Vector4{ 1.55f, 1.20f, 0.55f, 0.96f },
        Math::Vector4{ 0.72f, 1.20f, 1.65f, 0.96f },
        Math::Vector4{ 1.60f, 1.52f, 1.80f, 0.96f }
    };

    AddHitEffectVisual(
        effect,
        ringModel,
        worldPosition,
        { 1.45f, 1.35f, 1.75f, 0.94f },
        0.72f,
        4.50f,
        0.0f,
        0.0f,
        1.26f,
        0.72f,
        {}, true, 0.30f);
    AddHitEffectVisual(
        effect,
        ringModel,
        worldPosition,
        { 1.20f, 0.34f, 1.45f, 0.78f },
        0.50f,
        3.45f,
        1.25f,
        0.02f,
        0.82f,
        1.22f,
        {}, true, 0.42f);
    AddHitEffectVisual(
        effect,
        coreModel,
        worldPosition,
        { 1.80f, 1.65f, 1.90f, 1.0f },
        1.60f,
        -0.45f,
        -0.72f,
        0.0f,
        1.12f,
        0.90f,
        {}, true, 0.24f);

    constexpr int kShardCount = 8;
    for (int index = 0; index < kShardCount; ++index) {
        const float angle =
            kTwoPi * static_cast<float>(index) /
            static_cast<float>(kShardCount) + 0.18f;
        const float speed = 0.25f + 0.03f * static_cast<float>(index % 3);
        const Math::Vector3 velocity{
            std::cos(angle) * speed,
            std::sin(angle) * speed,
            0.008f * static_cast<float>((index % 2) * 2 - 1)
        };
        AddHitEffectVisual(
            effect,
            shardModel,
            worldPosition,
            rainbowColors[static_cast<size_t>(index) % rainbowColors.size()],
            0.68f + 0.10f * static_cast<float>(index % 2),
            -0.55f,
            (index % 2 == 0 ? 0.25f : -0.25f),
            0.01f * static_cast<float>(index % 3),
            0.12f,
            2.8f,
            velocity, true, 0.72f, 0, 0.032f);
    }
    AddHitEffectVisual(
        effect,
        starModel,
        worldPosition,
        { 0.34f, 1.0f, 0.88f, 0.90f },
        0.50f,
        1.15f,
        1.8f,
        0.02f,
        1.0f,
        1.0f,
        { -0.052f, 0.096f, 0.0f });
    AddHitEffectVisual(
        effect,
        starModel,
        worldPosition,
        { 1.0f, 0.38f, 0.82f, 0.86f },
        0.42f,
        1.32f,
        -2.1f,
        0.05f,
        1.0f,
        1.0f,
        { 0.066f, -0.076f, 0.0f });

    AddCombatBurstVisuals(effect, true);
    if (effect.visualCount > 0) {
        hitEffects_.push_back(std::move(effect));
    }
}

void GameRuntime::AddEnemyImpactEffect(
    const Math::Vector3& worldPosition,
    float strength)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    const bool isHeavyImpact = strength >= 1.35f;
    effect.duration = isHeavyImpact ? 20 : 14;
    effect.strength = strength * (isHeavyImpact ? 1.08f : 0.96f);
    effect.type = HitEffectType::EnemyImpact;

    Model* burstModel =
        effectImpactBurstModel_ ? effectImpactBurstModel_ : effectGlowCoreModel_;
    Model* shardModel =
        effectCombatBladeModel_;

    AddHitEffectVisual(
        effect, burstModel, worldPosition,
        { 1.0f, 0.97f, 0.86f, 0.96f },
        isHeavyImpact ? 1.05f : 0.72f,
        -0.22f, 0.0f, 0.0f, 1.45f, 0.62f, {}, true, 0.36f);
    AddHitEffectVisual(
        effect, burstModel, worldPosition,
        { 1.05f, 0.88f, 0.62f, 0.62f },
        isHeavyImpact ? 0.85f : 0.55f,
        isHeavyImpact ? 0.82f : 0.62f,
        -0.4f, 0.0f, 1.62f, 0.34f, {}, true, 0.55f);

    for (int side = 0; side < 2; ++side) {
        AddHitEffectVisual(effect, effectCombatBladeModel_, worldPosition,
            { 1.55f, 1.35f, 1.62f, 0.94f }, isHeavyImpact ? 1.0f : 0.65f,
            -0.70f, 0, 0, 0.13f, 4.2f, {}, true, 0.42f,
            1.1f + static_cast<float>(side) * kContactShadowHalfPi * 2.0f);
    }
    constexpr int kImpactShardCount = 6;
    for (int index = 0; index < kImpactShardCount; ++index) {
        const float angle =
            kTwoPi * static_cast<float>(index) /
            static_cast<float>(kImpactShardCount) + 0.28f;
        const float speed =
            (isHeavyImpact ? 0.26f : 0.20f) + 0.018f * static_cast<float>(index % 2);
        AddHitEffectVisual(
            effect,
            shardModel,
            worldPosition,
            index % 2 == 0 ?
                Math::Vector4{ 1.55f, 1.20f, 0.60f, 0.96f } :
                Math::Vector4{ 1.20f, 1.30f, 1.60f, 0.90f },
            isHeavyImpact ? 0.45f : 0.30f,
            -0.55f,
            index % 2 == 0 ? 0.18f : -0.18f,
            0.0f,
            0.12f,
            2.4f,
            { std::cos(angle) * speed, std::sin(angle) * speed, 0.004f }, true, 0.88f, 0, 0.04f);
    }

    if (effect.visualCount > 0) {
        hitEffects_.push_back(std::move(effect));
    }
}

void GameRuntime::AddFeverEnemyImpactEffect(
    const Math::Vector3& worldPosition,
    float strength)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 20;
    effect.strength = strength * 1.10f;
    effect.type = HitEffectType::EnemyImpact;

    Model* ringModel =
        effectCombatShockModel_ ? effectCombatShockModel_ : effectGlowRingModel_;
    Model* starModel =
        effectSparkStarModel_ ? effectSparkStarModel_ : effectGlowCoreModel_;
    Model* shardModel =
        effectCombatBladeModel_;
    AddHitEffectVisual(
        effect,
        ringModel,
        worldPosition,
        { 1.25f, 0.62f, 1.60f, 0.90f },
        0.65f,
        2.75f,
        0.0f,
        0.0f,
        1.20f,
        0.72f,
        {}, true, 0.62f);
    AddHitEffectVisual(
        effect,
        effectGlowCoreModel_,
        worldPosition,
        { 1.70f, 1.55f, 1.80f, 0.96f },
        0.78f,
        0.34f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        {}, true, 0.40f);
    AddHitEffectVisual(
        effect,
        starModel,
        worldPosition,
        { 1.40f, 0.56f, 1.70f, 0.92f },
        0.29f,
        0.58f,
        1.5f,
        0.0f,
        1.0f,
        1.0f,
        { -0.060f, 0.036f, 0.0f });
    AddHitEffectVisual(
        effect,
        shardModel,
        worldPosition,
        { 0.32f, 0.72f, 1.0f, 0.82f },
        0.25f,
        0.42f,
        -1.9f,
        0.02f,
        0.38f,
        1.45f,
        { 0.072f, -0.026f, 0.0f });
    AddHitEffectVisual(
        effect,
        shardModel,
        worldPosition,
        { 0.72f, 1.0f, 0.32f, 0.78f },
        0.23f,
        0.48f,
        2.1f,
        0.03f,
        0.38f,
        1.45f,
        { -0.068f, -0.040f, 0.0f });
    for (int index = 0; index < 5; ++index) {
        const float angle = kTwoPi * static_cast<float>(index) / 5.0f + 0.32f;
        AddHitEffectVisual(effect, shardModel, worldPosition,
            { 1.45f, 1.15f, 1.70f, 0.94f }, 0.48f, -0.60f, 0.12f, 0,
            0.12f, 2.8f, { std::cos(angle) * 0.27f, std::sin(angle) * 0.27f, 0 },
            true, 0.84f, 0, 0.035f);
    }
    if (effect.visualCount > 0) {
        hitEffects_.push_back(std::move(effect));
    }
}

void GameRuntime::AddEnemyMuzzleFlashEffect(const Math::Vector3& worldPosition)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 8;
    effect.strength = 0.76f;
    effect.type = HitEffectType::EnemyImpact;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 1.12f, 0.10f, 0.08f, 0.70f }, 0.42f, 0.18f, 0.0f, 0.0f, 1.20f, 0.86f, {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.25f, 0.84f, 0.68f, 0.68f }, 0.24f, 0.42f, 0.0f, 0.02f, 0.76f, 1.24f, { 0.0f, 0.0f, -0.018f });
    hitEffects_.push_back(std::move(effect));
}

void GameRuntime::AddMuzzleFlashEffect(
    const Math::Vector3& worldPosition,
    bool isCharged)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = isCharged ? 10 : 6;
    effect.strength = isCharged ? 1.30f : 0.90f;
    effect.type = HitEffectType::EnemyImpact;

    AddHitEffectVisual(effect, effectImpactBurstModel_ ? effectImpactBurstModel_ : effectGlowCoreModel_, worldPosition,
        isCharged ?
            Math::Vector4{ 1.45f, 1.55f, 1.65f, 0.84f } :
            Math::Vector4{ 1.08f, 1.20f, 1.36f, 0.74f },
        isCharged ? 0.68f : 0.40f,
        isCharged ? 0.22f : 0.14f,
        0.0f,
        0.0f,
        1.25f,
        0.72f,
        {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        isCharged ?
            Math::Vector4{ 0.62f, 0.94f, 1.0f, 0.62f } :
            Math::Vector4{ 0.30f, 0.76f, 1.0f, 0.50f },
        isCharged ? 0.24f : 0.16f,
        0.36f,
        isCharged ? 1.4f : -1.0f,
        0.02f,
        1.0f,
        0.58f,
        {});

    for (int side = 0; side < 2; ++side) {
        AddHitEffectVisual(effect, effectCombatBladeModel_, worldPosition,
            { 1.40f, 1.50f, 1.80f, 0.90f }, 0.65f, -0.76f,
            0, 0, 0.11f, isCharged ? 4.5f : 2.8f, {}, true, 0.55f,
            kContactShadowHalfPi + static_cast<float>(side) * kContactShadowHalfPi * 2.0f);
    }
    hitEffects_.push_back(std::move(effect));
}

void GameRuntime::AddRewardHeartCollectEffect(const Math::Vector3& worldPosition)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 16;
    effect.strength = 0.48f;
    effect.scoreValue = kRewardHeartScoreValue;
    effect.type = HitEffectType::RewardCollect;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 1.0f, 0.66f, 0.30f, 0.44f }, 0.24f, 0.08f, 0.0f, 0.0f, 1.0f, 0.82f, {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 1.0f, 0.84f, 0.46f, 0.50f }, 0.23f, 0.24f, 1.10f, 0.02f, 1.08f, 0.62f, {});
    hitEffects_.push_back(std::move(effect));
}

void GameRuntime::AddPlayerDamageEffect(const Math::Vector3& worldPosition)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 20;
    effect.strength = 0.92f;
    effect.type = HitEffectType::PlayerDamage;

    Model* burstModel =
        effectImpactBurstModel_ ? effectImpactBurstModel_ : effectGlowCoreModel_;
    Model* shardModel =
        effectMagicShardModel_ ? effectMagicShardModel_ : effectSparkStarModel_;
    AddHitEffectVisual(
        effect, burstModel, worldPosition,
        { 1.0f, 0.92f, 0.78f, 0.92f },
        0.48f, -0.16f, 0.0f, 0.0f, 1.38f, 0.62f, {});
    AddHitEffectVisual(
        effect, burstModel, worldPosition,
        { 1.0f, 0.24f, 0.12f, 0.74f },
        0.44f, 0.58f, -2.8f, 0.0f, 1.68f, 0.30f, {});
    AddHitEffectVisual(
        effect, burstModel, worldPosition,
        { 1.0f, 0.58f, 0.22f, 0.58f },
        0.34f, 0.48f, 2.4f, 0.0f, 0.30f, 1.58f, {});

    constexpr std::array<Math::Vector3, 4> kDamageShardVelocities{
        Math::Vector3{ -0.115f, 0.070f, 0.006f },
        Math::Vector3{ 0.120f, 0.052f, 0.006f },
        Math::Vector3{ -0.070f, -0.104f, 0.004f },
        Math::Vector3{ 0.082f, -0.096f, 0.004f }
    };
    for (size_t index = 0; index < kDamageShardVelocities.size(); ++index) {
        AddHitEffectVisual(
            effect,
            shardModel,
            worldPosition,
            index % 2 == 0 ?
                Math::Vector4{ 1.0f, 0.42f, 0.20f, 0.88f } :
                Math::Vector4{ 1.0f, 0.78f, 0.36f, 0.84f },
            0.20f,
            0.16f,
            index % 2 == 0 ? 2.2f : -2.2f,
            0.0f,
            0.28f,
            1.68f,
            kDamageShardVelocities[index]);
    }
    if (effect.visualCount > 0) {
        hitEffects_.push_back(std::move(effect));
    }
}

void GameRuntime::AddPlayerDodgeGrazeEffect(const Math::Vector3& worldPosition)
{
    HitEffect effect{};
    effect.worldPosition = worldPosition;
    effect.duration = 12;
    effect.strength = 0.58f;
    effect.type = HitEffectType::EnemyImpact;

    AddHitEffectVisual(effect, effectGlowCoreModel_, worldPosition,
        { 0.38f, 1.0f, 0.92f, 0.48f }, 0.38f, 0.12f, 0.0f, 0.0f, 1.34f, 0.42f, {});
    AddHitEffectVisual(effect, effectSparkStarModel_, worldPosition,
        { 0.84f, 1.0f, 0.96f, 0.62f }, 0.17f, 0.36f, 1.25f, 0.02f, 0.72f, 1.08f, { 0.018f, 0.012f, 0.0f });

    if (effect.visualCount > 0) {
        hitEffects_.push_back(std::move(effect));
    }
}

void GameRuntime::TriggerJustDodge(Bullet& bullet, const Math::Vector3& worldPosition)
{
    if (!player_ || player_->IsDead()) {
        return;
    }
    if (!justDodgedEnemyBullets_.insert(&bullet).second) {
        return;
    }

    ++justDodgeCount_;
    PlaySfx("dodge");
    AddScore(kJustDodgeScoreBonus);
    AddFeverGauge(22);
    if (defeatChainCount_ > 0) {
        defeatChainTimer_ = (std::max)(defeatChainTimer_, 90);
    }
    chargeTimer_ = (std::min)(chargeTimer_ + kJustDodgeChargeBonus, kChargeShotMax);
    chargeFlashTimer_ = (std::max)(chargeFlashTimer_, 26);
    justDodgeFlashTimer_ = (std::max)(justDodgeFlashTimer_, kJustDodgeFlashDuration);
    justDodgeSlowTimer_ = (std::max)(justDodgeSlowTimer_, kJustDodgeSlowDuration);

    const Math::Vector3 playerPosition = player_->GetTranslate();
    AddPlayerDodgeGrazeEffect(worldPosition);
    AddPlayerDodgeGrazeEffect({
        playerPosition.x,
        playerPosition.y + 0.12f,
        playerPosition.z + 0.10f
    });
    SpawnPlayerDodgeAfterimage();
    playerDodgeAfterimageTimer_ = 0;
    AddCameraShake(0.165f, 18);
}

void GameRuntime::TriggerPlayerImpactMoment(
    bool isCharged,
    bool isBossHit,
    bool isDestroyed)
{
    const int flashDuration =
        isBossHit ?
            (isDestroyed ? 28 : 15) :
            (isDestroyed ?
                (isCharged ? 13 : 7) :
                (isCharged ? kPlayerImpactFlashDuration : 4));
    const int slowDuration =
        isBossHit ?
            (isDestroyed ? 24 : 11) :
            (isDestroyed ?
                (isCharged ? 10 : 5) :
                (isCharged ? kPlayerImpactSlowDuration : 2));
    const float slowScale =
        isBossHit ?
            (isDestroyed ? 0.18f : 0.32f) :
            (isDestroyed ?
                (isCharged ? kPlayerImpactRailSlowScale : 0.62f) :
                (isCharged ? kPlayerImpactRailSlowScale : 0.78f));

    if (flashDuration >= playerImpactFlashTimer_) {
        playerImpactFlashDuration_ = flashDuration;
    }
    playerImpactFlashTimer_ =
        (std::max)(playerImpactFlashTimer_, flashDuration);

    if (slowDuration >= playerImpactSlowTimer_) {
        playerImpactSlowDuration_ = slowDuration;
    }
    playerImpactSlowTimer_ =
        (std::max)(playerImpactSlowTimer_, slowDuration);
    playerImpactSlowScale_ =
        playerImpactSlowTimer_ > 0 ?
        (std::min)(playerImpactSlowScale_, slowScale) :
        slowScale;

    AddCameraShake(
        isBossHit ? (isDestroyed ? 0.32f : 0.12f) :
            (isDestroyed ? (isCharged ? 0.145f : 0.095f) :
                (isCharged ? 0.052f : 0.018f)),
        isBossHit ? (isDestroyed ? 42 : 14) :
            (isDestroyed ? (isCharged ? 12 : 8) :
                (isCharged ? 7 : 3)));
}

void GameRuntime::TriggerHitConfirm(
    const Math::Vector3& worldPosition,
    bool isCharged,
    bool isBossHit,
    bool isDestroyed)
{
    Math::Vector2 screenPosition{};
    hitConfirmScreen_ = TryProjectToScreen(worldPosition, screenPosition) ?
        screenPosition : reticleScreen_;
    hitConfirmDuration_ = isDestroyed ? (isBossHit ? 30 : 24) :
        (isCharged || isBossHit ? 20 : 16);
    hitConfirmTimer_ = hitConfirmDuration_;
    hitConfirmStrength_ = isBossHit ? 1.35f :
        (isDestroyed ? 1.22f : (isCharged ? 1.14f : 1.0f));
    hitConfirmCharged_ = isCharged;
    hitConfirmBoss_ = isBossHit;
    hitConfirmDestroyed_ = isDestroyed;

    hitConfirmComboCount_ = hitConfirmComboTimer_ > 0 ?
        (std::min)(hitConfirmComboCount_ + 1, 99) : 1;
    hitConfirmComboTimer_ = 90;
}

void GameRuntime::TriggerPlayerDamageFeedback(
    const Math::Vector3& worldPosition,
    const Math::Vector3& incomingVelocity)
{
    Math::Vector2 screenPosition{};
    playerDamageScreen_ = TryProjectToScreen(worldPosition, screenPosition) ?
        screenPosition : reticleScreen_;

    Math::Vector2 sourceDirection{
        -incomingVelocity.x,
        incomingVelocity.y
    };
    const float directionLength = std::sqrt(
        sourceDirection.x * sourceDirection.x +
        sourceDirection.y * sourceDirection.y);
    if (directionLength > 0.001f) {
        sourceDirection.x /= directionLength;
        sourceDirection.y /= directionLength;
    } else {
        sourceDirection = { 0.0f, -1.0f };
    }
    playerDamageDirection_ = sourceDirection;
    playerDamageHudDuration_ = 26;
    playerDamageHudTimer_ = playerDamageHudDuration_;
}

void GameRuntime::AddHitEffectVisual(
    HitEffect& effect,
    Model* model,
    const Math::Vector3& worldPosition,
    const Math::Vector4& color,
    float baseSize,
    float growth,
    float spin,
    float popDelay,
    float aspectX,
    float aspectY,
    const Math::Vector3& velocity,
    bool additive,
    float lifeEnd,
    float roll,
    float drag)
{
    if (!object3dCommon_ || !model) {
        return;
    }
    if (effect.visualCount >= HitEffect::kMaxVisuals) {
        return;
    }

    HitEffect::Visual& visual = effect.visuals[effect.visualCount];
    visual = HitEffect::Visual{};
    visual.object = AcquireHitEffectObject();
    if (!visual.object) {
        return;
    }
    visual.object->SetModel(model);
    visual.object->SetTranslate(worldPosition);
    visual.object->SetScale({
        baseSize * aspectX * effect.strength,
        baseSize * aspectY * effect.strength,
        1.0f
    });
    visual.object->SetRotate(camera_ ? camera_->GetRotate() : Math::Vector3{});
    visual.object->SetColor(color);
    visual.object->SetLightingMode(0);
    visual.object->SetEnvironmentCoefficient(0.0f);
    visual.object->Update();
    visual.color = color;
    visual.baseSize = baseSize;
    visual.growth = growth;
    visual.spin = spin;
    visual.popDelay = popDelay;
    visual.lifeEnd = std::clamp(lifeEnd, popDelay + 0.001f, 1.0f);
    visual.roll = roll;
    visual.drag = (std::max)(drag, 0.0f);
    visual.aspectX = aspectX;
    visual.aspectY = aspectY;
    visual.velocity = velocity;
    visual.additive = additive;
    ++effect.visualCount;
}

void GameRuntime::UpdateHitEffects()
{
    float effectFrameStep = 1.0f;
    if (dxCommon_) {
        effectFrameStep =
            std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 4.0f);
    }

    for (auto iterator = hitEffects_.begin(); iterator != hitEffects_.end();) {
        iterator->age += effectFrameStep;
        // 短い閃光の枠は煙の終了を待たず返す。連続撃破時の枠不足を防ぐ。
        for (size_t index = 0; index < iterator->visualCount; ++index) {
            auto& visual = iterator->visuals[index];
            if (visual.object && iterator->age >= visual.lifeEnd * static_cast<float>(iterator->duration)) {
                hitEffectObjectPool_.push_back(std::move(visual.object));
            }
        }
        if (iterator->age >= static_cast<float>(iterator->duration)) {
            RecycleHitEffectVisuals(*iterator);
            iterator = hitEffects_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void GameRuntime::DrawHitEffects()
{
    return;
}

void GameRuntime::InitializePlayerDodgeAfterimages()
{
    playerDodgeAfterimageTimer_ = 0;
    nextPlayerDodgeAfterimageIndex_ = 0;
    wasPlayerDodging_ = false;
    Model* afterimageModel =
        effectPlayerBulletTrailModel_ ? effectPlayerBulletTrailModel_ : effectGlowCoreModel_;

    for (PlayerDodgeAfterimage& afterimage : playerDodgeAfterimages_) {
        afterimage = PlayerDodgeAfterimage{};
        afterimage.object = std::make_unique<Object3d>();
        afterimage.object->Initialize(object3dCommon_.get());
        if (afterimageModel) {
            afterimage.object->SetModel(afterimageModel);
        }
        afterimage.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        afterimage.object->SetScale({ 0.01f, 0.01f, 1.0f });
        afterimage.object->SetColor({ 0.40f, 0.96f, 1.0f, 0.0f });
        afterimage.object->SetLightingMode(0);
        afterimage.object->SetEnvironmentCoefficient(0.0f);
        afterimage.object->Update();
    }
}

void GameRuntime::InitializePlayerFlightAura()
{
    Model* softGlowModel = effectGlowCoreModel_;
    Model* exhaustTrailModel =
        effectPlayerBulletTrailModel_ ? effectPlayerBulletTrailModel_ : softGlowModel;
    Model* exhaustOuterModel =
        effectPlayerChargeTrailModel_ ? effectPlayerChargeTrailModel_ : exhaustTrailModel;
    Model* exhaustCoreModel =
        effectPlayerBulletCoreModel_ ? effectPlayerBulletCoreModel_ : softGlowModel;
    if (!object3dCommon_ || !softGlowModel) {
        return;
    }

    auto setupAura =
        [this](
            size_t index,
            Model* model,
            const Math::Vector3& offset,
            const Math::Vector4& color,
            float baseSize,
            float aspectX,
            float aspectY,
            float pulseOffset,
            float roll,
            float rollSpeed) {
            if (index >= playerFlightAuras_.size() || !model) {
                return;
            }

            PlayerFlightAura& aura = playerFlightAuras_[index];
            aura = PlayerFlightAura{};
            aura.object = std::make_unique<Object3d>();
            aura.object->Initialize(object3dCommon_.get());
            aura.object->SetModel(model);
            aura.object->SetLightingMode(0);
            aura.object->SetEnvironmentCoefficient(0.0f);
            aura.object->SetAlphaReference(0.01f);
            aura.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
            aura.object->SetScale({ 0.01f, 0.01f, 1.0f });
            aura.object->SetColor({ color.x, color.y, color.z, 0.0f });
            aura.object->Update();
            aura.model = model;
            aura.offset = offset;
            aura.color = color;
            aura.baseSize = baseSize;
            aura.aspectX = aspectX;
            aura.aspectY = aspectY;
            aura.pulseOffset = pulseOffset;
            aura.roll = roll;
            aura.rollSpeed = rollSpeed;
        };

    setupAura(
        0,
        exhaustOuterModel,
        { -0.72f, -0.11f, -1.35f },
        { 0.10f, 0.55f, 1.0f, 0.68f },
        0.58f,
        1.05f,
        1.12f,
        0.0f,
        0.0f,
        0.052f);
    setupAura(
        1,
        exhaustOuterModel,
        { 0.72f, -0.11f, -1.35f },
        { 0.10f, 0.55f, 1.0f, 0.68f },
        0.58f,
        1.05f,
        1.12f,
        1.8f,
        0.0f,
        0.061f);
    setupAura(
        2,
        exhaustTrailModel,
        { -0.72f, -0.10f, -1.39f },
        { 0.80f, 0.97f, 1.0f, 0.95f },
        0.40f,
        0.82f,
        1.02f,
        3.6f,
        0.0f,
        0.074f);
    setupAura(
        3,
        exhaustTrailModel,
        { 0.72f, -0.10f, -1.39f },
        { 0.80f, 0.97f, 1.0f, 0.95f },
        0.40f,
        0.82f,
        1.02f,
        5.4f,
        0.0f,
        0.079f);
    setupAura(
        4,
        exhaustCoreModel,
        { -0.72f, -0.11f, -1.32f },
        { 1.0f, 1.0f, 1.0f, 0.96f },
        0.36f,
        0.90f,
        0.90f,
        2.7f,
        0.0f,
        0.068f);
    setupAura(
        5,
        exhaustCoreModel,
        { 0.72f, -0.11f, -1.32f },
        { 1.0f, 1.0f, 1.0f, 0.96f },
        0.36f,
        0.90f,
        0.90f,
        4.5f,
        0.0f,
        0.083f);

}

void GameRuntime::InitializePlayerExhaustParticles()
{
    Model* particleModel =
        effectGlowCoreModel_ ? effectGlowCoreModel_ : effectPlayerBulletCoreModel_;
    if (!object3dCommon_ || !particleModel) {
        return;
    }

    for (PlayerExhaustParticle& particle : playerExhaustParticles_) {
        particle = PlayerExhaustParticle{};
        particle.object = std::make_unique<Object3d>();
        particle.object->Initialize(object3dCommon_.get());
        particle.object->SetModel(particleModel);
        particle.object->SetLightingMode(0);
        particle.object->SetEnvironmentCoefficient(0.0f);
        particle.object->SetAlphaReference(0.01f);
        particle.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
        particle.object->SetScale({ 0.01f, 0.01f, 1.0f });
        particle.object->SetColor({ 0.35f, 0.82f, 1.0f, 0.0f });
        particle.object->Update();
        particle.model = particleModel;
    }

    nextPlayerExhaustParticleIndex_ = 0;
}

void GameRuntime::InitializeGpuPlayerExhaustParticles()
{
    gpuPlayerExhaustEnabled_ = false;
    ParticleManager* particleManager = ParticleManager::GetInstance();
    if (!particleManager) {
        return;
    }

    ParticleManager::EmitSettings outerSettings{};
    outerSettings.radius = 0.045f;
    outerSettings.direction = { 0.0f, -0.06f, -1.0f };
    outerSettings.spread = 0.13f;
    outerSettings.colorMin = { 0.04f, 0.34f, 1.0f, 0.20f };
    outerSettings.colorMax = { 0.24f, 0.78f, 1.0f, 0.55f };
    outerSettings.scaleMin = { 0.08f, 0.08f };
    outerSettings.scaleMax = { 0.19f, 0.19f };
    outerSettings.lifeTimeMin = 0.16f;
    outerSettings.lifeTimeMax = 0.30f;
    outerSettings.speedMin = 4.4f;
    outerSettings.speedMax = 6.8f;
    outerSettings.endScale = 0.30f;

    ParticleManager::EmitSettings coreSettings = outerSettings;
    coreSettings.radius = 0.025f;
    coreSettings.spread = 0.07f;
    coreSettings.colorMin = { 0.68f, 0.92f, 1.0f, 0.46f };
    coreSettings.colorMax = { 1.0f, 1.0f, 1.0f, 0.86f };
    coreSettings.scaleMin = { 0.045f, 0.045f };
    coreSettings.scaleMax = { 0.11f, 0.11f };
    coreSettings.lifeTimeMin = 0.10f;
    coreSettings.lifeTimeMax = 0.18f;
    coreSettings.speedMin = 6.0f;
    coreSettings.speedMax = 8.6f;
    coreSettings.endScale = 0.18f;

    constexpr const char* kTexturePath = "resources/effects/glow_core.png";
    const bool allGroupsCreated =
        particleManager->CreateParticleGroup(
            kPlayerExhaustGpuLeftOuterGroup, kTexturePath, outerSettings) &&
        particleManager->CreateParticleGroup(
            kPlayerExhaustGpuRightOuterGroup, kTexturePath, outerSettings) &&
        particleManager->CreateParticleGroup(
            kPlayerExhaustGpuLeftCoreGroup, kTexturePath, coreSettings) &&
        particleManager->CreateParticleGroup(
            kPlayerExhaustGpuRightCoreGroup, kTexturePath, coreSettings);
    if (!allGroupsCreated) {
        particleManager->RemoveParticleGroup(kPlayerExhaustGpuLeftOuterGroup);
        particleManager->RemoveParticleGroup(kPlayerExhaustGpuRightOuterGroup);
        particleManager->RemoveParticleGroup(kPlayerExhaustGpuLeftCoreGroup);
        particleManager->RemoveParticleGroup(kPlayerExhaustGpuRightCoreGroup);
        return;
    }

    gpuPlayerExhaustEnabled_ = true;
}

void GameRuntime::EmitPlayerExhaustParticles(
    const Math::Vector3& playerPosition,
    const Math::Vector3& playerRotate)
{
    if (!dxCommon_) {
        return;
    }

    playerExhaustParticleTimer_ += dxCommon_->GetDeltaTime();
    if (playerExhaustParticleTimer_ < kPlayerExhaustParticleInterval) {
        return;
    }
    playerExhaustParticleTimer_ = std::fmod(
        playerExhaustParticleTimer_,
        kPlayerExhaustParticleInterval);

    const Math::Vector3 leftOffset =
        RotateLocalOffset({ -kPlayerExhaustNozzle.x, kPlayerExhaustNozzle.y, kPlayerExhaustNozzle.z }, playerRotate);
    const Math::Vector3 rightOffset =
        RotateLocalOffset(kPlayerExhaustNozzle, playerRotate);
    const Math::Vector3 exhaustDirection = Math::Normalize(
        RotateLocalOffset({ 0.0f, -0.06f, -1.0f }, playerRotate));
    const Math::Vector3 leftPosition{
        playerPosition.x + leftOffset.x,
        playerPosition.y + leftOffset.y,
        playerPosition.z + leftOffset.z
    };
    const Math::Vector3 rightPosition{
        playerPosition.x + rightOffset.x,
        playerPosition.y + rightOffset.y,
        playerPosition.z + rightOffset.z
    };
    if (gpuPlayerExhaustEnabled_) {
        ParticleManager* particleManager = ParticleManager::GetInstance();
        particleManager->Emit(
            kPlayerExhaustGpuLeftOuterGroup, leftPosition, exhaustDirection, 2);
        particleManager->Emit(
            kPlayerExhaustGpuRightOuterGroup, rightPosition, exhaustDirection, 2);
        particleManager->Emit(
            kPlayerExhaustGpuLeftCoreGroup, leftPosition, exhaustDirection, 1);
        particleManager->Emit(
            kPlayerExhaustGpuRightCoreGroup, rightPosition, exhaustDirection, 1);
        return;
    }
    const auto spawnParticle =
        [this, &playerRotate, &exhaustDirection](
            const Math::Vector3& nozzlePosition,
            bool isCore,
            float phase) {
            PlayerExhaustParticle& particle =
                playerExhaustParticles_[nextPlayerExhaustParticleIndex_];
            nextPlayerExhaustParticleIndex_ =
                (nextPlayerExhaustParticleIndex_ + 1) % playerExhaustParticles_.size();

            if (!particle.object) {
                return;
            }

            Model* model = isCore ?
                (effectPlayerBulletCoreModel_ ?
                    effectPlayerBulletCoreModel_ : effectGlowCoreModel_) :
                (effectGlowCoreModel_ ?
                    effectGlowCoreModel_ : effectPlayerBulletCoreModel_);
            if (!model) {
                return;
            }

            const float wobbleX = std::sin(phase * 1.73f) * 0.10f;
            const float wobbleY = std::cos(phase * 2.11f) * 0.07f;
            const Math::Vector3 positionJitter = RotateLocalOffset(
                { wobbleX, wobbleY, -0.03f },
                playerRotate);
            const Math::Vector3 velocityJitter = RotateLocalOffset(
                { wobbleX * 2.6f, wobbleY * 2.0f, 0.0f },
                playerRotate);
            const float feverRate =
                std::clamp(feverSpeedEffectRate_, 0.0f, 1.0f);
            const float speed =
                (isCore ? 6.6f : 4.8f) + playerExhaustThrust_ * 2.1f +
                feverRate * (isCore ? 3.4f : 2.8f);

            particle.model = model;
            particle.position = {
                nozzlePosition.x + positionJitter.x,
                nozzlePosition.y + positionJitter.y,
                nozzlePosition.z + positionJitter.z
            };
            particle.velocity = {
                exhaustDirection.x * speed + velocityJitter.x,
                exhaustDirection.y * speed + velocityJitter.y,
                exhaustDirection.z * speed + velocityJitter.z
            };
            particle.color = isCore ?
                Math::Vector4{
                    Lerp(0.78f, 1.0f, feverRate),
                    Lerp(0.97f, 1.0f, feverRate),
                    Lerp(1.0f, 0.92f, feverRate),
                    Lerp(0.92f, 1.0f, feverRate) } :
                Math::Vector4{
                    Lerp(0.06f, 0.34f, feverRate),
                    Lerp(0.48f, 0.82f, feverRate),
                    1.0f,
                    Lerp(0.66f, 0.86f, feverRate) };
            particle.age = 0.0f;
            particle.lifetime =
                (isCore ? 0.15f : 0.28f) +
                feverRate * (isCore ? 0.06f : 0.09f);
            particle.startSize =
                (isCore ? 0.16f : 0.27f) *
                (0.92f + playerExhaustThrust_ * 0.22f) *
                (1.0f + feverRate * (isCore ? 0.38f : 0.52f));
            particle.endSize =
                (isCore ? 0.025f : 0.075f) * (1.0f + feverRate * 0.40f);
            particle.aspectX = isCore ? 0.74f : 1.08f;
            particle.aspectY = isCore ? 1.24f : 1.16f;
            particle.roll = phase * 0.37f;
            particle.rollSpeed = (isCore ? 2.8f : 1.6f) *
                (std::sin(phase) >= 0.0f ? 1.0f : -1.0f);
            particle.isActive = true;
        };

    const float phase = cameraTimer_ * 0.173f +
        static_cast<float>(nextPlayerExhaustParticleIndex_) * 0.61f;
    spawnParticle(leftPosition, false, phase);
    spawnParticle(rightPosition, false, phase + 1.7f);
    spawnParticle(leftPosition, true, phase + 3.1f);
    spawnParticle(rightPosition, true, phase + 4.8f);
}

void GameRuntime::UpdateAndDrawPlayerExhaustParticles()
{
    if (!camera_ || !dxCommon_) {
        return;
    }

    const float deltaTime = std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 1.0f / 20.0f);
    const Math::Vector3 cameraRotate = camera_->GetRotate();
    for (PlayerExhaustParticle& particle : playerExhaustParticles_) {
        if (!particle.isActive || !particle.object || !particle.model) {
            continue;
        }

        particle.age += deltaTime;
        if (particle.age >= particle.lifetime) {
            particle.isActive = false;
            particle.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
            particle.object->SetScale({ 0.01f, 0.01f, 1.0f });
            particle.object->SetColor({ particle.color.x, particle.color.y, particle.color.z, 0.0f });
            particle.object->Update();
            continue;
        }

        const float lifeRate = std::clamp(
            particle.age / (std::max)(particle.lifetime, 0.001f),
            0.0f,
            1.0f);
        particle.position.x += particle.velocity.x * deltaTime;
        particle.position.y += particle.velocity.y * deltaTime;
        particle.position.z += particle.velocity.z * deltaTime;
        const float damping = std::pow(0.94f, deltaTime * 60.0f);
        particle.velocity.x *= damping;
        particle.velocity.y *= damping;
        particle.velocity.z *= damping;
        particle.roll += particle.rollSpeed * deltaTime;

        const float fade = (1.0f - lifeRate) * (1.0f - lifeRate);
        const float size =
            particle.startSize + (particle.endSize - particle.startSize) * lifeRate;
        const float pulse = 1.0f +
            std::sin(cameraTimer_ * 0.29f + particle.roll * 2.0f) * 0.06f;
        Math::Vector4 color = particle.color;
        color.w *= fade;
        Math::Vector3 rotate = cameraRotate;
        rotate.z += particle.roll;

        particle.object->SetModel(particle.model);
        particle.object->SetTranslate(particle.position);
        particle.object->SetRotate(rotate);
        particle.object->SetScale({
            size * particle.aspectX * pulse,
            size * particle.aspectY * (2.0f - pulse),
            1.0f
        });
        particle.object->SetColor(color);
        particle.object->Update();
        particle.object->Draw();
    }
}

void GameRuntime::SpawnPlayerDodgeAfterimage()
{
    Model* afterimageModel =
        effectPlayerBulletTrailModel_ ? effectPlayerBulletTrailModel_ : effectGlowCoreModel_;
    if (!player_ || player_->IsDead() || !afterimageModel) {
        return;
    }

    PlayerDodgeAfterimage& afterimage =
        playerDodgeAfterimages_[nextPlayerDodgeAfterimageIndex_];
    if (!afterimage.object) {
        if (!object3dCommon_) {
            return;
        }
        afterimage.object = std::make_unique<Object3d>();
        afterimage.object->Initialize(object3dCommon_.get());
    }

    afterimage.object->SetModel(afterimageModel);
    afterimage.position = player_->GetTranslate();
    afterimage.position.y += 0.06f;
    afterimage.position.z += 0.06f;
    afterimage.direction = player_->GetDodgeDirection() >= 0 ? 1 : -1;
    afterimage.age = 0.0f;
    afterimage.duration = kPlayerDodgeAfterimageDuration;
    afterimage.isActive = true;

    nextPlayerDodgeAfterimageIndex_ =
        (nextPlayerDodgeAfterimageIndex_ + 1) % playerDodgeAfterimages_.size();
}

void GameRuntime::UpdatePlayerDodgeAfterimages()
{
    float effectFrameStep = 1.0f;
    if (dxCommon_) {
        effectFrameStep =
            std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 4.0f);
    }

    const bool isDodging =
        player_ && !player_->IsDead() && player_->IsDodging();
    const bool isJustDodgeCinematic =
        player_ && !player_->IsDead() && justDodgeSlowTimer_ > 0;
    if (playerDodgeAfterimageTimer_ > 0) {
        --playerDodgeAfterimageTimer_;
    }
    if (isDodging && !wasPlayerDodging_ && player_) {
        const Math::Vector3 playerPosition = player_->GetTranslate();
        const float direction = static_cast<float>(player_->GetDodgeDirection() >= 0 ? 1 : -1);
        AddPlayerDodgeGrazeEffect({
            playerPosition.x - direction * 0.58f,
            playerPosition.y + 0.10f,
            playerPosition.z + 0.16f
        });
        AddCameraShake(0.060f, 10);
    }
    if ((isDodging && (!wasPlayerDodging_ || playerDodgeAfterimageTimer_ <= 0)) ||
        (isJustDodgeCinematic && playerDodgeAfterimageTimer_ <= 0)) {
        SpawnPlayerDodgeAfterimage();
        playerDodgeAfterimageTimer_ =
            isJustDodgeCinematic ? 1 : kPlayerDodgeAfterimageIntervalFrames;
    }
    wasPlayerDodging_ = isDodging;

    for (PlayerDodgeAfterimage& afterimage : playerDodgeAfterimages_) {
        if (!afterimage.isActive) {
            continue;
        }

        afterimage.age += effectFrameStep;
        if (afterimage.age >= afterimage.duration) {
            afterimage.isActive = false;
            if (afterimage.object) {
                afterimage.object->SetTranslate({ 0.0f, -1000.0f, -1000.0f });
                afterimage.object->SetScale({ 0.01f, 0.01f, 1.0f });
                afterimage.object->SetColor({ 0.45f, 1.0f, 0.92f, 0.0f });
                afterimage.object->Update();
            }
        }
    }
}

void GameRuntime::DrawPlayerFlightAura()
{
    if (!object3dCommon_ || !camera_ || !player_ || player_->IsDead()) {
        return;
    }

    bool hasAuraObject = false;
    for (const PlayerFlightAura& aura : playerFlightAuras_) {
        if (aura.object && aura.model) {
            hasAuraObject = true;
            break;
        }
    }
    if (!hasAuraObject) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();

    object3dCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->CommonDrawSetting();

    const Math::Vector3 playerPosition = player_->GetTranslate();
    const Math::Vector3 playerRotate = player_->GetVisualRotate();
    const Math::Vector3 cameraRotate = camera_->GetRotate();
    const float speedRate = GetFlightSpeedRate();
    const float accelerationRate = std::clamp(
        (targetRailSpeed_ - railSpeed_) / 0.045f,
        0.0f,
        1.0f);
    const float feverThrustBoost = feverSpeedEffectRate_;
    const float dodgeBoost = player_->IsDodging() ? 1.0f : 0.0f;
    const float justDodgeBoost =
        justDodgeFlashTimer_ > 0 ?
        static_cast<float>(justDodgeFlashTimer_) /
            static_cast<float>((std::max)(kJustDodgeFlashDuration, 1)) :
        0.0f;
    const float targetThrust = std::clamp(
        0.10f +
            speedRate * 0.42f +
            accelerationRate * 0.20f +
            dodgeBoost * 0.82f +
            justDodgeBoost * 0.34f +
            flightReleaseKick_ * 0.35f +
            feverThrustBoost * 0.95f,
        0.0f,
        1.75f);
    float effectFrameStep = 1.0f;
    if (dxCommon_) {
        effectFrameStep = std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.25f, 3.0f);
    }
    const float thrustResponse =
        targetThrust > playerExhaustThrust_ ?
        kPlayerExhaustRiseResponse :
        kPlayerExhaustFallResponse;
    const float thrustBlend =
        1.0f - std::pow(1.0f - thrustResponse, effectFrameStep);
    playerExhaustThrust_ +=
        (targetThrust - playerExhaustThrust_) * thrustBlend;
    EmitPlayerExhaustParticles(playerPosition, playerRotate);
    const float exhaustAxisWobble =
        std::sin(cameraTimer_ * 0.071f) * 0.012f;

    for (size_t auraIndex = 0; auraIndex < playerFlightAuras_.size(); ++auraIndex) {
        PlayerFlightAura& aura = playerFlightAuras_[auraIndex];
        if (!aura.object || !aura.model) {
            continue;
        }

        const float pulse =
            1.0f + std::sin(cameraTimer_ * 0.074f + aura.pulseOffset) * 0.055f;
        const float flicker = std::clamp(
            0.90f +
                std::sin(cameraTimer_ * 0.137f + aura.pulseOffset) * 0.075f +
                std::sin(cameraTimer_ * 0.311f + aura.pulseOffset * 1.7f) * 0.035f,
            0.78f,
            1.08f);
        const bool isOuterFlame = auraIndex < 2;
        const bool isInnerFlame = auraIndex >= 2 && auraIndex < 4;
        const float lengthResponse =
            isOuterFlame ? 0.92f :
            isInnerFlame ? 0.66f :
            0.22f;
        const float widthResponse =
            isOuterFlame ? 0.18f :
            isInnerFlame ? 0.12f :
            0.20f;
        const float alphaResponse =
            isOuterFlame ? 0.26f :
            isInnerFlame ? 0.20f :
            0.14f;
        Math::Vector3 localOffset = aura.offset;
        // 既存6層の相対位置を保ち、Omenのノズルへ移す。
        localOffset.x += (auraIndex % 2 == 0 ? -1.0f : 1.0f) * (kPlayerExhaustNozzle.x - 0.72f);
        localOffset.y += kPlayerExhaustNozzle.y + 0.10f;
        localOffset.z += kPlayerExhaustNozzle.z + 1.38f;
        localOffset.z -=
            playerExhaustThrust_ * aura.baseSize * lengthResponse * 0.05f;
        const Math::Vector3 rotatedOffset = RotateLocalOffset(localOffset, playerRotate);
        const Math::Vector3 translate{
            playerPosition.x + rotatedOffset.x,
            playerPosition.y + rotatedOffset.y,
            playerPosition.z + rotatedOffset.z
        };
        Math::Vector3 rotate = cameraRotate;
        rotate.z += playerRotate.z + aura.roll + exhaustAxisWobble;
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
        color.w *= flicker *
            (0.76f + speedRate * 0.10f + playerExhaustThrust_ * alphaResponse) *
            (1.0f + feverThrustBoost * 0.20f);
        color.w = std::clamp(color.w, 0.0f, 1.0f);

        const float feverWidthScale =
            1.0f + feverThrustBoost *
                (isOuterFlame ? 0.24f : isInnerFlame ? 0.18f : 0.12f);
        const float feverLengthScale =
            1.0f + feverThrustBoost *
                (isOuterFlame ? 0.56f : isInnerFlame ? 0.42f : 0.18f);

        aura.object->SetModel(aura.model);
        aura.object->SetTranslate(translate);
        aura.object->SetRotate(rotate);
        aura.object->SetScale({
            aura.baseSize * aura.aspectX * pulse *
                (1.0f + playerExhaustThrust_ * widthResponse) * feverWidthScale,
            aura.baseSize * aura.aspectY * (2.0f - pulse) *
                (1.0f + playerExhaustThrust_ * lengthResponse) * feverLengthScale,
            1.0f
        });
        aura.object->SetColor(color);
        aura.object->Update();
        aura.object->Draw();
    }

    UpdateAndDrawPlayerExhaustParticles();

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::DrawPlayerDodgeAfterimages()
{
    Model* afterimageModel =
        effectPlayerBulletTrailModel_ ? effectPlayerBulletTrailModel_ : effectGlowCoreModel_;
    if (!object3dCommon_ || !camera_ || !afterimageModel) {
        return;
    }

    bool hasActiveAfterimage = false;
    for (const PlayerDodgeAfterimage& afterimage : playerDodgeAfterimages_) {
        if (afterimage.isActive && afterimage.object) {
            hasActiveAfterimage = true;
            break;
        }
    }
    if (!hasActiveAfterimage) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();

    object3dCommon_->SetDepthDrawMode(DepthDrawMode::Overlay);
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->CommonDrawSetting();

    for (PlayerDodgeAfterimage& afterimage : playerDodgeAfterimages_) {
        if (!afterimage.isActive || !afterimage.object) {
            continue;
        }

        const float rate = std::clamp(
            afterimage.age / (std::max)(afterimage.duration, 1.0f),
            0.0f,
            1.0f);
        const float fade = 1.0f - rate;
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

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::InitializeRifts()
{
    ResetRifts();
    Model* rimModel = ModelManager::GetInstance()->FindModel("rift_rim");
    if (IsTutorial() || !object3dCommon_ || !rimModel || !effectGlowRingModel_) { return; }
    for (auto& rift : rifts_) {
        rift.rim = std::make_unique<Object3d>();
        rift.glow = std::make_unique<Object3d>();
        rift.rim->Initialize(object3dCommon_.get());
        rift.glow->Initialize(object3dCommon_.get());
        rift.rim->SetModel(rimModel);
        rift.glow->SetModel(effectGlowRingModel_);
        for (Object3d* object : { rift.rim.get(), rift.glow.get() }) {
            object->SetLightingMode(0);
            object->SetEnvironmentCoefficient(0.0f);
            object->SetShadowReceiveStrength(0.0f);
        }
    }
}

void GameRuntime::ResetRifts()
{
    for (auto& rift : rifts_) { rift.active = false; rift.age = 0.0f; }
    riftSpawnCount_ = 0;
    riftPassCount_ = 0;
    riftNoticeTimer_ = 0;
    riftRecoveredFrames_ = 0;
}

void GameRuntime::SpawnRift(const Math::Vector3& position)
{
    if (IsTutorial() || isGameOver_ || isGameClear_ || !player_ ||
        position.z <= player_->GetTranslate().z + 1.0f) { return; }
    for (auto& rift : rifts_) {
        if (rift.active || !rift.rim || !rift.glow) { continue; }
        // 奥行きも撃破位置のまま残す。前方の敵へ早く当てるほど操縦の猶予が増える。
        rift.position = position;
        rift.age = 0.0f;
        rift.lifetime = std::clamp(
            (position.z - railDistance_) / (std::max)(railSpeed_, 0.15f) + 120.0f, 120.0f, 480.0f);
        rift.active = true;
        ++riftSpawnCount_;
        return;
    }
}

void GameRuntime::UpdateRifts(const Math::Vector3& previous, const Math::Vector3& current)
{
    if (isPaused_ || IsTutorial()) { return; }
    if (isGameOver_ || isGameClear_ || !player_ || player_->IsDead()) {
        for (auto& rift : rifts_) { rift.active = false; }
        riftNoticeTimer_ = 0;
        return;
    }
    if (riftNoticeTimer_ > 0) { --riftNoticeTimer_; }
    const float advance = current.z - previous.z;
    for (auto& rift : rifts_) {
        if (!rift.active) { continue; }
        rift.age += GetCinematicWorldTimeScale();
        // 高速飛行・横回避でも、通過した瞬間のXYを補間して判定する。
        if (advance > 0.0001f && previous.z < rift.position.z && current.z >= rift.position.z) {
            const float rate = (rift.position.z - previous.z) / advance;
            const float x = (Lerp(previous.x, current.x, rate) - rift.position.x) / kRiftRadiusX;
            const float y = (Lerp(previous.y, current.y, rate) - rift.position.y) / kRiftRadiusY;
            rift.active = false; // 命中・報酬とも一度だけ。外してもペナルティは付けない。
            if (x * x + y * y <= 1.0f) {
                ++riftPassCount_;
                riftRecoveredFrames_ = 0;
                if (feverTimer_ > 0) {
                    const int before = feverTimer_;
                    feverTimer_ = (std::min)(feverTimer_ + kRiftFeverRecoveryFrames, kFeverDurationFrames);
                    riftRecoveredFrames_ = feverTimer_ - before;
                } else {
                    AddFeverGauge(kRiftGaugeReward);
                }
                chargeTimer_ = kChargeShotMax;
                shootCooldown_ = (std::min)(shootCooldown_, 4); // 通過直後の攻撃へつなげる。
                if (defeatChainCount_ > 0) { defeatChainTimer_ = kDefeatChainDurationFrames; }
                riftNoticeTimer_ = 54;
                AddRewardHeartCollectEffect(current);
                AddCameraShake(0.055f, 8);
            }
        } else if (current.z > rift.position.z + 2.0f || rift.age >= rift.lifetime) {
            rift.active = false;
        }
    }
}

void GameRuntime::DrawRiftObjects()
{
    if (!object3dCommon_ || !player_ ||
        std::none_of(rifts_.begin(), rifts_.end(), [](const RiftGate& rift) { return rift.active; })) { return; }
    const BlendMode previousBlend = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepth = object3dCommon_->GetDepthDrawMode();
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    object3dCommon_->CommonDrawSetting();
    for (auto& rift : rifts_) {
        if (!rift.active || !rift.rim || !rift.glow) { continue; }
        const float lead = rift.position.z - player_->GetTranslate().z;
        // 最後の1mだけ薄くして、カメラ手前に巨大な輪が残らないようにする。
        const float fade = std::clamp(lead, 0.0f, 1.0f);
        const float birth = 0.65f + 0.35f * std::clamp(rift.age / 12.0f, 0.0f, 1.0f);
        const float pulse = 1.0f + 0.025f * std::sin(rift.age * 0.09f);
        const float scale = birth * pulse;
        rift.rim->SetTranslate(rift.position);
        rift.rim->SetRotate({ 0.0f, 0.0f, 0.0f });
        rift.rim->SetScale({ 1.80f * scale, 1.46f * scale, 1.0f });
        rift.rim->SetColor({ 1.8f, 0.94f, 0.24f, 0.90f * fade });
        rift.rim->Update();
        rift.rim->Draw();
        rift.glow->SetTranslate(rift.position);
        rift.glow->SetRotate({ 0.0f, 0.0f, 0.0f });
        rift.glow->SetScale({ 2.15f * scale, 1.74f * scale, 1.0f });
        rift.glow->SetColor({ 1.6f, 0.62f, 0.13f, 0.46f * fade });
        rift.glow->Update();
        rift.glow->Draw();
    }
    object3dCommon_->SetBlendMode(previousBlend);
    object3dCommon_->SetDepthDrawMode(previousDepth);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::DrawRiftHud()
{
    if (IsTutorial() || isGameOver_ || isGameClear_ || !player_) { return; }
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImU32 gold = CombatHud::GaugeGold;
    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead() || !enemy->IsTargetable() || !enemy->IsRiftCarrier()) { continue; }
        auto position = enemy->GetAimPosition();
        position.y += enemy->GetAimRadius() + 0.55f;
        Math::Vector2 screen{};
        if (!TryProjectToScreen(position, screen)) { continue; }
        // 金色の小さな二重矢印で、裂け目を残す敵を射撃前から識別できる。
        for (float offset : { -5.0f, 0.0f }) {
            drawList->AddLine({ screen.x - 5.0f, screen.y + offset },
                { screen.x, screen.y + offset + 4.0f }, gold, 2.0f);
            drawList->AddLine({ screen.x, screen.y + offset + 4.0f },
                { screen.x + 5.0f, screen.y + offset }, gold, 2.0f);
        }
    }
    const RiftGate* nearest = nullptr;
    for (const auto& rift : rifts_) {
        if (rift.active && (!nearest || rift.position.z < nearest->position.z)) { nearest = &rift; }
    }
    if (nearest && nearest->position.z > railDistance_ + 6.0f) {
        auto position = nearest->position;
        position.y -= 1.75f;
        Math::Vector2 screen{};
        if (TryProjectToScreen(position, screen)) {
            const char* text = feverTimer_ > 0 ? "通過でフィーバー延長" : "通過でチャージ";
            const float width = CombatHud::ReadoutWidth(text, 14.0f);
            CombatHud::Readout(drawList, { screen.x - width * 0.5f, screen.y }, 14.0f, gold, text);
        }
    }
    if (riftNoticeTimer_ > 0) {
        Math::Vector2 min{}, size{};
        GetEffectiveHudViewportRect(min, size);
        char text[64]{};
        if (riftRecoveredFrames_ > 0) {
            std::snprintf(text, sizeof(text), "突破  +%.1f秒", static_cast<float>(riftRecoveredFrames_) / 60.0f);
        } else {
            std::snprintf(text, sizeof(text), "突破  チャージ獲得");
        }
        const int alpha = (std::min)(255, riftNoticeTimer_ * 18);
        const float width = CombatHud::ReadoutWidth(text, 23.0f);
        CombatHud::Readout(drawList, { min.x + size.x * 0.5f - width * 0.5f, min.y + size.y * 0.78f },
            23.0f, IM_COL32(255, 214, 126, alpha), text);
    }
}

void GameRuntime::DrawBulletEffectObjects()
{
    if (!object3dCommon_ || !camera_) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();

    object3dCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->CommonDrawSetting();

    const Math::Matrix4x4& cameraWorld = camera_->GetWorldMatrix();
    for (const auto& bullet : playerBullets_) {
        if (bullet) {
            bullet->DrawGlow(cameraWorld);
        }
    }
    for (const auto& bullet : enemyBullets_) {
        if (bullet) {
            bullet->DrawGlow(cameraWorld);
        }
    }

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

void GameRuntime::DrawHitEffectObjects()
{
    if (hitEffects_.empty() || !object3dCommon_) {
        return;
    }

    const BlendMode previousBlendMode = object3dCommon_->GetBlendMode();
    const DepthDrawMode previousDepthMode = object3dCommon_->GetDepthDrawMode();

    object3dCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);

    auto drawVisualPass = [&](bool additivePass) {
        object3dCommon_->SetBlendMode(additivePass ? BlendMode::Add : BlendMode::Normal);
        object3dCommon_->CommonDrawSetting();

        for (const HitEffect& effect : hitEffects_) {
            if (effect.visualCount == 0) {
                continue;
            }

            const float rate =
                effect.age /
                static_cast<float>((std::max)(effect.duration, 1));
            for (size_t index = 0; index < effect.visualCount; ++index) {
                const HitEffect::Visual& visual = effect.visuals[index];
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
                    effect.type == HitEffectType::RewardCollect ? fade :
                    effect.type == HitEffectType::PlayerDamage ? fade * fade * fade :
                    effect.type == HitEffectType::EnemyImpact ? fade :
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

    object3dCommon_->SetBlendMode(previousBlendMode);
    object3dCommon_->SetDepthDrawMode(previousDepthMode);
    object3dCommon_->CommonDrawSetting();
}

#ifdef ENABLE_DEBUG_GUI
void GameRuntime::DrawEditorOverlayGuiRich()
{
    if (!isEditorOverlayVisible_) {
        hasEditorOverlayViewportRect_ = false;
        return;
    }

    hasEditorOverlayViewportRect_ = false;

    const char* kWindowHierarchy = "ヒエラルキー";
    const char* kWindowInspector = "インスペクター";
    const char* kWindowGameView = "ゲームビュー";
    const char* kWindowProject = "プロジェクト";
    const char* kWindowConsole = "コンソール";
    const char* kWindowStats = "統計";
    const char* kWindowTuning = "ゲーム調整";
    const char* kWindowNodeGraph = "ノードグラフ";
    const char* kWindowTextView = "テキスト表示";

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiID dockspaceId = ImGui::GetID("GameOverlayDockSpace");
    const ImGuiDockNodeFlags dockspaceFlags =
        ImGuiDockNodeFlags_NoWindowMenuButton |
        ImGuiDockNodeFlags_NoCloseButton;
    bool requestDockLayoutReset = false;
    static bool showAdvancedDebugPanels = false;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB")) {
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " \xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x81\xBF", "Ctrl+O")) {
                IGFD::FileDialogConfig config{};
                config.path = "resources";
                ImGuiFileDialog::Instance()->OpenDialog(
                    "GameSceneOpenDialog",
                    "シーンを開く",
                    ".json",
                    config);
            }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " \xE4\xBF\x9D\xE5\xAD\x98", "Ctrl+S")) {
                SaveSceneObjects(currentSceneFilePath_.c_str());
            }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " 名前を付けて保存")) {
                IGFD::FileDialogConfig config{};
                config.path = "resources";
                config.fileName = "game_scene.json";
                ImGuiFileDialog::Instance()->OpenDialog(
                    "GameSceneSaveDialog",
                    "名前を付けてシーンを保存",
                    ".json",
                    config);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB\xE3\x81\xB8\xE6\x88\xBB\xE3\x82\x8B", "F2")) {
                isExitRequested_ = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE7\xB7\xA8\xE9\x9B\x86")) {
            ImGui::MenuItem(ICON_FA_ROTATE_LEFT " \xE5\x85\x83\xE3\x81\xAB\xE6\x88\xBB\xE3\x81\x99", "Ctrl+Z", false, false);
            ImGui::MenuItem(ICON_FA_ROTATE_RIGHT " \xE3\x82\x84\xE3\x82\x8A\xE7\x9B\xB4\xE3\x81\x97", "Ctrl+Y", false, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x82\xAA\xE3\x83\x96\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88")) {
            ImGui::MenuItem(ICON_FA_USER " プレイヤー", nullptr, false, false);
            ImGui::MenuItem(ICON_FA_BULLSEYE " 敵", nullptr, false, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE3\x82\xA6\xE3\x82\xA3\xE3\x83\xB3\xE3\x83\x89\xE3\x82\xA6")) {
            if (ImGui::MenuItem(ICON_FA_TABLE_COLUMNS " \xE5\x88\x9D\xE6\x9C\x9F\xE3\x83\xAC\xE3\x82\xA4\xE3\x82\xA2\xE3\x82\xA6\xE3\x83\x88\xE3\x81\xAB\xE6\x88\xBB\xE3\x81\x99")) {
                requestDockLayoutReset = true;
            }
            ImGui::MenuItem(
                ICON_FA_CODE_BRANCH " 詳細デバッグパネル",
                nullptr,
                &showAdvancedDebugPanels);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE5\x86\x8D\xE7\x94\x9F")) {
            ImGui::MenuItem(ICON_FA_PLAY " \xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE8\xA1\xA8\xE7\xA4\xBA\xE4\xB8\xAD", nullptr, true, false);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::TextDisabled("F1 GUI表示切り替え / F2 タイトルへ戻る");
        ImGui::EndMainMenuBar();
    }

    ImGui::DockSpaceOverViewport(dockspaceId, viewport, dockspaceFlags);

    static bool hasBuiltDefaultDockLayout = false;
    if (!hasBuiltDefaultDockLayout || requestDockLayoutReset) {
        hasBuiltDefaultDockLayout = true;
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, dockspaceFlags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        ImGuiID mainId = dockspaceId;
        ImGuiID leftId = 0;
        ImGuiID rightId = 0;
        ImGuiID rightBottomId = 0;
        ImGuiID bottomId = 0;
        ImGuiID centerId = 0;
        leftId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.22f, nullptr, &mainId);
        rightId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.27f, nullptr, &mainId);
        bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.24f, nullptr, &centerId);
        rightBottomId = ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.48f, nullptr, &rightId);

        ImGui::DockBuilderDockWindow(kWindowHierarchy, leftId);
        ImGui::DockBuilderDockWindow(kWindowInspector, rightId);
        ImGui::DockBuilderDockWindow(kWindowProject, bottomId);
        ImGui::DockBuilderDockWindow(kWindowConsole, bottomId);
        ImGui::DockBuilderDockWindow(kWindowTuning, rightBottomId);
        ImGui::DockBuilderDockWindow(kWindowStats, rightBottomId);
        ImGui::DockBuilderDockWindow(kWindowNodeGraph, bottomId);
        ImGui::DockBuilderDockWindow(kWindowTextView, bottomId);
        ImGui::DockBuilderDockWindow(kWindowGameView, centerId);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    const ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin(kWindowHierarchy, nullptr, panelFlags)) {
        ImGui::TextUnformatted("検索");
        ImGui::SameLine();
        ImGui::Button(ICON_FA_PLUS, ImVec2(32.0f, 0.0f));
        ImGui::SameLine();
        ImGui::Button(ICON_FA_MINUS, ImVec2(32.0f, 0.0f));
        ImGui::Separator();
        ImGui::Selectable(ICON_FA_USER " プレイヤー", true);
        ImGui::Selectable((std::string(ICON_FA_BULLSEYE " 敵 x ") + std::to_string(enemies_.size())).c_str(), false);
        ImGui::Selectable((std::string(ICON_FA_CIRCLE " 自弾 x ") + std::to_string(playerBullets_.size())).c_str(), false);
        ImGui::Selectable((std::string(ICON_FA_CIRCLE_DOT " 敵弾 x ") + std::to_string(enemyBullets_.size())).c_str(), false);
        size_t activeRewardHeartCount = 0;
        for (const RewardHeart& heart : rewardHearts_) {
            if (heart.isActive && heart.object) {
                ++activeRewardHeartCount;
            }
        }
        ImGui::Selectable((std::string(ICON_FA_HEART " reward hearts x ") + std::to_string(activeRewardHeartCount)).c_str(), false);
        ImGui::Selectable((std::string(ICON_FA_CUBE " シーンオブジェクト x ") + std::to_string(sceneObjects_.size())).c_str(), false);
        ImGui::Separator();
        ImGui::Text("ウェーブ: %d / %d", (std::min)(currentWaveIndex_ + 1, kWaveCount), kWaveCount);
        ImGui::Text(
            "出現数: %d / %d",
            currentWaveIndex_ < kWaveCount ? spawnedEnemyCountInWave_ : 0,
            currentWaveIndex_ < kWaveCount ? waveTuning_[currentWaveIndex_].enemyCount : 0);
    }
    ImGui::End();

    if (ImGui::Begin(kWindowInspector, nullptr, panelFlags)) {
        if (ImGui::CollapsingHeader(ICON_FA_PALETTE " 描画", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox(ICON_FA_EYE " スカイボックス", &showSkybox_);
            const char* postEffectItems[] = {
                "なし",
                "グレースケール",
                "ビネット",
                "ボックス 3x3",
                "ボックス 5x5",
                "ガウシアン",
                "輝度アウトライン",
                "深度アウトライン",
                "ラジアルブラー",
                "ディゾルブ",
                "ランダム",
                "ビネット + スムージング",
                "ゲームトーン 自動",
                "ゲームトーン 低HP",
                "ゲームトーン クリア",
                "ゲームトーン ゲームオーバー",
                "ブルーム"
            };
            ImGui::Combo("ポストエフェクト", &postEffectMode_, postEffectItems, IM_ARRAYSIZE(postEffectItems));
        }
        if (ImGui::CollapsingHeader(ICON_FA_USER " プレイヤー", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("HP: %d / %d", GetPlayerHp(), GetPlayerMaxHp());
            ImGui::Text("スコア: %d", score_);
            ImGui::Text("チャージ: %s", chargeTimer_ >= chargeShotThreshold_ ? "完了" : "蓄積中");
            ImGui::Text("チャージ補助: %s", hasLockTarget_ ? "ON" : "OFF");
        }
        if (ImGui::CollapsingHeader(ICON_FA_LOCATION_DOT " トランスフォーム", ImGuiTreeNodeFlags_DefaultOpen)) {
            const Math::Vector3 playerPosition = player_ ? player_->GetTranslate() : Math::Vector3{};
            ImGui::Text("位置 X: %.2f", playerPosition.x);
            ImGui::Text("位置 Y: %.2f", playerPosition.y);
            ImGui::Text("位置 Z: %.2f", playerPosition.z);
            ImGui::Text("レール距離: %.1f", railDistance_);
        }
        if (ImGui::CollapsingHeader(ICON_FA_GAMEPAD " ゲーム進行", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("撃破数: %d / %d", defeatedEnemyCount_, GetTotalEnemyTargetCount());
            ImGui::Text("敵数: %zu", enemies_.size());
            ImGui::Text("自弾: %zu", playerBullets_.size());
            ImGui::Text("敵弾: %zu", enemyBullets_.size());
        }
    }
    ImGui::End();

    if (ImGui::Begin(kWindowGameView, nullptr, panelFlags)) {
        const ImVec2 contentMin = ImGui::GetCursorScreenPos();
        const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        constexpr float kGameViewAspect = 16.0f / 9.0f;
        ImVec2 imageSize(
            (std::max)(contentAvail.x, 1.0f),
            (std::max)(contentAvail.y, 1.0f));
        if (imageSize.x / imageSize.y > kGameViewAspect) {
            imageSize.x = imageSize.y * kGameViewAspect;
        } else {
            imageSize.y = imageSize.x / kGameViewAspect;
        }
        const ImVec2 imageOffset(
            (std::max)(contentAvail.x - imageSize.x, 0.0f) * 0.5f,
            (std::max)(contentAvail.y - imageSize.y, 0.0f) * 0.5f);
        ImGui::SetCursorScreenPos(
            ImVec2(contentMin.x + imageOffset.x, contentMin.y + imageOffset.y));
        if (dxCommon_) {
            ImGui::Image(
                static_cast<ImTextureID>(dxCommon_->GetRenderTextureGpuDescriptorHandle().ptr),
                imageSize);
            editorOverlayViewportMin_ = {
                contentMin.x + imageOffset.x,
                contentMin.y + imageOffset.y
            };
            editorOverlayViewportSize_ = { imageSize.x, imageSize.y };
            hasEditorOverlayViewportRect_ = imageSize.x > 1.0f && imageSize.y > 1.0f;
        }
    }
    ImGui::End();

    if (ImGui::Begin(kWindowProject, nullptr, panelFlags)) {
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " 開く")) {
            IGFD::FileDialogConfig config{};
            config.path = "resources";
            ImGuiFileDialog::Instance()->OpenDialog(
                "ProjectAssetDialog",
                "アセットを開く",
                ".json,.png,.gltf,.obj,.dds",
                config);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("ImGuiFileDialog");
        ImGui::Text("現在のシーン: %s", currentSceneFilePath_.c_str());
        ImGui::TextDisabled("%s", editorStatusMessage_.c_str());
        ImGui::Separator();
        ImGui::Columns(4, "GameOverlayAssetsRich", false);
        ImGui::TextUnformatted("シーン");
        ImGui::NextColumn();
        ImGui::TextUnformatted("プレハブ");
        ImGui::NextColumn();
        ImGui::TextUnformatted("モデル");
        ImGui::NextColumn();
        ImGui::TextUnformatted("テクスチャ");
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::TextUnformatted("resources/game_scene.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/prefab_00.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("game_player");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/checkerBoard.png");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/scene_01.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/prefab_01.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("primitive_sphere");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/uvChecker.png");
        ImGui::Columns(1);
    }
    ImGui::End();

    if (ImGui::Begin(kWindowStats, nullptr, panelFlags)) {
        static float historyTime[120]{};
        static float enemyHistory[120]{};
        static float bulletHistory[120]{};
        static float frameMsHistory[120]{};
        static int historyOffset = 0;
        const ImGuiIO& io = ImGui::GetIO();
        const float frameMs = io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f;
        size_t activeRewardHeartCount = 0;
        for (const RewardHeart& heart : rewardHearts_) {
            if (heart.isActive && heart.object) {
                ++activeRewardHeartCount;
            }
        }
        size_t hitVisualCount = 0;
        for (const HitEffect& effect : hitEffects_) {
            hitVisualCount += effect.visualCount;
        }
        const size_t activePlayerBulletCount = playerBullets_.size();
        const size_t activeEnemyBulletCount = enemyBullets_.size();
        const size_t activeBulletCount = activePlayerBulletCount + activeEnemyBulletCount;
        const size_t activeBulletDrawEstimate =
            activePlayerBulletCount * 3u +
            activeEnemyBulletCount * 2u;
        const size_t pooledBulletCount = playerBulletPool_.size() + enemyBulletPool_.size();
        const size_t activeSceneryCount = visibleSceneryCount_;
        const size_t totalSceneryCount = railSceneryObjects_.size();
        const size_t activeDepthCueCount = depthCueEffects_.size();
        const size_t estimatedActiveDrawObjects =
            sceneObjects_.size() +
            activeRewardHeartCount +
            activeSceneryCount +
            activeDepthCueCount +
            (player_ ? 1u : 0u) +
            enemies_.size() +
            activeBulletDrawEstimate +
            hitVisualCount;
        const size_t estimatedPooledBulletObjects = pooledBulletCount * 5u;
        historyTime[historyOffset] = static_cast<float>(historyOffset);
        enemyHistory[historyOffset] = static_cast<float>(enemies_.size());
        bulletHistory[historyOffset] =
            static_cast<float>(playerBullets_.size() + enemyBullets_.size());
        frameMsHistory[historyOffset] = frameMs;
        historyOffset = (historyOffset + 1) % 120;

        ImGui::Text("Perf: %.1f FPS / %.2f ms", io.Framerate, frameMs);
        ImGui::Text(
            "Objects: draw est %zu / pooled bullet objects %zu",
            estimatedActiveDrawObjects,
            estimatedPooledBulletObjects);
        ImGui::Text(
            "Active: depth %zu, scenery %zu/%zu, enemies %zu, bullets %zu, hit effects %zu, hit visuals %zu",
            activeDepthCueCount,
            activeSceneryCount,
            totalSceneryCount,
            enemies_.size(),
            activeBulletCount,
            hitEffects_.size(),
            hitVisualCount);
        ImGui::Text(
            "Pools: player %zu, enemy %zu",
            playerBulletPool_.size(),
            enemyBulletPool_.size());
        ImGui::Text(
            "Scene: objects %zu, hearts %zu/%zu miss %zu",
            sceneObjects_.size(),
            activeRewardHeartCount,
            rewardHearts_.size(),
            rewardHeartPoolMisses_);
        ImGui::Separator();
        if (ImPlot::BeginPlot("Frame time", ImVec2(-1.0f, 130.0f))) {
            ImPlot::SetupAxes("frame", "ms", ImPlotAxisFlags_NoTickLabels, 0);
            ImPlot::PlotLine("ms", historyTime, frameMsHistory, 120);
            ImPlot::EndPlot();
        }

        ImGui::Text("スコア: %d", score_);
        ImGui::Text("敵数: %zu", enemies_.size());
        ImGui::Text("弾数: %zu", playerBullets_.size() + enemyBullets_.size());
        if (ImPlot::BeginPlot("実行中カウント", ImVec2(-1.0f, 170.0f))) {
            ImPlot::SetupAxes("フレーム", "数", ImPlotAxisFlags_NoTickLabels, 0);
            ImPlot::PlotLine("敵", historyTime, enemyHistory, 120);
            ImPlot::PlotLine("弾", historyTime, bulletHistory, 120);
            ImPlot::EndPlot();
        }
    }
    ImGui::End();

    if (ImGui::Begin(kWindowTuning, nullptr, panelFlags)) {
        if (ImGui::Button(ICON_FA_ROTATE_LEFT " 調整をリセット")) {
            waveTuning_ = { {
                { 6, 42, 30.0f },
                { 8, 38, 34.0f },
                { 10, 34, 38.0f }
            } };
            railSpeed_ = IsTutorial() ? 0.145f : 0.235f;
            targetRailSpeed_ = 0.145f;
            playerBulletSpeed_ = 1.36f;
            lockBulletSpeed_ = 1.62f;
            chargedBulletSpeedMultiplier_ = 1.12f;
            enemyBulletSpeed_ = 0.36f;
            lockRadius_ = 118.0f;
            chargeShotThreshold_ = 72;
            normalShootCooldown_ = 14;
            chargedShootCooldown_ = 22;
            enemyShotInterval_ = 64;
            waveStartDelay_ = 90;
            editorStatusMessage_ = "ゲーム調整をリセットしました。";
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FORWARD_STEP " 今すぐ出現")) {
            enemySpawnTimer_ = 0;
        }

#ifdef ENABLE_DEBUG_GUI
        if (ImGui::CollapsingHeader("デバッグ操作", ImGuiTreeNodeFlags_DefaultOpen)) {
            const bool isFeverActive = feverTimer_ > 0;
            const float feverRate = isFeverActive ?
                static_cast<float>(feverTimer_) / static_cast<float>(kFeverDurationFrames) :
                static_cast<float>(feverGauge_) / static_cast<float>(kFeverGaugeMax);
            char feverLabel[64]{};
            if (isFeverActive) {
                std::snprintf(
                    feverLabel,
                    sizeof(feverLabel),
                    "FEVER %.1f sec",
                    static_cast<double>(feverTimer_) / 60.0);
            } else {
                std::snprintf(
                    feverLabel,
                    sizeof(feverLabel),
                    "Gauge %d / %d",
                    feverGauge_,
                    kFeverGaugeMax);
            }
            ImGui::ProgressBar(
                (std::clamp)(feverRate, 0.0f, 1.0f),
                ImVec2(-1.0f, 0.0f),
                feverLabel);

            const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfButtonWidth =
                (ImGui::GetContentRegionAvail().x - buttonSpacing) * 0.5f;
            if (ImGui::Button("MAX → 自動発動", ImVec2(halfButtonWidth, 0.0f))) {
                feverGauge_ = kFeverGaugeMax;
                editorStatusMessage_ = "ゲージを最大にしました。通常更新時に自動発動します。";
            }
            ImGui::SameLine();
            if (ImGui::Button("即時発動", ImVec2(halfButtonWidth, 0.0f))) {
                ActivateFever();
                editorStatusMessage_ = "フィーバーを発動しました。";
            }
            ImGui::BeginDisabled(!isFeverActive);
            if (ImGui::Button("終了", ImVec2(-1.0f, 0.0f))) {
                feverTimer_ = 0;
                feverActivationFlashTimer_ = 0;
                editorStatusMessage_ = "フィーバーを終了しました。";
            }
            ImGui::EndDisabled();

            ImGui::SeparatorText("ファントムレイド");
            ImGui::BeginDisabled(isGameOver_ || isGameClear_ || IsPhantomRaidActive());
            if (ImGui::Button("残像連撃を即回復", ImVec2(halfButtonWidth, 0.0f))) {
                phantomCooldown_ = 0.0f;
                GrantPhantomRaid();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!phantomReady_);
            if (ImGui::Button("残像連撃 [Q]", ImVec2(halfButtonWidth, 0.0f))) {
                TryActivatePhantomRaid();
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::SeparatorText("フェーズ移動");
            const bool phaseJumpDisabled = isGameOver_ || isGameClear_;
            ImGui::BeginDisabled(phaseJumpDisabled);
            if (ImGui::Button("Wave 1", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(0);
            }
            ImGui::SameLine();
            if (ImGui::Button("Wave 2", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(1);
            }
            if (ImGui::Button("Wave 3", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(2);
            }
            ImGui::SameLine();
            if (ImGui::Button("Boss", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(3);
            }
            if (ImGui::Button("狙撃機＋小型編隊", ImVec2(-1.0f, 0.0f))) {
                DebugJumpToStagePhase(4);
            }
            ImGui::EndDisabled();

            Enemy* debugBoss = nullptr;
            for (const auto& enemy : enemies_) {
                if (enemy && enemy->IsBoss() && !enemy->IsDead()) {
                    debugBoss = enemy.get();
                    break;
                }
            }
            ImGui::SeparatorText("Boss Test");
            ImGui::BeginDisabled(debugBoss == nullptr || bossPhase_ >= 2);
            if (ImGui::Button("Boss HP 50%", ImVec2(halfButtonWidth, 0.0f)) && debugBoss) {
                const int targetHp = (std::max)(1, debugBoss->GetMaxHp() / 2);
                const int damage = (std::max)(0, debugBoss->GetHp() - targetHp);
                if (damage > 0) {
                    debugBoss->Damage(damage);
                }
                editorStatusMessage_ = "Boss phase 2 trigger armed.";
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(debugBoss == nullptr || bossPhaseTransitionTimer_ > 0);
            if (ImGui::Button("Next Boss Attack", ImVec2(halfButtonWidth, 0.0f))) {
                bossAttackCooldown_ = 0;
                bossAttackStepTimer_ = 0;
                bossAttackStep_ = -1;
                editorStatusMessage_ = "Boss attack advanced.";
            }
            ImGui::EndDisabled();
            if (debugBoss) {
                ImGui::Text(
                    "Phase %d / Pattern %d / Cooldown %d / Counter %d",
                    bossPhase_,
                    bossAttackPattern_ + 1,
                    bossAttackCooldown_,
                    bossCounterTimer_);
            }
            if (phaseJumpDisabled) {
                ImGui::TextDisabled("フェーズ移動はゲーム進行中のみ使用できます。 ");
            }
        }
#endif

        if (ImGui::CollapsingHeader(ICON_FA_WAND_MAGIC_SPARKLES " 操作感", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("レール速度", &railSpeed_, 0.010f, 0.900f, "%.3f");
            ImGui::SliderFloat("自弾速度", &playerBulletSpeed_, 0.30f, 1.80f, "%.2f");
            ImGui::SliderFloat("チャージ弾速度", &lockBulletSpeed_, 0.70f, 2.20f, "%.2f");
            ImGui::SliderFloat("チャージ弾速度倍率", &chargedBulletSpeedMultiplier_, 1.00f, 2.00f, "%.2f");
            ImGui::SliderFloat("敵弾速度", &enemyBulletSpeed_, 0.12f, 0.70f, "%.2f");
            ImGui::SliderFloat("チャージ補助範囲", &lockRadius_, 40.0f, 260.0f, "%.0f px");
            ImGui::SliderInt("チャージ必要フレーム", &chargeShotThreshold_, 20, kChargeShotMax);
            ImGui::SliderInt("通常射撃クールダウン", &normalShootCooldown_, 2, 24);
            ImGui::SliderInt("チャージ射撃クールダウン", &chargedShootCooldown_, 4, 36);
            ImGui::SliderInt("敵射撃間隔", &enemyShotInterval_, 20, 180);
        }

        if (ImGui::CollapsingHeader(ICON_FA_LAYER_GROUP " ウェーブ", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderInt("ウェーブ開始待ち", &waveStartDelay_, 0, 180);
            for (int index = 0; index < kWaveCount; ++index) {
                ImGui::PushID(index);
                char label[32]{};
                std::snprintf(label, sizeof(label), "ウェーブ %d", index + 1);
                if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderInt("敵数", &waveTuning_[index].enemyCount, 1, 30);
                    ImGui::SliderInt("出現間隔", &waveTuning_[index].spawnInterval, 10, 180);
                    ImGui::SliderFloat("出現距離", &waveTuning_[index].spawnLeadDistance, 12.0f, 60.0f, "%.1f");
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }

        ImGui::Separator();
        ImGui::Text("現在のウェーブ: %d / %d", (std::min)(currentWaveIndex_ + 1, kWaveCount), kWaveCount);
        ImGui::Text("出現済み: %d", spawnedEnemyCountInWave_);
        ImGui::Text("次の出現タイマー: %d", enemySpawnTimer_);
        ImGui::Text("合計敵数: %d", GetTotalEnemyTargetCount());
    }
    ImGui::End();

    if (showAdvancedDebugPanels) {
        if (ImGui::Begin(kWindowNodeGraph, &showAdvancedDebugPanels, panelFlags)) {
            namespace ed = ax::NodeEditor;
            static ed::EditorContext* editorContext = ed::CreateEditor();
            ed::SetCurrentEditor(editorContext);
            ed::Begin("RuntimeFlow");
            ed::BeginNode(1);
            ImGui::TextUnformatted(ICON_FA_PLAY " 実行処理");
            ed::BeginPin(11, ed::PinKind::Output);
            ImGui::TextUnformatted("更新");
            ed::EndPin();
            ed::EndNode();
            ed::BeginNode(2);
            ImGui::TextUnformatted(ICON_FA_CHART_LINE " 統計");
            ed::BeginPin(21, ed::PinKind::Input);
            ImGui::Text("スコア %d", score_);
            ed::EndPin();
            ed::EndNode();
            ed::Link(100, 11, 21);
            ed::End();
            ed::SetCurrentEditor(nullptr);
        }
        ImGui::End();

        if (ImGui::Begin(kWindowTextView, &showAdvancedDebugPanels, panelFlags)) {
            static TextEditor editor;
            static bool isEditorInitialized = false;
            if (!isEditorInitialized) {
                editor.SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
                editor.SetReadOnly(true);
                editor.SetText(
                    "{\n"
                    "  \"scene\": \"resources/game_scene.json\",\n"
                    "  \"score\": 0,\n"
                    "  \"features\": [\"ImGuiFileDialog\", \"ImPlot\", \"NodeEditor\", \"ColorTextEdit\"]\n"
                    "}\n");
                isEditorInitialized = true;
            }
            static std::string lastTextViewScenePath;
            static int lastTextViewScore = -1;
            if (lastTextViewScenePath != currentSceneFilePath_ ||
                lastTextViewScore != score_) {
                char buffer[512]{};
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "{\n"
                    "  \"scene\": \"%s\",\n"
                    "  \"score\": %d,\n"
                    "  \"sceneObjects\": %zu,\n"
                    "  \"features\": [\"ImGuiFileDialog\", \"ImPlot\", \"NodeEditor\", \"ColorTextEdit\"]\n"
                    "}\n",
                    currentSceneFilePath_.c_str(),
                    score_,
                    sceneObjects_.size());
                editor.SetText(buffer);
                lastTextViewScenePath = currentSceneFilePath_;
                lastTextViewScore = score_;
            }
            ImGui::TextDisabled("ImGuiColorTextEdit");
            editor.Render("RuntimeJsonPreview", ImVec2(-1.0f, -1.0f));
        }
        ImGui::End();
    }

    if (ImGui::Begin(kWindowConsole, nullptr, panelFlags)) {
        ImGui::TextUnformatted("F1: GUI表示切り替え");
        ImGui::TextUnformatted("F2: タイトルへ戻る");
        ImGui::TextUnformatted("タブをドラッグするとレイアウトを並べ替えできます。");
        ImGui::Separator();
        ImGui::TextWrapped("%s", editorStatusMessage_.c_str());
    }
    ImGui::End();

    if (ImGuiFileDialog::Instance()->Display("GameSceneOpenDialog", ImGuiWindowFlags_NoCollapse, ImVec2(760, 460))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            LoadSceneObjects(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("GameSceneSaveDialog", ImGuiWindowFlags_NoCollapse, ImVec2(760, 460))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            SaveSceneObjects(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ProjectAssetDialog", ImGuiWindowFlags_NoCollapse, ImVec2(760, 460))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            editorStatusMessage_ =
                "選択中のアセット: " +
                ImGuiFileDialog::Instance()->GetFilePathName();
        }
        ImGuiFileDialog::Instance()->Close();
    }
}
#endif

void GameRuntime::DrawFlightSpeedOverlay()
{
    if (IsTutorial() || isGameOver_ || isGameClear_ || feverSpeedEffectRate_ > 0.15f) { return; }
    Math::Vector2 viewportMin{}, viewportSize{};
    GetEffectiveHudViewportRect(viewportMin, viewportSize);
    if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) { return; }
    // 速度線は周辺のみ。中央60%は照準と敵弾のために空ける。
    const float intensity = (0.24f + GetFlightSpeedRate() * 0.30f + flightReleaseKick_ * 0.65f) *
        GetCinematicWorldTimeScale();
    Math::Vector2 vanishing{ viewportMin.x + viewportSize.x * 0.5f, viewportMin.y + viewportSize.y * 0.45f };
    TryProjectToScreen({ 0.0f, 1.0f, railDistance_ + 260.0f }, vanishing);
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    drawList->PushClipRect({ viewportMin.x, viewportMin.y },
        { viewportMin.x + viewportSize.x, viewportMin.y + viewportSize.y }, true);
    for (int index = 0; index < 24; ++index) {
        const float seed = PseudoRandom01(index, 8.71f);
        const float angle = (static_cast<float>(index) + seed * 0.7f) * kTwoPi / 24.0f;
        const float phase = std::fmod(railDistance_ * (0.12f + seed * 0.03f) + seed, 1.0f);
        const float front = 0.76f + phase * 0.64f;
        const float back = (std::max)(0.70f, front - (0.06f + phase * 0.13f));
        const float dx = std::cos(angle) * viewportSize.x * 0.70f;
        const float dy = std::sin(angle) * viewportSize.y * 0.70f;
        const int alpha = static_cast<int>(66.0f * intensity * std::sin(phase * std::numbers::pi_v<float>));
        drawList->AddLine({ vanishing.x + dx * back, vanishing.y + dy * back },
            { vanishing.x + dx * front, vanishing.y + dy * front },
            IM_COL32(235, 240, 242, alpha), 1.0f + phase * 0.7f);
    }
    drawList->PopClipRect();
}

void GameRuntime::DrawFeverBackdrop()
{
    const float feverVisualRate =
        std::clamp(feverSpeedEffectRate_, 0.0f, 1.0f);
    if (feverVisualRate <= 0.001f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);
    const ImVec2 viewportMax(origin.x + drawSize.x, origin.y + drawSize.y);
    if (drawSize.x <= 1.0f || drawSize.y <= 1.0f) {
        return;
    }

    const float rainbowTime = cameraTimer_ * 0.008f;
    const auto rainbowColor = [rainbowTime, feverVisualRate](float offset, int alpha) {
        float hue = std::fmod(rainbowTime + offset, 1.0f);
        if (hue < 0.0f) {
            hue += 1.0f;
        }
        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        ImGui::ColorConvertHSVtoRGB(hue, 0.76f, 1.0f, red, green, blue);
        return IM_COL32(
            static_cast<int>(red * 255.0f),
            static_cast<int>(green * 255.0f),
            static_cast<int>(blue * 255.0f),
            (std::clamp)(
                static_cast<int>(static_cast<float>(alpha) * feverVisualRate),
                0,
                255));
    };

    const float ambiencePulse =
        0.5f + 0.5f * std::sin(cameraTimer_ * 0.055f);
    const int dimAlpha = static_cast<int>(
        (50.0f + ambiencePulse * 9.0f) * feverVisualRate);
    drawList->AddRectFilled(
        origin,
        viewportMax,
        IM_COL32(2, 4, 15, dimAlpha));
    drawList->AddRectFilled(
        origin,
        viewportMax,
        rainbowColor(0.0f, 8));

    const float vignetteSize =
        std::clamp((std::min)(drawSize.x, drawSize.y) * 0.14f, 54.0f, 112.0f);
    const int vignetteAlpha = static_cast<int>(
        (68.0f + ambiencePulse * 18.0f) * feverVisualRate);
    drawList->AddRectFilledMultiColor(
        origin,
        ImVec2(viewportMax.x, origin.y + vignetteSize),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, 0));
    drawList->AddRectFilledMultiColor(
        ImVec2(origin.x, viewportMax.y - vignetteSize),
        viewportMax,
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, vignetteAlpha));
    drawList->AddRectFilledMultiColor(
        origin,
        ImVec2(origin.x + vignetteSize, viewportMax.y),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, vignetteAlpha));
    drawList->AddRectFilledMultiColor(
        ImVec2(viewportMax.x - vignetteSize, origin.y),
        viewportMax,
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, 0));

    const float activationElapsedFrames =
        static_cast<float>(kFeverActivationFlashFrames - feverActivationFlashTimer_);
    const float accelerationKickPhase =
        std::clamp(activationElapsedFrames / 28.0f, 0.0f, 1.0f);
    const float feverAccelerationKick =
        feverActivationFlashTimer_ > 0 ?
        std::sin(accelerationKickPhase * std::numbers::pi_v<float>) : 0.0f;
    const float windIntensity = std::clamp(
        feverVisualRate + feverAccelerationKick * 0.48f,
        0.0f,
        1.28f);
    const ImVec2 windVanishingPoint(
        origin.x + drawSize.x * 0.50f,
        origin.y + drawSize.y * 0.445f);
    constexpr int kWindStreakCount = 60;
    constexpr float kPi = 3.14159265358979323846f;
    drawList->PushClipRect(origin, viewportMax, true);
    for (int index = 0; index < kWindStreakCount; ++index) {
        float seed = std::sin(
            static_cast<float>(index) * 12.9898f + 78.233f) * 43758.5453f;
        seed -= std::floor(seed);
        const float angle =
            2.0f * kPi *
            (static_cast<float>(index) + seed * 0.58f) /
            static_cast<float>(kWindStreakCount);
        const float travelSpeed =
            0.028f + static_cast<float>(index % 5) * 0.0017f +
            feverAccelerationKick * 0.009f;
        float phase = std::fmod(cameraTimer_ * travelSpeed + seed, 1.0f);
        if (phase < 0.0f) {
            phase += 1.0f;
        }

        const float frontRate = 0.17f + phase * 1.04f;
        const float edgeRate =
            std::clamp((frontRate - 0.24f) / 0.80f, 0.0f, 1.0f);
        const float streakLength =
            0.035f + edgeRate * (0.16f + seed * 0.08f);
        const float backRate = (std::max)(0.15f, frontRate - streakLength);
        const float directionX = std::cos(angle) * drawSize.x * 0.72f;
        const float directionY = std::sin(angle) * drawSize.y * 0.72f;
        const ImVec2 streakStart(
            windVanishingPoint.x + directionX * backRate,
            windVanishingPoint.y + directionY * backRate);
        const ImVec2 streakEnd(
            windVanishingPoint.x + directionX * frontRate,
            windVanishingPoint.y + directionY * frontRate);
        const float lifeFade = std::sin(phase * kPi);
        const int coreAlpha = static_cast<int>(
            (18.0f + edgeRate * 108.0f) * lifeFade * windIntensity);
        if (coreAlpha <= 1) {
            continue;
        }

        const bool useRainbowGlint = index % 6 == 0;
        const ImU32 coreColor = useRainbowGlint ?
            rainbowColor(seed + phase * 0.14f, coreAlpha) :
            IM_COL32(190, 236, 255, coreAlpha);
        drawList->AddLine(
            streakStart,
            streakEnd,
            IM_COL32(90, 190, 255, coreAlpha / 4),
            4.0f + edgeRate * 5.0f);
        drawList->AddLine(
            streakStart,
            streakEnd,
            coreColor,
            1.0f + edgeRate * 1.9f);
        drawList->AddCircleFilled(
            streakEnd,
            0.8f + edgeRate * 1.5f,
            IM_COL32(
                235,
                252,
                255,
                (std::min)(
                    coreAlpha + static_cast<int>(38.0f * feverVisualRate),
                    190)),
            8);
    }
    drawList->PopClipRect();

    constexpr int kBackdropHorizontalSegments = 20;
    constexpr int kBackdropVerticalSegments = 12;
    for (int layer = 0; layer < 4; ++layer) {
        const float margin = 6.0f + static_cast<float>(layer) * 4.0f;
        const float left = origin.x + margin;
        const float right = viewportMax.x - margin;
        const float top = origin.y + margin;
        const float bottom = viewportMax.y - margin;
        const int alpha = 34 - layer * 6;
        const float thickness = 7.0f - static_cast<float>(layer) * 1.2f;
        for (int segment = 0; segment < kBackdropHorizontalSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) /
                static_cast<float>(kBackdropHorizontalSegments);
            const float rate1 =
                static_cast<float>(segment + 1) /
                static_cast<float>(kBackdropHorizontalSegments);
            drawList->AddLine(
                ImVec2(Lerp(left, right, rate0), top),
                ImVec2(Lerp(left, right, rate1) + 1.0f, top),
                rainbowColor(rate0 * 0.78f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
            drawList->AddLine(
                ImVec2(Lerp(left, right, rate0), bottom),
                ImVec2(Lerp(left, right, rate1) + 1.0f, bottom),
                rainbowColor(0.78f - rate0 * 0.78f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
        }
        for (int segment = 0; segment < kBackdropVerticalSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) /
                static_cast<float>(kBackdropVerticalSegments);
            const float rate1 =
                static_cast<float>(segment + 1) /
                static_cast<float>(kBackdropVerticalSegments);
            drawList->AddLine(
                ImVec2(left, Lerp(top, bottom, rate0)),
                ImVec2(left, Lerp(top, bottom, rate1) + 1.0f),
                rainbowColor(0.18f + rate0 * 0.46f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
            drawList->AddLine(
                ImVec2(right, Lerp(top, bottom, rate0)),
                ImVec2(right, Lerp(top, bottom, rate1) + 1.0f),
                rainbowColor(0.94f - rate0 * 0.46f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
        }
    }

    const auto getPerimeterPoint = [origin, viewportMax](float rate, float margin) {
        const float left = origin.x + margin;
        const float right = viewportMax.x - margin;
        const float top = origin.y + margin;
        const float bottom = viewportMax.y - margin;
        const float width = (std::max)(right - left, 1.0f);
        const float height = (std::max)(bottom - top, 1.0f);
        const float perimeter = (width + height) * 2.0f;
        float distance = std::fmod(rate, 1.0f) * perimeter;
        if (distance < 0.0f) {
            distance += perimeter;
        }
        if (distance < width) {
            return ImVec2(left + distance, top);
        }
        distance -= width;
        if (distance < height) {
            return ImVec2(right, top + distance);
        }
        distance -= height;
        if (distance < width) {
            return ImVec2(right - distance, bottom);
        }
        distance -= width;
        return ImVec2(left, bottom - distance);
    };

    constexpr int kBorderSparkleCount = 42;
    for (int index = 0; index < kBorderSparkleCount; ++index) {
        const float indexRate =
            static_cast<float>(index) / static_cast<float>(kBorderSparkleCount);
        const float travelRate =
            indexRate + cameraTimer_ * (0.00042f + 0.00005f * static_cast<float>(index % 3));
        const ImVec2 position = getPerimeterPoint(
            travelRate,
            17.0f + static_cast<float>(index % 3) * 4.0f);
        const float rawTwinkle =
            0.5f + 0.5f * std::sin(cameraTimer_ * 0.18f + static_cast<float>(index) * 1.73f);
        const float twinkle = rawTwinkle * rawTwinkle * rawTwinkle;
        const float radius = 2.2f + twinkle * (5.8f + static_cast<float>(index % 4));
        const int glowAlpha = static_cast<int>(65.0f + twinkle * 125.0f);
        const ImU32 color = rainbowColor(indexRate + twinkle * 0.10f, glowAlpha);
        drawList->AddCircleFilled(position, radius * 1.35f, rainbowColor(indexRate, glowAlpha / 3), 16);
        drawList->AddQuadFilled(
            ImVec2(position.x, position.y - radius * 1.55f),
            ImVec2(position.x + radius * 0.34f, position.y),
            ImVec2(position.x, position.y + radius * 1.55f),
            ImVec2(position.x - radius * 0.34f, position.y),
            color);
        drawList->AddQuadFilled(
            ImVec2(position.x - radius * 1.55f, position.y),
            ImVec2(position.x, position.y - radius * 0.34f),
            ImVec2(position.x + radius * 1.55f, position.y),
            ImVec2(position.x, position.y + radius * 0.34f),
            color);
        drawList->AddCircleFilled(
            position,
            (std::max)(1.0f, radius * 0.24f),
            IM_COL32(
                255,
                255,
                255,
                static_cast<int>((145.0f + twinkle * 110.0f) * feverVisualRate)),
            10);
    }
}

void GameRuntime::DrawEnemyTypeTelegraphs()
{
    if (!player_) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const bool sniperTelegraphActive =
        enemyShotTimer_ > 0 &&
        enemyShotTimer_ <= kSniperTelegraphLeadFrames;
    const float tutorialChargeRate = 1.0f - std::clamp(
        static_cast<float>(enemyShotTimer_) /
            static_cast<float>(kSniperTelegraphLeadFrames),
        0.0f,
        1.0f);
    bool sniperTelegraphDrawn = false;

    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }

        Math::Vector2 enemyScreen{};
        if (!TryProjectToScreen(enemy->GetAimPosition(), enemyScreen)) {
            continue;
        }

        if (enemy->IsBoss() && bossAttackStep_ >= 0) {
            const int chargeDuration = BossWindupDuration(bossAttackPattern_, bossPhase_);
            const float bossChargeRate = 1.0f - std::clamp(
                static_cast<float>(bossAttackStepTimer_) /
                    static_cast<float>((std::max)(chargeDuration, 1)),
                0.0f,
                1.0f);
            // 実際の発射位置を投影する。自機を囲む照準線・追尾マーカーは付けない。
            const bool charge = bossAttackPattern_ == 2;
            const bool aimLocked = bossAttackStep_ > 0 || bossAttackStepTimer_ <= kBossAimLockFrames;
            const auto center = enemy->GetAimPosition();
            const int muzzleCount = charge ? 1 : 2;
            for (int index = 0; index < muzzleCount; ++index) {
                if (bossAttackPattern_ == 1 && index != bossAttackStep_ % 2) { continue; }
                const Math::Vector3 position{ center.x + (charge ? 0.0f : (index == 0 ? -2.2f : 2.2f)),
                    center.y + (charge ? -0.45f : 0.25f), center.z - 1.0f };
                Math::Vector2 screen{};
                Math::Vector2 rim{};
                if (!TryProjectToScreen(position, screen) ||
                    !TryProjectToScreen({ position.x + 0.55f, position.y, position.z }, rim)) { continue; }
                const ImVec2 muzzle(screen.x, screen.y);
                const float radius = std::clamp(std::abs(rim.x - screen.x), 3.0f, 13.0f) *
                    (0.55f + bossChargeRate * (charge ? 1.10f : 0.50f));
                const int alpha = static_cast<int>(100.0f + bossChargeRate * 125.0f);
                drawList->AddCircleFilled(muzzle, radius * 2.0f, IM_COL32(255, 65, 18, alpha / 5), 24);
                drawList->AddCircleFilled(muzzle, radius, IM_COL32(255, 104, 32, alpha), 24);
                drawList->AddCircleFilled(muzzle, radius * (aimLocked ? 0.55f : 0.30f),
                    IM_COL32(255, 240, 195, aimLocked ? 255 : alpha), 16);
            }
        }

        if (enemy->IsShield() && enemy->HasShield()) {
            const float shieldRate = std::clamp(
                static_cast<float>(enemy->GetShieldHp()) /
                    static_cast<float>((std::max)(enemy->GetShieldMaxHp(), 1)),
                0.0f,
                1.0f);
            const float shieldPulse =
                0.5f + 0.5f * std::sin(cameraTimer_ * 0.16f + enemyScreen.x * 0.01f);
            const int shieldAlpha = static_cast<int>(
                105.0f + shieldRate * 90.0f + shieldPulse * 35.0f);
            const ImVec2 center(enemyScreen.x, enemyScreen.y);
            const float radius = 25.0f + shieldPulse * 4.0f;
            drawList->AddCircleFilled(
                center,
                radius,
                IM_COL32(32, 176, 255, shieldAlpha / 7),
                32);
            drawList->AddCircle(
                center,
                radius,
                IM_COL32(86, 222, 255, shieldAlpha),
                32,
                2.5f);
            drawList->AddCircle(
                center,
                radius + 5.0f,
                IM_COL32(180, 248, 255, shieldAlpha / 2),
                32,
                1.0f);
        }

        if (enemy->IsSupport()) {
            const float supportPulse =
                0.5f + 0.5f * std::sin(cameraTimer_ * 0.18f + enemyScreen.y * 0.01f);
            const int supportAlpha = static_cast<int>(
                120.0f + supportPulse * 95.0f);
            const ImVec2 supportCenter(enemyScreen.x, enemyScreen.y);
            drawList->AddCircleFilled(
                supportCenter,
                13.0f + supportPulse * 3.0f,
                IM_COL32(112, 255, 94, supportAlpha / 6),
                20);
            drawList->AddCircle(
                supportCenter,
                17.0f + supportPulse * 4.0f,
                IM_COL32(132, 255, 104, supportAlpha),
                4,
                2.5f);

            int linkCount = 0;
            constexpr int kMaxSupportLinks = 3;
            for (const auto& linkedEnemy : enemies_) {
                if (!linkedEnemy || linkedEnemy.get() == enemy.get() ||
                    linkedEnemy->IsDead() || linkedEnemy->IsSupport()) {
                    continue;
                }
                Math::Vector2 linkedScreen{};
                if (!TryProjectToScreen(linkedEnemy->GetAimPosition(), linkedScreen)) {
                    continue;
                }
                drawList->AddLine(
                    supportCenter,
                    ImVec2(linkedScreen.x, linkedScreen.y),
                    IM_COL32(102, 255, 132, supportAlpha / 2),
                    1.5f + supportPulse);
                if (++linkCount >= kMaxSupportLinks) {
                    break;
                }
            }
        }

        if (!IsTutorial() && enemy->IsSniper()) {
            const auto& fire = enemy->GetFireControl();
            const float charge = fire.ChargeRate();
            const float flash = fire.ShotFlash();
            if ((charge > 0.0f || flash > 0.0f) && enemy->CanShoot()) {
                Math::Vector3 muzzleWorld = enemy->GetAimPosition();
                muzzleWorld.z -= 1.0f; // FireEnemyBulletと同じ発射位置に付ける。照準上には描かない。
                Math::Vector2 muzzleScreen{}, edgeScreen{};
                const Math::Vector3 edgeWorld{ muzzleWorld.x + 0.32f, muzzleWorld.y, muzzleWorld.z };
                if (TryProjectToScreen(muzzleWorld, muzzleScreen) && TryProjectToScreen(edgeWorld, edgeScreen)) {
                    const ImVec2 muzzle(muzzleScreen.x, muzzleScreen.y);
                    const float radius = std::clamp(std::abs(edgeScreen.x - muzzleScreen.x), 2.0f, 5.0f) *
                        (0.65f + charge * 0.60f + flash * 0.50f);
                    const int alpha = static_cast<int>(100.0f + charge * 100.0f + flash * 55.0f);
                    drawList->AddCircleFilled(muzzle, radius * 2.5f, IM_COL32(255, 76, 40, alpha / 6), 20);
                    drawList->AddCircleFilled(muzzle, radius, IM_COL32(255, 130, 70, alpha), 16);
                    drawList->AddCircleFilled(muzzle, radius * 0.42f, IM_COL32(255, 246, 220, alpha), 12);
                    // 照準が固定された最後の瞬間だけ、短い横の閃光で発射を知らせる。
                    if ((!fire.IsTracking() && charge > 0.0f) || flash > 0.0f) {
                        const float width = radius * (2.8f + flash * 1.8f);
                        drawList->AddLine({ muzzle.x - width, muzzle.y }, { muzzle.x + width, muzzle.y },
                            IM_COL32(255, 228, 176, alpha), 1.5f);
                    }
                }
            }
            continue;
        }
        const float chargeRate = tutorialChargeRate;
        if (!enemy->IsSniper() || !enemy->CanShoot() || chargeRate <= 0.0f ||
            (IsTutorial() && (sniperTelegraphDrawn || !sniperTelegraphActive))) {
            continue;
        }

        sniperTelegraphDrawn = true;
        const float pulse = 0.5f + 0.5f * std::sin(cameraTimer_ * 0.48f);
        const int lineAlpha = static_cast<int>(70.0f + chargeRate * 130.0f + pulse * 35.0f);
        const ImVec2 muzzle(enemyScreen.x, enemyScreen.y + 4.0f);
        const float coreRadius = 3.5f + chargeRate * 5.5f;
        drawList->AddCircleFilled(
            muzzle,
            coreRadius * 2.3f,
            IM_COL32(255, 28, 82, (std::clamp)(lineAlpha / 5, 0, 52)),
            24);
        drawList->AddCircleFilled(
            muzzle,
            coreRadius,
            IM_COL32(255, 60, 106, (std::clamp)(lineAlpha, 0, 255)),
            20);
        drawList->AddCircleFilled(
            muzzle,
            (std::max)(1.5f, coreRadius * 0.32f),
            IM_COL32(255, 236, 224, (std::clamp)(lineAlpha + 20, 0, 255)),
            14);
        constexpr int kSniperEnergyRayCount = 5;
        for (int rayIndex = 0; rayIndex < kSniperEnergyRayCount; ++rayIndex) {
            const float angle =
                -1.72f + static_cast<float>(rayIndex) * 0.86f +
                std::sin(cameraTimer_ * 0.07f) * 0.08f;
            const float outerRadius = 25.0f - chargeRate * 8.0f;
            const float innerRadius = 11.0f - chargeRate * 2.0f;
            const ImVec2 outer(
                muzzle.x + std::cos(angle) * outerRadius,
                muzzle.y + std::sin(angle) * outerRadius);
            const ImVec2 inner(
                muzzle.x + std::cos(angle) * innerRadius,
                muzzle.y + std::sin(angle) * innerRadius);
            drawList->AddLine(
                outer,
                inner,
                IM_COL32(255, 82, 124, (std::clamp)(lineAlpha, 0, 255)),
                1.3f + chargeRate * 1.2f);
        }
    }
}

void GameRuntime::DrawHud()
{
    if (!player_ || ((isGameClear_ || isGameOver_) && resultTransitionTimer_ <= 0)) {
        return;
    }

    // スキル表示も同じHUD入口から描く。通常・ポーズ・プレビューで表示が欠けないようにする。
    DrawPhantomRaidOverlay();
    ImGui::PushFont(CombatHud::BattleFont(), 16.0f);
    DrawFlightSpeedOverlay();
    DrawFeverBackdrop();
    DrawEnemyTypeTelegraphs();
    DrawRiftHud();

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);

    const int maxHp = (std::max)(GetPlayerMaxHp(), 1);
    const int hp = (std::clamp)(GetPlayerHp(), 0, maxHp);
    const float hpRate =
        static_cast<float>(hp) / static_cast<float>(maxHp);
    const float chargeRate = (std::clamp)(
        static_cast<float>(chargeTimer_) /
        static_cast<float>((std::max)(chargeShotThreshold_, 1)),
        0.0f,
        1.0f);
    const bool isChargeReady = chargeTimer_ >= chargeShotThreshold_;
    const float hudScale = GetCombatHudScale(hudSize);
    if (hpRate <= 0.34f && !isGameOver_) {
        const int alertAlpha = static_cast<int>(
            14.0f + (0.34f - hpRate) * 60.0f +
            8.0f * (0.5f + 0.5f * std::sin(cameraTimer_ * 8.0f)));
        const int clampedAlertAlpha = (std::clamp)(alertAlpha, 0, 32);
        const float alertWidth = 44.0f;
        const ImU32 alertColor = IM_COL32(230, 42, 58, clampedAlertAlpha);
        const ImU32 clearAlertColor = IM_COL32(230, 42, 58, 0);
        const ImVec2 viewportMax(origin.x + drawSize.x, origin.y + drawSize.y);
        drawList->AddRectFilledMultiColor(
            origin,
            ImVec2(viewportMax.x, origin.y + alertWidth),
            alertColor,
            alertColor,
            clearAlertColor,
            clearAlertColor);
        drawList->AddRectFilledMultiColor(
            ImVec2(origin.x, viewportMax.y - alertWidth),
            viewportMax,
            clearAlertColor,
            clearAlertColor,
            alertColor,
            alertColor);
        drawList->AddRectFilledMultiColor(
            origin,
            ImVec2(origin.x + alertWidth, viewportMax.y),
            alertColor,
            clearAlertColor,
            clearAlertColor,
            alertColor);
        drawList->AddRectFilledMultiColor(
            ImVec2(viewportMax.x - alertWidth, origin.y),
            viewportMax,
            clearAlertColor,
            alertColor,
            alertColor,
            clearAlertColor);
    }

    const ImVec2 playerAnchor(origin.x + 24.0f * hudScale, origin.y + 24.0f * hudScale);
    const auto hpPoint = [&](float x, float y) {
        return ImVec2(playerAnchor.x + x * hudScale, playerAnchor.y + y * hudScale);
    };
    char hpText[16]{};
    std::snprintf(hpText, sizeof(hpText), "%d", hp);
    const bool critical = hpRate <= 0.34f;
    DrawCombatHudShade(drawList, origin, hpPoint(292, 82), false);
    CombatHud::ShipIcon(drawList, hpPoint(22, 29), hudScale,
        critical ? CombatHud::Danger : CombatHud::White);
    DrawCombatHudText(drawList, hpPoint(54, 0), 18.0f * hudScale, CombatHud::White, "HP");
    CombatHud::Number(drawList, hpPoint(244, -4), 26.0f * hudScale,
        critical ? CombatHud::Danger : CombatHud::White, hpText, true);
    CombatHud::Meter(drawList, hpPoint(54, 24), hpPoint(244, 40), hpRate,
        critical ? CombatHud::Danger : CombatHud::Health, hudScale);
    DrawCombatHudText(drawList, hpPoint(54, 48), 16.0f * hudScale,
        CombatHud::Muted, "チャージ");
    CombatHud::Meter(drawList, hpPoint(130, 51), hpPoint(244, 61), chargeRate,
        isChargeReady ? CombatHud::Energy : CombatHud::Mix(CombatHud::Energy, CombatHud::White, 0.25f), hudScale);

    const ImVec2 scoreAnchor(origin.x + drawSize.x - 32.0f * hudScale, playerAnchor.y);
    const int waveNumber = currentWaveIndex_ < kWaveCount ? currentWaveIndex_ + 1 : kWaveCount;
    char scoreText[32]{};
    char waveText[48]{};
    std::snprintf(scoreText, sizeof(scoreText), "%06d", score_);
    if (IsTutorial()) {
        std::snprintf(waveText, sizeof(waveText), "チュートリアル");
    } else {
        std::snprintf(waveText, sizeof(waveText), "ウェーブ  %d / %d", waveNumber, kWaveCount);
    }
    DrawCombatHudShade(drawList,
        ImVec2(origin.x + drawSize.x - 280.0f * hudScale, origin.y),
        ImVec2(origin.x + drawSize.x, origin.y + 120.0f * hudScale), true);
    DrawCombatHudText(drawList, scoreAnchor, 18.0f * hudScale, CombatHud::White, "スコア", true);
    CombatHud::Number(drawList, ImVec2(scoreAnchor.x, scoreAnchor.y + 21.0f * hudScale),
        38.0f * hudScale, CombatHud::White, scoreText, true);
    DrawCombatHudText(drawList, ImVec2(scoreAnchor.x, scoreAnchor.y + 58.0f * hudScale),
        16.0f * hudScale, CombatHud::Muted, bossSpawned_ ? "ボス戦" : waveText, true);

    DrawBossHud();
    DrawTutorialGuideHud();
    DrawStageCueHud();
    DrawHitEffects();
    DrawLockOnHud();
    DrawHitConfirmHud();
    DrawPlayerDamageHud();
    DrawDefeatChainHud();
    DrawFeverHud();
    ImGui::PopFont();
}

void GameRuntime::DrawControlsHelp()
{
    if (!showControlsHelp_) { return; }
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    if (MenuUi::Controls({ min.x, min.y }, { size.x, size.y })) { showControlsHelp_ = false; }
}

void GameRuntime::DrawPauseOverlay()
{
    if (!isPaused_ || showControlsHelp_) { return; }
    if (MenuUi::Pressed(input_, DIK_UP) || MenuUi::Pressed(input_, DIK_W)) {
        pauseSelectedItem_ = (pauseSelectedItem_ + 3) % 4;
    }
    if (MenuUi::Pressed(input_, DIK_DOWN) || MenuUi::Pressed(input_, DIK_S)) {
        pauseSelectedItem_ = (pauseSelectedItem_ + 1) % 4;
    }
    const bool confirm = !menuInputConsumed_ && MenuUi::Pressed(input_, DIK_RETURN);
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    const float scale = GetCombatHudScale(size);
    const ImVec2 origin{ min.x + (size.x - 440 * scale) * 0.5f, min.y + (size.y - 440 * scale) * 0.5f };
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    auto* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled({ min.x, min.y }, { min.x + size.x, min.y + size.y }, IM_COL32(0, 0, 0, 170));
    MenuUi::Sheet(draw, origin, p(440, 440), scale);
    MenuUi::Text(draw, p(48, 36), 32 * scale, MenuUi::Paper, "ポーズ");
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({ 440 * scale, 440 * scale });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::Begin("##PauseActions", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    const char* labels[] = { "再開", "再挑戦", "操作方法", "タイトルへ" };
    for (int index = 0; index < 4; ++index) {
        const bool clicked = MenuUi::Button(draw, labels[index], labels[index], p(48, 120 + index * 62.0f),
            { 344 * scale, 48 * scale }, scale, pauseSelectedItem_ == index, false);
        MenuUi::SelectHovered(pauseSelectedItem_, index);
        if (clicked || (confirm && pauseSelectedItem_ == index)) {
            if (index == 0) { isPaused_ = false; }
            if (index == 1) { isRetryRequested_ = true; }
            if (index == 2) { showControlsHelp_ = true; }
            if (index == 3) { isExitRequested_ = true; }
            break;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
void GameRuntime::DrawBossHud()
{
    const Enemy* boss = GetBossEnemy();
    if (!boss || !bossSpawned_ || bossDefeated_) { return; }

    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    const float scale = GetCombatHudScale(size);
    const float width = 416.0f * scale;
    const ImVec2 anchor(min.x + (size.x - width) * 0.5f, min.y + 24.0f * scale);
    const auto p = [&](float x, float y) {
        return ImVec2(anchor.x + x * scale, anchor.y + y * scale);
    };
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const float hpRate = std::clamp(static_cast<float>(boss->GetHp()) /
        static_cast<float>((std::max)(boss->GetMaxHp(), 1)), 0.0f, 1.0f);
    const bool counter = bossCounterTimer_ > 0;
    const bool attacking = !counter && bossAttackStep_ >= 0 && bossAttackStepTimer_ > 0;
    const ImU32 clear = IM_COL32(12, 13, 14, 0);
    const ImU32 shade = IM_COL32(12, 13, 14, 150);
    const float shadeBottom = attacking || counter ? 83.0f : 58.0f;
    draw->AddRectFilledMultiColor(p(-64, -24), p(0, shadeBottom),
        clear, shade, clear, clear);
    draw->AddRectFilledMultiColor(p(0, -24), p(416, shadeBottom),
        shade, shade, clear, clear);
    draw->AddRectFilledMultiColor(p(416, -24), p(480, shadeBottom),
        shade, clear, clear, clear);
    CombatHud::Readout(draw, p(0, 0), 18.0f * scale, CombatHud::White, "ボス");
    CombatHud::Meter(draw, p(0, 29), p(416, 43), hpRate, CombatHud::Danger, scale);

    if (counter || attacking) {
        // 敵の状態と反撃可能時間だけを提示し、注意文やHPの重複数値は出さない。
        const char* label = counter ? "反撃 ×2" :
            bossAttackPattern_ == 0 ? "拡散弾" :
            bossAttackPattern_ == 1 ? "なぎ払い" : "チャージ砲";
        const ImU32 color = counter ? CombatHud::GaugeGold : CombatHud::Danger;
        const int duration = counter ? bossCounterDuration_ : BossWindupDuration(bossAttackPattern_, bossPhase_);
        const float remaining = std::clamp(
            static_cast<float>(counter ? bossCounterTimer_ : bossAttackStepTimer_) /
                static_cast<float>((std::max)(duration, 1)), 0.0f, 1.0f);
        const float rate = counter ? remaining : (bossAttackStep_ > 0 ? 1.0f : 1.0f - remaining);
        DrawCombatHudText(draw, p(416, 0), 16.0f * scale, color, label, true);
        CombatHud::Segments(draw, p(0, 49), p(416, 52), rate, color, 1, 0);
    }
}
void GameRuntime::DrawStageCueHud()
{
    if (bossWarningTimer_ <= 0 &&
        bossDefeatFlashTimer_ <= 0 &&
        bossPhaseTransitionTimer_ <= 0 &&
        justDodgeFlashTimer_ <= 0) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);
    const ImVec2 center(origin.x + drawSize.x * 0.5f, origin.y + drawSize.y * 0.29f);

    if (justDodgeFlashTimer_ > 0) {
        const float rate =
            static_cast<float>(justDodgeFlashTimer_) /
            static_cast<float>((std::max)(kJustDodgeFlashDuration, 1));
        const float sweep = 1.0f - rate;
        const float easedRate = rate * rate;
        const float hudDodgeLean =
            player_ ? static_cast<float>(player_->GetDodgeDirection() >= 0 ? 1 : -1) : 1.0f;
        const float shear = hudDodgeLean * (28.0f + sweep * 78.0f);
        const int lineAlpha = static_cast<int>(128.0f * easedRate);
        const int echoAlpha = static_cast<int>(72.0f * easedRate);
        const float topY = origin.y + drawSize.y * 0.145f;
        const float bottomY = origin.y + drawSize.y * 0.855f;
        const float leftX = origin.x + drawSize.x * (0.08f + sweep * 0.10f);
        const float rightX = origin.x + drawSize.x * (0.92f - sweep * 0.10f);

        drawList->AddLine(
            ImVec2(leftX + shear, topY),
            ImVec2(leftX + drawSize.x * 0.20f + shear * 0.15f, topY + 7.0f),
            IM_COL32(168, 242, 255, (std::clamp)(lineAlpha, 0, 148)),
            2.2f);
        drawList->AddLine(
            ImVec2(rightX + shear * 0.20f, bottomY - 7.0f),
            ImVec2(rightX - drawSize.x * 0.20f + shear, bottomY),
            IM_COL32(255, 154, 232, (std::clamp)(echoAlpha, 0, 92)),
            1.8f);
        drawList->AddLine(
            ImVec2(origin.x + drawSize.x * 0.14f - shear * 0.30f, bottomY),
            ImVec2(origin.x + drawSize.x * 0.34f - shear, bottomY - 8.0f),
            IM_COL32(168, 242, 255, (std::clamp)(echoAlpha, 0, 92)),
            1.8f);
    }

    if (bossDefeatFlashTimer_ > 0) {
        const float rate =
            static_cast<float>(bossDefeatFlashTimer_) /
            static_cast<float>((std::max)(kBossDefeatFlashDuration, 1));
        const int alpha = static_cast<int>(72.0f * rate * rate);
        drawList->AddRectFilled(
            origin,
            ImVec2(origin.x + drawSize.x, origin.y + drawSize.y),
            IM_COL32(255, 174, 76, (std::clamp)(alpha, 0, 120)));
    }

    const bool phaseChange = bossPhaseTransitionTimer_ > 0;
    const int timer = phaseChange ? bossPhaseTransitionTimer_ : bossWarningTimer_;
    const int duration = phaseChange ? kBossPhaseTransitionDuration : kBossWarningDuration;
    if (timer > 0) {
        const float remaining = static_cast<float>(timer) / static_cast<float>((std::max)(duration, 1));
        const float fade = std::clamp((1.0f - remaining) / 0.10f, 0.0f, 1.0f) *
            std::clamp(remaining / 0.20f, 0.0f, 1.0f);
        const float scale = GetCombatHudScale(hudSize);
        const char* label = phaseChange ? "第2段階" : "大型敵接近";
        const float fontSize = 30.0f * scale;
        const float width = CombatHud::ReadoutWidth(label, fontSize);
        DrawCombatHudText(drawList, ImVec2(center.x - width * 0.5f, center.y), fontSize,
            phaseChange ? IM_COL32(241, 240, 235, static_cast<int>(255.0f * fade)) :
                          IM_COL32(237, 100, 88, static_cast<int>(255.0f * fade)), label);
    }
}
void GameRuntime::DrawDefeatChainHud()
{
    const bool isChainActive = defeatChainCount_ >= 2;
    const bool isBreakNoticeVisible =
        !isChainActive && defeatChainBreakFlashTimer_ > 0;
    if (!isChainActive && !isBreakNoticeVisible) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);
    const float hudScale = GetCombatHudScale(hudSize);
    const float panelWidth = 190.0f * hudScale;
    const ImVec2 panelMin(
        origin.x + drawSize.x - 32.0f * hudScale - panelWidth,
        origin.y + 115.0f * hudScale);
    const ImVec2 panelMax(panelMin.x + panelWidth, panelMin.y + 42.0f * hudScale);
    ImU32 accentColor = CombatHud::GaugeGold;
    if (isBreakNoticeVisible) {
        const int alpha = static_cast<int>(
            100.0f + 145.0f *
            static_cast<float>(defeatChainBreakFlashTimer_) /
            static_cast<float>(kDefeatChainBreakFlashFrames));
        accentColor = IM_COL32(255, 96, 116, (std::clamp)(alpha, 0, 255));
    }

    if (isBreakNoticeVisible) {
        CombatHud::Readout(drawList, ImVec2(panelMax.x, panelMin.y),
            18.0f * hudScale, accentColor, "連続撃破終了", true);
        return;
    }

    const int totalScoreMultiplier =
        GetDefeatChainScoreMultiplier() *
        (feverTimer_ > 0 ? kFeverScoreMultiplier : 1);
    char chainText[32]{};
    char multiplierText[32]{};
    std::snprintf(chainText, sizeof(chainText), "%d 連続撃破", defeatChainCount_);
    std::snprintf(multiplierText, sizeof(multiplierText), "×%d", totalScoreMultiplier);
    DrawCombatHudText(drawList, ImVec2(panelMin.x, panelMin.y + 9.0f * hudScale),
        18.0f * hudScale, CombatHud::White, chainText);
    if (totalScoreMultiplier > 1) {
        const float punch = 1.0f - std::clamp(
            static_cast<float>(kDefeatChainDurationFrames - defeatChainTimer_) / 14.0f, 0.0f, 1.0f);
        CombatHud::Number(drawList, { panelMax.x, panelMin.y - punch * 3.0f * hudScale },
            (28.0f + punch * 3.0f) * hudScale, accentColor, multiplierText, true);
    }

    const ImVec2 timerMin(panelMin.x, panelMax.y - 2.0f * hudScale);
    const ImVec2 timerMax(panelMax.x, panelMax.y);
    const float timerRate = std::clamp(
        static_cast<float>(defeatChainTimer_) /
            static_cast<float>(kDefeatChainDurationFrames),
        0.0f,
        1.0f);
    CombatHud::Segments(drawList, timerMin, timerMax, timerRate, accentColor, 1, 0);
}

void GameRuntime::DrawFeverHud()
{
    if (!player_) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);
    const bool isActive = feverTimer_ > 0;
    const bool isReady = !isActive && feverGauge_ >= kFeverGaugeMax;
    const float rate = isActive ?
        static_cast<float>(feverTimer_) / static_cast<float>(kFeverDurationFrames) :
        static_cast<float>(feverGauge_) / static_cast<float>(kFeverGaugeMax);
    const float pulse = 0.5f + 0.5f * std::sin(cameraTimer_ * (isActive ? 8.5f : 5.0f));
    const float rainbowTime = cameraTimer_ * (isActive ? 0.010f : 0.006f);
    const auto rainbowColor = [rainbowTime](float offset, int alpha) {
        float hue = std::fmod(rainbowTime + offset, 1.0f);
        if (hue < 0.0f) {
            hue += 1.0f;
        }
        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        ImGui::ColorConvertHSVtoRGB(hue, 0.78f, 1.0f, red, green, blue);
        return IM_COL32(
            static_cast<int>(red * 255.0f),
            static_cast<int>(green * 255.0f),
            static_cast<int>(blue * 255.0f),
            (std::clamp)(alpha, 0, 255));
    };
    const auto rainbowReadout = [&](ImVec2 position, float size, int alpha, const char* text) {
        position.x = std::round(position.x);
        position.y = std::round(position.y);
        alpha = std::clamp(alpha, 0, 255);
        const float width = (std::max)(CombatHud::ReadoutWidth(text, size), 1.0f);
        // 影は暗色のまま残し、文字の頂点だけをゲージと同じ流れる虹色にする。
        drawList->AddText(CombatHud::BattleFont(), size, { position.x, position.y + 1.0f },
            IM_COL32(0, 0, 0, alpha * 4 / 5), text);
        const int firstVertex = drawList->VtxBuffer.Size;
        drawList->AddText(CombatHud::BattleFont(), size, position, IM_COL32(255, 255, 255, alpha), text);
        for (int index = firstVertex; index < drawList->VtxBuffer.Size; ++index) {
            ImDrawVert& vertex = drawList->VtxBuffer[index];
            const float offset = std::clamp((vertex.pos.x - position.x) / width, 0.0f, 1.0f);
            vertex.col = CombatHud::SurfaceColor(rainbowColor(offset, alpha));
        }
    };

    const float hudScale = GetCombatHudScale(hudSize);
    const ImVec2 anchor(origin.x + 32.0f * hudScale, origin.y + drawSize.y - 80.0f * hudScale);
    const auto p = [&](float x, float y) {
        return ImVec2(anchor.x + x * hudScale, anchor.y + y * hudScale);
    };
    CombatHud::Shade(drawList, p(-32, -16), p(280, 80));
    if (isActive || isReady) {
        rainbowReadout(p(0, 0), 20.0f * hudScale, 255, "フィーバー");
    } else {
        CombatHud::Readout(drawList, p(0, 0), 20.0f * hudScale, CombatHud::White, "フィーバー");
    }
    CombatHud::Meter(drawList, p(0, 32), p(232, 47), rate, CombatHud::FeverCharge, hudScale);
    // 発動中だけ計器の中が虹に変わる。通常の画面へ色を散らさない。
    if (isActive || isReady) {
        for (int index = 0; index < 24; ++index) {
            const float part = std::clamp(rate * 24.0f - static_cast<float>(index), 0.0f, 1.0f);
            if (part <= 0.0f) { break; }
            const float x = 2.0f + static_cast<float>(index) * 9.5f;
            drawList->AddRectFilledMultiColor(p(x, 34), p(x + 9.5f * part, 45),
                CombatHud::SurfaceColor(rainbowColor(static_cast<float>(index) / 24.0f, 255)),
                CombatHud::SurfaceColor(rainbowColor(static_cast<float>(index + 1) / 24.0f, 255)),
                CombatHud::SurfaceColor(CombatHud::Mix(rainbowColor(static_cast<float>(index + 1) / 24.0f, 255), IM_COL32(0, 0, 0, 255), 0.30f)),
                CombatHud::SurfaceColor(CombatHud::Mix(rainbowColor(static_cast<float>(index) / 24.0f, 255), IM_COL32(0, 0, 0, 255), 0.30f)));
        }
    }
    if (isActive) {
        char status[32]{};
        std::snprintf(status, sizeof(status), "%.1f秒", static_cast<float>(feverTimer_) / 60.0f);
        CombatHud::Readout(drawList, p(232, -1), 20.0f * hudScale, CombatHud::White, status, true);
    }
    if (isActive) {
        const int edgeAlpha = static_cast<int>(45.0f + pulse * 38.0f);
        constexpr int kHorizontalEdgeSegments = 16;
        constexpr int kVerticalEdgeSegments = 9;
        const float edgeLeft = origin.x + 7.0f;
        const float edgeRight = origin.x + drawSize.x - 7.0f;
        const float edgeTop = origin.y + 7.0f;
        const float edgeBottom = origin.y + drawSize.y - 7.0f;
        for (int segment = 0; segment < kHorizontalEdgeSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) / static_cast<float>(kHorizontalEdgeSegments);
            const float rate1 =
                static_cast<float>(segment + 1) / static_cast<float>(kHorizontalEdgeSegments);
            const float x0 = Lerp(edgeLeft, edgeRight, rate0);
            const float x1 = Lerp(edgeLeft, edgeRight, rate1);
            drawList->AddLine(
                ImVec2(x0, edgeTop),
                ImVec2(x1 + 1.0f, edgeTop),
                rainbowColor(rate0 * 0.76f, edgeAlpha),
                4.0f);
            drawList->AddLine(
                ImVec2(x0, edgeBottom),
                ImVec2(x1 + 1.0f, edgeBottom),
                rainbowColor(0.76f - rate0 * 0.76f, edgeAlpha),
                4.0f);
        }
        for (int segment = 0; segment < kVerticalEdgeSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) / static_cast<float>(kVerticalEdgeSegments);
            const float rate1 =
                static_cast<float>(segment + 1) / static_cast<float>(kVerticalEdgeSegments);
            const float y0 = Lerp(edgeTop, edgeBottom, rate0);
            const float y1 = Lerp(edgeTop, edgeBottom, rate1);
            drawList->AddLine(
                ImVec2(edgeLeft, y0),
                ImVec2(edgeLeft, y1 + 1.0f),
                rainbowColor(0.16f + rate0 * 0.48f, edgeAlpha),
                4.0f);
            drawList->AddLine(
                ImVec2(edgeRight, y0),
                ImVec2(edgeRight, y1 + 1.0f),
                rainbowColor(0.92f - rate0 * 0.48f, edgeAlpha),
                4.0f);
        }
        drawList->AddRect(
            ImVec2(origin.x + 12.0f, origin.y + 12.0f),
            ImVec2(origin.x + drawSize.x - 12.0f, origin.y + drawSize.y - 12.0f),
            IM_COL32(255, 255, 255, edgeAlpha / 3),
            0.0f,
            0,
            1.5f);
    }

    // 練習中は課題カードが発動を伝える。中央の大見出しを重ねて案内を隠さない。
    if (feverActivationFlashTimer_ > 0 && !IsTutorial()) {
        const float remaining = static_cast<float>(feverActivationFlashTimer_) /
            static_cast<float>(kFeverActivationFlashFrames);
        const float elapsed = 1.0f - remaining;
        const float visibility = std::clamp(elapsed / 0.12f, 0.0f, 1.0f) *
            std::clamp(remaining / 0.28f, 0.0f, 1.0f);
        const float fontSize = (44.0f + 8.0f * (1.0f - elapsed)) * hudScale;
        const char* title = "フィーバー";
        const float titleWidth = CombatHud::ReadoutWidth(title, fontSize);
        const ImVec2 center(origin.x + drawSize.x * 0.5f,
            origin.y + drawSize.y * 0.23f - elapsed * 8.0f * hudScale);
        rainbowReadout(
            { center.x - titleWidth * 0.5f, center.y },
            fontSize, static_cast<int>(255.0f * visibility), title);
    }
}

void GameRuntime::DrawLockOnHud()
{
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const bool aligned = isReticleOnTarget_;
    const bool charged = chargeTimer_ >= chargeShotThreshold_;
    const ImU32 color = aligned ? CombatHud::Danger : charged ? CombatHud::GaugeGold : CombatHud::White;
    const ImU32 shadow = IM_COL32(10, 11, 12, 190);
    const ImVec2 center(reticleScreen_.x, reticleScreen_.y);
    constexpr float radius = 21.0f;
    const float thickness = aligned ? 2.2f : 1.5f;
    // 明るい空でも読めるよう、発光ではなく細い暗色の下描きを使う。
    const auto line = [&](ImVec2 from, ImVec2 to) {
        draw->AddLine(from, to, shadow, thickness + 2.0f);
        draw->AddLine(from, to, color, thickness);
    };
    // 中央を空けた四隅の照準。追尾機能を示す偽のロックオン演出は追加しない。
    for (const float x : { -1.0f, 1.0f }) {
        for (const float y : { -1.0f, 1.0f }) {
            const ImVec2 corner(center.x + x * radius, center.y + y * radius);
            line(corner, { corner.x - x * 9.0f, corner.y });
            line(corner, { corner.x, corner.y - y * 9.0f });
        }
    }
    line({ center.x - 3.0f, center.y }, { center.x + 3.0f, center.y });
    line({ center.x, center.y - 3.0f }, { center.x, center.y + 3.0f });
}
void GameRuntime::DrawHitConfirmHud()
{
    if (hitConfirmTimer_ <= 0) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const float rate = std::clamp(
        static_cast<float>(hitConfirmTimer_) /
            static_cast<float>((std::max)(hitConfirmDuration_, 1)),
        0.0f,
        1.0f);
    const float elapsed = 1.0f - rate;
    const float settle = 1.0f - std::pow(1.0f - elapsed, 3.0f);
    const float fade = std::clamp(rate * 1.65f, 0.0f, 1.0f);
    const int alpha = static_cast<int>(255.0f * fade);
    const float radius =
        Lerp(32.0f * hitConfirmStrength_, 14.0f, settle) +
        (std::max)(elapsed - 0.72f, 0.0f) * 22.0f;
    const float lineLength = hitConfirmDestroyed_ ? 12.0f : 9.0f;
    const float thickness = hitConfirmDestroyed_ ? 3.0f : 2.2f;
    const ImVec2 center(hitConfirmScreen_.x, hitConfirmScreen_.y);

    const ImU32 mainColor = hitConfirmDestroyed_ || hitConfirmCharged_ ?
        IM_COL32(231, 189, 109, alpha) : IM_COL32(241, 240, 235, alpha);
    const int softAlpha = (std::clamp)(alpha / 3, 0, 96);
    const ImU32 softColor = IM_COL32(231, 189, 109, softAlpha);

    if (hitConfirmDestroyed_) {
        drawList->AddNgon(
            center,
            radius * 0.58f,
            softColor,
            4,
            3.2f);
    }
    for (int xSign : { -1, 1 }) {
        for (int ySign : { -1, 1 }) {
            const ImVec2 inner(
                center.x + static_cast<float>(xSign) * radius,
                center.y + static_cast<float>(ySign) * radius);
            const ImVec2 outer(
                center.x + static_cast<float>(xSign) * (radius + lineLength),
                center.y + static_cast<float>(ySign) * (radius + lineLength));
            drawList->AddLine(inner, outer, mainColor, thickness);
        }
    }
    drawList->AddCircleFilled(center, hitConfirmDestroyed_ ? 3.2f : 2.4f, mainColor, 16);

}

void GameRuntime::DrawPlayerDamageHud()
{
    if (playerDamageHudTimer_ <= 0) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const float rate = std::clamp(
        static_cast<float>(playerDamageHudTimer_) /
            static_cast<float>((std::max)(playerDamageHudDuration_, 1)),
        0.0f,
        1.0f);
    const float elapsed = 1.0f - rate;
    const float pulse = rate * rate;
    const int mainAlpha = static_cast<int>(220.0f * pulse);
    const int edgeAlpha = static_cast<int>(112.0f * pulse);
    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const ImVec2 viewportMin(hudMin.x, hudMin.y);
    const ImVec2 viewportMax(hudMin.x + hudSize.x, hudMin.y + hudSize.y);
    const ImVec2 viewportCenter(
        hudMin.x + hudSize.x * 0.5f,
        hudMin.y + hudSize.y * 0.5f);
    const float edgeWidth = 74.0f;
    const ImU32 edgeColor = IM_COL32(255, 32, 24, (std::clamp)(edgeAlpha, 0, 112));
    const ImU32 clearColor = IM_COL32(255, 32, 24, 0);

    drawList->AddRectFilled(
        viewportMin,
        viewportMax,
        IM_COL32(132, 0, 8, (std::clamp)(static_cast<int>(20.0f * pulse), 0, 20)));
    if (playerDamageDirection_.x < -0.18f) {
        drawList->AddRectFilledMultiColor(
            viewportMin,
            ImVec2(viewportMin.x + edgeWidth, viewportMax.y),
            edgeColor, clearColor, clearColor, edgeColor);
    } else if (playerDamageDirection_.x > 0.18f) {
        drawList->AddRectFilledMultiColor(
            ImVec2(viewportMax.x - edgeWidth, viewportMin.y),
            viewportMax,
            clearColor, edgeColor, edgeColor, clearColor);
    }
    if (playerDamageDirection_.y < -0.18f) {
        drawList->AddRectFilledMultiColor(
            viewportMin,
            ImVec2(viewportMax.x, viewportMin.y + edgeWidth),
            edgeColor, edgeColor, clearColor, clearColor);
    } else if (playerDamageDirection_.y > 0.18f) {
        drawList->AddRectFilledMultiColor(
            ImVec2(viewportMin.x, viewportMax.y - edgeWidth),
            viewportMax,
            clearColor, clearColor, edgeColor, edgeColor);
    }

    const ImVec2 impactCenter(playerDamageScreen_.x, playerDamageScreen_.y);
    const float impactRadius = 18.0f + elapsed * 18.0f;
    const ImU32 impactColor =
        IM_COL32(255, 116, 72, (std::clamp)(mainAlpha, 0, 220));
    for (int xSign : { -1, 1 }) {
        for (int ySign : { -1, 1 }) {
            const ImVec2 corner(
                impactCenter.x + static_cast<float>(xSign) * impactRadius,
                impactCenter.y + static_cast<float>(ySign) * impactRadius);
            const float segment = 9.0f + 5.0f * rate;
            drawList->AddLine(
                corner,
                ImVec2(corner.x - static_cast<float>(xSign) * segment, corner.y),
                impactColor,
                2.6f);
            drawList->AddLine(
                corner,
                ImVec2(corner.x, corner.y - static_cast<float>(ySign) * segment),
                impactColor,
                2.6f);
        }
    }
    for (int index = 0; index < 4; ++index) {
        const float angle =
            kTwoPi * static_cast<float>(index) / 4.0f + 0.785398f;
        const ImVec2 direction(std::cos(angle), std::sin(angle));
        const float innerRadius = impactRadius + 3.0f;
        const float outerRadius = innerRadius + 8.0f + 7.0f * rate;
        drawList->AddLine(
            ImVec2(
                impactCenter.x + direction.x * innerRadius,
                impactCenter.y + direction.y * innerRadius),
            ImVec2(
                impactCenter.x + direction.x * outerRadius,
                impactCenter.y + direction.y * outerRadius),
            impactColor,
            2.0f);
    }

    const ImVec2 direction(
        playerDamageDirection_.x,
        playerDamageDirection_.y);
    const ImVec2 tangent(-direction.y, direction.x);
    const float markerMinX = viewportMin.x + 38.0f;
    const float markerMaxX = viewportMax.x - 38.0f;
    const float markerMinY = viewportMin.y + 146.0f;
    const float markerMaxY = viewportMax.y - 54.0f;
    const float edgeScaleX = direction.x > 0.001f ?
        (markerMaxX - viewportCenter.x) / direction.x :
        direction.x < -0.001f ?
            (markerMinX - viewportCenter.x) / direction.x : 100000.0f;
    const float edgeScaleY = direction.y > 0.001f ?
        (markerMaxY - viewportCenter.y) / direction.y :
        direction.y < -0.001f ?
            (markerMinY - viewportCenter.y) / direction.y : 100000.0f;
    const float edgeScale = (std::min)(edgeScaleX, edgeScaleY);
    const ImVec2 sourceMarker(
        viewportCenter.x + direction.x * edgeScale,
        viewportCenter.y + direction.y * edgeScale);
    for (int index = 0; index < 2; ++index) {
        const float offset = static_cast<float>(index) * 10.0f;
        const ImVec2 tip(
            sourceMarker.x - direction.x * offset,
            sourceMarker.y - direction.y * offset);
        drawList->AddLine(
            ImVec2(
                tip.x + direction.x * 13.0f + tangent.x * 9.0f,
                tip.y + direction.y * 13.0f + tangent.y * 9.0f),
            tip,
            impactColor,
            3.0f);
        drawList->AddLine(
            tip,
            ImVec2(
                tip.x + direction.x * 13.0f - tangent.x * 9.0f,
                tip.y + direction.y * 13.0f - tangent.y * 9.0f),
            impactColor,
            3.0f);
    }

}

void GameRuntime::DrawResultOverlay()
{
    if ((!isGameOver_ && !isGameClear_) || resultTransitionTimer_ > 0) { return; }
    if (IsTutorial() && isGameClear_) { DrawTutorialResult(); return; }

    Math::Vector2 hudMin{}, hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);
    const float scale = GetCombatHudScale(hudSize);
    const ImVec2 origin{ hudMin.x, hudMin.y };
    const ImVec2 panelMin{ origin.x + (hudSize.x - 920 * scale) * 0.5f,
        origin.y + (hudSize.y - 500 * scale) * 0.5f };
    const auto p = [&](float x, float y) { return ImVec2(panelMin.x + x * scale, panelMin.y + y * scale); };
    auto* draw = ImGui::GetForegroundDrawList();
    // 戦闘HUDより前に一枚の結果シートを置く。黄色の囲み・見出し・評価の重複は使わない。
    draw->AddRectFilled(origin, { origin.x + hudSize.x, origin.y + hudSize.y }, IM_COL32(0, 0, 0, 175));
    MenuUi::Sheet(draw, panelMin, p(920, 500), scale);
    MenuUi::Text(draw, p(48, 34), 32 * scale, MenuUi::Paper,
        (IsTutorial() && isGameClear_) ? "チュートリアル完了" : (isGameClear_ ? "ステージクリア" : "ゲームオーバー"));

    char value[96]{};
    MenuUi::Text(draw, p(48, 109), 16 * scale, MenuUi::Quiet, "スコア");
    std::snprintf(value, sizeof(value), "%06d", score_);
    MenuUi::Number(draw, p(44, 124), 78 * scale, MenuUi::Paper, value);
    if (isGameClear_ && !IsTutorial()) {
        const char* rank = playerDamageCount_ == 0 && escapedEnemyCount_ <= 2 ? "S" :
            (playerDamageCount_ <= 2 && escapedEnemyCount_ <= 5 ? "A" : "B");
        MenuUi::Text(draw, p(750, 109), 16 * scale, MenuUi::Quiet, "評価");
        MenuUi::Number(draw, p(872, 75), 132 * scale, MenuUi::Paper, rank, true);
    }
    const ImU32 rule = CombatHud::SurfaceColor(IM_COL32(88, 92, 101, 155));
    draw->AddLine(p(48, 218), p(872, 218), rule, scale);
    // 一列四項目。数値が先に目へ入り、単位や名称は同じ場所で読める。
    const auto stat = [&](int column, int row, const char* label, const char* number) {
        const float x = 48.0f + static_cast<float>(column) * 212.0f;
        const float y = 242.0f + static_cast<float>(row) * 77.0f;
        MenuUi::Text(draw, p(x, y), 17 * scale, MenuUi::Quiet, label);
        MenuUi::Number(draw, p(x, y + 22), 29 * scale, MenuUi::Paper, number);
    };
    const int total = (std::max)(0, static_cast<int>(gameplayElapsedSeconds_ * 100));
    std::snprintf(value, sizeof(value), "%02d:%02d.%02d", total / 6000, (total / 100) % 60, total % 100);
    stat(0, 0, "プレイ時間", value);
    std::snprintf(value, sizeof(value), "%d / %d", defeatedEnemyCount_, GetTotalEnemyTargetCount() + (bossSpawned_ ? 1 : 0));
    stat(1, 0, "撃破数", value);
    std::snprintf(value, sizeof(value), "%d", maxDefeatChainCount_);
    stat(2, 0, "最大連続撃破", value);
    std::snprintf(value, sizeof(value), "%d", playerDamageCount_);
    stat(3, 0, "被弾回数", value);
    std::snprintf(value, sizeof(value), "%d", playerShotsFired_);
    stat(0, 1, "発射数", value);
    std::snprintf(value, sizeof(value), "%d", playerHitCount_);
    stat(1, 1, "命中数", value);
    std::snprintf(value, sizeof(value), "%d", justDodgeCount_);
    stat(2, 1, "ジャスト回避", value);
    std::snprintf(value, sizeof(value), "%d", escapedEnemyCount_);
    stat(3, 1, "逃した敵", value);
    draw->AddLine(p(48, 408), p(872, 408), rule, scale);

    ImGui::SetNextWindowPos(panelMin, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ 920 * scale, 500 * scale }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::Begin("##ResultActions", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
    if (MenuUi::Pressed(input_, DIK_LEFT) || MenuUi::Pressed(input_, DIK_A) ||
        MenuUi::Pressed(input_, DIK_UP) || MenuUi::Pressed(input_, DIK_W) ||
        MenuUi::Pressed(input_, DIK_RIGHT) || MenuUi::Pressed(input_, DIK_D) ||
        MenuUi::Pressed(input_, DIK_DOWN) || MenuUi::Pressed(input_, DIK_S)) {
        resultSelectedItem_ = 1 - resultSelectedItem_;
    }
    const bool confirm = MenuUi::Pressed(input_, DIK_RETURN);
    const int confirmedItem = resultSelectedItem_;
    if (MenuUi::Button(draw, "retry", "再挑戦", p(452, 434), { 200 * scale, 44 * scale }, scale,
        resultSelectedItem_ == 0, false) || (confirm && confirmedItem == 0)) { isRetryRequested_ = true; }
    MenuUi::SelectHovered(resultSelectedItem_, 0);
    if (MenuUi::Button(draw, "title", "タイトルへ", p(672, 434), { 200 * scale, 44 * scale }, scale,
        resultSelectedItem_ == 1, false) || (confirm && confirmedItem == 1)) {
        isExitRequested_ = true;
    }
    MenuUi::SelectHovered(resultSelectedItem_, 1);
    ImGui::End();
    ImGui::PopStyleVar();
}

#ifdef ENABLE_DEBUG_GUI
void GameRuntime::DrawPerformanceOverlay()
{
    if (!isPerformanceOverlayVisible_) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();

    size_t activeRewardHeartCount = 0;
    for (const RewardHeart& heart : rewardHearts_) {
        if (heart.isActive && heart.object) {
            ++activeRewardHeartCount;
        }
    }

    size_t hitVisualCount = 0;
    for (const HitEffect& effect : hitEffects_) {
        hitVisualCount += effect.visualCount;
    }

    const size_t activePlayerBulletCount = playerBullets_.size();
    const size_t activeEnemyBulletCount = enemyBullets_.size();
    const size_t activeBulletCount = activePlayerBulletCount + activeEnemyBulletCount;
    const size_t activeBulletDrawEstimate =
        activePlayerBulletCount * 3u +
        activeEnemyBulletCount * 2u;
    const size_t pooledBulletCount = playerBulletPool_.size() + enemyBulletPool_.size();
    const size_t pooledHitEffectObjects = hitEffectObjectPool_.size();
    const size_t activeSceneryCount = visibleSceneryCount_;
    const size_t totalSceneryCount = railSceneryObjects_.size();
    const size_t activeDepthCueCount = depthCueEffects_.size();
    const size_t estimatedActiveDrawObjects =
        sceneObjects_.size() +
        activeRewardHeartCount +
        activeSceneryCount +
        activeDepthCueCount +
        (showSkybox_ && skybox_ ? 1u : 0u) +
        (player_ ? 1u : 0u) +
        enemies_.size() +
        activeBulletDrawEstimate +
        hitVisualCount;
    const size_t estimatedPooledBulletObjects = pooledBulletCount * 5u;
    DirectXCommon::FrameTiming timing{};
    if (dxCommon_) {
        timing = dxCommon_->GetFrameTiming();
    }
    const float measuredFrameMs =
        timing.frameCpuMs > 0.0f ?
        timing.frameCpuMs :
        (io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    const float measuredFps =
        measuredFrameMs > 0.0f ? 1000.0f / measuredFrameMs : 0.0f;

    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 panelMin(hudMin.x + 18.0f, hudMin.y + hudSize.y - 206.0f);
    const ImVec2 panelMax(panelMin.x + 540.0f, panelMin.y + 186.0f);
    const ImU32 accentColor =
        measuredFrameMs > 28.0f ? IM_COL32(255, 90, 90, 235) :
        measuredFrameMs > 18.0f ? IM_COL32(255, 205, 92, 235) :
                                  IM_COL32(116, 242, 190, 235);

    drawList->AddRectFilled(
        ImVec2(panelMin.x + 3.0f, panelMin.y + 4.0f),
        ImVec2(panelMax.x + 3.0f, panelMax.y + 4.0f),
        IM_COL32(0, 0, 0, 82),
        6.0f);
    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(6, 10, 18, 180), 6.0f);
    drawList->AddRect(panelMin, panelMax, accentColor, 6.0f, 0, 2.0f);

    char line[256]{};
    std::snprintf(
        line,
        sizeof(line),
        "F3 PERF inst %.1f FPS / %.2f ms  avg %.1f FPS",
        measuredFps,
        measuredFrameMs,
        io.Framerate);
    drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 10.0f), accentColor, line);

    std::snprintf(
        line,
        sizeof(line),
        "draw est %zu  depth %zu  scenery %zu/%zu  hearts %zu/%zu  bullets %zu  enemies %zu",
        estimatedActiveDrawObjects,
        activeDepthCueCount,
        activeSceneryCount,
        totalSceneryCount,
        activeRewardHeartCount,
        rewardHearts_.size(),
        activeBulletCount,
        enemies_.size());
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 35.0f),
        IM_COL32(224, 238, 248, 230),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "pool P %zu/%d E %zu/%d  miss P/E %zu/%zu  preallocated",
        playerBulletPool_.size(),
        kTargetPlayerBulletPoolCount,
        enemyBulletPool_.size(),
        kTargetEnemyBulletPoolCount,
        playerBulletPoolMisses_,
        enemyBulletPoolMisses_);
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 58.0f),
        IM_COL32(188, 210, 228, 220),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "pool objs bullet %zu  hitFx %zu/%d miss %zu  heart miss %zu  post %d%s sky %s",
        estimatedPooledBulletObjects,
        pooledHitEffectObjects,
        kTargetHitEffectObjectPoolCount,
        hitEffectObjectPoolMisses_,
        rewardHeartPoolMisses_,
        GetPostEffectMode(),
        isPostEffectBypassEnabled_ ? " BYPASS" : "",
        showSkybox_ ? "ON" : "OFF");
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 82.0f),
        IM_COL32(188, 210, 228, 220),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "cpu upd %.1f pre %.1f scene %.1f post %.1f total %.1f",
        timing.updateMs,
        timing.preDrawMs,
        timing.sceneDrawMs,
        timing.postEffectMs,
        timing.frameCpuMs);
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 106.0f),
        IM_COL32(206, 224, 238, 224),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "rail %.1f speed %.3f  stage %.1f pace %.3f  %s / %s",
        railDistance_,
        railSpeed_,
        stageProgress_,
        stageTimelineSpeed_,
        stageSectionName_ ? stageSectionName_ : "Unknown",
        stageCombatBeatName_ ? stageCombatBeatName_ : "Intro");
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 130.0f),
        IM_COL32(206, 224, 238, 224),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "wait present %.1f fence %.1f fps %.1f  F4 post/F5 sky",
        timing.presentMs,
        timing.fenceWaitMs,
        timing.fpsWaitMs);
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 154.0f),
        IM_COL32(206, 224, 238, 224),
        line);
}

#endif

void GameRuntime::UpdatePlayerBullets()
{
    for (auto iterator = playerBullets_.begin();
        iterator != playerBullets_.end();) {
        if ((*iterator)->CanHome()) {
            Bullet* bullet = iterator->get();
            auto targetIterator = homingBulletTargets_.find(bullet);
            const Enemy* target =
                targetIterator != homingBulletTargets_.end() ?
                targetIterator->second :
                nullptr;
            const Math::Vector3 targetPosition =
                target ? target->GetAimPosition() : Math::Vector3{};
            const bool hasPassedHomingTarget =
                target &&
                targetPosition.z + 1.25f < bullet->GetTranslate().z;
            if (target && !target->IsDead() && !hasPassedHomingTarget) {
                (*iterator)->SetHomingTarget(targetPosition);
            } else {
                (*iterator)->ClearHomingTarget();
                homingBulletTargets_.erase(bullet);
            }
        }
        (*iterator)->Update();
        if ((*iterator)->IsDead()) {
            homingBulletTargets_.erase(iterator->get());
            playerBulletPool_.push_back(std::move(*iterator));
            iterator = playerBullets_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void GameRuntime::UpdateEnemyBullets()
{
    const float worldTimeScale = GetCinematicWorldTimeScale();
    for (auto iterator = enemyBullets_.begin();
        iterator != enemyBullets_.end();) {
        Bullet* bullet = iterator->get();
        bullet->Update(worldTimeScale);
        if (bullet->IsDead()) {
            justDodgedEnemyBullets_.erase(bullet);
            enemyBulletPool_.push_back(std::move(*iterator));
            iterator = enemyBullets_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void GameRuntime::UpdateEnemies()
{
    const float worldTimeScale = GetCinematicWorldTimeScale();
    const bool sniperTelegraphActive =
        enemyShotTimer_ > 0 &&
        enemyShotTimer_ <= kSniperTelegraphLeadFrames;
    const float sniperChargeRate = 1.0f - std::clamp(
        static_cast<float>(enemyShotTimer_) /
            static_cast<float>(kSniperTelegraphLeadFrames),
        0.0f,
        1.0f);
    bool sniperTelegraphAssigned = false;
    for (auto iterator = enemies_.begin(); iterator != enemies_.end();) {
        float attackTelegraphRate = IsTutorial() ? 0.0f : (*iterator)->GetFireControl().ChargeRate();
        if ((*iterator)->IsBoss() &&
            bossAttackStep_ == 0 &&
            bossAttackStepTimer_ > 0) {
            const int chargeDuration = BossWindupDuration(bossAttackPattern_, bossPhase_);
            attackTelegraphRate = 1.0f - std::clamp(
                static_cast<float>(bossAttackStepTimer_) /
                    static_cast<float>((std::max)(chargeDuration, 1)),
                0.0f,
                1.0f);
        } else if (IsTutorial() && !sniperTelegraphAssigned &&
            sniperTelegraphActive &&
            (*iterator)->IsSniper() &&
            (*iterator)->CanShoot()) {
            attackTelegraphRate = sniperChargeRate;
            sniperTelegraphAssigned = true;
        }
        (*iterator)->SetAttackTelegraphRate(attackTelegraphRate);
        (*iterator)->SetBossRecoveryRate(static_cast<float>(bossCounterTimer_) /
            static_cast<float>((std::max)(bossCounterDuration_, 1)));
        (*iterator)->Update(railDistance_, worldTimeScale);
        if ((*iterator)->IsDead()) {
            if ((*iterator)->HasEscaped()) {
                ++escapedEnemyCount_;
                BreakEnemyDefeatChain();
            }
            const Enemy* removedEnemy = iterator->get();
            for (auto& target : phantomTargets_) {
                if (target.enemy == removedEnemy) {
                    target.enemy = nullptr;
                }
            }
            for (auto targetIterator = homingBulletTargets_.begin();
                targetIterator != homingBulletTargets_.end();) {
                if (targetIterator->second == removedEnemy) {
                    targetIterator = homingBulletTargets_.erase(targetIterator);
                } else {
                    ++targetIterator;
                }
            }
            iterator = enemies_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

float GameRuntime::GetCinematicWorldTimeScale() const
{
    float timeScale = 1.0f;
    if (IsPhantomRaidActive()) {
        // 構えと連撃中も前進を残す。最後の一撃だけ約5フレーム強く止める。
        timeScale = phantomClock_ < 12.0f ? 0.55f :
            (phantomClock_ < PhantomFinisherTime() ? 0.75f :
                (phantomClock_ < PhantomFinisherTime() + 5.0f ? 0.08f : 1.0f));
    }
    if (justDodgeSlowTimer_ > 0) {
        timeScale = (std::min)(timeScale, kJustDodgeRailSlowScale);
    }
    if (playerImpactSlowTimer_ > 0) {
        timeScale *= playerImpactSlowScale_;
    }
    return std::clamp(timeScale, 0.05f, 1.0f);
}

float GameRuntime::GetFlightSpeedRate() const
{
    const float base = IsTutorial() ? 0.13f : 0.24f;
    const float range = IsTutorial() ? 0.10f : 0.18f;
    return std::clamp((railSpeed_ - base) / range, 0.0f, 1.0f);
}

Math::Vector3 GameRuntime::CalculateAimDirection(const Math::Vector3& origin) const
{
    if (!camera_) {
        return { 0.0f, 0.0f, 1.0f };
    }

    Math::Vector2 viewportMin{};
    Math::Vector2 viewportSize{};
    GetEffectiveHudViewportRect(viewportMin, viewportSize);
    if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) {
        return { 0.0f, 0.0f, 1.0f };
    }

    const float ndcX =
        ((reticleScreen_.x - viewportMin.x) / viewportSize.x) * 2.0f - 1.0f;
    const float ndcY =
        1.0f - ((reticleScreen_.y - viewportMin.y) / viewportSize.y) * 2.0f;
    const Math::Matrix4x4 inverseViewProjection =
        Math::Inverse(camera_->GetViewProjectionMatrix());
    const Math::Vector3 nearPoint =
        TransformCoord({ ndcX, ndcY, 0.0f }, inverseViewProjection);
    const Math::Vector3 farPoint =
        TransformCoord({ ndcX, ndcY, 1.0f }, inverseViewProjection);
    const Math::Vector3 cameraRay = Math::Normalize({
        farPoint.x - nearPoint.x,
        farPoint.y - nearPoint.y,
        farPoint.z - nearPoint.z
    });
    if (cameraRay.z <= 0.001f) {
        return { 0.0f, 0.0f, 1.0f };
    }
    // カメラの視線と平行に撃つと、横移動した銃口からの弾道が照準を外れる。
    // 敵が近くにいる場合はその深度、空中は45m先で照準へ収束させる。
    const float aimDepth = lockedEnemy_ && !lockedEnemy_->IsDead() ?
        (std::max)(origin.z + 4.0f, lockedEnemy_->GetAimPosition().z) : origin.z + 45.0f;
    const float rayDistance = (aimDepth - nearPoint.z) / cameraRay.z;
    const Math::Vector3 aimPoint{
        nearPoint.x + cameraRay.x * rayDistance,
        nearPoint.y + cameraRay.y * rayDistance,
        aimDepth };
    return Math::Normalize({ aimPoint.x - origin.x, aimPoint.y - origin.y, aimPoint.z - origin.z });
}

const Enemy* GameRuntime::FindHomingTargetForBullet(const Bullet& bullet) const
{
    const Enemy* bestEnemy = nullptr;
    float bestScore = 42.0f * 42.0f;
    const Math::Vector3 bulletPosition = bullet.GetTranslate();

    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead() || !enemy->IsTargetable()) {
            continue;
        }
        const Math::Vector3 enemyPosition = enemy->GetAimPosition();
        if (enemyPosition.z + 2.0f < bulletPosition.z) {
            continue;
        }
        const float distanceSq = DistanceSquared(bulletPosition, enemyPosition);
        if (distanceSq < bestScore) {
            bestScore = distanceSq;
            bestEnemy = enemy.get();
        }
    }

    return bestEnemy;
}

int GameRuntime::GetTotalEnemyTargetCount() const
{
    return static_cast<int>(GetEnemySchedule(IsTutorial()).size());
}

int GameRuntime::GetRequiredEnemyDefeatsForClear() const
{
    const int totalEnemyCount = GetTotalEnemyTargetCount();
    return (std::max)(1, (totalEnemyCount * 7 + 9) / 10);
}

const Enemy* GameRuntime::GetBossEnemy() const
{
    for (const auto& enemy : enemies_) {
        if (enemy && enemy->IsBoss() && !enemy->IsDead()) {
            return enemy.get();
        }
    }
    return nullptr;
}

void GameRuntime::CheckBulletEnemyCollisions()
{
    for (auto& bullet : playerBullets_) {
        if (bullet->IsDead()) {
            continue;
        }

        // 大きいチャージ弾が複数機に同時接触したとき、リストの先頭ではなく
        // 弾の中心に最も近い機体で爆発させる。編隊中央を狙う操作を尊重する。
        const Enemy* chargeContact = nullptr;
        if (bullet->GetRadius() >= 0.8f && !bullet->IsFeverShot()) {
            float closestDistance = FLT_MAX;
            for (const auto& candidate : enemies_) {
                if (candidate->IsDead() || !candidate->IsTargetable()) { continue; }
                const float distance = DistanceSquared(bullet->GetTranslate(), candidate->GetAimPosition());
                const float contactRadius = bullet->GetRadius() + candidate->GetAimRadius();
                if (distance <= contactRadius * contactRadius && distance < closestDistance) {
                    closestDistance = distance;
                    chargeContact = candidate.get();
                }
            }
        }
        for (auto& enemy : enemies_) {
            if (chargeContact && enemy.get() != chargeContact) { continue; }
            if (enemy->IsDead() || !enemy->IsTargetable()) {
                continue;
            }

            const Math::Vector3 enemyAimPosition = enemy->GetAimPosition();
            const float radius = bullet->GetRadius() + enemy->GetAimRadius();
            if (DistanceSquared(
                bullet->GetTranslate(),
                enemyAimPosition) <= radius * radius) {
                if (!bullet->TryRegisterHitTarget(enemy.get())) {
                    continue;
                }
                const bool isChargedHit = bullet->GetRadius() >= 0.8f;
                const bool isFeverHit = bullet->IsFeverShot();
                const bool isBossHit = enemy->IsBoss();
                const Math::Vector3 bulletImpactPosition = bullet->GetTranslate();
                const float centerEffectRate = isBossHit ? 0.68f : 0.42f;
                const Math::Vector3 visibleImpactPosition{
                    Lerp(bulletImpactPosition.x, enemyAimPosition.x, centerEffectRate),
                    Lerp(bulletImpactPosition.y, enemyAimPosition.y, centerEffectRate),
                    Lerp(bulletImpactPosition.z, enemyAimPosition.z, centerEffectRate)
                };
                bullet->RegisterHit();
                ++playerHitCount_;
                DealPlayerShotDamage(*enemy, visibleImpactPosition, isChargedHit, isFeverHit);
                if (isChargedHit && !isFeverHit) {
                    // 弾の手前側ではなく命中した機体を中心に一度だけ広げる。
                    ApplyChargeSplash(*enemy, enemyAimPosition);
                }
                if (bullet->IsDead()) {
                    break;
                }
            }
        }
    }
}

bool GameRuntime::DealPlayerShotDamage(Enemy& enemy, const Math::Vector3& impactPosition,
    bool charged, bool fever, bool splash)
{
    if (enemy.IsDead() || !enemy.IsTargetable()) { return false; }
    const bool boss = enemy.IsBoss();
    const int baseDamage = splash ? kChargeSplashDamage : (fever ? 4 : (charged ? (boss ? 4 : 3) : 1));
    const int damage = boss && bossCounterTimer_ > 0 ? baseDamage * 2 : baseDamage;
    if (fever) {
        AddFeverEnemyImpactEffect(impactPosition, boss ? 1.75f : 1.38f);
    } else {
        AddEnemyImpactEffect(impactPosition, boss ? 1.85f : (charged ? 1.42f : 1.20f));
    }
    const bool destroyed = enemy.Damage(damage);
    if (IsTutorial() && tutorial_.lesson == TutorialLesson::Charge && charged && !splash) {
        tutorial_.chargedHit = true;
    }
    RecoverPhantomRaidOnHit(charged, destroyed);
    AddFeverGauge(destroyed ? (boss ? 25 : (charged ? 16 : 13)) : (charged ? 4 : 2));
    // 音・照準表示・画面揺れは直撃で一回。巻き込み数ぶん画面を揺らさない。
    if (!splash) {
        PlaySfx(destroyed ? "destroy" : "hit");
        TriggerHitConfirm(impactPosition, charged, boss, destroyed);
        AddCameraShake(destroyed ? (boss ? 0.18f : (charged ? 0.075f : 0.055f)) :
            (boss ? 0.035f : 0.022f), destroyed ? (boss ? 30 : (charged ? 11 : 8)) : (boss ? 6 : 4));
        if (!fever || boss || destroyed) {
            TriggerPlayerImpactMoment(charged, boss, destroyed);
        }
    }
    if (destroyed) { OnEnemyDestroyed(enemy, charged, fever); }
    return destroyed;
}

void GameRuntime::ApplyChargeSplash(const Enemy& directTarget, const Math::Vector3& center)
{
    HitEffect effect{};
    effect.worldPosition = center;
    effect.duration = 22;
    effect.type = HitEffectType::EnemyImpact;
    // 薄い衝撃波だけを広げる。白い面で敵弾や後ろの狙撃機を隠さない。
    AddHitEffectVisual(effect, effectCombatShockModel_, center,
        { 0.86f, 0.62f, 1.65f, 0.88f }, 0.85f, kChargeSplashRadius / 0.85f - 1.0f,
        0.0f, 0.0f, 1.4f, 0.78f, {}, true, 0.64f, -0.15f);
    AddHitEffectVisual(effect, effectImpactBurstModel_, center,
        { 1.60f, 1.40f, 1.85f, 0.96f }, 1.20f, -0.55f,
        0.0f, 0.0f, 3.20f, 0.25f, {}, true, 0.40f);
    if (effect.visualCount > 0) { hitEffects_.push_back(std::move(effect)); }

    bool destroyedAny = false;
    for (auto& nearby : enemies_) {
        if (nearby.get() == &directTarget || nearby->IsDead() || !nearby->IsTargetable()) { continue; }
        const auto position = nearby->GetAimPosition();
        // ワールド空間の球判定。画面上で重なるだけの遠い敵には届かない。
        if (DistanceSquared(center, position) > kChargeSplashRadius * kChargeSplashRadius) { continue; }
        destroyedAny |= DealPlayerShotDamage(*nearby, position, true, false, true);
    }
    if (destroyedAny) { TriggerPlayerImpactMoment(true, false, true); }
}

void GameRuntime::OnEnemyDestroyed(Enemy& enemy, bool charged, bool fever)
{
    // 弾とスキルで撃破数・報酬・ボスクリア処理が食い違わないよう共有する。
    const bool boss = enemy.IsBoss();
    const Math::Vector3 position = enemy.GetAimPosition();
    if (enemy.IsRiftCarrier() && !boss && !IsTutorial()) { SpawnRift(position); }
    RegisterEnemyDefeatChain();
    if (fever) {
        AddFeverEnemyHitEffect(position, boss ? 1.72f : 1.08f);
    } else {
        AddEnemyHitEffect(position, boss ? 2.0f : (charged ? 1.32f : 1.0f));
    }
    SpawnRewardHearts(position, boss ? 14 : (charged ? 6 : 4));
    AddScore(boss ? 1500 : 100);
    ++defeatedEnemyCount_;
    ++defeatedEnemyCountInWave_;
    if (boss) {
        bossDefeatFlashTimer_ = kBossDefeatFlashDuration;
        bossDefeatPosition_ = position;
        bossCounterTimer_ = 0;
        bossAttackStep_ = -1;
        // 撃破後に残弾だけが画面へ迫らないよう、確保済みプールへ返す。
        while (!enemyBullets_.empty()) {
            justDodgedEnemyBullets_.erase(enemyBullets_.front().get());
            enemyBulletPool_.push_back(std::move(enemyBullets_.front()));
            enemyBullets_.pop_front();
        }
        AddCameraShake(0.22f, 18);
        bossDefeated_ = true;
        currentWaveIndex_ = kWaveCount - 1;
        isGameClear_ = true;
        resultTransitionTimer_ = 120;
        stageCombatBeatName_ = "Boss destroyed";
    }
}

void GameRuntime::CheckEnemyBulletPlayerCollisions()
{
    if (!player_ || player_->IsDead()) {
        return;
    }
    // 技の攻撃区間だけ保護。ここではジャスト回避を発生させず、自己再充填を防ぐ。
    if (IsPhantomRaidActive() && phantomClock_ < PhantomFinisherTime() + 7.0f) {
        return;
    }

    for (auto& bullet : enemyBullets_) {
        if (bullet->IsDead()) {
            continue;
        }

        const float radius = bullet->GetRadius() + player_->GetRadius();
        const Math::Vector3 bulletPosition = bullet->GetTranslate();
        const float distanceSq = DistanceSquared(
            bulletPosition,
            player_->GetTranslate());
        if (player_->IsDodging()) {
            const float justDodgeRadius = radius + kJustDodgeGrazePadding;
            if (distanceSq > radius * radius &&
                distanceSq <= justDodgeRadius * justDodgeRadius) {
                TriggerJustDodge(*bullet, bulletPosition);
                continue;
            }
        }
        if (distanceSq <= radius * radius) {
            if (player_->IsDodging()) {
                bullet->Kill();
                AddPlayerDodgeGrazeEffect(bulletPosition);
                AddCameraShake(0.018f, 4);
                continue;
            }
            // Player側の無敵フラグも尊重し、保護中の弾でチェインを切らない。
            if (player_->IsInvincible()) {
                bullet->Kill();
                continue;
            }
            const Math::Vector3 incomingVelocity = bullet->GetVelocity();
            const int incomingDamage = bullet->GetDamage();
            bullet->Kill();
            ++playerDamageCount_;
            PlaySfx("damage");
            BreakEnemyDefeatChain();
            const int damage = feverTimer_ > 0 ? (std::max)(1, (incomingDamage + 1) / 2) : incomingDamage;
            // 練習では被弾の反応は残し、HP1で止めて何度でも試せるようにする。
            player_->Damage(IsTutorial() ? (std::min)(damage, (std::max)(0, player_->GetHp() - 1)) : damage);
            if (feverTimer_ <= 0) {
                feverGauge_ = (std::max)(0, feverGauge_ - 20);
            }
            AddPlayerDamageEffect(player_->GetTranslate());
            TriggerPlayerDamageFeedback(
                player_->GetTranslate(),
                incomingVelocity);
            AddCameraShake(0.2f, 18);
            if (player_->IsDead() && !isGameOver_) {
                isGameOver_ = true;
                resultTransitionTimer_ = 90;
            }
            break;
        }
    }
}

void GameRuntime::UpdateFlightCamera()
{
    const float frameStep = dxCommon_ ?
        std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.1f, 3.0f) : 1.0f;
    cameraTimer_ += frameStep;
    const auto position = player_->GetTranslate();
    const float moveX = std::clamp(
        (position.x - previousPlayerTranslate_.x) / (0.205f * frameStep), -1.0f, 1.0f);
    const float moveY = std::clamp(
        (position.y - previousPlayerTranslate_.y) / (0.205f * frameStep), -1.0f, 1.0f);
    // レール移動と慣性を分離。高速時でもカメラが際限なく取り残されない。
    cameraTranslate_.z += position.z - previousPlayerTranslate_.z;
    previousPlayerTranslate_ = position;
    auto& motion = flightCameraMotion_;
    const float worldScale = GetCinematicWorldTimeScale();
    const float effectiveSpeed = railSpeed_ * worldScale;
    const float force = std::clamp(
        (effectiveSpeed - motion.previousSpeed) * 18.0f / frameStep, -1.0f, 1.0f);
    motion.previousSpeed = effectiveSpeed;
    motion.acceleration = Lerp(motion.acceleration, force, 1.0f - std::pow(0.82f, frameStep));
    const bool isFlying = !isGameOver_ && !isGameClear_;
    const float speed = GetFlightSpeedRate();
    const float fever = feverSpeedEffectRate_;
    const float dodge = player_->IsDodging() && isFlying ?
        static_cast<float>(player_->GetDodgeDirection()) : 0.0f;
    const float justDodge = static_cast<float>(justDodgeFlashTimer_) / kJustDodgeFlashDuration;
    const float justDodgePush = justDodge * justDodge;
    const float burst = isFlying ? std::clamp(
        flightReleaseKick_ * 0.75f + (std::max)(motion.acceleration, 0.0f) * 2.5f, 0.0f, 1.0f) : 0.0f;
    const float controlX = isFlying ? moveX : 0.0f;
    const float controlY = isFlying ? moveY : 0.0f;

    // ばね＋減衰で追従。一定周期の揺れではなく、操作の開始・反転・停止で余韻が生まれる。
    // 小分けに積分して低fps時も安定させる（固定回数、確保なし）。
    const auto spring = [frameStep](float& value, float& velocity, float target, float frequency, float damping) {
        const int steps = (std::max)(1, static_cast<int>(std::ceil(frameStep / 0.5f)));
        const float step = frameStep / static_cast<float>(steps);
        for (int index = 0; index < steps; ++index) {
            velocity += ((target - value) * frequency * frequency -
                velocity * (2.0f * damping * frequency)) * step;
            value += velocity * step;
        }
    };
    const Math::Vector3 targetPosition{
        position.x * 0.28f - controlX * 0.30f,
        2.10f + position.y * 0.32f - controlY * 0.18f - motion.acceleration * 0.20f,
        railDistance_ - (kRushCameraDistance + speed * 0.65f + fever * 1.05f +
            motion.acceleration * 1.40f + burst * 0.60f) + justDodgePush * 1.25f,
    };
    spring(cameraTranslate_.x, motion.translationVelocity.x, targetPosition.x, 0.16f, 0.86f);
    spring(cameraTranslate_.y, motion.translationVelocity.y, targetPosition.y, 0.15f, 0.84f);
    spring(cameraTranslate_.z, motion.translationVelocity.z, targetPosition.z, 0.18f, 0.82f);
    const Math::Vector3 targetRotation{
        0.060f - position.y * 0.025f - controlY * 0.048f + motion.acceleration * 0.030f,
        -position.x * 0.006f + controlX * 0.012f + stageCameraYawBias_ * 0.35f,
        std::clamp(-controlX * 0.075f - dodge * 0.045f - position.x * 0.0025f +
            stageCameraRollBias_ * 0.55f, -0.15f, 0.15f),
    };
    spring(cameraRotate_.x, motion.rotationVelocity.x, targetRotation.x, 0.13f, 0.82f);
    spring(cameraRotate_.y, motion.rotationVelocity.y, targetRotation.y, 0.14f, 0.88f);
    spring(cameraRotate_.z, motion.rotationVelocity.z, targetRotation.z, 0.13f, 0.76f);
    const float targetFov = kRushCameraBaseFovY + speed * 0.035f + fever * 0.110f +
        burst * 0.065f + std::abs(dodge) * 0.020f - justDodgePush * 0.025f + stageCameraFovBoost_;
    spring(cameraFovY_, motion.fovVelocity, targetFov, 0.16f, 0.88f);

    const float targetBlur = isFlying ? std::clamp(
        (speed * 0.14f + fever * 0.35f + burst * 0.48f + std::abs(dodge) * 0.20f) * worldScale,
        0.0f, 0.85f) : 0.0f;
    motion.blurStrength = Lerp(motion.blurStrength, targetBlur,
        1.0f - std::pow(targetBlur > motion.blurStrength ? 0.70f : 0.88f, frameStep));

    float impact = 0.0f;
    if (cameraShakeTimer_ > 0) {
        impact = cameraShakePower_ * static_cast<float>(cameraShakeTimer_) /
            static_cast<float>((std::max)(cameraShakeDuration_, 1));
        --cameraShakeTimer_;
    } else { cameraShakePower_ = 0.0f; }
    // 微振動は最終表示だけへ加える。ばね状態に蓄積させず、通常飛行は1px前後に抑える。
    const float wind = isFlying ? (speed * 0.35f + fever * 0.45f + burst * 0.30f) * worldScale : 0.0f;
    Math::Vector3 displayPosition = cameraTranslate_;
    Math::Vector3 displayRotation = cameraRotate_;
    displayPosition.x += std::sin(cameraTimer_ * 1.9f) * (impact + wind * 0.012f);
    displayPosition.y += std::cos(cameraTimer_ * 2.3f) * (impact + wind * 0.008f);
    displayRotation.z += std::sin(cameraTimer_ * 1.37f) * wind * 0.0009f;
    camera_->SetTranslate(displayPosition);
    camera_->SetRotate(displayRotation);
    camera_->SetFovY(cameraFovY_);
    camera_->Update();
}

void GameRuntime::UpdateGameCamera()
{
    if (!camera_ || !player_) {
        return;
    }
    if (!IsTutorial()) {
        UpdateFlightCamera();
        return;
    }

    const float frameStep =
        dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 2.0f) : 1.0f;
    cameraTimer_ += frameStep;

    const Math::Vector3 playerTranslate = player_->GetTranslate();
    if (!IsTutorial()) {
        // 前進分を先に追従し、距離だけ補間する。高速時の遅れで自機が縮むのを防ぐ。
        cameraTranslate_.z += playerTranslate.z - previousPlayerTranslate_.z;
    }
    const Math::Vector3 playerVelocity = {
        playerTranslate.x - previousPlayerTranslate_.x,
        playerTranslate.y - previousPlayerTranslate_.y,
        0.0f,
    };
    previousPlayerTranslate_ = playerTranslate;

    float shakeRate = 0.0f;
    if (cameraShakeTimer_ > 0) {
        shakeRate =
            static_cast<float>(cameraShakeTimer_) /
            static_cast<float>((std::max)(cameraShakeDuration_, 1));
        --cameraShakeTimer_;
    } else {
        cameraShakePower_ = 0.0f;
    }

    const float bob = std::sin(cameraTimer_ * 0.045f) * 0.08f;
    const float railCurve =
        std::sin(railDistance_ * kRailCameraCurveFrequency);
    const float railDrift =
        std::sin(railDistance_ * kRailCameraDriftFrequency + 1.35f);
    const float railLift =
        std::sin(railDistance_ * 0.034f + 0.45f) * 0.18f;
    const float railWideCurve =
        railCurve * 0.32f +
        railDrift * 0.10f +
        std::sin(railDistance_ * 0.014f + 2.20f) * 0.14f;
    const float turnRate = std::clamp(railWideCurve, -0.65f, 0.65f);
    const float speedPulse =
        0.5f + 0.5f * std::sin(railDistance_ * 0.021f + 0.80f);
    const float railSpeedRate = GetFlightSpeedRate();
    const float feverSpeedRate = feverSpeedEffectRate_;
    const float feverActivationElapsedFrames =
        static_cast<float>(kFeverActivationFlashFrames - feverActivationFlashTimer_);
    const float feverAccelerationKickPhase =
        std::clamp(feverActivationElapsedFrames / 28.0f, 0.0f, 1.0f);
    const float feverAccelerationKick =
        feverActivationFlashTimer_ > 0 ?
        std::sin(feverAccelerationKickPhase * std::numbers::pi_v<float>) : 0.0f;
    const bool isPlayerDodging = player_->IsDodging();
    const float justDodgeCameraRate =
        justDodgeFlashTimer_ > 0 ?
        static_cast<float>(justDodgeFlashTimer_) /
            static_cast<float>((std::max)(kJustDodgeFlashDuration, 1)) :
        0.0f;
    const float justDodgeImpulse =
        justDodgeCameraRate > 0.0f ?
        std::sin((1.0f - justDodgeCameraRate) * std::numbers::pi_v<float>) :
        0.0f;
    const float justDodgeWhip = justDodgeCameraRate * justDodgeCameraRate;
    const float justDodgeRipple =
        justDodgeCameraRate > 0.0f ?
        std::sin((1.0f - justDodgeCameraRate) * std::numbers::pi_v<float> * 2.0f) *
            justDodgeCameraRate :
        0.0f;
    const float justDodgeChromaticShift =
        justDodgeImpulse * (0.35f + justDodgeCameraRate * 0.65f);
    const float playerImpactCameraRate =
        playerImpactFlashTimer_ > 0 ?
        static_cast<float>(playerImpactFlashTimer_) /
            static_cast<float>((std::max)(playerImpactFlashDuration_, 1)) :
        0.0f;
    const float edgeRollRate = std::clamp(
        (std::abs(playerTranslate.x) - kGameplayCameraEdgeRollStart) /
            kGameplayCameraEdgeRollRange,
        0.0f,
        1.0f);
    const float smoothEdgeRollRate =
        edgeRollRate * edgeRollRate * (3.0f - 2.0f * edgeRollRate);
    const float edgeDirection =
        playerTranslate.x > 0.0f ? 1.0f :
        playerTranslate.x < 0.0f ? -1.0f :
        0.0f;
    const float playerEdgeRoll =
        edgeDirection * smoothEdgeRollRate * kGameplayCameraEdgeRollMax *
        (1.0f - justDodgeWhip * 0.92f);
    const float dodgeLean =
        isPlayerDodging ?
        static_cast<float>(player_->GetDodgeDirection() >= 0 ? 1 : -1) :
        0.0f;
    const float inputSpeed = std::clamp(
        std::abs(playerVelocity.x) * 4.2f +
            std::abs(playerVelocity.y) * 2.8f,
        0.0f,
        1.0f);
    const float shakeX = std::sin(cameraTimer_ * 1.9f) * cameraShakePower_ * shakeRate;
    const float shakeY = std::cos(cameraTimer_ * 2.3f) * cameraShakePower_ * shakeRate;
    const float feverWindBuffetX = feverSpeedRate * (
        std::sin(cameraTimer_ * 0.21f) * 0.040f +
        std::sin(cameraTimer_ * 0.53f + 1.20f) * 0.016f);
    const float feverWindBuffetY = feverSpeedRate * (
        std::sin(cameraTimer_ * 0.27f + 0.70f) * 0.024f +
        std::sin(cameraTimer_ * 0.61f) * 0.010f);
    const float resultZoom =
        isGameClear_ ? 0.9f :
        isGameOver_ ? -0.8f :
        0.0f;

    const Math::Vector3 targetTranslate = {
            playerTranslate.x * (0.08f + justDodgeWhip * kJustDodgeCameraPlayerFollowX) +
            playerVelocity.x * 0.55f +
            dodgeLean * 0.18f +
            dodgeLean * (justDodgeWhip * 0.78f + justDodgeRipple * 0.22f) +
            std::sin(cameraTimer_ * 3.7f) * justDodgeChromaticShift * 0.085f +
            railWideCurve * 0.22f +
            railDrift * 0.04f +
            feverWindBuffetX +
            shakeX,
        (IsTutorial() ? 2.65f : 2.10f) +
            playerTranslate.y * (0.04f + justDodgeWhip * kJustDodgeCameraPlayerFollowY) +
            playerVelocity.y * 0.38f +
            railLift +
            stageCameraLiftBias_ +
            std::sin(railDistance_ * 0.017f + 0.35f) * 0.16f +
            justDodgeImpulse * 0.24f +
            justDodgeWhip * (0.08f - kJustDodgeCameraLowAngle) +
            feverSpeedRate * -0.12f +
            bob +
            feverWindBuffetY +
            shakeY,
            railDistance_ -
            ((IsTutorial() ? kGameplayCameraBaseDistance : kRushCameraDistance) +
                speedPulse * 0.55f +
                railSpeedRate * 0.46f +
                feverSpeedRate * (1.15f + speedPulse * 0.25f) +
                feverAccelerationKick * 0.78f +
                inputSpeed * 0.22f +
            std::abs(turnRate) * 0.18f) +
            resultZoom +
            justDodgeWhip * kJustDodgeCameraClosePushIn +
            justDodgeImpulse * 0.36f +
            playerImpactCameraRate * playerImpactCameraRate * 0.42f,
    };

    const float cameraTranslateRate =
        0.065f +
        justDodgeCameraRate * 0.105f +
        playerImpactCameraRate * 0.020f +
        feverSpeedRate * 0.035f +
        feverAccelerationKick * 0.025f;
    cameraTranslate_ = Lerp(cameraTranslate_, targetTranslate, cameraTranslateRate);

    const Math::Vector3 targetRotate = {
        0.065f +
            playerTranslate.y * 0.003f +
            railLift * 0.005f +
            speedPulse * 0.004f +
            bob * 0.006f +
            feverWindBuffetY * 0.012f +
            shakeY * 0.008f,
        -playerTranslate.x * 0.0045f +
            railWideCurve * 0.018f +
            railDrift * 0.004f +
            stageCameraYawBias_ * 0.35f +
            playerVelocity.x * 0.006f,
        stageCameraRollBias_ * 0.55f -
            turnRate * 0.024f +
            (IsTutorial() ? 0.0f : -std::clamp(playerVelocity.x * 0.045f, -0.045f, 0.045f)) +
            playerEdgeRoll +
            playerImpactCameraRate * 0.010f +
            feverWindBuffetX * 0.020f +
            shakeX * 0.006f,
    };

    const float targetFov =
        (IsTutorial() ? kGameplayCameraBaseFovY : kRushCameraBaseFovY) +
        speedPulse * 0.040f +
        railSpeedRate * 0.035f +
        std::abs(turnRate) * 0.020f +
        inputSpeed * 0.025f +
        (isPlayerDodging ? 0.024f : 0.0f) +
        playerImpactCameraRate * 0.030f -
        justDodgeWhip * (IsTutorial() ? kJustDodgeCameraFovTighten : 0.055f) -
        justDodgeImpulse * 0.026f +
        feverSpeedRate *
            (0.115f + std::sin(cameraTimer_ * 0.11f) * 0.012f) +
        feverAccelerationKick * 0.060f +
        flightReleaseKick_ * 0.065f +
        stageCameraFovBoost_ +
        (isGameClear_ ? -0.035f : 0.0f) +
        (isGameOver_ ? 0.025f : 0.0f) +
        cameraShakePower_ * shakeRate * 0.04f;

    cameraRotate_ = Lerp(cameraRotate_, targetRotate, 0.075f + justDodgeCameraRate * 0.082f);
    cameraFovY_ = Lerp(
        cameraFovY_,
        targetFov,
        0.060f + justDodgeCameraRate * 0.080f + feverSpeedRate * 0.040f);

    camera_->SetTranslate(cameraTranslate_);
    camera_->SetRotate(cameraRotate_);
    camera_->SetFovY(cameraFovY_);
    camera_->Update();
}

void GameRuntime::AddCameraShake(float power, int duration)
{
    cameraShakePower_ = (std::max)(cameraShakePower_, power);
    cameraShakeDuration_ = (std::max)(duration, 1);
    cameraShakeTimer_ = (std::max)(cameraShakeTimer_, cameraShakeDuration_);
}
