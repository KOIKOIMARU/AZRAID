// 粒子ごとの姿勢と色を使い、同じ四角形を複数の粒子として描く。

struct VertexIn
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR0;
};

struct Particle
{
    float3 translate;
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
    float3 initialScale;
    float endScale;
};

struct PerView
{
    float4x4 viewProjection;
    float4x4 billboardMatrix;
};

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

VertexOut main(VertexIn input, uint instanceId : SV_InstanceID)
{
    VertexOut output;
    Particle particle = gParticles[instanceId];

    float4x4 worldMatrix = gPerView.billboardMatrix;
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;

    output.position = mul(input.position, mul(worldMatrix, gPerView.viewProjection));
    output.texcoord = input.texcoord;
    output.color = particle.color;
    return output;
}
