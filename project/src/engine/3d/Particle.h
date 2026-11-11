#pragma once

#include "engine/base/Math.h"

/// <summary>
/// パーティクル1粒分のデータ
/// ※ インスタンシング描画前提なので
///   描画リソースは持たない
/// </summary>
struct Particle
{
    // 位置
    Math::Vector3 position{};
    Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };
    Math::Vector3 rotate{};

    // 速度
    Math::Vector3 velocity{};

    // 加速度（場の影響）
    Math::Vector3 acceleration{};

    // 寿命（最大）
    float lifeTime = 1.0f;

    // 経過時間
    float currentTime = 0.0f;

    // 生存判定
    bool IsAlive() const
    {
        return currentTime < lifeTime;
    }
};
