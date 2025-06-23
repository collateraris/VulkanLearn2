#pragma once

#include <vk_types.h>

#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>
#include <vk_descriptors.h>

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;

class VulkanPTRef
{
public:

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
		uint32_t mode = 0;
		uint32_t enableAccumulation = 0;
		uint32_t widthScreen = 0;
		uint32_t heightScreen = 0;
		float weightSum;
		uint32_t pad0;
		uint32_t pad1;
		uint32_t pad2;
	};

	VulkanPTRef() = default;
	void init(VulkanEngine* engine);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	void copy_global_uniform_data(VulkanPTRef::SGlobalRQParams& rqData, int current_frame_index);
	void copy_global_uniform_data(VulkanPTRef::GlobalGIParams& giData, int current_frame_index);

	void barrier_for_reading(VulkanCommandBuffer* cmd);
	void barrier_for_writing(VulkanCommandBuffer* cmd);

	Texture& get_tex(ETextureResourceNames name) const;

	void reset_accumulation();

private:

	void init_description_set_global_buffer();

	void init_tex();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	VkFormat      _colorFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };

	bool bResetAccumulation = false;

	AllocatedBuffer                 _rtSBTBuffer;

	std::array<AllocatedBuffer, 2> _globalUniformsBuffer;
	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;

	VkStridedDeviceAddressRegionKHR _rgenRegion{};
	VkStridedDeviceAddressRegionKHR _missRegion{};
	VkStridedDeviceAddressRegionKHR _hitRegion{};
	VkStridedDeviceAddressRegionKHR _callRegion{};

	vkutil::DescriptorManager _rpDescrMan; // render pass descriptor manager
};


