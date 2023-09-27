// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_scene.h>
#include <vk_resource_manager.h>
#include <vk_mesh.h>
#include <vk_descriptors.h>
#include <vk_shaders.h>
#include <vk_material_system.h>
#include <vk_camera.h>

struct Material {
	VkDescriptorSet textureSet{ VK_NULL_HANDLE }; //texture defaulted to null
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject {
	Mesh* mesh;

	Material* material;

	glm::mat4 transformMatrix;
};

struct IndirectBatch {
	Mesh* mesh;
	Material* material;
	glm::mat4 transformMatrix;
	uint32_t first;
	uint32_t count;
};

struct GPUCameraData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
};

struct GPUSceneData {
	glm::vec4 fogColor; // w is for exponent
	glm::vec4 fogDistances; //x for min, y for max, zw unused.
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; //w for sun power
	glm::vec4 sunlightColor;
};

struct GPUObjectData {
	glm::mat4 modelMatrix;
};

struct UploadContext {
	VkFence _uploadFence;
	VkCommandPool _commandPool;
	VkCommandBuffer _commandBuffer;
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

struct FrameData {
	VkSemaphore _presentSemaphore, _renderSemaphore;
	VkFence _renderFence;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	//buffer that holds a single GPUCameraData to use when rendering
	AllocatedBuffer cameraBuffer;
	AllocatedBuffer indirectBuffer;

	AllocatedBuffer objectBuffer;
	VkDescriptorSet objectDescriptor;

	VkDescriptorSet globalDescriptor;
};

//number of frames to overlap when rendering
constexpr unsigned int FRAME_OVERLAP = 2;
constexpr unsigned int MAX_COMMANDS = 1e4;

class VulkanEngine {
public:

	VkInstance _instance; // Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
	VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface; // Vulkan window surface

	VkImageView _depthImageView;
	AllocatedImage _depthImage;

	//the format for the depth image
	VkFormat _depthFormat;

	VkSwapchainKHR _swapchain; // from other articles

	// image format expected by the windowing system
	VkFormat _swapchainImageFormat;

	std::vector<VkImage> _swapchainImages = {};
	std::vector<VkImageView> _swapchainImageViews = {};

	VkQueue _graphicsQueue; //queue we will submit to
	uint32_t _graphicsQueueFamily; //family of that queue

	VkRenderPass _renderPass;

	std::vector<VkFramebuffer> _framebuffers;

	//frame storage
	FrameData _frames[FRAME_OVERLAP];

	//getter for the frame we are rendering to right now.
	FrameData& get_current_frame();

	VkDescriptorSetLayout _globalSetLayout;
	VkDescriptorSetLayout _objectSetLayout;
	VkDescriptorSetLayout _singleTextureSetLayout;
	VkDescriptorSetLayout _meshletsSetLayout;

	std::unique_ptr<vkutil::DescriptorAllocator> _descriptorAllocator;
	std::unique_ptr<vkutil::DescriptorLayoutCache> _descriptorLayoutCache;
	std::unique_ptr<vkutil::MaterialSystem> _materialSystem;

	VkPhysicalDeviceProperties _gpuProperties;

	GPUSceneData _sceneParameters;
	AllocatedBuffer _sceneParameterBuffer;

	size_t pad_uniform_buffer_size(size_t originalSize);

	bool _isInitialized{ false };
	int _frameNumber {0};

	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	DeletionQueue _mainDeletionQueue;

	UploadContext _uploadContext;

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	VmaAllocator _allocator; //vma lib allocator

	//default array of renderable objects
	std::vector<RenderObject> _renderables;

	PlayerCamera _camera;

	ShaderCache _shaderCache;

	std::unordered_map<std::string, Material> _materials;

	ResourceManager _resManager;
	Scene _scene;

	//create material and add it to the map
	Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

	//returns nullptr if it can't be found
	Material* get_material(const std::string& name);

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

	AllocatedBuffer create_gpuonly_buffer(size_t allocSize, VkBufferUsageFlags usage);
	AllocatedBuffer create_staging_buffer(size_t allocSize, VkBufferUsageFlags usage);
	AllocatedBuffer create_cpu_to_gpu_buffer(size_t allocSize, VkBufferUsageFlags usage);

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags);

	void map_buffer(VmaAllocator& allocator, VmaAllocation& allocation, std::function<void(void*& data)> func);

	void upload_mesh(Mesh& mesh);

	void load_images();

	static std::string asset_path(std::string_view path);

	static std::string shader_path(std::string_view path);

private:

	void init_vulkan();

	void init_swapchain();

	void init_commands();

	void init_default_renderpass();

	void init_framebuffers();

	void init_sync_structures();

	void init_pipelines();

	void init_scene();

	void load_meshes();

	void load_materials(VkPipeline pipeline, VkPipelineLayout layout);

	void init_descriptors();

	void init_imgui();

	std::vector<IndirectBatch> compact_draws(RenderObject* first, int count);
};
