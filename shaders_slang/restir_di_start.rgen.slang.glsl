#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_control_flow_attributes : require
layout(row_major) uniform;
layout(row_major) buffer;

#line 548 0
struct _MatrixStorage_float4x4_ColMajorstd140_0
{
    vec4  data_0[4];
};


#line 24 1
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
    uint widthScreen_0;
    uint heightScreen_0;
};


#line 24
struct SLANG_ParameterGroup_MyContantBuffer_std140_0
{
    SGlobalGIParams_std140_0 giParams_0;
};


#line 30 2
layout(binding = 0, set = 1)
layout(std140) uniform block_SLANG_ParameterGroup_MyContantBuffer_std140_0
{
    SGlobalGIParams_std140_0 giParams_0;
}MyContantBuffer_0;

#line 17
layout(binding = 2)
uniform accelerationStructureEXT topLevelAS_0;


#line 101 1
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

#line 141 1
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

layout(binding = 6)
uniform sampler linearSampler_0;


#line 37
layout(rgba32f)
layout(binding = 0, set = 2)
uniform image2D ptAlbedoMetalnessOutput_0;


#line 40
layout(rgba32f)
layout(binding = 1, set = 2)
uniform image2D ptEmissionRoughnessOutput_0;


#line 43
layout(rgba32f)
layout(binding = 2, set = 2)
uniform image2D ptNormalOutput_0;


#line 46
layout(rgba32f)
layout(binding = 3, set = 2)
uniform image2D ptWposObjectIdOutput_0;


#line 238 1
struct SReservoir_std430_0
{
    float weightSum_0;
    int lightSampler_0;
    uint samplesNumber_0;
    float finalWeight_0;
    vec4 bary_0;
};


#line 26 2
layout(std430, binding = 8) buffer StructuredBuffer_SReservoir_std430_t_0 {
    SReservoir_std430_0 _data[];
} reservoirInitBuffer_0;

#line 84 1
struct ShadowHitInfo_0
{
    bool hasHit_0;
};


#line 16545 0
layout(location = 0)
rayPayloadEXT
ShadowHitInfo_0 p_0;


#line 35 1
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


#line 202 1
uvec4 initRNG_0(uvec2 pixelCoords_0, uvec2 resolution_0, uint frameNumber_0)
{

#line 203
    return uvec4(pixelCoords_0.xy, frameNumber_0, 0U);
}


#line 232
struct SReservoir_0
{
    float weightSum_0;
    int lightSampler_0;
    uint samplesNumber_0;
    float finalWeight_0;
    vec4 bary_0;
};


#line 232
SReservoir_std430_0 packStorage_0(SReservoir_0 _S2)
{

#line 232
    SReservoir_std430_0 _S3 = { _S2.weightSum_0, _S2.lightSampler_0, _S2.samplesNumber_0, _S2.finalWeight_0, _S2.bary_0 };

#line 232
    return _S3;
}


#line 232
SReservoir_0 SReservoir_x24init_0(float weightSum_1, int lightSampler_1, uint samplesNumber_1, float finalWeight_1, vec4 bary_1)
{

#line 232
    SReservoir_0 _S4;

    _S4.weightSum_0 = weightSum_1;
    _S4.lightSampler_0 = lightSampler_1;
    _S4.samplesNumber_0 = samplesNumber_1;
    _S4.finalWeight_0 = finalWeight_1;
    _S4.bary_0 = bary_1;

#line 232
    return _S4;
}


#line 16303 0
struct RayDesc_0
{
    vec3 Origin_0;
    float TMin_0;
    vec3 Direction_0;
    float TMax_0;
};


#line 49 2
IndirectGbufferRayPayload_0 shootRay_0(vec3 orig_0, vec3 dir_0, float max_0)
{

    RayDesc_0 ray_0;
    ray_0.Origin_0 = orig_0;
    ray_0.Direction_0 = dir_0;
    ray_0.TMin_0 = 0.0;
    ray_0.TMax_0 = max_0;

    RayDesc_0 _S5 = ray_0;

#line 58
    traceRayEXT(topLevelAS_0, 0U, 255U, 0U, 0U, 0U, _S5.Origin_0, _S5.TMin_0, _S5.Direction_0, _S5.TMax_0, 1);

#line 69
    return p_1;
}


