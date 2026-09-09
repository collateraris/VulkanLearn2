#pragma once

#include <vk_types.h>

#include <array>

class VulkanEngine;
namespace rg { class RenderGraph; }

// Filters displayed HDR without feeding filtered pixels back into accumulation.
class VulkanSpatialDenoiserPass
{
public:
    void init(VulkanEngine* engine, const Texture& sourceAccumulated);
    void append_passes(rg::RenderGraph& graph, int frameSlot);
    void reset_history();
    const Texture& get_output() const;

private:
    struct PushConstants
    {
        uint32_t width;
        uint32_t height;
        uint32_t stepWidth;
        uint32_t passIndex;
    };
    static_assert(sizeof(PushConstants) == 16);

    struct TemporalConstants
    {
        glm::mat4 previousViewProjection{1.0f};
        glm::vec4 previousCameraPosition{};
        glm::vec4 currentCameraPosition{};
        glm::uvec4 frameInfo{};
        glm::vec4 options{};
    };
    static_assert(sizeof(TemporalConstants) == 128);

    VulkanEngine* _engine = nullptr;
    const Texture* _sourceTexture = nullptr;
    VkExtent3D _imageExtent{};
    std::array<Texture, 2> _textures{};
    Texture _prefiltered{};
    // Color/count, position/object ID, packed normal/roughness/metalness.
    std::array<std::array<Texture, 3>, 2> _history{};
    std::array<const Texture*, 4> _guides{};
    VkDescriptorSet _prefilterSet = VK_NULL_HANDLE;
    std::array<std::array<VkDescriptorSet, 2>, 2> _spatialSets{};
    std::array<VkDescriptorSet, 2> _temporalSets{};
    std::array<VkDescriptorSet, 2> _uniformSets{};
    std::array<AllocatedBuffer, 2> _uniformBuffers{};
    VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
    VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
    VkPipeline _pipeline = VK_NULL_HANDLE;
    VkPipeline _prefilterPipeline = VK_NULL_HANDLE;
    VkPipeline _temporalPipeline = VK_NULL_HANDLE;
    VkPipelineLayout _temporalPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout _temporalSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout _uniformSetLayout = VK_NULL_HANDLE;
    bool _historyValid = false;
    bool _wasEnabled = false;
    bool _lastAccumulationEnabled = false;
    uint32_t _historySlot = 0;
    glm::mat4 _previousView{1.0f};
    glm::mat4 _previousProjection{1.0f};
    glm::mat4 _previousViewProjection{1.0f};
    glm::vec3 _previousCameraPosition{};
    glm::vec3 _previousForward{};
};
