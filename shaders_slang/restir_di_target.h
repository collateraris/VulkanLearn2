// DI initialization and reuse must normalize against the same visible target.
// Ray generation uses the shadow hit group; compute reuse uses inline queries.
float restirDirectTarget(SReservoir sample, float3 worldPos, MaterialProperties material,
    float3 geometryNormal, float3 shadingNormal, float3 V)
{
    float3 direct = restirEvaluateDirect(sample, worldPos, material, geometryNormal, shadingNormal, V);
    if (any(direct > 0.0f))
    {
#if defined(RESTIR_DI_RAYGEN)
        bool visible = restirTraceLight(sample, worldPos, geometryNormal);
#else
        bool visible = restirLightVisible(sample, worldPos, geometryNormal);
#endif
        if (!visible) direct = float3(0.0f);
    }
    // Preserve support for a light that becomes visible after temporal or
    // spatial reuse. This floor affects sampling only, never output radiance.
    return restirTarget(direct);
}
