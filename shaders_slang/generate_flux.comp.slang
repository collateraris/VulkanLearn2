#include "gi_pathtrace.h"
#include "brdf.h"

struct SFluxData
{
    uint lightCount;
    uint pad0;
    uint pad1;
    uint pad2;
};

[[vk::binding(0, 0)]]
cbuffer MyContantBuffer : register(b0)
{
	SFluxData fluxParams;
};

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t1);

[[vk::binding(2, 0)]]
SamplerState      linearSampler : register(s6);


[[vk::binding(3, 0)]]
RWStructuredBuffer<SLight> lightsBuffer  : register(t4);

[[vk::binding(4, 0)]]
StructuredBuffer<SMaterialData> matBuffer  : register(t7);


[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint triIdx = DTid.x;
    if (triIdx >= fluxParams.lightCount) return;

    //SLight light = SLight(0);
///*
    SLight light = lightsBuffer[triIdx];
    
    uint type = uint(light.color_type.w); 

    [branch]
    if (type == EMISSION_LIGHT)
    {
        SMaterialData mat = matBuffer[uint(light.uv2_objectId_.z)];
        float3 averageEmissiveColor = float3(0.f);
        float2 uv, bary, texCoord;

        RngStateType rngState = initRNG(DTid.xx, 15, 1);

        averageEmissiveColor += texSet[mat.emissionTexIndex].SampleLevel(linearSampler, light.uv0_uv1.xy, 0).rgb;
        averageEmissiveColor += texSet[mat.emissionTexIndex].SampleLevel(linearSampler, light.uv0_uv1.zw, 0).rgb;
        averageEmissiveColor += texSet[mat.emissionTexIndex].SampleLevel(linearSampler, light.uv2_objectId_.xy, 0).rgb;

        [unroll]
        for (int i = 0; i < 6; i++)
        {
            uv = float2(rand(rngState), rand(rngState));
            bary = UniformSampleTriangle(uv);
            texCoord = (1.0f - bary.x - bary.y) * light.uv0_uv1.xy + bary.x * light.uv0_uv1.zw + bary.y * light.uv2_objectId_.xy;
            averageEmissiveColor += texSet[mat.emissionTexIndex].SampleLevel(linearSampler, texCoord, 0).rgb;
        }

        averageEmissiveColor /= 15.f;

        float3 averageRadiance = averageEmissiveColor * mat.emissiveFactorMult_emissiveStrength.xyz * mat.emissiveFactorMult_emissiveStrength.w;

        // Pre-compute the luminous flux emitted, which is what we use during sampling to set probabilities.
        // We assume diffuse emitters and integrate per side (hemisphere) => the scale factor is pi.
        // Triangle area in m^2 (the scene units are assumed to be in meters).
        light.direction_flux.w = luminance(averageRadiance) * light.normal_area.w * (float) M_PI;  // Flux in lumens.

    }
    else if (type == POINT_LIGHT)
    {
        light.direction_flux.w = luminance(light.color_type.xyz);
    }
    else if (type == DIRECTIONAL_LIGHT)
    {
        light.direction_flux.w = 100;
    }

    light.position_radius.w = sqrt(light.direction_flux.w / (4 * M_PI * 0.01));
//*/
    //light.direction_flux.w = triIdx;
    lightsBuffer[triIdx] = light;
}