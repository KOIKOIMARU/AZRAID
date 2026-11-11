// ===== StageDirector.cpp =====
// ステージの進行度と出現中の敵の状況に応じて、敵・リング・レール演出を順に出す。

#include "app/stage/StageDirector.h"

#include <algorithm>
#include <string_view>

namespace {
constexpr float kCruiseSpeed = 0.090f;
constexpr float kOpeningFrames = 36.0f;
constexpr float kHandoffFrames = 24.0f;
constexpr float kFeverHandoffFrames = 24.0f;
constexpr float kEmptyEncounterMaxFrames = 12.0f;
constexpr float kEmptyBossApproachMaxFrames = 36.0f;
constexpr float kGroupingDistance = 10.5f;

// 先頭の敵から、同じ場面名で近い進行度に並ぶ敵をひとつの編隊として数える。
size_t GroupEnd(std::span<const MainStage::EnemySpawn> events, size_t first)
{
    size_t end = first + 1;
    while (end < events.size() &&
        std::string_view(events[end].beatName) == std::string_view(events[first].beatName) &&
        events[end].distance - events[first].distance <= kGroupingDistance) {
        ++end;
    }
    return end;
}
}

// 本編の配置表に対応する消費済みの記録と進行度を初期化する。練習では本編イベントの記録を作らない。
void StageDirector::Reset(bool tutorial)
{
    tutorial_ = tutorial;
    progress_ = timelineSpeed_ = handoffFrames_ = emptyFrames_ = 0.0f;
    enemyTriggered_.assign(tutorial ? 0 : MainStage::EnemySpawns().size(), false);
    railTriggered_.assign(tutorial ? 0 : MainStage::RailEvents().size(), false);
    gateTriggered_.assign(tutorial ? 0 : MainStage::GateSpawns().size(), false);
}

// 敵の機種を同時出現判定で使う重みに置き換える。狙撃など対処を要する機種には大きい値を使う。
int StageDirector::EnemyPressure(EnemyBehavior behavior)
{
    switch (behavior) {
    case EnemyBehavior::Sniper:
    case EnemyBehavior::Shield:
    case EnemyBehavior::Support:
        return 2; // 明確に対処する主役は一機に絞る。
    case EnemyBehavior::Crossfire:
    case EnemyBehavior::StrafeShooter:
        return 1; // 左右の一組は一緒に出す。
    default:
        return 0;
    }
}

// 出したい編隊の残り全機を足し、同時出現数と攻撃の負荷が両方とも上限内か確認する。
bool StageDirector::CanAdmitGroup(size_t eventIndex, EnemyLoad load) const
{
    const auto events = MainStage::EnemySpawns();
    if (eventIndex >= events.size() || eventIndex >= enemyTriggered_.size()) { return false; }
    // 編隊の残り全機を予約する。狙撃編隊や左右挟撃を空き一枠だけで分断しない。
    const size_t end = GroupEnd(events, eventIndex);
    for (size_t index = eventIndex; index < end; ++index) {
        if (!enemyTriggered_[index]) {
            ++load.count;
            load.pressure += EnemyPressure(events[index].behavior);
        }
    }
    return load.count <= kMaxActiveEnemies && load.pressure <= kMaxEnemyPressure;
}

// 敵の残り数を見て、次の配置イベントまで進める。枠が足りない場合は次の編隊の手前で待つ。
void StageDirector::Advance(float frameStep, EnemyLoad load, bool feverActive, bool bossSpawned)
{
    if (tutorial_) { return; }
    const auto events = MainStage::EnemySpawns();
    const size_t eventCount = (std::min)(enemyTriggered_.size(), events.size());
    size_t nextIndex = eventCount;
    for (size_t index = 0; index < eventCount; ++index) {
        if (!enemyTriggered_[index]) { nextIndex = index; break; }
    }
    const bool hasNext = nextIndex < eventCount;
    const bool canAdmit = hasNext ? CanAdmitGroup(nextIndex, load) : load.count == 0;
    const float destination = hasNext ? events[nextIndex].distance : MainStage::kBossSpawnProgress;
    if (load.count > 0 || bossSpawned) { emptyFrames_ = 0.0f; }
    else { emptyFrames_ += frameStep; }
    if (!bossSpawned && canAdmit && load.count <= 1) { handoffFrames_ += frameStep; }
    else { handoffFrames_ = 0.0f; }

    if (bossSpawned) { timelineSpeed_ = 0.0f; }
    else {
        // 見た目の飛行速度とは分ける。空き枠がある時だけ、次の出現までの待ちを短くする。
        timelineSpeed_ = kCruiseSpeed * (feverActive ? 1.15f : 1.0f);
        if (canAdmit && load.count <= 1) {
            const float deadline = !hasNext ? kEmptyBossApproachMaxFrames :
                nextIndex == 0 ? kOpeningFrames : load.count == 0 ? kEmptyEncounterMaxFrames :
                feverActive ? kFeverHandoffFrames : kHandoffFrames;
            const float remaining = (std::max)(1.0f, deadline - handoffFrames_);
            timelineSpeed_ = (std::max)(timelineSpeed_, (std::max)(0.0f, destination - progress_) / remaining);
        }
    }
    float nextProgress = progress_ + timelineSpeed_ * frameStep;
    if (!bossSpawned) {
        // 入場を待つ編隊の先へ飛ばず、空きができるまで直前にとどめる。
        const float limit = canAdmit ? destination : destination - 0.001f;
        nextProgress = (std::min)(nextProgress, (std::max)(progress_, limit));
    }
    progress_ = nextProgress;
}

