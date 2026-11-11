#pragma once

#include "app/player/GameplayAim.h"
#include "app/player/PlayerShooting.h"

#include <optional>

class Input;
class Player;
class ProjectileSystem;

// 操作入力を機体の移動・照準・射撃へ反映する。演出は発射結果を受け取る側で行う。
class PlayerControl {
public:
    struct AimView {
        Math::Vector2 min{};
        Math::Vector2 size{};
        const Math::Matrix4x4* viewProjection = nullptr;
    };
    struct ShotFrame {
        const Player* player;
        AimView view;
        float railSpeed;
        float cameraClock;
        bool tutorial;
        bool fever;
    };
    struct ShotResult {
        Math::Vector3 muzzle{};
        bool charged = false;
        bool fired = false;
    };
    struct ShotTuning {
        float normalSpeed = 1.36f;
        float chargedSpeed = 1.62f;
        float chargeMultiplier = 1.12f;
    };

    void Reset();
    void UpdateMovement(Player& player, Input* input, float timeScale, float controlStep, float railDistance) const;
    void UpdateAim(const AimView& view, const std::list<std::unique_ptr<Enemy>>& enemies,
        std::optional<Math::Vector2> cursor);
    Math::Vector3 AimDirection(const Math::Vector3& origin, const AimView& view) const;
    // 値ありは発射要求。プールが満杯なら fired は false のままチャージを保持する。
    std::optional<ShotResult> UpdateShooting(Input* input, ProjectileSystem& projectiles, const ShotFrame& frame);

    GameplayAim& Aim() { return aim_; }
    const GameplayAim& Aim() const { return aim_; }
    PlayerShooting& Shooting() { return shooting_; }
    const PlayerShooting& Shooting() const { return shooting_; }
    ShotTuning GetShotTuning() const { return shotTuning_; }
    int ShotsFired() const { return shotsFired_; }

#ifdef ENABLE_DEBUG_GUI
    void SetShotTuning(ShotTuning tuning) { shotTuning_ = tuning; }
    void ResetShotCountForDebug() { shotsFired_ = 0; }
#endif
#ifdef _DEBUG
    void RecordShotForProbe() { ++shotsFired_; }
#endif

private:
    GameplayAim aim_;
    PlayerShooting shooting_;
    ShotTuning shotTuning_;
    int shotsFired_ = 0;
};
