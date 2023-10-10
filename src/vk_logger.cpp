#include "vk_logger.h"
#include "plog/Initializers/RollingFileInitializer.h"

const char* to_string_message_severity(VkDebugUtilsMessageSeverityFlagBitsEXT s) {
	switch (s) {
	case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		return "VERBOSE";
	case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		return "ERROR";
	case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		return "WARNING";
	case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		return "INFO";
	default:
		return "UNKNOWN";
	}
}
const char* to_string_message_type(VkDebugUtilsMessageTypeFlagsEXT s) {
	if (s == 7) return "General | Validation | Performance";
	if (s == 6) return "Validation | Performance";
	if (s == 5) return "General | Performance";
	if (s == 4 /*VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT*/) return "Performance";
	if (s == 3) return "General | Validation";
	if (s == 2 /*VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT*/) return "Validation";
	if (s == 1 /*VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT*/) return "General";
	return "Unknown";
}

void VkLogger::init(const std::string& logFilePath)
{
    plog::init(plog::debug, logFilePath.c_str());
    PLOG_DEBUG << "Hello VkLogger!\n";
}

VkBool32 vk_logger_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
{
	auto ms = to_string_message_severity(messageSeverity);
	auto mt = to_string_message_type(messageType);
	std::string log = std::format("[{}: {}\n{}\n", ms, mt, pCallbackData->pMessage);
	PLOG_DEBUG << log.c_str();
	printf(log.c_str());

	return VK_FALSE;
}
