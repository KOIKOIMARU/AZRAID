// ===== TutorialProgress.cpp =====
// 課題ごとの達成条件と計測値を持ち、練習開始・成功・やり直しのタイミングを決める

#include "app/stage/TutorialProgress.h"

#include "app/ui/CombatHud.h"
#include "engine/3d/ScreenProjection.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef _DEBUG
#include <stdexcept>
#endif

namespace {
constexpr std::array<const char*, TutorialProgress::kLessonCount> kLessonNames{
    "移動", "射撃", "回避", "チャージショット", "スキル", "リング", "フィーバー"
};
constexpr std::array<Math::Vector2, 3> kMoveTargets{ Math::Vector2{-4.0f, 2.0f}, {4.0f, 3.2f}, {0.0f, 0.4f} };
constexpr float kMarkerHoldFrames = 8.0f;
constexpr int kDodgeTelegraphLeadFrames = 34;
}

void TutorialProgress::Reset()
{
    *this = {};
}

// 課題開始時の命中数などを控え、この課題中に増えた分で成功を判定できるようにする
TutorialProgress::Setup TutorialProgress::BeginLesson(Lesson lesson, Stats stats)
{
    Reset();
    started_ = true;
    lesson_ = lesson;
    start_ = stats;
    switch (lesson_) {
    case Lesson::Shoot:
    case Lesson::Charge:
        return { .targets = { 2, 3 } };
    case Lesson::Dodge:
        return { .targets = { 1, 3 } };
    case Lesson::Skill:
        return { .targets = { 3, 3 }, .prepareSkill = true };
    case Lesson::Ring:
        return { .feverGauge = 40, .spawnRing = true };
    case Lesson::Fever:
        // 満タン手前から始め、命中・撃破による本編と同じ自動発動を練習する
        return { .targets = { 3, 2 }, .feverGauge = 65 };
    default:
        return {};
    }
}

// 練習中の入力と戦闘結果を調べ、課題の成功や次の課題への移行に必要な通知を返す
TutorialProgress::Events TutorialProgress::Advance(float frameStep, const Frame& frame)
{
    Events events;
    elapsed_ += frameStep;
    if (ShowingSuccess()) {
        success_ = (std::max)(0.0f, success_ - frameStep);
        if (!ShowingSuccess()) {
            events.nextLesson = static_cast<Lesson>(static_cast<int>(lesson_) + 1);
        }
        return events;
    }

    const auto complete = [&] {
        success_ = 90.0f;
        events.clearedLesson = true;
    };
    switch (lesson_) {
    case Lesson::Move: {
        const auto target = MoveTarget(frame.playerPosition.z);
        const float dx = frame.playerPosition.x - target.x;
        const float dy = frame.playerPosition.y - target.y;
        markerHold_ = dx * dx + dy * dy < 1.0f ? markerHold_ + frameStep : 0.0f;
        if (markerHold_ >= kMarkerHoldFrames) {
            ++progress_;
            markerHold_ = 0.0f;
            events.reachedMarker = true;
            if (progress_ >= static_cast<int>(kMoveTargets.size())) { complete(); }
        }
        break;
    }
    case Lesson::Shoot:
        progress_ = frame.stats.defeats - start_.defeats;
        if (progress_ >= 2) { complete(); }
        else if (!frame.targetsAlive) { events.targets = { 2 - progress_, 3 }; }
        break;
    case Lesson::Dodge:
        if (dodgeConfirmed_) { progress_ = 1; complete(); }
        else if (!frame.targetsAlive) { events.targets = { 1, 3 }; }
        break;
    case Lesson::Charge:
        if (chargedHit_) { progress_ = 1; complete(); }
        else if (!frame.targetsAlive) { events.targets = { 2, 3 }; }
        break;
    case Lesson::Skill:
        if (frame.stats.skillUses > start_.skillUses && frame.stats.skillDefeats > start_.skillDefeats &&
            !frame.skillActive) {
            progress_ = 1;
            complete();
        } else if (!frame.targetsAlive && !frame.skillActive) { events.targets = { 3, 3 }; }
        break;
    case Lesson::Ring:
        progress_ = frame.stats.ringPasses - start_.ringPasses;
        if (progress_ > 0) { complete(); }
        else if (!frame.targetsAlive && !frame.ringActive) { events.respawnRing = true; }
        break;
    case Lesson::Fever:
        if (frame.feverActive) {
            feverExperience_ += frameStep;
            progress_ += frame.stats.defeats - start_.defeats;
        }
        // 発動が切れても体験済みの撃破数は残す　次の発動で続きを練習できる
        start_.defeats = frame.stats.defeats;
        if (progress_ >= 3 && feverExperience_ >= 120.0f && !frame.skillActive) { complete(); }
        if (!frame.targetsAlive && !ShowingSuccess() && !frame.skillActive) { events.targets = { 3, 2 }; }
        break;
    case Lesson::Complete:
        break;
    }
    return events;
}

