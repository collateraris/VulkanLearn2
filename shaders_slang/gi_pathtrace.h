
#define M_PI     3.14159265358979323846
#define FLT_MAX 3.402823466e+38F
// Number of candidates used for resampling of analytical lights
#define RIS_CANDIDATES_LIGHTS 64
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
	uint widthScreen;
	uint heightScreen;
};

struct STemporalReservoirInfo
{
	uint currTempIndx;
	uint prevTempIndx;
	uint pad0;
	uint pad1;
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

	bool hasEmissive()
	{
		return (dot(emission_roughness.xyz, emission_roughness.xyz));
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
#define EMISSION_LIGHT   3

struct SLight
{
	float4 position;
	float4 direction;
	float4 color_type;
	float4 position1;
	float4 position2;
	float4 uv0_uv1;
	float4 uv2_objectId_;
};

struct SVertex {

	float4 positionXYZ_normalX;
	float4 normalYZ_texCoordUV;
	float4 tangentXYZ_;
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
	float4x4 modelIT; // for normal
	uint meshIndex;
	uint meshletCount;
	uint materialIndex;
	uint pad1;
};

struct SMaterialData
{
	uint diffuseTexIndex;
	int normalTexIndex;
	int metalnessTexIndex;
	int roughnessTexIndex;
	int emissionTexIndex;
	int opacityTexIndex;
	int pad0;
	int pad1;
	float4 baseColorFactor;
	float4 emissiveFactorMult_emissiveStrength;
	float4 metallicFactor_roughnessFactor;
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

// Reservoir reminder:
// .x: weight sum
// .y: chosen light for the pixel
// .z: the number of samples seen for this current light
// .w: the final adjusted weight for the current pixel following the formula in algorithm 3 (r.W) 
struct SReservoir
{
	float weightSum;
	int lightSampler;
	uint samplesNumber;
	float finalWeight;
	float4 bary__;

	[mutating]
	void updateReservoir(inout RngStateType randSeed, SReservoir res, float weight) {
		weightSum = weightSum + weight; // r.w_sum
		samplesNumber = samplesNumber + 1; // r.M
		if (rand(randSeed) < weight / (weightSum + 1e-6)) {
			lightSampler = res.lightSampler; // r.y
			bary__ = res.bary__;
		}
	};
};

struct SReservoirPT
{
	uint4 randomSeed = uint4(0, 0, 0, 0);
	float4 radiance = float4(0., 0., 0., 0.);
	float weightSum = 0;
	uint samplesNumber = 0;
	float finalWeight = 1;
	float pad0 = 0;

	[mutating]
	void updateReservoir(inout RngStateType randSeed, SReservoirPT res, float weight) {
		weightSum = weightSum + weight; // r.w_sum
		samplesNumber = samplesNumber + 1; // r.M
		if (rand(randSeed) < weight / (weightSum + 1e-6)) {
			radiance = res.radiance; // r.y
			randomSeed = res.randomSeed;
		}
	};
};

// -------------------------------------------------------------------------
//    Utilities
// -------------------------------------------------------------------------

// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
float3 getPerpendicularVector(float3 u)
{
	float3 a = abs(u);
	uint xm = ((a.x - a.y)<0 && (a.x - a.z)<0) ? 1 : 0;
	uint ym = (a.y - a.z)<0 ? (1 ^ xm) : 0;
	uint zm = 1 ^ (xm | ym);
	return cross(u, float3(xm, ym, zm));
}

// Get a cosine-weighted random vector centered around a specified normal direction.
float3 getCosHemisphereSample(inout RngStateType randSeed, float3 hitNorm)
{
	// Get 2 random numbers to select our sample with
	float2 randVal = float2(rand(randSeed), rand(randSeed));

	// Cosine weighted hemisphere sample from RNG
	float3 bitangent = getPerpendicularVector(hitNorm);
	float3 tangent = cross(bitangent, hitNorm);
	float r = sqrt(randVal.x);
	float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
	return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + hitNorm.xyz * sqrt(1 - randVal.x);
}

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

// Any point inside the triangle with vertices V0, V1, and V2 can be expressed as
//        P = b0 * V0 + b1 * V1 + b2 * V2
// where b0 + b1 + b2 = 1. Therefore, only b1 and b2 need to be sampled.
float2 UniformSampleTriangle(float2 u)
{
	// Ref: Eric Heitz. A Low-Distortion Map Between Triangle and Square. 2019.
	float b1, b2;
	
	if (u.y > u.x)
	{
		b1 = u.x * 0.5f;
		b2 = u.y - b1;
	}
	else
	{
		b2 = u.y * 0.5f;
		b1 = u.x - b2;
	}

	return float2(b1, b2);
};

// -------------------------------------------------------------------------
//    Light Functions
// -------------------------------------------------------------------------

// Decodes light vector and distance from Light structure based on the light type
void getLightData(SLight light, float3 hitPosition, out float3 lightVector, out float lightDistance) {
	uint type = uint(light.color_type.w);
	if (type == EMISSION_LIGHT)
	{
		lightVector = light.position.xyz - hitPosition;
		lightDistance = length(lightVector);
	} else if (type == POINT_LIGHT) {
		lightVector = light.position.xyz - hitPosition;
		lightDistance = length(lightVector);
	} else if (type == DIRECTIONAL_LIGHT) {
		lightVector = -light.direction.xyz; 
		lightDistance = FLT_MAX;
	} else {
		lightDistance = FLT_MAX;
		lightVector = float3(0.0f, 1.0f, 0.0f);
	}
};

// Returns intensity of given light at specified distance
float3 getLightIntensityAtPoint(SLight light, float distance) {
	uint type = uint(light.color_type.w);
	if (type == EMISSION_LIGHT || type == POINT_LIGHT) {
		// Cem Yuksel's improved attenuation avoiding singularity at distance=0
		// Source: http://www.cemyuksel.com/research/pointlightattenuation/
		const float radius = 0.5f; //< We hardcode radius at 0.5, but this should be a light parameter
		const float radiusSquared = radius * radius;
		const float distanceSquared = distance * distance;
		const float attenuation = 2.0f / (distanceSquared + radiusSquared + distance * sqrt(distanceSquared + radiusSquared));

		return light.color_type.xyz * attenuation;

	} else if (type == DIRECTIONAL_LIGHT) {
		return light.color_type.xyz;
	} else {
		return float3(0.0f, 0.0f, 0.0f);
	}
};