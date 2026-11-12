// ===== GameplayHud.cpp =====
// HP、照準、チャージ、スコアなどを描き、命中や被弾時の表示変化を管理する

#include "app/ui/GameplayHud.h"
#include "app/ui/CombatHud.h"
#include "app/runtime/GameRuntimeMath.h"
#include "app/player/JustDodgeFeedback.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
using GameRuntimeMath::Lerp;
constexpr float kTwoPi = 6.28318530718f;
}

// 新しいプレイに向けて命中表示、被弾表示、チャージの発光などを初期化する
void GameplayHud::Reset()
{
    chargeFlash_ = hitFrames_ = damageFrames_ = 0;
    hitDuration_ = damageDuration_ = 1;
    hitStrength_ = 1.0f;
    hitCharged_ = hitDestroyed_ = false;
    hitScreen_ = damageScreen_ = {};
    damageDirection_ = { 0, -1 };
}

// 命中位置と弾の種類を記録し、照準周辺に短く命中の印を出せるようにする
void GameplayHud::RecordHit(Math::Vector2 screen, bool charged, bool boss, bool destroyed)
{
    hitScreen_ = screen;
    hitDuration_ = destroyed ? (boss ? 30 : 24) : (charged || boss ? 20 : 16);
    hitFrames_ = hitDuration_;
    hitStrength_ = boss ? 1.35f : destroyed ? 1.22f : charged ? 1.14f : 1.0f;
    hitCharged_ = charged;
    hitDestroyed_ = destroyed;
}

// 被弾の位置と方向を控え、被弾表示が消えるまでの時間を設定する
void GameplayHud::RecordDamage(Math::Vector2 screen, Math::Vector2 incomingVelocity)
{
    damageScreen_ = screen;
    damageDirection_ = { -incomingVelocity.x, incomingVelocity.y };
    const float length = std::sqrt(damageDirection_.x * damageDirection_.x + damageDirection_.y * damageDirection_.y);
    if (length > 0.001f) { damageDirection_.x /= length; damageDirection_.y /= length; }
    else { damageDirection_ = { 0, -1 }; }
    damageFrames_ = damageDuration_ = 26;
}

// 命中表示や被弾時の点滅を進め、表示時間が終わったものを画面から消す
void GameplayHud::TickFeedback()
{
    if (hitFrames_ > 0 && --hitFrames_ <= 0) { hitDuration_ = 1; }
    if (damageFrames_ > 0 && --damageFrames_ <= 0) { damageDuration_ = 1; }
}

// チャージの表示を強調する残りフレームを進める
void GameplayHud::TickCharge() { if (chargeFlash_ > 0) { --chargeFlash_; } }
void GameplayHud::FlashCharge(int frames) { chargeFlash_ = (std::max)(chargeFlash_, frames); }
void GameplayHud::FlashChargeShot() { chargeFlash_ = 18; }

// リング通過で得た効果を、残り表示時間に合わせて画面に出す
void GameplayHud::DrawRingNotice(const Frame& frame, ImDrawList* draw) const
{
    if (frame.ringNoticeFrames > 0) {
        const auto& min = frame.min;
        const auto& size = frame.size;
        const CombatHud::Layout layout(min, size);
        const float fade = std::clamp(static_cast<float>(frame.ringNoticeFrames) / 12.0f, 0.0f, 1.0f);
        CombatHud::Heading(draw, {layout.charge.x, layout.charge.y - 29.0f * layout.scale},
            20.0f * layout.scale, frame.ringExtendedFever ?
                IM_COL32(255, 223, 151, static_cast<int>(255 * fade)) : IM_COL32(120, 225, 255, static_cast<int>(255 * fade)),
            frame.ringExtendedFever ? "加速 / フィーバー延長" : "加速 / チャージ補充");
    }
}

