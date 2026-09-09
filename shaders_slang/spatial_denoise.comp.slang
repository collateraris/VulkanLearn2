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

struct DenoiseConstants
{
    uint width;
    uint height;
    uint stepWidth;
    uint passIndex;
};
[[vk::push_constant]]
ConstantBuffer<DenoiseConstants> params;

bool denoiseInBounds(int2 pixel)
{
    return all(pixel >= int2(0)) && pixel.x < int(params.width) && pixel.y < int(params.height);
}

float denoiseKernel(int offset)
{
    // Separable cubic B-spline [1, 4, 6, 4, 1] / 16.
    int distance = abs(offset);
    return distance == 0 ? 6.0f / 16.0f : (distance == 1 ? 4.0f / 16.0f : 1.0f / 16.0f);
}

float denoiseMaxComponent(float3 value)
{
    return max(value.x, max(value.y, value.z));
}

float2 denoisePixelFootprint(int2 pixel, float4 centerPosition, float precisionFloor)
{
    float2 footprint = float2(FLT_MAX);
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        uint axis = i / 2;
        int sign = (i & 1) == 0 ? -1 : 1;
        int2 offset = axis == 0 ? int2(sign, 0) : int2(0, sign);
        int2 neighborPixel = pixel + offset;
        if (!denoiseInBounds(neighborPixel)) continue;
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

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    int2 pixel = int2(dispatchThreadId.xy);
    if (!denoiseInBounds(pixel)) return;

    float3 source = sourceRadiance.Load(int3(pixel, 0)).rgb;
    bool sourceValid = all(isfinite(source));
    float3 center = sourceValid ? max(source, float3(0.0f)) : float3(0.0f);
    float4 position = worldPosition.Load(int3(pixel, 0));
    float4 material = albedoMetalness.Load(int3(pixel, 0));
    float4 emission = emissionRoughness.Load(int3(pixel, 0));
    float4 normals = packedNormals.Load(int3(pixel, 0));

    // Background and directly visible emitters must not bleed into geometry.
    // Guides always come from the current G-buffer, never a filtered version.
    if (!all(isfinite(position)) || position.w < 0.0f ||
        !all(isfinite(material)) || !all(isfinite(emission)) ||
        !all(isfinite(normals)) || any(emission.rgb > 0.0f))
    {
        outputRadiance[pixel] = float4(center, 1.0f);
        return;
    }

    float roughness = saturate(emission.w);
    float metalness = saturate(material.w);
    // Total radiance has no separate specular buffer. Reduce filtering on
    // glossy surfaces and leave mirrors intact instead of blurring reflections.
    float strength = smoothstep(lerp(0.10f, 0.22f, metalness),
        lerp(0.45f, 0.65f, metalness), roughness);
    if (params.passIndex == 0)
        // The first B3 pass has a one-pixel standard deviation. Use this local
        // filter on non-mirror surfaces without widening glossy reflections
        // through the later, more strictly roughness-gated passes.
        strength = max(strength, 0.85f * smoothstep(0.001f, 0.03f, roughness));
    else
        strength *= smoothstep(0.20f, 0.55f, roughness);
    if (!(strength > 0.0f))
    {
        outputRadiance[pixel] = float4(center, 1.0f);
        return;
    }

    float3 geometryNormal, shadingNormal;
    decodeNormals(normals, geometryNormal, shadingNormal);
    if (!all(isfinite(geometryNormal)) || !all(isfinite(shadingNormal)))
    {
        outputRadiance[pixel] = float4(center, 1.0f);
        return;
    }

    float precisionFloor = max(1e-6f, 2e-6f * denoiseMaxComponent(abs(position.xyz)));
    float2 footprint = denoisePixelFootprint(pixel, position, precisionFloor);
    int stride = int(clamp(params.stepWidth, 1u, 4u));
    float centerWeight = sourceValid ? denoiseKernel(0) * denoiseKernel(0) : 0.0f;
    float3 weightedRadiance = center * centerWeight;
    float weightSum = centerWeight;

    [loop]
    for (int y = -2; y <= 2; ++y)
    {
        [loop]
        for (int x = -2; x <= 2; ++x)
        {
            if (x == 0 && y == 0) continue;
            int2 offset = int2(x, y) * stride;
            int2 neighborPixel = pixel + offset;
            if (!denoiseInBounds(neighborPixel)) continue;
            float4 neighborPosition = worldPosition.Load(int3(neighborPixel, 0));
            if (!all(isfinite(neighborPosition)) || neighborPosition.w != position.w) continue;

            float4 neighborEmission = emissionRoughness.Load(int3(neighborPixel, 0));
            float4 neighborMaterial = albedoMetalness.Load(int3(neighborPixel, 0));
            float4 neighborNormals = packedNormals.Load(int3(neighborPixel, 0));
            if (!all(isfinite(neighborEmission)) || any(neighborEmission.rgb > 0.0f) ||
                !all(isfinite(neighborMaterial)) || !all(isfinite(neighborNormals))) continue;
            float3 neighborGeometry, neighborShading;
            decodeNormals(neighborNormals, neighborGeometry, neighborShading);
            if (!all(isfinite(neighborGeometry)) || !all(isfinite(neighborShading))) continue;

            float3 delta = neighborPosition.xyz - position.xyz;
            float expectedDistance = dot(abs(float2(offset)), footprint);
            float distance = length(delta);
            if (!isfinite(distance) || distance > 4.0f * expectedDistance + precisionFloor) continue;
            float planeDistance = max(abs(dot(delta, geometryNormal)), abs(dot(delta, neighborGeometry)));
            float planeTolerance = precisionFloor + 0.10f * max(footprint.x, footprint.y) * stride;
            float planeWeight = exp(-planeDistance / planeTolerance);
            float normalWeight = pow(saturate(dot(geometryNormal, neighborGeometry)), 32.0f)
                * pow(saturate(dot(shadingNormal, neighborShading)), 32.0f);
            float3 albedoDelta = saturate(material.rgb) - saturate(neighborMaterial.rgb);
            float materialWeight = exp(-dot(albedoDelta, albedoDelta) / (0.12f * 0.12f)
                - 16.0f * abs(metalness - saturate(neighborMaterial.w))
                - 16.0f * abs(roughness - saturate(neighborEmission.w)));
            float weight = denoiseKernel(x) * denoiseKernel(y) * planeWeight * normalWeight * materialWeight;
            if (!(weight > 0.0f) || !isfinite(weight)) continue;
            float3 radiance = sourceRadiance.Load(int3(neighborPixel, 0)).rgb;
            if (!all(isfinite(radiance))) continue;
            weightedRadiance += max(radiance, float3(0.0f)) * weight;
            weightSum += weight;
        }
    }

    // One nonnegative normalized weight is shared by all RGB channels. There
    // is no exposure, tone curve or intensity clipping here. Isolated bright
    // samples are averaged, not rejected as if they were necessarily invalid.
    float3 filtered = weightSum > 0.0f ? weightedRadiance / weightSum : center;
    float3 result = lerp(center, filtered, sourceValid ? strength : 1.0f);
    outputRadiance[pixel] = float4(all(isfinite(result)) ? max(result, float3(0.0f)) : center, 1.0f);
}
