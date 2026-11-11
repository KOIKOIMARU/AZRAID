// 周囲の画素を重み付きで平均し、解像度を落とした画像に明るさを集約する。

#include "BloomCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 texel = gBloom.texelSize;
    float32_t2 uv = input.texcoord;
    float32_t3 color =
        gTexture.Sample(gSampler, uv).rgb * 0.32f +
        gTexture.Sample(gSampler, uv + texel * float32_t2( 1.0f,  0.0f)).rgb * 0.09f +
        gTexture.Sample(gSampler, uv + texel * float32_t2(-1.0f,  0.0f)).rgb * 0.09f +
        gTexture.Sample(gSampler, uv + texel * float32_t2( 0.0f,  1.0f)).rgb * 0.09f +
        gTexture.Sample(gSampler, uv + texel * float32_t2( 0.0f, -1.0f)).rgb * 0.09f +
        gTexture.Sample(gSampler, uv + texel * float32_t2( 1.5f,  1.5f)).rgb * 0.08f +
        gTexture.Sample(gSampler, uv + texel * float32_t2(-1.5f,  1.5f)).rgb * 0.08f +
        gTexture.Sample(gSampler, uv + texel * float32_t2( 1.5f, -1.5f)).rgb * 0.08f +
        gTexture.Sample(gSampler, uv + texel * float32_t2(-1.5f, -1.5f)).rgb * 0.08f;

    PixelShaderOutput output;
    output.color = float32_t4(color, 1.0f);
    return output;
}
