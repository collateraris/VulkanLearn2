#include "gi_pathtrace.h"
#include "brdf.h"
#include "network_config.h"
#include "nrc_common.h"

import NS_CooperativeVectorAutoDiff;
import NS_CooperativeVectorFunctions;
import NS_Utils;
import NS_Activation;
import NS_MLP;

[[vk::binding(0, 0)]]
StructuredBuffer<SVertex> vertices[] : register(t0);

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t1);

[[vk::binding(2, 0)]]
RaytracingAccelerationStructure topLevelAS : register(t2);

[[vk::binding(3, 0)]]
StructuredBuffer<SObjectData> objectBuffer : register(t3);

[[vk::binding(5, 0)]]
StructuredBuffer<uint> indices[] : register(t5);

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

#include "restir_lighting.h"
#include "restir_visibility.h"

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= giParams.widthScreen ||  dispatchThreadId.y >= giParams.heightScreen)
    {
        return;
    };

    float4 wposObjectId = ptWposObjectIdOutput[dispatchThreadId.xy];
    uint reservoirIndx = dispatchThreadId.y * giParams.widthScreen + dispatchThreadId.x;

    SReservoir reservoirSpacial = reservoirSpacialBuffer[reservoirIndx];
    SReservoirPT indirect = reservoirPTSpacialBuffer[reservoirIndx];

    if (wposObjectId.w < 0.f || !all(isfinite(wposObjectId.xyz)))
    {
        ptOutput[dispatchThreadId.xy] = float4(min(restirEnvironment(-restirViewDirection(dispatchThreadId.xy)),
            float3(65504.f)), 1.f);
        return;
    };

    float4 albedoMetalness = ptAlbedoMetalnessOutput[dispatchThreadId.xy];
    float4 emissiveRoughtness = ptEmissionRoughnessOutput[dispatchThreadId.xy];
    float4 normalEncode = ptNormalOutput[dispatchThreadId.xy];
    
    MaterialProperties material = restirMaterial(albedoMetalness, emissiveRoughtness);

    float3 geometryNormal;
    float3 shadingNormal;
    decodeNormals(normalEncode, geometryNormal, shadingNormal);

    float3 V = normalize(giParams.camPos.xyz - wposObjectId.xyz);
    var inputParams = nrcEncodeSurface(albedoMetalness, material.roughness, shadingNormal,
        wposObjectId.xyz, V, giParams.gridMin.xyz, giParams.gridMax.xyz);

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

    var outputParams = model.forward(inputParams, rtxns::mlp::ReLUAct<half, HIDDEN_NEURONS>(),
        rtxns::mlp::NoneAct<half, OUTPUT_NEURONS>());
    float3 prediction = float3(outputParams[0], outputParams[1], outputParams[2]);
    float3 indirectRadiance = nrcIndirectTarget(indirect);
    if (all(isfinite(prediction)))
    {
        // Warmup alone does not establish that the cache has converged on a
        // bright scene. Keep at least 75% PT so an undertrained cache cannot
        // erase the traced indirect lighting. Both estimate the same term.
        float cacheWeight = NRC_CACHE_BLEND_MAX * saturate((gConst.currentStep - 32.f) / 96.f);
        indirectRadiance = lerp(indirectRadiance, max(prediction, float3(0.f)), cacheWeight);
    }
    if (giParams.numRays == 0)
        indirectRadiance = float3(0.f);

    float3 radiance = material.emissive + indirectRadiance;
    restirOrientNormals(V, geometryNormal, shadingNormal);
    if (restirValidLight(reservoirSpacial) && isfinite(reservoirSpacial.finalWeight) &&
        reservoirSpacial.finalWeight > 0.f && restirLightVisible(reservoirSpacial, wposObjectId.xyz, geometryNormal))
    {
        radiance += restirEvaluateDirect(reservoirSpacial, wposObjectId.xyz, material,
            geometryNormal, shadingNormal, V) * reservoirSpacial.finalWeight;
    }
    ptOutput[dispatchThreadId.xy] = float4(clamp(restirFiniteRadiance(radiance), float3(0.f), float3(65504.f)), 1.f);
}
