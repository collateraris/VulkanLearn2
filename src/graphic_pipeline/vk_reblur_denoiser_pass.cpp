#include <graphic_pipeline/vk_reblur_denoiser_pass.h>

#include <vk_engine.h>
#include <vk_render_graph.h>
#include <vk_initializers.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <rhi/vulkan_resources.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {
glm::mat4 nrd_projection(glm::mat4 projection) {
    // NRD uses D3D screen-UV conventions and [0,1] clip Z. Camera matrices use
    // RH [-1,1] clip Z with a Vulkan raster Y flip. NRD converts RH internally.
    for (uint32_t column = 0; column < 4; ++column) projection[column][1] *= -1.0f;
    glm::mat4 depthRange(1.0f);
    depthRange[2][2] = 0.5f;
    depthRange[3][2] = 0.5f;
    return depthRange * projection;
}
}

void ReblurDenoiserPass::init(VulkanEngine* engine, const Texture& diffuse,
    const Texture& specular, const Texture& bypass) {
    _engine = engine;
    _extent = diffuse.extend;
    _raw = {&diffuse, &specular, &bypass};
    if (const char* setting = std::getenv("RESTIR_DENOISER"))
        engine->_denoiserEnabled = std::strcmp(setting, "0") != 0;
    _guides = {
        engine->get_engine_texture(ETextureResourceNames::PT_GBUFFER_WPOS_OBJECT_ID),
        engine->get_engine_texture(ETextureResourceNames::PT_GBUFFER_NORMAL),
        engine->get_engine_texture(ETextureResourceNames::PT_GBUFFER_ALBEDO_METALNESS),
        engine->get_engine_texture(ETextureResourceNames::PT_GBUFFER_EMISSION_ROUGHNESS)};
    auto makeImage = [&](rhi::Format format) {
        const auto resource = engine->_rhi.resources().create_image({_extent.width, _extent.height, format,
            rhi::ImageUsage::Sampled | rhi::ImageUsage::Storage | rhi::ImageUsage::TransferSource});
        engine->_mainDeletionQueue.push_function([engine, resource]() { engine->_rhi.resources().destroy(resource); });
        return engine->_rhi.vulkan_resources().texture(resource);
    };
    const std::array<rhi::Format, 5> formats{rhi::Format::Rgba16Float, rhi::Format::Rgb10A2Unorm,
        rhi::Format::R32Float, rhi::Format::Rgba16Float, rhi::Format::Rgba16Float};
    for (size_t i = 0; i < formats.size(); ++i) _prepared[i] = makeImage(formats[i]);
    _output = makeImage(rhi::Format::Rgba32Float);
    _reblur.init(*engine, _extent.width, _extent.height);

    const std::array<VkDescriptorPoolSize, 3> poolSizes{{
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 30}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 12},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4}}};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 4;
    poolInfo.poolSizeCount = uint32_t(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    VK_CHECK(vkCreateDescriptorPool(engine->_device, &poolInfo, nullptr, &_pool));
    for (uint32_t slot = 0; slot < _uniforms.size(); ++slot) {
        const auto buffer = engine->_rhi.resources().create_buffer({sizeof(Constants), rhi::BufferUsage::Uniform, rhi::MemoryUsage::Upload});
        _uniforms[slot] = engine->_rhi.vulkan_resources().buffer(buffer);
        engine->_mainDeletionQueue.push_function([engine, buffer]() { engine->_rhi.resources().destroy(buffer); });
    }
    for (uint32_t pass = 0; pass < 2; ++pass) {
        const std::vector<const Texture*> images = pass == 0
            ? std::vector<const Texture*>{_raw[0], _raw[1], _guides[0], _guides[1], _guides[2], _guides[3],
                &_prepared[0], &_prepared[1], &_prepared[2], &_prepared[3], &_prepared[4]}
            : std::vector<const Texture*>{&_reblur.diffuse_output(), &_reblur.specular_output(), _raw[2],
                _raw[0], _raw[1], _guides[0], _guides[1], _guides[2], _guides[3], &_output};
        const uint32_t storageStart = pass == 0 ? 6 : 9;
        std::vector<VkDescriptorSetLayoutBinding> bindings(images.size() + 1);
        for (uint32_t i = 0; i < bindings.size(); ++i)
            bindings[i] = vkinit::descriptorset_layout_binding(i == images.size() ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
                (i >= storageStart ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE), VK_SHADER_STAGE_COMPUTE_BIT, i);
        VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setInfo.bindingCount = uint32_t(bindings.size());
        setInfo.pBindings = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(engine->_device, &setInfo, nullptr, &_setLayouts[pass]));
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &_setLayouts[pass];
        VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &_layouts[pass]));
        for (uint32_t slot = 0; slot < _uniforms.size(); ++slot) {
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocation.descriptorPool = _pool;
            allocation.descriptorSetCount = 1;
            allocation.pSetLayouts = &_setLayouts[pass];
            VK_CHECK(vkAllocateDescriptorSets(engine->_device, &allocation, &_sets[pass][slot]));
            std::vector<VkDescriptorImageInfo> infos(images.size());
            std::vector<VkWriteDescriptorSet> writes(bindings.size());
            VkDescriptorBufferInfo uniformInfo{_uniforms[slot]._buffer, 0, sizeof(Constants)};
            for (uint32_t i = 0; i < bindings.size(); ++i) {
                auto& write = writes[i];
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = _sets[pass][slot];
                write.dstBinding = i;
                write.descriptorCount = 1;
                write.descriptorType = bindings[i].descriptorType;
                if (i == images.size()) write.pBufferInfo = &uniformInfo;
                else {
                    infos[i] = {VK_NULL_HANDLE, images[i]->imageView, VK_IMAGE_LAYOUT_GENERAL};
                    write.pImageInfo = &infos[i];
                }
            }
            vkUpdateDescriptorSets(engine->_device, uint32_t(writes.size()), writes.data(), 0, nullptr);
        }
    }
    engine->_mainDeletionQueue.push_function([device = engine->_device, layouts = _layouts, setLayouts = _setLayouts, pool = _pool]() {
        for (auto layout : layouts) vkDestroyPipelineLayout(device, layout, nullptr);
        vkDestroyDescriptorPool(device, pool, nullptr);
        for (auto layout : setLayouts) vkDestroyDescriptorSetLayout(device, layout, nullptr);
    });
    const std::array<const char*, 2> shaders{"reblur_prepare.comp.slang.spv", "reblur_composite.comp.slang.spv"};
    for (uint32_t pass = 0; pass < 2; ++pass) {
        ShaderEffect effect;
        effect.add_stage(engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path(shaders[pass])), VK_SHADER_STAGE_COMPUTE_BIT);
        effect.reflect_layout(engine->_device, nullptr, 0);
        ComputePipelineBuilder builder;
        builder.setShaders(&effect);
        builder._pipelineLayout = _layouts[pass];
        _pipelines[pass] = builder.build_compute_pipeline(engine->_device);
        if (!_pipelines[pass]) throw std::runtime_error("Failed to create REBLUR preparation/composition pipeline");
        engine->_mainDeletionQueue.push_function([device = engine->_device, pipeline = _pipelines[pass]]() { vkDestroyPipeline(device, pipeline, nullptr); });
    }
}

