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

[shader("fragment")]
float4 main(v2f vo)
{
    float3 gi = giTex.Sample(giTex_Sampler, vo.uv).rgb;

    const float gamma = 2.2;
    float3 mapped = gi / (gi + float3(1.0));
    mapped = pow(mapped, float3(1.0 / gamma));
	return float4(mapped,1.0f);
}