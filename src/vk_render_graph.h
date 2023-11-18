#pragma once

#include <vk_types.h>

class VulkanEngine;

namespace vk_rgraph {

	class VulkanRenderGraph;
	class VulkanRenderPass;

	enum RenderGraphQueueFlagBits
	{
		RENDER_GRAPH_QUEUE_GRAPHICS_BIT = 1 << 0,
		RENDER_GRAPH_QUEUE_COMPUTE_BIT = 1 << 1,
	};
	using RenderGraphQueueFlags = uint32_t;

	enum AttachmentInfoFlagBits
	{
		ATTACHMENT_INFO_PERSISTENT_BIT = 1 << 0,
		ATTACHMENT_INFO_UNORM_SRGB_ALIAS_BIT = 1 << 1,
		ATTACHMENT_INFO_SUPPORTS_PREROTATE_BIT = 1 << 2,
		ATTACHMENT_INFO_MIPGEN_BIT = 1 << 3
	};

	enum AttachmentInfoInternalFlagBits
	{
		ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT = 1 << 16,
		ATTACHMENT_INFO_INTERNAL_PROXY_BIT = 1 << 17
	};
	using AttachmentInfoFlags = uint32_t;

	enum SizeClass
	{
		Absolute,
		SwapchainRelative,
		InputRelative
	};

	struct AttachmentInfo
	{
		SizeClass size_class = SizeClass::SwapchainRelative;
		float size_x = 1.0f;
		float size_y = 1.0f;
		float size_z = 0.0f;
		VkFormat format = VK_FORMAT_UNDEFINED;
		std::string size_relative_name;
		unsigned samples = 1;
		unsigned levels = 1;
		unsigned layers = 1;
		VkImageUsageFlags aux_usage = 0;
		AttachmentInfoFlags flags = ATTACHMENT_INFO_PERSISTENT_BIT;
	};

	class VulkanRenderResource
	{
	public:
		enum class Type
		{
			Buffer,
			Texture,
		};

		VulkanRenderResource(Type type_, uint32_t index_, const std::string& name_)
			: _resource_type(type_), _index(index_), _name(name_)
		{
		}

		virtual ~VulkanRenderResource() = default;

		Type get_type() const;

		void written_in_pass(uint32_t index_);

		void read_in_pass(uint32_t index_);

		const std::unordered_set<uint32_t>& get_read_passes() const;

		const std::unordered_set<uint32_t>& get_write_passes() const;

		std::unordered_set<uint32_t>& get_read_passes();

		std::unordered_set<uint32_t>& get_write_passes();

		uint32_t get_render_graph_index() const;

		void set_physical_index(uint32_t index_);

		uint32_t get_physical_index() const;

		const std::string& get_name() const;

		void add_queue(RenderGraphQueueFlagBits queue);

		RenderGraphQueueFlags get_used_queues() const;

	private:
		Type _resource_type;
		uint32_t _index;
		std::optional<uint32_t> _physical_index;
		std::unordered_set<uint32_t> _written_in_passes;
		std::unordered_set<uint32_t> _read_in_passes;
		std::string _name;
		RenderGraphQueueFlags _used_queues = 0;
	};

	struct BufferInfo
	{
		VkDeviceSize size = 0;
		VkBufferUsageFlags usage = 0;
		AttachmentInfoFlags flags = ATTACHMENT_INFO_PERSISTENT_BIT;

		bool operator==(const BufferInfo& other) const
		{
			return size == other.size &&
				usage == other.usage &&
				flags == other.flags;
		}

		bool operator!=(const BufferInfo& other) const
		{
			return !(*this == other);
		}
	};

	class VulkanRenderBufferResource : public VulkanRenderResource
	{
	public:
		explicit VulkanRenderBufferResource(uint32_t index_, std::string& name_)
			: VulkanRenderResource(VulkanRenderResource::Type::Buffer, index_, name_)
		{
		}

		void set_buffer_info(const BufferInfo& info_);

		const BufferInfo& get_buffer_info() const;

		void add_buffer_usage(VkBufferUsageFlags flags);

		VkBufferUsageFlags get_buffer_usage() const;

	private:
		BufferInfo _buffer_info;
		VkBufferUsageFlags _buffer_usage = 0;
	};

	class VulkanRenderTextureResource : public VulkanRenderResource
	{
	public:
		explicit VulkanRenderTextureResource(uint32_t index_, std::string& name_)
			: VulkanRenderResource(VulkanRenderResource::Type::Texture, index_, name_)
		{
		}

		void set_attachment_info(const AttachmentInfo& info_);

		const AttachmentInfo& get_attachment_info() const;

		AttachmentInfo& get_attachment_info();

		void set_transient_state(bool enable);

		bool get_transient_state() const;

		void add_image_usage(VkImageUsageFlags flags);

		VkImageUsageFlags get_image_usage() const;

	private:
		AttachmentInfo _attach_info;
		VkImageUsageFlags _image_usage = 0;
		bool _transient = false;
	};

	class VulkanRenderPass
	{
	public:
		VulkanRenderPass(VulkanRenderGraph* graph_, uint32_t index_, RenderGraphQueueFlagBits queue_, const std::string& name);


	private:
		VulkanRenderGraph* _graph;
		uint32_t _indexInGraph;
		RenderGraphQueueFlagBits _queue;
		const std::string& _name;
	};

	class VulkanRenderGraph
	{
	public:
		VulkanRenderGraph();

		void init(VulkanEngine* engine);

		VulkanRenderPass& add_pass(const std::string& name, RenderGraphQueueFlagBits queue);
		VulkanRenderPass* find_pass(const std::string& name);

	private:
		VulkanEngine* _engine;

		std::vector<std::unique_ptr<VulkanRenderPass>> passes;
		std::unordered_map<std::string, uint32_t> pass_to_index;
	};
}