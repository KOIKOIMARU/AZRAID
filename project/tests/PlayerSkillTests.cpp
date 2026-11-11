#include "app/player/PlayerSkill.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

// 選択処理は敵を参照・操作しない。識別用のオブジェクトだけで検証できる。
class Enemy {};
namespace {
void Require(bool ok, const char* message) { if (!ok) { throw std::runtime_error(message); } }
PlayerSkill::Selection Select(Enemy* enemies, int count, bool empowered)
{
    PlayerSkill::Selection result(empowered);
    for (int index = 0; index < count; ++index) {
        result.Consider(&enemies[index], { static_cast<float>(index), 1, 32 }, static_cast<float>(count - index), false);
    }
    return result;
}

void CheckSelection()
{
    Enemy enemies[8];
    PlayerSkill skill;
    Require(!skill.TryActivate(PlayerSkill::Selection(false)) && skill.IsReady() &&
        skill.CooldownFrames() == 0 && skill.NoTargetNoticeFrames() == 60, "Empty selection consumed the skill");
    Require(skill.TryActivate(Select(enemies, 8, false)) && skill.TargetCount() == 3, "Normal capacity changed");
    Require(skill.Targets()[0].enemy == &enemies[7] && skill.Targets()[2].enemy == &enemies[5], "Nearest targets were not selected");
    Require(!skill.TryActivate(Select(enemies, 8, true)), "Active sequence reactivated");
    skill.Reset();
    Require(skill.TryActivate(Select(enemies, 8, true)) && skill.TargetCount() == 5, "Fever capacity changed");
    skill.Reset();
    PlayerSkill::Selection tied(false);
    tied.Consider(&enemies[0], {}, 4, false); tied.Consider(&enemies[1], {}, 4, false);
    tied.Consider(&enemies[0], {}, 4, false); tied.Consider(&enemies[2], {}, 4, false);
    tied.Consider(&enemies[3], {}, std::numeric_limits<float>::quiet_NaN(), false);
    Require(skill.TryActivate(tied) && skill.TargetCount() == 3 && skill.Targets()[1].enemy == &enemies[1], "Tie order/uniqueness changed");
    skill.Reset();
    PlayerSkill::Selection boss(true);
    boss.Consider(&enemies[0], {}, 10, false); boss.Consider(&enemies[1], {}, 2, true);
    boss.Consider(&enemies[2], {}, 1, false); boss.Consider(&enemies[3], {}, 3, false);
    Require(skill.TryActivate(boss) && skill.TargetCount() == 2 && skill.Targets()[1].enemy == &enemies[1], "Selection continued beyond the boss");
}

void CheckTimeline(bool empowered, float step)
{
    Enemy enemies[5];
    PlayerSkill skill;
    Require(skill.TryActivate(Select(enemies, empowered ? 5 : 3, empowered)), "Activation failed");
    std::vector<int> strikes;
    int finishes = 0;
    float elapsed = 0;
    const std::vector<int> expected = empowered ? std::vector<int>{12,19,26,33,40} : std::vector<int>{12,19,26};
    const float finishTime = empowered ? 47.0f : 33.0f;
    while (skill.IsActive()) {
        skill.TickTimers(step); skill.AdvanceSequence(step); elapsed += step;
        const float slow = elapsed < 12 ? 0.55f : elapsed < finishTime ? 0.75f : elapsed < finishTime + 5 ? 0.08f : 1.0f;
        Require(skill.WorldTimeScale() == slow, "World slow timing changed");
        Require(skill.ProtectsPlayer() == (elapsed < finishTime + 7), "Protection timing changed");
        for (int index = skill.ConsumeStrike(); index >= 0; index = skill.ConsumeStrike()) {
            Require(elapsed >= expected[index] && elapsed - step < expected[index], "Strike crossed the wrong deadline");
            strikes.push_back(index);
            skill.MarkStrike(index, { static_cast<float>(index), 2, 40 });
        }
        if (skill.ConsumeFinisher()) {
            Require(elapsed >= finishTime && elapsed - step < finishTime, "Finisher deadline changed");
            ++finishes;
        }
        Require(!skill.ConsumeFinisher(), "Finisher could be consumed twice");
        skill.EndSequenceIfElapsed();
        Require(elapsed < 80, "Sequence never ended");
    }
    Require(strikes.size() == expected.size() && finishes == 1 && skill.TargetCount() == 0, "Incomplete sequence or retained references");
    Require(skill.IsEmpowered() == empowered && skill.FinisherDamage() == (empowered ? 4 : 3), "Activation snapshot changed");
}

void CheckSelectionOrder()
{
    Enemy enemies[64];
    for (bool empowered : {false,true}) {
        for (int sample = 0; sample < 64; ++sample) {
            PlayerSkill skill;
            PlayerSkill::Selection selection(empowered);
            std::vector<int> order;
            const auto distance = [&](int index) { return (index * 37 + sample * 19) % 17; };
            for (int index = 0; index < 64; ++index) {
                order.push_back(index);
                selection.Consider(&enemies[index], {}, static_cast<float>(distance(index)), index == sample);
            }
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return distance(a) < distance(b); });
            std::vector<int> expected;
            for (int index : order) {
                expected.push_back(index);
                if (index == sample || expected.size() == (empowered ? 5u : 3u)) { break; }
            }
            Require(skill.TryActivate(selection) && skill.Targets().size() == expected.size(), "Sorted selection count changed");
            for (size_t index = 0; index < expected.size(); ++index) {
                Require(skill.Targets()[index].enemy == &enemies[expected[index]], "Bounded selection lost stable distance order");
            }
        }
    }
}

