#include "engine/scene/TitleWordmark.h"

#include "app/ui/CombatHud.h"
#include "engine/scene/TitleMotion.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <stdexcept>

using namespace TitleMotion;

namespace {
ImU32 DepartureMetalColor(float heat, bool core)
{
    ImU32 tint = CombatHud::Mix(IM_COL32(180,35,12,255),
        IM_COL32(255,143,31,255),Smooth(heat / 0.55f));
    return core ? CombatHud::Mix(tint,IM_COL32(255,251,222,255),
        Smooth((heat - 0.08f) / 0.40f)) : tint;
}

constexpr const char* kTitle = "AZRAID";
constexpr const char* kTitleJapanese = "アズレイド";
constexpr float kKanaSize = 16.0f;
constexpr ImVec2 kKanaOrigin{8.0f, 108.0f};

struct LogoPiece {
    std::array<ImVec2, 16> points{};
    int count = 0;
    int half = 0;
    int glyph = 0;
    size_t edgeStart = 0;
    size_t edgeCount = 0;
    float contactX = 0.0f;
    float contactRightX = 0.0f;
};
struct LogoEdge {
    ImVec2 start{}, end{};
    bool cut = false;
};
struct LogoGeometry {
    std::array<LogoPiece, 64> pieces{};
    size_t count = 0;
    std::array<LogoEdge, 1024> edges{};
    size_t edgeCount = 0;
};

// ロゴのポリゴンから、切断面と外周の縁を描くための辺を抽出する。
void BuildLogoEdges(LogoGeometry& geometry, int pattern)
{
    const auto cut = GetTitleCut(pattern);
    const auto cross = [](ImVec2 a, ImVec2 b) { return a.x * b.y - a.y * b.x; };
    const auto onCut = [&](ImVec2 point) {
        return std::abs(std::abs(point.y - cut.base - point.x * cut.slope) - 0.65f) < 0.005f;
    };
    for (size_t index = 0; index < geometry.count; ++index) {
        auto& piece = geometry.pieces[index];
        piece.edgeStart = geometry.edgeCount;
        for (int vertex = 0; vertex < piece.count; ++vertex) {
            const auto a = piece.points[vertex], b = piece.points[(vertex + 1) % piece.count];
            const ImVec2 edge{b.x - a.x,b.y - a.y};
            const float length = std::hypot(edge.x,edge.y);
            if (length <= 0.001f) { continue; }
            // 同じ文字の別ストロークに埋まる辺を除く。共有境界は外側へ少し出して判定。
            const ImVec2 probe{a.x + edge.y / length * 0.01f,a.y - edge.x / length * 0.01f};
            std::array<std::array<float,2>,64> covered{};
            size_t coveredCount = 0;
            for (size_t otherIndex = 0; otherIndex < geometry.count; ++otherIndex) {
                const auto& other = geometry.pieces[otherIndex];
                if (otherIndex == index || other.glyph != piece.glyph || other.half != piece.half) { continue; }
                float begin = 0.0f, end = 1.0f;
                for (int otherVertex = 0; otherVertex < other.count; ++otherVertex) {
                    const auto p = other.points[otherVertex], q = other.points[(otherVertex + 1) % other.count];
                    const ImVec2 side{q.x - p.x,q.y - p.y};
                    const float startDistance = cross(side,{probe.x - p.x,probe.y - p.y});
                    const float endDistance = startDistance + cross(side,edge);
                    if (startDistance < 0.0f && endDistance < 0.0f) { end = begin; break; }
                    if ((startDistance < 0.0f) != (endDistance < 0.0f)) {
                        const float t = startDistance / (startDistance - endDistance);
                        if (startDistance < 0.0f) { begin = (std::max)(begin,t); }
                        else { end = (std::min)(end,t); }
                    }
                    if (end <= begin) { break; }
                }
                if (end > begin) { covered[coveredCount++] = {begin,end}; }
            }
            std::sort(covered.begin(),covered.begin() + coveredCount,
                [](const auto& aRange, const auto& bRange) { return aRange[0] < bRange[0]; });
            const auto append = [&](float begin, float end) {
                if ((end - begin) * length <= 0.01f) { return; }
                if (geometry.edgeCount >= geometry.edges.size()) { throw std::runtime_error("Title logo edge capacity exceeded"); }
                geometry.edges[geometry.edgeCount++] = {{a.x + edge.x * begin,a.y + edge.y * begin},
                    {a.x + edge.x * end,a.y + edge.y * end},piece.half >= 0 && onCut(a) && onCut(b)};
            };
            float cursor = 0.0f;
            for (size_t range = 0; range < coveredCount; ++range) {
                append(cursor,covered[range][0]);
                cursor = (std::max)(cursor,covered[range][1]);
            }
            append(cursor,1.0f);
        }
        piece.edgeCount = geometry.edgeCount - piece.edgeStart;
    }
}

// 文字の頂点を生成し、斬撃パターンに応じてロゴの断片を作る。
LogoGeometry CreateLogoGeometry(int pattern = 0, bool whole = false)
{
    LogoGeometry geometry;
    const auto cut = GetTitleCut(pattern);
    float pen = 0.0f;
    int glyph = 0;
    const auto polygon = [&](std::initializer_list<ImVec2> shape) {
        std::array<ImVec2, 12> source{};
        int count = 0;
        for (const auto point : shape) {
            source[count++] = { pen + point.x + (100.0f - point.y) * 0.19f, point.y };
        }
        for (int half = 0; half < 2; ++half) {
            LogoPiece piece;
            piece.half = half;
            piece.glyph = glyph;
            const float letterX = pen + 4.0f;
            piece.contactX = 195.0f + ProjectLogoCanvas({letterX,cut.base + letterX * cut.slope},
                0.0f,half,0.0f).x * 1.75f;
            for (int vertex = 0; vertex < count; ++vertex) {
                piece.contactRightX = (std::max)(piece.contactRightX,
                    195.0f + ProjectLogoCanvas(source[vertex],0.0f,half,0.0f).x * 1.75f);
            }
            if (whole) {
                piece.half = -1;
                piece.count = count;
                std::copy_n(source.begin(),count,piece.points.begin());
                geometry.pieces[geometry.count++] = piece;
                break;
            }
            const auto distance = [&](ImVec2 point) {
                const float distanceToCut = point.y - cut.base - point.x * cut.slope;
                return half == 0 ? -distanceToCut - 0.65f : distanceToCut - 0.65f;
            };
            for (int index = 0; index < count; ++index) {
                const auto a = source[index], b = source[(index + 1) % count];
                const float da = distance(a), db = distance(b);
                if (da >= 0.0f) { piece.points[piece.count++] = a; }
                if ((da >= 0.0f) != (db >= 0.0f)) {
                    const float t = da / (da - db);
                    piece.points[piece.count++] = { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
                }
            }
            if (piece.count >= 3) { geometry.pieces[geometry.count++] = piece; }
        }
    };
    for (const char* letter = kTitle; *letter; ++letter) {
        switch (*letter) {
        case 'A':
            polygon({ {0,100}, {35,0}, {55,0}, {26,100} });
            polygon({ {43,0}, {58,0}, {90,100}, {65,100} });
            polygon({ {25,59}, {64,53}, {70,72}, {19,79} });
            pen += 93.0f;
            break;
        case 'Z':
            // 隣のA・Rへ張り出さず、斜線の外郭と上下の棒を同じ輪郭で繋ぐ。
            // 継ぎ目は輪郭の内側で2単位重ね、AAの細い隙間も残さない。
            polygon({ {0,0}, {88,0}, {84,20}, {0,20} });
            polygon({ {61.8667f,18}, {84.4f,18}, {84,20}, {28,80}, {4,80} });
            polygon({ {5.8667f,78}, {84.4f,78}, {80,100}, {4,100}, {4,80} });
            pen += 90.0f;
            break;
        case 'R':
            polygon({ {0,0}, {22,0}, {22,100}, {0,100} });
            polygon({ {21,0}, {65,0}, {82,18}, {21,18} });
            polygon({ {61,17}, {82,17}, {82,43}, {61,43} });
            polygon({ {21,42}, {82,42}, {63,61}, {21,61} });
            polygon({ {36,58}, {62,58}, {90,100}, {64,100} });
            pen += 96.0f;
            break;
        case 'I':
            polygon({ {3,0}, {27,0}, {21,100}, {-3,100} });
            pen += 37.0f;
            break;
        case 'D':
            polygon({ {0,0}, {22,0}, {22,100}, {0,100} });
            polygon({ {21,0}, {62,0}, {84,20}, {63,20}, {21,18} });
            polygon({ {63,19}, {84,20}, {84,78}, {63,82} });
            polygon({ {21,81}, {84,78}, {62,100}, {21,100} });
            pen += 92.0f;
            break;
        }
        ++glyph;
    }
    // 逆方向でも一文字の全ストロークを同時に斬る。棒ごとの右端を使うと、
    // 一部だけ未切断のまま残り、同じ文字内で分離・反動の時計がずれる。
    for (size_t index = 0; index < geometry.count; ++index) {
        auto& piece = geometry.pieces[index];
        for (size_t other = 0; other < geometry.count; ++other) {
            if (geometry.pieces[other].glyph == piece.glyph) {
                piece.contactRightX = (std::max)(piece.contactRightX,geometry.pieces[other].contactRightX);
            }
        }
    }
    BuildLogoEdges(geometry,pattern);
    return geometry;
}
} // namespace

// ロゴの断片に飛行と切断の動きを反映し、輪郭・奥行き・切断面の光を重ねて描く。
void DrawTitleWordmark(ImDrawList* draw, ImVec2 origin, float scale, float elapsed, const TitleFlyby& flyby,
    float departure, ImTextureID surface)
{
    static const std::array<LogoGeometry,4> geometries{CreateLogoGeometry(),CreateLogoGeometry(2),
        CreateLogoGeometry(0,true),CreateLogoGeometry(2,true)};
    const auto& geometry = geometries[flyby.cutPattern == 2 ? 1 : 0];
    const auto& wholeGeometry = geometries[flyby.cutPattern == 2 ? 3 : 2];
    const auto cutLine = GetTitleCut(flyby.cutPattern);
    const float idleTime = (std::max)(0.0f, elapsed - 2.10f);
    const float idle = Smooth(idleTime / 1.40f);
    const TitleStrike strike = departure > 0.0f ? GetDepartureStrike(departure) : GetTitleStrike(flyby);
    const LogoMotion motion = GetLogoMotion(elapsed, flyby, departure);
    float separation = LogoSeparation(flyby);
    float cutPresence = 0.0f;
    const auto onCut = [&](ImVec2 point) {
        return std::abs(std::abs(point.y - cutLine.base - point.x * cutLine.slope) - 0.65f) < 0.005f;
    };
    const auto position = [&](ImVec2 local, int half, float depth) {
        if (half >= 0 && onCut(local)) {
            const float side = half == 0 ? -1.0f : 1.0f;
            local.y -= side * 0.65f * (1.0f - cutPresence);
        }
        const auto canvas = ProjectLogoCanvas(local, separation, half, depth,flyby.pattern);
        const auto moved = MoveLogoCanvas({195.0f + canvas.x * 1.75f,
            247.0f + canvas.y * 1.75f}, motion);
        const auto offset = DepartureOffset(departure, half);
        return ImVec2{ origin.x + (moved.x + offset.x - 195.0f) / 1.75f * scale,
            origin.y + (moved.y + offset.y - 247.0f) / 1.75f * scale };
    };
    const float lightX = -220.0f + 1720.0f * std::fmod(idleTime / 3.60f + 0.35f, 1.0f);
    const auto cutLight = [&](ImVec2 screen) {
        // 投影後の画面位置を使い、実機の通過と背景・文字の反射を揃える。
        const float canvasX = 195.0f + (screen.x - origin.x) / scale * 1.75f;
        const float distance = (canvasX - lightX) / 130.0f;
        const float passed = (strike.bladeX - canvasX) * (departure > 0.0f ? 1.0f : TitleDashDirection(flyby.pattern));
        const float flybyDistance = passed / 95.0f;
        const float wake = passed > 0.0f ? std::exp(-passed / 410.0f) * 0.50f : 0.0f;
        return std::clamp(std::exp(-distance * distance) * idle * 0.32f +
            (std::exp(-flybyDistance * flybyDistance) + wake) * strike.blade +
            strike.charge * 0.18f + strike.flash * 0.72f + strike.afterglow * 0.16f, 0.0f, 1.0f);
    };
    // 側面を先にまとめて描く。隣り合う文字ストロークの側面が正面へ重ならない。
    for (int pass = 0; pass < 3; ++pass) {
        if (pass == 2) { draw->PushTexture(surface); }
        for (size_t index = 0; index < geometry.count + wholeGeometry.count; ++index) {
            const bool whole = index >= geometry.count;
            const auto& currentGeometry = whole ? wholeGeometry : geometry;
            const auto& piece = currentGeometry.pieces[whole ? index - geometry.count : index];
            const float contactX = departure <= 0.0f && flyby.pattern == 1 ? piece.contactRightX : piece.contactX;
            cutPresence = departure > 0.0f ?
                (departure >= kDepartureCutTime ? Smooth((strike.bladeX - contactX) / 45.0f) : 0.0f) :
                LogoCutPresence(flyby,contactX);
            // 正面は一枚だけ描く。閉じ際に未切断の文字を重ねると残像に見える。
            const bool opened = cutPresence > 0.001f;
            if (whole == opened) { continue; }
            separation = whole ? 0.0f : LogoSeparation(flyby);
            float centerX = 0.0f;
            for (int vertex = 0; vertex < piece.count; ++vertex) { centerX += piece.points[vertex].x; }
            centerX /= static_cast<float>(piece.count);
            const float reveal = Smooth((elapsed - 0.28f - centerX * 0.0013f) / 0.25f);
            if (reveal <= 0.001f) { continue; }
            const auto color = [&](ImU32 value) {
                const int alpha = static_cast<int>(static_cast<float>((value >> IM_COL32_A_SHIFT) & 255) * reveal);
                return CombatHud::SurfaceColor((value & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT));
            };
            std::array<ImVec2, 16> front{};
            for (int vertex = 0; vertex < piece.count; ++vertex) {
                front[vertex] = position(piece.points[vertex], piece.half, 0.0f);
            }
            if (pass == 0) {
                std::array<ImVec2, 16> back{};
                for (int vertex = 0; vertex < piece.count; ++vertex) {
                    back[vertex] = position(piece.points[vertex], piece.half, 1.25f);
                }
                draw->AddConvexPolyFilled(back.data(), piece.count, color(IM_COL32(12,21,43,255)));
                for (size_t edge = piece.edgeStart; edge < piece.edgeStart + piece.edgeCount; ++edge) {
                    const auto& contour = currentGeometry.edges[edge];
                    if (contour.cut) { continue; }
                    const auto a = position(contour.start,piece.half,0.0f);
                    const auto b = position(contour.end,piece.half,0.0f);
                    const auto backA = position(contour.start,piece.half,1.25f);
                    const auto backB = position(contour.end,piece.half,1.25f);
                    // 正面の奥に隠れる側面は描かない。可視面の頂点順も時計回りに揃う。
                    if ((backA.x - a.x) * (b.y - a.y) - (backA.y - a.y) * (b.x - a.x) <= 0.0f) { continue; }
                    const std::array<ImVec2, 4> side{a,backA,backB,b};
                    draw->AddConvexPolyFilled(side.data(), 4, color(IM_COL32(29,47,80,255)));
                }
            } else if (pass == 1) {
                if (whole) { continue; }
                for (size_t edge = piece.edgeStart; edge < piece.edgeStart + piece.edgeCount; ++edge) {
                    const auto& contour = currentGeometry.edges[edge];
                    if (!contour.cut) { continue; }
                    const auto a = position(contour.start,piece.half,0.0f);
                    const auto b = position(contour.end,piece.half,0.0f);
                    const std::array<ImVec2, 4> cut{a,position(contour.start,piece.half,0.72f),
                        position(contour.end,piece.half,0.72f),b};
                    const ImVec2 localMidpoint{(contour.start.x + contour.end.x)*0.5f,
                        (contour.start.y + contour.end.y)*0.5f};
                    const auto contact = ProjectLogoCanvas(localMidpoint,0.0f,piece.half,0.0f,flyby.pattern);
                    const float heatAge = departure - DepartureContactTime(195.0f + contact.x*1.75f);
                    const float heatPresence = departure >= kDepartureCutTime && heatAge >= 0.0f ?
                        Smooth(heatAge / 0.008f) * (1.0f - Smooth((heatAge - 0.28f) / 0.55f)) : 0.0f;
                    const ImU32 heatTint = DepartureMetalColor(std::exp(-(std::max)(0.0f,heatAge) / 0.17f),true);
                    const int first = draw->VtxBuffer.Size;
                    draw->AddConvexPolyFilled(cut.data(), 4, IM_COL32_WHITE);
                    for (int cutVertex = first; cutVertex < draw->VtxBuffer.Size; ++cutVertex) {
                        auto& value = draw->VtxBuffer[cutVertex];
                        ImU32 tint = CombatHud::Mix(IM_COL32(91,125,192,255),
                            IM_COL32(242,248,255,255), cutLight(value.pos));
                        tint = CombatHud::Mix(tint,heatTint,heatPresence);
                        const ImU32 aa = (value.col >> IM_COL32_A_SHIFT) & 255;
                        const ImU32 alpha = static_cast<ImU32>(static_cast<float>(aa) * reveal * cutPresence);
                        value.col = (CombatHud::SurfaceColor(tint) & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
                    }
                    const ImVec2 midpoint{(a.x + b.x) * 0.5f,(a.y + b.y) * 0.5f};
                    draw->AddLine(a, b, color(CombatHud::Mix(
                        IM_COL32(133,155,184,static_cast<int>(255.0f * cutPresence)),
                        IM_COL32(255,255,255,static_cast<int>(255.0f * cutPresence)), cutLight(midpoint))), 1.0f * scale);
                }
            } else {
                const int first = draw->VtxBuffer.Size;
                draw->AddConvexPolyFilled(front.data(), piece.count, IM_COL32_WHITE);
                for (int vertex = first; vertex < draw->VtxBuffer.Size; ++vertex) {
                    auto& value = draw->VtxBuffer[vertex];
                    const auto offset = DepartureOffset(departure, piece.half);
                    const ImVec2 neutralScreen{value.pos.x - offset.x / 1.75f * scale,
                        value.pos.y - offset.y / 1.75f * scale};
                    const float metalX = 195.0f + (neutralScreen.x - origin.x) / scale * 1.75f;
                    // 共通の画面座標で面全体をサンプリング。ストロークごとの頂点補間で明暗を切らない。
                    value.uv = {0.5f + (metalX - lightX) / 4096.0f,
                        (neutralScreen.y - origin.y) / (100.0f * scale)};
                    // 衝撃面が白く開く数フレームだけ、正面を鋼のシルエットへ落とす。
                    const float silhouette = departure > 0.0f ? DepartureImpact(departure) *
                        (1.0f - Smooth((departure - kDepartureImpactTime - 0.040f) / 0.025f)) : 0.0f;
                    const int shade = static_cast<int>(255.0f * (1.0f - silhouette * 0.82f) *
                        (1.0f - strike.charge * (departure > 0.0f ? 0.32f : 0.0f)) *
                        (1.0f - (!whole && piece.half == 1 ? cutPresence * 0.18f : 0.0f)));
                    const ImU32 aa = (value.col >> IM_COL32_A_SHIFT) & 255;
                    const ImU32 alpha = static_cast<ImU32>(static_cast<float>(aa) * reveal);
                    value.col = IM_COL32(shade,shade,shade,alpha);
                }
            }
        }
        if (pass == 2) { draw->PopTexture(); }
    }

    // 読みは自然な字形を保つ。追加の斜体・立体化はせず、英字の下の面と投影・動きを共有する。
    const float kanaReveal = Smooth((elapsed - 0.85f) / 0.35f);
    if (kanaReveal > 0.001f) {
        cutPresence = 0.0f;
        separation = LogoSeparation(flyby);
        const int first = draw->VtxBuffer.Size;
        draw->AddText(CombatHud::HeadingFont(), kKanaSize * scale, origin,
            CombatHud::SurfaceColor(IM_COL32(211,220,233,static_cast<int>(255.0f * kanaReveal))), kTitleJapanese);
        for (int index = first; index < draw->VtxBuffer.Size; ++index) {
            auto& vertex = draw->VtxBuffer[index];
            const ImVec2 local{kKanaOrigin.x + (vertex.pos.x - origin.x) / scale,
                kKanaOrigin.y + (vertex.pos.y - origin.y) / scale};
            vertex.pos = position(local, 1, 0.0f);
        }
    }

    // 出撃だけは線ではなく、鋭い先端と厚い腹を持つ刃で画面を斬る。
    // 全て固定領域。待機中の斬撃と噴射には影響させない。
    if (departure > 0.0f) {
        const float cutTime = departure - kDepartureCutTime;
        const float strength = strike.blade;
        const float impact = DepartureImpact(departure);
        const float resistance = DepartureResistance(departure);
        const auto screen = [&](ImVec2 canvas) {
            const auto moved = MoveLogoCanvas(canvas,motion);
            return ImVec2{origin.x + (moved.x - 195.0f) / 1.75f * scale,
                origin.y + (moved.y - 247.0f) / 1.75f * scale};
        };
        constexpr int count = 33;
        std::array<ImVec2,count> blade{};
        const float head = (std::min)(strike.bladeX,1450.0f);
        const float tail = head - 1580.0f;
        for (int index = 0; index < count; ++index) {
            const float x = tail + (head - tail) * static_cast<float>(index) / (count - 1);
            blade[index] = screen({x,TitleCutY(x,flyby.cutPattern)});
        }
        const auto ribbon = [&](float width, ImU32 ink) {
            // 一枚の共有頂点メッシュにする。各区間をAA付きの四角として描くと、
            // 重なった境界が光の板の継ぎ目になってしまう。
            const ImU32 solid = CombatHud::SurfaceColor(ink);
            const ImU32 clear = solid & ~IM_COL32_A_MASK;
            const ImVec2 uv = ImGui::GetIO().Fonts->TexUvWhitePixel;
            draw->PrimReserve((count - 1)*18,count*4);
            const unsigned int base = draw->_VtxCurrentIdx;
            for (int index = 0; index < count - 1; ++index) {
                for (int band = 0; band < 3; ++band) {
                    const unsigned int a = base + static_cast<unsigned int>(index*4 + band);
                    const unsigned int b = a + 4;
                    draw->PrimWriteIdx(static_cast<ImDrawIdx>(a));
                    draw->PrimWriteIdx(static_cast<ImDrawIdx>(b));
                    draw->PrimWriteIdx(static_cast<ImDrawIdx>(b + 1));
                    draw->PrimWriteIdx(static_cast<ImDrawIdx>(a));
                    draw->PrimWriteIdx(static_cast<ImDrawIdx>(b + 1));
                    draw->PrimWriteIdx(static_cast<ImDrawIdx>(a + 1));
                }
            }
            for (int index = 0; index < count; ++index) {
                const float t = static_cast<float>(index) / (count - 1);
                const float halfWidth = width * scale / 1.75f *
                    std::pow((std::max)(0.0f,std::sin(t * 3.1415927f)),0.70f) * (0.30f + t * 0.70f);
                const auto a = blade[(std::max)(0,index - 1)], b = blade[(std::min)(count - 1,index + 1)];
                const float dx = b.x - a.x, dy = b.y - a.y;
                const float length = (std::max)(0.01f,std::sqrt(dx*dx + dy*dy));
                const ImVec2 normal{-dy/length,dx/length};
                const auto center = blade[index];
                draw->PrimWriteVtx({center.x + normal.x*(halfWidth + 1.0f),
                    center.y + normal.y*(halfWidth + 1.0f)},uv,clear);
                draw->PrimWriteVtx({center.x + normal.x*halfWidth,center.y + normal.y*halfWidth},uv,solid);
                draw->PrimWriteVtx({center.x - normal.x*halfWidth,center.y - normal.y*halfWidth},uv,solid);
                draw->PrimWriteVtx({center.x - normal.x*(halfWidth + 1.0f),
                    center.y - normal.y*(halfWidth + 1.0f)},uv,clear);
            }
        };
        if (strength > 0.001f) {
            ribbon((228.0f + impact*90.0f)*(1.0f - resistance*0.58f),IM_COL32(109,145,202,static_cast<int>(50.0f*strength)));
            ribbon((78.0f + impact*36.0f)*(1.0f - resistance*0.65f),IM_COL32(4,8,17,static_cast<int>(232.0f*strength)));
            ribbon((65.0f + impact*32.0f)*(1.0f - resistance*0.55f),IM_COL32(181,207,243,static_cast<int>(225.0f*strength)));
            ribbon((29.0f + impact*23.0f)*(1.0f - resistance*0.75f),IM_COL32(255,255,255,static_cast<int>(255.0f*strength)));
        }
        // 出撃時だけ、実際の文字の切り口を加熱する。背景へ一本の暖色線は残さない。
        // 固定の輪郭と頂点列を使い、GPU粒子・追加画像・毎フレームの独自確保は増やさない。
        if (cutTime >= 0.0f && departure < kDepartureDuration - 0.25f) {
            for (size_t pieceIndex = 0; pieceIndex < geometry.count; ++pieceIndex) {
                const auto& piece = geometry.pieces[pieceIndex];
                cutPresence = Smooth((strike.bladeX - piece.contactX) / 45.0f);
                if (cutPresence <= 0.001f) { continue; }
                separation = LogoSeparation(flyby);
                ImVec2 localCenter{};
                for (int vertex = 0; vertex < piece.count; ++vertex) {
                    localCenter.x += piece.points[vertex].x;
                    localCenter.y += piece.points[vertex].y;
                }
                localCenter.x /= static_cast<float>(piece.count);
                localCenter.y /= static_cast<float>(piece.count);
                const ImVec2 faceCenter = position(localCenter,piece.half,0.0f);
                for (size_t edgeIndex = piece.edgeStart; edgeIndex < piece.edgeStart + piece.edgeCount; ++edgeIndex) {
                    const auto& edge = geometry.edges[edgeIndex];
                    if (!edge.cut) { continue; }
                    constexpr int samples = 9;
                    std::array<ImVec2,samples> points{};
                    std::array<float,samples> heat{}, opacity{};
                    for (int sample = 0; sample < samples; ++sample) {
                        const float t = static_cast<float>(sample) / (samples - 1);
                        const ImVec2 local{edge.start.x + (edge.end.x - edge.start.x)*t,
                            edge.start.y + (edge.end.y - edge.start.y)*t};
                        points[sample] = position(local,piece.half,0.0f);
                        const auto canvas = ProjectLogoCanvas(local,0.0f,piece.half,0.0f,flyby.pattern);
                        const float age = departure - DepartureContactTime(195.0f + canvas.x*1.75f);
                        if (age < 0.0f) { continue; }
                        heat[sample] = std::exp(-age / 0.17f);
                        opacity[sample] = Smooth(age / 0.008f) *
                            (1.0f - Smooth((age - 0.28f) / 0.55f)) * cutPresence;
                    }
                    const ImVec2 axis{points.back().x - points.front().x,points.back().y - points.front().y};
                    const float length = (std::max)(0.01f,std::hypot(axis.x,axis.y));
                    ImVec2 inward{-axis.y/length,axis.x/length};
                    const ImVec2 midpoint{(points.front().x + points.back().x)*0.5f,
                        (points.front().y + points.back().y)*0.5f};
                    if ((faceCenter.x - midpoint.x)*inward.x + (faceCenter.y - midpoint.y)*inward.y < 0.0f) {
                        inward.x = -inward.x; inward.y = -inward.y;
                    }
                    const auto band = [&](float outer, float inner, float outerAlpha, float innerAlpha, bool core) {
                        const ImVec2 uv = ImGui::GetIO().Fonts->TexUvWhitePixel;
                        draw->PrimReserve((samples - 1)*6,samples*2);
                        const unsigned int base = draw->_VtxCurrentIdx;
                        for (int sample = 0; sample < samples - 1; ++sample) {
                            const auto a = base + static_cast<unsigned int>(sample*2);
                            for (const unsigned int index : {a,a+2,a+3,a,a+3,a+1}) {
                                draw->PrimWriteIdx(static_cast<ImDrawIdx>(index));
                            }
                        }
                        for (int sample = 0; sample < samples; ++sample) {
                            const ImU32 tint = DepartureMetalColor(heat[sample],core);
                            const auto write = [&](float width, float alpha) {
                                const auto a = static_cast<ImU32>(255.0f * opacity[sample] * alpha);
                                const ImU32 ink = CombatHud::SurfaceColor((tint & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT));
                                draw->PrimWriteVtx({points[sample].x + inward.x*width*scale,
                                    points[sample].y + inward.y*width*scale},uv,ink);
                            };
                            write(outer,outerAlpha); write(inner,innerAlpha);
                        }
                    };
                    band(-18.0f,0.0f,0.0f,0.38f,false);
                    band(0.0f,8.0f,0.88f,0.0f,false);
                    band(-0.65f,1.15f,0.92f,0.94f,true);

                    // 火花は切り口に接触した時だけ生まれ、上下の面から離れて慣性で飛ぶ。
                    // 上半分だけを発生源にして、同じ断面から二重に噴き出さない。
                    if (piece.half != 0) { continue; }
                    for (int spark = 0; spark < 8; ++spark) {
                        const float seed = static_cast<float>((piece.glyph*13 + static_cast<int>(edgeIndex)*7 + spark*19)%29) / 28.0f;
                        const float t = 0.08f + static_cast<float>(spark)*0.12f;
                        const ImVec2 local{edge.start.x + (edge.end.x - edge.start.x)*t,
                            edge.start.y + (edge.end.y - edge.start.y)*t};
                        const auto canvas = ProjectLogoCanvas(local,separation,piece.half,0.0f,flyby.pattern);
                        const auto neutral = ProjectLogoCanvas(local,0.0f,piece.half,0.0f,flyby.pattern);
                        const float birth = DepartureContactTime(195.0f + neutral.x*1.75f) + seed*0.025f;
                        const float age = departure - birth;
                        const float lifetime = 0.44f + seed*0.24f;
                        if (age <= 0.0f || age >= lifetime) { continue; }
                        const auto birthMotion = GetLogoMotion(elapsed - age,flyby,birth);
                        const auto source = MoveLogoCanvas({195.0f + canvas.x*1.75f,247.0f + canvas.y*1.75f},birthMotion);
                        const ImVec2 direction{axis.x/length,axis.y/length};
                        const float side = spark % 2 == 0 ? -1.0f : 1.0f;
                        const ImVec2 velocity{direction.x*(180.0f + seed*460.0f) + inward.x*side*(310.0f + seed*1060.0f),
                            direction.y*(180.0f + seed*460.0f) + inward.y*side*(310.0f + seed*1060.0f)};
                        const ImVec2 start{origin.x + (source.x - 195.0f) / 1.75f * scale,
                            origin.y + (source.y - 247.0f) / 1.75f * scale};
                        const auto flight = [&](float time) {
                            return ImVec2{start.x + velocity.x*time*scale,
                                start.y + (velocity.y*time + 420.0f*time*time)*scale};
                        };
                        const auto head = flight(age), tail = flight((std::max)(0.0f,age - 0.008f - seed*0.012f));
                        const float alpha = Smooth(age / 0.008f) * (1.0f - Smooth(age / lifetime));
                        const ImU32 tint = DepartureMetalColor(std::exp(-age / 0.17f),true);
                        const auto ink = [&](float strength) {
                            return CombatHud::SurfaceColor((tint & ~IM_COL32_A_MASK) |
                                (static_cast<ImU32>(255.0f*alpha*strength) << IM_COL32_A_SHIFT));
                        };
                        draw->AddLine(tail,head,ink(0.16f),(2.7f + seed*1.8f)*scale);
                        draw->AddLine(tail,head,ink(0.95f),(0.55f + seed*0.50f)*scale);
                        if (spark % 3 == 0) {
                            // 熱い金属片は面を持たせる。点の光だけで切断を軽く見せない。
                            const float spin = seed*6.2831853f + age*(11.0f + seed*17.0f);
                            const float size = (2.2f + seed*3.4f)*scale*(1.0f - age/lifetime*0.45f);
                            const ImVec2 axis{std::cos(spin)*size,std::sin(spin)*size};
                            const ImVec2 cross{-axis.y*0.55f,axis.x*0.55f};
                            draw->AddQuadFilled({head.x + axis.x,head.y + axis.y},
                                {head.x + cross.x,head.y + cross.y},{head.x - axis.x,head.y - axis.y},
                                {head.x - cross.x,head.y - cross.y},ink(0.92f));
                            draw->AddLine({head.x - axis.x,head.y - axis.y},
                                {head.x - cross.x,head.y - cross.y},
                                CombatHud::SurfaceColor(IM_COL32(34,15,10,static_cast<int>(205.0f*alpha))),scale);
                        }
                    }
                }
            }
        }
    } else if (strike.blade > 0.001f || strike.afterglow > 0.02f) {
        constexpr int count = 33;
        std::array<ImVec2, count> points{};
        const float tail = departure > 0.0f ? 1500.0f : 280.0f + strike.afterglow * 520.0f;
        const float head = std::clamp(strike.bladeX, 20.0f, 1260.0f);
        const float direction = departure > 0.0f ? 1.0f : TitleDashDirection(flyby.pattern);
        const float start = std::clamp(head - direction * tail,20.0f,1260.0f);
        for (int index = 0; index < count; ++index) {
            const float x = start + (head - start) * static_cast<float>(index) / (count - 1);
            const auto moved = MoveLogoCanvas({x, TitleCutY(x,flyby.cutPattern)}, motion);
            points[index] = {origin.x + (moved.x - 195.0f) / 1.75f * scale,
                origin.y + (moved.y - 247.0f) / 1.75f * scale};
        }
        const float strength = std::clamp(strike.blade + strike.afterglow * 0.14f, 0.0f, 1.0f);
        const auto bladeLayer = [&](ImU32 ink, float width) {
            const int first = draw->VtxBuffer.Size;
            draw->AddPolyline(points.data(), count, CombatHud::SurfaceColor(ink), 0, width * scale);
            const float delta = points.back().x - points.front().x;
            const float span = std::abs(delta) > 1.0f ? delta : direction;
            for (int index = first; index < draw->VtxBuffer.Size; ++index) {
                auto& vertex = draw->VtxBuffer[index];
                const float travel = (vertex.pos.x - points.front().x) / span;
                const float taper = Smooth(travel / 0.22f) * (1.0f - Smooth((travel - 0.91f) / 0.09f));
                const auto alpha = static_cast<ImU32>(static_cast<float>((vertex.col >> IM_COL32_A_SHIFT) & 255) * taper);
                vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
            }
        };
        bladeLayer(IM_COL32(112,158,231,static_cast<int>(64 * strength)), departure > 0.0f ? 22.0f : 19.0f);
        bladeLayer(IM_COL32(204,226,255,static_cast<int>(160 * strength)), departure > 0.0f ? 7.0f : 6.0f);
        bladeLayer(IM_COL32(255,255,255,static_cast<int>(245 * strength)), 2.0f);
    }
}
