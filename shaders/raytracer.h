
// Uniform buffer set at each frame
struct GlobalUniforms
{
  mat4 viewProj;     // Camera view * projection
  mat4 viewInverse;  // Camera inverse view matrix
  mat4 projInverse;  // Camera inverse projection matrix
};

struct hitPayload
{
  vec3 hitValue;
};

struct ObjectData
{
  uint meshIndex;
	uint diffuseTexIndex;
	uint pad1;
  uint pad2;
};

struct Vertex {

	vec4 positionXYZ_normalX;
	vec4 normalYZ_texCoordUV;
};