#pragma once

#include <vk_render_graph.h>

class VulkanEngine;

// Read-only assets exposed by the bindless scene set. Reservoirs and per-frame
// uniforms are declared separately by the passes that actually use them.
std::vector<rg::Use> import_scene_reads(VulkanEngine& engine, rg::RenderGraph& graph, rhi::Stage stage);
