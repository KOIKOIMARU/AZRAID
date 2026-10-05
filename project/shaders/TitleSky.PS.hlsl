#include "Fullscreen.hlsli"

// タイトル専用の空と雲。画像素材を使わず、視線に沿って立体密度を積分する。
cbuffer Atmosphere : register(b0)
{
    float4 cameraRight;   // xyz: カメラの右、w: tan(FOV/2) * aspect
    float4 cameraUp;      // xyz: カメラの上、w: tan(FOV/2)
    float4 cameraForward;
    float4 cameraOrigin;  // 実空間をゆっくり前進する。
};

float Hash(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float Noise(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    return lerp(
        lerp(lerp(Hash(i), Hash(i + float3(1,0,0)), f.x),
             lerp(Hash(i + float3(0,1,0)), Hash(i + float3(1,1,0)), f.x), f.y),
        lerp(lerp(Hash(i + float3(0,0,1)), Hash(i + float3(1,0,1)), f.x),
             lerp(Hash(i + float3(0,1,1)), Hash(i + float3(1,1,1)), f.x), f.y), f.z);
}

float Density(float3 p)
{
    float3 q = p * float3(0.006f, 0.0045f, 0.006f);
    float shape = Noise(q) * 0.64f + Noise(q * 2.07f + 17.1f) * 0.25f +
        Noise(q * 4.13f + 31.7f) * 0.11f;
    float height = (p.y + 55.0f) / 395.0f;
    float profile = smoothstep(0.0f, 0.12f, height) * (1.0f - smoothstep(0.30f, 1.0f, height));
    float body = saturate((shape - (1.0f - profile * 0.69f)) * 5.0f);
    // 航路を囲む雲の塊。低い雲海は残し、機体の正面には抜けを作る。
    float channel = abs(p.x + 80.0f * sin(p.z * 0.0013f));
    float bank = smoothstep(80.0f, 190.0f, channel);
    return body * lerp(1.0f, bank, smoothstep(55.0f, 125.0f, p.y));
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float2 screen = input.texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float3 ray = normalize(cameraForward.xyz + cameraRight.xyz * screen.x * cameraRight.w +
        cameraUp.xyz * screen.y * cameraUp.w);
    const float3 sun = normalize(float3(0.58f, 0.26f, 1.0f));
    float sunFacing = saturate(dot(ray, sun));
    float horizon = pow(saturate(ray.y + 0.30f), 0.35f);
    float3 sky = lerp(float3(0.07f, 0.19f, 0.38f), float3(0.002f, 0.012f, 0.075f), horizon);
    sky += float3(0.19f, 0.19f, 0.16f) * pow(sunFacing, 22.0f);
    sky += float3(0.62f, 0.49f, 0.31f) * pow(sunFacing, 380.0f);

    float safeY = abs(ray.y) < 0.001f ? (ray.y < 0.0f ? -0.001f : 0.001f) : ray.y;
    float edgeA = (340.0f - cameraOrigin.y) / safeY;
    float edgeB = (-55.0f - cameraOrigin.y) / safeY;
    float first = max(0.0f, min(edgeA, edgeB));
    float last = min(6000.0f, max(edgeA, edgeB));
    if (first >= last) { return float4(sky, 1.0f); }

    const int sampleCount = 96;
    // 画素に固定した微小オフセット。毎フレームの乱数によるちらつきを避ける。
    float jitter = frac(dot(floor(input.position.xy), float2(0.754877f, 0.569840f)));
    float transmittance = 1.0f;
    float3 cloud = 0.0f;
    [loop]
    for (int index = 0; index < sampleCount; ++index)
    {
        float fraction = float(index) / float(sampleCount);
        float nextFraction = float(index + 1) / float(sampleCount);
        float stepSize = (nextFraction * nextFraction - fraction * fraction) * (last - first);
        float t = first + fraction * fraction * (last - first) + stepSize * (0.35f + jitter * 0.30f);
        float3 p = cameraOrigin.xyz + ray * t;
        float density = Density(p);
        if (density > 0.001f)
        {
            float illumination = exp(-Density(p + sun * 44.0f) * 1.8f -
                Density(p + sun * 108.0f) * 2.6f);
            float heightLight = smoothstep(30.0f, 310.0f, p.y);
            float light = smoothstep(0.12f, 0.86f, illumination * 0.84f + heightLight * 0.16f);
            float3 color = lerp(float3(0.014f, 0.042f, 0.13f), float3(0.81f, 0.87f, 0.94f), light);
            color += float3(0.18f, 0.13f, 0.06f) * pow(sunFacing, 12.0f) * illumination;
            color = lerp(color, sky, 1.0f - exp(-t * 0.00013f));
            float alpha = 1.0f - exp(-density * stepSize * 0.047f);
            cloud += color * (alpha * transmittance);
            transmittance *= 1.0f - alpha;
            if (transmittance < 0.015f) { break; }
        }
    }
    return float4(cloud + sky * transmittance, 1.0f);
}
