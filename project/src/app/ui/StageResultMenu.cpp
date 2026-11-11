// ===== StageResultMenu.cpp =====
// クリア・敗北・チュートリアルの結果画面を描き、項目の選択を受け付ける。

#include "app/ui/StageResultMenu.h"

#include "app/ui/CombatHud.h"
#include "app/ui/GameplayMenu.h"
#include "app/ui/MenuUi.h"
#include "app/ui/StageRecords.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>

namespace {
using Action = GameplayMenu::Action;

bool PreviousPressed(Input* input)
{
    return MenuUi::Pressed(input, DIK_LEFT) || MenuUi::Pressed(input, DIK_A) ||
        MenuUi::Pressed(input, DIK_UP) || MenuUi::Pressed(input, DIK_W);
}

bool NextPressed(Input* input)
{
    return MenuUi::Pressed(input, DIK_RIGHT) || MenuUi::Pressed(input, DIK_D) ||
        MenuUi::Pressed(input, DIK_DOWN) || MenuUi::Pressed(input, DIK_S);
}

float EaseInOut(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}

void StageResultMenu::ResetDisplay()
{
    kind_ = Kind::Hidden;
    started_ = -1.0;
}

// リザルトの種類が変わった時点で表示の時計を始め、出現演出に使う経過秒数を返す。
float StageResultMenu::BeginDisplay(Kind kind)
{
    const double now = ImGui::GetTime();
    if (kind_ != kind) {
        kind_ = kind;
        started_ = now;
        details_ = false;
        defeatActionEmphasis_ = {};
    }
    return static_cast<float>(now - started_);
}

float StageResultMenu::UpdateClear()
{
    return BeginDisplay(Kind::Clear);
}

// クリアの得点、ランク、ベスト記録を描き、詳細表示と次に行う操作の入力を処理する。
void StageResultMenu::DrawClear(Input* input, const Math::Vector2& hudMin, const Math::Vector2& hudSize,
    const Stats& stats, float elapsed, GameplayMenu& menu)
{
    const float scale = CombatHud::Scale(hudSize);
    const ImVec2 origin{ hudMin.x, hudMin.y };
    const ImVec2 panelMin{ origin.x + (hudSize.x - 1280 * scale) * 0.5f,
        origin.y + (hudSize.y - 720 * scale) * 0.5f };
    const auto p = [&](float x, float y) { return ImVec2(panelMin.x + x * scale, panelMin.y + y * scale); };
    auto* draw = ImGui::GetForegroundDrawList();
    draw->PushClipRect(origin, { origin.x + hudSize.x, origin.y + hudSize.y }, true);
    if (MenuUi::Pressed(input, DIK_TAB)) { details_ = !details_; }
    const float reveal = 1.0f - std::pow(1.0f - std::clamp(elapsed / 0.65f, 0.0f, 1.0f), 3.0f);
    const ImU32 paper = MenuUi::Paper;
    const ImU32 ink = IM_COL32(13, 19, 32, 255);
    const ImU32 quiet = IM_COL32(170, 181, 197, 255);
    const auto surface = [](ImU32 color) { return CombatHud::SurfaceColor(color); };
    const ImVec2 viewportMax{ origin.x + hudSize.x, origin.y + hudSize.y };
    // 自機と空を残し、戦績の背後だけを暗くする。下の翼面は機体のバンク角につなぐ。
    draw->AddRectFilledMultiColor(origin, viewportMax, surface(IM_COL32(8, 13, 25, 80)),
        surface(IM_COL32(8, 13, 25, 20)), surface(IM_COL32(8, 13, 25, 160)), surface(IM_COL32(8, 13, 25, 170)));
    draw->AddRectFilledMultiColor(origin, { p(780, 0).x, viewportMax.y }, surface(IM_COL32(8, 13, 25, 248)),
        surface(IM_COL32(8, 13, 25, 0)), surface(IM_COL32(8, 13, 25, 0)), surface(IM_COL32(8, 13, 25, 248)));
    draw->AddQuadFilled(p(-240, 470), p(1520, 420), p(1520, 920), p(-240, 920), surface(ink));
    draw->AddQuadFilled(p(930, 485), p(1300, 452), p(1300, 720), p(890, 720),
        surface(IM_COL32(24, 34, 49, 255)));
    if (details_) {
        draw->AddRectFilled(origin, viewportMax, surface(IM_COL32(13, 19, 32, 242)));
        MenuUi::Heading(draw, p(64, 82), 58 * scale, paper, "戦績");
    } else {
        CombatHud::Slant(draw, p(54 - (1 - reveal) * 35, 76), 96 * scale, paper,
            "CLEAR", false, false, 0.0f, 580 * scale);
    }

    char value[96]{};
    if (!details_) {
        MenuUi::Heading(draw, p(64, 226), 28 * scale, paper, "スコア");
        std::snprintf(value, sizeof(value), "%06d", stats.score);
        const float scoreFit = (std::min)(1.0f, 580 * scale / (std::max)(CombatHud::Width(value, 110 * scale, true) + 30 * scale, 1.0f));
        const float scoreT = std::clamp((elapsed - 0.08f) / 0.52f, 0.0f, 1.0f);
        std::snprintf(value, sizeof(value), "%06d", static_cast<int>(std::round(static_cast<double>(stats.score) *
            (1.0f - std::pow(1.0f - scoreT, 3.0f)))));
        CombatHud::Slant(draw, p(54, 270), 110 * scale * scoreFit, paper, value, false, false);
        {
            const char rank = StageRecords::EvaluateRank(stats.score);
            const float stamp = std::clamp((elapsed - 0.52f) / 0.18f, 0.0f, 1.0f);
            const int alpha = static_cast<int>(255 * stamp);
            const ImU32 rankInk = (rank == 'S' ? IM_COL32(237, 208, 143, 255) : paper) & ~IM_COL32_A_MASK;
            // ロゴと同じ切断角を持つ専用字形。汎用フォントの巨大な一文字にはしない。
            const auto glyph = [&](std::initializer_list<ImVec2> shape) {
                std::array<ImVec2, 12> points{};
                int count = 0;
                const float punch = 1.0f + 0.18f * (1.0f - stamp) * (1.0f - stamp);
                for (const auto point : shape) {
                    const float x = point.x + (100 - point.y) * 0.19f;
                    points[count++] = p(1030 + 50 * 1.75f + (x - 50) * 1.75f * punch,
                        430 + 50 * 1.75f + (point.y - 50) * 1.75f * punch);
                }
                draw->AddConvexPolyFilled(points.data(), count,
                    surface(rankInk | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT)));
            };
            if (rank == 'A') {
                glyph({ {0,100},{35,0},{55,0},{26,100} });
                glyph({ {43,0},{58,0},{90,100},{65,100} });
                glyph({ {25,59},{64,53},{70,72},{19,79} });
            } else if (rank == 'S') {
                glyph({ {18,0},{82,0},{74,22},{15,22} });
                glyph({ {0,18},{18,0},{23,23},{23,44},{0,44} });
                glyph({ {0,42},{64,42},{83,61},{20,64} });
                glyph({ {60,61},{83,61},{83,82},{65,100},{60,80} });
                glyph({ {-6,80},{65,80},{65,100},{-14,100} });
            } else {
                glyph({ {0,0},{23,0},{23,100},{0,100} });
                glyph({ {22,0},{65,0},{80,19},{22,19} });
                glyph({ {58,18},{80,18},{80,40},{58,54} });
                glyph({ {22,41},{73,41},{83,60},{22,60} });
                glyph({ {62,59},{84,59},{84,81},{62,100} });
                glyph({ {22,81},{64,81},{64,100},{22,100} });
            }
            if (stats.damageCount == 0) { MenuUi::Heading(draw, p(980, 610), 24 * scale, CombatHud::GaugeGold, "ノーダメージ"); }
        }
        if (stats.record) {
            std::snprintf(value, sizeof(value), "%s   ベスト %06d",
                stats.record->newBest ? "自己ベスト更新" : "クリア記録", stats.record->bestScore);
            MenuUi::Text(draw, p(64, 406), 25 * scale,
                stats.record->newBest ? CombatHud::GaugeGold : quiet, value);
        }
        const auto mainStat = [&](int column, const char* label, const char* number) {
            const float x = 64.0f + static_cast<float>(column) * 220.0f;
            MenuUi::Heading(draw, p(x, 488), 25 * scale, paper, label);
            CombatHud::Slant(draw, p(x, 530), 48 * scale, paper, number, false, false, 0.0f, 198 * scale);
        };
        std::snprintf(value, sizeof(value), "%d", stats.maxChain);
        mainStat(0, "最大連続撃破", value);
        std::snprintf(value, sizeof(value), "%d", stats.damageCount);
        mainStat(1, "被弾回数", value);
        std::snprintf(value, sizeof(value), "%d / %d", stats.defeatedCount, stats.targetCount);
        mainStat(2, "撃破数", value);
    } else {
        const auto detail = [&](int index, const char* label, const char* number) {
            const float x = 64.0f + static_cast<float>(index % 2) * 560.0f;
            const float y = 222.0f + static_cast<float>(index / 2) * 128.0f;
            MenuUi::Heading(draw, p(x, y), 30 * scale, quiet, label);
            CombatHud::Slant(draw, p(x, y + 48), 48 * scale, paper, number);
        };
        const int total = (std::max)(0, static_cast<int>(stats.elapsedSeconds * 100));
        std::snprintf(value, sizeof(value), "%02d:%02d.%02d", total / 6000, (total / 100) % 60, total % 100);
        detail(0, "プレイ時間", value);
        std::snprintf(value, sizeof(value), "%d", stats.shots);
        detail(1, "発射数", value);
        std::snprintf(value, sizeof(value), "%d", stats.hits);
        detail(2, "命中数", value);
        std::snprintf(value, sizeof(value), "%d", stats.justDodges);
        detail(3, "ジャスト回避", value);
        std::snprintf(value, sizeof(value), "%d", stats.escapedCount);
        detail(4, "逃した敵", value);
        std::snprintf(value, sizeof(value), "%d / %d", stats.ringPasses, stats.ringSpawns);
        detail(5, "リング通過", value);
    }

