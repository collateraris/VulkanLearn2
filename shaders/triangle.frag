#version 460

//shader input
layout (location = 0) in vec2 texCoord;
//output write
layout (location = 0) out vec4 outFragColor;

layout(set = 2, binding = 0) uniform sampler2D tex1;

void main()
{
	vec3 color = texture(tex1,texCoord).xyz;
	outFragColor = vec4(color,1.0f);
}