#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
layout(row_major) uniform;
layout(row_major) buffer;

#line 548 0
struct _MatrixStorage_float4x4_ColMajorstd140_0
{
    vec4  data_0[4];
};


#line 23 1
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
    uint enableAccumulation_0;
    uint pad2_0;
    uint pad3_0;
};


#line 23
struct SLANG_ParameterGroup_MyContantBuffer_std140_0
{
    SGlobalGIParams_std140_0 giParams_0;
};


#line 26 2
layout(binding = 0, set = 1)
layout(std140) uniform block_SLANG_ParameterGroup_MyContantBuffer_std140_0
{
    SGlobalGIParams_std140_0 giParams_0;
}MyContantBuffer_0;

#line 17
layout(binding = 2)
uniform accelerationStructureEXT topLevelAS_0;


#line 92 1
struct SLight_std430_0
{
    vec4 position_0;
    vec4 direction_0;
    vec4 color_type_0;
    vec4 position1_0;
    vec4 position2_0;
    vec4 uv0_uv1_0;
    vec4 uv2_objectId_0;
};


#line 20 2
layout(std430, binding = 4) readonly buffer StructuredBuffer_SLight_std430_t_0 {
    SLight_std430_0 _data[];
} lightsBuffer_0;

#line 132 1
struct SMaterialData_std430_0
{
    uint diffuseTexIndex_0;
    int normalTexIndex_0;
    int metalnessTexIndex_0;
    int roughnessTexIndex_0;
    int emissionTexIndex_0;
    int opacityTexIndex_0;
    int pad0_0;
    int pad1_0;
    vec4 baseColorFactor_0;
    vec4 emissiveFactorMult_emissiveStrength_0;
    vec4 metallicFactor_roughnessFactor_0;
};


#line 23 2
layout(std430, binding = 7) readonly buffer StructuredBuffer_SMaterialData_std430_t_0 {
    SMaterialData_std430_0 _data[];
} matBuffer_0;

#line 11
layout(binding = 1)
uniform texture2D  texSet_0[];

layout(binding = 8)
uniform sampler linearSampler_0;


#line 32
layout(rgba32f)
layout(binding = 0, set = 2)
uniform image2D ptOutput_0;


#line 35
layout(rgba32f)
layout(binding = 1, set = 2)
uniform image2D ptAlbedoMetalnessOutput_0;


#line 38
layout(rgba32f)
layout(binding = 2, set = 2)
uniform image2D ptEmissionRoughnessOutput_0;


#line 41
layout(rgba32f)
layout(binding = 3, set = 2)
uniform image2D ptNormalOutput_0;


#line 44
layout(rgba32f)
layout(binding = 4, set = 2)
uniform image2D ptWposObjectIdOutput_0;


#line 75 1
struct ShadowHitInfo_0
{
    bool hasHit_0;
};


#line 16545 0
layout(location = 0)
rayPayloadEXT
ShadowHitInfo_0 p_0;


#line 26 1
struct IndirectGbufferRayPayload_0
{
    vec4 albedo_metalness_0;
    vec4 emission_roughness_0;
    vec4 normal_0;
    vec4 position_objectID_0;
};


#line 16545 0
layout(location = 1)
rayPayloadEXT
IndirectGbufferRayPayload_0 p_1;


#line 16545
mat4x4 unpackStorage_0(_MatrixStorage_float4x4_ColMajorstd140_0 _S1)
{

#line 16545
    return mat4x4(_S1.data_0[0][0], _S1.data_0[1][0], _S1.data_0[2][0], _S1.data_0[3][0], _S1.data_0[0][1], _S1.data_0[1][1], _S1.data_0[2][1], _S1.data_0[3][1], _S1.data_0[0][2], _S1.data_0[1][2], _S1.data_0[2][2], _S1.data_0[3][2], _S1.data_0[0][3], _S1.data_0[1][3], _S1.data_0[2][3], _S1.data_0[3][3]);
}


#line 193 1
uvec4 initRNG_0(uvec2 pixelCoords_0, uvec2 resolution_0, uint frameNumber_0)
{

#line 194
    return uvec4(pixelCoords_0.xy, frameNumber_0, 0U);
}


#line 16303 0
struct RayDesc_0
{
    vec3 Origin_0;
    float TMin_0;
    vec3 Direction_0;
    float TMax_0;
};


#line 47 2
IndirectGbufferRayPayload_0 shootRay_0(vec3 orig_0, vec3 dir_0, float max_0)
{

    RayDesc_0 ray_0;
    ray_0.Origin_0 = orig_0;
    ray_0.Direction_0 = dir_0;
    ray_0.TMin_0 = 0.0;
    ray_0.TMax_0 = max_0;

    RayDesc_0 _S2 = ray_0;

#line 56
    traceRayEXT(topLevelAS_0, 0U, 255U, 0U, 0U, 0U, _S2.Origin_0, _S2.TMin_0, _S2.Direction_0, _S2.TMax_0, 1);

#line 67
    return p_1;
}


#line 33 1
bool IndirectGbufferRayPayload_hasHit_0(IndirectGbufferRayPayload_0 this_0)
{
    return (this_0.position_objectID_0.w) > 0.00100000004749745;
}


#line 171 3
struct MaterialProperties_0
{
    vec3 baseColor_0;
    float metalness_0;
    vec3 emissive_0;
    float roughness_0;
    float transmissivness_0;
    float opacity_0;
};


#line 97 2
MaterialProperties_0 loadMaterialProperties_0(IndirectGbufferRayPayload_0 payload_0)
{
    const vec3 _S3 = vec3(0.0, 0.0, 0.0);

#line 99
    MaterialProperties_0 result_0;

#line 99
    result_0.baseColor_0 = _S3;

#line 99
    result_0.metalness_0 = 0.0;

#line 99
    result_0.emissive_0 = _S3;

#line 99
    result_0.roughness_0 = 0.0;

#line 99
    result_0.transmissivness_0 = 0.0;

#line 99
    result_0.opacity_0 = 0.0;
    result_0.baseColor_0 = payload_0.albedo_metalness_0.xyz;
    result_0.metalness_0 = payload_0.albedo_metalness_0.w;
    result_0.emissive_0 = payload_0.emission_roughness_0.xyz;
    result_0.roughness_0 = payload_0.emission_roughness_0.w;

    return result_0;
}


