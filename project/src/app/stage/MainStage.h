#pragma once

#include "app/enemy/EnemyTypes.h"

#include <span>

// 航空都市の配置データ　出現順と区間の設定を置き、進行中の状態は持たない
namespace MainStage {
inline constexpr float kBossWarningProgress = 300.0f;
inline constexpr float kBossSpawnProgress = 330.0f;

struct Segment {
    float startDistance;
    float endDistance;
    float targetSpeed;
    float yawBias;
    float rollBias;
    float liftBias;
    float fovBoost;
    const char* name;
};

struct RailEvent {
    float distance;
    float shakePower;
    int shakeDuration;
};

struct GateSpawn {
    float distance; // 編隊の進行に合わせて、余裕を持って前方へ見せる
    float x, y, leadDistance;
};

struct EnemySpawn {
    float distance;     // 配置上の進行値　背景のワールド座標とは分けて管理する
    float x;
    float y;
    float leadDistance; // 現在のレール位置から前方へ離すワールド距離
    EnemyBehavior behavior;
    EnemyEntryStyle entryStyle;
    float shakePower;
    int shakeDuration;
    int maxHpOverride;
    float scaleMultiplier;
    const char* beatName;
};

std::span<const Segment> Segments();
std::span<const RailEvent> RailEvents();
std::span<const EnemySpawn> EnemySpawns();
std::span<const GateSpawn> GateSpawns();
}
