// 粒子の位置と寿命をGPUで更新し、寿命が尽きた番号を空き枠へ戻す。

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

struct PerFrame
{
    float time;
    float deltaTime;
    float2 padding;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    if (gParticles[particleIndex].color.a != 0.0f)
    {
        gParticles[particleIndex].translate +=
            gParticles[particleIndex].velocity * gPerFrame.deltaTime;
        gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

        float alpha =
            1.0f - (gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
        gParticles[particleIndex].color.a = saturate(alpha);
        float lifeRate = saturate(
            gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
        float smoothLifeRate = lifeRate * lifeRate * (3.0f - 2.0f * lifeRate);
        gParticles[particleIndex].scale = lerp(
            gParticles[particleIndex].initialScale,
            gParticles[particleIndex].initialScale * gParticles[particleIndex].endScale,
            smoothLifeRate);

        if (gParticles[particleIndex].color.a == 0.0f)
        {
            gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);

            int freeListIndex;
            InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

            if ((freeListIndex + 1) < kMaxParticles)
            {
                gFreeList[freeListIndex + 1] = particleIndex;
            }
            else
            {
                // 通常は到達しないが、FreeListが壊れないように戻しておく
                int unused;
                InterlockedAdd(gFreeListIndex[0], -1, unused);
            }
        }
    }
}
