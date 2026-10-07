#include "app/GameRuntime.h"
#include "app/CombatHud.h"
#include "app/MenuUi.h"
#include "engine/base/DirectXCommon.h"
#include "engine/io/Input.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#ifdef _DEBUG
#include <fstream>
#include <stdexcept>
#endif

namespace {
constexpr std::array<const char*, 6> kLessonNames{
    "機体を動かす", "狙って撃つ", "弾をかわす", "チャージショット", "残像連撃", "フィーバー" };
constexpr std::array<Math::Vector2, 3> kMoveTargets{ Math::Vector2{-4.0f, 2.0f}, {4.0f, 3.2f}, {0.0f, 0.4f} };
}

Math::Vector3 GameRuntime::GetTutorialMoveTarget() const
{
    const auto target = kMoveTargets[std::clamp(tutorial_.progress, 0, 2)];
    return { target.x, target.y, railDistance_ };
}

bool GameRuntime::TutorialAllowsShooting() const
{
    if (!IsTutorial()) { return true; }
    return tutorial_.started && tutorial_.success <= 0.0f &&
        (tutorial_.lesson == TutorialLesson::Shoot || tutorial_.lesson == TutorialLesson::Charge ||
            tutorial_.lesson == TutorialLesson::Fever);
}

void GameRuntime::SpawnTutorialTargets(int count, int hp)
{
    // 通常の標的は機体同士が重ならない間隔。チャージだけは爆風で巻き込める小編隊。
    const float spacing = tutorial_.lesson == TutorialLesson::Charge ? 3.0f : 5.2f;
    for (int index = 0; index < count; ++index) {
        const float x = (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f) * spacing;
        const size_t before = enemies_.size();
        SpawnStageEnemy(x, 1.4f, 42.0f, Enemy::Behavior::Formation,
            Enemy::EntryStyle::TightFormation, hp, 1.2f);
        if (enemies_.size() > before) { enemies_.back()->SetTrainingTarget(true); }
    }
}

void GameRuntime::BeginTutorialLesson(TutorialLesson lesson)
{
    // 残弾やロック先を次の課題へ持ち越さない。弾は通常の更新経路でプールへ戻す。
    for (auto& bullet : playerBullets_) { bullet->Kill(); }
    for (auto& bullet : enemyBullets_) { bullet->Kill(); }
    for (auto& enemy : enemies_) { enemy->Kill(); }
    homingBulletTargets_.clear();
    tutorial_ = {};
    tutorial_.started = true;
    tutorial_.lesson = lesson;
    tutorial_.defeatedAtStart = defeatedEnemyCount_;
    tutorial_.skillAtStart = phantomActivationCount_;
    shootCooldown_ = 0;
    shootBufferTimer_ = 0;
    chargeTimer_ = 0;
    chargeFlashTimer_ = 0;
    enemyShotTimer_ = 120;
    if (lesson == TutorialLesson::Shoot) { SpawnTutorialTargets(2, 3); }
    if (lesson == TutorialLesson::Dodge) { SpawnTutorialTargets(1, 3); }
    if (lesson == TutorialLesson::Charge) { SpawnTutorialTargets(2, 3); }
    if (lesson == TutorialLesson::Skill) {
        phantomReady_ = true;
        phantomCooldown_ = 0.0f;
        SpawnTutorialTargets(3, 3);
    }
    if (lesson == TutorialLesson::Fever) {
        // 満タン手前から始めるが、発動は本編と同じ命中・撃破による自動発動を使う。
        feverGauge_ = 65;
        SpawnTutorialTargets(3, 2);
    }
    if (lesson == TutorialLesson::Complete) {
        isGameClear_ = true;
        resultTransitionTimer_ = 30;
        resultSelectedItem_ = 0;
    }
}

void GameRuntime::CompleteTutorialLesson()
{
    if (tutorial_.success > 0.0f) { return; }
    tutorial_.success = 66.0f;
    for (auto& bullet : enemyBullets_) { bullet->Kill(); }
    PlaySfx("skill_ready");
}

