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


#line 51 1
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


#line 39 1
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


#line 139 1
uvec4 initRNG_0(uvec2 pixelCoords_0, uvec2 resolution_0, uint frameNumber_0)
{

#line 140
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


#line 93 1
uvec4 pcg4d_0(uvec4 v_0)
{
    uvec4 _S3 = v_0 * 1664525U + 1013904223U;

#line 93
    uvec4 _S4 = _S3;



    _S4[0] = _S4[0] + _S3.y * _S3.w;
    _S4[1] = _S4[1] + _S4.z * _S4.x;
    _S4[2] = _S4[2] + _S4.x * _S4.y;
    _S4[3] = _S4[3] + _S4.y * _S4.z;

    uvec4 _S5 = _S4 ^ (_S4 >> 16U);

#line 102
    _S4 = _S5;

    _S4[0] = _S4[0] + _S5.y * _S5.w;
    _S4[1] = _S4[1] + _S4.z * _S4.x;
    _S4[2] = _S4[2] + _S4.x * _S4.y;
    _S4[3] = _S4[3] + _S4.y * _S4.z;

    return _S4;
}


#line 132
float uintToFloat_0(uint x_0)
{

#line 133
    return uintBitsToFloat(1065353216U | (x_0 >> 9)) - 1.0;
}


#line 144
float rand_0(inout uvec4 rngState_0)
{

#line 145
    rngState_0[3] = rngState_0[3] + 1U;
    return uintToFloat_0(pcg4d_0(rngState_0).x);
}


#line 47
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
    uint _S6 = MyContantBuffer_0.giParams_0.lightsCount_0 - 1U;

#line 95
    float _S7 = rand_0(rngState_1);

#line 95
    vec4 _S8 = lightsBuffer_0._data[uint(min(_S6, uint(_S7 * float(MyContantBuffer_0.giParams_0.lightsCount_0))))].direction_0;

#line 95
    vec4 _S9 = lightsBuffer_0._data[uint(min(_S6, uint(_S7 * float(MyContantBuffer_0.giParams_0.lightsCount_0))))].color_type_0;

#line 95
    light_0.position_0 = lightsBuffer_0._data[uint(min(_S6, uint(_S7 * float(MyContantBuffer_0.giParams_0.lightsCount_0))))].position_0;

#line 95
    light_0.direction_0 = _S8;

#line 95
    light_0.color_type_0 = _S9;



    lightSampleWeight_0 = float(MyContantBuffer_0.giParams_0.lightsCount_0);

    return true;
}


#line 196 1
void getLightData_0(SLight_0 light_1, vec3 hitPosition_1, out vec3 lightVector_0, out float lightDistance_0)
{

#line 197
    float _S10 = light_1.color_type_0.w;

#line 197
    if((abs(_S10 - 2.0)) < 9.99999997475242708e-07)
    {

#line 198
        vec3 _S11 = light_1.position_0.xyz - hitPosition_1;

#line 198
        lightVector_0 = _S11;
        lightDistance_0 = length(_S11);

#line 197
    }
    else
    {
        if((abs(_S10 - 1.0)) < 9.99999997475242708e-07)
        {

#line 201
            lightVector_0 = - light_1.direction_0.xyz;
            lightDistance_0 = 3.4028234663852886e+38;

#line 200
        }
        else
        {

            lightDistance_0 = 3.4028234663852886e+38;
            lightVector_0 = vec3(0.0, 1.0, 0.0);

#line 200
        }

#line 197
    }

#line 207
    return;
}