#line 45 1
vec2 octWrap_0(vec2 v_0)
{
    float _S4 = v_0.y;

#line 47
    float _S5 = 1.0 - abs(_S4);

#line 47
    float _S6 = v_0.x;

#line 47
    float _S7;

#line 47
    if(_S6 >= 0.0)
    {

#line 47
        _S7 = 1.0;

#line 47
    }
    else
    {

#line 47
        _S7 = -1.0;

#line 47
    }

#line 47
    float _S8 = _S5 * _S7;

#line 47
    float _S9 = 1.0 - abs(_S6);

#line 47
    if(_S4 >= 0.0)
    {

#line 47
        _S7 = 1.0;

#line 47
    }
    else
    {

#line 47
        _S7 = -1.0;

#line 47
    }

#line 47
    return vec2(_S8, _S9 * _S7);
}


#line 57
vec3 decodeNormalOctahedron_0(vec2 p_2)
{
    float _S10 = p_2.x;

#line 59
    float _S11 = p_2.y;

#line 59
    float _S12 = 1.0 - abs(_S10) - abs(_S11);

#line 59
    vec3 n_0 = vec3(_S10, _S11, _S12);

#line 59
    vec2 tmp_0;
    if(_S12 < 0.0)
    {

#line 60
        tmp_0 = octWrap_0(vec2(n_0.x, n_0.y));

#line 60
    }
    else
    {

#line 60
        tmp_0 = vec2(n_0.x, n_0.y);

#line 60
    }
    n_0[0] = tmp_0.x;
    n_0[1] = tmp_0.y;
    return normalize(n_0);
}


#line 70
void decodeNormals_0(vec4 encodedNormals_0, out vec3 geometryNormal_0, out vec3 shadingNormal_0)
{

#line 71
    geometryNormal_0 = decodeNormalOctahedron_0(encodedNormals_0.xy);
    shadingNormal_0 = decodeNormalOctahedron_0(encodedNormals_0.zw);
    return;
}


#line 84
struct SLight_0
{
    vec4 position_0;
    vec4 direction_0;
    vec4 color_type_0;
    vec4 position1_0;
    vec4 position2_0;
    vec4 uv0_uv1_0;
    vec4 uv2_objectId_0;
};


#line 84
SLight_0 unpackStorage_1(SLight_std430_0 _S13)
{

#line 84
    SLight_0 _S14 = { _S13.position_0, _S13.direction_0, _S13.color_type_0, _S13.position1_0, _S13.position2_0, _S13.uv0_uv1_0, _S13.uv2_objectId_0 };

#line 84
    return _S14;
}


#line 147
uvec4 pcg4d_0(uvec4 v_1)
{
    uvec4 _S15 = v_1 * 1664525U + 1013904223U;

#line 147
    uvec4 _S16 = _S15;



    _S16[0] = _S16[0] + _S15.y * _S15.w;
    _S16[1] = _S16[1] + _S16.z * _S16.x;
    _S16[2] = _S16[2] + _S16.x * _S16.y;
    _S16[3] = _S16[3] + _S16.y * _S16.z;

    uvec4 _S17 = _S16 ^ (_S16 >> 16U);

#line 156
    _S16 = _S17;

    _S16[0] = _S16[0] + _S17.y * _S17.w;
    _S16[1] = _S16[1] + _S16.z * _S16.x;
    _S16[2] = _S16[2] + _S16.x * _S16.y;
    _S16[3] = _S16[3] + _S16.y * _S16.z;

    return _S16;
}


#line 186
float uintToFloat_0(uint x_0)
{

#line 187
    return uintBitsToFloat(1065353216U | (x_0 >> 9)) - 1.0;
}


#line 198
float rand_0(inout uvec4 rngState_0)
{

#line 199
    rngState_0[3] = rngState_0[3] + 1U;
    return uintToFloat_0(pcg4d_0(rngState_0).x);
}


#line 109 2
bool sampleLightUniform_0(inout uvec4 rngState_1, vec3 hitPosition_0, vec3 surfaceNormal_0, out SLight_0 light_0, out float lightSampleWeight_0)
{
    if((MyContantBuffer_0.giParams_0.lightsCount_0) == 0U)
    {

#line 111
        return false;
    }
    uint _S18 = MyContantBuffer_0.giParams_0.lightsCount_0 - 1U;

#line 113
    float _S19 = rand_0(rngState_1);
    light_0 = unpackStorage_1(lightsBuffer_0._data[uint(min(_S18, uint(_S19 * float(MyContantBuffer_0.giParams_0.lightsCount_0))))]);


    lightSampleWeight_0 = float(MyContantBuffer_0.giParams_0.lightsCount_0);

    return true;
}


#line 274 1
vec2 UniformSampleTriangle_0(vec2 u_0)
{



    float _S20 = u_0.y;

#line 279
    float _S21 = u_0.x;

#line 279
    float b1_0;

#line 279
    float b2_0;

#line 279
    if(_S20 > _S21)
    {
        float b1_1 = _S21 * 0.5;
        float _S22 = _S20 - b1_1;

#line 282
        b1_0 = b1_1;

#line 282
        b2_0 = _S22;

#line 279
    }
    else
    {

#line 286
        float b2_1 = _S20 * 0.5;

#line 286
        b1_0 = _S21 - b2_1;

#line 286
        b2_0 = b2_1;

#line 279
    }

#line 290
    return vec2(b1_0, b2_0);
}


