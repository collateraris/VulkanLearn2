#include "gi_pathtrace.h"

[[vk::binding(0, 0)]]
Texture2D<float4> sourceRadiance;
[[vk::binding(1, 0)]]
[[vk::image_format("rgba32f")]]
RWTexture2D<float4> outputRadiance;
[[vk::binding(2, 0)]]
Texture2D<float4> worldPosition;
[[vk::binding(3, 0)]]
Texture2D<float4> packedNormals;
[[vk::binding(4, 0)]]
Texture2D<float4> albedoMetalness;
[[vk::binding(5, 0)]]
Texture2D<float4> emissionRoughness;

struct PrefilterConstants
{
    uint width;
    uint height;
    uint stepWidth;
    uint passIndex;
};
[[vk::push_constant]]
ConstantBuffer<PrefilterConstants> params;

bool prefilterInBounds(int2 pixel)
{
    return all(pixel >= int2(0)) && pixel.x < int(params.width) && pixel.y < int(params.height);
}

float prefilterLuminance(float3 radiance)
{
    return dot(radiance, float3(0.2126f, 0.7152f, 0.0722f));
}

float prefilterMaxComponent(float3 value)
{
    return max(value.x, max(value.y, value.z));
}

void prefilterSort(inout float values[24], uint count)
{
    for (uint i = 1; i < count; ++i)
    {
        float value = values[i];
        uint j = i;
        while (j > 0)
        {
            if (values[j - 1] <= value) break;
            values[j] = values[j - 1];
            --j;
        }
        values[j] = value;
    }
}

float prefilterMedian(float values[24], uint count)
{
    // Multiplying before adding also avoids overflow for very bright HDR data.
    return (count & 1u) != 0 ? values[count / 2]
        : 0.5f * values[count / 2 - 1] + 0.5f * values[count / 2];
}

float2 prefilterPixelFootprint(int2 pixel, float4 centerPosition, float precisionFloor)
{
    float2 footprint = float2(FLT_MAX);
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        uint axis = i / 2;
        int direction = (i & 1u) == 0 ? -1 : 1;
        int2 neighborPixel = pixel + (axis == 0 ? int2(direction, 0) : int2(0, direction));
        if (!prefilterInBounds(neighborPixel)) continue;
        float4 neighbor = worldPosition.Load(int3(neighborPixel, 0));
        if (!all(isfinite(neighbor)) || neighbor.w != centerPosition.w) continue;
        float distance = length(neighbor.xyz - centerPosition.xyz);
        if (isfinite(distance) && distance > precisionFloor)
            footprint[axis] = min(footprint[axis], distance);
    }
    float fallback = min(footprint.x, footprint.y);
    if (fallback == FLT_MAX) fallback = precisionFloor;
    if (footprint.x == FLT_MAX) footprint.x = fallback;
    if (footprint.y == FLT_MAX) footprint.y = fallback;
    return max(footprint, float2(precisionFloor));
}