// HP・チャージ・スコアなどの基本情報を、現在の表示領域に収まるように配置する
void GameplayHud::DrawCore(const Frame& frame, ImDrawList* draw) const
{
    const auto& hudMin = frame.min;
    const auto& hudSize = frame.size;
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);

    const int maxHp = (std::max)(frame.maxHp, 1);
    const int hp = (std::clamp)(frame.hp, 0, maxHp);
    const float hpRate =
        static_cast<float>(hp) / static_cast<float>(maxHp);
    const float chargeRate = frame.charge;
    const bool isChargeReady = frame.chargeReady;
    const float hudScale = CombatHud::Scale(hudSize);
    if (hpRate <= 0.34f) {
        const int alertAlpha = static_cast<int>(
            14.0f + (0.34f - hpRate) * 60.0f +
            8.0f * (0.5f + 0.5f * std::sin(frame.clock * 8.0f)));
        const int clampedAlertAlpha = (std::clamp)(alertAlpha, 0, 32);
        const float alertWidth = 44.0f;
        const ImU32 alertColor = IM_COL32(230, 42, 58, clampedAlertAlpha);
        const ImU32 clearAlertColor = IM_COL32(230, 42, 58, 0);
        const ImVec2 viewportMax(origin.x + drawSize.x, origin.y + drawSize.y);
        draw->AddRectFilledMultiColor(
            origin,
            ImVec2(viewportMax.x, origin.y + alertWidth),
            alertColor,
            alertColor,
            clearAlertColor,
            clearAlertColor);
        draw->AddRectFilledMultiColor(
            ImVec2(origin.x, viewportMax.y - alertWidth),
            viewportMax,
            clearAlertColor,
            clearAlertColor,
            alertColor,
            alertColor);
        draw->AddRectFilledMultiColor(
            origin,
            ImVec2(origin.x + alertWidth, viewportMax.y),
            alertColor,
            clearAlertColor,
            clearAlertColor,
            alertColor);
        draw->AddRectFilledMultiColor(
            ImVec2(viewportMax.x - alertWidth, origin.y),
            viewportMax,
            clearAlertColor,
            alertColor,
            alertColor,
            clearAlertColor);
    }

    const CombatHud::Layout layout(hudMin, hudSize);
    const ImVec2 playerAnchor = layout.health;
    const auto hpPoint = [&](float x, float y) {
        return ImVec2(playerAnchor.x + x * hudScale, playerAnchor.y + y * hudScale);
    };
    char hpText[16]{};
    std::snprintf(hpText, sizeof(hpText), "%d", hp);
    const ImU32 hpColor = CombatHud::HealthColor(hpRate);
    CombatHud::Wake(draw, hpPoint(-32, -28), hpPoint(234, 92));
    CombatHud::Slant(draw, hpPoint(0, -8), 48.0f * hudScale,
        hpColor, hpText);
    CombatHud::FlightMeter(draw, hpPoint(0, 51), 192 * hudScale, 7 * hudScale, hpRate,
        hpColor, hudScale);

    const auto weaponPoint = [&](float x, float y) {
        return ImVec2(layout.charge.x + x * hudScale, layout.charge.y + y * hudScale);
    };
    CombatHud::Wake(draw, weaponPoint(-28, -18), weaponPoint(276, 130), true);
    const ImU32 chargeColor = isChargeReady ? CombatHud::Gold :
        chargeRate > 0.0f ? CombatHud::Energy : CombatHud::Muted;
    CombatHud::Heading(draw, weaponPoint(0, 0), 22.0f * hudScale, chargeColor, "チャージ");
    CombatHud::KeyBadge(draw, weaponPoint(184, -1), weaponPoint(244, 27), hudScale, "SPACE", isChargeReady, chargeColor);
    CombatHud::FlightMeter(draw, weaponPoint(0, 33), 244 * hudScale, 5 * hudScale,
        chargeRate, isChargeReady ? CombatHud::Gold : CombatHud::Energy, hudScale);
    if (chargeFlash_ > 0) {
        const float flash = std::clamp(static_cast<float>(chargeFlash_) / 30.0f, 0.0f, 1.0f);
        if (isChargeReady) {
            draw->AddQuadFilled(weaponPoint(0, 30), weaponPoint(244, 30),
                weaponPoint(238, 41), weaponPoint(0, 41),
                CombatHud::SurfaceColor(IM_COL32(239, 241, 244, static_cast<int>(100 * flash * flash))));
        }
    }

    // 練習では得点や連続撃破を競わない　今の課題と操作に必要な計器を優先する
    if (!frame.tutorial) {
        const auto scorePoint = [&](float x, float y) {
            return ImVec2(layout.score.x + x * hudScale, layout.score.y + y * hudScale);
        };
        const int waveNumber = frame.wave;
        char scoreText[32]{};
        std::snprintf(scoreText, sizeof(scoreText), "%06d", frame.score);
        CombatHud::Wake(draw, scorePoint(-8, -28), scorePoint(304, 90), true);
        CombatHud::Slant(draw, scorePoint(272, -4), 40.0f * hudScale, CombatHud::White, scoreText,
            true, true, 0.0f, 272 * hudScale);
        if (!frame.bossSpawned) {
            // 三つの面は実際のウェーブ数　進行済みだけを埋め、説明ラベルを常駐させない
            for (int index = 0; index < frame.waveCount; ++index) {
                const float x = 196.0f + static_cast<float>(index) * 26.0f;
                draw->AddQuadFilled(scorePoint(x + 3, 51), scorePoint(x + 21, 51),
                    scorePoint(x + 18, 56), scorePoint(x, 56), CombatHud::SurfaceColor(
                        index < waveNumber ? CombatHud::White : IM_COL32(58, 65, 77, 235)));
            }
        }
    }

}

