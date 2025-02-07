#version 460

#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_EXT_ray_query : enable
#extension GL_GOOGLE_include_directive : enable

#include "ray_queries_func.h"

layout(set = 0, binding = 2) uniform accelerationStructureEXT topLevelAS;

layout (location = 0) in PerVertexData
{
  vec3 wpos;
  vec3 normal;
  vec2 uv;  
  flat uint drawID;
} fragIn;

layout (location = 0) out vec4 outColor;

layout(early_fragment_tests) in;
void main()
{
	const float ao = calculate_ambient_occlusion(fragIn.wpos.xyz, fragIn.normal, topLevelAS);
	const vec4 lighting = vec4(1, 1, 1, 1);
	outColor = lighting * vec4(ao * vec3(1, 1, 1), 1);    
}