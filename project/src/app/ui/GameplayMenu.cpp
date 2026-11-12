// ===== GameplayMenu.cpp =====
// ポーズと操作ガイドを表示し、再開や画面移動の操作を受け付ける

#include "app/ui/GameplayMenu.h"

#include "app/ui/CombatHud.h"
#include "app/ui/MenuUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

// Esc・H・決定キーを、今のメニュー状態に応じて開く・閉じる・再開する操作へ変換する
void GameplayMenu::HandleShortcuts(Input* input, bool canPause)
{
    inputConsumed_ = false;
    if (!canPause) { return; }

    if (MenuUi::Pressed(input, DIK_ESCAPE)) {
        if (controlsOpen_) { Begin(Action::Back); }
        else if (paused_) { Begin(Action::Resume); }
        else {
            paused_ = true;
            selectedItem_ = 0;
            pauseStarted_ = -1.0;
        }
    } else if (MenuUi::Pressed(input, DIK_H)) {
        Begin(controlsOpen_ ? Action::Resume : Action::Controls);
        selectedItem_ = 0;
    } else if (controlsOpen_ && MenuUi::ConfirmPressed(input)) {
        Begin(Action::Back);
        inputConsumed_ = true;
    }
}

// 画面の切り替えを予約し、演出の開始時刻を記録する　他の操作を処理中ならfalseを返す
bool GameplayMenu::Begin(Action action)
{
    if (action == Action::None || action_ != Action::None || sceneAction_ != Action::None) {
        return false;
    }
    action_ = action;
    actionStarted_ = ImGui::GetTime();
    paused_ = true;
    inputConsumed_ = true;
    return true;
}

