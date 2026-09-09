// Include after restir_lighting.h and the scene/output resource declarations.
// These are additional NRD inputs. The existing unfiltered total is composed
// independently, so toggling the denoiser never changes the light estimator.
struct RestirDenoiserConstants
{
    uint enabled;
};
[[vk::push_constant]]
ConstantBuffer<RestirDenoiserConstants> denoiserConstants;

void restirDenoiserDirectLobes(SReservoir reservoir, float3 worldPos,
    MaterialProperties material, float3 geometryNormal, float3 shadingNormal,
    float3 V, out float3 diffuse, out float3 specular)
{
    diffuse = float3(0.0f);
    specular = float3(0.0f);
    if (!restirValidLight(reservoir)) return;
    SLight light = restirLoadLight(reservoir);
    float3 lightVector;
    float distance;
    getLightData(light, worldPos, lightVector, distance);
    float vectorLength = length(lightVector);
    if (!(vectorLength > 1e-6f)) return;
    float3 L = lightVector / vectorLength;
    restirOrientNormals(V, geometryNormal, shadingNormal);
    if (dot(geometryNormal, L) <= 0.0f) return;
    BrdfData data = prepareBRDFData(shadingNormal, L, V, material);
    if (data.Vbackfacing || data.Lbackfacing) return;
    diffuse = evalDiffuse(data);
#if COMBINE_BRDFS_WITH_FRESNEL
    diffuse *= float3(1.0f) - data.F;
#endif
    specular = evalSpecular(data);
    float3 intensity = restirLightIntensity(light, L, distance);
    diffuse = restirFiniteRadiance(diffuse * intensity * reservoir.finalWeight);
    specular = restirFiniteRadiance(specular * intensity * reservoir.finalWeight);
}

float restirDenoiserGuideDistance(uint2 pixel, int lobe, float3 worldPos,
    MaterialProperties material, float3 geometryNormal, float3 shadingNormal, float3 V)
{
    // A separate random stream samples a geometric guide only. It does not
    // consume a reservoir RNG, estimate additional light, or change any PDF.
    RngStateType rng = initRNG(pixel, uint2(giParams.widthScreen, giParams.heightScreen), giParams.frameCount);
    rng.z ^= 0x243f6a88u ^ uint(lobe);
    float3 direction, unusedWeight;
    if (!evalIndirectCombinedBRDF(float2(rand(rng), rand(rng)), shadingNormal,
        geometryNormal, V, material, lobe, direction, unusedWeight)) return 0.0f;
    if (!all(isfinite(direction))) return 0.0f;

    RayDesc ray;
    ray.Origin = offsetRay(worldPos, geometryNormal);
    ray.Direction = direction;
    ray.TMin = 0.0f;
    ray.TMax = FLT_MAX;
    // Find the closest accepted surface, not the first traversal candidate.
    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(topLevelAS, RAY_FLAG_NONE, 0xFF, ray);
    while (query.Proceed())
    {
        if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
        {
            SObjectData object = objectBuffer[query.CandidateInstanceID()];
            SMaterialData hitMaterial = matBuffer[object.materialIndex];
            if (!materialUsesAlphaCutout(hitMaterial))
            {
                query.CommitNonOpaqueTriangleHit();
                continue;
            }
            uint primitive = query.CandidatePrimitiveIndex();
            float2 bary = query.CandidateTriangleBarycentrics();
            float2 uv0 = vertices[NonUniformResourceIndex(object.meshIndex)][indices[NonUniformResourceIndex(object.meshIndex)][primitive * 3]].normalYZ_texCoordUV.zw;
            float2 uv1 = vertices[NonUniformResourceIndex(object.meshIndex)][indices[NonUniformResourceIndex(object.meshIndex)][primitive * 3 + 1]].normalYZ_texCoordUV.zw;
            float2 uv2 = vertices[NonUniformResourceIndex(object.meshIndex)][indices[NonUniformResourceIndex(object.meshIndex)][primitive * 3 + 2]].normalYZ_texCoordUV.zw;
            float2 uv = (1.0f - bary.x - bary.y) * uv0 + bary.x * uv1 + bary.y * uv2;
            float opacity = 1.0f;
            if (hitMaterial.diffuseTexIndex != 0xffffffffu)
                opacity = texSet[NonUniformResourceIndex(hitMaterial.diffuseTexIndex)].SampleLevel(linearSampler, uv, 0).a;
            if (hitMaterial.opacityTexIndex >= 0)
                opacity = texSet[NonUniformResourceIndex(hitMaterial.opacityTexIndex)].SampleLevel(linearSampler, uv, 0).r;
            opacity *= hitMaterial.baseColorFactor.w;
            if (opacity >= 0.5f) query.CommitNonOpaqueTriangleHit();
        }
    }
    if (query.CommittedStatus() == COMMITTED_NOTHING) return FLT_MAX;
    float distance = length(ray.Origin + ray.Direction * query.CommittedRayT() - worldPos);
    return isfinite(distance) ? max(0.0f, distance) : 0.0f;
}

