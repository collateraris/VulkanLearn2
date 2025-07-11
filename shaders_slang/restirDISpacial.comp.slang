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
    uint type = uint(light.color_type.w);
    if (type == EMISSION_LIGHT)
    {
        float2 bary = reservoirTemporal.bary__.xy;
        light.position_radius.xyz = (1.0f - bary.x - bary.y) * light.position_radius.xyz + bary.x * light.position1.xyz + bary.y * light.position2.xyz;
        float2 texCoord = (1.0f - bary.x - bary.y) * light.uv0_uv1.xy + bary.x * light.uv0_uv1.zw + bary.y * light.uv2_objectId_.xy;
        SMaterialData mat = matBuffer[uint(light.uv2_objectId_.z)];
        light.color_type.xyz = texSet[mat.emissionTexIndex].SampleLevel(linearSampler, texCoord, 0).rgb;
        light.color_type.xyz = float3(light.color_type.x * mat.emissiveFactorMult_emissiveStrength.x, light.color_type.y * mat.emissiveFactorMult_emissiveStrength.y, light.color_type.z * mat.emissiveFactorMult_emissiveStrength.z);
    }
    getLightData(light, wposObjectId.xyz, lightVector, lightDistance);
    float3 L = normalize(lightVector);

    float target_pdf = luminance(evalCombinedBRDF(shadingNormal, L, V, material) * getLightIntensityAtPoint(light, lightDistance));

    // Initialize random numbers generator
	RngStateType rngState = initRNG(LaunchIndex, LaunchDimensions, giParams.frameCount);

    SReservoir reservoirNew = SReservoir(0.f, -1, 0.f, 0.f, float4(0., 0., 0., 0.));
    reservoirNew.updateReservoir(rngState, reservoirTemporal, target_pdf * reservoirTemporal.finalWeight * reservoirTemporal.samplesNumber);

    uint lightSamplesCount = reservoirTemporal.samplesNumber;

    int2 neighborOffset;
    int2	neighborIndex;
    SReservoir neighborReservoir = SReservoir(0.f, -1, 0.f, 0.f, float4(0., 0., 0., 0.));

    int neighborsCount = 7;
    int neighborsRange = 2; // Want to sample neighbors within [-neighborsRange, neighborsRange] offset

    const float dist_threshold = 0.01f;
    const float angle_threshold = 25.f * M_PI / 180.f;

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
        reservoirIndx = neighborIndex.y * (giParams.widthScreen + 1) + neighborIndex.x;
        neighborReservoir = reservoirTemporalBuffer[curTemporalIndx][reservoirIndx];

        if (wposObjectId1.w < 0.f || neighborReservoir.lightSampler < 0)
            continue;

        albedoMetalness = ptAlbedoMetalnessOutput[neighborIndex];
        emissiveRoughtness = ptEmissionRoughnessOutput[neighborIndex];
        normalEncode = ptNormalOutput[neighborIndex];
        
        MaterialProperties material1 = (MaterialProperties) 0;
        material1.baseColor = albedoMetalness.xyz;
        material1.metalness = albedoMetalness.w;
        material1.emissive = emissiveRoughtness.xyz;
        material1.roughness = emissiveRoughtness.w;

        float3 geometryNormal1;
        float3 shadingNormal1;
        decodeNormals(normalEncode, geometryNormal1, shadingNormal1);

        // Geometric similarity
        float dist = dot(wposObjectId1.xyz - wposObjectId.xyz, wposObjectId1.xyz - wposObjectId.xyz);
        if (dist > dist_threshold ||
            (dot(shadingNormal1.xyz, shadingNormal.xyz)) < cos(angle_threshold)) {
                continue;
        }

        pixelCenter = float2(neighborIndex) + float2(0.5);
        inUV = pixelCenter/float2(LaunchDimensions);
        d = inUV * 2.0 - 1.0;

        target    = mul(giParams.projInverse , float4(d.x, d.y, 1, 1));
        direction = mul(giParams.viewInverse , float4(normalize(target.xyz), 0));
        rayDir = normalize(direction.xyz);
        float3 V1 = -rayDir;

        // Prepare data needed to evaluate the light
        light = lightsBuffer[neighborReservoir.lightSampler];
        uint type = uint(light.color_type.w);
        if (type == EMISSION_LIGHT)
        {
            float2 bary = neighborReservoir.bary__.xy;
            light.position_radius.xyz = (1.0f - bary.x - bary.y) * light.position_radius.xyz + bary.x * light.position1.xyz + bary.y * light.position2.xyz;
            float2 texCoord = (1.0f - bary.x - bary.y) * light.uv0_uv1.xy + bary.x * light.uv0_uv1.zw + bary.y * light.uv2_objectId_.xy;
            SMaterialData mat = matBuffer[uint(light.uv2_objectId_.z)];
            light.color_type.xyz = texSet[mat.emissionTexIndex].SampleLevel(linearSampler, texCoord, 0).rgb;
            light.color_type.xyz = float3(light.color_type.x * mat.emissiveFactorMult_emissiveStrength.x, light.color_type.y * mat.emissiveFactorMult_emissiveStrength.y, light.color_type.z * mat.emissiveFactorMult_emissiveStrength.z);
        }
        getLightData(light, wposObjectId1.xyz, lightVector, lightDistance);
        L = normalize(lightVector);

        target_pdf = luminance(evalCombinedBRDF(shadingNormal1, L, V1, material1) * getLightIntensityAtPoint(light, lightDistance) );

        reservoirNew.updateReservoir(rngState, neighborReservoir, target_pdf * neighborReservoir.finalWeight * neighborReservoir.samplesNumber);

        lightSamplesCount += neighborReservoir.samplesNumber;
    };

    reservoirNew.samplesNumber = lightSamplesCount;
    if (reservoirNew.lightSampler > -1)
    {
        light = lightsBuffer[reservoirNew.lightSampler];
        type = uint(light.color_type.w);
        if (type == EMISSION_LIGHT)
        {
            float2 bary = reservoirNew.bary__.xy;
            light.position_radius.xyz = (1.0f - bary.x - bary.y) * light.position_radius.xyz + bary.x * light.position1.xyz + bary.y * light.position2.xyz;
            float2 texCoord = (1.0f - bary.x - bary.y) * light.uv0_uv1.xy + bary.x * light.uv0_uv1.zw + bary.y * light.uv2_objectId_.xy;
            SMaterialData mat = matBuffer[uint(light.uv2_objectId_.z)];
            light.color_type.xyz = texSet[mat.emissionTexIndex].SampleLevel(linearSampler, texCoord, 0).rgb;
            light.color_type.xyz = float3(light.color_type.x * mat.emissiveFactorMult_emissiveStrength.x, light.color_type.y * mat.emissiveFactorMult_emissiveStrength.y, light.color_type.z * mat.emissiveFactorMult_emissiveStrength.z);
        }
        getLightData(light, wposObjectId.xyz, lightVector, lightDistance);
        L = normalize(lightVector);

        target_pdf = luminance(evalCombinedBRDF(shadingNormal, L, V, material) * getLightIntensityAtPoint(light, lightDistance) );
        reservoirNew.finalWeight = target_pdf * reservoirNew.samplesNumber  == 0.f ? 0.f :  reservoirNew.weightSum / (target_pdf * reservoirNew.samplesNumber);
    }
    else
    {
        reservoirNew.finalWeight = 0;
    }

    reservoirSpacialBuffer[reservoirIndx] = reservoirNew;
    //reservoirSpacialBuffer[reservoirIndx] = reservoirTemporal;
}