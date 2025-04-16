#version 460
#extension GL_EXT_ray_tracing : require
layout(row_major) uniform;
layout(row_major) buffer;

#line 553 0
struct _MatrixStorage_float4x4_ColMajorstd140_0
{
    vec4  data_0[4];
};


#line 19 1
struct SGlobalGIParams_std140_0
{
    _MatrixStorage_float4x4_ColMajorstd140_0 projView_0;
    _MatrixStorage_float4x4_ColMajorstd140_0 viewInverse_0;
    _MatrixStorage_float4x4_ColMajorstd140_0 projInverse_0;
    _MatrixStorage_float4x4_ColMajorstd140_0 prevProjView_0;
    vec4 camPos_0;
    uint frameCount_0;
    float shadowMult_0;
    uint lightsCount_0;
    uint numRays_0;
    uint mode_0;
    uint pad1_0;
    uint pad2_0;
    uint pad3_0;
};


#line 19
struct SLANG_ParameterGroup_MyContantBuffer_std140_0
{
    SGlobalGIParams_std140_0 giParams_0;
};


#line 13 2
layout(binding = 0, set = 1)
layout(std140) uniform block_SLANG_ParameterGroup_MyContantBuffer_std140_0
{
    SGlobalGIParams_std140_0 giParams_0;
}MyContantBuffer_0;

#line 7
layout(binding = 2)
uniform accelerationStructureEXT topLevelAS_0;


#line 47 1
struct SLight_std430_0
{
    vec4 position_0;
    vec4 direction_0;
    vec4 color_type_0;
};


#line 10 2
layout(std430, binding = 6) readonly buffer StructuredBuffer_SLight_std430_t_0 {
    SLight_std430_0 _data[];
} lightsBuffer_0;

#line 19
layout(rgba32f)
layout(binding = 0, set = 2)
uniform image2D ptOutput_0;


#line 35 1
struct ShadowHitInfo_0
{
    bool hasHit_0;
};


#line 16823 0
layout(location = 0)
rayPayloadEXT
ShadowHitInfo_0 p_0;


#line 22 1
struct IndirectGbufferRayPayload_0
{
    vec4 albedo_metalness_0;
    vec4 emission_roughness_0;
    vec4 normal_0;
    vec4 position_objectID_0;
};


#line 16823 0
layout(location = 1)
rayPayloadEXT
IndirectGbufferRayPayload_0 p_1;


#line 16581
struct RayDesc_0
{
    vec3 Origin_0;
    float TMin_0;
    vec3 Direction_0;
    float TMax_0;
};


#line 22 2
IndirectGbufferRayPayload_0 shootRay_0(vec3 orig_0, vec3 dir_0)
{

    RayDesc_0 ray_0;
    ray_0.Origin_0 = orig_0;
    ray_0.Direction_0 = dir_0;
    ray_0.TMin_0 = 0.0;
    ray_0.TMax_0 = 3.4028234663852886e+38;

    RayDesc_0 _S1 = ray_0;

#line 31
    traceRayEXT(topLevelAS_0, 0U, 255U, 0U, 0U, 0U, _S1.Origin_0, _S1.TMin_0, _S1.Direction_0, _S1.TMax_0, 1);

#line 42
    return p_1;
}


#line 29 1
bool IndirectGbufferRayPayload_hasHit_0(IndirectGbufferRayPayload_0 this_0)
{
    return (this_0.position_objectID_0.w) > 0.00100000004749745;
}


#line 43
struct SLight_0
{
    vec4 position_0;
    vec4 direction_0;
    vec4 color_type_0;
};


#line 103
void getLightData_0(SLight_0 light_0, vec3 hitPosition_0, out vec3 lightVector_0, out float lightDistance_0)
{

#line 104
    float _S2 = light_0.color_type_0.w;

#line 104
    if((abs(_S2 - 2.0)) < 9.99999997475242708e-07)
    {

#line 105
        vec3 _S3 = light_0.position_0.xyz - hitPosition_0;

#line 105
        lightVector_0 = _S3;
        lightDistance_0 = length(_S3);

#line 104
    }
    else
    {
        if((abs(_S2 - 1.0)) < 9.99999997475242708e-07)
        {

#line 108
            lightVector_0 = light_0.direction_0.xyz;
            lightDistance_0 = 3.4028234663852886e+38;

#line 107
        }
        else
        {

            lightDistance_0 = 3.4028234663852886e+38;
            lightVector_0 = vec3(0.0, 1.0, 0.0);

#line 107
        }

#line 104
    }

#line 114
    return;
}


