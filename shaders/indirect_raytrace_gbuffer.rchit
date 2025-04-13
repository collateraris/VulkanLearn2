#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "gi_raytrace.h"

layout(location = 0) rayPayloadInEXT  IndirectGbufferRayPayload indirectRpl;

hitAttributeEXT vec2 baryCoord;

layout (set = 0, std140, binding = 0) readonly buffer _vertices
{
	SVertex vertices[];
} Vertices[];

layout(set = 0, binding = 1) uniform sampler2D texSet[];
layout(set = 0, binding = 2) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, std140, binding = 3) readonly buffer ObjectBuffer{

	SObjectData objects[];
} objectBuffer;

layout (set = 0, std140, binding = 7) readonly buffer _indices
{
	ivec4 indices[];
} Indices[];

layout(set = 1, binding = 0) uniform _GlobalGIParams { SGlobalGIParams giParams; };

void main()
{
  SObjectData shadeData = objectBuffer.objects[gl_InstanceID];

  ivec4 index = Indices[shadeData.meshIndex].indices[gl_PrimitiveID];
  
  SVertex v0 = Vertices[shadeData.meshIndex].vertices[index.x];
  SVertex v1 = Vertices[shadeData.meshIndex].vertices[index.y];
  SVertex v2 = Vertices[shadeData.meshIndex].vertices[index.z];

  const vec3 barycentrics = vec3(1.0 - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);

  vec2 uv0 = v0.normalYZ_texCoordUV.zw;
  vec2 uv1 = v1.normalYZ_texCoordUV.zw;  
  vec2 uv2 = v2.normalYZ_texCoordUV.zw;

  vec2 texCoord =
        uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

  vec3 v0_pos = vec3(v0.positionXYZ_normalX.x, v0.positionXYZ_normalX.y, v0.positionXYZ_normalX.z);
  vec3 v1_pos = vec3(v1.positionXYZ_normalX.x, v1.positionXYZ_normalX.y, v1.positionXYZ_normalX.z);
  vec3 v2_pos = vec3(v2.positionXYZ_normalX.x, v2.positionXYZ_normalX.y, v2.positionXYZ_normalX.z);      

  const vec3 pos      = v0_pos * barycentrics.x + v1_pos * barycentrics.y + v2_pos * barycentrics.z;
  const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space

  // Computing the normal at hit position

  vec3 v0_nrm = vec3(v0.positionXYZ_normalX.w, v0.normalYZ_texCoordUV.x, v0.normalYZ_texCoordUV.y);
  vec3 v1_nrm = vec3(v1.positionXYZ_normalX.w, v1.normalYZ_texCoordUV.x, v1.normalYZ_texCoordUV.y);
  vec3 v2_nrm = vec3(v2.positionXYZ_normalX.w, v2.normalYZ_texCoordUV.x, v2.normalYZ_texCoordUV.y); 

  const vec3 nrm      = v0_nrm * barycentrics.x + v1_nrm * barycentrics.y + v2_nrm * barycentrics.z;
  vec3 worldNorm = normalize(vec3(gl_WorldToObjectEXT * vec4(nrm, 1.0)));  // Transforming the normal to world space

  vec3 edge1 = v1_pos - v0_pos;
  vec3 edge2 = v2_pos - v0_pos;
  vec2 deltaUV1 = uv1 - uv0;
  vec2 deltaUV2 = uv2 - uv0;

  float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

  vec3 tangent = vec3(0., 0., 0.);
  tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
  tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
  tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
  tangent = normalize(tangent);
  vec3 worldTangent = normalize(vec3(gl_WorldToObjectEXT * vec4(tangent, 1.0)));

  vec3 bitangent = vec3(0., 0., 0.);
  bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
  bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
  bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
  bitangent = normalize(bitangent); 
  vec3 worldBitangent = normalize(vec3(gl_WorldToObjectEXT * vec4(bitangent, 1.0)));

  mat3 TBN = mat3(worldTangent, worldBitangent, worldNorm);

  indirectRpl.albedo_metalness.xyz = texture(texSet[shadeData.diffuseTexIndex], texCoord).rgb;

  indirectRpl.emission_roughness.xyz = vec3(0., 0., 0);
  if (shadeData.emissionTexIndex > 0)
      indirectRpl.emission_roughness.xyz = texture(texSet[shadeData.emissionTexIndex], texCoord).rgb;    

  indirectRpl.albedo_metalness.w = 0.;
  if (shadeData.metalnessTexIndex > 0)
      indirectRpl.albedo_metalness.w = 1. - texture(texSet[shadeData.metalnessTexIndex], texCoord).r;  

  indirectRpl.emission_roughness.w = 1.;
  if (shadeData.roughnessTexIndex > 0)
      indirectRpl.emission_roughness.w = texture(texSet[shadeData.roughnessTexIndex], texCoord).g;    

  indirectRpl.normal_ = vec4(worldNorm, 1.);
  if (shadeData.normalTexIndex > 0)
  {
      vec3 normal = texture(texSet[shadeData.normalTexIndex], texCoord).rgb;
      normal = normalize(normal * 2.0 - 1.0);   
      indirectRpl.normal_.xyz = normalize(TBN * normal);
      indirectRpl.normal_.w = 1.;
  } 

	indirectRpl.position_objectID = vec4(worldPos, float(gl_InstanceID));
}
