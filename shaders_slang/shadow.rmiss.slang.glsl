#version 460
#extension GL_EXT_ray_tracing : require
layout(row_major) uniform;
layout(row_major) buffer;

#line 70 0
struct ShadowHitInfo_0
{
    bool hasHit_0;
};


#line 4 1
rayPayloadInEXT ShadowHitInfo_0 _S1;


#line 4
void main()
{

    _S1.hasHit_0 = false;
    return;
}

