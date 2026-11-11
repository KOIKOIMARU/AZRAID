// 指定された方向にぼかす。呼び出し側が横と縦を分けて実行する。

#include "BloomCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 stepUv = gBloom.texelSize * gBloom.direction;
    float32_t3 color = gTexture.Sample(gSampler, input.texcoord).rgb * 0.196482f;
    color += gTexture.Sample(gSampler, input.texcoord + stepUv * 1.411764f).rgb * 0.296906f;
    color += gTexture.Sample(gSampler, input.texcoord - stepUv * 1.411764f).rgb * 0.296906f;
    color += gTexture.Sample(gSampler, input.texcoord + stepUv * 3.294118f).rgb * 0.094470f;
    color += gTexture.Sample(gSampler, input.texcoord - stepUv * 3.294118f).rgb * 0.094470f;
    color += gTexture.Sample(gSampler, input.texcoord + stepUv * 5.176470f).rgb * 0.010383f;
    color += gTexture.Sample(gSampler, input.texcoord - stepUv * 5.176470f).rgb * 0.010383f;

    PixelShaderOutput output;
    output.color = float32_t4(color, 1.0f);
    return output;
}
