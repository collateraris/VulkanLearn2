#include "vk_command_pool.h"

#include <vk_engine.h>
#include <vk_initializers.h>

VulkanCommandPool::~VulkanCommandPool()
{
	vkFreeCommandBuffers(_engine->_device, _pool, _cmdList.size(), _cmdList.data());

	vkDestroyCommandPool(_engine->_device, _pool, nullptr);
}

void VulkanCommandPool::init(VulkanEngine* engine, const VkCommandPoolCreateInfo& commandPoolInfo)
{
	_engine = engine;

	vkCreateCommandPool(_engine->_device, &commandPoolInfo, nullptr, &_pool);
}

VkCommandBuffer VulkanCommandPool::request_command_buffer()
{
	//allocate the default command buffer that we will use for rendering
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_pool, 1);

	VkCommandBuffer cmd;

	vkAllocateCommandBuffers(_engine->_device, &cmdAllocInfo, &cmd);

	return cmd;
}
