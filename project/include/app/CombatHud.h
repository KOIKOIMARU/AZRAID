#pragma once
#include "engine/base/ImGuiManager.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

// 戦闘HUD共通の文字・色・寸法。画面ごとに影や縮尺の流儀を増やさない。
namespace CombatHud {
inline constexpr ImU32 White = IM_COL32(239, 241, 244, 255);
inline constexpr ImU32 Muted = IM_COL32(168, 171, 178, 255);
inline constexpr ImU32 Blue = IM_COL32(91, 206, 242, 255);
inline constexpr ImU32 Gold = IM_COL32(231, 189, 109, 255);
inline constexpr ImU32 Danger = IM_COL32(237, 100, 88, 255);
inline constexpr ImU32 Track = IM_COL32(41, 42, 43, 220);
// ゲージの意味で色を固定する。青はチャージ・スキルのエネルギー、金は報酬・フィーバー。
inline constexpr ImU32 GaugeGold = IM_COL32(255, 202, 70, 255);
inline constexpr ImU32 Health = IM_COL32(66, 218, 142, 255);
inline constexpr ImU32 Energy = IM_COL32(82, 145, 244, 255);
inline constexpr ImU32 FeverCharge = IM_COL32(245, 172, 59, 255);
inline ImU32 SurfaceColor(ImU32 srgb);

inline float Scale(const Math::Vector2& viewport)
{
    return std::clamp((std::min)(viewport.x / 1280.0f, viewport.y / 720.0f), 0.5f, 2.0f);
}

// 各計器は同じ画面端・列・間隔へ揃える。通信と操作計器は中央の射撃領域へ置かない。
struct Layout {
    float scale;
    ImVec2 health, score, chain, boss, charge, skill, fever, radio;
    Layout(const Math::Vector2& min, const Math::Vector2& size) : scale(Scale(size)) {
        health = { min.x + 32 * scale, min.y + 26 * scale };
        score = { min.x + size.x - 304 * scale, min.y + 28 * scale };
        chain = { score.x, min.y + 130 * scale };
        boss = { min.x + (size.x - 416 * scale) * 0.5f, min.y + 32 * scale };
        charge = { min.x + size.x - 276 * scale, min.y + size.y - 180 * scale };
        skill = { charge.x, min.y + size.y - 104 * scale };
        fever = { health.x, min.y + size.y - 136 * scale };
        radio = { health.x, min.y + size.y - 260 * scale };
    }
};

inline ImFont* Font(bool number = false)
{
    return number ? ImGuiManager::GetHudNumberFont() : ImGuiManager::GetHudFont();
}

inline float Width(const char* text, float size, bool number = false)
{
    return Font(number)->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
}

inline void Text(ImDrawList* draw, ImVec2 position, float size, ImU32 color,
    const char* text, bool right = false, bool number = false)
{
    if (right) { position.x -= Width(text, size, number); }
    position.x = std::round(position.x);
    position.y = std::round(position.y);
    const int alpha = static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xff);
    // 太さは書体で確保し、何重もの縁取りで小さな文字を潰さない。
    draw->AddText(Font(number), size, { position.x, position.y + 1.0f },
        IM_COL32(10, 11, 12, alpha * 3 / 4), text);
    draw->AddText(Font(number), size, position, color, text);
}

inline void Shade(ImDrawList* draw, ImVec2 min, ImVec2 max, bool right = false)
{
    const ImU32 dark = IM_COL32(12, 13, 14, 180);
    const ImU32 clear = IM_COL32(12, 13, 14, 0);
    draw->AddRectFilledMultiColor(min, max, right ? clear : dark, right ? dark : clear,
        right ? dark : clear, right ? clear : dark);
}

inline ImFont* BattleFont() { return ImGuiManager::GetCombatFont(); }
inline ImFont* HeadingFont() { return ImGuiManager::GetHudHeadingFont(); }

// 和文を含む行は同じ和文書体で組む。短い英字・キー名だけは数字と書体を揃える。
inline ImFont* ReadoutFont(const char* text, bool heading = false)
{
    for (const char* letter = text; *letter; ++letter) {
        if (static_cast<unsigned char>(*letter) >= 0x80) { return heading ? HeadingFont() : BattleFont(); }
    }
    return Font(true);
}

inline float ReadoutWidth(const char* text, float size, bool heading = false)
{
    return ReadoutFont(text, heading)->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
}

