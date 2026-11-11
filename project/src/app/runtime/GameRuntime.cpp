// ===== GameRuntime.cpp =====
// ゲームプレイを初期化し、各フレームでステージ・自機・敵を更新して描画へつなぐ。

#include "app/runtime/GameRuntime.h"
#include "app/runtime/GameResources.h"
#include "app/runtime/GameRuntimeTuning.h"
#include "app/stage/SceneryAssets.h"
#include "app/ui/MenuUi.h"
#include "app/ui/CombatHud.h"
#include "app/combat/CombatOverlay.h"

#include "engine/3d/Skybox.h"
#include "engine/3d/ScreenProjection.h"
#include "engine/base/DirectXCommon.h"
#include "engine/io/Input.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>

namespace {
using namespace GameRuntimeTuning;
using namespace SceneryAssets;
} // namespace

GameRuntime::GameRuntime() = default;
GameRuntime::~GameRuntime() = default;

// 効果音の名前と再生ピッチを音声担当に渡す。音源の選択と再生枠の管理はGameplayAudioが行う。
void GameRuntime::PlaySfx(const char* key, float pitch)
{
    audio_.PlayEffect(key, pitch, gameplayElapsedSeconds_, IsPhantomRaidActive());
}

// 敗北・クリア・フィーバー・ボス戦の順に現在の状態を見て、再生する曲を選ぶ。
void GameRuntime::UpdateMusic()
{
    using Track = GameplayAudio::Track;
    const Track desired = isGameOver_ || isGameClear_ ? Track::Silent :
        rewards_.Fever().IsActive() ? Track::Fever : stageEnemies_.BossSpawned() ? Track::Boss : Track::Stage;
    audio_.UpdateMusic(desired, dxCommon_ ? dxCommon_->GetDeltaTime() : 1.0f / 60.0f,
        IsPhantomRaidActive() || bossPresentation_.IsPhaseTransitionPlaying(), IsTutorial());
}

// 描画と入力に使う共通オブジェクトの参照を受け取る。ゲーム側で必要なリソースは後の初期化で作る。
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

// ゲームの準備を同期的に完了させる入口。共有素材の待機中は短く休止してから再確認する。
void GameRuntime::Initialize(PlayMode mode)
{
    BeginInitialize(mode);
    while (!InitializeStep()) {
        if (!GameResources::IsPreloaded()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    }
}

// プレイモードを確定し、今回の戦闘・表示・配置を初期状態へ戻す。実際の素材準備はInitializeStepへ引き継ぐ。
void GameRuntime::BeginInitialize(PlayMode mode)
{
    // 初回出撃・再挑戦・練習開始で、担当クラスの状態を同じ順序で初期化する。
    // 永続記録はStageRecordsが保持し、ここでは今回のリザルト表示だけを戻す。
    playMode_ = mode;
    editorExitRequested_ = false;
    gameplayMenu_ = {};
    stageResults_.Reset();
    tutorial_.Reset();
    isGameClear_ = false;
    isGameOver_ = false;
    playerDefeatPresentation_.Reset();
    stageEnemies_.Reset(EnemyTargetReferences());
    isPerformanceOverlayVisible_ = false;
    showSkybox_ = true;
    combatResolver_.Reset();
    rewards_.Reset();
    combatProgress_.Reset();
    gameplayElapsedSeconds_ = 0.0f;
    flightMotionBlur_.Initialize();
    stageEntryPresentation_.Reset();
    enemyAttackPatterns_.Reset();
    bossPresentation_.Reset();
    bossAttack_.Reset();
    bossShotsFired_ = 0;
    stageFlight_.Reset(IsTutorial());
    // 地形は開始時から固定する。撃破速度による区間確定で視界内の施設を出現・変形させない。
    railScenery_.SetDistricts(IsTutorial(), IsTutorial() ? -1.0f : kDefenseStartWorldZ,
        IsTutorial() ? -1.0f : kPlazaStartWorldZ);
    gameplayCamera_.Reset(stageFlight_.Speed());
    playerFlightEffects_.Reset();
    playerControl_.Reset();
    hud_.Reset();
    playerCombatFeedback_ = {};
    combatEffects_.Clear();

    sceneSetup_.Reset();
    audio_.BeginLoad();
    projectiles_.Clear();
}

// 初期化側が設定する機体・背景・弾・UIなどへの参照をまとめる。
GameplaySceneSetup::Targets GameRuntime::SceneSetupTargets()
{
    return {
        .dx = dxCommon_,
        .srv = srvManager_,
        .common = object3dCommon_,
        .camera = camera_,
        .skybox = skybox_,
        .player = player_,
        .cameraMotion = gameplayCamera_,
        .enemies = stageEnemies_,
        .controls = playerControl_,
        .projectiles = projectiles_,
        .combatEffects = combatEffects_,
        .scenery = railScenery_,
        .speedEffects = flightSpeedEffects_,
        .sceneObjects = sceneObjects_,
        .playerEffects = playerFlightEffects_,
        .shadows = gameplayShadows_,
        .skill = playerSkill_,
        .skillPresentation = playerSkillPresentation_,
        .rings = ringGates_,
        .clearPresentation = stageResults_.Presentation(),
        .defeatPresentation = playerDefeatPresentation_,
        .audio = audio_,
        .sceneObjectStatus = editorStatusMessage_,
    };
}

// 弾やエフェクトの準備、音声ロードの進み具合を合わせた進捗を返す。
float GameRuntime::GetInitializationProgress() const
{
    return sceneSetup_.Progress(projectiles_, combatEffects_, audio_);
}

// 初回ロードと出撃準備を少しずつ進める。準備完了後にだけ、開発用の開始地点を適用できる。
bool GameRuntime::InitializeStep()
{
    if (sceneSetup_.IsReady()) { return true; }
    if (!sceneSetup_.Advance(SceneSetupTargets(), IsTutorial(), stageFlight_.Distance(), enemyBulletSpeed_)) {
        return false;
    }
#ifdef ENABLE_DEBUG_GUI
    ApplyDebugStartPhase();
#endif
    return true;
}

// リングとこのシーンが使っていた描画リソースを終了時に解放する。
void GameRuntime::Finalize()
{
    ResetRifts();
    sceneSetup_.Release(SceneSetupTargets());
}

// 本編の1フレーム。出撃・メニュー・墜落の専用処理を優先し、通常戦闘では移動、射撃、衝突、表示の順に進める。
void GameRuntime::Update()
{
    flightMotionBlur_.UpdateReview(*dxCommon_, !isGameOver_ && !isGameClear_ && !IsTutorial());
    // 出撃の最終姿勢・前進位置をそのまま通常のばね追従へ引き渡す。
    stageEntryPresentation_.FinishTitleEntry();
    // ゲーム進行を止めたままでも、表示先の変更だけは反映する。
    if (camera_) {
        Math::Vector2 viewportMin{}, viewportSize{};
        GetEffectiveHudViewportRect(viewportMin, viewportSize);
        const float aspect = viewportSize.x / (std::max)(viewportSize.y, 1.0f);
        if (aspect > 0.0f && camera_->GetAspectRatio() != aspect) {
            flightMotionBlur_.ResetHistory();
            camera_->SetAspectRatio(aspect);
            camera_->Update();
            if (skybox_) { skybox_->Update(camera_.get()); }
            UpdatePlayerAim();
        }
    }
    // 出撃や再挑戦の演出中はここで抜ける。機体とカメラの姿勢は演出側が更新する。
    if (UpdateRetryEntry()) { return; }
    if (UpdateMenuAction()) { return; }
#ifdef _DEBUG
    if (phantomPreviewPaused_) {
        DrawHud();
        DrawPlayerDefeatOverlay();
        if (isGameOver_ && playerDefeatPresentation_.IsActive()) {
            MenuUi::InputBlock block;
            DrawResultOverlay();
        } else { DrawResultOverlay(); }
        DrawPauseOverlay();
        DrawControlsHelp();
        return;
    }
#endif
    if (HandleRuntimeShortcuts()) {
        return;
    }
    if (UpdateMenuAction()) { return; }

    // 自機の撃墜後は墜落演出とリザルトだけを進める。
    if (isGameOver_ && playerDefeatPresentation_.IsActive()) {
        UpdatePlayerDefeat();
        DrawPlayerDefeatOverlay();
        DrawResultOverlay();
        UpdateMusic();
        return;
    }

    if (gameplayMenu_.IsPaused() || flightMotionBlur_.IsFrozen()) {
        // カメラ・敵・弾・衝突・ゲージを更新せず、最後のゲーム画面を描き続ける。
        DrawHud();
        DrawPauseOverlay();
        DrawControlsHelp();
        return;
    }

    if (!isGameOver_ && !isGameClear_ && dxCommon_) {
        gameplayElapsedSeconds_ +=
            std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 0.10f);
        stageEntryPresentation_.AdvanceBriefing(dxCommon_->GetDeltaTime());
    }

    const Math::Vector3 previousRiftPlayerPosition = player_ ? player_->GetTranslate() : Math::Vector3{};
    UpdateBossRadio(dxCommon_ ? dxCommon_->GetDeltaTime() : 1.0f / 60.0f);
    // レールと機体の位置を先に確定し、その位置と更新済みの照準から発射する。
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

    // 敵と弾の位置を進めてから、同じフレームの位置で衝突を判定する。
    UpdateWorldEntities();
    UpdateGameplayCollisions();
    if (player_) { UpdateRifts(previousRiftPlayerPosition, player_->GetTranslate()); }
    UpdateTutorialLesson();
    UpdateDefeatChain();
    // HUDを描く前に、撃破や退場を照準表示へ反映する。
    UpdatePlayerAim();
    DrawHud();
    DrawStageBriefing(stageEntryPresentation_.BriefingTime());
    DrawPlayerDefeatOverlay();
#ifdef ENABLE_DEBUG_GUI
    DrawPerformanceOverlay();
#endif
    DrawResultOverlay();
    DrawControlsHelp();
#ifdef ENABLE_DEBUG_GUI
    DrawEditorOverlay();
#endif

    UpdateResultAndSceneObjects();
    UpdatePlayerFlightEffects();
    UpdateMusic();
}

