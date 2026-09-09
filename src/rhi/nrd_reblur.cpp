#include "nrd_reblur.h"
#include "vulkan_resources.h"
#include <vk_engine.h>
#include <vk_render_graph.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rhi {
namespace {
constexpr nrd::Identifier reblurIdentifier = 0;

void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string("NRD Vulkan: ") + operation + " failed (" + std::to_string(result) + ")");
}
void check(nrd::Result result, const char* operation) {
    if (result != nrd::Result::SUCCESS)
        throw std::runtime_error(std::string("NRD: ") + operation + " failed (" + std::to_string(uint32_t(result)) + ")");
}
Format pool_format(nrd::Format format) {
    switch (format) {
    case nrd::Format::R8_UNORM: return Format::R8Unorm;
    case nrd::Format::RG8_UNORM: return Format::Rg8Unorm;
    case nrd::Format::R10_G10_B10_A2_UNORM: return Format::Rgb10A2Unorm;
    case nrd::Format::RGBA8_UNORM: return Format::Rgba8Unorm;
    case nrd::Format::R16_SFLOAT: return Format::R16Float;
    case nrd::Format::R16_UINT: return Format::R16Uint;
    case nrd::Format::R32_UINT: return Format::R32Uint;
    case nrd::Format::R32_SFLOAT: return Format::R32Float;
    case nrd::Format::RG16_SFLOAT: return Format::Rg16Float;
    case nrd::Format::RGBA16_SFLOAT: return Format::Rgba16Float;
    case nrd::Format::RGBA32_SFLOAT: return Format::Rgba32Float;
    default: throw std::runtime_error("NRD REBLUR requested an unsupported pool format: " + std::to_string(uint32_t(format)));
    }
}
VkDescriptorType descriptor_type(nrd::DescriptorType type) {
    if (type == nrd::DescriptorType::TEXTURE) return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    if (type == nrd::DescriptorType::STORAGE_TEXTURE) return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    throw std::runtime_error("NRD requested an unsupported descriptor type");
}
}

struct NrdReblur::Impl {
    struct PipelineState {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> sets;
        bool clear = false;
    };
    struct FrameState {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        Resource constants{};
        uint32_t lastFrame = 0;
        bool prepared = false;
    };
    VulkanEngine* engine = nullptr;
    nrd::Instance* instance = nullptr;
    uint32_t width = 0, height = 0;
    uint32_t constantStride = 0, dispatchCapacity = 0;
    bool recorded = false;
    std::vector<PipelineState> pipelines;
    std::vector<VkSampler> samplers;
    std::vector<Resource> ownedImages;
    std::vector<Texture> permanent, transient;
    Texture diffuse{}, specular{};
    std::array<FrameState, FRAME_OVERLAP> frames{};

