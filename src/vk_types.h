// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#define MESHSHADER_ON 0
#define GI_RAYTRACER_ON 0 
#define ReSTIR_PATHTRACER_ON 0
#define PATHTRACER_ON 1
#if GI_RAYTRACER_ON || ReSTIR_PATHTRACER_ON || PATHTRACER_ON
#define RAYTRACER_ON 1 
#endif
#define VBUFFER_ON 0
#define GBUFFER_ON 0
#define VULKAN_DEBUG_ON 0
#define VULKAN_RAYTRACE_DEBUG_ON 0
#if VULKAN_RAYTRACE_DEBUG_ON == 1
#define VULKAN_DEBUG_ON 1
#endif
#define STREAMLINE_ON 0

#include <volk.h>
#include "vk_mem_alloc.h"

enum ERenderMode
{
	None,
	ReSTIR,
	Pathtracer,
	ReSTIR_NRC,
};


#define VK_ASSERT assert

#define BIT(x) 1u << x

constexpr uint32_t VULKAN_NUM_ATTACHMENTS = 8;
constexpr uint32_t VULKAN_MAX_LIGHT_COUNT = 300 * 300 * 300;

enum class EResOp : uint8_t
{
	NONE = 0,
	WRITE,
	READ,
	READ_STORAGE,
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
	Generate_Flux,
	PathTracer,
	GI_Raytracing,
	SimpleAccumulation,
	DrawHDRtoEnvMap,
	DrawEnvMapToIrradianceMap,
	DrawEnvMapToPrefilteredMap,
	DrawBRDFLUT,
	Raytrace_Reflection,
	PT_Gbuffer,
	PT_Reference,
	ReSTIR_Init,
	ReSTIR_DI_Init,
	ReSTIR_DI_Temporal,
	ReSTIR_DI_SpaceReuse,
	ReSTIR_GI_Temporal,
	ReSTIR_GI_SpaceReuse,
	ReSTIR_PT_Temporal,
	ReSTIR_PT_SpaceReuse,
	ReSTIR_UpdateReservoir_PlusShade,
	NRC_Optimize, 
	NRC_Training,
	NRC_Inference,
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
	ReSTIR_PT,
	Max,
};

enum class ETextureResourceNames : uint32_t
{
	GBUFFER_WPOS = 0,
	GBUFFER_NORM,
	GBUFFER_UV,
	GBUFFER_OBJ_ID,
	GBUFFER_DEPTH_BUFFER,
	PT_GBUFFER_ALBEDO_METALNESS,
	PT_GBUFFER_EMISSION_ROUGHNESS,
	PT_GBUFFER_NORMAL,
	PT_GBUFFER_WPOS_OBJECT_ID,
	PT_REFERENCE_OUTPUT,
	PT_REFERENCE_ACCUMULATE,
	VBUFFER,
	VBUFFER_DEPTH_BUFFER,
	PATHTRACER_OUTPUT,
	PATHTRACER_DEPTH_BUFFER,
	IBL_ENV,
	IBL_IRRADIANCE,
	IBL_PREFILTEREDENV,
	IBL_BRDFLUT,
	ReSTIR_PT_OUTPUT,
	ReSTIR_INIT_RESERVOIRS,
	ReSTIR_DI_CURRENT_RESERVOIRS,
	ReSTIR_DI_PREV_RESERVOIRS,
	ReSTIR_GI_CURRENT_RESERVOIRS,
	ReSTIR_GI_PREV_RESERVOIRS,
	ReSTIR_DI_SPACIAL_RESERVOIRS,
	ReSTIR_GI_SPACIAL_RESERVOIRS,
	ReSTIR_INDIRECT_LO_INIT,
	ReSTIR_GI_SAMPLES_POSITION_INIT,
	ReSTIR_GI_SAMPLES_NORMAL_INIT,
	ReSTIR_INDIRECT_LO_PREV,
	ReSTIR_GI_SAMPLES_POSITION_PREV,
	ReSTIR_GI_SAMPLES_NORMAL_PREV,
	ReSTIR_INDIRECT_LO_CURRENT,
	ReSTIR_GI_SAMPLES_POSITION_CURRENT,
	ReSTIR_GI_SAMPLES_NORMAL_CURRENT,
	ReSTIR_INDIRECT_LO_SPACIAL,
	ReSTIR_GI_SAMPLES_POSITION_SPACIAL,
	ReSTIR_GI_SAMPLES_NORMAL_SPACIAL,
	RAYTRACE_REFLECTION,
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
	NRC_GlobalUniformBuffer_Frame0,
	NRC_GlobalUniformBuffer_Frame1,
	NRC_MLP_Optimize,
	NRC_MLP_Train_Inference,
	GBuffer,
	FluxData,
	MAX
};

enum class EBufferResourceNames : uint32_t
{
	GlobalUniformBuffer_Frame0 = 0,
	GlobalUniformBuffer_Frame1 = 1,

};

#define GLM_ENABLE_EXPERIMENTAL
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
#include <random>

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

