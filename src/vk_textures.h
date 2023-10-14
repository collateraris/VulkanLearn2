#pragma once

#include <vk_types.h>

class VulkanEngine;

namespace vkutil {

	bool load_image_from_file(VulkanEngine& engine, const std::string& file, AllocatedImage& outImage, VkFormat& image_format);

	uint32_t getImageMipLevels(uint32_t width, uint32_t height);
}

class VkTextureBuilder
{
public:
	VkTextureBuilder() = default;

	void init(VulkanEngine* engine);
	VkTextureBuilder& start();
	VkTextureBuilder& make_img_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
	VkTextureBuilder& fill_img_info(std::function<void(VkImageCreateInfo& imgInfo)>& func);
	VkTextureBuilder& make_img_allocinfo(VmaMemoryUsage usage, VkMemoryPropertyFlags requiredFlags);
	VkTextureBuilder& fill_img_allocinfo(std::function<void(VmaAllocationCreateInfo& _img_allocinfo)>& func);
	VkTextureBuilder& make_view_info(VkFormat format, VkImageAspectFlags aspectFlags);
	VkTextureBuilder& fill_view_info(std::function<void(VkImageViewCreateInfo& _view_info)>& func);
	Texture create_texture();



private:
	VulkanEngine* _engine = nullptr;

	VkImageCreateInfo _img_info;
	VmaAllocationCreateInfo _img_allocinfo;
	VkImageViewCreateInfo _view_info;
};