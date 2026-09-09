#pragma once

#include <cstddef>
#include <cstdint>

// The active any-hit and inline-query shaders discard alpha < 0.5. Alpha in
// an RGBA8 sRGB image stays linear: byte 127 is discarded, byte 128 is retained.
inline bool rgba8_has_alpha_cutout(const uint8_t* pixels, std::size_t pixelCount)
{
    if (!pixels) return false;
    for (std::size_t i = 0; i < pixelCount; ++i)
        if (pixels[4 * i + 3] < 128) return true;
    return false;
}

struct AlphaCutoutMaterial {
    bool hasOpacityTexture = false;
    bool explicitlyNonOpaque = false;
    bool inferDiffuseAlpha = false;
    bool diffuseHasCutoutAlpha = false;
};

inline bool merge_mesh_alpha_cutout(bool previous, const AlphaCutoutMaterial& material)
{
    return previous || material.hasOpacityTexture || material.explicitlyNonOpaque ||
        (material.inferDiffuseAlpha && material.diffuseHasCutoutAlpha);
}
