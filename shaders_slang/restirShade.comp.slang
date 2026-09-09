#include "gi_pathtrace.h"
#include "brdf.h"

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

[[vk::binding(10, 0)]]
RWStructuredBuffer<SReservoir> reservoirSpacialBuffer  : register(t10);

[[vk::binding(11, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTInitBuffer  : register(t11);

[[vk::binding(12, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTTemporalBuffer[]  : register(t12);

[[vk::binding(13, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTSpacialBuffer  : register(t13);

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

[[vk::binding(4, 2)]]
RWTexture2D<float4> ptOutput : register(u4);

[[vk::binding(0, 0)]]
StructuredBuffer<SVertex> vertices[] : register(t0);
[[vk::binding(2, 0)]]
RaytracingAccelerationStructure topLevelAS : register(t2);
[[vk::binding(3, 0)]]
StructuredBuffer<SObjectData> objectBuffer : register(t3);
[[vk::binding(5, 0)]]
StructuredBuffer<int> indices[] : register(t5);

#include "restir_lighting.h"
#include "restir_visibility.h"

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadId.xy;
    if (pixel.x >= giParams.widthScreen || pixel.y >= giParams.heightScreen) return;
    uint index = pixel.y * giParams.widthScreen + pixel.x;
    float4 position = ptWposObjectIdOutput[pixel];
    if (position.w < 0.0f)
    {
        ptOutput[pixel] = float4(min(restirEnvironment(-restirViewDirection(pixel)), float3(65504.0f)), 1.0f);
        return;
    }
    MaterialProperties material = restirMaterial(ptAlbedoMetalnessOutput[pixel], ptEmissionRoughnessOutput[pixel]);
    float3 geometryNormal, shadingNormal;
    decodeNormals(ptNormalOutput[pixel], geometryNormal, shadingNormal);
    float3 V = restirViewDirection(pixel);
    restirOrientNormals(V, geometryNormal, shadingNormal);
    SReservoir light = reservoirSpacialBuffer[index];
    float3 direct = restirEvaluateDirect(light, position.xyz, material, geometryNormal, shadingNormal, V);
    if (!any(direct > 0.0f) || !restirLightVisible(light, position.xyz, geometryNormal)) direct = float3(0.0f);
    SReservoirPT indirect = reservoirPTSpacialBuffer[index];
    float3 radiance = material.emissive + restirFiniteRadiance(direct * light.finalWeight)
        + restirFiniteRadiance(indirect.radiance.xyz * indirect.finalWeight);
    // The output image is RGBA16F; finite float32 values can still overflow it.
    ptOutput[pixel] = float4(min(restirFiniteRadiance(radiance), float3(65504.0f)), 1.0f);
}