#line 84
vec3 offsetRay_0(vec3 p_2, vec3 n_0)
{

#line 90
    float _S4 = n_0.x;

#line 90
    int _S5 = int(256.0 * _S4);

#line 90
    float _S6 = n_0.y;

#line 90
    int _S7 = int(256.0 * _S6);

#line 90
    float _S8 = n_0.z;

#line 90
    int _S9 = int(256.0 * _S8);


    float _S10 = p_2.x;

#line 93
    int _S11 = floatBitsToInt(_S10);

#line 93
    int _S12;

#line 93
    if(_S10 < 0.0)
    {

#line 93
        _S12 = - _S5;

#line 93
    }
    else
    {

#line 93
        _S12 = _S5;

#line 93
    }

#line 93
    float _S13 = intBitsToFloat(_S11 + _S12);
    float _S14 = p_2.y;

#line 94
    int _S15 = floatBitsToInt(_S14);

#line 94
    if(_S14 < 0.0)
    {

#line 94
        _S12 = - _S7;

#line 94
    }
    else
    {

#line 94
        _S12 = _S7;

#line 94
    }

#line 94
    float _S16 = intBitsToFloat(_S15 + _S12);
    float _S17 = p_2.z;

#line 95
    int _S18 = floatBitsToInt(_S17);

#line 95
    if(_S17 < 0.0)
    {

#line 95
        _S12 = - _S9;

#line 95
    }
    else
    {

#line 95
        _S12 = _S9;

#line 95
    }

#line 95
    float _S19 = intBitsToFloat(_S18 + _S12);

#line 95
    float _S20;

    if((abs(_S10)) < 0.03125)
    {

#line 97
        _S20 = _S10 + 0.0000152587890625 * _S4;

#line 97
    }
    else
    {

#line 97
        _S20 = _S13;

#line 97
    }

#line 97
    float _S21;
    if((abs(_S14)) < 0.03125)
    {

#line 98
        _S21 = _S14 + 0.0000152587890625 * _S6;

#line 98
    }
    else
    {

#line 98
        _S21 = _S16;

#line 98
    }

#line 98
    float _S22;
    if((abs(_S17)) < 0.03125)
    {

#line 99
        _S22 = _S17 + 0.0000152587890625 * _S8;

#line 99
    }
    else
    {

#line 99
        _S22 = _S19;

#line 99
    }

#line 97
    return vec3(_S20, _S21, _S22);
}


#line 47 2
bool castShadowRay_0(vec3 hitPosition_1, vec3 surfaceNormal_0, vec3 directionToLight_0, float TMax_1)
{
    RayDesc_0 ray_1;
    ray_1.Origin_0 = offsetRay_0(hitPosition_1, surfaceNormal_0);
    ray_1.Direction_0 = directionToLight_0;
    ray_1.TMin_0 = 0.0;
    ray_1.TMax_0 = TMax_1;

    ShadowHitInfo_0 payload_0;
    payload_0.hasHit_0 = true;


    RayDesc_0 _S23 = ray_1;

#line 59
    p_0 = payload_0;

#line 59
    traceRayEXT(topLevelAS_0, 12U, 255U, 1U, 0U, 1U, _S23.Origin_0, _S23.TMin_0, _S23.Direction_0, _S23.TMax_0, 0);

#line 59
    payload_0 = p_0;

#line 69
    return !payload_0.hasHit_0;
}



void main()
{
    uvec3 _S24 = ((gl_LaunchIDEXT));

#line 76
    uvec2 LaunchIndex_0 = _S24.xy;
    uvec3 _S25 = ((gl_LaunchSizeEXT));


    vec2 d_0 = (vec2(LaunchIndex_0) + vec2(0.5)) / vec2(_S25.xy) * 2.0 - 1.0;

    vec4 _S26 = (((vec4(0.0, 0.0, 0.0, 1.0)) * (mat4x4(MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][3]))));

    vec4 _S27 = (((vec4(normalize((((vec4(d_0.x, d_0.y, 1.0, 1.0)) * (mat4x4(MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][3], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][3], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][3], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][3])))).xyz), 0.0)) * (mat4x4(MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][3]))));


    const vec3 _S28 = vec3(0.0, 0.0, 0.0);

#line 87
    int bounce_0 = 0;

#line 87
    vec3 radiance_0 = _S28;


    for(;;)
    {

#line 90
        if(uint(bounce_0) < (MyContantBuffer_0.giParams_0.numRays_0))
        {
        }
        else
        {

#line 90
            break;
        }
        IndirectGbufferRayPayload_0 payload_1 = shootRay_0(_S26.xyz, _S27.xyz);

        if(!IndirectGbufferRayPayload_hasHit_0(payload_1))
        {

            break;
        }

#line 97
        SLight_0 _S29 = { lightsBuffer_0._data[uint(0)].position_0, lightsBuffer_0._data[uint(0)].direction_0, lightsBuffer_0._data[uint(0)].color_type_0 };

#line 103
        vec3 _S30 = payload_1.position_objectID_0.xyz;

#line 101
        vec3 lightVector_1;
        float lightDistance_1;
        getLightData_0(_S29, _S30, lightVector_1, lightDistance_1);



        bool _S31 = castShadowRay_0(_S30, payload_1.normal_0.xyz, - normalize(lightVector_1), lightDistance_1);

#line 107
        float k_0;

#line 107
        if(_S31)
        {

#line 107
            k_0 = 3.0;

#line 107
        }
        else
        {

#line 107
            k_0 = 0.05000000074505806;

#line 107
        }


        vec3 radiance_1 = radiance_0 + k_0 * payload_1.albedo_metalness_0.xyz / 3.14159274101257324;

#line 90
        bounce_0 = bounce_0 + 1;

#line 90
        radiance_0 = radiance_1;

#line 90
    }

#line 113
    imageStore((ptOutput_0), (ivec2(LaunchIndex_0)), vec4(radiance_0, 1.0));
    return;
}

