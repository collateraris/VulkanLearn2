#include <graphic_pipeline/vk_spatial_denoiser_pass.h>

#include <vk_render_graph.h>
#include <vk_engine.h>
#include <vk_initializers.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <rhi/vulkan_resources.h>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

void VulkanSpatialDenoiserPass::init(VulkanEngine* engine, const Texture& sourceAccumulated)
{
    _engine = engine;
    _sourceTexture = &sourceAccumulated;
    _imageExtent = sourceAccumulated.extend;
    if (const char* setting = std::getenv("RESTIR_DENOISER"))
        engine->_denoiserEnabled = std::strcmp(setting, "0") != 0;

    auto makeImage = [&]() {
        const auto image = engine->_rhi.resources().create_image({
            _imageExtent.width, _imageExtent.height, rhi::Format::Rgba32Float,
            rhi::ImageUsage::Sampled | rhi::ImageUsage::Storage | rhi::ImageUsage::TransferSource});
        engine->_mainDeletionQueue.push_function([engine, image]() { engine->_rhi.resources().destroy(image); });
        // Native descriptors remain explicit compatibility setup outside the graph.
        return engine->_rhi.vulkan_resources().texture(image);
    };
    for (auto& texture : _textures) texture = makeImage();
    _prefiltered = makeImage();
    for (auto& slot : _history)
        for (auto& texture : slot) texture = makeImage();

    auto makeSetLayout = [&](uint32_t count, bool temporal, bool uniform) {
        std::vector<VkDescriptorSetLayoutBinding> bindings(count);
        for (uint32_t i = 0; i < count; ++i)
            bindings[i] = vkinit::descriptorset_layout_binding(uniform ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
                (i == 1 || (temporal && i >= 9) ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE),
                VK_SHADER_STAGE_COMPUTE_BIT, i);
        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = count;
        info.pBindings = bindings.data();
        VkDescriptorSetLayout result;
        VK_CHECK(vkCreateDescriptorSetLayout(engine->_device, &info, nullptr, &result));
        return result;
    };
    _descriptorSetLayout = makeSetLayout(6, false, false);
    _temporalSetLayout = makeSetLayout(12, true, false);
    _uniformSetLayout = makeSetLayout(1, false, true);

    const std::array<VkDescriptorPoolSize, 3> poolSizes{{
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 41}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 13},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2}
    }};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 9;
    poolInfo.poolSizeCount = uint32_t(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    VK_CHECK(vkCreateDescriptorPool(engine->_device, &poolInfo, nullptr, &_descriptorPool));
    auto allocateSet = [&](VkDescriptorSetLayout layout) {
        VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        info.descriptorPool = _descriptorPool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &layout;
        VkDescriptorSet result;
        VK_CHECK(vkAllocateDescriptorSets(engine->_device, &info, &result));
        return result;
    };
    const std::array<const Texture*, 4> guides{
        engine->get_engine_texture(ETextureResourceNames::PT_GBUFFER_WPOS_OBJECT_ID),
        engine->get_engine_texture(ETextureResourceNames::PT_GBUFFER_NORMAL),
        engine->get_engine_texture(ETextureResourceNames::PT_GBUFFER_ALBEDO_METALNESS),
        engine->get_engine_texture(ETextureResourceNames::PT_GBUFFER_EMISSION_ROUGHNESS)
    };
    _guides = guides;
    auto writeImages = [&](VkDescriptorSet set, const std::vector<const Texture*>& textures, bool accumulatedInput) {
        std::vector<VkDescriptorImageInfo> infos(textures.size());
        std::vector<VkWriteDescriptorSet> writes(textures.size());
        for (uint32_t i = 0; i < textures.size(); ++i)
        {
            infos[i] = {VK_NULL_HANDLE, textures[i]->imageView,
                accumulatedInput && i == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL};
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = set;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = i == 1 || i >= 9 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[i].pImageInfo = &infos[i];
        }
        vkUpdateDescriptorSets(engine->_device, uint32_t(writes.size()), writes.data(), 0, nullptr);
    };
    _prefilterSet = allocateSet(_descriptorSetLayout);
    writeImages(_prefilterSet, {&sourceAccumulated, &_prefiltered, guides[0], guides[1], guides[2], guides[3]}, true);
    for (uint32_t slot = 0; slot < 2; ++slot)
    {
        _temporalSets[slot] = allocateSet(_temporalSetLayout);
        const auto& previous = _history[1 - slot];
        auto& current = _history[slot];
        writeImages(_temporalSets[slot], {&_textures[1], &current[0], guides[0], guides[1], guides[2], guides[3],
            &previous[0], &previous[1], &previous[2], &current[1], &current[2], &_textures[0]}, false);
        // Spatial filtering always starts from this frame's cleaned source.
        // Temporal history never re-enters these passes or accumulates blur.
        _spatialSets[slot][0] = allocateSet(_descriptorSetLayout);
        writeImages(_spatialSets[slot][0], {&_prefiltered, &_textures[0], guides[0], guides[1], guides[2], guides[3]}, false);
        _spatialSets[slot][1] = allocateSet(_descriptorSetLayout);
        writeImages(_spatialSets[slot][1], {&_textures[0], &_textures[1], guides[0], guides[1], guides[2], guides[3]}, false);

        _uniformSets[slot] = allocateSet(_uniformSetLayout);
        const auto buffer = engine->_rhi.resources().create_buffer({
            sizeof(TemporalConstants), rhi::BufferUsage::Uniform, rhi::MemoryUsage::Upload});
        _uniformBuffers[slot] = engine->_rhi.vulkan_resources().buffer(buffer);
        engine->_mainDeletionQueue.push_function([engine, buffer]() {
            engine->_rhi.resources().destroy(buffer);
        });
        VkDescriptorBufferInfo bufferInfo{_uniformBuffers[slot]._buffer, 0, sizeof(TemporalConstants)};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = _uniformSets[slot];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(engine->_device, 1, &write, 0, nullptr);
    }

    VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &_descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &_pipelineLayout));
    const std::array<VkDescriptorSetLayout, 2> temporalLayouts{_temporalSetLayout, _uniformSetLayout};
    layoutInfo.setLayoutCount = uint32_t(temporalLayouts.size());
    layoutInfo.pSetLayouts = temporalLayouts.data();
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;
    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &_temporalPipelineLayout));
    auto makePipeline = [&](const char* shader, VkPipelineLayout layout) {
        ShaderEffect effect;
        effect.add_stage(engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path(shader)), VK_SHADER_STAGE_COMPUTE_BIT);
        effect.reflect_layout(engine->_device, nullptr, 0);
        ComputePipelineBuilder pipelineBuilder;
        pipelineBuilder.setShaders(&effect);
        pipelineBuilder._pipelineLayout = layout;
        VkPipeline pipeline = pipelineBuilder.build_compute_pipeline(engine->_device);
        if (!pipeline) throw std::runtime_error("Failed to create denoiser pipeline");
        engine->_mainDeletionQueue.push_function([device = engine->_device, pipeline]() { vkDestroyPipeline(device, pipeline, nullptr); });
        return pipeline;
    };
    // Layouts/pool must outlive all pipelines; deletion queue is LIFO.
    engine->_mainDeletionQueue.push_function([device = engine->_device, spatialLayout = _pipelineLayout,
        temporalLayout = _temporalPipelineLayout, pool = _descriptorPool, spatialSet = _descriptorSetLayout,
        temporalSet = _temporalSetLayout, uniformSet = _uniformSetLayout]() {
        vkDestroyPipelineLayout(device, spatialLayout, nullptr);
        vkDestroyPipelineLayout(device, temporalLayout, nullptr);
        vkDestroyDescriptorPool(device, pool, nullptr);
        vkDestroyDescriptorSetLayout(device, spatialSet, nullptr);
        vkDestroyDescriptorSetLayout(device, temporalSet, nullptr);
        vkDestroyDescriptorSetLayout(device, uniformSet, nullptr);
    });
    _prefilterPipeline = makePipeline("denoise_prefilter.comp.slang.spv", _pipelineLayout);
    _temporalPipeline = makePipeline("denoise_temporal.comp.slang.spv", _temporalPipelineLayout);
    _pipeline = makePipeline("spatial_denoise.comp.slang.spv", _pipelineLayout);
}

