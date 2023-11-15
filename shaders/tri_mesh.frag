#version 460

layout (location = 0) in PerVertexData
{
  vec4 color;
  vec2 texCoord;
} fragIn;

layout (location = 0) out vec4 FragColor;

layout(set = 2, binding = 0) uniform sampler2D tex0;

layout(early_fragment_tests) in;
void main()
{
  vec3 color = texture(tex0,fragIn.texCoord).xyz;
	FragColor = vec4(color,1.0f) * fragIn.color;
}