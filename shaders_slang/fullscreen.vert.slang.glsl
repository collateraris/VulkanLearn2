#version 460
layout(row_major) uniform;
layout(row_major) buffer;

#line 3 0
layout(location = 0)
out vec2 entryPointParam_main_uv_0;


#line 3
struct VertexShaderOutput_0
{
    vec4 pos_0;
    vec2 uv_0;
};


void main()
{

#line 10
    uint _S1 = uint(gl_VertexIndex);

    const vec2 _S2 = vec2(0.0, 0.0);

#line 12
    VertexShaderOutput_0 o_0;

#line 12
    o_0.pos_0 = vec4(0.0, 0.0, 0.0, 0.0);

#line 12
    o_0.uv_0 = _S2;
    vec2 _S3 = vec2(float((_S1 << 1) & 2U), float(_S1 & 2U));

#line 13
    o_0.uv_0 = _S3;
    o_0.pos_0 = vec4(_S3 * 2.0 + -1.0, 0.0, 1.0);
    VertexShaderOutput_0 _S4 = o_0;

#line 15
    gl_Position = o_0.pos_0;

#line 15
    entryPointParam_main_uv_0 = _S4.uv_0;

#line 15
    return;
}