// ボスのHPと戦闘中の状態を、ボス用のゲージと表示に反映する
void GameplayHud::DrawBoss(const Frame& frame, ImDrawList* draw) const
{
    if (!frame.boss.visible) { return; }

    const auto& min = frame.min;
    const auto& size = frame.size;
    const float scale = CombatHud::Scale(size);
    const CombatHud::Layout layout(min, size);
    const ImVec2 anchor = layout.boss;
    const auto p = [&](float x, float y) {
        return ImVec2(anchor.x + x * scale, anchor.y + y * scale);
    };
    const float hpRate = frame.boss.hp;
    const bool counter = frame.boss.cue == BossCue::Counter;
    const bool attacking = frame.boss.cue != BossCue::None && !counter;
    CombatHud::Wake(draw, p(-48, -32), p(208, 100), true);
    CombatHud::Wake(draw, p(208, -32), p(464, 100));
    CombatHud::Heading(draw, p(18, 10), 26.0f * scale, CombatHud::White, "重装迎撃機");
    CombatHud::WingMeter(draw, p(18, 56), 380 * scale, 9 * scale, hpRate, CombatHud::Danger, scale);

    if (counter || attacking) {
        // 敵の状態と反撃可能時間だけを提示し、注意文やHPの重複数値は出さない
        const char* label = counter ? "反撃 ×2" :
            frame.boss.cue == BossCue::Fan ? "拡散弾" :
            frame.boss.cue == BossCue::Sweep ? "なぎ払い" :
            frame.boss.cue == BossCue::Crossfire ? "交差射撃" :
            frame.boss.cue == BossCue::Rush ? "接近射撃" : "チャージ砲";
        const ImU32 color = counter ? CombatHud::GaugeGold : CombatHud::Danger;
        const float rate = frame.boss.action;
        CombatHud::Readout(draw, p(398, 12), 22.0f * scale, color, label, true);
        CombatHud::WingMeter(draw, p(18, 69), 380 * scale, 3 * scale, rate, color, scale);
    }
}

// ステージの現在区間や次の敵、ボスの状態を短い案内として描く
void GameplayHud::DrawStageCue(const Frame& frame, ImDrawList* draw) const
{
    if (frame.bossDefeat <= 0 &&
        frame.justDodge <= 0) {
        return;
    }

    const auto& hudMin = frame.min;
    const auto& hudSize = frame.size;
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);

    if (frame.justDodge > 0) {
        const auto pulse = JustDodgeFeedback::Evaluate(frame.justDodge);
        const float scale = drawSize.y / 720.0f;
        const float sweep = (1.0f - frame.justDodge) * frame.dodgeDirection * 95.0f * scale;
        const int alpha = static_cast<int>(150.0f * pulse.contact + 52.0f * pulse.release);
        for (int side : { -1, 1 }) {
            const float x = origin.x + drawSize.x * (side < 0 ? 0.13f : 0.87f);
            const float y = origin.y + drawSize.y * (side < 0 ? 0.30f : 0.76f);
            draw->AddLine({ x + sweep, y }, { x + sweep - frame.dodgeDirection * 140.0f * scale, y + 18.0f * scale },
                IM_COL32(150, 230, 255, alpha), 1.6f * scale);
        }
    }

    if (frame.bossDefeat > 0) {
        const float rate =
            frame.bossDefeat;
        const int alpha = static_cast<int>(72.0f * rate * rate);
        draw->AddRectFilled(
            origin,
            ImVec2(origin.x + drawSize.x, origin.y + drawSize.y),
            IM_COL32(255, 174, 76, (std::clamp)(alpha, 0, 120)));
    }

}

// 照準位置と敵の狙いやすさ、チャージ状態を見て照準の色や形を選ぶ
void GameplayHud::DrawReticle(const Frame& frame, ImDrawList* draw) const
{
    const auto& hudMin = frame.min;
    const auto& hudSize = frame.size;
    const float scale = CombatHud::Scale(hudSize);
    const bool aligned = frame.reticleAligned;
    const bool charged = frame.chargeReady;
    const ImU32 color = CombatHud::SurfaceColor(aligned ? CombatHud::Danger : CombatHud::White);
    const ImU32 shadow = IM_COL32(10, 11, 12, 190);
    const ImVec2 center(frame.reticle.x, frame.reticle.y);
    const float radius = 21.0f * scale;
    const float thickness = (aligned ? 2.2f : 1.5f) * scale;
    // 明るい空でも読めるよう、発光ではなく細い暗色の下描きを使う
    const auto line = [&](ImVec2 from, ImVec2 to) {
        draw->AddLine(from, to, shadow, thickness + 2.0f);
        draw->AddLine(from, to, color, thickness);
    };
    // 中央を空けた四隅の照準　追尾機能を示す偽のロックオン演出は追加しない
    for (const float x : { -1.0f, 1.0f }) {
        for (const float y : { -1.0f, 1.0f }) {
            const ImVec2 corner(center.x + x * radius, center.y + y * radius);
            line(corner, { corner.x - x * 9.0f * scale, corner.y });
            line(corner, { corner.x, corner.y - y * 9.0f * scale });
        }
    }
    line({ center.x - 3.0f * scale, center.y }, { center.x + 3.0f * scale, center.y });
    line({ center.x, center.y - 3.0f * scale }, { center.x, center.y + 3.0f * scale });
    // 左右の二本が満ちると金色へ変わり、四隅の外に切っ先が出る
    const float charge = frame.charge;
    for (const float side : { -1.0f, 1.0f }) {
        const float x = center.x + side * 28.0f * scale;
        const float bottom = center.y + 18.0f * scale;
        draw->AddLine({ x, bottom }, { x, center.y - 18.0f * scale }, shadow, 5 * scale);
        if (charge > 0.0f) {
            draw->AddLine({ x, bottom }, { x, bottom - charge * 36.0f * scale },
                CombatHud::SurfaceColor(charged ? CombatHud::Gold : CombatHud::Energy), 2.5f * scale);
        }
        if (charged) {
            draw->AddLine({ x, center.y - 18.0f * scale },
                { x - side * 5.0f * scale, center.y - 23.0f * scale },
                CombatHud::SurfaceColor(CombatHud::Gold), 2 * scale);
        }
    }
    if (charged) {
        const ImU32 readyColor = CombatHud::SurfaceColor(CombatHud::Gold);
        for (const float side : { -1.0f, 1.0f }) {
            const ImVec2 tip(center.x, center.y + side * 30.0f * scale);
            draw->AddLine(tip, { center.x - 5 * scale, tip.y + side * 5 * scale }, readyColor, 2 * scale);
            draw->AddLine(tip, { center.x + 5 * scale, tip.y + side * 5 * scale }, readyColor, 2 * scale);
        }
    }
}

