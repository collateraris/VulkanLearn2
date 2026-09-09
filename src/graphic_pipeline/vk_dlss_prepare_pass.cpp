#include <graphic_pipeline/vk_dlss_prepare_pass.h>

#include <rhi/vulkan_resources.h>
#include <vk_engine.h>
#include <vk_initializers.h>
#include <vk_render_graph.h>
#include <vk_shaders.h>

#include <stdexcept>

void VulkanDlssPreparePass::init(VulkanEngine* engine)
{
    _engine = engine;
    _extent = {engine->_renderExtent.width, engine->_renderExtent.height, 1};
    _worldPosition = engine->get_engine_texture(ETextureResourceNames::PT_GBUFFER_WPOS_OBJECT_ID);
    if (!_worldPosition || _worldPosition->extend.width != _extent.width || _worldPosition->extend.height != _extent.height)
        throw std::runtime_error("DLSS preparation requires render-resolution ReSTIR position guides");

    auto createImage = [&](rhi::Format format) {
        const auto image = engine->_rhi.resources().create_image({_extent.width, _extent.height, format,
            rhi::ImageUsage::Sampled | rhi::ImageUsage::Storage | rhi::ImageUsage::TransferSource});
        engine->_mainDeletionQueue.push_function([engine, image]() { engine->_rhi.resources().destroy(image); });
        return engine->_rhi.vulkan_resources().texture(image);
    };
    _color = createImage(rhi::Format::Rgba16Float);
    _depth = createImage(rhi::Format::R32Float);
    _motion = createImage(rhi::Format::Rg16Float);
    for (auto& uniform : _uniforms)
    {
        const auto buffer = engine->_rhi.resources().create_buffer({sizeof(Constants),
            rhi::BufferUsage::Uniform, rhi::MemoryUsage::Upload});
        uniform = engine->_rhi.vulkan_resources().buffer(buffer);
        engine->_mainDeletionQueue.push_function([engine, buffer]() { engine->_rhi.resources().destroy(buffer); });
    }

    std::array<VkDescriptorSetLayoutBinding, 6> bindings{};
    for (uint32_t i = 0; i < bindings.size(); ++i)
        bindings[i] = vkinit::descriptorset_layout_binding(i == 5 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
            (i < 2 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE), VK_SHADER_STAGE_COMPUTE_BIT, i);
    VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setInfo.bindingCount = uint32_t(bindings.size());
    setInfo.pBindings = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(engine->_device, &setInfo, nullptr, &_setLayout));
    engine->_mainDeletionQueue.push_function([device = engine->_device, layout = _setLayout]() {
        vkDestroyDescriptorSetLayout(device, layout, nullptr);
    });

    const std::array<VkDescriptorPoolSize, 3> poolSizes{{
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 6},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2}
    }};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = uint32_t(_sets.size());
    poolInfo.poolSizeCount = uint32_t(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    VK_CHECK(vkCreateDescriptorPool(engine->_device, &poolInfo, nullptr, &_pool));
    engine->_mainDeletionQueue.push_function([device = engine->_device, pool = _pool]() {
        vkDestroyDescriptorPool(device, pool, nullptr);
    });
    const std::array<VkDescriptorSetLayout, 2> layouts{_setLayout, _setLayout};
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool = _pool;
    allocation.descriptorSetCount = uint32_t(layouts.size());
    allocation.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(engine->_device, &allocation, _sets.data()));

    const std::array<const Texture*, 4> images{_worldPosition, &_color, &_depth, &_motion};
    for (uint32_t slot = 0; slot < _sets.size(); ++slot)
    {
        std::array<VkDescriptorImageInfo, 4> imageInfos{};
        std::array<VkWriteDescriptorSet, 5> writes{};
        for (uint32_t i = 0; i < images.size(); ++i)
        {
            imageInfos[i] = {VK_NULL_HANDLE, images[i]->imageView, VK_IMAGE_LAYOUT_GENERAL};
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = _sets[slot];
            writes[i].dstBinding = i + 1;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[i].pImageInfo = &imageInfos[i];
        }
        const VkDescriptorBufferInfo bufferInfo{_uniforms[slot]._buffer, 0, sizeof(Constants)};
        auto& uniformWrite = writes.back();
        uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniformWrite.dstSet = _sets[slot];
        uniformWrite.dstBinding = 5;
        uniformWrite.descriptorCount = 1;
        uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformWrite.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(engine->_device, uint32_t(writes.size()), writes.data(), 0, nullptr);
    }

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &_setLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &_pipelineLayout));
    engine->_mainDeletionQueue.push_function([device = engine->_device, layout = _pipelineLayout]() {
        vkDestroyPipelineLayout(device, layout, nullptr);
    });
    const auto shader = engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("dlss_prepare.comp.slang.spv"));
    VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shader->module;
    pipelineInfo.stage.pName = "main";
    VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline));
    engine->_mainDeletionQueue.push_function([device = engine->_device, pipeline = _pipeline]() {
        vkDestroyPipeline(device, pipeline, nullptr);
    });
}

