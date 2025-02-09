#extension GL_EXT_ray_query : enable
#extension GL_EXT_control_flow_attributes : enable

struct rqShadingData
{
    vec4 worldPos;
    vec4 albedo_lambert_out;
    vec4 fresnel_0_;
    vec4 emission_roughness;
    vec4 out_dir_;
    vec4 normal_;
};

struct SGlobalRQParams
{
	mat4 proj_to_world_space;
	mat4 world_to_proj_space;
	vec4 width_height_fov_frameIndex;
};

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

vec3 pinholeCameraSampleRay(float w, float h, float fov, vec3 camPos, mat4 camToWorld, vec2 uv) {
	vec2 biasedCoord = uv;
	vec2 ndc = biasedCoord * 2.0 - 1.0;
	float aspect = float(w) / float(h);
	float tanFOV = tan(radians(fov * 0.5));

	vec4 pFocusPlane = vec4(ndc * vec2(aspect, 1.0) * tanFOV, 1., 1.);
	vec4 rayOrigin = vec4(camPos, 1.);
	vec4 pFocusPlaneInWorld = camToWorld * pFocusPlane;
	vec4 rayOriginInWorld = camToWorld * rayOrigin;
	vec3 rayDirInWorld = normalize(pFocusPlaneInWorld.xyz - rayOriginInWorld.xyz);

	return rayDirInWorld;
}