void CheckRecoveryAndLifetime()
{
    Enemy enemies[3];
    PlayerSkill skill;
    Require(skill.TryActivate(Select(enemies, 3, false)) && skill.CooldownFrames() == 1080, "Full cooldown changed");
    skill.RecoverOnHit(true, true);
    Require(skill.CooldownFrames() == 1080 && !skill.TryRecover(), "Active skill recharged itself");
    const Enemy* removed = skill.Targets()[0].enemy;
    const auto oldPosition = skill.Targets()[0].position;
    skill.InvalidateTarget(removed);
    skill.UpdateTargetPosition(removed, { 100, 100, 100 });
    Require(!skill.Targets()[0].enemy && skill.Targets()[0].position.x == oldPosition.x, "Invalidated target was updated");
    skill.CancelSequence();
    Require(!skill.IsActive() && skill.Targets().empty(), "Cancellation retained targets");
    skill.RecoverOnHit(false, false); Require(skill.CooldownFrames() == 1078.5f, "Normal hit recovery changed");
    skill.RecoverOnHit(true, true); Require(skill.CooldownFrames() == 1069.5f, "Charged kill recovery changed");
    for (int index = 0; index < 267; ++index) { skill.TickTimers(4); }
    Require(!skill.TryRecover() && skill.CooldownFrames() == 1.5f, "Recovered before the cooldown boundary");
    skill.RecoverOnHit(false, false);
    Require(skill.TryRecover() && skill.IsReady() && skill.ReadyFlashFrames() == 72 && !skill.TryRecover(), "Recovery cue repeated or failed");
    skill.RecordDefeat(); skill.MakeReadyForPractice();
    Require(skill.ActivationCount() == 1 && skill.DefeatCount() == 1, "Practice setup reset statistics");
    skill.Reset();
    Require(skill.IsReady() && !skill.IsActive() && skill.ActivationCount() == 0 && skill.DefeatCount() == 0 &&
        skill.CooldownFrames() == 0 && skill.NextStrike() == 0, "Retry did not reset all skill state");
}
}

int main()
{
    try {
        CheckSelection(); CheckSelectionOrder(); CheckRecoveryAndLifetime();
        for (bool empowered : {false,true}) { for (float step : {0.5f,1.0f,4.0f}) { CheckTimeline(empowered,step); } }
        std::cout << "PLAYER_SKILL_PASS selection=nearest_stable boss_stop=1 capacities=3,5 timing=12,19,26,33,40 finisher=33,47 cooldown=1080 no_self_recovery=1 invalidation=1 retry=1 steps=0.5,1,4\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
