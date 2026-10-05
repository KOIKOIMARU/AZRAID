#pragma once

#include "app/Bullet.h"
#include "app/Enemy.h"
#include "app/Player.h"
#include "engine/3d/Camera.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/scene/SceneSerializer.h"

#include <array>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class DirectXCommon;
class ImGuiManager;
class Input;
class Model;
class Skybox;
class SpriteCommon;
class SrvManager;
class SoundManager;

class GameRuntime {
public:
    enum class PlayMode { Game, Tutorial };
    GameRuntime();
    ~GameRuntime();

    static bool PreloadSharedResourceStep(DirectXCommon* dxCommon, SrvManager* srvManager);
    static bool AreSharedResourcesPreloaded();
    static int GetSharedResourcePreloadStep();
    static int GetSharedResourcePreloadStepCount();
    static const char* GetSharedResourcePreloadLabel();
    static float GetSharedResourceLastStepMs();
    static float GetSharedResourceTotalMs();

    void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input);
    void Initialize(PlayMode mode = PlayMode::Game);
    void Finalize();
    void Update();
    void Draw();
    void SetHudViewportRect(bool isEnabled, const Math::Vector2& min, const Math::Vector2& size);
    void SetRenderingOptions(bool showSkybox, int postEffectMode);
    int GetPostEffectMode() const;
    const Math::Matrix4x4& GetProjectionMatrix() const;
    bool IsExitRequested() const { return isExitRequested_; }
    bool IsRetryRequested() const { return isRetryRequested_; }
    bool IsMainGameRequested() const { return isMainGameRequested_; }
    bool IsTutorial() const { return playMode_ == PlayMode::Tutorial; }
    int GetPlayerHp() const;
    int GetPlayerMaxHp() const { return 100; }
#ifdef _DEBUG
    bool RunPlaythroughProbe(const std::string& logPath, bool tutorialPreview = false);
    bool RunTutorialProbe(const std::string& logPath, bool preview = false);
    bool RunPhantomProbe(const std::string& logPath, bool preview = false);
    bool RunChargeShotProbe(const std::string& logPath, bool preview = false);
    bool RunRiftProbe(const std::string& logPath, bool preview = false);
    bool RunBossProbe(const std::string& logPath, bool preview = false);
#endif

private:
    struct RiftGate {
        std::unique_ptr<Object3d> rim;
        std::unique_ptr<Object3d> glow;
        Math::Vector3 position{}; // 撃破地点に固定。レールや自機へ追従させない。
        float age = 0.0f;
        float lifetime = 0.0f;
        bool active = false;
    };
    static constexpr float kRiftRadiusX = 1.65f;
    static constexpr float kRiftRadiusY = 1.30f;
    static constexpr int kRiftGaugeReward = 22;
    static constexpr int kRiftFeverRecoveryFrames = 90;
    std::array<RiftGate, 8> rifts_{}; // 起動時だけ確保し、撃破・通過時には生成しない。
    int riftSpawnCount_ = 0;
    int riftPassCount_ = 0;
    int riftNoticeTimer_ = 0;
    int riftRecoveredFrames_ = 0;
    void InitializeRifts();
    void ResetRifts();
    void SpawnRift(const Math::Vector3& position);
    void UpdateRifts(const Math::Vector3& previous, const Math::Vector3& current);
    void DrawRiftObjects();
    void DrawRiftHud();
#ifdef _DEBUG
    bool phantomPreviewPaused_ = false; // 明示的な映像確認テストだけで演出をコマ止めする。