#line 42 1
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


#line 99 2
MaterialProperties_0 loadMaterialProperties_0(IndirectGbufferRayPayload_0 payload_0)
{
    const vec3 _S6 = vec3(0.0, 0.0, 0.0);

#line 101
    MaterialProperties_0 result_0;

#line 101
    result_0.baseColor_0 = _S6;

#line 101
    result_0.metalness_0 = 0.0;

#line 101
    result_0.emissive_0 = _S6;

#line 101
    result_0.roughness_0 = 0.0;

#line 101
    result_0.transmissivness_0 = 0.0;

#line 101
    result_0.opacity_0 = 0.0;
    result_0.baseColor_0 = payload_0.albedo_metalness_0.xyz;
    result_0.metalness_0 = payload_0.albedo_metalness_0.w;
    result_0.emissive_0 = payload_0.emission_roughness_0.xyz;
    result_0.roughness_0 = payload_0.emission_roughness_0.w;

    return result_0;
}


#line 54 1
vec2 octWrap_0(vec2 v_0)
{
    float _S7 = v_0.y;

#line 56
    float _S8 = 1.0 - abs(_S7);

#line 56
    float _S9 = v_0.x;

#line 56
    float _S10;

#line 56
    if(_S9 >= 0.0)
    {

#line 56
        _S10 = 1.0;

#line 56
    }
    else
    {

#line 56
        _S10 = -1.0;

#line 56
    }

#line 56
    float _S11 = _S8 * _S10;

#line 56
    float _S12 = 1.0 - abs(_S9);

#line 56
    if(_S7 >= 0.0)
    {

#line 56
        _S10 = 1.0;

#line 56
    }
    else
    {

#line 56
        _S10 = -1.0;

#line 56
    }

#line 56
    return vec2(_S11, _S12 * _S10);
}


#line 66
vec3 decodeNormalOctahedron_0(vec2 p_2)
{
    float _S13 = p_2.x;

#line 68
    float _S14 = p_2.y;

#line 68
    float _S15 = 1.0 - abs(_S13) - abs(_S14);

#line 68
    vec3 n_0 = vec3(_S13, _S14, _S15);

#line 68
    vec2 tmp_0;
    if(_S15 < 0.0)
    {

#line 69
        tmp_0 = octWrap_0(vec2(n_0.x, n_0.y));

#line 69
    }
    else
    {

#line 69
        tmp_0 = vec2(n_0.x, n_0.y);

#line 69
    }
    n_0[0] = tmp_0.x;
    n_0[1] = tmp_0.y;
    return normalize(n_0);
}


#line 79
void decodeNormals_0(vec4 encodedNormals_0, out vec3 geometryNormal_0, out vec3 shadingNormal_0)
{

#line 80
    geometryNormal_0 = decodeNormalOctahedron_0(encodedNormals_0.xy);
    shadingNormal_0 = decodeNormalOctahedron_0(encodedNormals_0.zw);
    return;
}


#line 93
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


#line 93
SLight_0 unpackStorage_1(SLight_std430_0 _S16)
{

#line 93
    SLight_0 _S17 = { _S16.position_0, _S16.direction_0, _S16.color_type_0, _S16.position1_0, _S16.position2_0, _S16.uv0_uv1_0, _S16.uv2_objectId_0 };

#line 93
    return _S17;
}


#line 156
uvec4 pcg4d_0(uvec4 v_1)
{
    uvec4 _S18 = v_1 * 1664525U + 1013904223U;

#line 156
    uvec4 _S19 = _S18;



    _S19[0] = _S19[0] + _S18.y * _S18.w;
    _S19[1] = _S19[1] + _S19.z * _S19.x;
    _S19[2] = _S19[2] + _S19.x * _S19.y;
    _S19[3] = _S19[3] + _S19.y * _S19.z;

    uvec4 _S20 = _S19 ^ (_S19 >> 16U);

#line 165
    _S19 = _S20;

    _S19[0] = _S19[0] + _S20.y * _S20.w;
    _S19[1] = _S19[1] + _S19.z * _S19.x;
    _S19[2] = _S19[2] + _S19.x * _S19.y;
    _S19[3] = _S19[3] + _S19.y * _S19.z;

    return _S19;
}


