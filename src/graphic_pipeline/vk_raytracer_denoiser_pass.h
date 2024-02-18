#pragma once

#include <vk_types.h>

#if GI_RAYTRACER_ON
#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>
#include <vk_descriptors.h>
#include <NRD.h>

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;

namespace NrdConfig {

	enum class DenoiserMethod : uint32_t
	{
		REBLUR,
		RELAX,
		MaxCount
	};

	nrd::RelaxDiffuseSpecularSettings getDefaultRELAXSettings();

	nrd::ReblurSettings getDefaultREBLURSettings();
}

class VulkanRaytracerDenoiserPass
{
	struct DescriptorInfo
	{
		VkDescriptorBufferInfo bufferInfo;
		VkDescriptorImageInfo imageInfo;
		uint32_t set;
		uint32_t binding;
	};

	struct PipelineDesc
	{
		std::string shaderName;
		EPipelineType pipType;
		std::vector<VkWriteDescriptorSet> writeDescSet;
		std::unordered_map<VkDescriptorType, std::vector<DescriptorInfo>> bindings = {};
	};

public:

	struct DenoiserParams
	{
		uint32_t pass = 0;
		float disocclusionThreshold = 0.01f;
		float disocclusionThresholdAlternate = 0.1f;
		bool useDisocclusionThresholdAlternateMix = true;
		float timeDeltaBetweenFrames = 0.f;
		bool enableValidation = true;
	};

	VulkanRaytracerDenoiserPass() = default;
	void init(VulkanEngine* engine);
	void set_params(DenoiserParams&& params);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

private:

	void init_denoiser_textures();

	VulkanEngine* _engine = nullptr;

	DenoiserParams _params;

	nrd::Instance* m_Instance;
	nrd::Denoiser m_Denoiser;
	nrd::Identifier m_Identifier;

	AllocatedBuffer m_ConstantBuffer;

	std::vector<Texture> m_PermanentTextures;
	std::vector<Texture> m_TransientTextures;
	std::vector<ESamplerType> m_Samplers;

	std::vector<PipelineDesc> _pipelineBindingLayouts;
};

#endif

