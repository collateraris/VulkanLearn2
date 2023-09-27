#include <vk_textures.h>
#include <iostream>

#include <vk_initializers.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void load_image_with_stbi(const char* file, void*& pixel_ptr, VkFormat& image_format, VkDeviceSize& imageSize, int& texWidth, int& texHeight);

void load_image_for_dds(const char* file, void*& pixel_ptr, VkFormat& image_format, VkDeviceSize& imageSize, int& texWidth, int& texHeight);

bool vkutil::load_image_from_file(VulkanEngine& engine, const std::string& file, AllocatedImage& outImage, VkFormat& image_format)
{
	void* pixel_ptr = nullptr;
	VkDeviceSize imageSize = 0;
	int texWidth = -1, texHeight = -1;

	bool isDDS = file.substr(file.find_last_of(".") + 1).compare("dds") == 0;

	if (isDDS)
	load_image_for_dds(file.c_str(), pixel_ptr, image_format, imageSize, texWidth, texHeight);
		else
	load_image_with_stbi(file.c_str(), pixel_ptr, image_format, imageSize, texWidth, texHeight);

	if (pixel_ptr == nullptr || imageSize <= 0 || texWidth <= 0 || texHeight <= 0)
	{
		std::cout << "Failed to load texture file " << file << std::endl;
		return false;
	}

	//allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer = engine.create_staging_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	//copy data to buffer
	void* data;
	vmaMapMemory(engine._allocator, stagingBuffer._allocation, &data);

	memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));

	vmaUnmapMemory(engine._allocator, stagingBuffer._allocation);
	//we no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
	stbi_image_free(pixel_ptr);

	VkExtent3D imageExtent;
	imageExtent.width = static_cast<uint32_t>(texWidth);
	imageExtent.height = static_cast<uint32_t>(texHeight);
	imageExtent.depth = 1;

	VkImageCreateInfo dimg_info = vkinit::image_create_info(image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

	AllocatedImage newImage;

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	//allocate and create the image
	vmaCreateImage(engine._allocator, &dimg_info, &dimg_allocinfo, &newImage._image, &newImage._allocation, nullptr);

	engine.immediate_submit([&](VkCommandBuffer cmd) {
		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		VkImageMemoryBarrier imageBarrier_toTransfer = {};
		imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

		imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toTransfer.image = newImage._image;
		imageBarrier_toTransfer.subresourceRange = range;

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
		copyRegion.imageExtent = imageExtent;

		//copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, stagingBuffer._buffer, newImage._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

		imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		//barrier the image into the shader readable layout
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);

		});

	engine._mainDeletionQueue.push_function([&]() {

		vmaDestroyImage(engine._allocator, newImage._image, newImage._allocation);
		});

	vmaDestroyBuffer(engine._allocator, stagingBuffer._buffer, stagingBuffer._allocation);

	std::cout << "Texture loaded successfully " << file << std::endl;

	outImage = newImage;

	return true;
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