void ReblurDenoiserPass::append_passes(rg::RenderGraph& graph, uint32_t frameSlot) {
    if (!_engine->_denoiserEnabled) { _wasEnabled = false; reset_history(); return; }
    auto& camera = _engine->_camera;
    const glm::mat4 view = camera.get_view_matrix();
    const glm::mat4 projection = camera.get_projection_matrix(false);
    const glm::vec3 position = camera.position;
    const glm::vec3 forward = glm::vec3(camera.get_rotation_matrix() * glm::vec4(0, 0, -1, 0));
    const glm::vec2 jitter = camera.bUseJitter ? -camera.get_current_jitter() * glm::vec2(_extent.width, _extent.height) : glm::vec2(0.0f);
    const bool accumulation = _engine->_frameAccumulationEnabled;
    const float sceneDiagonal = glm::length(_engine->_resManager.maxCube - _engine->_resManager.minCube);
    const bool cut = projection != _previousProjection || glm::distance(position, _previousPosition) > std::max(0.25f, sceneDiagonal * 0.025f)
        || glm::dot(forward, _previousForward) < 0.8660254f;
    const bool valid = _historyValid && _wasEnabled && !cut && _lastAccumulation == accumulation;

    nrd::CommonSettings common{};
    const auto nrdProjection = nrd_projection(projection);
    const auto previousProjection = valid ? nrd_projection(_previousProjection) : nrdProjection;
    std::memcpy(common.viewToClipMatrix, &nrdProjection, sizeof(glm::mat4));
    std::memcpy(common.viewToClipMatrixPrev, &previousProjection, sizeof(glm::mat4));
    std::memcpy(common.worldToViewMatrix, &view, sizeof(glm::mat4));
    std::memcpy(common.worldToViewMatrixPrev, valid ? &_previousView : &view, sizeof(glm::mat4));
    for (uint32_t axis = 0; axis < 2; ++axis) {
        common.cameraJitter[axis] = jitter[axis];
        common.cameraJitterPrev[axis] = valid ? _previousJitter[axis] : jitter[axis];
        const uint16_t size = uint16_t(axis == 0 ? _extent.width : _extent.height);
        common.resourceSize[axis] = common.resourceSizePrev[axis] = common.rectSize[axis] = common.rectSizePrev[axis] = size;
    }
    common.isMotionVectorInWorldSpace = true;
    common.motionVectorScale[0] = common.motionVectorScale[1] = common.motionVectorScale[2] = 1.0f;
    common.frameIndex = _frameIndex;
    common.accumulationMode = valid ? nrd::AccumulationMode::CONTINUE : nrd::AccumulationMode::CLEAR_AND_RESTART;
    nrd::ReblurSettings settings{};
    // Assets use different units (OBJ centimetres and glTF metres). This is a
    // scene-relative filtering length, not a radiance/exposure multiplier.
    settings.hitDistanceParameters.A = std::max(0.001f, sceneDiagonal * 0.01f);
    settings.minHitDistanceWeight = 0.05f; // ReSTIR reservoirs have correlated guides.
    settings.hitDistanceReconstructionMode = nrd::HitDistanceReconstructionMode::AREA_5X5;
    settings.enableAntiFirefly = true;
    // Keep the history near half a second in native and DLSS modes instead of
    // retaining the same frame count at very different rendering rates.
    const float frameMs = _engine->_stats.frameCpuAvg > 0.0 ? float(_engine->_stats.frameCpuAvg) : 1000.0f / 60.0f;
    settings.maxAccumulatedFrameNum = std::clamp(nrd::GetMaxAccumulatedFrameNum(nrd::REBLUR_DEFAULT_ACCUMULATION_TIME,
        1000.0f / std::max(1.0f, frameMs)), 1u, nrd::REBLUR_MAX_HISTORY_FRAME_NUM);
    settings.maxFastAccumulatedFrameNum = std::max(1u, settings.maxAccumulatedFrameNum / 5);
    settings.maxStabilizedFrameNum = settings.maxAccumulatedFrameNum;
    settings.historyFixFrameNum = std::min(3u, settings.maxFastAccumulatedFrameNum - 1);
    settings.responsiveAccumulationSettings.minAccumulatedFrameNum = settings.historyFixFrameNum;
    if (!accumulation) {
        settings.maxAccumulatedFrameNum = 0;
        settings.maxFastAccumulatedFrameNum = 0;
        settings.maxStabilizedFrameNum = 0;
        settings.historyFixFrameNum = 0;
        settings.responsiveAccumulationSettings.minAccumulatedFrameNum = 0;
    }
    Constants constants;
    constants.worldToView = view;
    constants.cameraPosition = glm::vec4(position, 1.0f);
    constants.extent = glm::uvec4(_extent.width, _extent.height, 0, 0);
    constants.hitDistanceAndRange = glm::vec4(settings.hitDistanceParameters.A, settings.hitDistanceParameters.B,
        settings.hitDistanceParameters.C, common.denoisingRange);
    auto& device = _engine->_rhi;
    device.resources().write_buffer(device.buffer(_uniforms[frameSlot]), &constants, sizeof(constants));
    using rhi::Stage; using rhi::Access; using rhi::Layout;
    const rhi::ResourceState read{Stage::Compute, Access::ShaderRead, Layout::General};
    const rhi::ResourceState write{Stage::Compute, Access::ShaderWrite, Layout::General};
    std::vector<rg::Use> prepareUses, compositeUses;
    auto addImage = [&](std::vector<rg::Use>& uses, const std::string& name, const Texture& texture, bool output) {
        uses.push_back({graph.import_resource(name, device.image(texture), false), output ? write : read});
    };
    for (uint32_t i = 0; i < _raw.size(); ++i) {
        if (i < 2) addImage(prepareUses, "REBLUR.Raw" + std::to_string(i), *_raw[i], false);
        addImage(compositeUses, "REBLUR.Raw" + std::to_string(i), *_raw[i], false);
    }
    for (uint32_t i = 0; i < _guides.size(); ++i) {
        addImage(prepareUses, "REBLUR.Guide" + std::to_string(i), *_guides[i], false);
        addImage(compositeUses, "REBLUR.Guide" + std::to_string(i), *_guides[i], false);
    }
    for (uint32_t i = 0; i < _prepared.size(); ++i)
        addImage(prepareUses, "REBLUR.Prepared" + std::to_string(i), _prepared[i], true);
    const auto uniforms = graph.import_resource("REBLUR.PrepareConstants", device.buffer(_uniforms[frameSlot]));
    const rg::Use uniformUse{uniforms, {Stage::Compute, Access::UniformRead, Layout::Undefined}};
    prepareUses.push_back(uniformUse); compositeUses.push_back(uniformUse);
    const uint32_t groupsX = (_extent.width + 15) / 16, groupsY = (_extent.height + 15) / 16;
    const auto preparePipeline = device.pipeline(_pipelines[0], _layouts[0], VK_PIPELINE_BIND_POINT_COMPUTE);
    const auto prepareSet = device.descriptor(_sets[0][frameSlot]);
    graph.add_pass("REBLUR.Prepare", std::move(prepareUses), [=](rhi::CommandList& commands) {
        commands.bind_pipeline(preparePipeline);
        commands.bind_descriptor_set(preparePipeline, 0, prepareSet);
        commands.dispatch(groupsX, groupsY, 1);
    });
    const rhi::NrdReblurInputs inputs{&_prepared[0], &_prepared[1], &_prepared[2], &_prepared[3], &_prepared[4]};
    _reblur.append_passes(graph, frameSlot, inputs, common, settings);
    addImage(compositeUses, "REBLUR.FilteredDiffuse", _reblur.diffuse_output(), false);
    addImage(compositeUses, "REBLUR.FilteredSpecular", _reblur.specular_output(), false);
    addImage(compositeUses, "REBLUR.Output", _output, true);
    const auto compositePipeline = device.pipeline(_pipelines[1], _layouts[1], VK_PIPELINE_BIND_POINT_COMPUTE);
    const auto compositeSet = device.descriptor(_sets[1][frameSlot]);
    graph.add_pass("REBLUR.Composite", std::move(compositeUses), [=, this](rhi::CommandList& commands) {
        commands.bind_pipeline(compositePipeline);
        commands.bind_descriptor_set(compositePipeline, 0, compositeSet);
        commands.dispatch(groupsX, groupsY, 1);
        _historyValid = _wasEnabled = true;
        _lastAccumulation = accumulation;
        _previousView = view; _previousProjection = projection;
        _previousPosition = position; _previousForward = forward; _previousJitter = jitter;
        ++_frameIndex;
    });
}