    void release() {
        if (!engine) return;
        const VkDevice device = engine->_device;
        for (auto& frame : frames) {
            if (frame.pool) vkDestroyDescriptorPool(device, frame.pool, nullptr);
            if (frame.constants.id) engine->_rhi.resources().destroy(frame.constants);
            frame = {};
        }
        for (auto& pipeline : pipelines) {
            if (pipeline.pipeline) vkDestroyPipeline(device, pipeline.pipeline, nullptr);
            if (pipeline.layout) vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
            for (auto layout : pipeline.sets) if (layout) vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
        pipelines.clear();
        for (auto sampler : samplers) vkDestroySampler(device, sampler, nullptr);
        samplers.clear();
        for (auto image : ownedImages) engine->_rhi.resources().destroy(image);
        ownedImages.clear();
        if (instance) nrd::DestroyInstance(*instance);
        instance = nullptr;
        engine = nullptr;
    }

    Texture create_image(uint32_t w, uint32_t h, Format format) {
        const auto resource = engine->_rhi.resources().create_image({w, h, format,
            ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::TransferSource});
        ownedImages.push_back(resource);
        return engine->_rhi.vulkan_resources().texture(resource);
    }

    const Texture& resolve(const nrd::ResourceDesc& resource, const NrdReblurInputs& inputs) const {
        const Texture* texture = nullptr;
        switch (resource.type) {
        case nrd::ResourceType::IN_MV: texture = inputs.motion; break;
        case nrd::ResourceType::IN_NORMAL_ROUGHNESS: texture = inputs.normalRoughness; break;
        case nrd::ResourceType::IN_VIEWZ: texture = inputs.viewZ; break;
        case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST: texture = inputs.diffuseRadianceHitDistance; break;
        case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST: texture = inputs.specularRadianceHitDistance; break;
        case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST: texture = &diffuse; break;
        case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST: texture = &specular; break;
        case nrd::ResourceType::PERMANENT_POOL: texture = &permanent.at(resource.indexInPool); break;
        case nrd::ResourceType::TRANSIENT_POOL: texture = &transient.at(resource.indexInPool); break;
        default: throw std::runtime_error("NRD REBLUR requested an unbound optional resource: " +
            std::string(nrd::GetResourceTypeString(resource.type)));
        }
        if (!texture || !texture->imageView)
            throw std::runtime_error("NRD REBLUR input is missing: " + std::string(nrd::GetResourceTypeString(resource.type)));
        return *texture;
    }

    void create_pipelines() {
        const auto& description = *nrd::GetInstanceDesc(*instance);
        const auto& offsets = nrd::GetLibraryDesc()->spirvBindingOffsets;
        const uint32_t sharedSpace = description.constantBufferAndSamplersSpaceIndex;
        const uint32_t setCount = 1 + std::max(sharedSpace, description.resourcesSpaceIndex);
        if (setCount > engine->_gpuProperties.limits.maxBoundDescriptorSets)
            throw std::runtime_error("NRD descriptor spaces exceed the Vulkan device limit");
        pipelines.resize(description.pipelinesNum);
        for (uint32_t index = 0; index < description.pipelinesNum; ++index) {
            const auto& source = description.pipelines[index];
            auto& target = pipelines[index];
            target.clear = std::string_view(source.shaderIdentifier).starts_with("Clear");
            std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindings(setCount);
            if (source.hasConstantData)
                bindings[sharedSpace].push_back({
                    description.constantBufferRegisterIndex + offsets.constantBufferOffset,
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
            for (uint32_t i = 0; i < description.samplersNum; ++i)
                bindings[sharedSpace].push_back({
                    description.samplersBaseRegisterIndex + offsets.samplerOffset + i,
                    VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &samplers[i]});
            for (uint32_t rangeIndex = 0; rangeIndex < source.resourceRangesNum; ++rangeIndex) {
                const auto& range = source.resourceRanges[rangeIndex];
                const uint32_t offset = range.descriptorType == nrd::DescriptorType::TEXTURE ?
                    offsets.textureOffset : offsets.storageTextureAndBufferOffset;
                for (uint32_t i = 0; i < range.descriptorsNum; ++i)
                    bindings[description.resourcesSpaceIndex].push_back({description.resourcesBaseRegisterIndex + offset + i,
                        descriptor_type(range.descriptorType), 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr});
            }
            target.sets.resize(setCount);
            for (uint32_t set = 0; set < setCount; ++set) {
                auto& sorted = bindings[set];
                std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.binding < b.binding; });
                for (size_t i = 1; i < sorted.size(); ++i)
                    if (sorted[i - 1].binding == sorted[i].binding)
                        throw std::runtime_error("NRD SPIR-V binding offsets overlap");
                VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
                layoutInfo.bindingCount = uint32_t(sorted.size());
                layoutInfo.pBindings = sorted.data();
                check(vkCreateDescriptorSetLayout(engine->_device, &layoutInfo, nullptr, &target.sets[set]), "create descriptor layout");
            }
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = uint32_t(target.sets.size());
            layoutInfo.pSetLayouts = target.sets.data();
            check(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &target.layout), "create pipeline layout");
            const auto& shader = source.computeShaderSPIRV;
            if (!shader.bytecode || !shader.size || shader.size % 4)
                throw std::runtime_error("NRD must be built with embedded SPIR-V shaders");
            // SDK bytecode storage need not have uint32_t alignment.
            std::vector<uint32_t> code(size_t(shader.size / 4));
            std::memcpy(code.data(), shader.bytecode, size_t(shader.size));
            VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            moduleInfo.codeSize = size_t(shader.size);
            moduleInfo.pCode = code.data();
            VkShaderModule module = VK_NULL_HANDLE;
            check(vkCreateShaderModule(engine->_device, &moduleInfo, nullptr, &module), "create shader module");
            VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            pipelineInfo.stage.module = module;
            pipelineInfo.stage.pName = description.shaderEntryPoint;
            pipelineInfo.layout = target.layout;
            const VkResult result = vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &target.pipeline);
            vkDestroyShaderModule(engine->_device, module, nullptr);
            check(result, "create compute pipeline");
        }
    }
};

