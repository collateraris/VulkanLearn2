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

vec3 ggxDirect(uint randSeed, SObjectData shadeData, vec2 uv, vec3 worldPos, vec3 worldNorm, vec3 camPos, vec3 albedo, float roughness, vec3 lightDir, vec3 viewDir, vec3 sunColor, inout vec3 F0)
{
	float metalness = 0.;
	if (shadeData.metalnessTexIndex > 0)
		metalness = 1. - texture(texSet[shadeData.metalnessTexIndex], uv).r;

	float metallic = metalness;        

	vec3 H = normalize(viewDir + lightDir);

	float HdotV = clamp(dot(H, viewDir), 0.f, 1.f);
	float NdotV = clamp(dot(worldNorm.xyz, viewDir), 0.f, 1.f);
	float NdotL = clamp(dot(worldNorm.xyz, lightDir), 0.f, 1.f);
    
	F0      = mix(F0, albedo, metallic);
	vec3 F  = fresnelSchlick(HdotV, F0);

	float NDF = DistributionGGX(worldNorm, H, roughness);
	float G = GeometrySmith(worldNorm, viewDir, lightDir, roughness);

    // Evaluate the Cook-Torrance Microfacet BRDF model
	//     Cancel out NdotL here & the next eq. to avoid catastrophic numerical precision issues.
	vec3 specular = (NDF * G) * F / (4.0f * NdotV /* * NdotL */ + 0.001f);

	vec3 kS = F;
	vec3 kD = vec3(1.0f) - kS;
	
	kD *= 1.0f - metallic;

	float shadowMult = shadowRayVisibility(worldPos.xyz, lightDir, giParams.shadowMult);

    //AO
    // Start accumulating from zero if we don't hit the background
    float ambientOcclusion = 0.0f;

    for (int i = 0; i < giParams.numRays; i++)
    {
        // Sample cosine-weighted hemisphere around surface normal to pick a random ray direction
        vec3 worldDir = getCosHemisphereSample(randSeed, worldNorm.xyz);

        // Shoot our ambient occlusion ray and update the value we'll output with the result
        ambientOcclusion += shootAmbientOcclusionRay(worldPos.xyz, worldDir, giParams.aoRadius, 0.f);
    }

    float aoColor = ambientOcclusion / float(giParams.numRays); 

    vec3 Lo = (kD * albedo * aoColor * M_INV_PI + specular) * sunColor;

	return Lo;
};

vec3 ggxIndirect(uint randSeed, vec3 worldPos, vec3 worldNorm, vec3 camPos, vec3 albedo, float roughness, vec3 lightDir, vec3 viewDir, in vec3 F0)
{
	// We have to decide whether we sample our diffuse or specular/ggx lobe.
	float probDiffuse = probabilityToSampleDiffuse(albedo, F0);
	bool chooseDiffuse = (nextRand(randSeed) < probDiffuse);

	// We'll need NdotV for both diffuse and specular...
	float NdotV = clamp(dot(worldNorm, viewDir), 0.f, 1.f);

    if (chooseDiffuse)
    {
        // Shoot a randomly selected cosine-sampled diffuse ray.
		vec3 L = getCosHemisphereSample(randSeed, worldNorm);
		vec3 bounceColor = shootIndirectRay(worldPos.xyz, L, randSeed);

		// Accumulate the color: (NdotL * incomingLight * dif / pi) 
		// Probability of sampling:  (NdotL / pi) * probDiffuse
		return bounceColor * albedo / max(1e-3, probDiffuse);
    }
    else
    {
        // Randomly sample the NDF to get a microfacet in our BRDF to reflect off of
        vec3 H = getGGXMicrofacet(randSeed, roughness, worldNorm);

        vec3 L = normalize(2.f * dot(viewDir, H) * H - viewDir);

        //   Shoot our indirect global illumination ray
        vec3 bounceColor = shootIndirectRay(worldPos.xyz, L, randSeed);  

        // // Compute some dot products needed for shading
        float  NdotL = clamp(dot(worldNorm, L), 0.f, 1.f);
        float  NdotH = clamp(dot(worldNorm, H), 0.f, 1.f);
        float  LdotH = clamp(dot(L, H), 0.f, 1.f);

        // Evaluate our BRDF using a microfacet BRDF model
        float  D = ggxNormalDistribution(NdotH, roughness);          // The GGX normal distribution
        float  G = IndirectGeometrySmith(worldNorm, viewDir, L, roughness);   // Use Schlick's masking term approx
        vec3 F = fresnelSchlickRoughness(LdotH, F0, roughness);  // Use Schlick's approx to Fresnel
        vec3 ggxTerm = D * G * F / (4.f * NdotL * NdotV + 0.001);        // The Cook-Torrance microfacet BRDF

        // What's the probability of sampling vector H from getGGXMicrofacet()?
        float  ggxProb = D * NdotH / (4.f * LdotH);

        // Accumulate the color:  ggx-BRDF * incomingLight * NdotL / probability-of-sampling
        //    -> Should really simplify the math above.
        return NdotL * bounceColor * ggxTerm / max(1e-3, (ggxProb * (1.0f - probDiffuse)));
    }
};
