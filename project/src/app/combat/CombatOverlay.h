// 戦闘中の画面に、フィーバーの背景演出や敵の攻撃予告を重ねる

#pragma once

#include "engine/base/Math.h"

#include <list>
#include <memory>

class BossAttackController;
class Enemy;
struct ImDrawList;

namespace CombatOverlay {

struct TelegraphView {
    const Math::Matrix4x4& viewProjection;
    Math::Vector2 viewportMin;
    Math::Vector2 viewportSize;
    float clock;
    float playerRailZ;
    float tutorialSniperCharge;
    bool tutorial;
};

void DrawFeverBackdrop(ImDrawList& drawList, const Math::Vector2& hudMin,
    const Math::Vector2& hudSize, float speedEffectRate, float clock);
void DrawEnemyTelegraphs(ImDrawList& drawList, const TelegraphView& view,
    const std::list<std::unique_ptr<Enemy>>& enemies, const BossAttackController& bossAttack);

}