void NrdReblur::init(VulkanEngine& engine, uint32_t width, uint32_t height) {
    if (_impl) throw std::logic_error("NRD REBLUR has already been initialized");
    if (!width || !height || width > UINT16_MAX || height > UINT16_MAX)
        throw std::invalid_argument("NRD REBLUR extent is invalid");
    auto state = std::make_shared<Impl>();
    state->engine = &engine;
    state->width = width;
    state->height = height;
    try {
        const auto& library = *nrd::GetLibraryDesc();
        if (library.versionMajor != NRD_VERSION_MAJOR || library.versionMinor != NRD_VERSION_MINOR ||
            library.versionBuild != NRD_VERSION_BUILD)
            throw std::runtime_error("NRD runtime DLL does not match the compiled SDK headers");
        if (library.normalEncoding != nrd::NormalEncoding::R10_G10_B10_A2_UNORM ||
            library.roughnessEncoding != nrd::RoughnessEncoding::LINEAR)
            throw std::runtime_error("NRD REBLUR integration requires normal encoding2 and linear roughness encoding1");
        const nrd::DenoiserDesc denoiser{reblurIdentifier, nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR};
        nrd::InstanceCreationDesc creation{};
        creation.denoisers = &denoiser;
        creation.denoisersNum = 1;
        check(nrd::CreateInstance(creation, state->instance), "create REBLUR_DIFFUSE_SPECULAR instance");
        const auto& description = *nrd::GetInstanceDesc(*state->instance);
        auto createPool = [&](const nrd::TextureDesc* descriptions, uint32_t count, std::vector<Texture>& pool) {
            pool.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                const auto& texture = descriptions[i];
                const uint32_t factor = texture.downsampleFactor;
                if (!factor) throw std::runtime_error("NRD REBLUR pool has invalid downsample factor");
                pool.push_back(state->create_image((width + factor - 1) / factor,
                    (height + factor - 1) / factor, pool_format(texture.format)));
            }
        };
        createPool(description.permanentPool, description.permanentPoolSize, state->permanent);
        createPool(description.transientPool, description.transientPoolSize, state->transient);
        state->diffuse = state->create_image(width, height, Format::Rgba16Float);
        state->specular = state->create_image(width, height, Format::Rgba16Float);
        state->samplers.resize(description.samplersNum);
        for (uint32_t i = 0; i < description.samplersNum; ++i) {
            const auto mode = description.samplers[i];
            VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            const bool linear = mode == nrd::Sampler::LINEAR_CLAMP;
            if (!linear && mode != nrd::Sampler::NEAREST_CLAMP)
                throw std::runtime_error("NRD REBLUR requested unsupported sampler mode");
            sampler.magFilter = sampler.minFilter = linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            check(vkCreateSampler(engine._device, &sampler, nullptr, &state->samplers[i]), "create sampler");
        }
        state->create_pipelines();
        const uint64_t alignment = std::max(uint64_t(16), uint64_t(engine._gpuProperties.limits.minUniformBufferOffsetAlignment));
        const uint64_t stride = (description.constantBufferMaxDataSize + alignment - 1) / alignment * alignment;
        if (!stride || stride > engine._gpuProperties.limits.maxUniformBufferRange)
            throw std::runtime_error("NRD constant buffer exceeds the device uniform range");
        state->constantStride = uint32_t(stride);
        state->dispatchCapacity = description.descriptorPoolDesc.setsMaxNum;
        const auto& limits = description.descriptorPoolDesc;
        if (!state->dispatchCapacity || description.samplersNum == 0 || state->pipelines.empty())
            throw std::runtime_error("NRD returned an empty dispatch resource contract");
        const uint32_t setCount = uint32_t(state->pipelines.front().sets.size());
        const std::array<VkDescriptorPoolSize, 4> poolSizes{{
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, std::max(1u, state->dispatchCapacity * limits.perSetTexturesMaxNum)},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, std::max(1u, state->dispatchCapacity * limits.perSetStorageTexturesMaxNum)},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, state->dispatchCapacity},
            {VK_DESCRIPTOR_TYPE_SAMPLER, state->dispatchCapacity * description.samplersNum}
        }};
        for (auto& frame : state->frames) {
            frame.constants = engine._rhi.resources().create_buffer({stride * state->dispatchCapacity,
                BufferUsage::Uniform, MemoryUsage::Upload});
            VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool.maxSets = state->dispatchCapacity * setCount;
            pool.poolSizeCount = uint32_t(poolSizes.size());
            pool.pPoolSizes = poolSizes.data();
            check(vkCreateDescriptorPool(engine._device, &pool, nullptr, &frame.pool), "create frame descriptor pool");
        }
        // The queue executes after GPU completion and before the VMA allocator.
        // Capturing shared state also keeps cleanup valid if the pass is replaced.
        engine._mainDeletionQueue.push_function([state]() { state->release(); });
        _impl = state;
        std::cout << "NRD REBLUR_DIFFUSE_SPECULAR " << unsigned(library.versionMajor) << '.'
            << unsigned(library.versionMinor) << '.' << unsigned(library.versionBuild)
            << ": " << width << 'x' << height << ", " << description.permanentPoolSize
            << " permanent / " << description.transientPoolSize << " transient images; normal encoding "
            << unsigned(library.normalEncoding) << ", roughness encoding " << unsigned(library.roughnessEncoding) << '\n';
    } catch (...) {
        state->release();
        throw;
    }
}

