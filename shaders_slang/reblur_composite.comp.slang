#include "reblur_material.h"

[[vk::binding(0, 0)]] Texture2D<float4> filteredDiffuse;
[[vk::binding(1, 0)]] Texture2D<float4> filteredSpecular;
[[vk::binding(2, 0)]] Texture2D<float4> bypassRadiance;
[[vk::binding(3, 0)]] Texture2D<float4> rawDiffuse;
[[vk::binding(4, 0)]] Texture2D<float4> rawSpecular;
[[vk::binding(5, 0)]] Texture2D<float4> worldPosition;
[[vk::binding(6, 0)]] Texture2D<float4> packedNormals;
[[vk::binding(7, 0)]] Texture2D<float4> baseColorMetalness;
[[vk::binding(8, 0)]] Texture2D<float4> emissionRoughness;
[[vk::binding(9, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> outputColor;
[[vk::binding(10, 0)]] ConstantBuffer<ReblurConstants> params;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    uint2 pixel = thread.xy;
    if (any(pixel >= params.extent.xy)) return;
    float3 color = bypassRadiance.Load(int3(pixel, 0)).rgb;
    float4 position = worldPosition.Load(int3(pixel, 0));
    float viewZ;
    if (reblurSurface(position, params, viewZ))
    {
        float3 N, diffuseFactor, specularFactor;
        reblurMaterial(packedNormals.Load(int3(pixel, 0)), baseColorMetalness.Load(int3(pixel, 0)),
            emissionRoughness.Load(int3(pixel, 0)).w, normalize(params.cameraPosition.xyz - position.xyz),
            N, diffuseFactor, specularFactor);
        color += REBLUR_BackEnd_UnpackRadianceAndNormHitDist(filteredDiffuse.Load(int3(pixel, 0))).rgb * diffuseFactor;
        color += REBLUR_BackEnd_UnpackRadianceAndNormHitDist(filteredSpecular.Load(int3(pixel, 0))).rgb * specularFactor;
    }
    else
    {
        // NRD does not write outside its denoising range, including the sky.
        // Do not even load those undefined pixels. Retain the raw estimator.
        color += rawDiffuse.Load(int3(pixel, 0)).rgb + rawSpecular.Load(int3(pixel, 0)).rgb;
    }
    outputColor[pixel] = float4(all(isfinite(color)) ? clamp(color, float3(0.0f), float3(65504.0f)) : float3(0.0f), 1.0f);
}
