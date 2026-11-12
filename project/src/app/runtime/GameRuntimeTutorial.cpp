// ===== GameRuntimeTutorial.cpp =====
// チュートリアルの練習用の敵やリングを出し、達成状況に応じて次の課題へ進める

#include "app/runtime/GameRuntime.h"
#include "engine/base/DirectXCommon.h"
#include "engine/io/Input.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#ifdef _DEBUG
#include <imgui.h>
#include <fstream>
#include <stdexcept>
#endif

// 射撃や撃破などの累計を練習側へ渡し、課題開始から増えた分を判定できるようにする
TutorialProgress::Stats GameRuntime::ReadTutorialStats() const
{
    return { combatResolver_.Stats().defeated, playerSkill_.ActivationCount(), playerSkill_.DefeatCount(), ringGates_.PassCount() };
}

bool GameRuntime::HasActiveTutorialRing() const
{
    return std::any_of(ringGates_.Gates().begin(), ringGates_.Gates().end(),
        [](const auto& gate) { return gate.active; });
}

void GameRuntime::SpawnTutorialRing()
{
    SpawnRift({ 3.6f, 1.4f, stageFlight_.Distance() + 48.0f });
}

void GameRuntime::SpawnTutorialTargets(int count, int hp)
{
    // 通常の標的は機体同士が重ならない間隔　チャージだけは爆風で巻き込める小編隊
    const float spacing = tutorial_.CurrentLesson() == TutorialProgress::Lesson::Charge ? 3.0f : 5.2f;
    stageEnemies_.SpawnTrainingTargets(stageFlight_.Distance(), count, hp, spacing);
}

// 課題を切り替え、達成判定の基準値と練習用の機体・敵・リングをそろえる
void GameRuntime::BeginTutorialLesson(TutorialProgress::Lesson lesson)
{
    // 残弾やロック先を次の課題へ持ち越さない　弾は通常の更新経路でプールへ戻す
    projectiles_.KillAll();
    stageEnemies_.KillAll();
    ringGates_.Deactivate();
    const auto setup = tutorial_.BeginLesson(lesson, ReadTutorialStats());
    // 直前の被弾を持ち越さず、各課題を同じ条件で練習する　位置は変えない
    if (player_) { const auto position = player_->GetTranslate(); player_->Reset(position); }
    playerControl_.Shooting().Reset();
    hud_.ClearChargeFlash();
    if (setup.prepareSkill) { playerSkill_.MakeReadyForPractice(); }
    if (setup.feverGauge >= 0) { rewards_.Fever().SetGauge(setup.feverGauge); }
    if (setup.targets.count > 0) { SpawnTutorialTargets(setup.targets.count, setup.targets.hp); }
    if (setup.spawnRing) { SpawnTutorialRing(); }
    if (lesson == TutorialProgress::Lesson::Complete) {
        isGameClear_ = true;
        stageResults_.SetDelay(30);
        stageResults_.ResetSelection();
    }
}

// 回避課題の発射予告とカウントを進め、練習用の敵弾を撃つ
void GameRuntime::UpdateTutorialAttack()
{
    if (!tutorial_.Started() || tutorial_.CurrentLesson() != TutorialProgress::Lesson::Dodge ||
        tutorial_.ShowingSuccess()) { return; }
    const float step = dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.0f, 3.0f) : 1.0f;
    if (tutorial_.AdvanceDodgeAttack(step)) {
        for (const auto& enemy : stageEnemies_.Active()) {
            if (!enemy->IsDead() && enemy->CanShoot()) {
                FireEnemyBullet(enemy->GetAimPosition(), EnemyBulletStyle::Standard);
                tutorial_.OnDodgeShotFired();
                break;
            }
        }
    }
    if (player_->IsDodging()) {
        const auto player = player_->GetTranslate();
        for (const auto& bullet : projectiles_.EnemyPool().Active()) {
            if (!bullet->IsDead()) { tutorial_.ObserveDodge(player, bullet->GetTranslate()); }
        }
    }
}