struct LightConfig
{
	bool bUseSun = false;
	bool bUseUniformGeneratePointLight = false;
	uint8_t numUniformPointLightPerAxis = 0;
	glm::vec3 sunDirection = glm::vec3(1., 0, 0.);
	glm::vec3 sunColor = glm::vec3(1., 0.1, 0.1);
};

struct SceneConfig
{
	std::string fileName;
	std::string hdrCubemapPath;
	float scaleFactor = 1.;
	glm::mat4 model = glm::mat4(1.0);
	LightConfig lightConfig;
	bool bUseCustomCam = false;
	glm::vec3 camPos = glm::vec3(1.);
	float camPith = 1;
	float camYaw = 1;
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

struct alignas(16) SGlobalRQParams
{
	glm::mat4 view;
	glm::mat4 proj;
};

struct alignas(16) GlobalGIParams
{
	glm::mat4 projView;
	glm::mat4 viewInverse;
	glm::mat4 projInverse;
	glm::mat4 prevProjView;
	glm::vec4 camPos;
	uint32_t  frameCount;
	float shadowMult;
	uint32_t lightsCount;
	uint32_t  numRays;
	int32_t sunIndex = -1;
	uint32_t enableAccumulation = 0;
	uint32_t widthScreen = 0;
	uint32_t heightScreen = 0;
	glm::vec4 gridMax = glm::vec4(1);
	glm::vec4 gridMin = glm::vec4(1);
};

struct STemporalReservoirInfo
{
	uint32_t currTempIndx;
	uint32_t prevTempIndx;
	uint32_t pad0;
	uint32_t pad1;
};

namespace donut::math
{
	// "uint" is a lot shorter than "unsigned int"
	typedef unsigned int uint;

	constexpr float PI_f = 3.141592654f;
	constexpr double PI_d = 3.14159265358979323;

	// Convenient float constants
	constexpr float epsilon = 1e-6f;		// A reasonable general-purpose epsilon
	constexpr float infinity = std::numeric_limits<float>::infinity();
	constexpr float NaN = std::numeric_limits<float>::quiet_NaN();

	// Generic min/max/abs/clamp/saturate
	template <typename T>
	constexpr T min(T a, T b) { return (a < b) ? a : b; }
	template <typename T>
	constexpr T max(T a, T b) { return (a < b) ? b : a; }
	template <typename T>
	constexpr T abs(T a) { return (a < T(0)) ? -a : a; }
	template <typename T>
	constexpr T clamp(T value, T lower, T upper) { return min(max(value, lower), upper); }
	template <typename T>
	constexpr T saturate(T value) { return clamp(value, T(0), T(1)); }

	// Generic lerp
	template <typename T>
	constexpr T lerp(T a, T b, float u) { return a + (b - a) * u; }

	// Generic square
	template <typename T>
	constexpr T square(T a) { return a * a; }

	// Equality test with epsilon
	//constexpr bool isnear(float a, float b, float eps = dm::epsilon)
	//{
	//	return (abs(b - a) < eps);
	//}

	// Test for finiteness
	inline bool isfinite(float f)
	{
		union { uint i; float f; } u;
		u.f = f;
		return ((u.i & 0x7f800000) != 0x7f800000);
	}

	// Rounding to nearest integer
	inline int round(float f)
	{
		return int(floor(f + 0.5f));
	}

	// Modulus with always positive remainders (assuming positive divisor)
	inline int modPositive(int dividend, int divisor)
	{
		int result = dividend % divisor;
		if (result < 0)
			result += divisor;
		return result;
	}
	inline float modPositive(float dividend, float divisor)
	{
		float result = fmod(dividend, divisor);
		if (result < 0)
			result += divisor;
		return result;
	}

	// Base-2 exp and log
	inline float exp2f(float x) { return expf(0.693147181f * x); }
	inline float log2f(float x) { return 1.442695041f * logf(x); }

	inline bool ispow2(int x) { return (x > 0) && ((x & (x - 1)) == 0); }

	// Integer division, with rounding up (assuming positive arguments)
	inline int div_ceil(int dividend, int divisor) { return (dividend + (divisor - 1)) / divisor; }

	// Integer rounding to multiples
	inline int roundDown(int i, int multiple) { return (i / multiple) * multiple; }
	inline int roundUp(int i, int multiple) { return ((i + (multiple - 1)) / multiple) * multiple; }

	// Advance a pointer by a given number of bytes, regardless of pointer's type
	// (note: number of bytes can be negative)
	template <typename T>
	inline T* advanceBytes(T* ptr, int bytes)
	{
		return (T*)((char*)ptr + bytes);
	}

	inline float degrees(float rad) { return rad * (180.f / PI_f); }
	inline float radians(float deg) { return deg * (PI_f / 180.f); }
	inline double degrees(double rad) { return rad * (180.0 / PI_d); }
	inline double radians(double deg) { return deg * (PI_d / 180.0); }

	template<typename T>
	constexpr T insertBits(T value, int width, int offset)
	{
		return T((value & ((T(1) << width) - 1)) << offset);
	}

	template<typename T>
	constexpr T extractBits(T value, int width, int offset)
	{
		return T((value >> offset) & ((T(1) << width) - 1));
	}
}