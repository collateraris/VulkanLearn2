#pragma once

#include <vk_types.h>

class VulkanEngine;

namespace vkutil {

	bool load_image_from_file(VulkanEngine& engine, const std::string& file, Texture& outImage, VkFormat& image_format);
	void generateMipmaps(VulkanEngine& engine, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	uint32_t getImageMipLevels(uint32_t width, uint32_t height);

	uint32_t previousPow2(uint32_t v);

	VkImageAspectFlags format_to_aspect_mask(VkFormat format);
}

class VulkanTextureBuilder
{
public:
	VulkanTextureBuilder() = default;

	void init(VulkanEngine* engine);
	VulkanTextureBuilder& start();
	VulkanTextureBuilder& make_img_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
	VulkanTextureBuilder& make_cubemap_img_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
	VulkanTextureBuilder& fill_img_info(const std::function<void(VkImageCreateInfo& imgInfo)>& func);
	VulkanTextureBuilder& make_img_allocinfo(VmaMemoryUsage usage, VkMemoryPropertyFlags requiredFlags);
	VulkanTextureBuilder& fill_img_allocinfo(const std::function<void(VmaAllocationCreateInfo& _img_allocinfo)>& func);
	VulkanTextureBuilder& make_view_info(VkFormat format, VkImageAspectFlags aspectFlags);
	VulkanTextureBuilder& make_cubemap_view_info(VkFormat format, VkImageAspectFlags aspectFlags);
	VulkanTextureBuilder& fill_view_info(const std::function<void(VkImageViewCreateInfo& _view_info)>& func);
	Texture create_texture();
	AllocatedImage create_image();
	VkImageView create_image_view(AllocatedImage img);


private:
	VulkanEngine* _engine = nullptr;

	VkImageCreateInfo _img_info;
	VmaAllocationCreateInfo _img_allocinfo;
	VkImageViewCreateInfo _view_info;
	VkExtent3D _extend;
};