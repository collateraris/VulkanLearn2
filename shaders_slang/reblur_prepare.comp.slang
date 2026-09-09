#include "reblur_material.h"

[[vk::binding(0, 0)]] Texture2D<float4> rawDiffuse;
[[vk::binding(1, 0)]] Texture2D<float4> rawSpecular;
[[vk::binding(2, 0)]] Texture2D<float4> worldPosition;
[[vk::binding(3, 0)]] Texture2D<float4> packedNormals;
[[vk::binding(4, 0)]] Texture2D<float4> baseColorMetalness;
[[vk::binding(5, 0)]] Texture2D<float4> emissionRoughness;
[[vk::binding(6, 0)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> outputMotion;
[[vk::binding(7, 0)]] [[vk::image_format("rgb10_a2")]] RWTexture2D<float4> outputNormalRoughness;
[[vk::binding(8, 0)]] [[vk::image_format("r32f")]] RWTexture2D<float> outputViewZ;
[[vk::binding(9, 0)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> outputDiffuse;
[[vk::binding(10, 0)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> outputSpecular;
[[vk::binding(11, 0)]] ConstantBuffer<ReblurConstants> params;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    uint2 pixel = thread.xy;
    if (any(pixel >= params.extent.xy)) return;
    float4 position = worldPosition.Load(int3(pixel, 0));
    // Static geometry has zero WORLD-space motion. NRD computes camera motion
    // from its unjittered current/previous matrices, including DLSS jitter.
    outputMotion[pixel] = float4(0.0f);
    float viewZ;
    if (!reblurSurface(position, params, viewZ))
    {
        outputViewZ[pixel] = params.hitDistanceAndRange.w * 2.0f;
        outputNormalRoughness[pixel] = NRD_FrontEnd_PackNormalAndRoughness(float3(0, 0, 1), 1.0f, 0.0f);
        outputDiffuse[pixel] = float4(0.0f);
        outputSpecular[pixel] = float4(0.0f);
        return;
    }
    float roughness = saturate(emissionRoughness.Load(int3(pixel, 0)).w);
    float3 N, diffuseFactor, specularFactor;
    reblurMaterial(packedNormals.Load(int3(pixel, 0)), baseColorMetalness.Load(int3(pixel, 0)),
        roughness, normalize(params.cameraPosition.xyz - position.xyz), N, diffuseFactor, specularFactor);
    float4 diffuse = rawDiffuse.Load(int3(pixel, 0));
    float4 specular = rawSpecular.Load(int3(pixel, 0));
    float diffuseHit = diffuse.w > 0.0f ? REBLUR_FrontEnd_GetNormHitDist(diffuse.w, viewZ, params.hitDistanceAndRange.xyz, 1.0f) : 0.0f;
    float specularHit = specular.w > 0.0f ? REBLUR_FrontEnd_GetNormHitDist(specular.w, viewZ, params.hitDistanceAndRange.xyz, roughness) : 0.0f;
    outputViewZ[pixel] = viewZ;
    outputNormalRoughness[pixel] = NRD_FrontEnd_PackNormalAndRoughness(N, roughness, 0.0f);
    outputDiffuse[pixel] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(diffuse.rgb / diffuseFactor, diffuseHit, true);
    outputSpecular[pixel] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(specular.rgb / specularFactor, specularHit, true);
}