#line 195
float uintToFloat_0(uint x_0)
{

#line 196
    return uintBitsToFloat(1065353216U | (x_0 >> 9)) - 1.0;
}


#line 207
float rand_0(inout uvec4 rngState_0)
{

#line 208
    rngState_0[3] = rngState_0[3] + 1U;
    return uintToFloat_0(pcg4d_0(rngState_0).x);
}


#line 111 2
bool sampleLightUniform_0(inout uvec4 rngState_1, vec3 hitPosition_0, vec3 surfaceNormal_0, out SLight_0 light_0, out float lightSampleWeight_0, out uint selectLightIndex_0)
{
    if((MyContantBuffer_0.giParams_0.lightsCount_0) == 0U)
    {

#line 113
        return false;
    }
    uint _S21 = MyContantBuffer_0.giParams_0.lightsCount_0 - 1U;

#line 115
    float _S22 = rand_0(rngState_1);

#line 115
    uint randomLightIndex_0 = min(_S21, uint(_S22 * float(MyContantBuffer_0.giParams_0.lightsCount_0)));
    light_0 = unpackStorage_1(lightsBuffer_0._data[uint(randomLightIndex_0)]);


    lightSampleWeight_0 = float(MyContantBuffer_0.giParams_0.lightsCount_0);

    selectLightIndex_0 = randomLightIndex_0;

    return true;
}


#line 326 1
vec2 UniformSampleTriangle_0(vec2 u_0)
{



    float _S23 = u_0.y;

#line 331
    float _S24 = u_0.x;

#line 331
    float b1_0;

#line 331
    float b2_0;

#line 331
    if(_S23 > _S24)
    {
        float b1_1 = _S24 * 0.5;
        float _S25 = _S23 - b1_1;

#line 334
        b1_0 = b1_1;

#line 334
        b2_0 = _S25;

#line 331
    }
    else
    {

#line 338
        float b2_1 = _S23 * 0.5;

#line 338
        b1_0 = _S24 - b2_1;

#line 338
        b2_0 = b2_1;

#line 331
    }

#line 342
    return vec2(b1_0, b2_0);
}


#line 350
void getLightData_0(SLight_0 light_1, vec3 hitPosition_1, out vec3 lightVector_0, out float lightDistance_0)
{

#line 351
    uint type_0 = uint(light_1.color_type_0.w);
    if(type_0 == 3U)
    {
        vec3 _S26 = light_1.position_0.xyz - hitPosition_1;

#line 354
        lightVector_0 = _S26;
        lightDistance_0 = length(_S26);

#line 352
    }
    else
    {

        if(type_0 == 2U)
        {

#line 357
            vec3 _S27 = light_1.position_0.xyz - hitPosition_1;

#line 357
            lightVector_0 = _S27;
            lightDistance_0 = length(_S27);

#line 356
        }
        else
        {
            if(type_0 == 1U)
            {

#line 360
                lightVector_0 = - light_1.direction_0.xyz;
                lightDistance_0 = 3.4028234663852886e+38;

#line 359
            }
            else
            {

                lightDistance_0 = 3.4028234663852886e+38;
                lightVector_0 = vec3(0.0, 1.0, 0.0);

#line 359
            }

#line 356
        }

#line 352
    }

#line 366
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
    float _S28 = material_0.roughness_0 * material_0.roughness_0;

#line 866
    data_1.alpha_0 = _S28;
    data_1.alphaSquared_0 = _S28 * _S28;


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
    bool _S29;


    if(data_5.Vbackfacing_0)
    {

#line 882
        _S29 = true;

#line 882
    }
    else
    {

#line 882
        _S29 = data_5.Lbackfacing_0;

#line 882
    }

#line 882
    if(_S29)
    {

#line 882
        return vec3(0.0, 0.0, 0.0);
    }

#line 893
    return evalFrostbiteDisneyDiffuse_0(data_5) + evalMicrofacet_0(data_5);
}


