#include "Fullscreen.hlsli"

cbuffer TitleComposition : register(b0)
{
    float2 resolution;
    float elapsed;
    float departure;
    float4 flyby; // 光位置、航跡の余韻、飛行割合、航跡だけの描画。
    float4 engines[2]; // 実モデルの左右ノズルの画面位置と、噴射方向。
    float4 wakePoints[12]; // 左右6点ずつの過去のノズル位置、幅、残光。
};

float Ease(float t)
{
    t = saturate(t);
    return t * t * (3.0f - 2.0f * t);
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float fit = min(resolution.x / 1280.0f, resolution.y / 720.0f);
    float2 canvas = (input.texcoord * resolution - (resolution - float2(1280,720) * fit) * 0.5f) / fit;
    float2 screenCanvas = canvas;
    float2 centered = (canvas - float2(640,340)) / float2(760,440);
    float vignette = saturate(1.0f - dot(centered, centered) * 0.38f);
    float3 color = lerp(float3(0.0014f,0.0030f,0.0080f),
        float3(0.0050f,0.0150f,0.0400f), vignette);

    // ロゴと同じ透視・傾きへ戻す。形は固定し、光だけを通過させる。
    float2 relative = canvas - float2(641.25f,334.5f);
    float2 upright = float2(relative.x * 0.9950042f - relative.y * 0.0998334f,
        relative.x * 0.0998334f + relative.y * 0.9950042f);
    canvas = float2(641.25f,334.5f) + upright / max(1.0f - upright.x * 0.00024f, 0.30f);
    float signedDistance = canvas.y - (387.0f - (canvas.x - 195.0f) * 0.11f);

    if (flyby.w > 0.5f)
    {
        float alpha = 0.0f;
        float white = 0.0f;
        float proximity = exp(-pow((flyby.z - 0.42f) / 0.18f,2.0f));
        [unroll]
        for (int nozzle = 0; nozzle < 2; ++nozzle)
        {
            float2 delta = screenCanvas - engines[nozzle].xy;
            float2 direction = engines[nozzle].zw;
            float behind = dot(delta,direction);
            float distance = dot(delta,float2(-direction.y,direction.x));
            float taper = 1.0f + saturate(behind / 220.0f) * 0.65f;
            float core = exp(-pow(distance / ((1.0f + proximity * 1.4f) * taper),2.0f));
            float halo = exp(-pow(distance / ((5.0f + proximity * 3.0f) * taper),2.0f));
            float wake = smoothstep(3.0f,18.0f,behind) *
                (1.0f - smoothstep(60.0f,115.0f,behind)) * exp(-max(behind,0.0f) / 70.0f);
            alpha = max(alpha,saturate((core * 0.88f + halo * 0.18f) * wake * flyby.y *
                (0.65f + proximity * 0.35f)));
            white = max(white,core);
            [unroll]
            for (int segment = 0; segment < 5; ++segment)
            {
                float4 start = wakePoints[nozzle * 6 + segment];
                float4 end = wakePoints[nozzle * 6 + segment + 1];
                float2 edge = end.xy - start.xy;
                float lengthSquared = dot(edge,edge);
                float t = saturate(dot(screenCanvas - start.xy,edge) / max(lengthSquared,0.01f));
                float distanceToWake = length(screenCanvas - lerp(start.xy,end.xy,t));
                float width = lerp(start.z,end.z,t);
                float strength = lerp(start.w,end.w,t) * flyby.y * step(1.0f,lengthSquared);
                float historyCore = exp(-pow(distanceToWake / max(width,0.5f),2.0f));
                float historyHalo = exp(-pow(distanceToWake / (width * 3.0f + 4.0f),2.0f));
                alpha = max(alpha,saturate((historyCore * 0.82f + historyHalo * 0.12f) * strength));
                white = max(white,historyCore * strength);
            }
        }
        return float4(lerp(float3(0.07f,0.18f,0.48f),float3(0.92f,0.97f,1.0f),white),alpha);
    }

    float opening = Ease((elapsed - 0.32f) / 0.72f);
    float idleTime = max(elapsed - 2.10f, 0.0f);
    float idle = Ease(idleTime / 1.40f) * (1.0f - Ease(departure / 0.20f));
    float envelope = pow(saturate(1.0f - abs(canvas.x - 640.0f) / 690.0f), 0.58f);
    float halfGap = 4.0f * envelope + pow(Ease(departure / 1.30f),2.0f) * 130.0f;
    float aa = max(fwidth(signedDistance), 0.8f);
    float inside = 1.0f - smoothstep(halfGap - aa, halfGap + aa, abs(signedDistance));
    color = lerp(color,float3(0.0007f,0.0015f,0.0040f),inside * opening);

    float upperFace = exp(-abs(signedDistance + halfGap + 38.0f) / 135.0f) * envelope;
    color += float3(0.008f,0.019f,0.042f) * upperFace * opening;
    float edgeDistance = abs(signedDistance + halfGap);
    float edge = 1.0f - smoothstep(0.6f,0.6f + aa,edgeDistance);
    float reflection = exp(-edgeDistance / 15.0f);
    float extent = smoothstep(30.0f,140.0f,canvas.x) * (1.0f - smoothstep(1110.0f,1250.0f,canvas.x));
    color += float3(0.025f,0.039f,0.061f) * edge * extent * opening;

    // 白い噴射の反射が切断面を走る。静かな時間は弱い反射だけを残す。
    float lightX = -160.0f + 1600.0f * frac(idleTime / 10.0f + 0.35f);
    float passed = flyby.x - screenCanvas.x;
    float wake = passed > 0.0f ? exp(-passed / 410.0f) * 0.50f : 0.0f;
    float travelingLight = saturate(exp(-pow((canvas.x - lightX) / 130.0f,2.0f)) * idle * 0.10f +
        (exp(-pow(passed / 95.0f,2.0f)) + wake) * flyby.y);
    color += float3(0.65f,0.77f,0.92f) * edge * extent * travelingLight;
    color += float3(0.035f,0.051f,0.080f) * reflection * envelope * travelingLight;

    float sweep = Ease((elapsed - 0.12f) / 0.55f);
    float bladeX = lerp(-200.0f,1480.0f,sweep);
    float blade = exp(-pow((canvas.x - bladeX) / 75.0f,2.0f));
    float bladeLine = exp(-pow(signedDistance / 1.8f,2.0f));
    float bladeGlow = exp(-pow(signedDistance / 28.0f,2.0f));
    float sweepActive = 1.0f - Ease((elapsed - 0.62f) / 0.20f);
    color += float3(0.88f,0.94f,1.0f) * blade * (bladeLine + bladeGlow * 0.09f) * sweepActive;
    float exitLight = Ease((departure - 0.12f) / 0.52f) * (1.0f - Ease((departure - 0.85f) / 0.40f));
    color += float3(0.24f,0.29f,0.36f) * exp(-pow(signedDistance / max(halfGap,1.0f),2.0f)) * exitLight;
    return float4(color,1.0f);
}
