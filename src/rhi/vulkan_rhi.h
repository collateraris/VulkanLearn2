#pragma once

#include "rhi.h"
#include <vk_types.h>
#include <optional>
#include <memory>
#include <unordered_map>

struct ImDrawData;

namespace rhi {
class VulkanDevice;
class ResourceDevice;
class VulkanResourceDevice;
class VulkanCommandList final : public CommandList {
public:
    explicit VulkanCommandList(VulkanDevice& device) : _device(device) {}
    void transition(Resource, ResourceState) override;
    void begin_label(const char*) override;
    void end_label() override;
    void bind_pipeline(Pipeline) override;
    void bind_descriptor_set(Pipeline, uint32_t, DescriptorSet) override;
    void push_constants(Pipeline, Stage, const void*, uint32_t) override;
    void dispatch(uint32_t, uint32_t, uint32_t) override;
    void trace_rays(const ShaderBindingTable&, uint32_t, uint32_t, uint32_t) override;
    void draw(uint32_t, uint32_t, uint32_t, uint32_t) override;
    void copy_image(Resource, Resource, uint32_t, uint32_t, uint32_t) override;
    void fill_buffer(Resource, uint32_t) override;
    void begin_render_pass(RenderTarget, const ClearValues&) override;
    void end_render_pass() override;
    void timestamp(QueryPool, uint32_t, Stage) override;
private:
    friend class VulkanDevice;
    VulkanDevice& _device;
    VkCommandBuffer _cmd = VK_NULL_HANDLE;
};

// Vulkan ownership/bootstrap remains with the engine. Imports are non-owning,
// canonical per native object; state survives frame-graph reset and frame slots.
class VulkanDevice {
public:
    VulkanDevice();
    ~VulkanDevice();
    void init(VkDevice device, bool debugUtilsEnabled = false);
    void init_resources(VmaAllocator allocator);
    ResourceDevice& resources();
    VulkanResourceDevice& vulkan_resources();
    CommandList& begin_commands(VkCommandBuffer commandBuffer);
    Resource image(const Texture&, std::optional<ResourceState> initial = std::nullopt);
    Resource image(VkImage, VkImageAspectFlags, uint32_t levels, uint32_t layers, ResourceState initial);
    Resource buffer(const AllocatedBuffer&);
    Pipeline pipeline(VkPipeline, VkPipelineLayout, VkPipelineBindPoint);
    DescriptorSet descriptor(VkDescriptorSet) const;
    RenderTarget render_target(VkRenderPass, VkFramebuffer, uint32_t width, uint32_t height, bool hasDepth = false);
    QueryPool query_pool(VkQueryPool) const;
    static ShaderBindingTable shader_table(const VkStridedDeviceAddressRegionKHR&, const VkStridedDeviceAddressRegionKHR&,
        const VkStridedDeviceAddressRegionKHR&, const VkStridedDeviceAddressRegionKHR&);
    // Interop for Vulkan render-pass final layouts and diagnostics that restore
    // layouts themselves. Ordinary passes must declare states through the graph.
    void set_external_state(Resource, ResourceState);
    void forget(Resource);
    void reset();
    void draw_imgui(ImDrawData*);
private:
    friend class VulkanCommandList;
    struct ResourceRecord {
        VkImage image = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkImageSubresourceRange range{};
        ResourceState state{};
        ResourceState lastWriter{Stage::None, Access::None, Layout::Undefined};
    };
    struct PipelineRecord { VkPipeline pipeline; VkPipelineLayout layout; VkPipelineBindPoint bindPoint; };
    struct TargetRecord { VkRenderPass pass; VkFramebuffer framebuffer; uint32_t width, height; bool depth; };
    ResourceRecord& resource(Resource);
    VkDevice _device = VK_NULL_HANDLE;
    VulkanCommandList _commands;
    std::unique_ptr<VulkanResourceDevice> _resourceDevice;
    uint64_t _nextResource = 1;
    std::unordered_map<uint64_t, ResourceRecord> _resources;
    std::unordered_map<VkImage, Resource> _images;
    std::unordered_map<VkBuffer, Resource> _buffers;
    std::unordered_map<uint64_t, PipelineRecord> _pipelines;
    std::unordered_map<uint64_t, TargetRecord> _targets;
    PFN_vkCmdBeginDebugUtilsLabelEXT _beginLabel = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT _endLabel = nullptr;
};
} // namespace rhi
