[[vk::binding(0, 0)]] Texture2D<float4> sourceRadiance;
[[vk::binding(1, 0)]] Texture2D<float4> worldPosition;
[[vk::binding(2, 0)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> outputColor;
[[vk::binding(3, 0)]] [[vk::image_format("r32f")]] RWTexture2D<float> outputDepth;
[[vk::binding(4, 0)]] [[vk::image_format("rg16f")]] RWTexture2D<float2> outputMotion;

struct PrepareConstants
{
    float4x4 currentViewProjection;  // Unjittered, legacy RH clip Z [-1, 1].
    float4x4 previousViewProjection;
    float4x4 jitteredViewProjection;
    float4x4 inverseProjection;      // Current unjittered projection inverse.
    float4x4 inverseView;
    uint4 extent;
    float4 jitterUv;                 // Actual clip-translation jitter / 2.
};
[[vk::binding(5, 0)]] ConstantBuffer<PrepareConstants> params;

float2 pixelMotion(float4 currentClip, float4 previousClip)
{
    // A point behind the old camera has no usable history. Give DLSS a finite
    // out-of-frame coordinate instead of falsely declaring zero movement.
    if (!all(isfinite(currentClip)) || !all(isfinite(previousClip)) ||
        currentClip.w <= 1e-6f || previousClip.w <= 1e-6f)
        return min(2.0f * float2(params.extent.xy), float2(65504.0f));
    float2 motion = (previousClip.xy / previousClip.w - currentClip.xy / currentClip.w)
        * 0.5f * float2(params.extent.xy);
    if (!all(isfinite(motion)))
        return min(2.0f * float2(params.extent.xy), float2(65504.0f));
    // Current -> previous, in render pixels. Both projections exclude jitter;
    // Vulkan's Y flip is already in them, so positive Y points down the image.
    return clamp(motion, float2(-65504.0f), float2(65504.0f));
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadId.xy;
    if (any(pixel >= params.extent.xy)) return;
    float3 radiance = sourceRadiance.Load(int3(pixel, 0)).rgb;
    // FP16 representability only; no exposure, tone mapping, or gamma here.
    radiance = all(isfinite(radiance)) ? clamp(radiance, float3(0.0f), float3(65504.0f)) : float3(0.0f);
    outputColor[pixel] = float4(radiance, 1.0f);

    float4 position = worldPosition.Load(int3(pixel, 0));
    float depth = 1.0f;
    float2 motion;
    if (all(isfinite(position)) && position.w >= 0.0f)
    {
        float4 world = float4(position.xyz, 1.0f);
        float4 clip = mul(params.jitteredViewProjection, world);
        if (all(isfinite(clip)) && clip.w > 1e-6f)
        {
            // PlayerCamera uses GLM's RH_NO projection, not RH_ZO. Streamline's
            // clip matrices must use the same [-1,1] -> [0,1] Z conversion.
            depth = saturate(0.5f * (clip.z / clip.w) + 0.5f);
        }
        motion = pixelMotion(mul(params.currentViewProjection, world), mul(params.previousViewProjection, world));
    }
    else
    {
        float2 ndc = (float2(pixel) + 0.5f) / float2(params.extent.xy) * 2.0f - 1.0f;
        // The visible sky sample came from the jittered projection. Undo the
        // exact clip shift before reconstructing with its unjittered inverse.
        ndc -= 2.0f * params.jitterUv.xy;
        float3 viewRay = normalize(mul(params.inverseProjection, float4(ndc, 1.0f, 1.0f)).xyz);
        float3 worldRay = mul(params.inverseView, float4(viewRay, 0.0f)).xyz;
        // Homogeneous w=0 removes camera translation: an environment map is
        // infinitely distant but still needs correct rotational motion.
        motion = pixelMotion(mul(params.currentViewProjection, float4(worldRay, 0.0f)),
                             mul(params.previousViewProjection, float4(worldRay, 0.0f)));
    }
    outputDepth[pixel] = depth;
    outputMotion[pixel] = motion;
}
