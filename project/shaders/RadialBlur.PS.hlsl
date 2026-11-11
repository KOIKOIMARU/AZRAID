// 画面中心を基準に放射方向へ色を複数回サンプリングし、動きのブラーを作る。

#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct RadialBlurParameter {
    float32_t2 center;
    float32_t blurWidth;
    float32_t padding;
};

ConstantBuffer<RadialBlurParameter> gRadialBlurParameter : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    const int32_t kNumSamples = 10;

    float32_t2 direction = input.texcoord - gRadialBlurParameter.center;
    float32_t4 outputColor = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int32_t sampleIndex = 0; sampleIndex < kNumSamples; ++sampleIndex) {
        float32_t2 texcoord =
            input.texcoord - direction *
            gRadialBlurParameter.blurWidth * (float32_t)sampleIndex;
        outputColor += gTexture.Sample(gSampler, texcoord);
    }

    outputColor *= rcp((float32_t)kNumSamples);

    PixelShaderOutput output;
    output.color.rgb = outputColor.rgb;
    output.color.a = 1.0f;
    return output;
}
