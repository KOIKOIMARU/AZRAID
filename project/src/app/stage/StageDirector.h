#pragma once

#include "app/stage/MainStage.h"

#include <cstddef>
#include <vector>

// 配置の進行、編隊を出せる空き枠、出現済みの状態を管理する
// 敵の所有・生成はStageEnemies、カメラや警告の演出はGameRuntimeが担当する
class StageDirector {
public:
    static constexpr int kMaxActiveEnemies = 5;
    static constexpr int kMaxEnemyPressure = 2;
    static constexpr int kMaxSpawnsPerFrame = 4;
    struct EnemyLoad {
        int count = 0;    // 生存している通常敵の数　ボスと撃破済みの敵は含めない
        int pressure = 0; // 狙撃・盾など、同時に対処する攻撃の負荷
    };

    void Reset(bool tutorial = false); // 出撃・再挑戦時だけ配置の件数に合わせて領域を確保する
    static int EnemyPressure(EnemyBehavior behavior);
    bool CanAdmitGroup(size_t eventIndex, EnemyLoad load) const;
    // 時間は60fps換算　停止中には呼ばず、演出スロー中は同じ倍率を渡す
    void Advance(float frameStep, EnemyLoad load, bool feverActive, bool bossSpawned);
    // 発生した項目を一度だけ返す　敵の生成後は最新の負荷を渡して次の項目を確認する
    const MainStage::EnemySpawn* TryConsumeEnemy(EnemyLoad load);
    // 進行度で発生する画面演出を、各イベントにつき一度だけ取り出す
    const MainStage::RailEvent* TryConsumeRailEvent();
    // 進行度に届いた未使用のリングを1件返す　該当がなければnullptr
    const MainStage::GateSpawn* TryConsumeGate();

    const MainStage::Segment& CurrentSegment() const;
    int WaveIndex() const;
    bool IsBossSpawnDue() const { return !tutorial_ && progress_ >= MainStage::kBossSpawnProgress; }
    bool IsTutorial() const { return tutorial_; }
    float Progress() const { return progress_; }
    float TimelineSpeed() const { return timelineSpeed_; }
    float HandoffFrames() const { return handoffFrames_; }
    float EmptyFrames() const { return emptyFrames_; }
    int EnemyCount() const { return static_cast<int>(enemyTriggered_.size()); }
    int EmittedEnemyCount() const;
    bool HasPendingEnemies() const; // 未出現の配置が残っているか　フラグの持ち方は外へ公開しない

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
    int SeekForDebug(float progress, bool skipAllEnemies = false);
    void DisableSpawnsForDebug();
    void HoldForDebug(float progress); // 演出確認用　出現済みの状態は保ち、進行と経過時間だけを固定する
#endif

private:
    bool tutorial_ = false;
    float progress_ = 0.0f; // 自機の移動とは独立して進む、敵やイベントの配置表用の進行度
    float timelineSpeed_ = 0.0f; // 配置表の進行をフレームごとに進める速さ
    float handoffFrames_ = 0.0f; // 次の編隊を出せる状態で経過したフレーム数
    float emptyFrames_ = 0.0f; // 通常敵がいない状態で経過したフレーム数
    std::vector<bool> enemyTriggered_; // 配置表の各敵を出現済みとして扱うかの一覧
    std::vector<bool> railTriggered_; // レール演出を発生済みとして扱うかの一覧
    std::vector<bool> gateTriggered_; // リングを出現済みとして扱うかの一覧
};
