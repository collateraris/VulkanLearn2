#include <texture_alpha_cutout.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

// A real 4x1 RGBA PNG: white alpha 0, white alpha 127, black alpha 128,
// black alpha 255. Its RGB channels intentionally disagree with the mask.
constexpr uint8_t png[] = {
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,1,8,6,0,0,0,249,60,15,205,
    0,0,0,22,73,68,65,84,120,156,99,248,255,255,63,3,16,215,51,48,48,52,0,241,127,0,
    85,185,7,249,205,255,240,172,0,0,0,0,73,69,78,68,174,66,96,130
};

void test_decoded_png()
{
    int width = 0, height = 0, channels = 0;
    auto* pixels = stbi_load_from_memory(png, int(sizeof(png)), &width, &height, &channels, STBI_rgb_alpha);
    check(pixels && width == 4 && height == 1 && channels == 4, "RGBA PNG decoding failed");
    const bool alpha0 = rgba8_has_alpha_cutout(pixels, 1);
    const bool alpha127 = rgba8_has_alpha_cutout(pixels + 4, 1);
    const bool alpha128 = rgba8_has_alpha_cutout(pixels + 8, 1);
    const bool alpha255 = rgba8_has_alpha_cutout(pixels + 12, 1);
    const bool complete = rgba8_has_alpha_cutout(pixels, 4);
    const bool empty = rgba8_has_alpha_cutout(pixels, 0);
    stbi_image_free(pixels);
    check(alpha0 && alpha127 && !alpha128 && !alpha255 && complete && !empty,
        "Cutout scan used RGB or disagreed with the shader's 0.5 alpha cutoff");
    check(!rgba8_has_alpha_cutout(nullptr, 0), "An empty texture was classified as a mask");
}

void test_material_and_mesh_classification()
{
    const AlphaCutoutMaterial objLeaf{false, false, true, true};
    const AlphaCutoutMaterial explicitOpaque{false, false, false, true};
    const AlphaCutoutMaterial explicitMask{false, true, false, false};
    const AlphaCutoutMaterial legacyOpacityMap{true, false, true, false};
    const AlphaCutoutMaterial bistroGlass{false, false, true, false};
    check(merge_mesh_alpha_cutout(false, objLeaf), "OBJ diffuse alpha did not enable any-hit");
    check(!merge_mesh_alpha_cutout(false, explicitOpaque), "Explicit OPAQUE inferred diffuse transparency");
    check(merge_mesh_alpha_cutout(false, explicitMask), "Explicit mask behavior changed");
    check(merge_mesh_alpha_cutout(false, legacyOpacityMap), "Existing opacity-map behavior changed");
    check(!merge_mesh_alpha_cutout(false, bistroGlass), "Constant glass opacity enabled the reverted transparency path");
    bool shared = merge_mesh_alpha_cutout(false, objLeaf);
    shared = merge_mesh_alpha_cutout(shared, explicitOpaque);
    check(shared, "The final opaque instance disabled a shared mesh's alpha test");
    shared = merge_mesh_alpha_cutout(false, explicitOpaque);
    shared = merge_mesh_alpha_cutout(shared, objLeaf);
    check(shared, "Shared-mesh classification depends on instance order");
    shared = false;
    check(!merge_mesh_alpha_cutout(shared, bistroGlass), "A fresh scene retained a prior mesh's mask state");
}

void test_scene_textures(const std::filesystem::path& directory)
{
    for (const char* name : {"citrus_limon_leaf.png", "aglaonema_leaf.png", "sm_leaf_02a.png", "dracaena_fragrans_leaf.png"}) {
        const auto path = (directory / name).string();
        int width = 0, height = 0, channels = 0;
        auto* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        check(pixels && width > 0 && height > 0, "Could not load a requested San Miguel leaf texture");
        const std::size_t count = std::size_t(width) * std::size_t(height);
        const bool cutout = rgba8_has_alpha_cutout(pixels, count);
        std::size_t discarded = 0;
        for (std::size_t i = 0; i < count; ++i) discarded += pixels[4 * i + 3] < 128;
        stbi_image_free(pixels);
        check(cutout && discarded > 0 && discarded < count, "San Miguel leaf alpha mask was lost");
        std::cout << name << ": cutout coverage " << double(discarded) / double(count) << '\n';
    }
}
}

int main(int argc, char** argv)
{
    try {
        test_decoded_png();
        test_material_and_mesh_classification();
        if (argc > 1) test_scene_textures(argv[1]);
        std::cout << "Texture alpha-cutout regression tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Texture alpha-cutout regression failed: " << error.what() << '\n';
        return 1;
    }
}