bool prefilterNeighborLuminance(int2 pixel, int2 offset, float4 position, float4 material,
    float roughness, float metalness, float3 geometryNormal, float3 shadingNormal,
    float2 footprint, float precisionFloor, float planeTolerance, out float luminance)
{
    luminance = 0.0f;
    int2 neighborPixel = pixel + offset;
    if (!prefilterInBounds(neighborPixel)) return false;
    float4 neighborPosition = worldPosition.Load(int3(neighborPixel, 0));
    if (!all(isfinite(neighborPosition)) || neighborPosition.w != position.w) return false;

    float4 neighborEmission = emissionRoughness.Load(int3(neighborPixel, 0));
    float4 neighborMaterial = albedoMetalness.Load(int3(neighborPixel, 0));
    float4 neighborNormals = packedNormals.Load(int3(neighborPixel, 0));
    if (!all(isfinite(neighborEmission)) || any(neighborEmission.rgb > 0.0f) ||
        !all(isfinite(neighborMaterial)) || !all(isfinite(neighborNormals))) return false;
    float3 neighborGeometry, neighborShading;
    decodeNormals(neighborNormals, neighborGeometry, neighborShading);
    if (!all(isfinite(neighborGeometry)) || !all(isfinite(neighborShading)) ||
        dot(geometryNormal, neighborGeometry) < 0.95f ||
        dot(shadingNormal, neighborShading) < 0.85f) return false;

    float3 delta = neighborPosition.xyz - position.xyz;
    float distance = length(delta);
    float expectedDistance = dot(abs(float2(offset)), footprint);
    if (!isfinite(distance) || distance > 4.0f * expectedDistance + precisionFloor) return false;
    float planeDistance = max(abs(dot(delta, geometryNormal)), abs(dot(delta, neighborGeometry)));
    if (planeDistance > planeTolerance) return false;
    float3 albedoDelta = saturate(material.rgb) - saturate(neighborMaterial.rgb);
    if (dot(albedoDelta, albedoDelta) > 0.15f * 0.15f ||
        abs(metalness - saturate(neighborMaterial.w)) > 0.10f ||
        abs(roughness - saturate(neighborEmission.w)) > 0.10f) return false;

    float3 neighborRadiance = sourceRadiance.Load(int3(neighborPixel, 0)).rgb;
    if (!all(isfinite(neighborRadiance))) return false;
    luminance = prefilterLuminance(max(neighborRadiance, float3(0.0f)));
    return isfinite(luminance);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    int2 pixel = int2(dispatchThreadId.xy);
    if (!prefilterInBounds(pixel)) return;

    float3 source = sourceRadiance.Load(int3(pixel, 0)).rgb;
    float3 center = all(isfinite(source)) ? max(source, float3(0.0f)) : float3(0.0f);
    outputRadiance[pixel] = float4(center, 1.0f);

    float centerLuminance = prefilterLuminance(center);
    if (!(centerLuminance > 0.0f) || !isfinite(centerLuminance)) return;

    float4 position = worldPosition.Load(int3(pixel, 0));
    float4 material = albedoMetalness.Load(int3(pixel, 0));
    float4 emission = emissionRoughness.Load(int3(pixel, 0));
    float4 normals = packedNormals.Load(int3(pixel, 0));
    if (!all(isfinite(position)) || position.w < 0.0f ||
        !all(isfinite(material)) || !all(isfinite(emission)) ||
        !all(isfinite(normals)) || any(emission.rgb > 0.0f)) return;

    float roughness = saturate(emission.w);
    float metalness = saturate(material.w);
    // A reflected emitter in a mirror can legitimately occupy a single pixel.
    if (roughness <= 0.001f) return;

    float3 geometryNormal, shadingNormal;
    decodeNormals(normals, geometryNormal, shadingNormal);
    if (!all(isfinite(geometryNormal)) || !all(isfinite(shadingNormal))) return;

    float precisionFloor = max(1e-6f, 2e-6f * prefilterMaxComponent(abs(position.xyz)));
    float2 footprint = prefilterPixelFootprint(pixel, position, precisionFloor);
    float planeTolerance = precisionFloor + 0.25f * max(footprint.x, footprint.y);
    float luminances[24];
    float deviations[24];
    uint count = 0;
    uint positiveCount = 0;
    float multiplier = lerp(8.0f, 3.0f, smoothstep(0.03f, 0.35f, roughness));

    // First test the inner ring. Its median, rather than its upper quartile,
    // keeps a 2x2 cluster (three bright neighbors) eligible for the wider test.
    [loop]
    for (int y = -1; y <= 1; ++y)
    {
        [loop]
        for (int x = -1; x <= 1; ++x)
        {
            if (x == 0 && y == 0) continue;
            float luminance;
            if (!prefilterNeighborLuminance(pixel, int2(x, y), position, material, roughness, metalness,
                geometryNormal, shadingNormal, footprint, precisionFloor, planeTolerance, luminance)) continue;
            luminances[count++] = luminance;
            if (luminance > 0.0f) ++positiveCount;
        }
    }

    if (count >= 5 && positiveCount >= 3)
    {
        prefilterSort(luminances, count);
        float innerMedian = prefilterMedian(luminances, count);
        for (uint i = 0; i < count; ++i)
            deviations[i] = abs(luminances[i] - innerMedian);
        prefilterSort(deviations, count);
        float innerMad = prefilterMedian(deviations, count);
        float ordinaryLimit = max(multiplier * innerMedian, innerMedian + multiplier * innerMad);
        if (centerLuminance <= ordinaryLimit) return;
    }

    // Sparse first-frame neighborhoods and suspected outliers receive the
    // complete original 5x5 test. Append only the outer ring, with identical
    // surface compatibility checks and no duplicate inner samples.
    [loop]
    for (int y = -2; y <= 2; ++y)
    {
        [loop]
        for (int x = -2; x <= 2; ++x)
        {
            if (abs(x) != 2 && abs(y) != 2) continue;
            float luminance;
            if (!prefilterNeighborLuminance(pixel, int2(x, y), position, material, roughness, metalness,
                geometryNormal, shadingNormal, footprint, precisionFloor, planeTolerance, luminance)) continue;
            luminances[count++] = luminance;
            if (luminance > 0.0f) ++positiveCount;
        }
    }

    // Sparse first-frame paths are not evidence of a firefly. In particular,
    // an otherwise black neighborhood must never erase its sole lit sample.
    if (count < 5 || positiveCount < 3) return;
    prefilterSort(luminances, count);
    float median = prefilterMedian(luminances, count);
    float upperQuartile = luminances[(3 * count - 1) / 4];
    if (!(upperQuartile > 0.0f)) return;
    for (uint i = 0; i < count; ++i)
        deviations[i] = abs(luminances[i] - median);
    prefilterSort(deviations, count);
    float mad = prefilterMedian(deviations, count);

    // Everything here scales with the local HDR signal: no exposure-dependent
    // cap. Glossy surfaces get a wider allowance for real specular highlights.
    float threshold = max(multiplier * upperQuartile, median + multiplier * mad);
    if (centerLuminance > threshold && threshold > 0.0f)
    {
        // One multiplier preserves the sample's RGB ratios. The separate pass
        // also prevents an unfiltered outlier from spreading to adjacent pixels
        // before the spatial denoiser has processed the outlier's own center.
        outputRadiance[pixel] = float4(center * (threshold / centerLuminance), 1.0f);
    }
}
