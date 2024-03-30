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
    float falloff = 1000.f / ((0.01 * 0.01) + distSquared); // The 0.01 is to avoid infs when the light source is close to the shading point
    return falloff;
};

PBRShadeData getShadeData(DirectInputData inputData)
{
    PBRShadeData data;

    int objectId = unpackObjID_DirectInputData(inputData);
    vec2 texCoord = unpackUV_DirectInputData(inputData);
    data.worldPos.xyz = unpackWorldPos_DirectInputData(inputData);
    data.worldNorm.xyz = unpackWorldNorm_DirectInputData(inputData);

    SObjectData shadeData = objectBuffer.objects[objectId];

    data.albedo_metalness.xyz = texture(texSet[shadeData.diffuseTexIndex], texCoord).rgb;

    data.emission_roughness.xyz = vec3(0., 0., 0);
    if (shadeData.emissionTexIndex > 0)
        data.emission_roughness.xyz = texture(texSet[shadeData.emissionTexIndex], texCoord).rgb;    

    data.albedo_metalness.w = 0.;
    if (shadeData.metalnessTexIndex > 0)
        data.albedo_metalness.w = 1. - texture(texSet[shadeData.metalnessTexIndex], texCoord).r;  

    data.emission_roughness.w = 1.;
    if (shadeData.roughnessTexIndex > 0)
        data.emission_roughness.w = texture(texSet[shadeData.roughnessTexIndex], texCoord).g;    

    if (shadeData.normalTexIndex > 0)
    {
        mat3 TBN = getTBN(data.worldNorm.xyz);
        vec3 normal = texture(texSet[shadeData.normalTexIndex], texCoord).rgb;
        normal = normalize(normal * 2.0 - 1.0);   
        data.worldNorm.xyz = normalize(TBN * normal);
    }  

    return data;
};

#define LightSun            1
#define LightPoint          2

DirectOutputData ggxDirect(uint lightToSample, PBRShadeData prbSD, vec3 camPos, bool withShadow)
{
    vec3 viewDir = normalize(prbSD.worldPos.xyz - camPos.xyz);

    SLight lightInfo = lightsBuffer.lights[lightToSample];

    vec3 lightDir = vec3(0.f, 0.f, 0.f);
    vec3 lightColor = lightInfo.color_type.xyz;
    float lightDistance = 1.f;

    if (int(lightInfo.color_type.w) == LightPoint)
    {
        lightDir = lightInfo.position.xyz - prbSD.worldPos.xyz;
        // Avoid NaN
        float distSquared = dot(lightDir, lightDir);
        lightDistance = (distSquared > 1e-5f) ? length(lightDir) : 0.f;
        lightDir = (distSquared > 1e-5f) ? normalize(lightDir) : vec3(0.f, 0.f, 0.f);
        // Calculate the falloff
        float falloff = getDistanceFalloff(distSquared);
        lightColor = lightColor * falloff;
    }
    else if (int(lightInfo.color_type.w) == LightSun)
    {
        lightDir = normalize(-lightInfo.direction.xyz);
        lightDistance = 1.0e38f;
    };
    

	vec3 H = normalize(viewDir + lightDir);

	float HdotV = clamp(dot(H, viewDir), 0.f, 1.f);
	float NdotV = clamp(dot(prbSD.worldNorm.xyz, viewDir), 0.f, 1.f);
	float NdotL = clamp(dot(prbSD.worldNorm.xyz, lightDir), 0.f, 1.f);
    
    vec3 F0 = vec3(0.04); 
	F0      = mix(F0, prbSD.albedo_metalness.xyz, prbSD.albedo_metalness.w);

	vec3 F  = fresnelSchlick(HdotV, F0);

	float NDF = DistributionGGX(prbSD.worldNorm.xyz, H, prbSD.emission_roughness.w);
	float G = GeometrySmith(prbSD.worldNorm.xyz, viewDir, lightDir, prbSD.emission_roughness.w);

	vec3 specular = (NDF * G) * F / (4.0f * NdotV  * NdotL  + 0.001f);

	vec3 kS = F;
	vec3 kD = vec3(1.0f) - kS;
	
	kD *= 1.0f - prbSD.albedo_metalness.w;

	float shadowColor = 1.f;
    if (withShadow)
        shadowColor = shadowRayVisibility(prbSD.worldPos.xyz, lightDir, lightDistance, giParams.shadowMult);
    vec3 Lo = prbSD.emission_roughness.xyz + float(giParams.lightsCount) * shadowColor * (kD * prbSD.albedo_metalness.xyz * M_INV_PI + specular) * lightColor * NdotL;

	return packDirectOutputData(prbSD.worldNorm.xyz, prbSD.albedo_metalness.xyz, F0, Lo, prbSD.albedo_metalness.w, prbSD.emission_roughness.w, kD);
};