void VulkanSpatialDenoiserPass::reset_history()
{
    _historyValid = false;
}

void VulkanSpatialDenoiserPass::append_passes(rg::RenderGraph& graph, int frameSlot)
{
    if (!_engine->_denoiserEnabled)
    {
        _wasEnabled = false;
        reset_history();
        return;
    }
    auto& camera = _engine->_camera;
    const glm::mat4 view = camera.get_view_matrix();
    const glm::mat4 projection = camera.get_projection_matrix(false);
    const glm::mat4 viewProjection = camera.get_projection_matrix() * view;
    const glm::vec3 position = camera.position;
    const glm::vec3 forward = glm::vec3(camera.get_rotation_matrix() * glm::vec4(0, 0, -1, 0));
    const bool accumulationEnabled = _engine->_frameAccumulationEnabled;
    const uint32_t historySlot = _historySlot;
    const bool still = view == _previousView && projection == _previousProjection;
    const float cutDistance = std::max(0.25f, glm::length(_engine->_resManager.maxCube - _engine->_resManager.minCube) * 0.025f);
    const bool cameraCut = projection != _previousProjection ||
        glm::distance(camera.position, _previousCameraPosition) > cutDistance || glm::dot(forward, _previousForward) < 0.8660254f;
    const bool valid = _historyValid && _wasEnabled && !cameraCut &&
        _lastAccumulationEnabled == accumulationEnabled;
    TemporalConstants temporal;
    temporal.previousViewProjection = _previousViewProjection;
    temporal.previousCameraPosition = glm::vec4(_previousCameraPosition, 1.0f);
    temporal.currentCameraPosition = glm::vec4(camera.position, 1.0f);
    temporal.frameInfo = glm::uvec4(_imageExtent.width, _imageExtent.height, valid ? 1u : 0u, still ? 1u : 0u);
    temporal.options.x = accumulationEnabled ? 1.0f : 0.0f;
    // The frame slot's fence has completed before graph registration.
    _engine->_rhi.resources().write_buffer(_engine->_rhi.buffer(_uniformBuffers[frameSlot]), &temporal, sizeof(temporal));

    auto& device = _engine->_rhi;
    const auto source = graph.import_resource("Denoiser.Source", device.image(*_sourceTexture));
    const auto prefiltered = graph.import_resource("Denoiser.Prefiltered", device.image(_prefiltered), false);
    const auto imageA = graph.import_resource("Denoiser.A", device.image(_textures[0]), false);
    const auto imageB = graph.import_resource("Denoiser.B", device.image(_textures[1]), false);
    const auto uniforms = graph.import_resource("Denoiser.Uniforms", device.buffer(_uniformBuffers[frameSlot]));
    using rhi::Stage;
    using rhi::Access;
    using rhi::Layout;
    const rhi::ResourceState sampled{Stage::Compute, Access::ShaderRead, Layout::General};
    const rhi::ResourceState storage{Stage::Compute, Access::ShaderWrite, Layout::General};
    std::vector<rg::Use> guideReads;
    const std::array<const char*, 4> guideNames{"Position", "Normal", "Albedo", "Emission"};
    for (size_t i = 0; i < _guides.size(); ++i)
    {
        const auto guide = graph.import_resource(std::string("Denoiser.GBuffer.") + guideNames[i], device.image(*_guides[i]), false);
        guideReads.push_back({guide, sampled});
    }
    const auto guidedPass = [&](rg::ResourceHandle input, rg::ResourceHandle output, Layout inputLayout = Layout::General) {
        auto uses = guideReads;
        uses.push_back({input, {Stage::Compute, Access::ShaderRead, inputLayout}});
        uses.push_back({output, storage});
        return uses;
    };
    const uint32_t groupsX = (_imageExtent.width + 15) / 16;
    const uint32_t groupsY = (_imageExtent.height + 15) / 16;
    const auto prefilterPipeline = device.pipeline(_prefilterPipeline, _pipelineLayout, VK_PIPELINE_BIND_POINT_COMPUTE);
    const auto prefilterSet = device.descriptor(_prefilterSet);
    const PushConstants prefilter{_imageExtent.width, _imageExtent.height, 1, 0};
    graph.add_pass("Denoiser.Prefilter", guidedPass(source, prefiltered, Layout::ShaderReadOnly),
        [prefilterPipeline, prefilterSet, prefilter, groupsX, groupsY](rhi::CommandList& cmd) {
            cmd.bind_pipeline(prefilterPipeline);
            cmd.bind_descriptor_set(prefilterPipeline, 0, prefilterSet);
            cmd.push_constants(prefilterPipeline, Stage::Compute, &prefilter, sizeof(prefilter));
            cmd.dispatch(groupsX, groupsY, 1);
        });

    const auto spatialPipeline = device.pipeline(_pipeline, _pipelineLayout, VK_PIPELINE_BIND_POINT_COMPUTE);
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        const auto spatialSet = device.descriptor(_spatialSets[historySlot][pass]);
        const PushConstants constants{_imageExtent.width, _imageExtent.height, 1u << pass, pass};
        graph.add_pass(std::string("Denoiser.Spatial") + std::to_string(pass),
            guidedPass(pass == 0 ? prefiltered : imageA, pass == 0 ? imageA : imageB),
            [spatialPipeline, spatialSet, constants, groupsX, groupsY](rhi::CommandList& cmd) {
                cmd.bind_pipeline(spatialPipeline);
                cmd.bind_descriptor_set(spatialPipeline, 0, spatialSet);
                cmd.push_constants(spatialPipeline, Stage::Compute, &constants, sizeof(constants));
                cmd.dispatch(groupsX, groupsY, 1);
            });
    }

    auto temporalUses = guidedPass(imageB, imageA);
    temporalUses.push_back({uniforms, {Stage::Compute, Access::UniformRead, Layout::Undefined}});
    const std::array<const char*, 3> historyNames{"Color", "Position", "NormalRoughness"};
    for (uint32_t i = 0; i < historyNames.size(); ++i)
    {
        // Initial invalid history is never sampled by the shader. Import it
        // as available so the graph can establish its descriptor's layout.
        const auto previous = graph.import_resource("Denoiser.History" + std::to_string(1 - historySlot) + "." + historyNames[i],
            device.image(_history[1 - historySlot][i]));
        const auto current = graph.import_resource("Denoiser.History" + std::to_string(historySlot) + "." + historyNames[i],
            device.image(_history[historySlot][i]));
        temporalUses.push_back({previous, sampled});
        temporalUses.push_back({current, storage});
    }
    const auto temporalPipeline = device.pipeline(_temporalPipeline, _temporalPipelineLayout, VK_PIPELINE_BIND_POINT_COMPUTE);
    const auto temporalSet = device.descriptor(_temporalSets[historySlot]);
    const auto uniformSet = device.descriptor(_uniformSets[frameSlot]);
    graph.add_pass("Denoiser.Temporal", std::move(temporalUses), [=, this](rhi::CommandList& cmd) {
        cmd.bind_pipeline(temporalPipeline);
        cmd.bind_descriptor_set(temporalPipeline, 0, temporalSet);
        cmd.bind_descriptor_set(temporalPipeline, 1, uniformSet);
        cmd.dispatch(groupsX, groupsY, 1);

        // Commit only when this frame's temporal dispatch is recorded. Spatial
        // passes always start from the new prefilter, never from this history.
        _historyValid = true;
        _wasEnabled = true;
        _lastAccumulationEnabled = accumulationEnabled;
        _previousView = view;
        _previousProjection = projection;
        _previousViewProjection = viewProjection;
        _previousCameraPosition = position;
        _previousForward = forward;
        _historySlot = 1 - historySlot;
    });
}

const Texture& VulkanSpatialDenoiserPass::get_output() const
{
    return _textures[0];
}
