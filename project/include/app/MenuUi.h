#pragma once
#include "app/CombatHud.h"
#include "engine/io/Input.h"
#include <initializer_list>

// タイトル・操作説明・リザルトの共通部品。戦闘HUDと同じ書体で組む。
namespace MenuUi {
inline constexpr ImU32 Ink = IM_COL32(17, 22, 30, 255);
inline constexpr ImU32 Paper = CombatHud::White;
inline constexpr ImU32 Quiet = CombatHud::Muted;
inline constexpr ImU32 CanvasPaper = IM_COL32(239, 238, 231, 255);

// メニュー入力はDirectInputの立ち上がりだけを使う。
// ImGui側のイベントをORすると、配送タイミングのずれで一押しを二度処理してしまう。
inline bool Pressed(Input* input, BYTE key)
{
    if (!input) { return false; }
    if (key == DIK_RETURN && (input->PushKey(DIK_LMENU) || input->PushKey(DIK_RMENU))) { return false; }
    return input->TriggerKey(key);
}

inline void SelectHovered(int& selected, int item)
{
    if (ImGui::IsItemHovered() && (ImGui::GetIO().MouseDelta.x != 0 || ImGui::GetIO().MouseDelta.y != 0)) {
        selected = item;
    }
}

inline float Width(const char* text, float size, bool heading = false) { return CombatHud::ReadoutWidth(text, size, heading); }

inline void Text(ImDrawList* draw, ImVec2 at, float size, ImU32 color, const char* text, bool right = false,
    bool heading = false)
{
    if (right) { at.x -= Width(text, size, heading); }
    draw->AddText(CombatHud::ReadoutFont(text, heading), size, { std::round(at.x), std::round(at.y) },
        CombatHud::SurfaceColor(color), text);
}

inline void Heading(ImDrawList* draw, ImVec2 at, float size, ImU32 color, const char* text, bool right = false)
{
    Text(draw, at, size, color, text, right, true);
}

inline void Number(ImDrawList* draw, ImVec2 at, float size, ImU32 color, const char* text, bool right = false)
{
    CombatHud::Number(draw, at, size * 1.25f, color, text, right);
}

// メニューは選択中の一項目だけを翼の面に載せる。未選択の四角いボタンを並べない。
inline bool FlightAction(ImDrawList* draw, const char* id, const char* label, ImVec2 min,
    ImVec2 size, float scale, bool selected, bool lightCanvas = false)
{
    ImGui::SetCursorScreenPos(min);
    const bool clicked = ImGui::InvisibleButton(id, size);
    if (selected) {
        const ImU32 fill = lightCanvas ? Ink : Paper;
        draw->AddQuadFilled({ min.x + 16 * scale, min.y }, { min.x + size.x, min.y },
            { min.x + size.x - 16 * scale, min.y + size.y }, { min.x, min.y + size.y },
            CombatHud::SurfaceColor(fill));
    }
    const float font = (selected ? 30.0f : 25.0f) * scale;
    Heading(draw, { min.x + 30 * scale, min.y + (size.y - font) * 0.5f - scale }, font,
        selected ? (lightCanvas ? Paper : Ink) : (lightCanvas ? Ink : Paper), label);
    if (selected) {
        Text(draw, { min.x + size.x - 28 * scale, min.y + (size.y - 14 * scale) * 0.5f },
            14 * scale, lightCanvas ? Quiet : Ink, "ENTER", true);
    }
    return clicked;
}

// タイトルで使うAZRAIDの字形と切断角を、小さな署名にも使う。
inline void Brand(ImDrawList* draw, ImVec2 at, float height, ImU32 color)
{
    float pen = 0;
    const float scale = height / 100.0f;
    const auto polygon = [&](std::initializer_list<ImVec2> shape) {
        std::array<ImVec2, 12> source{};
        int count = 0;
        for (const auto point : shape) { source[count++] = { pen + point.x + (100 - point.y) * 0.19f, point.y }; }
        for (int half = 0; half < 2; ++half) {
            std::array<ImVec2, 16> clipped{};
            int size = 0;
            const auto distance = [&](ImVec2 point) {
                const float cut = point.y - 80 + point.x * 0.11f;
                return half == 0 ? -cut - 0.65f : cut - 0.65f;
            };
            const auto append = [&](ImVec2 point) { clipped[size++] = { at.x + point.x * scale, at.y + point.y * scale }; };
            for (int index = 0; index < count; ++index) {
                const auto a = source[index], b = source[(index + 1) % count];
                const float da = distance(a), db = distance(b);
                if (da >= 0) { append(a); }
                if ((da >= 0) != (db >= 0)) {
                    const float t = da / (da - db);
                    append({ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t });
                }
            }
            if (size >= 3) { draw->AddConvexPolyFilled(clipped.data(), size, CombatHud::SurfaceColor(color)); }
        }
    };
    for (const char* letter = "AZRAID"; *letter; ++letter) {
        switch (*letter) {
        case 'A':
            polygon({ {0,100},{35,0},{55,0},{26,100} });
            polygon({ {43,0},{58,0},{90,100},{65,100} });
            polygon({ {25,59},{64,53},{70,72},{19,79} }); pen += 93; break;
        case 'Z':
            polygon({ {0,0},{97,-5},{78,19},{0,19} });
            polygon({ {55,18},{82,18},{26,83},{0,83} });
            polygon({ {0,81},{83,81},{67,105},{-12,105} }); pen += 90; break;
        case 'R':
            polygon({ {0,0},{22,0},{22,100},{0,100} });
            polygon({ {21,0},{65,0},{82,18},{21,18} });
            polygon({ {61,17},{82,17},{82,43},{61,43} });
            polygon({ {21,42},{82,42},{63,61},{21,61} });
            polygon({ {36,58},{62,58},{90,100},{64,100} }); pen += 96; break;
        case 'I': polygon({ {3,0},{27,0},{21,100},{-3,100} }); pen += 37; break;
        case 'D':
            polygon({ {0,0},{22,0},{22,100},{0,100} });
            polygon({ {21,0},{62,0},{84,20},{63,20},{21,18} });
            polygon({ {63,19},{84,20},{84,78},{63,82} });
            polygon({ {21,81},{84,78},{62,100},{21,100} }); pen += 92; break;
        }
    }
}

inline void Surface(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale, ImU32 top, ImU32 bottom)
{
    const int firstVertex = draw->VtxBuffer.Size;
    CombatHud::Plate(draw, min, max, scale, static_cast<int>((top >> IM_COL32_A_SHIFT) & 255));
    for (int index = firstVertex; index < draw->VtxBuffer.Size; ++index) {
        auto& vertex = draw->VtxBuffer[index];
        const float t = std::clamp((vertex.pos.y - min.y) / (std::max)(max.y - min.y, 1.0f), 0.0f, 1.0f);
        const ImU32 color = CombatHud::SurfaceColor(CombatHud::Mix(top, bottom, t));
        vertex.col = (color & ~IM_COL32_A_MASK) | (vertex.col & IM_COL32_A_MASK);
    }
}

inline void Sheet(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale)
{
    CombatHud::Plate(draw, { min.x + 6 * scale, min.y + 10 * scale },
        { max.x + 6 * scale, max.y + 10 * scale }, scale, 70);
    Surface(draw, min, max, scale, IM_COL32(31, 36, 44, 252), IM_COL32(15, 19, 25, 252));
}

// 選択矢印や下線は付けず、面の明暗で押せる場所を示す。
inline bool Button(ImDrawList* draw, const char* id, const char* label, ImVec2 min, ImVec2 size,
    float scale, bool selected = false, bool trackHover = true)
{
    ImGui::SetCursorScreenPos(min);
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool lit = selected || (trackHover && ImGui::IsItemHovered());
    const ImVec2 max{ min.x + size.x, min.y + size.y };
    const ImU32 top = lit ? Paper : IM_COL32(49, 51, 57, 255);
    const ImU32 bottom = lit ? IM_COL32(204, 207, 212, 255) : IM_COL32(29, 31, 36, 255);
    Surface(draw, min, max, scale, top, bottom);
    const float fontSize = 21.0f * scale;
    Heading(draw, { min.x + (size.x - Width(label, fontSize, true)) * 0.5f,
        min.y + (size.y - fontSize) * 0.5f - scale }, fontSize, lit ? Ink : Paper, label);
    return clicked;
}

inline void Key(ImDrawList* draw, ImVec2 min, float width, float scale, const char* label)
{
    const ImVec2 max{ min.x + width * scale, min.y + 36 * scale };
    draw->AddRectFilled({ min.x, min.y + 3 * scale }, { max.x, max.y + 3 * scale },
        CombatHud::SurfaceColor(IM_COL32(9, 12, 18, 255)), 3 * scale);
    draw->AddRectFilled(min, max, CombatHud::SurfaceColor(IM_COL32(60, 63, 70, 255)), 3 * scale);
    draw->AddRect(min, max, CombatHud::SurfaceColor(IM_COL32(128, 132, 143, 200)), 3 * scale, 0, scale);
    Text(draw, { min.x + (width * scale - Width(label, 21 * scale)) * 0.5f,
        min.y + 6 * scale }, 21 * scale, Paper, label);
}

// 同じ説明をタイトルと本編で共有。キーと動作の対応を一画面に置く。
inline bool Controls(ImVec2 viewportMin, ImVec2 viewportSize)
{
    const float scale = CombatHud::Scale({ viewportSize.x, viewportSize.y });
    const ImVec2 origin{ viewportMin.x + (viewportSize.x - 1280 * scale) * 0.5f,
        viewportMin.y + (viewportSize.y - 720 * scale) * 0.5f };
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    ImGui::SetNextWindowPos(viewportMin);
    ImGui::SetNextWindowSize(viewportSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    ImGui::Begin("##ControlSheet", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    auto* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(viewportMin, { viewportMin.x + viewportSize.x, viewportMin.y + viewportSize.y },
        CombatHud::SurfaceColor(CanvasPaper));
    draw->PushClipRect(viewportMin, { viewportMin.x + viewportSize.x, viewportMin.y + viewportSize.y }, true);
    draw->AddQuadFilled(p(-200, -200), p(472, -200), p(296, 920), p(-200, 920), CombatHud::SurfaceColor(Ink));
    Brand(draw, p(48, 44), 24 * scale, Paper);
    CombatHud::Slant(draw, p(30, 128), 76 * scale, Paper, "CONTROL", false, false);
    Heading(draw, p(48, 222), 26 * scale, Paper, "操作方法");
    Text(draw, p(48, 650), 17 * scale, Quiet, "F11  全画面切替");
    Heading(draw, p(452, 94), 28 * scale, Ink, "移動");
    Key(draw, p(532, 150), 40, scale, "W");
    Key(draw, p(486, 192), 40, scale, "A");
    Key(draw, p(532, 192), 40, scale, "S");
    Key(draw, p(578, 192), 40, scale, "D");
    Heading(draw, p(452, 302), 28 * scale, Ink, "照準");
    Text(draw, p(575, 302), 25 * scale, Ink, "マウス");
    Heading(draw, p(452, 420), 28 * scale, Ink, "回避");
    Key(draw, p(452, 468), 68, scale, "A / D");
    Text(draw, p(540, 472), 23 * scale, Ink, "+");
    Key(draw, p(575, 468), 88, scale, "SHIFT");
    Heading(draw, p(850, 94), 28 * scale, Ink, "射撃");
    Key(draw, p(850, 148), 142, scale, "SPACE");
    Text(draw, p(850, 212), 18 * scale, Ink, "長押しで連射");
    Heading(draw, p(850, 302), 28 * scale, Ink, "残像連撃");
    Key(draw, p(1140, 297), 42, scale, "Q");
    Heading(draw, p(850, 420), 28 * scale, Ink, "ポーズ");
    Key(draw, p(1095, 416), 86, scale, "ESC");
    const bool close = FlightAction(draw, "close_controls", "戻る", p(900, 620),
        { 310 * scale, 60 * scale }, scale, true, true);
    draw->PopClipRect();
    ImGui::End();
    ImGui::PopStyleVar();
    return close;
}
} // namespace MenuUi
