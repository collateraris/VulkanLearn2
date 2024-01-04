#include "vk_ibl_maps_generator_graphics_pipeline.h"

#include <vk_engine.h>
#include <vk_textures.h>
#include <vk_initializers.h>

#include <sys_config/ConfigManager.h>
#include <sys_config/vk_strings.h>

void VulkanIblMapsGeneratorGraphicsPipeline::init(VulkanEngine* engine, std::string hdrCubemapPath)
{
	_engine = engine;

	loadEnvironment(hdrCubemapPath);
}

const Texture& VulkanIblMapsGeneratorGraphicsPipeline::getHDR() const
{
	return _hdr;
}

const Texture& VulkanIblMapsGeneratorGraphicsPipeline::getEnvCubemap() const
{
	return _environmentCube;
}

void VulkanIblMapsGeneratorGraphicsPipeline::loadEnvironment(std::string hdrCubemapPath)
{
	{
		_hdr.flags = ETexFlags::NO_MIPS | ETexFlags::HDR_CUBEMAP;
		VkFormat image_format;
		if (vkutil::load_image_from_file(*_engine, hdrCubemapPath, _hdr, image_format))
		{
			VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(image_format, _hdr.image._image, VK_IMAGE_ASPECT_COLOR_BIT);
			imageinfo.subresourceRange.levelCount = _hdr.mipLevels;
			vkCreateImageView(_engine->_device, &imageinfo, nullptr, &_hdr.imageView);
		}
	}

	_imageExtent = {
		vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetEnvMapSize(),
		vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetEnvMapSize(),
		1
	};

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_environmentCube = texBuilder.start()
			.make_cubemap_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_cubemap_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();
	}
}
