#include "app/player/PlayerControl.h"

#include "app/enemy/Enemy.h"
#include "app/player/Player.h"
#include "app/combat/ProjectileSystem.h"
#include "engine/io/Input.h"

#include <algorithm>

// プレイ開始時に照準、射撃状態、発射数を戻す。
void PlayerControl::Reset()
{
    aim_.ClearTarget();
    shooting_.Reset();
    shotsFired_ = 0;
}

// 方向キーと回避入力をまとめ、自機へ移動を渡す。ステージの進行距離は前後位置の基準に使う。
void PlayerControl::UpdateMovement(Player& player, Input* input, float timeScale,
    float controlStep, float railDistance) const
{
    if (input) {
        const bool up = input->PushKey(DIK_W) || input->PushKey(DIK_UP);
        const bool down = input->PushKey(DIK_S) || input->PushKey(DIK_DOWN);
        const bool right = input->PushKey(DIK_D) || input->PushKey(DIK_RIGHT);
        const bool left = input->PushKey(DIK_A) || input->PushKey(DIK_LEFT);
        const Player::MovementInput movement{
            .horizontal = static_cast<int>(right) - static_cast<int>(left),
            .vertical = static_cast<int>(up) - static_cast<int>(down),
            .dodgeTriggered = input->TriggerKey(DIK_LSHIFT) || input->TriggerKey(DIK_RSHIFT)
        };
        player.Update(movement, timeScale, controlStep);
    }
    player.SetRailZ(railDistance);
}

// マウス位置が取れれば照準へ反映し、取れなければ画面中央を使う。投影行列がある場合は照準に近い敵を探す。
void PlayerControl::UpdateAim(const AimView& view, const std::list<std::unique_ptr<Enemy>>& enemies,
    std::optional<Math::Vector2> cursor)
{
    const Math::Vector2 center{ view.min.x + view.size.x * 0.5f, view.min.y + view.size.y * 0.5f };
    aim_.UpdateReticle(cursor.value_or(center), view.min, view.size);
    if (view.viewProjection) {
        aim_.SelectTarget(enemies, *view.viewProjection, view.min, view.size);
    } else {
        aim_.ClearTarget();
    }
}

// 発射位置と現在の照準から、弾を飛ばす向きを求める。
Math::Vector3 PlayerControl::AimDirection(const Math::Vector3& origin, const AimView& view) const
{
    if (!view.viewProjection) { return { 0.0f, 0.0f, 1.0f }; }

    const Enemy* target = aim_.Target();
    const float depth = target && !target->IsDead() ?
        (std::max)(origin.z + 4.0f, target->GetAimPosition().z) : origin.z + 45.0f;
    return ProjectileSystem::AimDirection(origin, *view.viewProjection, aim_.Reticle(), view.min, view.size, depth);
}

// 発射間隔とチャージの状態を確かめ、撃てるときだけ弾の生成を依頼する。
std::optional<PlayerControl::ShotResult> PlayerControl::UpdateShooting(Input* input,
    ProjectileSystem& projectiles, const ShotFrame& frame)
{
    const PlayerShooting::InputState shootingInput{
        input && input->PushKey(DIK_SPACE),
        input && input->TriggerKey(DIK_SPACE)
    };
    if (!shooting_.Update(shootingInput, frame.fever)) { return std::nullopt; }

    ShotResult result{ .charged = shooting_.IsChargeReady() };
    if (!frame.player) { return result; }

    result.muzzle = frame.player->GetTranslate();
    result.muzzle.z += 1.75f;
    const Enemy* selected = aim_.Target();
    const Enemy* target = aim_.IsReticleOnTarget() && selected && !selected->IsDead() ? selected : nullptr;
    const ProjectileSystem::PlayerShot shot{
        .origin = result.muzzle,
        .aimDirection = AimDirection(result.muzzle, frame.view),
        .target = target,
        .normalSpeed = shotTuning_.normalSpeed,
        .chargedSpeed = shotTuning_.chargedSpeed,
        .chargeMultiplier = shotTuning_.chargeMultiplier,
        .railSpeed = frame.railSpeed,
        .clock = frame.cameraClock,
        .tutorial = frame.tutorial,
        .charged = result.charged,
        .fever = frame.fever,
    };
    if (projectiles.FirePlayer(shot)) {
        result.fired = true;
        ++shotsFired_;
        shooting_.OnShotFired(frame.fever);
    }
    return result;
}