#endif
    struct PhantomTarget {
        Enemy* enemy = nullptr; // 毎回enemies_への所属を確認してから参照する。
        Math::Vector3 position{}; // 最新の狙い位置。敵が消えた後も斬撃の表示位置として保持。
        int marks = 0; // 雑魚への斬撃は刻印し、最後にまとめてダメージを与える。
    };
    struct PhantomSlash {
        std::unique_ptr<Object3d> ghost; // 使い回す自機の残像モデル。
        Math::Vector3 position{}; // この一撃が命中したワールド座標。
        float angle = 0.0f; // 画面上の斬る方向。ラジアン。
        float age = -1.0f; // 60fps換算の経過時間。負値は非表示。
    };
    void InitializePhantomRaid();
    void InitializePhantomAudio();
    void ResetPhantomRaid();
    void GrantPhantomRaid();
    void RecoverPhantomRaidOnHit(bool charged, bool destroyed);
    bool TryActivatePhantomRaid();
    void UpdatePhantomRaid();
    void DrawPhantomRaidObjects();
    void DrawPhantomRaidOverlay();
    Enemy* FindPhantomTarget(const Enemy* target) const;
    void DealPhantomDamage(Enemy& enemy, int damage);
    bool IsPhantomRaidActive() const { return phantomClock_ >= 0.0f; }
    float PhantomFinisherTime() const { return 12.0f + static_cast<float>(phantomStrikeCount_) * 7.0f; }
    std::array<PhantomTarget, 5> phantomTargets_{};
    std::array<PhantomSlash, 5> phantomSlashes_{}; // 起動時に確保し、技の最中には生成しない。
    static constexpr float kPhantomCooldownFrames = 8.0f * 60.0f;
    bool phantomReady_ = true; // 開幕から使用可能。使用後は時間経過で自動回復する。
    bool phantomEmpowered_ = false; // 発動した瞬間のフィーバー状態を固定。
    bool phantomFinished_ = false; // 最後の一撃を二重に処理しないための状態。
    float phantomClock_ = -1.0f; // 60fps換算の演出時間。ワールドのスローに巻き込まない。
    float phantomCooldown_ = 0.0f; // 再使用までの時間。60fps換算。射撃命中で短縮する。
    float phantomReadyFlash_ = 0.0f;
    float phantomNoTargetNotice_ = 0.0f;
    int phantomTargetCount_ = 0;
    int phantomStrikeCount_ = 3;
    int phantomNextStrike_ = 0;
    int phantomActivationCount_ = 0;
    int phantomDefeatCount_ = 0;
    Math::Vector3 phantomFinishPosition_{};
    Math::Vector3 phantomModelCenter_{};
    void PlaySfx(const char* key, float pitch = 1.0f);
    void UpdateMusic();
    std::unique_ptr<SoundManager> sound_;
    bool phantomLocalAudio_ = false; // Git管理外の試聴用バンクが全部読めた場合だけ有効。
#ifdef _DEBUG
    std::array<uint32_t, 2> phantomAudioPlays_{}; // 実際に再生できた斬撃・決め技。連撃試験用。
