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
RWTexture2D<float4> ptWposObjectIdOutput : register(u0);

[shader("raygeneration")]
void main() 
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    float4 wposObjectId = ptWposObjectIdOutput[LaunchIndex];
    uint curTemporalIndx = (giParams.frameCount + 1) % 2;
    uint reservoirIndx = LaunchIndex.y * (LaunchDimensions.x + 1) + LaunchIndex.x;

    SReservoirPT reservoirTemporal = reservoirPTTemporalBuffer[curTemporalIndx][reservoirIndx];

    if (wposObjectId.w < 0.f || reservoirTemporal.samplesNumber == 0)
    {
        reservoirPTSpacialBuffer[reservoirIndx] = reservoirTemporal;
        return;
    };

    float target_pdf = luminance(reservoirTemporal.radiance.xyz);

    // Initialize random numbers generator
	RngStateType rngState = initRNG(LaunchIndex, LaunchDimensions, giParams.frameCount);

    SReservoirPT reservoirNew = SReservoirPT(uint4(0, 0, 0, 0), float4(0., 0., 0., 0.), 0., 0, 1., 0.);
    reservoirNew.updateReservoir(rngState, reservoirTemporal, target_pdf * reservoirTemporal.finalWeight * reservoirTemporal.samplesNumber);

    uint lightSamplesCount = reservoirTemporal.samplesNumber;

    int2 neighborOffset;
    int2	neighborIndex;
    SReservoirPT neighborReservoir = SReservoirPT(uint4(0, 0, 0, 0), float4(0., 0., 0., 0.), 0., 0, 1., 0.);

    int neighborsCount = 7;
    int neighborsRange = 5; // Want to sample neighbors within [-neighborsRange, neighborsRange] offset

    const float dist_threshold = 0.01f;
    const float angle_threshold = 25.f * M_PI / 180.f;

    for (int i = 0; i < neighborsCount; i++)
    {
        // Generate a random number from range [0, 2 * neighborsRange] then offset in negative direction 
        // by spatialNeighborCount to get range [-neighborsRange, neighborsRange]. 
        // Need to take care of out of bound case hence the max and min
        neighborOffset.x = int(rand(rngState) * float(neighborsRange * 2)) - neighborsRange;
        neighborOffset.y = int(rand(rngState) * float(neighborsRange * 2)) - neighborsRange;
        
        neighborIndex.x = max(0, min(LaunchDimensions.x - 1, LaunchIndex.x + neighborOffset.x));
		neighborIndex.y = max(0, min(LaunchDimensions.y - 1, LaunchIndex.y + neighborOffset.y));

        float4 wposObjectId1 = ptWposObjectIdOutput[neighborIndex];
        reservoirIndx = neighborIndex.y * (LaunchDimensions.x + 1) + neighborIndex.x;
        neighborReservoir = reservoirPTTemporalBuffer[curTemporalIndx][reservoirIndx];

        if (wposObjectId1.w < 0.f || neighborReservoir.samplesNumber == 0)
            continue;

        // Geometric similarity
        float dist = dot(wposObjectId1.xyz - wposObjectId.xyz, wposObjectId1.xyz - wposObjectId.xyz);
        if (dist > dist_threshold) {
                continue;
        }

        target_pdf = luminance(neighborReservoir.radiance.xyz);

        reservoirNew.updateReservoir(rngState, neighborReservoir, target_pdf * neighborReservoir.finalWeight * neighborReservoir.samplesNumber);

        lightSamplesCount += neighborReservoir.samplesNumber;
    };

    reservoirNew.samplesNumber = lightSamplesCount;
    target_pdf = luminance(reservoirNew.radiance.xyz);
    reservoirNew.finalWeight = target_pdf * reservoirNew.samplesNumber  == 0.f ? 0.f :  reservoirNew.weightSum / (target_pdf * reservoirNew.samplesNumber);

    reservoirPTSpacialBuffer[reservoirIndx] = reservoirNew;
    //reservoirPTSpacialBuffer[reservoirIndx] = reservoirTemporal;
}