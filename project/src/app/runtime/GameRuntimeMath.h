#pragma once

#include "engine/base/Math.h"

#include <algorithm>
#include <cmath>

// GameRuntimeの各機能で共有する、状態を持たない計算。
namespace GameRuntimeMath {

// 銃口や噴射口など、機体基準のずれを機体の回転に合わせる。
inline Math::Vector3 RotateLocalOffset(
    const Math::Vector3& offset,
    const Math::Vector3& rotate)
{
    const Math::Matrix4x4 rotateMatrix =
        Math::Multiply(
            Math::MakeRotateXMatrix(rotate.x),
            Math::Multiply(
                Math::MakeRotateYMatrix(rotate.y),
                Math::MakeRotateZMatrix(rotate.z)));
    return {
        offset.x * rotateMatrix.m[0][0] +
            offset.y * rotateMatrix.m[1][0] +
            offset.z * rotateMatrix.m[2][0],
        offset.x * rotateMatrix.m[0][1] +
            offset.y * rotateMatrix.m[1][1] +
            offset.z * rotateMatrix.m[2][1],
        offset.x * rotateMatrix.m[0][2] +
            offset.y * rotateMatrix.m[1][2] +
            offset.z * rotateMatrix.m[2][2],
    };
}

// 2点間の距離の二乗を返す。球同士の当たり判定では半径の和の二乗と直接比較する。
inline float DistanceSquared(const Math::Vector3& a, const Math::Vector3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

inline float Lerp(float start, float end, float rate)
{
    return start + (end - start) * rate;
}

inline Math::Vector3 Lerp(const Math::Vector3& start, const Math::Vector3& end, float rate)
{
    return {
        Lerp(start.x, end.x, rate),
        Lerp(start.y, end.y, rate),
        Lerp(start.z, end.z, rate),
    };
}

// 点を行列で変換する。透視投影を含む場合に使えるよう、最後にwで割る。
inline Math::Vector3 TransformCoord(const Math::Vector3& v, const Math::Matrix4x4& m)
{
    const float x =
        v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
    const float y =
        v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
    const float z =
        v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
    const float w =
        v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
    if (std::abs(w) <= 0.0001f) {
        return { x, y, z };
    }
    return { x / w, y / w, z / w };
}

// 飛行方向を前向きにして、上方向と旋回時の傾きから機体の回転角を作る。
inline Math::Vector3 StageFlightRotation(Math::Vector3 direction, Math::Vector3 referenceUp, float bank)
{
    const auto cross=[](Math::Vector3 a,Math::Vector3 b) {
        return Math::Vector3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
    };
    const auto forward=Math::Normalize(direction);
    auto right=cross(referenceUp,forward);
    if (right.x*right.x+right.y*right.y+right.z*right.z < 0.000001f) { right=cross({0,0,1},forward); }
    right=Math::Normalize(right);
    const auto up=cross(forward,right);
    const float cosine=std::cos(bank), sine=std::sin(bank);
    const Math::Vector3 bankedRight{right.x*cosine+up.x*sine,right.y*cosine+up.y*sine,right.z*cosine+up.z*sine};
    const Math::Vector3 bankedUp{up.x*cosine-right.x*sine,up.y*cosine-right.y*sine,up.z*cosine-right.z*sine};
    const float yaw=std::asin(std::clamp(-bankedRight.z,-1.0f,1.0f));
    if (std::abs(std::cos(yaw)) < 0.0001f) { return {0.0f,yaw,std::atan2(-bankedUp.x,bankedUp.y)}; }
    return {std::atan2(bankedUp.z,forward.z),yaw,std::atan2(bankedRight.y,bankedRight.x)};
}

}
