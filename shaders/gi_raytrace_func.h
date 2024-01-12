// A wrapper function that encapsulates shooting an ambient occlusion ray query
float shootAmbientOcclusionRay( vec3 orig, vec3 dir, float maxT, float defaultVal)
{
	aoRpl.aoValue = defaultVal;

    uint  rayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;

    traceRayEXT(topLevelAS, // acceleration structure
            rayFlags,       // rayFlags
            0xFF,           // cullMask
            0,              // sbtRecordOffset
            0,              // sbtRecordStride
            1,              // missIndex
            orig.xyz,       // ray origin
            1e-3,           // ray min range
            dir.xyz,         // ray direction
            maxT,           // ray max range
            1      // payload (location = 1)
    );        

	return aoRpl.aoValue;
};

float shadowRayVisibility( vec3 orig, vec3 dir, float defaultVal)
{
    return shootAmbientOcclusionRay(orig, dir, 1.0e38f, defaultVal);
};

IndirectRayPayload shootIndirectRay(vec3 orig, vec3 dir)
{
    uint  rayFlags = gl_RayFlagsOpaqueEXT;

    traceRayEXT(topLevelAS, // acceleration structure
            rayFlags,       // rayFlags
            0xFF,           // cullMask
            0,              // sbtRecordOffset
            0,              // sbtRecordStride
            0,              // missIndex
            orig.xyz,       // ray origin
            1e-3,           // ray min range
            dir.xyz,         // ray direction
            1.0e38f,           // ray max range
            0      // payload (location = 0)
    );   

	// Return the color we got from our ray
	return indirectRpl;
};

DirectOutputData ggxDirect(DirectInputData inputData, vec3 camPos, vec3 lightDir, vec3 sunColor)
{
    int objectId = unpackObjID_DirectInputData(inputData);
    vec2 texCoord = unpackUV_DirectInputData(inputData);
    vec3 worldPos = unpackWorldPos_DirectInputData(inputData);
    vec3 worldNorm = unpackWorldNorm_DirectInputData(inputData);

    SObjectData shadeData = objectBuffer.objects[objectId];

    vec3 albedo = texture(texSet[shadeData.diffuseTexIndex], texCoord).rgb;

    vec3 emission = vec3(0., 0., 0);
    if (shadeData.emissionTexIndex > 0)
        emission = texture(texSet[shadeData.emissionTexIndex], texCoord).rgb;    

    float metalness = 0.;
    if (shadeData.metalnessTexIndex > 0)
        metalness = 1. - texture(texSet[shadeData.metalnessTexIndex], texCoord).r;  

    float roughness = 1.;
    if (shadeData.roughnessTexIndex > 0)
        roughness = texture(texSet[shadeData.roughnessTexIndex], texCoord).g;    

    if (shadeData.normalTexIndex > 0)
    {
        mat3 TBN = getTBN(worldNorm);
        vec3 normal = texture(texSet[shadeData.normalTexIndex], texCoord).rgb;
        normal = normalize(normal * 2.0 - 1.0);   
        worldNorm = normalize(TBN * normal);
    }  

    vec3 viewDir = normalize(camPos.xyz - worldPos.xyz);

	vec3 H = normalize(viewDir + lightDir);

	float HdotV = clamp(dot(H, viewDir), 0.f, 1.f);
	float NdotV = clamp(dot(worldNorm.xyz, viewDir), 0.f, 1.f);
	float NdotL = clamp(dot(worldNorm.xyz, lightDir), 0.f, 1.f);
    
    vec3 F0 = vec3(0.04); 
	F0      = mix(F0, albedo, metalness);

	vec3 F  = fresnelSchlick(HdotV, F0);

	float NDF = DistributionGGX(worldNorm, H, roughness);
	float G = GeometrySmith(worldNorm, viewDir, lightDir, roughness);

	vec3 specular = (NDF * G) * F / (4.0f * NdotV  * NdotL  + 0.001f);

	vec3 kS = F;
	vec3 kD = vec3(1.0f) - kS;
	
	kD *= 1.0f - metalness;

	float shadowMult = shadowRayVisibility(worldPos.xyz, lightDir, giParams.shadowMult);
    vec3 Lo =  emission + shadowMult * (kD * albedo * M_INV_PI + specular) * sunColor * NdotL;

	return packDirectOutputData(worldNorm, albedo, F0, Lo, metalness, roughness);
};
