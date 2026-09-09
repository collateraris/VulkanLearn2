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

[[vk::binding(8, 0)]]
RWStructuredBuffer<SReservoir> reservoirInitBuffer  : register(t8);

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
#define RESTIR_DI_RAYGEN 1
#include "restir_di_target.h"

[shader("raygeneration")]
void main()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 size = uint2(giParams.widthScreen, giParams.heightScreen);
    uint index = pixel.y * size.x + pixel.x;
    SReservoir reservoir = SReservoir(0.0f, -1, 0, 0.0f, float4(0.0f));
    float3 V = restirViewDirection(pixel);
    float3 origin = mul(giParams.viewInverse, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    IndirectGbufferRayPayload payload = restirTraceRay(origin, -V);
    ptAlbedoMetalnessOutput[pixel] = float4(0.0f);
    ptEmissionRoughnessOutput[pixel] = float4(0.0f);
    ptNormalOutput[pixel] = float4(0.0f);
    ptWposObjectIdOutput[pixel] = float4(0.0f, 0.0f, 0.0f, -1.0f);

    if (payload.hasHit())
    {
        MaterialProperties material = restirMaterial(payload.albedo_metalness, payload.emission_roughness);
        float3 geometryNormal, shadingNormal;
        decodeNormals(payload.normal_, geometryNormal, shadingNormal);
        restirOrientNormals(V, geometryNormal, shadingNormal);
        ptAlbedoMetalnessOutput[pixel] = float4(material.baseColor, material.metalness);
        ptEmissionRoughnessOutput[pixel] = float4(material.emissive, material.roughness);
        ptNormalOutput[pixel] = encodeNormals(geometryNormal, shadingNormal);
        ptWposObjectIdOutput[pixel] = payload.position_objectID;

        if (giParams.lightsCount > 0)
        {
            RngStateType rng = initRNG(pixel, size, giParams.frameCount);
            rng.z ^= 0xc2b2ae35u;
            const uint candidatesPerGroup = 8;
            uint sampleCount = 0;
            [loop]
            for (uint first = 0; first < RIS_CANDIDATES_LIGHTS; first += candidatesPerGroup)
            {
                // First resample a small group without visibility, then trace
                // only its representative. The second RIS level uses the
                // group's inverse PDF and sample count, preserving its energy.
                SReservoir group = SReservoir(0.0f, -1, 0, 0.0f, float4(0.0f));
                [loop]
                for (uint i = first; i < min(first + candidatesPerGroup, uint(RIS_CANDIDATES_LIGHTS)); ++i)
                {
                    SReservoir candidate = restirSampleLight(rng, payload.position_objectID.xyz, aliasTable, cellGridData);
                    float target = restirTarget(restirEvaluateDirect(candidate, payload.position_objectID.xyz,
                        material, geometryNormal, shadingNormal, V));
                    group.updateReservoir(rng, candidate, target * candidate.finalWeight);
                }
                float groupTarget = restirTarget(restirEvaluateDirect(group, payload.position_objectID.xyz,
                    material, geometryNormal, shadingNormal, V));
                group.finalWeight = restirFinalWeight(group.weightSum, group.samplesNumber, groupTarget);
                float target = restirDirectTarget(group, payload.position_objectID.xyz,
                    material, geometryNormal, shadingNormal, V);
                reservoir.updateReservoir(rng, group, target * group.finalWeight * group.samplesNumber);
                sampleCount += group.samplesNumber;
            }
            reservoir.samplesNumber = sampleCount;
            float target = restirDirectTarget(reservoir, payload.position_objectID.xyz,
                material, geometryNormal, shadingNormal, V);
            reservoir.finalWeight = restirFinalWeight(reservoir.weightSum, reservoir.samplesNumber, target);
            // Visibility is part of the target, including its support floor;
            // never zero the resulting reservoir weight after resampling.
        }
    }
    reservoirInitBuffer[index] = reservoir;
}
