#pragma once

#include <vk_types.h>
#include <vk_mesh.h>
#include <vk_raytracer_builder.h>
#include <neural_shading/NeuralRadianceCache.h>


class VulkanEngine;
struct RenderObject;
struct IndirectBatch;
struct Scene;

struct MaterialDesc
{
	std::string matName = {};
	std::string diffuseTexture = {};
	uint32_t diffuseTextureIndex;
	std::string normalTexture = {};
	int32_t normalTextureIndex = -1;
	std::string metalnessTexture = {};
	int32_t metalnessTextureIndex = -1;
	std::string roughnessTexture = {};
	int32_t roughnessTextureIndex = -1;
	std::string emissionTexture = {};
	int32_t emissionTextureIndex = -1;
	std::string opacityTexture = {};
	int32_t opacityTextureIndex = -1;
	glm::vec4 baseColorFactor = glm::vec4(1., 1., 1., 1.);
	glm::vec4 emissiveFactorMult_emissiveStrength = glm::vec4(1., 1., 1., 1.);
	glm::vec4 metallicFactor_roughnessFactor_transparent_ = glm::vec4(1., 1., 1., 1.);
};

struct GlobalObjectData
{
	glm::mat4 model;
	glm::mat4 modelIT;//inverse transpose for normal mapping
	uint32_t meshIndex;
	uint32_t meshletCount;
	uint32_t materialIndex;
	uint32_t pad1;
};

struct GlobalMaterialData
{
	uint32_t diffuseTexIndex;
	int32_t normalTexIndex;
	int32_t metalnessTexIndex;
	int32_t roughnessTexIndex;
	int32_t emissionTexIndex;
	int32_t opacityTexIndex;
	int32_t pad0;
	int32_t pad1;	
	glm::vec4 baseColorFactor;
	glm::vec4 emissiveFactorMult_emissiveStrength;
	glm::vec4 metallicFactor_roughnessFactor_transparent_;
};

struct Reservoir
{
	float weightSum = 0;
	int32_t lightSampler = -1;
	uint32_t samplesNumber = 0;
	uint32_t finalWeight = 0;
	vec4 bary__;
};

struct ReservoirPT
{
	glm::ivec4 randomSeed = glm::ivec4(0, 0, 0, 0);
	glm::vec4 radiance = glm::vec4(1., 0., 1., 1.);
	float weightSum = 0;
	uint32_t samplesNumber = 0;
	float finalWeight = 1;
	float pad0 = 0;
};

class ResourceManager
{
public:

	std::unordered_map<std::string, std::unique_ptr<Texture>> textureCache;
	std::array<std::unique_ptr<AllocatedSampler>, (uint32_t)ESamplerType::MAX> engineSamplerCache;
	std::array<std::unique_ptr<Texture>, (uint32_t)ETextureResourceNames::MAX> engineTextureCache;
	std::array<std::unique_ptr<AllocateDescriptor>, (uint32_t)EDescriptorResourceNames::MAX> engineDescriptorCache;
	std::vector<std::unique_ptr<Mesh>> meshList;
	std::vector<std::unique_ptr<MaterialDesc>> matDescList; 
	std::vector<Texture*> textureList;

	//default array of renderable objects
	std::vector<RenderObject> renderables;
	std::vector<IndirectBatch> indirectBatchRO;
	AllocatedBuffer globalObjectBuffer;
	AllocatedBuffer globalMaterialBuffer;

	AllocatedBuffer globalReservoirDIInitBuffer;
	AllocatedBuffer globalReservoirDITemporalBuffer[2];
	AllocatedBuffer globalReservoirDISpacialBuffer;

	AllocatedBuffer globalReservoirPTInitBuffer;
	AllocatedBuffer globalReservoirPTTemporalBuffer[2];
	AllocatedBuffer globalReservoirPTSpacialBuffer;

	std::unique_ptr<NeuralRadianceCache> nrc_cache;

	VulkanRaytracerBuilder _rtBuilder;
	glm::vec3 maxCube = { std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()};
	glm::vec3 minCube = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};

	uint32_t store_texture(std::string& name);

	AllocatedSampler* create_engine_sampler(ESamplerType sampleNameId);
	AllocatedSampler* get_engine_sampler(ESamplerType sampleNameId);

	Texture* create_engine_texture(ETextureResourceNames texNameId);
	Texture* get_engine_texture(ETextureResourceNames texNameId);

	AllocateDescriptor* create_engine_descriptor(EDescriptorResourceNames descrNameId);
	AllocateDescriptor* get_engine_descriptor(EDescriptorResourceNames descrNameId);

	static void load_meshes(VulkanEngine* _engine, const std::vector<std::unique_ptr<Mesh>>& meshList);
	static void load_images(VulkanEngine* _engine, const std::unordered_map<std::string, std::unique_ptr<Texture>>& textureCache);
	static void init_samplers(VulkanEngine* _engine, ResourceManager& resManager);

	static std::vector<IndirectBatch> compact_draws(RenderObject* objects, int count);
	static void init_scene(VulkanEngine* _engine, ResourceManager& resManager, Scene& scene);
	static void init_nrc_cache(VulkanEngine* _engine, ResourceManager& resManager);

	static void init_rt_scene(VulkanEngine* _engine, ResourceManager& resManager);
	static void init_global_bindless_descriptor(VulkanEngine* _engine, ResourceManager& resManager);
};