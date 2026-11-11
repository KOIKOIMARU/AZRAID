// ===== GameRuntimeDefeat.cpp =====
// 撃墜後の戦闘を止め、墜落演出を進めて敗北リザルトにつなぐ。

#include "app/runtime/GameRuntime.h"

#include "engine/3d/Skybox.h"
#include "engine/base/DirectXCommon.h"

#include <algorithm>
#ifdef _DEBUG
#include "engine/audio/SoundManager.h"
#include "engine/io/Input.h"
#include <imgui.h>
#include <cmath>
#include <fstream>
#include <stdexcept>
#endif

// HPが尽きた直後に敗北状態へ切り替え、戦闘を閉じて墜落演出を始める。
void GameRuntime::BeginPlayerDefeat(const Math::Vector3& incomingVelocity)
{
    if (!player_ || !camera_ || IsTutorial() || isGameOver_) { return; }
    isGameOver_ = true;
    stageResults_.BeginDefeat();
    playerDefeatPresentation_.Start(*player_, *camera_, gameplayCamera_.FollowPose().fov,
        stageFlight_.Speed(), incomingVelocity);
    rewards_.Fever().Stop(true);
    playerCombatFeedback_.ClearImpact();
    bossPresentation_.CancelArrival();
    bossAttack_.CancelAttack();
    stageEntryPresentation_.HideBriefing();
    ResetPhantomRaid();
    ResetRifts();
    projectiles_.KillAll();
    playerControl_.Aim().ClearTarget();
    stageEnemies_.UpdateDeparture(0.0f);
    playerFlightEffects_.Reset();
    AddEnemyHitEffect(player_->GetTranslate(), 0.55f);
}

// 墜落中の自機、破片、敵の離脱を進める。リザルトは墜落演出の完了を待つ。
void GameRuntime::UpdatePlayerDefeat()
{
    if (!camera_ || !player_ || !playerDefeatPresentation_.IsActive()) { return; }
    const float dt = dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime(), 0.0f, 0.10f) : 1.0f / 60.0f;
    // 衝突判定を終えてから残弾をプールへ返す。退場する敵は演出中も保持する。
    projectiles_.RecycleAll();
    stageEnemies_.UpdateDeparture(dt);
    const auto frame = playerDefeatPresentation_.Update(*player_, dt);
    if (frame.burstPosition) {
        AddEnemyHitEffect(*frame.burstPosition, 1.80f);
        PlaySfx("destroy");
    }
    gameplayCamera_.AdoptPose(frame.camera);
    ApplyGameplayCameraPose();
    if (skybox_) { skybox_->Update(camera_.get()); }
    railScenery_.Update(stageFlight_.Distance());
    UpdateHitEffects();
    if (playerDefeatPresentation_.IsResultReady()) {
        stageResults_.SetDelay(0);
        if (stageResults_.TryMarkSoundPlayed()) { PlaySfx("fail"); }
    }
}

// 敗北演出中に限って、現在の表示領域に画面の暗転を重ねる。
void GameRuntime::DrawPlayerDefeatOverlay()
{
    if (!isGameOver_ || !playerDefeatPresentation_.IsActive()) { return; }
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    playerDefeatPresentation_.DrawOverlay(min, size);
}