// 練習弾までの待ち時間を進める　画面の予告にも残り時間を渡し、発射時刻にtrueを返す
bool TutorialProgress::AdvanceDodgeAttack(float frameStep)
{
    attackTimer_ -= frameStep;
    attackTelegraph_ = static_cast<int>((std::max)(0.0f, attackTimer_));
    return attackTimer_ <= 0.0f;
}

// 練習弾を撃った後、次の練習弾までの待ち時間を設定する
void TutorialProgress::OnDodgeShotFired()
{
    attackTimer_ = 150.0f;
}

// 回避した位置で弾が十分近ければ、この練習課題の回避を成功として記録する
void TutorialProgress::ObserveDodge(const Math::Vector3& player, const Math::Vector3& bullet)
{
    // 空振りのSHIFTでは進めない　実際の弾が近ければジャスト判定より広く認める
    if (bullet.z - player.z >= -1.0f && bullet.z - player.z < 11.0f &&
        std::abs(bullet.x - player.x) < 6.0f && std::abs(bullet.y - player.y) < 2.5f) {
        dodgeConfirmed_ = true;
    }
}

// 自機の前方近くにある弾かを距離と横・縦のずれで判定し、回避案内に使う
bool TutorialProgress::IsDodgePromptShot(const Math::Vector3& player, const Math::Vector3& bullet)
{
    return bullet.z > player.z && bullet.z < player.z + 16.0f &&
        std::abs(bullet.x - player.x) < 5.0f && std::abs(bullet.y - player.y) < 2.5f;
}

// 練習用に、チャージ弾が直撃した回数を記録する
void TutorialProgress::RecordChargedHit()
{
    if (lesson_ == Lesson::Charge) { chargedHit_ = true; }
}

bool TutorialProgress::IsDodgeTelegraphActive() const
{
    return attackTelegraph_ > 0 && attackTelegraph_ <= kDodgeTelegraphLeadFrames;
}

float TutorialProgress::DodgeTelegraphRate() const
{
    return IsDodgeTelegraphActive() ?
        1.0f - static_cast<float>(attackTelegraph_) / kDodgeTelegraphLeadFrames : 0.0f;
}

bool TutorialProgress::AllowsShooting() const
{
    return started_ && !ShowingSuccess() &&
        (lesson_ == Lesson::Shoot || lesson_ == Lesson::Charge || lesson_ == Lesson::Skill ||
            lesson_ == Lesson::Ring || lesson_ == Lesson::Fever);
}

bool TutorialProgress::AllowsSkill() const
{
    return !ShowingSuccess() && (lesson_ == Lesson::Skill || AllowsFever());
}

bool TutorialProgress::AllowsRing() const
{
    return started_ && lesson_ == Lesson::Ring && !ShowingSuccess();
}

bool TutorialProgress::AllowsFever() const
{
    return lesson_ == Lesson::Ring || lesson_ == Lesson::Fever;
}

Math::Vector3 TutorialProgress::MoveTarget(float railDistance) const
{
    const auto target = kMoveTargets[std::clamp(progress_, 0, static_cast<int>(kMoveTargets.size()) - 1)];
    return { target.x, target.y, railDistance };
}

const std::array<const char*, TutorialProgress::kLessonCount>& TutorialProgress::LessonNames()
{
    return kLessonNames;
}

