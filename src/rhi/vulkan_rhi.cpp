#include "vulkan_rhi.h"
#include "vulkan_resources.h"
#include <vk_utils.h>
#include <vk_textures.h>
#include <imgui_impl_vulkan.h>
#include <array>
#include <stdexcept>

namespace rhi {
namespace {
template<class T> uint64_t handle(T value) { return reinterpret_cast<uint64_t>(value); }
template<class T> T native(uint64_t value) { return reinterpret_cast<T>(value); }
VkPipelineStageFlags stages(Stage s) {
    VkPipelineStageFlags out = 0;
    auto add = [&](Stage bit, VkPipelineStageFlags vk) { if ((s & bit) != Stage::None) out |= vk; };
    add(Stage::Top, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    add(Stage::RayTracing, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
    add(Stage::Compute, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    add(Stage::Vertex, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
    add(Stage::Fragment, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    add(Stage::ColorOutput, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    add(Stage::Depth, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    add(Stage::Transfer, VK_PIPELINE_STAGE_TRANSFER_BIT);
    add(Stage::Host, VK_PIPELINE_STAGE_HOST_BIT);
    add(Stage::Bottom, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    add(Stage::AllCommands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    add(Stage::Indirect, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
    return out ? out : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
}
VkAccessFlags access(Access a) {
    VkAccessFlags out = 0;
    auto add = [&](Access bit, VkAccessFlags vk) { if ((a & bit) != Access::None) out |= vk; };
    add(Access::ShaderRead, VK_ACCESS_SHADER_READ_BIT); add(Access::ShaderWrite, VK_ACCESS_SHADER_WRITE_BIT);
    add(Access::UniformRead, VK_ACCESS_UNIFORM_READ_BIT); add(Access::ColorRead, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    add(Access::ColorWrite, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT); add(Access::DepthRead, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    add(Access::DepthWrite, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT); add(Access::TransferRead, VK_ACCESS_TRANSFER_READ_BIT);
    add(Access::TransferWrite, VK_ACCESS_TRANSFER_WRITE_BIT); add(Access::HostRead, VK_ACCESS_HOST_READ_BIT);
    add(Access::HostWrite, VK_ACCESS_HOST_WRITE_BIT); add(Access::VertexRead, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
    add(Access::IndexRead, VK_ACCESS_INDEX_READ_BIT); add(Access::IndirectRead, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
    add(Access::MemoryRead, VK_ACCESS_MEMORY_READ_BIT); add(Access::MemoryWrite, VK_ACCESS_MEMORY_WRITE_BIT);
    return out;
}
VkImageLayout layout(Layout l) {
    switch (l) {
    case Layout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
    case Layout::General: return VK_IMAGE_LAYOUT_GENERAL;
    case Layout::ShaderReadOnly: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case Layout::ColorAttachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case Layout::DepthAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case Layout::TransferSource: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case Layout::TransferDestination: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case Layout::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    throw std::invalid_argument("RHI: unsupported image layout");
}
Layout layout(VkImageLayout l) {
    switch (l) {
    case VK_IMAGE_LAYOUT_UNDEFINED: return Layout::Undefined;
    case VK_IMAGE_LAYOUT_GENERAL: return Layout::General;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return Layout::ShaderReadOnly;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return Layout::ColorAttachment;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return Layout::DepthAttachment;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return Layout::TransferSource;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return Layout::TransferDestination;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return Layout::Present;
    default: throw std::invalid_argument("RHI: unsupported imported image layout");
    }
}
}
VulkanDevice::VulkanDevice() : _commands(*this) {}
VulkanDevice::~VulkanDevice() = default;
void VulkanDevice::init_resources(VmaAllocator allocator) {
    _resourceDevice = std::make_unique<VulkanResourceDevice>();
    _resourceDevice->init(*this, _device, allocator);
}
ResourceDevice& VulkanDevice::resources() { return *_resourceDevice; }
VulkanResourceDevice& VulkanDevice::vulkan_resources() { return *_resourceDevice; }
void VulkanDevice::init(VkDevice device, bool debugUtilsEnabled) {
    _device = device;
    // A non-null proc address does not imply that its extension was enabled.
    _beginLabel = debugUtilsEnabled ? reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT")) : nullptr;
    _endLabel = debugUtilsEnabled ? reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT")) : nullptr;
}
CommandList& VulkanDevice::begin_commands(VkCommandBuffer cmd) { _commands._cmd = cmd; return _commands; }
Resource VulkanDevice::image(const Texture& tex, std::optional<ResourceState> initial) {
    ResourceState state = initial.value_or(ResourceState{Stage::AllCommands,
        Access::MemoryRead | Access::MemoryWrite, layout(tex.currImageLayout)});
    if (state.layout == Layout::Undefined) state = {};
    return image(tex.image._image, vkutil::format_to_aspect_mask(tex.createInfo.format),
        tex.createInfo.mipLevels, tex.createInfo.arrayLayers, state);
}
Resource VulkanDevice::image(VkImage image, VkImageAspectFlags aspect, uint32_t levels, uint32_t layers, ResourceState initial) {
    if (!image) throw std::invalid_argument("RHI: null image import");
    if (auto it = _images.find(image); it != _images.end()) return it->second;
    Resource id{_nextResource++, ResourceKind::Image};
    ResourceRecord record{}; record.image = image; record.range = {aspect, 0, levels, 0, layers}; record.state = initial;
    if (writes(initial.access)) record.lastWriter = initial;
    _resources.emplace(id.id, record); _images.emplace(image, id); return id;
}
Resource VulkanDevice::buffer(const AllocatedBuffer& buffer) {
    if (!buffer._buffer) throw std::invalid_argument("RHI: null buffer import");
    if (auto it = _buffers.find(buffer._buffer); it != _buffers.end()) return it->second;
    Resource id{_nextResource++, ResourceKind::Buffer};
    ResourceRecord record{}; record.buffer = buffer._buffer;
    record.state = {Stage::AllCommands | Stage::Host, Access::MemoryRead | Access::MemoryWrite | Access::HostWrite, Layout::General};
    record.lastWriter = record.state;
    _resources.emplace(id.id, record); _buffers.emplace(buffer._buffer, id); return id;
}
Pipeline VulkanDevice::pipeline(VkPipeline pipeline, VkPipelineLayout layout, VkPipelineBindPoint point) {
    if (!pipeline || !layout) throw std::invalid_argument("RHI: null pipeline import");
    auto id = handle(pipeline); _pipelines.insert_or_assign(id, PipelineRecord{pipeline, layout, point}); return {id};
}
DescriptorSet VulkanDevice::descriptor(VkDescriptorSet set) const { return {handle(set)}; }
QueryPool VulkanDevice::query_pool(VkQueryPool pool) const { return {handle(pool)}; }
RenderTarget VulkanDevice::render_target(VkRenderPass pass, VkFramebuffer fb, uint32_t w, uint32_t h, bool depth) {
    auto id = handle(fb); _targets.insert_or_assign(id, TargetRecord{pass, fb, w, h, depth}); return {id};
}
ShaderBindingTable VulkanDevice::shader_table(const VkStridedDeviceAddressRegionKHR& r, const VkStridedDeviceAddressRegionKHR& m,
    const VkStridedDeviceAddressRegionKHR& h, const VkStridedDeviceAddressRegionKHR& c) {
    auto convert = [](const auto& v) { return ShaderTableRegion{v.deviceAddress, v.stride, v.size}; };
    return {convert(r), convert(m), convert(h), convert(c)};
}
VulkanDevice::ResourceRecord& VulkanDevice::resource(Resource r) {
    auto& record = _resources.at(r.id);
    if ((r.kind == ResourceKind::Image) != bool(record.image)) throw std::invalid_argument("RHI: resource kind mismatch");
    return record;
}
void VulkanDevice::set_external_state(Resource r, ResourceState state) {
    auto& record = resource(r);
    record.state = state;
    record.lastWriter = writes(state.access) ? state : ResourceState{Stage::None, Access::None, Layout::Undefined};
}
void VulkanDevice::forget(Resource r) {
    auto& record = resource(r);
    if (record.image) _images.erase(record.image); else _buffers.erase(record.buffer);
    _resources.erase(r.id);
}
void VulkanDevice::reset() { _resources.clear(); _images.clear(); _buffers.clear(); _pipelines.clear(); _targets.clear(); }
void VulkanDevice::draw_imgui(ImDrawData* data) { ImGui_ImplVulkan_RenderDrawData(data, _commands._cmd); }

void VulkanCommandList::transition(Resource resource, ResourceState next) {
    auto& rec = _device.resource(resource);
    auto prev = rec.state;
    // Keep all outstanding readers for WAR. Retain the producer separately to
    // make it visible when a later reader uses another stage. Immutable scene
    // assets then require no repeated barriers in every pass/frame. Equal
    // layouts alone do NOT eliminate RAW/WAW dependencies.
    const bool image = resource.kind == ResourceKind::Image;
    const bool change = image && prev.layout != next.layout;
    const bool hazard = writes(prev.access) || writes(next.access);
    const bool newReader = (prev.stages & next.stages) != next.stages || (prev.access & next.access) != next.access;
    if (change || hazard || newReader) {
        ResourceState source = prev;
        source.stages = source.stages | rec.lastWriter.stages;
        source.access = source.access | rec.lastWriter.access;
        if (image) {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.srcAccessMask = prev.layout == Layout::Undefined ? 0 : access(source.access);
            barrier.dstAccessMask = access(next.access);
            barrier.oldLayout = layout(prev.layout); barrier.newLayout = layout(next.layout);
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = rec.image; barrier.subresourceRange = rec.range;
            // Acquired swapchain images use ColorOutput as their source scope,
            // including first use from UNDEFINED, to chain the acquire wait.
            vkCmdPipelineBarrier(_cmd, stages(source.stages),
                stages(next.stages), 0, 0, nullptr, 0, nullptr, 1, &barrier);
        } else {
            VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = access(source.access); barrier.dstAccessMask = access(next.access);
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = rec.buffer; barrier.offset = 0; barrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(_cmd, stages(source.stages), stages(next.stages), 0, 0, nullptr, 1, &barrier, 0, nullptr);
        }
    }
    rec.state = next;
    if (writes(next.access)) rec.lastWriter = next;
    else if (!writes(prev.access) && !change) {
        rec.state.stages = prev.stages | next.stages;
        rec.state.access = prev.access | next.access;
    }
}
void VulkanCommandList::begin_label(const char* name) {
    if (_device._beginLabel) { VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT}; label.pLabelName = name; _device._beginLabel(_cmd, &label); }
}
void VulkanCommandList::end_label() { if (_device._endLabel) _device._endLabel(_cmd); }
void VulkanCommandList::bind_pipeline(Pipeline p) { const auto& v = _device._pipelines.at(p.id); vkCmdBindPipeline(_cmd, v.bindPoint, v.pipeline); }
void VulkanCommandList::bind_descriptor_set(Pipeline p, uint32_t slot, DescriptorSet set) {
    const auto& v = _device._pipelines.at(p.id); auto descriptor = native<VkDescriptorSet>(set.id);
    vkCmdBindDescriptorSets(_cmd, v.bindPoint, v.layout, slot, 1, &descriptor, 0, nullptr);
}
void VulkanCommandList::push_constants(Pipeline p, Stage stage, const void* data, uint32_t size) {
    VkShaderStageFlags flags = 0;
    if ((stage & Stage::Compute) != Stage::None) flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    if ((stage & Stage::Fragment) != Stage::None) flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if ((stage & Stage::Vertex) != Stage::None) flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if ((stage & Stage::RayTracing) != Stage::None) flags |= VK_SHADER_STAGE_ALL;
    vkCmdPushConstants(_cmd, _device._pipelines.at(p.id).layout, flags, 0, size, data);
}
void VulkanCommandList::dispatch(uint32_t x, uint32_t y, uint32_t z) { vkCmdDispatch(_cmd, x, y, z); }
void VulkanCommandList::trace_rays(const ShaderBindingTable& table, uint32_t w, uint32_t h, uint32_t d) {
    auto convert = [](const auto& v) { return VkStridedDeviceAddressRegionKHR{v.address, v.stride, v.size}; };
    auto r = convert(table.raygen), m = convert(table.miss), hit = convert(table.hit), c = convert(table.callable);
    vkCmdTraceRaysKHR(_cmd, &r, &m, &hit, &c, w, h, d);
}
void VulkanCommandList::draw(uint32_t v, uint32_t i, uint32_t first, uint32_t firstInstance) { vkCmdDraw(_cmd, v, i, first, firstInstance); }
void VulkanCommandList::copy_image(Resource src, Resource dst, uint32_t w, uint32_t h, uint32_t d) {
    const auto& s = _device.resource(src); const auto& t = _device.resource(dst);
    VkImageCopy copy{}; copy.srcSubresource = {s.range.aspectMask, 0, 0, 1}; copy.dstSubresource = {t.range.aspectMask, 0, 0, 1}; copy.extent = {w, h, d};
    vkCmdCopyImage(_cmd, s.image, layout(s.state.layout), t.image, layout(t.state.layout), 1, &copy);
}
void VulkanCommandList::fill_buffer(Resource r, uint32_t value) { vkCmdFillBuffer(_cmd, _device.resource(r).buffer, 0, VK_WHOLE_SIZE, value); }
void VulkanCommandList::begin_render_pass(RenderTarget target, const ClearValues& clear) {
    const auto& t = _device._targets.at(target.id);
    VkClearValue values[2]{}; for (int i = 0; i < 4; ++i) values[0].color.float32[i] = clear.color[i];
    values[1].depthStencil = {clear.depth, clear.stencil};
    VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; info.renderPass = t.pass; info.framebuffer = t.framebuffer;
    info.renderArea.extent = {t.width, t.height}; info.clearValueCount = t.depth ? 2 : 1; info.pClearValues = values;
    vkCmdBeginRenderPass(_cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{0, 0, float(t.width), float(t.height), 0, 1}; VkRect2D scissor{{0, 0}, {t.width, t.height}};
    vkCmdSetViewport(_cmd, 0, 1, &viewport); vkCmdSetScissor(_cmd, 0, 1, &scissor); vkCmdSetDepthBias(_cmd, 0, 0, 0);
}
void VulkanCommandList::end_render_pass() { vkCmdEndRenderPass(_cmd); }
void VulkanCommandList::timestamp(QueryPool pool, uint32_t index, Stage stage) {
    const auto mask = stages(stage);
    if ((mask & (mask - 1)) != 0) throw std::invalid_argument("RHI: a timestamp needs one pipeline stage");
    vkCmdWriteTimestamp(_cmd, static_cast<VkPipelineStageFlagBits>(mask), native<VkQueryPool>(pool.id), index);
}
} // namespace rhi
