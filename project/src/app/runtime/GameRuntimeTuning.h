#pragma once

#include "app/combat/FeverState.h"
#include "app/presentation/GameplayCamera.h"
#include "app/combat/CombatEffects.h"

// 複数の処理で共有する調整値　ゲーム固有の数値はここを参照する
namespace GameRuntimeTuning {
    // 起動時に用意する弾・エフェクトの再利用枠
    inline constexpr int kTargetPlayerBulletPoolCount = 40; // 9f間隔・寿命280fのフィーバー空撃ちも起動時の固定枠で賄う
    inline constexpr int kTargetEnemyBulletPoolCount = 64; // 連射・扇状弾も起動時の固定プールだけで賄う
    inline constexpr int kTargetHitEffectObjectPoolCount = static_cast<int>(CombatEffects::kVisualCapacity); // 編隊同時撃破＋着弾＋回収まで起動時の固定枠で賄う
    // ワールド座標での距離と、爆風一回あたりのダメージ
    inline constexpr float kChargeSplashRadius = 3.4f; // 敵の中心間の距離　編隊中央なら左右へ届き、端なら反対端へ届かない
    inline constexpr int kChargeSplashDamage = 3; // 周囲の小型機を削る　直撃対象には重ねず、爆風からの再誘爆もしない
    inline constexpr int kNormalShotDamage = 1;
    inline constexpr int kChargeDirectDamage = 4;
    inline constexpr int kChargeBossDamage = 6;
    inline constexpr int kFeverShotDamage = 2; // 高速連射と三機貫通を主な強みにし、単体火力を抑える
    inline constexpr int kRiftPassScore = 500; // 連続撃破・フィーバーの倍率も乗る、通過に対する得点
    // FovYはラジアン、Distanceはワールド座標の距離
    inline constexpr float kGameplayCameraBaseFovY = GameplayCamera::kTutorialFov;
    inline constexpr float kRushCameraBaseFovY = GameplayCamera::kFlightFov; // 路面・側方の流れを広く見せ、中央の戦闘領域は残す
    inline constexpr float kRushCameraDistance = GameplayCamera::kFlightDistance; // 広角化と同時に寄せ、自機の画面上の大きさを保つ
    inline constexpr float kGameplayCameraInitialDistance = 15.8f;
    inline constexpr float kJustDodgeGrazePadding = 1.25f;
    inline constexpr int kFeverGaugeMax = FeverState::kGaugeMax;
    inline constexpr int kFeverDurationFrames = FeverState::kDurationFrames;
    inline constexpr int kWaveCount = 3;
}