vec3 getLightIntensityAtPoint_0(SLight_0 light_2, float distance_0)
{

#line 211
    float _S12 = light_2.color_type_0.w;

#line 211
    if((abs(_S12 - 2.0)) < 9.99999997475242708e-07)
    {

#line 217
        float _S13 = distance_0 * distance_0 + 0.25;

        return light_2.color_type_0.xyz * (2.0 / (_S13 + distance_0 * sqrt(_S13)));
    }
    else
    {

#line 221
        if((abs(_S12 - 1.0)) < 9.99999997475242708e-07)
        {

#line 222
            return light_2.color_type_0.xyz;
        }
        else
        {

#line 224
            return vec3(1.0, 0.0, 1.0);
        }

#line 224
    }

#line 224
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
    const vec4 _S14 = vec4(0.0, 0.0, 0.0, 0.0);

#line 109
    selectedSample_0.position_0 = _S14;

#line 109
    selectedSample_0.direction_0 = _S14;

#line 109
    selectedSample_0.color_type_0 = _S14;

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
        bool _S15 = sampleLightUniform_0(rngState_2, hitPosition_2, surfaceNormal_1, candidate_0, candidateWeight_0);

#line 117
        float samplePdfG_1;

#line 117
        float totalWeights_1;

#line 117
        if(_S15)
        {
            vec3 lightVector_1;
            float lightDistance_1;
            getLightData_0(candidate_0, hitPosition_2, lightVector_1, lightDistance_1);



            if((dot(surfaceNormal_1, normalize(lightVector_1))) < 0.00000999999974738)
            {

#line 125
                samplePdfG_1 = samplePdfG_0;

#line 113
                int _S16 = i_0 + 1;

#line 113
                samplePdfG_0 = samplePdfG_1;

#line 113
                i_0 = _S16;

#line 113
                continue;
            }

#line 127
            float candidatePdfG_0 = luminance_0(getLightIntensityAtPoint_0(candidate_0, length(lightVector_1)));
            float candidateRISWeight_0 = candidatePdfG_0 * candidateWeight_0;

            float totalWeights_2 = totalWeights_0 + candidateRISWeight_0;
            float _S17 = rand_0(rngState_2);

#line 131
            if(_S17 < (candidateRISWeight_0 / totalWeights_2))
            {

#line 132
                selectedSample_0 = candidate_0;

#line 132
                samplePdfG_1 = candidatePdfG_0;

#line 131
            }
            else
            {

#line 131
                samplePdfG_1 = samplePdfG_0;

#line 131
            }

#line 131
            totalWeights_1 = totalWeights_2;

#line 117
        }
        else
        {

#line 117
            samplePdfG_1 = samplePdfG_0;

#line 117
            totalWeights_1 = totalWeights_0;

#line 117
        }

#line 117
        totalWeights_0 = totalWeights_1;

#line 113
        int _S16 = i_0 + 1;

#line 113
        samplePdfG_0 = samplePdfG_1;

#line 113
        i_0 = _S16;

#line 113
    }

#line 138
    if(totalWeights_0 == 0.0)
    {

#line 139
        return false;
    }
    else
    {

#line 141
        lightSampleWeight_1 = totalWeights_0 / 8.0 / samplePdfG_0;
        return true;
    }

#line 142
}


#line 54
bool castShadowRay_0(vec3 hitPosition_3, vec3 surfaceNormal_2, vec3 directionToLight_0, float TMax_1)
{
    RayDesc_0 ray_1;
    ray_1.Origin_0 = hitPosition_3;
    ray_1.Direction_0 = directionToLight_0;
    ray_1.TMin_0 = 0.0;
    ray_1.TMax_0 = TMax_1;

    ShadowHitInfo_0 payload_1;
    payload_1.hasHit_0 = true;


    RayDesc_0 _S18 = ray_1;

#line 66
    p_0 = payload_1;

#line 66
    traceRayEXT(topLevelAS_0, 12U, 255U, 1U, 0U, 1U, _S18.Origin_0, _S18.TMin_0, _S18.Direction_0, _S18.TMax_0, 0);

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
    float _S19 = material_0.roughness_0 * material_0.roughness_0;

#line 866
    data_1.alpha_0 = _S19;
    data_1.alphaSquared_0 = _S19 * _S19;


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
    bool _S20;


    if(data_4.Vbackfacing_0)
    {

#line 882
        _S20 = true;

#line 882
    }
    else
    {

#line 882
        _S20 = data_4.Lbackfacing_0;

#line 882
    }

#line 882
    if(_S20)
    {

#line 882
        return vec3(0.0, 0.0, 0.0);
    }

#line 891
    return (vec3(1.0, 1.0, 1.0) - data_4.F_0) * evalLambertian_0(data_4) + evalMicrofacet_0(data_4);
}


