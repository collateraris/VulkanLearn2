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
	uint mode;
	uint enableAccumulation;
	uint pad2;
	uint pad3;
};

const uint SUN_COLOR_TYPE = 0;

#define LightSun            1
#define LightPoint          2

struct SLight
{
	vec4 position;
	vec4 direction;
	vec4 color_type;
	vec4 position1;
	vec4 position2;
	vec4 uv0_uv1;
	vec4 uv2_objectId_;
};

struct SVertex {

	vec4 positionXYZ_normalX;
	vec4 normalYZ_texCoordUV;
	vec4 tangentXYZ_;
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
	mat4 model;
	mat4 modelIT;
	uint meshIndex;
	uint meshletCount;
	uint diffuseTexIndex;
	int normalTexIndex;
	int metalnessTexIndex;
	int roughnessTexIndex;
	int emissionTexIndex;
	int opacityTexIndex;
	vec4 baseColorFactor;
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
};

struct IndirectGbufferRayPayload
{
	vec4 albedo_metalness;
	vec4 emission_roughness;
	vec4 normal_;
	vec4 position_objectID;
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
	return vec3(inputData.positionXYZ_normalShadX.w, inputData.normalShadYZ_texCoordUV.xy);
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
	vec4 kD;
};

DirectOutputData packDirectOutputData(vec3 worldNorm, vec3 albedo, vec3 Fo, vec3 Lo, float metalness, float roughness, vec3 kD)
{
	DirectOutputData inputData;
    inputData.worldNormXYZ_albedoX = vec4(worldNorm, albedo.x);
    inputData.albedoYZ_FoXY = vec4(albedo.yz, Fo.xy);
    inputData.FoZ_LoXYZ = vec4(Fo.z, Lo);
	inputData.metalness_roughness = vec4(metalness, roughness, 0.f, 0.f);
	inputData.kD.xyz = kD;
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

struct VbufferExtraCommonData
{
    vec3 worldPos;
    vec3 worldNorm;
    vec2 uvCoord;
    float pad;
};

struct PBRShadeData
{
	vec4 albedo_metalness;
	vec4 emission_roughness;
	vec4 worldPos;
	vec4 worldNorm;	
};


#include "ggx_brdf.h"
#include "restir.h"