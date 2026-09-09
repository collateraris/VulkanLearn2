#pragma once

#include <vk_types.h>
#include <rhi/rhi.h>

#include <vk_mesh.h>

class VulkanEngine;
class VulkanCommandBuffer;
class RenderObject;

class VulkanGbufferShadingGraphicsPipeline
{
public:

	void init(VulkanEngine* engine, const Texture& gi, const Texture* denoised = nullptr, const Texture* upscaled = nullptr);
	void draw(rhi::CommandList& cmd, int current_frame_index);

private:
	void init_description_set(const Texture& gi, const Texture* denoised, const Texture* upscaled);

	VulkanEngine* _engine;

	VkDescriptorSetLayout          _gBufDescSetLayout;
	std::array<VkDescriptorSet, 2>  _gBufDescSet;
	std::array<VkDescriptorSet, 2>  _denoisedDescSet{};
	bool _hasDenoisedInput = false;
	std::array<VkDescriptorSet, 2> _upscaledDescSet{};
	bool _hasUpscaledInput = false;
};