#ifdef _DEBUG
// 敗北の開始、墜落中の挙動、破片、リザルト表示までを実際の戦闘処理で確認する。
bool GameRuntime::RunDefeatProbe(const std::string& logPath, bool preview)
{
    static int phase = 0, sample = 0, menuFrames = 0, resumedFrames = 0;
    static float progress = 0.0f, rail = 0.0f;
    static int damageCount = 0, defeatedCount = 0, escapedCount = 0;
    static uint32_t initialVoiceCount = 0;
    static std::array<Enemy*, 3> departureEnemies{};
    static std::array<Math::Vector3, 3> departureOrigins{};
    const auto log = [&](const char* message) {
        std::ofstream file(logPath, std::ios::app);
        file << "DEFEAT_TEST " << message << '\n';
    };
    const auto require = [&](bool ok, const char* message) {
        if (!ok) { log(message); throw std::runtime_error(message); }
    };
    if (!player_ || !camera_ || stageEntryPresentation_.IsTitleActive()) { return false; }
    std::array<BYTE, 256> keys{};
    if (phase == 0) {
        require(audio_.SoundForProbe() && audio_.SoundForProbe()->GetVoiceCount() > 0, "audio bank was not prepared before defeat");
        initialVoiceCount = audio_.SoundForProbe()->GetVoiceCount();
        require(playerDefeatPresentation_.HasFuelStainForProbe(), "fuel stain was not preallocated");
        require(playerDefeatPresentation_.HasDebrisForProbe(false), "wreckage was not preallocated or started visible");
        rewards_.Fever().SetRemainingFramesForDebug(0);
        ResetPhantomRaid();
        for (auto& bullet : projectiles_.EnemyPool().Active()) { bullet->Kill(); }
        // 無敵時間を通常の更新で消化してから、実際の衝突経路で死亡させる。
        player_->Damage(player_->GetHp() - 1);
        input_->SetTestFrame({}, { 1200, 100 });
        for (int frame = 0; frame < 24; ++frame) { player_->Update({}, 1.0f, 1.0f); }
        require(!player_->IsInvincible() && player_->GetHp() == 1, "fatal fixture retained damage protection");
        // 敵がいる最中に死亡する条件を再現。小型・射撃型・ボスの退場を確認する。
        departureEnemies[0] = SpawnStageEnemyForProbe(-4.0f, 2.0f, 52.0f,
            Enemy::Behavior::Formation, Enemy::EntryStyle::Direct, 4);
        departureEnemies[1] = SpawnStageEnemyForProbe(4.0f, 3.0f, 60.0f,
            Enemy::Behavior::StrafeShooter, Enemy::EntryStyle::Direct, 10);
        departureEnemies[2] = SpawnStageEnemyForProbe(0.0f, 2.0f, 65.0f,
            Enemy::Behavior::Boss, Enemy::EntryStyle::Direct, 160);
        for (size_t index = 0; index < departureEnemies.size(); ++index) {
            require(departureEnemies[index] != nullptr, "could not create departure enemy");
            for (int frame = 0; frame < 120; ++frame) { departureEnemies[index]->Update(stageFlight_.Distance()); }
            require(departureEnemies[index]->IsTargetable(), "departure fixture was not visible and targetable");
            departureOrigins[index] = departureEnemies[index]->GetTranslate();
        }
        playerControl_.Aim().SetTargetForProbe(*departureEnemies[0]);
        auto homingBullet = projectiles_.AcquirePlayerForDebug();
        require(homingBullet != nullptr, "departure fixture exhausted player bullet pool");
        homingBullet->Initialize(object3dCommon_.get(), projectiles_.BodyModelForDebug(), player_->GetTranslate(), { 0.0f, 0.0f, 1.0f });
        projectiles_.SetHomingTargetForDebug(*homingBullet, playerControl_.Aim().Target());
        auto bullet = projectiles_.AcquireEnemyForDebug();
        require(bullet != nullptr, "fatal fixture exhausted bullet pool");
        bullet->Initialize(object3dCommon_.get(), projectiles_.BodyModelForDebug(), player_->GetTranslate(), { 0.35f, 0.0f, -0.8f });
        projectiles_.MarkDodged(*bullet);
        combatProgress_.SetScoreForDebug(32100);
        defeatedCount = combatResolver_.Stats().defeated; escapedCount = stageEnemies_.EscapedCount();
        combatResolver_.ResolveEnemyShots(BuildCombatContext());
        require(isGameOver_ && player_->IsDead() && playerDefeatPresentation_.ElapsedForProbe() == 0.0f && stageResults_.IsWaiting(),
            "fatal collision did not enter death sequence");
        require(!playerControl_.Aim().Target() && !playerControl_.Aim().IsReticleOnTarget() && !projectiles_.HasTrackingForDebug(),
            "fatal collision retained targeting references");
        progress = stageFlight_.Schedule().Progress(); rail = stageFlight_.Distance(); damageCount = combatResolver_.Stats().playerDamage;
        log("FATAL_COLLISION_OK normal_collision_path=1");
        phase = 1;
    }
    if (phase == 1) {
        require(stageFlight_.Schedule().Progress() == progress && stageFlight_.Distance() == rail && combatProgress_.Score() == 32100 && combatResolver_.Stats().playerDamage == damageCount,
            "gameplay advanced during defeat");
        require(combatResolver_.Stats().defeated == defeatedCount && stageEnemies_.EscapedCount() == escapedCount,
            "cinematic departure changed combat statistics");
        for (size_t index = 0; index < departureEnemies.size(); ++index) {
            const auto& enemy = *departureEnemies[index];
            require(!enemy.IsTargetable() && !enemy.CanShoot(), "departing enemy could attack or be targeted");
            const auto position = enemy.GetTranslate();
            require(std::isfinite(position.x + position.y + position.z), "enemy departure became non-finite");
            if (playerDefeatPresentation_.ElapsedForProbe() > 0.10f) {
                require(position.z > departureOrigins[index].z, "enemy stayed frozen during defeat");
            }
            if (playerDefeatPresentation_.ElapsedForProbe() >= 1.0f) { require(enemy.HasDeparted(), "enemy remained visible after departure"); }
        }
        if (playerDefeatPresentation_.ElapsedForProbe() > 0.0f) {
            require(projectiles_.PlayerPool().Active().empty() && projectiles_.EnemyPool().Active().empty(),
                "defeat retained live projectiles");
        }
        require(!HasResultPresentation(), "defeat displayed the undamaged result ship");
        if (playerDefeatPresentation_.ElapsedForProbe() < 2.05f) {
            require(stageResults_.IsWaiting() && !stageResults_.MenuForProbe().IsDefeatVisible(), "result opened before death sequence ended");
        }
        if (playerDefeatPresentation_.ElapsedForProbe() > 1.30f) {
            require(playerDefeatPresentation_.HasExploded(), "death burst did not run");
            require(playerDefeatPresentation_.HasDebrisForProbe(true), "death did not leave four finite wreckage pieces");
            require(playerDefeatPresentation_.IsDebrisAboveRoadForProbe(), "wreckage penetrated the road");
        }
        require(std::isfinite(gameplayCamera_.FollowPose().position.x + gameplayCamera_.FollowPose().position.y + gameplayCamera_.FollowPose().position.z + gameplayCamera_.FollowPose().fov),
            "death camera became non-finite");
        constexpr float stops[]{ 0.13f, 0.65f, 1.35f, 2.80f };
        if (preview && sample < 4 && playerDefeatPresentation_.ElapsedForProbe() >= stops[sample]) {
            phantomPreviewPaused_ = true;
            if (!ImGui::IsKeyPressed(ImGuiKey_F8, false)) { input_->SetTestFrame({}, { 1200, 100 }); return false; }
            phantomPreviewPaused_ = false;
            ++sample;
        }
        if (playerDefeatPresentation_.ElapsedForProbe() >= 2.85f) {
            require(playerDefeatPresentation_.HasLandedFuelForProbe(),
                "fuel did not remain on the road after impact");
            require(stageResults_.DelayForProbe() == 0 && stageResults_.MenuForProbe().IsDefeatVisible() && stageResults_.MenuForProbe().SelectedItem() == 0,
                "game over was not ready with retry selected");
            log("SEQUENCE_OK no_gameplay=1 camera_finite=1 burst=1 no_result_ship=1 menu_delayed=1 enemy_departure=3 no_residual_bullets=1 no_extra_score=1");
            phase = 2;
        } else if (stageResults_.IsWaiting()) {
            keys[DIK_SPACE] = keys[DIK_A] = keys[DIK_Q] = 0x80;
            if (playerDefeatPresentation_.ElapsedForProbe() < 1.0f) { keys[DIK_R] = 0x80; }
        }
    }
    if (phase == 2) {
        ++menuFrames;
        if (menuFrames == 2) { keys[DIK_RIGHT] = 0x80; }
        if (menuFrames == 3) { require(stageResults_.MenuForProbe().SelectedItem() == 1, "game over direction input failed"); }
        if (menuFrames == 4) { keys[DIK_LEFT] = 0x80; }
        if (menuFrames == 5) { require(stageResults_.MenuForProbe().SelectedItem() == 0, "retry selection did not restore"); keys[DIK_TAB] = 0x80; }
        if (menuFrames == 6) { require(stageResults_.MenuForProbe().IsDetailsOpen(), "game over details did not open"); }
        if (menuFrames == 7) { keys[DIK_TAB] = 0x80; }
        if (menuFrames == 8) { require(!stageResults_.MenuForProbe().IsDetailsOpen(), "game over details did not close"); }
        if (menuFrames == 10) { keys[DIK_R] = 0x80; log("MENU_OK selection=1 details=1 retry_key=1"); phase = 3; }
    }
    if (phase == 3 && !isGameOver_) {
        require(std::none_of(stageEnemies_.Active().begin(), stageEnemies_.Active().end(), [](const auto& enemy) { return enemy && enemy->HasDeparted(); }),
            "retry retained departed enemies");
        require(player_->GetHp() == 100 && playerDefeatPresentation_.IsResetForProbe(),
            "retry retained death state");
        require(playerDefeatPresentation_.AreMotesInactiveForProbe(),
            "retry retained death particles");
        require(playerDefeatPresentation_.HasDebrisForProbe(false),
            "retry retained wreckage");
        if (!stageEntryPresentation_.IsRetryActive() && ++resumedFrames >= 30) {
            require(audio_.SoundForProbe() && audio_.SoundForProbe()->GetVoiceCount() == initialVoiceCount,
                "retry changed the bounded audio bank");
            log("PASS retry_resets_hp_camera_particles=1 short_entry=1 voice_count_matches=1");
            input_->SetTestFrame({}, { 1200, 100 });
            return true;
        }
    }
    input_->SetTestFrame(keys, { 1200, 100 });
    return false;
}
#endif
