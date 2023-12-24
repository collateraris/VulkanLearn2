#version 460

#extension GL_EXT_nonuniform_qualifier:enable

layout(location = 0) in flat uint drawID;
layout(location = 0) out vec4 outColour;

layout(set = 1, binding = 0, rg32ui) uniform uimage2D vbuffer;

layout(early_fragment_tests) in;
void main()
{
	  // Write to colour attachments to avoid undefined behaviour
	  outColour = vec4(0.0); 

    ivec2 coord = ivec2(gl_FragCoord.xy);
    imageStore(vbuffer, coord, uvec4(drawID, gl_PrimitiveID, drawID, drawID));
    
}