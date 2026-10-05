#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepthTexture : register(t1);
SamplerState gSamplerLinear : register(s0);
SamplerState gSamplerPoint : register(s1);

cbuffer GameToneParameter : register(b0)
{
    float32_t4x4 gProjectionInverse;
    float32_t vignetteStrength;
    float32_t saturation;
    float32_t contrast;
    float32_t damageTint;
    float32_t fogStart;
    float32_t fogEnd;
    float32_t fogStrength;
    float32_t horizonFogStrength;
    float32_t exposure;
    float32_t blackPoint;
    float32_t highlightCompression;
    float32_t colorTemperature;
    float32_t2 flightBlurCenter;
    float32_t flightBlurStrength;
    float32_t flightBlurAspect;
    float32_t2 flightBlurPlayer;
    float32_t2 flightBlurReticle;
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

float32_t3 ApplySaturation(float32_t3 color, float32_t amount) {
    float32_t luma = dot(color, float32_t3(0.299f, 0.587f, 0.114f));
    return lerp(float32_t3(luma, luma, luma), color, amount);
}

float32_t GetLuminance(float32_t3 color) {
    return dot(color, float32_t3(0.2126f, 0.7152f, 0.0722f));
}

float32_t3 SampleSceneColor(float32_t2 texcoord) {
    return gTexture.Sample(gSamplerLinear, saturate(texcoord)).rgb;
}

float32_t3 ApplyEdgeAntialias(
    float32_t2 texcoord,
    float32_t2 texelSize,
    float32_t3 sourceColor) {
    float32_t lumaCenter = GetLuminance(sourceColor);
    float32_t3 left =
        SampleSceneColor(texcoord - float32_t2(texelSize.x, 0.0f));
    float32_t3 right =
        SampleSceneColor(texcoord + float32_t2(texelSize.x, 0.0f));
    float32_t3 up =
        SampleSceneColor(texcoord - float32_t2(0.0f, texelSize.y));
    float32_t3 down =
        SampleSceneColor(texcoord + float32_t2(0.0f, texelSize.y));

    float32_t lumaLeft = GetLuminance(left);
    float32_t lumaRight = GetLuminance(right);
    float32_t lumaUp = GetLuminance(up);
    float32_t lumaDown = GetLuminance(down);
    float32_t lumaMin =
        min(lumaCenter, min(min(lumaLeft, lumaRight), min(lumaUp, lumaDown)));
    float32_t lumaMax =
        max(lumaCenter, max(max(lumaLeft, lumaRight), max(lumaUp, lumaDown)));
    float32_t lumaRange = lumaMax - lumaMin;
    float32_t contrastThreshold = max(0.045f, lumaMax * 0.135f);
    if (lumaRange < contrastThreshold) {
        return sourceColor;
    }

    float32_t horizontalGradient = abs(lumaLeft - lumaRight);
    float32_t verticalGradient = abs(lumaUp - lumaDown);
    float32_t3 antialiasedColor =
        horizontalGradient > verticalGradient ?
        sourceColor * 0.62f + (up + down) * 0.19f :
        sourceColor * 0.62f + (left + right) * 0.19f;
    float32_t edgeStrength =
        saturate((lumaRange - contrastThreshold) * 2.8f) * 0.46f;
    return lerp(sourceColor, antialiasedColor, edgeStrength);
}

float32_t3 ApplyDistantShimmerReduction(
    float32_t2 texcoord,
    float32_t2 texelSize,
    float32_t3 sourceColor,
    float32_t viewDepth,
    float32_t skyMask)
{
    float32_t distanceFactor =
        smoothstep(135.0f, 320.0f, viewDepth) * (1.0f - skyMask);
    if (distanceFactor <= 0.001f) {
        return sourceColor;
    }

    float32_t2 offset = float32_t2(0.0f, texelSize.y * 1.25f);
    float32_t3 calmColor =
        sourceColor * 0.62f +
        (SampleSceneColor(texcoord + offset) +
         SampleSceneColor(texcoord - offset)) *
        0.19f;

    float32_t centerLuma = GetLuminance(sourceColor);
    float32_t calmLuma = GetLuminance(calmColor);
    float32_t highFrequency =
        saturate(abs(centerLuma - calmLuma) * 6.0f);
    return lerp(sourceColor, calmColor, distanceFactor * highFrequency * 0.26f);
}

float32_t3 ApplyFilmicCurve(float32_t3 color) {
    color = max(color, float32_t3(0.0f, 0.0f, 0.0f));
    return saturate(
        (color * (2.51f * color + 0.03f)) /
        (color * (2.43f * color + 0.59f) + 0.14f));
}

float32_t3 ApplyColorTemperature(float32_t3 color, float32_t temperature)
{
    float32_t t = clamp(temperature, -1.0f, 1.0f);
    float32_t3 balance = float32_t3(
        1.0f + t * 0.055f,
        1.0f + abs(t) * 0.010f,
        1.0f - t * 0.065f);
    return color * balance;
}

float32_t3 ApplyCinematicGrade(
    float32_t3 color,
    float32_t viewDepth,
    float32_t skyMask)
{
    color *= exposure;
    color = max(color - blackPoint, float32_t3(0.0f, 0.0f, 0.0f)) *
        rcp(max(1.0f - blackPoint, 0.001f));

    float32_t luminance = GetLuminance(color);
    float32_t shadowMask = pow(saturate(1.0f - luminance * 1.70f), 2.0f);
    float32_t midMask =
        saturate(1.0f - abs(luminance - 0.43f) * 2.35f);
    float32_t highlightMask = smoothstep(0.52f, 1.08f, luminance);

    color = lerp(
        color,
        color * float32_t3(0.88f, 0.95f, 1.06f),
        shadowMask * 0.085f * (1.0f - skyMask * 0.55f));
    color +=
        float32_t3(0.010f, 0.012f, 0.015f) *
        shadowMask *
        (1.0f - skyMask * 0.60f);
    color = lerp(
        color,
        color * float32_t3(1.035f, 1.018f, 0.970f),
        midMask * 0.055f);
    color = lerp(
        color,
        color * float32_t3(1.060f, 1.025f, 0.955f),
        highlightMask * 0.085f);

    float32_t distanceWash =
        smoothstep(125.0f, 320.0f, viewDepth) * (1.0f - skyMask);
    color = lerp(color, ApplySaturation(color, 0.90f), distanceWash * 0.14f);

    float32_t shoulder = saturate(highlightCompression);
    float32_t3 compressed =
        color * rcp(1.0f + color * (0.42f + shoulder * 0.46f));
    color = lerp(color, compressed * 1.18f, shoulder * 0.36f);
    return ApplyColorTemperature(color, colorTemperature);
}

float32_t3 ReconstructViewPosition(float32_t2 texcoord, float32_t ndcDepth) {
    float32_t4 ndcPosition = float32_t4(
        texcoord.x * 2.0f - 1.0f,
        1.0f - texcoord.y * 2.0f,
        ndcDepth,
        1.0f);

    float32_t4 viewPosition = mul(ndcPosition, gProjectionInverse);
    float32_t safeW =
        abs(viewPosition.w) < 0.00001f ? 0.00001f : viewPosition.w;
    viewPosition.xyz /= safeW;
    return viewPosition.xyz;
}

float32_t3 FetchViewPosition(float32_t2 texcoord) {
    return ReconstructViewPosition(texcoord, gDepthTexture.SampleLevel(gSamplerPoint, texcoord, 0));
}

float32_t FetchViewDepth(float32_t2 texcoord) {
    return abs(FetchViewPosition(texcoord).z);
}

float32_t3 ApplyFlightBlur(float32_t2 uv, float32_t3 sourceColor, float32_t viewDepth)
{
    if (flightBlurStrength <= 0.002f) { return sourceColor; }
    float32_t2 ray = uv - flightBlurCenter;
    float32_t2 screenMetric = float32_t2(flightBlurAspect, 1.0f);
    // 中央の戦闘領域・自機・照準の周囲は鮮明なまま残す。
    float32_t edge = smoothstep(0.30f, 0.72f, length(ray * screenMetric));
    float32_t playerMask = smoothstep(1.0f, 1.45f,
        length((uv - flightBlurPlayer) * screenMetric / float32_t2(0.23f, 0.17f)));
    float32_t aimMask = smoothstep(0.13f, 0.24f, length((uv - flightBlurReticle) * screenMetric));
    float32_t amount = flightBlurStrength * edge * playerMask * aimMask * smoothstep(2.0f, 8.0f, viewDepth);
    if (amount <= 0.002f) { return sourceColor; }
    float32_t3 accumulated = sourceColor;
    float32_t totalWeight = 1.0f;
    [unroll]
    for (int32_t index = 1; index <= 6; ++index) {
        float32_t t = float32_t(index) / 6.0f;
        float32_t2 sampleUv = saturate(uv - ray * (amount * 0.090f * t));
        // 建物と敵など深度が離れた面同士を混ぜず、輪郭のにじみを抑える。
        float32_t depthDelta = abs(FetchViewDepth(sampleUv) - viewDepth);
        float32_t weight = (1.0f - t * 0.45f) *
            (1.0f - smoothstep(max(2.0f, viewDepth * 0.06f), max(4.0f, viewDepth * 0.14f), depthDelta));
        accumulated += gTexture.SampleLevel(gSamplerLinear, sampleUv, 0).rgb * weight;
        totalWeight += weight;
    }
    // 元の輪郭を最低45%残し、周辺にいる敵弾も消さない。
    return lerp(sourceColor, accumulated / totalWeight, min(amount * 1.2f, 0.55f));
}

float32_t CalculateScreenSpaceAO(
    float32_t2 texcoord,
    float32_t2 texelSize,
    float32_t viewDepth,
    float32_t ndcDepth) {
    if (ndcDepth >= 0.9999f) {
        return 1.0f;
    }

    // 深度差だけでは斜めの道路まで汚れるため、復元した面の法線に対して遮蔽を測る。
    float32_t3 center = ReconstructViewPosition(texcoord, ndcDepth);
    float32_t3 left = FetchViewPosition(saturate(texcoord - float32_t2(texelSize.x, 0)));
    float32_t3 right = FetchViewPosition(saturate(texcoord + float32_t2(texelSize.x, 0)));
    float32_t3 up = FetchViewPosition(saturate(texcoord - float32_t2(0, texelSize.y)));
    float32_t3 down = FetchViewPosition(saturate(texcoord + float32_t2(0, texelSize.y)));
    float32_t3 dx = abs(right.z - center.z) < abs(center.z - left.z) ? right - center : center - left;
    float32_t3 dy = abs(down.z - center.z) < abs(center.z - up.z) ? down - center : center - up;
    float32_t3 crossNormal = cross(dx, dy);
    float32_t3 normal = crossNormal * rsqrt(max(dot(crossNormal, crossNormal), 0.000001f));
    normal *= dot(normal, center) > 0.0f ? -1.0f : 1.0f;
    static const float32_t2 aoOffsets[8] = {
        float32_t2(1, 0), float32_t2(-1, 0),
        float32_t2(0, 1), float32_t2(0, -1),
        float32_t2(0.53f, 0.53f), float32_t2(-0.53f, 0.53f),
        float32_t2(0.53f, -0.53f), float32_t2(-0.53f, -0.53f),
    };
    // 1.6mの接地領域。遠くの物体だけ影が巨大化するスクリーン固定半径にはしない。
    const float32_t worldRadius = 1.6f;
    float32_t radiusY = clamp(0.5f * worldRadius /
        max(viewDepth * abs(gProjectionInverse[1][1]), 0.01f), texelSize.y * 1.5f, texelSize.y * 18.0f);
    float32_t2 radiusUv = radiusY * float32_t2(texelSize.x / texelSize.y, 1);
    float32_t occlusion = 0.0f;

    [unroll]
    for (int32_t index = 0; index < 8; ++index) {
        float32_t2 sampleUv = saturate(texcoord + aoOffsets[index] * radiusUv);
        float32_t3 delta = FetchViewPosition(sampleUv) - center;
        float32_t distanceSquared = max(dot(delta, delta), 0.0001f);
        float32_t hemisphere = max(dot(normal, delta) * rsqrt(distanceSquared) - 0.10f, 0.0f);
        float32_t rangeFade = saturate(1.0f - distanceSquared / (worldRadius * worldRadius * 4.0f));
        occlusion += hemisphere * rangeFade;
    }

    float32_t nearFade = 1.0f - smoothstep(160.0f, 285.0f, viewDepth);
    return 1.0f - saturate(occlusion / 8.0f) * 0.48f * nearFade;
}

PixelShaderOutput main(VertexShaderOutput input) {
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(width), rcp(height));

    float32_t3 baseColor =
        gTexture.Sample(gSamplerLinear, input.texcoord).rgb;
    float32_t3 softColor =
        baseColor * 0.58f +
        (
            SampleSceneColor(input.texcoord + float32_t2(uvStepSize.x, 0.0f)) +
            SampleSceneColor(input.texcoord - float32_t2(uvStepSize.x, 0.0f)) +
            SampleSceneColor(input.texcoord + float32_t2(0.0f, uvStepSize.y)) +
            SampleSceneColor(input.texcoord - float32_t2(0.0f, uvStepSize.y))
        ) *
        0.105f;
    float32_t viewDepth = FetchViewDepth(input.texcoord);
    float32_t ndcDepth =
        gDepthTexture.Sample(gSamplerPoint, input.texcoord);
    float32_t skyMask = step(0.9999f, ndcDepth);
    baseColor = ApplyEdgeAntialias(input.texcoord, uvStepSize, baseColor);
    baseColor = ApplyDistantShimmerReduction(
        input.texcoord,
        uvStepSize,
        baseColor,
        viewDepth,
        skyMask);
    baseColor = ApplyFlightBlur(input.texcoord, baseColor, viewDepth);
    float32_t3 color = lerp(baseColor, softColor, 0.050f);
    color += (baseColor - softColor) * 0.12f;

    color = (color - 0.5f) * contrast + 0.5f;
    color = ApplySaturation(color, saturation);
    color *= float32_t3(1.030f, 1.018f, 1.030f);

    float32_t luminance = GetLuminance(softColor);
    float32_t softGlow = saturate((luminance - 0.60f) * 1.85f);
    color += softColor * softGlow * 0.035f;

    float32_t aoFactor = CalculateScreenSpaceAO(
        input.texcoord,
        uvStepSize,
        viewDepth,
        ndcDepth);
    color *= aoFactor;

    float32_t farStability = saturate((viewDepth - 96.0f) / 180.0f);
    color = lerp(color, softColor, farStability * 0.015f);
    float32_t fogRange = max(fogEnd - fogStart, 1.0f);
    // 距離に応じた光の減衰。手前の機体・敵弾は鮮明なまま、遠景だけ空気の色へ寄せる。
    float32_t opticalDistance = max(viewDepth - fogStart, 0.0f) / fogRange;
    float32_t depthFog = 1.0f - exp(-opticalDistance * fogStrength);
    float32_t3 viewRay = normalize(ReconstructViewPosition(input.texcoord, ndcDepth));
    float32_t horizonFog = pow(saturate(1.0f - abs(viewRay.y) * 3.0f), 2.0f);
    float32_t fogAmount = (depthFog + horizonFog * horizonFogStrength * depthFog) * (1.0f - skyMask);
    // シーン色は線形色空間。sRGBの見た目の値を直接足して白いベールにしない。
    float32_t3 fogColor = lerp(float32_t3(0.10f, 0.16f, 0.24f),
        float32_t3(0.22f, 0.30f, 0.39f), horizonFog);
    color = lerp(color, fogColor, saturate(fogAmount));
    color = ApplyCinematicGrade(color, viewDepth, skyMask);
    float32_t filmicAmount = 0.25f + saturate(highlightCompression) * 0.12f;
    color = lerp(color, ApplyFilmicCurve(color) * 1.07f, filmicAmount);

    float32_t2 centeredUv = input.texcoord - float32_t2(0.5f, 0.5f);
    float32_t edgeDistance = dot(centeredUv, centeredUv) * 2.0f;
    float32_t vignette = saturate(1.0f - edgeDistance * vignetteStrength);
    color *= lerp(1.0f, vignette, 0.66f);

    float32_t3 damageColor = float32_t3(1.0f, 0.22f, 0.16f);
    color = lerp(color, color * damageColor, damageTint);

    PixelShaderOutput output;
    output.color = float32_t4(saturate(color), 1.0f);
    return output;
}
