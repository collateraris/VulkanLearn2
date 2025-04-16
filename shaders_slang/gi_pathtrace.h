
#define M_PI     3.14159265358979323846
#define FLT_MAX 3.402823466e+38F
// Number of candidates used for resampling of analytical lights
#define RIS_CANDIDATES_LIGHTS 8
// Switches between two RNGs
#define USE_PCG 1

struct SGlobalGIParams
{
	float4x4 projView;
	float4x4 viewInverse;
	float4x4 projInverse;
	float4x4 prevProjView;
	float4 camPos;
	uint  frameCount;
	float shadowMult;
	uint lightsCount;
	uint  numRays;
	uint mode;
	uint enableAccumulation;
	uint pad2;
	uint pad3;
};

struct IndirectGbufferRayPayload
{
	float4 albedo_metalness;
	float4 emission_roughness;
	float4 normal_;
	float4 position_objectID;

    bool hasHit()
    {
        return (position_objectID.w > 1e-3);
    }
};

// Helpers for octahedron encoding of normals
float2 octWrap(float2 v)
{
	return float2((1.0f - abs(v.y)) * (v.x >= 0.0f ? 1.0f : -1.0f), (1.0f - abs(v.x)) * (v.y >= 0.0f ? 1.0f : -1.0f));
}

float2 encodeNormalOctahedron(float3 n)
{
	float2 p = float2(n.x, n.y) * (1.0f / (abs(n.x) + abs(n.y) + abs(n.z)));
	p = (n.z < 0.0f) ? octWrap(p) : p;
	return p;
}

float3 decodeNormalOctahedron(float2 p)
{
	float3 n = float3(p.x, p.y, 1.0f - abs(p.x) - abs(p.y));
	float2 tmp = (n.z < 0.0f) ? octWrap(float2(n.x, n.y)) : float2(n.x, n.y);
	n.x = tmp.x;
	n.y = tmp.y;
	return normalize(n);
}

float4 encodeNormals(float3 geometryNormal, float3 shadingNormal) {
	return float4(encodeNormalOctahedron(geometryNormal), encodeNormalOctahedron(shadingNormal));
}

void decodeNormals(float4 encodedNormals, out float3 geometryNormal, out float3 shadingNormal) {
	geometryNormal = decodeNormalOctahedron(encodedNormals.xy);
	shadingNormal = decodeNormalOctahedron(encodedNormals.zw);
}

struct ShadowHitInfo
{
	bool hasHit;
};

#define DIRECTIONAL_LIGHT      1
#define POINT_LIGHT   2

struct SLight
{
	float4 position;
	float4 direction;
	float4 color_type;
};

struct SVertex {

	float4 positionXYZ_normalX;
	float4 normalYZ_texCoordUV;
};

struct SMeshlet
{
	uint dataOffset;
	uint triangleCount;
	uint vertexCount;
	uint pad;
};

struct SObjectData
{
	float4x4 model;
	uint meshIndex;
	uint meshletCount;
	uint diffuseTexIndex;
	int normalTexIndex;
	int metalnessTexIndex;
	int roughnessTexIndex;
	int emissionTexIndex;
	int opacityTexIndex;
};

// -------------------------------------------------------------------------
//    Random
// -------------------------------------------------------------------------

#if USE_PCG
	#define RngStateType uint4
#else
	#define RngStateType uint
#endif

// PCG random numbers generator
// Source: "Hash Functions for GPU Rendering" by Jarzynski & Olano
uint4 pcg4d(uint4 v)
{
	v = v * 1664525u + 1013904223u;

	v.x += v.y * v.w; 
	v.y += v.z * v.x; 
	v.z += v.x * v.y; 
	v.w += v.y * v.z;

	v = v ^ (v >> 16u);

	v.x += v.y * v.w; 
	v.y += v.z * v.x; 
	v.z += v.x * v.y; 
	v.w += v.y * v.z;

	return v;
}