// 和文ラベルはメニューと同じ書体・色変換を使う。細い影だけで背景から分離する。
inline void Readout(ImDrawList* draw, ImVec2 position, float size, ImU32 color,
    const char* text, bool right = false, bool heading = false)
{
    if (right) { position.x -= ReadoutWidth(text, size, heading); }
    position.x = std::round(position.x);
    position.y = std::round(position.y);
    const int alpha = static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xff);
    draw->AddText(ReadoutFont(text, heading), size, { position.x, position.y + 1.0f },
        IM_COL32(0, 0, 0, alpha * 4 / 5), text);
    draw->AddText(ReadoutFont(text, heading), size, position, SurfaceColor(color), text);
}

inline void Heading(ImDrawList* draw, ImVec2 position, float size, ImU32 color,
    const char* text, bool right = false)
{
    Readout(draw, position, size, color, text, right, true);
}

// 大きな数字は角を落とした書体で揃える。和文の行中にある数字は和文書体で組む。
inline void Number(ImDrawList* draw, ImVec2 position, float size, ImU32 color,
    const char* text, bool right = false)
{
    if (right) { position.x -= Width(text, size, true); }
    position = { std::round(position.x), std::round(position.y) };
    draw->AddText(Font(true), size, { position.x, position.y + 1.0f }, IM_COL32(0, 0, 0, 210), text);
    draw->AddText(Font(true), size, position, SurfaceColor(color), text);
}

// ImGuiは頂点色をそのまま出力し、描画先がsRGBへ変換する。
// 新しい計器の指定色だけを線形化し、暗部が灰色へ持ち上がるのを防ぐ。
// タイトルや既存VFXの色・レンダリング設定は変更しない。
inline ImU32 SurfaceColor(ImU32 srgb)
{
    static const std::array<ImU32, 256> linear = [] {
        std::array<ImU32, 256> values{};
        for (size_t i = 0; i < values.size(); ++i) {
            const float c = static_cast<float>(i) / 255.0f;
            const float value = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
            values[i] = static_cast<ImU32>(std::round(value * 255.0f));
        }
        return values;
    }();
    return (srgb & IM_COL32_A_MASK) |
        (linear[(srgb >> IM_COL32_R_SHIFT) & 255] << IM_COL32_R_SHIFT) |
        (linear[(srgb >> IM_COL32_G_SHIFT) & 255] << IM_COL32_G_SHIFT) |
        (linear[(srgb >> IM_COL32_B_SHIFT) & 255] << IM_COL32_B_SHIFT);
}

inline ImU32 Mix(ImU32 from, ImU32 to, float rate)
{
    ImU32 result = 0;
    for (const int shift : { IM_COL32_R_SHIFT, IM_COL32_G_SHIFT, IM_COL32_B_SHIFT, IM_COL32_A_SHIFT }) {
        const float a = static_cast<float>((from >> shift) & 255);
        const float b = static_cast<float>((to >> shift) & 255);
        result |= static_cast<ImU32>(std::round(a + (b - a) * rate)) << shift;
    }
    return result;
}

// タイトルと同じ前傾を数字にも通す。幅を指定した欄では長い値も収める。
inline void Slant(ImDrawList* draw, ImVec2 at, float size, ImU32 color,
    const char* text, bool right = false, bool shadow = true, float rise = 0.0f, float maxWidth = 0.0f)
{
    if (maxWidth > 0.0f) {
        const float width = (std::max)(Width(text, size, true) + size * 0.19f, 1.0f);
        size *= (std::min)(1.0f, maxWidth / width);
    }
    const float shear = size * 0.19f;
    if (right) { at.x -= Width(text, size, true) + shear; }
    const auto pass = [&](ImVec2 position, ImU32 ink) {
        const int first = draw->VtxBuffer.Size;
        draw->AddText(Font(true), size, position, ink, text);
        for (int index = first; index < draw->VtxBuffer.Size; ++index) {
            auto& vertex = draw->VtxBuffer[index];
            vertex.pos.x += (position.y + size - vertex.pos.y) * 0.19f;
            vertex.pos.y -= (vertex.pos.x - position.x) * rise;
        }
    };
    if (shadow) { pass({ at.x + 2, at.y + 2 }, IM_COL32(0, 0, 0, 180)); }
    pass(at, SurfaceColor(color));
}