// 次の出現位置に到達し、編隊を出せるときだけ配置表の1件を消費する。
const MainStage::EnemySpawn* StageDirector::TryConsumeEnemy(EnemyLoad load)
{
    const auto events = MainStage::EnemySpawns();
    for (size_t index = 0; index < enemyTriggered_.size(); ++index) {
        if (enemyTriggered_[index]) { continue; }
        if (progress_ < events[index].distance || !CanAdmitGroup(index, load)) { return nullptr; }
        enemyTriggered_[index] = true;
        handoffFrames_ = 0.0f;
        return &events[index];
    }
    return nullptr;
}

// 進行度で発生する画面演出を、各イベントにつき一度だけ取り出す。
const MainStage::RailEvent* StageDirector::TryConsumeRailEvent()
{
    const auto events = MainStage::RailEvents();
    for (size_t index = 0; index < railTriggered_.size(); ++index) {
        if (railTriggered_[index] || progress_ < events[index].distance) { continue; }
        railTriggered_[index] = true;
        return &events[index];
    }
    return nullptr;
}

// 進行度に届いた未使用のリングを1件返す。該当がなければnullptr。
const MainStage::GateSpawn* StageDirector::TryConsumeGate()
{
    const auto events = MainStage::GateSpawns();
    for (size_t index = 0; index < gateTriggered_.size(); ++index) {
        if (gateTriggered_[index] || progress_ < events[index].distance) { continue; }
        gateTriggered_[index] = true;
        return &events[index];
    }
    return nullptr;
}

// 現在の進行度が入る区間を返し、その区間の速度やカメラ設定を取得できるようにする。
const MainStage::Segment& StageDirector::CurrentSegment() const
{
    const auto segments = MainStage::Segments();
    for (const auto& segment : segments) {
        if (progress_ >= segment.startDistance && progress_ < segment.endDistance) { return segment; }
    }
    return segments.front();
}

int StageDirector::WaveIndex() const
{
    return progress_ < 104.0f ? 0 : progress_ < 230.0f ? 1 : 2;
}

int StageDirector::EmittedEnemyCount() const
{
    return static_cast<int>(std::count(enemyTriggered_.begin(), enemyTriggered_.end(), true));
}

// 配置表に未出現の通常敵が残っているか返す。
bool StageDirector::HasPendingEnemies() const
{
    return std::any_of(enemyTriggered_.begin(), enemyTriggered_.end(), [](bool emitted) { return !emitted; });
}

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
// 開発用に進行度を指定し、その地点より前に済んでいる配置イベントを反映する。
int StageDirector::SeekForDebug(float progress, bool skipAllEnemies)
{
    HoldForDebug(progress);
    const auto enemies = MainStage::EnemySpawns();
    for (size_t index = 0; index < enemyTriggered_.size(); ++index) {
        enemyTriggered_[index] = skipAllEnemies || enemies[index].distance < progress;
    }
    const auto rail = MainStage::RailEvents();
    for (size_t index = 0; index < railTriggered_.size(); ++index) { railTriggered_[index] = rail[index].distance < progress; }
    const auto gates = MainStage::GateSpawns();
    for (size_t index = 0; index < gateTriggered_.size(); ++index) { gateTriggered_[index] = gates[index].distance < progress; }
    return EmittedEnemyCount();
}

void StageDirector::DisableSpawnsForDebug()
{
    std::fill(enemyTriggered_.begin(), enemyTriggered_.end(), true);
    std::fill(gateTriggered_.begin(), gateTriggered_.end(), true);
}

void StageDirector::HoldForDebug(float progress)
{
    progress_ = progress;
    timelineSpeed_ = handoffFrames_ = emptyFrames_ = 0.0f;
}
#endif
