#include <vk_types.h>
#include <plog/Log.h>

VkBool32 vk_logger_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void*);

class VkLogger
{
public:
	void init(const std::string& logFilePath);
};