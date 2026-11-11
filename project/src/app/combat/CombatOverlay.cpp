#include "app/combat/CombatOverlay.h"

#include "app/enemy/BossAttackController.h"
#include "app/enemy/Enemy.h"
#include "app/runtime/GameRuntimeMath.h"
#include "engine/3d/ScreenProjection.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {
using CombatOverlay::TelegraphView;
using GameRuntimeMath::Lerp;

bool Project(const TelegraphView& view, const Math::Vector3& worldPosition, Math::Vector2& screenPosition)
{
    return ScreenProjection::ToViewport(worldPosition, view.viewProjection, view.viewportMin, view.viewportSize, screenPosition);
}

// ボスの銃口を画面座標へ写し、予告中の発光を対応する位置に描く。
void DrawBossMuzzles(ImDrawList& drawList, const TelegraphView& view, const Enemy& enemy,
    const BossAttackController& bossAttack)
{
    const int chargeDuration = bossAttack.WindupFrames();
    const float bossChargeRate = 1.0f - std::clamp(
        static_cast<float>(bossAttack.RemainingFrames()) /
            static_cast<float>((std::max)(chargeDuration, 1)),
        0.0f,
        1.0f);
    // 実際の発射位置を投影する。自機を囲む照準線・追尾マーカーは付けない。
    const bool charge = bossAttack.CurrentPattern() == BossAttackController::Pattern::Charge;
    const bool aimLocked = bossAttack.IsAimLocked();
    const int muzzleCount = charge ? 1 : 2;
    for (int index = 0; index < muzzleCount; ++index) {
        if (bossAttack.CurrentPattern() == BossAttackController::Pattern::Sweep &&
            index != bossAttack.ShotIndex() % 2) { continue; }
        auto position = enemy.GetBossMuzzlePosition(charge ? 0.0f : index == 0 ? -1.0f : 1.0f);
        position.z -= 1.0f; // FireEnemyBulletと同じ発射位置。
        Math::Vector2 screen{};
        Math::Vector2 rim{};
        if (!Project(view, position, screen) ||
            !Project(view, { position.x + 0.55f, position.y, position.z }, rim)) { continue; }
        const ImVec2 muzzle(screen.x, screen.y);
        const float radius = std::clamp(std::abs(rim.x - screen.x), 3.0f, 13.0f) *
            (0.55f + bossChargeRate * (charge ? 1.10f : 0.50f));
        const int alpha = static_cast<int>(100.0f + bossChargeRate * 125.0f);
        drawList.AddCircleFilled(muzzle, radius * 2.0f, IM_COL32(255, 65, 18, alpha / 5), 24);
        drawList.AddCircleFilled(muzzle, radius, IM_COL32(255, 104, 32, alpha), 24);
        drawList.AddCircleFilled(muzzle, radius * (aimLocked ? 0.55f : 0.30f),
            IM_COL32(255, 240, 195, aimLocked ? 255 : alpha), 16);
    }
}

// シールドを持つ敵の周囲に防御状態を示す輪や光を重ねる。
void DrawShield(ImDrawList& drawList, const Enemy& enemy, const Math::Vector2& enemyScreen, float clock)
{
    const float shieldRate = std::clamp(
        static_cast<float>(enemy.GetShieldHp()) /
            static_cast<float>((std::max)(enemy.GetShieldMaxHp(), 1)),
        0.0f,
        1.0f);
    const float shieldPulse =
        0.5f + 0.5f * std::sin(clock * 0.16f + enemyScreen.x * 0.01f);
    const int shieldAlpha = static_cast<int>(
        105.0f + shieldRate * 90.0f + shieldPulse * 35.0f);
    const ImVec2 center(enemyScreen.x, enemyScreen.y);
    const float radius = 25.0f + shieldPulse * 4.0f;
    drawList.AddCircleFilled(
        center,
        radius,
        IM_COL32(32, 176, 255, shieldAlpha / 7),
        32);
    drawList.AddCircle(
        center,
        radius,
        IM_COL32(86, 222, 255, shieldAlpha),
        32,
        2.5f);
    drawList.AddCircle(
        center,
        radius + 5.0f,
        IM_COL32(180, 248, 255, shieldAlpha / 2),
        32,
        1.0f);
}

