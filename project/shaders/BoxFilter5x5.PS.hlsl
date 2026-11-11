// 5×5の範囲の画素を均等に平均する、広めのぼかし。

#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

static const float32_t kKernel5x5[5][5] = {
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
};

PixelShaderOutput main(VertexShaderOutput input) {
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(width), rcp(height));

    PixelShaderOutput output;
    output.color.rgb = float32_t3(0.0f, 0.0f, 0.0f);
    output.color.a = 1.0f;

    for (int32_t x = 0; x < 5; ++x) {
        for (int32_t y = 0; y < 5; ++y) {
            float32_t2 index = float32_t2(x - 2, y - 2);
            float32_t2 texcoord = input.texcoord + index * uvStepSize;
            float32_t3 fetchColor =
                gTexture.Sample(gSampler, texcoord).rgb;
            output.color.rgb += fetchColor * kKernel5x5[x][y];
        }
    }

    return output;
}
