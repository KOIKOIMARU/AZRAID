// 深度からカメラまでの距離を復元し、その差から物体の輪郭を求める。

#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepthTexture : register(t1);
SamplerState gSamplerLinear : register(s0);
SamplerState gSamplerPoint : register(s1);

cbuffer DepthOutlineParameter : register(b0)
{
    float4x4 gProjectionInverse;
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

static const float32_t kPrewittHorizontalKernel[3][3] = {
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};

static const float32_t kPrewittVerticalKernel[3][3] = {
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
};

static const float32_t2 kIndex3x3[3][3] = {
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f,  0.0f }, { 0.0f,  0.0f }, { 1.0f,  0.0f } },
    { { -1.0f,  1.0f }, { 0.0f,  1.0f }, { 1.0f,  1.0f } },
};

float32_t FetchViewDepth(float32_t2 texcoord)
{
    float32_t ndcDepth = gDepthTexture.Sample(gSamplerPoint, texcoord);
    float32_t4 ndcPosition = float32_t4(
        texcoord.x * 2.0f - 1.0f,
        1.0f - texcoord.y * 2.0f,
        ndcDepth,
        1.0f);

    float32_t4 viewPosition = mul(ndcPosition, gProjectionInverse);
    float32_t safeW =
        abs(viewPosition.w) < 0.00001f ? 0.00001f : viewPosition.w;
    viewPosition.xyz /= safeW;
    return viewPosition.z;
}

PixelShaderOutput main(VertexShaderOutput input) {
    uint32_t width;
    uint32_t height;
    gDepthTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(width), rcp(height));

    float32_t2 difference = float32_t2(0.0f, 0.0f);
    for (int32_t x = 0; x < 3; ++x) {
        for (int32_t y = 0; y < 3; ++y) {
            float32_t2 texcoord =
                input.texcoord + kIndex3x3[x][y] * uvStepSize;
            float32_t viewDepth = FetchViewDepth(texcoord);
            difference.x += viewDepth * kPrewittHorizontalKernel[x][y];
            difference.y += viewDepth * kPrewittVerticalKernel[x][y];
        }
    }

    float32_t weight = length(difference) * 6.0f;
    weight = saturate(weight);

    PixelShaderOutput output;
    output.color.rgb =
        (1.0f - weight) *
        gTexture.Sample(gSamplerLinear, input.texcoord).rgb;
    output.color.a = 1.0f;
    return output;
}