#line 298
void getLightData_0(SLight_0 light_1, vec3 hitPosition_1, out vec3 lightVector_0, out float lightDistance_0)
{

#line 299
    uint type_0 = uint(light_1.color_type_0.w);
    if(type_0 == 3U)
    {
        vec3 _S23 = light_1.position_0.xyz - hitPosition_1;

#line 302
        lightVector_0 = _S23;
        lightDistance_0 = length(_S23);

#line 300
    }
    else
    {

        if(type_0 == 2U)
        {

#line 305
            vec3 _S24 = light_1.position_0.xyz - hitPosition_1;

#line 305
            lightVector_0 = _S24;
            lightDistance_0 = length(_S24);

#line 304
        }
        else
        {
            if(type_0 == 1U)
            {

#line 308
                lightVector_0 = - light_1.direction_0.xyz;
                lightDistance_0 = 3.4028234663852886e+38;

#line 307
            }
            else
            {

                lightDistance_0 = 3.4028234663852886e+38;
                lightVector_0 = vec3(0.0, 1.0, 0.0);

#line 307
            }

#line 304
        }

#line 300
    }

#line 314
    return;
}


#line 12372 0
float saturate_0(float x_1)
{

#line 12380
    return clamp(x_1, 0.0, 1.0);
}


#line 243 3
vec3 baseColorToSpecularF0_0(vec3 baseColor_1, float metalness_1)
{

#line 244
    return mix(vec3(0.03999999910593033, 0.03999999910593033, 0.03999999910593033), baseColor_1, vec3(metalness_1));
}

vec3 baseColorToDiffuseReflectance_0(vec3 baseColor_2, float metalness_2)
{
    return baseColor_2 * (1.0 - metalness_2);
}


#line 238
float luminance_0(vec3 rgb_0)
{
    return dot(rgb_0, vec3(0.2125999927520752, 0.71520000696182251, 0.07220000028610229));
}


#line 381
float shadowedF90_0(vec3 F0_0)
{



    return min(1.0, 25.0 * luminance_0(F0_0));
}


#line 348
vec3 evalFresnelSchlick_0(vec3 f0_0, float f90_0, float NdotS_0)
{
    return f0_0 + (f90_0 - f0_0) * pow(1.0 - NdotS_0, 5.0);
}


#line 370
vec3 evalFresnel_0(vec3 f0_1, float f90_1, float NdotS_1)
{

    return evalFresnelSchlick_0(f0_1, f90_1, NdotS_1);
}


#line 184
struct BrdfData_0
{
    vec3 specularF0_0;
    vec3 diffuseReflectance_0;
    float roughness_1;
    float alpha_0;
    float alphaSquared_0;
    vec3 F_0;
    vec3 V_0;
    vec3 N_0;
    vec3 H_0;
    vec3 L_0;
    float NdotL_0;
    float NdotV_0;
    float LdotH_0;
    float NdotH_0;
    float VdotH_0;
    bool Vbackfacing_0;
    bool Lbackfacing_0;
};


#line 838
BrdfData_0 prepareBRDFData_0(vec3 N_1, vec3 L_1, vec3 V_1, MaterialProperties_0 material_0)
{

#line 839
    BrdfData_0 data_1;


    data_1.V_0 = V_1;
    data_1.N_0 = N_1;
    data_1.H_0 = normalize(L_1 + V_1);
    data_1.L_0 = L_1;

    float NdotL_1 = dot(N_1, L_1);
    float NdotV_1 = dot(N_1, V_1);
    data_1.Vbackfacing_0 = NdotV_1 <= 0.0;
    data_1.Lbackfacing_0 = NdotL_1 <= 0.0;


    data_1.NdotL_0 = min(max(0.00000999999974738, NdotL_1), 1.0);
    data_1.NdotV_0 = min(max(0.00000999999974738, NdotV_1), 1.0);

    data_1.LdotH_0 = saturate_0(dot(L_1, data_1.H_0));
    data_1.NdotH_0 = saturate_0(dot(N_1, data_1.H_0));
    data_1.VdotH_0 = saturate_0(dot(V_1, data_1.H_0));


    data_1.specularF0_0 = baseColorToSpecularF0_0(material_0.baseColor_0, material_0.metalness_0);
    data_1.diffuseReflectance_0 = baseColorToDiffuseReflectance_0(material_0.baseColor_0, material_0.metalness_0);


    data_1.roughness_1 = material_0.roughness_0;
    float _S25 = material_0.roughness_0 * material_0.roughness_0;

#line 866
    data_1.alpha_0 = _S25;
    data_1.alphaSquared_0 = _S25 * _S25;


    data_1.F_0 = evalFresnel_0(data_1.specularF0_0, shadowedF90_0(data_1.specularF0_0), data_1.LdotH_0);

    return data_1;
}


#line 689
float GGX_D_0(float alphaSquared_1, float NdotH_1)
{

#line 690
    float b_0 = (alphaSquared_1 - 1.0) * NdotH_1 * NdotH_1 + 1.0;
    return alphaSquared_1 / (3.14159274101257324 * b_0 * b_0);
}


#line 632
float Smith_G2_Height_Correlated_GGX_Lagarde_0(float alphaSquared_2, float NdotL_2, float NdotV_2)
{

    return 0.5 / (NdotV_2 * sqrt(alphaSquared_2 + NdotL_2 * (NdotL_2 - alphaSquared_2 * NdotL_2)) + NdotL_2 * sqrt(alphaSquared_2 + NdotV_2 * (NdotV_2 - alphaSquared_2 * NdotV_2)));
}


#line 657
float Smith_G2_0(float alpha_1, float alphaSquared_3, float NdotL_3, float NdotV_3)
{



    return Smith_G2_Height_Correlated_GGX_Lagarde_0(alphaSquared_3, NdotL_3, NdotV_3);
}


#line 818
vec3 evalMicrofacet_0(BrdfData_0 data_2)
{

#line 825
    return data_2.F_0 * (Smith_G2_0(data_2.alpha_0, data_2.alphaSquared_0, data_2.NdotL_0, data_2.NdotV_0) * GGX_D_0(max(0.00000999999974738, data_2.alphaSquared_0), data_2.NdotH_0) * data_2.NdotL_0);
}


