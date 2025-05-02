#version 460

//shader input
layout (location = 0) in vec2 texCoord;
//output write
layout (location = 0) out vec4 outFragColor;

struct PerFrameCB
{
    uint accumCount;
    uint initLastFrame;
    uint pad1;
    uint pad2;
};

layout(set = 0, binding = 0) uniform _PerFrameCB { PerFrameCB perFrame; };

layout(set = 1, binding = 0) uniform sampler2D currentFrame;
layout(set = 1, binding = 1) uniform sampler2D prevFrame;

void main()
{
	vec4 curColor = texture(currentFrame,texCoord).rgba;
    vec4 prevColor = texture(prevFrame,texCoord).rgba;
    if (perFrame.initLastFrame == 0)
	    outFragColor = curColor;
    else
        outFragColor = curColor;//(perFrame.accumCount * prevColor + curColor) / (perFrame.accumCount + 1);
}