// 周囲の画素をならしてから、ブルームに使う明るい部分を抜き出す。

#include "BloomCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 uv = input.texcoord;
    float32_t2 diagonal = gBloom.texelSize * 1.35f;
    float32_t3 color =
        gTexture.Sample(gSampler, uv).rgb * 0.38f +
        gTexture.Sample(gSampler, uv + float32_t2( gBloom.texelSize.x, 0.0f)).rgb * 0.105f +
        gTexture.Sample(gSampler, uv + float32_t2(-gBloom.texelSize.x, 0.0f)).rgb * 0.105f +
        gTexture.Sample(gSampler, uv + float32_t2(0.0f,  gBloom.texelSize.y)).rgb * 0.105f +
        gTexture.Sample(gSampler, uv + float32_t2(0.0f, -gBloom.texelSize.y)).rgb * 0.105f +
        gTexture.Sample(gSampler, uv + diagonal).rgb * 0.050f +
        gTexture.Sample(gSampler, uv - diagonal).rgb * 0.050f +
        gTexture.Sample(gSampler, uv + diagonal * float32_t2(1.0f, -1.0f)).rgb * 0.050f +
        gTexture.Sample(gSampler, uv + diagonal * float32_t2(-1.0f, 1.0f)).rgb * 0.050f;

    PixelShaderOutput output;
    output.color = float32_t4(ExtractBloom(color, gBloom.threshold), 1.0f);
    return output;
}
