#pragma once 

#include <vk_types.h>

#if AO_RAYTRACER_ON && GBUFFER_ON

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;
class PlayerCamera;

class VulkanSimpleAccumulationGraphicsPipeline
{
	struct PerFrameCB
	{
		uint32_t accumCount;
	};

public:
	void init(VulkanEngine* engine, const Texture& currentTex);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);
	void try_reset_accumulation(PlayerCamera& camera);
	void reset_accumulation();

	const Texture& get_output() const;
private:

	void init_render_pass();
	void init_description_set(const Texture& currentTex);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	Texture _outputTexture;
	VkFormat      _outputFormat{ VK_FORMAT_R32_SFLOAT };
	Texture _lastFrameTexture;
	VkFormat      _lastFrameFormat{ VK_FORMAT_R32_SFLOAT };
	Texture _depthTexture;
	VkFormat      _depthFormat{ VK_FORMAT_D32_SFLOAT };

	VkFramebuffer _simpleAccumFramebuffer;

	std::array<AllocatedBuffer, 2> _perFrameCount;

	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;

	VkDescriptorSetLayout          _imageDescSetLayout;
	std::array<VkDescriptorSet, 2>  _imageDescSet;

	PerFrameCB _counter = { 0 };

	glm::mat4 _lastViewMatrix;
};
#endif
