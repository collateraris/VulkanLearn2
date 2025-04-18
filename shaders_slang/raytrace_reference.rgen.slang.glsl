#version 460
#extension GL_EXT_ray_tracing : require
layout(row_major) uniform;
layout(row_major) buffer;

#line 553 0
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


#line 17 2
layout(binding = 0, set = 1)
layout(std140) uniform block_SLANG_ParameterGroup_MyContantBuffer_std140_0
{
    SGlobalGIParams_std140_0 giParams_0;
}MyContantBuffer_0;

#line 11
layout(binding = 2)
uniform accelerationStructureEXT topLevelAS_0;


#line 87 1
struct SLight_std430_0
{
    vec4 position_0;
    vec4 direction_0;
    vec4 color_type_0;
};


#line 14 2
layout(std430, binding = 6) readonly buffer StructuredBuffer_SLight_std430_t_0 {
    SLight_std430_0 _data[];
} lightsBuffer_0;

#line 26
layout(rgba32f)
layout(binding = 1, set = 2)
uniform image2D accumulationBuffer_0;


#line 23
layout(rgba32f)
layout(binding = 0, set = 2)
uniform image2D ptOutput_0;


#line 75 1
struct ShadowHitInfo_0
{
    bool hasHit_0;
};


#line 16823 0
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


#line 16823 0
layout(location = 1)
rayPayloadEXT
IndirectGbufferRayPayload_0 p_1;


#line 176 1
uvec4 initRNG_0(uvec2 pixelCoords_0, uvec2 resolution_0, uint frameNumber_0)
{

#line 177
    return uvec4(pixelCoords_0.xy, frameNumber_0, 0U);
}


#line 16581 0
struct RayDesc_0
{
    vec3 Origin_0;
    float TMin_0;
    vec3 Direction_0;
    float TMax_0;
};