// 現在の操作や戦闘結果を課題に渡し、必要な練習物の再配置や次の課題への移行を行う
void GameRuntime::UpdateTutorialLesson()
{
    if (!IsTutorial() || !player_ || isGameOver_ || isGameClear_) { return; }
    if (!tutorial_.Started()) { BeginTutorialLesson(TutorialProgress::Lesson::Move); }
    const float step = dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.0f, 3.0f) : 1.0f;
    const auto events = tutorial_.Advance(step, {
        .stats = ReadTutorialStats(),
        .playerPosition = player_->GetTranslate(),
        .targetsAlive = stageEnemies_.HasLiving(),
        .skillActive = IsPhantomRaidActive(),
        .ringActive = HasActiveTutorialRing(),
        .feverActive = rewards_.Fever().IsActive()
    });
    if (events.nextLesson) { BeginTutorialLesson(*events.nextLesson); }
    if (events.reachedMarker) { PlaySfx("hit"); }
    if (events.clearedLesson) {
        for (auto& bullet : projectiles_.EnemyPool().Active()) { bullet->Kill(); }
        PlaySfx("skill_ready");
    }
    if (events.targets.count > 0) { SpawnTutorialTargets(events.targets.count, events.targets.hp); }
    if (events.respawnRing) {
        rewards_.Fever().SetGauge(40);
        SpawnTutorialRing();
    }
}

// 練習用の入力案内をHUDの表示範囲へ描き、現在の達成条件を示す
void GameRuntime::DrawTutorialGuideHud(ImDrawList& draw, const Math::Vector2& min, const Math::Vector2& size)
{
    if (!IsTutorial() || !tutorial_.Started() || isGameClear_ || isGameOver_) { return; }
    const auto player = player_->GetTranslate();
    const auto& bullets = projectiles_.EnemyPool().Active();
    const bool incomingShot = tutorial_.CurrentLesson() == TutorialProgress::Lesson::Dodge &&
        std::any_of(bullets.begin(), bullets.end(), [&](const auto& bullet) {
            return !bullet->IsDead() && TutorialProgress::IsDodgePromptShot(player, bullet->GetTranslate());
        });
    const auto* viewProjection = camera_ ? &camera_->GetViewProjectionMatrix() : nullptr;
    tutorial_.DrawGuide(draw, min, size, viewProjection, stageFlight_.Distance(), {
        .chargeReady = playerControl_.Shooting().IsChargeReady(),
        .skillActive = IsPhantomRaidActive(),
        .skillReady = playerSkill_.IsReady(),
        .ringActive = HasActiveTutorialRing(),
        .feverActive = rewards_.Fever().IsActive(),
        .incomingShot = incomingShot
    });
}

