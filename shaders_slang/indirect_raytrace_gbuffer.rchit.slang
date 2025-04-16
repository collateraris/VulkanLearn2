#include "gi_pathtrace.h"

struct BuiltInTriangleIntersectionAttributes
{
    float2 baryCoord;
};

[[vk::binding(0, 0)]]
StructuredBuffer<SVertex> vertices[]			: register(t0);

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t0);
[[vk::binding(8, 0)]]
SamplerState      linearSampler : register(s8);

[[vk::binding(3, 0)]]
StructuredBuffer<SObjectData> objectBuffer  : register(t3);

[[vk::binding(7, 0)]]
StructuredBuffer<int4> indices[]   : register(t7);


[shader("closesthit")]
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

  float2 texCoord =
        uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

  float3 v0_pos = float3(v0.positionXYZ_normalX.x, v0.positionXYZ_normalX.y, v0.positionXYZ_normalX.z);
  float3 v1_pos = float3(v1.positionXYZ_normalX.x, v1.positionXYZ_normalX.y, v1.positionXYZ_normalX.z);
  float3 v2_pos = float3(v2.positionXYZ_normalX.x, v2.positionXYZ_normalX.y, v2.positionXYZ_normalX.z); 

  const float3 pos      = v0_pos * barycentrics.x + v1_pos * barycentrics.y + v2_pos * barycentrics.z;
  const float3 worldPos = mul(ObjectToWorld3x4(), float4(pos, 0.0f)); // Transforming the position to world space

  // Computing the normal at hit position

  float3 v0_nrm = float3(v0.positionXYZ_normalX.w, v0.normalYZ_texCoordUV.x, v0.normalYZ_texCoordUV.y);
  float3 v1_nrm = float3(v1.positionXYZ_normalX.w, v1.normalYZ_texCoordUV.x, v1.normalYZ_texCoordUV.y);
  float3 v2_nrm = float3(v2.positionXYZ_normalX.w, v2.normalYZ_texCoordUV.x, v2.normalYZ_texCoordUV.y); 

  const float3 nrm      = v0_nrm * barycentrics.x + v1_nrm * barycentrics.y + v2_nrm * barycentrics.z;
  float3 worldNorm = normalize(mul(ObjectToWorld3x4(), float4(nrm, 0.0f)));  // Transforming the normal to world space

  float3 edge1 = v1_pos - v0_pos;
  float3 edge2 = v2_pos - v0_pos;
  float2 deltaUV1 = uv1 - uv0;
  float2 deltaUV2 = uv2 - uv0;

  float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

  float3 tangent = float3(0., 0., 0.);
  tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
  tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
  tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
  tangent = normalize(tangent);
  float3 worldTangent = normalize(mul(ObjectToWorld3x4(), float4(tangent, 0.0f)));

  float3 bitangent = float3(0., 0., 0.);
  bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
  bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
  bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
  bitangent = normalize(bitangent); 
  float3 worldBitangent = normalize(mul(ObjectToWorld3x4(), float4(bitangent, 0.0f)));

  float3x3 TBN = float3x3(worldTangent, worldBitangent, worldNorm);

  indirectRpl.albedo_metalness.xyz = texSet[shadeData.diffuseTexIndex].Sample(linearSampler, texCoord).rgb;

  indirectRpl.emission_roughness.xyz = float3(0., 0., 0);
  if (shadeData.emissionTexIndex > 0)
      indirectRpl.emission_roughness.xyz = texSet[shadeData.emissionTexIndex].Sample(linearSampler, texCoord).rgb;  

  indirectRpl.albedo_metalness.w = 0.;
  if (shadeData.metalnessTexIndex > 0)
      indirectRpl.albedo_metalness.w = 1. - texSet[shadeData.metalnessTexIndex].Sample(linearSampler, texCoord).r; 

  indirectRpl.emission_roughness.w = 1.;
  if (shadeData.roughnessTexIndex > 0)
      indirectRpl.emission_roughness.w = texSet[shadeData.roughnessTexIndex].Sample(linearSampler, texCoord).b;    

  float3 geometryNormal = worldNorm;
  float3 shadingNormal = worldNorm;
  if (shadeData.normalTexIndex > 0 && 0)
  {
      float3 normal = texSet[shadeData.normalTexIndex].Sample(linearSampler, texCoord).b;
      normal = normalize(normal * 2.0 - 1.0);   
      shadingNormal = normalize(mul(TBN, normal));
  } 
    indirectRpl.normal_ = encodeNormals(geometryNormal, shadingNormal);
	indirectRpl.position_objectID = float4(worldPos, float(InstanceID()));
}
