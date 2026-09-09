// Requires the global vertices, indices, objectBuffer, textures and AS bindings.
// Include after restir_lighting.h.
bool restirLightVisible(SReservoir sample, float3 worldPos, float3 geometryNormal)
{
    if (!restirValidLight(sample)) return false;
    SLight light = restirLoadLight(sample);
    float3 lightVector;
    float distance;
    RayDesc ray;
    ray.Origin = offsetRay(worldPos, geometryNormal);
    getLightData(light, ray.Origin, lightVector, distance);
    if (!(length(lightVector) > 1e-6f)) return false;
    ray.Direction = normalize(lightVector);
    ray.TMin = 0.0f;
    ray.TMax = uint(light.color_type.w) == DIRECTIONAL_LIGHT ? FLT_MAX : max(0.0f, distance - max(1e-4f, distance * 1e-5f));
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(topLevelAS, RAY_FLAG_NONE, 0xFF, ray);
    while (query.Proceed())
    {
        if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
        {
            SObjectData object = objectBuffer[query.CandidateInstanceID()];
            SMaterialData material = matBuffer[object.materialIndex];
            uint primitive = query.CandidatePrimitiveIndex();
            float2 bary = query.CandidateTriangleBarycentrics();
            float2 uv0 = vertices[NonUniformResourceIndex(object.meshIndex)][indices[NonUniformResourceIndex(object.meshIndex)][primitive * 3]].normalYZ_texCoordUV.zw;
            float2 uv1 = vertices[NonUniformResourceIndex(object.meshIndex)][indices[NonUniformResourceIndex(object.meshIndex)][primitive * 3 + 1]].normalYZ_texCoordUV.zw;
            float2 uv2 = vertices[NonUniformResourceIndex(object.meshIndex)][indices[NonUniformResourceIndex(object.meshIndex)][primitive * 3 + 2]].normalYZ_texCoordUV.zw;
            float2 uv = (1.0f - bary.x - bary.y) * uv0 + bary.x * uv1 + bary.y * uv2;
            float opacity = 1.0f;
            if (material.diffuseTexIndex != 0xffffffffu)
                opacity = texSet[NonUniformResourceIndex(material.diffuseTexIndex)].SampleLevel(linearSampler, uv, 0).a;
            if (material.opacityTexIndex >= 0)
                opacity = texSet[NonUniformResourceIndex(material.opacityTexIndex)].SampleLevel(linearSampler, uv, 0).r;
            opacity *= material.baseColorFactor.w;
            if (opacity >= 0.5f) query.CommitNonOpaqueTriangleHit();
        }
    }
    return query.CommittedStatus() == COMMITTED_NOTHING;
}
