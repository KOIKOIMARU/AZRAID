#include "app/runtime/GameplaySceneSetup.h"

#include "app/presentation/ClearResultPresentation.h"
#include "app/combat/CombatEffects.h"
#include "app/presentation/FlightSpeedEffects.h"
#include "app/runtime/GameResources.h"
#include "app/runtime/GameRuntimeTuning.h"
#include "app/presentation/GameplayAudio.h"
#include "app/presentation/GameplayCamera.h"
#include "app/presentation/GameplayShadows.h"
#include "app/player/Player.h"
#include "app/player/PlayerControl.h"
#include "app/player/PlayerDefeatPresentation.h"
#include "app/player/PlayerFlightEffects.h"
#include "app/player/PlayerSkill.h"
#include "app/player/PlayerSkillPresentation.h"
#include "app/combat/ProjectileSystem.h"
#include "app/stage/RailScenery.h"
#include "app/stage/RingGates.h"
#include "app/runtime/RuntimeSceneObjects.h"
#include "app/enemy/StageEnemies.h"
#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Skybox.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"

#include <algorithm>
#include <array>
#include <chrono>

namespace {
using namespace GameRuntimeTuning;
using namespace GameResources;

GameplaySceneSetup::Timing gTiming{};

struct StepTimer {
    const char* label;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    // GPUリソースは作成途中で止められない。物体を一つ準備した時点で次の作業を判断する。
    bool BudgetUsed() const {
        return std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(6);
    }

    // 工程の開始時に時刻を控え、工程終了時の計測に使う。
    ~StepTimer() {
        const float elapsed = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > gTiming.workMs) { gTiming = { label, elapsed }; }
    }
};

Model* FindModel(const char* name)
{
    return ModelManager::GetInstance()->FindModel(name);
}

bool BulletPoolsReady(const ProjectileSystem& projectiles)
{
    return projectiles.PlayerPool().AvailableCount() >= kTargetPlayerBulletPoolCount &&
        projectiles.EnemyPool().AvailableCount() >= kTargetEnemyBulletPoolCount;
}
} // namespace

void GameplaySceneSetup::Reset()
{
    step_ = Step::Renderer;
    sceneryItem_ = 0;
}

void GameplaySceneSetup::ResetTiming() { gTiming = {}; }
GameplaySceneSetup::Timing GameplaySceneSetup::GetTiming() { return gTiming; }

// 現在工程と、弾・エフェクト・音声など工程内の準備状況をロード進捗に換算する。
float GameplaySceneSetup::Progress(const ProjectileSystem& projectiles,
    const CombatEffects& effects, const GameplayAudio& audio) const
{
    float fraction = 0.0f;
    if (step_ == Step::Bullets) {
        fraction = static_cast<float>(projectiles.PlayerPool().AvailableCount() + projectiles.EnemyPool().AvailableCount()) /
            static_cast<float>(kTargetPlayerBulletPoolCount + kTargetEnemyBulletPoolCount);
    } else if (step_ == Step::CombatEffects) {
        fraction = static_cast<float>(effects.AvailableCount()) / static_cast<float>(kTargetHitEffectObjectPoolCount);
    } else if (step_ == Step::Audio) {
        fraction = audio.LoadProgress();
    }
    return std::clamp((static_cast<float>(step_) + std::clamp(fraction, 0.0f, 1.0f)) /
        static_cast<float>(Step::Complete), 0.0f, 1.0f);
}

