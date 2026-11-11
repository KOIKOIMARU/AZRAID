// 背景の立方体を最も奥に表示できるように座標を変換する。

#include "Skybox.hlsli"

cbuffer ViewProjectionCB : register(b0)
{
    float4x4 gViewProjection;
};

struct VertexShaderInput
{
    float4 position : POSITION0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gViewProjection);

    // 常に最奥に描画されるように、射影後のzをwに合わせる
    output.position.z = output.position.w;

    // Cubemapは2DのUVではなく、中心からの方向ベクトルで参照する
    output.texcoord = input.position.xyz;
    return output;
}