#line 524
float frostbiteDisneyDiffuse_0(BrdfData_0 data_3)
{


    float FD90MinusOne_0 = 0.5 * data_3.roughness_1 + 2.0 * data_3.LdotH_0 * data_3.LdotH_0 * data_3.roughness_1 - 1.0;

#line 533
    return (1.0 + FD90MinusOne_0 * pow(1.0 - data_3.NdotL_0, 5.0)) * (1.0 + FD90MinusOne_0 * pow(1.0 - data_3.NdotV_0, 5.0)) * mix(1.0, 0.66225165128707886, data_3.roughness_1);
}

vec3 evalFrostbiteDisneyDiffuse_0(BrdfData_0 data_4)
{

#line 537
    return data_4.diffuseReflectance_0 * (frostbiteDisneyDiffuse_0(data_4) * 0.31830987334251404 * data_4.NdotL_0);
}


#line 876
vec3 evalCombinedBRDF_0(vec3 N_2, vec3 L_2, vec3 V_2, MaterialProperties_0 material_1)
{

    BrdfData_0 data_5 = prepareBRDFData_0(N_2, L_2, V_2, material_1);

#line 879
    bool _S26;


    if(data_5.Vbackfacing_0)
    {

#line 882
        _S26 = true;

#line 882
    }
    else
    {

#line 882
        _S26 = data_5.Lbackfacing_0;

#line 882
    }

#line 882
    if(_S26)
    {

#line 882
        return vec3(0.0, 0.0, 0.0);
    }

#line 893
    return evalFrostbiteDisneyDiffuse_0(data_5) + evalMicrofacet_0(data_5);
}


#line 317 1
vec3 getLightIntensityAtPoint_0(SLight_0 light_2, float distance_0)
{

#line 318
    uint type_1 = uint(light_2.color_type_0.w);

#line 318
    bool _S27;
    if(type_1 == 3U)
    {

#line 319
        _S27 = true;

#line 319
    }
    else
    {

#line 319
        _S27 = type_1 == 2U;

#line 319
    }

#line 319
    if(_S27)
    {

#line 325
        float _S28 = distance_0 * distance_0 + 0.25;

        return light_2.color_type_0.xyz * (2.0 / (_S28 + distance_0 * sqrt(_S28)));
    }
    else
    {

#line 329
        if(type_1 == 1U)
        {

#line 330
            return light_2.color_type_0.xyz;
        }
        else
        {

#line 332
            return vec3(0.0, 0.0, 0.0);
        }

#line 332
    }

#line 332
}


#line 123 2
bool sampleLightRIS_0(inout uvec4 rngState_2, vec3 hitPosition_2, vec3 surfaceNormal_1, out SLight_0 selectedSample_0, out float lightSampleWeight_1, MaterialProperties_0 material_2, vec3 shadingNormal_1, vec3 V_3)
{
    if((MyContantBuffer_0.giParams_0.lightsCount_0) == 0U)
    {

#line 125
        return false;
    }
    const vec4 _S29 = vec4(0.0, 0.0, 0.0, 0.0);

#line 127
    selectedSample_0.position_0 = _S29;

#line 127
    selectedSample_0.direction_0 = _S29;

#line 127
    selectedSample_0.color_type_0 = _S29;

#line 127
    selectedSample_0.position1_0 = _S29;

#line 127
    selectedSample_0.position2_0 = _S29;

#line 127
    selectedSample_0.uv0_uv1_0 = _S29;

#line 127
    selectedSample_0.uv2_objectId_0 = _S29;

#line 127
    float samplePdfG_0 = 0.0;

#line 127
    int i_0 = 0;

#line 127
    float totalWeights_0 = 0.0;



    for(;;)
    {

#line 131
        if(i_0 < 16)
        {
        }
        else
        {

#line 131
            break;
        }
        float candidateWeight_0;
        SLight_0 candidate_0;
        bool _S30 = sampleLightUniform_0(rngState_2, hitPosition_2, surfaceNormal_1, candidate_0, candidateWeight_0);

#line 135
        if(_S30)
        {

            if(uint(candidate_0.color_type_0.w) == 3U)
            {
                float _S31 = rand_0(rngState_2);

#line 140
                float _S32 = rand_0(rngState_2);
                vec2 bary_0 = UniformSampleTriangle_0(vec2(_S31, _S32));
                float _S33 = bary_0.x;

#line 142
                float _S34 = bary_0.y;

#line 142
                float _S35 = 1.0 - _S33 - _S34;

#line 142
                candidate_0.position_0.xyz = _S35 * candidate_0.position_0.xyz + _S33 * candidate_0.position1_0.xyz + _S34 * candidate_0.position2_0.xyz;


                candidate_0.color_type_0.xyz = (textureLod(sampler2D(texSet_0[matBuffer_0._data[uint(uint(candidate_0.uv2_objectId_0.z))].emissionTexIndex_0],linearSampler_0), (_S35 * candidate_0.uv0_uv1_0.xy + _S33 * candidate_0.uv0_uv1_0.zw + _S34 * candidate_0.uv2_objectId_0.xy), (0.0))).xyz;
                candidate_0.color_type_0.xyz = vec3(candidate_0.color_type_0.x * matBuffer_0._data[uint(uint(candidate_0.uv2_objectId_0.z))].emissiveFactorMult_emissiveStrength_0.x, candidate_0.color_type_0.y * matBuffer_0._data[uint(uint(candidate_0.uv2_objectId_0.z))].emissiveFactorMult_emissiveStrength_0.y, candidate_0.color_type_0.z * matBuffer_0._data[uint(uint(candidate_0.uv2_objectId_0.z))].emissiveFactorMult_emissiveStrength_0.z);

#line 138
            }

#line 148
            vec3 lightVector_1;
            float lightDistance_1;
            getLightData_0(candidate_0, hitPosition_2, lightVector_1, lightDistance_1);


            float candidatePdfG_0 = luminance_0(evalCombinedBRDF_0(shadingNormal_1, normalize(lightVector_1), V_3, material_2) * getLightIntensityAtPoint_0(candidate_0, length(lightVector_1)));
            float candidateRISWeight_0 = candidatePdfG_0 * candidateWeight_0;

            float totalWeights_1 = totalWeights_0 + candidateRISWeight_0;
            float _S36 = rand_0(rngState_2);

#line 157
            float samplePdfG_1;

#line 157
            if(_S36 < (candidateRISWeight_0 / totalWeights_1))
            {
                selectedSample_0 = candidate_0;

#line 159
                samplePdfG_1 = candidatePdfG_0;

#line 157
            }
            else
            {

#line 157
                samplePdfG_1 = samplePdfG_0;

#line 157
            }

#line 157
            samplePdfG_0 = samplePdfG_1;

#line 157
            totalWeights_0 = totalWeights_1;

#line 135
        }

#line 131
        i_0 = i_0 + 1;

#line 131
    }

#line 165
    if(totalWeights_0 == 0.0)
    {

#line 166
        return false;
    }
    else
    {

#line 168
        lightSampleWeight_1 = totalWeights_0 / 16.0 / samplePdfG_0;
        return true;
    }

#line 169
}


