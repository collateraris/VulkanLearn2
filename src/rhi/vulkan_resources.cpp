#include "vulkan_resources.h"
#include "vulkan_rhi.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace rhi {
namespace {
void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string("RHI: ") + operation + " failed (" + std::to_string(result) + ")");
}
VkFormat native_format(Format format) {
    switch (format) {
    case Format::Rgba16Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Format::Rgba32Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    throw std::invalid_argument("RHI: unsupported image format");
}
VkImageUsageFlags native_usage(ImageUsage usage) {
    const auto bits = uint32_t(usage);
    VkImageUsageFlags result = 0;
    if (bits & uint32_t(ImageUsage::Sampled)) result |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (bits & uint32_t(ImageUsage::Storage)) result |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (bits & uint32_t(ImageUsage::ColorAttachment)) result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (bits & uint32_t(ImageUsage::TransferSource)) result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (bits & uint32_t(ImageUsage::TransferDestination)) result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (bits & ~uint32_t(ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::ColorAttachment |
        ImageUsage::TransferSource | ImageUsage::TransferDestination))
        throw std::invalid_argument("RHI: unsupported image usage");
    return result;
}
VkBufferUsageFlags native_usage(BufferUsage usage) {
    const auto bits = uint32_t(usage);
    VkBufferUsageFlags result = 0;
    if (bits & uint32_t(BufferUsage::Uniform)) result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (bits & uint32_t(BufferUsage::Storage)) result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (bits & uint32_t(BufferUsage::TransferSource)) result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (bits & uint32_t(BufferUsage::TransferDestination)) result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (bits & ~uint32_t(BufferUsage::Uniform | BufferUsage::Storage |
        BufferUsage::TransferSource | BufferUsage::TransferDestination))
        throw std::invalid_argument("RHI: unsupported buffer usage");
    return result;
}
}

void VulkanResourceDevice::init(VulkanDevice& rhi, VkDevice device, VmaAllocator allocator) {
    if (!device || !allocator) throw std::invalid_argument("RHI: resource device needs a device and allocator");
    if (!_allocations.empty()) throw std::logic_error("RHI: cannot reinitialize a device with live resources");
    _rhi = &rhi;
    _device = device;
    _allocator = allocator;
}
void VulkanResourceDevice::require_initialized() const {
    if (!_rhi || !_device || !_allocator) throw std::logic_error("RHI: resource device is not initialized");
}
const VulkanResourceDevice::Allocation& VulkanResourceDevice::allocation(Resource resource) const {
    const auto& result = _allocations.at(resource.id);
    if (result.resource.kind != resource.kind) throw std::invalid_argument("RHI: resource kind mismatch");
    return result;
}

Resource VulkanResourceDevice::create_image(const ImageDescription& description) {
    require_initialized();
    if (!description.width || !description.height || description.usage == ImageUsage::None)
        throw std::invalid_argument("RHI: image dimensions and usage must be nonzero");
    Allocation result{};
    auto& texture = result.texture;
    texture.extend = {description.width, description.height, 1};
    texture.mipLevels = 1;
    texture.currAccessFlag = VK_ACCESS_NONE;
    texture.currImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    texture.currPipStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    auto& info = texture.createInfo;
    info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = native_format(description.format);
    info.extent = texture.extend;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = native_usage(description.usage);
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo memory{};
    memory.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    memory.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    check(vmaCreateImage(_allocator, &info, &memory, &texture.image._image,
        &texture.image._allocation, nullptr), "create image");
    try {
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = texture.image._image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = info.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(_device, &view, nullptr, &texture.imageView), "create image view");
        result.resource = _rhi->image(texture, ResourceState{});
        _allocations.emplace(result.resource.id, result);
    } catch (...) {
        if (result.resource.id) _rhi->forget(result.resource);
        if (texture.imageView) vkDestroyImageView(_device, texture.imageView, nullptr);
        vmaDestroyImage(_allocator, texture.image._image, texture.image._allocation);
        throw;
    }
    return result.resource;
}

Resource VulkanResourceDevice::create_buffer(const BufferDescription& description) {
    require_initialized();
    // The legacy descriptor adapter stores byte size in uint32_t.
    if (!description.size || description.size > (std::numeric_limits<uint32_t>::max)() || description.usage == BufferUsage::None)
        throw std::invalid_argument("RHI: buffer size or usage is invalid");
    if (description.memory != MemoryUsage::Upload && description.memory != MemoryUsage::DeviceLocal)
        throw std::invalid_argument("RHI: unsupported memory usage");
    Allocation result{};
    result.upload = description.memory == MemoryUsage::Upload;
    result.buffer._size = uint32_t(description.size);
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = description.size;
    info.usage = native_usage(description.usage);
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo memory{};
    memory.usage = VMA_MEMORY_USAGE_AUTO;
    if (result.upload)
        memory.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    else
        memory.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    check(vmaCreateBuffer(_allocator, &info, &memory, &result.buffer._buffer,
        &result.buffer._allocation, nullptr), "create buffer");
    try {
        result.resource = _rhi->buffer(result.buffer);
        _rhi->set_external_state(result.resource, ResourceState{});
        _allocations.emplace(result.resource.id, result);
    } catch (...) {
        if (result.resource.id) _rhi->forget(result.resource);
        vmaDestroyBuffer(_allocator, result.buffer._buffer, result.buffer._allocation);
        throw;
    }
    return result.resource;
}

void VulkanResourceDevice::write_buffer(Resource resource, const void* data, size_t size, uint64_t offset) {
    require_initialized();
    const auto& record = allocation(resource);
    if (resource.kind != ResourceKind::Buffer || !record.upload)
        throw std::invalid_argument("RHI: writes require an upload buffer");
    if (offset > record.buffer._size || size > record.buffer._size - offset || (size && !data))
        throw std::out_of_range("RHI: buffer write exceeds allocation");
    if (!size) return;
    void* mapped = nullptr;
    check(vmaMapMemory(_allocator, record.buffer._allocation, &mapped), "map buffer");
    std::memcpy(static_cast<char*>(mapped) + size_t(offset), data, size);
    const auto flushResult = vmaFlushAllocation(_allocator, record.buffer._allocation, offset, size);
    vmaUnmapMemory(_allocator, record.buffer._allocation);
    check(flushResult, "flush buffer");
    _rhi->set_external_state(resource, {Stage::Host, Access::HostWrite, Layout::Undefined});
}

const Texture& VulkanResourceDevice::texture(Resource resource) const {
    const auto& result = allocation(resource);
    if (resource.kind != ResourceKind::Image) throw std::invalid_argument("RHI: texture export needs an image");
    return result.texture;
}
const AllocatedBuffer& VulkanResourceDevice::buffer(Resource resource) const {
    const auto& result = allocation(resource);
    if (resource.kind != ResourceKind::Buffer) throw std::invalid_argument("RHI: buffer export needs a buffer");
    return result.buffer;
}
void VulkanResourceDevice::destroy(Resource resource) {
    if (!resource.id) return;
    require_initialized();
    const auto& record = allocation(resource);
    // The engine drains the deletion queue after vkDeviceWaitIdle and before
    // resetting VulkanDevice's canonical imports or destroying its allocator.
    _rhi->forget(resource);
    if (resource.kind == ResourceKind::Image) {
        vkDestroyImageView(_device, record.texture.imageView, nullptr);
        vmaDestroyImage(_allocator, record.texture.image._image, record.texture.image._allocation);
    } else {
        vmaDestroyBuffer(_allocator, record.buffer._buffer, record.buffer._allocation);
    }
    _allocations.erase(resource.id);
}
} // namespace rhi