// 通常戦闘ではポーズとヘルプ、リザルト表示後はRの再挑戦を処理する。戻り値trueは開発用キーを処理したため今回の更新を抜ける合図。
bool GameRuntime::HandleRuntimeShortcuts()
{
    gameplayMenu_.HandleShortcuts(input_, !isGameOver_ && !isGameClear_);
    if ((isGameOver_ || isGameClear_) && !stageResults_.IsWaiting() &&
        input_ && input_->TriggerKey(DIK_R)) {
        if (gameplayMenu_.Begin(GameplayMenu::Action::Retry) && isGameOver_) {
            stageResults_.ResetSelection();
        }
        return false;
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
#endif
    return false;
}

// 現在区間の飛行設定を取り出し、到達したイベントを揺れ・リング・ボス警告に変える。
void GameRuntime::PrepareStageFrame()
{
    stageFlight_.PrepareFrame(rewards_.Fever().IsActive(), ringGates_.BoostStrength());
    if (IsTutorial()) { return; }

    while (const auto* event = stageFlight_.TryConsumeRailEvent()) {
        AddCameraShake(event->shakePower, event->shakeDuration);
    }
    while (const auto position = stageFlight_.TryConsumeGatePosition()) {
        SpawnRift(*position);
    }
    if (!stageEnemies_.BossSpawned() && !isGameOver_ && !isGameClear_ &&
        stageFlight_.IsBossWarningDue() && bossPresentation_.TryBeginWarning()) {
        stageFlight_.SetCombatBeat("WARNING");
        RequestBossRadio();
        AddCameraShake(0.13f, 28);
    }
}

// 前方への移動を進める。敵の同時出現数とボスの状態は配置の進み方にも渡す。
void GameRuntime::UpdateRailProgress()
{
    if (isGameOver_ || isGameClear_) { return; }

    PrepareStageFrame();
    stageFlight_.Advance(GetCinematicWorldTimeScale(), stageEnemies_.Load(),
        rewards_.Fever().IsActive(), ringGates_.BoostStrength(), stageEnemies_.BossSpawned());
}

// 入力で自機を動かし、その移動を敵の照準に渡す。更新したカメラから照準の位置を求め直す。
void GameRuntime::UpdatePlayerAndCamera()
{
    const float controlStep = dxCommon_ ? dxCommon_->GetDeltaTime() * 60.0f : 1.0f;
    const Math::Vector3 before = player_->GetTranslate();
    playerControl_.UpdateMovement(*player_, input_, GetCinematicWorldTimeScale(), controlStep, stageFlight_.Distance());
    const Math::Vector3 after = player_->GetTranslate();
    enemyAttackPatterns_.TrackPlayerMovement(before, after, controlStep, player_->IsDodging());
    UpdateGameCamera();
    if (skybox_) {
        skybox_->Update(camera_.get());
    }
    UpdatePlayerAim();
}

// 自機・照準・飛行速度など、そのフレームで必要な値をまとめて射撃処理へ渡す。
void GameRuntime::UpdatePlayerShooting()
{
    const PlayerControl::ShotFrame frame{
        .player = player_.get(),
        .view = GetPlayerAimView(),
        .railSpeed = stageFlight_.Speed(),
        .cameraClock = gameplayCamera_.Clock(),
        .tutorial = IsTutorial(),
        .fever = rewards_.Fever().IsActive(),
    };
    if (const auto shot = playerControl_.UpdateShooting(input_, projectiles_, frame)) {
        if (shot->fired) {
            // フィーバー連射には通常の発射音を使い、単発のチャージショットには重い発射音を使う。
            PlaySfx(shot->charged && !frame.fever ? "charge" : "shot");
            combatEffects_.PlayerShot(shot->muzzle, shot->charged, frame.fever);
            if (shot->charged) { hud_.FlashChargeShot(); }
        }
        // 発射の入力に合わせ、単発のチャージショットで揺れを強める。
        AddCameraShake(
            frame.fever ? 0.012f : (shot->charged ? 0.052f : 0.010f),
            frame.fever ? 2 : (shot->charged ? 6 : 2));
    }
    hud_.TickCharge();
}

// 自機が生存していて戦闘中なら自動発動を許可する。発動が通知されたフレームだけ専用の音と光を出す。
void GameRuntime::UpdateFever()
{
    const bool canActivate = !isGameOver_ && !isGameClear_ && player_ && !player_->IsDead();
    if (rewards_.UpdateFever(playerControl_.Shooting(), canActivate)) {
        ShowFeverActivation();
    }
}

// フィーバー開始の合図を効果音、HUDの光、自機の周囲のエフェクト、カメラの揺れで表示する。
void GameRuntime::ShowFeverActivation()
{
    PlaySfx("fever");
    hud_.FlashCharge(30);
    if (player_) {
        const Math::Vector3 playerPosition = player_->GetTranslate();
        combatEffects_.DodgeGraze(playerPosition);
        combatEffects_.DodgeGraze({
            playerPosition.x,
            playerPosition.y + 0.18f,
            playerPosition.z + 0.70f
        });
    }
    AddCameraShake(0.16f, 24);
}

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
// 開発用の手動発動。戦闘が終わっていなければフィーバー状態を設定して演出を出す。
void GameRuntime::ActivateFever()
{
    if (isGameOver_ || isGameClear_) { return; }
    rewards_.ActivateFever(playerControl_.Shooting());
    ShowFeverActivation();
}
#endif

#ifdef _DEBUG
// 自動テストがゲージを加算する入口。練習課題や戦闘終了時の制限も通す。
void GameRuntime::AddFeverGauge(int amount)
{
    if (CanEarnFever()) { rewards_.Fever().AddGauge(amount); }
}
#endif

// 戦闘中にゲージを得られるか返す。練習ではフィーバーが許可された課題だけ有効。
bool GameRuntime::CanEarnFever() const
{
    return !isGameOver_ && !isGameClear_ && (!IsTutorial() || tutorial_.AllowsFever());
}

// 戦闘中か、狙える通常敵がいるかを渡し、撃破チェインの猶予を更新する。
void GameRuntime::UpdateDefeatChain()
{
    combatProgress_.Update(stageEnemies_.HasTargetableNormal(), !isGameOver_ && !isGameClear_);
}


// 配置表から敵を出し、ボスと通常敵の発射を処理する。通常敵には画面内で見えているかも渡す。
void GameRuntime::UpdateEnemyActions()
{
    if (IsTutorial()) { UpdateTutorialAttack(); return; }
    if (playerCombatFeedback_.IsDodgeSlowActive()) { return; }
    UpdateStageEnemyEvents();
    SpawnBossEnemy();
    UpdateBossActions();
    const bool hasSupportDrone = stageEnemies_.HasSupport();
    Math::Vector2 viewportMin{}, viewportSize{};
    GetEffectiveHudViewportRect(viewportMin, viewportSize);
    const float worldStep = GetCinematicWorldTimeScale();
    for (const auto& enemy : stageEnemies_.Active()) {
        if (!enemy || enemy->IsBoss() || enemy->IsSupport()) { continue; }
        Math::Vector2 screen{};
        const bool visible = TryProjectToScreen(enemy->GetAimPosition(), screen) &&
            screen.x >= viewportMin.x + 12.0f && screen.x <= viewportMin.x + viewportSize.x - 12.0f &&
            screen.y >= viewportMin.y + 12.0f && screen.y <= viewportMin.y + viewportSize.y - 12.0f;
        const auto volley = enemyAttackPatterns_.Update(*enemy, player_->GetTranslate(),
            worldStep, enemyShotInterval_, hasSupportDrone, visible);
        if (!volley) { continue; }
        for (size_t index = 0; index < volley->count; ++index) {
            const auto& shot = volley->shots[index];
            FireEnemyBullet(volley->muzzle, shot.style, shot.aimOffset, &volley->aimPoint);
        }
    }
}

// 攻撃コントローラの通知を、発射・形態移行・画面演出へつなぐ。
void GameRuntime::UpdateBossActions()
{
    const Enemy* boss = stageEnemies_.Boss();
    const auto events = bossAttack_.Update({ boss != nullptr,
        boss && boss->CanShoot() && !bossPresentation_.BlocksAttacks(),
        boss ? boss->GetHp() : 0, boss ? boss->GetMaxHp() : 1,
        player_ ? player_->GetTranslate() : Math::Vector3{} });
    // 第2形態の演出に入る前に、前の形態の敵弾を片づける。
    if (events.phaseChanged) {
        bossPresentation_.BeginPhaseTransition();
        stageFlight_.SetCombatBeat("Boss phase 2");
        projectiles_.RecycleEnemyBullets();
        combatEffects_.EnemyImpact(boss->GetAimPosition(), 2.0f);
        AddCameraShake(0.24f, 42);
        return;
    }
    const auto pattern = bossAttack_.CurrentPattern();
    if (events.attackStarted) {
        constexpr const char* names[]{ "Boss fan volley", "Boss sweep", "Boss charge cannon", "Boss crossfire", "Boss strafing run" };
        stageFlight_.SetCombatBeat(names[static_cast<int>(pattern)]);
        if (pattern == BossAttackController::Pattern::Charge) { AddCameraShake(0.08f, 18); }
    }
    // 攻撃の判断を受け取り、実際の敵の銃口から既存の弾・照準計算で発射する。
    for (int index = 0; index < events.shotCount; ++index) {
        const auto& shot = events.shots[index];
        const auto origin = boss->GetBossMuzzlePosition(shot.muzzle == BossAttackController::Muzzle::Left ? -1.0f :
            shot.muzzle == BossAttackController::Muzzle::Right ? 1.0f : 0.0f);
        FireEnemyBullet(origin, shot.charged ? EnemyBulletStyle::BossCharge : EnemyBulletStyle::BossCannon,
            shot.targetOffset, &bossAttack_.AimPoint());
        if (shot.charged) { AddCameraShake(0.20f, 26); }
    }
    if (events.recoveryStarted) {
        stageFlight_.SetCombatBeat("Boss exposed");
        if (pattern == BossAttackController::Pattern::Fan) { AddCameraShake(0.11f, 16); }
        if (pattern == BossAttackController::Pattern::Rush) { AddCameraShake(0.12f, 18); }
    }
}

// 弾と敵を進め、戦闘の表示と背景も更新する。直後の衝突判定は更新済みの位置を使う。
void GameRuntime::UpdateWorldEntities()
{
    projectiles_.UpdatePlayerBullets();
    projectiles_.UpdateEnemyBullets(GetCinematicWorldTimeScale());
    UpdateEnemies();
    UpdateHitEffects();
    UpdatePlayerDodgeAfterimages();
    UpdateDepthCueEffects();
    railScenery_.Update(stageFlight_.Distance());
}

// ボス撃破の爆発、結果音、画面表示の時計を進める。クリア記録は本編の結果表示が可能になった時点で保存する。
void GameRuntime::UpdateResultAndSceneObjects()
{
    if (isGameOver_ && playerDefeatPresentation_.IsActive()) { return; }
    // 撃破の最後の爆発が終わってからリザルトの音を鳴らす。
    if ((isGameClear_ || isGameOver_) && !bossPresentation_.IsDefeatPlaying() &&
        stageResults_.TryMarkSoundPlayed()) {
        PlaySfx(isGameClear_ ? "clear" : "fail");
    }
    if (const auto burst = bossPresentation_.AdvanceFrame()) {
        AddEnemyHitEffect(burst->position, burst->strength);
        PlaySfx("destroy");
        AddCameraShake(burst->shakePower, burst->shakeFrames);
    }
    playerCombatFeedback_.Tick();
    hud_.TickFeedback();
    stageResults_.Advance(isGameClear_ && !IsTutorial(), combatProgress_.Score());

    sceneObjects_.Update();
}

// エディタなどで選ばれた空の表示と画面効果の種類を、次の描画へ反映する。
void GameRuntime::SetRenderingOptions(bool showSkybox, int postEffectMode)
{
    showSkybox_ = showSkybox;
    postEffectMode_ = postEffectMode;
}

// 自機が準備済みなら現在HPを返し、未初期化時は0を返す。
int GameRuntime::GetPlayerHp() const
{
    return player_ ? player_->GetHp() : 0;
}

// 画面効果で深度から3D位置を復元するために、現在の投影行列を公開する。
const Math::Matrix4x4& GameRuntime::GetProjectionMatrix() const
{
    static const Math::Matrix4x4 kIdentity = Math::MakeIdentity4x4();
    if (HasResultPresentation()) { return stageResults_.Presentation().GetProjectionMatrix(); }
    return camera_ ? camera_->GetProjectionMatrix() : kIdentity;
}

// エディタ内の表示範囲を受け取り、HUDと照準の座標基準にできるようにする。
void GameRuntime::SetHudViewportRect(
    bool isEnabled,
    const Math::Vector2& min,
    const Math::Vector2& size)
{
    isHudViewportRectEnabled_ = isEnabled && size.x > 1.0f && size.y > 1.0f;
    hudViewportMin_ = min;
    hudViewportSize_ = size;
}

// ウィンドウ全体を基準にし、指定されたプレビュー領域やエディタ領域があれば優先する。左上と幅・高さを引数に返す。
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

// レール位置、速度、フィーバー、回避の時間など、速度演出に必要な状態を1フレーム分まとめる。
FlightSpeedEffects::Frame GameRuntime::BuildFlightSpeedEffectsFrame(bool useViewport) const
{
    FlightSpeedEffects::Frame frame{};
    frame.camera = camera_.get();
    if (useViewport) {
        GetEffectiveHudViewportRect(frame.viewportMin, frame.viewportSize);
    }
    frame.railDistance = stageFlight_.Distance();
    frame.speedRate = stageFlight_.SpeedRate();
    frame.feverSpeedRate = rewards_.Fever().SpeedEffectRate();
    return frame;
}

// 背景に配置した速度の手がかりを更新する。本編の飛行中は進行方向に光の流れをそろえる。
void GameRuntime::UpdateDepthCueEffects()
{
    if (flightSpeedEffects_.WorldCueCount() == 0) { return; }
    const float frameStep =
        dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.5f, 4.0f) : 1.0f;
    const bool alignStreaks = !IsTutorial() && !stageEntryPresentation_.IsTitleActive();
    flightSpeedEffects_.UpdateWorld(BuildFlightSpeedEffectsFrame(alignStreaks && camera_),
        frameStep, alignStreaks);
}
// 現在使う画面効果の番号を返す。標準設定では墜落中・敗北後・クリア後の見せ方を状態に合わせて切り替える。
int GameRuntime::GetPostEffectMode() const
{
    if (isPostEffectBypassEnabled_) {
        return 0;
    }
    if (postEffectMode_ != 12) {
        return postEffectMode_;
    }
    if (isGameOver_) {
        return playerDefeatPresentation_.IsActive() ? 12 : 15;
    }
    if (isGameClear_) {
        return 14;
    }
    return 12;
}

