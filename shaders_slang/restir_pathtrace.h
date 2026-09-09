#include "restir_lighting.h"

IndirectGbufferRayPayload restirTraceRay(float3 origin, float3 direction)
{
    IndirectGbufferRayPayload payload = (IndirectGbufferRayPayload)0;
    payload.position_objectID.w = -1.0f;
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.0f;
    ray.TMax = FLT_MAX;
    TraceRay(topLevelAS, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
    return payload;
}

bool restirTraceLight(SReservoir sample, float3 worldPos, float3 geometryNormal)
{
    if (!restirValidLight(sample)) return false;
    SLight light = restirLoadLight(sample);
    float3 lightVector;
    float distance;
    float3 origin = offsetRay(worldPos, geometryNormal);
    // Recompute the segment from the offset origin, and stop before the light.
    getLightData(light, origin, lightVector, distance);
    if (!(length(lightVector) > 1e-6f)) return false;
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = normalize(lightVector);
    ray.TMin = 0.0f;
    ray.TMax = uint(light.color_type.w) == DIRECTIONAL_LIGHT ? FLT_MAX : max(0.0f, distance - max(1e-4f, distance * 1e-5f));
    ShadowHitInfo payload;
    payload.hasHit = true;
    TraceRay(topLevelAS, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
        0xFF, 1, 0, 1, ray, payload);
    return !payload.hasHit;
}

float restirSpecularProbability(MaterialProperties material, float3 V, float3 normal)
{
    float f0 = luminance(baseColorToSpecularF0(material.baseColor, material.metalness));
    float fresnel = saturate(luminance(evalFresnel(f0, shadowedF90(f0), max(0.0f, dot(V, normal)))));
    float diffuse = luminance(baseColorToDiffuseReflectance(material.baseColor, material.metalness)) * (1.0f - fresnel);
    return clamp(fresnel / max(1e-4f, fresnel + diffuse), 0.1f, 0.9f);
}

bool restirSampleDirection(inout RngStateType rng, MaterialProperties material,
    float3 V, float3 geometryNormal, float3 shadingNormal, inout float3 throughput,
    out float3 direction, out float firstLobe)
{
    direction = float3(0.0f);
    firstLobe = 0.0f;
    int type = SPECULAR_TYPE;
    float probability = 1.0f;
    if (!(material.metalness == 1.0f && material.roughness == 0.0f))
    {
        probability = restirSpecularProbability(material, V, shadingNormal);
        if (rand(rng) >= probability)
        {
            type = DIFFUSE_TYPE;
            probability = 1.0f - probability;
        }
    }
    firstLobe = type == SPECULAR_TYPE ? 1.0f : 0.0f;
    float3 weight;
    if (!evalIndirectCombinedBRDF(float2(rand(rng), rand(rng)), shadingNormal,
        geometryNormal, V, material, type, direction, weight)) return false;
    throughput *= weight / probability;
    return all(isfinite(throughput)) && all(isfinite(direction)) && any(throughput > 0.0f);
}

// Reservoirs store a primary-sample-space seed. Initialization and spatial
// shifting must execute precisely the same estimator/random-number sequence.
// Replaying the seed at the receiving pixel is the identity map in this space
// (Jacobian = 1); the BRDF/PDF factors are already included in throughput.
// See Lin et al., Generalized Resampled Importance Sampling (2022), and
// Sawhney et al., Decorrelating ReSTIR Samplers via MCMC Mutations (2024), PSS.
float3 restirTraceIndirect(uint2 pixel, RngStateType rng, out float firstHitDistance, out float firstLobe)
{
    firstHitDistance = 0.0f;
    firstLobe = 0.0f;
    float4 position = ptWposObjectIdOutput[pixel];
    if (position.w < 0.0f || giParams.numRays == 0) return float3(0.0f);
    MaterialProperties material = restirMaterial(ptAlbedoMetalnessOutput[pixel], ptEmissionRoughnessOutput[pixel]);
    float3 geometryNormal, shadingNormal;
    decodeNormals(ptNormalOutput[pixel], geometryNormal, shadingNormal);
    float3 V = restirViewDirection(pixel);
    restirOrientNormals(V, geometryNormal, shadingNormal);
    float3 throughput = float3(1.0f);
    float3 direction;
    if (!restirSampleDirection(rng, material, V, geometryNormal, shadingNormal, throughput, direction, firstLobe))
        return float3(0.0f);
    float3 origin = offsetRay(position.xyz, geometryNormal);
    float3 radiance = float3(0.0f);

    [loop]
    for (uint bounce = 1; bounce <= giParams.numRays; ++bounce)
    {
        IndirectGbufferRayPayload payload = restirTraceRay(origin, direction);
        if (bounce == 1)
        {
            // NRD needs the real first segment after the primary surface, with
            // neither camera distance nor any reservoir/PDF scaling included.
            firstHitDistance = payload.hasHit()
                ? length(payload.position_objectID.xyz - position.xyz) : FLT_MAX;
        }
        if (!payload.hasHit())
        {
            // BRDF-sampled escape to the environment, including its PDF in
            // throughput. No unoccluded ambient term or duplicate sky NEE.
            if (giParams.environmentIntensity > 0.0f)
                radiance += throughput * restirEnvironment(direction);
            break;
        }
        material = restirMaterial(payload.albedo_metalness, payload.emission_roughness);
        decodeNormals(payload.normal_, geometryNormal, shadingNormal);
        V = -direction;
        restirOrientNormals(V, geometryNormal, shadingNormal);

        // Keep actual surface emission in addition to the scene's legacy
        // per-triangle soft point lights. Those proxies do not sample the
        // emissive-area integral, so they cannot replace BSDF emitter hits.
        radiance += throughput * material.emissive;

        SReservoir light = restirSampleLight(rng, payload.position_objectID.xyz, aliasTable, cellGridData);
        float3 direct = restirEvaluateDirect(light, payload.position_objectID.xyz, material, geometryNormal, shadingNormal, V);
        if (any(direct > 0.0f) && restirTraceLight(light, payload.position_objectID.xyz, geometryNormal))
            radiance += throughput * direct * light.finalWeight * restirIndirectProxyPower(light);

        if (bounce == giParams.numRays) break;
        if (bounce > 3)
        {
            float survival = clamp(luminance(throughput), 0.0f, 0.95f);
            if (!(survival > 0.0f) || rand(rng) >= survival) break;
            throughput /= survival;
        }
        float bounceLobe;
        if (!restirSampleDirection(rng, material, V, geometryNormal, shadingNormal, throughput, direction, bounceLobe)) break;
        origin = offsetRay(payload.position_objectID.xyz, geometryNormal);
    }
    return restirFiniteRadiance(radiance);
}

void restirReplayIndirect(uint2 pixel, inout SReservoirPT reservoir)
{
    float firstHitDistance, firstLobe;
    float3 radiance = restirTraceIndirect(pixel, reservoir.randomSeed, firstHitDistance, firstLobe);
    reservoir.radiance = float4(radiance, firstHitDistance);
    reservoir.pad0 = firstLobe;
}
