#version 460 

struct v2f
{
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float2 uv : TEXCOORD;
};

[shader("vertex")]
v2f main(uint vid : SV_VertexID) 
{
    v2f o = v2f(0);
    o.uv = float2((vid << 1) & 2, vid & 2);
    o.pos = float4(o.uv * 2.0f + -1.0f, 0.0f, 1.0f);
    return o;
}