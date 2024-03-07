#version 460

#extension GL_EXT_nonuniform_qualifier:enable

layout (location = 0) in PerVertexData
{  
  flat uint drawID;
} fragIn;

layout (location = 0) out uvec2 vbuffer;

layout(early_fragment_tests) in;
void main()
{
	vbuffer = uvec2(uint(fragIn.drawID  + 1), uint(gl_PrimitiveID));
}