#ifdef _DEBUG
// 課題ごとに移動、射撃、回避などを実行して、課題の進行と完了後の切り替えを確認する
bool GameRuntime::RunTutorialProbe(const std::string& logPath, bool preview, bool departure)
{
    // 実際の入力・弾・衝突で全課題を通す　課題の成功フラグや撃破数は書き換えない
    static int frame = 0;
    static int lessonFrame = 0;
    static int previousLesson = -1;
    static int previewMask = 0;
    static double previewResumeAt = 0.0;
    static int resultFrame = 0;
    static bool awaitingRetry = false;
    static float pausedElapsed = 0.0f;
    static float pausedRail = 0.0f;
    static bool resumeVerified = false;
    static int chargedWrongKills = 0;
    static bool ringMissVerified = false;
    static bool ringPassVerified = false;
    static bool ringBoostVerified = false;
    static bool feverWasActive = false;
    static bool feverRetryVerified = false;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "TUTORIAL " << message << '\n';
    };
    const auto require = [](bool ok, const char* message) {
        if (!ok) { throw std::runtime_error(message); }
    };
    std::array<BYTE, 256> keys{};
    Math::Vector2 mouse{ 10, 350 };
    if (awaitingRetry && !isGameClear_) {
        require(tutorial_.CurrentLesson() == TutorialProgress::Lesson::Move && tutorial_.ProgressForProbe() == 0 &&
            combatResolver_.Stats().defeated == 0 && player_->GetHp() == 100 && rewards_.Fever().ActivationCount() == 0 &&
            !IsMainGameRequested() && !IsRetryRequested(), "Tutorial retry did not reset all lesson state");
        require(ringGates_.PassCount() == 0 && ringGates_.SpawnCount() == 0,
            "Tutorial retry inherited ring progress");
        if (tutorial_.ElapsedForProbe() == 0.0f) {
            require(stageFlight_.Distance() == 0.0f && stageEnemies_.Active().empty() && projectiles_.PlayerPool().Active().empty() && projectiles_.EnemyPool().Active().empty(),
                "Tutorial progressed during retry arrival wipe");
            input_->SetTestFrame(keys, mouse);
            return false;
        }
        log("RETRY_RESET_OK");
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    const int lesson = static_cast<int>(tutorial_.CurrentLesson());
    if (previousLesson != lesson) {
        if (previousLesson >= 0) { log("LESSON_OK index=" + std::to_string(previousLesson)); }
        previousLesson = lesson;
        lessonFrame = 0;
        log("BEGIN index=" + std::to_string(lesson));
    }
    if (preview) {
        int point = isGameClear_ ? TutorialProgress::kLessonCount :
            tutorial_.ShowingSuccess() ? TutorialProgress::kLessonCount + 1 : lesson;
        const bool ringVisible = tutorial_.CurrentLesson() == TutorialProgress::Lesson::Ring &&
            std::any_of(ringGates_.Gates().begin(), ringGates_.Gates().end(), [](const auto& gate) { return gate.active; });
        if (ringVisible) { point = TutorialProgress::kLessonCount + 2; }
        if (tutorial_.CurrentLesson() == TutorialProgress::Lesson::Fever && rewards_.Fever().IsActive()) { point = TutorialProgress::kLessonCount + 3; }
        if (((lessonFrame >= 120 && !gameplayMenu_.IsPaused()) || (isGameClear_ && !stageResults_.IsWaiting()) || tutorial_.ShowingSuccess()) &&
            !(previewMask & (1 << point))) {
            previewMask |= 1 << point;
            phantomPreviewPaused_ = true;
            previewResumeAt = ImGui::GetTime() + (isGameClear_ ? 30.0 : 8.0);
        }
        if (phantomPreviewPaused_) {
            ImGui::SetNextWindowPos({ 20, 155 }, ImGuiCond_Always);
            ImGui::SetNextWindowSize({ 154, 94 }, ImGuiCond_Always);
            ImGui::Begin("Tutorial QA", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
            const bool next = ImGui::Button("NEXT / F8") || ImGui::IsKeyPressed(ImGuiKey_F8, false);
            ImGui::Text("Auto %.0fs", (std::max)(0.0, previewResumeAt - ImGui::GetTime()));
            ImGui::End();
            if (next || ImGui::GetTime() >= previewResumeAt) { phantomPreviewPaused_ = false; }
            input_->SetTestFrame({}, mouse);
            return false;
        }
    }
    ++frame;
    ++lessonFrame;
    require(!isGameOver_, "Tutorial failed instead of allowing retry");
    require(!stageEnemies_.BossSpawned() && stageFlight_.Schedule().Progress() == 0.0f && (stageFlight_.Schedule().EnemyCount() == 0),
        "Tutorial entered main-game schedule");
    if (frame == 30) { keys[DIK_ESCAPE] = 0x80; }
    if (frame == 31) {
        require(gameplayMenu_.IsPaused(), "Tutorial pause did not open");
        pausedElapsed = tutorial_.ElapsedForProbe();
        pausedRail = stageFlight_.Distance();
    }
    if (frame > 31 && frame <= 90) {
        require(gameplayMenu_.IsPaused() && tutorial_.ElapsedForProbe() == pausedElapsed && stageFlight_.Distance() == pausedRail,
            "Tutorial progressed while paused");
    }
    if (frame == 91) { keys[DIK_ESCAPE] = 0x80; }
    if (frame >= 92 && !resumeVerified) {
        require(tutorial_.ElapsedForProbe() == pausedElapsed && stageFlight_.Distance() == pausedRail,
            "Tutorial progressed before resume transition finished");
        if (gameplayMenu_.IsPaused()) { require(gameplayMenu_.PendingAction() == GameplayMenu::Action::Resume, "Tutorial pause did not begin closing"); }
        else { resumeVerified = true; log("PAUSE_FREEZE_OK resume_transition_frozen=1"); }
    }
    if (frame == 192) { require(resumeVerified, "Tutorial resume transition did not finish"); }
    if (isGameClear_) {
        require(lesson == TutorialProgress::kLessonCount && rewards_.Fever().ActivationCount() >= 2 && playerSkill_.ActivationCount() > 0 &&
            ringPassVerified && ringBoostVerified && ringMissVerified && feverRetryVerified,
            "Tutorial completed without skill, ring recovery and automatic fever retry");
        if (!stageResults_.IsWaiting()) {
            if (!departure && resultFrame < 10) { keys[DIK_D] = 0x80; }
            if (resultFrame > 0 && resultFrame <= 20) {
                require(stageResults_.MenuForProbe().SelectedItem() == (departure ? 0 : 1), "Tutorial result selection repeated during held key");
            }
            if (resultFrame == 21) {
                keys[DIK_RETURN] = 0x80;
                awaitingRetry = !departure;
                log(departure ? "ALL_LESSONS_OK departure_by_menu=1" : "ALL_LESSONS_OK retry_by_menu=1");
            }
            ++resultFrame;
        }
        input_->SetTestFrame(keys, mouse);
        return false;
    }
    if (tutorial_.ShowingSuccess() || gameplayMenu_.IsPaused()) {
        if (tutorial_.CurrentLesson() == TutorialProgress::Lesson::Ring && ringGates_.BoostAge() >= 8.0f && !ringBoostVerified) {
            require(ringGates_.BoostStrength() > 0.99f && stageFlight_.Speed() > 0.17f,
                "Tutorial success screen froze gate acceleration");
            ringBoostVerified = true;
            log("RING_BOOST_OK success_overlay_keeps_effect_clock=1 real_acceleration=1");
        }
        if (tutorial_.CurrentLesson() == TutorialProgress::Lesson::Ring && tutorial_.ShowingSuccess() && !ringPassVerified) {
            require(ringGates_.PassCount() == 1 && playerControl_.Shooting().IsChargeReady() && rewards_.Fever().Gauge() > 40,
                "Ring lesson completed without a real pass and its rewards");
            ringPassVerified = true;
            log("RING_PASS_OK real_collision=1 charge_refilled=1 gauge_increased=1");
        }
        input_->SetTestFrame(keys, mouse);
        return false;
    }
    const Enemy* target = nullptr;
    for (const auto& enemy : stageEnemies_.Active()) {
        Math::Vector2 screen{};
        if (!enemy->IsDead() && enemy->IsTargetable() && TryProjectToScreen(enemy->GetAimPosition(), screen)) {
            target = enemy.get();
            const auto origin = ImGui::GetMainViewport()->Pos;
            mouse = { screen.x - origin.x, screen.y - origin.y };
            break;
        }
    }
    switch (tutorial_.CurrentLesson()) {
    case TutorialProgress::Lesson::Move:
        if (lessonFrame <= 190) {
            require(tutorial_.ProgressForProbe() == 0, "Idle time skipped movement lesson");
            if (lessonFrame == 150) { keys[DIK_Q] = keys[DIK_SPACE] = 0x80; }
            if (lessonFrame == 190) { log("IDLE_AND_UNRELATED_ACTIONS_DONT_SKIP_OK"); }
        } else {
            const auto destination = tutorial_.MoveTarget(stageFlight_.Distance());
            const auto player = player_->GetTranslate();
            if (std::abs(destination.x - player.x) > 0.25f) { keys[destination.x > player.x ? DIK_D : DIK_A] = 0x80; }
            if (std::abs(destination.y - player.y) > 0.25f) { keys[destination.y > player.y ? DIK_W : DIK_S] = 0x80; }
        }
        break;
    case TutorialProgress::Lesson::Shoot:
        // 元の退場期限を超えても二機の標的が残ることを確認してから撃つ
        if (lessonFrame == 720) {
            require(stageEnemies_.Active().size() == 2 && stageEnemies_.EscapedCount() == 0 && tutorial_.ProgressForProbe() == 0,
                "Training targets escaped before player learned aiming");
            log("TARGETS_WAIT_FOR_PLAYER_OK");
        }
        if (lessonFrame > 720 && target) { keys[DIK_SPACE] = 0x80; }
        break;
    case TutorialProgress::Lesson::Dodge:
        if (lessonFrame == 20) { keys[DIK_LSHIFT] = 0x80; }
        if (lessonFrame == 60) { require(!tutorial_.DodgeConfirmedForProbe(), "Empty dodge skipped dodge lesson"); }
        if (lessonFrame > 200) {
            const auto player = player_->GetTranslate();
            for (const auto& bullet : projectiles_.EnemyPool().Active()) {
                const auto p = bullet->GetTranslate();
                if (!bullet->IsDead() && p.z - player.z > 5.0f && p.z - player.z < 10.0f && !player_->IsDodging()) {
                    keys[DIK_LSHIFT] = 0x80;
                    keys[player.x > 0.0f ? DIK_A : DIK_D] = 0x80;
                    break;
                }
            }
        }
        break;
    case TutorialProgress::Lesson::Charge:
        if (lessonFrame < 360 && target) { keys[DIK_SPACE] = 0x80; }
        if (lessonFrame == 360) {
            require(!tutorial_.ChargedHitForProbe() && !tutorial_.ShowingSuccess(), "Normal shot skipped charged-hit lesson");
            chargedWrongKills = combatResolver_.Stats().defeated - tutorial_.StartingStatsForProbe().defeats;
            require(chargedWrongKills > 0, "Wrong-input test never hit the target");
            log("NORMAL_SHOTS_DONT_SKIP_CHARGE_OK kills=" + std::to_string(chargedWrongKills));
        }
        if (lessonFrame > 360 && target && playerControl_.Shooting().IsChargeReady()) { keys[DIK_SPACE] = 0x80; }
        break;
    case TutorialProgress::Lesson::Skill:
        if (target && lessonFrame < 240) { keys[DIK_SPACE] = 0x80; }
        if (lessonFrame == 240) {
            require(playerSkill_.ActivationCount() == tutorial_.StartingStatsForProbe().skillUses && !tutorial_.ShowingSuccess() &&
                combatResolver_.Stats().defeated > tutorial_.StartingStatsForProbe().defeats, "Normal shots skipped the skill lesson");
            log("NORMAL_SHOTS_DONT_SKIP_SKILL_OK");
        }
        if (target && lessonFrame > 240 && !IsPhantomRaidActive()) { keys[DIK_Q] = 0x80; }
        break;
    case TutorialProgress::Lesson::Ring: {
        const RingGates::Gate* ring = nullptr;
        for (const auto& gate : ringGates_.Gates()) { if (gate.active) { ring = &gate; break; } }
        if (!ring && target && lessonFrame > 120) { keys[DIK_SPACE] = 0x80; }
        const auto player = player_->GetTranslate();
        Math::Vector3 destination{ -6.0f, 1.0f, player.z };
        if (ringGates_.SpawnCount() >= 2) {
            if (!ringMissVerified) {
                require(tutorial_.ProgressForProbe() == 0 && ringGates_.PassCount() == 0,
                    "Missing a ring completed the lesson");
                ringMissVerified = true;
                log("RING_MISS_RETRY_OK new_target=1 no_auto_success=1");
            }
            if (ring) { destination = ring->position; }
        }
        if (std::abs(destination.x - player.x) > 0.12f) { keys[destination.x > player.x ? DIK_D : DIK_A] = 0x80; }
        if (std::abs(destination.y - player.y) > 0.12f) { keys[destination.y > player.y ? DIK_W : DIK_S] = 0x80; }
        break;
    }
    case TutorialProgress::Lesson::Fever: {
        const bool feverActive = rewards_.Fever().IsActive();
        const bool feverReady = rewards_.Fever().IsReady();
        // 初回は発動前の残弾も片付け、撃破を持ち込まず時間切れを確認する
        if (!feverRetryVerified && (feverReady || (feverActive && !feverWasActive))) {
            for (const auto& bullet : projectiles_.PlayerPool().Active()) { bullet->Kill(); }
        }
        if (feverWasActive && !feverActive && !feverRetryVerified) {
            require(!tutorial_.ShowingSuccess(), "Fever timeout completed the lesson");
            feverRetryVerified = true;
            log("FEVER_EXPIRED_RETRY_OK no_auto_success=1");
        }
        feverWasActive = feverActive;
        if (target && ((!feverActive && !feverReady) || (feverActive && feverRetryVerified))) {
            keys[DIK_SPACE] = 0x80;
        }
        break;
    }
    case TutorialProgress::Lesson::Complete: break;
    }
    if (lessonFrame % 600 == 0) {
        log("PROGRESS index=" + std::to_string(lesson) + " progress=" + std::to_string(tutorial_.ProgressForProbe()) +
            " kills=" + std::to_string(combatResolver_.Stats().defeated) + " hp=" + std::to_string(player_->GetHp()));
    }
    input_->SetTestFrame(keys, mouse);
    return false;
}
#endif