// 3D描画の順番をまとめる。先に影を作り、不透明な物体の後にガラス・弾の光・エフェクトを重ねる。
void GameRuntime::Draw()
{
    if (HasResultPresentation()) {
        stageResults_.Presentation().Draw(*dxCommon_, skybox_.get());
        return;
    }
    if (dxCommon_ && camera_ && player_ && !IsTutorial()) {
        Math::Vector2 viewportMin{}, viewportSize{};
        GetEffectiveHudViewportRect(viewportMin, viewportSize);
        bool paused = gameplayMenu_.IsPaused();
#ifdef _DEBUG
        paused = paused || phantomPreviewPaused_;
#endif
        const bool inProgress = !isGameOver_ && !isGameClear_;
        flightMotionBlur_.Apply(*dxCommon_, *camera_, viewportMin, viewportSize,
            { 0.0f, 1.0f, stageFlight_.Distance() + 260.0f }, player_->GetTranslate(), playerControl_.Aim().Reticle(),
            inProgress ? gameplayCamera_.MotionState().blurStrength : 0.0f,
            inProgress && !stageEntryPresentation_.IsTitleActive(), paused);
    }
    // 墜落後の機体の破片も、残っている間は地面に影を落とす。
    std::array<Object3d*, PlayerDefeatPresentation::kDebrisCount> wreckage{};
    const size_t wreckageCount = playerDefeatPresentation_.CopyShadowCasters(wreckage);
    const GameplayShadows::Scene shadowScene{
        sceneObjects_.Objects(), railScenery_, stageEnemies_.Active(), player_.get(),
        { wreckage.data(), wreckageCount }, stageFlight_.Distance()
    };
    gameplayShadows_.RenderMap(*object3dCommon_, shadowScene);

    if (showSkybox_ && skybox_) {
        skybox_->Draw();
    }

    railScenery_.Draw(ModelDrawPass::Opaque);
    gameplayShadows_.DrawContacts(*object3dCommon_, shadowScene, gameplayCamera_.Clock());
    if (!stageEntryPresentation_.IsOverviewActive()) { flightSpeedEffects_.DrawWorld(); }

    object3dCommon_->CommonDrawSetting();
    sceneObjects_.Draw();
    if (player_ && !IsTitleEntryForeground() && !playerDefeatPresentation_.HasExploded()) {
        player_->Draw();
    }
    if (isGameOver_) { playerDefeatPresentation_.DrawDebris(); }
    if (!IsTitleEntryForeground()) { DrawPlayerFlightEffects(); }
    playerFlightEffects_.DrawAfterimages();
    projectiles_.DrawBodies(camera_ ? &camera_->GetWorldMatrix() : nullptr);
    stageEnemies_.Draw();
    // 不透明な背景・機体を描き終えてから、窓ガラスだけを合成する。
    railScenery_.Draw(ModelDrawPass::Transparent);
    if (camera_) { projectiles_.DrawGlows(camera_->GetWorldMatrix()); }
    DrawRiftObjects();
    combatEffects_.Draw();
    if (isGameOver_ && camera_ && player_) {
        playerDefeatPresentation_.DrawEffects(*object3dCommon_, *camera_, *player_);
    }
    if (object3dCommon_ && camera_) {
        playerSkillPresentation_.DrawObjects(*object3dCommon_, *camera_, playerSkill_);
    }

}