void GameRuntime::UpdateTutorialAttack()
{
    if (!tutorial_.started || tutorial_.lesson != TutorialLesson::Dodge || tutorial_.success > 0.0f) { return; }
    const float step = dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.0f, 3.0f) : 1.0f;
    tutorial_.attackTimer -= step;
    enemyShotTimer_ = static_cast<int>((std::max)(0.0f, tutorial_.attackTimer));
    if (tutorial_.attackTimer <= 0.0f) {
        for (const auto& enemy : enemies_) {
            if (!enemy->IsDead() && enemy->CanShoot()) {
                FireEnemyBullet(enemy->GetAimPosition(), EnemyBulletStyle::Standard);
                tutorial_.attackTimer = 150.0f;
                break;
            }
        }
    }
    // 空振りのSHIFTでは進めない。接近した弾に対する回避ならジャスト判定より広く認める。
    if (player_->IsDodging()) {
        const auto player = player_->GetTranslate();
        for (const auto& bullet : enemyBullets_) {
            const auto p = bullet->GetTranslate();
            if (!bullet->IsDead() && p.z - player.z >= -1.0f && p.z - player.z < 11.0f &&
                std::abs(p.x - player.x) < 6.0f && std::abs(p.y - player.y) < 2.5f) {
                tutorial_.dodgeConfirmed = true;
            }
        }
    }
}

void GameRuntime::UpdateTutorialLesson()
{
    if (!IsTutorial() || !player_ || isGameOver_ || isGameClear_) { return; }
    if (!tutorial_.started) { BeginTutorialLesson(TutorialLesson::Move); }
    const float step = dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.0f, 3.0f) : 1.0f;
    tutorial_.elapsed += step;
    if (tutorial_.success > 0.0f) {
        tutorial_.success = (std::max)(0.0f, tutorial_.success - step);
        if (tutorial_.success <= 0.0f) {
            BeginTutorialLesson(static_cast<TutorialLesson>(static_cast<int>(tutorial_.lesson) + 1));
        }
        return;
    }
    const int alive = static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(),
        [](const auto& enemy) { return !enemy->IsDead(); }));
    switch (tutorial_.lesson) {
    case TutorialLesson::Move: {
        const auto target = GetTutorialMoveTarget();
        const auto player = player_->GetTranslate();
        const float dx = player.x - target.x;
        const float dy = player.y - target.y;
        tutorial_.markerHold = dx * dx + dy * dy < 1.0f ? tutorial_.markerHold + step : 0.0f;
        if (tutorial_.markerHold >= 8.0f) {
            ++tutorial_.progress;
            tutorial_.markerHold = 0.0f;
            PlaySfx("hit");
            if (tutorial_.progress >= 3) { CompleteTutorialLesson(); }
        }
        break;
    }
    case TutorialLesson::Shoot:
        tutorial_.progress = defeatedEnemyCount_ - tutorial_.defeatedAtStart;
        if (tutorial_.progress >= 2) { CompleteTutorialLesson(); }
        else if (alive == 0) { SpawnTutorialTargets(2 - tutorial_.progress, 3); }
        break;
    case TutorialLesson::Dodge:
        if (tutorial_.dodgeConfirmed) { tutorial_.progress = 1; CompleteTutorialLesson(); }
        else if (alive == 0) { SpawnTutorialTargets(1, 3); }
        break;
    case TutorialLesson::Charge:
        if (tutorial_.chargedHit) { tutorial_.progress = 1; CompleteTutorialLesson(); }
        else if (alive == 0) { SpawnTutorialTargets(2, 3); }
        break;
    case TutorialLesson::Skill:
        if (phantomActivationCount_ > tutorial_.skillAtStart && !IsPhantomRaidActive()) {
            tutorial_.progress = 1;
            CompleteTutorialLesson();
        } else if (alive == 0 && !IsPhantomRaidActive()) { SpawnTutorialTargets(3, 3); }
        break;
    case TutorialLesson::Fever:
        if (feverTimer_ > 0) {
            if (tutorial_.feverDefeatedAtStart < 0) { tutorial_.feverDefeatedAtStart = defeatedEnemyCount_; }
            tutorial_.feverExperience += step;
            tutorial_.progress = defeatedEnemyCount_ - tutorial_.feverDefeatedAtStart;
            if (tutorial_.progress >= 3 && tutorial_.feverExperience >= 300.0f && !IsPhantomRaidActive()) {
                CompleteTutorialLesson();
            }
        }
        if (alive == 0 && tutorial_.success <= 0.0f && !IsPhantomRaidActive()) { SpawnTutorialTargets(3, 2); }
        break;
    case TutorialLesson::Complete: break;
    }
}