#line 253 1
vec3 offsetRay_0(vec3 p_3, vec3 n_1)
{

#line 259
    float _S37 = n_1.x;

#line 259
    int _S38 = int(256.0 * _S37);

#line 259
    float _S39 = n_1.y;

#line 259
    int _S40 = int(256.0 * _S39);

#line 259
    float _S41 = n_1.z;

#line 259
    int _S42 = int(256.0 * _S41);


    float _S43 = p_3.x;

#line 262
    int _S44 = floatBitsToInt(_S43);

#line 262
    int _S45;

#line 262
    if(_S43 < 0.0)
    {

#line 262
        _S45 = - _S38;

#line 262
    }
    else
    {

#line 262
        _S45 = _S38;

#line 262
    }

#line 262
    float _S46 = intBitsToFloat(_S44 + _S45);
    float _S47 = p_3.y;

#line 263
    int _S48 = floatBitsToInt(_S47);

#line 263
    if(_S47 < 0.0)
    {

#line 263
        _S45 = - _S40;

#line 263
    }
    else
    {

#line 263
        _S45 = _S40;

#line 263
    }

#line 263
    float _S49 = intBitsToFloat(_S48 + _S45);
    float _S50 = p_3.z;

#line 264
    int _S51 = floatBitsToInt(_S50);

#line 264
    if(_S50 < 0.0)
    {

#line 264
        _S45 = - _S42;

#line 264
    }
    else
    {

#line 264
        _S45 = _S42;

#line 264
    }

#line 264
    float _S52 = intBitsToFloat(_S51 + _S45);

#line 264
    float _S53;

    if((abs(_S43)) < 0.03125)
    {

#line 266
        _S53 = _S43 + 0.0000152587890625 * _S37;

#line 266
    }
    else
    {

#line 266
        _S53 = _S46;

#line 266
    }

#line 266
    float _S54;
    if((abs(_S47)) < 0.03125)
    {

#line 267
        _S54 = _S47 + 0.0000152587890625 * _S39;

#line 267
    }
    else
    {

#line 267
        _S54 = _S49;

#line 267
    }

#line 267
    float _S55;
    if((abs(_S50)) < 0.03125)
    {

#line 268
        _S55 = _S50 + 0.0000152587890625 * _S41;

#line 268
    }
    else
    {

#line 268
        _S55 = _S52;

#line 268
    }

#line 266
    return vec3(_S53, _S54, _S55);
}


#line 72 2
bool castShadowRay_0(vec3 hitPosition_3, vec3 surfaceNormal_2, vec3 directionToLight_0, float TMax_1)
{
    RayDesc_0 ray_1;
    ray_1.Origin_0 = offsetRay_0(hitPosition_3, surfaceNormal_2);
    ray_1.Direction_0 = directionToLight_0;
    ray_1.TMin_0 = 0.0;
    ray_1.TMax_0 = TMax_1;

    ShadowHitInfo_0 payload_1;
    payload_1.hasHit_0 = true;


    RayDesc_0 _S56 = ray_1;

#line 84
    p_0 = payload_1;

#line 84
    traceRayEXT(topLevelAS_0, 12U, 255U, 1U, 0U, 1U, _S56.Origin_0, _S56.TMin_0, _S56.Direction_0, _S56.TMax_0, 0);

#line 84
    payload_1 = p_0;

#line 94
    return !payload_1.hasHit_0;
}


#line 174
float getBrdfProbability_0(MaterialProperties_0 material_3, vec3 V_4, vec3 shadingNormal_2)
{

#line 180
    vec3 _S57 = vec3(luminance_0(baseColorToSpecularF0_0(material_3.baseColor_0, material_3.metalness_0)));

#line 180
    float Fresnel_0 = saturate_0(luminance_0(evalFresnel_0(_S57, shadowedF90_0(_S57), max(0.0, dot(V_4, shadingNormal_2)))));

#line 190
    return clamp(Fresnel_0 / max(0.00009999999747379, Fresnel_0 + luminance_0(baseColorToDiffuseReflectance_0(material_3.baseColor_0, material_3.metalness_0)) * (1.0 - Fresnel_0)), 0.10000000149011612, 0.89999997615814209);
}


#line 280 3
vec4 getRotationToZAxis_0(vec3 input_0)
{

    float _S58 = input_0.z;

#line 283
    if(_S58 < -0.99998998641967773)
    {

#line 283
        return vec4(1.0, 0.0, 0.0, 0.0);
    }
    return normalize(vec4(input_0.y, - input_0.x, 0.0, 1.0 + _S58));
}


#line 306
vec3 rotatePoint_0(vec4 q_0, vec3 v_2)
{

#line 307
    vec3 qAxis_0 = vec3(q_0.x, q_0.y, q_0.z);
    float _S59 = q_0.w;

#line 308
    return 2.0 * dot(qAxis_0, v_2) * qAxis_0 + (_S59 * _S59 - dot(qAxis_0, qAxis_0)) * v_2 + 2.0 * _S59 * cross(qAxis_0, v_2);
}