#line 369 1
vec3 getLightIntensityAtPoint_0(SLight_0 light_2, float distance_0)
{

#line 370
    uint type_1 = uint(light_2.color_type_0.w);

#line 370
    bool _S30;
    if(type_1 == 3U)
    {

#line 371
        _S30 = true;

#line 371
    }
    else
    {

#line 371
        _S30 = type_1 == 2U;

#line 371
    }

#line 371
    if(_S30)
    {

#line 377
        float _S31 = distance_0 * distance_0 + 0.25;

        return light_2.color_type_0.xyz * (2.0 / (_S31 + distance_0 * sqrt(_S31)));
    }
    else
    {

#line 381
        if(type_1 == 1U)
        {

#line 382
            return light_2.color_type_0.xyz;
        }
        else
        {

#line 384
            return vec3(0.0, 0.0, 0.0);
        }

#line 384
    }

#line 384
}


#line 127 2
bool sampleLightRIS_0(inout uvec4 rngState_2, vec3 hitPosition_2, vec3 surfaceNormal_1, out SLight_0 selectedSample_0, out float lightSampleWeight_1, MaterialProperties_0 material_2, vec3 shadingNormal_1, vec3 V_3, out SReservoir_0 reservoir_0)
{
    if((MyContantBuffer_0.giParams_0.lightsCount_0) == 0U)
    {

#line 129
        return false;
    }
    const vec4 _S32 = vec4(0.0, 0.0, 0.0, 0.0);

#line 131
    selectedSample_0.position_0 = _S32;

#line 131
    selectedSample_0.direction_0 = _S32;

#line 131
    selectedSample_0.color_type_0 = _S32;

#line 131
    selectedSample_0.position1_0 = _S32;

#line 131
    selectedSample_0.position2_0 = _S32;

#line 131
    selectedSample_0.uv0_uv1_0 = _S32;

#line 131
    selectedSample_0.uv2_objectId_0 = _S32;


    uint candidateLightIndex_0 = 4294967295U;

#line 134
    vec2 candidateBary_0;

#line 134
    vec2 selectBary_0;

#line 134
    float samplePdfG_0 = 0.0;

#line 134
    uint selectLightIndex_1 = 4294967295U;

#line 134
    int i_0 = 0;

#line 134
    float totalWeights_0 = 0.0;

#line 140
    [[unroll]]
    for(;;)
    {

#line 140
        if(i_0 < 64)
        {
        }
        else
        {

#line 140
            break;
        }
        float candidateWeight_0;
        SLight_0 candidate_0;
        bool _S33 = sampleLightUniform_0(rngState_2, hitPosition_2, surfaceNormal_1, candidate_0, candidateWeight_0, candidateLightIndex_0);

#line 144
        if(_S33)
        {

#line 144
            vec2 candidateBary_1;


            if(uint(candidate_0.color_type_0.w) == 3U)
            {
                float _S34 = rand_0(rngState_2);

#line 149
                float _S35 = rand_0(rngState_2);
                vec2 bary_2 = UniformSampleTriangle_0(vec2(_S34, _S35));

                float _S36 = bary_2.x;

#line 152
                float _S37 = bary_2.y;

#line 152
                float _S38 = 1.0 - _S36 - _S37;

#line 152
                candidate_0.position_0.xyz = _S38 * candidate_0.position_0.xyz + _S36 * candidate_0.position1_0.xyz + _S37 * candidate_0.position2_0.xyz;


                candidate_0.color_type_0.xyz = (textureLod(sampler2D(texSet_0[matBuffer_0._data[uint(uint(candidate_0.uv2_objectId_0.z))].emissionTexIndex_0],linearSampler_0), (_S38 * candidate_0.uv0_uv1_0.xy + _S36 * candidate_0.uv0_uv1_0.zw + _S37 * candidate_0.uv2_objectId_0.xy), (0.0))).xyz;
                candidate_0.color_type_0.xyz = vec3(candidate_0.color_type_0.x * matBuffer_0._data[uint(uint(candidate_0.uv2_objectId_0.z))].emissiveFactorMult_emissiveStrength_0.x, candidate_0.color_type_0.y * matBuffer_0._data[uint(uint(candidate_0.uv2_objectId_0.z))].emissiveFactorMult_emissiveStrength_0.y, candidate_0.color_type_0.z * matBuffer_0._data[uint(uint(candidate_0.uv2_objectId_0.z))].emissiveFactorMult_emissiveStrength_0.z);

#line 156
                candidateBary_1 = bary_2;

#line 147
            }
            else
            {

#line 147
                candidateBary_1 = candidateBary_0;

#line 147
            }

#line 158
            vec3 lightVector_1;
            float lightDistance_1;
            getLightData_0(candidate_0, hitPosition_2, lightVector_1, lightDistance_1);


            float candidatePdfG_0 = luminance_0(evalCombinedBRDF_0(shadingNormal_1, normalize(lightVector_1), V_3, material_2) * getLightIntensityAtPoint_0(candidate_0, length(lightVector_1)));
            float candidateRISWeight_0 = candidatePdfG_0 * candidateWeight_0;

            float totalWeights_1 = totalWeights_0 + candidateRISWeight_0;
            float _S39 = rand_0(rngState_2);

#line 167
            vec2 selectBary_1;

#line 167
            float samplePdfG_1;

#line 167
            uint selectLightIndex_2;

#line 167
            if(_S39 < (candidateRISWeight_0 / totalWeights_1))
            {
                selectedSample_0 = candidate_0;

#line 169
                samplePdfG_1 = candidatePdfG_0;

#line 169
                selectLightIndex_2 = candidateLightIndex_0;

#line 169
                selectBary_1 = candidateBary_1;

#line 167
            }
            else
            {

#line 167
                samplePdfG_1 = samplePdfG_0;

#line 167
                selectLightIndex_2 = selectLightIndex_1;

#line 167
                selectBary_1 = selectBary_0;

#line 167
            }

#line 167
            candidateBary_0 = candidateBary_1;

#line 167
            samplePdfG_0 = samplePdfG_1;

#line 167
            selectLightIndex_1 = selectLightIndex_2;

#line 167
            selectBary_0 = selectBary_1;

#line 167
            totalWeights_0 = totalWeights_1;

#line 144
        }

#line 140
        i_0 = i_0 + 1;

#line 140
    }

#line 177
    if(totalWeights_0 == 0.0)
    {

#line 178
        return false;
    }
    else
    {

#line 180
        lightSampleWeight_1 = totalWeights_0 / 64.0 / samplePdfG_0;
        reservoir_0.weightSum_0 = totalWeights_0;
        reservoir_0.lightSampler_0 = int(selectLightIndex_1);
        reservoir_0.samplesNumber_0 = 64U;
        reservoir_0.finalWeight_0 = lightSampleWeight_1;
        reservoir_0.bary_0.xy = selectBary_0;
        return true;
    }

#line 186
}


