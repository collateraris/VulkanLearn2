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
    // Accumulate linear radiance at the exact same pixel. Filtering the history
    // on every frame would repeatedly blur it and lose energy at image edges.
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec3 current = texelFetch(currentFrame, pixel, 0).rgb;
    bool currentValid = !any(isnan(current)) && !any(isinf(current));

    // Do not read undefined history on the first frame or after a reset.
    if (perFrame.initLastFrame == 0 || perFrame.accumCount == 0)
    {
        outFragColor = vec4(currentValid ? current : vec3(0.0), 1.0);
        return;
    }

    vec3 previous = texelFetch(prevFrame, pixel, 0).rgb;
    if (any(isnan(previous)) || any(isinf(previous)))
    {
        outFragColor = vec4(currentValid ? current : vec3(0.0), 1.0);
        return;
    }

    // A bad path must not poison all subsequent frames. Otherwise update the
    // FP32 running mean without multiplying HDR radiance by the frame count.
    float weight = 1.0 / (float(perFrame.accumCount) + 1.0);
    outFragColor = vec4(currentValid ? previous + (current - previous) * weight : previous, 1.0);
}