// 32-bit Xorshift random number generator
uint xorshift(inout uint rngState)
{
	rngState ^= rngState << 13;
	rngState ^= rngState >> 17;
	rngState ^= rngState << 5;
	return rngState;
}

// Jenkins's "one at a time" hash function
uint jenkinsHash(uint x) {
	x += x << 10;
	x ^= x >> 6;
	x += x << 3;
	x ^= x >> 11;
	x += x << 15;
	return x;
}

// Converts unsigned integer into float int range <0; 1) by using 23 most significant bits for mantissa
float uintToFloat(uint x) {
	return asfloat(0x3f800000 | (x >> 9)) - 1.0f;
}

#if USE_PCG

// Initialize RNG for given pixel, and frame number (PCG version)
RngStateType initRNG(uint2 pixelCoords, uint2 resolution, uint frameNumber) {
	return RngStateType(pixelCoords.xy, frameNumber, 0); //< Seed for PCG uses a sequential sample number in 4th channel, which increments on every RNG call and starts from 0
}

// Return random float in <0; 1) range  (PCG version)
float rand(inout RngStateType rngState) {
	rngState.w++; //< Increment sample index
	return uintToFloat(pcg4d(rngState).x);
}

#else

// Initialize RNG for given pixel, and frame number (Xorshift-based version)
RngStateType initRNG(uint2 pixelCoords, uint2 resolution, uint frameNumber) {
	RngStateType seed = dot(pixelCoords, uint2(1, resolution.x)) ^ jenkinsHash(frameNumber);
	return jenkinsHash(seed);
}

// Return random float in <0; 1) range (Xorshift-based version)
float rand(inout RngStateType rngState) {
	return uintToFloat(xorshift(rngState));
}

#endif


// -------------------------------------------------------------------------
//    Utilities
// -------------------------------------------------------------------------

// Clever offset_ray function from Ray Tracing Gems chapter 6
// Offsets the ray origin from current position p, along normal n (which must be geometric normal)
// so that no self-intersection can occur.
float3 offsetRay(const float3 p, const float3 n)
{
	static const float origin = 1.0f / 32.0f;
	static const float float_scale = 1.0f / 65536.0f;
	static const float int_scale = 256.0f;

	int3 of_i = int3(int_scale * n.x, int_scale * n.y, int_scale * n.z);

	float3 p_i = float3(
		asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
		asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
		asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

	return float3(abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
		abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
		abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
}


// -------------------------------------------------------------------------
//    Light Functions
// -------------------------------------------------------------------------

// Decodes light vector and distance from Light structure based on the light type
void getLightData(SLight light, float3 hitPosition, out float3 lightVector, out float lightDistance) {
	if (abs(light.color_type.w - POINT_LIGHT) < 1e-6) {
		lightVector = light.position.xyz - hitPosition;
		lightDistance = length(lightVector);
	} else if (abs(light.color_type.w - DIRECTIONAL_LIGHT) < 1e-6) {
		lightVector = -light.direction.xyz; 
		lightDistance = FLT_MAX;
	} else {
		lightDistance = FLT_MAX;
		lightVector = float3(0.0f, 1.0f, 0.0f);
	}
};

// Returns intensity of given light at specified distance
float3 getLightIntensityAtPoint(SLight light, float distance) {
	if (abs(light.color_type.w - POINT_LIGHT) < 1e-6) {
		// Cem Yuksel's improved attenuation avoiding singularity at distance=0
		// Source: http://www.cemyuksel.com/research/pointlightattenuation/
		const float radius = 0.5f; //< We hardcode radius at 0.5, but this should be a light parameter
		const float radiusSquared = radius * radius;
		const float distanceSquared = distance * distance;
		const float attenuation = 2.0f / (distanceSquared + radiusSquared + distance * sqrt(distanceSquared + radiusSquared));

		return light.color_type.xyz * attenuation;

	} else if (abs(light.color_type.w - DIRECTIONAL_LIGHT) < 1e-6) {
		return light.color_type.xyz;
	} else {
		return float3(1.0f, 0.0f, 1.0f);
	}
};