#line 305 1
vec3 offsetRay_0(vec3 p_3, vec3 n_1)
{

#line 311
    float _S40 = n_1.x;

#line 311
    int _S41 = int(256.0 * _S40);

#line 311
    float _S42 = n_1.y;

#line 311
    int _S43 = int(256.0 * _S42);

#line 311
    float _S44 = n_1.z;

#line 311
    int _S45 = int(256.0 * _S44);


    float _S46 = p_3.x;

#line 314
    int _S47 = floatBitsToInt(_S46);

#line 314
    int _S48;

#line 314
    if(_S46 < 0.0)
    {

#line 314
        _S48 = - _S41;

#line 314
    }
    else
    {

#line 314
        _S48 = _S41;

#line 314
    }

#line 314
    float _S49 = intBitsToFloat(_S47 + _S48);
    float _S50 = p_3.y;

#line 315
    int _S51 = floatBitsToInt(_S50);

#line 315
    if(_S50 < 0.0)
    {

#line 315
        _S48 = - _S43;

#line 315
    }
    else
    {

#line 315
        _S48 = _S43;

#line 315
    }

#line 315
    float _S52 = intBitsToFloat(_S51 + _S48);
    float _S53 = p_3.z;

#line 316
    int _S54 = floatBitsToInt(_S53);

#line 316
    if(_S53 < 0.0)
    {

#line 316
        _S48 = - _S45;

#line 316
    }
    else
    {

#line 316
        _S48 = _S45;

#line 316
    }

#line 316
    float _S55 = intBitsToFloat(_S54 + _S48);

#line 316
    float _S56;

    if((abs(_S46)) < 0.03125)
    {

#line 318
        _S56 = _S46 + 0.0000152587890625 * _S40;

#line 318
    }
    else
    {

#line 318
        _S56 = _S49;

#line 318
    }

#line 318
    float _S57;
    if((abs(_S50)) < 0.03125)
    {

#line 319
        _S57 = _S50 + 0.0000152587890625 * _S42;

#line 319
    }
    else
    {

#line 319
        _S57 = _S52;

#line 319
    }

#line 319
    float _S58;
    if((abs(_S53)) < 0.03125)
    {

#line 320
        _S58 = _S53 + 0.0000152587890625 * _S44;

#line 320
    }
    else
    {

#line 320
        _S58 = _S55;

#line 320
    }

#line 318
    return vec3(_S56, _S57, _S58);
}


