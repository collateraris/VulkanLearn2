#extension GL_EXT_ray_query : enable

float calculate_ambient_occlusion(vec3 object_point, vec3 object_normal, accelerationStructureEXT topLevelAS)
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
	for (uint j = 0; j < max_ao_each; ++j)
	{
		float phi = 0.5*(-3.14159 + 2 * 3.14159 * (float(j + 1) / float(max_ao_each + 2)));
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