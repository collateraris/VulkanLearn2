#include "gi_pathtrace.h"
#include "brdf.h"

#define STANDARD_RAY_INDEX 0
#define SHADOW_RAY_INDEX 1

// Defines after how many bounces will be the Russian Roulette applied
#define MIN_BOUNCES 3

[[vk::binding(1, 0)]]
Texture2D<float4> texSet[] : register(t1);

[[vk::binding(8, 0)]]
SamplerState      linearSampler : register(s8);

[[vk::binding(2, 0)]]
RaytracingAccelerationStructure topLevelAS	: register(t2);

[[vk::binding(3, 0)]]
StructuredBuffer<SObjectData> objectBuffer  : register(t3);

[[vk::binding(6, 0)]]
StructuredBuffer<SLight> lightsBuffer  : register(t6);

[[vk::binding(0, 1)]]
cbuffer MyContantBuffer : register(b0)
{
	SGlobalGIParams giParams;
};

[[vk::binding(0, 2)]]
RWTexture2D<float4> ptOutput : register(u0);

[[vk::binding(1, 2)]]
RWTexture2D<float4> accumulationBuffer	: register(u1);


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
}

// Samples a random light from the pool of all lights using simplest uniform distirbution
bool sampleLightUniform(inout RngStateType rngState, float3 hitPosition, float3 surfaceNormal, out SLight light, out float lightSampleWeight) {

	if (giParams.lightsCount == 0) return false;

	uint randomLightIndex = min(giParams.lightsCount - 1, uint(rand(rngState) * giParams.lightsCount));
	light = lightsBuffer[randomLightIndex];

	// PDF of uniform distribution is (1/light count). Reciprocal of that PDF (simply a light count) is a weight of this sample
	lightSampleWeight = float(giParams.lightsCount);

	return true;
};

// Samples a random light from the pool of all lights using RIS (Resampled Importance Sampling)
bool sampleLightRIS(inout RngStateType rngState, float3 hitPosition, float3 surfaceNormal, out SLight selectedSample, out float lightSampleWeight) {
	
	if (giParams.lightsCount == 0) return false;

	selectedSample = (SLight) 0;
	float totalWeights = 0.0f;
	float samplePdfG = 0.0f;

	for (int i = 0; i < RIS_CANDIDATES_LIGHTS; i++) {

		float candidateWeight;
		SLight candidate;
		if (sampleLightUniform(rngState, hitPosition, surfaceNormal, candidate, candidateWeight)) {

			uint type = uint(candidate.color_type.w);
			if (type == EMISSION_LIGHT)
			{
				float2 uv = float2(rand(rngState), rand(rngState));
				float2 bary = UniformSampleTriangle(uv);
				candidate.position.xyz = (1.0f - bary.x - bary.y) * candidate.position.xyz + bary.x * candidate.position1.xyz + bary.y * candidate.position2.xyz;
				float2 texCoord = (1.0f - bary.x - bary.y) * candidate.uv0_uv1.xy + bary.x * candidate.uv0_uv1.zw + bary.y * candidate.uv2_objectId_.xy;
				SObjectData shadeData = objectBuffer[uint(candidate.uv2_objectId_.z)];
				candidate.color_type.xyz = texSet[shadeData.emissionTexIndex].SampleLevel(linearSampler, texCoord, 0).rgb;  
			}
			float3	lightVector;
			float lightDistance;
			getLightData(candidate, hitPosition, lightVector, lightDistance);

			float3 L = normalize(lightVector);
			float candidatePdfG = luminance(getLightIntensityAtPoint(candidate, length(lightVector)));
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


[shader("raygeneration")]
void main() 
{
	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;
    const float2 pixelCenter = float2(LaunchIndex) + float2(0.5);
    const float2 inUV = pixelCenter/float2(LaunchDimensions);
    float2 d = inUV * 2.0 - 1.0;

	float4 origin    = mul(giParams.viewInverse , float4(0, 0, 0, 1));
    float4 target    = mul(giParams.projInverse , float4(d.x, d.y, 1, 1));
    float4 direction = mul(giParams.viewInverse , float4(normalize(target.xyz), 0));

	float3 rayDir = normalize(direction.xyz);
	float3 rayOri = origin.xyz;

	// Initialize random numbers generator
	RngStateType rngState = initRNG(LaunchIndex, LaunchDimensions, giParams.frameCount);

	// Initialize path tracing data
	float3 radiance = float3(0.0f, 0.0f, 0.0f);
	float3 throughput = float3(1.0f, 1.0f, 1.0f);

	for (int bounce = 0; bounce <= giParams.numRays; bounce++) 
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
#if 1
        if(dot(V, geometryNormal) > 0 && dot(V, shadingNormal) < 0)
        {
            V = normalize(V);
            shadingNormal = shadingNormal - dot(shadingNormal, V) * V;
            shadingNormal = 1e-4f * V + shadingNormal;
            shadingNormal = normalize(shadingNormal);
        }
#endif	
		radiance += throughput * material.emissive;// * 10 + material.baseColor/M_PI;
		//radiance = shadingNormal;
		//radiance = float3(material.metalness, material.metalness, material.metalness);
		//radiance += float3(material.baseColor.x, material.baseColor.x, material.baseColor.x);
		//break;

		// Evaluate direct light (next event estimation), start by sampling one light 
		SLight light;
		float lightWeight = -1;
		if (sampleLightRIS(rngState, payload.position_objectID.xyz, geometryNormal, light, lightWeight)) {

			// Prepare data needed to evaluate the light
			float3 lightVector;
			float lightDistance;
			getLightData(light, payload.position_objectID.xyz, lightVector, lightDistance);
			float3 L = normalize(lightVector);

			if (castShadowRay(payload.position_objectID.xyz, geometryNormal, L, lightDistance))
			{
				// If light is not in shadow, evaluate BRDF and accumulate its contribution into radiance
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

	// Temporal accumulation
	float3 previousColor = accumulationBuffer[LaunchIndex].rgb;
	float3 accumulatedColor = radiance;
	if (giParams.enableAccumulation) accumulatedColor = previousColor + radiance;
	accumulationBuffer[LaunchIndex] = float4(accumulatedColor, 1.0f);	

	ptOutput[LaunchIndex] = float4(radiance, 1.);
};
