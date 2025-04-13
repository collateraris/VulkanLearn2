#extension GL_EXT_ray_query : enable
#extension GL_EXT_control_flow_attributes : enable

//! The inverse of the error function (used to sample Gaussians).
float erfinv(float x) {
	float w = -log(max(1.0e-37, 1.0 - x * x));
	float a = w - 2.5;
	float b = sqrt(w) - 3.0;
	return x * ((w < 5.0)
		? fma(fma(fma(fma(fma(fma(fma(fma(2.81022636e-08, a, 3.43273939e-07), a, -3.5233877e-06), a, -4.39150654e-06), a, 0.00021858087), a, -0.00125372503), a, -0.00417768164), a, 0.246640727), a, 1.50140941)
		: fma(fma(fma(fma(fma(fma(fma(fma(-0.000200214257, b, 0.000100950558), b, 0.00134934322), b, -0.00367342844), b, 0.00573950773), b, -0.0076224613), b, 0.00943887047), b, 1.00167406), b, 2.83297682));
}

/*! Computes a point on the near clipping plane for a camera defined using a
	homogeneous transform.
	\param ray_tex_coord The screen-space location of the point in a coordinate
		frame where the left top of the viewport is (0, 0), the right top is
		(1, 0) and the right bottom (1, 1).
	\param proj_to_world_space The projection to world space transform of
		the camera (to be multiplied from the left).
	\return The ray origin on the near clipping plane in world space.*/
vec3 get_camera_ray_origin(vec2 ray_tex_coord, mat4 proj_to_world_space) {
	vec4 pos_0_proj = vec4(2.0 * ray_tex_coord - vec2(1.0), 0.0, 1.0);
	vec4 pos_0_world = proj_to_world_space * pos_0_proj;
	return pos_0_world.xyz / pos_0_world.w;
}


/*! Computes a ray direction for a camera defined using a homogeneous
	transform.
	\param ray_tex_coord The screen-space location of the point in a coordinate
		frame where the left top of the viewport is (0, 0), the right top is
		(1, 0) and the right bottom (1, 1).
	\param world_to_proj_space The world to projection space transform of the
		camera (to be multiplied from the left).
	\return The normalized world-space ray direction for the camera ray.*/
vec3 get_camera_ray_direction(vec2 ray_tex_coord, mat4 world_to_proj_space) {
	vec2 dir_proj = 2.0 * ray_tex_coord - vec2(1.0);
	vec3 ray_dir;
	// This implementation is a bit fancy. The code is automatically generated
	// but the derivation goes as follows: Construct Pluecker coordinates of
	// the ray in projection space, transform those to world space, then take
	// the intersection with the plane at infinity. Of course, the parts that
	// only operate on world_to_proj_space could be pre-computed, but 24 fused
	// multiply-adds is not so bad and I like how broadly applicable this
	// implementation is.
	ray_dir.x =  (world_to_proj_space[1][1] * world_to_proj_space[2][3] - world_to_proj_space[1][3] * world_to_proj_space[2][1]) * dir_proj.x;
	ray_dir.x += (world_to_proj_space[1][3] * world_to_proj_space[2][0] - world_to_proj_space[1][0] * world_to_proj_space[2][3]) * dir_proj.y;
	ray_dir.x +=  world_to_proj_space[1][0] * world_to_proj_space[2][1] - world_to_proj_space[1][1] * world_to_proj_space[2][0];
	ray_dir.y =  (world_to_proj_space[0][3] * world_to_proj_space[2][1] - world_to_proj_space[0][1] * world_to_proj_space[2][3]) * dir_proj.x;
	ray_dir.y += (world_to_proj_space[0][0] * world_to_proj_space[2][3] - world_to_proj_space[0][3] * world_to_proj_space[2][0]) * dir_proj.y;
	ray_dir.y +=  world_to_proj_space[0][1] * world_to_proj_space[2][0] - world_to_proj_space[0][0] * world_to_proj_space[2][1];
	ray_dir.z =  (world_to_proj_space[0][1] * world_to_proj_space[1][3] - world_to_proj_space[0][3] * world_to_proj_space[1][1]) * dir_proj.x;
	ray_dir.z += (world_to_proj_space[0][3] * world_to_proj_space[1][0] - world_to_proj_space[0][0] * world_to_proj_space[1][3]) * dir_proj.y;
	ray_dir.z +=  world_to_proj_space[0][0] * world_to_proj_space[1][1] - world_to_proj_space[0][1] * world_to_proj_space[1][0];
	return normalize(ray_dir);
}

/*! Generates a pair of pseudo-random numbers.
	\param seed Integers that change with each invocation. They get updated so
		that you can reuse them.
	\return A uniform, pseudo-random point in [0,1)^2.*/
vec2 get_random_numbers(inout uvec2 seed) {
	// PCG2D, as described here: https://jcgt.org/published/0009/03/02/
	seed = 1664525u * seed + 1013904223u;
	seed.x += 1664525u * seed.y;
	seed.y += 1664525u * seed.x;
	seed ^= (seed >> 16u);
	seed.x += 1664525u * seed.y;
	seed.y += 1664525u * seed.x;
	seed ^= (seed >> 16u);
	// Multiply by 2^-32 to get floats
	return vec2(seed) * 2.32830643654e-10;
}






