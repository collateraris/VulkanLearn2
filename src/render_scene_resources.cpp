#include <render_scene_resources.h>

#include <vk_engine.h>
#include <vk_light_manager.h>
#include <vk_mesh.h>
#include <vk_resource_manager.h>

#include <unordered_set>

std::vector<rg::Use> import_scene_reads(VulkanEngine& engine, rg::RenderGraph& graph, rhi::Stage stage)
{
    const auto& scene = engine._resManager;
    std::vector<rg::Use> uses;
    uses.reserve(scene.meshList.size() * 2 + scene.textureList.size() + 5);
    std::unordered_set<rg::ResourceHandle> seen;
    const auto addBuffer = [&](std::string name, const AllocatedBuffer& buffer) {
        if (buffer._buffer == VK_NULL_HANDLE) return;
        const auto resource = graph.import_resource(std::move(name), engine._rhi.buffer(buffer), true);
        if (seen.insert(resource).second)
            uses.push_back({resource, {stage, rhi::Access::ShaderRead, rhi::Layout::Undefined}});
    };

    // Match ResourceManager::init_global_bindless_descriptor bindings 0 and 5:
    // shaders index the RT geometry buffers, not raster/meshlet variants.
    for (size_t i = 0; i < scene.meshList.size(); ++i)
    {
        const auto& mesh = scene.meshList[i];
        if (!mesh) continue;
        const auto prefix = "Scene.Mesh" + std::to_string(i);
        addBuffer(prefix + ".Vertices", mesh->_vertexBufferRT);
        addBuffer(prefix + ".Indices", mesh->_indicesBufferRT);
    }
    addBuffer("Scene.Objects", scene.globalObjectBuffer);
    addBuffer("Scene.Materials", scene.globalMaterialBuffer);
    addBuffer("Scene.Lights", engine._lightManager.get_light_buffer());
    addBuffer("Scene.LightAliasTable", engine._lightManager.get_lights_alias_table_buffer());
    addBuffer("Scene.LightGrid", engine._lightManager.get_lights_cell_grid_buffer());

    // Binding 1 uses GENERAL even for sampled reads. This includes the optional
    // equirectangular HDR appended to the same bindless texture array.
    for (size_t i = 0; i < scene.textureList.size(); ++i)
    {
        const Texture* texture = scene.textureList[i];
        if (!texture || texture->image._image == VK_NULL_HANDLE || texture->imageView == VK_NULL_HANDLE)
            continue;
        const auto resource = graph.import_resource("Scene.Texture" + std::to_string(i), engine._rhi.image(*texture), true);
        // Multiple descriptor indices can refer to the same cached texture.
        if (seen.insert(resource).second)
            uses.push_back({resource, {stage, rhi::Access::ShaderRead, rhi::Layout::General}});
    }

    // The TLAS descriptor is also read by tracing/query shaders. BLAS/TLAS
    // construction completes through immediate_submit before the frame graph,
    // and no active frame pass updates them. Their private backing allocations
    // are not represented by a fabricated shader-storage buffer dependency.
    return uses;
}
