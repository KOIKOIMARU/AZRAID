#include "engine/base/Math.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace Math {

    // 内部専用：3x3 行列式
    static float Determinant3x3(float matrix[3][3]) {
        return
            matrix[0][0] * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) -
            matrix[0][1] * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0]) +
            matrix[0][2] * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]);
    }

    // 内部専用：余因子
    static float Minor(const Matrix4x4& m, int row, int col) {
        float sub[3][3];
        int sub_i = 0;

        for (int i = 0; i < 4; ++i) {
            if (i == row) continue;
            int sub_j = 0;
            for (int j = 0; j < 4; ++j) {
                if (j == col) continue;
                sub[sub_i][sub_j] = m.m[i][j];
                sub_j++;
            }
            sub_i++;
        }
        return Determinant3x3(sub);
    }

    // 公開関数
    Matrix4x4 Inverse(const Matrix4x4& m) {
        XMMATRIX xm =
            XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&m));
        XMMATRIX inv = XMMatrixInverse(nullptr, xm);

        Matrix4x4 result{};
        XMStoreFloat4x4(
            reinterpret_cast<XMFLOAT4X4*>(&result),
            inv
        );
        return result;
    }

    // 2つの回転を球面線形補間する。tが0で開始の回転、1で終了の回転になる。
    Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t)
    {
        XMVECTOR xmQ0 = XMVectorSet(q0.x, q0.y, q0.z, q0.w);
        XMVECTOR xmQ1 = XMVectorSet(q1.x, q1.y, q1.z, q1.w);
        XMVECTOR xmResult = XMQuaternionSlerp(xmQ0, xmQ1, t);

        Quaternion result{};
        XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&result), xmResult);
        return result;
    }

} // namespace Math
