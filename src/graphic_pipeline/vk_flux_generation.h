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

class VulkanFluxGeneration
{
	struct SFluxData
	{
		uint32_t lightCount;
		uint32_t pad0;
		uint32_t pad1;
		uint32_t pad2;
	};

public:

	VulkanFluxGeneration() = default;
	void init(VulkanEngine* engine);
	void draw(VkCommandBuffer cmd, uint32_t lights_num);

private:

	void init_description_set_global_buffer();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;

	AllocatedBuffer _fluxData;
};

