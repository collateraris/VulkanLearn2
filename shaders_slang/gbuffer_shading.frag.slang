//shader input
struct v2f
{
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float2 uv : TEXCOORD;
};

[[vk::binding(0, 0)]]
Texture2D<float4> giTex : register(t0);
[[vk::binding(0, 0)]]
SamplerState      giTex_Sampler : register(s0);

[[vk::constant_id(0)]]
const uint outputIsSrgb = 1;

float encodeSrgb(float linear)
{
    return linear <= 0.0031308f ? 12.92f * linear : 1.055f * pow(linear, 1.f / 2.4f) - 0.055f;
}

[shader("fragment")]
float4 main(v2f vo)
{
    float3 gi = giTex.Sample(giTex_Sampler, vo.uv).rgb;

    gi = all(isfinite(gi)) ? max(gi, float3(0.f)) : float3(0.f);
    float3 mapped = gi / (gi + float3(1.0));
    // Preserve the project's original display contrast curve. Apply it only
    // here, after linear HDR frame accumulation, never to the history itself.
    mapped = pow(mapped, float3(1.0f / 2.2f));
    // SRGB attachments perform the transfer function during the write.
    if (outputIsSrgb == 0)
        mapped = float3(encodeSrgb(mapped.x), encodeSrgb(mapped.y), encodeSrgb(mapped.z));
	return float4(mapped,1.0f);
}