// 命中した座標の周辺に、命中の種類と残り時間に合った印を描く
void GameplayHud::DrawHitConfirm(ImDrawList* draw) const
{
    if (hitFrames_ <= 0) {
        return;
    }

    const float rate = std::clamp(
        static_cast<float>(hitFrames_) /
            static_cast<float>((std::max)(hitDuration_, 1)),
        0.0f,
        1.0f);
    const float elapsed = 1.0f - rate;
    const float settle = 1.0f - std::pow(1.0f - elapsed, 3.0f);
    const float fade = std::clamp(rate * 1.65f, 0.0f, 1.0f);
    const int alpha = static_cast<int>(255.0f * fade);
    const float radius =
        Lerp(32.0f * hitStrength_, 14.0f, settle) +
        (std::max)(elapsed - 0.72f, 0.0f) * 22.0f;
    const float lineLength = hitDestroyed_ ? 12.0f : 9.0f;
    const float thickness = hitDestroyed_ ? 3.0f : 2.2f;
    const ImVec2 center(hitScreen_.x, hitScreen_.y);

    const ImU32 mainColor = hitDestroyed_ || hitCharged_ ?
        IM_COL32(231, 189, 109, alpha) : IM_COL32(241, 240, 235, alpha);
    const int softAlpha = (std::clamp)(alpha / 3, 0, 96);
    const ImU32 softColor = IM_COL32(231, 189, 109, softAlpha);

    if (hitDestroyed_) {
        draw->AddNgon(
            center,
            radius * 0.58f,
            softColor,
            4,
            3.2f);
    }
    for (int xSign : { -1, 1 }) {
        for (int ySign : { -1, 1 }) {
            const ImVec2 inner(
                center.x + static_cast<float>(xSign) * radius,
                center.y + static_cast<float>(ySign) * radius);
            const ImVec2 outer(
                center.x + static_cast<float>(xSign) * (radius + lineLength),
                center.y + static_cast<float>(ySign) * (radius + lineLength));
            draw->AddLine(inner, outer, mainColor, thickness);
        }
    }
    draw->AddCircleFilled(center, hitDestroyed_ ? 3.2f : 2.4f, mainColor, 16);

}

