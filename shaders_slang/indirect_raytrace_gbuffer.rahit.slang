#include "gi_pathtrace.h"

struct BuiltInTriangleIntersectionAttributes
{
    float2 baryCoord;
};

[[vk::binding(0, 0)]]
StructuredBuffer<SVertex> vertices[]			: register(t0);

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t0);
[[vk::binding(11, 0)]]
SamplerState      linearSampler : register(s8);

[[vk::binding(3, 0)]]
StructuredBuffer<SObjectData> objectBuffer  : register(t3);

[[vk::binding(7, 0)]]
StructuredBuffer<int4> indices[]   : register(t7);

[shader("anyhit")]
void main(inout IndirectGbufferRayPayload indirectRpl, BuiltInTriangleIntersectionAttributes attrib)
{
    SObjectData shadeData = objectBuffer[InstanceID()];

    uint4 index = indices[shadeData.meshIndex][PrimitiveIndex()];
    
    SVertex v0 = vertices[shadeData.meshIndex][index.x];
    SVertex v1 = vertices[shadeData.meshIndex][index.y];
    SVertex v2 = vertices[shadeData.meshIndex][index.z];

    const float3 barycentrics = float3(1.0 - attrib.baryCoord.x - attrib.baryCoord.y, attrib.baryCoord.x, attrib.baryCoord.y);

    float2 uv0 = v0.normalYZ_texCoordUV.zw;
    float2 uv1 = v1.normalYZ_texCoordUV.zw;  
    float2 uv2 = v2.normalYZ_texCoordUV.zw;

    float2 texCoord = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

    float opacity = texSet[shadeData.diffuseTexIndex].Sample(linearSampler, texCoord).a;

    opacity *= shadeData.baseColorFactor.w;

    if (shadeData.opacityTexIndex > 0)
        opacity = texSet[shadeData.opacityTexIndex].Sample(linearSampler, texCoord).r;

    if (opacity < 0.5)
        IgnoreHit();
}