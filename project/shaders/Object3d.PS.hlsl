// 3D物体の画素色を求める。テクスチャ・材質・ライト・影・環境反射を組み合わせる。

#include "Object3d.hlsli" 

cbuffer MaterialCB : register(b0)
{
    Material gMaterial;
};

// glTFの材質色。白なら従来のテクスチャ／Object3dカラーのみの描画と同じ。
cbuffer ModelMaterialColorCB : register(b7)
{
    float4 gModelBaseColor;
};

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
Texture2D<float> gShadowMap : register(t2);
Texture2D<float4> gNormalTexture : register(t3);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

cbuffer CameraCB : register(b2)
{
    Camera gCamera;
};

cbuffer DirectionalLightCB : register(b3)
{
    DirectionalLight gDirectionalLight;
};

cbuffer PointLightCB : register(b4)
{
    PointLight gPointLight;
};

cbuffer SpotLightCB : register(b5)
{
    SpotLight gSpotLight;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 光源から見た位置を影用の深度と比較し、直射光が届く割合を求める。
float CalculateDirectionalShadow(float3 worldPosition, float3 normal, float3 lightToSurface)
{
    if (gDirectionalLight.shadowMapEnabled < 0.5f ||
        gDirectionalLight.shadowStrength <= 0.001f)
    {
        return 1.0f;
    }

    float4 lightClip =
        mul(float4(worldPosition, 1.0f), gDirectionalLight.lightViewProjection);
    if (lightClip.w <= 0.0001f)
    {
        return 1.0f;
    }

    float3 lightNdc = lightClip.xyz / lightClip.w;
    if (lightNdc.z < 0.0f || lightNdc.z > 1.0f)
    {
        return 1.0f;
    }

    float2 shadowUv = lightNdc.xy * float2(0.5f, -0.5f) + 0.5f;
    if (any(shadowUv < 0.002f) || any(shadowUv > 0.998f))
    {
        return 1.0f;
    }

    uint shadowWidth = 0;
    uint shadowHeight = 0;
    gShadowMap.GetDimensions(shadowWidth, shadowHeight);
    float2 texelSize = 1.0f / float2(shadowWidth, shadowHeight);

    float normalBias =
        (1.0f - saturate(dot(normal, lightToSurface))) *
        gDirectionalLight.shadowNormalBias;
    float receiverDepth = lightNdc.z - gDirectionalLight.shadowBias - normalBias;

    // 境界の深度は遮蔽物と背景が混ざるため、そこからぼかし幅を決めると
    // 移動のたびに影が締まったり膨らんだりする。固定幅の密なPCFで境界を保つ。
    // 既存の光空間texelスナップと合わせ、追加パスや描画資源は使わない。
    float visibility = 0.0f;
    float totalWeight = 0.0f;
    [unroll]
    for (int y = -2; y <= 2; ++y) {
        [unroll]
        for (int x = -2; x <= 2; ++x) {
            float weight = float((3 - abs(x)) * (3 - abs(y)));
            visibility += gShadowMap.SampleCmpLevelZero(gShadowSampler,
                shadowUv + float2(x, y) * texelSize * 0.75f, receiverDepth) * weight;
            totalWeight += weight;
        }
    }
    visibility /= totalWeight;
    // 影マップの外周を段差なく抜く。遠景で四角い影の終端を見せない。
    float edge = max(abs(shadowUv.x - 0.5f), abs(shadowUv.y - 0.5f)) * 2.0f;
    float coverage = 1.0f - smoothstep(0.82f, 0.98f, edge);
    return 1.0f - (1.0f - visibility) * gDirectionalLight.shadowStrength * coverage;
}

float3 SampleEnvironment(float3 direction)
{
    return gEnvironmentTexture.Sample(gSampler, normalize(direction)).rgb;
}

float3 CalculateDiffuseEnvironment(float3 normal, float roughness, float metallic)
{
    float upFactor = saturate(normal.y * 0.5f + 0.5f);
    float3 skyDirection = normalize(normal + float3(0.0f, 0.55f, 0.0f));
    float3 horizonDirection =
        normalize(float3(normal.x, 0.18f, normal.z) + float3(0.0f, 0.0f, 0.001f));

    float3 skyColor = SampleEnvironment(skyDirection);
    float3 horizonColor = SampleEnvironment(horizonDirection);
    float3 groundBounce = float3(0.40f, 0.43f, 0.48f);
    float3 upperHemisphere = lerp(horizonColor, skyColor, upFactor);
    float3 hemisphereColor = lerp(groundBounce, upperHemisphere, upFactor);

    float surfaceWrap = lerp(0.78f, 1.04f, upFactor);
    float diffuseStrength =
        lerp(0.135f, 0.205f, roughness) *
        lerp(1.0f, 0.52f, metallic);
    return hemisphereColor * surfaceWrap * diffuseStrength;
}

// テクスチャの法線をワールド座標の方向へ変換し、面の細かい凹凸をライティングに反映する。
float3 ApplyNormalMap(
    float3 geometryNormal,
    float3 worldPosition,
    float2 uv,
    float normalStrength)
{
    float strength = saturate(normalStrength);
    if (strength <= 0.001f)
    {
        return geometryNormal;
    }

    float3 dp1 = ddx(worldPosition);
    float3 dp2 = ddy(worldPosition);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2Perpendicular = cross(dp2, geometryNormal);
    float3 dp1Perpendicular = cross(geometryNormal, dp1);
    float3 tangent = dp2Perpendicular * duv1.x + dp1Perpendicular * duv2.x;
    float3 bitangent = dp2Perpendicular * duv1.y + dp1Perpendicular * duv2.y;

    float tangentLength = dot(tangent, tangent);
    float bitangentLength = dot(bitangent, bitangent);
    float inverseLength = rsqrt(max(max(tangentLength, bitangentLength), 0.000001f));
    tangent *= inverseLength;
    bitangent *= inverseLength;

    float3 tangentNormal = gNormalTexture.Sample(gSampler, uv).xyz * 2.0f - 1.0f;
    tangentNormal.xy *= strength;
    tangentNormal = normalize(tangentNormal);

    return normalize(
        tangent * tangentNormal.x +
        bitangent * tangentNormal.y +
        geometryNormal * tangentNormal.z);
}

float3 SampleFlameFrame(float2 uv, float frame)
{
    // 公開された元画像の実配置は16列×5行。各コマは同じ位置に火元がある。
    float2 cell = float2(fmod(frame, 16.0f), floor(frame / 16.0f));
    float2 pixel = 0.5f / float2(128.0f, 1024.0f / 5.0f);
    uv.y *= 0.965f; // 下端の余白を除き、発光している部分を部品の表面につなぐ。
    float2 local = lerp(pixel, 1.0f - pixel, saturate(uv));
    return gTexture.SampleLevel(gSampler, (cell + local) / float2(16.0f, 5.0f), 0.0f).rgb;
}

float4 DrawFlame(float2 uv)
{
    float time = gMaterial.uvTransform[0][3];
    float seed = gMaterial.uvTransform[1][3];
    if (frac(seed * 0.37f) > 0.5f) { uv.x = 1.0f - uv.x; }
    // 火元を動かさず、上部だけが流れる。画像全体を拡大縮小して脈打たせない。
    uv.x += (1.0f - uv.y) * (1.0f - uv.y) * sin(time * 0.65f + seed) * 0.045f;
    float frame = fmod(time * (28.0f + frac(seed) * 3.0f) + seed * 13.7f, 80.0f);
    float current = floor(frame);
    float3 emission = lerp(SampleFlameFrame(uv, current),
        SampleFlameFrame(uv, fmod(current + 1.0f, 80.0f)), frac(frame));
    float intensity = max(emission.r, max(emission.g, emission.b));
    // 炎の発光は加算で描き、煙の吸収は別の透過面に任せる。
    // LDR描画先なので、透過率でRGBを割って色を白く飽和させない。
    float opacity = pow(saturate(intensity), 0.45f) * 0.85f;
    opacity *= smoothstep(0.0002f, 0.002f, intensity);
    float3 colour = emission * gMaterial.color.rgb * float3(0.95f, 0.85f, 0.80f);
    return float4(colour, opacity * gMaterial.color.a);
}

float4 SampleWreckSmoke(float2 uv, float frame)
{
    float2 cell = float2(fmod(frame, 8.0f), floor(frame / 8.0f));
    float2 local = lerp(0.5f / 128.0f, 1.0f - 0.5f / 128.0f, saturate(uv));
    return gTexture.SampleLevel(gSampler, (cell + local) / 8.0f, 0.0f);
}

float4 DrawWreckSmoke(float2 uv)
{
    float age = gMaterial.uvTransform[0][3];
    float seed = gMaterial.uvTransform[1][3];
    if (frac(seed * 0.17f) > 0.5f) { uv.x = 1.0f - uv.x; }
    float frame = fmod(age * 18.0f + seed * 7.3f, 64.0f);
    float current = floor(frame);
    float4 soot = lerp(SampleWreckSmoke(uv, current),
        SampleWreckSmoke(uv, fmod(current + 1.0f, 64.0f)), frac(frame));
    float shade = 0.55f + sqrt(soot.r) * 0.65f;
    return float4(gMaterial.color.rgb * shade, saturate(soot.a * 1.7f) * gMaterial.color.a);
}

float FuelNoise(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float4 n = frac(sin(float4(dot(cell, float2(127.1f, 311.7f)),
        dot(cell + float2(1, 0), float2(127.1f, 311.7f)),
        dot(cell + float2(0, 1), float2(127.1f, 311.7f)),
        dot(cell + 1.0f, float2(127.1f, 311.7f)))) * 43758.5453f);
    return lerp(lerp(n.x, n.y, f.x), lerp(n.z, n.w, f.x), f.y);
}

float4 DrawBurntFuel(float2 uv)
{
    float seed = gMaterial.uvTransform[1][3];
    float2 p = (uv - 0.5f) * 2.0f;
    float rough = FuelNoise(p * 4.2f + seed) * 0.10f + FuelNoise(p * 11.0f - seed) * 0.04f;
    float edge = min(length(p / float2(0.83f, 0.94f)),
        length((p - float2(0.25f, -0.28f)) / float2(0.65f, 0.59f))) + rough;
    float mask = 1.0f - smoothstep(0.88f, 1.02f, edge);
    float3 colour = lerp(float3(0.012f, 0.009f, 0.007f), float3(0.040f, 0.031f, 0.022f),
        FuelNoise(p * 6.0f + seed));
    return float4(colour, mask * gMaterial.color.a);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 墜落時の炎・煙・燃料は、それぞれ専用の計算で色と透明度を求める。
    if (gMaterial.lightingMode == 3) {
        output.color = DrawFlame(input.texcoord);
        if (output.color.a < 0.0005f) { discard; }
        return output;
    }
    if (gMaterial.lightingMode == 4) {
        output.color = DrawWreckSmoke(input.texcoord);
        if (output.color.a < 0.003f) { discard; }
        return output;
    }
    if (gMaterial.lightingMode == 5) {
        output.color = DrawBurntFuel(input.texcoord);
        if (output.color.a < 0.003f) { discard; }
        return output;
    }

    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 tex = gTexture.Sample(gSampler, uv);
    tex *= gModelBaseColor;
    float3 geometryNormal = normalize(input.normal);
    float3 normal = ApplyNormalMap(
        geometryNormal,
        input.worldPosition,
        uv,
        gMaterial.normalStrength);
    float3 baseColor = gMaterial.color.rgb * tex.rgb;
    float roughness = max(saturate(gMaterial.roughness), 0.04f);
    float metallic = saturate(gMaterial.metallic);
    float3 diffuseAlbedo = baseColor * (1.0f - metallic * 0.65f);
    float3 specularTint = lerp(gMaterial.specularColor, baseColor, metallic);
    float specularPower = lerp(max(gMaterial.shininess, 1.0f), 12.0f, roughness);
    float specularEnergy =
        lerp(0.95f, 0.24f, roughness) *
        lerp(1.0f, 1.45f, metallic);

    if (tex.a <= gMaterial.alphaReference)
    {
        discard;
    }

    if (gMaterial.lightingMode != 0)
    {
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // 方向光は画面全体で向きが一定。面の法線との角度と影から受ける光を計算する。
        float3 Ld = normalize(-gDirectionalLight.direction);
        float NdotLd = dot(normal, Ld);
        float diffuseFactorDir = saturate(NdotLd);
        if (gMaterial.lightingMode == 2) {
            float halfLambert = NdotLd * 0.5f + 0.5f;
            diffuseFactorDir = halfLambert * halfLambert;
        }

        float directionalShadow =
            CalculateDirectionalShadow(input.worldPosition, normal, Ld);

        float3 diffuseDir =
            diffuseAlbedo *
            gDirectionalLight.color.rgb *
            diffuseFactorDir *
            gDirectionalLight.intensity *
            directionalShadow;

        float3 Hd = normalize(Ld + toEye);
        float specularPowDir =
            pow(saturate(dot(normal, Hd)), specularPower);
        float3 specularDir =
            gDirectionalLight.color.rgb *
            gDirectionalLight.intensity *
            specularPowDir *
            specularTint *
            specularEnergy *
            directionalShadow;

        float3 LpVec = gPointLight.position - input.worldPosition;
        float distP = length(LpVec);
        float3 Lp =
            (distP > 0.0001f) ? (LpVec / distP) : float3(0.0f, 1.0f, 0.0f);
        float attenP =
            pow(saturate(1.0f - distP / gPointLight.radius), gPointLight.decay);
        float NdotLp = dot(normal, Lp);
        float diffuseFactorPoint = saturate(NdotLp);
        if (gMaterial.lightingMode == 2) {
            float halfLambertP = NdotLp * 0.5f + 0.5f;
            diffuseFactorPoint = halfLambertP * halfLambertP;
        }

        float3 diffusePoint =
            diffuseAlbedo *
            gPointLight.color.rgb *
            diffuseFactorPoint *
            gPointLight.intensity *
            attenP;

        float3 Hp = normalize(Lp + toEye);
        float specularPowPoint =
            pow(saturate(dot(normal, Hp)), specularPower);
        float3 specularPoint =
            gPointLight.color.rgb *
            gPointLight.intensity *
            attenP *
            specularPowPoint *
            specularTint *
            specularEnergy;

        // スポットライトは距離に加え、光が届く円すいの角度でも減衰させる。
        float3 LsFromLight = input.worldPosition - gSpotLight.position;
        float distS = length(LsFromLight);
        float3 Ls =
            (distS > 0.0001f) ?
            (LsFromLight / distS) :
            float3(0.0f, -1.0f, 0.0f);

        float attenS =
            pow(saturate(1.0f - distS / gSpotLight.distance), gSpotLight.decay);
        float cosTheta = dot(Ls, normalize(gSpotLight.direction));
        float denom =
            max(gSpotLight.cosFalloffStart - gSpotLight.cosAngle, 0.0001f);
        float falloff =
            saturate((cosTheta - gSpotLight.cosAngle) / denom);
        float spotFactor = gSpotLight.intensity * attenS * falloff;

        float3 lightToSurface = -Ls;
        float NdotLs = dot(normal, lightToSurface);
        float diffuseFactorSpot = saturate(NdotLs);
        if (gMaterial.lightingMode == 2) {
            float halfLambertS = NdotLs * 0.5f + 0.5f;
            diffuseFactorSpot = halfLambertS * halfLambertS;
        }

        float3 diffuseSpot =
            diffuseAlbedo *
            gSpotLight.color.rgb *
            diffuseFactorSpot *
            spotFactor;

        float3 Hs = normalize(lightToSurface + toEye);
        float specularPowSpot =
            pow(saturate(dot(normal, Hs)), specularPower);
        float3 specularSpot =
            gSpotLight.color.rgb *
            spotFactor *
            specularPowSpot *
            specularTint *
            specularEnergy;

        float3 cameraToPosition =
            normalize(input.worldPosition - gCamera.worldPosition);
        // 反射方向から環境画像を読み、粗さや金属度に合わせて反射の強さを変える。
        float3 reflectedVector = reflect(cameraToPosition, normal);
        float viewFresnel =
            pow(1.0f - saturate(dot(normal, toEye)), 4.0f);
        float environmentScale =
            lerp(1.10f, 0.38f, roughness) *
            lerp(1.0f, 1.55f, metallic) *
            lerp(0.82f, 1.28f, viewFresnel);
        float3 environmentColor =
            lerp(
                SampleEnvironment(reflectedVector),
                SampleEnvironment(normal + float3(0.0f, 0.35f, 0.0f)),
                saturate(roughness * 0.70f)) *
            gMaterial.environmentCoefficient *
            environmentScale;
        float3 diffuseEnvironment =
            diffuseAlbedo *
            CalculateDiffuseEnvironment(normal, roughness, metallic);
        float rimFactor = pow(
            1.0f - saturate(dot(normal, toEye)),
            lerp(3.4f, 2.5f, metallic));
        float rimVisibility =
            saturate(0.32f + (1.0f - diffuseFactorDir) * 0.68f);
        float3 stylizedRim =
            lerp(float3(0.34f, 0.62f, 1.0f), specularTint, metallic * 0.42f) *
            rimFactor *
            rimVisibility *
            lerp(0.024f, 0.052f, 1.0f - roughness);

        output.color.rgb =
            diffuseDir + specularDir +
            diffusePoint + specularPoint +
            diffuseSpot + specularSpot +
            diffuseEnvironment +
            environmentColor +
            stylizedRim;
        output.color.a = gMaterial.color.a * tex.a;
    }
    else
    {
        output.color = gMaterial.color * tex;
    }
    
    return output;

}