// 被弾の位置と方向を使い、画面の光や線で被弾したことを示す
void GameplayHud::DrawDamage(const Frame& frame, ImDrawList* draw) const
{
    if (damageFrames_ <= 0) {
        return;
    }

    const float rate = std::clamp(
        static_cast<float>(damageFrames_) /
            static_cast<float>((std::max)(damageDuration_, 1)),
        0.0f,
        1.0f);
    const float elapsed = 1.0f - rate;
    const float pulse = rate * rate;
    const int mainAlpha = static_cast<int>(220.0f * pulse);
    const int edgeAlpha = static_cast<int>(112.0f * pulse);
    const auto& hudMin = frame.min;
    const auto& hudSize = frame.size;
    const ImVec2 viewportMin(hudMin.x, hudMin.y);
    const ImVec2 viewportMax(hudMin.x + hudSize.x, hudMin.y + hudSize.y);
    const ImVec2 viewportCenter(
        hudMin.x + hudSize.x * 0.5f,
        hudMin.y + hudSize.y * 0.5f);
    const float edgeWidth = 74.0f;
    const ImU32 edgeColor = IM_COL32(255, 32, 24, (std::clamp)(edgeAlpha, 0, 112));
    const ImU32 clearColor = IM_COL32(255, 32, 24, 0);

    draw->AddRectFilled(
        viewportMin,
        viewportMax,
        IM_COL32(132, 0, 8, (std::clamp)(static_cast<int>(20.0f * pulse), 0, 20)));
    if (damageDirection_.x < -0.18f) {
        draw->AddRectFilledMultiColor(
            viewportMin,
            ImVec2(viewportMin.x + edgeWidth, viewportMax.y),
            edgeColor, clearColor, clearColor, edgeColor);
    } else if (damageDirection_.x > 0.18f) {
        draw->AddRectFilledMultiColor(
            ImVec2(viewportMax.x - edgeWidth, viewportMin.y),
            viewportMax,
            clearColor, edgeColor, edgeColor, clearColor);
    }
    if (damageDirection_.y < -0.18f) {
        draw->AddRectFilledMultiColor(
            viewportMin,
            ImVec2(viewportMax.x, viewportMin.y + edgeWidth),
            edgeColor, edgeColor, clearColor, clearColor);
    } else if (damageDirection_.y > 0.18f) {
        draw->AddRectFilledMultiColor(
            ImVec2(viewportMin.x, viewportMax.y - edgeWidth),
            viewportMax,
            clearColor, clearColor, edgeColor, edgeColor);
    }

    const ImVec2 impactCenter(damageScreen_.x, damageScreen_.y);
    const float impactRadius = 18.0f + elapsed * 18.0f;
    const ImU32 impactColor =
        IM_COL32(255, 116, 72, (std::clamp)(mainAlpha, 0, 220));
    for (int xSign : { -1, 1 }) {
        for (int ySign : { -1, 1 }) {
            const ImVec2 corner(
                impactCenter.x + static_cast<float>(xSign) * impactRadius,
                impactCenter.y + static_cast<float>(ySign) * impactRadius);
            const float segment = 9.0f + 5.0f * rate;
            draw->AddLine(
                corner,
                ImVec2(corner.x - static_cast<float>(xSign) * segment, corner.y),
                impactColor,
                2.6f);
            draw->AddLine(
                corner,
                ImVec2(corner.x, corner.y - static_cast<float>(ySign) * segment),
                impactColor,
                2.6f);
        }
    }
    for (int index = 0; index < 4; ++index) {
        const float angle =
            kTwoPi * static_cast<float>(index) / 4.0f + 0.785398f;
        const ImVec2 direction(std::cos(angle), std::sin(angle));
        const float innerRadius = impactRadius + 3.0f;
        const float outerRadius = innerRadius + 8.0f + 7.0f * rate;
        draw->AddLine(
            ImVec2(
                impactCenter.x + direction.x * innerRadius,
                impactCenter.y + direction.y * innerRadius),
            ImVec2(
                impactCenter.x + direction.x * outerRadius,
                impactCenter.y + direction.y * outerRadius),
            impactColor,
            2.0f);
    }

    const ImVec2 direction(
        damageDirection_.x,
        damageDirection_.y);
    const ImVec2 tangent(-direction.y, direction.x);
    const float markerMinX = viewportMin.x + 38.0f;
    const float markerMaxX = viewportMax.x - 38.0f;
    const float markerMinY = viewportMin.y + 146.0f;
    const float markerMaxY = viewportMax.y - 54.0f;
    const float edgeScaleX = direction.x > 0.001f ?
        (markerMaxX - viewportCenter.x) / direction.x :
        direction.x < -0.001f ?
            (markerMinX - viewportCenter.x) / direction.x : 100000.0f;
    const float edgeScaleY = direction.y > 0.001f ?
        (markerMaxY - viewportCenter.y) / direction.y :
        direction.y < -0.001f ?
            (markerMinY - viewportCenter.y) / direction.y : 100000.0f;
    const float edgeScale = (std::min)(edgeScaleX, edgeScaleY);
    const ImVec2 sourceMarker(
        viewportCenter.x + direction.x * edgeScale,
        viewportCenter.y + direction.y * edgeScale);
    for (int index = 0; index < 2; ++index) {
        const float offset = static_cast<float>(index) * 10.0f;
        const ImVec2 tip(
            sourceMarker.x - direction.x * offset,
            sourceMarker.y - direction.y * offset);
        draw->AddLine(
            ImVec2(
                tip.x + direction.x * 13.0f + tangent.x * 9.0f,
                tip.y + direction.y * 13.0f + tangent.y * 9.0f),
            tip,
            impactColor,
            3.0f);
        draw->AddLine(
            tip,
            ImVec2(
                tip.x + direction.x * 13.0f - tangent.x * 9.0f,
                tip.y + direction.y * 13.0f - tangent.y * 9.0f),
            impactColor,
            3.0f);
    }

}

