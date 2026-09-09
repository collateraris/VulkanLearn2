#include "vk_command_pool.h"

#include <vk_engine.h>
#include <vk_initializers.h>

void VulkanCommandPool::init(VulkanEngine* engine, const VkCommandPoolCreateInfo& commandPoolInfo)
{
	_engine = engine;

	vkCreateCommandPool(_engine->_device, &commandPoolInfo, nullptr, &_pool);
	// Destroying a pool also frees every command buffer allocated from it.
	// Queue this before the VkDevice is destroyed, rather than in member teardown.
	_engine->_mainDeletionQueue.push_function([device = engine->_device, pool = _pool]() {
		vkDestroyCommandPool(device, pool, nullptr);
	});
}

VkCommandBuffer VulkanCommandPool::request_command_buffer()
{
	//allocate the default command buffer that we will use for rendering
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_pool, 1);

	VkCommandBuffer cmd;

	vkAllocateCommandBuffers(_engine->_device, &cmdAllocInfo, &cmd);

	return cmd;
}

void VulkanCommandPool::reset()
{
	vkResetCommandPool(_engine->_device, _pool, 0);
}

void VulkanCommandPool::free_command_buffer(VkCommandBuffer& cmd)
{
	vkFreeCommandBuffers(_engine->_device, _pool, 1, &cmd);
}