#endif
    float sfxAccentUntil_ = 0.0f; // 決め技・被弾・フィーバーの直後に通常SEを抑える終了時刻（秒）。
    std::array<float, 3> musicLevels_{}; // 通常・ボス・フィーバーのクロスフェード音量。
    int musicTrack_ = -1; // -1は結果画面などの無音状態。
    bool resultSoundPlayed_ = false;
    PlayMode playMode_ = PlayMode::Game; // 入場時に確定。本編と練習の進行・成績を混在させない。
    enum class TutorialLesson { Move, Shoot, Dodge, Charge, Skill, Fever, Complete };
    struct TutorialState {
        TutorialLesson lesson = TutorialLesson::Move;
        float elapsed = 0.0f; // 現在の練習の経過時間。60fps換算、ポーズ中は止める。
        float success = 0.0f; // 成功表示の残り時間。終了後に次の練習へ移る。
        float markerHold = 0.0f; // 移動先に自機が重なった時間。通り過ぎても少し余裕を持つ。
        float attackTimer = 120.0f;
        float feverExperience = 0.0f;
        int progress = 0; // 移動地点数、撃破数、回避成功数など各練習の実績。
        int defeatedAtStart = 0;
        int skillAtStart = 0;
        int feverDefeatedAtStart = -1;
        bool chargedHit = false; // 発射ではなく、チャージ弾の直撃を確認する。
        bool dodgeConfirmed = false;
        bool started = false;
    } tutorial_;
    void BeginTutorialLesson(TutorialLesson lesson);
    void UpdateTutorialLesson();
    void UpdateTutorialAttack();
    void SpawnTutorialTargets(int count, int hp);
    void CompleteTutorialLesson();
    bool TutorialAllowsShooting() const;
    Math::Vector3 GetTutorialMoveTarget() const;
    void DrawTutorialResult();
    enum class HitEffectType {
        EnemyImpact,
        EnemyDestroy,
        RewardCollect,
        PlayerDamage
    };

    struct HitEffect {
        Math::Vector3 worldPosition{};
        float age = 0.0f;
        int duration = 24;
        float strength = 1.0f;
        int scoreValue = 0;
        HitEffectType type = HitEffectType::EnemyDestroy;
        struct Visual {
            std::unique_ptr<Object3d> object;
            Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
            float baseSize = 1.0f;
            float growth = 1.0f;
            float spin = 0.0f;
            float popDelay = 0.0f;
            float lifeEnd = 1.0f; // 閃光・衝撃波・煙を別々の時間で収束させる。
            float roll = 0.0f;
            float drag = 0.0f;
            float aspectX = 1.0f;
            float aspectY = 1.0f;
            Math::Vector3 velocity{};
            bool additive = true;
        };
        static constexpr size_t kMaxVisuals = 24;
        std::array<Visual, kMaxVisuals> visuals{};
        size_t visualCount = 0;
    };

    struct PlayerDodgeAfterimage {
        std::unique_ptr<Object3d> object;
        Math::Vector3 position{};
        int direction = 1;
        float age = 0.0f;
        float duration = 16.0f;
        bool isActive = false;
    };

    struct PlayerFlightAura {
        std::unique_ptr<Object3d> object;
        Model* model = nullptr;
        Math::Vector3 offset{};
        Math::Vector4 color{ 0.70f, 0.95f, 1.0f, 0.16f };
        float baseSize = 1.0f;
        float aspectX = 1.0f;
        float aspectY = 1.0f;
        float pulseOffset = 0.0f;
        float roll = 0.0f;
        float rollSpeed = 0.0f;
    };

    struct PlayerExhaustParticle {
        std::unique_ptr<Object3d> object;
        Model* model = nullptr;
        Math::Vector3 position{};
        Math::Vector3 velocity{};
        Math::Vector4 color{ 0.35f, 0.82f, 1.0f, 0.0f };
        float age = 0.0f;
        float lifetime = 0.24f;
        float startSize = 0.24f;
        float endSize = 0.04f;
        float aspectX = 1.0f;
        float aspectY = 1.0f;
        float roll = 0.0f;
        float rollSpeed = 0.0f;
        bool isActive = false;
    };

    struct ContactShadow {
        std::unique_ptr<Object3d> object;
    };

    struct RewardHeart {
        std::unique_ptr<Object3d> object;
        Math::Vector3 position{};
        Math::Vector3 velocity{};
        float collisionRadius = 0.42f;
        float age = 0.0f;
        float collectDelay = 12.0f;
        float life = 180.0f;
        float phase = 0.0f;
        float baseScale = 0.34f;
        int scoreValue = 25;
        bool isActive = false;
    };

    struct RailSceneryObject {
        std::unique_ptr<Object3d> object;
        Math::Vector3 anchor{};
        Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };
        Math::Vector3 rotate{};
        Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        float loopLength = 120.0f;
        float speedMultiplier = 1.0f;
        float lateralDrift = 0.0f;
        float verticalDrift = 0.0f;
        float driftSpeed = 0.02f;
        float rollSpeed = 0.0f;
        float curveInfluence = 0.0f;
        float phase = 0.0f;
        float currentLocalZ = 0.0f;
        float drawFarLocalZ = 300.0f;
        bool isVisible = true;
        bool isInView = true; // 色描画だけのカメラ判定。画面外の施設が落とす影は別に残す。
        bool castsShadow = false; // 不透明な構造物だけを投影し、路面と別モデルの窓は除外する。
        Math::Vector3 boundsMin{};
        Math::Vector3 boundsMax{};
        bool billboard = false;
        bool isBuilding = false; // 街区ごとに幅と高さを変える対象。元のanchor/scaleは保持する。
        bool isRoad = false;
        bool isBackRow = false; // 奥の建物は道路沿いより外側へ置き、空の輪郭を作る。
        bool isLandmark = false; // 一度だけ通過する施設。道路のようにループ再配置しない。
        bool isTrackside = false; // 飛行域の外側に置く低い設備。近景の流れで速度を伝える。
        bool isTerrain = false; // 敷地や道路の隙間から空を見せない連続した不透明地表。
        bool isDefenseDistrict = false; // 中盤の固定間隔の敷地。再配置は視界の外だけで行う。
        bool isBossApron = false; // 終盤の広い航空エプロン。中央の飛行域に構造物を置かない。
        bool isRoadDetail = false; // 車道用の排水溝・マンホール。航空エプロンには持ち込まない。
        float halfDepth = 0.0f; // 大きな施設の前後端まで含めた可視・影の判定範囲。
    };

    struct DepthCueEffect {
        std::unique_ptr<Object3d> object;
        Model* model = nullptr;
        Math::Vector3 anchor{};
        Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 0.35f };
        float loopLength = 120.0f;
        float speedMultiplier = 1.0f;
        float lateralDrift = 0.0f;
        float verticalDrift = 0.0f;
        float driftSpeed = 0.02f;
        float phase = 0.0f;
        float baseScale = 0.18f;
        float aspectX = 1.0f;
        float aspectY = 1.0f;
        float spinSpeed = 0.0f;
    };

    struct WaveTuning {
        int enemyCount = 6;
        int spawnInterval = 80;
        float spawnLeadDistance = 26.0f;
    };

    enum class EnemyBulletStyle {
        Standard,
        Crossfire,
        Sniper,
        ShieldOrb,
        BossCannon,
        BossCharge
    };

    void FirePlayerBullet();
    void FireEnemyBullet(
        const Math::Vector3& position,
        EnemyBulletStyle style = EnemyBulletStyle::Standard,
        Math::Vector2 aimOffset = {},
        const Math::Vector3* fixedAim = nullptr);
    void UpdateEnemyAttackPatterns(bool hasSupportDrone);
    void SpawnEnemy();
    void InitializeRewardHearts();
    void SpawnRewardHearts(const Math::Vector3& worldPosition, int count);
    void UpdateRewardHearts();
    void InitializeDepthCueEffects();
    void UpdateDepthCueEffects();
    void DrawDepthCueEffects();
    void InitializeRailScenery();
    void UpdateRailScenery();
    Math::Vector2 GetSceneryDistrictWeights(float worldZ) const;
    float GetBossApronStartZ() const;
    void RenderShadowMap();
    void DrawRailScenery(ModelDrawPass drawPass);
    void InitializeContactShadows();
    void DrawContactShadows();
    void DrawContactShadowObject(
        Object3d& object,
        const Math::Vector3& position,
        float scaleX,
        float scaleZ,
        float alpha,
        float yaw);
    void UpdateStageDirector();
    void SpawnStageEnemy(
        float x,
        float y,
        float leadDistance,
        Enemy::Behavior behavior,
        Enemy::EntryStyle entryStyle,
        int maxHpOverride = 0,
        float scaleMultiplier = 1.0f);
    Model* GetEnemyModelForBehavior(Enemy::Behavior behavior) const;
    const char* GetEnemyTextureOverrideForBehavior(Enemy::Behavior behavior) const;
    void SpawnBossEnemy();
    void UpdateStageEnemyEvents();
    void UpdateEnemyWave();
    void AdvanceEnemyWaveIfCleared();
    void UpdateLockOnTarget();
    bool TryProjectToScreen(
        const Math::Vector3& worldPosition,
        Math::Vector2& screenPosition) const;
    void AddEnemyHitEffect(
        const Math::Vector3& worldPosition,
        float strength = 1.0f);
    void AddFeverEnemyHitEffect(
        const Math::Vector3& worldPosition,
        float strength = 1.0f);
    void AddEnemyImpactEffect(
        const Math::Vector3& worldPosition,
        float strength = 1.0f);
    void AddFeverEnemyImpactEffect(
        const Math::Vector3& worldPosition,
        float strength = 1.0f);
    void AddEnemyMuzzleFlashEffect(const Math::Vector3& worldPosition);
    void AddMuzzleFlashEffect(
        const Math::Vector3& worldPosition,
        bool isCharged);
    void AddRewardHeartCollectEffect(const Math::Vector3& worldPosition);
    void AddPlayerDamageEffect(const Math::Vector3& worldPosition);
    void AddPlayerDodgeGrazeEffect(const Math::Vector3& worldPosition);
    void TriggerJustDodge(Bullet& bullet, const Math::Vector3& worldPosition);
    void TriggerPlayerImpactMoment(
        bool isCharged,
        bool isBossHit,
        bool isDestroyed);
    void TriggerHitConfirm(
        const Math::Vector3& worldPosition,
        bool isCharged,
        bool isBossHit,
        bool isDestroyed);
    void TriggerPlayerDamageFeedback(
        const Math::Vector3& worldPosition,
        const Math::Vector3& incomingVelocity);
    void AddHitEffectVisual(
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
        bool additive = true,
        float lifeEnd = 1.0f,
        float roll = 0.0f,
        float drag = 0.0f);
    void AddCombatBurstVisuals(HitEffect& effect, bool fever);
    void PrewarmHitEffectObjectPool();
    std::unique_ptr<Object3d> CreatePooledHitEffectObject();
    std::unique_ptr<Object3d> AcquireHitEffectObject();
    void RecycleHitEffectVisuals(HitEffect& effect);
    bool HandleRuntimeShortcuts();
    void UpdateRailProgress();
    void UpdatePlayerAndCamera();
    void UpdateFever();
    void ActivateFever();
    void AddFeverGauge(int amount);
    void AddScore(int baseScore);
    void UpdateDefeatChain();
    void RegisterEnemyDefeatChain();
    void BreakEnemyDefeatChain();
    int GetDefeatChainScoreMultiplier() const;
    void UpdatePlayerShooting();
    void UpdateEnemyActions();
    void UpdateBossActions();
    void UpdateWorldEntities();
    void UpdateGameplayCollisions();
    void UpdateResultAndSceneObjects();
    void UpdateHitEffects();
    void DrawHitEffects();
    void InitializePlayerDodgeAfterimages();
    void SpawnPlayerDodgeAfterimage();
    void UpdatePlayerDodgeAfterimages();
    void DrawPlayerDodgeAfterimages();
    void InitializePlayerFlightAura();
    void InitializePlayerExhaustParticles();
    void InitializeGpuPlayerExhaustParticles();
    void EmitPlayerExhaustParticles(const Math::Vector3& playerPosition,
        const Math::Vector3& playerRotate);
    void UpdateAndDrawPlayerExhaustParticles();
    void DrawPlayerFlightAura();
    void DrawFeverBackdrop();
    void DrawFlightSpeedOverlay(); // 中央の照準を避け、周辺だけに薄い風の線を出す。
    void DrawEnemyTypeTelegraphs();
    void DrawHud();
    void DrawBossHud();
    void DrawStageCueHud();
    void DrawTutorialGuideHud();
    void DrawControlsHelp();
    void DrawPauseOverlay();
    void DrawFeverHud();
    void DrawDefeatChainHud();
    void DrawLockOnHud();
    void DrawHitConfirmHud();
    void DrawPlayerDamageHud();
    void DrawResultOverlay();
