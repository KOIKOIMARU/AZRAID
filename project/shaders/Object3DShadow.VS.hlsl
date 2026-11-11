// 影用の深度を作るために、頂点を光源から見た座標へ写す。

#include "Object3d.hlsli"

cbuffer TransformCB : register(b1)
{
    float4x4 gWVP;
    float4x4 gWorld;
    float4x4 gWorldInverseTranspose;
};

cbuffer SkinningCB : register(b6)
{
    int gEnableSkinning;
    float3 gSkinningPadding;
    WellForGPU gMatrixPalette[128];
};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 weight : BLENDWEIGHT0;
    uint4 jointIndices : BLENDINDICES0;
};

struct ShadowVertexOutput
{
    float4 position : SV_POSITION;
};

ShadowVertexOutput main(VertexShaderInput input)
{
    ShadowVertexOutput output;
    float4 skinnedPosition = input.position;

    if (gEnableSkinning != 0)
    {
        skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);

        if (input.weight.x > 0.0f)
        {
            skinnedPosition += input.weight.x * mul(input.position, gMatrixPalette[input.jointIndices.x].skeletonSpaceMatrix);
        }
        if (input.weight.y > 0.0f)
        {
            skinnedPosition += input.weight.y * mul(input.position, gMatrixPalette[input.jointIndices.y].skeletonSpaceMatrix);
        }
        if (input.weight.z > 0.0f)
        {
            skinnedPosition += input.weight.z * mul(input.position, gMatrixPalette[input.jointIndices.z].skeletonSpaceMatrix);
        }
        if (input.weight.w > 0.0f)
        {
            skinnedPosition += input.weight.w * mul(input.position, gMatrixPalette[input.jointIndices.w].skeletonSpaceMatrix);
        }
    }

    output.position = mul(skinnedPosition, gWVP);
    return output;
}