// 準備を段階ごとに進める。共通描画、ステージ、弾やエフェクトの再利用枠、音声がそろうと出撃できる。
bool GameplaySceneSetup::Advance(const Targets& targets, bool tutorial, float railDistance, float enemyBulletSpeed)
{
    if (IsReady()) { return true; }

    constexpr std::array labels{ "Renderer and player", "Bullet pools", "Hit effect pool", "Scenery batch",
        "Depth cues", "Scene objects", "Scene ready", "Dodge afterimages", "Engine plumes", "Charge lights",
        "Contact shadows", "Phantom raid", "Rifts", "Result presentation", "Audio", "Final setup" };
    static_assert(labels.size() == static_cast<std::size_t>(Step::Complete));
    const StepTimer timer{ step_ == Step::Audio ? targets.audio.LoadLabel() : labels[static_cast<std::size_t>(step_)] };

    switch (step_) {
    case Step::Renderer:
        if (!PrepareRenderer(targets, tutorial, railDistance)) { return false; }
        break;
    case Step::Bullets:
        // 出撃直後に大量のGPUリソースを作らないよう、通常の予備数まで先に確保する。
        for (int index = 0; index < 8; ++index) {
            if (targets.projectiles.PlayerPool().AvailableCount() < kTargetPlayerBulletPoolCount) {
                targets.projectiles.PreparePlayerBullet(targets.controls.GetShotTuning().normalSpeed);
            } else if (targets.projectiles.EnemyPool().AvailableCount() < kTargetEnemyBulletPoolCount) {
                targets.projectiles.PrepareEnemyBullet(enemyBulletSpeed);
            } else {
                break;
            }
            if (timer.BudgetUsed()) { break; }
        }
        if (!BulletPoolsReady(targets.projectiles)) { return false; }
        break;
    case Step::CombatEffects:
        for (int index = 0; index < 24 && targets.combatEffects.AvailableCount() < kTargetHitEffectObjectPoolCount; ++index) {
            targets.combatEffects.PrepareOne();
            if (timer.BudgetUsed()) { break; }
        }
        if (targets.combatEffects.AvailableCount() < kTargetHitEffectObjectPoolCount) { return false; }
        break;
    case Step::Scenery: {
        bool ready = false;
        for (int index = 0; index < 24; ++index) {
            ready = targets.scenery.Prepare(targets.common.get(), targets.camera.get(), sceneryItem_++, 1);
            if (ready || timer.BudgetUsed()) { break; }
        }
        if (!ready) { return false; }
        break;
    }
    case Step::DepthCues:
        targets.speedEffects.Initialize(targets.common.get(), {
            FindModel("effect_spark_star"), FindModel("effect_glow_core"),
            FindModel("effect_player_bullet_trail"), FindModel("effect_bullet_trail") }, railDistance);
        break;
    case Step::SceneObjects:
        LoadSceneObjects(targets, RuntimeSceneObjects::kDefaultPath, tutorial);
        break;
    case Step::SceneReady:
        break;
    case Step::Afterimages:
        targets.playerEffects.Initialize(targets.common.get(), targets.camera.get(), {
            FindModel("effect_glow_core"), FindModel("effect_player_bullet_trail"), FindModel("effect_player_bullet_core"),
            FindModel("effect_player_engine_plume"), FindModel("effect_combat_arc"), FindModel("effect_spark_star"), playerModel_ });
        targets.playerEffects.PrepareAfterimages();
        break;
    case Step::Exhaust:
        targets.playerEffects.PrepareExhaust();
        break;
    case Step::ChargeLights:
        targets.playerEffects.PrepareCharge();
        break;
    case Step::ContactShadows:
        targets.shadows.PrepareContacts(*targets.common, FindModel("effect_contact_shadow"));
        break;
    case Step::Skill:
        targets.skill.Reset();
        targets.skillPresentation.Reset();
        if (playerModel_ && targets.common) {
            targets.skillPresentation.Initialize(*targets.common, *playerModel_);
        }
        break;
    case Step::Rings:
        targets.rings.Reset();
        targets.rings.Initialize(targets.common.get(), FindModel("boost_gate_frame"),
            FindModel("boost_gate_light"), FindModel("boost_gate_rear"), FindModel("boost_gate_flow"));
        break;
    case Step::Results:
        targets.clearPresentation.Initialize(*targets.common, playerModel_,
            FindModel("effect_player_bullet_trail"), FindModel("effect_player_bullet_core"));
        targets.defeatPresentation.Initialize(*targets.common, *targets.camera,
            { playerModel_, FindModel("effect_combat_blade"), FindModel("effect_explosion_smoke"),
              FindModel("effect_explosion_fireball"), FindModel("effect_glow_core") }, targets.player->GetVisualScale());
        break;
    case Step::Audio:
        if (!targets.audio.LoadStep()) { return false; }
        break;
    case Step::FinalSetup:
        targets.player->SetRailZ(railDistance);
        targets.scenery.Update(railDistance);
        break;
    case Step::Complete:
        return true;
    }

    step_ = static_cast<Step>(static_cast<int>(step_) + 1);
    return IsReady();
}

