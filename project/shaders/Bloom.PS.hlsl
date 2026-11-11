// 周囲の複数方向から明るい色を拾い、1パスで元画像へ光を足すブルーム。

#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

float32_t3 ApplySaturation(float32_t3 color, float32_t amount)
{
    float32_t luma = dot(color, float32_t3(0.299f, 0.587f, 0.114f));
    return lerp(float32_t3(luma, luma, luma), color, amount);
}

float32_t3 ExtractBright(float32_t3 color)
{
    float32_t luminance = dot(color, float32_t3(0.2126f, 0.7152f, 0.0722f));
    float32_t mask = smoothstep(0.58f, 1.08f, luminance);
    return color * mask;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 texel = float32_t2(rcp(width), rcp(height));

    float32_t3 baseColor = gTexture.Sample(gSampler, input.texcoord).rgb;

    float32_t3 bloom = ExtractBright(baseColor) * 0.24f;
    static const float32_t2 directions[8] = {
        float32_t2( 1.0f,  0.0f),
        float32_t2(-1.0f,  0.0f),
        float32_t2( 0.0f,  1.0f),
        float32_t2( 0.0f, -1.0f),
        float32_t2( 0.707f,  0.707f),
        float32_t2(-0.707f,  0.707f),
        float32_t2( 0.707f, -0.707f),
        float32_t2(-0.707f, -0.707f)
    };

    for (int32_t index = 0; index < 8; ++index) {
        float32_t2 direction = directions[index];
        bloom +=
            ExtractBright(gTexture.Sample(gSampler, input.texcoord + direction * texel * 2.0f).rgb) *
            0.105f;
        bloom +=
            ExtractBright(gTexture.Sample(gSampler, input.texcoord + direction * texel * 5.0f).rgb) *
            0.068f;
        bloom +=
            ExtractBright(gTexture.Sample(gSampler, input.texcoord + direction * texel * 10.0f).rgb) *
            0.036f;
    }

    float32_t3 color = baseColor + bloom * 0.72f;
    color = (color - 0.5f) * 1.08f + 0.5f;
    color = ApplySaturation(color, 1.12f);
    color *= float32_t3(1.03f, 1.02f, 1.08f);

    float32_t2 centeredUv = input.texcoord - float32_t2(0.5f, 0.5f);
    float32_t edgeDistance = dot(centeredUv, centeredUv) * 2.0f;
    float32_t vignette = saturate(1.0f - edgeDistance * 0.46f);
    color *= lerp(1.0f, vignette, 0.58f);

    PixelShaderOutput output;
    output.color = float32_t4(saturate(color), 1.0f);
    return output;
}
