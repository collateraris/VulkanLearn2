#include "gi_pathtrace.h"
#include "../third_party/NRD/Shaders/NRD.hlsli"

struct ReblurConstants
{
    float4x4 worldToView;
    float4 cameraPosition;
    uint4 extent;
    float4 hitDistanceAndRange;
};

// Use the primary BRDF's oriented shading normal and the same material factors
// on both sides of NRD. Never remodulate with its quantized history roughness.
void reblurMaterial(float4 packedNormals, float4 baseColorMetalness,
    float roughness, float3 V, out float3 N, out float3 diffuseFactor, out float3 specularFactor)
{
    float3 Ng;
    decodeNormals(packedNormals, Ng, N);
    if (dot(Ng, V) < 0.0f) Ng = -Ng;
    if (dot(Ng, N) < 0.0f) N = -N;
    if (dot(V, Ng) > 0.0f && dot(V, N) < 0.0f)
        N = normalize(N - dot(N, V) * V + 1e-4f * V);
    float3 baseColor = max(baseColorMetalness.rgb, float3(0.0f));
    float metalness = saturate(baseColorMetalness.a);
    NRD_MaterialFactors(N, V, baseColor * (1.0f - metalness),
        lerp(float3(0.04f), baseColor, metalness), saturate(roughness), diffuseFactor, specularFactor);
}

bool reblurSurface(float4 position, ReblurConstants constants, out float viewZ)
{
    viewZ = -mul(constants.worldToView, float4(position.xyz, 1.0f)).z;
    return all(isfinite(position)) && position.w >= 0.0f && isfinite(viewZ)
        && viewZ > 0.0f && viewZ < constants.hitDistanceAndRange.w;
}
