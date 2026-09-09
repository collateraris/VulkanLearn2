#pragma once

#include <volk.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rhi {
struct TemporalUpscaleDescription;

struct DlssRequirements {
    std::vector<std::string> instanceExtensions;
    std::vector<std::string> deviceExtensions;
    VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    uint32_t graphicsQueues = 0;
    uint32_t computeQueues = 0;
};

struct DlssNativeTexture {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent2D extent{};
    VkImageUsageFlags usage = 0;
};

// Vulkan's optional DLSS Super Resolution backend. No Streamline SDK headers or
// DLL imports escape this interface. Native presentation hooks remain loaded
// through shutdown until the object is destroyed after its Vulkan device.
class StreamlineDlss {
public:
    StreamlineDlss();
    ~StreamlineDlss();
    StreamlineDlss(const StreamlineDlss&) = delete;
    StreamlineDlss& operator=(const StreamlineDlss&) = delete;

    bool initialize_before_device();
    const DlssRequirements& requirements() const noexcept;
    bool initialize_device(VkInstance instance, VkPhysicalDevice physicalDevice,
                           VkDevice device, uint32_t graphicsFamily, uint32_t queueIndex = 0,
                           uint32_t computeFamily = VK_QUEUE_FAMILY_IGNORED,
                           uint32_t computeQueueIndex = 0);
    bool supported() const noexcept;
    const std::string& status() const noexcept;
    // Keep interposer code resident when optional device requirements cannot
    // be enabled; already obtained Vulkan function pointers must stay valid.
    void disable(std::string reason);
    // Modes: 0 Off, 1 Quality, 2 Balanced, 3 Performance, 4 Ultra Performance, 5 DLAA.
    // Unavailable/invalid modes return the native output extent.
    VkExtent2D optimal_extent(int mode, VkExtent2D output);
    bool evaluate(VkCommandBuffer command, const DlssNativeTexture& color,
                  const DlssNativeTexture& depth, const DlssNativeTexture& motion,
                  const DlssNativeTexture& output, const TemporalUpscaleDescription& description);
    PFN_vkGetInstanceProcAddr instance_proc_addr() const noexcept;
    PFN_vkGetDeviceProcAddr device_proc_addr() const noexcept;
    void shutdown();

private:
    struct Implementation;
    std::unique_ptr<Implementation> _implementation;
};
} // namespace rhi
