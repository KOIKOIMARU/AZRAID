// マスク画像としきい値を比べ、画面が消える範囲と境目を決める。

#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);

struct DissolveParameter {
    float32_t threshold;
    float32_t edgeWidth;
    float32_t2 padding;
};

ConstantBuffer<DissolveParameter> gDissolveParameter : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    float32_t mask = gMaskTexture.Sample(gSampler, input.texcoord);

    if (mask <= gDissolveParameter.threshold) {
        discard;
    }

    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);

    float32_t edge = 1.0f - smoothstep(
        gDissolveParameter.threshold,
        gDissolveParameter.threshold + gDissolveParameter.edgeWidth,
        mask);
    output.color.rgb += edge * float32_t3(1.0f, 0.4f, 0.3f);
    output.color.a = 1.0f;
    return output;
}
