#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "gi_raytrace.h"

layout(location = 0) rayPayloadInEXT IndirectRayPayload indirectRpl;

void main()
{    
    indirectRpl.worldNormGeometryXYZ_ObjectId = vec4(0.f, 0.f, 0.f, -1.f);
}