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

class VulkanNRC_InferencePass
{
public:

	VulkanNRC_InferencePass() = default;
	void init(VulkanEngine* engine);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	const Texture& get_output() const;

	void barrier_for_frag_read(VulkanCommandBuffer* cmd);
	void barrier_for_compute_write(VulkanCommandBuffer* cmd);

private:

	Texture& get_tex(ETextureResourceNames name) const;

	void init_description_set_global_buffer();

	VulkanEngine* _engine = nullptr;

	Texture _outputTex;

	VkExtent3D _imageExtent;
	VkFormat      _colorFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };
	const uint32_t _tileSize = 16;
	uint32_t _tileNumberWidth;
	uint32_t _tileNumberHeight;

	vkutil::DescriptorManager _rpDescrMan; // render pass descriptor manager
};


