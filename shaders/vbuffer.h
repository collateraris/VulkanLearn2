#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_EXT_shader_explicit_arithmetic_types: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16: enable

struct SGlobalCamera
{
  mat4 viewProj;     // Camera view * projection
};

struct SVertex_VisB
{
	vec4 position;
};

struct SMeshlet
{
	uint dataOffset;
	uint8_t triangleCount;
	uint8_t vertexCount;
};

struct SObjectData
{
	mat4 model;
	uint meshletCount;
   	uint meshIndex;
	uint pad1;
    uint pad2;
};