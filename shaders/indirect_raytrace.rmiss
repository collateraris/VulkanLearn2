#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "gi_raytrace.h"

layout(location = 2) rayPayloadEXT IndirectRayPayload indirectRpl;

void main()
{
	// Our ambient occlusion value is 1 if we hit nothing.    
    indirectRpl.color = vec3(0., 0., 0.);
}