void GameRuntime::DrawTutorialGuideHud()
{
    if (!IsTutorial() || !tutorial_.started || isGameClear_ || isGameOver_) { return; }
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    const float s = CombatHud::Scale(size);
    const int lesson = std::clamp(static_cast<int>(tutorial_.lesson), 0, 5);
    const bool success = tutorial_.success > 0.0f;
    const ImU32 accent = success ? CombatHud::Health :
        (tutorial_.lesson == TutorialLesson::Fever ? CombatHud::FeverCharge : CombatHud::White);
    auto* draw = ImGui::GetForegroundDrawList();
    const ImVec2 start{ min.x + (size.x - 500.0f * s) * 0.5f, min.y + 112.0f * s };
    const auto p = [&](float x, float y) { return ImVec2(start.x + x * s, start.y + y * s); };
    CombatHud::Panel(draw, start, p(500, 112), s);
    char stage[32]{};
    std::snprintf(stage, sizeof(stage), "%02d / 06", lesson + 1);
    CombatHud::Number(draw, p(476, 17), 23 * s, CombatHud::Muted, stage, true);
    CombatHud::Heading(draw, p(22, 17), 24 * s, accent, success ? "クリア" : kLessonNames[lesson]);
    char progress[128]{};
    const char* action = "W A S D で、輪の位置へ機体を動かす";
    switch (tutorial_.lesson) {
    case TutorialLesson::Move: break;
    case TutorialLesson::Shoot:
        std::snprintf(progress, sizeof(progress), "マウスで狙う / SPACE 長押し   %d / 2 機", (std::min)(tutorial_.progress, 2));
        action = progress;
        break;
    case TutorialLesson::Dodge: {
        const auto player = player_->GetTranslate();
        const bool incoming = std::any_of(enemyBullets_.begin(), enemyBullets_.end(), [&](const auto& bullet) {
            const auto b = bullet->GetTranslate();
            return !bullet->IsDead() && b.z > player.z && b.z < player.z + 16.0f &&
                std::abs(b.x - player.x) < 5.0f && std::abs(b.y - player.y) < 2.5f;
        });
        action = incoming ? "今！ A・D + SHIFT でかわす" : "弾が近づいたら A・D + SHIFT";
        break;
    }
    case TutorialLesson::Charge:
        action = chargeTimer_ >= chargeShotThreshold_ ? "チャージ完了 / 狙って SPACE" : "SPACE を離してためる";
        break;
    case TutorialLesson::Skill: action = "敵を画面に捉えて Q / 3機へ連続攻撃"; break;
    case TutorialLesson::Fever:
        if (feverTimer_ > 0) {
            std::snprintf(progress, sizeof(progress), "フィーバー発動！   %d / 3 機撃破", (std::min)(tutorial_.progress, 3));
            action = progress;
        } else { action = "敵を倒す / ゲージ満タンで自動発動"; }
        break;
    case TutorialLesson::Complete: break;
    }
    if (success) {
        action = lesson < 5 ? kLessonNames[lesson + 1] : "すべての練習をクリア";
        CombatHud::Readout(draw, p(22, 55), 18 * s, CombatHud::Muted, lesson < 5 ? "次へ" : "完了");
        CombatHud::Readout(draw, p(76, 55), 18 * s, CombatHud::White, action);
    } else {
        CombatHud::Readout(draw, p(22, 55), 18 * s, CombatHud::White, action);
    }
    for (int index = 0; index < 6; ++index) {
        const bool done = index < lesson || (index == lesson && success);
        const ImU32 color = done ? CombatHud::Health : (index == lesson ? CombatHud::White : IM_COL32(73, 77, 84, 255));
        draw->AddRectFilled(p(22 + index * 77.0f, 94), p(91 + index * 77.0f, 97), CombatHud::SurfaceColor(color));
    }
    if (tutorial_.lesson == TutorialLesson::Move && !success) {
        Math::Vector2 screen{};
        if (TryProjectToScreen(GetTutorialMoveTarget(), screen)) {
            const float radius = 31.0f * s;
            const ImVec2 center{ screen.x, screen.y };
            draw->AddCircleFilled(center, radius, IM_COL32(10, 15, 18, 95), 48);
            draw->AddCircle(center, radius, CombatHud::SurfaceColor(CombatHud::White), 48, 2.0f * s);
            const float rate = std::clamp(tutorial_.markerHold / 8.0f, 0.0f, 1.0f);
            if (rate > 0.0f) {
                draw->PathArcTo(center, radius + 5.0f * s, -1.5708f, -1.5708f + rate * 6.2832f, 40);
                draw->PathStroke(CombatHud::SurfaceColor(CombatHud::Health), 0, 3.0f * s);
            }
            char count[16]{};
            std::snprintf(count, sizeof(count), "%d / 3", tutorial_.progress + 1);
            CombatHud::Readout(draw, { center.x - 22 * s, center.y + 38 * s }, 16 * s, CombatHud::White, count);
        }
    }
}

