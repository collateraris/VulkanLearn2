#include "gi_pathtrace.h"
#include "network_config.h"

import NS_CooperativeVectorAutoDiff;
import NS_CooperativeVectorFunctions;
import NS_Utils;
import NS_Activation;
import NS_MLP;
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

[[vk::binding(1, 0)]]
RWByteAddressBuffer gMLPParamsGradients  : register(t1);


[[vk::binding(0, 2)]]
RWTexture2D<float4> ptAlbedoMetalnessOutput : register(u0);

[[vk::binding(1, 2)]]
RWTexture2D<float4> ptEmissionRoughnessOutput : register(u1);

[[vk::binding(2, 2)]]
RWTexture2D<float4> ptNormalOutput : register(u2);

[[vk::binding(3, 2)]]
RWTexture2D<float4> ptWposObjectIdOutput : register(u3);

[shader("compute")]
[numthreads(64, 1, 1)] 
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= giParams.widthScreen ||  dispatchThreadId.y >= giParams.heightScreen)
    {
        return;
    };

    PCG32 rng = PCG32(gConst.seed, dispatchThreadId.x);

    float metalness = ptAlbedoMetalnessOutput[dispatchThreadId.xy].a;
    float roughtness = ptEmissionRoughnessOutput[dispatchThreadId.xy].a;
    float4 normalEncode = ptNormalOutput[dispatchThreadId.xy];
    float3 wpos = ptWposObjectIdOutput[dispatchThreadId.xy].xyz;

    uint reservoirIndx = dispatchThreadId.y * (giParams.widthScreen + 1) + dispatchThreadId.x;
    SReservoirPT indirect = reservoirPTSpacialBuffer[reservoirIndx];
    float4 indirectRadiance = float4(indirect.radiance.xyz * indirect.finalWeight, 1.);


    //----------- Training step
    float params[INPUT_FEATURES] = {metalness, roughtness, normalEncode.x, normalEncode.y, normalEncode.z, normalEncode.w, wpos.x, wpos.y, wpos.z};
    var inputParams = rtxns::EncodeFrequency<half, INPUT_FEATURES>(params);

    var model = rtxns::mlp::TrainingMLP<half, 
        NUM_HIDDEN_LAYERS, INPUT_NEURONS, HIDDEN_NEURONS, OUTPUT_NEURONS, 
        CoopVecMatrixLayout::TrainingOptimal, CoopVecComponentType::Float16>(
        gMLPParams, 
        gMLPParamsGradients, 
        rtxns::UnpackArray<NUM_TRANSITIONS_ALIGN4, NUM_TRANSITIONS>(gConst.weightOffsets), 
        rtxns::UnpackArray<NUM_TRANSITIONS_ALIGN4, NUM_TRANSITIONS>(gConst.biasOffsets));

    var hiddenActivation = rtxns::mlp::ReLUAct<half, HIDDEN_NEURONS>();
    var finalActivation = rtxns::mlp::ExponentialAct<half, OUTPUT_NEURONS>(); 

    var outputParams = model.forward(inputParams, hiddenActivation, finalActivation);

    float4 predictedIndirect = { outputParams[0], outputParams[1], outputParams[2], outputParams[3] };

    float4 lossGradient = rtxns::mlp::L2Relative<float, 4>.deriv(indirectRadiance, predictedIndirect, float4(LOSS_SCALE / (gConst.batchSize * 4)) * COMPONENT_WEIGHTS);
   
    model.backward(inputParams, hiddenActivation, finalActivation, rtxns::HCoopVec<OUTPUT_NEURONS>(lossGradient[0], lossGradient[1], lossGradient[2], lossGradient[3]));
}



