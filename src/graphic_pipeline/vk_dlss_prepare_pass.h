#pragma once

#include <vk_types.h>

#include <array>

class VulkanEngine;
namespace rg { class RenderGraph; }

// Prepares render-resolution, linear HDR/depth/motion inputs before DLSS.
// Geometry is static: motion includes camera movement; sky ignores translation.
class VulkanDlssPreparePass
{
public:
    void init(VulkanEngine* engine);
    void append_passes(rg::RenderGraph& graph, int frameSlot, const Texture& currentHDR);

    const Texture& output_color() const { return _color; }
    const Texture& output_depth() const { return _depth; }
    const Texture& output_motion() const { return _motion; }

private:
    struct Constants
    {
        glm::mat4 currentViewProjection{1.0f};
        glm::mat4 previousViewProjection{1.0f};
        glm::mat4 jitteredViewProjection{1.0f};
        glm::mat4 inverseProjection{1.0f};
        glm::mat4 inverseView{1.0f};
        glm::uvec4 extent{};
        glm::vec4 jitterUv{};
    };
    static_assert(sizeof(Constants) == 352);

    VulkanEngine* _engine = nullptr;
    const Texture* _worldPosition = nullptr;
    VkExtent3D _extent{};
    Texture _color{};
    Texture _depth{};
    Texture _motion{};
    std::array<AllocatedBuffer, 2> _uniforms{};
    std::array<VkDescriptorSet, 2> _sets{};
    std::array<VkImageView, 2> _boundColorViews{};
    VkDescriptorSetLayout _setLayout = VK_NULL_HANDLE;
    VkDescriptorPool _pool = VK_NULL_HANDLE;
    VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
    VkPipeline _pipeline = VK_NULL_HANDLE;
};
