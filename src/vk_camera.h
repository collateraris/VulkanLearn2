#pragma once

#include <vk_types.h>

#include <SDL_events.h>
#include <glm/glm.hpp>


struct PlayerCamera {
	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 inputAxis;
	bool bActiveCamera = true;

	glm::mat4 lastViewMatrix;

	float farDistance = 10000.0f;
	float nearDistance = 0.01f;

	float pitch{ 0 }; //up-down rotation
	float yaw{ 0 }; //left-right rotation

	bool bSprint = false;

	void process_input_event(SDL_Event* ev);
	void update_camera(float deltaSeconds);

	glm::mat4 get_view_matrix();
	glm::mat4 get_projection_matrix();
	glm::mat4 get_rotation_matrix();

	std::array<glm::vec4, 6> calcFrustumPlanes();
};