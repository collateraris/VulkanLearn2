#include "gi_pathtrace.h"
#include "brdf.h"

#define STANDARD_RAY_INDEX 0
#define SHADOW_RAY_INDEX 1

// Defines after how many bounces will be the Russian Roulette applied
#define MIN_BOUNCES 3

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t1);

[[vk::binding(6, 0)]]
SamplerState      linearSampler : register(s6);

[[vk::binding(2, 0)]]
RaytracingAccelerationStructure topLevelAS	: register(t2);

[[vk::binding(4, 0)]]
StructuredBuffer<SLight> lightsBuffer  : register(t4);

[[vk::binding(7, 0)]]
StructuredBuffer<SMaterialData> matBuffer  : register(t7);

[[vk::binding(12, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTTemporalBuffer[]  : register(t12);

[[vk::binding(13, 0)]]
RWStructuredBuffer<SReservoirPT> reservoirPTSpacialBuffer  : register(t13);

[[vk::binding(14, 0)]]
StructuredBuffer<SAliasTable> aliasTable  : register(t14);

[[vk::binding(15, 0)]]
StructuredBuffer<SCell> cellGridData  : register(t15);

[[vk::binding(0, 1)]]
cbuffer MyContantBuffer : register(b0)
{
	SGlobalGIParams giParams;
};

[[vk::binding(0, 2)]]
RWTexture2D<float4> ptAlbedoMetalnessOutput : register(u1);

[[vk::binding(1, 2)]]
RWTexture2D<float4> ptEmissionRoughnessOutput : register(u2);

[[vk::binding(2, 2)]]
RWTexture2D<float4> ptNormalOutput : register(u3);

[[vk::binding(3, 2)]]
RWTexture2D<float4> ptWposObjectIdOutput : register(u4);

IndirectGbufferRayPayload shootRay(float3 orig, float3 dir, float max)
{
	IndirectGbufferRayPayload payload;
	RayDesc ray;
	ray.Origin = orig;
	ray.Direction = dir;
	ray.TMin = 0.f;
	ray.TMax = max;

	TraceRay(
		topLevelAS,
		RAY_FLAG_NONE,
		0xFF,
		STANDARD_RAY_INDEX,
		0,
		STANDARD_RAY_INDEX,
		ray,
		payload);  

	// Return the color we got from our ray
	return payload;
};

// Casts a shadow ray and returns true if light is unoccluded
// Note that we use dedicated hit group with simpler shaders for shadow rays
bool castShadowRay(float3 hitPosition, float3 surfaceNormal, float3 directionToLight, float TMax)
{
	RayDesc ray;
	ray.Origin = offsetRay(hitPosition, surfaceNormal);
	ray.Direction = directionToLight;
	ray.TMin = 0.0f;
	ray.TMax = TMax;

	ShadowHitInfo payload;
	payload.hasHit = true; //< Initialize hit flag to true, it will be set to false on a miss

	// Trace the ray
	TraceRay(
		topLevelAS,
		RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
		0xFF,
		SHADOW_RAY_INDEX,
		0,
		SHADOW_RAY_INDEX,
		ray,
		payload);

	return !payload.hasHit;
}

MaterialProperties loadMaterialProperties(IndirectGbufferRayPayload payload)
{
    MaterialProperties result = (MaterialProperties) 0;
    result.baseColor = payload.albedo_metalness.xyz;
    result.metalness = payload.albedo_metalness.w;
    result.emissive = payload.emission_roughness.xyz;
    result.roughness = payload.emission_roughness.w;

    return result;
};

MaterialProperties loadMaterialProperties2(uint2 LaunchIndex)
{
    MaterialProperties result = (MaterialProperties) 0;
    float4 albedo_metalness = ptAlbedoMetalnessOutput[LaunchIndex];
    result.baseColor = albedo_metalness.xyz;
    result.metalness = albedo_metalness.w;
    float4  emission_roughness = ptEmissionRoughnessOutput[LaunchIndex];
    result.emissive = emission_roughness.xyz;
    result.roughness = emission_roughness.w;

    return result;
};

// Samples a random light from the pool of all lights using simplest uniform distirbution
bool sampleLightUniform(inout RngStateType rngState, float3 hitPosition, float3 surfaceNormal, out SLight light, out float lightSampleWeight, out uint selectLightIndex) {

	if (giParams.lightsCount == 0) return false;

	float3 gridCoord = (hitPosition - giParams.gridMin.xyz) / (giParams.gridMax.xyz - giParams.gridMin.xyz);
    uint3 cellIdx = clamp(uint3(gridCoord * GRID_SIZE), uint3(0), GRID_SIZE - uint3(1));

	uint linearIdx = cellIdx.x * GRID_SIZE*GRID_SIZE + cellIdx.y * GRID_SIZE + cellIdx.z;
	SCell cell = cellGridData[linearIdx];

	if (cell.startIndex == -1) return false;
	
	uint randomLightIndex = min(cell.numLights - 1, uint(rand(rngState) * cell.numLights));

	SAliasTable alias = aliasTable[cell.startIndex + randomLightIndex];
	uint lightIndex = alias.sample(rngState);
	light = lightsBuffer[lightIndex];

	float weight = light.direction_flux.w;
	lightSampleWeight = cell.weightsSum / weight;

	// PDF of uniform distribution is (1/light count). Reciprocal of that PDF (simply a light count) is a weight of this sample
	//lightSampleWeight = float(giParams.lightsCount);

	selectLightIndex = lightIndex;

	return true;
};

// Samples a random light from the pool of all lights using RIS (Resampled Importance Sampling)
bool sampleLightRIS(inout RngStateType rngState, float3 hitPosition, float3 surfaceNormal, out SLight selectedSample, out float lightSampleWeight, MaterialProperties material, float3 shadingNormal, float3 V) {
	
	if (giParams.lightsCount == 0) return false;

	selectedSample = (SLight) 0;
	float totalWeights = 0.0f;
	float samplePdfG = 0.0f;
	uint candidateLightIndex = -1;
	[unroll]
	for (int i = 0; i < RIS_CANDIDATES_LIGHTS2; i++) {

		float candidateWeight;
		SLight candidate;
		if (sampleLightUniform(rngState, hitPosition, surfaceNormal, candidate, candidateWeight, candidateLightIndex)) {

			uint type = uint(candidate.color_type.w);
			if (type == EMISSION_LIGHT)
			{
				float2 uv = float2(rand(rngState), rand(rngState));
				float2 bary = UniformSampleTriangle(uv);
				candidate.position_radius.xyz = (1.0f - bary.x - bary.y) * candidate.position_radius.xyz + bary.x * candidate.position1.xyz + bary.y * candidate.position2.xyz;
				float2 texCoord = (1.0f - bary.x - bary.y) * candidate.uv0_uv1.xy + bary.x * candidate.uv0_uv1.zw + bary.y * candidate.uv2_objectId_.xy;
				SMaterialData mat = matBuffer[uint(candidate.uv2_objectId_.z)];
				candidate.color_type.xyz = texSet[mat.emissionTexIndex].SampleLevel(linearSampler, texCoord, 0).rgb;
				candidate.color_type.xyz = float3(candidate.color_type.x * mat.emissiveFactorMult_emissiveStrength.x, candidate.color_type.y * mat.emissiveFactorMult_emissiveStrength.y, candidate.color_type.z * mat.emissiveFactorMult_emissiveStrength.z);
				candidate.color_type.xyz *= mat.emissiveFactorMult_emissiveStrength.w;
			}
			float3	lightVector;
			float lightDistance;
			getLightData(candidate, hitPosition, lightVector, lightDistance);

			float3 L = normalize(lightVector);
			float candidatePdfG = luminance(evalCombinedBRDF(shadingNormal, L, V, material) * getLightIntensityAtPoint(candidate, length(lightVector)));
			const float candidateRISWeight = candidatePdfG * candidateWeight; 

			totalWeights += candidateRISWeight;
			if (rand(rngState) < (candidateRISWeight / totalWeights)) 
			{
				selectedSample = candidate;
				samplePdfG = candidatePdfG;
			}
		}
	}

	if (totalWeights == 0.0f) {
		return false;
	} else {
		lightSampleWeight = (totalWeights / float(RIS_CANDIDATES_LIGHTS)) / samplePdfG;
		return true;
	}
};

// Calculates probability of selecting BRDF (specular or diffuse) using the approximate Fresnel term
float getBrdfProbability(MaterialProperties material, float3 V, float3 shadingNormal) {
	
	// Evaluate Fresnel term using the shading normal
	// Note: we use the shading normal instead of the microfacet normal (half-vector) for Fresnel term here. That's suboptimal for rough surfaces at grazing angles, but half-vector is yet unknown at this point
	float specularF0 = luminance(baseColorToSpecularF0(material.baseColor, material.metalness));
	float diffuseReflectance = luminance(baseColorToDiffuseReflectance(material.baseColor, material.metalness));
	float Fresnel = saturate(luminance(evalFresnel(specularF0, shadowedF90(specularF0), max(0.0f, dot(V, shadingNormal)))));

	// Approximate relative contribution of BRDFs using the Fresnel term
	float specular = Fresnel;
	float diffuse = diffuseReflectance * (1.0f - Fresnel); //< If diffuse term is weighted by Fresnel, apply it here as well

	// Return probability of selecting specular BRDF over diffuse BRDF
	float p = (specular / max(0.0001f, (specular + diffuse)));

	// Clamp probability to avoid undersampling of less prominent BRDF
	return clamp(p, 0.1f, 0.9f);
};

float3 randomReplayPath(uint4 rngState, float3 rayDir, float3 rayOri, float3 throughput, out float3 worldPos1st, out float3 shadingNormal1st)
{
	float3 radiance = float3(0., 0., 0.,);

	[loop]
    for (int bounce = 1; bounce <= giParams.numRays; bounce++) 
	{
		IndirectGbufferRayPayload payload = shootRay(rayOri, rayDir, FLT_MAX);

		if (!payload.hasHit())
		{
			radiance += throughput * float3(0., 0., 0.);
			break;
		}

		// Load material properties at the hit point
		MaterialProperties material = loadMaterialProperties(payload);

		float3 geometryNormal;
		float3 shadingNormal;
		decodeNormals(payload.normal_, geometryNormal, shadingNormal);

		float3 V = -rayDir;
		if (dot(geometryNormal, V) < 0.0f) geometryNormal = -geometryNormal;
		if (dot(geometryNormal, shadingNormal) < 0.0f) shadingNormal = -shadingNormal;

        // Neighborhood around surface point (approximated by tangent plane from geometry normal) is visible, 
        // yet camera would be behind the tangent plane formed by bumped normal -- this can cause black spots 
        // as brdf evaluates to 0 in these regions. To mitigate the issue, rotate the bumped normal towards
        // wo until their dot product becomes greater than zero:
        //
        // 1. Project bumped normal onto plane formed by normal vector wo
        // 2. Slightly rotate the projection towards wo so that their dot product becomes greater than zero
        if(dot(V, geometryNormal) > 0 && dot(V, shadingNormal) < 0)
        {
            V = normalize(V);
            shadingNormal = shadingNormal - dot(shadingNormal, V) * V;
            shadingNormal = 1e-4f * V + shadingNormal;
            shadingNormal = normalize(shadingNormal);
        }	

		if (bounce == 1)
		{
			worldPos1st = payload.position_objectID.xyz;
			shadingNormal1st = shadingNormal;
		}

		radiance += throughput * material.emissive;

		// Evaluate direct light (next event estimation), start by sampling one light 
		SLight light;
		float lightWeight = -1;
		if (sampleLightRIS(rngState, payload.position_objectID.xyz, geometryNormal, light, lightWeight, material, shadingNormal, V)) {

			// Prepare data needed to evaluate the light
			float3 lightVector;
			float lightDistance;
			getLightData(light, payload.position_objectID.xyz, lightVector, lightDistance);
			float3 L = normalize(lightVector);
			
			// If light is not in shadow, evaluate BRDF and accumulate its contribution into radiance
			if (castShadowRay(payload.position_objectID.xyz, geometryNormal, L, lightDistance))
			{
				radiance += throughput * evalCombinedBRDF(shadingNormal, L, V, material) * (getLightIntensityAtPoint(light, lightDistance) * lightWeight);
			}
		}

		// Russian Roulette
		if (bounce > MIN_BOUNCES) {
			float rrProbability = min(0.95f, luminance(throughput));
			if (rrProbability < rand(rngState)) break;
			else throughput /= rrProbability;
		}

		// Sample BRDF to generate the next ray
		// First, figure out whether to sample diffuse or specular BRDF
		int brdfType;
		if (material.metalness == 1.0f && material.roughness == 0.0f) {
			// Fast path for mirrors
			brdfType = SPECULAR_TYPE;
		} else {

			// Decide whether to sample diffuse or specular BRDF (based on Fresnel term)
			float brdfProbability = getBrdfProbability(material, V, shadingNormal);

			if (rand(rngState) < brdfProbability) {
				brdfType = SPECULAR_TYPE;
				throughput /= brdfProbability;
			} else {
				brdfType = DIFFUSE_TYPE;
				throughput /= (1.0f - brdfProbability);
			}
		}

		// Run importance sampling of selected BRDF to generate reflecting ray direction
		float3 brdfWeight;
		float2 u = float2(rand(rngState), rand(rngState));
		if (!evalIndirectCombinedBRDF(u, shadingNormal, geometryNormal, V, material, brdfType, rayDir, brdfWeight)) {
			break; // Ray was eaten by the surface :(
		}

		// Account for surface properties using the BRDF "weight"
		throughput *= brdfWeight;

		// Offset a new ray origin from the hitpoint to prevent self-intersections
		rayOri = offsetRay(payload.position_objectID.xyz, geometryNormal);
	}	

    return radiance;
};

bool getBoundParams(SReservoirPT reservoir, MaterialProperties material, out float3 V, out float3 shadingNormal, out float3 geometryNormal, float3 worldPos, out float3 throughputOut, out float3 rayDirOut, out float3 rayOriOut, out uint4 rngStateOut)
{
    uint4 rngState = reservoir.randomSeed;
    float3 throughput = float3(1., 1., 1.,);

	if (dot(geometryNormal, V) < 0.0f) geometryNormal = -geometryNormal;
	if (dot(geometryNormal, shadingNormal) < 0.0f) shadingNormal = -shadingNormal;

	// Neighborhood around surface point (approximated by tangent plane from geometry normal) is visible, 
	// yet camera would be behind the tangent plane formed by bumped normal -- this can cause black spots 
	// as brdf evaluates to 0 in these regions. To mitigate the issue, rotate the bumped normal towards
	// wo until their dot product becomes greater than zero:
	//
	// 1. Project bumped normal onto plane formed by normal vector wo
	// 2. Slightly rotate the projection towards wo so that their dot product becomes greater than zero
	if(dot(V, geometryNormal) > 0 && dot(V, shadingNormal) < 0)
	{
		V = normalize(V);
		shadingNormal = shadingNormal - dot(shadingNormal, V) * V;
		shadingNormal = 1e-4f * V + shadingNormal;
		shadingNormal = normalize(shadingNormal);
	}

    // Sample BRDF to generate the next ray
    // First, figure out whether to sample diffuse or specular BRDF
    int brdfType;
    if (material.metalness == 1.0f && material.roughness == 0.0f) {
        // Fast path for mirrors
        brdfType = SPECULAR_TYPE;
    } else {

        // Decide whether to sample diffuse or specular BRDF (based on Fresnel term)
        float brdfProbability = getBrdfProbability(material, V, shadingNormal);

        if (rand(rngState) < brdfProbability) {
            brdfType = SPECULAR_TYPE;
            throughput /= brdfProbability;
        } else {
            brdfType = DIFFUSE_TYPE;
            throughput /= (1.0f - brdfProbability);
        }
    }

    // Run importance sampling of selected BRDF to generate reflecting ray direction
    float3 brdfWeight;
    float2 u = float2(rand(rngState), rand(rngState));
    if(!evalIndirectCombinedBRDF(u, shadingNormal, geometryNormal, V, material, brdfType, rayDirOut, brdfWeight))
		return false;

    // Account for surface properties using the BRDF "weight"
    throughput *= brdfWeight;

    // Offset a new ray origin from the hitpoint to prevent self-intersections
    rayOriOut = offsetRay(worldPos, geometryNormal);

    reservoir.randomSeed = rngState;
    throughputOut = throughput;
    rngStateOut = rngState;

	return true;
};

[shader("raygeneration")]
void main() 
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    float4 wposObjectId = ptWposObjectIdOutput[LaunchIndex];
    uint curTemporalIndx = (giParams.frameCount + 1) % 2;
    uint reservoirIndx = LaunchIndex.y * (LaunchDimensions.x + 1) + LaunchIndex.x;

    SReservoirPT reservoirTemporal = reservoirPTTemporalBuffer[curTemporalIndx][reservoirIndx];

    if (wposObjectId.w < 0.f || reservoirTemporal.samplesNumber == 0)
    {
        reservoirPTSpacialBuffer[reservoirIndx] = reservoirTemporal;
        return;
    };

    float4 normalEncode = ptNormalOutput[LaunchIndex];
    float3 geometryNormalCentr;
    float3 shadingNormalCentr;
    decodeNormals(normalEncode, geometryNormalCentr, shadingNormalCentr);

    float target_pdf = luminance(reservoirTemporal.radiance.xyz);

    // Initialize random numbers generator
	RngStateType rngState = initRNG(LaunchIndex, LaunchDimensions, giParams.frameCount);

    SReservoirPT reservoirNew = SReservoirPT(uint4(0, 0, 0, 0), float4(0., 0., 0., 0.), 0., 0, 1., 0.);
    reservoirNew.updateReservoir(rngState, reservoirTemporal, target_pdf * reservoirTemporal.finalWeight * reservoirTemporal.samplesNumber);

    uint lightSamplesCount = reservoirTemporal.samplesNumber;

    int2 neighborOffset;
    int2	neighborIndex;
    SReservoirPT neighborReservoir = SReservoirPT(uint4(0, 0, 0, 0), float4(0., 0., 0., 0.), 0., 0, 1., 0.);

    int neighborsCount = 7;
    int neighborsRange = 1; // Want to sample neighbors within [-neighborsRange, neighborsRange] offset

    const float dist_threshold = 0.01f;
    const float angle_threshold = 25.f * M_PI / 180.f;
    
	[loop]
    for (int i = 0; i < neighborsCount; i++)
    {
        // Generate a random number from range [0, 2 * neighborsRange] then offset in negative direction 
        // by spatialNeighborCount to get range [-neighborsRange, neighborsRange]. 
        // Need to take care of out of bound case hence the max and min
        neighborOffset.x = int(rand(rngState) * float(neighborsRange * 2)) - neighborsRange;
        neighborOffset.y = int(rand(rngState) * float(neighborsRange * 2)) - neighborsRange;
        
        neighborIndex.x = max(0, min(LaunchDimensions.x - 1, LaunchIndex.x + neighborOffset.x));
		neighborIndex.y = max(0, min(LaunchDimensions.y - 1, LaunchIndex.y + neighborOffset.y));

        float4 wposObjectIdN = ptWposObjectIdOutput[neighborIndex];
        reservoirIndx = neighborIndex.y * (LaunchDimensions.x + 1) + neighborIndex.x;
        neighborReservoir = reservoirPTTemporalBuffer[curTemporalIndx][reservoirIndx];

        if (wposObjectIdN.w < 0.f)
            continue;

        normalEncode = ptNormalOutput[neighborIndex];
        float3 geometryNormalN;
        float3 shadingNormalN;
        decodeNormals(normalEncode, geometryNormalN, shadingNormalN);

        // Geometric similarity
        float dist = dot(wposObjectIdN.xyz - wposObjectId.xyz, wposObjectIdN.xyz - wposObjectId.xyz);
        if (dist > dist_threshold  ||
            (dot(shadingNormalCentr.xyz, shadingNormalN.xyz)) < cos(angle_threshold)) {
                continue;
        }

		float target_pdf1 = 0;
		float target_pdf2 = 0;

		target_pdf1 = neighborReservoir.samplesNumber > 0 ? luminance(neighborReservoir.radiance.xyz) : 0.f;
		reservoirNew.updateReservoir(rngState, neighborReservoir, target_pdf1 * neighborReservoir.finalWeight * neighborReservoir.samplesNumber);

		if (neighborReservoir.samplesNumber > 0)
		{
			float2 pixelCenter = float2(neighborIndex) + float2(0.5);
			float2 inUV = pixelCenter/float2(LaunchDimensions);
			float2 d = inUV * 2.0 - 1.0;

			float4 target    = mul(giParams.projInverse , float4(d.x, d.y, 1, 1));
			float4 direction = mul(giParams.viewInverse , float4(normalize(target.xyz), 0));
			float3 rayDir = normalize(direction.xyz);
			float3 V = -rayDir;
			
			//random randomReplayPath
			float3 throughputN;
			float3 rayDirN;
			float3 rayOriN;
			uint4 rngStateN;
			MaterialProperties materialN =  loadMaterialProperties2(neighborIndex);
			if (getBoundParams(reservoirTemporal, materialN, V, shadingNormalN, geometryNormalN, wposObjectIdN.xyz, throughputN, rayDirN, rayOriN, rngStateN))
			{
				float3 worldPos1stN;
				float3 shadingNormal1stN;
				neighborReservoir.radiance.xyz = randomReplayPath(rngStateN, rayDirN, rayOriN, throughputN, worldPos1stN, shadingNormal1stN);

				float target_pdf2 = luminance(neighborReservoir.radiance.xyz);

				// Calculate Jacobian
				float3 w_qq = wposObjectId.xyz - worldPos1stN.xyz;
				const float w_qq_len = length(w_qq);
				w_qq /= w_qq_len;
				float3 w_rq = wposObjectIdN.xyz - worldPos1stN.xyz;
				const float w_rq_len = length(w_rq);
				w_rq /= w_rq_len;
				const float qq = w_qq_len * w_qq_len;
				const float rq = w_rq_len * w_rq_len;
				const float div = rq * abs(dot(w_qq, shadingNormal1stN));
				target_pdf2 *= div == 0 ? 0 : abs(dot(w_rq, shadingNormal1stN)) * qq / div;

				neighborReservoir.samplesNumber += 1;
			}
		}

		reservoirNew.updateReservoir(rngState, neighborReservoir, target_pdf2 * neighborReservoir.finalWeight * neighborReservoir.samplesNumber);

        lightSamplesCount += neighborReservoir.samplesNumber;
    };

    reservoirNew.samplesNumber = lightSamplesCount;
    target_pdf = luminance(reservoirNew.radiance.xyz);
    reservoirNew.finalWeight = target_pdf * reservoirNew.samplesNumber  == 0.f ? 0.f :  reservoirNew.weightSum / (target_pdf * reservoirNew.samplesNumber);

    reservoirPTSpacialBuffer[reservoirIndx] = reservoirNew;
    //reservoirPTSpacialBuffer[reservoirIndx] = reservoirTemporal;
}