// 機体の後退翼と同じ傾き。枠を増やさず、残量そのものを長い刃として描く。
inline void WingMeter(ImDrawList* draw, ImVec2 at, float width, float thickness,
    float rate, ImU32 color, float scale, float shine = -1.0f, float rainbowTime = -1.0f)
{
    rate = std::clamp(rate, 0.0f, 1.0f);
    const auto p = [&](float x, float y) { return ImVec2(at.x + x, at.y + y - x * 0.075f); };
    const auto blade = [&](float length, ImU32 ink) {
        if (length <= 0) { return; }
        const float cut = (std::min)(length * 0.25f, 10 * scale);
        const ImVec2 points[] = { p(cut, 0), p(length, 0), p(length - cut, thickness), p(0, thickness) };
        draw->AddConvexPolyFilled(points, 4, SurfaceColor(ink));
    };
    blade(width, IM_COL32(10, 17, 28, 235));
    const int first = draw->VtxBuffer.Size;
    const float length = width * rate;
    blade(length, color);
    for (int index = first; index < draw->VtxBuffer.Size; ++index) {
        auto& vertex = draw->VtxBuffer[index];
        const float t = std::clamp((vertex.pos.x - at.x) / (std::max)(length, 1.0f), 0.0f, 1.0f);
        ImU32 tint = color;
        if (rainbowTime >= 0) {
            float r{}, g{}, b{};
            ImGui::ColorConvertHSVtoRGB(std::fmod(rainbowTime + t * 0.68f, 1.0f), 0.72f, 1.0f, r, g, b);
            tint = IM_COL32(static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255), 255);
        }
        const ImU32 ink = SurfaceColor(Mix(Mix(tint, IM_COL32(10, 17, 28, 255), 0.35f), tint, t));
        vertex.col = (ink & ~IM_COL32_A_MASK) | (vertex.col & IM_COL32_A_MASK);
    }
    if (length > 0) {
        draw->AddLine(p((std::min)(4 * scale, length * 0.2f), scale), p(length, scale),
            SurfaceColor(Mix(color, White, 0.48f)), scale);
    }
    if (shine >= 0 && rate > 0.98f) {
        const float x = std::clamp(shine, 0.0f, 1.0f) * (width - 16 * scale);
        draw->AddQuadFilled(p(x + 8 * scale, 0), p(x + 16 * scale, 0),
            p(x + 8 * scale, thickness), p(x, thickness), SurfaceColor(IM_COL32(239, 241, 244, 115)));
    }
}

// 下地は端で消える。情報ごとに箱を作らず、空と建物から文字だけを分離する。
inline void Wake(ImDrawList* draw, ImVec2 min, ImVec2 max, bool right = false)
{
    const ImU32 dark = IM_COL32(0, 3, 9, 168), clear = IM_COL32(0, 3, 9, 0);
    const float middle = min.y + (max.y - min.y) * 0.5f;
    const ImU32 left = right ? clear : dark, edge = right ? dark : clear;
    draw->AddRectFilledMultiColor(min, { max.x, middle }, clear, clear, edge, left);
    draw->AddRectFilledMultiColor({ min.x, middle }, max, left, edge, clear, clear);
}

inline void Plate(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale, int alpha = 226)
{
    const float cut = (std::min)(8 * scale, (std::min)(max.x - min.x, max.y - min.y) * 0.25f);
    const ImVec2 points[] = { min, { max.x - cut, min.y }, { max.x, min.y + cut },
        max, { min.x + cut, max.y }, { min.x, max.y - cut } };
    draw->AddConvexPolyFilled(points, 6, SurfaceColor(IM_COL32(15, 19, 25, alpha)));
}

inline void KeyBadge(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale,
    const char* key, bool ready)
{
    const ImU32 color = ready ? Energy : Muted;
    draw->AddRectFilled(min, max, SurfaceColor(IM_COL32(39, 45, 57, 235)), 2 * scale);
    draw->AddRect(min, max, SurfaceColor(Mix(color, IM_COL32(39, 45, 57, 255), 0.45f)), 2 * scale, 0, scale);
    const float size = 15 * scale;
    Readout(draw, { min.x + (max.x - min.x - ReadoutWidth(key, size)) * 0.5f,
        min.y + (max.y - min.y - size) * 0.5f }, size, ready ? White : Muted, key);
}

inline void Panel(ImDrawList* draw, ImVec2 min, ImVec2 max, float scale)
{
    Plate(draw, min, max, scale, 244);
}

