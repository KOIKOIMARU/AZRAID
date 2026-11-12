// ===== MainStage.cpp =====
// 本編の配置データ　区間ごとの飛行設定と、敵・リング・カメラ演出が出る進行度を定義する

#include "app/stage/MainStage.h"

namespace MainStage {
namespace {
constexpr Segment kSegments[] = {
    {   0.0f,  42.0f, 0.160f,  0.000f,  0.000f,  0.00f, 0.048f, "Opening" },
    {  42.0f, 104.0f, 0.205f, -0.022f,  0.042f,  0.16f, 0.068f, "Side sweep" },
    { 104.0f, 170.0f, 0.185f,  0.018f, -0.036f,  0.34f, 0.062f, "Climb and dodge" },
    { 170.0f, 230.0f, 0.235f,  0.000f, -0.055f, -0.16f, 0.102f, "Fast dive" },
    { 230.0f, 999.0f, 0.198f,  0.018f,  0.034f,  0.08f, 0.068f, "Final approach" }
};

constexpr RailEvent kRailEvents[] = {
    {  12.0f, 0.045f,  8 },
    {  54.0f, 0.065f, 10 },
    { 112.0f, 0.058f,  8 },
    { 176.0f, 0.080f, 12 },
    { 246.0f, 0.066f, 10 }
};

// 敵を撃破する前から選べる寄り道　左右・高低を変え、戦闘中の移動に理由を作る
constexpr GateSpawn kGateSpawns[] = {
    { 30.0f, 3.8f, 1.6f, 68.0f }, { 84.0f, -4.3f, 2.2f, 74.0f },
    { 138.0f, 4.7f, 0.5f, 70.0f }, { 192.0f, -4.4f, 1.8f, 74.0f },
    { 246.0f, 4.0f, 2.3f, 72.0f }, { 288.0f, -3.8f, 1.0f, 76.0f }
};

// 本編の配置　チュートリアルの案内・練習用HP・反撃制限は適用しない
constexpr EnemySpawn kEnemySpawns[] = {
    {  10.0f, -2.0f,  0.0f, 42.0f, EnemyBehavior::Formation,     EnemyEntryStyle::VFormation, 0.020f,  4,  4, 1.08f, "Opening pair" },
    {  16.0f,  2.7f,  0.2f, 42.0f, EnemyBehavior::Formation,     EnemyEntryStyle::VFormation, 0.020f,  4,  4, 1.08f, "Opening pair" },
    {  28.0f,  0.0f,  1.4f, 50.0f, EnemyBehavior::DiveBomber,    EnemyEntryStyle::Direct,     0.030f,  7,  7, 1.08f, "Crossing assault" },
    {  32.0f,  5.1f,  1.2f, 48.0f, EnemyBehavior::Swoop,         EnemyEntryStyle::RightSweep, 0.032f,  6,  7, 1.08f, "Crossing assault" },
    {  40.0f, -2.6f, -0.4f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Avenue pursuit" },
    {  40.0f,  0.0f,  0.6f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Avenue pursuit" },
    {  48.0f,  2.2f,  1.0f, 48.0f, EnemyBehavior::DiveBomber, EnemyEntryStyle::RightSweep, 0.000f, 0, 7, 1.08f, "Roof dive" },
    {  48.0f, -2.2f, -0.3f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::VFormation, 0.000f, 0, 5, 1.00f, "Roof dive" },
    {  58.0f, -5.1f,  0.7f, 48.0f, EnemyBehavior::Swoop,         EnemyEntryStyle::LeftSweep,  0.032f,  6,  7, 1.08f, "Cross sweep" },
    {  66.0f, -2.6f,  0.0f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Wing lead-in" },
    {  66.0f,  0.0f,  1.0f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Wing lead-in" },
    // 手前の三機を範囲攻撃で削るか、奥の狙撃機を先に処理するかを選べる組み合わせ
    {  78.0f, -4.6f, -0.6f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.022f, 5, 4, 1.00f, "Sniper wing" },
    {  78.0f, -2.0f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Sniper wing" },
    {  78.0f,  0.6f, -0.6f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Sniper wing" },
    {  78.0f,  5.3f,  2.0f, 48.0f, EnemyBehavior::Sniper,    EnemyEntryStyle::PopShooter,    0.000f, 0, 8, 1.18f, "Sniper wing" },
    {  90.0f,  3.6f,  0.6f, 48.0f, EnemyBehavior::Swoop, EnemyEntryStyle::RightSweep, 0.000f, 0, 7, 1.08f, "Wing follow-through" },
    {  90.0f, -2.6f, -0.3f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::VFormation, 0.000f, 0, 5, 1.00f, "Wing follow-through" },
    { 100.0f,  0.0f,  0.9f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Climb pursuit" },
    { 100.0f,  2.6f, -0.1f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Climb pursuit" },
    { 112.0f,  4.6f,  1.6f, 54.0f, EnemyBehavior::DiveBomber,    EnemyEntryStyle::RightSweep, 0.036f,  8,  7, 1.12f, "High dive" },
    { 124.0f, -2.9f, -0.4f, 44.0f, EnemyBehavior::Formation,     EnemyEntryStyle::VFormation, 0.024f,  5,  6, 1.12f, "Guard pair" },
    { 128.0f,  2.9f, -0.2f, 44.0f, EnemyBehavior::Formation,     EnemyEntryStyle::VFormation, 0.024f,  5,  6, 1.12f, "Guard pair" },
    { 136.0f, -2.6f,  0.4f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Guard follow-through" },
    { 136.0f,  0.0f, -0.6f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Guard follow-through" },
    { 148.0f, -4.6f,  1.5f, 54.0f, EnemyBehavior::DiveBomber,    EnemyEntryStyle::LeftSweep,  0.036f,  8,  7, 1.12f, "High dive" },
    { 152.0f,  2.6f, -0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::VFormation, 0.000f, 0, 5, 1.00f, "Shield escort" },
    { 152.0f, -2.6f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::VFormation, 0.000f, 0, 5, 1.00f, "Shield escort" },
    { 160.0f,  0.0f,  1.45f, 54.0f, EnemyBehavior::Shield,        EnemyEntryStyle::Direct,     0.070f, 14, 14, 1.24f, "Shield wall" },
    { 174.0f, -5.4f,  0.55f, 52.0f, EnemyBehavior::Crossfire,     EnemyEntryStyle::LeftSweep,  0.048f,  9,  8, 1.12f, "Crossfire pair" },
    { 174.0f,  5.4f,  1.15f, 52.0f, EnemyBehavior::Crossfire,     EnemyEntryStyle::RightSweep, 0.048f,  9,  8, 1.12f, "Crossfire pair" },
    { 182.0f,  0.0f, -0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Crossfire opening" },
    { 182.0f,  2.6f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Crossfire opening" },
    { 188.0f, -4.9f,  1.0f, 52.0f, EnemyBehavior::Swoop,         EnemyEntryStyle::LeftSweep,  0.038f,  7,  7, 1.14f, "High-speed pass" },
    { 194.0f,  2.4f,  1.3f, 48.0f, EnemyBehavior::DiveBomber, EnemyEntryStyle::RightSweep, 0.000f, 0, 7, 1.08f, "Dive pursuit" },
    { 194.0f, -2.4f, -0.3f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::VFormation, 0.000f, 0, 5, 1.00f, "Dive pursuit" },
    { 202.0f,  4.9f,  1.0f, 52.0f, EnemyBehavior::Swoop,         EnemyEntryStyle::RightSweep, 0.038f,  7,  7, 1.14f, "High-speed pass" },
    { 208.0f, -2.6f, -0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Break lead-in" },
    { 208.0f,  0.0f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Break lead-in" },
    { 214.0f,  0.0f,  1.0f, 50.0f, EnemyBehavior::Formation,     EnemyEntryStyle::VFormation, 0.030f,  6,  6, 1.18f, "Break formation" },
    { 220.0f,  2.6f, -0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Crossfire lead-in" },
    { 220.0f,  0.0f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Crossfire lead-in" },
    { 228.0f, -3.8f,  0.6f, 48.0f, EnemyBehavior::StrafeShooter, EnemyEntryStyle::PopShooter, 0.040f,  8,  10, 1.18f, "Final crossfire" },
    { 234.0f,  3.8f,  0.6f, 48.0f, EnemyBehavior::StrafeShooter, EnemyEntryStyle::PopShooter, 0.040f,  8,  10, 1.18f, "Final crossfire" },
    { 242.0f, -2.6f, -0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Final pursuit" },
    { 242.0f,  0.0f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Final pursuit" },
    { 250.0f,  4.6f,  1.0f, 48.0f, EnemyBehavior::Swoop, EnemyEntryStyle::RightSweep, 0.000f, 0, 7, 1.08f, "Support lead-in" },
    { 250.0f, -2.6f, -0.4f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::VFormation, 0.000f, 0, 5, 1.00f, "Support lead-in" },
    { 258.0f,  0.0f,  1.9f, 48.0f, EnemyBehavior::Support,       EnemyEntryStyle::Direct,     0.050f, 10,  10, 1.16f, "Support gate" },
    { 258.0f, -2.9f, -0.5f, 44.0f, EnemyBehavior::Formation,     EnemyEntryStyle::VFormation, 0.030f,  6,  6, 1.16f, "Support gate" },
    { 258.0f,  2.9f, -0.5f, 44.0f, EnemyBehavior::Formation,     EnemyEntryStyle::VFormation, 0.030f,  6,  6, 1.16f, "Support gate" },
    { 266.0f,  0.0f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Reprise lead-in" },
    { 266.0f,  2.6f, -0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Reprise lead-in" },
    // 終盤は左右を入れ替えて再登場　初見と同じ構え・攻撃なので学んだ対処を使える
    { 274.0f,  4.6f, -0.6f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.030f, 6, 4, 1.00f, "Sniper wing reprise" },
    { 274.0f,  2.0f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Sniper wing reprise" },
    { 274.0f, -0.6f, -0.6f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Sniper wing reprise" },
    { 274.0f, -5.3f,  2.0f, 48.0f, EnemyBehavior::Sniper,    EnemyEntryStyle::PopShooter,    0.000f, 0, 8, 1.18f, "Sniper wing reprise" },
    { 282.0f, -2.6f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Boss screen lead-in" },
    { 282.0f,  0.0f, -0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Boss screen lead-in" },
    { 286.0f, -5.2f,  1.2f, 54.0f, EnemyBehavior::Swoop,         EnemyEntryStyle::LeftSweep,  0.034f,  7,  7, 1.12f, "Boss screen" },
    { 296.0f,  5.2f,  1.2f, 54.0f, EnemyBehavior::Swoop,         EnemyEntryStyle::RightSweep, 0.034f,  7,  7, 1.12f, "Boss screen" },
    { 304.0f, -2.6f, -0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Boss approach pursuit" },
    { 304.0f,  0.0f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Boss approach pursuit" },
    { 316.0f,  2.6f,  1.2f, 48.0f, EnemyBehavior::DiveBomber, EnemyEntryStyle::RightSweep, 0.000f, 0, 7, 1.08f, "Last dive" },
    { 316.0f, -2.6f, -0.3f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::VFormation, 0.000f, 0, 5, 1.00f, "Last dive" },
    { 324.0f,  0.0f,  0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Boss escort" },
    { 324.0f,  2.6f, -0.5f, 44.0f, EnemyBehavior::Formation, EnemyEntryStyle::TightFormation, 0.000f, 0, 4, 1.00f, "Boss escort" }
};

template <size_t Count>
// 配置表が進行度の順に並び、敵の登場地点がボス登場より手前にあることを確認する
constexpr bool IsStageEnemyScheduleValid(const EnemySpawn (&events)[Count])
{
    for (size_t index = 0; index < Count; ++index) {
        const EnemySpawn& event = events[index];
        if (event.distance < 0.0f || event.distance >= kBossSpawnProgress ||
            event.maxHpOverride <= 0 || event.scaleMultiplier <= 0.0f ||
            (index > 0 && event.distance < events[index - 1].distance)) {
            return false;
        }
    }
    return Count > 0;
}
static_assert(IsStageEnemyScheduleValid(kEnemySpawns));

}

std::span<const Segment> Segments() { return kSegments; }
std::span<const RailEvent> RailEvents() { return kRailEvents; }
std::span<const EnemySpawn> EnemySpawns() { return kEnemySpawns; }
std::span<const GateSpawn> GateSpawns() { return kGateSpawns; }
}
