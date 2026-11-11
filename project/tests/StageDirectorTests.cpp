#include "app/stage/StageDirector.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void Require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}

void CheckOpeningAndGroupAdmission()
{
    StageDirector director;
    director.Reset();
    Require(director.EnemyCount() == 66 && director.HasPendingEnemies(), "Main stage enemy count or initial state changed");
    for (int frame = 0; frame < 34; ++frame) {
        director.Advance(1.0f, {}, false, false);
        Require(!director.TryConsumeEnemy({}), "Opening enemies appeared too early");
    }
    director.Advance(1.0f, {}, false, false);
    const auto* opening = director.TryConsumeEnemy({});
    Require(opening == &MainStage::EnemySpawns().front() && director.EmittedEnemyCount() == 1,
        "Opening did not reach its first event");
    Require(director.HandoffFrames() == 0.0f && !director.TryConsumeEnemy({ 1, 0 }),
        "Emitting an enemy failed to reset handoff or skipped a future event");
    Require(director.CanAdmitGroup(11, { 1, 0 }), "One light enemy blocked a four-ship group");
    Require(!director.CanAdmitGroup(11, { 2, 0 }), "Four-ship group was split into insufficient slots");
    Require(!director.CanAdmitGroup(11, { 0, 2 }), "Primary threats overlapped beyond the pressure limit");
    Require(director.CanAdmitGroup(0, { 1, 2 }), "A primary threat blocked light reinforcements");
    Require(director.CanAdmitGroup(28, {}) && !director.CanAdmitGroup(28, { 1, 1 }),
        "Crossfire pair did not reserve both pressure slots");
    Require(!director.CanAdmitGroup(999, {}), "Invalid event index was admitted");
}

void CheckBoundedProgressAndPartialGroup()
{
    StageDirector director;
    director.Reset();
    director.SeekForDebug(66.0f);
    for (int index = 0; index < 2; ++index) {
        Require(director.TryConsumeEnemy({}) != nullptr, "Lead-in fixture did not emit");
    }
    for (int frame = 0; frame < 300; ++frame) { director.Advance(1.0f, { 2, 0 }, false, false); }
    Require(director.Progress() < 78.0f && director.Progress() > 77.99f && !director.TryConsumeEnemy({ 2, 0 }),
        "Full encounter advanced past the next group boundary");
    for (int frame = 0; frame < 24; ++frame) { director.Advance(1.0f, { 1, 0 }, false, false); }
    StageDirector::EnemyLoad load{ 1, 0 };
    for (int index = 0; index < 4; ++index) {
        const auto* event = director.TryConsumeEnemy(load);
        Require(event == &MainStage::EnemySpawns()[11 + index], "Partially emitted group lost order or its reserved slots");
        ++load.count;
        load.pressure += StageDirector::EnemyPressure(event->behavior);
    }
    Require(load.count == 5 && load.pressure == 2 && !director.TryConsumeEnemy(load),
        "Encounter exceeded the count or pressure cap");

    director.Reset();
    director.SeekForDebug(16.0f);
    Require(director.TryConsumeEnemy({}) != nullptr, "Second opening event missing");
    for (int frame = 0; frame < 12; ++frame) { director.Advance(1.0f, {}, false, false); }
    Require(director.Progress() == 28.0f && director.TryConsumeEnemy({}) == &MainStage::EnemySpawns()[2],
        "Empty encounter failed to reach the next event within its deadline");
}

void CheckScheduleAndBossFreeze(bool fever)
{
    StageDirector director;
    director.Reset();
    const auto schedule = MainStage::EnemySpawns();
    size_t emitted = 0;
    size_t gates = 0;
    for (int frame = 0; frame < 2000 && !director.IsBossSpawnDue(); ++frame) {
        director.Advance(fever ? 0.55f : 1.0f, {}, fever, false);
        for (int count = 0; count < StageDirector::kMaxSpawnsPerFrame; ++count) {
            const auto* event = director.TryConsumeEnemy({});
            if (!event) { break; }
            Require(emitted < schedule.size() && event == &schedule[emitted], "Schedule repeated or reordered an enemy");
            ++emitted;
        }
        while (const auto* gate = director.TryConsumeGate()) {
            Require(gate == &MainStage::GateSpawns()[gates], "Gate schedule repeated or changed order");
            Require(std::abs(gate->x) >= 3.5f && gate->leadDistance >= 68.0f, "Gate did not offer a visible side route");
            ++gates;
        }
    }
    Require(emitted == 66 && gates == 6 && !director.HasPendingEnemies() && director.IsBossSpawnDue(),
        "Schedule skipped enemies or failed to approach boss");
    const float progress = director.Progress();
    for (int frame = 0; frame < 120; ++frame) { director.Advance(1.0f, {}, fever, true); }
    Require(director.Progress() == progress && director.TimelineSpeed() == 0.0f && !director.TryConsumeEnemy({}),
        "Boss combat advanced normal encounters");
    director.Reset();
    Require(director.Progress() == 0.0f && director.HandoffFrames() == 0.0f && director.EmptyFrames() == 0.0f &&
        director.EmittedEnemyCount() == 0 && !director.IsBossSpawnDue(), "Retry retained progress or emitted state");
}

void CheckSectionsAndRailEvents()
{
    StageDirector director;
    director.Reset();
    director.SeekForDebug(42.0f);
    Require(std::string_view(director.CurrentSegment().name) == "Side sweep" && director.WaveIndex() == 0,
        "First section boundary changed");
    director.SeekForDebug(104.0f);
    Require(director.WaveIndex() == 1 && std::string_view(director.CurrentSegment().name) == "Climb and dodge",
        "Middle section boundary changed");
    director.SeekForDebug(230.0f);
    Require(director.WaveIndex() == 2 && std::string_view(director.CurrentSegment().name) == "Final approach",
        "Final section boundary changed");
    director.SeekForDebug(112.0f);
    const auto* rail = director.TryConsumeRailEvent();
    Require(rail && rail->distance == 112.0f && !director.TryConsumeRailEvent(), "Rail cue repeated or lost its exact boundary");
    director.Reset();
    director.HoldForDebug(246.0f);
    int cues = 0;
    while (director.TryConsumeRailEvent()) { ++cues; }
    Require(cues == 5 && !director.TryConsumeRailEvent(), "Rail cues did not emit exactly once");
    director.SeekForDebug(MainStage::kBossSpawnProgress, true);
    Require(director.EmittedEnemyCount() == 66 && !director.TryConsumeEnemy({}), "Boss debug entry retained normal enemies");
}

void CheckTutorialIsolation()
{
    StageDirector director;
    director.Reset(true);
    for (int frame = 0; frame < 600; ++frame) { director.Advance(1.0f, {}, true, false); }
    Require(director.EnemyCount() == 0 && director.Progress() == 0.0f && !director.TryConsumeEnemy({}) &&
        !director.TryConsumeRailEvent() && !director.TryConsumeGate() && !director.IsBossSpawnDue(), "Main-stage progression leaked into tutorial");
}
}

int main()
{
    try {
        CheckOpeningAndGroupAdmission();
        CheckBoundedProgressAndPartialGroup();
        CheckScheduleAndBossFreeze(false);
        CheckScheduleAndBossFreeze(true);
        CheckSectionsAndRailEvents();
        CheckTutorialIsolation();
        std::cout << "STAGE_DIRECTOR_PASS enemies=66 course_gates=6 group_reservation=1 pressure_cap=2 "
            "bounded_progress=1 empty_deadline=12 ordered_once=1 boss_freeze=1 retry=1 tutorial=1 sections=1 rail_cues=5\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
