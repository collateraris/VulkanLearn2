#pragma once

#include <cstdint>

// Output size is independent of the ray-tracing resolution selected by DLSS.
// A zero size asks the engine to load the authored configuration.
struct RenderSettings
{
    uint32_t outputWidth = 0;
    uint32_t outputHeight = 0;
    int dlssMode = 0; // Off, Quality, Balanced, Performance, Ultra Performance, DLAA.
};
