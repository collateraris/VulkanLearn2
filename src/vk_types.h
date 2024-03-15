// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#define MESHSHADER_ON 0
#define GI_RAYTRACER_ON 1 
#define VBUFFER_ON 1
#define GBUFFER_ON 0
#define VULKAN_DEBUG_ON 1
#define STREAMLINE_ON 0

#include <volk.h>
#include "vk_mem_alloc.h"
#include <taskflow.hpp>

#define VK_ASSERT assert

#define BIT(x) 1u << x

constexpr uint32_t VULKAN_NUM_ATTACHMENTS = 8;
constexpr uint32_t VULKAN_MAX_LIGHT_COUNT = 300 * 300 * 300;

enum class EResOp : uint8_t
{
	NONE = 0,
	WRITE,
	READ,
};

//we will add our main reusable types here

struct AllocatedBuffer {
    VkBuffer _buffer = VK_NULL_HANDLE;
    VmaAllocation _allocation = VK_NULL_HANDLE;
	uint32_t _size;
};

struct AllocatedImage {
    VkImage _image = VK_NULL_HANDLE;
    VmaAllocation _allocation = VK_NULL_HANDLE;
};

struct AllocateDescriptor
{
	VkDescriptorSet set;
	VkDescriptorSetLayout setLayout;
};

// identifies the underlying resource type in a binding
enum class EResourceType : uint8_t
{
	None = 0,
	Texture_SRV,
	Texture_UAV,
	TypedBuffer_SRV,
	TypedBuffer_UAV,
	StructuredBuffer_SRV,
	StructuredBuffer_UAV,
	RawBuffer_SRV,
	RawBuffer_UAV,
	ConstantBuffer,
	VolatileConstantBuffer,
	Sampler,
	RayTracingAccelStruct,
	PushConstants,

	MAX
};

enum ETexFlags: uint32_t
{
	NO_MIPS = BIT(1),
	HDR_CUBEMAP = BIT(2),
};

enum class ESamplerType : uint32_t
{
	NONE = 0,
	NEAREST_REPEAT,
	NEAREST_CLAMP,
	NEAREST_MIRRORED_REPEAT,
	LINEAR_REPEAT,
	LINEAR_CLAMP,
	LINEAR_MIRRORED_REPEAT,	
	MAX,
};

struct AllocatedSampler
{
	ESamplerType samplerType = ESamplerType::NEAREST_REPEAT;
	VkSampler sampler;
};


struct Texture {
	AllocatedImage image;
	VkImageView imageView = VK_NULL_HANDLE;
	VkExtent3D extend;
	VkImageCreateInfo createInfo;
	uint32_t flags = 0;
	uint32_t mipLevels = 0; 
	bool bIsSwapChainImage = false;
	ESamplerType samplerType = ESamplerType::NEAREST_REPEAT;
	VkAccessFlagBits currAccessFlag = VK_ACCESS_NONE;
	VkImageLayout currImageLayout = VK_IMAGE_LAYOUT_GENERAL;
	VkPipelineStageFlagBits currPipStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
};

struct Resources
{
	Texture* texture;
};

struct DescriptorInfo
{
	VkDescriptorBufferInfo bufferInfo;

	VkDescriptorImageInfo imageInfo;
};

struct AccelerationStruct
{
	VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
	AllocatedBuffer               buffer;
};

enum class MeshpassType : uint8_t {
	None = 0,
	Forward = 1,
	Transparency = 2,
	DirectionalShadow = 3,
	MAX
};

struct Stats
{
	double frameCpuAvg = 0;
	double frameGpuAvg = 0;
	uint32_t triangleCount = 0;
	double trianglesPerSec = 0;
};