void GameRuntime::DrawTutorialResult()
{
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    const float s = CombatHud::Scale(size);
    const ImVec2 origin{ min.x + (size.x - 1280 * s) * 0.5f, min.y + (size.y - 720 * s) * 0.5f };
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * s, origin.y + y * s); };
    auto* draw = ImGui::GetForegroundDrawList();
    draw->PushClipRect({ min.x, min.y }, { min.x + size.x, min.y + size.y }, true);
    draw->AddRectFilled({ min.x, min.y }, { min.x + size.x, min.y + size.y },
        CombatHud::SurfaceColor(MenuUi::CanvasPaper));
    draw->AddQuadFilled(p(986, -200), p(1500, -200), p(1500, 920), p(782, 920),
        CombatHud::SurfaceColor(MenuUi::Ink));
    MenuUi::Brand(draw, p(64, 36), 24 * s, MenuUi::Ink);
    CombatHud::Slant(draw, p(42, 108), 150 * s, MenuUi::Ink, "READY", false, false);
    MenuUi::Text(draw, p(64, 262), 27 * s, MenuUi::Ink, "チュートリアル完了");
    // 練習に順位や失敗回数の採点は不要。体験できた操作だけを振り返る。
    for (int index = 0; index < 6; ++index) {
        const float x = 64.0f + static_cast<float>(index / 3) * 330.0f;
        const float y = 376.0f + static_cast<float>(index % 3) * 68.0f;
        draw->AddLine(p(x, y + 10), p(x + 5, y + 15), CombatHud::SurfaceColor(CombatHud::Health), 2 * s);
        draw->AddLine(p(x + 5, y + 15), p(x + 15, y + 3), CombatHud::SurfaceColor(CombatHud::Health), 2 * s);
        MenuUi::Text(draw, p(x + 30, y), 24 * s, MenuUi::Ink, kLessonNames[index]);
    }
    if (MenuUi::Pressed(input_, DIK_LEFT) || MenuUi::Pressed(input_, DIK_A) ||
        MenuUi::Pressed(input_, DIK_UP) || MenuUi::Pressed(input_, DIK_W)) {
        resultSelectedItem_ = (resultSelectedItem_ + 2) % 3;
    }
    if (MenuUi::Pressed(input_, DIK_RIGHT) || MenuUi::Pressed(input_, DIK_D) ||
        MenuUi::Pressed(input_, DIK_DOWN) || MenuUi::Pressed(input_, DIK_S)) {
        resultSelectedItem_ = (resultSelectedItem_ + 1) % 3;
    }
    const bool confirm = MenuUi::Pressed(input_, DIK_RETURN);
    const int confirmed = resultSelectedItem_;
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({ 1280 * s, 720 * s });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::Begin("##TutorialResult", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
    const char* labels[]{ "本編へ", "もう一度", "タイトルへ" };
    for (int index = 0; index < 3; ++index) {
        const bool clicked = MenuUi::FlightAction(draw, labels[index], labels[index],
            p(950 - index * 18.0f, 254 + index * 100.0f), { 290 * s, 64 * s }, s, resultSelectedItem_ == index);
        MenuUi::SelectHovered(resultSelectedItem_, index);
        if (clicked || (confirm && confirmed == index)) {
            if (index == 0) { isMainGameRequested_ = true; }
            if (index == 1) { isRetryRequested_ = true; }
            if (index == 2) { isExitRequested_ = true; }
            break;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    draw->PopClipRect();
}

#ifdef _DEBUG
bool GameRuntime::RunTutorialProbe(const std::string& logPath, bool preview)
{
    // 実際の入力・弾・衝突で全課題を通す。課題の成功フラグや撃破数は書き換えない。
    static int frame = 0;
    static int lessonFrame = 0;
    static int previousLesson = -1;
    static int previewMask = 0;
    static int resultFrame = 0;
    static bool awaitingRetry = false;
    static float pausedElapsed = 0.0f;
    static float pausedRail = 0.0f;
    static int chargedWrongKills = 0;
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
        require(tutorial_.lesson == TutorialLesson::Move && tutorial_.progress == 0 &&
            defeatedEnemyCount_ == 0 && player_->GetHp() == 100 && feverActivationCount_ == 0 &&
            !isMainGameRequested_ && !isRetryRequested_, "Tutorial retry did not reset all lesson state");
        log("RETRY_RESET_OK");
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    const int lesson = static_cast<int>(tutorial_.lesson);
    if (previousLesson != lesson) {
        if (previousLesson >= 0) { log("LESSON_OK index=" + std::to_string(previousLesson)); }
        previousLesson = lesson;
        lessonFrame = 0;
        log("BEGIN index=" + std::to_string(lesson));
    }
    if (preview) {
        const int point = isGameClear_ ? 6 : (tutorial_.success > 0.0f ? 7 : lesson);
        if ((lessonFrame == 50 || (isGameClear_ && resultTransitionTimer_ <= 0) || tutorial_.success > 0.0f) &&
            !(previewMask & (1 << point))) {
            previewMask |= 1 << point;
            phantomPreviewPaused_ = true;
        }
        if (phantomPreviewPaused_) {
            ImGui::SetNextWindowPos({ 20, 155 }, ImGuiCond_Always);
            ImGui::SetNextWindowSize({ 154, 72 }, ImGuiCond_Always);
            ImGui::Begin("Tutorial QA", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
            const bool next = ImGui::Button("NEXT / F8") || ImGui::IsKeyPressed(ImGuiKey_F8, false);
            ImGui::End();
            if (next) { phantomPreviewPaused_ = false; }
            input_->SetTestFrame({}, mouse);
            return false;
        }
    }
    ++frame;
    ++lessonFrame;
    require(!isGameOver_, "Tutorial failed instead of allowing retry");
    require(!bossSpawned_ && stageProgress_ == 0.0f && stageEnemyEventTriggered_.empty(),
        "Tutorial entered main-game schedule");
    if (frame == 30) { keys[DIK_ESCAPE] = 0x80; }
    if (frame == 31) {
        require(isPaused_, "Tutorial pause did not open");
        pausedElapsed = tutorial_.elapsed;
        pausedRail = railDistance_;
    }
    if (frame > 31 && frame <= 90) {
        require(isPaused_ && tutorial_.elapsed == pausedElapsed && railDistance_ == pausedRail,
            "Tutorial progressed while paused");
    }
    if (frame == 91) { keys[DIK_ESCAPE] = 0x80; }
    if (frame == 92) { require(!isPaused_, "Tutorial pause did not close"); log("PAUSE_FREEZE_OK"); }
    if (isGameClear_) {
        require(lesson == 6 && feverActivationCount_ > 0 && phantomActivationCount_ > 0,
            "Tutorial completed without skill and automatic fever");
        if (resultTransitionTimer_ <= 0) {
            if (resultFrame < 10) { keys[DIK_D] = 0x80; }
            if (resultFrame > 0 && resultFrame <= 20) {
                require(resultSelectedItem_ == 1, "Tutorial result selection repeated during held key");
            }
            if (resultFrame == 21) { keys[DIK_RETURN] = 0x80; awaitingRetry = true; log("ALL_LESSONS_OK retry_by_menu=1"); }
            ++resultFrame;
        }
        input_->SetTestFrame(keys, mouse);
        return false;
    }
    if (tutorial_.success > 0.0f || isPaused_) {
        input_->SetTestFrame(keys, mouse);
        return false;
    }
    const Enemy* target = nullptr;
    for (const auto& enemy : enemies_) {
        Math::Vector2 screen{};
        if (!enemy->IsDead() && enemy->IsTargetable() && TryProjectToScreen(enemy->GetAimPosition(), screen)) {
            target = enemy.get();
            const auto origin = ImGui::GetMainViewport()->Pos;
            mouse = { screen.x - origin.x, screen.y - origin.y };
            break;
        }
    }
    switch (tutorial_.lesson) {
    case TutorialLesson::Move:
        if (lessonFrame <= 190) {
            require(tutorial_.progress == 0, "Idle time skipped movement lesson");
            if (lessonFrame == 150) { keys[DIK_Q] = keys[DIK_SPACE] = 0x80; }
            if (lessonFrame == 190) { log("IDLE_AND_UNRELATED_ACTIONS_DONT_SKIP_OK"); }
        } else {
            const auto destination = GetTutorialMoveTarget();
            const auto player = player_->GetTranslate();
            if (std::abs(destination.x - player.x) > 0.25f) { keys[destination.x > player.x ? DIK_D : DIK_A] = 0x80; }
            if (std::abs(destination.y - player.y) > 0.25f) { keys[destination.y > player.y ? DIK_W : DIK_S] = 0x80; }
        }
        break;
    case TutorialLesson::Shoot:
        // 元の退場期限を超えても二機の標的が残ることを確認してから撃つ。
        if (lessonFrame == 720) {
            require(enemies_.size() == 2 && escapedEnemyCount_ == 0 && tutorial_.progress == 0,
                "Training targets escaped before player learned aiming");
            log("TARGETS_WAIT_FOR_PLAYER_OK");
        }
        if (lessonFrame > 720 && target) { keys[DIK_SPACE] = 0x80; }
        break;
    case TutorialLesson::Dodge:
        if (lessonFrame == 20) { keys[DIK_LSHIFT] = 0x80; }
        if (lessonFrame == 60) { require(!tutorial_.dodgeConfirmed, "Empty dodge skipped dodge lesson"); }
        if (lessonFrame > 200) {
            const auto player = player_->GetTranslate();
            for (const auto& bullet : enemyBullets_) {
                const auto p = bullet->GetTranslate();
                if (!bullet->IsDead() && p.z - player.z > 5.0f && p.z - player.z < 10.0f && !player_->IsDodging()) {
                    keys[DIK_LSHIFT] = 0x80;
                    keys[player.x > 0.0f ? DIK_A : DIK_D] = 0x80;
                    break;
                }
            }
        }
        break;
    case TutorialLesson::Charge:
        if (lessonFrame < 360 && target) { keys[DIK_SPACE] = 0x80; }
        if (lessonFrame == 360) {
            require(!tutorial_.chargedHit && tutorial_.success == 0.0f, "Normal shot skipped charged-hit lesson");
            chargedWrongKills = defeatedEnemyCount_ - tutorial_.defeatedAtStart;
            require(chargedWrongKills > 0, "Wrong-input test never hit the target");
            log("NORMAL_SHOTS_DONT_SKIP_CHARGE_OK kills=" + std::to_string(chargedWrongKills));
        }
        if (lessonFrame > 360 && target && chargeTimer_ >= chargeShotThreshold_) { keys[DIK_SPACE] = 0x80; }
        break;
    case TutorialLesson::Skill:
        if (target && lessonFrame > 120 && !IsPhantomRaidActive()) { keys[DIK_Q] = 0x80; }
        break;
    case TutorialLesson::Fever:
        if (target) { keys[DIK_SPACE] = 0x80; }
        break;
    case TutorialLesson::Complete: break;
    }
    if (lessonFrame % 600 == 0) {
        log("PROGRESS index=" + std::to_string(lesson) + " progress=" + std::to_string(tutorial_.progress) +
            " kills=" + std::to_string(defeatedEnemyCount_) + " hp=" + std::to_string(player_->GetHp()));
    }
    input_->SetTestFrame(keys, mouse);
    return false;
}
#endif
