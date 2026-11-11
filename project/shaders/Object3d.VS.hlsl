// 必要に応じて骨格で頂点を変形し、ワールド座標と画面へ写す座標を作る。

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

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 skinnedPosition = input.position;
    float3 skinnedNormal = input.normal;

    if (gEnableSkinning != 0)
    {
        skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
        skinnedNormal = float3(0.0f, 0.0f, 0.0f);

        if (input.weight.x > 0.0f)
        {
            skinnedPosition += input.weight.x * mul(input.position, gMatrixPalette[input.jointIndices.x].skeletonSpaceMatrix);
            skinnedNormal += input.weight.x * mul(input.normal, (float3x3)gMatrixPalette[input.jointIndices.x].skeletonSpaceInverseTransposeMatrix);
        }
        if (input.weight.y > 0.0f)
        {
            skinnedPosition += input.weight.y * mul(input.position, gMatrixPalette[input.jointIndices.y].skeletonSpaceMatrix);
            skinnedNormal += input.weight.y * mul(input.normal, (float3x3)gMatrixPalette[input.jointIndices.y].skeletonSpaceInverseTransposeMatrix);
        }
        if (input.weight.z > 0.0f)
        {
            skinnedPosition += input.weight.z * mul(input.position, gMatrixPalette[input.jointIndices.z].skeletonSpaceMatrix);
            skinnedNormal += input.weight.z * mul(input.normal, (float3x3)gMatrixPalette[input.jointIndices.z].skeletonSpaceInverseTransposeMatrix);
        }
        if (input.weight.w > 0.0f)
        {
            skinnedPosition += input.weight.w * mul(input.position, gMatrixPalette[input.jointIndices.w].skeletonSpaceMatrix);
            skinnedNormal += input.weight.w * mul(input.normal, (float3x3)gMatrixPalette[input.jointIndices.w].skeletonSpaceInverseTransposeMatrix);
        }
    }

    output.position = mul(skinnedPosition, gWVP);
    output.texcoord = input.texcoord;
    output.worldPosition = mul(skinnedPosition, gWorld).xyz;

    output.normal = normalize(
        mul(skinnedNormal, (float3x3) gWorldInverseTranspose));

    return output;
}
