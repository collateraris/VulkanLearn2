#include <vk_textures.h>
#include <iostream>
#include <vk_engine.h>
#include <vk_initializers.h>
#include <vk_resource_manager.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void load_image_with_stbi(const char* file, void*& pixel_ptr, VkFormat& image_format, VkDeviceSize& imageSize, int& texWidth, int& texHeight);
void load_hdr_image_with_stbi(const char* file, void*& pixel_ptr, VkFormat& image_format, VkDeviceSize& imageSize, int& texWidth, int& texHeight);

void load_image_for_dds(const char* file, void*& pixel_ptr, VkFormat& image_format, VkDeviceSize& imageSize, int& texWidth, int& texHeight);

bool vkutil::load_image_from_file(VulkanEngine& engine, const std::string& file, Texture& outImage, VkFormat& image_format)
{
	void* pixel_ptr = nullptr;
	VkDeviceSize imageSize = 0;
	int texWidth = -1, texHeight = -1;

	bool isDDS = file.substr(file.find_last_of(".") + 1).compare("dds") == 0;

	if (outImage.flags & ETexFlags::HDR_CUBEMAP)
		load_image_with_stbi(file.c_str(), pixel_ptr, image_format, imageSize, texWidth, texHeight);
	else if (isDDS)
		load_image_for_dds(file.c_str(), pixel_ptr, image_format, imageSize, texWidth, texHeight);
	else
		load_image_with_stbi(file.c_str(), pixel_ptr, image_format, imageSize, texWidth, texHeight);

	if (pixel_ptr == nullptr || imageSize <= 0 || texWidth <= 0 || texHeight <= 0)
	{
		engine._logger.debug_log(std::format("Failed to load texture file {}\n", file));
		return false;
	}

	outImage.mipLevels =  (outImage.flags & ETexFlags::NO_MIPS) ? 1 : static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	//allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer = engine.create_staging_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	//copy data to buffer
	engine.map_buffer(engine._allocator, stagingBuffer._allocation, [&](void*& data) {
		memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
		});

	//we no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
	stbi_image_free(pixel_ptr);

	VkExtent3D imageExtent;
	imageExtent.width = static_cast<uint32_t>(texWidth);
	imageExtent.height = static_cast<uint32_t>(texHeight);
	imageExtent.depth = 1;

	VkImageCreateInfo dimg_info = vkinit::image_create_info(image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, imageExtent);
	dimg_info.mipLevels = outImage.mipLevels;

	AllocatedImage newImage;

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	engine.create_image(dimg_info, dimg_allocinfo, newImage._image, newImage._allocation, nullptr);

	engine.immediate_submit([&](VkCommandBuffer cmd) {
		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = outImage.mipLevels;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		VkImageMemoryBarrier imageBarrier_toTransfer = {};
		imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

		imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toTransfer.image = newImage._image;
		imageBarrier_toTransfer.subresourceRange = range;
		imageBarrier_toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier_toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		imageBarrier_toTransfer.srcAccessMask = 0;
		imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		//barrier the image into the transfer-receive layout
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);
		
		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageOffset = {0, 0, 0};
		copyRegion.imageExtent = imageExtent;

		//copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, stagingBuffer._buffer, newImage._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		});

	vmaDestroyBuffer(engine._allocator, stagingBuffer._buffer, stagingBuffer._allocation);

	engine._logger.debug_log(std::format("Texture loaded successfully {} w{} h{} mips{}\n", file, texWidth, texHeight, outImage.mipLevels));

	if (outImage.mipLevels > 1)
		generateMipmaps(engine, newImage._image, image_format, texWidth, texHeight, outImage.mipLevels);
	else
		engine.immediate_submit([&](VkCommandBuffer cmd) {

		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = outImage.mipLevels;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		VkImageMemoryBarrier imageBarrier_toReadable = {};
		imageBarrier_toReadable.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

		imageBarrier_toReadable.image = newImage._image;
		imageBarrier_toReadable.subresourceRange = range;
		imageBarrier_toReadable.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier_toReadable.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;


		imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_GENERAL;

		imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		//barrier the image into the shader readable layout
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);

	});

	outImage.image = newImage;

	return true;
}

