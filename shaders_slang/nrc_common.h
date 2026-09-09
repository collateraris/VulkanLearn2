#pragma once

#include "network_config.h"

import NS_Utils;

// Train and query exactly the same primary-surface outgoing indirect radiance.
// Material and normal are essential: coincident surfaces need not reflect the
// same radiance. Map directional features to [-0.5, 0.5] so frequency encoding
// does not identify opposite directions at its two-period boundary.
rtxns::HCoopVec<INPUT_NEURONS> nrcEncodeSurface(
    float4 albedoMetalness, float roughness, float3 normal, float3 position,
    float3 viewDirection, float3 sceneMin, float3 sceneMax)
{
    float3 extent = max(abs(sceneMax - sceneMin), float3(1.f));
    float3 p = (position - 0.5f * (sceneMin + sceneMax)) / extent;
    p = 0.5f * p / (1.f + abs(p));
    float3 n = normal * 0.5f;
    float3 v = viewDirection * 0.5f;
    float params[INPUT_FEATURES] = {
        saturate(albedoMetalness.w), saturate(roughness),
        v.x, v.y, v.z, n.x, n.y, n.z,
        saturate(albedoMetalness.x), saturate(albedoMetalness.y), saturate(albedoMetalness.z),
        p.x, p.y, p.z
    };
    return rtxns::EncodeFrequency<half, INPUT_FEATURES>(params);
}

float3 nrcIndirectTarget(SReservoirPT reservoir)
{
    float3 radiance = reservoir.radiance.xyz * reservoir.finalWeight;
    return all(isfinite(radiance)) ? max(radiance, float3(0.f)) : float3(0.f);
}
