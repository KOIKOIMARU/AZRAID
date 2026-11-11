// 粒子のテクスチャと個別の色を掛けて表示する。

struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR0;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexOut pin) : SV_TARGET
{
    return gTexture.Sample(gSampler, pin.texcoord) * pin.color;
}
