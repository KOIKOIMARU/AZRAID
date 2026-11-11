// 粒子の初期状態と、発生時に取り出せる空き番号をGPU側で準備する。

static const uint kMaxParticles = 1024;

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

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    gParticles[particleIndex] = (Particle)0;
    gFreeList[particleIndex] = particleIndex;

    if (particleIndex == 0)
    {
        gFreeListIndex[0] = (int)kMaxParticles - 1;
    }
}