// 現在のチェイン数と倍率、次の撃破までの猶予を表示する
void GameplayHud::DrawChain(const Frame& frame, ImDrawList* draw) const
{
    const bool isChainActive = frame.chain.count >= 2;
    const bool isBreakNoticeVisible =
        !isChainActive && frame.chain.breakFlash > 0;
    if (!isChainActive && !isBreakNoticeVisible) {
        return;
    }

    const auto& hudMin = frame.min;
    const auto& hudSize = frame.size;
    const float hudScale = CombatHud::Scale(hudSize);
    const CombatHud::Layout layout(hudMin, hudSize);
    const ImVec2 panelMin = layout.chain;
    const ImVec2 panelMax(panelMin.x + 272.0f * hudScale, panelMin.y + 90.0f * hudScale);
    CombatHud::Wake(draw, { panelMin.x - 48 * hudScale, panelMin.y - 8 * hudScale },
        { panelMax.x + 32 * hudScale, panelMax.y + 28 * hudScale }, true);
    ImU32 accentColor = CombatHud::GaugeGold;
    if (isBreakNoticeVisible) {
        const int alpha = static_cast<int>(
            100.0f + 145.0f *
            frame.chain.breakFlash);
        accentColor = IM_COL32(255, 96, 116, (std::clamp)(alpha, 0, 255));
    }

    if (isBreakNoticeVisible) {
        CombatHud::Readout(draw, ImVec2(panelMax.x, panelMin.y + 20 * hudScale),
            20.0f * hudScale, accentColor, "連続撃破終了", true);
        return;
    }

    const int totalScoreMultiplier =
        frame.chain.multiplier;
    char chainText[32]{};
    char multiplierText[32]{};
    std::snprintf(chainText, sizeof(chainText), "%d 連続撃破", frame.chain.count);
    std::snprintf(multiplierText, sizeof(multiplierText), "×%d", totalScoreMultiplier);
    CombatHud::Readout(draw, ImVec2(panelMin.x, panelMin.y + 45.0f * hudScale),
        22.0f * hudScale, CombatHud::White, chainText);
    if (totalScoreMultiplier > 1) {
        const float punch = frame.chain.punch;
        CombatHud::Slant(draw, { panelMax.x, panelMin.y - punch * 4 * hudScale },
            (64.0f + punch * 4.0f) * hudScale, accentColor, multiplierText, true);
    }

    const ImVec2 timerMin(panelMin.x, panelMax.y - 5 * hudScale);
    const float timerRate = frame.chain.remaining;
    CombatHud::WingMeter(draw, timerMin, 272 * hudScale, 4 * hudScale, timerRate, accentColor, hudScale);
}

