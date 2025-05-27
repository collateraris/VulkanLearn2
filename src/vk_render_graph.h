#pragma once

#include <vk_types.h>

#include <vk_render_pass.h>

class VulkanEngine;
class VulkanCommandBuffer;

namespace tf
{
	class Taskflow;
};

namespace vk_rgraph {

	class VulkanRenderGraph;
	class VulkanRenderPass;

	enum RenderGraphQueueFlagBits
	{
		RENDER_GRAPH_QUEUE_GRAPHICS_BIT = 1 << 0,
		RENDER_GRAPH_QUEUE_COMPUTE_BIT = 1 << 1,
	};
	using RenderGraphQueueFlags = uint32_t;

	static const RenderGraphQueueFlags compute_queues = RENDER_GRAPH_QUEUE_COMPUTE_BIT;

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
		uint32_t samples = 1;
		uint32_t levels = 1;
		uint32_t layers = 1;
		VkImageUsageFlags aux_usage = 0;
		AttachmentInfoFlags flags = ATTACHMENT_INFO_PERSISTENT_BIT;
	};

	struct BufferInfo
	{
		VkDeviceSize size = 0;
		VkBufferUsageFlags usage = 0;
		VmaAllocationCreateFlags vma_alloc_flags = 0;
		AttachmentInfoFlags flags = ATTACHMENT_INFO_PERSISTENT_BIT;

		bool operator==(const BufferInfo& other) const
		{
			return size == other.size &&
				usage == other.usage &&
				vma_alloc_flags == other.vma_alloc_flags &&
				flags == other.flags;
		}

		bool operator!=(const BufferInfo& other) const
		{
			return !(*this == other);
		}
	};

	struct ResourceDimensions
	{
		VkFormat format = VK_FORMAT_UNDEFINED;
		BufferInfo buffer_info = {};
		uint32_t layers = 1;
		uint32_t levels = 1;
		uint32_t samples = 1;
		VkExtent3D image_extend = {};
		AttachmentInfoFlags flags = ATTACHMENT_INFO_PERSISTENT_BIT;
		VkSurfaceTransformFlagBitsKHR transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		RenderGraphQueueFlags queues = 0;
		VkImageUsageFlags image_usage = 0;
		VkImageAspectFlags aspectFlags = 0;

		bool operator==(const ResourceDimensions& other) const;
		bool operator!=(const ResourceDimensions& other) const;
		bool uses_semaphore() const;
		bool is_storage_image() const;
		bool is_buffer_like() const;

		std::string name;
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

	class VulkanRenderBufferResource : public VulkanRenderResource
	{
	public:
		explicit VulkanRenderBufferResource(uint32_t index_, const std::string& name_)
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
		explicit VulkanRenderTextureResource(uint32_t index_, const std::string& name_)
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

		struct AccessedResource
		{
			VkPipelineStageFlags2 stages = 0;
			VkAccessFlags2 access = 0;
			VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		};

		struct AccessedTextureResource : AccessedResource
		{
			VulkanRenderTextureResource* texture = nullptr;
		};

		struct AccessedBufferResource : AccessedResource
		{
			VulkanRenderBufferResource* buffer = nullptr;
		};

		RenderGraphQueueFlagBits get_queue() const
		{
			return _queue;
		}

		VulkanRenderGraph& get_graph()
		{
			return *_graph;
		}

		uint32_t get_index() const
		{
			return _indexInGraph;
		}

		VulkanRenderTextureResource& set_depth_stencil_input(const std::string& name);
		VulkanRenderTextureResource& set_depth_stencil_output(const std::string& name, const AttachmentInfo& info);
		VulkanRenderTextureResource& add_color_output(const std::string& name, const AttachmentInfo& info, const std::string& input = "");
		VulkanRenderTextureResource& add_resolve_output(const std::string& name, const AttachmentInfo& info);
		VulkanRenderTextureResource& add_attachment_input(const std::string& name);
		VulkanRenderTextureResource& add_history_input(const std::string& name);

		VulkanRenderTextureResource& add_texture_input(const std::string& name,
			VkPipelineStageFlags2 stages = 0);
		VulkanRenderTextureResource& add_blit_texture_read_only_input(const std::string& name);
		VulkanRenderBufferResource& add_uniform_input(const std::string& name,
			VkPipelineStageFlags2 stages = 0);
		VulkanRenderBufferResource& add_storage_read_only_input(const std::string& name,
			VkPipelineStageFlags2 stages = 0);

		VulkanRenderBufferResource& add_storage_output(const std::string& name, const BufferInfo& info, const std::string& input = "");
		VulkanRenderBufferResource& add_transfer_output(const std::string& name, const BufferInfo& info);

		VulkanRenderTextureResource& add_storage_texture_output(const std::string& name, const AttachmentInfo& info, const std::string& input = "");
		VulkanRenderTextureResource& add_blit_texture_output(const std::string& name, const AttachmentInfo& info, const std::string& input = "");

		VulkanRenderBufferResource& add_vertex_buffer_input(const std::string& name);
		VulkanRenderBufferResource& add_index_buffer_input(const std::string& name);
		VulkanRenderBufferResource& add_indirect_buffer_input(const std::string& name);

		void make_color_input_scaled(uint32_t index_);
		const std::vector<VulkanRenderTextureResource*>& get_color_outputs() const;
		const std::vector<VulkanRenderTextureResource*>& get_resolve_outputs() const;
		const std::vector<VulkanRenderTextureResource*>& get_color_inputs() const;
		const std::vector<VulkanRenderTextureResource*>& get_color_scale_inputs() const;
		const std::vector<VulkanRenderTextureResource*>& get_storage_texture_outputs() const;
		const std::vector<VulkanRenderTextureResource*>& get_storage_texture_inputs() const;
		const std::vector<VulkanRenderTextureResource*>& get_blit_texture_inputs() const;
		const std::vector<VulkanRenderTextureResource*>& get_blit_texture_outputs() const;
		const std::vector<VulkanRenderTextureResource*>& get_attachment_inputs() const;
		const std::vector<VulkanRenderTextureResource*>& get_history_inputs() const;
		const std::vector<VulkanRenderBufferResource*>& get_storage_inputs() const;
		const std::vector<VulkanRenderBufferResource*>& get_storage_outputs() const;
		const std::vector<VulkanRenderBufferResource*>& get_transfer_outputs() const;
		const std::vector<AccessedTextureResource>& get_generic_texture_inputs() const;
		const std::vector<AccessedBufferResource>& get_generic_buffer_inputs() const;
		VulkanRenderTextureResource* get_depth_stencil_input() const;
		VulkanRenderTextureResource* get_depth_stencil_output() const;
		uint32_t get_physical_pass_index() const;
		void set_physical_pass_index(uint32_t index_);
		bool get_clear_color(uint32_t index_, VkClearColorValue* value = nullptr) const;
		bool get_clear_depth_stencil(VkClearDepthStencilValue* value = nullptr) const;
		void build_render_pass(VkCommandBuffer& cmd);
		void set_build_render_pass(std::function<void(VkCommandBuffer&)> func);
		void set_get_clear_depth_stencil(std::function<bool(VkClearDepthStencilValue*)> func);
		void set_get_clear_color(std::function<bool(uint32_t, VkClearColorValue*)> func);
		const std::string& get_name() const;

	private:
		VulkanRenderGraph* _graph;
		uint32_t _indexInGraph;
		std::optional<uint32_t> _physical_passIndex;
		RenderGraphQueueFlagBits _queue;
		const std::string& _name;

		std::function<void(VkCommandBuffer&)> build_render_pass_cb;
		std::function<bool(VkClearDepthStencilValue*)> get_clear_depth_stencil_cb;
		std::function<bool(uint32_t, VkClearColorValue*)> get_clear_color_cb;

		std::vector<VulkanRenderTextureResource*> _color_outputsList;
		std::vector<VulkanRenderTextureResource*> _resolve_outputsList;
		std::vector<VulkanRenderTextureResource*> _color_inputsList;
		std::vector<VulkanRenderTextureResource*> _color_scale_inputsList;
		std::vector<VulkanRenderTextureResource*> _storage_texture_inputsList;
		std::vector<VulkanRenderTextureResource*> _storage_texture_outputsList;
		std::vector<VulkanRenderTextureResource*> _blit_texture_inputsList;
		std::vector<VulkanRenderTextureResource*> _blit_texture_outputsList;
		std::vector<VulkanRenderTextureResource*> _attachments_inputsList;
		std::vector<VulkanRenderTextureResource*> _history_inputsList;
		std::vector<VulkanRenderBufferResource*> _storage_outputsList;
		std::vector<VulkanRenderBufferResource*> _storage_inputsList;
		std::vector<VulkanRenderBufferResource*> _transfer_outputsList;
		std::vector<AccessedTextureResource> _generic_textureList;
		std::vector<AccessedBufferResource> _generic_bufferList;
		VulkanRenderTextureResource* _depth_stencil_input = nullptr;
		VulkanRenderTextureResource* _depth_stencil_output = nullptr;

		std::vector<std::pair<VulkanRenderTextureResource*, VulkanRenderTextureResource*>> _fake_resource_alias;

		VulkanRenderBufferResource& add_generic_buffer_input(const std::string& name,
			VkPipelineStageFlags2 stages,
			VkAccessFlags2 access,
			VkBufferUsageFlags usage);
	};

	class VulkanRenderGraph
	{
	public:
		VulkanRenderGraph();

		void init(VulkanEngine* engine);

		VulkanRenderPass& add_pass(const std::string& name, RenderGraphQueueFlagBits queue);
		VulkanRenderPass* find_pass(const std::string& name);

		void set_backbuffer_source(const std::string& name);
		void set_backbuffer_dimensions(const ResourceDimensions& dim);
		const ResourceDimensions& get_backbuffer_dimensions() const;
		ResourceDimensions get_resource_dimensions(const VulkanRenderBufferResource& resource) const;
		ResourceDimensions get_resource_dimensions(const VulkanRenderTextureResource& resource) const;

		void bake();
		void reset();
		void enqueue_render_passes(tf::Taskflow& taskflow);

		VulkanRenderTextureResource& get_texture_resource(const std::string& name);
		VulkanRenderBufferResource& get_buffer_resource(const std::string& name);

	private:
		VulkanEngine* _engine = nullptr;

		std::vector<std::unique_ptr<VulkanRenderPass>> _passesList;
		std::unordered_map<std::string, uint32_t> _pass_to_indexMap;

		std::vector<std::unique_ptr<VulkanRenderResource>> _resourcesList;
		std::unordered_map<std::string, uint32_t> _resource_to_indexMap;

		std::vector<uint32_t> _pass_stack;

		std::string _backbuffer_source;

		struct Barrier
		{
			uint32_t resource_index;
			VkImageLayout layout;
			VkAccessFlags2 access;
			VkPipelineStageFlags2 stages;
			bool history;
		};

		struct Barriers
		{
			std::vector<Barrier> invalidate;
			std::vector<Barrier> flush;
		};

		std::vector<Barriers> _pass_barriersList;

		void filter_passes(std::vector<uint32_t>& list);
		void validate_passes();
		void build_barriers();

		ResourceDimensions _swapchain_dimensions;
		uint32_t _swapchain_physical_index = std::numeric_limits<uint32_t>::max();

		struct ColorClearRequest
		{
			VulkanRenderPass* pass;
			VkClearColorValue* target;
			uint32_t index;
		};

		struct DepthClearRequest
		{
			VulkanRenderPass* pass;
			VkClearDepthStencilValue* target;
		};

		struct ScaledClearRequests
		{
			uint32_t target;
			uint32_t physical_resource;
		};

		struct MipmapRequests
		{
			uint32_t physical_resource;
			VkPipelineStageFlags2 stages;
			VkAccessFlags2 access;
			VkImageLayout layout;
		};

		struct PhysicalPass
		{
			std::vector<uint32_t> passes;
			std::vector<uint32_t> discards;
			std::vector<Barrier> invalidate;
			std::vector<Barrier> flush;
			std::vector<Barrier> history;
			std::vector<std::pair<uint32_t, uint32_t>> alias_transfer;

			RenderPassInfo render_pass_info;
			std::vector<RenderPassInfo::Subpass> subpasses;
			std::vector<uint32_t> physical_color_attachments;
			std::optional<uint32_t> physical_depth_stencil_attachment;

			std::vector<ColorClearRequest> color_clear_requests;
			DepthClearRequest depth_clear_request;

			std::vector<std::vector<ScaledClearRequests>> scaled_clear_requests;
			std::vector<MipmapRequests> mipmap_requests;
			uint32_t layers = 1;
		};
		std::vector<PhysicalPass> _physical_passesList;
		std::vector<bool> _physical_image_has_historyList;

		void build_physical_passes();
		void build_transients();
		void build_physical_resources();
		void build_physical_barriers();
		void build_render_pass_info();


		std::vector<ResourceDimensions> _physical_dimensionsList;
		std::vector<VkImageView> _physical_attachmentsList;
		std::vector<AllocatedBuffer> _physical_buffersList;
		std::vector<Texture> _physical_image_attachmentsList;
		std::vector<Texture*> _physical_history_image_attachmentsList;

		void setup_physical_buffer(uint32_t attachment);
		void setup_physical_image(uint32_t attachment);

		void depend_passes_recursive(const VulkanRenderPass& pass, const std::unordered_set<uint32_t>& passes,
			uint32_t stack_count, bool no_check, bool ignore_self, bool merge_dependency);

		void traverse_dependencies(const VulkanRenderPass& pass, uint32_t stack_count);

		std::vector<std::unordered_set<uint32_t>> _pass_dependenciesList;
		std::vector<std::unordered_set<uint32_t>> _pass_merge_dependenciesList;
		bool depends_on_pass(uint32_t dst_pass, uint32_t src_pass);

		void reorder_passes(std::vector<uint32_t>& passes);

		struct PassSubmissionState
		{
			std::vector<VkBufferMemoryBarrier2> buffer_barriers;
			std::vector<VkImageMemoryBarrier2> image_barriers;

			std::vector<VkSubpassContents> subpass_contents;

			std::vector<VkSemaphore> wait_semaphores;
			std::vector<VkPipelineStageFlags2> wait_semaphore_stages;

			bool need_submission_semaphore = false;

			VulkanCommandBuffer* cmd = nullptr;

			bool graphics = false;
			bool active = false;

			void emit_pre_pass_barriers();
			void submit();
		};
		std::vector<PassSubmissionState> _pass_submission_stateList;

		void enqueue_render_pass(PhysicalPass& physical_pass, PassSubmissionState& state, tf::Taskflow& taskflow);
	};
}