void restirWriteDenoiserMiss(uint2 pixel, float3 environment)
{
    denoiserDiffuseOutput[pixel] = float4(0.0f);
    denoiserSpecularOutput[pixel] = float4(0.0f);
    denoiserBypassOutput[pixel] = float4(restirFiniteRadiance(environment), 1.0f);
}

void restirWriteDenoiserSignals(uint2 pixel, float3 worldPos, MaterialProperties material,
    float3 geometryNormal, float3 shadingNormal, float3 V, SReservoir direct,
    bool directVisible, SReservoirPT indirect, float tracedWeight, float3 cachedRadiance)
{
    float3 diffuse = float3(0.0f);
    float3 specular = float3(0.0f);
    if (directVisible)
        restirDenoiserDirectLobes(direct, worldPos, material, geometryNormal, shadingNormal, V, diffuse, specular);

    float diffuseHit = 0.0f;
    float specularHit = 0.0f;
    if (giParams.numRays > 0)
    {
        float3 tracedRadiance = restirFiniteRadiance(indirect.radiance.xyz * indirect.finalWeight) * tracedWeight;
        float hitDistance = isfinite(indirect.radiance.w) ? max(0.0f, indirect.radiance.w) : 0.0f;
        // The selected path contributes entirely to its sampled FIRST lobe.
        // Its radiance already includes selection probability and reservoir W;
        // its hit distance must never be multiplied by either weight.
        if (indirect.pad0 == 1.0f)
        {
            specular += tracedRadiance;
            specularHit = hitDistance;
        }
        else
        {
            diffuse += tracedRadiance;
            diffuseHit = hitDistance;
        }
    }

    // RIS selects PT paths by radiance, so even a BRDF-generated selected path
    // no longer has the hit-distance distribution NRD expects. Use independent
    // in-lobe geometry for every active signal instead of passing that biased
    // distance (or the distance to a DI-selected lamp) directly to NRD.
    // A completely skipped lobe keeps zero for NRD's hit-distance reconstruction.
    if (denoiserConstants.enabled != 0)
    {
        if (diffuseHit > 0.0f || any(diffuse > 0.0f))
            diffuseHit = restirDenoiserGuideDistance(pixel, DIFFUSE_TYPE, worldPos,
                material, geometryNormal, shadingNormal, V);
        if (specularHit > 0.0f || any(specular > 0.0f))
            specularHit = restirDenoiserGuideDistance(pixel, SPECULAR_TYPE, worldPos,
                material, geometryNormal, shadingNormal, V);
    }

    denoiserDiffuseOutput[pixel] = float4(restirFiniteRadiance(diffuse), diffuseHit);
    denoiserSpecularOutput[pixel] = float4(restirFiniteRadiance(specular), specularHit);
    // The current NRC predicts aggregate RGB, not separate BRDF integrals.
    // Keep that cache estimate and visible emission outside NRD instead of
    // inventing a material-ratio split. Only its stochastic PT remainder is
    // denoised, so the original NRC blend is retained in the complete output.
    denoiserBypassOutput[pixel] = float4(restirFiniteRadiance(material.emissive + cachedRadiance), 1.0f);
}
