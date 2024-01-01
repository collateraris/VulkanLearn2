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

layout(set = 1, binding = 1) readonly buffer _Lights{

	SLight lights[];
} lightsBuffer;

layout(set = 1, binding = 2) readonly buffer ObjectBuffer{

	SObjectData objects[];
} objectBuffer;

layout(set = 2, binding = 0) uniform accelerationStructureEXT topLevelAS;

// A wrapper function that encapsulates shooting an ambient occlusion ray query
float shootAmbientOcclusionRay( vec3 orig, vec3 dir, float maxT)
{
	aoRpl.aoValue = 0.;

    uint  rayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;

    traceRayEXT(topLevelAS, // acceleration structure
            rayFlags,       // rayFlags
            0xFF,           // cullMask
            0,              // sbtRecordOffset
            0,              // sbtRecordStride
            1,              // missIndex
            orig.xyz,       // ray origin
            1e-3,           // ray min range
            dir.xyz,         // ray direction
            maxT,           // ray max range
            1      // payload (location = 1)
    );        

	return aoRpl.aoValue;
};

float shadowRayVisibility( vec3 orig, vec3 dir)
{
    return shootAmbientOcclusionRay(orig, dir, 10000000);
};


void main()
{
  SObjectData objData = objectBuffer.objects[gl_InstanceID];
  
  SVertex v0 = Vertices[objData.meshIndex].vertices[gl_PrimitiveID * 3 + 0];
  SVertex v1 = Vertices[objData.meshIndex].vertices[gl_PrimitiveID * 3 + 1];
  SVertex v2 = Vertices[objData.meshIndex].vertices[gl_PrimitiveID * 3 + 2];

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
  const vec3 worldNormal = normalize(vec3(nrm * gl_WorldToObjectEXT));  // Transforming the normal to world space      

  vec3 diffuse = texture(texSet[objData.diffuseTexIndex], texCoord).xyz;

  SLight sunInfo = lightsBuffer.lights[0];
  vec3 toLight = normalize(-sunInfo.direction.xyz);

  // Compute our lambertion term (L dot N)
	float LdotN = clamp(dot(worldNormal, toLight), 0., 1.);
  float shadowMult = shadowRayVisibility(worldPos.xyz, toLight);

  indirectRpl.color = shadowMult * LdotN * diffuse * sunInfo.color.xyz * M_INV_PI;
}
