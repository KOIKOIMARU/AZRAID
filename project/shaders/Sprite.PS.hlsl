// スプライトの画像を読み、設定された色と透明度を反映する。

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor;
    int gEnableLighting;
    float3 gPadding;
    float4x4 gUVTransform;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput
{
    float4 color : SV_TARGET0;
};

PSOutput main(VSOutput input)
{
    PSOutput output;

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gUVTransform).xy;
    float4 tex = gTexture.Sample(gSampler, uv);

    output.color = gMaterialColor * tex;
    return output;
}
