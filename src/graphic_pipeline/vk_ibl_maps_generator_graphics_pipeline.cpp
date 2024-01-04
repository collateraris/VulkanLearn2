#include "vk_ibl_maps_generator_graphics_pipeline.h"

#include <vk_engine.h>
#include <vk_textures.h>
#include <vk_initializers.h>

void VulkanIblMapsGeneratorGraphicsPipeline::init(VulkanEngine* engine, std::string hdrCubemapPath)
{
	_engine = engine;

	loadEnvironment(hdrCubemapPath);
}

const Texture& VulkanIblMapsGeneratorGraphicsPipeline::getHDR() const
{
	return _hdr;
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


}