#line 317
vec3 sampleHemisphere_0(vec2 u_1, out float pdf_0)
{
    float _S60 = u_1.x;

#line 319
    float a_0 = sqrt(_S60);
    float b_1 = 6.28318548202514648 * u_1.y;

#line 325
    float _S61 = sqrt(1.0 - _S60);

#line 322
    vec3 result_1 = vec3(a_0 * cos(b_1), a_0 * sin(b_1), _S61);

#line 327
    pdf_0 = _S61 * 0.31830987334251404;

    return result_1;
}

vec3 sampleHemisphere_1(vec2 u_2)
{

#line 333
    float pdf_1;
    return sampleHemisphere_0(u_2, pdf_1);
}


#line 703
vec3 sampleGGXVNDF_0(vec3 Ve_0, vec2 alpha2D_0, vec2 u_3)
{

    float _S62 = alpha2D_0.x;

#line 706
    float _S63 = alpha2D_0.y;

#line 706
    vec3 Vh_0 = normalize(vec3(_S62 * Ve_0.x, _S63 * Ve_0.y, Ve_0.z));


    float _S64 = Vh_0.x;

#line 709
    float _S65 = Vh_0.y;

#line 709
    float lensq_0 = _S64 * _S64 + _S65 * _S65;

#line 709
    vec3 T1_0;
    if(lensq_0 > 0.0)
    {

#line 710
        T1_0 = vec3(- _S65, _S64, 0.0) * (inversesqrt((lensq_0)));

#line 710
    }
    else
    {

#line 710
        T1_0 = vec3(1.0, 0.0, 0.0);

#line 710
    }



    float r_0 = sqrt(u_3.x);
    float phi_0 = 6.28318548202514648 * u_3.y;
    float t1_0 = r_0 * cos(phi_0);


    float _S66 = 1.0 - t1_0 * t1_0;

#line 719
    float t2_0 = mix(sqrt(_S66), r_0 * sin(phi_0), 0.5 * (1.0 + Vh_0.z));


    vec3 Nh_0 = t1_0 * T1_0 + t2_0 * cross(Vh_0, T1_0) + sqrt(max(0.0, _S66 - t2_0 * t2_0)) * Vh_0;


    return normalize(vec3(_S62 * Nh_0.x, _S63 * Nh_0.y, max(0.0, Nh_0.z)));
}


#line 582
float Smith_G1_GGX_0(float alpha_2, float NdotS_2, float alphaSquared_4, float NdotSSquared_0)
{

#line 583
    return 2.0 / (sqrt((alphaSquared_4 * (1.0 - NdotSSquared_0) + NdotSSquared_0) / NdotSSquared_0) + 1.0);
}


#line 648
float Smith_G2_Over_G1_Height_Correlated_0(float alpha_3, float alphaSquared_5, float NdotL_4, float NdotV_4)
{

#line 649
    float G1V_0 = Smith_G1_GGX_0(alpha_3, NdotV_4, alphaSquared_5, NdotV_4 * NdotV_4);
    float G1L_0 = Smith_G1_GGX_0(alpha_3, NdotL_4, alphaSquared_5, NdotL_4 * NdotL_4);
    return G1L_0 / (G1V_0 + G1L_0 - G1V_0 * G1L_0);
}


#line 771
float specularSampleWeightGGXVNDF_0(float alpha_4, float alphaSquared_6, float NdotL_5, float NdotV_5, float HdotL_0, float NdotH_2)
{
    return Smith_G2_Over_G1_Height_Correlated_0(alpha_4, alphaSquared_6, NdotL_5, NdotV_5);
}


#line 786
vec3 sampleSpecularMicrofacet_0(vec3 Vlocal_0, float alpha_5, float alphaSquared_7, vec3 specularF0_1, vec2 u_4, out vec3 weight_0)
{

#line 786
    vec3 Hlocal_0;



    if(alpha_5 == 0.0)
    {

#line 790
        Hlocal_0 = vec3(0.0, 0.0, 1.0);

#line 790
    }
    else
    {

#line 790
        Hlocal_0 = sampleGGXVNDF_0(Vlocal_0, vec2(alpha_5, alpha_5), u_4);

#line 790
    }

#line 799
    vec3 Llocal_0 = reflect(- Vlocal_0, Hlocal_0);



    float HdotL_1 = max(0.00000999999974738, min(1.0, dot(Hlocal_0, Llocal_0)));
    const vec3 Nlocal_0 = vec3(0.0, 0.0, 1.0);

#line 812
    weight_0 = evalFresnel_0(specularF0_1, shadowedF90_0(specularF0_1), HdotL_1) * specularSampleWeightGGXVNDF_0(alpha_5, alphaSquared_7, max(0.00000999999974738, min(1.0, dot(Nlocal_0, Llocal_0))), max(0.00000999999974738, min(1.0, dot(Nlocal_0, Vlocal_0))), HdotL_1, max(0.00000999999974738, min(1.0, dot(Nlocal_0, Hlocal_0))));

    return Llocal_0;
}


#line 299
vec4 invertRotation_0(vec4 q_1)
{
    return vec4(- q_1.x, - q_1.y, - q_1.z, q_1.w);
}


