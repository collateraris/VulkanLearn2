#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "gi_raytrace.h"

layout(location = 0) rayPayloadInEXT  IndirectRayPayload indirectRpl;
layout(location = 1) rayPayloadEXT AORayPayload aoRpl;

hitAttributeEXT vec2 baryCoord;


layout (set = 0, std140, binding = 0) readonly buffer _vertices
{
	SVertex vertices[];
} Vertices[];

layout(set = 0, binding = 1) uniform sampler2D texSet[];

layout(set = 0, binding = 2) uniform accelerationStructureEXT topLevelAS;

layout(set = 1, binding = 0) uniform _GlobalGIParams { SGlobalGIParams giParams; };

layout(set = 1, binding = 1) readonly buffer _Lights{

	SLight lights[];
} lightsBuffer;

layout(set = 1, binding = 2) readonly buffer ObjectBuffer{

	SObjectData objects[];
} objectBuffer;

#include "gi_raytrace_func.h"

void main()
{
  SObjectData shadeData = objectBuffer.objects[gl_InstanceID];
  
  SVertex v0 = Vertices[shadeData.meshIndex].vertices[gl_PrimitiveID * 3 + 0];
  SVertex v1 = Vertices[shadeData.meshIndex].vertices[gl_PrimitiveID * 3 + 1];
  SVertex v2 = Vertices[shadeData.meshIndex].vertices[gl_PrimitiveID * 3 + 2];

  const vec3 barycentrics = vec3(1.0 - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);

  vec2 texCoord =
        v0.normalYZ_texCoordUV.zw * barycentrics.x + v1.normalYZ_texCoordUV.zw * barycentrics.y + v2.normalYZ_texCoordUV.zw * barycentrics.z;

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
  vec3 worldNorm = normalize(vec3(nrm * gl_WorldToObjectEXT));  // Transforming the normal to world space      

  vec3 albedo = texture(texSet[shadeData.diffuseTexIndex], texCoord).xyz;

  vec3 emission = vec3(0., 0., 0);
  if (shadeData.emissionTexIndex > 0)
    emission = texture(texSet[shadeData.emissionTexIndex], texCoord).rgb;

  float metalness = 0.;
  if (shadeData.metalnessTexIndex > 0)
      metalness = 1. - texture(texSet[shadeData.metalnessTexIndex], texCoord).r;   

  float roughness = 1.;
  if (shadeData.roughnessTexIndex > 0)
    roughness = texture(texSet[shadeData.roughnessTexIndex], texCoord).r; 

  if (shadeData.normalTexIndex > 0)
  {
      mat3 TBN = getTBN(worldNorm);
      vec3 normal = texture(texSet[shadeData.normalTexIndex], texCoord).rgb;
      normal = normalize(normal * 2.0 - 1.0);   
      worldNorm = normalize(TBN * normal);
  } 

  SLight sunInfo = lightsBuffer.lights[0];

  vec3 shadeColor = emission;

  vec3 lightDir = normalize(-sunInfo.direction.xyz);
	vec3 viewDir = normalize(giParams.camPos.xyz - worldPos.xyz);
  vec3 F0 = vec3(0.04); 

  indirectRpl.color = shadeColor + ggxDirect(indirectRpl.randSeed, shadeData, texCoord, worldPos.xyz, worldNorm.xyz, giParams.camPos.xyz, albedo, roughness, metalness, lightDir, viewDir, sunInfo.color.xyz, F0);
}
