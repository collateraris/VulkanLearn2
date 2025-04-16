#version 460
#extension GL_EXT_ray_tracing : require
layout(row_major) uniform;
layout(row_major) buffer;

#line 22 0
struct IndirectGbufferRayPayload_0
{
    vec4 albedo_metalness_0;
    vec4 emission_roughness_0;
    vec4 normal_0;
    vec4 position_objectID_0;
};


#line 4 1
rayPayloadInEXT IndirectGbufferRayPayload_0 _S1;


#line 4
void main()
{
    _S1.position_objectID_0 = vec4(0.0, 0.0, 0.0, -1.0);
    return;
}

