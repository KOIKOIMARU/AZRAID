// 本編とチュートリアルの入口　入力、飛行、戦闘、描画の更新順を管理し、具体的な処理は各担当クラスへ渡す

#pragma once

#include "app/combat/Bullet.h"
#include "app/combat/ProjectileSystem.h"
#include "app/enemy/Enemy.h"
#include "app/enemy/EnemyAttackPatterns.h"
#include "app/player/Player.h"
#include "app/player/PlayerControl.h"
#include "app/player/PlayerSkill.h"
#include "app/player/PlayerSkillPresentation.h"
#include "app/player/PlayerFlightEffects.h"
#include "app/player/PlayerCombatFeedback.h"
#include "app/ui/GameplayHud.h"
#include "app/stage/TutorialProgress.h"
#include "app/stage/StageFlight.h"
#include "app/enemy/BossAttackController.h"
#include "app/enemy/BossPresentation.h"
#include "app/combat/CombatProgress.h"
#include "app/ui/StageResults.h"
#include "app/combat/CombatEffects.h"
#include "app/combat/CombatResolver.h"
#include "app/combat/CombatRewards.h"
#include "app/stage/RailScenery.h"
#include "app/stage/RingGates.h"
#include "app/presentation/FlightMotionBlur.h"
#include "app/presentation/GameplayAudio.h"
#include "app/stage/StageEntryTiming.h"
#include "app/stage/StageEntryPresentation.h"
#include "app/enemy/StageEnemies.h"
#include "app/presentation/GameplayCamera.h"
#include "app/runtime/GameplaySceneSetup.h"
#include "app/player/PlayerDefeatPresentation.h"
#include "app/presentation/FlightSpeedEffects.h"
#include "app/presentation/GameplayShadows.h"
#include "app/ui/GameplayMenu.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "app/runtime/RuntimeSceneObjects.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

class DirectXCommon;
class ImGuiManager;
class Input;
class Skybox;
class SpriteCommon;
class SrvManager;
struct ImDrawList;

// GameSceneから呼ばれ、各担当クラスと戦闘・演出の更新順を取りまとめる
class GameRuntime : private CombatResolver::Reactions {
public:
    enum class PlayMode { Game, Tutorial };
    GameRuntime();
    ~GameRuntime();

    using InitializationTiming = GameplaySceneSetup::Timing;
    static void ResetInitializationTiming() { GameplaySceneSetup::ResetTiming(); }
    static InitializationTiming GetInitializationTiming() { return GameplaySceneSetup::GetTiming(); }