// 銃口、弾種、照準位置、レール速度を発射側へ渡す。生成に成功したら銃口の演出を出し、ボス弾なら発射数も記録する。
void GameRuntime::FireEnemyBullet(
    const Math::Vector3& position,
    EnemyBulletStyle style,
    Math::Vector2 aimOffset,
    const Math::Vector3* fixedAim)
{
    const ProjectileSystem::EnemyShot shot{
        .origin = position,
        .style = style,
        .aimOffset = aimOffset,
        .fixedAim = fixedAim,
        .playerPosition = player_ ? &player_->GetTranslate() : nullptr,
        .speed = enemyBulletSpeed_,
        .railSpeed = stageFlight_.Speed(),
        .tutorial = IsTutorial(),
    };
    const auto* bullet = projectiles_.FireEnemy(shot);
    if (!bullet) { return; }

    combatEffects_.EnemyShot(bullet->GetTranslate());
    if (style == EnemyBulletStyle::BossCannon || style == EnemyBulletStyle::BossCharge) { ++bossShotsFired_; }
}

#ifdef _DEBUG
// 開発テスト用に、自機の進行距離を基準とした位置に敵を出す。
Enemy* GameRuntime::SpawnStageEnemyForProbe(float x, float y, float leadDistance,
    Enemy::Behavior behavior, Enemy::EntryStyle entryStyle, int hp, float scale)
{
    return stageEnemies_.SpawnForProbe({ x, y, stageFlight_.Distance() + leadDistance }, behavior, entryStyle, hp, scale);
}
#endif

