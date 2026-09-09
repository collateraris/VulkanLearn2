#include "gi_pathtrace.h"
#include "brdf.h"

[[vk::binding(0, 0)]]
StructuredBuffer<SVertex> vertices[] : register(t0);
[[vk::binding(2, 0)]]
RaytracingAccelerationStructure topLevelAS : register(t2);
[[vk::binding(3, 0)]]
StructuredBuffer<SObjectData> objectBuffer : register(t3);
[[vk::binding(5, 0)]]
StructuredBuffer<int> indices[] : register(t5);

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t1);

[[vk::binding(6, 0)]]
SamplerState      linearSampler : register(s6);

[[vk::binding(4, 0)]]
StructuredBuffer<SLight> lightsBuffer  : register(t4);

[[vk::binding(7, 0)]]
StructuredBuffer<SMaterialData> matBuffer  : register(t7);

[[vk::binding(9, 0)]]
RWStructuredBuffer<SReservoir> reservoirTemporalBuffer[]  : register(t9);

[[vk::binding(10, 0)]]
RWStructuredBuffer<SReservoir> reservoirSpacialBuffer  : register(t10);

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

#include "restir_lighting.h"
#include "restir_visibility.h"
#include "restir_di_target.h"

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadId.xy;
    uint2 size = uint2(giParams.widthScreen, giParams.heightScreen);
    if (any(pixel >= size)) return;
    const uint outputIndex = pixel.y * size.x + pixel.x;
    uint current = (giParams.frameCount + 1) % 2;
    SReservoir center = reservoirTemporalBuffer[current][outputIndex];
    float4 position = ptWposObjectIdOutput[pixel];
    if (position.w < 0.0f || !restirValidLight(center))
    {
        reservoirSpacialBuffer[outputIndex] = center;
        return;
    }
    MaterialProperties material = restirMaterial(ptAlbedoMetalnessOutput[pixel], ptEmissionRoughnessOutput[pixel]);
    float3 geometryNormal, shadingNormal;
    decodeNormals(ptNormalOutput[pixel], geometryNormal, shadingNormal);
    float3 V = restirViewDirection(pixel);
    RngStateType rng = initRNG(pixel, size, giParams.frameCount);
    rng.z ^= 0x165667b1u;
    SReservoir result = SReservoir(0.0f, -1, 0, 0.0f, float4(0.0f));
    float target = restirDirectTarget(center, position.xyz, material, geometryNormal, shadingNormal, V);
    result.updateReservoir(rng, center, target * center.finalWeight * center.samplesNumber);
    uint sampleCount = center.samplesNumber;
    [loop]
    for (int i = 0; i < 7; ++i)
    {
        int2 offset = int2(rand(rng) * 5.0f, rand(rng) * 5.0f) - 2;
        int2 neighborPixel = int2(pixel) + offset;
        if (all(offset == 0) || any(neighborPixel < 0) || any(neighborPixel >= int2(size))) continue;
        float4 neighborPosition = ptWposObjectIdOutput[neighborPixel];
        if (neighborPosition.w < 0.0f || restirLightCellIndex(neighborPosition.xyz) != restirLightCellIndex(position.xyz)) continue;
        float3 neighborGeometry, neighborNormal;
        decodeNormals(ptNormalOutput[neighborPixel], neighborGeometry, neighborNormal);
        float3 delta = neighborPosition.xyz - position.xyz;
        if (dot(delta, delta) > 0.01f || dot(shadingNormal, neighborNormal) < cos(25.0f * M_PI / 180.0f)) continue;
        uint neighborIndex = uint(neighborPixel.y) * size.x + uint(neighborPixel.x);
        SReservoir candidate = reservoirTemporalBuffer[current][neighborIndex];
        if (!restirValidLight(candidate) || candidate.samplesNumber == 0) continue;
        // Evaluate the neighbor's sample using the CENTER material, view and
        // position. Reusing the neighbor's own target blurs lighting across edges.
        target = restirDirectTarget(candidate, position.xyz, material, geometryNormal, shadingNormal, V);
        result.updateReservoir(rng, candidate, target * candidate.finalWeight * candidate.samplesNumber);
        sampleCount += candidate.samplesNumber;
    }
    result.samplesNumber = sampleCount;
    target = restirDirectTarget(result, position.xyz, material, geometryNormal, shadingNormal, V);
    result.finalWeight = restirFinalWeight(result.weightSum, sampleCount, target);
    reservoirSpacialBuffer[outputIndex] = result;
}
