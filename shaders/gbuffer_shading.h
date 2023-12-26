#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_EXT_shader_explicit_arithmetic_types: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16: enable

struct SGlobalCamera
{
  mat4 viewProj;     // Camera view * projection
};

struct SObjectData
{
	mat4 model;
	uint meshIndex;
	uint diffuseTexIndex;
	uint pad1;
	uint pad2;
};