#include "vk_render_pipeline.h"

#include <vk_material_system.h>
#include <vk_render_pass.h>
#include <vk_shaders.h>
#include <vk_engine.h>
#include <vk_initializers.h>

VulkanRenderPipeline::~VulkanRenderPipeline()
{
	vkDestroyPipeline(_engine->_device, _pipeline, nullptr);

	vkDestroyPipelineLayout(_engine->_device, _pipelineLayout, nullptr);
}

void VulkanRenderPipeline::init(VulkanEngine* engine, EPipelineType type, std::function<void(VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)>&& func)
{
	_engine = engine;
	_type = type;

	func(_pipeline, _pipelineLayout);
}

EPipelineType VulkanRenderPipeline::get_type() const
{
	return _type;
}

VkPipeline VulkanRenderPipeline::get_pipeline() const
{
	return _pipeline;
}

VkPipelineLayout VulkanRenderPipeline::get_pipelineLayout() const
{
	return _pipelineLayout;
}

void VulkanRenderPipelineManager::init(VulkanEngine* engine, VulkanRenderPassManager* renderPassManager, ShaderCache* shaderCache)
{
	_pipelinesList.clear();
	_pipelinesList.resize(static_cast<uint32_t>(EPipelineType::Max));

	//EPipelineType::Bindless_TaskMeshIndirectForward
	{
#if MESHSHADER_ON
		//_pipelinesList[static_cast<uint32_t>(EPipelineType::Bindless_TaskMeshIndirectForward)]
		//	.init(EPipelineType::Bindless_TaskMeshIndirectForward,
		//		[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
		//			ShaderEffect defaultEffect;
		//			defaultEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("tri_mesh.task.spv")), VK_SHADER_STAGE_TASK_BIT_NV);
		//			defaultEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("tri_mesh.mesh.spv")), VK_SHADER_STAGE_MESH_BIT_NV);
		//			defaultEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("tri_mesh.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);

		//			defaultEffect.reflect_layout(engine->_device, nullptr, 0);
		//			build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
		//			GraphicPipelineBuilder pipelineBuilder;

		//			pipelineBuilder.setShaders(&defaultEffect);

		//			VkPipelineLayout meshPipLayout = pipelineBuilder._pipelineLayout;

		//			vertex input controls how to read vertices from vertex buffers. We arent using it yet
		//			pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

		//			input assembly is the configuration for drawing triangle lists, strips, or individual points.
		//			we are just going to draw triangle list
		//			pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		//			build viewport and scissor from the swapchain extents
		//			pipelineBuilder._viewport.x = 0.0f;
		//			pipelineBuilder._viewport.y = 0.0f;
		//			pipelineBuilder._viewport.width = (float)engine->_windowExtent.width;
		//			pipelineBuilder._viewport.height = (float)engine->_windowExtent.height;
		//			pipelineBuilder._viewport.minDepth = 0.0f;
		//			pipelineBuilder._viewport.maxDepth = 1.0f;

		//			pipelineBuilder._scissor.offset = { 0, 0 };
		//			pipelineBuilder._scissor.extent = engine->_windowExtent;

		//			configure the rasterizer to draw filled triangles
		//			pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

		//			we dont use multisampling, so just run the default one
		//			pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

		//			a single blend attachment with no blending and writing to RGBA
		//			pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

		//			default depthtesting
		//			pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
		//		
		//			VkPipeline meshPipeline = pipelineBuilder.build_graphic_pipeline(engine->_device,
		//				renderPassManager->get_render_pass(ERenderPassType::Default)->get_render_pass());

		//			pipeline = meshPipeline;
		//			pipelineLayout = meshPipLayout;
		//		});
#endif
	}

	//EPipelineType::DrawIndirectForward
	{
#if !MESHSHADER_ON
		_pipelinesList[static_cast<uint32_t>(EPipelineType::DrawIndirectForward)]
			.init(engine, EPipelineType::DrawIndirectForward,
				[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
					ShaderEffect defaultEffect;
					defaultEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("tri_mesh.vert.spv")), VK_SHADER_STAGE_VERTEX_BIT);
					defaultEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("triangle.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);

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

					//we dont use multisampling, so just run the default one
					pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

					//a single blend attachment with no blending and writing to RGBA
					pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

					//default depthtesting
					pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

					pipelineBuilder.vertexDescription = Vertex::get_vertex_description();
					pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
					//connect the pipeline builder vertex input info to the one we get from Vertex
					pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = pipelineBuilder.vertexDescription.attributes.data();
					pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)pipelineBuilder.vertexDescription.attributes.size();

					pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = pipelineBuilder.vertexDescription.bindings.data();
					pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)pipelineBuilder.vertexDescription.bindings.size();

					VkPipeline meshPipeline = pipelineBuilder.build_graphic_pipeline(engine->_device,
						renderPassManager->get_render_pass(ERenderPassType::Default)->get_render_pass());

					pipeline = meshPipeline;
					pipelineLayout = meshPipLayout;
				});
