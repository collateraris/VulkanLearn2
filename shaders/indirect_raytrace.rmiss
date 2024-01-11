#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "gi_raytrace.h"

layout(location = 1) rayPayloadInEXT IndirectRayPayload indirectRpl;

void main()
{    
    indirectRpl.objectId = -1;
}