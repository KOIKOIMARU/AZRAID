#include "app/presentation/GameplayCamera.h"
#include "app/player/JustDodgeFeedback.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool ok, const char* message)
{
    if (!ok) { throw std::runtime_error(message); }
}
bool Near(float a, float b) { return std::abs(a - b) < 0.0001f; }
void CheckFinite(const GameplayCamera::Pose& pose)
{
    Require(std::isfinite(pose.position.x + pose.position.y + pose.position.z +
        pose.rotation.x + pose.rotation.y + pose.rotation.z + pose.fov), "Camera produced a non-finite pose");
    Require(pose.fov > 0.1f && pose.fov < 1.5f, "Camera field of view escaped its usable range");
}
}

int main()
{
    try {
        using Rig = GameplayCamera;
        Rig camera;
        camera.Reset(0.528f);
        const Rig::Pose launch{ { 0, 1.8f, -13.8f }, { 0.06f, 0, 0 }, 0.8f };
        camera.AdoptPose(launch);
        camera.SynchronizePlayer({}, 120.0f);
        camera.SetCinematicMotion(0.528f, 0.2f, true);
        Require(Near(camera.Clock(), 120.0f) && Near(camera.FollowPose().position.z, -13.8f), "Cinematic handoff discarded its pose or clock");
        Require(Near(camera.MotionState().translationVelocity.z, 0.528f), "Cinematic handoff lost forward velocity");
        const auto unchanged = camera.DisplayPose();
        for (int read = 0; read < 100; ++read) { (void)camera.MotionState(); (void)camera.FollowPose(); (void)camera.DisplayPose(); }
        Require(camera.DisplayPose().position.z == unchanged.position.z && camera.Clock() == 120.0f,
            "Reading camera state advanced it during pause");
        camera.AddShake(0.2f, 18);
        camera.AddShake(0.0f, 100);
        Require(camera.ShakeFrames() == 18, "Zero-power shake extended a camera effect");
        Rig::Frame frame{};
        frame.railSpeed = 0.528f;
        frame.speedRate = 0.38f;
        camera.Update(frame, Rig::Mode::Flight);
        Require(camera.ShakeFrames() == 17, "Camera shake did not advance once per update");
        camera.ClearShake();
        Require(camera.ShakeFrames() == 0, "Camera shake did not clear");

        for (const auto mode : { Rig::Mode::Flight, Rig::Mode::Tutorial }) {
            for (const float delta : { 1.0f / 120.0f, 1.0f / 60.0f, 1.0f / 20.0f, 0.0f, 0.2f }) {
                camera.Reset(frame.railSpeed);
                camera.AdoptPose(launch);
                camera.SynchronizePlayer({}, 0.0f);
                for (int index = 0; index < 1200; ++index) {
                    const float phase = static_cast<float>(index) * 0.031f;
                    frame.deltaSeconds = delta;
                    frame.railDistance += frame.railSpeed * delta * 60.0f;
                    frame.playerPosition = { std::sin(phase) * 8.0f, std::cos(phase * 0.7f) * 3.0f, frame.railDistance };
                    frame.dodging = index % 120 < 12;
                    frame.dodgeDirection = index % 240 < 120 ? -1 : 1;
                    frame.fever = index % 300 < 100 ? 1.0f : 0.0f;
                    frame.worldScale = index % 90 < 8 ? 0.08f : 1.0f;
                    frame.justDodge = index % 120 < 8 ? 0.8f : 0.0f;
                    frame.impact = index % 60 < 5 ? 0.9f : 0.0f;
                    frame.activationKick = index % 300 < 15 ? 0.7f : 0.0f;
                    frame.releaseKick = index % 90 < 15 ? 0.8f : 0.0f;
                    if (index % 60 == 0) { camera.AddShake(0.22f, 18); }
                    camera.Update(frame, mode);
                    CheckFinite(camera.DisplayPose());
                    Require(camera.MotionState().blurStrength >= 0.0f && camera.MotionState().blurStrength <= 0.851f,
                        "Camera blur escaped its limit");
                    Require(camera.BoostKick(frame.activationKick, frame.releaseKick) >= 0.0f &&
                        camera.BoostKick(frame.activationKick, frame.releaseKick) <= 1.0f, "Camera boost escaped its limit");
                }
            }
        }
        // 停止中の微振動は追従の基準へ蓄積せず、揺れが終われば表示位置へ残らない
        camera.Reset(0.528f);
        camera.AdoptPose(launch);
        camera.SynchronizePlayer({}, 0.0f);
        frame = {};
        frame.railSpeed = 0.528f;
        frame.gameClear = true;
        camera.AddShake(0.4f, 20);
        for (int index = 0; index < 240; ++index) { camera.Update(frame, Rig::Mode::Flight); }
        Require(Near(camera.FollowPose().position.x, camera.DisplayPose().position.x) &&
            Near(camera.FollowPose().position.y, camera.DisplayPose().position.y), "Impact shake accumulated into the follow state");
        camera.Reset(0.4f);
        Require(camera.Clock() == 0.0f && camera.ShakeFrames() == 0 && camera.MotionState().blurStrength == 0.0f &&
            camera.MotionState().acceleration == 0.0f && Near(camera.MotionState().previousSpeed, 0.4f), "Retry retained camera motion");
        for (const auto mode : { Rig::Mode::Flight, Rig::Mode::Tutorial }) {
            Rig cruising, boosted;
            cruising.Reset(0.528f); boosted.Reset(0.528f);
            frame = {}; frame.railSpeed = 0.528f; frame.speedRate = 0.38f;
            for (int index = 0; index < 150; ++index) {
                frame.railDistance += frame.railSpeed;
                frame.playerPosition.z = frame.railDistance;
                cruising.Update(frame, mode); boosted.Update(frame, mode);
            }
            for (int index = 0; index < 24; ++index) {
                frame.railDistance += frame.railSpeed; frame.playerPosition.z = frame.railDistance;
                cruising.Update(frame, mode);
                frame.gateBoost = 1.0f; boosted.Update(frame, mode); frame.gateBoost = 0.0f;
            }
            Require(boosted.DisplayPose().fov > cruising.DisplayPose().fov + 0.06f &&
                boosted.DisplayPose().position.z < cruising.DisplayPose().position.z - 0.6f,
                "Gate did not widen and pull back the camera");
            for (int index = 0; index < 180; ++index) {
                frame.railDistance += frame.railSpeed; frame.playerPosition.z = frame.railDistance;
                cruising.Update(frame, mode); boosted.Update(frame, mode);
            }
            Require(std::abs(boosted.DisplayPose().fov - cruising.DisplayPose().fov) < 0.002f &&
                std::abs(boosted.DisplayPose().position.z - cruising.DisplayPose().position.z) < 0.03f,
                "Gate left a permanent camera offset after release");
        }
        for (const auto mode : { Rig::Mode::Flight, Rig::Mode::Tutorial }) {
            Rig normal, dodged;
            normal.Reset(0.528f); dodged.Reset(0.528f);
            frame = {}; frame.railSpeed = 0.528f; frame.speedRate = 0.38f;
            for (int index = 0; index < 180; ++index) {
                frame.railDistance += frame.railSpeed; frame.playerPosition.z = frame.railDistance;
                normal.Update(frame, mode); dodged.Update(frame, mode);
            }
            float closestFov = 0.0f, widestFov = 0.0f;
            float previousScale = 0.0f;
            for (int index = 0; index < JustDodgeFeedback::kDuration; ++index) {
                frame.railDistance += frame.railSpeed; frame.playerPosition.z = frame.railDistance;
                normal.Update(frame, mode);
                frame.justDodge = 1.0f - static_cast<float>(index) / JustDodgeFeedback::kDuration;
                dodged.Update(frame, mode); frame.justDodge = 0.0f;
                const float difference = dodged.DisplayPose().fov - normal.DisplayPose().fov;
                closestFov = (std::min)(closestFov, difference);
                widestFov = (std::max)(widestFov, difference);
                const float scale = JustDodgeFeedback::WorldTimeScale(JustDodgeFeedback::kSlowDuration - index);
                Require(scale >= previousScale && scale >= 0.13f && scale <= 1.0f,
                    "Just dodge world time jumped backwards or remained stopped");
                previousScale = scale;
            }
            Require(closestFov < -0.015f && widestFov > 0.01f,
                "Just dodge did not distinguish focus from release");
            for (int index = 0; index < 180; ++index) {
                frame.railDistance += frame.railSpeed; frame.playerPosition.z = frame.railDistance;
                normal.Update(frame, mode); dodged.Update(frame, mode);
            }
            Require(std::abs(dodged.DisplayPose().fov - normal.DisplayPose().fov) < 0.002f &&
                std::abs(dodged.DisplayPose().position.z - normal.DisplayPose().position.z) < 0.03f,
                "Just dodge left permanent camera drift");
        }
        std::cout << "GAMEPLAY_CAMERA_PASS modes=2 sequences=12000 finite=1 handoff=1 pause_reads=1 shake_lifetime=1 no_shake_drift=1 retry=1 dodge_focus_release=1\n";
    } catch (const std::exception& error) {
        std::cerr << "GAMEPLAY_CAMERA_FAIL " << error.what() << '\n';
        return 1;
    }
}
