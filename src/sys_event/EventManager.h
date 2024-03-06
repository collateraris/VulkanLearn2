#pragma once

#include <vk_types.h>
#include <sys_event/Delegate.h>

class EventManager
{
public:

	EventManager();
	~EventManager();

	static EventManager& Get();

private:

	EventManager(EventManager&) = delete;
	EventManager(EventManager&&) = delete;
	void operator=(EventManager&) = delete;
	void operator=(EventManager&&) = delete;
};

