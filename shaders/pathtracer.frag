#version 460

#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_EXT_ray_query : enable
#extension GL_GOOGLE_include_directive : enable

#include "gi_raytrace.h"

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

layout (location = 0) in PerVertexData
{
  vec3 wpos;
  vec3 normal;
  vec2 uv;  
  flat uint drawID;
} fragIn;

layout (location = 0) out vec4 outColor;

#include "ray_queries_func.h"

layout(set = 1, binding = 1) uniform _GlobalRQParams { SGlobalRQParams rqParams; };

layout(early_fragment_tests) in;
void main()
{	
	float w = rqParams.width_height_fov_frameIndex.x;
	float h = rqParams.width_height_fov_frameIndex.y;
	float fov = rqParams.width_height_fov_frameIndex.z;
	uint frame_index = uint(rqParams.width_height_fov_frameIndex.w);
	uvec2 seed = uvec2(gl_FragCoord) ^ uvec2(frame_index << 16, (frame_index + 237) << 16);

	const float std = 0.9;
	vec2 randoms = 2.0 * get_random_numbers(seed) - vec2(1.0);
	vec2 jitter = (std * sqrt(2.0)) * vec2(erfinv(randoms.x), erfinv(randoms.y));
	vec2 jittered = gl_FragCoord.xy + jitter;
	// Compute the primary ray using either a pinhole camera or a hemispherical
	// camera
	vec2 ray_tex_coord = jittered / (rqParams.width_height_fov_frameIndex.xy);
	vec3 ray_origin = get_camera_ray_origin(ray_tex_coord, rqParams.proj_to_world_space);
	vec3 ray_dir = get_camera_ray_direction(ray_tex_coord, rqParams.world_to_proj_space);

	const float ao = calculateAmbientOcclusion(fragIn.wpos.xyz, fragIn.normal);
	vec4 ray_radiance = vec4(pathTraceBrdf(ray_origin, ray_dir, seed, 15), 1.);
	outColor = ray_radiance * vec4(ao * vec3(1, 1, 1), 1);
	//outColor = vec4(ao * vec3(1, 1, 1), 1);    
}