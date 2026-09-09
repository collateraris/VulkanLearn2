#pragma once

#include <vk_types.h>
#include <rhi/rhi.h>

#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>
#include <vk_descriptors.h>

class VulkanEngine;
class VulkanFrameBuffer;
class RenderObject;

class VulkanReSTIRUpdateReservoirPlusShadePass
{
public:

	VulkanReSTIRUpdateReservoirPlusShadePass() = default;
	void init(VulkanEngine* engine);
	void draw(rhi::CommandList& cmd, int frameSlot);

	const Texture& get_output() const;
	const Texture& get_diffuse_output() const { return _diffuseOutput; }
	const Texture& get_specular_output() const { return _specularOutput; }
	const Texture& get_bypass_output() const { return _bypassOutput; }


private:
	void init_description_set_global_buffer();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	Texture _outputTex;
	Texture _diffuseOutput;
	Texture _specularOutput;
	Texture _bypassOutput;
	VkFormat      _colorFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };

	const uint32_t _tileSize = 16;
	uint32_t _tileNumberWidth;
	uint32_t _tileNumberHeight;

	vkutil::DescriptorManager _rpDescrMan; // render pass descriptor manager
};


