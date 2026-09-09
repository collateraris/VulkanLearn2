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

[[vk::binding(8, 0)]]
RWStructuredBuffer<SReservoir> reservoirInitBuffer  : register(t8);

[[vk::binding(9, 0)]]
RWStructuredBuffer<SReservoir> reservoirTemporalBuffer[]  : register(t9);

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
    uint index = pixel.y * size.x + pixel.x;
    uint current = (giParams.frameCount + 1) % 2;
    SReservoir initial = reservoirInitBuffer[index];
    if (giParams.historyValid == 0 || !restirValidLight(initial) || ptWposObjectIdOutput[pixel].w < 0.0f)
    {
        reservoirTemporalBuffer[current][index] = initial;
        return;
    }
    MaterialProperties material = restirMaterial(ptAlbedoMetalnessOutput[pixel], ptEmissionRoughnessOutput[pixel]);
    float3 geometryNormal, shadingNormal;
    decodeNormals(ptNormalOutput[pixel], geometryNormal, shadingNormal);
    float3 V = restirViewDirection(pixel);
    float3 position = ptWposObjectIdOutput[pixel].xyz;
    SReservoir previous = reservoirTemporalBuffer[giParams.frameCount % 2][index];
    previous.samplesNumber = restirValidLight(previous) && uint(previous.bary__.z) == restirLightCellIndex(position) ? min(previous.samplesNumber, 10u * initial.samplesNumber) : 0;
    RngStateType rng = initRNG(pixel, size, giParams.frameCount);
    rng.z ^= 0x27d4eb2fu;
    SReservoir result = SReservoir(0.0f, -1, 0, 0.0f, float4(0.0f));
    float target = restirDirectTarget(initial, position, material, geometryNormal, shadingNormal, V);
    result.updateReservoir(rng, initial, target * initial.finalWeight * initial.samplesNumber);
    target = restirDirectTarget(previous, position, material, geometryNormal, shadingNormal, V);
    result.updateReservoir(rng, previous, target * previous.finalWeight * previous.samplesNumber);
    result.samplesNumber = initial.samplesNumber + previous.samplesNumber;
    target = restirDirectTarget(result, position, material, geometryNormal, shadingNormal, V);
    result.finalWeight = restirFinalWeight(result.weightSum, result.samplesNumber, target);
    reservoirTemporalBuffer[current][index] = result;
}