// 支援機と支援対象を画面へ写し、つながっていることが分かる線を描く。
void DrawSupportLinks(ImDrawList& drawList, const TelegraphView& view, const Enemy& enemy,
    const Math::Vector2& enemyScreen, const std::list<std::unique_ptr<Enemy>>& enemies)
{
    const float supportPulse =
        0.5f + 0.5f * std::sin(view.clock * 0.18f + enemyScreen.y * 0.01f);
    const int supportAlpha = static_cast<int>(
        120.0f + supportPulse * 95.0f);
    const ImVec2 supportCenter(enemyScreen.x, enemyScreen.y);
    drawList.AddCircleFilled(
        supportCenter,
        13.0f + supportPulse * 3.0f,
        IM_COL32(112, 255, 94, supportAlpha / 6),
        20);
    drawList.AddCircle(
        supportCenter,
        17.0f + supportPulse * 4.0f,
        IM_COL32(132, 255, 104, supportAlpha),
        4,
        2.5f);

    int linkCount = 0;
    constexpr int kMaxSupportLinks = 3;
    for (const auto& linkedEnemy : enemies) {
        if (!linkedEnemy || linkedEnemy.get() == &enemy ||
            linkedEnemy->IsDead() || linkedEnemy->IsSupport()) {
            continue;
        }
        Math::Vector2 linkedScreen{};
        if (!Project(view, linkedEnemy->GetAimPosition(), linkedScreen)) {
            continue;
        }
        drawList.AddLine(
            supportCenter,
            ImVec2(linkedScreen.x, linkedScreen.y),
            IM_COL32(102, 255, 132, supportAlpha / 2),
            1.5f + supportPulse);
        if (++linkCount >= kMaxSupportLinks) {
            break;
        }
    }
}

// 狙撃機の構え・発射・反撃可能時間を画面上の線や光で示す。
void DrawSniper(ImDrawList& drawList, const TelegraphView& view, const Enemy& enemy,
    const Math::Vector2& enemyScreen)
{
    const auto& fire = enemy.GetFireControl();
    const float charge = fire.ChargeRate();
    const float flash = fire.ShotFlash();
    const bool aimLocked = fire.IsBraced() && !fire.IsTracking();
    // 危険な終盤だけ射線を出す。固定後の終点は横・縦へ追尾しない。
    // レール前進による深度の変化は実際の射撃計算と揃える。
    if (enemy.CanShoot() && fire.IsBraced() && (charge >= 0.55f || aimLocked)) {
        Math::Vector2 muzzleScreen{}, aimScreen{};
        Math::Vector3 muzzleWorld = enemy.GetAimPosition();
        muzzleWorld.z -= 1.0f;
        Math::Vector3 aimWorld = fire.aimPoint;
        aimWorld.z = view.playerRailZ;
        aimWorld.y += 0.08f; // FireEnemyBulletの照準補正と一致させる。
        if (Project(view, muzzleWorld, muzzleScreen) && Project(view, aimWorld, aimScreen)) {
            const ImVec2 muzzle(muzzleScreen.x, muzzleScreen.y), aim(aimScreen.x, aimScreen.y);
            const int alpha = aimLocked ? 205 : 95;
            drawList.AddLine(muzzle, aim, IM_COL32(35, 9, 21, alpha), aimLocked ? 3.0f : 2.5f);
            drawList.AddLine(muzzle, aim, IM_COL32(255, aimLocked ? 120 : 58, 92, alpha), 1.0f);
            // 自機の輪郭を覆わず、被弾位置の左右だけを短く示す。
            const float radius = aimLocked ? 18.0f : 23.0f;
            for (const float side : { -1.0f, 1.0f }) {
                drawList.AddLine({ aim.x + side * radius, aim.y - 7.0f },
                    { aim.x + side * (radius - 4.0f), aim.y }, IM_COL32(255, 124, 132, alpha), 1.6f);
                drawList.AddLine({ aim.x + side * (radius - 4.0f), aim.y },
                    { aim.x + side * radius, aim.y + 7.0f }, IM_COL32(255, 124, 132, alpha), 1.6f);
            }
        }
    }
    if (enemy.IsChargeCounterOpen()) {
        Math::Vector2 rim{};
        const auto center = enemy.GetAimPosition();
        if (Project(view, { center.x + enemy.GetAimRadius(), center.y, center.z }, rim)) {
            const float radius = std::clamp(std::abs(rim.x - enemyScreen.x) + 8.0f, 16.0f, 42.0f);
            const float fade = std::clamp((66.0f - fire.RecoveryElapsed()) / 12.0f, 0.0f, 1.0f);
            const int alpha = static_cast<int>(240.0f * fade);
            // 赤い射線から金の開いた括弧へ。撃ち終わりのチャージ反撃だけを知らせる。
            for (const float side : { -1.0f, 1.0f }) {
                const ImVec2 corner(enemyScreen.x + side * radius, enemyScreen.y);
                drawList.AddLine({ corner.x - side * 6.0f, corner.y - 9.0f }, corner,
                    IM_COL32(255, 218, 133, alpha), 2.2f);
                drawList.AddLine(corner, { corner.x - side * 6.0f, corner.y + 9.0f },
                    IM_COL32(255, 218, 133, alpha), 2.2f);
            }
        }
    }
    if ((charge > 0.0f || flash > 0.0f) && enemy.CanShoot()) {
        Math::Vector3 muzzleWorld = enemy.GetAimPosition();
        muzzleWorld.z -= 1.0f; // FireEnemyBulletと同じ発射位置に付ける。照準上には描かない。
        Math::Vector2 muzzleScreen{}, edgeScreen{};
        const Math::Vector3 edgeWorld{ muzzleWorld.x + 0.32f, muzzleWorld.y, muzzleWorld.z };
        if (Project(view, muzzleWorld, muzzleScreen) && Project(view, edgeWorld, edgeScreen)) {
            const ImVec2 muzzle(muzzleScreen.x, muzzleScreen.y);
            const float radius = std::clamp(std::abs(edgeScreen.x - muzzleScreen.x), 2.0f, 5.0f) *
                (0.65f + charge * 0.60f + flash * 0.50f);
            const int alpha = static_cast<int>(100.0f + charge * 100.0f + flash * 55.0f);
            drawList.AddCircleFilled(muzzle, radius * 2.5f, IM_COL32(255, 76, 40, alpha / 6), 20);
            drawList.AddCircleFilled(muzzle, radius, IM_COL32(255, 130, 70, alpha), 16);
            drawList.AddCircleFilled(muzzle, radius * 0.42f, IM_COL32(255, 246, 220, alpha), 12);
            // 照準固定を銃口の白い閃光でも知らせる。
            if (aimLocked || flash > 0.0f) {
                const float width = radius * (2.8f + flash * 1.8f);
                drawList.AddLine({ muzzle.x - width, muzzle.y }, { muzzle.x + width, muzzle.y },
                    IM_COL32(255, 228, 176, alpha), 1.5f);
            }
        }
    }
}

