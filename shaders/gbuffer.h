#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_EXT_shader_explicit_arithmetic_types: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16: enable

struct SGlobalCamera
{
  mat4 viewProj;     // Camera view * projection
};

struct SVertex
{
	float vx, vy, vz;
	uint8_t nx, ny, nz, nw;
	float16_t tu, tv;
};

struct SMeshlet
{
	uint dataOffset;
	uint8_t triangleCount;
	uint8_t vertexCount;
	float16_t pad;
};

struct SObjectData
{
	mat4 model;
	uint meshIndex;
	uint meshletCount;
	uint pad1;
	uint pad2;
};