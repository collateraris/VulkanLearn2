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
    return shootAmbientOcclusionRay(orig, dir, 10000000, defaultVal);
};

vec3 shootIndirectRay(vec3 orig, vec3 dir, uint randSeed)
{
    indirectRpl.color = vec3(0., 0., 0.);
    indirectRpl.randSeed = randSeed;

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
            1,//.0e38f,           // ray max range
            0      // payload (location = 0)
    );   

	// Return the color we got from our ray
	return indirectRpl.color;
};

vec3 ggxDirect(SObjectData shadeData, vec2 uv, vec3 worldPos, vec3 worldNorm, vec3 camPos, vec3 albedo, float roughness, vec3 lightDir, vec3 viewDir, vec3 sunColor, inout vec3 F0)
{
	float metalness = 0.;
	if (shadeData.metalnessTexIndex > 0)
		metalness = 1. - texture(texSet[shadeData.metalnessTexIndex], uv).r;

	float metallic = metalness;        

	vec3 H = normalize(viewDir + lightDir);

	float HdotV = max(dot(H, viewDir), 0.0);
	float NdotV = max(dot(worldNorm.xyz, viewDir), 0.0);
	float NdotL = max(dot(worldNorm.xyz, lightDir), 0.0);
    
	F0      = mix(F0, albedo, metallic);
	vec3 F  = fresnelSchlick(HdotV, F0);

	float NDF = DistributionGGX(worldNorm, H, roughness);
	float G = GeometrySmith(worldNorm, viewDir, lightDir, roughness);

	vec3 specular = (NDF * G) * F / (4.0 * NdotV * NdotL + 0.001);

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	
	kD *= 1.0 - metallic;

	float shadowMult = shadowRayVisibility(worldPos.xyz, lightDir, giParams.shadowMult);

    vec3 Lo = shadowMult * (kD * albedo * M_INV_PI + specular) * sunColor * NdotL;

	return Lo;
};

vec3 ggxIndirect(uint randSeed, vec3 worldPos, vec3 worldNorm, vec3 camPos, float roughness, vec3 lightDir, vec3 viewDir, in vec3 F0)
{
    // Randomly sample the NDF to get a microfacet in our BRDF to reflect off of
    vec3 H = getGGXMicrofacet(randSeed, roughness, worldNorm);

    vec3 L = normalize(2.f * dot(viewDir, H) * H - viewDir);

    //   Shoot our indirect global illumination ray
    vec3 bounceColor = shootIndirectRay(worldPos.xyz, L, randSeed);  

    // // Compute some dot products needed for shading
    float  NdotL = clamp(dot(worldNorm, L), 0.0, 1.);
    float  NdotH = clamp(dot(worldNorm, H), 0.0, 1.);
    float  LdotH = clamp(dot(L, H), 0.0, 1.);
    float NdotV = clamp(dot(worldNorm, viewDir), 0.0, 1.);

    // Evaluate our BRDF using a microfacet BRDF model
    float  D = ggxNormalDistribution(NdotH, roughness);          // The GGX normal distribution
    float  G = IndirectGeometrySmith(worldNorm, viewDir, L, roughness);   // Use Schlick's masking term approx
    vec3 F = fresnelSchlickRoughness(LdotH, F0, roughness);  // Use Schlick's approx to Fresnel
    vec3 ggxTerm = D * G * F / (4 * NdotL * NdotV);        // The Cook-Torrance microfacet BRDF

    // What's the probability of sampling vector H from getGGXMicrofacet()?
    float  ggxProb = D * NdotH / (4 * LdotH);

    // Accumulate the color:  ggx-BRDF * incomingLight * NdotL / probability-of-sampling
    //    -> Should really simplify the math above.
    return NdotL * bounceColor * ggxTerm;
};
