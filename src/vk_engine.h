// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_utils.h>
#include <vk_scene.h>
#include <vk_resource_manager.h>
#include <vk_mesh.h>
#include <vk_descriptors.h>
#include <vk_shaders.h>
#include <vk_material_system.h>
#include <vk_camera.h>
#include <vk_logger.h>
#include <vk_textures.h>
#include <vk_render_passes.h>
#include <vk_raytracer_builder.h>
#include <vk_render_graph.h>
#include <vk_render_pass.h>
#include <vk_command_pool.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_framebuffer.h>
#include <graphic_pipeline/vk_fullscreen_graphics_pipeline.h>
#include <graphic_pipeline/vbuffer_graphics_pipeline.h>
#include <graphic_pipeline/vk_vbuffer_shading_graphics_pipeline.h>
#include <graphic_pipeline/vk_gbuffer_graphics_pipeline.h>
#include <graphic_pipeline/vk_gbuffer_shading_graphics_pipeline.h>
#include <graphic_pipeline/vk_gi_raytrace_graphics_pipeline.h>
#include <graphic_pipeline/vk_simple_accumulation_graphics_pipeline.h>
#include <graphic_pipeline/vk_ibl_maps_generator_graphics_pipeline.h>
#include <graphic_pipeline/vk_restir_pathtrace_graphics_pipeline.h>
#include <vk_light_manager.h>

constexpr size_t MAX_OBJECTS = 10000;

struct PerFrameData
{
	vec4 frustumPlanes[6];
};

struct Material {
	VkDescriptorSet textureSet{ VK_NULL_HANDLE }; //texture defaulted to null
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject {
	Mesh* mesh;

	glm::mat4 transformMatrix;
	int meshIndex;
	int matDescIndex;
};

struct IndirectBatch {
	Mesh* mesh;
	int matDescIndex;
	glm::mat4 transformMatrix;
	uint32_t first;
	uint32_t count;
};

struct alignas(16) GPUCameraData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	vec4 frustumPlanes[6];
	float pyramidWidth;
	float pyramidHeight;
	float znear;
};

struct GPUSceneData {
	glm::vec4 fogColor; // w is for exponent
	glm::vec4 fogDistances; //x for min, y for max, zw unused.
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; //w for sun power
	glm::vec4 sunlightColor;
};

struct UploadContext {
	VkFence _uploadFence;
	VulkanCommandPool _commandPool;
	VulkanCommandBuffer _commandBuffer;
};

struct FrameData {
	VkSemaphore _presentSemaphore, _renderSemaphore;
	VkFence _renderFence;

	VulkanCommandPool _commandPool;
	VulkanCommandBuffer _mainCommandBuffer;

	//buffer that holds a single GPUCameraData to use when rendering
	AllocatedBuffer cameraBuffer;

	VkDescriptorSet globalDescriptor;

	VkDescriptorSet objectDescriptor;

	VkQueryPool queryPool;
};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call the function
		}

		deletors.clear();
	}
};

//number of frames to overlap when rendering
constexpr unsigned int FRAME_OVERLAP = 2;
constexpr unsigned int MAX_COMMANDS = 1e4;

class VulkanEngine {
public:

	VkInstance _instance; // Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
	VkPhysicalDevice _chosenPhysicalDeviceGPU; // GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface; // Vulkan window surface

	Texture _depthTex;

	//the format for the depth image
	VkFormat _depthFormat;

	VkSwapchainKHR _swapchain; // from other articles

	// image format expected by the windowing system
	VkFormat _swapchainImageFormat;
	
	std::vector<Texture> _swapchainTextures = {};

	VkQueue _graphicsQueue; //queue we will submit to
	uint32_t _graphicsQueueFamily; //family of that queue

	VulkanDepthReduceRenderPass _depthReduceRenderPass;
	std::vector<VkFramebuffer> _framebuffers;
#if VBUFFER_ON
	VulkanVbufferGraphicsPipeline _visBufGenerateGraphicsPipeline;
#endif
#if GBUFFER_ON
	VulkanGbufferGenerateGraphicsPipeline _gBufGenerateGraphicsPipeline;
#endif
	VulkanIblMapsGeneratorGraphicsPipeline _iblGenGraphicsPipeline;
#if ReSTIR_PATHTRACER_ON
	VulkanReSTIRPathtracingGraphicsPipeline _ptReSTIRGraphicsPipeline;
#endif
#if GI_RAYTRACER_ON || ReSTIR_PATHTRACER_ON
	VulkanGbufferShadingGraphicsPipeline _gBufShadingGraphicsPipeline;
#endif
#if GI_RAYTRACER_ON	
	VulkanGIShadowsRaytracingGraphicsPipeline _giRtGraphicsPipeline;
#endif	
#if RAYTRACER_ON
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR _rtProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
#endif


	VulkanLightManager _lightManager;

	//frame storage
	FrameData _frames[FRAME_OVERLAP];

