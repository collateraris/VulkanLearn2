#include <graphic_pipeline/vk_spatial_denoiser_pass.h>

#include <vk_command_buffer.h>
#include <vk_engine.h>
#include <vk_initializers.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_textures.h>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

void VulkanSpatialDenoiserPass::init(VulkanEngine* engine, const Texture& sourceAccumulated)
{
    _engine = engine;
    _imageExtent = sourceAccumulated.extend;
    if (const char* setting = std::getenv("RESTIR_DENOISER"))
        engine->_denoiserEnabled = std::strcmp(setting, "0") != 0;

    VulkanTextureBuilder builder;
    builder.init(engine);
    auto makeImage = [&]() {
        return builder.start()
            .make_img_info(VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, _imageExtent)
            .make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            .make_view_info(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT).create_texture();
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
    for (size_t i = 0; i < guides.size(); ++i) _gbufferImages[i] = guides[i]->image._image;
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
        _uniformBuffers[slot] = engine->create_cpu_to_gpu_buffer(sizeof(TemporalConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        engine->_mainDeletionQueue.push_function([engine, buffer = _uniformBuffers[slot]]() mutable {
            engine->destroy_buffer(engine->_allocator, buffer);
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

void VulkanSpatialDenoiserPass::draw(VulkanCommandBuffer* cmd, int current_frame_index)
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
    const glm::vec3 forward = glm::vec3(camera.get_rotation_matrix() * glm::vec4(0, 0, -1, 0));
    const bool still = view == _previousView && projection == _previousProjection;
    const float cutDistance = std::max(0.25f, glm::length(_engine->_resManager.maxCube - _engine->_resManager.minCube) * 0.025f);
    const bool cameraCut = projection != _previousProjection ||
        glm::distance(camera.position, _previousCameraPosition) > cutDistance || glm::dot(forward, _previousForward) < 0.8660254f;
    const bool valid = _historyValid && _wasEnabled && !cameraCut &&
        _lastAccumulationEnabled == _engine->_frameAccumulationEnabled;
    TemporalConstants temporal;
    temporal.previousViewProjection = _previousViewProjection;
    temporal.previousCameraPosition = glm::vec4(_previousCameraPosition, 1.0f);
    temporal.currentCameraPosition = glm::vec4(camera.position, 1.0f);
    temporal.frameInfo = glm::uvec4(_imageExtent.width, _imageExtent.height, valid ? 1u : 0u, still ? 1u : 0u);
    temporal.options.x = _engine->_frameAccumulationEnabled ? 1.0f : 0.0f;
    _engine->write_buffer(_engine->_allocator, _uniformBuffers[current_frame_index]._allocation, &temporal, sizeof(temporal));

    const VkCommandBuffer command = cmd->get_cmd();
    std::vector<VkImageMemoryBarrier> imageBarriers;
    if (!_imagesInitialized)
    {
        auto initialize = [&](const Texture& texture) {
            imageBarriers.push_back(vkinit::image_barrier(texture.image._image, 0,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT));
        };
        for (const auto& texture : _textures) initialize(texture);
        initialize(_prefiltered);
        for (const auto& slot : _history) for (const auto& texture : slot) initialize(texture);
    }
    else
        imageBarriers.push_back(vkinit::image_barrier(_textures[0].image._image,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT));
    VkMemoryBarrier memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    memory.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    memory.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &memory, 0, nullptr, uint32_t(imageBarriers.size()), imageBarriers.data());
    auto computeBarrier = [&]() {
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    };
    auto dispatch = [&]() { vkCmdDispatch(command, (_imageExtent.width + 15) / 16, (_imageExtent.height + 15) / 16, 1); };
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, _prefilterPipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, _pipelineLayout, 0, 1, &_prefilterSet, 0, nullptr);
    const PushConstants prefilter{_imageExtent.width, _imageExtent.height, 1, 0};
    vkCmdPushConstants(command, _pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(prefilter), &prefilter);
    dispatch();
    computeBarrier();
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        const PushConstants constants{_imageExtent.width, _imageExtent.height, 1u << pass, pass};
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, _pipelineLayout,
            0, 1, &_spatialSets[_historySlot][pass], 0, nullptr);
        vkCmdPushConstants(command, _pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(constants), &constants);
        dispatch();
        computeBarrier();
    }
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, _temporalPipeline);
    const std::array<VkDescriptorSet, 2> temporalSets{_temporalSets[_historySlot], _uniformSets[current_frame_index]};
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, _temporalPipelineLayout,
        0, uint32_t(temporalSets.size()), temporalSets.data(), 0, nullptr);
    dispatch();
    const VkImageMemoryBarrier output = vkinit::image_barrier(_textures[0].image._image,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &output);

    _imagesInitialized = true;
    _historyValid = true;
    _wasEnabled = true;
    _lastAccumulationEnabled = _engine->_frameAccumulationEnabled;
    _previousView = view;
    _previousProjection = projection;
    _previousViewProjection = camera.get_projection_matrix() * view;
    _previousCameraPosition = camera.position;
    _previousForward = forward;
    _historySlot = 1 - _historySlot;
}

const Texture& VulkanSpatialDenoiserPass::get_output() const
{
    return _textures[0];
}
