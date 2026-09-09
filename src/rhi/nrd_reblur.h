#pragma once

#include <vk_types.h>
#include <NRD.h>
#include <memory>

class VulkanEngine;
namespace rg { class RenderGraph; }

namespace rhi {
struct NrdReblurInputs {
    const Texture* motion = nullptr;
    const Texture* normalRoughness = nullptr;
    const Texture* viewZ = nullptr;
    const Texture* diffuseRadianceHitDistance = nullptr;
    const Texture* specularRadianceHitDistance = nullptr;
    const Texture* baseColorMetalness = nullptr;
};

// Executes the SDK's REBLUR_DIFFUSE_SPECULAR dispatch list. Preparation and
// composition stay in the renderer; every SDK image/constant dependency is
// declared to the same render graph as the rest of the frame.
class NrdReblur {
public:
    void init(VulkanEngine& engine, uint32_t width, uint32_t height);
    // Call once after the frame-slot fence has completed. The settings' camera
    // matrices, jitter and signals must follow the linked SDK's encoding.
    void append_passes(rg::RenderGraph& graph, uint32_t frameSlot,
        const NrdReblurInputs& inputs, const nrd::CommonSettings& common,
        const nrd::ReblurSettings& settings);
    const Texture& diffuse_output() const;
    const Texture& specular_output() const;

private:
    struct Impl;
    std::shared_ptr<Impl> _impl;
};
} // namespace rhi
