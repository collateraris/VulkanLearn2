#pragma once

#include "rhi.h"

#include <cstddef>

namespace rhi {
enum class Format : uint8_t { Rgba16Float, Rgba32Float, Rg16Float, R32Float };
enum class ImageUsage : uint32_t {
    None = 0, Sampled = 1 << 0, Storage = 1 << 1,
    ColorAttachment = 1 << 2, TransferSource = 1 << 3, TransferDestination = 1 << 4
};
enum class BufferUsage : uint32_t {
    None = 0, Uniform = 1 << 0, Storage = 1 << 1,
    TransferSource = 1 << 2, TransferDestination = 1 << 3
};
constexpr ImageUsage operator|(ImageUsage a, ImageUsage b) { return ImageUsage(uint32_t(a) | uint32_t(b)); }
constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) { return BufferUsage(uint32_t(a) | uint32_t(b)); }
enum class MemoryUsage : uint8_t { DeviceLocal, Upload };

// The active post-processing passes require single-mip, single-layer 2D images.
// Creation leaves their contents undefined; the graph declares the first use.
struct ImageDescription {
    uint32_t width = 0, height = 0;
    Format format = Format::Rgba32Float;
    ImageUsage usage = ImageUsage::None;
};
struct BufferDescription {
    uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    MemoryUsage memory = MemoryUsage::DeviceLocal;
};

class ResourceDevice {
public:
    virtual ~ResourceDevice() = default;
    virtual Resource create_image(const ImageDescription&) = 0;
    virtual Resource create_buffer(const BufferDescription&) = 0;
    // Upload buffers only. The caller must first retire GPU use of this range
    // (the active passes wait for their frame-slot fence before registration).
    virtual void write_buffer(Resource, const void* data, size_t size, uint64_t offset = 0) = 0;
    // Called after GPU completion, before the backend allocator is destroyed.
    virtual void destroy(Resource) = 0;
};
} // namespace rhi
