// ===== GameRuntimePhantom.cpp =====
// スキルの発動時に対象を決め、連撃のタイミングに合わせてダメージと演出を反映する

#include "app/runtime/GameRuntime.h"

#include "engine/base/DirectXCommon.h"
#include "engine/io/Input.h"
#include <algorithm>

// スキル本体と表示を一緒にリセットし、新しい戦闘に前回の状態を残さない
void GameRuntime::ResetPhantomRaid()
{
    playerSkill_.Reset();
    playerSkillPresentation_.Reset();
}

// 練習課題でスキルをすぐ試せる状態へ切り替える
void GameRuntime::GrantPhantomRaid()
{
    if (!isGameOver_ && !isGameClear_ && playerSkill_.TryRecover()) { PlaySfx("skill_ready"); }
}

// 発動できるときに対象を決め、スキルの進行と最初の演出を始める
bool GameRuntime::TryActivatePhantomRaid()
{
    if (IsTutorial() && !tutorial_.AllowsSkill()) { return false; }
    if (!playerSkill_.IsReady() || IsPhantomRaidActive() || !player_ || player_->IsDead() || isGameClear_ || isGameOver_) {
        return false;
    }
    PlayerSkill::Selection selection(rewards_.Fever().IsActive());
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    // 照準に近い敵を最初に選び、画面内・前方75m以内だけへ連鎖する
    for (const auto& enemy : stageEnemies_.Active()) {
        if (!enemy || enemy->IsDead() || !enemy->IsTargetable()) { continue; }
        const auto position = enemy->GetAimPosition();
        const float ahead = position.z - player_->GetTranslate().z;
        Math::Vector2 screen{};
        if (ahead < 1.0f || ahead > 75.0f || !TryProjectToScreen(position, screen) ||
            screen.x < min.x || screen.x > min.x + size.x || screen.y < min.y || screen.y > min.y + size.y) {
            continue;
        }
        const float dx = screen.x - playerControl_.Aim().Reticle().x;
        const float dy = screen.y - playerControl_.Aim().Reticle().y;
        selection.Consider(enemy.get(), position, dx * dx + dy * dy, enemy->IsBoss());
    }
    if (!playerSkill_.TryActivate(selection)) { return false; }
    playerSkillPresentation_.Reset();
    PlaySfx("skill_start");
    return true;
}

// スキル側から届く連撃・最後の一撃の通知に合わせて、実際のダメージと演出を適用する
void GameRuntime::UpdatePhantomRaid()
{
    const float step = dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.0f, 4.0f) : 1.0f;
    playerSkill_.TickTimers(step);
    playerSkillPresentation_.Advance(step);
    if (!player_ || player_->IsDead() || isGameOver_) {
        playerSkill_.CancelSequence();
        return;
    }
    if (!IsPhantomRaidActive()) {
        // 回避や命中がなくても時間経過だけで回復し、使いどころを自由に選べる
        GrantPhantomRaid();
        if (input_ && input_->TriggerKey(DIK_Q)) {
            TryActivatePhantomRaid();
        }
        return;
    }
    playerSkill_.AdvanceSequence(step);
    const auto battle = BuildCombatContext();
    for (const auto& target : playerSkill_.Targets()) {
        if (const Enemy* enemy = stageEnemies_.FindTargetable(target.enemy)) {
            playerSkill_.UpdateTargetPosition(target.enemy, enemy->GetAimPosition());
        }
    }
    for (int strike = playerSkill_.ConsumeStrike(); strike >= 0; strike = playerSkill_.ConsumeStrike()) {
        const auto& target = playerSkill_.Targets()[strike % playerSkill_.TargetCount()];
        Enemy* enemy = stageEnemies_.FindTargetable(target.enemy);
        if (enemy) {
            playerSkill_.MarkStrike(strike, enemy->GetAimPosition());
            playerSkillPresentation_.ShowStrike(strike, player_->GetTranslate(), target.position);
            // 連撃の段階が耳でも分かるよう、2打目以降だけ少しずつ上げる
            PlaySfx("slash", 0.98f + 0.035f * static_cast<float>(strike));
            AddCameraShake(0.032f, 3);
            if (enemy->IsBoss()) {
                combatResolver_.ApplySkillDamage(battle, *enemy, 1);
            }
        }
    }
    if (playerSkill_.ConsumeFinisher()) {
        for (const auto& target : playerSkill_.Targets()) {
            if (Enemy* enemy = stageEnemies_.FindTargetable(target.enemy)) {
                playerSkill_.UpdateTargetPosition(target.enemy, enemy->GetAimPosition());
                if (target.marks > 0) {
                    // 通常時は小型編隊を削り、フィーバー中なら一掃できる　硬い敵には追撃が必要
                    combatResolver_.ApplySkillDamage(battle, *enemy, playerSkill_.FinisherDamage());
                }
            }
        }
        playerSkillPresentation_.ShowFinisher(playerSkill_);
        // 斬った場所の近くの弾だけを払う　画面全体を無条件に消さない
        for (auto& bullet : projectiles_.EnemyPool().Active()) {
            for (int index = 0; index < playerSkill_.TargetCount(); ++index) {
                const auto& p = playerSkill_.Targets()[index].position;
                const auto& b = bullet->GetTranslate();
                const float dx = p.x - b.x, dy = p.y - b.y, dz = p.z - b.z;
                if (dx * dx + dy * dy + dz * dz < 49.0f) {
                    bullet->Kill();
                }
            }
        }
        PlaySfx("slash_finish");
        AddCameraShake(playerSkill_.IsEmpowered() ? 0.18f : 0.14f, 7);
    }
    playerSkill_.EndSequenceIfElapsed();
}
