#pragma once

#include <cstdint>
#include <array>
#include <type_traits>

// The frame graph and command recording use this API. Native Vulkan objects
// enter through VulkanDevice's import functions, never through graph callbacks.
namespace rhi {
enum class ResourceKind : uint8_t { Image, Buffer };
enum class Stage : uint32_t {
    None = 0, Top = 1 << 0, RayTracing = 1 << 1, Compute = 1 << 2,
    Vertex = 1 << 3, Fragment = 1 << 4, ColorOutput = 1 << 5,
    Depth = 1 << 6, Transfer = 1 << 7, Host = 1 << 8,
    Bottom = 1 << 9, AllCommands = 1 << 10, Indirect = 1 << 11
};
enum class Access : uint32_t {
    None = 0, ShaderRead = 1 << 0, ShaderWrite = 1 << 1,
    UniformRead = 1 << 2, ColorRead = 1 << 3, ColorWrite = 1 << 4,
    DepthRead = 1 << 5, DepthWrite = 1 << 6, TransferRead = 1 << 7,
    TransferWrite = 1 << 8, HostRead = 1 << 9, HostWrite = 1 << 10,
    VertexRead = 1 << 11, IndexRead = 1 << 12, IndirectRead = 1 << 13,
    MemoryRead = 1 << 14, MemoryWrite = 1 << 15
};
constexpr Stage operator|(Stage a, Stage b) { return Stage(uint32_t(a) | uint32_t(b)); }
constexpr Stage operator&(Stage a, Stage b) { return Stage(uint32_t(a) & uint32_t(b)); }
constexpr Access operator|(Access a, Access b) { return Access(uint32_t(a) | uint32_t(b)); }
constexpr Access operator&(Access a, Access b) { return Access(uint32_t(a) & uint32_t(b)); }
constexpr bool writes(Access a) {
    return (a & (Access::ShaderWrite | Access::ColorWrite | Access::DepthWrite |
        Access::TransferWrite | Access::HostWrite | Access::MemoryWrite)) != Access::None;
}
constexpr bool reads(Access a) {
    return (a & (Access::ShaderRead | Access::UniformRead | Access::ColorRead |
        Access::DepthRead | Access::TransferRead | Access::HostRead | Access::VertexRead |
        Access::IndexRead | Access::IndirectRead | Access::MemoryRead)) != Access::None;
}
enum class Layout : uint8_t {
    Undefined, General, ShaderReadOnly, ColorAttachment, DepthAttachment,
    TransferSource, TransferDestination, Present
};
struct Resource {
    uint64_t id = 0;
    ResourceKind kind = ResourceKind::Image;
    bool operator==(const Resource&) const = default;
};
struct ResourceState {
    Stage stages = Stage::Top;
    Access access = Access::None;
    Layout layout = Layout::Undefined;
    bool operator==(const ResourceState&) const = default;
};
struct Pipeline { uint64_t id = 0; };
struct DescriptorSet { uint64_t id = 0; };
struct RenderTarget { uint64_t id = 0; };
struct QueryPool { uint64_t id = 0; };
struct ShaderTableRegion { uint64_t address = 0, stride = 0, size = 0; };
struct ShaderBindingTable { ShaderTableRegion raygen, miss, hit, callable; };
struct ClearValues { float color[4] = {0, 0, 0, 1}; float depth = 1; uint32_t stencil = 0; };

enum class UpscaleMode : uint32_t { Off = 0, Quality = 1, Balanced = 2, Performance = 3, UltraPerformance = 4, DLAA = 5 };
struct TemporalUpscaleDescription {
    Resource color, depth, motion, output;
    UpscaleMode mode = UpscaleMode::Off;
    uint32_t frameIndex = 0;
    uint32_t renderWidth = 0, renderHeight = 0, outputWidth = 0, outputHeight = 0;
    // Row-major, row-vector camera transforms without projection jitter.
    std::array<float, 16> viewToClip{}, clipToView{}, clipToPrevClip{}, prevClipToClip{};
    // Jitter is in render-resolution pixels; motion scales convert to UV units.
    float jitterX = 0, jitterY = 0;
    float motionScaleX = 1, motionScaleY = 1;
    std::array<float, 3> cameraPosition{}, cameraUp{}, cameraRight{}, cameraForward{};
    float cameraNear = 0.01f, cameraFar = 10000.f, cameraFov = 1.2217305f, cameraAspect = 1.7777778f;
    float preExposure = 1.f;
    bool reset = true, depthInverted = false;
};

class CommandList {
public:
    virtual ~CommandList() = default;
    virtual void transition(Resource resource, ResourceState state) = 0;
    virtual void begin_label(const char* name) = 0;
    virtual void end_label() = 0;
    virtual void bind_pipeline(Pipeline pipeline) = 0;
    virtual void bind_descriptor_set(Pipeline pipeline, uint32_t slot, DescriptorSet set) = 0;
    virtual void push_constants(Pipeline pipeline, Stage stages, const void* data, uint32_t size) = 0;
    virtual void dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;
    virtual void trace_rays(const ShaderBindingTable& table, uint32_t width, uint32_t height, uint32_t depth = 1) = 0;
    virtual void draw(uint32_t vertices, uint32_t instances = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
    virtual void copy_image(Resource source, Resource destination, uint32_t width, uint32_t height, uint32_t depth = 1) = 0;
    virtual void fill_buffer(Resource buffer, uint32_t value) = 0;
    virtual void begin_render_pass(RenderTarget target, const ClearValues& clear) = 0;
    virtual void end_render_pass() = 0;
    virtual void timestamp(QueryPool pool, uint32_t index, Stage stage = Stage::Bottom) = 0;
    // Optional temporal upscaler. Backends without one return false.
    virtual bool evaluate_upscaler(const TemporalUpscaleDescription&) { return false; }
};
} // namespace rhi
