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
	vec3 color;    // The (returned) color in the ray's direction
	uint randSeed;
};

#include "ggx_brdf.h"