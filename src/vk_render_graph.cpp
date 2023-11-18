#include <vk_render_graph.h>

vk_rgraph::VulkanRenderGraph::VulkanRenderGraph()
{

}

void vk_rgraph::VulkanRenderGraph::init(VulkanEngine* engine)
{
	_engine = engine;
}

vk_rgraph::VulkanRenderPass& vk_rgraph::VulkanRenderGraph::add_pass(const std::string& name, RenderGraphQueueFlagBits queue)
{
	auto itr = pass_to_index.find(name);
	if (itr != end(pass_to_index))
	{
		return *passes[itr->second];
	}
	else
	{
		uint32_t index = passes.size();
		passes.emplace_back(std::make_unique<VulkanRenderPass>(this, index, queue, name));
		pass_to_index[name] = index;
		return *passes.back();
	}
}

vk_rgraph::VulkanRenderPass* vk_rgraph::VulkanRenderGraph::find_pass(const std::string& name)
{
	auto itr = pass_to_index.find(name);
	if (itr != end(pass_to_index))
		return passes[itr->second].get();
	else
		return nullptr;
}

vk_rgraph::VulkanRenderPass::VulkanRenderPass(VulkanRenderGraph* graph_, unsigned int index_, RenderGraphQueueFlagBits queue_, const std::string& name_)
	: _graph{graph_}
	, _indexInGraph{index_}
	, _queue{queue_}
	, _name{name_}
{
}

vk_rgraph::VulkanRenderResource::Type vk_rgraph::VulkanRenderResource::get_type() const
{
	return _resource_type;
}

void vk_rgraph::VulkanRenderResource::written_in_pass(uint32_t index_)
{
	_written_in_passes.insert(index_);
}

void vk_rgraph::VulkanRenderResource::read_in_pass(uint32_t index_)
{
	_read_in_passes.insert(index_);
}

const std::unordered_set<uint32_t>& vk_rgraph::VulkanRenderResource::get_read_passes() const
{
	return _read_in_passes;
}

const std::unordered_set<uint32_t>& vk_rgraph::VulkanRenderResource::get_write_passes() const
{
	return _written_in_passes;
}

std::unordered_set<uint32_t>& vk_rgraph::VulkanRenderResource::get_read_passes()
{
	return _read_in_passes;
}

std::unordered_set<uint32_t>& vk_rgraph::VulkanRenderResource::get_write_passes()
{
	return _written_in_passes;
}

uint32_t vk_rgraph::VulkanRenderResource::get_render_graph_index() const
{
	return _index;
}

void vk_rgraph::VulkanRenderResource::set_physical_index(uint32_t index_)
{
	_physical_index = index_;
}

uint32_t vk_rgraph::VulkanRenderResource::get_physical_index() const
{
	assert(_physical_index.has_value());
	return _physical_index.value_or(0);
}

const std::string& vk_rgraph::VulkanRenderResource::get_name() const
{
	return _name;
}

void vk_rgraph::VulkanRenderResource::add_queue(vk_rgraph::RenderGraphQueueFlagBits queue)
{
	_used_queues |= queue;
}

vk_rgraph::RenderGraphQueueFlags vk_rgraph::VulkanRenderResource::get_used_queues() const
{
	return _used_queues;
}

void vk_rgraph::VulkanRenderBufferResource::set_buffer_info(const vk_rgraph::BufferInfo& info_)
{
	_buffer_info = info_;
}

const vk_rgraph::BufferInfo& vk_rgraph::VulkanRenderBufferResource::get_buffer_info() const
{
	return _buffer_info;
}

void vk_rgraph::VulkanRenderBufferResource::add_buffer_usage(VkBufferUsageFlags flags)
{
	_buffer_usage |= flags;
}

VkBufferUsageFlags vk_rgraph::VulkanRenderBufferResource::get_buffer_usage() const
{
	return _buffer_usage;
}

void vk_rgraph::VulkanRenderTextureResource::set_attachment_info(const AttachmentInfo& info_)
{
	_attach_info = info_;
}

const vk_rgraph::AttachmentInfo& vk_rgraph::VulkanRenderTextureResource::get_attachment_info() const
{
	return _attach_info;
}

vk_rgraph::AttachmentInfo& vk_rgraph::VulkanRenderTextureResource::get_attachment_info()
{
	return _attach_info;
}

void vk_rgraph::VulkanRenderTextureResource::set_transient_state(bool enable)
{
	_transient = enable;
}

bool vk_rgraph::VulkanRenderTextureResource::get_transient_state() const
{
	return _transient;
}

void vk_rgraph::VulkanRenderTextureResource::add_image_usage(VkImageUsageFlags flags)
{
	_image_usage |= flags;
}

VkImageUsageFlags vk_rgraph::VulkanRenderTextureResource::get_image_usage() const
{
	return _image_usage;
}