// フィーバーのゲージと発動中の残り時間を、その時点の状態に合わせて表示する
void GameplayHud::DrawFever(const Frame& frame, ImDrawList* draw) const
{
    const auto& hudMin = frame.min;
    const auto& hudSize = frame.size;
    const ImVec2 origin(hudMin.x, hudMin.y);
    const ImVec2 drawSize(hudSize.x, hudSize.y);
    const bool isActive = frame.fever.active;
    const bool isReady = frame.fever.ready;
    const float rate = frame.fever.rate;
    const float pulse = 0.5f + 0.5f * std::sin(frame.clock * (isActive ? 8.5f : 5.0f));
    const float rainbowTime = frame.clock * (isActive ? 0.010f : 0.006f);
    const auto rainbowColor = [rainbowTime](float offset, int alpha) {
        float hue = std::fmod(rainbowTime + offset, 1.0f);
        if (hue < 0.0f) {
            hue += 1.0f;
        }
        float red = 1.0f;
        float green = 1.0f;
        float blue = 1.0f;
        ImGui::ColorConvertHSVtoRGB(hue, 0.78f, 1.0f, red, green, blue);
        return IM_COL32(
            static_cast<int>(red * 255.0f),
            static_cast<int>(green * 255.0f),
            static_cast<int>(blue * 255.0f),
            (std::clamp)(alpha, 0, 255));
    };
    const auto rainbowReadout = [&](ImVec2 position, float size, int alpha, const char* text) {
        position.x = std::round(position.x);
        position.y = std::round(position.y);
        alpha = std::clamp(alpha, 0, 255);
        const float width = (std::max)(CombatHud::ReadoutWidth(text, size, true), 1.0f);
        // 影は暗色のまま残し、文字の頂点だけをゲージと同じ流れる虹色にする
        draw->AddText(CombatHud::HeadingFont(), size, { position.x, position.y + 1.0f },
            IM_COL32(0, 0, 0, alpha * 4 / 5), text);
        const int firstVertex = draw->VtxBuffer.Size;
        draw->AddText(CombatHud::HeadingFont(), size, position, IM_COL32(255, 255, 255, alpha), text);
        for (int index = firstVertex; index < draw->VtxBuffer.Size; ++index) {
            ImDrawVert& vertex = draw->VtxBuffer[index];
            const float offset = std::clamp((vertex.pos.x - position.x) / width, 0.0f, 1.0f);
            vertex.col = CombatHud::SurfaceColor(rainbowColor(offset, alpha));
        }
    };

    const float hudScale = CombatHud::Scale(hudSize);
    const CombatHud::Layout layout(hudMin, hudSize);
    const ImVec2 anchor = layout.fever;
    const auto p = [&](float x, float y) {
        return ImVec2(anchor.x + x * hudScale, anchor.y + y * hudScale);
    };
    CombatHud::Wake(draw, p(-32, -20), p(296, 116));
    const bool ending = isActive && frame.fever.seconds <= 2.0f;
    const ImU32 feverColor = ending ? CombatHud::Danger :
        isActive || isReady || rate >= 0.80f ? CombatHud::GaugeGold :
        rate >= 0.50f ? CombatHud::FeverCharge :
        rate > 0.0f ? CombatHud::White : CombatHud::Muted;
    CombatHud::Slant(draw, p(0, 0), 34 * hudScale, feverColor, "FEVER");
    CombatHud::WingMeter(draw, p(0, 58), 224 * hudScale, 10 * hudScale, rate,
        feverColor, hudScale, isActive ? std::fmod(frame.clock * 0.7f, 1.0f) : -1.0f,
        (isActive && !ending) || isReady ? frame.clock * 0.08f : -1.0f);
    if (isActive) {
        char status[32]{};
        std::snprintf(status, sizeof(status), "%.1f秒", frame.fever.seconds);
        CombatHud::Readout(draw, p(224, 14), 24.0f * hudScale, feverColor, status, true);
    } else {
        char status[16]{};
        std::snprintf(status, sizeof(status), "%d%%", static_cast<int>(std::clamp(rate, 0.0f, 1.0f) * 100));
        CombatHud::Slant(draw, p(224, 6), 28.0f * hudScale, feverColor, status, true);
    }
    if (isActive) {
        const int edgeAlpha = static_cast<int>(45.0f + pulse * 38.0f);
        constexpr int kHorizontalEdgeSegments = 16;
        constexpr int kVerticalEdgeSegments = 9;
        const float edgeLeft = origin.x + 7.0f;
        const float edgeRight = origin.x + drawSize.x - 7.0f;
        const float edgeTop = origin.y + 7.0f;
        const float edgeBottom = origin.y + drawSize.y - 7.0f;
        for (int segment = 0; segment < kHorizontalEdgeSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) / static_cast<float>(kHorizontalEdgeSegments);
            const float rate1 =
                static_cast<float>(segment + 1) / static_cast<float>(kHorizontalEdgeSegments);
            const float x0 = Lerp(edgeLeft, edgeRight, rate0);
            const float x1 = Lerp(edgeLeft, edgeRight, rate1);
            draw->AddLine(
                ImVec2(x0, edgeTop),
                ImVec2(x1 + 1.0f, edgeTop),
                rainbowColor(rate0 * 0.76f, edgeAlpha),
                4.0f);
            draw->AddLine(
                ImVec2(x0, edgeBottom),
                ImVec2(x1 + 1.0f, edgeBottom),
                rainbowColor(0.76f - rate0 * 0.76f, edgeAlpha),
                4.0f);
        }
        for (int segment = 0; segment < kVerticalEdgeSegments; ++segment) {
            const float rate0 =
                static_cast<float>(segment) / static_cast<float>(kVerticalEdgeSegments);
            const float rate1 =
                static_cast<float>(segment + 1) / static_cast<float>(kVerticalEdgeSegments);
            const float y0 = Lerp(edgeTop, edgeBottom, rate0);
            const float y1 = Lerp(edgeTop, edgeBottom, rate1);
            draw->AddLine(
                ImVec2(edgeLeft, y0),
                ImVec2(edgeLeft, y1 + 1.0f),
                rainbowColor(0.16f + rate0 * 0.48f, edgeAlpha),
                4.0f);
            draw->AddLine(
                ImVec2(edgeRight, y0),
                ImVec2(edgeRight, y1 + 1.0f),
                rainbowColor(0.92f - rate0 * 0.48f, edgeAlpha),
                4.0f);
        }
        draw->AddRect(
            ImVec2(origin.x + 12.0f, origin.y + 12.0f),
            ImVec2(origin.x + drawSize.x - 12.0f, origin.y + drawSize.y - 12.0f),
            IM_COL32(255, 255, 255, edgeAlpha / 3),
            0.0f,
            0,
            1.5f);
    }

    // 練習中は課題カードが発動を伝える　中央の大見出しを重ねて案内を隠さない
    if (frame.fever.activationFlash > 0.0f && !frame.tutorial) {
        const float remaining = frame.fever.activationFlash;
        const float elapsed = 1.0f - remaining;
        const float visibility = std::clamp(elapsed / 0.12f, 0.0f, 1.0f) *
            std::clamp(remaining / 0.28f, 0.0f, 1.0f);
        const float fontSize = (44.0f + 8.0f * (1.0f - elapsed)) * hudScale;
        const char* title = "フィーバー";
        const float titleWidth = CombatHud::ReadoutWidth(title, fontSize, true);
        const ImVec2 center(origin.x + drawSize.x * 0.5f,
            origin.y + drawSize.y * 0.23f - elapsed * 8.0f * hudScale);
        rainbowReadout(
            { center.x - titleWidth * 0.5f, center.y },
            fontSize, static_cast<int>(255.0f * visibility), title);
    }
}

