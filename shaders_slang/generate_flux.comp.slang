#include "gi_pathtrace.h"

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t1);

[[vk::binding(6, 0)]]
SamplerState      linearSampler : register(s6);

[[vk::binding(6, 0)]]
SamplerState      linearSampler : register(s6);

[[vk::binding(4, 0)]]
StructuredBuffer<SLight> lightsBuffer  : register(t4);

[[vk::binding(7, 0)]]
StructuredBuffer<SMaterialData> matBuffer  : register(t7);

[[vk::binding(0, 1)]]
cbuffer MyContantBuffer : register(b0)
{
	SGlobalGIParams giParams;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void generateFlux(uint3 DTid : SV_DispatchThreadID)
{
    const uint triIdx = DTid.x;
    if (triIdx >= giParams.lightsCount) return;

    SLight light = lightsBuffer[triIdx];
    uint type = uint(candidate.color_type.w); 

    [branch]
    if (type == EMISSION_LIGHT)
    {
        SMaterialData mat = matBuffer[uint(light.uv2_objectId_.z)];
        float3 averageEmissiveColor = float3(0.f);
        float2 uv, bary, texCoord;

        RngStateType rngState = initRNG(DTid.xy, DTid.xy, 1);

        for (int i = 0; i < 15; i++)
        {
            uv = float2(rand(rngState), rand(rngState));
            bary = UniformSampleTriangle(uv);
            texCoord = (1.0f - bary.x - bary.y) * light.uv0_uv1.xy + bary.x * light.uv0_uv1.zw + bary.y * light.uv2_objectId_.xy;
            averageEmissiveColor += texSet[mat.emissionTexIndex].SampleLevel(linearSampler, texCoord, 0).rgb;
        }

        averageEmissiveColor /= 15.f;

        float3 averageRadiance = averageEmissiveColor * mat.emissiveFactorMult_emissiveStrength.xyz;

        // Pre-compute the luminous flux emitted, which is what we use during sampling to set probabilities.
        // We assume diffuse emitters and integrate per side (hemisphere) => the scale factor is pi.
        // Triangle area in m^2 (the scene units are assumed to be in meters).
        light.direction_flux.w = luminance(averageRadiance) * light.normal_area.w * (float) M_PI;  // Flux in lumens.
    }
    else
    {
        light.direction_flux.w = 0.f;
    }

    lightsBuffer[triIdx] = light;
}