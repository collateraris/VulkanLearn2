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


#line 115 1
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

#line 93 1
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
layout(binding = 11)
uniform sampler linearSampler_0;


#line 45 1
vec2 octWrap_0(vec2 v_0)
{
    float _S1 = v_0.y;

#line 47
    float _S2 = 1.0 - abs(_S1);

#line 47
    float _S3 = v_0.x;

#line 47
    float _S4;

#line 47
    if(_S3 >= 0.0)
    {

#line 47
        _S4 = 1.0;

#line 47
    }
    else
    {

#line 47
        _S4 = -1.0;

#line 47
    }

#line 47
    float _S5 = _S2 * _S4;

#line 47
    float _S6 = 1.0 - abs(_S3);

#line 47
    if(_S1 >= 0.0)
    {

#line 47
        _S4 = 1.0;

#line 47
    }
    else
    {

#line 47
        _S4 = -1.0;

#line 47
    }

#line 47
    return vec2(_S5, _S6 * _S4);
}

vec2 encodeNormalOctahedron_0(vec3 n_0)
{
    float _S7 = n_0.x;

#line 52
    float _S8 = n_0.y;

#line 52
    float _S9 = n_0.z;

#line 52
    vec2 p_0 = vec2(_S7, _S8) * (1.0 / (abs(_S7) + abs(_S8) + abs(_S9)));

#line 52
    vec2 p_1;
    if(_S9 < 0.0)
    {

#line 53
        p_1 = octWrap_0(p_0);

#line 53
    }
    else
    {

#line 53
        p_1 = p_0;

#line 53
    }
    return p_1;
}


#line 66
vec4 encodeNormals_0(vec3 geometryNormal_0, vec3 shadingNormal_0)
{

#line 67
    return vec4(encodeNormalOctahedron_0(geometryNormal_0), encodeNormalOctahedron_0(shadingNormal_0));
}


#line 26
struct IndirectGbufferRayPayload_0
{
    vec4 albedo_metalness_0;
    vec4 emission_roughness_0;
    vec4 normal_0;
    vec4 position_objectID_0;
};


#line 24 2
rayPayloadInEXT IndirectGbufferRayPayload_0 _S10;


#line 3
struct BuiltInTriangleIntersectionAttributes_0
{
    vec2 baryCoord_0;
};


#line 24
hitAttributeEXT BuiltInTriangleIntersectionAttributes_0 _S11;


#line 24
void main()
{
    uint _S12 = ((gl_InstanceCustomIndexEXT));

    uint _S13 = ((gl_PrimitiveID));

#line 28
    uvec4 _S14 = uvec4(indices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S13)]);

#line 34
    float _S15 = _S11.baryCoord_0.x;

#line 34
    float _S16 = _S11.baryCoord_0.y;

#line 34
    float _S17 = 1.0 - _S15 - _S16;

    vec2 uv0_0 = vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.x)].normalYZ_texCoordUV_0.zw;
    vec2 uv1_0 = vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.y)].normalYZ_texCoordUV_0.zw;
    vec2 uv2_0 = vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.z)].normalYZ_texCoordUV_0.zw;


    vec2 texCoord_0 = uv0_0 * _S17 + uv1_0 * _S15 + uv2_0 * _S16;

    vec3 v0_pos_0 = vec3(vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.x)].positionXYZ_normalX_0.x, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.x)].positionXYZ_normalX_0.y, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.x)].positionXYZ_normalX_0.z);
    vec3 v1_pos_0 = vec3(vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.y)].positionXYZ_normalX_0.x, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.y)].positionXYZ_normalX_0.y, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.y)].positionXYZ_normalX_0.z);
    vec3 v2_pos_0 = vec3(vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.z)].positionXYZ_normalX_0.x, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.z)].positionXYZ_normalX_0.y, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.z)].positionXYZ_normalX_0.z);

    vec3 pos_0 = v0_pos_0 * _S17 + v1_pos_0 * _S15 + v2_pos_0 * _S16;
    mat3x4 _S18 = (transpose(gl_ObjectToWorldEXT));

#line 48
    vec3 worldPos_0 = (((vec4(pos_0, 0.0)) * (_S18)));

