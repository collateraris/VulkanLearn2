#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "gi_raytrace.h"

layout(location = 1) rayPayloadInEXT AORayPayload aoRpl;

void main()
{
	// Our ambient occlusion value is 1 if we hit nothing.    
    aoRpl.aoValue = 1.0f;
}