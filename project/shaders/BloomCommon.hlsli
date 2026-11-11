// ブルームの各パスで使う画像、明るさのしきい値、色調整の計算を共有する。

#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t4> gSecondaryTexture : register(t1);
SamplerState gSampler : register(s0);

struct BloomParameter {
    float32_t2 texelSize;
    float32_t2 direction;
    float32_t threshold;
    float32_t intensity;
    float32_t scatter;
    float32_t padding;
};

ConstantBuffer<BloomParameter> gBloom : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

float32_t Luminance(float32_t3 color)
{
    return dot(color, float32_t3(0.2126f, 0.7152f, 0.0722f));
}

float32_t3 ApplySaturation(float32_t3 color, float32_t amount)
{
    float32_t luma = dot(color, float32_t3(0.299f, 0.587f, 0.114f));
    return lerp(float32_t3(luma, luma, luma), color, amount);
}

float32_t3 ExtractBloom(float32_t3 color, float32_t threshold)
{
    float32_t brightness = max(max(color.r, color.g), color.b);
    float32_t knee = max(threshold * 0.42f, 0.0001f);
    float32_t soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0f, knee * 2.0f);
    soft = soft * soft / (knee * 4.0f + 0.0001f);
    float32_t contribution =
        max(brightness - threshold, soft) /
        max(brightness, 0.0001f);
    float32_t luminanceMask =
        smoothstep(threshold * 0.72f, threshold + knee, Luminance(color));
    return color * saturate(contribution * luminanceMask);
}

float32_t3 ApplyFilmicCurve(float32_t3 color)
{
    color = max(color, float32_t3(0.0f, 0.0f, 0.0f));
    return saturate(
        (color * (2.51f * color + 0.03f)) /
        (color * (2.43f * color + 0.59f) + 0.14f));
}
