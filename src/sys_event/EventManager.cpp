#include <sys_event/EventManager.h>


EventManager::EventManager()
{

}

EventManager::~EventManager()
{

}

EventManager& EventManager::Get()
{
	static std::unique_ptr<EventManager> eventManager;
	if (!eventManager)
		eventManager = std::make_unique<EventManager>();
	return *eventManager.get();
}