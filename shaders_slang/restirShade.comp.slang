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

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= giParams.widthScreen ||  dispatchThreadId.y >= giParams.heightScreen)
    {
        return;
    };

    float4 wposObjectId = ptWposObjectIdOutput[dispatchThreadId.xy];
    uint reservoirIndx = dispatchThreadId.y * (giParams.widthScreen + 1) + dispatchThreadId.x;

    SReservoir reservoirSpacial = reservoirSpacialBuffer[reservoirIndx];
    uint curTemporalIndx = (giParams.frameCount + 1) % 2;
    //reservoirTemporalBuffer[curTemporalIndx][reservoirIndx] = reservoirSpacial;
    //SReservoir reservoirSpacial = reservoirInitBuffer[reservoirIndx];
    //SReservoir reservoirSpacial = reservoirTemporalBuffer[curTemporalIndx][reservoirIndx];

    //SReservoirPT indirect = reservoirPTTemporalBuffer[curTemporalIndx][reservoirIndx];
    SReservoirPT indirect = reservoirPTSpacialBuffer[reservoirIndx];
    //SReservoirPT indirect = reservoirPTInitBuffer[reservoirIndx];
    float4 indirectRadiance = float4(indirect.radiance.xyz * indirect.finalWeight, 1.);

    if (wposObjectId.w < 0.f || reservoirSpacial.lightSampler < 0)
    {
        ptOutput[dispatchThreadId.xy] = indirectRadiance;
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
    const float2 pixelCenter = float2(LaunchIndex) + float2(0.5);
    const float2 inUV = pixelCenter/float2(LaunchDimensions);
    float2 d = inUV * 2.0 - 1.0;

    float4 origin    = mul(giParams.viewInverse , float4(0, 0, 0, 1));
    float4 target    = mul(giParams.projInverse , float4(d.x, d.y, 1, 1));
    float4 direction = mul(giParams.viewInverse , float4(normalize(target.xyz), 0));

	float3 rayDir = normalize(direction.xyz);
    float3 V = -rayDir;

    // Prepare data needed to evaluate the light
    float3 lightVector;
    float lightDistance;
    SLight light = lightsBuffer[reservoirSpacial.lightSampler];
    uint type = uint(light.color_type.w);
    if (type == EMISSION_LIGHT)
    {
        float2 bary = reservoirSpacial.bary__.xy;
        light.position_radius.xyz = (1.0f - bary.x - bary.y) * light.position_radius.xyz + bary.x * light.position1.xyz + bary.y * light.position2.xyz;
        float2 texCoord = (1.0f - bary.x - bary.y) * light.uv0_uv1.xy + bary.x * light.uv0_uv1.zw + bary.y * light.uv2_objectId_.xy;
        SMaterialData mat = matBuffer[uint(light.uv2_objectId_.z)];
        light.color_type.xyz = texSet[mat.emissionTexIndex].SampleLevel(linearSampler, texCoord, 0).rgb;
        light.color_type.xyz = float3(light.color_type.x * mat.emissiveFactorMult_emissiveStrength.x, light.color_type.y * mat.emissiveFactorMult_emissiveStrength.y, light.color_type.z * mat.emissiveFactorMult_emissiveStrength.z);
    }
    getLightData(light, wposObjectId.xyz, lightVector, lightDistance);
    float3 L = normalize(lightVector);

    float3 radiance = material.emissive + evalCombinedBRDF(shadingNormal, L, V, material) * (getLightIntensityAtPoint(light, lightDistance) * reservoirSpacial.finalWeight);

    ptOutput[dispatchThreadId.xy] = float4(radiance, 1.f) + indirectRadiance;
    //ptOutput[dispatchThreadId.xy] = indirectRadiance;
    //ptOutput[dispatchThreadId.xy] = float4(radiance, 1.f);
}