#line 898
bool evalIndirectCombinedBRDF_0(vec2 u_5, vec3 shadingNormal_3, vec3 geometryNormal_1, vec3 V_5, MaterialProperties_0 material_4, int brdfType_0, out vec3 rayDirection_0, out vec3 sampleWeight_0)
{

    if((dot(shadingNormal_3, V_5)) <= 0.0)
    {

#line 901
        return false;
    }


    vec4 qRotationToZ_0 = getRotationToZAxis_0(shadingNormal_3);
    vec3 Vlocal_1 = rotatePoint_0(qRotationToZ_0, V_5);
    const vec3 Nlocal_1 = vec3(0.0, 0.0, 1.0);

    const vec3 _S67 = vec3(0.0, 0.0, 0.0);

#line 909
    vec3 rayDirectionLocal_0;

    if(brdfType_0 == 4)
    {

        vec3 rayDirectionLocal_1 = sampleHemisphere_1(u_5);
        BrdfData_0 data_6 = prepareBRDFData_0(Nlocal_1, rayDirectionLocal_1, Vlocal_1, material_4);


        sampleWeight_0 = data_6.diffuseReflectance_0 * frostbiteDisneyDiffuse_0(data_6);

#line 918
        rayDirectionLocal_0 = rayDirectionLocal_1;

#line 911
    }
    else
    {

#line 929
        if(brdfType_0 == 1)
        {

#line 930
            BrdfData_0 data_7 = prepareBRDFData_0(Nlocal_1, Nlocal_1, Vlocal_1, material_4);
            vec3 _S68 = sampleSpecularMicrofacet_0(Vlocal_1, data_7.alpha_0, data_7.alphaSquared_0, data_7.specularF0_0, u_5, sampleWeight_0);

#line 931
            rayDirectionLocal_0 = _S68;

#line 929
        }
        else
        {

#line 929
            rayDirectionLocal_0 = _S67;

#line 929
        }

#line 911
    }

#line 935
    if((luminance_0(sampleWeight_0)) == 0.0)
    {

#line 935
        return false;
    }

    vec3 _S69 = normalize(rotatePoint_0(invertRotation_0(qRotationToZ_0), rayDirectionLocal_0));

#line 938
    rayDirection_0 = _S69;


    if((dot(geometryNormal_1, _S69)) <= 0.0)
    {

#line 941
        return false;
    }
    return true;
}


