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

[[vk::binding(12, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTTemporalBuffer[]  : register(t12);

[[vk::binding(11, 0)]]
StructuredBuffer<SReservoirPT> reservoirPTInitBuffer : register(t11);

[[vk::binding(13, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTSpacialBuffer  : register(t13);

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
RWTexture2D<float4> ptAlbedoMetalnessOutput : register(u1);

[[vk::binding(1, 2)]]
RWTexture2D<float4> ptEmissionRoughnessOutput : register(u2);

[[vk::binding(2, 2)]]
RWTexture2D<float4> ptNormalOutput : register(u3);

[[vk::binding(3, 2)]]
RWTexture2D<float4> ptWposObjectIdOutput : register(u4);

#include "restir_pathtrace.h"

[shader("raygeneration")]
void main()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 size = uint2(giParams.widthScreen, giParams.heightScreen);
    const uint outputIndex = pixel.y * size.x + pixel.x;
    const uint temporalIndex = (giParams.frameCount + 1) % 2;
    SReservoirPT center = reservoirPTTemporalBuffer[temporalIndex][outputIndex];
    float4 position = ptWposObjectIdOutput[pixel];
    if (position.w < 0.0f || center.samplesNumber == 0)
    {
        reservoirPTSpacialBuffer[outputIndex] = center;
        return;
    }

    float3 geometryNormal, shadingNormal;
    decodeNormals(ptNormalOutput[pixel], geometryNormal, shadingNormal);
    RngStateType rng = initRNG(pixel, size, giParams.frameCount);
    rng.z ^= 0x9e3779b9u; // Separate selection from path generation.

    if (giParams.historyValid != 0)
    {
        SReservoirPT previous = reservoirPTTemporalBuffer[giParams.frameCount % 2][outputIndex];
        previous.samplesNumber = min(previous.samplesNumber, 10u * center.samplesNumber);
        if (previous.samplesNumber > 0 && previous.finalWeight > 0.0f && isfinite(previous.finalWeight))
        {
            // Replay is also necessary on a stationary camera: subpixel
            // jitter can change the primary hit across a silhouette/material.
            restirReplayIndirect(pixel, previous);
            SReservoirPT temporal = SReservoirPT(uint4(0), float4(0.0f), 0.0f, 0, 0.0f, 0.0f);
            temporal.updateReservoir(rng, center, restirTarget(center.radiance.xyz) * center.finalWeight * center.samplesNumber);
            temporal.updateReservoir(rng, previous, restirTarget(previous.radiance.xyz) * previous.finalWeight * previous.samplesNumber);
            temporal.samplesNumber = center.samplesNumber + previous.samplesNumber;
            temporal.finalWeight = restirFinalWeight(temporal.weightSum, temporal.samplesNumber, restirTarget(temporal.radiance.xyz));
            center = temporal;
        }
    }
    // Save temporal history before spatial reuse. Neighbor reads below use the
    // separate immutable initial buffer, never this buffer being written.
    reservoirPTTemporalBuffer[temporalIndex][outputIndex] = center;

    SReservoirPT result = SReservoirPT(uint4(0), float4(0.0f), 0.0f, 0, 0.0f, 0.0f);
    result.updateReservoir(rng, center, restirTarget(center.radiance.xyz) * center.finalWeight * center.samplesNumber);
    uint sampleCount = center.samplesNumber;

    [loop]
    for (int i = 0; i < 7; ++i)
    {
        int2 offset = int2(rand(rng) * 3.0f, rand(rng) * 3.0f) - 1;
        int2 neighborPixel = int2(pixel) + offset;
        if (all(offset == 0) || any(neighborPixel < 0) || any(neighborPixel >= int2(size))) continue;
        float4 neighborPosition = ptWposObjectIdOutput[neighborPixel];
        if (neighborPosition.w < 0.0f) continue;
        float3 neighborGeometry, neighborNormal;
        decodeNormals(ptNormalOutput[neighborPixel], neighborGeometry, neighborNormal);
        float3 delta = neighborPosition.xyz - position.xyz;
        if (dot(delta, delta) > 0.01f || dot(shadingNormal, neighborNormal) < cos(25.0f * M_PI / 180.0f)) continue;
        uint neighborIndex = uint(neighborPixel.y) * size.x + uint(neighborPixel.x);
        SReservoirPT candidate = reservoirPTInitBuffer[neighborIndex];
        if (candidate.samplesNumber == 0 || !isfinite(candidate.finalWeight) || candidate.finalWeight <= 0.0f) continue;

        // Move the NEIGHBOR seed to the RECEIVING surface. Copying the neighbor
        // radiance or replaying the center seed at the neighbor is not a shift.
        restirReplayIndirect(pixel, candidate);
        result.updateReservoir(rng, candidate, restirTarget(candidate.radiance.xyz) * candidate.finalWeight * candidate.samplesNumber);
        sampleCount += candidate.samplesNumber;
    }
    result.samplesNumber = sampleCount;
    result.finalWeight = restirFinalWeight(result.weightSum, sampleCount, restirTarget(result.radiance.xyz));
    // Each invocation owns exactly one output element, including border pixels.
    reservoirPTSpacialBuffer[outputIndex] = result;
}
