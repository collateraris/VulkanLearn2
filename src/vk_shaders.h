#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>


struct ShaderModule {
	std::vector<uint32_t> code;
	VkShaderModule module;
};

class ShaderLoader
{
public:

	static bool load_shader_module(VkDevice device, const char* filePath, ShaderModule* outShaderModule);

	static bool hlsl_to_spirv_cross_compiler(const std::string& filePath, const std::string& shaderType, const std::string& entryName, const std::vector<std::string>& macros);

	static uint32_t hash_descriptor_layout_info(VkDescriptorSetLayoutCreateInfo* info);
};

//holds all information for a given shader set for pipeline
struct ShaderEffect {

	struct ReflectionOverrides {
		const char* name;
		VkDescriptorType overridenType;
	};

	size_t add_stage(ShaderModule* shaderModule, VkShaderStageFlagBits stage);

	void set_user_flag(VkDescriptorSetLayoutCreateFlagBits flag);

	void reflect_layout(VkDevice device, ReflectionOverrides* overrides, int overrideCount);

	void fill_stages(std::vector<VkPipelineShaderStageCreateInfo>& pipelineStages);
	VkPipelineLayout builtLayout;

	struct ReflectedBinding {
		uint32_t set;
		uint32_t binding;
		VkDescriptorType type;
	};
	std::unordered_map<std::string, ReflectedBinding> bindings;
	std::array<VkDescriptorSetLayout, 4> setLayouts;
	std::array<uint32_t, 4> setHashes;
private:
	struct ShaderStage {
		ShaderModule* shaderModule;
		VkShaderStageFlagBits stage;
	};

	struct DescriptorSetLayoutData {
		uint32_t set_number;
		VkDescriptorSetLayoutCreateInfo create_info;
		std::vector<VkDescriptorSetLayoutBinding> bindings;
	};

	std::vector<DescriptorSetLayoutData> set_layouts;

	std::vector<VkPushConstantRange> constant_ranges;

	std::vector<ShaderStage> stages;

	std::optional<VkDescriptorSetLayoutCreateFlagBits> user_flag;
};

struct ShaderDescriptorBinder {

	struct BufferWriteDescriptor {
		int dstSet;
		int dstBinding;
		VkDescriptorType descriptorType;
		VkDescriptorBufferInfo bufferInfo;

		uint32_t dynamic_offset;
	};

	void bind_buffer(const char* name, const VkDescriptorBufferInfo& bufferInfo);

	void bind_dynamic_buffer(const char* name, uint32_t offset, const VkDescriptorBufferInfo& bufferInfo);

	void apply_binds(VkCommandBuffer cmd);

	//void build_sets(VkDevice device, VkDescriptorPool allocator);
	void build_sets(VkDevice device, vkutil::DescriptorAllocator& allocator);

	void set_shader(ShaderEffect* newShader);

	std::array<VkDescriptorSet, 4> cachedDescriptorSets;
private:
	struct DynOffsets {
		std::array<uint32_t, 16> offsets;
		uint32_t count{ 0 };
	};
	std::array<DynOffsets, 4> setOffsets;

	ShaderEffect* shaders{ nullptr };
	std::vector<BufferWriteDescriptor> bufferWrites;
};

class ShaderCache {

public:

	ShaderModule* get_shader(const std::string& path);

	void init(VkDevice device) { _device = device; };
private:
	VkDevice _device;
	std::unordered_map<std::string, std::unique_ptr<ShaderModule>> module_cache;
};