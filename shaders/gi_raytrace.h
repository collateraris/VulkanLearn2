struct SGlobalGIParams
{
	mat4 viewInverse;
	mat4 projInverse;
	vec4 camPos;
	float aoRadius;
	uint  frameCount;
	float shadowMult;
	uint  numRays;
};

const uint SUN_COLOR_TYPE = 0;

struct SLight
{
	vec4 position;
	vec4 direction;
	vec4 color;
	uint type;
	uint pad1;
	uint pad2;
	uint pad3;
};

struct SVertex {

	vec4 positionXYZ_normalX;
	vec4 normalYZ_texCoordUV;
};

struct SObjectData
{
	uint meshIndex;
	uint diffuseTexIndex;
	int normalTexIndex;
	int metalnessTexIndex;
	int roughnessTexIndex;
	int emissionTexIndex;
	int opacityTexIndex;
	uint pad;
};

struct AORayPayload
{
  float aoValue;  // Store 0 if we hit a surface, 1 if we miss all surfaces
};

// The payload used for our indirect global illumination rays
struct IndirectRayPayload
{
	int objectId;
	vec2 texCoord;
	vec3 worldPos;
	vec3 worldNorm;
};

struct DirectInputData
{
	int objectId;
	vec2 texCoord;
	vec3 worldPos;
	vec3 worldNorm;	
};

struct DirectOutputData
{
	float metalness;
	float roughness;	
	vec3 worldNorm; // after normal mapping
	vec3 albedo;
	vec3 F0;
	vec3 Lo;
};

#include "ggx_brdf.h"
#include "restir.h"