//! The Fresnel-Schlick approximation for the Fresnel term
vec3 fresnel_schlick(vec3 f_0, vec3 f_90, float lambert) {
	float flip_1 = 1.0 - lambert;
	float flip_2 = flip_1 * flip_1;
	float flip_5 = flip_2 * flip_1 * flip_2;
	return flip_5 * (f_90 - f_0) + f_0;
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

/*! Samples the distribution of visible normals in the GGX normal distribution
	function for the given outgoing direction. All vectors must be in a
	coordinate frame where the shading normal is (0.0, 0.0, 1.0).
	\param out_dir Direction towards the camera along the path. Need not be
		normalized.
	\param roughness The roughness parameter of GGX for the local x- and
		y-axis.
	\param randoms A point distributed uniformly in [0,1)^2.
	\return The normalized half vector (*not* the incoming direction).*/
vec3 sample_ggx_vndf(vec3 out_dir, vec2 roughness, vec2 randoms) {
	// This implementation is based on:
	// https://gist.github.com/jdupuy/4c6e782b62c92b9cb3d13fbb0a5bd7a0
	// It is described in detail here:
	// https://doi.org/10.1111/cgf.14867

	// Warp to the hemisphere configuration
	vec3 out_dir_std = normalize(vec3(out_dir.xy * roughness, out_dir.z));
	// Sample a spherical cap in (-out_dir_std.z, 1]
	float azimuth = (2.0 * M_PI) * randoms[0] - M_PI;
	float z = 1.0 - randoms[1] * (1.0 + out_dir_std.z);
	float sine = sqrt(max(0.0, 1.0 - z * z));
	vec3 cap = vec3(sine * cos(azimuth), sine * sin(azimuth), z);
	// Compute the half vector in the hemisphere configuration
	vec3 half_dir_std = cap + out_dir_std;
	// Warp back to the ellipsoid configuration
	return normalize(vec3(half_dir_std.xy * roughness, half_dir_std.z));
}

//! Forwards to sample_ggx_vndf() and constructs a local reflection vector from
//! the result which can be used as incoming light direction.
vec3 sample_ggx_in_dir(vec3 out_dir, float roughness, vec2 randoms) {
	vec3 half_dir = sample_ggx_vndf(out_dir, vec2(roughness), randoms);
	return -reflect(out_dir, half_dir);
}

//! Heuristically computes a probability with which projected solid angle
//! sampling should be used when sampling the BRDF for the given shading point.
float get_diffuse_sampling_probability(rqShadingData s) {
	// In principle we use the luminance of the diffuse albedo. But in specular
	// highlights, the specular component is much more important than the
	// diffuse component. Thus, we always sample it at least 50% of the time in
	// the spirit of defensive sampling.
	return min(0.5, dot(s.albedo_lambert_out.xyz, vec3(0.2126, 0.7152, 0.0722)));
}

//! Produces a sample distributed uniformly with respect to projected solid
//! angle (PSA) in the upper hemisphere (positive z).
vec3 sample_hemisphere_psa(vec2 randoms) {
	// Sample a disk uniformly
	float azimuth = (2.0 * M_PI) * randoms[0] - M_PI;
	float radius = sqrt(randoms[1]);
	// Project to the hemisphere
	float z = sqrt(1.0 - radius * radius);
	return vec3(radius * cos(azimuth), radius * sin(azimuth), z);
}

//! Constructs a special orthogonal matrix where the given normalized normal
//! vector is the third column
mat3 get_shading_space(vec3 n) {
	// This implementation is from: https://www.jcgt.org/published/0006/01/01/
	float s = (n.z > 0.0) ? 1.0 : -1.0;
	float a = -1.0 / (s + n.z);
	float b = n.x * n.y * a;
	vec3 b1 = vec3(1.0 + s * n.x * n.x * a, s * b, -s * n.x);
	vec3 b2 = vec3(b, s + n.y * n.y * a, -n.y);
	return mat3(b1, b2, n);
}

/*! Samples an incoming light direction using sampling strategies that provide
	good importance sampling for the Frostbite BRDF times cosine (namely
	projected solid angle sampling and GGX VNDF sampling with probabilities as
	indicated by get_diffuse_sampling_probability()).
	\param s Complete shading data.
	\param randoms A uniformly distributed point in [0,1)^2.
	\return A normalized direction vector for the sampled direction.*/
vec3 sample_frostbite_brdf(rqShadingData s, vec2 randoms) {
	mat3 shading_to_world_space = get_shading_space(s.normal_.xyz);
	// Decide stochastically whether to sample the specular or the diffuse
	// component
	float diffuse_prob = get_diffuse_sampling_probability(s);
	bool diffuse = randoms[0] < diffuse_prob;
	// Sample the direction and determine the density according to a
	// single-sample MIS estimate with the balance heuristic
	float density;
	vec3 sampled_dir;
	if (diffuse) {
		// Reuse the random number
		randoms[0] /= diffuse_prob;
		// Sample the projected solid angle uniformly
		sampled_dir = shading_to_world_space * sample_hemisphere_psa(randoms);
	}
	else {
		// Reuse the random number
		randoms[0] = (randoms[0] - diffuse_prob) / (1.0 - diffuse_prob);
		// Sample the half-vector from the GGX VNDF
		vec3 local_out_dir = transpose(shading_to_world_space) * s.out_dir_.xyz;
		vec3 local_in_dir = sample_ggx_in_dir(local_out_dir, s.emission_roughness.w, randoms);
		sampled_dir = shading_to_world_space * local_in_dir;
	}
	return sampled_dir;
}

/*! Computes the density that is sampled by sample_ggx_vndf().
	\param lambert_out Dot product between the shading normal and the outgoing
		light direction.
	\param half_dot_normal Dot product between the sampled half vector and
		the shading normal.
	\param half_dot_out Dot product between the sampled half vector and the
		outgoing light direction.
	\param roughness The GGX roughness parameter (along both x- and y-axis).
	\return The density w.r.t. solid angle for the sampled half vector (*not*
		for the incoming direction).*/
float get_ggx_vndf_density(float lambert_out, float half_dot_normal, float half_dot_out, float roughness) {
	// Based on Equation 2 in this paper: https://doi.org/10.1111/cgf.14867
	// A few factors have been cancelled to optimize evaluation.
	if (half_dot_normal < 0.0)
		return 0.0;
	float roughness_2 = roughness * roughness;
	float flip_roughness_2 = 1.0 - roughness * roughness;
	float length_M_inv_out_2 = roughness_2 + flip_roughness_2 * lambert_out * lambert_out;
	float D_vis_std = max(0.0, half_dot_out) * (2.0 / M_PI) / (lambert_out + sqrt(length_M_inv_out_2));
	float length_M_half_2 = 1.0 - flip_roughness_2 * half_dot_normal * half_dot_normal;
	return D_vis_std * roughness_2 / (length_M_half_2 * length_M_half_2);
}

//! Returns the density w.r.t. solid angle that is sampled by
//! sample_ggx_in_dir(). Most parameters come from shading_data_t.
float get_ggx_in_dir_density(float lambert_out, vec3 out_dir, vec3 in_dir, vec3 normal, float roughness) {
	vec3 half_dir = normalize(in_dir + out_dir);
	float half_dot_out = dot(half_dir, out_dir);
	float half_dot_normal = dot(half_dir, normal);
	// Compute the density of the half vector
	float density = get_ggx_vndf_density(lambert_out, half_dot_normal, half_dot_out, roughness);
	// Account for the mirroring in the density
	density /= 4.0 * half_dot_out;
	return density;
}

//! Returns the density w.r.t. solid angle sampled by sample_hemisphere_psa().
//! It only needs the z-coordinate of the sampled direction (in shading space).
float get_hemisphere_psa_density(float sampled_dir_z) {
	return (1.0 / M_PI) * max(0.0, sampled_dir_z);
}
//! Returns the density w.r.t. solid angle sampled by sample_frostbite_brdf().
float get_frostbite_brdf_density(rqShadingData s, vec3 sampled_dir) {
	float diffuse_prob = get_diffuse_sampling_probability(s);
	float specular_density = get_ggx_in_dir_density(s.albedo_lambert_out.w, s.out_dir_.xyz, sampled_dir, s.normal_.xyz, s.emission_roughness.w);
	float diffuse_density = get_hemisphere_psa_density(dot(s.normal_.xyz, sampled_dir));
	return mix(specular_density, diffuse_density, diffuse_prob);
}

/*! Evaluates the Frostbite BRDF as described at the links below for the given
	shading point and normalized incoming light direction:
	https://dl.acm.org/doi/abs/10.1145/2614028.2615431
	https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf */
vec3 frostbite_brdf(rqShadingData s, vec3 in_dir) {
	// The BRDF is zero in the lower hemisphere
	float lambert_in = dot(s.normal_.xyz, in_dir);
	if (min(lambert_in, s.albedo_lambert_out.w) < 0.0)
		return vec3(0.0);
	// It uses the half-vector between incoming and outgoing light direction
	vec3 half_dir = normalize(in_dir + s.out_dir_.xyz);
	float half_dot_out = dot(half_dir, s.out_dir_.xyz);
	// This is the Disney diffuse BRDF
	float f_90 = (half_dot_out * half_dot_out) * (2.0 * s.emission_roughness.w) + 0.5;
	float fresnel_diffuse =
		fresnel_schlick(vec3(1.0), vec3(f_90), s.albedo_lambert_out.w).x *
		fresnel_schlick(vec3(1.0), vec3(f_90), lambert_in).x;
	vec3 brdf = fresnel_diffuse * s.albedo_lambert_out.xyz;
	// The Frostbite specular BRDF uses the GGX normal distribution function...
	float half_dot_normal = dot(half_dir, s.normal_.xyz);
	float roughness_2 = s.emission_roughness.w * s.emission_roughness.w;
	float ggx = (roughness_2 * half_dot_normal - half_dot_normal) * half_dot_normal + 1.0;
	ggx = roughness_2 / (ggx * ggx);
	// ... Smith masking-shadowing...
	float masking = lambert_in * sqrt((s.albedo_lambert_out.w - roughness_2 * s.albedo_lambert_out.w) * s.albedo_lambert_out.w + roughness_2);
	float shadowing = s.albedo_lambert_out.w * sqrt((lambert_in - roughness_2 * lambert_in) * lambert_in + roughness_2);
	float smith = 0.5 / (masking + shadowing);
	// ... and the Fresnel-Schlick approximation
	vec3 fresnel = fresnel_schlick(s.fresnel_0_.xyz, vec3(1.0), max(0.0, half_dot_out));
	brdf += ggx * smith * fresnel;
	return brdf * (1.0 / M_PI);
}

rqShadingData unpackGBuffer(vec3 wpos, vec3 normal, vec2 tex_coords, uint drawID, vec3 out_dir)
{
	SObjectData shadeData = objectBuffer.objects[drawID];

	vec3 normal_tex = normal;
	if (shadeData.normalTexIndex > 0)
	{
		normal_tex = texture(texSet[shadeData.normalTexIndex], tex_coords).rgb;
		mat3 TBN = getTBN(normal);
		normal_tex = normalize(normal_tex * 2.0 - 1.0); 
		normal_tex = normalize(TBN * normal_tex);
	}   

    vec3 base_color = texture(texSet[shadeData.diffuseTexIndex], tex_coords).rgb;

    float metalness = 0.;
    if (shadeData.metalnessTexIndex > 0)
        metalness = texture(texSet[shadeData.metalnessTexIndex], tex_coords).r; 

    vec3 emission = vec3(0., 0., 0);
    if (shadeData.emissionTexIndex > 0)
        emission = texture(texSet[shadeData.emissionTexIndex], tex_coords).rgb;     

    float roughness = 1.;
    if (shadeData.roughnessTexIndex > 0)
        roughness = texture(texSet[shadeData.roughnessTexIndex], tex_coords).g;

    rqShadingData shData;
    shData.worldPos.xyz = wpos;
    shData.normal_.xyz = normal_tex;
    shData.out_dir_.xyz = out_dir;
    shData.albedo_lambert_out.xyz = base_color - metalness * base_color;
    shData.albedo_lambert_out.w = dot(shData.normal_.xyz, shData.out_dir_.xyz);
    shData.fresnel_0_.xyz = mix(vec3(0.02), base_color, metalness);
    shData.emission_roughness.xyz = emission;
    shData.emission_roughness.w = roughness;

    return shData;
}

rqShadingData getShadingData(int instance_id, int primitive_id, vec2 baryCoord, bool front, vec3 out_dir) 
{
    SObjectData shadeData = objectBuffer.objects[instance_id];

    ivec4 index = Indices[shadeData.meshIndex].indices[primitive_id];
    
    SVertex v0 = Vertices[shadeData.meshIndex].vertices[index.x];
    SVertex v1 = Vertices[shadeData.meshIndex].vertices[index.y];
    SVertex v2 = Vertices[shadeData.meshIndex].vertices[index.z];

    const vec3 barycentrics = vec3(1.0 - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);

    vec2 tex_coords =
            v0.normalYZ_texCoordUV.zw * barycentrics.x + v1.normalYZ_texCoordUV.zw * barycentrics.y + v2.normalYZ_texCoordUV.zw * barycentrics.z;

    vec3 v0_pos = vec3(v0.positionXYZ_normalX.x, v0.positionXYZ_normalX.y, v0.positionXYZ_normalX.z);
    vec3 v1_pos = vec3(v1.positionXYZ_normalX.x, v1.positionXYZ_normalX.y, v1.positionXYZ_normalX.z);
    vec3 v2_pos = vec3(v2.positionXYZ_normalX.x, v2.positionXYZ_normalX.y, v2.positionXYZ_normalX.z);      

    const vec3 pos      = v0_pos * barycentrics.x + v1_pos * barycentrics.y + v2_pos * barycentrics.z;
    const vec3 worldPos = vec3(shadeData.model * vec4(pos, 1.0));  // Transforming the position to world space

    // Computing the normal at hit position

    vec3 v0_nrm = vec3(v0.positionXYZ_normalX.w, v0.normalYZ_texCoordUV.x, v0.normalYZ_texCoordUV.y);
    vec3 v1_nrm = vec3(v1.positionXYZ_normalX.w, v1.normalYZ_texCoordUV.x, v1.normalYZ_texCoordUV.y);
    vec3 v2_nrm = vec3(v2.positionXYZ_normalX.w, v2.normalYZ_texCoordUV.x, v2.normalYZ_texCoordUV.y); 

    const vec3 nrm      = v0_nrm * barycentrics.x + v1_nrm * barycentrics.y + v2_nrm * barycentrics.z;
    mat3 normalMatrix = transpose(inverse(mat3(shadeData.model)));
    vec3 world_norm = normalize(vec3(normalMatrix * nrm));  // Transforming the normal to world space

	vec3 normal_tex = world_norm;
	if (shadeData.normalTexIndex > 0)
	{
		normal_tex = texture(texSet[shadeData.normalTexIndex], tex_coords).rgb;
		mat3 TBN = getTBN(world_norm);
		normal_tex = normalize(normal_tex * 2.0 - 1.0); 
		normal_tex = normalize(TBN * normal_tex);
	}   

    vec3 base_color = texture(texSet[shadeData.diffuseTexIndex], tex_coords).rgb;

    float metalness = 0.;
    if (shadeData.metalnessTexIndex > 0)
        metalness = texture(texSet[shadeData.metalnessTexIndex], tex_coords).r; 

    vec3 emission = vec3(0., 0., 0);
    if (shadeData.emissionTexIndex > 0)
        emission = texture(texSet[shadeData.emissionTexIndex], tex_coords).rgb;     

    float roughness = 1.;
    if (shadeData.roughnessTexIndex > 0)
        roughness = texture(texSet[shadeData.roughnessTexIndex], tex_coords).g;

    rqShadingData shData;
    shData.worldPos.xyz = worldPos;
    shData.normal_.xyz = normal_tex;
    shData.normal_.xyz = front ? shData.normal_.xyz : -shData.normal_.xyz;
    shData.out_dir_.xyz = out_dir;
    shData.albedo_lambert_out.xyz = base_color - metalness * base_color;
    shData.albedo_lambert_out.w = dot(shData.normal_.xyz, shData.out_dir_.xyz);
    shData.fresnel_0_.xyz = mix(vec3(0.02), base_color, metalness);
    shData.emission_roughness.xyz = emission;
    shData.emission_roughness.w = roughness;

    return shData;
}
/*! Traces the given ray (with normalized ray_dir). If it hits a scene surface,
	it constructs the shading data and returns true. Otherwise, it returns
	false and only writes the sky emission to the shading data.*/
bool traceRay(out rqShadingData out_shading_data, vec3 ray_origin, vec3 ray_dir) {
	// Trace a ray
	rayQueryEXT ray_query;
	rayQueryInitializeEXT(ray_query, topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, ray_origin, 1.0e-3, ray_dir, 1e38);
	while (rayQueryProceedEXT(ray_query)) {}
	// If there was no hit, use the sky color
	if (rayQueryGetIntersectionTypeEXT(ray_query, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
		out_shading_data.emission_roughness.xyz = vec3(1., 1., 1.);
		return false;
	}
	// Construct shading data
	else {
        int instance_id = rayQueryGetIntersectionInstanceIdEXT(ray_query, true);
		int primitive_id = rayQueryGetIntersectionPrimitiveIndexEXT(ray_query, true);
		vec2 barys = rayQueryGetIntersectionBarycentricsEXT(ray_query, true);
		bool front = rayQueryGetIntersectionFrontFaceEXT(ray_query, true);
		out_shading_data = getShadingData(instance_id, primitive_id, barys, front, -ray_dir);
		return true;
	}
}

vec3 pathTraceBrdf(vec3 ray_origin, vec3 ray_dir, inout uvec2 seed, uint PATH_LENGTH) {
	vec3 throughput_weight = vec3(1.0);
	vec3 radiance = vec3(0.0);
	[[unroll]]
	for (uint k = 1; k != PATH_LENGTH + 1; ++k) {
		rqShadingData s;
		bool hit = traceRay(s, ray_origin, ray_dir);
		radiance += throughput_weight * s.emission_roughness.xyz;
		if (hit && k < PATH_LENGTH) {
			// Sample the BRDF and update the ray
			ray_origin = s.worldPos.xyz;
			ray_dir = sample_frostbite_brdf(s, get_random_numbers(seed));
			float density = get_frostbite_brdf_density(s, ray_dir);
			// The sample may have ended up in the lower hemisphere
			float lambert_in = dot(s.normal_.xyz, ray_dir);
			if (lambert_in <= 0.0)
				break;
			// Update the throughput weight
			throughput_weight *= frostbite_brdf(s, ray_dir) * lambert_in / density;
		}
		else
			// End the path
			break;
	}
	return radiance;
}

float calculateAmbientOcclusion(vec3 object_point, vec3 object_normal)
{
	const float ao_mult = 1;
	uint max_ao_each = 3;
	uint max_ao = max_ao_each * max_ao_each;
	const float max_dist = 2;
	const float tmin = 0.01, tmax = max_dist;
	float accumulated_ao = 0.f;
	vec3 u = abs(dot(object_normal, vec3(0, 0, 1))) > 0.9 ? cross(object_normal, vec3(1, 0, 0)) : cross(object_normal, vec3(0, 0, 1));
	vec3 v = cross(object_normal, u);
	float accumulated_factor = 0;
	[[unroll]]
	for (uint j = 0; j < max_ao_each; ++j)
	{
		float phi = 0.5*(-3.14159 + 2 * 3.14159 * (float(j + 1) / float(max_ao_each + 2)));
		[[unroll]]
		for (uint k = 0; k < max_ao_each; ++k){
			float theta =  0.5*(-3.14159 + 2 * 3.14159 * (float(k + 1) / float(max_ao_each + 2)));
			float x = cos(phi) * sin(theta);
			float y = sin(phi) * sin(theta);
			float z = cos(theta);
			vec3 direction = x * u + y * v + z * object_normal;

			rayQueryEXT query;
			rayQueryInitializeEXT(query, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, object_point, tmin, direction.xyz, tmax);
			rayQueryProceedEXT(query);
			float dist = max_dist;
			if (rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT)
			{
				dist = rayQueryGetIntersectionTEXT(query, true);
			}
			float ao = min(dist, max_dist);
			float factor = 0.2 + 0.8 * z * z;
			accumulated_factor += factor;
			accumulated_ao += ao * factor;
		}
	}
	accumulated_ao /= (max_dist * accumulated_factor);
	accumulated_ao *= accumulated_ao;
	accumulated_ao = max(min((accumulated_ao) * ao_mult, 1), 0);
	return accumulated_ao; 
}