#line 74 2
bool castShadowRay_0(vec3 hitPosition_3, vec3 surfaceNormal_2, vec3 directionToLight_0, float TMax_1)
{
    RayDesc_0 ray_1;
    ray_1.Origin_0 = offsetRay_0(hitPosition_3, surfaceNormal_2);
    ray_1.Direction_0 = directionToLight_0;
    ray_1.TMin_0 = 0.0;
    ray_1.TMax_0 = TMax_1;

    ShadowHitInfo_0 payload_1;
    payload_1.hasHit_0 = true;


    RayDesc_0 _S59 = ray_1;

#line 86
    p_0 = payload_1;

#line 86
    traceRayEXT(topLevelAS_0, 12U, 255U, 1U, 0U, 1U, _S59.Origin_0, _S59.TMin_0, _S59.Direction_0, _S59.TMax_0, 0);

#line 86
    payload_1 = p_0;

#line 96
    return !payload_1.hasHit_0;
}


#line 212
void main()
{
    uvec3 _S60 = ((gl_LaunchIDEXT));

#line 214
    uvec2 LaunchIndex_0 = _S60.xy;
    uvec3 _S61 = ((gl_LaunchSizeEXT));

#line 215
    uvec2 LaunchDimensions_0 = _S61.xy;


    vec2 d_0 = (vec2(LaunchIndex_0) + vec2(0.5)) / vec2(LaunchDimensions_0) * 2.0 - 1.0;

#line 224
    vec3 rayDir_0 = normalize((((vec4(normalize((((vec4(d_0.x, d_0.y, 1.0, 1.0)) * (unpackStorage_0(MyContantBuffer_0.giParams_0.projInverse_0)))).xyz), 0.0)) * (unpackStorage_0(MyContantBuffer_0.giParams_0.viewInverse_0)))).xyz);
    vec3 rayOri_0 = (((vec4(0.0, 0.0, 0.0, 1.0)) * (unpackStorage_0(MyContantBuffer_0.giParams_0.viewInverse_0)))).xyz;


    uvec4 rngState_3 = initRNG_0(LaunchIndex_0, LaunchDimensions_0, MyContantBuffer_0.giParams_0.frameCount_0);

#line 234
    const vec4 _S62 = vec4(0.0, 0.0, 0.0, 0.0);


    const vec4 _S63 = vec4(0.0, 0.0, 0.0, -1.0);

    SReservoir_0 _S64 = SReservoir_x24init_0(0.0, -1, 0U, 0.0, _S62);

    IndirectGbufferRayPayload_0 payload_2 = shootRay_0(rayOri_0, rayDir_0, 3.4028234663852886e+38);

#line 241
    vec4 albedoMetalnessOutput_0;

#line 241
    vec4 emissionRoughnessOutput_0;

#line 241
    vec4 normalOutput_0;

#line 241
    vec4 wposObjectIdOutput_0;

#line 241
    SReservoir_0 DI_RISreservoir_0;


    if(!IndirectGbufferRayPayload_hasHit_0(payload_2))
    {

#line 244
        albedoMetalnessOutput_0 = _S62;

#line 244
        emissionRoughnessOutput_0 = _S62;

#line 244
        normalOutput_0 = _S62;

#line 244
        wposObjectIdOutput_0 = _S63;

#line 244
        DI_RISreservoir_0 = _S64;

#line 244
    }
    else
    {

#line 252
        MaterialProperties_0 material_3 = loadMaterialProperties_0(payload_2);

        vec3 geometryNormal_1;
        vec3 shadingNormal_2;
        decodeNormals_0(payload_2.normal_0, geometryNormal_1, shadingNormal_2);

        vec3 V_4 = - rayDir_0;
        if((dot(geometryNormal_1, V_4)) < 0.0)
        {

#line 259
            geometryNormal_1 = - geometryNormal_1;

#line 259
        }
        if((dot(geometryNormal_1, shadingNormal_2)) < 0.0)
        {

#line 260
            shadingNormal_2 = - shadingNormal_2;

#line 260
        }

#line 260
        bool _S65;

#line 270
        if((dot(V_4, geometryNormal_1)) > 0.0)
        {

#line 270
            _S65 = (dot(V_4, shadingNormal_2)) < 0.0;

#line 270
        }
        else
        {

#line 270
            _S65 = false;

#line 270
        }

#line 270
        vec3 V_5;

#line 270
        if(_S65)
        {
            vec3 V_6 = normalize(V_4);


            shadingNormal_2 = normalize(0.00009999999747379 * V_6 + (shadingNormal_2 - dot(shadingNormal_2, V_6) * V_6));

#line 275
            V_5 = V_6;

#line 270
        }
        else
        {

#line 270
            V_5 = V_4;

#line 270
        }

#line 282
        float lightWeight_0 = -1.0;
        SReservoir_0 candidateReservoir_0 = _S64;
        vec3 _S66 = payload_2.position_objectID_0.xyz;

#line 281
        SLight_0 light_3;


        bool _S67 = sampleLightRIS_0(rngState_3, _S66, geometryNormal_1, light_3, lightWeight_0, material_3, shadingNormal_2, V_5, candidateReservoir_0);

#line 284
        if(_S67)
        {

            vec3 lightVector_2;
            float lightDistance_2;
            getLightData_0(light_3, _S66, lightVector_2, lightDistance_2);



            bool _S68 = castShadowRay_0(_S66, geometryNormal_1, normalize(lightVector_2), lightDistance_2);

#line 293
            if(_S68)
            {
            }
            else
            {

                candidateReservoir_0.finalWeight_0 = 0.0;

#line 293
            }

#line 284
        }

#line 304
        vec4 _S69 = vec4(material_3.emissive_0, material_3.roughness_0);

#line 304
        albedoMetalnessOutput_0 = vec4(material_3.baseColor_0, material_3.metalness_0);

#line 304
        emissionRoughnessOutput_0 = _S69;

#line 304
        normalOutput_0 = payload_2.normal_0;

#line 304
        wposObjectIdOutput_0 = payload_2.position_objectID_0;

#line 304
        DI_RISreservoir_0 = candidateReservoir_0;

#line 244
    }

#line 310
    ivec2 _S70 = ivec2(LaunchIndex_0);

#line 310
    imageStore((ptAlbedoMetalnessOutput_0), (_S70), albedoMetalnessOutput_0);
    imageStore((ptEmissionRoughnessOutput_0), (_S70), emissionRoughnessOutput_0);
    imageStore((ptNormalOutput_0), (_S70), normalOutput_0);
    imageStore((ptWposObjectIdOutput_0), (_S70), wposObjectIdOutput_0);

    reservoirInitBuffer_0._data[uint(LaunchIndex_0.y * (LaunchDimensions_0.x + 1U) + LaunchIndex_0.x)] = packStorage_0(DI_RISreservoir_0);
    return;
}

