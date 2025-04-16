#include "gi_pathtrace.h"

#define STANDARD_RAY_INDEX 0
#define SHADOW_RAY_INDEX 1

[[vk::binding(2, 0)]]
RaytracingAccelerationStructure topLevelAS	: register(t2);

[[vk::binding(6, 0)]]
StructuredBuffer<SLight> lightsBuffer  : register(t6);

[[vk::binding(0, 1)]]
cbuffer MyContantBuffer : register(b0)
{
	SGlobalGIParams giParams;
};

[[vk::binding(0, 2)]]
RWTexture2D<float4> ptOutput : register(u0);


IndirectGbufferRayPayload shootRay(float3 orig, float3 dir)
{
	IndirectGbufferRayPayload payload;
	RayDesc ray;
	ray.Origin = orig;
	ray.Direction = dir;
	ray.TMin = 0.f;
	ray.TMax = FLT_MAX;

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

	// Initialize path tracing data
	float3 radiance = float3(0.0f, 0.0f, 0.0f);
	float3 throughput = float3(1.0f, 1.0f, 1.0f);

	for (int bounce = 0; bounce < giParams.numRays; bounce++) 
	{
		IndirectGbufferRayPayload payload = shootRay(origin.xyz, direction.xyz);

		if (!payload.hasHit())
		{
			radiance += throughput * float3(0., 0., 0.);
			break;
		}

		SLight sunLight = lightsBuffer[0];
		float3 lightVector;
		float lightDistance;
		getLightData(sunLight, payload.position_objectID.xyz, lightVector, lightDistance);
		float3 L = -normalize(lightVector);

		float k = 0.05;
		if (castShadowRay(payload.position_objectID.xyz, payload.normal_.xyz, L, lightDistance))
			k = 3;

		radiance += k * payload.albedo_metalness.xyz / M_PI;
	}

	ptOutput[LaunchIndex] = float4(radiance, 1.);
};