// 配置の進行が登場条件に届いたらボスを生成する。生成成功時に登場演出、射撃状態、画面表示をそろえる。
void GameRuntime::SpawnBossEnemy()
{
    if (IsTutorial() || isGameOver_ || isGameClear_ || !stageFlight_.Schedule().IsBossSpawnDue() ||
        !stageEnemies_.SpawnBoss(stageFlight_.Distance())) { return; }

    bossPresentation_.BeginIntro();
    bossAttack_.Reset();
    bossShotsFired_ = 0;
    stageFlight_.SetCombatBeat("Boss");
    RequestBossRadio(); // 接近時に受信済みなら、入場時には繰り返さない。
    AddCameraShake(0.18f, 36);
}

// 現在の進行度で出せる敵を順に生成する。敵の出現状況は区間表示にも使う。
void GameRuntime::UpdateStageEnemyEvents()
{
    if (IsTutorial() || isGameOver_ || isGameClear_ || stageEnemies_.BossSpawned()) { return; }
    for (int count = 0; count < StageDirector::kMaxSpawnsPerFrame; ++count) {
        const auto* event = stageFlight_.TryConsumeEnemy(stageEnemies_.Load());
        if (!event) { break; }
        stageEnemies_.SpawnScheduled(*event, stageFlight_.Distance());
        stageFlight_.SetCombatBeat(event->beatName);
        AddCameraShake(event->shakePower, event->shakeDuration);
    }
}

// 照準が使う表示領域と、敵を画面座標へ写すビュー投影行列をまとめて返す。
PlayerControl::AimView GameRuntime::GetPlayerAimView() const
{
    PlayerControl::AimView view;
    GetEffectiveHudViewportRect(view.min, view.size);
    if (camera_) { view.viewProjection = &camera_->GetViewProjectionMatrix(); }
    return view;
}

// 今のカメラと表示領域で照準を更新し、有効な敵が狙えるか判定する。
void GameRuntime::UpdatePlayerAim()
{
    auto view = GetPlayerAimView();
    if (isGameOver_ || isGameClear_) { view.viewProjection = nullptr; }

    std::optional<Math::Vector2> cursor;
    if (input_) {
        // エディタのマウス座標とゲームウィンドウ基準の座標を、表示先と同じ座標系にそろえる。
        if (isEditorOverlayVisible_ && hasEditorOverlayViewportRect_) {
            const auto mouse = ImGui::GetMousePos();
            cursor = Math::Vector2{ mouse.x, mouse.y };
        } else {
            const auto origin = ImGui::GetMainViewport()->Pos;
            const auto& mouse = input_->GetMousePosition();
            cursor = Math::Vector2{ origin.x + mouse.x, origin.y + mouse.y };
        }
    }
    playerControl_.UpdateAim(view, stageEnemies_.Active(), cursor);
}

// ワールド上の点をHUDの座標へ変換する。カメラがない場合や投影できない点ではfalseを返す。
bool GameRuntime::TryProjectToScreen(
    const Math::Vector3& worldPosition,
    Math::Vector2& screenPosition) const
{
    if (!camera_) { return false; }
    Math::Vector2 viewportMin{}, viewportSize{};
    GetEffectiveHudViewportRect(viewportMin, viewportSize);
    return ScreenProjection::ToViewport(worldPosition, camera_->GetViewProjectionMatrix(),
        viewportMin, viewportSize, screenPosition);
}

// 指定位置で撃破と同じ爆発演出を再生する。現在のチェイン段階で見た目を変える。
void GameRuntime::AddEnemyHitEffect(
    const Math::Vector3& worldPosition,
    float strength, bool fever)
{
    combatEffects_.EnemyDestroyed(worldPosition, strength, CombatResolver::HitEffectChainStyle(combatProgress_), fever);
}

// ジャスト回避の成立を受けて、報酬と演出を一緒に発生させる。
void GameRuntime::RewardJustDodge(const Math::Vector3& bulletPosition)
{
    PlaySfx("dodge");
    rewards_.ApplyJustDodge(combatProgress_, playerControl_.Shooting(), CanEarnFever());
    hud_.FlashCharge(26);
    playerCombatFeedback_.StartJustDodge();

    const Math::Vector3 playerPosition = player_->GetTranslate();
    combatEffects_.JustDodged(playerPosition, bulletPosition, player_->GetDodgeDirection());
    PlayerFlightEffects::DodgeFrame frame{};
    frame.position = playerPosition;
    frame.visualPosition = player_->GetVisualTranslate();
    frame.rotation = player_->GetVisualRotate();
    frame.scale = player_->GetVisualScale();
    frame.direction = player_->GetDodgeDirection();
    playerFlightEffects_.FlashJustDodge(frame);
    AddCameraShake(0.045f, 5);
}

// チャージ、ボス命中、撃破の情報をもとに短いスローを開始し、対応するカメラの揺れを加える。
void GameRuntime::TriggerPlayerImpactMoment(
    bool isCharged,
    bool isBossHit,
    bool isDestroyed)
{
    const auto shake = playerCombatFeedback_.StartImpact(isCharged, isBossHit, isDestroyed);
    AddCameraShake(shake.strength, shake.frames);
}

// 命中位置を画面へ写してHUDに印を出す。画面へ写せなかった場合は照準位置を使う。
void GameRuntime::TriggerHitConfirm(
    const Math::Vector3& worldPosition,
    bool isCharged,
    bool isBossHit,
    bool isDestroyed)
{
    Math::Vector2 screenPosition{};
    const auto screen = TryProjectToScreen(worldPosition, screenPosition) ? screenPosition : playerControl_.Aim().Reticle();
    hud_.RecordHit(screen, isCharged, isBossHit, isDestroyed);
}

// 被弾位置を画面へ写し、敵弾が来た向きをHUDの被弾表示に渡す。
void GameRuntime::TriggerPlayerDamageFeedback(
    const Math::Vector3& worldPosition,
    const Math::Vector3& incomingVelocity)
{
    Math::Vector2 screenPosition{};
    const auto screen = TryProjectToScreen(worldPosition, screenPosition) ? screenPosition : playerControl_.Aim().Reticle();
    hud_.RecordDamage(screen, { incomingVelocity.x, incomingVelocity.y });
}

