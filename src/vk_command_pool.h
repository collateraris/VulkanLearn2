#pragma once

#include <vk_types.h>

class VulkanEngine;
class VulkanCommandPool
{
public:
	VulkanCommandPool() = default;
	~VulkanCommandPool();

	void init(VulkanEngine* engine, const VkCommandPoolCreateInfo& commandPoolInfo);

	VkCommandBuffer request_command_buffer();

private:
	VulkanEngine* _engine = nullptr;

	VkCommandPool _pool = VK_NULL_HANDLE;

	std::vector<VkCommandBuffer> _cmdList = {};
};