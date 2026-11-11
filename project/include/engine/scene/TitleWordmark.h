// タイトルロゴの描画と、飛行演出に合わせた表示の変化を扱う。

#pragma once

#include <imgui.h>

namespace TitleMotion { struct TitleFlyby; }

// ロゴの形、断面、光を、タイトルの飛行と同じ時刻で描く。
void DrawTitleWordmark(ImDrawList* draw, ImVec2 origin, float scale, float elapsed,
    const TitleMotion::TitleFlyby& flyby, float departure, ImTextureID surface);