// 戦闘用エフェクトの経過時間を進め、表示の終わったものを再利用枠へ戻す。
void GameRuntime::UpdateHitEffects()
{
    combatEffects_.Update(dxCommon_ ? dxCommon_->GetDeltaTime() : 1.0f / 60.0f);
}

// 自機の姿勢と回避状態を渡し、残像の発生と表示の終了を進める。
void GameRuntime::UpdatePlayerDodgeAfterimages()
{
    PlayerFlightEffects::DodgeFrame frame{};
    frame.deltaSeconds = dxCommon_ ? dxCommon_->GetDeltaTime() : 1.0f / 60.0f;
    frame.alive = player_ && !player_->IsDead();
    frame.dodging = frame.alive && player_->IsDodging();
    frame.justDodge = playerCombatFeedback_.IsDodgeSlowActive();
    if (player_) {
        frame.position = player_->GetTranslate(); frame.direction = player_->GetDodgeDirection();
        frame.visualPosition = player_->GetVisualTranslate(); frame.rotation = player_->GetVisualRotate();
        frame.scale = player_->GetVisualScale();
    }
    if (playerFlightEffects_.UpdateAfterimages(frame)) {
        const float direction = frame.direction >= 0 ? 1.0f : -1.0f;
        combatEffects_.DodgeGraze({ frame.position.x - direction * 0.58f,
            frame.position.y + 0.10f, frame.position.z + 0.16f });
        AddCameraShake(0.060f, 10);
    }
}

// 自機が生存中なら、噴射とチャージの光を現在の速度・機体姿勢・チャージ状態に合わせて更新する。
void GameRuntime::UpdatePlayerFlightEffects()
{
    if (!player_ || player_->IsDead()) { return; }
    PlayerFlightEffects::FlightFrame frame{};
    frame.position = player_->GetTranslate();
    frame.rotation = player_->GetVisualRotate();
    frame.deltaSeconds = dxCommon_ ? dxCommon_->GetDeltaTime() : 1.0f / 60.0f;
    frame.clockFrames = gameplayCamera_.Clock();
    frame.speed = stageFlight_.SpeedRate();
    frame.acceleration = stageFlight_.AccelerationRate();
    frame.fever = rewards_.Fever().SpeedEffectRate();
    frame.boost = GetFlightBoostKick();
    frame.gateBoost = ringGates_.BoostStrength();
    frame.justDodge = playerCombatFeedback_.DodgeRate();
    frame.releaseKick = stageFlight_.ReleaseKick();
    frame.charge = playerControl_.Shooting().ChargeRate();
    frame.dodging = player_->IsDodging();
    frame.feverActive = rewards_.Fever().IsActive();
    playerFlightEffects_.UpdateFlight(frame);
}

// 更新済みの自機の噴射やチャージ効果を描く。
void GameRuntime::DrawPlayerFlightEffects()
{
    if (player_ && !player_->IsDead()) { playerFlightEffects_.DrawFlight(); }
}

// リングの配置と加速の状態をリセットする。終了や再初期化で使う。
void GameRuntime::ResetRifts()
{
    ringGates_.Reset();
    rewards_.ResetRingNotice();
}

// ステージから届いた位置に、通過できるリングを1つ置く。
void GameRuntime::SpawnRift(const Math::Vector3& position)
{
    if (!TutorialAllowsRings() || isGameOver_ || isGameClear_ || !player_ ||
        position.z <= player_->GetTranslate().z + 1.0f) { return; }
    ringGates_.Spawn(position, stageFlight_.Distance(), stageFlight_.Speed());
}

// 前フレームから現在までの自機の移動を渡し、通過したリング分の報酬を適用する。
void GameRuntime::UpdateRifts(const Math::Vector3& previous, const Math::Vector3& current)
{
    if (gameplayMenu_.IsPaused()) { return; }
    if (isGameOver_ || isGameClear_ || !player_ || player_->IsDead()) {
        ringGates_.Deactivate();
        rewards_.HideRingNotice();
        return;
    }
    if (!TutorialAllowsRings()) {
        // 練習の達成表示中も、通過後の開放と加速の余韻は進める。
        ringGates_.Update(current, current, GetCinematicWorldTimeScale());
        return;
    }
    rewards_.TickRingNotice();
    const int passes = ringGates_.Update(previous, current, GetCinematicWorldTimeScale());
    for (int pass = 0; pass < passes; ++pass) { ApplyRingPassReward(current); }
}

// リングを通った1回分の加速・報酬・音と光を発生させる。
void GameRuntime::ApplyRingPassReward(const Math::Vector3& position)
{
    ringGates_.StartBoost();
    PlaySfx("gate_boost");
    rewards_.ApplyRingPass(combatProgress_, playerControl_.Shooting(), CanEarnFever());
    hud_.FlashCharge(24);
    combatEffects_.RingCollected(position);
    AddCameraShake(0.055f, 8);
}

// 現在の自機位置とフィーバー状態を使ってリングの3D表示を描く。
void GameRuntime::DrawRiftObjects()
{
    if (!player_ || !camera_ || !dxCommon_) { return; }
    ringGates_.Draw(player_->GetTranslate(), rewards_.Fever().IsActive());
}

// 本編の戦闘中に、ボス接近の通信を出すよう演出側へ依頼する。
void GameRuntime::RequestBossRadio()
{
    if (CanShowBossRadio()) { bossPresentation_.RequestRadio(); }
}

// ポーズ中は通信の時計を止め、進行中は実時間で残り表示時間を減らす。
void GameRuntime::UpdateBossRadio(float deltaSeconds)
{
    if (!gameplayMenu_.IsPaused()) {
        bossPresentation_.AdvanceRadio(deltaSeconds, CanShowBossRadio());
    }
}

// 画面の周囲へ加速の線を重ねる。練習ではリング加速中に表示する。
void GameRuntime::DrawFlightSpeedOverlay()
{
    if ((IsTutorial() && ringGates_.BoostStrength() <= 0.0f) || isGameOver_ || isGameClear_) { return; }
    const FlightSpeedEffects::ScreenStreaks streaks{
        player_ ? &player_->GetTranslate() : nullptr,
        playerControl_.Aim().Reticle(),
        stageFlight_.Speed(),
        GetCinematicWorldTimeScale(),
        GetFlightBoostKick(),
        rewards_.Fever().ActivationFlashFrames(),
    };
    flightSpeedEffects_.DrawScreenStreaks(BuildFlightSpeedEffectsFrame(true),
        streaks, *ImGui::GetForegroundDrawList());
}

