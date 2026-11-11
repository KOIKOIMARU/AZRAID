// 3D描画で頂点とピクセルのシェーダーが共用するデータ型。

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : TEXCOORD1;
};

struct Material
{
    float4 color;
    int lightingMode;
    float shininess;
    float environmentCoefficient;
    float alphaReference;
    float3 specularColor;
    float roughness;
    float metallic;
    float normalStrength;
    float2 padding1;
    float4x4 uvTransform;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
    float4x4 lightViewProjection;
    float shadowStrength;
    float shadowBias;
    float shadowNormalBias;
    float shadowMapEnabled;
};

struct Camera
{
    float3 worldPosition;
    float padding;
};

struct WellForGPU
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

struct PointLight
{
    float4 color;
    float3 position;
    float intensity;
    float radius;
    float decay;
    float2 padding;
};

struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
    float padding;
};