void vkutil::generateMipmaps(VulkanEngine& engine, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(engine._chosenPhysicalDeviceGPU, imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	engine.immediate_submit([&](VkCommandBuffer cmd) {

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(cmd,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

	});
}

void load_hdr_image_with_stbi(const char* file, void*& pixel_ptr, VkFormat& image_format, VkDeviceSize& imageSize, int& texWidth, int& texHeight)
{
	int texChannels;

	stbi_set_flip_vertically_on_load(true);

	float* pixels = stbi_loadf(file, &texWidth, &texHeight, &texChannels, 0);

	stbi_set_flip_vertically_on_load(false);

	if (!pixels) {
		std::cout << "Failed to load texture file " << file << std::endl;
		return;
	}

	pixel_ptr = pixels;
	imageSize = texWidth * texHeight * 2 * 3;

	image_format = VK_FORMAT_R16G16B16_SFLOAT;
}

void load_image_with_stbi(const char* file, void*& pixel_ptr, VkFormat& image_format, VkDeviceSize& imageSize, int& texWidth, int& texHeight)
{
	int texChannels;

	stbi_uc* pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		std::cout << "Failed to load texture file " << file << std::endl;
		return;
	}

	pixel_ptr = pixels;
	imageSize = texWidth * texHeight * 4;

	//the format R8G8B8A8 matches exactly with the pixels loaded from stb_image lib
	image_format = VK_FORMAT_R8G8B8A8_SRGB;
}

#define FOURCC_DXT1 0x31545844 // Equivalent to "DXT1" in ASCII
#define FOURCC_DXT3 0x33545844 // Equivalent to "DXT3" in ASCII
#define FOURCC_DXT5 0x35545844 // Equivalent to "DXT5" in ASCII
#define FOURCC_BPTC 0x30315844
#define FOURCC_BC5  0x32495441

void load_image_for_dds(const char* file, void*& pixel_ptr, VkFormat& image_format, VkDeviceSize& imageSize, int& texWidth, int& texHeight)
{
	unsigned char header[124];

	FILE* fp;

	/* try to open the file */
	fp = fopen(file, "rb");
	if (fp == NULL)
	{
		std::cout << "Failed to load Image: could not open the file" << std::endl;
		return;
	}

	/* verify the type of file */
	char filecode[4];
	fread(filecode, 1, 4, fp);
	if (strncmp(filecode, "DDS ", 4) != 0)
	{
		fclose(fp);
		std::cout << "Failed to load Image: not a direct draw surface file" << std::endl;
		return;
	}

	/* get the surface desc */
	fread(&header, 124, 1, fp);

	unsigned int height = *(unsigned int*)&(header[8]);
	unsigned int width = *(unsigned int*)&(header[12]);
	unsigned int linearSize = *(unsigned int*)&(header[16]);
	unsigned int mipMapCount = *(unsigned int*)&(header[24]);
	unsigned int fourCC = *(unsigned int*)&(header[80]);

	unsigned char* buffer;
	unsigned int bufsize;

	/* how big is it going to be including all mipmaps? */
	if (fourCC == FOURCC_BPTC) {//in BC7 header, linearSize is small(8192), width/height is 2048.
		linearSize = height * width;
		bufsize = mipMapCount > 1 ? linearSize * 2 : linearSize;
		buffer = (unsigned char*)malloc(bufsize * sizeof(unsigned char));
		fread(buffer, 1, bufsize, fp);
	}
	else {
		/* how big is it going to be including all mipmaps? */
		bufsize = mipMapCount > 1 ? linearSize * 2 : linearSize;
		buffer = (unsigned char*)malloc(bufsize * sizeof(unsigned char));
		fread(buffer, 1, bufsize, fp);
	}

	pixel_ptr = (void*)buffer;
	imageSize = bufsize;
	texWidth = width;
	texHeight = height;

	/* close the file pointer */
	fclose(fp);

	unsigned int components = (fourCC == FOURCC_DXT1) ? 3 : 4;
	switch (fourCC)
	{
	case FOURCC_DXT1:
		image_format = VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		break;
	case FOURCC_DXT3:
		image_format = VK_FORMAT_BC2_SRGB_BLOCK;
		break;
	case FOURCC_DXT5:
		image_format = VK_FORMAT_BC3_SRGB_BLOCK;
		break;
	case FOURCC_BPTC:
		image_format = VK_FORMAT_BC7_SRGB_BLOCK;
		break;
	case FOURCC_BC5:
		image_format = VK_FORMAT_BC5_SNORM_BLOCK;
		break;
	default:
		free(buffer);
		std::cout << "Failed to load Image: dds file format not supported (supported formats: DXT1, DXT3, DXT5)" << std::endl;
		std::cout << "Texture failed to load at path: " << file << std::endl;
		return;
	}
}

uint32_t vkutil::getImageMipLevels(uint32_t width, uint32_t height)
{
	uint32_t result = 1;

	while (width > 1 || height > 1)
	{
		result++;
		width /= 2;
		height /= 2;
	}

	return result;
}

uint32_t vkutil::previousPow2(uint32_t v)
{
	uint32_t r = 1;

	while (r * 2 < v)
		r *= 2;

	return r;
}

VkImageAspectFlags vkutil::format_to_aspect_mask(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_UNDEFINED:
		return 0;

	case VK_FORMAT_S8_UINT:
		return VK_IMAGE_ASPECT_STENCIL_BIT;

	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;

	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D32_SFLOAT:
	case VK_FORMAT_X8_D24_UNORM_PACK32:
		return VK_IMAGE_ASPECT_DEPTH_BIT;

	default:
		return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}

void VulkanTextureBuilder::init(VulkanEngine* engine)
{
	_engine = engine;
}

VulkanTextureBuilder& VulkanTextureBuilder::start()
{
	_img_info = {};
	_img_allocinfo = {};
	_view_info = {};
	_extend = {};
	return *this;
}

VulkanTextureBuilder& VulkanTextureBuilder::make_img_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent)
{
	_img_info = vkinit::image_create_info(format, usageFlags, extent);
	_extend = extent;
	return *this;
}

