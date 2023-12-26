#include "vk_gbuffer_shading_graphics_pipeline.h"

#if GBUFFER_ON

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_initializers.h>

void VulkanGbufferShadingGraphicsPipeline::init(VulkanEngine* engine, const std::array<Texture, 4>& gbuffer)
{
	_engine = engine;

	init_description_set(gbuffer);
	init_scene_buffer(_engine->_renderables);
	init_bindless(_engine->_resManager.textureList);

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::GBufferShading,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect defaultEffect;
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("fullscreen.vert.spv")), VK_SHADER_STAGE_VERTEX_BIT);
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("gbuffer_shading.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);

				defaultEffect.reflect_layout(engine->_device, nullptr, 0);
				//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
				GraphicPipelineBuilder pipelineBuilder;

				pipelineBuilder.setShaders(&defaultEffect);

				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _bindlessSetLayout, _globalDescSetLayout, _gBufDescSetLayout };
				mesh_pipeline_layout_info.setLayoutCount = setLayout.size();
				mesh_pipeline_layout_info.pSetLayouts = setLayout.data();

				vkCreatePipelineLayout(_engine->_device, &mesh_pipeline_layout_info, nullptr, &pipelineBuilder._pipelineLayout);
				VkPipelineLayout meshPipLayout = pipelineBuilder._pipelineLayout;

				//vertex input controls how to read vertices from vertex buffers. We arent using it yet
				pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

				//input assembly is the configuration for drawing triangle lists, strips, or individual points.
				//we are just going to draw triangle list
				pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

				//build viewport and scissor from the swapchain extents
				pipelineBuilder._viewport.x = 0.0f;
				pipelineBuilder._viewport.y = 0.0f;
				pipelineBuilder._viewport.width = (float)engine->_windowExtent.width;
				pipelineBuilder._viewport.height = (float)engine->_windowExtent.height;
				pipelineBuilder._viewport.minDepth = 0.0f;
				pipelineBuilder._viewport.maxDepth = 1.0f;

				pipelineBuilder._scissor.offset = { 0, 0 };
				pipelineBuilder._scissor.extent = engine->_windowExtent;

				//configure the rasterizer to draw filled triangles
				pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
				pipelineBuilder._rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
				pipelineBuilder._rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

				//we dont use multisampling, so just run the default one
				pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

				//a single blend attachment with no blending and writing to RGBA
				pipelineBuilder._colorBlendAttachment.push_back(vkinit::color_blend_attachment_state());

				//default depthtesting
				pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

				pipelineBuilder.vertexDescription = Vertex::get_vertex_description();
				pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
				//connect the pipeline builder vertex input info to the one we get from Vertex
				pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = nullptr;
				pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = 0;

				pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = nullptr;
				pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = 0;

				VkPipeline meshPipeline = pipelineBuilder.build_graphic_pipeline(engine->_device,
					_engine->_renderPassManager.get_render_pass(ERenderPassType::Default)->get_render_pass());

				pipeline = meshPipeline;
				pipelineLayout = meshPipLayout;
			});
	}
}

void VulkanGbufferShadingGraphicsPipeline::copy_global_uniform_data(VulkanGbufferShadingGraphicsPipeline::SGlobalCamera& camData, int current_frame_index)
{
	_engine->map_buffer(_engine->_allocator, _globalCameraBuffer[current_frame_index]._allocation, [&](void*& data) {
		memcpy(data, &camData, sizeof(VulkanGbufferShadingGraphicsPipeline::SGlobalCamera));
		});
}

void VulkanGbufferShadingGraphicsPipeline::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	cmd->draw_quad([&](VkCommandBuffer cmd) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipeline(EPipelineType::GBufferShading));
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::GBufferShading), 0,
			1, &_bindlessSet, 0, nullptr);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::GBufferShading), 1,
			1, &_globalDescSet[current_frame_index], 0, nullptr);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::GBufferShading), 2,
			1, &_gBufDescSet[current_frame_index], 0, nullptr);
		}); 
}

