#version 460
layout(row_major) uniform;
layout(row_major) buffer;

#line 9 0
layout(binding = 0)
uniform texture2D giTex_0;


#line 11
layout(binding = 0)
uniform sampler giTex_Sampler_0;


#line 12039 1
layout(location = 0)
out vec4 entryPointParam_main_0;


#line 12039
layout(location = 0)
in vec2 vo_uv_0;


#line 14 0
void main()
{
    vec3 gi_0 = (texture(sampler2D(giTex_0,giTex_Sampler_0), (vo_uv_0))).xyz;

#line 16
    entryPointParam_main_0 = vec4(pow(gi_0 / (gi_0 + vec3(1.0)), vec3(0.45454543828964233)), 1.0);

#line 16
    return;
}