	Stats _stats;

	//getter for the frame we are rendering to right now.
	FrameData& get_current_frame();
	int get_current_frame_index() const;

	VkDescriptorSetLayout _globalSetLayout;
	VkDescriptorSetLayout _objectSetLayout;
	VkDescriptorSetLayout _singleTextureSetLayout;
	VkDescriptorSetLayout _meshletsSetLayout;

	AllocatedBuffer _indirectBuffer;
	AllocatedBuffer _objectBuffer;

	std::unique_ptr<vkutil::DescriptorAllocator> _descriptorAllocator;
	std::unique_ptr<vkutil::DescriptorLayoutCache> _descriptorLayoutCache;

	std::unique_ptr<vkutil::DescriptorAllocator> _descriptorBindlessAllocator;
	std::unique_ptr<vkutil::DescriptorLayoutCache> _descriptorBindlessLayoutCache;
	
	VkDescriptorSet _bindlessSet;
	VkDescriptorSetLayout _bindlessSetLayout;

	std::unique_ptr<vkutil::MaterialSystem> _materialSystem;

	VkPhysicalDeviceProperties _gpuProperties;
	VkPhysicalDeviceProperties _physDevProp{};

	GPUSceneData _sceneParameters;
	AllocatedBuffer _sceneParameterBuffer;

	size_t pad_uniform_buffer_size(size_t originalSize);

	bool _isInitialized{ false };
	int _frameNumber {0};

	VkExtent2D _windowExtent{ 2500 , 1400 };

	struct SDL_Window* _window{ nullptr };

	DeletionQueue _mainDeletionQueue;

	UploadContext _uploadContext;

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	void immediate_submit2(std::function<void(VulkanCommandBuffer& cmd)>&& function);

	VmaAllocator _allocator; //vma lib allocator

	PlayerCamera _camera;

	ShaderCache _shaderCache;

	VulkanRenderPipelineManager _renderPipelineManager;
	VulkanRenderPassManager _renderPassManager;
	VulkanFrameBufferManager _framebufferManager;

	std::unordered_map<std::string, Material> _materials;

	ResourceManager _resManager;
	Scene _scene;

	VkLogger _logger;
	vk_rgraph::VulkanRenderGraph _rgraph;

	//create material and add it to the map
	Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

	//returns nullptr if it can't be found
	Material* get_material(const std::string& name);

	void compute_pass(VkCommandBuffer cmd);
	//our draw function
	void draw_objects(VkCommandBuffer cmd, RenderObject* first, int count);

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	AllocatedSampler* get_engine_sampler(ESamplerType sampleNameId);
	Texture* get_engine_texture(ETextureResourceNames texNameId);
	AllocateDescriptor* get_engine_descriptor(EDescriptorResourceNames descrNameId);

	AllocatedBuffer create_buffer_n_copy_data(size_t allocSize, void* copyData, VkBufferUsageFlags usage);
	AllocatedBuffer create_gpuonly_buffer(size_t allocSize, VkBufferUsageFlags usage);
	AllocatedBuffer create_gpuonly_buffer_with_device_address(size_t allocSize, VkBufferUsageFlags usage);
	AllocatedBuffer create_staging_buffer(size_t allocSize, VkBufferUsageFlags usage);
	AllocatedBuffer create_cpu_to_gpu_buffer(size_t allocSize, VkBufferUsageFlags usage);

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags);

	void map_buffer(VmaAllocator& allocator, VmaAllocation& allocation, std::function<void(void*& data)> func);
	void write_buffer(VmaAllocator& allocator, VmaAllocation& allocation, const void* srcData, size_t dataSize);

	void create_image(const VkImageCreateInfo& _img_info, const VmaAllocationCreateInfo& _img_allocinfo, VkImage& img, VmaAllocation& img_alloc, VmaAllocationInfo* vma_allocinfo);

	void create_image_view(const VkImageViewCreateInfo& _view_info, VkImageView& image_view);

	void create_render_pass(const VkRenderPassCreateInfo& info, VkRenderPass& renderPass);

	VkDeviceAddress get_buffer_device_address(VkBuffer buffer);

	static std::string asset_path(std::string_view path);

	static std::string shader_path(std::string_view path);

	VkQueryPool createQueryPool(uint32_t queryCount);

	uint64_t padSizeToMinUniformBufferOffsetAlignment(uint64_t originalSize);
	uint64_t padSizeToMinStorageBufferOffsetAlignment(uint64_t originalSize);

private:

	void init_vulkan();

	void init_swapchain();

	void init_commands();

	void init_default_renderpass();

	void init_framebuffers();

	void init_sync_structures();

	void init_pipelines();

	void init_scene();

	void init_bindless_scene();

	void load_materials(VkPipeline pipeline, VkPipelineLayout layout);

	void init_descriptors();

	void init_imgui();

	std::vector<IndirectBatch> compact_draws(RenderObject* first, int count);
};
