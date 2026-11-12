// 座標・回転・行列の共通計算　ゲームと描画側で同じ座標変換を使うためにまとめている

// engine/base/Math.h
#pragma once
#include <cmath>

namespace Math {

    struct Vector2 {
        float x, y;
    };

    struct Vector3 {
        float x, y, z;
    };

    struct Vector4 {
        float x, y, z, w;
    };

    struct Quaternion {
        float x, y, z, w;
    };

    struct Matrix4x4 {
        float m[4][4];
    };

    struct Transform {
        Vector3 scale;
        Vector3 rotate;
        Vector3 translate;
    };

    struct QuaternionTransform {
        Vector3 scale;
        Quaternion rotate;
        Vector3 translate;
    };

    inline Math::Vector3 operator*(const Math::Vector3& v, float s)
    {
        return { v.x * s, v.y * s, v.z * s };
    }

    inline Math::Vector3 operator*(float s, const Math::Vector3& v)
    {
        return { v.x * s, v.y * s, v.z * s };
    }

    inline Math::Vector3& operator+=(Math::Vector3& a, const Math::Vector3& b)
    {
        a.x += b.x;
        a.y += b.y;
        a.z += b.z;
        return a;
    }



    // 変換前と同じ座標を返す単位行列を作る　複数の変換を合成する際の初期値にも使う
    inline Matrix4x4 MakeIdentity4x4() {
        Matrix4x4 result{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        return result;
    }

    // 左右の行列を積の順番どおりに合成し、まとめて座標へ適用できるようにする
    inline Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
        Matrix4x4 result{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float v = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    v += m1.m[i][k] * m2.m[k][j];
                }
                result.m[i][j] = v;
            }
        }
        return result;
    }

    // 各軸の倍率を対角成分に入れ、拡大縮小用の行列を作る
    inline Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
        Matrix4x4 result{};
        result.m[0][0] = scale.x;
        result.m[1][1] = scale.y;
        result.m[2][2] = scale.z;
        result.m[3][3] = 1.0f;
        return result;
    }

    // 移動量を最終行に入れ、行ベクトルの座標へ平行移動を加える
    inline Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
        Matrix4x4 result{};
        result.m[0][0] = 1.0f;
        result.m[1][1] = 1.0f;
        result.m[2][2] = 1.0f;
        result.m[3][3] = 1.0f;
        result.m[3][0] = translate.x;
        result.m[3][1] = translate.y;
        result.m[3][2] = translate.z;
        return result;
    }

    // ラジアンで受け取った角度からX軸周りの回転行列を作る
    inline Matrix4x4 MakeRotateXMatrix(float angle) {
        Matrix4x4 result{};
        result.m[0][0] = 1.0f;
        result.m[3][3] = 1.0f;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[1][1] = c;
        result.m[1][2] = s;
        result.m[2][1] = -s;
        result.m[2][2] = c;
        return result;
    }

    // ラジアンで受け取った角度からY軸周りの回転行列を作る
    inline Matrix4x4 MakeRotateYMatrix(float angle) {
        Matrix4x4 result{};
        result.m[1][1] = 1.0f;
        result.m[3][3] = 1.0f;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[0][0] = c;
        result.m[0][2] = -s;
        result.m[2][0] = s;
        result.m[2][2] = c;
        return result;
    }

    // ラジアンで受け取った角度からZ軸周りの回転行列を作る
    inline Matrix4x4 MakeRotateZMatrix(float angle) {
        Matrix4x4 result{};
        result.m[2][2] = 1.0f;
        result.m[3][3] = 1.0f;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[0][0] = c;
        result.m[0][1] = s;
        result.m[1][0] = -s;
        result.m[1][1] = c;
        return result;
    }

    // クォータニオンで表した回転を、座標変換に使える4×4の行列へ変換する
    inline Matrix4x4 MakeRotateMatrix(const Quaternion& q)
    {
        Matrix4x4 result = MakeIdentity4x4();

        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;
        const float xy = q.x * q.y;
        const float xz = q.x * q.z;
        const float yz = q.y * q.z;
        const float wx = q.w * q.x;
        const float wy = q.w * q.y;
        const float wz = q.w * q.z;

        result.m[0][0] = 1.0f - 2.0f * (yy + zz);
        result.m[0][1] = 2.0f * (xy + wz);
        result.m[0][2] = 2.0f * (xz - wy);
        result.m[1][0] = 2.0f * (xy - wz);
        result.m[1][1] = 1.0f - 2.0f * (xx + zz);
        result.m[1][2] = 2.0f * (yz + wx);
        result.m[2][0] = 2.0f * (xz + wy);
        result.m[2][1] = 2.0f * (yz - wx);
        result.m[2][2] = 1.0f - 2.0f * (xx + yy);
        return result;
    }

    // 各軸の拡大、オイラー角の回転、移動を順に合成する
    inline Matrix4x4 MakeAffineMatrix(
        const Vector3& scale,
        const Vector3& rotate,
        const Vector3& translate)
    {
        Matrix4x4 result{};
        Matrix4x4 rotateXYZ =
            Multiply(MakeRotateXMatrix(rotate.x),
                Multiply(MakeRotateYMatrix(rotate.y),
                    MakeRotateZMatrix(rotate.z)));

        result.m[0][0] = scale.x * rotateXYZ.m[0][0];
        result.m[0][1] = scale.x * rotateXYZ.m[0][1];
        result.m[0][2] = scale.x * rotateXYZ.m[0][2];
        result.m[1][0] = scale.y * rotateXYZ.m[1][0];
        result.m[1][1] = scale.y * rotateXYZ.m[1][1];
        result.m[1][2] = scale.y * rotateXYZ.m[1][2];
        result.m[2][0] = scale.z * rotateXYZ.m[2][0];
        result.m[2][1] = scale.z * rotateXYZ.m[2][1];
        result.m[2][2] = scale.z * rotateXYZ.m[2][2];
        result.m[3][0] = translate.x;
        result.m[3][1] = translate.y;
        result.m[3][2] = translate.z;
        result.m[3][3] = 1.0f;

        return result;
    }

    // 拡大、クォータニオンの回転、移動を合成し、物体の姿勢を表す行列を作る
    inline Matrix4x4 MakeAffineMatrix(
        const Vector3& scale,
        const Quaternion& rotate,
        const Vector3& translate)
    {
        Matrix4x4 result{};
        Matrix4x4 rotateMatrix = MakeRotateMatrix(rotate);

        result.m[0][0] = scale.x * rotateMatrix.m[0][0];
        result.m[0][1] = scale.x * rotateMatrix.m[0][1];
        result.m[0][2] = scale.x * rotateMatrix.m[0][2];
        result.m[1][0] = scale.y * rotateMatrix.m[1][0];
        result.m[1][1] = scale.y * rotateMatrix.m[1][1];
        result.m[1][2] = scale.y * rotateMatrix.m[1][2];
        result.m[2][0] = scale.z * rotateMatrix.m[2][0];
        result.m[2][1] = scale.z * rotateMatrix.m[2][1];
        result.m[2][2] = scale.z * rotateMatrix.m[2][2];
        result.m[3][0] = translate.x;
        result.m[3][1] = translate.y;
        result.m[3][2] = translate.z;
        result.m[3][3] = 1.0f;

        return result;
    }
    
    // 行と列を入れ替える　行列をシェーダーの読み方に合わせる場面でも使う
    inline Matrix4x4 Transpose(const Matrix4x4& m)
    {
        Matrix4x4 result;
        result.m[0][0] = m.m[0][0]; result.m[0][1] = m.m[1][0]; result.m[0][2] = m.m[2][0]; result.m[0][3] = m.m[3][0];
        result.m[1][0] = m.m[0][1]; result.m[1][1] = m.m[1][1]; result.m[1][2] = m.m[2][1]; result.m[1][3] = m.m[3][1];
        result.m[2][0] = m.m[0][2]; result.m[2][1] = m.m[1][2]; result.m[2][2] = m.m[2][2]; result.m[2][3] = m.m[3][2];
        result.m[3][0] = m.m[0][3]; result.m[3][1] = m.m[1][3]; result.m[3][2] = m.m[2][3]; result.m[3][3] = m.m[3][3];
        return result;
    }


    // ベクトルを同じ方向の単位ベクトルにし、長さが0ならゼロベクトルを返す
    inline Vector3 Normalize(const Vector3& v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len == 0.0f) return { 0.0f, 0.0f, 0.0f };
        return { v.x / len, v.y / len, v.z / len };
    }

    // 回転用クォータニオンの長さを1にそろえ、長さが0なら回転なしに戻す
    inline Quaternion NormalizeQuaternion(const Quaternion& q) {
        float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (len == 0.0f) {
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        }
        return { q.x / len, q.y / len, q.z / len, q.w / len };
    }

    // 縦の画角、画面比率、手前と奥の距離から、遠い物体ほど小さく写る投影行列を作る
    inline Matrix4x4 MakePerspectiveFovMatrix(
        float fovY, float aspectRatio, float nearClip, float farClip)
    {
        Matrix4x4 result{};
        float f = 1.0f / std::tan(fovY / 2.0f);
        result.m[0][0] = f / aspectRatio;
        result.m[1][1] = f;
        result.m[2][2] = farClip / (farClip - nearClip);
        result.m[2][3] = 1.0f;
        result.m[3][2] = -(farClip * nearClip) / (farClip - nearClip);
        return result;
    }

    // 左右上下と深度の範囲を、遠近による縮みのない表示範囲へ変換する
    inline Matrix4x4 MakeOrthographicMatrix(
        float left, float top, float right, float bottom,
        float nearClip, float farClip)
    {
        Matrix4x4 result{};
        result.m[0][0] = 2.0f / (right - left);
        result.m[1][1] = 2.0f / (top - bottom);
        result.m[2][2] = 1.0f / (farClip - nearClip);
        result.m[3][0] = (left + right) / (left - right);
        result.m[3][1] = (top + bottom) / (bottom - top);
        result.m[3][2] = -nearClip / (farClip - nearClip);
        result.m[3][3] = 1.0f;
        return result;
    }

    // 4x4行列の逆行列
    Matrix4x4 Inverse(const Matrix4x4& m);
    Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);

} // namespace Math
