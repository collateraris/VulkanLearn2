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
public:

	VulkanRaytracerDenoiserPass() = default;
	void init(VulkanEngine* engine);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

private:

	VulkanEngine* _engine = nullptr;

	nrd::Instance* m_Instance;
	nrd::Denoiser m_Denoiser;
	nrd::Identifier m_Identifier;

	AllocatedBuffer m_ConstantBuffer;

	std::vector<Texture> m_PermanentTextures;
	std::vector<Texture> m_TransientTextures;
};

#endif

