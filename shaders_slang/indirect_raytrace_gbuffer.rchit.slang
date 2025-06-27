#include "gi_pathtrace.h"

struct BuiltInTriangleIntersectionAttributes
{
    float2 baryCoord;
};

[[vk::binding(0, 0)]]
StructuredBuffer<SVertex> vertices[]			: register(t0);

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t1);
[[vk::binding(6, 0)]]
SamplerState      linearSampler : register(s6);

[[vk::binding(3, 0)]]
StructuredBuffer<SObjectData> objectBuffer  : register(t3);

[[vk::binding(5, 0)]]
StructuredBuffer<int> indices[]   : register(t5);

[[vk::binding(7, 0)]]
StructuredBuffer<SMaterialData> matBuffer  : register(t7);


[shader("closesthit")]
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

  float2 texCoord =
        uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

  float3 v0_pos = float3(v0.positionXYZ_normalX.x, v0.positionXYZ_normalX.y, v0.positionXYZ_normalX.z);
  float3 v1_pos = float3(v1.positionXYZ_normalX.x, v1.positionXYZ_normalX.y, v1.positionXYZ_normalX.z);
  float3 v2_pos = float3(v2.positionXYZ_normalX.x, v2.positionXYZ_normalX.y, v2.positionXYZ_normalX.z); 

  float4x4 modelT = transpose(shadeData.model);
  float4x4 modelIT = transpose(shadeData.modelIT);
  float3 triangleVertices[3];

  triangleVertices[0] = mul(float4(v0_pos, 1.), modelT).xyz;
  triangleVertices[1] = mul(float4(v1_pos, 1.), modelT).xyz;
  triangleVertices[2] = mul(float4(v2_pos, 1.), modelT).xyz;

  float3 edge20 = triangleVertices[2] - triangleVertices[0];
  float3 edge21 = triangleVertices[2] - triangleVertices[1];
  float3 edge10 = triangleVertices[1] - triangleVertices[0];

  float3 geometryNormal = normalize(cross(edge20, edge10));

  const float3 worldPos      = triangleVertices[0] * barycentrics.x + triangleVertices[1] * barycentrics.y + triangleVertices[2] * barycentrics.z;

  // Computing the normal at hit position

  float3 v0_nrm = float3(v0.positionXYZ_normalX.w, v0.normalYZ_texCoordUV.x, v0.normalYZ_texCoordUV.y);
  float3 v1_nrm = float3(v1.positionXYZ_normalX.w, v1.normalYZ_texCoordUV.x, v1.normalYZ_texCoordUV.y);
  float3 v2_nrm = float3(v2.positionXYZ_normalX.w, v2.normalYZ_texCoordUV.x, v2.normalYZ_texCoordUV.y); 

  const float3 nrm      = v0_nrm * barycentrics.x + v1_nrm * barycentrics.y + v2_nrm * barycentrics.z;
  float3 worldNormal = normalize(mul(float4(nrm, 0.0f), modelIT).xyz);  // Transforming the normal to world space

  float3 v0_tan = float3(v0.tangentXYZ_.x, v0.tangentXYZ_.y, v0.tangentXYZ_.z);
  float3 v1_tan = float3(v1.tangentXYZ_.x, v1.tangentXYZ_.y, v1.tangentXYZ_.z);
  float3 v2_tan = float3(v2.tangentXYZ_.x, v2.tangentXYZ_.y, v2.tangentXYZ_.z); 

  const float3 tan      = v0_tan * barycentrics.x + v1_tan * barycentrics.y + v2_tan * barycentrics.z;
  float3 worldTangent = normalize(mul(float4(tan, 0.0f), modelT).xyz);  // Transforming the normal to world space
  // Graham-Schmidt orthogonalization
  worldTangent = normalize(worldTangent - dot(worldTangent, worldNormal) * worldNormal);
  float3 worldBitangent = normalize(cross(worldNormal, worldTangent));
  float3x3 TBN = float3x3(worldTangent, worldBitangent, worldNormal);

  indirectRpl.albedo_metalness.xyz = texSet[mat.diffuseTexIndex].SampleLevel(linearSampler, texCoord, 0).rgb;

  indirectRpl.emission_roughness.xyz = float3(0., 0., 0);
  if (mat.emissionTexIndex > 0)
  {
      indirectRpl.emission_roughness.xyz = texSet[mat.emissionTexIndex].SampleLevel(linearSampler, texCoord, 0).rgb; 
      indirectRpl.emission_roughness.xyz = float3(indirectRpl.emission_roughness.x * mat.emissiveFactorMult_emissiveStrength.x, indirectRpl.emission_roughness.y * mat.emissiveFactorMult_emissiveStrength.y, indirectRpl.emission_roughness.z * mat.emissiveFactorMult_emissiveStrength.z);
  } 

  indirectRpl.albedo_metalness.w = 0.;
  if (mat.metalnessTexIndex > 0)
      indirectRpl.albedo_metalness.w = texSet[mat.metalnessTexIndex].SampleLevel(linearSampler, texCoord, 0).b; 
  indirectRpl.albedo_metalness.w *= mat.metallicFactor_roughnessFactor_transparent_.x;

  indirectRpl.emission_roughness.w = 1.;
  if (mat.roughnessTexIndex > 0)
      indirectRpl.emission_roughness.w = texSet[mat.roughnessTexIndex].SampleLevel(linearSampler, texCoord, 0).g;
  indirectRpl.emission_roughness.w *= mat.metallicFactor_roughnessFactor_transparent_.y;
      
  float3 shadingNormal = worldNormal;
  // avoid normal mapping if tangent = (0, 0, 0), which results in NaN
  if (mat.normalTexIndex > 0 && abs(dot(worldTangent, worldTangent)) > 1e-6 && 0)
  {
      float2 normal = texSet[mat.normalTexIndex].SampleLevel(linearSampler, texCoord, 0).rg;
      float3 bumpNormal = float3(mad(2.0f, normal, -1.f), 0.f);
      bumpNormal.z = sqrt(1.f - dot(bumpNormal, bumpNormal));
      // Invalid bump, normalize() leads to NaN
      if (dot(bumpNormal, bumpNormal) > 1e-6f)
      {
          bumpNormal = normalize(bumpNormal);
          shadingNormal = normalize(mul(bumpNormal, TBN).xyz);
      }
            
  } 
    indirectRpl.normal_ = encodeNormals(geometryNormal, shadingNormal);
	indirectRpl.position_objectID = float4(worldPos, float(InstanceID()));
}