// 現在の課題名、操作、達成状況を、ゲーム内のガイド領域へ表示する
void TutorialProgress::DrawGuide(ImDrawList& list, const Math::Vector2& min, const Math::Vector2& size,
    const Math::Matrix4x4* viewProjection, float railDistance, const Guide& guide) const
{
    const float s = CombatHud::Scale(size);
    const int lesson = std::clamp(static_cast<int>(lesson_), 0, kLessonCount - 1);
    const bool success = ShowingSuccess();
    const bool goldAccent = lesson_ == Lesson::Ring || lesson_ == Lesson::Fever ||
        (lesson_ == Lesson::Charge && guide.chargeReady);
    const ImU32 accent = success ? CombatHud::Health : goldAccent ? CombatHud::GaugeGold : CombatHud::Blue;
    auto* draw = &list;
    draw->PushClipRect({ min.x, min.y }, { min.x + size.x, min.y + size.y }, true);
    // 中央の照準と機体を隠さない、一段の操作案内　色は本編の計器と揃える
    const ImVec2 start{ min.x + (size.x - 800.0f * s) * 0.5f, min.y + 28.0f * s };
    const auto p = [&](float x, float y) { return ImVec2(start.x + x * s, start.y + y * s); };
    draw->AddRectFilled(start, p(800, 138), CombatHud::SurfaceColor(IM_COL32(12, 16, 22, 225)));
    draw->AddRectFilled(start, p(3, 138), CombatHud::SurfaceColor(accent));
    char stage[32]{};
    std::snprintf(stage, sizeof(stage), "%02d / %02d", lesson + 1, kLessonCount);
    CombatHud::Number(draw, p(24, 17), 18 * s, CombatHud::Muted, stage);
    CombatHud::Heading(draw, p(130, 15), 20 * s, accent, kLessonNames[lesson]);
    char progress[128]{};
    const char* key = "WASD / 矢印";
    const char* action = "輪の中心へ機体を動かす";
    const char* hint = "前進は自動。上下左右に移動して、白い輪に重なる。";
    std::snprintf(progress, sizeof(progress), "%d / 3 地点", (std::min)(progress_, 3));
    switch (lesson_) {
    case Lesson::Move: break;
    case Lesson::Shoot:
        key = "SPACE";
        action = "敵を狙って2機倒す";
        hint = "マウスで照準を動かし、SPACEを押し続けて連射する。";
        std::snprintf(progress, sizeof(progress), "%d / 2 機撃破", (std::min)(progress_, 2));
        break;
    case Lesson::Dodge: {
        key = "A / D + SHIFT";
        action = guide.incomingShot ? "SHIFTで横に回避" : "弾を引きつけてから回避する";
        hint = "AかDを押しながらSHIFT。直前に避けるとスローになり、チャージも回復。";
        std::snprintf(progress, sizeof(progress), "%d / 1 回成功", progress_);
        break;
    }
    case Lesson::Charge:
        key = "SPACE";
        action = guide.chargeReady ? "敵にチャージショットを当てる" : "SPACEを離してチャージ";
        hint = "撃たずに待つとゲージがたまる。満タンで撃つと、爆風が広がる強い弾になる。";
        std::snprintf(progress, sizeof(progress), "%d / 1 回命中", progress_);
        break;
    case Lesson::Skill:
        key = "Q";
        action = guide.skillActive ? "スキル発動中" : guide.skillReady ? "Qでスキルを使って倒す" : "ゲージが戻るまで待つ";
        hint = "画面内の敵を最大3機まで自動攻撃。射撃を当てると再使用までの時間が縮まる。";
        std::snprintf(progress, sizeof(progress), "%d / 1 回成功", progress_);
        break;
    case Lesson::Ring: {
        key = "WASD / 矢印";
        action = guide.ringActive ? "リングの中心に合わせて通る" : "次のリングが前方に出現";
        hint = "通ると加速し、チャージとゲージが回復。フィーバー中は残り時間も延びる。";
        std::snprintf(progress, sizeof(progress), "%d / 1 回通過", progress_);
        break;
    }
    case Lesson::Fever:
        key = "SPACE";
        if (guide.feverActive) {
            action = "フィーバー中に3機倒す";
            hint = "弾が敵を貫通し、得点は3倍。スキルの対象も最大5機に増える。";
        } else {
            action = "敵を倒してゲージをためる";
            hint = "左下のゲージが満タンになると自動発動。撃破してためていく。";
        }
        std::snprintf(progress, sizeof(progress), "%d / 3 機撃破", (std::min)(progress_, 3));
        break;
    case Lesson::Complete: break;
    }
    if (success) {
        key = "完了";
        action = "クリア";
        hint = "";
        std::snprintf(progress, sizeof(progress), lesson + 1 < kLessonCount ? "次は %s" : "%s",
            lesson + 1 < kLessonCount ? kLessonNames[lesson + 1] : "練習終了");
    }
    const float keyWidth = (std::max)(150.0f, CombatHud::ReadoutWidth(key, 18.0f) + 28.0f);
#ifdef _DEBUG
    // 翻訳や文面の追加で、キー表示・説明・枠が重なったら実画面の試験で検出する
    if (CombatHud::ReadoutWidth(action, 26 * s) > (736.0f - keyWidth) * s ||
        CombatHud::ReadoutWidth(hint, 18 * s) > 752 * s ||
        CombatHud::ReadoutWidth(progress, 18 * s) > 300 * s || keyWidth > 230.0f) {
        throw std::runtime_error("Tutorial guide text exceeds its layout");
    }
#endif
    CombatHud::Readout(draw, p(776, 17), 18 * s, success ? accent : CombatHud::Muted, progress, true);
    CombatHud::Readout(draw, p(24, 52), 26 * s, success ? accent : CombatHud::White, action);
    CombatHud::KeyBadge(draw, p(776 - keyWidth, 49), p(776, 85), s, key, true, accent);
    CombatHud::Readout(draw, p(24, 104), 18 * s, CombatHud::Muted, hint);
    // 説明している計器だけを同系色で示す　未習得の計器は通常の位置のまま控えめにする
    const CombatHud::Layout layout(min, size);
    if (lesson_ == Lesson::Charge || lesson_ == Lesson::Skill ||
        lesson_ == Lesson::Fever) {
        const ImVec2 anchor = lesson_ == Lesson::Charge ? layout.charge :
            lesson_ == Lesson::Skill ? layout.skill : layout.fever;
        const float height = lesson_ == Lesson::Fever ? 82.0f : 54.0f;
        draw->AddRect({ anchor.x - 12 * s, anchor.y - 12 * s },
            { anchor.x + 256 * s, anchor.y + height * s }, CombatHud::SurfaceColor(accent), 3 * s, 0, s);
    }
    const char* footer = "練習中は撃墜されません    ESC：ポーズ";
    CombatHud::Readout(draw, { min.x + (size.x - CombatHud::ReadoutWidth(footer, 17 * s)) * 0.5f,
        min.y + size.y - 26 * s }, 17 * s, CombatHud::White, footer);
    if (lesson_ == Lesson::Move && !success) {
        Math::Vector2 screen{};
        if (viewProjection && ScreenProjection::ToViewport(MoveTarget(railDistance), *viewProjection, min, size, screen)) {
            const float radius = 31.0f * s;
            const ImVec2 center{ screen.x, screen.y };
            draw->AddCircleFilled(center, radius, IM_COL32(10, 15, 18, 95), 48);
            draw->AddCircle(center, radius, CombatHud::SurfaceColor(CombatHud::White), 48, 2.0f * s);
            const float rate = std::clamp(markerHold_ / kMarkerHoldFrames, 0.0f, 1.0f);
            if (rate > 0.0f) {
                draw->PathArcTo(center, radius + 5.0f * s, -1.5708f, -1.5708f + rate * 6.2832f, 40);
                draw->PathStroke(CombatHud::SurfaceColor(CombatHud::Health), 0, 3.0f * s);
            }
            char count[16]{};
            std::snprintf(count, sizeof(count), "%d / 3", progress_ + 1);
            CombatHud::Readout(draw, { center.x - 22 * s, center.y + 38 * s }, 16 * s, CombatHud::White, count);
        }
    }
    draw->PopClipRect();
}
