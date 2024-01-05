#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;

layout(push_constant) uniform PushConsts {
	layout (offset = 0) mat4 mvp;
} pushConsts;

layout (location = 0) out vec3 localPos;

void main() 
{
	localPos = vPosition;
	gl_Position = pushConsts.mvp * vec4(vPosition.xyz, 1.0);
}