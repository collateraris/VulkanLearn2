struct s_vertex
{
	float vx, vy, vz;
	uint8_t nx, ny, nz, nw;
	float16_t tu, tv;
};

struct s_meshlet
{
	uint dataOffset;
	uint8_t triangleCount;
	uint8_t vertexCount;
	float center_radius[4];
	float aabb_min[3];
	float aabb_max[3];
};

struct ObjectData
{
	mat4 model;
	vec4 center_radius;
	uint meshletCount;
};

struct MeshDrawCommand
{
    uint taskCount;
    uint firstTask;
};