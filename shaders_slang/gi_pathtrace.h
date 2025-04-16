
#define M_PI     3.14159265358979323846
#define FLT_MAX 3.402823466e+38F

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
	uint pad1;
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

// Decodes light vector and distance from Light structure based on the light type
void getLightData(SLight light, float3 hitPosition, out float3 lightVector, out float lightDistance) {
	if (abs(light.color_type.w - POINT_LIGHT) < 1e-6) {
		lightVector = light.position.xyz - hitPosition;
		lightDistance = length(lightVector);
	} else if (abs(light.color_type.w - DIRECTIONAL_LIGHT) < 1e-6) {
		lightVector = light.direction.xyz; 
		lightDistance = FLT_MAX;
	} else {
		lightDistance = FLT_MAX;
		lightVector = float3(0.0f, 1.0f, 0.0f);
	}
}