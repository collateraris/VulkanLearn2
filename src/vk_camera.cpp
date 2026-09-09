#include <vk_camera.h>
#include "SDL.h"

#include <glm/gtx/transform.hpp>

namespace
{
// Preserve the existing logical key bindings while tracking aliases separately.
SDL_Scancode movement_key(SDL_Keycode key)
{
	switch (key)
	{
	case SDLK_w: return SDL_SCANCODE_W;
	case SDLK_s: return SDL_SCANCODE_S;
	case SDLK_a: return SDL_SCANCODE_A;
	case SDLK_d: return SDL_SCANCODE_D;
	case SDLK_UP: return SDL_SCANCODE_UP;
	case SDLK_DOWN: return SDL_SCANCODE_DOWN;
	case SDLK_LEFT: return SDL_SCANCODE_LEFT;
	case SDLK_RIGHT: return SDL_SCANCODE_RIGHT;
	case SDLK_r: return SDL_SCANCODE_R;
	case SDLK_f: return SDL_SCANCODE_F;
	case SDLK_LSHIFT: return SDL_SCANCODE_LSHIFT;
	default: return SDL_SCANCODE_UNKNOWN;
	}
}
}

void PlayerCamera::init()
{
	auto now = std::chrono::high_resolution_clock::now();
	auto msTime = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
	rng = std::mt19937(uint32_t(msTime.time_since_epoch().count()));
	clear_input_state();
	_hasInputFocus = SDL_GetKeyboardFocus() != nullptr;
	update_mouse_mode();
}

void PlayerCamera::clear_input_state()
{
	_heldKeys.fill(false);
	inputAxis = glm::vec3(0.0f);
	velocity = glm::vec3(0.0f);
	bSprint = false;
}

void PlayerCamera::update_input_axes()
{
	inputAxis.x = float(_heldKeys[SDL_SCANCODE_S] || _heldKeys[SDL_SCANCODE_DOWN])
		- float(_heldKeys[SDL_SCANCODE_W] || _heldKeys[SDL_SCANCODE_UP]);
	inputAxis.y = float(_heldKeys[SDL_SCANCODE_D] || _heldKeys[SDL_SCANCODE_RIGHT])
		- float(_heldKeys[SDL_SCANCODE_A] || _heldKeys[SDL_SCANCODE_LEFT]);
	inputAxis.z = float(_heldKeys[SDL_SCANCODE_R]) - float(_heldKeys[SDL_SCANCODE_F]);
	bSprint = _heldKeys[SDL_SCANCODE_LSHIFT];
}

void PlayerCamera::update_mouse_mode()
{
	const SDL_bool relative = bActiveCamera && _hasInputFocus ? SDL_TRUE : SDL_FALSE;
	if (SDL_GetRelativeMouseMode() != relative)
		SDL_SetRelativeMouseMode(relative);
}

void PlayerCamera::process_input_event(SDL_Event* ev)
{
	if (!bActiveCamera || !_hasInputFocus)
		clear_input_state();

	if (ev->type == SDL_WINDOWEVENT)
	{
		if (ev->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
		{
			_hasInputFocus = false;
			clear_input_state();
		}
		else if (ev->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
			_hasInputFocus = true;
	}
	else if (ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP)
	{
		const SDL_Keycode key = ev->key.keysym.sym;
		const SDL_Scancode movement = movement_key(key);
		const bool pressed = ev->type == SDL_KEYDOWN;
		if (movement != SDL_SCANCODE_UNKNOWN)
		{
			// Releases always clear state, including while the UI owns input.
			_heldKeys[movement] = pressed && bActiveCamera && _hasInputFocus;
			update_input_axes();
		}
		if (pressed && key == SDLK_m && ev->key.repeat == 0 && _hasInputFocus)
		{
			bActiveCamera = !bActiveCamera;
			clear_input_state();
		}
		else if (pressed && bActiveCamera && _hasInputFocus)
		{
			// Keep the original 0.1-radian keyboard steps, including key repeat.
			// Releasing a rotation key must never add another angle step.
			switch (key)
			{
			case SDLK_q: yaw += 0.1f; break;
			case SDLK_e: yaw -= 0.1f; break;
			case SDLK_z: pitch += 0.1f; break;
			case SDLK_x: pitch -= 0.1f; break;
			default: break;
			}
		}
	}
	else if (ev->type == SDL_MOUSEMOTION)
	{
		if (bActiveCamera && _hasInputFocus)
		{
			pitch -= ev->motion.yrel * 0.003f;
			yaw -= ev->motion.xrel * 0.003f;
		}
	}

	update_mouse_mode();
}

void PlayerCamera::update_camera(float deltaSeconds)
{
	// The engine passes milliseconds; preserve the project's movement speeds.
	// Honor direct mode changes too (for example, deterministic diagnostics).
	if (!bActiveCamera || !_hasInputFocus)
		clear_input_state();
	update_mouse_mode();

	const float cam_vel = 0.001f + bSprint * 0.1;
	glm::vec3 forward = { 0,0,cam_vel };
	glm::vec3 right = { cam_vel,0,0 };
	glm::vec3 up = { 0,cam_vel,0 };

	glm::mat4 cam_rot = get_rotation_matrix();

	forward = cam_rot * glm::vec4(forward, 0.f);
	right = cam_rot * glm::vec4(right, 0.f);

	velocity = inputAxis.x * forward + inputAxis.y * right + inputAxis.z * up;

	velocity *= 10 * deltaSeconds;

	if (bActiveCamera && _hasInputFocus)
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
	currentProjMatrix = glm::perspective(glm::radians(FOV), aspectRatio, nearDistance, farDistance);
	currentProjMatrix[1][1] *= -1;
	currentProjWithJitterMatrix = currentProjMatrix;

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
