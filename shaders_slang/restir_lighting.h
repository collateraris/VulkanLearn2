// Shared by ReSTIR DI, path replay and NRC composition. Include after bindings.
float3 restirFiniteRadiance(float3 value)
{
    return all(isfinite(value)) ? max(value, float3(0.0f)) : float3(0.0f);
}

float3 restirEnvironment(float3 direction)
{
    // Lighting is opt-in per scene. A disabled environment must not read a
    // descriptor or change the legacy black-miss estimator.
    if (giParams.environmentTextureIndex < 0 || !(giParams.environmentIntensity > 0.0f))
        return float3(0.0f);
    uint textureIndex = uint(giParams.environmentTextureIndex);
    uint width, height;
    texSet[textureIndex].GetDimensions(width, height);
    // The HDR loader flips rows, matching the existing spherical-map convention.
    float2 uv = float2(atan2(direction.z, direction.x) / (2.0f * M_PI),
        asin(clamp(direction.y, -1.0f, 1.0f)) / M_PI) + 0.5f;
    // Longitude repeats, latitude must not blend the opposite pole.
    float halfTexel = 0.5f / float(height);
    uv.y = clamp(uv.y, halfTexel, 1.0f - halfTexel);
    return restirFiniteRadiance(texSet[textureIndex].SampleLevel(linearSampler, uv, 0).xyz
        * giParams.environmentIntensity);
}

float restirTarget(float3 value)
{
    // Keep support for dark samples: a seed/light that is black at a source
    // pixel can carry light after a spatial shift. This floor is a sampling
    // target only; it never adds energy to the rendered radiance. Keep it tiny:
    // with many emissive triangles, a 1e-4 floor gives black candidates more
    // combined sampling mass than the actual lights and makes raw frames dark.
    return max(1e-8f, luminance(restirFiniteRadiance(value)));
}

float restirFinalWeight(float sum, uint count, float target)
{
    float result = count > 0 && target > 0.0f ? sum / (float(count) * target) : 0.0f;
    return isfinite(result) && result >= 0.0f ? result : 0.0f;
}

MaterialProperties restirMaterial(float4 albedo, float4 emission)
{
    MaterialProperties material = (MaterialProperties)0;
    material.baseColor = max(albedo.xyz, float3(0.0f));
    material.metalness = saturate(albedo.w);
    material.emissive = restirFiniteRadiance(emission.xyz);
    material.roughness = saturate(emission.w);
    return material;
}

void restirOrientNormals(float3 V, inout float3 geometryNormal, inout float3 shadingNormal)
{
    if (dot(geometryNormal, V) < 0.0f) geometryNormal = -geometryNormal;
    if (dot(geometryNormal, shadingNormal) < 0.0f) shadingNormal = -shadingNormal;
    if (dot(V, geometryNormal) > 0.0f && dot(V, shadingNormal) < 0.0f)
        shadingNormal = normalize(shadingNormal - dot(shadingNormal, V) * V + 1e-4f * V);
}

float3 restirViewDirection(uint2 pixel)
{
    float2 d = (float2(pixel) + 0.5f) / float2(giParams.widthScreen, giParams.heightScreen) * 2.0f - 1.0f;
    float4 target = mul(giParams.projInverse, float4(d, 1.0f, 1.0f));
    return -normalize(mul(giParams.viewInverse, float4(normalize(target.xyz), 0.0f)).xyz);
}

bool restirValidLight(SReservoir reservoir)
{
    return reservoir.lightSampler >= 0 && uint(reservoir.lightSampler) < giParams.lightsCount;
}

SLight restirLoadLight(SReservoir reservoir)
{
    SLight light = lightsBuffer[reservoir.lightSampler];
    if (uint(light.color_type.w) == EMISSION_LIGHT)
    {
        float2 bary = reservoir.bary__.xy;
        light.position_radius.xyz = (1.0f - bary.x - bary.y) * light.position_radius.xyz + bary.x * light.position1.xyz + bary.y * light.position2.xyz;
        float2 uv = (1.0f - bary.x - bary.y) * light.uv0_uv1.xy + bary.x * light.uv0_uv1.zw + bary.y * light.uv2_objectId_.xy;
        SMaterialData material = matBuffer[uint(light.uv2_objectId_.z)];
        float3 textureEmission = material.emissionTexIndex >= 0
            ? texSet[NonUniformResourceIndex(material.emissionTexIndex)].SampleLevel(linearSampler, uv, 0).xyz
            : float3(1.0f);
        // Preserve the scene's original rendered emission. Strength was used
        // for the flux proposal/radius, not the final DI or visible surface.
        // Applying it here changes the relative power of differently coloured
        // lamps (for example, subway tunnel lights versus ceiling lights).
        light.color_type.xyz = textureEmission * material.emissiveFactorMult_emissiveStrength.xyz;
        if (materialUsesAlphaCutout(material))
        {
            float opacity = 1.0f;
            if (material.diffuseTexIndex != 0xffffffffu)
                opacity = texSet[NonUniformResourceIndex(material.diffuseTexIndex)].SampleLevel(linearSampler, uv, 0).a;
            if (material.opacityTexIndex >= 0)
                opacity = texSet[NonUniformResourceIndex(material.opacityTexIndex)].SampleLevel(linearSampler, uv, 0).r;
            if (opacity * material.baseColorFactor.w < 0.5f) light.color_type.xyz = float3(0.0f);
        }
    }
    return light;
}

