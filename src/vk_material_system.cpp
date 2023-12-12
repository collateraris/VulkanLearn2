#include <vk_material_system.h>
#include <vk_initializers.h>
#include <vk_descriptors.h>
#include <vk_engine.h>

VkPipeline GraphicPipelineBuilder::build_graphic_pipeline(VkDevice device, VkRenderPass pass) 
{
	//make viewport state from our stored viewport and scissor.
	//at the moment we won't support multiple viewports or scissors
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;

	viewportState.viewportCount = 1;
	viewportState.pViewports = &_viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &_scissor;

	//setup dummy color blending. We aren't using transparent objects yet
	//the blending is just "no blend", but we do write to the color attachment
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;

	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &_colorBlendAttachment;

	//build the actual pipeline
	//we now use all of the info structs we have been writing into into this one to create the pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;

	pipelineInfo.stageCount = _shaderStages.size();
	pipelineInfo.pStages = _shaderStages.data();
	pipelineInfo.pVertexInputState = &_vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &_inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &_rasterizer;
	pipelineInfo.pMultisampleState = &_multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = _pipelineLayout;
	pipelineInfo.renderPass = pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.pDepthStencilState = &_depthStencil;

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;


	std::vector<VkDynamicState> dynamicStates;
	dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
	dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
	dynamicState.pDynamicStates = dynamicStates.data();
	dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();

	pipelineInfo.pDynamicState = &dynamicState;

	//it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
	VkPipeline newPipeline;
	if (vkCreateGraphicsPipelines(
		device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
		std::cout << "failed to create pipeline\n";
		return VK_NULL_HANDLE; // failed to create graphics pipeline
	}
	else
	{
		return newPipeline;
	}
}

void GraphicPipelineBuilder::clear_vertex_input()
{
	_vertexInputInfo.pVertexAttributeDescriptions = nullptr;
	_vertexInputInfo.vertexAttributeDescriptionCount = 0;

	_vertexInputInfo.pVertexBindingDescriptions = nullptr;
	_vertexInputInfo.vertexBindingDescriptionCount = 0;
}

void GraphicPipelineBuilder::setShaders(ShaderEffect* effect)
{
	_shaderStages.clear();
	effect->fill_stages(_shaderStages);

	_pipelineLayout = effect->builtLayout;
}

bool vkutil::MaterialData::operator==(const MaterialData& other) const
{
	if (other.baseTemplate.compare(baseTemplate) != 0 || other.textures.size() != textures.size())
	{
		return false;
	}
	else {
		//binary compare textures
		bool comp = memcmp(other.textures.data(), textures.data(), textures.size() * sizeof(textures[0])) == 0;
		return comp;
	}
}

size_t vkutil::MaterialData::hash() const
{
	using std::size_t;
	using std::hash;

	size_t result = hash<std::string>()(baseTemplate);

	for (const auto& b : textures)
	{
		//pack the binding data into a single int64. Not fully correct but its ok
		size_t texture_hash = (std::hash<size_t>()((size_t)b.sampler) << 3) && (std::hash<size_t>()((size_t)b.view) >> 7);

		//shuffle the packed binding data and xor it with the main hash
		result ^= std::hash<size_t>()(texture_hash);
	}

	return result;
}

void vkutil::MaterialSystem::init(VulkanEngine* owner)
{
	engine = owner;
}

void vkutil::MaterialSystem::build_default_templates()
{
	fill_builders();

	ShaderEffect* texturedLit = build_effect(engine, "tri_mesh.vert.spv", "triangle.frag.spv");

	ShaderPass* texturedLitPass = build_shader(engine->_renderPassManager.get_render_pass(ERenderPassType::Default)->get_render_pass(), forwardBuilder, texturedLit);

	EffectTemplate defaultTextured{};
	defaultTextured.passShaders[MeshpassType::Transparency] = nullptr;
	defaultTextured.passShaders[MeshpassType::DirectionalShadow] = nullptr;
	defaultTextured.passShaders[MeshpassType::Forward] = texturedLitPass;


	templateCache["defaultmesh"] = defaultTextured;
}

ShaderEffect* vkutil::MaterialSystem::build_effect(VulkanEngine* engine, std::string_view vertexShader, std::string_view fragmentShader)
{
	ShaderEffect::ReflectionOverrides overrides[] = {
		{"sceneData", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
		{"cameraData", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC}
	};
	//textured defaultlit shader
	ShaderEffect* effect = nullptr;
	auto fragmentMap = shaderEffectCache.find(vertexShader);
	if (fragmentMap == shaderEffectCache.end())
	{
		shaderEffectCache[vertexShader][fragmentShader] = std::make_unique<ShaderEffect>();
	}
	else
	{
		auto effectFound = fragmentMap->second.find(fragmentShader);
		if (effectFound == fragmentMap->second.end())
		{
			shaderEffectCache[vertexShader][fragmentShader] = std::make_unique<ShaderEffect>();
		}
	}
	effect = shaderEffectCache[vertexShader][fragmentShader].get();

	effect->add_stage(engine->_shaderCache.get_shader(VulkanEngine::shader_path(vertexShader)), VK_SHADER_STAGE_VERTEX_BIT);
	effect->add_stage(engine->_shaderCache.get_shader(VulkanEngine::shader_path(fragmentShader)), VK_SHADER_STAGE_FRAGMENT_BIT);
	effect->reflect_layout(engine->_device, overrides, 2);

	return effect;
}

vkutil::ShaderPass* vkutil::MaterialSystem::build_shader(VkRenderPass renderPass, GraphicPipelineBuilder& builder, ShaderEffect* effect)
{
	auto shaderPassFound = shaderPassCache.find(effect);
	if (shaderPassFound == shaderPassCache.end())
	{
		shaderPassCache[effect] = std::make_unique<ShaderPass>();
	}
	ShaderPass* pass = shaderPassCache[effect].get();

	pass->effect = effect;
	pass->layout = effect->builtLayout;

	GraphicPipelineBuilder pipbuilder = builder;

	pipbuilder.setShaders(effect);

	pass->pipeline = pipbuilder.build_graphic_pipeline(engine->_device, renderPass);

	return pass;
}

vkutil::Material* vkutil::MaterialSystem::build_material(const std::string& materialName, const MaterialData& info)
{
	Material* mat;
	//search material in the cache first in case its already built
	auto it = materialCache.find(info);
	if (it != materialCache.end())
	{
		mat = it->second.get();
		materials[materialName] = mat;
	}
	else {

		//need to build the material
		materialCache[info] = std::make_unique<vkutil::Material>();
		mat = materialCache[info].get();
		materials[materialName] = mat;
		mat->original = &templateCache[info.baseTemplate];
		mat->textures = info.textures;

		vkutil::DescriptorBuilder db = vkutil::DescriptorBuilder::begin(engine->_descriptorLayoutCache.get(), engine->_descriptorAllocator.get());

		for (int i = 0; i < info.textures.size(); i++)
		{
			VkDescriptorImageInfo imageBufferInfo;
			imageBufferInfo.sampler = info.textures[i].sampler;
			imageBufferInfo.imageView = info.textures[i].view;
			imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			db.bind_image(i, &imageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		}

		db.build(mat->passSets[MeshpassType::Forward]);
	}

	return mat;
}

vkutil::Material* vkutil::MaterialSystem::get_material(const std::string& materialName)
{
	auto it = materials.find(materialName);
	if (it != materials.end())
	{
		return it->second;
	}
	else {
		return nullptr;
	}
}

void vkutil::MaterialSystem::fill_builders()
{
	forwardBuilder.vertexDescription = Vertex::get_vertex_description();

	forwardBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);


	forwardBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	forwardBuilder._rasterizer.cullMode = VK_CULL_MODE_NONE;//BACK_BIT;

	forwardBuilder._multisampling = vkinit::multisampling_state_create_info();

	forwardBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

	//default depthtesting
	forwardBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
}

VkPipeline ComputePipelineBuilder::build_compute_pipeline(VkDevice device)
{
	VkComputePipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.stage = _shaderStages[0];
	createInfo.layout = _pipelineLayout;

	VkPipeline pipeline = 0;
	vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, 0, &pipeline);

	return pipeline;
}

void ComputePipelineBuilder::setShaders(ShaderEffect* effect)
{
	_shaderStages.clear();
	effect->fill_stages(_shaderStages);

	_pipelineLayout = effect->builtLayout;
}

VkPipeline RTPipelineBuilder::build_rt_pipeline(VkDevice device)
{
	_rayPipelineInfo.stageCount = static_cast<uint32_t>(_shaderStages.size());  // Stages are shaders
	_rayPipelineInfo.pStages = _shaderStages.data();


	_rayPipelineInfo.layout = _pipelineLayout;

	VkPipeline rtPipeline;

	vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &_rayPipelineInfo, nullptr, &rtPipeline);

	return rtPipeline;
}

void RTPipelineBuilder::setShaders(ShaderEffect* effect)
{
	_shaderStages.clear();
	effect->fill_stages(_shaderStages);

	_pipelineLayout = effect->builtLayout;
}