// 練習弾の発射前に、狙撃機の位置を基準とした予告を表示する。
void DrawTutorialSniper(ImDrawList& drawList, const Math::Vector2& enemyScreen, float chargeRate, float clock)
{
    const float pulse = 0.5f + 0.5f * std::sin(clock * 0.48f);
    const int lineAlpha = static_cast<int>(70.0f + chargeRate * 130.0f + pulse * 35.0f);
    const ImVec2 muzzle(enemyScreen.x, enemyScreen.y + 4.0f);
    const float coreRadius = 3.5f + chargeRate * 5.5f;
    drawList.AddCircleFilled(
        muzzle,
        coreRadius * 2.3f,
        IM_COL32(255, 28, 82, (std::clamp)(lineAlpha / 5, 0, 52)),
        24);
    drawList.AddCircleFilled(
        muzzle,
        coreRadius,
        IM_COL32(255, 60, 106, (std::clamp)(lineAlpha, 0, 255)),
        20);
    drawList.AddCircleFilled(
        muzzle,
        (std::max)(1.5f, coreRadius * 0.32f),
        IM_COL32(255, 236, 224, (std::clamp)(lineAlpha + 20, 0, 255)),
        14);
    constexpr int kSniperEnergyRayCount = 5;
    for (int rayIndex = 0; rayIndex < kSniperEnergyRayCount; ++rayIndex) {
        const float angle =
            -1.72f + static_cast<float>(rayIndex) * 0.86f +
            std::sin(clock * 0.07f) * 0.08f;
        const float outerRadius = 25.0f - chargeRate * 8.0f;
        const float innerRadius = 11.0f - chargeRate * 2.0f;
        const ImVec2 outer(
            muzzle.x + std::cos(angle) * outerRadius,
            muzzle.y + std::sin(angle) * outerRadius);
        const ImVec2 inner(
            muzzle.x + std::cos(angle) * innerRadius,
            muzzle.y + std::sin(angle) * innerRadius);
        drawList.AddLine(
            outer,
            inner,
            IM_COL32(255, 82, 124, (std::clamp)(lineAlpha, 0, 255)),
            1.3f + chargeRate * 1.2f);
    }
}

}