// カメラ、3D描画の共通設定、自機を用意し、ステージを描ける状態にする。
bool GameplaySceneSetup::PrepareRenderer(const Targets& targets, bool tutorial, float railDistance)
{
    if (!PreloadStep(targets.dx, targets.srv)) { return false; }
    if (!targets.common) {
        targets.common = std::make_unique<Object3dCommon>();
        targets.common->BeginInitialize(targets.dx, targets.srv);
        return false;
    }
    if (!targets.common->InitializeStep()) { return false; }
    targets.common->SetEnvironmentTexturePath(kGameEnvironmentTexturePath);

    playerModel_ = FindModel(kFreePlayerModelPath);
    if (!playerModel_) { playerModel_ = FindModel("game_player"); }
    targets.enemies.Prepare(*targets.common, *ModelManager::GetInstance());

    targets.camera = std::make_unique<Camera>();
    targets.camera->SetRotate({ 0.06f, 0.0f, 0.0f });
    targets.camera->SetTranslate({ 0.0f, 2.65f, -kGameplayCameraInitialDistance });
    targets.camera->SetFovY(kGameplayCameraBaseFovY);
    targets.camera->SetAspectRatio(targets.dx->GetPresentationAspectRatio());
    targets.camera->SetFarClip(520.0f);
    targets.camera->Update();
    targets.common->SetDefaultCamera(targets.camera.get());

    targets.skybox = std::make_unique<Skybox>();
    targets.skybox->Initialize(targets.dx, targets.srv, kGameEnvironmentTexturePath);
    targets.skybox->Update(targets.camera.get());

    targets.player = std::make_unique<Player>();
    targets.player->Initialize(targets.common.get(), playerModel_);
    targets.player->SetRailZ(railDistance);
    const auto& position = targets.player->GetTranslate();
    const GameplayCamera::Pose initialPose{
        { position.x * 0.10f,
            (tutorial ? 2.70f : 2.10f) + position.y * 0.05f,
            railDistance - (tutorial ? kGameplayCameraInitialDistance : kRushCameraDistance) },
        { 0.065f + position.y * 0.003f, -position.x * 0.004f, 0.0f },
        tutorial ? kGameplayCameraBaseFovY : kRushCameraBaseFovY };
    targets.cameraMotion.AdoptPose(initialPose);
    targets.cameraMotion.SynchronizePlayer(position, 0.0f);
    targets.camera->SetTranslate(initialPose.position);
    targets.camera->SetRotate(initialPose.rotation);
    targets.camera->SetFovY(initialPose.fov);
    targets.camera->Update();
    targets.skybox->Update(targets.camera.get());

    targets.projectiles.Initialize(targets.common.get(), {
        .body = FindModel("game_bullet"),
        .chargeBody = FindModel("game_charge_lance"),
        .bulletGlow = FindModel("effect_bullet_glow"),
        .fallbackGlow = FindModel("effect_glow_core"),
        .playerCore = FindModel("effect_player_bullet_core"),
        .playerTrail = FindModel("effect_player_bullet_trail"),
        .chargeCore = FindModel("effect_player_charge_core"),
        .chargeTrail = FindModel("effect_player_charge_trail"),
        .enemyCore = FindModel("effect_enemy_bullet_core"),
        .enemyTrail = FindModel("effect_enemy_bullet_tail"),
    });
    targets.combatEffects.Initialize(targets.common.get(), targets.camera.get(), {
        .glowCore = FindModel("effect_glow_core"),
        .glowRing = FindModel("effect_glow_ring"),
        .sparkStar = FindModel("effect_spark_star"),
        .impactBurst = FindModel("effect_impact_burst"),
        .fireball = FindModel("effect_explosion_fireball"),
        .smoke = FindModel("effect_explosion_smoke"),
        .sparks = FindModel("effect_explosion_sparks"),
        .blade = FindModel("effect_combat_blade"),
        .shock = FindModel("effect_combat_shock"),
        .arc = FindModel("effect_combat_arc"),
        .disc = FindModel("effect_combat_disc"),
        .magicShard = FindModel("effect_magic_shard"),
    });
    return true;
}

// 配置ファイルを読み込み、使う場面に応じて結果と表示用の状態を反映する。
bool GameplaySceneSetup::LoadSceneObjects(const Targets& targets, const char* path, bool tutorial)
{
    SceneSerializer::SceneSettings settings{};
    if (!targets.common || !targets.sceneObjects.Load(path, *targets.common, settings)) {
        targets.sceneObjectStatus = "読み込み失敗: " + std::string(path ? path : "");
        return false;
    }

    if (settings.hasCamera && targets.camera) {
        targets.cameraMotion.AdoptPose({ settings.cameraTranslate, settings.cameraRotate,
            tutorial ? kGameplayCameraBaseFovY : kRushCameraBaseFovY });
        targets.camera->SetTranslate(settings.cameraTranslate);
        targets.camera->SetRotate(settings.cameraRotate);
        targets.camera->SetFovY(targets.cameraMotion.FollowPose().fov);
    }
    // 読み込んだカメラの位置を反映してから描画用の行列を更新する。
    targets.sceneObjects.Update();
    targets.sceneObjectStatus = "読み込みました: " + targets.sceneObjects.Path();
    return true;
}

// シーン終了時に、描画オブジェクトと各システムのシーン用リソースを解放する。
void GameplaySceneSetup::Release(const Targets& targets)
{
    targets.rings.Clear();
    targets.skillPresentation.Clear();
    targets.skill.Reset();
    targets.skillPresentation.Reset();
    targets.audio.Clear();
    targets.sceneObjects.Clear();
    targets.playerEffects.Clear();
    targets.shadows.Clear();
    targets.projectiles.Clear();
    targets.controls.Aim().ClearTarget();
    targets.enemies.Clear({ targets.skill, targets.projectiles, targets.controls.Aim() });
    targets.combatEffects.Clear();
    targets.scenery.Clear();
    targets.speedEffects.Clear();
    targets.player.reset();
    targets.defeatPresentation.Clear();
    targets.clearPresentation.Clear();
    targets.camera.reset();
    targets.skybox.reset();
    // 共通描画オブジェクトの破棄前に、シーンが確保したシャドウのSRVを返す。
    if (targets.common && targets.srv && targets.common->GetShadowMapSrvIndex() != UINT32_MAX) {
        targets.srv->Free(targets.common->GetShadowMapSrvIndex());
    }
    targets.common.reset();
    playerModel_ = nullptr;
}
