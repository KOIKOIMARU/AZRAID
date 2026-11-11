#include "app/runtime/GameRuntime.h"

// 衝突判定が使う敵、弾、得点、演出への参照をそろえる。命中とダメージの計算はCombatResolverが担当する。
CombatResolver::Context GameRuntime::BuildCombatContext()
{
    return {
        .enemies = stageEnemies_.Active(),
        .projectiles = projectiles_,
        .progress = combatProgress_,
        .effects = combatEffects_,
        .fever = rewards_.Fever(),
        .skill = playerSkill_,
        .bossAttack = bossAttack_,
        .player = player_.get(),
        .tutorial = IsTutorial() ? &tutorial_ : nullptr,
        .gameOver = isGameOver_,
        .gameClear = isGameClear_,
        .reactions = *this
    };
}

// 自機弾から先に判定する。同じフレームでボスを倒したら、その後の敵弾判定は終了する。
void GameRuntime::UpdateGameplayCollisions()
{
    if (isGameOver_ || isGameClear_) { return; }

    const auto battle = BuildCombatContext();
    combatResolver_.ResolvePlayerShots(battle);
    if (!isGameClear_) { combatResolver_.ResolveEnemyShots(battle); }
}

// ボス撃破をステージクリアに反映し、撃破演出からリザルトへ進む準備をする。
void GameRuntime::OnBossDefeated(const Math::Vector3& position)
{
    bossPresentation_.BeginDefeat(position);
    bossAttack_.CancelAttack();
    // 撃破後に残弾だけが画面へ迫らないよう、確保済みプールへ返す。
    projectiles_.RecycleEnemyBullets();
    AddCameraShake(0.22f, 18);
    stageEnemies_.MarkBossDefeated();
    stageFlight_.FinishStage();
    isGameClear_ = true;
    stageResults_.SetDelay(BossPresentation::kResultDelayFrames);
}
