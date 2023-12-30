#include <vk_camera.h>
#include "SDL.h"

#include <glm/gtx/transform.hpp>
void PlayerCamera::process_input_event(SDL_Event* ev)
{
	if (ev->type == SDL_KEYDOWN)
	{
		switch (ev->key.keysym.sym)
		{
		case SDLK_UP:
		case SDLK_w:
			if (bActiveCamera)
				inputAxis.x -= 1.f;
			break;
		case SDLK_DOWN:
		case SDLK_s:
			if (bActiveCamera)
				inputAxis.x += 1.f;
			break;
		case SDLK_LEFT:
		case SDLK_a:
			if (bActiveCamera)
				inputAxis.y -= 1.f;
			break;
		case SDLK_RIGHT:
		case SDLK_d:
			if (bActiveCamera)
				inputAxis.y += 1.f;
			break;
		case SDLK_q:
			if (bActiveCamera)
				inputAxis.z -= 1.f;
			break;

		case SDLK_e:
			if (bActiveCamera)
				inputAxis.z += 1.f;
			break;
		case SDLK_LSHIFT:
			if (bActiveCamera)
				bSprint = true;
			break;
		case SDLK_m: // m - menu
			bActiveCamera = !bActiveCamera;
			break;
		}
	}
	else if (ev->type == SDL_KEYUP)
	{
		switch (ev->key.keysym.sym)
		{
		case SDLK_UP:
		case SDLK_w:
			if (bActiveCamera)
				inputAxis.x += 1.f;
			break;
		case SDLK_DOWN:
		case SDLK_s:
			if (bActiveCamera)
				inputAxis.x -= 1.f;
			break;
		case SDLK_LEFT:
		case SDLK_a:
			if (bActiveCamera)
				inputAxis.y += 1.f;
			break;
		case SDLK_RIGHT:
		case SDLK_d:
			if (bActiveCamera)
				inputAxis.y -= 1.f;
			break;
		case SDLK_q:
			if (bActiveCamera)
				inputAxis.z += 1.f;
			break;

		case SDLK_e:
			if (bActiveCamera)
				inputAxis.z -= 1.f;
			break;
		case SDLK_LSHIFT:
			if (bActiveCamera)
				bSprint = false;
			break;
		}
	}
	else if (ev->type == SDL_MOUSEMOTION) {
		if (bActiveCamera)
		{
			pitch -= ev->motion.yrel * 0.003f;
			yaw -= ev->motion.xrel * 0.003f;
		}
	}

	inputAxis = glm::clamp(inputAxis, { -1.0,-1.0,-1.0 }, { 1.0,1.0,1.0 });
}

void PlayerCamera::update_camera(float deltaSeconds)
{
	const float cam_vel = 0.001f + bSprint * 0.01;
	glm::vec3 forward = { 0,0,cam_vel };
	glm::vec3 right = { cam_vel,0,0 };
	glm::vec3 up = { 0,cam_vel,0 };

	glm::mat4 cam_rot = get_rotation_matrix();

	forward = cam_rot * glm::vec4(forward, 0.f);
	right = cam_rot * glm::vec4(right, 0.f);

	velocity = inputAxis.x * forward + inputAxis.y * right + inputAxis.z * up;

	velocity *= 10 * deltaSeconds;

	position += velocity;
}


glm::mat4 PlayerCamera::get_view_matrix()
{
	glm::vec3 camPos = position;

	glm::mat4 cam_rot = (get_rotation_matrix());

	glm::mat4 view = glm::translate(glm::mat4{ 1 }, camPos)* cam_rot;

	//we need to invert the camera matrix
	view = glm::inverse(view);

	return view;
}

glm::mat4 PlayerCamera::get_projection_matrix()
{
	glm::mat4 pro = glm::perspective(glm::radians(70.f), 1700.f / 900.f, nearDistance, farDistance);
	pro[1][1] *= -1;
	return pro;
}

glm::mat4 PlayerCamera::get_rotation_matrix()
{
	glm::mat4 yaw_rot = glm::rotate(glm::mat4{ 1 }, yaw, { 0,1,0 });
	glm::mat4 pitch_rot = glm::rotate(glm::mat4{ yaw_rot }, pitch, { 1,0,0 });

	return pitch_rot;
}

std::array<glm::vec4, 6> PlayerCamera::calcFrustumPlanes()
{
	glm::mat4 view = get_view_matrix();
	glm::mat4 projection = get_projection_matrix();
	glm::mat4 viewproj = projection * view;
	glm::mat4 viewprojT = glm::transpose(viewproj);
	std::array<glm::vec4, 6> frustum;
	
	frustum[0] = viewprojT[3] + viewprojT[0]; // x + w < 0
	frustum[1] = viewprojT[3] - viewprojT[0]; // x - w > 0
	frustum[2] = viewprojT[3] + viewprojT[1]; // y + w < 0
	frustum[3] = viewprojT[3] - viewprojT[1]; // y - w > 0
	frustum[4] = viewprojT[3] + viewprojT[2]; // z + w > 0 near
	frustum[5] = viewprojT[3] - viewprojT[2]; // z - w > 0 far

	for (int i = 0; i < frustum.size(); ++i)
	{
		float len = glm::length(glm::vec3(frustum[i]));
		frustum[i].x /= len;
		frustum[i].y /= len;
		frustum[i].z /= len;
		frustum[i].w /= len;
	}
	
	return frustum;
}