    // ゲームを動かす前に、シーン管理側が持つ描画と入力の機能を受け取る
    void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input);
    void Initialize(PlayMode mode = PlayMode::Game);
    void BeginInitialize(PlayMode mode);
    bool InitializeStep();
    float GetInitializationProgress() const;
    void Finalize();
    void Update();
    void Draw();
    void SetStageRecords(StageRecords* records) { stageResults_.SetRecords(records); }
    // タイトルで準備した同じ本編を、入力・戦闘を進めず出撃させる
    static constexpr float kTitleEntryDuration = StageEntryTiming::kTitleEntryDuration;
    static constexpr float kTitleEntryWorldTime = StageEntryTiming::kTitleEntryWorldTime;
    static constexpr float kTitleEntryDiveTime = StageEntryTiming::kTitleEntryDiveTime;
    static constexpr float kStageFlybyFollowTime = StageEntryTiming::kStageFlybyFollowTime;
    static constexpr float kStagePortalBoostTime = StageEntryTiming::kStagePortalBoostTime;
    static constexpr float kStagePortalTransitDuration = StageEntryTiming::kStagePortalTransitDuration;
    static constexpr float kStagePortalClearTime = StageEntryTiming::kStagePortalClearTime;
    static constexpr float kStageStartFlightEnd = StageEntryTiming::kStageStartFlightEnd;
    static constexpr float kStageStartOverviewDuration = StageEntryTiming::kStageStartOverviewDuration;
    static constexpr float kStageStartLaunchDuration = StageEntryTiming::kStageStartLaunchDuration;
    static constexpr float kStagePatrolDuration = StageEntryTiming::kStagePatrolDuration;
    static constexpr float kRetryEntryDuration = StageEntryTiming::kRetryEntryDuration;
    static constexpr std::array<float,2> kStagePatrolCutTimes = StageEntryTiming::kStagePatrolCutTimes;
    static Math::Vector3 GetStagePatrolPosition(float elapsed, Math::Vector2 seam = {500.0f,-0.19f});
    void SetStagePortalSeam(Math::Vector2 seam) { stageEntryPresentation_.SetPortalSeam(seam); }
    void UpdateStagePreview(float elapsed);
    void UpdateStageReturn(float elapsed);
    float GetStageReturnDuration() const { return stageEntryPresentation_.ReturnDuration(); }
    void UpdateStageOverview(float elapsed);
    void DrawStageBriefing(float elapsed);
    Camera* GetStageCamera() const { return camera_.get(); }
    const Math::Matrix4x4& GetStagePortalFrame() const { return stageEntryPresentation_.PortalFrame(); }
    void BeginRetryEntry();
    void BeginTitleEntry(const Math::Vector3& viewPosition, const Math::Vector3& rotation,
        const Math::Vector3& cameraRotation, float titleScale);
    void UpdateTitleEntry(float time);
    void DrawTitleEntryFlight();
    bool IsTitleEntryForeground() const;
    void DrawTitleEntryHud();
    void DrawMenuArrivalHud() { DrawHud(); }
    bool HasMenuSceneTransition() const { return gameplayMenu_.SceneAction() != GameplayMenu::Action::None; }
    void SetHudViewportRect(bool isEnabled, const Math::Vector2& min, const Math::Vector2& size);
    void SetRenderingOptions(bool showSkybox, int postEffectMode);
    int GetPostEffectMode() const;
    const Math::Matrix4x4& GetProjectionMatrix() const;
    bool IsExitRequested() const { return editorExitRequested_ || gameplayMenu_.SceneAction() == GameplayMenu::Action::Exit; }
    bool IsRetryRequested() const { return gameplayMenu_.SceneAction() == GameplayMenu::Action::Retry; }
    bool IsMainGameRequested() const { return gameplayMenu_.SceneAction() == GameplayMenu::Action::MainGame; }
    bool IsTutorial() const { return playMode_ == PlayMode::Tutorial; }
    int GetPlayerHp() const;
    int GetPlayerMaxHp() const { return 100; }
#ifdef _DEBUG
    // 起動時に指定した自動検証の入口　検証ごとの操作と判定は実装ファイルに分けている
    bool RunPlaythroughProbe(const std::string& logPath, bool tutorialPreview = false, bool tutorialDeparture = false);
    bool RunTutorialProbe(const std::string& logPath, bool preview = false, bool departure = false);
    bool RunPhantomProbe(const std::string& logPath, bool preview = false);
    bool RunChargeShotProbe(const std::string& logPath, bool preview = false);
    bool RunJustDodgeProbe(const std::string& logPath, bool preview = false);
    bool RunRiftProbe(const std::string& logPath, bool preview = false);
    bool RunBossProbe(const std::string& logPath, bool preview = false);
    bool RunDefeatProbe(const std::string& logPath, bool preview = false);
#endif

private:
    // リングの配置と通過を管理し、通過時に加速と報酬を適用する
    RingGates ringGates_;
    void ResetRifts();
    void SpawnRift(const Math::Vector3& position);
    void UpdateRifts(const Math::Vector3& previous, const Math::Vector3& current);
    void ApplyRingPassReward(const Math::Vector3& position);
    void DrawRiftObjects();
    bool CanShowBossRadio() const { return !IsTutorial() && !isGameOver_ && !isGameClear_; }
    void RequestBossRadio();
    void UpdateBossRadio(float deltaSeconds);
#ifdef _DEBUG
    bool phantomPreviewPaused_ = false; // 明示的な映像確認テストだけで演出をコマ止めする
