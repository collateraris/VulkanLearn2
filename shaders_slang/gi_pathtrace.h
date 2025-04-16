
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

#define LightSun            1
#define LightPoint          2

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