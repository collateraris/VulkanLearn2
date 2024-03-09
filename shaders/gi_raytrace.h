struct SGlobalGIParams
{
	mat4 projView;
	mat4 viewInverse;
	mat4 projInverse;
	mat4 prevProjView;
	vec4 camPos;
	uint  frameCount;
	float shadowMult;
	uint lightsCount;
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
	mat4 model;
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


struct DirectInputData
{
	vec4 objectIdX;
	vec4 positionXYZ_normalX;
	vec4 normalYZ_texCoordUV;
};

DirectInputData packDirectInputData(float objIDf, vec3 worldPos, vec3 worldNorm, vec2 uv)
{
	DirectInputData inputData;
    inputData.objectIdX = vec4(objIDf, 0.f, 0.f, 0.f);
    inputData.positionXYZ_normalX = vec4(worldPos.xyz, worldNorm.x);
    inputData.normalYZ_texCoordUV = vec4(worldNorm.yz, uv);
	return inputData;
};

int unpackObjID_DirectInputData(DirectInputData inputData)
{
	return int(inputData.objectIdX.x);
};

vec3 unpackWorldPos_DirectInputData(DirectInputData inputData)
{
	return vec3(inputData.positionXYZ_normalX.xyz);
};

vec3 unpackWorldNorm_DirectInputData(DirectInputData inputData)
{
	return vec3(inputData.positionXYZ_normalX.w, inputData.normalYZ_texCoordUV.xy);
};

vec2 unpackUV_DirectInputData(DirectInputData inputData)
{
	return vec2(inputData.normalYZ_texCoordUV.zw);
};

// The payload used for our indirect global illumination rays
struct IndirectRayPayload
{
	vec4 positionXYZ_normalShadX;
	vec4 normalShadYZ_texCoordUV;
	vec4 worldNormGeometryXYZ_ObjectId;
	vec4 baryCoord;
};

int unpackObjID_IndirectRayPayload(IndirectRayPayload inputData)
{
	return int(inputData.worldNormGeometryXYZ_ObjectId.w);
};

vec3 unpackWorldPos_IndirectRayPayload(IndirectRayPayload inputData)
{
	return vec3(inputData.positionXYZ_normalShadX.xyz);
};

vec3 unpackWorldNormShading_IndirectRayPayload(IndirectRayPayload inputData)
{
	return vec3(inputData.positionXYZ_normalShadX.x, inputData.normalShadYZ_texCoordUV.xy);
};

vec3 unpackWorldNormGeometry_IndirectRayPayload(IndirectRayPayload inputData)
{
	return vec3(inputData.worldNormGeometryXYZ_ObjectId.xyz);
};

vec2 unpackUV_IndirectRayPayload(IndirectRayPayload inputData)
{
	return vec2(inputData.normalShadYZ_texCoordUV.zw);
};

struct DirectOutputData
{
	vec4 worldNormXYZ_albedoX;
	vec4 albedoYZ_FoXY;
	vec4 FoZ_LoXYZ;
	vec4 metalness_roughness;
};

DirectOutputData packDirectOutputData(vec3 worldNorm, vec3 albedo, vec3 Fo, vec3 Lo, float metalness, float roughness)
{
	DirectOutputData inputData;
    inputData.worldNormXYZ_albedoX = vec4(worldNorm, albedo.x);
    inputData.albedoYZ_FoXY = vec4(albedo.yz, Fo.xy);
    inputData.FoZ_LoXYZ = vec4(Fo.z, Lo);
	inputData.metalness_roughness = vec4(metalness, roughness, 0.f, 0.f);
	return inputData;
};

vec3 unpackWorldNorm_DirectOutputData(DirectOutputData inputData)
{
	return vec3(inputData.worldNormXYZ_albedoX.xyz);
};

vec3 unpackAlbedo_DirectOutputData(DirectOutputData inputData)
{
	return vec3(inputData.worldNormXYZ_albedoX.w, inputData.albedoYZ_FoXY.xy);
};

vec3 unpackF0_DirectOutputData(DirectOutputData inputData)
{
	return vec3(inputData.albedoYZ_FoXY.zw, inputData.FoZ_LoXYZ.x);
};

vec3 unpackLo_DirectOutputData(DirectOutputData inputData)
{
	return vec3(inputData.FoZ_LoXYZ.yzw);
};

float unpackMetalness_DirectOutputData(DirectOutputData inputData)
{
	return inputData.metalness_roughness.x;
};

float unpackRoughness_DirectOutputData(DirectOutputData inputData)
{
	return inputData.metalness_roughness.y;
};


#include "ggx_brdf.h"
#include "restir.h"