#pragma once

#include <vk_types.h>

#if GI_RAYTRACER_ON && GBUFFER_ON
#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;

class VulkanReSTIRUpdateReservoirPlusShadePass
{
public:

	VulkanReSTIRUpdateReservoirPlusShadePass() = default;
	void init(VulkanEngine* engine, VkAccelerationStructureKHR  tlas, 
		std::array<AllocatedBuffer, 2>& globalUniformsBuffer, AllocatedBuffer& objectBuffer, 
		const Texture& indirectInput, const Texture& reservoirPrev, const Texture& reservoirSpatial);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	const Texture& get_output() const;

	void barrier_for_frag_read(VulkanCommandBuffer* cmd);
	void barrier_for_raytrace_write(VulkanCommandBuffer* cmd);

private:
	void init_description_set_global_buffer(std::array<AllocatedBuffer, 2>& globalUniformsBuffer, AllocatedBuffer& objectBuffer, const Texture& indirectInput, const Texture& reservoirPrev, const Texture& reservoirSpatial);
	void init_description_set();
	void init_bindless(const std::vector<std::unique_ptr<Mesh>>& meshList, const std::vector<Texture*>& textureList, VkAccelerationStructureKHR  tlas);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	Texture _outputTex;
	VkFormat      _colorFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };

	AllocatedBuffer                 _rtSBTBuffer;

	VkStridedDeviceAddressRegionKHR _rgenRegion{};
	VkStridedDeviceAddressRegionKHR _missRegion{};
	VkStridedDeviceAddressRegionKHR _hitRegion{};
	VkStridedDeviceAddressRegionKHR _callRegion{};

	VkDescriptorSetLayout          _rtDescSetLayout;
	std::array<VkDescriptorSet, 2>  _rtDescSet;

	VkDescriptorSetLayout          _globalUniformsDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalUniformsDescSet;

	VkDescriptorSetLayout _gBuffDescSetLayout;
	std::array<VkDescriptorSet, 2> _gBuffDescSet;

	VkDescriptorSet _bindlessSet;
	VkDescriptorSetLayout _bindlessSetLayout;
};

#endif