#line 29 2
IndirectGbufferRayPayload_0 shootRay_0(vec3 orig_0, vec3 dir_0)
{

    RayDesc_0 ray_0;
    ray_0.Origin_0 = orig_0;
    ray_0.Direction_0 = dir_0;
    ray_0.TMin_0 = 0.0;
    ray_0.TMax_0 = 3.4028234663852886e+38;

    RayDesc_0 _S1 = ray_0;

#line 38
    traceRayEXT(topLevelAS_0, 0U, 255U, 0U, 0U, 0U, _S1.Origin_0, _S1.TMin_0, _S1.Direction_0, _S1.TMax_0, 1);

#line 49
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


#line 79 2
MaterialProperties_0 loadMaterialProperties_0(IndirectGbufferRayPayload_0 payload_0)
{
    const vec3 _S2 = vec3(0.0, 0.0, 0.0);

#line 81
    MaterialProperties_0 result_0;

#line 81
    result_0.baseColor_0 = _S2;

#line 81
    result_0.metalness_0 = 0.0;

#line 81
    result_0.emissive_0 = _S2;

#line 81
    result_0.roughness_0 = 0.0;

#line 81
    result_0.transmissivness_0 = 0.0;

#line 81
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
    float _S3 = v_0.y;

#line 47
    float _S4 = 1.0 - abs(_S3);

#line 47
    float _S5 = v_0.x;

#line 47
    float _S6;

#line 47
    if(_S5 >= 0.0)
    {

#line 47
        _S6 = 1.0;

#line 47
    }
    else
    {

#line 47
        _S6 = -1.0;

#line 47
    }

#line 47
    float _S7 = _S4 * _S6;

#line 47
    float _S8 = 1.0 - abs(_S5);

#line 47
    if(_S3 >= 0.0)
    {

#line 47
        _S6 = 1.0;

#line 47
    }
    else
    {

#line 47
        _S6 = -1.0;

#line 47
    }

#line 47
    return vec2(_S7, _S8 * _S6);
}


#line 57
vec3 decodeNormalOctahedron_0(vec2 p_2)
{
    float _S9 = p_2.x;

#line 59
    float _S10 = p_2.y;

#line 59
    float _S11 = 1.0 - abs(_S9) - abs(_S10);

#line 59
    vec3 n_0 = vec3(_S9, _S10, _S11);

#line 59
    vec2 tmp_0;
    if(_S11 < 0.0)
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


#line 130
uvec4 pcg4d_0(uvec4 v_1)
{
    uvec4 _S12 = v_1 * 1664525U + 1013904223U;

#line 130
    uvec4 _S13 = _S12;



    _S13[0] = _S13[0] + _S12.y * _S12.w;
    _S13[1] = _S13[1] + _S13.z * _S13.x;
    _S13[2] = _S13[2] + _S13.x * _S13.y;
    _S13[3] = _S13[3] + _S13.y * _S13.z;

    uvec4 _S14 = _S13 ^ (_S13 >> 16U);

#line 139
    _S13 = _S14;

    _S13[0] = _S13[0] + _S14.y * _S14.w;
    _S13[1] = _S13[1] + _S13.z * _S13.x;
    _S13[2] = _S13[2] + _S13.x * _S13.y;
    _S13[3] = _S13[3] + _S13.y * _S13.z;

    return _S13;
}


#line 169
float uintToFloat_0(uint x_0)
{

#line 170
    return uintBitsToFloat(1065353216U | (x_0 >> 9)) - 1.0;
}


#line 181
float rand_0(inout uvec4 rngState_0)
{

#line 182
    rngState_0[3] = rngState_0[3] + 1U;
    return uintToFloat_0(pcg4d_0(rngState_0).x);
}


#line 83
struct SLight_0
{
    vec4 position_0;
    vec4 direction_0;
    vec4 color_type_0;
};


#line 91 2
bool sampleLightUniform_0(inout uvec4 rngState_1, vec3 hitPosition_0, vec3 surfaceNormal_0, out SLight_0 light_0, out float lightSampleWeight_0)
{
    if((MyContantBuffer_0.giParams_0.lightsCount_0) == 0U)
    {

#line 93
        return false;
    }
    uint _S15 = MyContantBuffer_0.giParams_0.lightsCount_0 - 1U;

#line 95
    float _S16 = rand_0(rngState_1);

#line 95
    vec4 _S17 = lightsBuffer_0._data[uint(min(_S15, uint(_S16 * float(MyContantBuffer_0.giParams_0.lightsCount_0))))].direction_0;

#line 95
    vec4 _S18 = lightsBuffer_0._data[uint(min(_S15, uint(_S16 * float(MyContantBuffer_0.giParams_0.lightsCount_0))))].color_type_0;

#line 95
    light_0.position_0 = lightsBuffer_0._data[uint(min(_S15, uint(_S16 * float(MyContantBuffer_0.giParams_0.lightsCount_0))))].position_0;

#line 95
    light_0.direction_0 = _S17;

#line 95
    light_0.color_type_0 = _S18;



    lightSampleWeight_0 = float(MyContantBuffer_0.giParams_0.lightsCount_0);

    return true;
}


#line 260 1
void getLightData_0(SLight_0 light_1, vec3 hitPosition_1, out vec3 lightVector_0, out float lightDistance_0)
{

#line 261
    float _S19 = light_1.color_type_0.w;

#line 261
    if((abs(_S19 - 2.0)) < 9.99999997475242708e-07)
    {

#line 262
        vec3 _S20 = light_1.position_0.xyz - hitPosition_1;

#line 262
        lightVector_0 = _S20;
        lightDistance_0 = length(_S20);

#line 261
    }
    else
    {
        if((abs(_S19 - 1.0)) < 9.99999997475242708e-07)
        {

#line 265
            lightVector_0 = - light_1.direction_0.xyz;
            lightDistance_0 = 3.4028234663852886e+38;

#line 264
        }
        else
        {

            lightDistance_0 = 3.4028234663852886e+38;
            lightVector_0 = vec3(0.0, 1.0, 0.0);

#line 264
        }

#line 261
    }

#line 271
    return;
}

vec3 getLightIntensityAtPoint_0(SLight_0 light_2, float distance_0)
{

#line 275
    float _S21 = light_2.color_type_0.w;

#line 275
    if((abs(_S21 - 2.0)) < 9.99999997475242708e-07)
    {

#line 281
        float _S22 = distance_0 * distance_0 + 0.25;

        return light_2.color_type_0.xyz * (2.0 / (_S22 + distance_0 * sqrt(_S22)));
    }
    else
    {

#line 285
        if((abs(_S21 - 1.0)) < 9.99999997475242708e-07)
        {

#line 286
            return light_2.color_type_0.xyz;
        }
        else
        {

#line 288
            return vec3(1.0, 0.0, 1.0);
        }

#line 288
    }

#line 288
}


#line 238 3
float luminance_0(vec3 rgb_0)
{
    return dot(rgb_0, vec3(0.2125999927520752, 0.71520000696182251, 0.07220000028610229));
}


#line 105 2
bool sampleLightRIS_0(inout uvec4 rngState_2, vec3 hitPosition_2, vec3 surfaceNormal_1, out SLight_0 selectedSample_0, out float lightSampleWeight_1)
{
    if((MyContantBuffer_0.giParams_0.lightsCount_0) == 0U)
    {

#line 107
        return false;
    }
    const vec4 _S23 = vec4(0.0, 0.0, 0.0, 0.0);

#line 109
    selectedSample_0.position_0 = _S23;

#line 109
    selectedSample_0.direction_0 = _S23;

#line 109
    selectedSample_0.color_type_0 = _S23;

#line 109
    float samplePdfG_0 = 0.0;

#line 109
    int i_0 = 0;

#line 109
    float totalWeights_0 = 0.0;



    for(;;)
    {

#line 113
        if(i_0 < 8)
        {
        }
        else
        {

#line 113
            break;
        }
        float candidateWeight_0;
        SLight_0 candidate_0;
        bool _S24 = sampleLightUniform_0(rngState_2, hitPosition_2, surfaceNormal_1, candidate_0, candidateWeight_0);

#line 117
        if(_S24)
        {
            vec3 lightVector_1;
            float lightDistance_1;
            getLightData_0(candidate_0, hitPosition_2, lightVector_1, lightDistance_1);



            float candidatePdfG_0 = luminance_0(getLightIntensityAtPoint_0(candidate_0, length(lightVector_1)));
            float candidateRISWeight_0 = candidatePdfG_0 * candidateWeight_0;

            float totalWeights_1 = totalWeights_0 + candidateRISWeight_0;
            float _S25 = rand_0(rngState_2);

#line 129
            float samplePdfG_1;

#line 129
            if(_S25 < (candidateRISWeight_0 / totalWeights_1))
            {

#line 130
                selectedSample_0 = candidate_0;

#line 130
                samplePdfG_1 = candidatePdfG_0;

#line 129
            }
            else
            {

#line 129
                samplePdfG_1 = samplePdfG_0;

#line 129
            }

#line 129
            samplePdfG_0 = samplePdfG_1;

#line 129
            totalWeights_0 = totalWeights_1;

#line 117
        }

#line 113
        i_0 = i_0 + 1;

#line 113
    }

#line 136
    if(totalWeights_0 == 0.0)
    {

#line 137
        return false;
    }
    else
    {

#line 139
        lightSampleWeight_1 = totalWeights_0 / 8.0 / samplePdfG_0;
        return true;
    }

#line 140
}


#line 236 1
vec3 offsetRay_0(vec3 p_3, vec3 n_1)
{

#line 242
    float _S26 = n_1.x;

#line 242
    int _S27 = int(256.0 * _S26);

#line 242
    float _S28 = n_1.y;

#line 242
    int _S29 = int(256.0 * _S28);

#line 242
    float _S30 = n_1.z;

#line 242
    int _S31 = int(256.0 * _S30);


    float _S32 = p_3.x;

#line 245
    int _S33 = floatBitsToInt(_S32);

#line 245
    int _S34;

#line 245
    if(_S32 < 0.0)
    {

#line 245
        _S34 = - _S27;

#line 245
    }
    else
    {

#line 245
        _S34 = _S27;

#line 245
    }

#line 245
    float _S35 = intBitsToFloat(_S33 + _S34);
    float _S36 = p_3.y;

#line 246
    int _S37 = floatBitsToInt(_S36);

#line 246
    if(_S36 < 0.0)
    {

#line 246
        _S34 = - _S29;

#line 246
    }
    else
    {

#line 246
        _S34 = _S29;

#line 246
    }

#line 246
    float _S38 = intBitsToFloat(_S37 + _S34);
    float _S39 = p_3.z;

#line 247
    int _S40 = floatBitsToInt(_S39);

#line 247
    if(_S39 < 0.0)
    {

#line 247
        _S34 = - _S31;

#line 247
    }
    else
    {

#line 247
        _S34 = _S31;

#line 247
    }

#line 247
    float _S41 = intBitsToFloat(_S40 + _S34);

#line 247
    float _S42;

    if((abs(_S32)) < 0.03125)
    {

#line 249
        _S42 = _S32 + 0.0000152587890625 * _S26;

#line 249
    }
    else
    {

#line 249
        _S42 = _S35;

#line 249
    }

#line 249
    float _S43;
    if((abs(_S36)) < 0.03125)
    {

#line 250
        _S43 = _S36 + 0.0000152587890625 * _S28;

#line 250
    }
    else
    {

#line 250
        _S43 = _S38;

#line 250
    }

#line 250
    float _S44;
    if((abs(_S39)) < 0.03125)
    {

#line 251
        _S44 = _S39 + 0.0000152587890625 * _S30;

#line 251
    }
    else
    {

#line 251
        _S44 = _S41;

#line 251
    }

#line 249
    return vec3(_S42, _S43, _S44);
}


#line 54 2
bool castShadowRay_0(vec3 hitPosition_3, vec3 surfaceNormal_2, vec3 directionToLight_0, float TMax_1)
{
    RayDesc_0 ray_1;
    ray_1.Origin_0 = offsetRay_0(hitPosition_3, surfaceNormal_2);
    ray_1.Direction_0 = directionToLight_0;
    ray_1.TMin_0 = 0.0;
    ray_1.TMax_0 = TMax_1;

    ShadowHitInfo_0 payload_1;
    payload_1.hasHit_0 = true;


    RayDesc_0 _S45 = ray_1;

#line 66
    p_0 = payload_1;

#line 66
    traceRayEXT(topLevelAS_0, 12U, 255U, 1U, 0U, 1U, _S45.Origin_0, _S45.TMin_0, _S45.Direction_0, _S45.TMax_0, 0);

#line 66
    payload_1 = p_0;

#line 76
    return !payload_1.hasHit_0;
}


#line 12620 0
float saturate_0(float x_1)
{

#line 12628
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
    float _S46 = material_0.roughness_0 * material_0.roughness_0;

#line 866
    data_1.alpha_0 = _S46;
    data_1.alphaSquared_0 = _S46 * _S46;


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


#line 397
vec3 evalLambertian_0(BrdfData_0 data_3)
{

#line 398
    return data_3.diffuseReflectance_0 * (0.31830987334251404 * data_3.NdotL_0);
}


#line 876
vec3 evalCombinedBRDF_0(vec3 N_2, vec3 L_2, vec3 V_2, MaterialProperties_0 material_1)
{

    BrdfData_0 data_4 = prepareBRDFData_0(N_2, L_2, V_2, material_1);

#line 879
    bool _S47;


    if(data_4.Vbackfacing_0)
    {

#line 882
        _S47 = true;

#line 882
    }
    else
    {

#line 882
        _S47 = data_4.Lbackfacing_0;

#line 882
    }

#line 882
    if(_S47)
    {

#line 882
        return vec3(0.0, 0.0, 0.0);
    }

#line 891
    return (vec3(1.0, 1.0, 1.0) - data_4.F_0) * evalLambertian_0(data_4) + evalMicrofacet_0(data_4);
}


#line 208 1
vec3 getPerpendicularVector_0(vec3 u_0)
{
    vec3 a_0 = abs(u_0);
    float _S48 = a_0.x;

#line 211
    float _S49 = a_0.y;

#line 211
    bool _S50;

#line 211
    if((_S48 - _S49) < 0.0)
    {

#line 211
        _S50 = (_S48 - a_0.z) < 0.0;

#line 211
    }
    else
    {

#line 211
        _S50 = false;

#line 211
    }

#line 211
    int _S51;

#line 211
    if(_S50)
    {

#line 211
        _S51 = 1;

#line 211
    }
    else
    {

#line 211
        _S51 = 0;

#line 211
    }

#line 211
    uint xm_0 = uint(_S51);

#line 211
    uint ym_0;
    if((_S49 - a_0.z) < 0.0)
    {

#line 212
        ym_0 = 1U ^ xm_0;

#line 212
    }
    else
    {

#line 212
        ym_0 = 0U;

#line 212
    }

    return cross(u_0, vec3(float(xm_0), float(ym_0), float(1U ^ (xm_0 | ym_0))));
}


vec3 getCosHemisphereSample_0(inout uvec4 randSeed_0, vec3 hitNorm_0)
{

    float _S52 = rand_0(randSeed_0);

#line 221
    float _S53 = rand_0(randSeed_0);


    vec3 bitangent_0 = getPerpendicularVector_0(hitNorm_0);

    float r_0 = sqrt(_S52);
    float phi_0 = 6.28318548202514648 * _S53;


    return cross(bitangent_0, hitNorm_0) * (r_0 * cos(phi_0)) + bitangent_0 * (r_0 * sin(phi_0)) + hitNorm_0.xyz * sqrt(1.0 - _S52);
}


#line 38
bool IndirectGbufferRayPayload_hasEmissive_0(IndirectGbufferRayPayload_0 this_1)
{
    vec3 _S54 = this_1.emission_roughness_0.xyz;

#line 40
    return bool(dot(_S54, _S54));
}


#line 145 2
float getBrdfProbability_0(MaterialProperties_0 material_2, vec3 V_3, vec3 shadingNormal_1)
{

#line 151
    vec3 _S55 = vec3(luminance_0(baseColorToSpecularF0_0(material_2.baseColor_0, material_2.metalness_0)));

#line 151
    float Fresnel_0 = saturate_0(luminance_0(evalFresnel_0(_S55, shadowedF90_0(_S55), max(0.0, dot(V_3, shadingNormal_1)))));

#line 161
    return clamp(Fresnel_0 / max(0.00009999999747379, Fresnel_0 + luminance_0(baseColorToDiffuseReflectance_0(material_2.baseColor_0, material_2.metalness_0)) * (1.0 - Fresnel_0)), 0.10000000149011612, 0.89999997615814209);
}


#line 280 3
vec4 getRotationToZAxis_0(vec3 input_0)
{

    float _S56 = input_0.z;

#line 283
    if(_S56 < -0.99998998641967773)
    {

#line 283
        return vec4(1.0, 0.0, 0.0, 0.0);
    }
    return normalize(vec4(input_0.y, - input_0.x, 0.0, 1.0 + _S56));
}


#line 306
vec3 rotatePoint_0(vec4 q_0, vec3 v_2)
{

#line 307
    vec3 qAxis_0 = vec3(q_0.x, q_0.y, q_0.z);
    float _S57 = q_0.w;

#line 308
    return 2.0 * dot(qAxis_0, v_2) * qAxis_0 + (_S57 * _S57 - dot(qAxis_0, qAxis_0)) * v_2 + 2.0 * _S57 * cross(qAxis_0, v_2);
}


#line 317
vec3 sampleHemisphere_0(vec2 u_1, out float pdf_0)
{
    float _S58 = u_1.x;

#line 319
    float a_1 = sqrt(_S58);
    float b_1 = 6.28318548202514648 * u_1.y;

#line 325
    float _S59 = sqrt(1.0 - _S58);

#line 322
    vec3 result_1 = vec3(a_1 * cos(b_1), a_1 * sin(b_1), _S59);

#line 327
    pdf_0 = _S59 * 0.31830987334251404;

    return result_1;
}

vec3 sampleHemisphere_1(vec2 u_2)
{

#line 333
    float pdf_1;
    return sampleHemisphere_0(u_2, pdf_1);
}


#line 393
float lambertian_0(BrdfData_0 data_5)
{

#line 394
    return 1.0;
}


#line 703
vec3 sampleGGXVNDF_0(vec3 Ve_0, vec2 alpha2D_0, vec2 u_3)
{

    float _S60 = alpha2D_0.x;

#line 706
    float _S61 = alpha2D_0.y;

#line 706
    vec3 Vh_0 = normalize(vec3(_S60 * Ve_0.x, _S61 * Ve_0.y, Ve_0.z));


    float _S62 = Vh_0.x;

#line 709
    float _S63 = Vh_0.y;

#line 709
    float lensq_0 = _S62 * _S62 + _S63 * _S63;

#line 709
    vec3 T1_0;
    if(lensq_0 > 0.0)
    {

#line 710
        T1_0 = vec3(- _S63, _S62, 0.0) * (inversesqrt((lensq_0)));

#line 710
    }
    else
    {

#line 710
        T1_0 = vec3(1.0, 0.0, 0.0);

#line 710
    }



    float r_1 = sqrt(u_3.x);
    float phi_1 = 6.28318548202514648 * u_3.y;
    float t1_0 = r_1 * cos(phi_1);


    float _S64 = 1.0 - t1_0 * t1_0;

#line 719
    float t2_0 = mix(sqrt(_S64), r_1 * sin(phi_1), 0.5 * (1.0 + Vh_0.z));


    vec3 Nh_0 = t1_0 * T1_0 + t2_0 * cross(Vh_0, T1_0) + sqrt(max(0.0, _S64 - t2_0 * t2_0)) * Vh_0;


    return normalize(vec3(_S60 * Nh_0.x, _S61 * Nh_0.y, max(0.0, Nh_0.z)));
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
bool evalIndirectCombinedBRDF_0(vec2 u_5, vec3 shadingNormal_2, vec3 geometryNormal_1, vec3 V_4, MaterialProperties_0 material_3, int brdfType_0, out vec3 rayDirection_0, out vec3 sampleWeight_0)
{

    if((dot(shadingNormal_2, V_4)) <= 0.0)
    {

#line 901
        return false;
    }


    vec4 qRotationToZ_0 = getRotationToZAxis_0(shadingNormal_2);
    vec3 Vlocal_1 = rotatePoint_0(qRotationToZ_0, V_4);
    const vec3 Nlocal_1 = vec3(0.0, 0.0, 1.0);

    const vec3 _S65 = vec3(0.0, 0.0, 0.0);

#line 909
    vec3 rayDirectionLocal_0;

    if(brdfType_0 == 1)
    {

        vec3 rayDirectionLocal_1 = sampleHemisphere_1(u_5);
        BrdfData_0 data_6 = prepareBRDFData_0(Nlocal_1, rayDirectionLocal_1, Vlocal_1, material_3);

#line 915
        float _S66 = data_6.alpha_0;

#line 926
        sampleWeight_0 = data_6.diffuseReflectance_0 * lambertian_0(data_6) * (vec3(1.0, 1.0, 1.0) - evalFresnel_0(data_6.specularF0_0, shadowedF90_0(data_6.specularF0_0), max(0.00000999999974738, min(1.0, dot(Vlocal_1, sampleGGXVNDF_0(Vlocal_1, vec2(_S66, _S66), u_5))))));

#line 926
        rayDirectionLocal_0 = rayDirectionLocal_1;

#line 911
    }
    else
    {

#line 929
        if(brdfType_0 == 2)
        {

#line 930
            BrdfData_0 data_7 = prepareBRDFData_0(Nlocal_1, Nlocal_1, Vlocal_1, material_3);
            vec3 _S67 = sampleSpecularMicrofacet_0(Vlocal_1, data_7.alpha_0, data_7.alphaSquared_0, data_7.specularF0_0, u_5, sampleWeight_0);

#line 931
            rayDirectionLocal_0 = _S67;

#line 929
        }
        else
        {

#line 929
            rayDirectionLocal_0 = _S65;

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

    vec3 _S68 = normalize(rotatePoint_0(invertRotation_0(qRotationToZ_0), rayDirectionLocal_0));

#line 938
    rayDirection_0 = _S68;


    if((dot(geometryNormal_1, _S68)) <= 0.0)
    {

#line 941
        return false;
    }
    return true;
}


#line 166 2
void main()
{

#line 166
    vec3 accumulatedColor_0;

#line 166
    vec3 radiance_0;

    uvec3 _S69 = ((gl_LaunchIDEXT));

#line 168
    uvec2 LaunchIndex_0 = _S69.xy;
    uvec3 _S70 = ((gl_LaunchSizeEXT));

#line 169
    uvec2 LaunchDimensions_0 = _S70.xy;


    vec2 d_0 = (vec2(LaunchIndex_0) + vec2(0.5)) / vec2(LaunchDimensions_0) * 2.0 - 1.0;

    vec4 origin_0 = (((vec4(0.0, 0.0, 0.0, 1.0)) * (mat4x4(MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][3]))));



    vec3 rayDir_0 = (((vec4(normalize((((vec4(d_0.x, d_0.y, 1.0, 1.0)) * (mat4x4(MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][3], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][3], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][3], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][3])))).xyz), 0.0)) * (mat4x4(MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][3])))).xyz;
    vec3 _S71 = origin_0.xyz;


    uvec4 rngState_3 = initRNG_0(LaunchIndex_0, LaunchDimensions_0, MyContantBuffer_0.giParams_0.frameCount_0);


    const vec3 _S72 = vec3(0.0, 0.0, 0.0);
    const vec3 _S73 = vec3(1.0, 1.0, 1.0);

#line 186
    vec3 rayOri_0 = _S71;

#line 186
    int bounce_0 = 0;

#line 186
    vec3 throughput_0 = _S73;

#line 186
    vec3 radiance_1 = _S72;

    for(;;)
    {

#line 188
        if(uint(bounce_0) <= (MyContantBuffer_0.giParams_0.numRays_0))
        {
        }
        else
        {

#line 188
            radiance_0 = radiance_1;

#line 188
            break;
        }
        IndirectGbufferRayPayload_0 payload_2 = shootRay_0(rayOri_0, rayDir_0);

        if(!IndirectGbufferRayPayload_hasHit_0(payload_2))
        {

#line 192
            radiance_0 = radiance_1 + throughput_0 * _S72;


            break;
        }


        MaterialProperties_0 material_4 = loadMaterialProperties_0(payload_2);

        vec3 geometryNormal_2;
        vec3 shadingNormal_3;
        decodeNormals_0(payload_2.normal_0, geometryNormal_2, shadingNormal_3);

        vec3 V_5 = - rayDir_0;



        vec3 radiance_2 = radiance_1 + throughput_0 * material_4.emissive_0;

#line 217
        vec3 _S74 = payload_2.position_objectID_0.xyz;

#line 215
        SLight_0 light_3;
        float lightWeight_0;
        bool _S75 = sampleLightRIS_0(rngState_3, _S74, geometryNormal_2, light_3, lightWeight_0);

#line 217
        if(_S75)
        {

            vec3 lightVector_2;
            float lightDistance_2;
            getLightData_0(light_3, _S74, lightVector_2, lightDistance_2);
            vec3 L_3 = normalize(lightVector_2);


            bool _S76 = castShadowRay_0(_S74, geometryNormal_2, L_3, lightDistance_2);

#line 226
            if(_S76)
            {

#line 226
                radiance_0 = radiance_2 + throughput_0 * evalCombinedBRDF_0(shadingNormal_3, L_3, V_5, material_4) * (getLightIntensityAtPoint_0(light_3, lightDistance_2) * lightWeight_0);

#line 226
            }
            else
            {

#line 226
                radiance_0 = radiance_2;

#line 226
            }

#line 217
        }
        else
        {

#line 217
            radiance_0 = radiance_2;

#line 217
        }

#line 217
        uint i_1 = 0U;

#line 217
        accumulatedColor_0 = radiance_0;

#line 235
        for(;;)
        {

#line 235
            if(i_1 < 10U)
            {
            }
            else
            {

#line 235
                break;
            }
            vec3 emissiveLightDir_0 = getCosHemisphereSample_0(rngState_3, geometryNormal_2);
            IndirectGbufferRayPayload_0 payloadEmissive_0 = shootRay_0(_S74, emissiveLightDir_0);
            if(IndirectGbufferRayPayload_hasEmissive_0(payloadEmissive_0))
            {
                vec3 lightVector_3 = payloadEmissive_0.position_objectID_0.xyz - _S74;
                float lightDistance_3 = length(lightVector_3);

#line 249
                float _S77 = lightDistance_3 * lightDistance_3 + 0.25;

#line 249
                accumulatedColor_0 = accumulatedColor_0 + throughput_0 * evalCombinedBRDF_0(shadingNormal_3, normalize(lightVector_3), V_5, material_4) * (payloadEmissive_0.emission_roughness_0.xyz * (2.0 / (_S77 + lightDistance_3 * sqrt(_S77))) * 10000.0);

#line 239
            }

#line 235
            i_1 = i_1 + 1U;

#line 235
        }

#line 235
        vec3 throughput_1;

#line 257
        if(bounce_0 > 3)
        {

#line 257
            vec3 _S78;
            float rrProbability_0 = min(0.94999998807907104, luminance_0(throughput_0));
            float _S79 = rand_0(rngState_3);

#line 259
            if(rrProbability_0 < _S79)
            {

#line 259
                radiance_0 = accumulatedColor_0;

#line 259
                break;
            }
            else
            {

#line 259
                _S78 = throughput_0 / rrProbability_0;

#line 259
            }

#line 259
            throughput_1 = _S78;

#line 257
        }
        else
        {

#line 257
            throughput_1 = throughput_0;

#line 257
        }

#line 257
        bool _S80;

#line 266
        if((material_4.metalness_0) == 1.0)
        {

#line 266
            _S80 = (material_4.roughness_0) == 0.0;

#line 266
        }
        else
        {

#line 266
            _S80 = false;

#line 266
        }

#line 266
        vec3 throughput_2;

#line 266
        int brdfType_1;

#line 266
        if(_S80)
        {

#line 266
            brdfType_1 = 2;

#line 266
            throughput_2 = throughput_1;

#line 266
        }
        else
        {



            float brdfProbability_0 = getBrdfProbability_0(material_4, V_5, shadingNormal_3);

            float _S81 = rand_0(rngState_3);

#line 274
            if(_S81 < brdfProbability_0)
            {
                vec3 throughput_3 = throughput_1 / brdfProbability_0;

#line 276
                brdfType_1 = 2;

#line 276
                throughput_2 = throughput_3;

#line 274
            }
            else
            {


                vec3 throughput_4 = throughput_1 / (1.0 - brdfProbability_0);

#line 279
                brdfType_1 = 1;

#line 279
                throughput_2 = throughput_4;

#line 274
            }

#line 266
        }

#line 285
        float _S82 = rand_0(rngState_3);

#line 285
        float _S83 = rand_0(rngState_3);

#line 284
        vec3 brdfWeight_0;

        bool _S84 = evalIndirectCombinedBRDF_0(vec2(_S82, _S83), shadingNormal_3, geometryNormal_2, V_5, material_4, brdfType_1, rayDir_0, brdfWeight_0);

#line 286
        if(!_S84)
        {

#line 286
            radiance_0 = accumulatedColor_0;
            break;
        }


        vec3 throughput_5 = throughput_2 * brdfWeight_0;


        vec3 _S85 = offsetRay_0(_S74, geometryNormal_2);

#line 188
        int _S86 = bounce_0 + 1;

#line 188
        rayOri_0 = _S85;

#line 188
        bounce_0 = _S86;

#line 188
        throughput_0 = throughput_5;

#line 188
        radiance_1 = accumulatedColor_0;

#line 188
    }

#line 298
    ivec2 _S87 = ivec2(LaunchIndex_0);

#line 298
    vec4 _S88 = (imageLoad((accumulationBuffer_0), (_S87)));

#line 298
    vec3 previousColor_0 = _S88.xyz;

    if(bool(MyContantBuffer_0.giParams_0.enableAccumulation_0))
    {

#line 300
        accumulatedColor_0 = previousColor_0 + radiance_0;

#line 300
    }
    else
    {

#line 300
        accumulatedColor_0 = radiance_0;

#line 300
    }
    imageStore((accumulationBuffer_0), (_S87), vec4(accumulatedColor_0, 1.0));

    imageStore((ptOutput_0), (_S87), vec4(radiance_0, 1.0));
    return;
}

