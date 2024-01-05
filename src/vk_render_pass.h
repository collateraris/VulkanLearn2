#pragma once

#include <vk_types.h>

class VulkanEngine;

enum class ERenderPassType
{
	NoInit = 0,
	Default = 1,
	VisBufferGenerate = 2,
	GBufferGenerate = 3,
	SimpleAccumulation = 4,
	DrawIntoCubemap = 5,
	Max,
};

enum RenderPassOp
{
	RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT = 1 << 0,
	RENDER_PASS_OP_LOAD_DEPTH_STENCIL_BIT = 1 << 1,
	RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT = 1 << 2,
	RENDER_PASS_OP_DEPTH_STENCIL_READ_ONLY_BIT = 1 << 3,
	RENDER_PASS_OP_ENABLE_TRANSIENT_STORE_BIT = 1 << 4,
	RENDER_PASS_OP_ENABLE_TRANSIENT_LOAD_BIT = 1 << 5
};
using RenderPassOpFlags = uint32_t;

struct RenderPassInfo
{
	std::array<Texture*, VULKAN_NUM_ATTACHMENTS> color_attachments;
	Texture* depth_stencil = nullptr;
	unsigned num_color_attachments = 0;
	RenderPassOpFlags op_flags = 0;
	uint32_t clear_attachments = 0;
	uint32_t load_attachments = 0;
	uint32_t store_attachments = 0;
	uint32_t base_layer = 0;
	uint32_t num_layers = 1;

	// Render area will be clipped to the actual framebuffer.
	VkRect2D render_area = { { 0, 0 }, { UINT32_MAX, UINT32_MAX } };

	std::array<VkClearColorValue, VULKAN_NUM_ATTACHMENTS> clear_color;
	VkClearDepthStencilValue clear_depth_stencil = { 1.0f, 0 };

	enum class DepthStencil
	{
		None,
		ReadOnly,
		ReadWrite
	};

	struct Subpass
	{
		std::array<uint32_t, VULKAN_NUM_ATTACHMENTS> color_attachments;
		std::array<uint32_t, VULKAN_NUM_ATTACHMENTS> input_attachments;
		std::array<uint32_t, VULKAN_NUM_ATTACHMENTS> resolve_attachments;
		unsigned num_color_attachments = 0;
		unsigned num_input_attachments = 0;
		unsigned num_resolve_attachments = 0;
		DepthStencil depth_stencil_mode = DepthStencil::ReadWrite;
	};
	// If 0/nullptr, assume a default subpass.
	const Subpass* subpasses = nullptr;
	unsigned num_subpasses = 0;

	static void compute_dimensions(const RenderPassInfo& info, uint32_t& width, uint32_t& height);
	static void compute_attachment_dimensions(const RenderPassInfo& info, unsigned index, uint32_t& width, uint32_t& height);
	static std::vector<VkImageView> setup_raw_views(const RenderPassInfo& info);
};

class VulkanRenderPass
{
public:
	struct SubpassInfo
	{
		std::array<VkAttachmentReference, VULKAN_NUM_ATTACHMENTS> color_attachments;
		uint32_t num_color_attachments;
		std::array<VkAttachmentReference, VULKAN_NUM_ATTACHMENTS> input_attachments;
		uint32_t num_input_attachments;
		VkAttachmentReference depth_stencil_attachment;

		uint32_t samples;
	};

	VulkanRenderPass() = default;
	~VulkanRenderPass() = default;

	void init(VulkanEngine* engine, const VkRenderPassCreateInfo& create_info);
	void init(VulkanEngine* engine, const RenderPassInfo& create_info);

	uint32_t get_num_subpasses() const;
	VkRenderPass get_render_pass() const;
	uint32_t get_sample_count(uint32_t subpass) const;
	uint32_t get_num_color_attachments(uint32_t subpass) const;
	uint32_t get_num_input_attachments(uint32_t subpass) const;
	const VkAttachmentReference& get_color_attachment(uint32_t subpass, uint32_t index) const;
	const VkAttachmentReference& get_input_attachment(uint32_t subpass, uint32_t index) const;
	bool has_depth(uint32_t subpass) const;
	bool has_stencil(uint32_t subpass) const;

	const VkRenderPassCreateInfo& get_create_info() const;

private:
	VulkanEngine* _engine;
	VkRenderPass render_pass = VK_NULL_HANDLE;

	std::array<VkFormat, VULKAN_NUM_ATTACHMENTS> color_attachments = {};
	VkFormat depth_stencil = VK_FORMAT_UNDEFINED;
	std::vector<SubpassInfo> subpasses_info;

	VkRenderPassCreateInfo create_info;

	void setup_subpasses(const VkRenderPassCreateInfo& create_info);
};

class VulkanRenderPassManager
{
public:
	void init(VulkanEngine* _engine);

	VulkanRenderPass* get_render_pass(ERenderPassType type) const;
private:
	std::vector<std::unique_ptr<VulkanRenderPass>> _renderPassesList;
};