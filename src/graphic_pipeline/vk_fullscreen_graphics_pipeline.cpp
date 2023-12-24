#include "vk_fullscreen_graphics_pipeline.h"

#include <vk_engine.h>
#include <vk_command_buffer.h>
#include <vk_render_pass.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_initializers.h>

void VulkanFullScreenGraphicsPipeline::init(VulkanEngine* engine)
{
	_engine = engine;

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::FullScreen,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
					ShaderEffect defaultEffect;
					defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("fullscreen.vert.spv")), VK_SHADER_STAGE_VERTEX_BIT);
					defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("fullscreen.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);

					defaultEffect.reflect_layout(engine->_device, nullptr, 0);
					//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
					GraphicPipelineBuilder pipelineBuilder;

					pipelineBuilder.setShaders(&defaultEffect);

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
					pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

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

void VulkanFullScreenGraphicsPipeline::init_description_set(const Texture& inputTex)
{
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSamplerReductionModeCreateInfoEXT createInfoReduction = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

	createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;

	samplerInfo.pNext = &createInfoReduction;

	VkSampler sampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &sampler);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VkDescriptorImageInfo outImageBufferInfo;
		outImageBufferInfo.sampler = sampler;
		outImageBufferInfo.imageView = inputTex.imageView;
		outImageBufferInfo.imageLayout = inputTex.createInfo.initialLayout;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_image(0, &outImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(_descSet[i], _descSetLayout);
	}
}

void VulkanFullScreenGraphicsPipeline::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	cmd->draw_quad([&](VkCommandBuffer cmd) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipeline(EPipelineType::FullScreen));
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::FullScreen), 0,
			1, &_descSet[current_frame_index], 0, nullptr);
	});
}


