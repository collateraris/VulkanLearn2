#include "gi_pathtrace.h"
#include "brdf.h"

[[vk::binding(4, 0)]]
StructuredBuffer<SLight> lightsBuffer  : register(t4);

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

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= giParams.widthScreen ||  dispatchThreadId.y >= giParams.heightScreen)
    {
        return;
    };

    float4 wposObjectId = ptWposObjectIdOutput[dispatchThreadId.xy];
    uint curTemporalIndx = (giParams.frameCount + 1) % 2;
    uint reservoirIndx = dispatchThreadId.y * (giParams.widthScreen + 1) + dispatchThreadId.x;

    SReservoir reservoirTemporal = reservoirTemporalBuffer[curTemporalIndx][reservoirIndx];

    if (wposObjectId.w < 0.f || reservoirTemporal.lightSampler < 0)
    {
        reservoirSpacialBuffer[reservoirIndx] = reservoirTemporal;
        return;
    };

    float4 albedoMetalness = ptAlbedoMetalnessOutput[dispatchThreadId.xy];
    float4 emissiveRoughtness = ptEmissionRoughnessOutput[dispatchThreadId.xy];
    float4 normalEncode = ptNormalOutput[dispatchThreadId.xy];
    
    MaterialProperties material = (MaterialProperties) 0;
    material.baseColor = albedoMetalness.xyz;
    material.metalness = albedoMetalness.w;
    material.emissive = emissiveRoughtness.xyz;
    material.roughness = emissiveRoughtness.w;

    float3 geometryNormal;
    float3 shadingNormal;
    decodeNormals(normalEncode, geometryNormal, shadingNormal);

    uint2 LaunchIndex = dispatchThreadId.xy;
	uint2 LaunchDimensions = uint2(giParams.widthScreen,giParams.heightScreen);
    float2 pixelCenter = float2(LaunchIndex) + float2(0.5);
    float2 inUV = pixelCenter/float2(LaunchDimensions);
    float2 d = inUV * 2.0 - 1.0;

    float4 origin    = mul(giParams.viewInverse , float4(0, 0, 0, 1));
    float4 target    = mul(giParams.projInverse , float4(d.x, d.y, 1, 1));
    float4 direction = mul(giParams.viewInverse , float4(normalize(target.xyz), 0));

	float3 rayDir = normalize(direction.xyz);
    float3 V = -rayDir;

    // Prepare data needed to evaluate the light
    float3 lightVector;
    float lightDistance;
    SLight light = lightsBuffer[reservoirTemporal.lightSampler];
    getLightData(light, wposObjectId.xyz, lightVector, lightDistance);
    float3 L = normalize(lightVector);

    float target_pdf = luminance(evalCombinedBRDF(shadingNormal, L, V, material) * (getLightIntensityAtPoint(light, lightDistance) * reservoirTemporal.finalWeight));

    // Initialize random numbers generator
	RngStateType rngState = initRNG(LaunchIndex, LaunchDimensions, giParams.frameCount);

    SReservoir reservoirNew = SReservoir(0.f, -1, 0.f, 0.f);
    reservoirNew.updateReservoir(rngState, reservoirTemporal.lightSampler, target_pdf * reservoirTemporal.samplesNumber);

    uint lightSamplesCount = reservoirTemporal.samplesNumber;

    int2 neighborOffset;
    int2	neighborIndex;
    SReservoir neighborReservoir = SReservoir(0.f, -1, 0.f, 0.f);

    int neighborsCount = 7;
    int neighborsRange = 5; // Want to sample neighbors within [-neighborsRange, neighborsRange] offset

    for (int i = 0; i < neighborsCount; i++)
    {
        // Generate a random number from range [0, 2 * neighborsRange] then offset in negative direction 
        // by spatialNeighborCount to get range [-neighborsRange, neighborsRange]. 
        // Need to take care of out of bound case hence the max and min
        neighborOffset.x = int(rand(rngState) * float(neighborsRange * 2)) - neighborsRange;
        neighborOffset.y = int(rand(rngState) * float(neighborsRange * 2)) - neighborsRange;
        
        neighborIndex.x = max(0, min(giParams.widthScreen - 1, dispatchThreadId.x + neighborOffset.x));
		neighborIndex.y = max(0, min(giParams.heightScreen - 1, dispatchThreadId.y + neighborOffset.y));

        float4 wposObjectId1 = ptWposObjectIdOutput[neighborIndex];
        reservoirIndx = neighborIndex.x * giParams.widthScreen + neighborIndex.y;
        neighborReservoir = reservoirTemporalBuffer[curTemporalIndx][reservoirIndx];

        if (wposObjectId1.w < 0.f || neighborReservoir.lightSampler < 0)
            continue;

        albedoMetalness = ptAlbedoMetalnessOutput[dispatchThreadId.xy];
        emissiveRoughtness = ptEmissionRoughnessOutput[dispatchThreadId.xy];
        normalEncode = ptNormalOutput[dispatchThreadId.xy];
        
        MaterialProperties material1 = (MaterialProperties) 0;
        material1.baseColor = albedoMetalness.xyz;
        material1.metalness = albedoMetalness.w;
        material1.emissive = emissiveRoughtness.xyz;
        material1.roughness = emissiveRoughtness.w;

        float3 geometryNormal1;
        float3 shadingNormal1;
        decodeNormals(normalEncode, geometryNormal1, shadingNormal1);

        pixelCenter = float2(neighborIndex) + float2(0.5);
        inUV = pixelCenter/float2(LaunchDimensions);
        d = inUV * 2.0 - 1.0;

        target    = mul(giParams.projInverse , float4(d.x, d.y, 1, 1));
        direction = mul(giParams.viewInverse , float4(normalize(target.xyz), 0));
        rayDir = normalize(direction.xyz);
        float3 V1 = -rayDir;

        // Prepare data needed to evaluate the light
        light = lightsBuffer[neighborReservoir.lightSampler];
        getLightData(light, wposObjectId1.xyz, lightVector, lightDistance);
        L = normalize(lightVector);

        target_pdf = luminance(evalCombinedBRDF(shadingNormal1, L, V1, material1) * (getLightIntensityAtPoint(light, lightDistance) * neighborReservoir.finalWeight));

        reservoirNew.updateReservoir(rngState, neighborReservoir.lightSampler, target_pdf * neighborReservoir.samplesNumber);

        lightSamplesCount += neighborReservoir.samplesNumber;
    };

    reservoirNew.samplesNumber = lightSamplesCount;
    light = lightsBuffer[reservoirNew.lightSampler];
    getLightData(light, wposObjectId.xyz, lightVector, lightDistance);
    L = normalize(lightVector);

    target_pdf = luminance(evalCombinedBRDF(shadingNormal, L, V, material) * (getLightIntensityAtPoint(light, lightDistance) * neighborReservoir.finalWeight));
    reservoirNew.finalWeight = target_pdf * reservoirNew.samplesNumber  == 0.f ? 0.f :  reservoirNew.weightSum / (target_pdf * reservoirNew.samplesNumber);

    reservoirSpacialBuffer[reservoirIndx] = reservoirNew;
}