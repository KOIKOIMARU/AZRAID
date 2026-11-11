// 骨格の変形を頂点ごとにGPUで計算し、描画で使う頂点バッファへ書く。

#include "Object3d.hlsli"

struct Vertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
};

struct VertexInfluence
{
    float4 weight;
    int4 index;
};

struct OutputVertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
    float4 weight;
    uint4 jointIndices;
};

cbuffer SkinningInformation : register(b0)
{
    uint gNumVertices;
    float3 gSkinningInformationPadding;
};

StructuredBuffer<WellForGPU> gMatrixPalette : register(t0);
StructuredBuffer<Vertex> gInputVertices : register(t1);
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
RWStructuredBuffer<OutputVertex> gOutputVertices : register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;
    if (vertexIndex >= gNumVertices)
    {
        return;
    }

    Vertex input = gInputVertices[vertexIndex];
    VertexInfluence influence = gInfluences[vertexIndex];

    OutputVertex skinned;
    skinned.texcoord = input.texcoord;
    skinned.weight = influence.weight;
    skinned.jointIndices = uint4(
        influence.index.x,
        influence.index.y,
        influence.index.z,
        influence.index.w);

    float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    if (influence.weight.x > 0.0f)
    {
        skinnedPosition += influence.weight.x *
            mul(input.position, gMatrixPalette[influence.index.x].skeletonSpaceMatrix);
        skinnedNormal += influence.weight.x *
            mul(input.normal, (float3x3)gMatrixPalette[influence.index.x].skeletonSpaceInverseTransposeMatrix);
    }
    if (influence.weight.y > 0.0f)
    {
        skinnedPosition += influence.weight.y *
            mul(input.position, gMatrixPalette[influence.index.y].skeletonSpaceMatrix);
        skinnedNormal += influence.weight.y *
            mul(input.normal, (float3x3)gMatrixPalette[influence.index.y].skeletonSpaceInverseTransposeMatrix);
    }
    if (influence.weight.z > 0.0f)
    {
        skinnedPosition += influence.weight.z *
            mul(input.position, gMatrixPalette[influence.index.z].skeletonSpaceMatrix);
        skinnedNormal += influence.weight.z *
            mul(input.normal, (float3x3)gMatrixPalette[influence.index.z].skeletonSpaceInverseTransposeMatrix);
    }
    if (influence.weight.w > 0.0f)
    {
        skinnedPosition += influence.weight.w *
            mul(input.position, gMatrixPalette[influence.index.w].skeletonSpaceMatrix);
        skinnedNormal += influence.weight.w *
            mul(input.normal, (float3x3)gMatrixPalette[influence.index.w].skeletonSpaceInverseTransposeMatrix);
    }

    skinned.position = skinnedPosition;
    skinned.normal = normalize(skinnedNormal);
    gOutputVertices[vertexIndex] = skinned;
}
