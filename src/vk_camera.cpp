#include <vk_camera.h>
#include "SDL.h"

#include <glm/gtx/transform.hpp>
void PlayerCamera::init()
{
	auto now = std::chrono::high_resolution_clock::now();
	auto msTime = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
	rng = std::mt19937(uint32_t(msTime.time_since_epoch().count()));
}

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
		case SDLK_f:
			if (bActiveCamera)
				inputAxis.z -= 1.f;
			break;

		case SDLK_r:
			if (bActiveCamera)
				inputAxis.z += 1.f;
			break;
		case SDLK_q:
			if (bActiveCamera)
				yaw += 0.1f;
			break;

		case SDLK_e:
			if (bActiveCamera)
				yaw -= 0.1f;
			break;
		case SDLK_z:
			if (bActiveCamera)
				pitch += 0.1f;
			break;

		case SDLK_x:
			if (bActiveCamera)
				pitch -= 0.1f;
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
		case SDLK_f:
			if (bActiveCamera)
				inputAxis.z += 1.f;
			break;

		case SDLK_r:
			if (bActiveCamera)
				inputAxis.z -= 1.f;
			break;
		case SDLK_q:
			if (bActiveCamera)
				yaw += 0.1f;
			break;

		case SDLK_e:
			if (bActiveCamera)
				yaw -= 0.1f;
			break;

		case SDLK_z:
			if (bActiveCamera)
				pitch += 0.1f;
			break;

		case SDLK_x:
			if (bActiveCamera)
				pitch -= 0.1f;
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
	const float cam_vel = 0.001f + bSprint * 0.1;
	glm::vec3 forward = { 0,0,cam_vel };
	glm::vec3 right = { cam_vel,0,0 };
	glm::vec3 up = { 0,cam_vel,0 };

	glm::mat4 cam_rot = get_rotation_matrix();

	forward = cam_rot * glm::vec4(forward, 0.f);
	right = cam_rot * glm::vec4(right, 0.f);

	velocity = inputAxis.x * forward + inputAxis.y * right + inputAxis.z * up;

	velocity *= 10 * deltaSeconds;

	position += velocity;

	prevViewMatrix = currentViewMatrix;
	prevProjMatrix = currentProjMatrix;
	prevProjWithJitterMatrix = currentProjWithJitterMatrix;

	prevJitterX = jitterX;
	prevJitterY = jitterY;

	calculate_view_matrix();
	calculate_proj_matrix();
}


glm::mat4 PlayerCamera::get_view_matrix()
{
	return currentViewMatrix;
}


glm::mat4 PlayerCamera::get_projection_matrix(bool bUseJitter/* = true*/)
{
	return bUseJitter ? currentProjWithJitterMatrix : currentProjMatrix;
}

glm::mat4 PlayerCamera::get_prev_view_matrix()
{
	return prevViewMatrix;
}

glm::mat4 PlayerCamera::get_prev_projection_matrix(bool bUseJitter/* = true*/)
{
	return bUseJitter ? prevProjWithJitterMatrix : prevProjMatrix;
}

glm::mat4 PlayerCamera::get_rotation_matrix()
{
	glm::mat4 yaw_rot = glm::rotate(glm::mat4{ 1 }, yaw, { 0,1,0 });
	glm::mat4 pitch_rot = glm::rotate(glm::mat4{ yaw_rot }, pitch, { 1,0,0 });

	return pitch_rot;
}

void PlayerCamera::calculate_view_matrix()
{
	glm::vec3 camPos = position;

	glm::mat4 cam_rot = (get_rotation_matrix());

	glm::mat4 view = glm::translate(glm::mat4{ 1 }, camPos) * cam_rot;

	//we need to invert the camera matrix
	currentViewMatrix = glm::inverse(view);
}

void PlayerCamera::calculate_proj_matrix()
{
	currentProjMatrix = glm::perspective(glm::radians(FOV), 1700.f / 900.f, nearDistance, farDistance);
	currentProjMatrix[1][1] *= -1;

	if (bUseJitter)
	{

		// Build jitter matrix
		// (jitterX and jitterY are expressed as subpixel quantities divided by the screen resolution
		//  for instance to apply an offset of half pixel along the X axis we set jitterX = 0.5f / Width)
		glm::mat4 jitterMat(1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			2.0f * jitterX, 2.0f * jitterY, 0.0f, 1.0f);

		currentProjWithJitterMatrix = jitterMat * currentProjMatrix;
	}
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

void PlayerCamera::set_jitter(float x, float y)
{
	jitterX = x;
	jitterY = y;
}

void PlayerCamera::update_jitter(float w, float h)
{
	// Determine our offset in the pixel in the range [-0.5...0.5] 
	float xOff = rngDist(rng) - 0.5f;
	float yOff = rngDist(rng) - 0.5f;

	// Give our jitter to the scene camera
	set_jitter(xOff / w, yOff / h);
}

glm::vec2 PlayerCamera::get_current_jitter()
{
	return glm::vec2(jitterX, jitterY);
}

glm::vec2 PlayerCamera::get_prev_jitter()
{
	return glm::vec2(prevJitterX, prevJitterY);
}