#ifdef ENABLE_DEBUG_GUI
    void DrawEditorOverlayGuiRich();
    void DrawPerformanceOverlay();
    void DebugJumpToStagePhase(int phaseIndex);
#endif
    void DrawBulletEffectObjects();
    void DrawHitEffectObjects();
    void GetEffectiveHudViewportRect(Math::Vector2& min, Math::Vector2& size) const;
    void UpdatePlayerBullets();
    void UpdateEnemyBullets();
    void UpdateEnemies();
    float GetCinematicWorldTimeScale() const;
    float GetFlightSpeedRate() const; // 通常飛行の速度域を0～1へ正規化。フィーバーは別で加算。
    Math::Vector3 CalculateAimDirection(const Math::Vector3& origin) const;
    const Enemy* FindHomingTargetForBullet(const Bullet& bullet) const;
    int GetTotalEnemyTargetCount() const;
    int GetRequiredEnemyDefeatsForClear() const;
    const Enemy* GetBossEnemy() const;
    void CheckBulletEnemyCollisions();
    bool DealPlayerShotDamage(Enemy& enemy, const Math::Vector3& impactPosition,
        bool charged, bool fever, bool splash = false);
    void ApplyChargeSplash(const Enemy& directTarget, const Math::Vector3& center);
    void OnEnemyDestroyed(Enemy& enemy, bool charged, bool fever);
    void CheckEnemyBulletPlayerCollisions();
    void UpdateGameCamera();
    void UpdateFlightCamera(); // 操作・加減速に追従する本編用カメラ。チュートリアルとは分離。
    void AddCameraShake(float power, int duration);
    void PrewarmBulletPools();
    std::unique_ptr<Bullet> CreatePooledPlayerBullet();
    std::unique_ptr<Bullet> CreatePooledEnemyBullet();
    std::unique_ptr<Bullet> AcquireBullet(std::vector<std::unique_ptr<Bullet>>& pool);
    std::vector<SceneSerializer::ObjectRecord> BuildRuntimeSceneRecords() const;
    SceneSerializer::SceneSettings BuildRuntimeSceneSettings() const;
    bool LoadSceneObjects(const char* path);
    bool SaveSceneObjects(const char* path);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;

    std::unique_ptr<Object3dCommon> object3dCommon_;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Player> player_;
    std::vector<std::unique_ptr<Object3d>> sceneObjects_;
    std::vector<SceneSerializer::ObjectRecord> sceneObjectRecords_;
    std::list<std::unique_ptr<Bullet>> playerBullets_;
    std::list<std::unique_ptr<Bullet>> enemyBullets_;
    std::vector<std::unique_ptr<Bullet>> playerBulletPool_;
    std::vector<std::unique_ptr<Bullet>> enemyBulletPool_;
    std::list<std::unique_ptr<Enemy>> enemies_;
    std::unordered_map<Bullet*, const Enemy*> homingBulletTargets_;
    std::unordered_set<Bullet*> justDodgedEnemyBullets_;
    std::vector<HitEffect> hitEffects_;
    std::vector<std::unique_ptr<Object3d>> hitEffectObjectPool_;
    std::vector<RailSceneryObject> railSceneryObjects_;
    // 街区の切替地点をワールド座標に一度だけ固定する。手前の建物は変形させない。
    float sceneryCanyonStartZ_ = -1.0f;
    float sceneryPlazaStartZ_ = -1.0f;
    // 初期化時に容量を確保し、建物の半透明部分を奥から手前へ並べるために再利用する。
    std::vector<const RailSceneryObject*> transparentSceneryDrawOrder_;
    std::vector<DepthCueEffect> depthCueEffects_;
    std::vector<RewardHeart> rewardHearts_;
    std::array<PlayerDodgeAfterimage, 16> playerDodgeAfterimages_;
    std::array<PlayerFlightAura, 6> playerFlightAuras_;
    std::array<PlayerExhaustParticle, 64> playerExhaustParticles_;
    std::array<ContactShadow, 32> contactShadows_;
    std::array<bool, 5> stageRailEventTriggered_{};
    // 配置データの件数に合わせて開始時に確保する。プレイ中は増減させない。
    std::vector<bool> stageEnemyEventTriggered_;
    std::array<WaveTuning, 3> waveTuning_{ {
        { 6, 42, 30.0f },
        { 8, 38, 34.0f },
        { 10, 34, 38.0f }
    } };

    Model* playerModel_ = nullptr;
    Model* bulletModel_ = nullptr;
    Model* enemyModel_ = nullptr;
    Model* enemyFormationModel_ = nullptr;
    Model* enemySwoopModel_ = nullptr;
    Model* enemyShooterModel_ = nullptr;
    Model* enemyHeavyModel_ = nullptr;
    Model* bossModel_ = nullptr;
    Model* effectGlowCoreModel_ = nullptr;
    Model* effectGlowRingModel_ = nullptr;
    Model* effectSparkStarModel_ = nullptr;
    Model* effectBulletGlowModel_ = nullptr;
    Model* effectBulletTrailModel_ = nullptr;
    Model* effectPlayerBulletCoreModel_ = nullptr;
    Model* effectPlayerBulletTrailModel_ = nullptr;
    Model* effectPlayerChargeCoreModel_ = nullptr;
    Model* effectPlayerChargeTrailModel_ = nullptr;
    Model* effectEnemyBulletCoreModel_ = nullptr;
    Model* effectEnemyBulletTailModel_ = nullptr;
    Model* effectImpactBurstModel_ = nullptr;
    Model* effectCombatShockModel_ = nullptr;
    Model* effectCombatBladeModel_ = nullptr;
    Model* effectCombatDiscModel_ = nullptr;
    Model* effectCombatArcModel_ = nullptr;
    Model* effectMagicShardModel_ = nullptr;
    Model* effectExplosionFireballModel_ = nullptr;
    Model* effectExplosionSmokeModel_ = nullptr;
    Model* effectExplosionSparksModel_ = nullptr;
    Model* effectContactShadowModel_ = nullptr;

    int shootCooldown_ = 0;
    int shootBufferTimer_ = 0;
    int enemySpawnTimer_ = 0;
    int enemyShotTimer_ = 32;
    Math::Vector3 enemyAimVelocity_{}; // 通常移動の平滑化速度。回避の急加速は予測照準に使わない。
    int bossWarningTimer_ = 0;
    int bossIntroTimer_ = 0;
    int bossDefeatFlashTimer_ = 0;
    int bossPhaseTransitionTimer_ = 0;
    int bossAttackCooldown_ = 0;
    int bossAttackStepTimer_ = 0;
    int bossAttackStep_ = -1;
    int bossAttackPattern_ = 0;
    int bossAttackSequence_ = 0;
    int bossShotsFired_ = 0; // 通し試験で「登場しただけで倒される」状態を検出する実発射数。
    int bossPhase_ = 1;
    int bossCounterTimer_ = 0;
    int bossCounterDuration_ = 1;
    Math::Vector3 bossAimPoint_{}; // 予備動作の終盤で固定する狙い。連射中は自機を追い直さない。
    Math::Vector3 bossDefeatPosition_{}; // 敵の破棄後も、時間差の爆発だけを安全に再生する位置。
    int currentWaveIndex_ = 0;
    int spawnedEnemyCountInWave_ = 0;
    int defeatedEnemyCountInWave_ = 0;
    int spawnSequenceIndex_ = 0;
    int score_ = 0;
    int defeatedEnemyCount_ = 0;
    int escapedEnemyCount_ = 0;
    int playerShotsFired_ = 0;
    int playerHitCount_ = 0;
    int playerDamageCount_ = 0;
    int feverActivationCount_ = 0;
    int justDodgeCount_ = 0;
    int defeatChainCount_ = 0;
    int defeatChainTimer_ = 0;
    int defeatChainBreakFlashTimer_ = 0;
    int maxDefeatChainCount_ = 0;
    int resultTransitionTimer_ = -1;
    int chargeTimer_ = 0;
    int chargeFlashTimer_ = 0;
    int feverGauge_ = 0;
    int feverTimer_ = 0;
    int feverActivationFlashTimer_ = 0;
    float feverSpeedEffectRate_ = 0.0f;
    int playerDodgeAfterimageTimer_ = 0;
    int justDodgeFlashTimer_ = 0;
    int justDodgeSlowTimer_ = 0;
    int playerImpactFlashTimer_ = 0;
    int playerImpactFlashDuration_ = 1;
    int playerImpactSlowTimer_ = 0;
    int playerImpactSlowDuration_ = 1;
    int hitConfirmTimer_ = 0;
    int hitConfirmDuration_ = 1;
    int hitConfirmComboTimer_ = 0;
    int hitConfirmComboCount_ = 0;
    int playerDamageHudTimer_ = 0;
    int playerDamageHudDuration_ = 1;
    int cameraShakeTimer_ = 0;
    int cameraShakeDuration_ = 1;
    size_t playerBulletPoolMisses_ = 0;
    size_t enemyBulletPoolMisses_ = 0;
    size_t hitEffectObjectPoolMisses_ = 0;
    size_t rewardHeartPoolMisses_ = 0;
    size_t visibleSceneryCount_ = 0;
    size_t maxActivePlayerBullets_ = 0;
    size_t maxActiveEnemyBullets_ = 0;
    int chargeShotThreshold_ = 72;
    int normalShootCooldown_ = 14;
    int chargedShootCooldown_ = 22;
    int enemyShotInterval_ = 64;
    int waveStartDelay_ = 90;
    float cameraTimer_ = 0.0f;
    float gameplayElapsedSeconds_ = 0.0f;
    float cameraShakePower_ = 0.0f;
    float railDistance_ = 0.0f;
    float railSpeed_ = 0.115f;
    float targetRailSpeed_ = 0.115f;
    float previousFlightTimeScale_ = 1.0f; // スロー解除の瞬間を検出する前フレーム値。
    float flightReleaseKick_ = 0.0f; // スローから復帰した後の短い視界・噴射の加速感。
    float stageProgress_ = 0.0f;
    float stageTimelineSpeed_ = 0.0f;
    float stageCameraYawBias_ = 0.0f;
    float stageCameraRollBias_ = 0.0f;
    float stageCameraLiftBias_ = 0.0f;
    float stageCameraFovBoost_ = 0.0f;
    float playerExhaustThrust_ = 0.0f;
    float playerExhaustParticleTimer_ = 0.0f;
    bool gpuPlayerExhaustEnabled_ = false;
    float playerImpactSlowScale_ = 1.0f;
    float playerBulletSpeed_ = 1.36f;
    float lockBulletSpeed_ = 1.62f;
    float chargedBulletSpeedMultiplier_ = 1.12f;
    float enemyBulletSpeed_ = 0.36f;
    float lockRadius_ = 118.0f;
    float cameraFovY_ = 0.590f;
    Math::Vector2 hudViewportMin_{ 0.0f, 0.0f };
    Math::Vector2 hudViewportSize_{ 0.0f, 0.0f };
    Math::Vector2 editorOverlayViewportMin_{ 0.0f, 0.0f };
    Math::Vector2 editorOverlayViewportSize_{ 0.0f, 0.0f };
    size_t nextPlayerDodgeAfterimageIndex_ = 0;
    size_t nextPlayerExhaustParticleIndex_ = 0;
    Math::Vector3 cameraTranslate_{ 0.0f, 2.65f, -15.8f };
    Math::Vector3 cameraRotate_{ 0.18f, 0.0f, 0.0f };
    Math::Vector3 previousPlayerTranslate_{ 0.0f, 0.0f, 0.0f };
    struct FlightCameraMotion {
        Math::Vector3 translationVelocity{}; // ばね追従の移動速度。前進そのものは含めない。
        Math::Vector3 rotationVelocity{}; // バンクと上下の傾きが収まるまでの角速度。
        float fovVelocity = 0.0f; // 画角の復元速度。
        float acceleration = 0.0f; // 平滑化した加減速の力（-1～1）。
        float previousSpeed = 0.235f; // スローを含む直前の実効レール速度。
        float blurStrength = 0.0f; // 周辺ブラーの強さ（0～1）。
    } flightCameraMotion_;
    const Enemy* lockedEnemy_ = nullptr;
    const char* stageSectionName_ = "Opening";
    const char* stageCombatBeatName_ = "Intro";
    Math::Vector2 lockedEnemyScreen_{ 0.0f, 0.0f };
    Math::Vector2 reticleScreen_{ 0.0f, 0.0f };
    Math::Vector2 hitConfirmScreen_{ 0.0f, 0.0f };
    Math::Vector2 playerDamageScreen_{ 0.0f, 0.0f };
    Math::Vector2 playerDamageDirection_{ 0.0f, -1.0f };
    float hitConfirmStrength_ = 1.0f;
    std::string currentSceneFilePath_{ "resources/game_scene.json" };
    std::string editorStatusMessage_{ "Ready." };
    bool hasLockTarget_ = false;
    bool isReticleOnTarget_ = false;
    bool hitConfirmCharged_ = false;
    bool hitConfirmBoss_ = false;
    bool hitConfirmDestroyed_ = false;
    bool stageTimelineWasBlocked_ = false;
    int stageEncounterBreatherTimer_ = 0;
    float stageEmptyFrames_ = 0.0f; // 敵不在の経過時間。通常編隊間だけ次の出現までの待ちを制限する。
    int postEffectMode_ = 12;
    bool isGameOver_ = false;
    bool isGameClear_ = false;
    bool isRetryRequested_ = false; // 結果画面から同じモードを新規開始する。
    bool isMainGameRequested_ = false; // 練習完了画面から本編を新規開始する。
    bool showControlsHelp_ = false; // 操作説明の表示中もゲーム進行を止める。
    bool isPaused_ = false; // ESCで停止。描画とメニュー入力だけ継続する。
    bool menuInputConsumed_ = false; // 説明を閉じたEnterを下のメニューへ通さない。
    int pauseSelectedItem_ = 0; // 再開・再挑戦・操作方法・タイトルの選択位置。
    int resultSelectedItem_ = 0; // 再挑戦またはタイトル。方向キーで変更する。
    bool isEditorOverlayVisible_ = false;
    bool isPerformanceOverlayVisible_ = false;
    bool isPostEffectBypassEnabled_ = false;
    bool isExitRequested_ = false;
    bool showSkybox_ = true;
    bool bossSpawned_ = false;
    bool bossDefeated_ = false;
    bool bossWarningTriggered_ = false;
    bool wasPlayerDodging_ = false;
    bool isHudViewportRectEnabled_ = false;
    bool hasEditorOverlayViewportRect_ = false;
};
