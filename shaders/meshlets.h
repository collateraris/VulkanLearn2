struct s_vertex
{
	float vx, vy, vz;
	uint8_t nx, ny, nz, nw;
	float16_t tu, tv;
};

struct s_meshlet
{
    vec4 cone;
	uint dataOffset;
	uint8_t triangleCount;
	uint8_t vertexCount;
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