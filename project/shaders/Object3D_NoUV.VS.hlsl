// UVを持たないモデルの頂点処理。頂点を画面に写し、法線をピクセル側へ渡す。

struct VSInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0; // UVなしでも共通構造のため必要
    float3 normal : NORMAL0;
};

cbuffer TransformCB : register(b1)
{
    float4x4 WVP;
    float4x4 World;
};

VertexShaderOutput main(VSInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, WVP);
    output.normal = mul((float3x3) World, input.normal);
    output.texcoord = float2(0.0f, 0.0f); // ダミー
    return output;
}