#endif
    // Qキーで使うスキルの開始・連撃・終了を、敵や描画とつなぐ
    void ResetPhantomRaid();
    void GrantPhantomRaid();
    bool TryActivatePhantomRaid();
    void UpdatePhantomRaid();
    bool IsPhantomRaidActive() const { return playerSkill_.IsActive(); }
    PlayerSkill playerSkill_;
    PlayerSkillPresentation playerSkillPresentation_;
    // keyは効果音の識別名　pitchは再生周波数の倍率
    void PlaySfx(const char* key, float pitch = 1.0f) override;
    void UpdateMusic();
    GameplayAudio audio_;
    PlayMode playMode_ = PlayMode::Game; // 入場時に確定　本編と練習の進行・成績を混在させない
    TutorialProgress tutorial_;
    TutorialProgress::Stats ReadTutorialStats() const;
    void BeginTutorialLesson(TutorialProgress::Lesson lesson);
    void UpdateTutorialLesson();
    void UpdateTutorialAttack();
    void SpawnTutorialTargets(int count, int hp);
    void SpawnTutorialRing();
    bool HasActiveTutorialRing() const;
    bool TutorialAllowsShooting() const { return !IsTutorial() || tutorial_.AllowsShooting(); }
    bool TutorialAllowsRings() const { return !IsTutorial() || tutorial_.AllowsRing(); }

    using EnemyBulletStyle = ProjectileSystem::EnemyStyle;

    void FireEnemyBullet(
        const Math::Vector3& position,
        EnemyBulletStyle style = EnemyBulletStyle::Standard,
        Math::Vector2 aimOffset = {},
        const Math::Vector3* fixedAim = nullptr);
    FlightSpeedEffects::Frame BuildFlightSpeedEffectsFrame(bool useViewport) const;
    void UpdateDepthCueEffects();
    void PrepareStageFrame();
#ifdef _DEBUG
    Enemy* SpawnStageEnemyForProbe(
        float x,
        float y,
        float leadDistance,
        Enemy::Behavior behavior,
        Enemy::EntryStyle entryStyle,
        int hp = 0,
        float scale = 1.0f);
#endif
    void SpawnBossEnemy();
    void UpdateStageEnemyEvents();
    void UpdatePlayerAim();
    PlayerControl::AimView GetPlayerAimView() const;
    // ワールド上の点をHUDの座標へ変換する　カメラがない場合や投影できない点ではfalseを返す
    bool TryProjectToScreen(
        const Math::Vector3& worldPosition,
        Math::Vector2& screenPosition) const;
    void AddEnemyHitEffect(
        const Math::Vector3& worldPosition,
        float strength = 1.0f, bool fever = false);
    void RewardJustDodge(const Math::Vector3& bulletPosition) override;
    void TriggerPlayerImpactMoment(
        bool isCharged,
        bool isBossHit,
        bool isDestroyed) override;
    // 命中位置を画面へ写してHUDに印を出す　画面へ写せなかった場合は照準位置を使う
    void TriggerHitConfirm(
        const Math::Vector3& worldPosition,
        bool isCharged,
        bool isBossHit,
        bool isDestroyed) override;
    void TriggerPlayerDamageFeedback(
        const Math::Vector3& worldPosition,
        const Math::Vector3& incomingVelocity) override;
    // 通常戦闘ではポーズとヘルプ、リザルト表示後はRの再挑戦を処理する　戻り値trueは開発用キーを処理したため今回の更新を抜ける合図
    bool HandleRuntimeShortcuts();
    bool UpdateRetryEntry();
    StageEntryPresentation::FlightInput GetEntryFlightInput() const;
    void UpdateEntryWorld(bool updateGameplayOverlays);
    void BeginPlayerDefeat(const Math::Vector3& incomingVelocity) override;
    void UpdatePlayerDefeat();
    void DrawPlayerDefeatOverlay();
    void UpdateRailProgress();
    void UpdatePlayerAndCamera();
    void UpdateFever();
    void ShowFeverActivation();
#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
    void ActivateFever();
#endif
#ifdef _DEBUG
    void AddFeverGauge(int amount);
#endif
    bool CanEarnFever() const;
    void UpdateDefeatChain();
    // 通常戦闘の更新順に対応する処理　移動を終えてから射撃し、弾と敵を更新して衝突を判定する
    void UpdatePlayerShooting();
    void UpdateEnemyActions();
    void UpdateBossActions();
    void UpdateWorldEntities();
    void UpdateGameplayCollisions();
    void UpdateResultAndSceneObjects();
    void UpdateHitEffects();
    void UpdatePlayerDodgeAfterimages();
    void UpdatePlayerFlightEffects();
    void DrawPlayerFlightEffects();
    void DrawFlightSpeedOverlay(); // 中央の照準を避け、周辺だけに薄い風の線を出す
    GameplayHud::Frame BuildHudFrame() const;
    void DrawHud();
    void DrawTutorialGuideHud(ImDrawList& draw, const Math::Vector2& min, const Math::Vector2& size);
    void DrawControlsHelp();
    void DrawPauseOverlay();
    bool UpdateMenuAction();
    void DrawResultOverlay();
    bool HasResultPresentation() const;
