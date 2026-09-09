#include "gi_pathtrace.h"

[[vk::binding(0, 0)]] Texture2D<float4> spatialRadiance;
[[vk::binding(1, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> historyColorOutput;
[[vk::binding(2, 0)]] Texture2D<float4> worldPosition;
[[vk::binding(3, 0)]] Texture2D<float4> packedNormals;
[[vk::binding(4, 0)]] Texture2D<float4> albedoMetalness;
[[vk::binding(5, 0)]] Texture2D<float4> emissionRoughness;
[[vk::binding(6, 0)]] Texture2D<float4> previousHistoryColor;
[[vk::binding(7, 0)]] Texture2D<float4> previousHistoryPosition;
[[vk::binding(8, 0)]] Texture2D<float4> previousHistoryNormalRoughness;
[[vk::binding(9, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> historyPositionOutput;
[[vk::binding(10, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> historyNormalRoughnessOutput;
[[vk::binding(11, 0)]] [[vk::image_format("rgba32f")]] RWTexture2D<float4> displayOutput;

struct TemporalConstants
{
    float4x4 previousViewProjection;
    float4 previousCameraPosition;
    float4 currentCameraPosition;
    uint4 frameInfo; // width, height, historyValid, cameraStill
    float4 options; // accumulationEnabled, reserved, reserved, reserved
};
[[vk::binding(0, 1)]] ConstantBuffer<TemporalConstants> params;

bool temporalInBounds(int2 pixel)
{
    return all(pixel >= int2(0)) && pixel.x < int(params.frameInfo.x) && pixel.y < int(params.frameInfo.y);
}

float temporalLuminance(float3 radiance)
{
    return dot(radiance, float3(0.2126f, 0.7152f, 0.0722f));
}

float temporalFootprint(int2 pixel, float4 position, float precisionFloor)
{
    float2 footprint = float2(FLT_MAX);
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        int direction = (i & 1) == 0 ? -1 : 1;
        int2 neighborPixel = pixel + (i < 2 ? int2(direction, 0) : int2(0, direction));
        if (!temporalInBounds(neighborPixel)) continue;
        float4 neighbor = worldPosition.Load(int3(neighborPixel, 0));
        if (!all(isfinite(neighbor)) || neighbor.w != position.w) continue;
        float distance = length(neighbor.xyz - position.xyz);
        if (isfinite(distance) && distance > precisionFloor)
            footprint[i / 2] = min(footprint[i / 2], distance);
    }
    float fallback = min(footprint.x, footprint.y);
    if (fallback == FLT_MAX) return precisionFloor;
    if (footprint.x == FLT_MAX) footprint.x = fallback;
    if (footprint.y == FLT_MAX) footprint.y = fallback;
    return max(precisionFloor, max(footprint.x, footprint.y));
}

// Use compatible current samples to limit old bright trails. A shared scalar
// limits RGB luminance; no per-channel color box or display transfer is used.
float temporalHistoryLimit(int2 pixel, float4 position, float3 geometryNormal,
    float3 shadingNormal, float4 material, float roughness, float planeTolerance, float centerLuminance)
{
    float luminances[9];
    uint count = 0;
    uint positiveCount = 0;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            int2 neighborPixel = pixel + int2(x, y);
            if (!temporalInBounds(neighborPixel)) continue;
            float4 neighborPosition = worldPosition.Load(int3(neighborPixel, 0));
            float4 neighborNormals = packedNormals.Load(int3(neighborPixel, 0));
            float4 neighborMaterial = albedoMetalness.Load(int3(neighborPixel, 0));
            float4 neighborEmission = emissionRoughness.Load(int3(neighborPixel, 0));
            float3 radiance = spatialRadiance.Load(int3(neighborPixel, 0)).rgb;
            if (!all(isfinite(neighborPosition)) || neighborPosition.w != position.w ||
                !all(isfinite(neighborNormals)) || !all(isfinite(neighborMaterial)) ||
                !all(isfinite(neighborEmission)) || !all(isfinite(radiance)) ||
                any(neighborEmission.rgb > 0.0f)) continue;
            float3 neighborGeometry, neighborShading;
            decodeNormals(neighborNormals, neighborGeometry, neighborShading);
            if (dot(shadingNormal, neighborShading) < 0.9f ||
                abs(dot(neighborPosition.xyz - position.xyz, geometryNormal)) > planeTolerance ||
                length(saturate(material.rgb) - saturate(neighborMaterial.rgb)) > 0.2f ||
                abs(saturate(material.w) - saturate(neighborMaterial.w)) > 0.1f ||
                abs(roughness - saturate(neighborEmission.w)) > 0.1f) continue;
            float value = temporalLuminance(max(radiance, float3(0.0f)));
            if (value > 0.0f) ++positiveCount;
            uint index = count++;
            while (index > 0)
            {
                if (luminances[index - 1] <= value) break;
                luminances[index] = luminances[index - 1];
                --index;
            }
            luminances[index] = value;
        }
    }
    // Missing positive support is common for sparse Monte Carlo samples. It
    // must not turn an all-black current neighborhood into a zero history cap.
    if (count < 5 || positiveCount < 3) return FLT_MAX;
    float median = luminances[count / 2];
    float upperQuartile = luminances[(3 * (count - 1)) / 4];
    if (!(upperQuartile > 0.0f)) return FLT_MAX;
    float deviations[9];
    [loop]
    for (uint i = 0; i < count; ++i)
    {
        float value = abs(luminances[i] - median);
        uint index = i;
        while (index > 0)
        {
            if (deviations[index - 1] <= value) break;
            deviations[index] = deviations[index - 1];
            --index;
        }
        deviations[index] = value;
    }
    // The cleaned center remains supported even at a narrow valid highlight.
    return max(2.0f * centerLuminance, max(2.0f * upperQuartile, median + 4.0f * deviations[count / 2]));
}

float4 temporalResolve(int2 pixel)
{
    float4 position = worldPosition.Load(int3(pixel, 0));
    float4 normals = packedNormals.Load(int3(pixel, 0));
    float4 material = albedoMetalness.Load(int3(pixel, 0));
    float4 emission = emissionRoughness.Load(int3(pixel, 0));
    float3 source = spatialRadiance.Load(int3(pixel, 0)).rgb;
    float3 current = all(isfinite(source)) ? max(source, float3(0.0f)) : float3(0.0f);
    bool guidesValid = all(isfinite(position)) && all(isfinite(normals)) &&
        all(isfinite(material)) && all(isfinite(emission));
    float roughness = guidesValid ? saturate(emission.w) : 0.0f;
    float metalness = guidesValid ? saturate(material.w) : 0.0f;
    historyPositionOutput[pixel] = guidesValid ? position : float4(0.0f, 0.0f, 0.0f, -1.0f);
    historyNormalRoughnessOutput[pixel] = guidesValid
        ? float4(normals.zw, roughness, metalness) : float4(0.0f);
    float4 resolved = float4(current, 0.0f);
    if (!guidesValid || position.w < 0.0f || !all(isfinite(source)) ||
        any(emission.rgb > 0.0f) || roughness <= 0.001f) return resolved;

    float3 geometryNormal, shadingNormal;
    decodeNormals(normals, geometryNormal, shadingNormal);
    if (!all(isfinite(geometryNormal)) || !all(isfinite(shadingNormal))) return resolved;
    resolved = float4(current, 1.0f);
    if (params.frameInfo.z == 0) return resolved;

    float precisionFloor = max(1e-6f, 2e-6f * max(abs(position.x), max(abs(position.y), abs(position.z))));
    float footprint = temporalFootprint(pixel, position, precisionFloor);
    float planeTolerance = precisionFloor + 0.15f * footprint;
    bool samePixel = params.frameInfo.w != 0 && params.options.x > 0.5f;
    float2 previousPixel = float2(pixel);
    if (!samePixel)
    {
        float4 previousClip = mul(params.previousViewProjection, float4(position.xyz, 1.0f));
        // Primary rays are not clipped to the raster projection's near/far
        // interval. Previous world-position guides provide the depth test.
        if (!all(isfinite(previousClip)) || previousClip.w <= 0.0f) return resolved;
        // The previous matrix already contains Vulkan's Y convention and the
        // exact jitter used to produce the saved guides. Do not apply it twice.
        previousPixel = (previousClip.xy / previousClip.w * 0.5f + 0.5f) * float2(params.frameInfo.xy) - 0.5f;
        if (any(previousPixel < -0.5f) || any(previousPixel > float2(params.frameInfo.xy) - 0.5f)) return resolved;
    }

    int2 basePixel = int2(floor(previousPixel));
    float2 fraction = previousPixel - float2(basePixel);
    float3 history = float3(0.0f);
    float historyLength = 0.0f;
    float historyMass = 0.0f;
    [unroll]
    for (uint tap = 0; tap < 4; ++tap)
    {
        if (samePixel && tap != 0) continue;
        int2 offset = int2(tap & 1, tap >> 1);
        int2 tapPixel = samePixel ? pixel : basePixel + offset;
        if (!temporalInBounds(tapPixel)) continue;
        float weight = samePixel ? 1.0f
            : (offset.x == 0 ? 1.0f - fraction.x : fraction.x)
            * (offset.y == 0 ? 1.0f - fraction.y : fraction.y);
        if (!(weight > 0.0f)) continue;
        float4 oldPosition = previousHistoryPosition.Load(int3(tapPixel, 0));
        float4 oldNormalRoughness = previousHistoryNormalRoughness.Load(int3(tapPixel, 0));
        float4 oldColor = previousHistoryColor.Load(int3(tapPixel, 0));
        if (!all(isfinite(oldPosition)) || oldPosition.w != position.w ||
            !all(isfinite(oldNormalRoughness)) || !all(isfinite(oldColor)) || oldColor.w < 1.0f) continue;
        float3 oldNormal = decodeNormalOctahedron(oldNormalRoughness.xy);
        float3 delta = oldPosition.xyz - position.xyz;
        if (!all(isfinite(oldNormal)) || dot(shadingNormal, oldNormal) < 0.9f ||
            length(delta) > 2.0f * footprint + precisionFloor ||
            // The saved normal is a shading normal; a normal map must not
            // redefine the geometric plane used for disocclusion rejection.
            abs(dot(delta, geometryNormal)) > planeTolerance ||
            abs(roughness - oldNormalRoughness.z) > 0.1f || abs(metalness - oldNormalRoughness.w) > 0.1f) continue;
        history += max(oldColor.rgb, float3(0.0f)) * weight;
        historyLength += min(oldColor.w, 16.0f) * weight;
        historyMass += weight;
    }
    if (!(historyMass > 0.0f)) return resolved;
    history /= historyMass;
    historyLength /= historyMass;
    if (!all(isfinite(history))) return resolved;

    float historyLimit = temporalHistoryLimit(pixel, position, geometryNormal, shadingNormal,
        material, roughness, planeTolerance, temporalLuminance(current));
    float oldLuminance = temporalLuminance(history);
    if (oldLuminance > historyLimit && oldLuminance > 0.0f)
        history *= historyLimit / oldLuminance;

    // The same surface can reflect a different direction after camera motion.
    // Shorten glossy history instead of treating position agreement as proof
    // that its view-dependent radiance is unchanged.
    float viewConfidence = 1.0f;
    if (params.frameInfo.w == 0)
    {
        float3 currentView = normalize(params.currentCameraPosition.xyz - position.xyz);
        float3 previousView = normalize(params.previousCameraPosition.xyz - position.xyz);
        if (!all(isfinite(currentView)) || !all(isfinite(previousView))) return resolved;
        float sensitivity = lerp(0.25f, 1.0f, metalness) * (1.0f - roughness) * (1.0f - roughness);
        viewConfidence = exp(-sensitivity * length(currentView - previousView) / max(0.005f, 0.1f * roughness * roughness));
    }
    float maxHistory = lerp(4.0f, 16.0f, smoothstep(0.15f, 0.65f, roughness));
    float effectiveHistory = min(historyLength, maxHistory - 1.0f) * saturate(historyMass) * viewConfidence;
    float nextLength = 1.0f + effectiveHistory;
    float3 result = lerp(history, current, 1.0f / nextLength);
    if (all(isfinite(result)))
        resolved = float4(max(result, float3(0.0f)), nextLength);
    return resolved;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    int2 pixel = int2(dispatchThreadId.xy);
    if (!temporalInBounds(pixel)) return;
    // Stabilize the final spatial result. Each frame's spatial passes start
    // from freshly prefiltered radiance; history is never spatially blurred
    // again. This also stabilizes changes in the spatial G-buffer weights.
    float4 resolved = temporalResolve(pixel);
    historyColorOutput[pixel] = resolved;
    displayOutput[pixel] = float4(resolved.rgb, 1.0f);
}
