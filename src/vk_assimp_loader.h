#pragma once

#include <vk_types.h>
#include <vk_scene.h>
#include <vk_resource_manager.h>

class AsimpLoader
{
public:
	static void processScene(const SceneConfig& config, Scene& newScene, ResourceManager& resManager, glm::mat4 model = glm::mat4(1.0));
};