#line 195 2
void main()
{

#line 195
    vec3 radiance_0;

    uvec3 _S70 = ((gl_LaunchIDEXT));

#line 197
    uvec2 LaunchIndex_0 = _S70.xy;
    uvec3 _S71 = ((gl_LaunchSizeEXT));

#line 198
    uvec2 LaunchDimensions_0 = _S71.xy;


    vec2 d_0 = (vec2(LaunchIndex_0) + vec2(0.5)) / vec2(LaunchDimensions_0) * 2.0 - 1.0;

    vec4 origin_0 = (((vec4(0.0, 0.0, 0.0, 1.0)) * (unpackStorage_0(MyContantBuffer_0.giParams_0.viewInverse_0))));



    vec3 rayDir_0 = normalize((((vec4(normalize((((vec4(d_0.x, d_0.y, 1.0, 1.0)) * (unpackStorage_0(MyContantBuffer_0.giParams_0.projInverse_0)))).xyz), 0.0)) * (unpackStorage_0(MyContantBuffer_0.giParams_0.viewInverse_0)))).xyz);
    vec3 _S72 = origin_0.xyz;


    uvec4 rngState_3 = initRNG_0(LaunchIndex_0, LaunchDimensions_0, MyContantBuffer_0.giParams_0.frameCount_0);


    const vec3 _S73 = vec3(0.0, 0.0, 0.0);
    const vec3 _S74 = vec3(1.0, 1.0, 1.0);

    const vec4 _S75 = vec4(0.0, 0.0, 0.0, 0.0);


    const vec4 _S76 = vec4(0.0, 0.0, 0.0, -1.0);

#line 332
    ivec2 _S77 = ivec2(LaunchIndex_0);

#line 332
    vec3 rayOri_0 = _S72;

#line 332
    vec4 albedoMetalnessOutput_0 = _S75;

#line 332
    vec4 emissionRoughnessOutput_0 = _S75;

#line 332
    vec4 normalOutput_0 = _S75;

#line 332
    vec4 wposObjectIdOutput_0 = _S76;

#line 332
    int bounce_0 = 0;

#line 332
    vec3 throughput_0 = _S74;

#line 332
    vec3 radiance_1 = _S73;

#line 222
    for(;;)
    {

#line 222
        if(uint(bounce_0) <= (MyContantBuffer_0.giParams_0.numRays_0))
        {
        }
        else
        {

#line 222
            radiance_0 = radiance_1;

#line 222
            break;
        }
        IndirectGbufferRayPayload_0 payload_2 = shootRay_0(rayOri_0, rayDir_0, 3.4028234663852886e+38);

        if(!IndirectGbufferRayPayload_hasHit_0(payload_2))
        {

#line 226
            radiance_0 = radiance_1 + throughput_0 * _S73;


            break;
        }


        MaterialProperties_0 material_5 = loadMaterialProperties_0(payload_2);

#line 233
        vec4 albedoMetalnessOutput_1;

#line 233
        vec4 emissionRoughnessOutput_1;

#line 233
        vec4 normalOutput_1;

#line 233
        vec4 wposObjectIdOutput_1;

        if(bounce_0 == 0)
        {

            vec4 _S78 = vec4(material_5.emissive_0, material_5.roughness_0);

#line 238
            albedoMetalnessOutput_1 = vec4(material_5.baseColor_0, material_5.metalness_0);

#line 238
            emissionRoughnessOutput_1 = _S78;

#line 238
            normalOutput_1 = payload_2.normal_0;

#line 238
            wposObjectIdOutput_1 = payload_2.position_objectID_0;

#line 235
        }
        else
        {

#line 235
            albedoMetalnessOutput_1 = albedoMetalnessOutput_0;

#line 235
            emissionRoughnessOutput_1 = emissionRoughnessOutput_0;

#line 235
            normalOutput_1 = normalOutput_0;

#line 235
            wposObjectIdOutput_1 = wposObjectIdOutput_0;

#line 235
        }

#line 243
        vec3 geometryNormal_2;
        vec3 shadingNormal_4;
        decodeNormals_0(payload_2.normal_0, geometryNormal_2, shadingNormal_4);

        vec3 V_6 = - rayDir_0;
        if((dot(geometryNormal_2, V_6)) < 0.0)
        {

#line 248
            geometryNormal_2 = - geometryNormal_2;

#line 248
        }
        if((dot(geometryNormal_2, shadingNormal_4)) < 0.0)
        {

#line 249
            shadingNormal_4 = - shadingNormal_4;

#line 249
        }

#line 249
        bool _S79;

#line 259
        if((dot(V_6, geometryNormal_2)) > 0.0)
        {

#line 259
            _S79 = (dot(V_6, shadingNormal_4)) < 0.0;

#line 259
        }
        else
        {

#line 259
            _S79 = false;

#line 259
        }

#line 259
        vec3 V_7;

#line 259
        if(_S79)
        {
            vec3 V_8 = normalize(V_6);


            shadingNormal_4 = normalize(0.00009999999747379 * V_8 + (shadingNormal_4 - dot(shadingNormal_4, V_8) * V_8));

#line 264
            V_7 = V_8;

#line 259
        }
        else
        {

#line 259
            V_7 = V_6;

#line 259
        }

#line 267
        vec3 radiance_2 = radiance_1 + throughput_0 * material_5.emissive_0;

#line 275
        float lightWeight_0 = -1.0;
        vec3 _S80 = payload_2.position_objectID_0.xyz;

#line 274
        SLight_0 light_3;

        bool _S81 = sampleLightRIS_0(rngState_3, _S80, geometryNormal_2, light_3, lightWeight_0, material_5, shadingNormal_4, V_7);

#line 276
        if(_S81)
        {

            vec3 lightVector_2;
            float lightDistance_2;
            getLightData_0(light_3, _S80, lightVector_2, lightDistance_2);
            vec3 L_3 = normalize(lightVector_2);

            bool _S82 = castShadowRay_0(_S80, geometryNormal_2, L_3, lightDistance_2);

#line 284
            if(_S82)
            {

#line 284
                radiance_0 = radiance_2 + throughput_0 * evalCombinedBRDF_0(shadingNormal_4, L_3, V_7, material_5) * (getLightIntensityAtPoint_0(light_3, lightDistance_2) * lightWeight_0);

#line 284
            }
            else
            {

#line 284
                radiance_0 = radiance_2;

#line 284
            }

#line 276
        }
        else
        {

#line 276
            radiance_0 = radiance_2;

#line 276
        }

#line 276
        vec3 throughput_1;

#line 292
        if(bounce_0 > 3)
        {

#line 292
            vec3 _S83;
            float rrProbability_0 = min(0.94999998807907104, luminance_0(throughput_0));
            float _S84 = rand_0(rngState_3);

#line 294
            if(rrProbability_0 < _S84)
            {

#line 294
                albedoMetalnessOutput_0 = albedoMetalnessOutput_1;

#line 294
                emissionRoughnessOutput_0 = emissionRoughnessOutput_1;

#line 294
                normalOutput_0 = normalOutput_1;

#line 294
                wposObjectIdOutput_0 = wposObjectIdOutput_1;

#line 294
                break;
            }
            else
            {

#line 294
                _S83 = throughput_0 / rrProbability_0;

#line 294
            }

#line 294
            throughput_1 = _S83;

#line 292
        }
        else
        {

#line 292
            throughput_1 = throughput_0;

#line 292
        }

#line 292
        bool _S85;

#line 301
        if((material_5.metalness_0) == 1.0)
        {

#line 301
            _S85 = (material_5.roughness_0) == 0.0;

#line 301
        }
        else
        {

#line 301
            _S85 = false;

#line 301
        }

#line 301
        vec3 throughput_2;

#line 301
        int brdfType_1;

#line 301
        if(_S85)
        {

#line 301
            brdfType_1 = 1;

#line 301
            throughput_2 = throughput_1;

#line 301
        }
        else
        {



            float brdfProbability_0 = getBrdfProbability_0(material_5, V_7, shadingNormal_4);

            float _S86 = rand_0(rngState_3);

#line 309
            if(_S86 < brdfProbability_0)
            {
                vec3 throughput_3 = throughput_1 / brdfProbability_0;

#line 311
                brdfType_1 = 1;

#line 311
                throughput_2 = throughput_3;

#line 309
            }
            else
            {


                vec3 throughput_4 = throughput_1 / (1.0 - brdfProbability_0);

#line 314
                brdfType_1 = 4;

#line 314
                throughput_2 = throughput_4;

#line 309
            }

#line 301
        }

#line 320
        float _S87 = rand_0(rngState_3);

#line 320
        float _S88 = rand_0(rngState_3);

#line 319
        vec3 brdfWeight_0;

        bool _S89 = evalIndirectCombinedBRDF_0(vec2(_S87, _S88), shadingNormal_4, geometryNormal_2, V_7, material_5, brdfType_1, rayDir_0, brdfWeight_0);

#line 321
        if(!_S89)
        {

#line 321
            albedoMetalnessOutput_0 = albedoMetalnessOutput_1;

#line 321
            emissionRoughnessOutput_0 = emissionRoughnessOutput_1;

#line 321
            normalOutput_0 = normalOutput_1;

#line 321
            wposObjectIdOutput_0 = wposObjectIdOutput_1;
            break;
        }


        vec3 throughput_5 = throughput_2 * brdfWeight_0;


        vec3 _S90 = offsetRay_0(_S80, geometryNormal_2);

#line 222
        int _S91 = bounce_0 + 1;

#line 222
        rayOri_0 = _S90;

#line 222
        albedoMetalnessOutput_0 = albedoMetalnessOutput_1;

#line 222
        emissionRoughnessOutput_0 = emissionRoughnessOutput_1;

#line 222
        normalOutput_0 = normalOutput_1;

#line 222
        wposObjectIdOutput_0 = wposObjectIdOutput_1;

#line 222
        bounce_0 = _S91;

#line 222
        throughput_0 = throughput_5;

#line 222
        radiance_1 = radiance_0;

#line 222
    }

#line 332
    imageStore((ptOutput_0), (_S77), vec4(radiance_0, 1.0));

    imageStore((ptAlbedoMetalnessOutput_0), (_S77), albedoMetalnessOutput_0);
    imageStore((ptEmissionRoughnessOutput_0), (_S77), emissionRoughnessOutput_0);
    imageStore((ptNormalOutput_0), (_S77), normalOutput_0);
    imageStore((ptWposObjectIdOutput_0), (_S77), wposObjectIdOutput_0);
    return;
}