#line 147 2
float getBrdfProbability_0(MaterialProperties_0 material_2, vec3 V_3, vec3 shadingNormal_0)
{

#line 153
    vec3 _S21 = vec3(luminance_0(baseColorToSpecularF0_0(material_2.baseColor_0, material_2.metalness_0)));

#line 153
    float Fresnel_0 = saturate_0(luminance_0(evalFresnel_0(_S21, shadowedF90_0(_S21), max(0.0, dot(V_3, shadingNormal_0)))));

#line 163
    return clamp(Fresnel_0 / max(0.00009999999747379, Fresnel_0 + luminance_0(baseColorToDiffuseReflectance_0(material_2.baseColor_0, material_2.metalness_0)) * (1.0 - Fresnel_0)), 0.10000000149011612, 0.89999997615814209);
}


#line 280 3
vec4 getRotationToZAxis_0(vec3 input_0)
{

    float _S22 = input_0.z;

#line 283
    if(_S22 < -0.99998998641967773)
    {

#line 283
        return vec4(1.0, 0.0, 0.0, 0.0);
    }
    return normalize(vec4(input_0.y, - input_0.x, 0.0, 1.0 + _S22));
}


#line 306
vec3 rotatePoint_0(vec4 q_0, vec3 v_1)
{

#line 307
    vec3 qAxis_0 = vec3(q_0.x, q_0.y, q_0.z);
    float _S23 = q_0.w;

#line 308
    return 2.0 * dot(qAxis_0, v_1) * qAxis_0 + (_S23 * _S23 - dot(qAxis_0, qAxis_0)) * v_1 + 2.0 * _S23 * cross(qAxis_0, v_1);
}


#line 317
vec3 sampleHemisphere_0(vec2 u_0, out float pdf_0)
{
    float _S24 = u_0.x;

#line 319
    float a_0 = sqrt(_S24);
    float b_1 = 6.28318548202514648 * u_0.y;

#line 325
    float _S25 = sqrt(1.0 - _S24);

#line 322
    vec3 result_1 = vec3(a_0 * cos(b_1), a_0 * sin(b_1), _S25);

#line 327
    pdf_0 = _S25 * 0.31830987334251404;

    return result_1;
}

vec3 sampleHemisphere_1(vec2 u_1)
{

#line 333
    float pdf_1;
    return sampleHemisphere_0(u_1, pdf_1);
}


#line 393
float lambertian_0(BrdfData_0 data_5)
{

#line 394
    return 1.0;
}


