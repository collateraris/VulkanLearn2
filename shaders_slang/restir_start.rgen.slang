#include "gi_pathtrace.h"
#include "brdf.h"

#define STANDARD_RAY_INDEX 0
#define SHADOW_RAY_INDEX 1

// Defines after how many bounces will be the Russian Roulette applied
#define MIN_BOUNCES 3

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t1);

[[vk::binding(6, 0)]]
SamplerState      linearSampler : register(s6);

[[vk::binding(2, 0)]]
RaytracingAccelerationStructure topLevelAS	: register(t2);

[[vk::binding(4, 0)]]
StructuredBuffer<SLight> lightsBuffer  : register(t4);

[[vk::binding(7, 0)]]
StructuredBuffer<SMaterialData> matBuffer  : register(t7);

[[vk::binding(11, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTInitBuffer  : register(t11);

[[vk::binding(14, 0)]]
StructuredBuffer<SAliasTable> aliasTable  : register(t14);

[[vk::binding(15, 0)]]
StructuredBuffer<SCell> cellGridData  : register(t15);

[[vk::binding(0, 1)]]
cbuffer MyContantBuffer : register(b0)
{
	SGlobalGIParams giParams;
};

[[vk::binding(0, 2)]]
RWTexture2D<float4> ptAlbedoMetalnessOutput : register(u0);

[[vk::binding(1, 2)]]
RWTexture2D<float4> ptEmissionRoughnessOutput : register(u1);

[[vk::binding(2, 2)]]
RWTexture2D<float4> ptNormalOutput : register(u2);

[[vk::binding(3, 2)]]
RWTexture2D<float4> ptWposObjectIdOutput : register(u3);


#include "restir_pathtrace.h"

[shader("raygeneration")]
void main()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint index = pixel.y * giParams.widthScreen + pixel.x;
    SReservoirPT reservoir = SReservoirPT(uint4(0), float4(0.0f), 0.0f, 0, 0.0f, 0.0f);
    if (ptWposObjectIdOutput[pixel].w >= 0.0f)
    {
        RngStateType selectionRng = initRNG(pixel, uint2(giParams.widthScreen, giParams.heightScreen), giParams.frameCount);
        selectionRng.z ^= 0x85ebca6bu;
        // More independent paths improve emitter coverage in raw frames.
        // Resample them with their true count; this changes variance, not power.
        const uint pathCandidates = 8;
        [loop]
        for (uint i = 0; i < pathCandidates; ++i)
        {
            SReservoirPT candidate = SReservoirPT(pcg4d(uint4(pixel, giParams.frameCount, i)),
                float4(0.0f), 0.0f, 1, 1.0f, 0.0f);
            candidate.radiance = float4(restirTraceIndirect(pixel, candidate.randomSeed), 1.0f);
            reservoir.updateReservoir(selectionRng, candidate, restirTarget(candidate.radiance.xyz));
        }
        reservoir.finalWeight = restirFinalWeight(reservoir.weightSum, reservoir.samplesNumber, restirTarget(reservoir.radiance.xyz));
    }
    reservoirPTInitBuffer[index] = reservoir;
}
