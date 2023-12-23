#version 460

#extension GL_EXT_nonuniform_qualifier:enable

layout (location = 0) in PerVertexData
{
  flat uint meshIndex;
} fragIn;

layout(set = 2, binding = 0, r16ui) uniform uimage2D vbuffer;

layout(early_fragment_tests) in;
void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    imageStore(vbuffer, coord, uvec4(fragIn.meshIndex, fragIn.meshIndex, fragIn.meshIndex, fragIn.meshIndex));
}