// 自機、敵、スコアの現在値をHUD用に集める。表示側はこの時点の値を受け取って描く。
GameplayHud::Frame GameRuntime::BuildHudFrame() const
{
    GameplayHud::Frame frame{};
    GetEffectiveHudViewportRect(frame.min, frame.size);
    frame.hp = GetPlayerHp();
    frame.maxHp = GetPlayerMaxHp();
    frame.score = combatProgress_.Score();
    frame.wave = stageFlight_.WaveIndex() < kWaveCount ? stageFlight_.WaveIndex() + 1 : kWaveCount;
    frame.waveCount = kWaveCount;
    frame.clock = gameplayCamera_.Clock();
    frame.charge = playerControl_.Shooting().ChargeRate();
    frame.chargeReady = playerControl_.Shooting().IsChargeReady();
    frame.tutorial = IsTutorial();
    frame.bossSpawned = stageEnemies_.BossSpawned();
    frame.reticle = playerControl_.Aim().Reticle();
    frame.reticleAligned = playerControl_.Aim().IsReticleOnTarget();
    frame.justDodge = playerCombatFeedback_.DodgeRate();
    frame.bossDefeat = bossPresentation_.DefeatFlashRate();
    frame.dodgeDirection = player_ && player_->GetDodgeDirection() < 0 ? -1.0f : 1.0f;
    frame.fever.active = rewards_.Fever().IsActive();
    frame.fever.ready = rewards_.Fever().IsReady();
    frame.fever.rate = frame.fever.active ?
        static_cast<float>(rewards_.Fever().RemainingFrames()) / kFeverDurationFrames :
        static_cast<float>(rewards_.Fever().Gauge()) / kFeverGaugeMax;
    frame.fever.seconds = static_cast<float>(rewards_.Fever().RemainingFrames()) / 60.0f;
    frame.fever.activationFlash = static_cast<float>(rewards_.Fever().ActivationFlashFrames()) / FeverState::kActivationFlashFrames;
    frame.chain.count = combatProgress_.ChainCount();
    frame.chain.multiplier = combatProgress_.ChainMultiplier() * rewards_.Fever().ScoreMultiplier();
    frame.chain.remaining = std::clamp(static_cast<float>(combatProgress_.ChainRemainingFrames()) /
        CombatProgress::kChainDurationFrames, 0.0f, 1.0f);
    frame.chain.breakFlash = static_cast<float>(combatProgress_.ChainBreakFlashFrames()) / CombatProgress::kChainBreakFlashFrames;
    frame.chain.punch = 1.0f - std::clamp(static_cast<float>(CombatProgress::kChainDurationFrames -
        combatProgress_.ChainRemainingFrames()) / 14.0f, 0.0f, 1.0f);
    if (const auto* boss = stageEnemies_.Boss(); boss && stageEnemies_.BossSpawned() && !stageEnemies_.BossDefeated()) {
        frame.boss.visible = true;
        frame.boss.hp = std::clamp(static_cast<float>(boss->GetHp()) / (std::max)(boss->GetMaxHp(), 1), 0.0f, 1.0f);
        const bool counter = bossAttack_.IsCounterOpen();
        const bool attacking = !counter && bossAttack_.IsAttacking() && bossAttack_.RemainingFrames() > 0;
        if (counter || attacking) {
            using Cue = GameplayHud::BossCue;
            constexpr Cue cues[]{ Cue::Fan, Cue::Sweep, Cue::Charge, Cue::Crossfire, Cue::Rush };
            frame.boss.cue = counter ? Cue::Counter : cues[static_cast<int>(bossAttack_.CurrentPattern())];
            const int duration = counter ? bossAttack_.CounterDuration() : bossAttack_.WindupFrames();
            const float remaining = std::clamp(static_cast<float>(counter ? bossAttack_.CounterFrames() :
                bossAttack_.RemainingFrames()) / (std::max)(duration, 1), 0.0f, 1.0f);
            frame.boss.action = counter ? remaining : (bossAttack_.IsBursting() ? 1.0f : 1.0f - remaining);
        }
    }
    frame.skill.active = playerSkill_.IsActive();
    frame.skill.ready = playerSkill_.IsReady();
    frame.skill.locked = IsTutorial() && tutorial_.CurrentLesson() < TutorialProgress::Lesson::Skill;
    frame.skill.empowered = frame.skill.active ? playerSkill_.IsEmpowered() : rewards_.Fever().IsActive();
    frame.skill.noTarget = !frame.skill.active && playerSkill_.NoTargetNoticeFrames() > 0.0f;
    frame.skill.cooldownSeconds = playerSkill_.CooldownFrames() / 60.0f;
    frame.skill.recovery = 1.0f - std::clamp(playerSkill_.CooldownFrames() / PlayerSkill::kCooldownFrames, 0.0f, 1.0f);
    frame.skill.readyFlash = std::clamp(playerSkill_.ReadyFlashFrames() / 72.0f, 0.0f, 1.0f);
    if (TutorialAllowsRings() && !isGameOver_ && !isGameClear_ && player_) {
        frame.ringNoticeFrames = rewards_.RingNoticeFrames();
        frame.ringExtendedFever = rewards_.RingRecoveredFrames() > 0;
    }
    if (bossPresentation_.HasRadio() && CanShowBossRadio()) {
        frame.bossRadioSeconds = bossPresentation_.RadioRemaining();
        frame.bossRadioElapsed = bossPresentation_.RadioElapsed();
    }
    return frame;
}

// 戦闘中の表示値を集め、照準・ボス予告・スキルなどのHUDを画面へ重ねる。
void GameRuntime::DrawHud()
{
    if (gameplayMenu_.IsPaused() || !player_ || isGameOver_ || (isGameClear_ && !stageResults_.IsWaiting())) { return; }
    const auto frame = BuildHudFrame();
    auto* draw = ImGui::GetForegroundDrawList();
    // 重なり順も表示の一部。背景演出→計器→練習案内→照準・フィードバックの順を保つ。
    const auto* viewProjection = camera_ ? &camera_->GetViewProjectionMatrix() : nullptr;
    playerSkillPresentation_.DrawOverlay(*draw, frame.min, frame.size, viewProjection, playerSkill_);
    ImGui::PushFont(CombatHud::BattleFont(), 16.0f);
    CombatOverlay::DrawFeverBackdrop(*draw, frame.min, frame.size, rewards_.Fever().SpeedEffectRate(), frame.clock);
    DrawFlightSpeedOverlay();
    if (camera_) {
        const CombatOverlay::TelegraphView view{
            camera_->GetViewProjectionMatrix(), frame.min, frame.size, frame.clock,
            player_->GetTranslate().z, tutorial_.DodgeTelegraphRate(), frame.tutorial,
        };
        CombatOverlay::DrawEnemyTelegraphs(*draw, view, stageEnemies_.Active(), bossAttack_);
    }
    hud_.DrawRingNotice(frame, draw);
    hud_.DrawCore(frame, draw);
    hud_.DrawBoss(frame, draw);
    DrawTutorialGuideHud(*draw, frame.min, frame.size);
    hud_.DrawCombatFeedback(frame, draw);
    hud_.DrawStatus(frame, draw);
    ImGui::PopFont();
}

// 操作ヘルプを開いているときに、現在の表示領域へ説明を描く。
void GameRuntime::DrawControlsHelp()
{
    if (!gameplayMenu_.IsControlsVisible()) { return; }
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    gameplayMenu_.DrawControls(min, size);
}

// メニューの切り替え演出中は操作を一時停止し、演出の終了まで現在の画面を表示する。
bool GameRuntime::UpdateMenuAction()
{
    if (gameplayMenu_.PendingAction() == GameplayMenu::Action::None) { return false; }
    {
        MenuUi::InputBlock block;
        if (isGameOver_ || isGameClear_) { DrawPlayerDefeatOverlay(); DrawResultOverlay(); }
        else { DrawPauseOverlay(); DrawControlsHelp(); }
    }
    if (gameplayMenu_.UpdateTransition() == GameplayMenu::Action::Resume) {
        DrawHud();
    }
    // 完了フレームも世界は止める。次フレームから初めて敵・弾・入力を進める。
    return true;
}

