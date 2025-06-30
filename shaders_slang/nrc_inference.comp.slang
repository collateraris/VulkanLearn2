#include "gi_pathtrace.h"
#include "brdf.h"
#include "network_config.h"

import NS_CooperativeVectorAutoDiff;
import NS_CooperativeVectorFunctions;
import NS_Utils;
import NS_Activation;
import NS_MLP;

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t1);

[[vk::binding(6, 0)]]
SamplerState      linearSampler : register(s6);

[[vk::binding(4, 0)]]
StructuredBuffer<SLight> lightsBuffer  : register(t4);

[[vk::binding(7, 0)]]
StructuredBuffer<SMaterialData> matBuffer  : register(t7); 

[[vk::binding(10, 0)]]
RWStructuredBuffer<SReservoir> reservoirSpacialBuffer  : register(t10);


[[vk::binding(13, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTSpacialBuffer  : register(t13);


[[vk::binding(0, 1)]]
cbuffer MyContantBuffer : register(b0)
{
	SGlobalGIParams giParams;
};

[[vk::binding(0, 2)]]
cbuffer MyContantBuffer2 : register(b0)
{
	TrainingConstantBufferEntry gConst;
};

[[vk::binding(0, 3)]]
ByteAddressBuffer gMLPParams : register(t0);

[[vk::binding(0, 4)]]
RWTexture2D<float4> ptAlbedoMetalnessOutput : register(u0);

[[vk::binding(1, 4)]]
RWTexture2D<float4> ptEmissionRoughnessOutput : register(u1);

[[vk::binding(2, 4)]]
RWTexture2D<float4> ptNormalOutput : register(u2);

[[vk::binding(3, 4)]]
RWTexture2D<float4> ptWposObjectIdOutput : register(u3);

[[vk::binding(4, 4)]]
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
    SReservoirPT indirect = reservoirPTSpacialBuffer[reservoirIndx];

    if (wposObjectId.w < 0.f)
    {
        ptOutput[dispatchThreadId.xy] = float4(0., 0., 0., 0.);
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

    float params[INPUT_FEATURES] = {material.metalness, material.roughness, normalEncode.x, normalEncode.y, normalEncode.z, normalEncode.w, wposObjectId.x, wposObjectId.y, wposObjectId.z};
    var inputParams = rtxns::EncodeFrequency<half, INPUT_FEATURES>(params);

    var model = rtxns::mlp::InferenceMLP<half, 
        NUM_HIDDEN_LAYERS, 
        INPUT_NEURONS, 
        HIDDEN_NEURONS, 
        OUTPUT_NEURONS, 
        CoopVecMatrixLayout::TrainingOptimal, 
        CoopVecComponentType::Float16>
        (gMLPParams, 
        rtxns::UnpackArray<NUM_TRANSITIONS_ALIGN4, NUM_TRANSITIONS>(gConst.weightOffsets), 
        rtxns::UnpackArray<NUM_TRANSITIONS_ALIGN4, NUM_TRANSITIONS>(gConst.biasOffsets));

    var outputParams = model.forward(inputParams, rtxns::mlp::ReLUAct<half, HIDDEN_NEURONS>(), rtxns::mlp::ExponentialAct<half, OUTPUT_NEURONS>());
    float4 indirectRadiance =  float4(outputParams[0], outputParams[1], outputParams[2], outputParams[3]);    

    if (reservoirSpacial.lightSampler < 0)
    {
        ptOutput[dispatchThreadId.xy] = indirectRadiance;
        return;
    };

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