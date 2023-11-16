#version 460

#extension GL_EXT_nonuniform_qualifier:enable

layout (location = 0) in PerVertexData
{
  vec4 color;
  vec2 texCoord;
  flat uint diffuseTexIndex;
} fragIn;

layout (location = 0) out vec4 FragColor;

layout(set = 0, binding = 3) uniform sampler2D tex0[];

layout(early_fragment_tests) in;
void main()
{
  vec3 color = texture(tex0[fragIn.diffuseTexIndex],fragIn.texCoord).xyz;
	FragColor = vec4(color,1.0f) * fragIn.color;
}