#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
layout(row_major) uniform;
layout(row_major) buffer;

#line 553 0
struct _MatrixStorage_float4x4_ColMajorstd430_0
{
    vec4  data_0[4];
};


#line 118 1
struct SObjectData_std430_0
{
    _MatrixStorage_float4x4_ColMajorstd430_0 model_0;
    uint meshIndex_0;
    uint meshletCount_0;
    uint diffuseTexIndex_0;
    int normalTexIndex_0;
    int metalnessTexIndex_0;
    int roughnessTexIndex_0;
    int emissionTexIndex_0;
    int opacityTexIndex_0;
    vec4 baseColorFactor_0;
};


#line 17 2
layout(std430, binding = 3) readonly buffer StructuredBuffer_SObjectData_std430_t_0 {
    SObjectData_std430_0 _data[];
} objectBuffer_0;
layout(std430, binding = 7) readonly buffer StructuredBuffer_vectorx3Cintx2C4x3E_t_0 {
    ivec4 _data[];
} indices_0[];

#line 96 1
struct SVertex_std430_0
{
    vec4 positionXYZ_normalX_0;
    vec4 normalYZ_texCoordUV_0;
};


#line 9 2
layout(std430, binding = 0) readonly buffer StructuredBuffer_SVertex_std430_t_0 {
    SVertex_std430_0 _data[];
} vertices_0[];
layout(binding = 1)
uniform texture2D  texSet_0[];


#line 14
layout(binding = 8)
uniform sampler linearSampler_0;


#line 26 1
struct IndirectGbufferRayPayload_0
{
    vec4 albedo_metalness_0;
    vec4 emission_roughness_0;
    vec4 normal_0;
    vec4 position_objectID_0;
};


#line 23 2
rayPayloadInEXT IndirectGbufferRayPayload_0 _S1;


#line 3
struct BuiltInTriangleIntersectionAttributes_0
{
    vec2 baryCoord_0;
};


#line 23
hitAttributeEXT BuiltInTriangleIntersectionAttributes_0 _S2;


#line 23
void main()
{
    uint _S3 = ((gl_InstanceCustomIndexEXT));

    uint _S4 = ((gl_PrimitiveID));

#line 27
    uvec4 _S5 = uvec4(indices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S4)]);

#line 33
    float _S6 = _S2.baryCoord_0.x;

#line 33
    float _S7 = _S2.baryCoord_0.y;

#line 39
    vec2 texCoord_0 = vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.x)].normalYZ_texCoordUV_0.zw * (1.0 - _S6 - _S7) + vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.y)].normalYZ_texCoordUV_0.zw * _S6 + vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.z)].normalYZ_texCoordUV_0.zw * _S7;



    float opacity_0 = (texture(sampler2D(texSet_0[objectBuffer_0._data[uint(_S3)].diffuseTexIndex_0],linearSampler_0), (texCoord_0))).w * objectBuffer_0._data[uint(_S3)].baseColorFactor_0.w;

#line 43
    int _S8 = objectBuffer_0._data[uint(_S3)].opacityTexIndex_0;

#line 43
    float opacity_1;

    if((objectBuffer_0._data[uint(_S3)].opacityTexIndex_0) > 0)
    {

#line 45
        opacity_1 = (texture(sampler2D(texSet_0[_S8],linearSampler_0), (texCoord_0))).x;

#line 45
    }
    else
    {

#line 45
        opacity_1 = opacity_0;

#line 45
    }


    if(opacity_1 < 0.5)
    {

#line 49
        ignoreIntersectionEXT;;

#line 48
    }

    return;
}