// ポーズ中にHPやスコアを添えたメニューを表示する。
void GameRuntime::DrawPauseOverlay()
{
    if (!gameplayMenu_.IsPaused()) { return; }
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    gameplayMenu_.DrawPause(input_, min, size, combatProgress_.Score(), GetPlayerHp(), GetPlayerMaxHp());
}

// 本編クリア後の待ち時間が終わり、リザルト用の3D演出を描ける状態か返す。
bool GameRuntime::HasResultPresentation() const
{
    return isGameClear_ && !IsTutorial() && !stageResults_.IsWaiting() && stageResults_.HasClearPresentation();
}

// 結果が表示可能になってから、戦績・再挑戦などをリザルトへ渡す。練習では課題の結果画面を選ぶ。
void GameRuntime::DrawResultOverlay()
{
    if ((!isGameOver_ && !isGameClear_) || stageResults_.IsWaiting()) {
        stageResults_.Hide();
        return;
    }

    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    const StageResults::View view{
        .input = input_,
        .menu = gameplayMenu_,
        .min = min,
        .size = size,
        .deltaSeconds = dxCommon_ ? dxCommon_->GetDeltaTime() : 1.0f / 60.0f,
        .aspectRatio = dxCommon_ ? dxCommon_->GetPresentationAspectRatio() : 1.0f,
    };
    if (IsTutorial() && isGameClear_) {
        stageResults_.DrawTutorial(view);
        return;
    }

    const StageResultMenu::Stats stats{
        .score = combatProgress_.Score(),
        .maxChain = combatProgress_.MaxChainCount(),
        .damageCount = combatResolver_.Stats().playerDamage,
        .defeatedCount = combatResolver_.Stats().defeated,
        .targetCount = GetTotalEnemyTargetCount() + (stageEnemies_.BossSpawned() ? 1 : 0),
        .waveNumber = std::clamp(stageFlight_.WaveIndex() + 1, 1, kWaveCount),
        .waveCount = kWaveCount,
        .shots = playerControl_.ShotsFired(),
        .hits = combatResolver_.Stats().directHits,
        .justDodges = rewards_.JustDodgeCount(),
        .escapedCount = stageEnemies_.EscapedCount(),
        .ringPasses = ringGates_.PassCount(),
        .ringSpawns = ringGates_.SpawnCount(),
        .elapsedSeconds = gameplayElapsedSeconds_,
    };
    stageResults_.Draw(view, stats, isGameOver_);
}

// 敵が退場するときに参照を解除できるよう、敵を追っている照準・弾・スキルをまとめて渡す。
StageEnemies::TargetReferences GameRuntime::EnemyTargetReferences()
{
    return { playerSkill_, projectiles_, playerControl_.Aim() };
}

// そのフレームの進行距離と時間倍率を敵へ渡す。逃走した敵が出たら撃破チェインを途切れさせる。
void GameRuntime::UpdateEnemies()
{
    const StageEnemies::Frame frame{
        .railDistance = stageFlight_.Distance(),
        .worldTimeScale = GetCinematicWorldTimeScale(),
        .tutorial = IsTutorial(),
        .trainingTelegraphActive = tutorial_.IsDodgeTelegraphActive(),
        .trainingChargeRate = tutorial_.DodgeTelegraphRate(),
        .bossAttack = bossAttack_
    };
    if (stageEnemies_.Update(frame, EnemyTargetReferences())) {
        combatProgress_.BreakChain();
    }
}

// スキルとジャスト回避などのスローを、敵や弾に使う共通の時間倍率にまとめる。
float GameRuntime::GetCinematicWorldTimeScale() const
{
    return playerCombatFeedback_.WorldTimeScale(playerSkill_.WorldTimeScale());
}

// 発動時にカメラや速度演出へ足す、短い加速の強さを返す。
float GameRuntime::GetFeverActivationKick() const
{
    const float phase = std::clamp(static_cast<float>(FeverState::kActivationFlashFrames -
        rewards_.Fever().ActivationFlashFrames()) / 28.0f, 0.0f, 1.0f);
    return rewards_.Fever().ActivationFlashFrames() > 0 ? std::sin(phase * std::numbers::pi_v<float>) : 0.0f;
}

// リングの加速、回避スローの解除、フィーバーなどのうち、最も強い値を速度演出に使う。
float GameRuntime::GetFlightBoostKick() const
{
    if (isGameOver_ || isGameClear_) { return 0.0f; }
    const float dodgeRelease = playerCombatFeedback_.DodgeRelease() * 0.55f;
    return (std::max)(dodgeRelease, (std::max)(ringGates_.BoostStrength(), IsTutorial() ? 0.0f :
        gameplayCamera_.BoostKick(GetFeverActivationKick(), stageFlight_.ReleaseKick())));
}

// 現在のモードでリザルトに表示する撃破対象の総数を返す。
int GameRuntime::GetTotalEnemyTargetCount() const
{
    return stageFlight_.Schedule().EnemyCount();
}

// 自機の位置、レール速度、画面効果の強さをまとめ、現在モードの追従カメラを計算する。
void GameRuntime::UpdateGameCamera()
{
    if (!camera_ || !player_) { return; }
    GameplayCamera::Frame frame{};
    frame.playerPosition = player_->GetTranslate();
    frame.deltaSeconds = dxCommon_ ? dxCommon_->GetDeltaTime() : 1.0f / 60.0f;
    frame.railDistance = stageFlight_.Distance();
    frame.railSpeed = stageFlight_.Speed();
    frame.speedRate = stageFlight_.SpeedRate();
    frame.worldScale = GetCinematicWorldTimeScale();
    frame.fever = rewards_.Fever().SpeedEffectRate();
    frame.activationKick = GetFeverActivationKick();
    frame.releaseKick = stageFlight_.ReleaseKick();
    frame.gateBoost = ringGates_.BoostStrength();
    frame.justDodge = playerCombatFeedback_.DodgeRate();
    frame.impact = playerCombatFeedback_.ImpactRate();
    const auto& bias = stageFlight_.Bias();
    frame.yawBias = bias.yaw;
    frame.rollBias = bias.roll;
    frame.liftBias = bias.lift;
    frame.fovBoost = bias.fov;
    frame.dodgeDirection = player_->GetDodgeDirection();
    frame.dodging = player_->IsDodging();
    frame.gameOver = isGameOver_;
    frame.gameClear = isGameClear_;
    gameplayCamera_.Update(frame, IsTutorial() ? GameplayCamera::Mode::Tutorial : GameplayCamera::Mode::Flight);
    ApplyGameplayCameraPose();
}

// 計算したカメラ位置・回転・画角を実際のCameraへ設定し、描画用行列を更新する。
void GameRuntime::ApplyGameplayCameraPose()
{
    if (!camera_) { return; }
    const auto& pose = gameplayCamera_.DisplayPose();
    camera_->SetTranslate(pose.position);
    camera_->SetRotate(pose.rotation);
    camera_->SetFovY(pose.fov);
    camera_->Update();
}

// 揺れの強さと継続フレーム数をカメラへ伝える。
void GameRuntime::AddCameraShake(float power, int duration)
{
    gameplayCamera_.AddShake(power, duration);
}