void NrdReblur::append_passes(rg::RenderGraph& graph, uint32_t frameSlot,
    const NrdReblurInputs& inputs, const nrd::CommonSettings& common, const nrd::ReblurSettings& settings) {
    if (!_impl || !_impl->instance) throw std::logic_error("NRD REBLUR is not initialized");
    auto state = _impl;
    auto& frame = state->frames.at(frameSlot);
    if (frame.prepared && frame.lastFrame == common.frameIndex)
        throw std::logic_error("NRD REBLUR was registered twice for one frame");
    if (common.enableValidation || common.isHistoryConfidenceAvailable || common.isDisocclusionThresholdMixAvailable)
        throw std::invalid_argument("NRD optional validation/confidence inputs were not provided");
    if (common.resourceSize[0] != state->width || common.resourceSize[1] != state->height ||
        common.resourceSizePrev[0] != state->width || common.resourceSizePrev[1] != state->height ||
        common.rectSize[0] != state->width || common.rectSize[1] != state->height ||
        common.rectSizePrev[0] != state->width || common.rectSizePrev[1] != state->height ||
        common.rectOrigin[0] || common.rectOrigin[1])
        throw std::invalid_argument("NRD REBLUR common settings do not match the allocated full-frame extent");
    auto& engine = *state->engine;
    auto& device = engine._rhi;
    auto commonSettings = common;
    if (!state->recorded) commonSettings.accumulationMode = nrd::AccumulationMode::CLEAR_AND_RESTART;
    check(nrd::SetCommonSettings(*state->instance, commonSettings), "set common settings");
    check(nrd::SetDenoiserSettings(*state->instance, reblurIdentifier, &settings), "set REBLUR settings");
    const nrd::DispatchDesc* dispatches = nullptr;
    uint32_t dispatchCount = 0;
    check(nrd::GetComputeDispatches(*state->instance, &reblurIdentifier, 1, dispatches, dispatchCount), "get compute dispatches");
    if (!dispatchCount || dispatchCount > state->dispatchCapacity)
        throw std::runtime_error("NRD dispatch count exceeds its descriptor pool contract");
    // The caller has waited for this slot. Every dispatch owns its own immutable
    // descriptors and aligned constants until that same fence retires them.
    check(vkResetDescriptorPool(engine._device, frame.pool, 0), "reset retired descriptor pool");
    const auto& description = *nrd::GetInstanceDesc(*state->instance);
    const auto& offsets = nrd::GetLibraryDesc()->spirvBindingOffsets;
    const auto& buffer = device.vulkan_resources().buffer(frame.constants);
    const auto constants = graph.import_resource("NRD.Constants", frame.constants);
    std::vector<uint8_t> constantBytes(size_t(state->constantStride) * dispatchCount, 0);
    const ResourceState sampled{Stage::Compute, Access::ShaderRead, Layout::General};
    const ResourceState storage{Stage::Compute, Access::ShaderRead | Access::ShaderWrite, Layout::General};
    const ResourceState clear{Stage::Compute, Access::ShaderWrite, Layout::General};
    rg::PassHandle previousPass = 0;
    for (uint32_t index = 0; index < dispatchCount; ++index) {
        const auto& dispatch = dispatches[index];
        const auto& pipeline = state->pipelines.at(dispatch.pipelineIndex);
        const auto& pipelineDescription = description.pipelines[dispatch.pipelineIndex];
        if (dispatch.constantBufferDataSize > description.constantBufferMaxDataSize ||
            (dispatch.constantBufferDataSize && !dispatch.constantBufferData))
            throw std::runtime_error("NRD returned invalid constant buffer data");
        const VkDeviceSize constantOffset = VkDeviceSize(index) * state->constantStride;
        if (dispatch.constantBufferDataSize)
            std::memcpy(constantBytes.data() + size_t(constantOffset), dispatch.constantBufferData, dispatch.constantBufferDataSize);
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = frame.pool;
        allocation.descriptorSetCount = uint32_t(pipeline.sets.size());
        allocation.pSetLayouts = pipeline.sets.data();
        std::vector<VkDescriptorSet> sets(pipeline.sets.size());
        check(vkAllocateDescriptorSets(engine._device, &allocation, sets.data()), "allocate dispatch descriptor sets");
        std::vector<VkDescriptorImageInfo> images(dispatch.resourcesNum);
        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(dispatch.resourcesNum + 1);
        std::vector<rg::Use> uses;
        uses.reserve(dispatch.resourcesNum + 1);
        uint32_t resourceIndex = 0;
        for (uint32_t rangeIndex = 0; rangeIndex < pipelineDescription.resourceRangesNum; ++rangeIndex) {
            const auto& range = pipelineDescription.resourceRanges[rangeIndex];
            const bool readOnly = range.descriptorType == nrd::DescriptorType::TEXTURE;
            const uint32_t bindingOffset = readOnly ? offsets.textureOffset : offsets.storageTextureAndBufferOffset;
            for (uint32_t i = 0; i < range.descriptorsNum; ++i, ++resourceIndex) {
                if (resourceIndex >= dispatch.resourcesNum || dispatch.resources[resourceIndex].descriptorType != range.descriptorType)
                    throw std::runtime_error("NRD dispatch resources do not match its pipeline layout");
                const auto& resource = dispatch.resources[resourceIndex];
                const auto& texture = state->resolve(resource, inputs);
                images[resourceIndex] = {VK_NULL_HANDLE, texture.imageView, VK_IMAGE_LAYOUT_GENERAL};
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet = sets[description.resourcesSpaceIndex];
                write.dstBinding = bindingOffset + description.resourcesBaseRegisterIndex + i;
                write.descriptorCount = 1;
                write.descriptorType = descriptor_type(range.descriptorType);
                write.pImageInfo = &images[resourceIndex];
                writes.push_back(write);
                std::string name = "NRD." + std::string(nrd::GetResourceTypeString(resource.type));
                if (resource.type == nrd::ResourceType::PERMANENT_POOL || resource.type == nrd::ResourceType::TRANSIENT_POOL)
                    name += "." + std::to_string(resource.indexInPool);
                const bool externalInput = uint32_t(resource.type) < uint32_t(nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST);
                const auto handle = graph.import_resource(std::move(name), device.image(texture), externalInput || state->recorded);
                uses.push_back({handle, readOnly ? sampled : (pipeline.clear ? clear : storage)});
            }
        }
        if (resourceIndex != dispatch.resourcesNum)
            throw std::runtime_error("NRD dispatch has excess resources");
        VkDescriptorBufferInfo uniform{buffer._buffer, constantOffset, description.constantBufferMaxDataSize};
        if (pipelineDescription.hasConstantData) {
            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = sets[description.constantBufferAndSamplersSpaceIndex];
            write.dstBinding = offsets.constantBufferOffset + description.constantBufferRegisterIndex;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo = &uniform;
            writes.push_back(write);
            uses.push_back({constants, {Stage::Compute, Access::UniformRead, Layout::Undefined}});
        }
        vkUpdateDescriptorSets(engine._device, uint32_t(writes.size()), writes.data(), 0, nullptr);
        const auto pipelineHandle = device.pipeline(pipeline.pipeline, pipeline.layout, VK_PIPELINE_BIND_POINT_COMPUTE);
        std::vector<DescriptorSet> descriptorSets;
        for (auto set : sets) descriptorSets.push_back(device.descriptor(set));
        const uint32_t gridWidth = dispatch.gridWidth, gridHeight = dispatch.gridHeight;
        const bool last = index + 1 == dispatchCount;
        const auto pass = graph.add_pass("NRD.REBLUR." + std::to_string(index) + "." + dispatch.name, std::move(uses),
            [state, pipelineHandle, descriptorSets = std::move(descriptorSets), gridWidth, gridHeight, last](CommandList& commands) {
                commands.bind_pipeline(pipelineHandle);
                for (uint32_t set = 0; set < descriptorSets.size(); ++set)
                    commands.bind_descriptor_set(pipelineHandle, set, descriptorSets[set]);
                commands.dispatch(gridWidth, gridHeight, 1);
                if (last) state->recorded = true;
            });
        // Preserve the SDK's dispatch order as well as exposing each image's
        // hazards, including independent clears at the start of a reset.
        if (index) graph.depends_on(pass, previousPass);
        previousPass = pass;
    }
    // One host write/flush after copying all SDK-owned dispatch constants.
    device.resources().write_buffer(frame.constants, constantBytes.data(), constantBytes.size());
    frame.prepared = true;
    frame.lastFrame = common.frameIndex;
}

const Texture& NrdReblur::diffuse_output() const {
    if (!_impl) throw std::logic_error("NRD REBLUR is not initialized");
    return _impl->diffuse;
}
const Texture& NrdReblur::specular_output() const {
    if (!_impl) throw std::logic_error("NRD REBLUR is not initialized");
    return _impl->specular;
}
} // namespace rhi
