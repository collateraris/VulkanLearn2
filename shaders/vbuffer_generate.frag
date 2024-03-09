#version 460

#extension GL_EXT_nonuniform_qualifier:enable

layout (location = 0) out uvec2 vbuffer;

layout(early_fragment_tests) in;
void main()
{
  //gl_Layer contains meshletIndex
	vbuffer.xy = uvec2(uint(gl_Layer  + 1), uint(gl_PrimitiveID));
}