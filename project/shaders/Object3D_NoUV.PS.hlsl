// UVを持たないモデルの画素色を材質色と方向光から求める。

#include "Object3d.hlsli"

cbuffer MaterialCB : register(b0)
{
    float4 gMaterialColor;
    int gEnableLighting;
    float3 padding;
    float4x4 uvTransform; // 未使用
};

cbuffer DirectionalLightCB : register(b3)
{
    DirectionalLight gDirectionalLight;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(-gDirectionalLight.direction.xyz);
    float NdotL = saturate(dot(normal, lightDir));

    float lighting = 1.0f;
    if (gEnableLighting == 1)
    {
        lighting = NdotL; // Lambert
    }
    else if (gEnableLighting == 2)
    {
        lighting = NdotL * 0.5f + 0.5f; // Half-Lambert
    }

    float3 litColor = gMaterialColor.rgb * gDirectionalLight.color.rgb * lighting * gDirectionalLight.intensity;

    return float4(litColor, gMaterialColor.a);
}
