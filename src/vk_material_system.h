#pragma once

#include <vk_types.h>
#include <vk_mesh.h>
#include <vk_shaders.h>

class PipelineBuilder {
public:

	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
	VertexInputDescription vertexDescription;
	VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkViewport _viewport;
	VkRect2D _scissor;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;

	VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
	void clear_vertex_input();
	void setShaders(ShaderEffect* effect);
};
class VulkanEngine;
namespace vkutil {

	struct ShaderPass {
		ShaderEffect* effect{ nullptr };
		VkPipeline pipeline{ VK_NULL_HANDLE };
		VkPipelineLayout layout{ VK_NULL_HANDLE };
	};

	struct SampledTexture {
		VkSampler sampler;
		VkImageView view;
	};

	template<typename T>
	struct PerPassData {

	public:
		T& operator[](MeshpassType pass)
		{
			switch (pass)
			{
			case MeshpassType::Forward:
				return data[0];
			case MeshpassType::Transparency:
				return data[1];
			case MeshpassType::DirectionalShadow:
				return data[2];
			}
			assert(false);
			return data[0];
		};

		void clear(T&& val)
		{
			for (int i = 0; i < 3; i++)
			{
				data[i] = val;
			}
		}

	private:
		std::array<T, 3> data;
	};


	struct EffectTemplate {
		PerPassData<ShaderPass*> passShaders;
	};

	struct MaterialData {
		std::vector<SampledTexture> textures;
		std::string baseTemplate;

		bool operator==(const MaterialData& other) const;

		size_t hash() const;
	};

	struct Material {
		EffectTemplate* original;
		PerPassData<VkDescriptorSet> passSets;

		std::vector<SampledTexture> textures;

		Material& operator=(const Material& other) = default;
	};

	class MaterialSystem {
	public:
		void init(VulkanEngine* owner);

		void build_default_templates();

		ShaderEffect* build_effect(VulkanEngine* eng, std::string_view vertexShader, std::string_view fragmentShader);
		ShaderPass* build_shader(VkRenderPass renderPass, PipelineBuilder& builder, ShaderEffect* effect);

		Material* build_material(const std::string& materialName, const MaterialData& info);
		Material* get_material(const std::string& materialName);

		void fill_builders();
	private:

		struct MaterialInfoHash
		{

			std::size_t operator()(const MaterialData& k) const
			{
				return k.hash();
			}
		};

		PipelineBuilder forwardBuilder;
		using vertexShaderName = std::string_view;
		using fragmentShaderName = std::string_view;
		std::unordered_map<vertexShaderName, std::unordered_map<fragmentShaderName, std::unique_ptr<ShaderEffect>>> shaderEffectCache;
		std::unordered_map<ShaderEffect*, std::unique_ptr<ShaderPass>> shaderPassCache;

		std::unordered_map<std::string, EffectTemplate> templateCache;
		std::unordered_map<std::string, Material*> materials;
		std::unordered_map<MaterialData, std::unique_ptr<Material>, MaterialInfoHash> materialCache;
		VulkanEngine* engine;
	};
}