void VulkanDlssPreparePass::append_passes(rg::RenderGraph& graph, int frameSlot, const Texture& currentHDR)
{
    if (frameSlot < 0 || size_t(frameSlot) >= _sets.size())
        throw std::out_of_range("DLSS preparation frame slot");
    if (currentHDR.extend.width != _extent.width || currentHDR.extend.height != _extent.height)
        throw std::runtime_error("DLSS color input must have the same render resolution as its guides");

    // The engine has waited this frame slot's fence. Rebinding this set is safe
    // when the user switches between accumulated and denoised HDR sources.
    if (_boundColorViews[frameSlot] != currentHDR.imageView)
    {
        const VkDescriptorImageInfo imageInfo{VK_NULL_HANDLE, currentHDR.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = _sets[frameSlot];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(_engine->_device, 1, &write, 0, nullptr);
        _boundColorViews[frameSlot] = currentHDR.imageView;
    }
    auto& camera = _engine->_camera;
    const auto view = camera.get_view_matrix();
    const auto projection = camera.get_projection_matrix(false);
    Constants constants;
    constants.currentViewProjection = projection * view;
    constants.previousViewProjection = camera.get_prev_projection_matrix(false) * camera.get_prev_view_matrix();
    constants.jitteredViewProjection = camera.get_projection_matrix() * view;
    constants.inverseProjection = glm::inverse(projection);
    constants.inverseView = glm::inverse(view);
    constants.extent = glm::uvec4(_extent.width, _extent.height, 0, 0);
    constants.jitterUv = camera.bUseJitter ? glm::vec4(camera.get_current_jitter(), 0.0f, 0.0f) : glm::vec4(0.0f);

    auto& device = _engine->_rhi;
    const auto uniformBuffer = device.buffer(_uniforms[frameSlot]);
    device.resources().write_buffer(uniformBuffer, &constants, sizeof(constants));
    const auto source = graph.import_resource("DLSS.SourceHDR", device.image(currentHDR), false);
    const auto position = graph.import_resource("DLSS.WorldPosition", device.image(*_worldPosition), false);
    const auto color = graph.import_resource("DLSS.InputColor", device.image(_color), false);
    const auto depth = graph.import_resource("DLSS.DeviceDepth", device.image(_depth), false);
    const auto motion = graph.import_resource("DLSS.MotionVectors", device.image(_motion), false);
    const auto uniforms = graph.import_resource("DLSS.PrepareConstants", uniformBuffer);
    const auto pipeline = device.pipeline(_pipeline, _pipelineLayout, VK_PIPELINE_BIND_POINT_COMPUTE);
    const auto descriptor = device.descriptor(_sets[frameSlot]);
    const uint32_t groupsX = (_extent.width + 15) / 16;
    const uint32_t groupsY = (_extent.height + 15) / 16;
    using rhi::Stage;
    using rhi::Access;
    using rhi::Layout;
    graph.add_pass("DLSS.PrepareInputs", {
        {source, {Stage::Compute, Access::ShaderRead, Layout::ShaderReadOnly}},
        {position, {Stage::Compute, Access::ShaderRead, Layout::General}},
        {uniforms, {Stage::Compute, Access::UniformRead, Layout::Undefined}},
        {color, {Stage::Compute, Access::ShaderWrite, Layout::General}},
        {depth, {Stage::Compute, Access::ShaderWrite, Layout::General}},
        {motion, {Stage::Compute, Access::ShaderWrite, Layout::General}}
    }, [pipeline, descriptor, groupsX, groupsY](rhi::CommandList& commands) {
        commands.bind_pipeline(pipeline);
        commands.bind_descriptor_set(pipeline, 0, descriptor);
        commands.dispatch(groupsX, groupsY, 1);
    });
}
