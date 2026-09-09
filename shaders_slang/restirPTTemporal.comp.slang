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

#include "restir_lighting.h"

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadId.xy;
    if (pixel.x >= giParams.widthScreen || pixel.y >= giParams.heightScreen) return;
    uint index = pixel.y * giParams.widthScreen + pixel.x;
    uint current = (giParams.frameCount + 1) % 2;
    // Prepare the current slot. The ray-generation reuse pass replays and
    // merges the previous seed at the current surface: camera jitter changes
    // the primary hit even while the non-jittered camera is stationary.
    reservoirPTTemporalBuffer[current][index] = reservoirPTInitBuffer[index];
}
