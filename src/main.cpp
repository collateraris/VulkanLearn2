#include <vk_engine.h>

int main(int argc, char* argv[])
{
	RenderSettings settings;
	VulkanEngine::ResumeState resume;
	VulkanEngine::read_restart_state(settings, resume);
	auto engine = std::make_unique<VulkanEngine>();
	engine->_resumeState = resume;
	engine->init(settings);
	for (;;)
	{
		engine->run();
		if (!engine->_reloadRequested || engine->launch_renderer_restart()) break;
	}
	engine->cleanup();

	return 0;
}
