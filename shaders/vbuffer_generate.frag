#version 460

#extension GL_EXT_nonuniform_qualifier:enable

layout(location = 0) in flat uint drawID;
layout(location = 0) out uvec2 outColour;

layout(early_fragment_tests) in;
void main()
{
    // Write to colour attachments to avoid undefined behaviour
    outColour = uvec2(uint(drawID  + 1), uint(gl_PrimitiveID + 1));
}