VulkanTextureBuilder& VulkanTextureBuilder::make_cubemap_img_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent)
{
	_img_info = vkinit::cubemap_image_create_info(format, usageFlags, extent);
	_extend = extent;
	return *this;
}

VulkanTextureBuilder& VulkanTextureBuilder::fill_img_info(const std::function<void(VkImageCreateInfo& imgInfo)>& func)
{
	func(_img_info);
	return *this;
}

VulkanTextureBuilder& VulkanTextureBuilder::make_img_allocinfo(VmaMemoryUsage usage, VkMemoryPropertyFlags requiredFlags)
{
	_img_allocinfo.usage = usage;
	_img_allocinfo.requiredFlags = requiredFlags;
	return *this;
}

VulkanTextureBuilder& VulkanTextureBuilder::fill_img_allocinfo(const std::function<void(VmaAllocationCreateInfo& _img_allocinfo)>& func)
{
	func(_img_allocinfo);
	return *this;
}

VulkanTextureBuilder& VulkanTextureBuilder::make_view_info(VkFormat format, VkImageAspectFlags aspectFlags)
{
	_view_info = vkinit::imageview_create_info(format, {}, aspectFlags);
	return *this;
}

VulkanTextureBuilder& VulkanTextureBuilder::make_cubemap_view_info(VkFormat format, VkImageAspectFlags aspectFlags)
{
	_view_info = vkinit::cubemap_imageview_create_info(format, {}, aspectFlags);
	return *this;
}

VulkanTextureBuilder& VulkanTextureBuilder::fill_view_info(const std::function<void(VkImageViewCreateInfo& _view_info)>& func)
{
	func(_view_info);
	return *this;
}

Texture VulkanTextureBuilder::create_texture()
{
	Texture tex;
	tex.image = create_image();
	tex.imageView = create_image_view(tex.image);
	tex.extend = _extend;
	tex.createInfo = _img_info;
	tex.mipLevels = _img_info.mipLevels;
	tex.currImageLayout = _img_info.initialLayout;

	return tex;
}

void VulkanTextureBuilder::create_engine_texture(ETextureResourceNames texNameId)
{
	Texture* tex = _engine->_resManager.create_engine_texture(texNameId);
	tex->image = create_image();
	tex->imageView = create_image_view(tex->image);
	tex->extend = _extend;
	tex->createInfo = _img_info;
	tex->mipLevels = _img_info.mipLevels;
	tex->currImageLayout = _img_info.initialLayout;
}

AllocatedImage VulkanTextureBuilder::create_image()
{
	AllocatedImage img;
	_engine->create_image(_img_info, _img_allocinfo, img._image, img._allocation, nullptr);

	return img;
}

VkImageView VulkanTextureBuilder::create_image_view(AllocatedImage img)
{
	VkImageView img_view;
	_view_info.image = img._image;
	_engine->create_image_view(_view_info, img_view);

	return img_view;
}
