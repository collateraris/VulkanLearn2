#include <vk_utils.h>
#include <vk_initializers.h>

void vkutil::image_pipeline_barrier(const VkCommandBuffer& cmd, Texture& image, VkAccessFlagBits nextAccessFlag, VkImageLayout nextImageLayout, VkPipelineStageFlagBits nextPipStage, VkImageAspectFlagBits aspect/* = VK_IMAGE_ASPECT_COLOR_BIT*/)
{
	if (image.currAccessFlag == nextAccessFlag
		&& image.currImageLayout == nextImageLayout
		&& image.currPipStage == nextPipStage)
		return;

	std::array<VkImageMemoryBarrier, 1> outBarriers =
	{
		vkinit::image_barrier(image.image._image, image.currAccessFlag, nextAccessFlag,  image.currImageLayout, nextImageLayout, aspect),
	};

	vkCmdPipelineBarrier(cmd, image.currPipStage, nextPipStage, 0, 0, 0, 0, 0, outBarriers.size(), outBarriers.data());

	image.currAccessFlag = nextAccessFlag;
	image.currImageLayout = nextImageLayout;
	image.currPipStage = nextPipStage;
}