    if (stats.record && !stats.record->saved) {
        MenuUi::Text(draw, p(64, 600), 20 * scale, quiet, "記録を保存できませんでした");
    } else if (details_) {
        std::snprintf(value, sizeof(value), "S：%d点以上   A：%d点以上", StageRecords::kSScore, StageRecords::kAScore);
        MenuUi::Text(draw, p(64, 600), 20 * scale, quiet, value);
    }
    ImGui::SetNextWindowPos(panelMin, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ 1280 * scale, 720 * scale }, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::Begin("##ResultActions", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
    if (PreviousPressed(input) || NextPressed(input)) {
        selectedItem_ = 1 - selectedItem_;
    }
    const bool confirm = MenuUi::ConfirmPressed(input);
    const int confirmedItem = selectedItem_;
    if (MenuUi::FlightAction(draw, "retry", "再挑戦", p(64, 638), { 286 * scale, 60 * scale }, scale,
        selectedItem_ == 0) || (confirm && confirmedItem == 0)) { menu.Begin(Action::Retry); }
    MenuUi::SelectHovered(selectedItem_, 0);
    if (MenuUi::FlightAction(draw, "title", "タイトルへ", p(366, 638), { 286 * scale, 60 * scale }, scale,
        selectedItem_ == 1) || (confirm && confirmedItem == 1)) {
        menu.Begin(Action::Exit);
    }
    MenuUi::SelectHovered(selectedItem_, 1);
    if (MenuUi::FlightAction(draw, "result_details", details_ ? "戻る" : "詳細", p(886, 638),
        { 310 * scale, 60 * scale }, scale, false)) { details_ = !details_; }
    CombatHud::KeyBadge(draw, p(1196, 653), p(1240, 681), scale, "TAB", false);
    ImGui::End();
    ImGui::PopStyleVar();
    draw->PopClipRect();
}

// 敗北時の成績と選択肢を描き、再挑戦などの操作をメニュー遷移へ渡す。
void StageResultMenu::DrawDefeat(Input* input, const Math::Vector2& min, const Math::Vector2& size,
    const Stats& stats, float deltaSeconds, GameplayMenu& menu)
{
    const float scale = CombatHud::Scale(size);
    const ImVec2 origin{ min.x + (size.x - 1280 * scale) * 0.5f, min.y + (size.y - 720 * scale) * 0.5f };
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    const float elapsed = BeginDisplay(Kind::Defeat);
    const auto reveal = [&](float delay, float duration) { return EaseInOut((elapsed - delay) / duration); };
    const Action pending = menu.PendingAction();
    const float leaving = pending == Action::Retry || pending == Action::Exit ? menu.TransitionProgress() : 0.0f;
    const float contentVisibility = 1.0f - leaving * 0.55f;
    auto* draw = ImGui::GetForegroundDrawList();
    draw->PushClipRect({ min.x, min.y }, { min.x + size.x, min.y + size.y }, true);
    const auto paper = MenuUi::Paper;
    const auto quiet = IM_COL32(173, 183, 198, 255);
    const auto danger = IM_COL32(235, 136, 120, 255);

    // 読む場所だけを暗くし、中央の残骸と煙はゲーム画面の明るさで残す。
    const auto shade = CombatHud::SurfaceColor(IM_COL32(7, 12, 20, static_cast<int>(222.0f * reveal(0.0f, 0.24f))));
    const auto clear = IM_COL32(0, 0, 0, 0);
    const float bottom = min.y + size.y;
    draw->AddRectFilled({ min.x, min.y }, { p(330, 0).x, bottom }, shade);
    draw->AddRectFilledMultiColor({ p(330, 0).x, min.y }, { p(640, 0).x, bottom }, shade, clear, clear, shade);

    int first = draw->VtxBuffer.Size;
    const float heading = reveal(0.0f, 0.24f);
    CombatHud::Slant(draw, p(76, 164 + (1.0f - heading) * 9.0f), 62 * scale, danger,
        details_ ? "RESULT" : "GAME OVER", false, false, 0, 370 * scale);
    MenuUi::Opacity(draw, first, heading * contentVisibility);

    if (MenuUi::Pressed(input, DIK_TAB)) { details_ = !details_; }
    char value[96]{};
    if (!details_) {
        first = draw->VtxBuffer.Size;
        const float scoreReveal = reveal(0.08f, 0.24f);
        const float rise = (1.0f - scoreReveal) * 7.0f;
        MenuUi::Heading(draw, p(80, 252 + rise), 19 * scale, quiet, "スコア");
        std::snprintf(value, sizeof(value), "%06d", stats.score);
        CombatHud::Slant(draw, p(76, 279 + rise), 66 * scale, paper, value, false, true, 0, 324 * scale);
        MenuUi::Opacity(draw, first, scoreReveal * contentVisibility);

        first = draw->VtxBuffer.Size;
        const float statsReveal = reveal(0.14f, 0.24f);
        MenuUi::Heading(draw, p(80, 370), 18 * scale, quiet, "到達地点");
        std::snprintf(value, sizeof(value), "WAVE %d / %d",
            stats.waveNumber, stats.waveCount);
        CombatHud::Slant(draw, p(78, 398), 29 * scale, paper, value);
        MenuUi::Heading(draw, p(258, 370), 18 * scale, quiet, "撃破数");
        std::snprintf(value, sizeof(value), "%d", stats.defeatedCount);
        CombatHud::Slant(draw, p(256, 398), 29 * scale, paper, value);
        MenuUi::Opacity(draw, first, statsReveal * contentVisibility);
    } else {
        first = draw->VtxBuffer.Size;
        const char* labels[]{ "最大連続撃破", "被弾回数", "撃破数", "ジャスト回避" };
        const int values[]{ stats.maxChain, stats.damageCount, stats.defeatedCount, stats.justDodges };
        for (int index = 0; index < 4; ++index) {
            const float y = 264.0f + static_cast<float>(index) * 43.0f;
            MenuUi::Heading(draw, p(80, y), 21 * scale, quiet, labels[index]);
            std::snprintf(value, sizeof(value), "%d", values[index]);
            CombatHud::Slant(draw, p(388, y - 3), 29 * scale, paper, value, true);
        }
        MenuUi::Opacity(draw, first, reveal(0.08f, 0.24f) * contentVisibility);
    }

    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({ 1280 * scale, 720 * scale });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::Begin("##DefeatActions", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
    const bool ready = elapsed >= 0.20f;
    if (ready && (PreviousPressed(input) || NextPressed(input))) { selectedItem_ = 1 - selectedItem_; }
    const bool confirm = ready && MenuUi::ConfirmPressed(input);
    const int selected = selectedItem_;
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.10f);
    first = draw->VtxBuffer.Size;
    ImGui::BeginDisabled(!ready);
    // 再挑戦の面を細くし、選択時の移動と決定時の押し込みで反応を伝える。
    const auto action = [&](const char* id, const char* label, int item, ImVec2 at, ImVec2 hitSize, float font) {
        ImGui::SetCursorScreenPos(at);
        const bool clicked = ImGui::InvisibleButton(id, hitSize);
        const bool hovered = !MenuUi::InputBlocked && ImGui::IsItemHovered();
        const bool chosen = selected == item;
        const float target = chosen || hovered ? 1.0f : 0.0f;
        auto& emphasis = defeatActionEmphasis_[item];
        emphasis += (target - emphasis) * (1.0f - std::exp(-18.0f * dt));
        const bool pressed = ImGui::IsItemActive() || (confirm && chosen) || (menu.PendingAction() == Action::Retry && item == 0);
        const float push = pressed ? 2.0f * scale : 0.0f;
        const float launch = item == 0 && menu.PendingAction() == Action::Retry ? leaving : 0.0f;
        const float x = at.x + (5.0f * emphasis + 12.0f * launch) * scale;
        const float y = at.y + 3.0f * scale + push;
        const float width = hitSize.x - 10.0f * scale + 28.0f * launch * scale;
        const float height = hitSize.y - 8.0f * scale - push;
        draw->AddQuadFilled({ x + 12.0f * scale, y }, { x + width, y },
            { x + width - 12.0f * scale, y + height }, { x, y + height },
            CombatHud::SurfaceColor(IM_COL32(226, 231, 237, static_cast<int>(255.0f * emphasis))));
        const auto color = CombatHud::Mix(paper, MenuUi::Ink, emphasis);
        MenuUi::Heading(draw, { x + 24.0f * scale, y + (height - font) * 0.5f - scale }, font, color, label);
        if (item == 0 && !MenuUi::InputBlocked && menu.PendingAction() == Action::None) {
            MenuUi::Text(draw, { x + width - 18.0f * scale, y + (height - 13.0f * scale) * 0.5f },
                13.0f * scale, CombatHud::Mix(quiet, MenuUi::Ink, emphasis), "R", true);
        }
        MenuUi::SelectHovered(selectedItem_, item);
        return clicked || (confirm && chosen);
    };
    const float enter = 1.0f - reveal(0.20f, 0.22f);
    if (action("defeat_retry", "再挑戦", 0, p(80 - enter * 7.0f, 480), { 266 * scale, 50 * scale }, 28 * scale)) {
        if (menu.Begin(Action::Retry)) { ResetSelection(); }
    }
    if (action("defeat_title", "タイトルへ", 1, p(80 - enter * 7.0f, 544), { 182 * scale, 42 * scale }, 21 * scale)) {
        menu.Begin(Action::Exit);
    }
    ImGui::SetCursorScreenPos(p(284, 544));
    if (ImGui::InvisibleButton("defeat_details", { 104 * scale, 42 * scale })) { details_ = !details_; }
    const bool detailHovered = !MenuUi::InputBlocked && ImGui::IsItemHovered();
    MenuUi::Heading(draw, p(296, 553), 19 * scale, detailHovered ? paper : quiet, details_ ? "戻る" : "詳細");
    if (detailHovered) { draw->AddLine(p(296, 579), p(334, 579), CombatHud::SurfaceColor(paper), scale); }
    if (!MenuUi::InputBlocked) {
        MenuUi::Text(draw, p(80, 605), 13 * scale, quiet, "ENTER / SPACE：決定    TAB：詳細");
    }
    ImGui::EndDisabled();
    ImGui::End();
    ImGui::PopStyleVar();
    MenuUi::Opacity(draw, first, reveal(0.20f, 0.22f) * (1.0f - leaving * 0.15f));
    draw->PopClipRect();
}

// 練習完了時の結果と次へ進む操作を描き、選んだ操作をメニューの切り替えに渡す。
void StageResultMenu::DrawTutorial(Input* input, const Math::Vector2& min, const Math::Vector2& size,
    std::span<const char* const> lessons, GameplayMenu& menu)
{
    const float s = CombatHud::Scale(size);
    const ImVec2 origin{ min.x + (size.x - 1280 * s) * 0.5f, min.y + (size.y - 720 * s) * 0.5f };
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * s, origin.y + y * s); };
    auto* draw = ImGui::GetForegroundDrawList();
    draw->PushClipRect({ min.x, min.y }, { min.x + size.x, min.y + size.y }, true);
    draw->AddRectFilled({ min.x, min.y }, { min.x + size.x, min.y + size.y },
        CombatHud::SurfaceColor(MenuUi::Ink));
    MenuUi::Text(draw, p(64, 106), 22 * s, CombatHud::Blue, "チュートリアル");
    MenuUi::Heading(draw, p(64, 155), 42 * s, MenuUi::Paper, "練習終了");
    // 練習に順位や失敗回数の採点は不要。体験できた操作だけを振り返る。
    for (int index = 0; index < static_cast<int>(lessons.size()); ++index) {
        const float x = 64.0f + static_cast<float>(index / 4) * 330.0f;
        const float y = 272.0f + static_cast<float>(index % 4) * 58.0f;
        draw->AddLine(p(x, y + 10), p(x + 5, y + 15), CombatHud::SurfaceColor(CombatHud::Health), 2 * s);
        draw->AddLine(p(x + 5, y + 15), p(x + 15, y + 3), CombatHud::SurfaceColor(CombatHud::Health), 2 * s);
        MenuUi::Text(draw, p(x + 30, y), 24 * s, MenuUi::Paper, lessons[index]);
    }
    MenuUi::Heading(draw, p(64, 548), 24 * s, CombatHud::Blue, "本編の目標");
    MenuUi::Text(draw, p(64, 586), 23 * s, MenuUi::Paper, "敵編隊を突破し、最後のボスを撃破する。");
    MenuUi::Text(draw, p(64, 622), 19 * s, MenuUi::Quiet, "HPがなくなるとゲームオーバー。リザルトから再挑戦できます。");
    MenuUi::Text(draw, p(64, 674), 18 * s, MenuUi::Quiet, "W A S D / 矢印キーで選択    ENTER / SPACEで決定");
    if (PreviousPressed(input)) {
        selectedItem_ = (selectedItem_ + 2) % 3;
    }
    if (NextPressed(input)) {
        selectedItem_ = (selectedItem_ + 1) % 3;
    }
    const bool confirm = MenuUi::ConfirmPressed(input);
    const int confirmed = selectedItem_;
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({ 1280 * s, 720 * s });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::Begin("##TutorialResult", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
    const char* labels[]{ "出撃する", "もう一度練習", "タイトルへ" };
    constexpr Action actions[] = { Action::MainGame, Action::Retry, Action::Exit };
    for (int index = 0; index < 3; ++index) {
        const bool clicked = MenuUi::FlightAction(draw, labels[index], labels[index],
            p(950 - index * 18.0f, 254 + index * 100.0f), { 290 * s, 64 * s }, s, selectedItem_ == index);
        MenuUi::SelectHovered(selectedItem_, index);
        if (clicked || (confirm && confirmed == index)) {
            menu.Begin(actions[index]);
            break;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    draw->PopClipRect();
}
