#pragma once

#include "engine/base/Math.h"

namespace ScreenProjection {

// ワールド座標を指定した表示領域の座標へ変換する。画面端の演出用に少し余白を含める。
// カメラの後方や表示領域から大きく離れた点では、出力を変更せずfalseを返す。
inline bool ToViewport(const Math::Vector3& worldPosition, const Math::Matrix4x4& viewProjection,
    const Math::Vector2& viewportMin, const Math::Vector2& viewportSize, Math::Vector2& screenPosition)
{
    const float clipX = worldPosition.x * viewProjection.m[0][0] +
        worldPosition.y * viewProjection.m[1][0] + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
    const float clipY = worldPosition.x * viewProjection.m[0][1] +
        worldPosition.y * viewProjection.m[1][1] + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
    const float clipW = worldPosition.x * viewProjection.m[0][3] +
        worldPosition.y * viewProjection.m[1][3] + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];
    if (clipW <= 0.001f) { return false; }

    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    if (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f) { return false; }
    screenPosition = {
        viewportMin.x + (ndcX + 1.0f) * 0.5f * viewportSize.x,
        viewportMin.y + (1.0f - ndcY) * 0.5f * viewportSize.y
    };
    return true;
}

}
