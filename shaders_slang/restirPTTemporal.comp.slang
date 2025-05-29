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

[[vk::binding(11, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTInitBuffer  : register(t11);

[[vk::binding(12, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTTemporalBuffer[]  : register(t12);

[[vk::binding(0, 1)]]
cbuffer MyContantBuffer : register(b0)
{
	SGlobalGIParams giParams;
};

[[vk::binding(0, 2)]]
RWTexture2D<float4> ptWposObjectIdOutput : register(u0);

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

    if (wposObjectId.w < 0.f)
    {
        reservoirPTTemporalBuffer[curTemporalIndx][reservoirIndx] = SReservoirPT(uint4(0, 0, 0, 0), float4(0., 0., 0., 0.), 0., 0, 0., 0.);
        return;
    };

    SReservoirPT reservoirInit = reservoirPTInitBuffer[reservoirIndx];

    uint prevTemporalIndx = giParams.frameCount % 2;

    float target_pdf = luminance(reservoirInit.radiance.xyz);

    uint2 LaunchIndex = dispatchThreadId.xy;
	uint2 LaunchDimensions = uint2(giParams.widthScreen, giParams.heightScreen);

    // Initialize random numbers generator
	RngStateType rngState = initRNG(LaunchIndex, LaunchDimensions, giParams.frameCount);

    SReservoirPT temporalReservoir = SReservoirPT(uint4(0, 0, 0, 0), float4(0., 0., 0., 0.), 0., 0, 1., 0.);
    temporalReservoir.updateReservoir(rngState, reservoirInit, target_pdf * reservoirInit.finalWeight * reservoirInit.samplesNumber);

    SReservoirPT prevTemporalReservoir = reservoirPTTemporalBuffer[prevTemporalIndx][reservoirIndx];
    if (prevTemporalReservoir.samplesNumber > 0)
    {
        target_pdf = luminance(prevTemporalReservoir.radiance.xyz);
    }
    else
    {
        target_pdf = 0;
    };

    prevTemporalReservoir.samplesNumber = min(5 * reservoirInit.samplesNumber, prevTemporalReservoir.samplesNumber);

    temporalReservoir.updateReservoir(rngState, prevTemporalReservoir, target_pdf * prevTemporalReservoir.finalWeight * prevTemporalReservoir.samplesNumber);

    temporalReservoir.samplesNumber = reservoirInit.samplesNumber + prevTemporalReservoir.samplesNumber;
    if (temporalReservoir.samplesNumber > 0)
    {
        target_pdf = luminance(temporalReservoir.radiance.xyz);
    }
    else
    {
        target_pdf = 0;
    };

    temporalReservoir.finalWeight = target_pdf * temporalReservoir.samplesNumber  == 0.f ? 0.f :  temporalReservoir.weightSum / (target_pdf * temporalReservoir.samplesNumber);

    reservoirPTTemporalBuffer[curTemporalIndx][reservoirIndx] = temporalReservoir;
    //reservoirPTTemporalBuffer[curTemporalIndx][reservoirIndx] = reservoirInit;
}