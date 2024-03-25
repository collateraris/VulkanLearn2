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

  const vec3 e0 = v2_pos - v0_pos;
  const vec3 e1 = v1_pos - v0_pos;
  const vec3 e0t = gl_ObjectToWorldEXT * vec4(e0,0);
  const vec3 e1t = gl_ObjectToWorldEXT * vec4(e1,0);  

  vec3 worldNormGeometry = normalize(vec3(cross(e0, e1) * gl_WorldToObjectEXT));

  indirectRpl.positionXYZ_normalShadX = vec4(worldPos, worldNorm.x);
  indirectRpl.normalShadYZ_texCoordUV = vec4(worldNorm.yz, texCoord);
  indirectRpl.worldNormGeometryXYZ_ObjectId = vec4(worldNormGeometry, float(gl_InstanceID));
}