#line 703
vec3 sampleGGXVNDF_0(vec3 Ve_0, vec2 alpha2D_0, vec2 u_2)
{

    float _S26 = alpha2D_0.x;

#line 706
    float _S27 = alpha2D_0.y;

#line 706
    vec3 Vh_0 = normalize(vec3(_S26 * Ve_0.x, _S27 * Ve_0.y, Ve_0.z));


    float _S28 = Vh_0.x;

#line 709
    float _S29 = Vh_0.y;

#line 709
    float lensq_0 = _S28 * _S28 + _S29 * _S29;

#line 709
    vec3 T1_0;
    if(lensq_0 > 0.0)
    {

#line 710
        T1_0 = vec3(- _S29, _S28, 0.0) * (inversesqrt((lensq_0)));

#line 710
    }
    else
    {

#line 710
        T1_0 = vec3(1.0, 0.0, 0.0);

#line 710
    }



    float r_0 = sqrt(u_2.x);
    float phi_0 = 6.28318548202514648 * u_2.y;
    float t1_0 = r_0 * cos(phi_0);


    float _S30 = 1.0 - t1_0 * t1_0;

#line 719
    float t2_0 = mix(sqrt(_S30), r_0 * sin(phi_0), 0.5 * (1.0 + Vh_0.z));


    vec3 Nh_0 = t1_0 * T1_0 + t2_0 * cross(Vh_0, T1_0) + sqrt(max(0.0, _S30 - t2_0 * t2_0)) * Vh_0;


    return normalize(vec3(_S26 * Nh_0.x, _S27 * Nh_0.y, max(0.0, Nh_0.z)));
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
vec3 sampleSpecularMicrofacet_0(vec3 Vlocal_0, float alpha_5, float alphaSquared_7, vec3 specularF0_1, vec2 u_3, out vec3 weight_0)
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
        Hlocal_0 = sampleGGXVNDF_0(Vlocal_0, vec2(alpha_5, alpha_5), u_3);

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
bool evalIndirectCombinedBRDF_0(vec2 u_4, vec3 shadingNormal_1, vec3 geometryNormal_0, vec3 V_4, MaterialProperties_0 material_3, int brdfType_0, out vec3 rayDirection_0, out vec3 sampleWeight_0)
{

    if((dot(shadingNormal_1, V_4)) <= 0.0)
    {

#line 901
        return false;
    }


    vec4 qRotationToZ_0 = getRotationToZAxis_0(shadingNormal_1);
    vec3 Vlocal_1 = rotatePoint_0(qRotationToZ_0, V_4);
    const vec3 Nlocal_1 = vec3(0.0, 0.0, 1.0);

    const vec3 _S31 = vec3(0.0, 0.0, 0.0);

#line 909
    vec3 rayDirectionLocal_0;

    if(brdfType_0 == 1)
    {

        vec3 rayDirectionLocal_1 = sampleHemisphere_1(u_4);
        BrdfData_0 data_6 = prepareBRDFData_0(Nlocal_1, rayDirectionLocal_1, Vlocal_1, material_3);

#line 915
        float _S32 = data_6.alpha_0;

#line 926
        sampleWeight_0 = data_6.diffuseReflectance_0 * lambertian_0(data_6) * (vec3(1.0, 1.0, 1.0) - evalFresnel_0(data_6.specularF0_0, shadowedF90_0(data_6.specularF0_0), max(0.00000999999974738, min(1.0, dot(Vlocal_1, sampleGGXVNDF_0(Vlocal_1, vec2(_S32, _S32), u_4))))));

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
            vec3 _S33 = sampleSpecularMicrofacet_0(Vlocal_1, data_7.alpha_0, data_7.alphaSquared_0, data_7.specularF0_0, u_4, sampleWeight_0);

#line 931
            rayDirectionLocal_0 = _S33;

#line 929
        }
        else
        {

#line 929
            rayDirectionLocal_0 = _S31;

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

    vec3 _S34 = normalize(rotatePoint_0(invertRotation_0(qRotationToZ_0), rayDirectionLocal_0));

#line 938
    rayDirection_0 = _S34;


    if((dot(geometryNormal_0, _S34)) <= 0.0)
    {

#line 941
        return false;
    }
    return true;
}


#line 172 1
vec3 offsetRay_0(vec3 p_2, vec3 n_0)
{

#line 178
    float _S35 = n_0.x;

#line 178
    int _S36 = int(256.0 * _S35);

#line 178
    float _S37 = n_0.y;

#line 178
    int _S38 = int(256.0 * _S37);

#line 178
    float _S39 = n_0.z;

#line 178
    int _S40 = int(256.0 * _S39);


    float _S41 = p_2.x;

#line 181
    int _S42 = floatBitsToInt(_S41);

#line 181
    int _S43;

#line 181
    if(_S41 < 0.0)
    {

#line 181
        _S43 = - _S36;

#line 181
    }
    else
    {

#line 181
        _S43 = _S36;

#line 181
    }

#line 181
    float _S44 = intBitsToFloat(_S42 + _S43);
    float _S45 = p_2.y;

#line 182
    int _S46 = floatBitsToInt(_S45);

#line 182
    if(_S45 < 0.0)
    {

#line 182
        _S43 = - _S38;

#line 182
    }
    else
    {

#line 182
        _S43 = _S38;

#line 182
    }

#line 182
    float _S47 = intBitsToFloat(_S46 + _S43);
    float _S48 = p_2.z;

#line 183
    int _S49 = floatBitsToInt(_S48);

#line 183
    if(_S48 < 0.0)
    {

#line 183
        _S43 = - _S40;

#line 183
    }
    else
    {

#line 183
        _S43 = _S40;

#line 183
    }

#line 183
    float _S50 = intBitsToFloat(_S49 + _S43);

#line 183
    float _S51;

    if((abs(_S41)) < 0.03125)
    {

#line 185
        _S51 = _S41 + 0.0000152587890625 * _S35;

#line 185
    }
    else
    {

#line 185
        _S51 = _S44;

#line 185
    }

#line 185
    float _S52;
    if((abs(_S45)) < 0.03125)
    {

#line 186
        _S52 = _S45 + 0.0000152587890625 * _S37;

#line 186
    }
    else
    {

#line 186
        _S52 = _S47;

#line 186
    }

#line 186
    float _S53;
    if((abs(_S48)) < 0.03125)
    {

#line 187
        _S53 = _S48 + 0.0000152587890625 * _S39;

#line 187
    }
    else
    {

#line 187
        _S53 = _S50;

#line 187
    }

#line 185
    return vec3(_S51, _S52, _S53);
}


#line 168 2
void main()
{

#line 168
    vec3 radiance_0;

    uvec3 _S54 = ((gl_LaunchIDEXT));

#line 170
    uvec2 LaunchIndex_0 = _S54.xy;
    uvec3 _S55 = ((gl_LaunchSizeEXT));

#line 171
    uvec2 LaunchDimensions_0 = _S55.xy;


    vec2 d_0 = (vec2(LaunchIndex_0) + vec2(0.5)) / vec2(LaunchDimensions_0) * 2.0 - 1.0;

    vec4 origin_0 = (((vec4(0.0, 0.0, 0.0, 1.0)) * (mat4x4(MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][3]))));



    vec3 rayDir_0 = (((vec4(normalize((((vec4(d_0.x, d_0.y, 1.0, 1.0)) * (mat4x4(MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][0], MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][1], MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][2], MyContantBuffer_0.giParams_0.projInverse_0.data_0[0][3], MyContantBuffer_0.giParams_0.projInverse_0.data_0[1][3], MyContantBuffer_0.giParams_0.projInverse_0.data_0[2][3], MyContantBuffer_0.giParams_0.projInverse_0.data_0[3][3])))).xyz), 0.0)) * (mat4x4(MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][0], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][1], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][2], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[0][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[1][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[2][3], MyContantBuffer_0.giParams_0.viewInverse_0.data_0[3][3])))).xyz;
    vec3 _S56 = origin_0.xyz;


    uvec4 rngState_3 = initRNG_0(LaunchIndex_0, LaunchDimensions_0, MyContantBuffer_0.giParams_0.frameCount_0);


    const vec3 _S57 = vec3(0.0, 0.0, 0.0);
    const vec3 _S58 = vec3(1.0, 1.0, 1.0);