// スキルの再使用までの時間と、発動可能・対象なしなどの状態を表示する
void GameplayHud::DrawSkill(const Frame& frame, ImDrawList* draw) const
{
    const auto& min = frame.min;
    const auto& size = frame.size;
    const CombatHud::Layout layout(min, size);
    const float hudScale = layout.scale;
    const ImVec2 panel = layout.skill;
    const bool active = frame.skill.active;
    const auto p = [&](float x, float y) { return ImVec2(panel.x + x * hudScale, panel.y + y * hudScale); };
    const bool lessonLocked = frame.skill.locked;
    const bool ready = !lessonLocked && (frame.skill.ready || active);
    const float readyBurst = frame.skill.readyFlash;
    const bool empowered = frame.skill.empowered;
    const ImU32 hudAccent = lessonLocked || !ready ? CombatHud::Muted :
        empowered ? CombatHud::GaugeGold : CombatHud::Energy;
    CombatHud::BladeIcon(draw, p(12, 15), 0.70f * hudScale,
        CombatHud::SurfaceColor(hudAccent));
    const bool noTarget = frame.skill.noTarget;
    CombatHud::Heading(draw, p(38, 0), 22.0f * hudScale,
        noTarget ? CombatHud::Danger : hudAccent,
        noTarget ? "対象なし" : "スキル");
    CombatHud::KeyBadge(draw, p(212, -1), p(244, 27), hudScale, "Q", ready, hudAccent);
    if (!noTarget && !lessonLocked && !active && !frame.skill.ready && frame.skill.cooldownSeconds > 0.0f) {
        char cooldown[24]{};
        std::snprintf(cooldown, sizeof(cooldown), "%.1f", static_cast<double>(frame.skill.cooldownSeconds));
        CombatHud::Number(draw, p(194, 2), 22.0f * hudScale, CombatHud::Muted, cooldown, true);
    }
    const float recovery = lessonLocked ? 0.0f : (ready ? 1.0f : frame.skill.recovery);
    CombatHud::FlightMeter(draw, p(38, 33), 206 * hudScale, 7 * hudScale, recovery,
        ready ? hudAccent : CombatHud::Energy, hudScale);
    if (readyBurst > 0.0f) {
        draw->AddQuadFilled(p(38, 30), p(244, 30), p(238, 43), p(38, 43),
            CombatHud::SurfaceColor(IM_COL32(239, 241, 244, static_cast<int>(80 * readyBurst * readyBurst))));
    }
}

// ボス接近の通信を、表示時間と画面の空き領域に合わせて描く
void GameplayHud::DrawRadio(const Frame& frame, ImDrawList* draw) const
{
    if (frame.bossRadioSeconds <= 0.0f) { return; }
    const char* message = "正面に重装迎撃機。進路を塞いでる。";
    const float elapsed = frame.bossRadioElapsed;
    const float entry = std::clamp(elapsed / 0.18f, 0.0f, 1.0f);
    const float ease = 1.0f - std::pow(1.0f - entry, 3.0f);
    const float fade = entry * std::clamp(frame.bossRadioSeconds / 0.30f, 0.0f, 1.0f);
    const ImU32 accent = IM_COL32(237, 100, 88, static_cast<int>(255 * fade));
    const auto& min = frame.min;
    const auto& size = frame.size;
    const CombatHud::Layout layout(min, size);
    const float scale = layout.scale;
    const float messageSize = 26.0f * scale;
    const float width = 112.0f * scale + CombatHud::ReadoutWidth(message, messageSize);
    const ImVec2 anchor{ layout.radio.x - width * 0.5f,
        layout.radio.y + 8.0f * (1.0f - ease) * scale };
    const auto p = [&](float x, float y) { return ImVec2(anchor.x + x * scale, anchor.y + y * scale); };
    // 一枚の字幕帯　警告は最初から全文を読め、送信者と本文を同じ行で追える
    const float unscaledWidth = width / scale;
    const ImVec2 band[] = { p(-12, -4), p(unscaledWidth + 14, -4),
        p(unscaledWidth + 6, 44), p(-20, 44) };
    draw->AddConvexPolyFilled(band, 4,
        CombatHud::SurfaceColor(IM_COL32(8, 15, 26, static_cast<int>(207 * fade))));
    draw->AddQuadFilled(p(-12, -4), p(-8, -4), p(-16, 44), p(-20, 44),
        CombatHud::SurfaceColor(accent));
    // 通信マークは固定の送受信記号　架空の音声波形や数値は表示しない
    for (float radius : { 5.0f, 10.0f }) {
        draw->PathArcTo(p(8, 20), radius * scale, -0.9f, 0.9f, 10);
        draw->PathStroke(CombatHud::SurfaceColor(accent), 0, 1.5f * scale);
    }
    CombatHud::Heading(draw, p(30, 12), 22.0f * scale, accent, "管制");
    CombatHud::Readout(draw, p(98, 7), messageSize,
        IM_COL32(239, 241, 244, static_cast<int>(255 * fade)), message);
}

// 照準、命中、被弾などの戦闘中の表示を重ねる
void GameplayHud::DrawCombatFeedback(const Frame& frame, ImDrawList* draw) const
{
    DrawStageCue(frame, draw);
    DrawReticle(frame, draw);
    DrawHitConfirm(draw);
    DrawDamage(frame, draw);
}

// 機体の状態、進行、フィーバーやスキルの情報を画面へ描く
void GameplayHud::DrawStatus(const Frame& frame, ImDrawList* draw) const
{
    if (!frame.tutorial) { DrawChain(frame, draw); }
    DrawFever(frame, draw);
    DrawSkill(frame, draw);
    DrawRadio(frame, draw);
}

#ifdef _DEBUG
GameplayHud::Snapshot GameplayHud::Inspect() const
{
    return { hitFrames_, hitDuration_, damageFrames_, damageDuration_, chargeFlash_,
        hitScreen_, damageScreen_, damageDirection_ };
}
#endif
