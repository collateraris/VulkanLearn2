#include "network_config.h"

import NS_Optimizers;


[[vk::binding(0, 2)]]
cbuffer MyContantBuffer2 : register(b0)
{
	TrainingConstantBufferEntry gConst;
};

[[vk::binding(0, 3)]]
RWStructuredBuffer<half> gMLPParams  : register(t0);
[[vk::binding(1, 3)]]
RWStructuredBuffer<float> gMLPParams32  : register(t1);
[[vk::binding(2, 3)]]
RWStructuredBuffer<half> gMLPParamsGradients  : register(t2);
[[vk::binding(3, 3)]]
RWStructuredBuffer<float> gMoments1  : register(t3);
[[vk::binding(4, 3)]]
RWStructuredBuffer<float> gMoments2  : register(t4);


[shader("compute")]
[numthreads(32, 1, 1)] 
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint i = dispatchThreadID.x;
    if (i >= gConst.maxParamSize)
        return;

    float gradient = (float)gMLPParamsGradients[i];
    gMLPParamsGradients[i] = half(0.0);

    if (isfinite(gradient))
    {
        float weightbias = gMLPParams32[i];

        optimizers::Adam optimizer = optimizers::Adam(gMoments1, gMoments2, gConst.learningRate, LOSS_SCALE);

        float adjustedWeightbias = optimizer.step(weightbias, i, gradient, gConst.currentStep);

        gMLPParams32[i] = adjustedWeightbias;
        gMLPParams[i] = (half) adjustedWeightbias;
    }    
}