#include "gi_pathtrace.h"
#include "network_config.h"
#include "nrc_common.h"

import NS_CooperativeVectorAutoDiff;
import NS_CooperativeVectorFunctions;
import NS_Utils;
import NS_Activation;
import NS_MLP;
import NS_LinearOps;
import NS_Loss;
import NS_PCG32;

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

[[vk::binding(1, 3)]]
RWByteAddressBuffer gMLPParamsGradients  : register(t1);


[[vk::binding(0, 4)]]
RWTexture2D<float4> ptAlbedoMetalnessOutput : register(u0);

[[vk::binding(1, 4)]]
RWTexture2D<float4> ptEmissionRoughnessOutput : register(u1);

[[vk::binding(2, 4)]]
RWTexture2D<float4> ptNormalOutput : register(u2);

[[vk::binding(3, 4)]]
RWTexture2D<float4> ptWposObjectIdOutput : register(u3);

#include "nrc_training_mlp.h"

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint sampleIndex = dispatchThreadId.x;
    uint pixelCount = giParams.widthScreen * giParams.heightScreen;
    if (sampleIndex >= gConst.batchSize || pixelCount == 0 || giParams.numRays == 0)
        return;

    // One randomly selected pixel per stratum: bounded training cost, and the
    // number of gradient contributors matches the batch-size normalization.
    PCG32 rng = PCG32(gConst.seed, sampleIndex);
    uint reservoirIndx = min(uint((float(sampleIndex) + rng.nextFloat()) *
        (float(pixelCount) / float(gConst.batchSize))), pixelCount - 1);
    uint2 pixel = uint2(reservoirIndx % giParams.widthScreen, reservoirIndx / giParams.widthScreen);
    float4 wposObjectId = ptWposObjectIdOutput[pixel];
    if (wposObjectId.w < 0.f || !all(isfinite(wposObjectId.xyz)))
        return;

    float4 albedoMetalness = ptAlbedoMetalnessOutput[pixel];
    float roughness = ptEmissionRoughnessOutput[pixel].a;
    float3 geometryNormal, shadingNormal;
    decodeNormals(ptNormalOutput[pixel], geometryNormal, shadingNormal);

    SReservoirPT indirect = reservoirPTSpacialBuffer[reservoirIndx];
    if (!all(isfinite(indirect.radiance.xyz)) || !isfinite(indirect.finalWeight))
        return;
    float4 indirectRadiance = float4(nrcIndirectTarget(indirect), 0.f);

    float3 viewDirection = normalize(giParams.camPos.xyz - wposObjectId.xyz);
    var inputParams = nrcEncodeSurface(albedoMetalness, roughness, shadingNormal,
        wposObjectId.xyz, viewDirection, giParams.gridMin.xyz, giParams.gridMax.xyz);

    nrcTrainSample(inputParams, indirectRadiance.xyz);

}