#endif
	}

	//EPipelineType::PyramidDepthReduce
	{
		_pipelinesList[static_cast<uint32_t>(EPipelineType::PyramidDepthReduce)]
			.init(engine, EPipelineType::PyramidDepthReduce,
				[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
					ShaderEffect computeEffect;
					computeEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("depthreduce.comp.spv")), VK_SHADER_STAGE_COMPUTE_BIT);
					computeEffect.reflect_layout(engine->_device, nullptr, 0);

					ComputePipelineBuilder computePipelineBuilder;
					computePipelineBuilder.setShaders(&computeEffect);
					//hook the push constants layout
					pipelineLayout = computePipelineBuilder._pipelineLayout;

					pipeline = computePipelineBuilder.build_compute_pipeline(engine->_device);
				});
	}

	//EPipelineType::ComputePrepassForTaskMeshIndirect
	{
		_pipelinesList[static_cast<uint32_t>(EPipelineType::ComputePrepassForTaskMeshIndirect)]
			.init(engine, EPipelineType::ComputePrepassForTaskMeshIndirect,
				[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
					ShaderEffect computeEffect;
					computeEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("drawcmd.comp.spv")), VK_SHADER_STAGE_COMPUTE_BIT);
					computeEffect.reflect_layout(engine->_device, nullptr, 0);

					ComputePipelineBuilder computePipelineBuilder;
					computePipelineBuilder.setShaders(&computeEffect);
					//hook the push constants layout
					pipelineLayout = computePipelineBuilder._pipelineLayout;

					pipeline = computePipelineBuilder.build_compute_pipeline(engine->_device);
				});
	}

	//EPipelineType::BaseRaytracer
	{
#if RAYTRACER_ON
		_pipelinesList[static_cast<uint32_t>(EPipelineType::BaseRaytracer)]
			.init(engine, EPipelineType::BaseRaytracer,
				[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
					ShaderEffect defaultEffect;
					uint32_t rayGenIndex = defaultEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("raytrace.rgen.spv")), VK_SHADER_STAGE_RAYGEN_BIT_NV);
					uint32_t rayMissIndex = defaultEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("raytrace.rmiss.spv")), VK_SHADER_STAGE_MISS_BIT_NV);
					uint32_t rayClosestHitIndex = defaultEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("raytrace.rchit.spv")), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
					defaultEffect.reflect_layout(engine->_device, nullptr, 0);

					RTPipelineBuilder pipelineBuilder;

					pipelineBuilder.setShaders(&defaultEffect);

					// The ray tracing process can shoot rays from the camera, and a shadow ray can be shot from the
					// hit points of the camera rays, hence a recursion level of 2. This number should be kept as low
					// as possible for performance reasons. Even recursive ray tracing should be flattened into a loop
					// in the ray generation to avoid deep recursion.
					pipelineBuilder._rayPipelineInfo.maxPipelineRayRecursionDepth = 2;  // Ray depth

					// Shader groups
					std::vector<VkRayTracingShaderGroupCreateInfoKHR> rtShaderGroups;
					VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
					group.anyHitShader = VK_SHADER_UNUSED_KHR;
					group.closestHitShader = VK_SHADER_UNUSED_KHR;
					group.generalShader = VK_SHADER_UNUSED_KHR;
					group.intersectionShader = VK_SHADER_UNUSED_KHR;

					// Raygen
					group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
					group.generalShader = rayGenIndex;
					rtShaderGroups.push_back(group);

					// Miss
					group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
					group.generalShader = rayMissIndex;
					rtShaderGroups.push_back(group);

					// closest hit shader
					group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
					group.generalShader = VK_SHADER_UNUSED_KHR;
					group.closestHitShader = rayClosestHitIndex;
					rtShaderGroups.push_back(group);

					pipelineBuilder._rayPipelineInfo.groupCount = static_cast<uint32_t>(rtShaderGroups.size());
					pipelineBuilder._rayPipelineInfo.pGroups = rtShaderGroups.data();

					pipelineLayout = pipelineBuilder._pipelineLayout;

					pipeline = pipelineBuilder.build_rt_pipeline(engine->_device);
				});
#endif
	}
}

void VulkanRenderPipelineManager::init_render_pipeline(VulkanEngine* engine, EPipelineType type, std::function<void(VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)>&& func)
{
	_pipelinesList[static_cast<uint32_t>(type)]
		.init(engine, type, std::move(func));
}

VkPipeline VulkanRenderPipelineManager::get_pipeline(EPipelineType type) const
{
	return _pipelinesList[static_cast<uint32_t>(type)].get_pipeline();
}

VkPipelineLayout VulkanRenderPipelineManager::get_pipelineLayout(EPipelineType type) const
{
	return _pipelinesList[static_cast<uint32_t>(type)].get_pipelineLayout();
}