void VulkanGbufferShadingGraphicsPipeline::init_description_set(const std::array<Texture, 4>& gbuffer)
{
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSamplerReductionModeCreateInfoEXT createInfoReduction = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

	createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;

	samplerInfo.pNext = &createInfoReduction;

	VkSampler sampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &sampler);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VkDescriptorImageInfo wposImageBufferInfo;
		wposImageBufferInfo.sampler = sampler;
		wposImageBufferInfo.imageView = gbuffer[int(EGbufferTex::WPOS)].imageView;
		wposImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo normalImageBufferInfo;
		normalImageBufferInfo.sampler = sampler;
		normalImageBufferInfo.imageView = gbuffer[int(EGbufferTex::NORM)].imageView;
		normalImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo uvImageBufferInfo;
		uvImageBufferInfo.sampler = sampler;
		uvImageBufferInfo.imageView = gbuffer[int(EGbufferTex::UV)].imageView;
		uvImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo objIDImageBufferInfo;
		objIDImageBufferInfo.sampler = sampler;
		objIDImageBufferInfo.imageView = gbuffer[int(EGbufferTex::OBJ_ID)].imageView;
		objIDImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_image(0, &wposImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.bind_image(1, &normalImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.bind_image(2, &uvImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.bind_image(3, &objIDImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(_gBufDescSet[i], _gBufDescSetLayout);
	}
}

void VulkanGbufferShadingGraphicsPipeline::init_scene_buffer(const std::vector<RenderObject>& renderables)
{
	_objectBuffer = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanGbufferShadingGraphicsPipeline::ObjectData) * renderables.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_globalCameraBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanGbufferGenerateGraphicsPipeline::SGlobalCamera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		VkDescriptorBufferInfo globalUniformsInfo;
		globalUniformsInfo.buffer = _globalCameraBuffer[i]._buffer;
		globalUniformsInfo.offset = 0;
		globalUniformsInfo.range = _globalCameraBuffer[i]._size;

		VkDescriptorBufferInfo objectBufferInfo;
		objectBufferInfo.buffer = _objectBuffer._buffer;
		objectBufferInfo.offset = 0;
		objectBufferInfo.range = _objectBuffer._size;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.bind_buffer(1, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(_globalDescSet[i], _globalDescSetLayout);
	}

	_engine->map_buffer(_engine->_allocator, _objectBuffer._allocation, [&](void*& data) {
		VulkanGbufferShadingGraphicsPipeline::ObjectData* objectSSBO = (VulkanGbufferShadingGraphicsPipeline::ObjectData*)data;

		for (int i = 0; i < renderables.size(); i++)
		{
			const RenderObject& object = renderables[i];
			objectSSBO[i].model = object.transformMatrix;
			objectSSBO[i].meshIndex = object.meshIndex;
			objectSSBO[i].diffuseTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->diffuseTextureIndex;
		}
		});
}

void VulkanGbufferShadingGraphicsPipeline::init_bindless(const std::vector<Texture*>& textureList)
{
	const uint32_t textureBinding = 0;

	//BIND SAMPLERS
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_LINEAR);

	VkSampler blockySampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &blockySampler);

	std::vector<VkDescriptorImageInfo> imageInfoList{};
	imageInfoList.resize(textureList.size());

	for (uint32_t textureArrayIndex = 0; textureArrayIndex < textureList.size(); textureArrayIndex++)
	{
		VkDescriptorImageInfo& imageBufferInfo = imageInfoList[textureArrayIndex];
		imageBufferInfo.sampler = blockySampler;
		imageBufferInfo.imageView = textureList[textureArrayIndex]->imageView;
		imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	vkutil::DescriptorBuilder::begin(_engine->_descriptorBindlessLayoutCache.get(), _engine->_descriptorBindlessAllocator.get())
		.bind_image(textureBinding, imageInfoList.data(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, imageInfoList.size())
		.build_bindless(_bindlessSet, _bindlessSetLayout);
}

#endif
