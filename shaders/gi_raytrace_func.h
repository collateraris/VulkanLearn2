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

vec3 shootIndirectRay(vec3 orig, vec3 dir, uint randSeed, out uint hit)
{
    indirectRpl.color = vec3(0., 0., 0.);
    indirectRpl.randSeed = randSeed;
    indirectRpl.hit = 0;

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
    hit = indirectRpl.hit;   

	// Return the color we got from our ray
	return indirectRpl.color;
};

vec3 ggxDirect(uint randSeed, SObjectData shadeData, vec2 uv, vec3 worldPos, vec3 worldNorm, vec3 camPos, vec3 albedo, float roughness, float metallic, vec3 lightDir, vec3 viewDir, vec3 sunColor, inout vec3 F0)
{
	vec3 H = normalize(viewDir + lightDir);

	float HdotV = clamp(dot(H, viewDir), 0.f, 1.f);
	float NdotV = clamp(dot(worldNorm.xyz, viewDir), 0.f, 1.f);
	float NdotL = clamp(dot(worldNorm.xyz, lightDir), 0.f, 1.f);
    
	F0      = mix(F0, albedo, metallic);
	vec3 F  = fresnelSchlick(HdotV, F0);

	float NDF = DistributionGGX(worldNorm, H, roughness);
	float G = GeometrySmith(worldNorm, viewDir, lightDir, roughness);

	vec3 specular = (NDF * G) * F / (4.0f * NdotV  * NdotL  + 0.001f);

	vec3 kS = F;
	vec3 kD = vec3(1.0f) - kS;
	
	kD *= 1.0f - metallic;

	float shadowMult = shadowRayVisibility(worldPos.xyz, lightDir, giParams.shadowMult);

    vec3 Lo = (kD * albedo * M_INV_PI + specular) * sunColor * NdotL;

	return Lo;
};

vec3 ggxIndirect(uint randSeed, vec3 worldPos, vec3 worldNorm, vec3 camPos, vec3 albedo, vec3 ambient, float roughness, float metallic, vec3 lightDir, vec3 viewDir, in vec3 F0)
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
        uint hit = 0;
		vec3 bounceColor = shootIndirectRay(worldPos.xyz, L, randSeed, hit);

		// Accumulate the color: (NdotL * incomingLight * dif / pi) 
		// Probability of sampling:  (NdotL / pi) * probDiffuse
		return (hit * bounceColor * albedo / max(1e-3, probDiffuse)) + (1 - hit) * ambient;
    }
    else
    {
        // Randomly sample the NDF to get a microfacet in our BRDF to reflect off of
        vec3 H = getGGXMicrofacet(randSeed, roughness, worldNorm);

        vec3 L = normalize(2.f * dot(viewDir, H) * H - viewDir);

        //   Shoot our indirect global illumination ray
        uint hit = 0;
        vec3 bounceColor = shootIndirectRay(worldPos.xyz, L, randSeed, hit);  

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

        vec3 kS = F;
        vec3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;

        // Accumulate the color:  ggx-BRDF * incomingLight * NdotL / probability-of-sampling
        //    -> Should really simplify the math above.
        return (hit * NdotL * (kD * bounceColor + ggxTerm) / max(1e-3, (ggxProb * (1.0f - probDiffuse)))) + (1 - hit) * ambient;
    }
};