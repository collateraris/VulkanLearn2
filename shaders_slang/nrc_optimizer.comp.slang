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
RWStructuredBuffer<float> gMLPParamsGradients  : register(t2);
[[vk::binding(3, 3)]]
RWStructuredBuffer<float> gMoments1  : register(t3);
[[vk::binding(4, 3)]]
RWStructuredBuffer<float> gMoments2  : register(t4);
[[vk::binding(5, 3)]]
StructuredBuffer<uint> gGradientIndices : register(t5);


[shader("compute")]
[numthreads(32, 1, 1)] 
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint i = dispatchThreadID.x;
    if (i >= gConst.maxParamSize)
        return;

    uint gradientIndex = gGradientIndices[i];
    float gradient = 0.f;
    if (gradientIndex != 0xffffffffu)
    {
        uint stride = ((gConst.gradientBiasOffsets[NUM_HIDDEN_LAYERS / 4][NUM_HIDDEN_LAYERS % 4] + OUTPUT_NEURONS * 4 + 63) & ~63u) / 4;
        [ForceUnroll] for (uint bucket = 0; bucket < NRC_GRADIENT_BUCKET_COUNT; ++bucket)
        {
            uint index = gradientIndex + bucket * stride;
            gradient += gMLPParamsGradients[index] * exp2(float(bucket * NRC_GRADIENT_BUCKET_BITS));
            gMLPParamsGradients[index] = 0.f;
        }
    }

    // FP32 master parameters must start in the very same device layout as the
    // converted FP16 network, also after an optimizer reset.
    float weightbias = gConst.currentStep <= 1.f ? float(gMLPParams[i]) : gMLPParams32[i];
    if (!isfinite(weightbias))
    {
        weightbias = 0.f;
        gMoments1[i] = 0.f;
        gMoments2[i] = 0.f;
    }
    gMLPParams32[i] = weightbias;
    gMLPParams[i] = half(weightbias);

    if (isfinite(gradient))
    {
        optimizers::Adam optimizer = optimizers::Adam(gMoments1, gMoments2, gConst.learningRate, LOSS_SCALE);

        float adjustedWeightbias = optimizer.step(weightbias, i, gradient, max(gConst.currentStep, 1.f));

        if (isfinite(adjustedWeightbias))
        {
            adjustedWeightbias = clamp(adjustedWeightbias, -16.f, 16.f);
            gMLPParams32[i] = adjustedWeightbias;
            gMLPParams[i] = (half) adjustedWeightbias;
        }
        else
        {
            gMoments1[i] = 0.f;
            gMoments2[i] = 0.f;
        }
    }
}