// フィーバー中に画面の周辺へ光と流れを重ね、戦闘画面中央の視認性を保つ。
void CombatOverlay::DrawFeverBackdrop(ImDrawList& drawList, const Math::Vector2& hudMin,
    const Math::Vector2& hudSize, float speedEffectRate, float clock)
{
    const float feverVisualRate = std::clamp(speedEffectRate, 0.0f, 1.0f);
    if (feverVisualRate <= 0.001f) {
        return;
    }

    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);
    const ImVec2 viewportMax(origin.x + drawSize.x, origin.y + drawSize.y);
    if (drawSize.x <= 1.0f || drawSize.y <= 1.0f) {
        return;
    }

    const float rainbowTime = clock * 0.008f;
    const auto rainbowColor = [rainbowTime, feverVisualRate](float offset, int alpha) {
        float hue = std::fmod(rainbowTime + offset, 1.0f);
        if (hue < 0.0f) {
            hue += 1.0f;
        }
        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        ImGui::ColorConvertHSVtoRGB(hue, 0.76f, 1.0f, red, green, blue);
        return IM_COL32(
            static_cast<int>(red * 255.0f),
            static_cast<int>(green * 255.0f),
            static_cast<int>(blue * 255.0f),
            (std::clamp)(
                static_cast<int>(static_cast<float>(alpha) * feverVisualRate),
                0,
                255));
    };

    const float ambiencePulse =
        0.5f + 0.5f * std::sin(clock * 0.055f);
    const int dimAlpha = static_cast<int>(
        (50.0f + ambiencePulse * 9.0f) * feverVisualRate);
    drawList.AddRectFilled(
        origin,
        viewportMax,
        IM_COL32(2, 4, 15, dimAlpha));
    drawList.AddRectFilled(
        origin,
        viewportMax,
        rainbowColor(0.0f, 8));

    const float vignetteSize =
        std::clamp((std::min)(drawSize.x, drawSize.y) * 0.14f, 54.0f, 112.0f);
    const int vignetteAlpha = static_cast<int>(
        (68.0f + ambiencePulse * 18.0f) * feverVisualRate);
    drawList.AddRectFilledMultiColor(
        origin,
        ImVec2(viewportMax.x, origin.y + vignetteSize),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, 0));
    drawList.AddRectFilledMultiColor(
        ImVec2(origin.x, viewportMax.y - vignetteSize),
        viewportMax,
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, vignetteAlpha));
    drawList.AddRectFilledMultiColor(
        origin,
        ImVec2(origin.x + vignetteSize, viewportMax.y),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, vignetteAlpha));
    drawList.AddRectFilledMultiColor(
        ImVec2(viewportMax.x - vignetteSize, origin.y),
        viewportMax,
        IM_COL32(0, 1, 8, 0),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, vignetteAlpha),
        IM_COL32(0, 1, 8, 0));

    // 風の帯は通常飛行と共通で描き、発動時のカメラ・噴射とタイミングをそろえる。
    constexpr int kBackdropHorizontalSegments = 20;
    constexpr int kBackdropVerticalSegments = 12;
    for (int layer = 0; layer < 4; ++layer) {
        const float margin = 6.0f + static_cast<float>(layer) * 4.0f;
        const float left = origin.x + margin;
        const float right = viewportMax.x - margin;
        const float top = origin.y + margin;
        const float bottom = viewportMax.y - margin;
        const int alpha = 34 - layer * 6;
        const float thickness = 7.0f - static_cast<float>(layer) * 1.2f;
        for (int segment = 0; segment < kBackdropHorizontalSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) /
                static_cast<float>(kBackdropHorizontalSegments);
            const float rate1 =
                static_cast<float>(segment + 1) /
                static_cast<float>(kBackdropHorizontalSegments);
            drawList.AddLine(
                ImVec2(Lerp(left, right, rate0), top),
                ImVec2(Lerp(left, right, rate1) + 1.0f, top),
                rainbowColor(rate0 * 0.78f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
            drawList.AddLine(
                ImVec2(Lerp(left, right, rate0), bottom),
                ImVec2(Lerp(left, right, rate1) + 1.0f, bottom),
                rainbowColor(0.78f - rate0 * 0.78f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
        }
        for (int segment = 0; segment < kBackdropVerticalSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) /
                static_cast<float>(kBackdropVerticalSegments);
            const float rate1 =
                static_cast<float>(segment + 1) /
                static_cast<float>(kBackdropVerticalSegments);
            drawList.AddLine(
                ImVec2(left, Lerp(top, bottom, rate0)),
                ImVec2(left, Lerp(top, bottom, rate1) + 1.0f),
                rainbowColor(0.18f + rate0 * 0.46f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
            drawList.AddLine(
                ImVec2(right, Lerp(top, bottom, rate0)),
                ImVec2(right, Lerp(top, bottom, rate1) + 1.0f),
                rainbowColor(0.94f - rate0 * 0.46f + static_cast<float>(layer) * 0.05f, alpha),
                thickness);
        }
    }

    const auto getPerimeterPoint = [origin, viewportMax](float rate, float margin) {
        const float left = origin.x + margin;
        const float right = viewportMax.x - margin;
        const float top = origin.y + margin;
        const float bottom = viewportMax.y - margin;
        const float width = (std::max)(right - left, 1.0f);
        const float height = (std::max)(bottom - top, 1.0f);
        const float perimeter = (width + height) * 2.0f;
        float distance = std::fmod(rate, 1.0f) * perimeter;
        if (distance < 0.0f) {
            distance += perimeter;
        }
        if (distance < width) {
            return ImVec2(left + distance, top);
        }
        distance -= width;
        if (distance < height) {
            return ImVec2(right, top + distance);
        }
        distance -= height;
        if (distance < width) {
            return ImVec2(right - distance, bottom);
        }
        distance -= width;
        return ImVec2(left, bottom - distance);
    };

    constexpr int kBorderSparkleCount = 42;
    for (int index = 0; index < kBorderSparkleCount; ++index) {
        const float indexRate =
            static_cast<float>(index) / static_cast<float>(kBorderSparkleCount);
        const float travelRate =
            indexRate + clock * (0.00042f + 0.00005f * static_cast<float>(index % 3));
        const ImVec2 position = getPerimeterPoint(
            travelRate,
            17.0f + static_cast<float>(index % 3) * 4.0f);
        const float rawTwinkle =
            0.5f + 0.5f * std::sin(clock * 0.18f + static_cast<float>(index) * 1.73f);
        const float twinkle = rawTwinkle * rawTwinkle * rawTwinkle;
        const float radius = 2.2f + twinkle * (5.8f + static_cast<float>(index % 4));
        const int glowAlpha = static_cast<int>(65.0f + twinkle * 125.0f);
        const ImU32 color = rainbowColor(indexRate + twinkle * 0.10f, glowAlpha);
        drawList.AddCircleFilled(position, radius * 1.35f, rainbowColor(indexRate, glowAlpha / 3), 16);
        drawList.AddQuadFilled(
            ImVec2(position.x, position.y - radius * 1.55f),
            ImVec2(position.x + radius * 0.34f, position.y),
            ImVec2(position.x, position.y + radius * 1.55f),
            ImVec2(position.x - radius * 0.34f, position.y),
            color);
        drawList.AddQuadFilled(
            ImVec2(position.x - radius * 1.55f, position.y),
            ImVec2(position.x, position.y - radius * 0.34f),
            ImVec2(position.x + radius * 1.55f, position.y),
            ImVec2(position.x, position.y + radius * 0.34f),
            color);
        drawList.AddCircleFilled(
            position,
            (std::max)(1.0f, radius * 0.24f),
            IM_COL32(
                255,
                255,
                255,
                static_cast<int>((145.0f + twinkle * 110.0f) * feverVisualRate)),
            10);
    }
}

// 画面に映る敵を見て、攻撃の予告・シールド・支援リンクなどを画面座標に重ねる。
void CombatOverlay::DrawEnemyTelegraphs(ImDrawList& drawList, const TelegraphView& view,
    const std::list<std::unique_ptr<Enemy>>& enemies, const BossAttackController& bossAttack)
{
    bool tutorialSniperDrawn = false;
    for (const auto& enemy : enemies) {
        if (!enemy || enemy->IsDead()) { continue; }

        Math::Vector2 screen{};
        if (!Project(view, enemy->GetAimPosition(), screen)) { continue; }

        if (enemy->IsBoss() && bossAttack.IsAttacking()) {
            DrawBossMuzzles(drawList, view, *enemy, bossAttack);
        }
        if (enemy->IsShield() && enemy->HasShield()) {
            DrawShield(drawList, *enemy, screen, view.clock);
        }
        if (enemy->IsSupport()) {
            DrawSupportLinks(drawList, view, *enemy, screen, enemies);
        }
        if (!enemy->IsSniper()) { continue; }

        if (!view.tutorial) {
            DrawSniper(drawList, view, *enemy, screen);
        } else if (!tutorialSniperDrawn && view.tutorialSniperCharge > 0.0f && enemy->CanShoot()) {
            DrawTutorialSniper(drawList, screen, view.tutorialSniperCharge, view.clock);
            tutorialSniperDrawn = true;
        }
    }
}
