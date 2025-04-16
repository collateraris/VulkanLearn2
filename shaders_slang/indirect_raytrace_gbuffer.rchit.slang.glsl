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


#line 69 1
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
};


#line 17 2
layout(std430, binding = 3) readonly buffer StructuredBuffer_SObjectData_std430_t_0 {
    SObjectData_std430_0 _data[];
} objectBuffer_0;
layout(std430, binding = 7) readonly buffer StructuredBuffer_vectorx3Cintx2C4x3E_t_0 {
    ivec4 _data[];
} indices_0[];

#line 48 1
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


#line 22 1
struct IndirectGbufferRayPayload_0
{
    vec4 albedo_metalness_0;
    vec4 emission_roughness_0;
    vec4 normal_0;
    vec4 position_objectID_0;
};


#line 24 2
rayPayloadInEXT IndirectGbufferRayPayload_0 _S1;


#line 3
struct BuiltInTriangleIntersectionAttributes_0
{
    vec2 baryCoord_0;
};


#line 24
hitAttributeEXT BuiltInTriangleIntersectionAttributes_0 _S2;


#line 24
void main()
{
    uint _S3 = ((gl_InstanceCustomIndexEXT));

    uint _S4 = ((gl_PrimitiveID));

#line 28
    uvec4 _S5 = uvec4(indices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S4)]);

#line 34
    float _S6 = _S2.baryCoord_0.x;

#line 34
    float _S7 = _S2.baryCoord_0.y;

#line 34
    float _S8 = 1.0 - _S6 - _S7;

    vec2 uv0_0 = vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.x)].normalYZ_texCoordUV_0.zw;
    vec2 uv1_0 = vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.y)].normalYZ_texCoordUV_0.zw;
    vec2 uv2_0 = vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.z)].normalYZ_texCoordUV_0.zw;


    vec2 texCoord_0 = uv0_0 * _S8 + uv1_0 * _S6 + uv2_0 * _S7;

    vec3 v0_pos_0 = vec3(vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.x)].positionXYZ_normalX_0.x, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.x)].positionXYZ_normalX_0.y, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.x)].positionXYZ_normalX_0.z);
    vec3 v1_pos_0 = vec3(vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.y)].positionXYZ_normalX_0.x, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.y)].positionXYZ_normalX_0.y, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.y)].positionXYZ_normalX_0.z);
    vec3 v2_pos_0 = vec3(vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.z)].positionXYZ_normalX_0.x, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.z)].positionXYZ_normalX_0.y, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.z)].positionXYZ_normalX_0.z);

    vec3 pos_0 = v0_pos_0 * _S8 + v1_pos_0 * _S6 + v2_pos_0 * _S7;
    mat3x4 _S9 = (transpose(gl_ObjectToWorldEXT));

#line 48
    vec3 worldPos_0 = (((mat3x3(_S9[0].xyz, _S9[1].xyz, _S9[2].xyz)) * (pos_0)));

#line 56
    vec3 nrm_0 = vec3(vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.x)].positionXYZ_normalX_0.w, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.x)].normalYZ_texCoordUV_0.x, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.x)].normalYZ_texCoordUV_0.y) * _S8 + vec3(vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.y)].positionXYZ_normalX_0.w, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.y)].normalYZ_texCoordUV_0.x, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.y)].normalYZ_texCoordUV_0.y) * _S6 + vec3(vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.z)].positionXYZ_normalX_0.w, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.z)].normalYZ_texCoordUV_0.x, vertices_0[objectBuffer_0._data[uint(_S3)].meshIndex_0]._data[uint(_S5.z)].normalYZ_texCoordUV_0.y) * _S7;
    mat3x4 _S10 = (transpose(gl_ObjectToWorldEXT));

#line 57
    vec3 worldNorm_0 = normalize((((mat3x3(_S10[0].xyz, _S10[1].xyz, _S10[2].xyz)) * (nrm_0))));

    vec3 edge1_0 = v1_pos_0 - v0_pos_0;
    vec3 edge2_0 = v2_pos_0 - v0_pos_0;
    vec2 deltaUV1_0 = uv1_0 - uv0_0;
    vec2 deltaUV2_0 = uv2_0 - uv0_0;

    float _S11 = deltaUV1_0.x;

#line 64
    float _S12 = deltaUV2_0.y;

#line 64
    float _S13 = deltaUV2_0.x;

