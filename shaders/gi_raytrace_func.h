// A wrapper function that encapsulates shooting an ambient occlusion ray query
float shadowRayVisibility( vec3 orig, vec3 dir, float maxT, float defaultVal)
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

IndirectRayPayload shootDirectRay(vec3 orig, vec3 dir)
{
    return shootIndirectRay(orig, dir);
};

float getDistanceFalloff(float distSquared)
{
    float falloff = 1 / ((0.01 * 0.01) + distSquared); // The 0.01 is to avoid infs when the light source is close to the shading point
    return falloff;
};


DirectOutputData ggxDirect(uint lightToSample, DirectInputData inputData, vec3 camPos, bool withShadow)
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

    SLight lightInfo = lightsBuffer.lights[lightToSample];

    vec3 lightDir = lightInfo.position.xyz - worldPos.xyz;
     // Avoid NaN
    float distSquared = dot(lightDir, lightDir);
    float lightDistance = (distSquared > 1e-5f) ? length(lightDir) : 0.f;
    lightDir = (distSquared > 1e-5f) ? normalize(lightDir) : vec3(0.f, 0.f, 0.f);
    
    // Calculate the falloff
    float falloff = getDistanceFalloff(distSquared);
    vec3 lightColor = lightInfo.color.xyz * falloff;

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

	float shadowColor = 1.f;
    if (withShadow)
        shadowColor = shadowRayVisibility(worldPos.xyz, lightDir, lightDistance, giParams.shadowMult);
    vec3 Lo =  emission + shadowColor * (kD * albedo * M_INV_PI + specular) * lightColor * NdotL;

	return packDirectOutputData(worldNorm, albedo, F0, Lo, metalness, roughness);
};

struct VbufferExtraCommonData
{
    vec3 worldPos;
    vec3 worldNorm;
    vec2 uvCoord;
    float pad;
};


VbufferExtraCommonData proccessVbufferData(uvec2 objectID_vertexID, mat4 projView, vec2 ndc, vec2 screenSize)
{
    VbufferExtraCommonData data;
    int objID = int(objectID_vertexID.x) - 1;
    int vertexID = int(objectID_vertexID.y);

    SObjectData shadeData = objectBuffer.objects[objID];

    SVertex v0 = Vertices[shadeData.meshIndex].vertices[vertexID + 0];
    SVertex v1 = Vertices[shadeData.meshIndex].vertices[vertexID + 1];
    SVertex v2 = Vertices[shadeData.meshIndex].vertices[vertexID + 2];

    vec3 v0_pos = vec3(v0.positionXYZ_normalX.x, v0.positionXYZ_normalX.y, v0.positionXYZ_normalX.z);
    vec3 v1_pos = vec3(v1.positionXYZ_normalX.x, v1.positionXYZ_normalX.y, v1.positionXYZ_normalX.z);
    vec3 v2_pos = vec3(v2.positionXYZ_normalX.x, v2.positionXYZ_normalX.y, v2.positionXYZ_normalX.z);

    mat4 PVM = projView * shadeData.model;

    BarycentricDeriv bary = CalcFullBary(
        PVM * vec4(v0_pos, 1.0),
        PVM * vec4(v1_pos, 1.0),
        PVM * vec4(v2_pos, 1.0),
        ndc,
        screenSize
    );

    data.worldPos = interpolate(bary, v0_pos, v1_pos, v2_pos).value;

    vec3 v0_nrm = vec3(v0.positionXYZ_normalX.w, v0.normalYZ_texCoordUV.x, v0.normalYZ_texCoordUV.y);
    vec3 v1_nrm = vec3(v1.positionXYZ_normalX.w, v1.normalYZ_texCoordUV.x, v1.normalYZ_texCoordUV.y);
    vec3 v2_nrm = vec3(v2.positionXYZ_normalX.w, v2.normalYZ_texCoordUV.x, v2.normalYZ_texCoordUV.y); 

    data.worldNorm = interpolate(bary, v0_nrm, v1_nrm, v2_nrm).value;

    vec2 v0_uv= vec2(v0.normalYZ_texCoordUV.z, v0.normalYZ_texCoordUV.w);
    vec2 v1_uv = vec2(v1.normalYZ_texCoordUV.z, v1.normalYZ_texCoordUV.w);
    vec2 v2_uv = vec2(v2.normalYZ_texCoordUV.z, v2.normalYZ_texCoordUV.w);

    data.uvCoord = interpolate(bary, v0_uv, v1_uv, v2_uv).value;
    return data;
}