float3 restirLightIntensity(SLight light, float3 L, float distance)
{
    // Preserve the light units used by this project's scenes and reference
    // tracer: emissive triangles use softened point-light attenuation.
    // Switching only ReSTIR to area*cos/r^2 changes their authored brightness
    // by orders of magnitude, especially on finely tessellated emissive meshes.
    return getLightIntensityAtPoint(light, distance);
}

float3 restirEvaluateDirect(SReservoir reservoir, float3 worldPos, MaterialProperties material,
    float3 geometryNormal, float3 shadingNormal, float3 V)
{
    if (!restirValidLight(reservoir)) return float3(0.0f);
    SLight light = restirLoadLight(reservoir);
    float3 lightVector;
    float distance;
    getLightData(light, worldPos, lightVector, distance);
    float vectorLength = length(lightVector);
    if (!(vectorLength > 1e-6f)) return float3(0.0f);
    float3 L = lightVector / vectorLength;
    restirOrientNormals(V, geometryNormal, shadingNormal);
    if (dot(geometryNormal, L) <= 0.0f) return float3(0.0f);
    return restirFiniteRadiance(evalCombinedBRDF(shadingNormal, L, V, material) * restirLightIntensity(light, L, distance));
}


uint3 restirLightCell(float3 position)
{
    float3 gridExtent = max(giParams.gridMax.xyz - giParams.gridMin.xyz, float3(1e-6f));
    return uint3(clamp((position - giParams.gridMin.xyz) / gridExtent * GRID_SIZE,
        float3(0.0f), float3(GRID_SIZE - 1)));
}

uint restirLightCellIndex(float3 position)
{
    uint3 cell = restirLightCell(position);
    return cell.x * GRID_SIZE * GRID_SIZE + cell.y * GRID_SIZE + cell.z;
}

SReservoir restirSampleLight(inout RngStateType rng, float3 position,
    StructuredBuffer<SAliasTable> aliasEntries, StructuredBuffer<SCell> lightCells)
{
    SReservoir sample = SReservoir(0.0f, -1, 0, 0.0f, float4(0.0f));
    if (giParams.lightsCount == 0) return sample;
    // The original soft point proxies have a local grid domain. Sampling all
    // triangles globally adds distant light tails and changes scene lighting.
    uint cellIndex = restirLightCellIndex(position);
    SCell cell = lightCells[cellIndex];
    if (cell.startIndex < 0 || cell.numLights == 0 || !(cell.weightsSum > 0.0f)
        || !isfinite(cell.weightsSum)) return sample;
    uint column = min(uint(rand(rng) * cell.numLights), cell.numLights - 1);
    sample.lightSampler = int(aliasEntries[uint(cell.startIndex) + column].sample(rng));
    if (!restirValidLight(sample)) return SReservoir(0.0f, -1, 0, 0.0f, float4(0.0f));
    float flux = lightsBuffer[sample.lightSampler].direction_flux.w;
    if (!(flux > 0.0f) || !isfinite(flux)) return SReservoir(0.0f, -1, 0, 0.0f, float4(0.0f));
    sample.finalWeight = cell.weightsSum / flux;
    if (!isfinite(sample.finalWeight)) return SReservoir(0.0f, -1, 0, 0.0f, float4(0.0f));
    if (uint(lightsBuffer[sample.lightSampler].color_type.w) == EMISSION_LIGHT)
        sample.bary__.xy = UniformSampleTriangle(float2(rand(rng), rand(rng)));
    // DI reuse must stay within the same proposal support. Store the cell in
    // the unused component so temporal reuse can reject a cell transition.
    sample.bary__.z = float(cellIndex);
    sample.samplesNumber = 1;
    return sample;
}

float restirIndirectProxyPower(SReservoir sample)
{
    // The original PT gave secondary soft point proxies 1/64 of their full
    // power. Preserve that authored lighting balance explicitly, independently
    // of the number of RIS candidates and the reservoir's inverse PDF.
    const float indirectProxyPower = 1.0f / 64.0f;
    if (!restirValidLight(sample)) return 0.0f;
    SLight light = lightsBuffer[sample.lightSampler];
    // The sun is not a soft point proxy. New outdoor lighting can use its full
    // bounce contribution, while existing scenes keep their authored 1/64 scale.
    if (uint(light.color_type.w) == DIRECTIONAL_LIGHT)
        return giParams.indirectSunScale;
    float strength = uint(light.color_type.w) == EMISSION_LIGHT
        ? matBuffer[uint(light.uv2_objectId_.z)].emissiveFactorMult_emissiveStrength.w : 1.0f;
    return indirectProxyPower * strength;
}