#line 56
    vec3 nrm_0 = vec3(vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.x)].positionXYZ_normalX_0.w, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.x)].normalYZ_texCoordUV_0.x, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.x)].normalYZ_texCoordUV_0.y) * _S17 + vec3(vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.y)].positionXYZ_normalX_0.w, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.y)].normalYZ_texCoordUV_0.x, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.y)].normalYZ_texCoordUV_0.y) * _S15 + vec3(vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.z)].positionXYZ_normalX_0.w, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.z)].normalYZ_texCoordUV_0.x, vertices_0[objectBuffer_0._data[uint(_S12)].meshIndex_0]._data[uint(_S14.z)].normalYZ_texCoordUV_0.y) * _S16;
    mat3x4 _S19 = (transpose(gl_ObjectToWorldEXT));

#line 57
    vec3 worldNorm_0 = normalize((((vec4(nrm_0, 0.0)) * (_S19))));

    vec3 edge1_0 = v1_pos_0 - v0_pos_0;
    vec3 edge2_0 = v2_pos_0 - v0_pos_0;
    vec2 deltaUV1_0 = uv1_0 - uv0_0;
    vec2 deltaUV2_0 = uv2_0 - uv0_0;

    float _S20 = deltaUV1_0.x;

#line 64
    float _S21 = deltaUV2_0.y;

#line 64
    float _S22 = deltaUV2_0.x;

#line 64
    float _S23 = deltaUV1_0.y;

#line 64
    float f_0 = 1.0 / (_S20 * _S21 - _S22 * _S23);

    const vec3 _S24 = vec3(0.0, 0.0, 0.0);

#line 66
    vec3 tangent_0 = _S24;
    float _S25 = edge1_0.x;

#line 67
    float _S26 = edge2_0.x;

#line 67
    tangent_0[0] = f_0 * (_S21 * _S25 - _S23 * _S26);
    float _S27 = edge1_0.y;

#line 68
    float _S28 = edge2_0.y;

#line 68
    tangent_0[1] = f_0 * (_S21 * _S27 - _S23 * _S28);
    float _S29 = edge1_0.z;

#line 69
    float _S30 = edge2_0.z;

#line 69
    tangent_0[2] = f_0 * (_S21 * _S29 - _S23 * _S30);
    tangent_0 = normalize(tangent_0);
    mat3x4 _S31 = (transpose(gl_ObjectToWorldEXT));

    vec3 bitangent_0 = _S24;
    float _S32 = - _S22;

#line 74
    bitangent_0[0] = f_0 * (_S32 * _S25 + _S20 * _S26);
    bitangent_0[1] = f_0 * (_S32 * _S27 + _S20 * _S28);
    bitangent_0[2] = f_0 * (_S32 * _S29 + _S20 * _S30);
    bitangent_0 = normalize(bitangent_0);
    mat3x4 _S33 = (transpose(gl_ObjectToWorldEXT));



    _S10.albedo_metalness_0.xyz = (texture(sampler2D(texSet_0[objectBuffer_0._data[uint(_S12)].diffuseTexIndex_0],linearSampler_0), (texCoord_0))).xyz;


    _S10.emission_roughness_0.xyz = _S24;

#line 85
    int _S34 = objectBuffer_0._data[uint(_S12)].emissionTexIndex_0;
    if((objectBuffer_0._data[uint(_S12)].emissionTexIndex_0) > 0)
    {

#line 87
        _S10.emission_roughness_0.xyz = (texture(sampler2D(texSet_0[_S34],linearSampler_0), (texCoord_0))).xyz;

#line 86
    }


    _S10.albedo_metalness_0[3] = 0.0;

#line 89
    int _S35 = objectBuffer_0._data[uint(_S12)].metalnessTexIndex_0;
    if((objectBuffer_0._data[uint(_S12)].metalnessTexIndex_0) > 0)
    {

#line 91
        _S10.albedo_metalness_0[3] = 1.0 - (texture(sampler2D(texSet_0[_S35],linearSampler_0), (texCoord_0))).x;

#line 90
    }


    _S10.emission_roughness_0[3] = 1.0;

#line 93
    int _S36 = objectBuffer_0._data[uint(_S12)].roughnessTexIndex_0;
    if((objectBuffer_0._data[uint(_S12)].roughnessTexIndex_0) > 0)
    {

#line 95
        _S10.emission_roughness_0[3] = (texture(sampler2D(texSet_0[_S36],linearSampler_0), (texCoord_0))).z;

#line 94
    }

#line 105
    _S10.normal_0 = encodeNormals_0(worldNorm_0, worldNorm_0);
    uint _S37 = ((gl_InstanceCustomIndexEXT));

#line 106
    _S10.position_objectID_0 = vec4(worldPos_0, float(_S37));
    return;
}

