#pragma once

#include <vk_types.h>

class VulkanEngine;
class VulkanRenderPass
{
public:
	struct SubpassInfo
	{
		VkAttachmentReference color_attachments[VULKAN_NUM_ATTACHMENTS];
		uint32_t num_color_attachments;
		VkAttachmentReference input_attachments[VULKAN_NUM_ATTACHMENTS];
		uint32_t num_input_attachments;
		VkAttachmentReference depth_stencil_attachment;

		uint32_t samples;
	};

	VulkanRenderPass() = default;
	~VulkanRenderPass() = default;

	void init(VulkanEngine* engine, const VkRenderPassCreateInfo& create_info);

	uint32_t get_num_subpasses() const;
	VkRenderPass get_render_pass() const;
	uint32_t get_sample_count(uint32_t subpass) const;
	uint32_t get_num_color_attachments(uint32_t subpass) const;
	uint32_t get_num_input_attachments(uint32_t subpass) const;
	const VkAttachmentReference& get_color_attachment(uint32_t subpass, uint32_t index) const;
	const VkAttachmentReference& get_input_attachment(uint32_t subpass, uint32_t index) const;
	bool has_depth(uint32_t subpass) const;
	bool has_stencil(uint32_t subpass) const;

private:
	VulkanEngine* _engine;
	VkRenderPass render_pass = VK_NULL_HANDLE;

	VkFormat color_attachments[VULKAN_NUM_ATTACHMENTS] = {};
	VkFormat depth_stencil = VK_FORMAT_UNDEFINED;
	std::vector<SubpassInfo> subpasses_info;

	void setup_subpasses(const VkRenderPassCreateInfo& create_info);
};