enum class EPipelineType : uint32_t
{
	NoInit = 0,
	Bindless_TaskMeshIndirectForward,
	DrawIndirectForward,
	PyramidDepthReduce,
	ComputePrepassForTaskMeshIndirect,
	BaseRaytracer,
	FullScreen,
	VbufferGenerate,
	VBufferShading,
	GBufferGenerate,
	GBufferShading,
	GI_Raytracing,
	SimpleAccumulation,
	DrawHDRtoEnvMap,
	DrawEnvMapToIrradianceMap,
	DrawEnvMapToPrefilteredMap,
	DrawBRDFLUT,
	ReSTIR_Init,
	ReSTIR_DI_Temporal,
	ReSTIR_DI_SpaceReuse,
	ReSTIR_UpdateReservoir_PlusShade,
	NRD_RaytraceDenoiseShader_0,
	NRD_RaytraceDenoiseShader_1,
	NRD_RaytraceDenoiseShader_2,
	NRD_RaytraceDenoiseShader_3,
	NRD_RaytraceDenoiseShader_4,
	NRD_RaytraceDenoiseShader_5,
	NRD_RaytraceDenoiseShader_6,
	NRD_RaytraceDenoiseShader_7,
	NRD_RaytraceDenoiseShader_8,
	NRD_RaytraceDenoiseShader_9,
	NRD_RaytraceDenoiseShader_10,
	NRD_RaytraceDenoiseShader_11,
	NRD_RaytraceDenoiseShader_12,
	NRD_RaytraceDenoiseShader_13,
	NRD_RaytraceDenoiseShader_14,
	NRD_RaytraceDenoiseShader_15,
	NRD_RaytraceDenoiseShader_16,
	NRD_RaytraceDenoiseShader_17,
	NRD_RaytraceDenoiseShader_18,
	NRD_RaytraceDenoiseShader_19,
	NRD_RaytraceDenoiseShader_20,
	NRD_RaytraceDenoiseShader_21,
	NRD_RaytraceDenoiseShader_22,
	NRD_RaytraceDenoiseShader_23,
	Max,
};

enum class ETextureResourceNames : uint32_t
{
	GBUFFER_WPOS = 0,
	GBUFFER_NORM,
	GBUFFER_UV,
	GBUFFER_OBJ_ID,
	GBUFFER_DEPTH_BUFFER,
	VBUFFER,
	VBUFFER_DEPTH_BUFFER,
	IBL_ENV,
	IBL_IRRADIANCE,
	IBL_PREFILTEREDENV,
	IBL_BRDFLUT,
	ReSTIR_INIT_RESERVOIRS,
	ReSTIR_DI_CURRENT_RESERVOIRS,
	ReSTIR_DI_PREV_RESERVOIRS,
	ReSTIR_INDIRECT_LO,
	ReSTIR_DI_SPACIAL_RESERVOIRS,
	DenoiserMotionVectors,
	DenoiserNormalRoughness,
	DenoiserViewspaceZ,
	DenoiserSpecRadianceHitDist,
	DenoiserDiffRadianceHitDist,
	DenoiserOutSpecRadianceHitDist_0,
	DenoiserOutSpecRadianceHitDist_1,
	DenoiserOutSpecRadianceHitDist_2,
	DenoiserOutDiffRadianceHitDist_0,
	DenoiserOutDiffRadianceHitDist_1,
	DenoiserOutDiffRadianceHitDist_2,
	DenoiserOutValidation,
	DenoiserDisocclusionThresholdMix,
	MAX,
};

enum class EDescriptorResourceNames : uint32_t
{
	Bindless_Scene = 0,
	IBL,
	GI_GlobalUniformBuffer_Frame0,
	GI_GlobalUniformBuffer_Frame1,
	MAX
};

enum class EBufferResourceNames : uint32_t
{
	GlobalUniformBuffer_Frame0 = 0,
	GlobalUniformBuffer_Frame1 = 1,

};

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>
#include <assert.h>
#include <array>
#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <format>
#include <algorithm>
#include <type_traits>
#include <optional>
#include <limits>

#ifdef __GNUC__
#define PACKED_STRUCT __attribute__((packed,aligned(1)))
#else
#define PACKED_STRUCT
#endif

using glm::vec2;
using glm::vec4;

struct PACKED_STRUCT gpuvec4
{
	float x, y, z, w;

	gpuvec4() = default;
	explicit gpuvec4(float v) : x(v), y(v), z(v), w(v) {}
	gpuvec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
	explicit gpuvec4(const vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}
};

struct PACKED_STRUCT gpumat4
{
	float data_[16];

	gpumat4() = default;
	explicit gpumat4(const glm::mat4& m) { memcpy(data_, glm::value_ptr(m), 16 * sizeof(float)); }
};

struct SceneConfig
{
	std::string fileName;
	std::string hdrCubemapPath;
	float scaleFactor = 1.;
	glm::mat4 model = glm::mat4(1.0);
};

template <class T>
concept Integral = std::is_integral<T>::value;

static inline uint32_t trailing_zeroes(uint32_t x)
{
	unsigned long result;
	if (_BitScanForward(&result, x))
		return result;
	else
		return 32;
}

template <typename T>
inline void for_each_bit(uint32_t value, const T& func)
{
	while (value)
	{
		uint32_t bit = trailing_zeroes(value);
		func(bit);
		value &= ~(1u << bit);
	}
}