// 現在の操作ごとの演出時間で経過を割り、滑らかに変化する0〜1の進行度を返す
float GameplayMenu::TransitionProgress() const
{
    if (action_ == Action::None) { return 0.0f; }
    const float duration = action_ == Action::Resume ? 0.20f :
        action_ == Action::Controls || action_ == Action::Back ? 0.18f : 0.22f;
    const float t = std::clamp(static_cast<float>(ImGui::GetTime() - actionStarted_) / duration, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// 画面演出が終わったところでポーズ・ヘルプの状態を変える　再挑戦などはシーンへの指示として残す
GameplayMenu::Action GameplayMenu::UpdateTransition()
{
    const float progress = TransitionProgress();
    if (action_ == Action::Retry || action_ == Action::Exit || action_ == Action::MainGame) {
        const auto* viewport = ImGui::GetMainViewport();
        MenuUi::Wipe(viewport->Pos, viewport->Size, progress);
    }
    if (progress < 1.0f) { return Action::None; }

    const Action completed = action_;
    switch (completed) {
    case Action::Resume:
        paused_ = false;
        controlsOpen_ = false;
        pauseStarted_ = -1.0;
        break;
    case Action::Controls:
        controlsOpen_ = true;
        break;
    case Action::Back:
        controlsOpen_ = false;
        break;
    case Action::Retry:
    case Action::Exit:
    case Action::MainGame:
        sceneAction_ = completed;
        break;
    case Action::None:
        break;
    }
    action_ = Action::None;
    return completed;
}

// 操作ヘルプの表示と「戻る」の入力を処理する　開閉中は位置と透明度を補間する
void GameplayMenu::DrawControls(const Math::Vector2& min, const Math::Vector2& size)
{
    const float scale = CombatHud::Scale(size);
    const ImVec2 origin{ min.x + (size.x - 1280 * scale) * 0.5f, min.y + (size.y - 720 * scale) * 0.5f };
    const float t = TransitionProgress();
    const float visibility = action_ == Action::Controls ? t :
        action_ == Action::Back || action_ == Action::Resume ? 1.0f - t : 1.0f;
    const float slide = action_ == Action::Resume ? t * 560.0f : (1.0f - visibility) * 90.0f;
    const auto p = [&](float x, float y) { return ImVec2(origin.x + (x + slide) * scale, origin.y + y * scale); };
    auto* draw = ImGui::GetForegroundDrawList();
    const int first = draw->VtxBuffer.Size;
    draw->PushClipRect({ min.x, min.y }, { min.x + size.x, min.y + size.y }, true);
    MenuUi::Heading(draw, p(832, 144), 40 * scale, MenuUi::Paper, "操作方法");
    const char* labels[]{ "移動", "照準", "射撃・チャージ", "回避", "スキル", "ポーズ" };
    const char* keys[]{ "W A S D", "マウス", "SPACE", "A / D + SHIFT", "Q", "ESC" };
    for (int index = 0; index < 6; ++index) {
        const float y = 224.0f + static_cast<float>(index) * 58.0f;
        MenuUi::Heading(draw, p(832, y), 25 * scale, MenuUi::Paper, labels[index]);
        MenuUi::Text(draw, p(1216, y), 24 * scale, MenuUi::Paper, keys[index], true);
    }
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({ 1280 * scale, 720 * scale });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::Begin("##PausedControls", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    if (MenuUi::FlightAction(draw, "back_controls", "戻る", p(832, 604), { 384 * scale, 62 * scale }, scale, true)) {
        Begin(Action::Back);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    draw->PopClipRect();
    MenuUi::Opacity(draw, first, visibility);
}

// ポーズ画面のHP・得点・選択肢を描く　決定した操作はBeginに渡して画面の切り替えを始める
void GameplayMenu::DrawPause(Input* input, const Math::Vector2& min, const Math::Vector2& size,
    int score, int hp, int maxHp)
{
    if (!controlsOpen_ && (MenuUi::Pressed(input, DIK_UP) || MenuUi::Pressed(input, DIK_W))) {
        selectedItem_ = (selectedItem_ + 3) % 4;
    }
    if (!controlsOpen_ && (MenuUi::Pressed(input, DIK_DOWN) || MenuUi::Pressed(input, DIK_S))) {
        selectedItem_ = (selectedItem_ + 1) % 4;
    }
    const bool confirm = !inputConsumed_ && MenuUi::ConfirmPressed(input);
    const float scale = CombatHud::Scale(size);
    const ImVec2 origin{ min.x + (size.x - 1280 * scale) * 0.5f, min.y + (size.y - 720 * scale) * 0.5f };
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    auto* draw = ImGui::GetForegroundDrawList();
    const int first = draw->VtxBuffer.Size;
    draw->PushClipRect({ min.x, min.y }, { min.x + size.x, min.y + size.y }, true);
    if (pauseStarted_ < 0) { pauseStarted_ = ImGui::GetTime(); }
    const float elapsed = static_cast<float>(ImGui::GetTime() - pauseStarted_);
    const float enter = 1.0f - std::pow(1.0f - std::clamp(elapsed / 0.22f, 0.0f, 1.0f), 3.0f);
    const float t = TransitionProgress();
    const float closing = action_ == Action::Resume ? t : 0.0f;
    draw->AddRectFilled({ min.x, min.y }, { min.x + size.x, min.y + size.y }, IM_COL32(0, 0, 0, 88));
    // 実際の機体を中央に残し、前傾したメニューが右から滑り込む
    const float slide = (1.0f - enter) * 140 + closing * 560;
    draw->AddQuadFilled(p(840 + slide, -200), p(1500 + slide, -200), p(1500 + slide, 900),
        p(632 + slide, 900), CombatHud::SurfaceColor(IM_COL32(13, 19, 32, 244)));
    CombatHud::Wake(draw, p(0, 0), p(700, 316));
    CombatHud::Slant(draw, p(54 - (1 - enter) * 40 - closing * 80, 146), 104 * scale, MenuUi::Paper, "PAUSE", false, false);
    char pauseScore[32]{};
    std::snprintf(pauseScore, sizeof(pauseScore), "%06d", score);
    CombatHud::Slant(draw, p(1248, 24), 40 * scale, MenuUi::Paper, pauseScore, true);
    char pauseHp[16]{};
    std::snprintf(pauseHp, sizeof(pauseHp), "%d", hp);
    CombatHud::Slant(draw, p(32, 20), 48 * scale, MenuUi::Paper, pauseHp);
    CombatHud::FlightMeter(draw, p(32, 79), 192 * scale, 7 * scale,
        static_cast<float>(hp) / static_cast<float>((std::max)(maxHp, 1)),
        CombatHud::Health, scale);
    const int actionFirst = draw->VtxBuffer.Size;
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({ 1280 * scale, 720 * scale });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::Begin("##PauseActions", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    const char* labels[] = { "再開", "再挑戦", "操作方法", "タイトルへ" };
    constexpr Action actions[] = { Action::Resume, Action::Retry, Action::Controls, Action::Exit };
    for (int index = 0; index < 4 && (!controlsOpen_ || action_ == Action::Back); ++index) {
        const float x = 860 - static_cast<float>(index) * 14;
        const bool clicked = MenuUi::FlightAction(draw, labels[index], labels[index],
            p(x + slide, 218 + index * 78.0f), { 310 * scale, 62 * scale }, scale, selectedItem_ == index);
        MenuUi::SelectHovered(selectedItem_, index);
        if (clicked || (confirm && selectedItem_ == index)) {
            Begin(actions[index]);
            break;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    draw->PopClipRect();
    const float actionVisibility = action_ == Action::Controls ? 1.0f - t : action_ == Action::Back ? t : 1.0f;
    MenuUi::Opacity(draw, actionFirst, actionVisibility);
    MenuUi::Opacity(draw, first, 1.0f - closing);
}
