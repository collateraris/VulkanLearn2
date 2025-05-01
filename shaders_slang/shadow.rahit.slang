#include "gi_pathtrace.h"

struct BuiltInTriangleIntersectionAttributes
{
    float2 baryCoord;
};

[[vk::binding(0, 0)]]
StructuredBuffer<SVertex> vertices[]			: register(t0);

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t0);
[[vk::binding(6, 0)]]
SamplerState      linearSampler : register(s6);

[[vk::binding(3, 0)]]
StructuredBuffer<SObjectData> objectBuffer  : register(t3);

[[vk::binding(5, 0)]]
StructuredBuffer<int> indices[]   : register(t5);

[[vk::binding(7, 0)]]
StructuredBuffer<SMaterialData> matBuffer  : register(t7);

[shader("anyhit")]
void main(inout IndirectGbufferRayPayload indirectRpl, BuiltInTriangleIntersectionAttributes attrib)
{
    SObjectData shadeData = objectBuffer[InstanceID()];

    SMaterialData mat = matBuffer[shadeData.materialIndex];

    int index0 = indices[shadeData.meshIndex][PrimitiveIndex() * 3];
    int index1 = indices[shadeData.meshIndex][PrimitiveIndex() * 3 + 1];
    int index2 = indices[shadeData.meshIndex][PrimitiveIndex() * 3 + 2];
    
    SVertex v0 = vertices[shadeData.meshIndex][index0];
    SVertex v1 = vertices[shadeData.meshIndex][index1];
    SVertex v2 = vertices[shadeData.meshIndex][index2];

    const float3 barycentrics = float3(1.0 - attrib.baryCoord.x - attrib.baryCoord.y, attrib.baryCoord.x, attrib.baryCoord.y);

    float2 uv0 = v0.normalYZ_texCoordUV.zw;
    float2 uv1 = v1.normalYZ_texCoordUV.zw;  
    float2 uv2 = v2.normalYZ_texCoordUV.zw;

    float2 texCoord = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

    float opacity = texSet[mat.diffuseTexIndex].Sample(linearSampler, texCoord).a;
    opacity *= mat.baseColorFactor.w;

    if (mat.opacityTexIndex > 0)
        opacity = texSet[mat.opacityTexIndex].Sample(linearSampler, texCoord).r;

    if (opacity < 1.)
        IgnoreHit();
    else
        AcceptHitAndEndSearch();
}