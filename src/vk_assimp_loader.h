#pragma once

#include <vk_types.h>
#include <vk_scene.h>

class AsimpLoader
{
public:
	static void processScene(const SceneConfig& config, Scene& newScene);
};