#ifdef ENABLE_DEBUG_GUI
    void DrawEditorOverlay();
    void DrawPerformanceOverlay();
    void DebugJumpToStagePhase(int phaseIndex);
    void ApplyDebugStartPhase();
    bool SaveSceneObjects(const char* path);
#endif
    void GetEffectiveHudViewportRect(Math::Vector2& min, Math::Vector2& size) const;
    StageEnemies::TargetReferences EnemyTargetReferences();
    void UpdateEnemies();
    float GetCinematicWorldTimeScale() const;
    float GetFlightBoostKick() const; // カメラ・風・噴射で共有する短い加速の立ち上がり
    int GetTotalEnemyTargetCount() const;
    CombatResolver::Context BuildCombatContext();
    void OnBossDefeated(const Math::Vector3& position) override;
    void UpdateGameCamera();
    void ApplyGameplayCameraPose();
    float GetFeverActivationKick() const;
    // 揺れの強さと継続フレーム数をカメラへ伝える
    void AddCameraShake(float power, int duration) override;
    GameplaySceneSetup::Targets SceneSetupTargets();

    // シーンから借りている共通機能　初期化で渡され、GameRuntimeはこれらの実体を所有しない
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;

    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Player> player_;
    RuntimeSceneObjects sceneObjects_;
    ProjectileSystem projectiles_; // 自機弾と敵弾の発射・更新・回収を管理する処理
    StageEnemies stageEnemies_; // ステージ上の敵の生成と破棄を管理する処理
    CombatEffects combatEffects_;
    RailScenery railScenery_;
    FlightSpeedEffects flightSpeedEffects_;
    PlayerFlightEffects playerFlightEffects_;
    GameplayShadows gameplayShadows_;
    StageFlight stageFlight_;

    GameplaySceneSetup sceneSetup_;

    PlayerControl playerControl_;
    EnemyAttackPatterns enemyAttackPatterns_;
    BossPresentation bossPresentation_;
    BossAttackController bossAttack_;
    int bossShotsFired_ = 0; // 通し試験で「登場しただけで倒される」状態を検出する実発射数
    CombatProgress combatProgress_;
    GameplayHud hud_;
    CombatResolver combatResolver_; // 当たり判定とダメージを反映する処理
    PlayerDefeatPresentation playerDefeatPresentation_;
    CombatRewards rewards_;
    PlayerCombatFeedback playerCombatFeedback_;
    int enemyShotInterval_ = 64;
    float gameplayElapsedSeconds_ = 0.0f;
    FlightMotionBlur flightMotionBlur_;
    GameplayCamera gameplayCamera_; // 行列はCamera、追従・揺れの履歴はこのクラスが担当する
    StageEntryPresentation stageEntryPresentation_;
    float enemyBulletSpeed_ = 0.36f;
    Math::Vector2 hudViewportMin_{ 0.0f, 0.0f };
    Math::Vector2 hudViewportSize_{ 0.0f, 0.0f };
    Math::Vector2 editorOverlayViewportMin_{ 0.0f, 0.0f };
    Math::Vector2 editorOverlayViewportSize_{ 0.0f, 0.0f };
    std::string editorStatusMessage_{ "Ready." };
    int postEffectMode_ = 12;
    bool isGameOver_ = false;
    bool isGameClear_ = false;
    GameplayMenu gameplayMenu_;
    StageResults stageResults_;
    bool isEditorOverlayVisible_ = false;
    bool isPerformanceOverlayVisible_ = false;
    bool isPostEffectBypassEnabled_ = false;
    bool editorExitRequested_ = false;
    bool showSkybox_ = true;
    bool isHudViewportRectEnabled_ = false;
    bool hasEditorOverlayViewportRect_ = false;
};
