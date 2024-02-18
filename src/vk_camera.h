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

	glm::mat4 currentViewMatrix;
	glm::mat4 currentProjMatrix;
	glm::mat4 currentProjWithJitterMatrix;

	glm::mat4 prevViewMatrix;
	glm::mat4 prevProjMatrix;
	glm::mat4 prevProjWithJitterMatrix;

	float farDistance = 10000.0f;
	float nearDistance = 0.01f;

	float jitterX = 1.f;
	float jitterY = 1.f;

	float prevJitterX = 1.f;
	float prevJitterY = 1.f;

	bool                                  bUseJitter = true;
	std::uniform_real_distribution<float> rngDist;
	std::mt19937                          rng;

	float pitch{ 0 }; //up-down rotation
	float yaw{ 0 }; //left-right rotation

	bool bSprint = false;

	void init();

	void process_input_event(SDL_Event* ev);
	void update_camera(float deltaSeconds);

	glm::mat4 get_view_matrix();
	glm::mat4 get_projection_matrix(bool bUseJitter = true);
	glm::mat4 get_prev_view_matrix();
	glm::mat4 get_prev_projection_matrix(bool bUseJitter = true);
	glm::mat4 get_rotation_matrix();

	void calculate_view_matrix();
	void calculate_proj_matrix();

	std::array<glm::vec4, 6> calcFrustumPlanes();

	void set_jitter(float jitterX, float jitterY);
	void update_jitter(float w, float h);

	glm::vec2 get_current_jitter();
	glm::vec2 get_prev_jitter();

};