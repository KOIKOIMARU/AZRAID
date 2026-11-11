#include "app/player/PlayerShooting.h"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void Require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}

void CheckHeldFire()
{
    PlayerShooting shooting;
    std::vector<int> shots;
    for (int frame = 0; frame <= 42; ++frame) {
        if (shooting.Update({ true, frame == 0 }, false)) {
            shots.push_back(frame);
            shooting.OnShotFired(false);
        }
    }
    Require(shots == std::vector<int>{ 0, 14, 28, 42 }, "Held fire changed the 14-frame interval");
    Require(shooting.ChargeFrames() == 0, "Held fire accumulated charge");
}

void CheckCharge()
{
    PlayerShooting shooting;
    for (int frame = 0; frame < 65; ++frame) {
        Require(!shooting.Update({}, false), "Waiting fired a shot");
    }
    Require(!shooting.IsChargeReady(), "Charge became ready before 66 frames");
    Require(!shooting.Update({}, false) && shooting.IsChargeReady(), "Charge missed the 66-frame boundary");
    Require(shooting.ChargeRate() == 1.0f && shooting.Update({ true, true }, false), "Charged shot was not requested");
    Require(shooting.CooldownFrames() == 20 && shooting.ChargeFrames() == 66,
        "Requesting a shot consumed charge before a bullet existed");
    shooting.OnShotFired(false);
    Require(shooting.ChargeFrames() == 0 && !shooting.IsChargeReady(), "Successful shot did not consume charge");
    for (int frame = 0; frame < 150; ++frame) { shooting.Update({}, false); }
    Require(shooting.ChargeFrames() == PlayerShooting::kMaxChargeFrames, "Waiting exceeded the charge cap");
}

void CheckBufferedPress()
{
    PlayerShooting shooting;
    Require(shooting.Update({ true, true }, false), "Initial normal shot failed");
    shooting.OnShotFired(false);
    for (int frame = 1; frame <= 9; ++frame) { Require(!shooting.Update({}, false), "Released input repeated a shot"); }
    Require(!shooting.Update({ true, true }, false), "Buffered press ignored cooldown");
    Require(shooting.BufferedFrames() == 8, "Press did not enter the input buffer");
    for (int frame = 11; frame < 14; ++frame) { Require(!shooting.Update({}, false), "Buffered shot fired too early"); }
    Require(shooting.Update({}, false), "Press released before cooldown ended was lost");
    shooting.OnShotFired(false);
    Require(shooting.BufferedFrames() == 0, "Successful buffered shot retained its input");
    for (int frame = 0; frame < 30; ++frame) { Require(!shooting.Update({}, false), "Buffered press fired twice"); }

    shooting.SetTuning({ 66, 30, 20 });
    shooting.Reset();
    Require(shooting.Update({ true, true }, false), "Long-interval fixture could not fire");
    shooting.OnShotFired(false);
    Require(!shooting.Update({ true, true }, false), "Long cooldown accepted an immediate repeat");
    for (int frame = 0; frame < 35; ++frame) { Require(!shooting.Update({}, false), "Expired input fired later"); }
    Require(shooting.BufferedFrames() == 0 && shooting.ChargeFrames() > 0, "Expired buffer blocked charging");
}

void CheckFeverAndRefill()
{
    PlayerShooting shooting;
    shooting.FillCharge();
    std::vector<int> shots;
    for (int frame = 0; frame <= 27; ++frame) {
        if (shooting.Update({ true, frame == 0 }, true)) {
            shots.push_back(frame);
            shooting.OnShotFired(true);
        }
        Require(shooting.ChargeFrames() == 100, "Fever fire consumed its replenished charge");
    }
    Require(shots == std::vector<int>{ 0, 9, 18, 27 }, "Fever changed the 9-frame interval");
    for (int frame = 0; frame < 8; ++frame) { Require(!shooting.Update({ true, false }, false), "Fever exit fired before cooldown"); }
    Require(shooting.Update({ true, false }, false) && shooting.IsChargeReady() && shooting.CooldownFrames() == 20,
        "Fever exit lost the charged follow-up or retained rapid fire");
    shooting.OnShotFired(false);
    shooting.AddCharge(24);
    Require(shooting.ChargeFrames() == 24, "Dodge replenishment changed");
    shooting.AddCharge((std::numeric_limits<int>::max)());
    Require(shooting.ChargeFrames() == 100, "Large replenishment overflowed charge");
    shooting.CapCooldown(4);
    for (int frame = 0; frame < 3; ++frame) { Require(!shooting.Update({ true, false }, false), "Ring follow-up fired too early"); }
    Require(shooting.Update({ true, false }, false) && shooting.IsChargeReady(), "Ring replenishment lost its 4-frame follow-up");
}

void CheckAllocationFailureAndReset()
{
    PlayerShooting shooting;
    shooting.FillCharge();
    Require(shooting.Update({ true, true }, false), "Charged fixture could not request fire");
    // 確保失敗としてOnShotFiredを呼ばない。連射間隔を守り、蓄積済みチャージは残す。
    for (int frame = 0; frame < 19; ++frame) { Require(!shooting.Update({ true, false }, false), "Failed allocation caused rapid retry"); }
    Require(shooting.Update({ true, false }, false) && shooting.ChargeFrames() == 100,
        "Failed allocation consumed charge or lost the next attempt");
    shooting.SetTuning({ 72, 16, 22 });
    Require(!shooting.Update({ true, true }, false), "Reset fixture failed to buffer input");
    shooting.Reset();
    Require(shooting.ChargeFrames() == 0 && shooting.CooldownFrames() == 0 && shooting.BufferedFrames() == 0,
        "Reset retained weapon state");
    Require(shooting.GetTuning().chargeFrames == 72 && shooting.GetTuning().normalIntervalFrames == 16 &&
        shooting.GetTuning().chargedIntervalFrames == 22, "Retry reset the tuning");
    shooting.CapCooldown(4);
    Require(shooting.CooldownFrames() == 0, "Cooldown cap introduced a new wait");
    shooting.SetTuning({ 0, 0, -1 });
    Require(shooting.GetTuning().chargeFrames == 1 && shooting.GetTuning().normalIntervalFrames == 1 &&
        shooting.GetTuning().chargedIntervalFrames == 1 && shooting.ChargeRate() == 0.0f, "Invalid tuning was accepted");
}
}

int main()
{
    try {
        CheckHeldFire();
        CheckCharge();
        CheckBufferedPress();
        CheckFeverAndRefill();
        CheckAllocationFailureAndReset();
        std::cout << "PLAYER_SHOOTING_PASS normal_interval=14 charge_threshold=66 charged_interval=20 fever_interval=9 "
            "buffer_release=1 buffer_expiry=1 refill=1 allocation_failure=1 reset_preserves_tuning=1\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
