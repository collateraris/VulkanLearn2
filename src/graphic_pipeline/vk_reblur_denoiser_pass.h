#pragma once

#include <vk_types.h>
#include <rhi/nrd_reblur.h>
#include <array>

class VulkanEngine;
namespace rg { class RenderGraph; }

class ReblurDenoiserPass {
public:
    void init(VulkanEngine* engine, const Texture& diffuse, const Texture& specular, const Texture& bypass);
    void append_passes(rg::RenderGraph& graph, uint32_t frameSlot);
    void reset_history() { _historyValid = false; }
    const Texture& get_output() const { return _output; }

private:
    struct Constants {
        glm::mat4 worldToView{1.0f};
        glm::vec4 cameraPosition{};
        glm::uvec4 extent{};
        glm::vec4 hitDistanceAndRange{};
    };
    static_assert(sizeof(Constants) == 112);
    VulkanEngine* _engine = nullptr;
    VkExtent3D _extent{};
    std::array<const Texture*, 3> _raw{};
    std::array<const Texture*, 4> _guides{};
    // World motion, normal/roughness, positive view Z, demodulated YCoCg lobes.
    std::array<Texture, 5> _prepared{};
    Texture _output{};
    rhi::NrdReblur _reblur;
    std::array<AllocatedBuffer, 2> _uniforms{};
    std::array<std::array<VkDescriptorSet, 2>, 2> _sets{};
    std::array<VkDescriptorSetLayout, 2> _setLayouts{};
    std::array<VkPipelineLayout, 2> _layouts{};
    std::array<VkPipeline, 2> _pipelines{};
    VkDescriptorPool _pool = VK_NULL_HANDLE;
    bool _historyValid = false, _wasEnabled = false, _lastAccumulation = false;
    uint32_t _frameIndex = 0;
    glm::mat4 _previousView{1.0f}, _previousProjection{1.0f};
    glm::vec3 _previousPosition{}, _previousForward{};
    glm::vec2 _previousJitter{};
};
