#pragma once

#include <vk_types.h>
class VulkanEngine;
struct RenderObject;
class VulkanIblMapsGeneratorGraphicsPipeline
{
public:
	VulkanIblMapsGeneratorGraphicsPipeline() = default;
	void init(VulkanEngine* engine, std::string hdrCubemapPath);

	const Texture& getHDR() const;
	const Texture& getEnvCubemap() const;
	const Texture& getIrradianceCubemap() const;
	const std::array<Texture, 4>& getIblTex() const;

private:
	void loadEnvironment(std::string filename);
	void loadBoxMesh();
	void createOffscreenFramebuffer();
	void drawCubemaps();
	void drawHDRtoEnvMap();
	void drawEnvMapToIrradianceMap();
	void drawEnvMapToPrefilteredMap();
	void drawIntoFaceCubemap(uint32_t pipType, uint32_t cubemapSize, uint32_t cubeFace, Texture& cubemapTex, VkDescriptorSet&  descSet, std::function<void(VkCommandBuffer& cmd)>&& pushConst, uint32_t mipLevel = 0, uint32_t numMips = 1);
	void drawCube(VkCommandBuffer& cmd);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	VkFormat   _colorFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };

	Texture _offscreenRT;
	VkFramebuffer _framebuffer;

	AllocatedBuffer _boxVB;

	Texture _hdr;
	std::array<Texture, 4> _iblTex;

	const std::vector<glm::mat4> _viewMatrices = {
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};
};