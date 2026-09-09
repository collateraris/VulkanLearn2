#pragma once 

#include <vk_types.h>


class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;
class PlayerCamera;

class VulkanSimpleAccumulationGraphicsPipeline
{
	struct PerFrameCB
	{
		uint32_t accumCount = 0;
		uint32_t initLastFrame = 0;
		uint32_t pad1 = 0;
		uint32_t pad2 = 0;
	};

public:
	void init(VulkanEngine* engine, const Texture& currentTex);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index, ERenderMode mode = ERenderMode::ReSTIR);
	void try_reset_accumulation(PlayerCamera& camera);
	void reset_accumulation();

	const Texture& get_output() const;
private:

	Texture& get_tex(ETextureResourceNames name) const;
	void init_render_pass();
	void init_description_set(const Texture& currentTex);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	Texture _outputTexture;
	// The mean is fed back every frame: FP16 repeatedly quantizes the history
	// and loses small sample contributions during long accumulation runs.
	VkFormat      _outputFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };
	Texture _lastFrameTexture;
	VkFormat      _lastFrameFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };
	bool _imagesInitialized = false;
	bool _accumulationEnabled = true;

	VkFramebuffer _simpleAccumFramebuffer;

	std::array<AllocatedBuffer, 2> _perFrameCount;

	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;

	VkDescriptorSetLayout          _imageDescSetLayout;
	std::array<VkDescriptorSet, 2>  _imageDescSet;

	PerFrameCB _counter{};

	glm::mat4 _lastViewMatrix{ 1.0f };
	glm::mat4 _lastProjectionMatrix{ 1.0f };
};

