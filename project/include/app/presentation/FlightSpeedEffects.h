// 飛行速度に応じた光の流れを管理する。3D空間の光と、画面端に重ねる線を持つ。

#pragma once

#include "engine/3d/Object3d.h"

#include <cstddef>
#include <memory>
#include <vector>

class Camera;
class Model;
class Object3dCommon;
struct ImDrawList;

class FlightSpeedEffects {
public:
    struct Assets {
        Model* spark = nullptr;
        Model* glow = nullptr;
        Model* streak = nullptr;
        Model* fallbackStreak = nullptr;
    };

    struct Frame {
        const Camera* camera = nullptr;
        Math::Vector2 viewportMin{};
        Math::Vector2 viewportSize{};
        float railDistance = 0.0f;
        float speedRate = 0.0f;
        float feverSpeedRate = 0.0f;
    };

    struct ScreenStreaks {
        const Math::Vector3* playerPosition = nullptr;
        Math::Vector2 reticleScreen{};
        float railSpeed = 0.0f;
        float worldTimeScale = 1.0f;
        float boostKick = 0.0f;
        int feverFlashFrames = 0;
    };

    void Initialize(Object3dCommon* common, const Assets& assets, float railDistance);
    void Clear();
    void UpdateWorld(const Frame& frame, float frameStep, bool alignStreaks);
    void DrawWorld();
    void DrawScreenStreaks(const Frame& frame, const ScreenStreaks& streaks, ImDrawList& drawList) const;

    size_t WorldCueCount() const { return cues_.size(); }

private:
    struct Cue {
        std::unique_ptr<Object3d> object;
        Math::Vector3 anchor{};
        Math::Vector4 color{};
        float speedMultiplier = 0.0f;
        float lateralDrift = 0.0f;
        float verticalDrift = 0.0f;
        float driftSpeed = 0.0f;
        float phase = 0.0f;
        float baseScale = 0.0f;
        float aspectX = 1.0f;
        float aspectY = 1.0f;
        float spinSpeed = 0.0f;
    };

    Object3dCommon* common_ = nullptr;
    std::vector<Cue> cues_;
};
