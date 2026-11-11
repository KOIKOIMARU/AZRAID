// 細かい光と低解像度の広い光を足し、離れた範囲まで発光を広げる。

#include "BloomCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t3 fineBloom = gTexture.Sample(gSampler, input.texcoord).rgb;
    float32_t2 texel = gBloom.texelSize * 1.2f;
    float32_t3 wideBloom =
        gSecondaryTexture.Sample(gSampler, input.texcoord).rgb * 0.46f +
        gSecondaryTexture.Sample(gSampler, input.texcoord + float32_t2(texel.x, 0.0f)).rgb * 0.135f +
        gSecondaryTexture.Sample(gSampler, input.texcoord - float32_t2(texel.x, 0.0f)).rgb * 0.135f +
        gSecondaryTexture.Sample(gSampler, input.texcoord + float32_t2(0.0f, texel.y)).rgb * 0.135f +
        gSecondaryTexture.Sample(gSampler, input.texcoord - float32_t2(0.0f, texel.y)).rgb * 0.135f;

    PixelShaderOutput output;
    output.color = float32_t4(fineBloom + wideBloom * gBloom.scatter, 1.0f);
    return output;
}