#line 188
    vec3 rayOri_0 = _S56;

#line 188
    int bounce_0 = 0;

#line 188
    vec3 throughput_0 = _S58;

#line 188
    vec3 radiance_1 = _S57;

    for(;;)
    {

#line 190
        if(uint(bounce_0) <= (MyContantBuffer_0.giParams_0.numRays_0))
        {
        }
        else
        {

#line 190
            radiance_0 = radiance_1;

#line 190
            break;
        }
        IndirectGbufferRayPayload_0 _S59 = shootRay_0(rayOri_0, rayDir_0);

#line 192
        IndirectGbufferRayPayload_0 payload_2 = _S59;

        if(!IndirectGbufferRayPayload_hasHit_0(_S59))
        {

#line 194
            radiance_0 = radiance_1 + throughput_0 * _S57;


            break;
        }


        MaterialProperties_0 material_4 = loadMaterialProperties_0(payload_2);

        vec3 radiance_2 = radiance_1 + throughput_0 * material_4.emissive_0;

        vec3 V_5 = - rayDir_0;
        if((dot(payload_2.normal_0.xyz, V_5)) < 0.0)
        {

#line 206
            payload_2.normal_0.xyz = - payload_2.normal_0.xyz;

#line 206
        }


        SLight_0 light_3;
        float lightWeight_0;
        bool _S60 = sampleLightRIS_0(rngState_3, payload_2.position_objectID_0.xyz, payload_2.normal_0.xyz, light_3, lightWeight_0);

#line 211
        if(_S60)
        {

            vec3 lightVector_2;
            float lightDistance_2;
            getLightData_0(light_3, payload_2.position_objectID_0.xyz, lightVector_2, lightDistance_2);
            vec3 L_3 = normalize(lightVector_2);


            bool _S61 = castShadowRay_0(payload_2.position_objectID_0.xyz, payload_2.normal_0.xyz, L_3, lightDistance_2);

#line 220
            if(_S61)
            {

#line 220
                radiance_0 = radiance_2 + throughput_0 * evalCombinedBRDF_0(payload_2.normal_0.xyz, L_3, V_5, material_4) * (getLightIntensityAtPoint_0(light_3, lightDistance_2) * lightWeight_0);

#line 220
            }
            else
            {

#line 220
                radiance_0 = radiance_2;

#line 220
            }

#line 211
        }
        else
        {

#line 211
            radiance_0 = radiance_2;

#line 211
        }

#line 211
        vec3 throughput_1;

#line 228
        if(bounce_0 > 3)
        {

#line 228
            vec3 _S62;
            float rrProbability_0 = min(0.94999998807907104, luminance_0(throughput_0));
            float _S63 = rand_0(rngState_3);

#line 230
            if(rrProbability_0 < _S63)
            {

#line 230
                break;
            }
            else
            {

#line 230
                _S62 = throughput_0 / rrProbability_0;

#line 230
            }

#line 230
            throughput_1 = _S62;

#line 228
        }
        else
        {

#line 228
            throughput_1 = throughput_0;

#line 228
        }

#line 228
        bool _S64;

#line 237
        if((material_4.metalness_0) == 1.0)
        {

#line 237
            _S64 = (material_4.roughness_0) == 0.0;

#line 237
        }
        else
        {

#line 237
            _S64 = false;

#line 237
        }

#line 237
        vec3 throughput_2;

#line 237
        int brdfType_1;

#line 237
        if(_S64)
        {

#line 237
            brdfType_1 = 2;

#line 237
            throughput_2 = throughput_1;

#line 237
        }
        else
        {



            float brdfProbability_0 = getBrdfProbability_0(material_4, V_5, payload_2.normal_0.xyz);

            float _S65 = rand_0(rngState_3);

#line 245
            if(_S65 < brdfProbability_0)
            {
                vec3 throughput_3 = throughput_1 / brdfProbability_0;

#line 247
                brdfType_1 = 2;

#line 247
                throughput_2 = throughput_3;

#line 245
            }
            else
            {


                vec3 throughput_4 = throughput_1 / (1.0 - brdfProbability_0);

#line 250
                brdfType_1 = 1;

#line 250
                throughput_2 = throughput_4;

#line 245
            }

#line 237
        }

#line 256
        float _S66 = rand_0(rngState_3);

#line 256
        float _S67 = rand_0(rngState_3);

#line 255
        vec3 brdfWeight_0;

        bool _S68 = evalIndirectCombinedBRDF_0(vec2(_S66, _S67), payload_2.normal_0.xyz, payload_2.normal_0.xyz, V_5, material_4, brdfType_1, rayDir_0, brdfWeight_0);

#line 257
        if(!_S68)
        {

#line 258
            break;
        }


        vec3 throughput_5 = throughput_2 * brdfWeight_0;


        vec3 _S69 = offsetRay_0(payload_2.position_objectID_0.xyz, payload_2.normal_0.xyz);

#line 190
        int _S70 = bounce_0 + 1;

#line 190
        rayOri_0 = _S69;

#line 190
        bounce_0 = _S70;

#line 190
        throughput_0 = throughput_5;

#line 190
        radiance_1 = radiance_0;

#line 190
    }

#line 269
    ivec2 _S71 = ivec2(LaunchIndex_0);

#line 269
    vec4 _S72 = (imageLoad((accumulationBuffer_0), (_S71)));


    imageStore((accumulationBuffer_0), (_S71), vec4(_S72.xyz + radiance_0, 1.0));

    imageStore((ptOutput_0), (_S71), vec4(radiance_0, 1.0));
    return;
}