#line 64
    float _S14 = deltaUV1_0.y;

#line 64
    float f_0 = 1.0 / (_S11 * _S12 - _S13 * _S14);

    const vec3 _S15 = vec3(0.0, 0.0, 0.0);

#line 66
    vec3 tangent_0 = _S15;
    float _S16 = edge1_0.x;

#line 67
    float _S17 = edge2_0.x;

#line 67
    tangent_0[0] = f_0 * (_S12 * _S16 - _S14 * _S17);
    float _S18 = edge1_0.y;

#line 68
    float _S19 = edge2_0.y;

#line 68
    tangent_0[1] = f_0 * (_S12 * _S18 - _S14 * _S19);
    float _S20 = edge1_0.z;

#line 69
    float _S21 = edge2_0.z;

#line 69
    tangent_0[2] = f_0 * (_S12 * _S20 - _S14 * _S21);
    vec3 _S22 = normalize(tangent_0);

#line 70
    tangent_0 = _S22;
    mat3x4 _S23 = (transpose(gl_ObjectToWorldEXT));

#line 71
    vec3 worldTangent_0 = normalize((((mat3x3(_S23[0].xyz, _S23[1].xyz, _S23[2].xyz)) * (_S22))));

    vec3 bitangent_0 = _S15;
    float _S24 = - _S13;

#line 74
    bitangent_0[0] = f_0 * (_S24 * _S16 + _S11 * _S17);
    bitangent_0[1] = f_0 * (_S24 * _S18 + _S11 * _S19);
    bitangent_0[2] = f_0 * (_S24 * _S20 + _S11 * _S21);
    vec3 _S25 = normalize(bitangent_0);

#line 77
    bitangent_0 = _S25;
    mat3x4 _S26 = (transpose(gl_ObjectToWorldEXT));

    mat3x3 TBN_0 = mat3x3(worldTangent_0, normalize((((mat3x3(_S26[0].xyz, _S26[1].xyz, _S26[2].xyz)) * (_S25)))), worldNorm_0);

    _S1.albedo_metalness_0.xyz = (texture(sampler2D(texSet_0[objectBuffer_0._data[uint(_S3)].diffuseTexIndex_0],linearSampler_0), (texCoord_0))).xyz;

    _S1.emission_roughness_0.xyz = _S15;

#line 84
    int _S27 = objectBuffer_0._data[uint(_S3)].emissionTexIndex_0;
    if((objectBuffer_0._data[uint(_S3)].emissionTexIndex_0) > 0)
    {

#line 86
        _S1.emission_roughness_0.xyz = (texture(sampler2D(texSet_0[_S27],linearSampler_0), (texCoord_0))).xyz;

#line 85
    }


    _S1.albedo_metalness_0[3] = 0.0;

#line 88
    int _S28 = objectBuffer_0._data[uint(_S3)].metalnessTexIndex_0;
    if((objectBuffer_0._data[uint(_S3)].metalnessTexIndex_0) > 0)
    {

#line 90
        _S1.albedo_metalness_0[3] = 1.0 - (texture(sampler2D(texSet_0[_S28],linearSampler_0), (texCoord_0))).x;

#line 89
    }


    _S1.emission_roughness_0[3] = 1.0;

#line 92
    int _S29 = objectBuffer_0._data[uint(_S3)].roughnessTexIndex_0;
    if((objectBuffer_0._data[uint(_S3)].roughnessTexIndex_0) > 0)
    {

#line 94
        _S1.emission_roughness_0[3] = (texture(sampler2D(texSet_0[_S29],linearSampler_0), (texCoord_0))).z;

#line 93
    }


    _S1.normal_0 = vec4(worldNorm_0, 1.0);

#line 96
    int _S30 = objectBuffer_0._data[uint(_S3)].normalTexIndex_0;
    if((objectBuffer_0._data[uint(_S3)].normalTexIndex_0) > 0)
    {


        _S1.normal_0.xyz = normalize((((normalize(vec3((texture(sampler2D(texSet_0[_S30],linearSampler_0), (texCoord_0))).z) * 2.0 - 1.0)) * (TBN_0))));
        _S1.normal_0[3] = 1.0;

#line 97
    }

#line 105
    uint _S31 = ((gl_InstanceCustomIndexEXT));

#line 105
    _S1.position_objectID_0 = vec4(worldPos_0, float(_S31));
    return;
}

