#include "app/enemy/BossAttackController.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Boss = BossAttackController;
void Require(bool ok, const char* message) { if (!ok) { throw std::runtime_error(message); } }
Boss::Input Ready(int phase = 1) { return { true, true, phase == 1 ? 220 : 110, 220, { 0, 1, 0 } }; }

void CheckPatterns(int phase)
{
    for (int p = 0; p < static_cast<int>(Boss::Pattern::Count); ++p) {
        Boss boss;
        const auto pattern = static_cast<Boss::Pattern>(p);
        boss.PrepareForDebug(phase, pattern);
        int shotCount = 0;
        bool recovered = false;
        float lockedAim = 0.0f;
        for (int frame = 0; frame < 200; ++frame) {
            auto input = Ready(phase);
            input.playerPosition.x = static_cast<float>(frame);
            const auto events = boss.Update(input);
            Require(events.shotCount <= Boss::kMaxShotsPerUpdate, "Shot request array overflow");
            if (frame == boss.WindupFrames() - Boss::kAimLockFrames) { lockedAim = boss.AimPoint().x; }
            for (int index = 0; index < events.shotCount; ++index) {
                Require(frame > boss.WindupFrames(), "Fired before windup completed");
                Require(boss.AimPoint().x == lockedAim, "Burst tracked after aim lock");
                const auto& shot = events.shots[index];
                Require(std::isfinite(shot.targetOffset.x) && std::abs(shot.targetOffset.x) <= 6.0f,
                    "Unbounded target offset");
                Require(shot.charged == (pattern == Boss::Pattern::Charge && index == 0), "Wrong cannon type");
                ++shotCount;
            }
            if (events.recoveryStarted) {
                recovered = true;
                Require(boss.CounterDuration() >= 84 && boss.CounterDuration() <= 120, "No usable counter opening");
                Require(boss.CooldownFrames() == boss.CounterDuration() + 12, "Counter overlaps next attack");
                break;
            }
        }
        const int expected = pattern == Boss::Pattern::Fan ? (phase == 1 ? 5 : 7) :
            pattern == Boss::Pattern::Charge ? (phase == 1 ? 1 : 3) :
            pattern == Boss::Pattern::Crossfire ? (phase == 1 ? 6 : 9) : (phase == 1 ? 7 : 9);
        Require(recovered && shotCount == expected, "Pattern did not finish all volleys");
        for (int frame = 0; frame < boss.CounterDuration(); ++frame) {
            Require(boss.Update(Ready(phase)).shotCount == 0, "Fired during counter opening");
        }
    }
}

void CheckSequenceAndMotion(int phase)
{
    Boss boss;
    boss.Reset();
    std::vector<Boss::Pattern> patterns;
    float minX = 100.0f, maxX = -100.0f, minZ = 100.0f;
    for (int frame = 0; frame < 2000 && patterns.size() < 6; ++frame) {
        const auto events = boss.Update(Ready(phase));
        if (events.attackStarted) { patterns.push_back(boss.CurrentPattern()); }
        const auto offset = boss.FlightOffset();
        Require(std::isfinite(offset.x) && std::isfinite(offset.y) && std::isfinite(offset.z), "Invalid flight pose");
        Require(std::abs(offset.x) <= 7.0f && offset.y >= -1.0f && offset.y <= 3.0f && offset.z >= -14.1f,
            "Flight leaves readable combat area");
        if (boss.IsAttacking()) {
            minX = std::min(minX, offset.x); maxX = std::max(maxX, offset.x); minZ = std::min(minZ, offset.z);
        }
    }
    Require(patterns.size() == 6 && patterns.front() == patterns.back(), "Attack sequence does not loop");
    auto firstCycle = std::vector<Boss::Pattern>(patterns.begin(), patterns.begin() + 5);
    std::sort(firstCycle.begin(), firstCycle.end());
    Require(std::adjacent_find(firstCycle.begin(), firstCycle.end()) == firstCycle.end(), "A pattern is missing from cycle");
    Require(maxX - minX > 11.0f && minZ < -12.0f, "Boss does not traverse and approach while attacking");
}

void CheckGatesAndCancellation()
{
    Boss boss;
    auto input = Ready();
    input.canAttack = false;
    for (int frame = 0; frame < 100; ++frame) { Require(!boss.Update(input).attackStarted, "Attacked during entry"); }
    input.canAttack = true;
    Require(boss.Update(input).attackStarted, "Did not start after entry");
    const int remaining = boss.RemainingFrames();
    input.canAttack = false;
    for (int frame = 0; frame < 20; ++frame) { boss.Update(input); }
    Require(boss.RemainingFrames() == remaining, "Blocked windup advanced");
    input.hp = 111;
    Require(!boss.Update(input).phaseChanged, "Enraged above half HP");
    input.hp = 110;
    Require(boss.Update(input).phaseChanged && boss.Phase() == 2 && !boss.IsAttacking() && !boss.IsCounterOpen(),
        "Half-HP transition did not interrupt attack");
    for (int frame = 0; frame < 36; ++frame) { Require(!boss.Update(input).phaseChanged, "Phase transition repeated"); }
    input.canAttack = true;
    Require(boss.Update(input).attackStarted, "Did not resume after transition");
    boss.CancelAttack();
    Require(!boss.IsAttacking() && !boss.IsCounterOpen() && boss.RemainingFrames() == 0, "Cancelled attack remained live");
    input.present = false;
    for (int frame = 0; frame < 200; ++frame) { Require(boss.Update(input).shotCount == 0, "Missing boss emitted shot"); }
    boss.Reset();
    Require(boss.Phase() == 1 && boss.AttackCount() == 0 && boss.CounterDuration() == 1 &&
        boss.CooldownFrames() == 0 && boss.AimPoint().x == 0, "Retry did not reset state");
}
}

int main()
{
    try {
        CheckPatterns(1); CheckPatterns(2); CheckSequenceAndMotion(1); CheckSequenceAndMotion(2); CheckGatesAndCancellation();
        std::cout << "BOSS_CONTROLLER_PASS phases=2 patterns=5 fixed_aim=1 bounded_shots=1 flight_traverse=1 approach=1 counter=1 cancellation=1 retry=1\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
