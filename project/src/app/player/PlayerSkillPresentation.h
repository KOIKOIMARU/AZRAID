#pragma once

#include "app/player/PlayerSkill.h"
#include "engine/3d/Object3d.h"

#include <array>
#include <memory>

class Camera;
class Model;
struct ImDrawList;

// スキルが確定した攻撃位置を使い、残像・斬撃・画面効果を再生する
class PlayerSkillPresentation {
public:
    void Initialize(Object3dCommon& common, Model& ship);
    void Clear();
    void Reset();
    void Advance(float frameStep);
    void ShowStrike(int strike, const Math::Vector3& playerPosition, const Math::Vector3& targetPosition);
    void ShowFinisher(const PlayerSkill& skill);
    void DrawObjects(Object3dCommon& common, const Camera& camera, const PlayerSkill& skill);
    void DrawOverlay(ImDrawList& draw, const Math::Vector2& min, const Math::Vector2& size,
        const Math::Matrix4x4* viewProjection, const PlayerSkill& skill) const;

#ifdef _DEBUG
    float FirstStrikeAgeForProbe() const { return slashes_[0].age; }
#endif

private:
    struct Slash {
        std::unique_ptr<Object3d> ghost;
        std::unique_ptr<Object3d> blade;
        std::unique_ptr<Object3d> wake;
        Math::Vector3 from{};
        Math::Vector3 position{};
        float angle = 0.0f;
        float age = -1.0f; // 60fps換算　負値は非表示
    };
    std::array<Slash, PlayerSkill::kMaxTargets> slashes_{}; // 起動時に確保して繰り返し使う
    std::array<std::unique_ptr<Object3d>, 4> finisherVisuals_;
    Math::Vector3 finisherPosition_{};
    Math::Vector3 shipCenter_{};
};
