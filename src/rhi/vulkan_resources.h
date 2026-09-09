#pragma once

#include "resource.h"
#include <vk_types.h>

#include <unordered_map>

namespace rhi {
class VulkanDevice;

// VMA ownership is isolated here. The engine's deletion queue controls when
// resources die; this object never destroys Vulkan objects from its destructor.
class VulkanResourceDevice final : public ResourceDevice {
public:
    void init(VulkanDevice&, VkDevice, VmaAllocator);
    Resource create_image(const ImageDescription&) override;
    Resource create_buffer(const BufferDescription&) override;
    void write_buffer(Resource, const void* data, size_t size, uint64_t offset = 0) override;
    void destroy(Resource) override;

    // Explicit, non-owning compatibility exports for existing Vulkan descriptor
    // and framebuffer setup. Their allocations remain owned by this device.
    const Texture& texture(Resource) const;
    const AllocatedBuffer& buffer(Resource) const;

private:
    struct Allocation {
        Resource resource{};
        Texture texture{};
        AllocatedBuffer buffer{};
        bool upload = false;
    };
    void require_initialized() const;
    const Allocation& allocation(Resource) const;
    VulkanDevice* _rhi = nullptr;
    VkDevice _device = VK_NULL_HANDLE;
    VmaAllocator _allocator = VK_NULL_HANDLE;
    std::unordered_map<uint64_t, Allocation> _allocations;
};
} // namespace rhi
