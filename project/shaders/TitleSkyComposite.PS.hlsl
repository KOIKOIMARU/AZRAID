#include "Fullscreen.hlsli"

Texture2D<float4> cloudTexture : register(t0);
SamplerState cloudSampler : register(s0);

cbuffer FlightComposition : register(b0)
{
    float4 cameraRight;
    float4 cameraUp;
    float4 cameraForward;
    float4 cameraOrigin;
    float4 trailA01;
    float4 trailA23;
    float4 trailB01;
    float4 trailB23;
};

float2 Bezier(float2 a, float2 b, float2 c, float2 d, float t)
{
    float u = 1.0f - t;
    return u*u*u*a + 3.0f*u*u*t*b + 3.0f*u*t*t*c + t*t*t*d;
}

float3 Contrail(float2 uv, float4 start, float4 finish, float aspect)
{
    const float2 metric = float2(aspect, 1.0f);
    float2 previous = start.xy;
    float closest = 100.0f;
    float along = 0.0f;
    // 固定分割。機体の実際の左右ノズルを起点に、旋回の曲線を延長する。
    [unroll]
    for (int index = 1; index <= 32; ++index)
    {
        float t = float(index) / 32.0f;
        float2 curvePoint = Bezier(start.xy, start.zw, finish.xy, finish.zw, t);
        float2 edge = (curvePoint - previous) * metric;
        float2 delta = (uv - previous) * metric;
        float fraction = saturate(dot(delta, edge) / max(dot(edge, edge), 0.00000001f));
        float distance = length(delta - edge * fraction);
        if (distance < closest) {
            closest = distance;
            along = (float(index - 1) + fraction) / 32.0f;
        }
        previous = curvePoint;
    }
    float taper = pow(saturate(1.0f - along), 1.25f);
    float width = (0.0014f + 0.0040f * smoothstep(0.0f, 0.4f, along)) * taper;
    float pixelWidth = max(fwidth(closest), 0.00018f);
    float core = exp(-pow(closest / max(width, pixelWidth), 2.0f)) * saturate(width / pixelWidth);
    float halo = exp(-pow(closest / max(width * 7.0f, 0.00018f), 2.0f));
    float reveal = smoothstep(0.15f, 1.1f, cameraForward.w);
    return (float3(0.78f, 0.86f, 1.0f) * core +
        float3(0.022f, 0.080f, 0.28f) * halo) * taper * reveal;
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint width, height;
    cloudTexture.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);
    float3 color = cloudTexture.Sample(cloudSampler, input.texcoord).rgb * 0.5f;
    color += cloudTexture.Sample(cloudSampler, input.texcoord + texel * float2(0.7f,0.7f)).rgb * 0.125f;
    color += cloudTexture.Sample(cloudSampler, input.texcoord + texel * float2(-0.7f,0.7f)).rgb * 0.125f;
    color += cloudTexture.Sample(cloudSampler, input.texcoord + texel * float2(0.7f,-0.7f)).rgb * 0.125f;
    color += cloudTexture.Sample(cloudSampler, input.texcoord - texel * 0.7f).rgb * 0.125f;

    // ロゴ側は深い青に沈め、機体側の雲の稜線に明るさを残す。
    float focus = smoothstep(0.05f, 0.90f, input.texcoord.x);
    color *= lerp(float3(0.12f,0.21f,0.38f), float3(1,1,1), focus);
    float vignette = saturate(1.0f - 0.32f * dot(input.texcoord - 0.5f, input.texcoord - 0.5f));
    color *= vignette;
    float aspect = cameraRight.w / cameraUp.w;
    color += Contrail(input.texcoord, trailA01, trailA23, aspect);
    color += Contrail(input.texcoord, trailB01, trailB23, aspect);
    return float4(color, 1.0f);
}