// 計器は枠を重ねず、残量と先端だけを明確にする。全ゲージで同じ断面を使う。
inline void Meter(ImDrawList* draw, ImVec2 min, ImVec2 max, float rate, ImU32 color, float scale)
{
    const ImU32 troughTop = SurfaceColor(IM_COL32(5, 9, 15, 245));
    const ImU32 troughBottom = SurfaceColor(IM_COL32(43, 48, 58, 245));
    draw->AddRectFilledMultiColor(min, max, troughTop, troughTop, troughBottom, troughBottom);
    const ImVec2 inner(min.x, min.y + scale);
    const ImVec2 end(max.x, max.y - scale);
    rate = std::clamp(rate, 0.0f, 1.0f);
    if (rate <= 0.0f || end.x <= inner.x || end.y <= inner.y) { return; }
    const float edge = inner.x + (end.x - inner.x) * rate;
    const ImU32 left = Mix(color, IM_COL32(6, 14, 28, 255), 0.38f);
    const ImU32 right = color;
    const float middle = inner.y + (end.y - inner.y) * 0.46f;
    draw->AddRectFilledMultiColor(inner, { edge, middle },
        SurfaceColor(Mix(left, White, 0.26f)), SurfaceColor(Mix(right, White, 0.34f)),
        SurfaceColor(right), SurfaceColor(left));
    draw->AddRectFilledMultiColor({ inner.x, middle }, { edge, end.y },
        SurfaceColor(left), SurfaceColor(right),
        SurfaceColor(Mix(right, IM_COL32(0, 0, 0, 255), 0.28f)),
        SurfaceColor(Mix(left, IM_COL32(0, 0, 0, 255), 0.28f)));
    draw->AddLine(inner, { edge, inner.y }, SurfaceColor(Mix(color, White, 0.38f)), scale);
    draw->AddLine({ edge, inner.y }, { edge, end.y }, SurfaceColor(Mix(color, White, 0.48f)), scale);
}

inline void ShipIcon(ImDrawList* draw, ImVec2 center, float scale, ImU32 color)
{
    // 自機の正面シルエット。架空の残機数を表示せず、体力の所属だけを伝える。
    draw->AddTriangleFilled({ center.x, center.y - 17 * scale },
        { center.x + 5 * scale, center.y + 13 * scale }, { center.x - 5 * scale, center.y + 13 * scale }, color);
    for (const float side : { -1.0f, 1.0f }) {
        draw->AddTriangleFilled({ center.x + side * 4 * scale, center.y - 3 * scale },
            { center.x + side * 23 * scale, center.y + 9 * scale },
            { center.x + side * 5 * scale, center.y + 8 * scale }, color);
    }
}

inline void Segments(ImDrawList* draw, ImVec2 min, ImVec2 max,
    float rate, ImU32 color, int count, float gap)
{
    const float width = (max.x - min.x - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
    for (int index = 0; index < count; ++index) {
        const float x = min.x + static_cast<float>(index) * (width + gap);
        draw->AddRectFilled({ x, min.y }, { x + width, max.y }, Track);
        const float filled = std::clamp(rate * static_cast<float>(count) - static_cast<float>(index), 0.0f, 1.0f);
        if (filled > 0.0f) { draw->AddRectFilled({ x, min.y }, { x + width * filled, max.y }, color); }
    }
}

inline void BladeIcon(ImDrawList* draw, ImVec2 center, float scale, ImU32 color)
{
    // 二本の刃を機体の翼と同じ鋭い形で描く。文字ではなく技を識別する目印。
    for (const float offset : { -7.0f, 7.0f }) {
        const ImVec2 blade[] = {
            { center.x + (offset - 9.0f) * scale, center.y + 12.0f * scale },
            { center.x + (offset + 11.0f) * scale, center.y - 16.0f * scale },
            { center.x + (offset + 7.0f) * scale, center.y + 1.0f * scale },
            { center.x + (offset - 5.0f) * scale, center.y + 14.0f * scale }
        };
        draw->AddConvexPolyFilled(blade, 4, color);
        draw->AddLine({ center.x + (offset - 10.0f) * scale, center.y + 10.0f * scale },
            { center.x + (offset + 1.0f) * scale, center.y + 17.0f * scale }, color, 2.0f * scale);
    }
}
} // namespace CombatHud