VbufferExtraCommonData proccessVbufferData(uvec3 objectID_meshletsID_primitiveID, mat4 projView, vec2 ndc, vec2 screenSize, vec3 camPos)
{
    VbufferExtraCommonData data;
    int objID = int(objectID_meshletsID_primitiveID.x) - 1;
    int meshletIndex = int(objectID_meshletsID_primitiveID.y);
    int indexId = int(objectID_meshletsID_primitiveID.z);
    
    SObjectData shadeData = objectBuffer.objects[objID];

    SMeshlet currMeshlet = Meshlet[shadeData.meshIndex].meshlets[meshletIndex];

    int vertexCount = int(currMeshlet.vertexCount);
	int dataOffset = int(currMeshlet.dataOffset);
	int vertexOffset = int(dataOffset);
	int indexOffset = int(dataOffset + vertexCount);

    int index0 = int(MeshletData[shadeData.meshIndex].meshletData[indexOffset + indexId]);
    int index1 = int(MeshletData[shadeData.meshIndex].meshletData[indexOffset + indexId + 1]);
    int index2 = int(MeshletData[shadeData.meshIndex].meshletData[indexOffset + indexId + 2]);
    int vertexId0 = int(MeshletData[shadeData.meshIndex].meshletData[vertexOffset + index0]);
    int vertexId1 = int(MeshletData[shadeData.meshIndex].meshletData[vertexOffset + index1]);
    int vertexId2 = int(MeshletData[shadeData.meshIndex].meshletData[vertexOffset + index2]);

    SVertex v0 = Vertices[shadeData.meshIndex].vertices[vertexId0];
    SVertex v1 = Vertices[shadeData.meshIndex].vertices[vertexId1];
    SVertex v2 = Vertices[shadeData.meshIndex].vertices[vertexId2];

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

    const vec3 e0 = v2_pos - v0_pos;
    const vec3 e1 = v1_pos - v0_pos;
    const vec3 e0t = vec3(shadeData.model * vec4(e0,0));
    const vec3 e1t = vec3(shadeData.model * vec4(e1,0));
    vec3 geometry_nrm = normalize(vec3(vec4(cross(e0, e1), 0) * inverse(shadeData.model)));
    vec3 wo = normalize(camPos - data.worldPos.xyz);
    if (dot(geometry_nrm, wo) <= 0.)
        geometry_nrm = -geometry_nrm;
    if (dot(geometry_nrm, data.worldNorm) <= 0)
        data.worldNorm = -data.worldNorm;    

    vec2 v0_uv= vec2(v0.normalYZ_texCoordUV.z, v0.normalYZ_texCoordUV.w);
    vec2 v1_uv = vec2(v1.normalYZ_texCoordUV.z, v1.normalYZ_texCoordUV.w);
    vec2 v2_uv = vec2(v2.normalYZ_texCoordUV.z, v2.normalYZ_texCoordUV.w);

    data.uvCoord = interpolate(bary, v0_uv, v1_uv, v2_uv).value;
    return data;
};