#include <vk_render_graph.h>

#include <vk_utils.h>
#include <vk_engine.h>

vk_rgraph::VulkanRenderGraph::VulkanRenderGraph()
{

}

void vk_rgraph::VulkanRenderGraph::init(VulkanEngine* engine)
{
	_engine = engine;
}

vk_rgraph::VulkanRenderPass& vk_rgraph::VulkanRenderGraph::add_pass(const std::string& name, RenderGraphQueueFlagBits queue)
{
	auto itr = _pass_to_indexMap.find(name);
	if (itr != end(_pass_to_indexMap))
	{
		return *_passesList[itr->second];
	}
	else
	{
		uint32_t index = _passesList.size();
		_passesList.emplace_back(std::make_unique<VulkanRenderPass>(this, index, queue, name));
		_pass_to_indexMap[name] = index;
		return *_passesList.back();
	}
}

vk_rgraph::VulkanRenderPass* vk_rgraph::VulkanRenderGraph::find_pass(const std::string& name)
{
	auto itr = _pass_to_indexMap.find(name);
	if (itr != end(_pass_to_indexMap))
		return _passesList[itr->second].get();
	else
		return nullptr;
}

void vk_rgraph::VulkanRenderGraph::bake()
{
	//for (auto& pass : _passesList)
	//	pass->setup_dependencies();

	// First, validate that the graph is sane.
	validate_passes();

	auto itr = _resource_to_indexMap.find(_backbuffer_source);
	if (itr == std::end(_resource_to_indexMap))
		throw std::logic_error("Backbuffer source does not exist.");

	_pass_stack.clear();

	_pass_dependenciesList.clear();
	_pass_merge_dependenciesList.clear();
	_pass_dependenciesList.resize(_passesList.size());
	_pass_merge_dependenciesList.resize(_passesList.size());

	// Work our way back from the backbuffer, and sort out all the dependencies.
	auto& backbuffer_resource = *_resourcesList[itr->second];

	if (backbuffer_resource.get_write_passes().empty())
		throw std::logic_error("No pass exists which writes to resource.");

	for (auto& pass : backbuffer_resource.get_write_passes())
		_pass_stack.push_back(pass);

	auto tmp_pass_stack = _pass_stack;
	for (auto& pushed_pass : tmp_pass_stack)
	{
		auto& pass = *_passesList[pushed_pass];
		traverse_dependencies(pass, 0);
	}

	std::reverse(std::begin(_pass_stack), std::end(_pass_stack));
	filter_passes(_pass_stack);

	// Now, reorder passes to extract better pipelining.
	reorder_passes(_pass_stack);

	// Now, we have a linear list of passes to submit in-order which would obey the dependencies.

	// Figure out which physical resources we need. Here we will alias resources which can trivially alias via renaming.
	// E.g. depth input -> depth output is just one physical attachment, similar with color.
	build_physical_resources();

	// Next, try to merge adjacent passes together.
	build_physical_passes();

	// After merging physical passes and resources, if an image resource is only used in a single physical pass, make it transient.
	build_transients();

	// Now that we are done, we can make render passes.
	build_render_pass_info();

	// For each render pass in isolation, figure out the barriers required.
	build_barriers();

	// Check if the swapchain needs to be blitted to in case the geometry does not match the backbuffer,
	// or the usage of the image makes that impossible.
	_swapchain_physical_index = _resourcesList[_resource_to_indexMap[_backbuffer_source]]->get_physical_index();

	auto& backbuffer_dim = _physical_dimensionsList[_swapchain_physical_index];

	// If resource is touched in async-compute, we cannot alias with swapchain.
	// If resource is not transient, it's being used in multiple physical passes,
	// we can't use the implicit subpass dependencies for dealing with swapchain.
	bool can_alias_backbuffer = (backbuffer_dim.queues & compute_queues) == 0 &&
		(backbuffer_dim.flags & ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT) != 0;

	// Resources which do not alias with the backbuffer should not be pre-rotated.
	for (auto& dim : _physical_dimensionsList)
		if (&dim != &backbuffer_dim)
			dim.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	if (vk_utils::surface_transform_swaps_xy(backbuffer_dim.transform))
		std::swap(backbuffer_dim.image_extend.width, backbuffer_dim.image_extend.height);

	backbuffer_dim.flags &= ~(ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT | ATTACHMENT_INFO_SUPPORTS_PREROTATE_BIT);
	backbuffer_dim.flags |= _swapchain_dimensions.flags & ATTACHMENT_INFO_PERSISTENT_BIT;

	if (!can_alias_backbuffer || backbuffer_dim != _swapchain_dimensions)
	{

		_swapchain_physical_index = std::numeric_limits<uint32_t>::max();
		backbuffer_dim.queues |= RENDER_GRAPH_QUEUE_GRAPHICS_BIT;

		// We will need to sample from the image to blit to backbuffer.
		backbuffer_dim.image_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

		// Don't use pre-transform if we can't alias anyways.
		if (vk_utils::surface_transform_swaps_xy(backbuffer_dim.transform))
			std::swap(backbuffer_dim.image_extend.width, backbuffer_dim.image_extend.height);
		backbuffer_dim.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
		_physical_dimensionsList[_swapchain_physical_index].flags |= ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT;

	// Based on our render graph, figure out the barriers we actually need.
	// Some barriers are implicit (transients), and some are redundant, i.e. same texture read in multiple passes.
	build_physical_barriers();
}

void vk_rgraph::VulkanRenderGraph::reset()
{

}

void vk_rgraph::VulkanRenderGraph::filter_passes(std::vector<uint32_t>& list)
{
	std::unordered_set<uint32_t> seen;

	auto output_itr = begin(list);
	for (auto itr = begin(list); itr != end(list); ++itr)
	{
		if (!seen.count(*itr))
		{
			*output_itr = *itr;
			seen.insert(*itr);
			++output_itr;
		}
	}
	list.erase(output_itr, end(list));
}

void vk_rgraph::VulkanRenderGraph::validate_passes()
{
	for (auto& pass_ptr : _passesList)
	{
		auto& pass = *pass_ptr;

		if (pass.get_color_inputs().size() != pass.get_color_outputs().size())
			throw std::logic_error("Size of color inputs must match color outputs.");

		if (pass.get_storage_inputs().size() != pass.get_storage_outputs().size())
			throw std::logic_error("Size of storage inputs must match storage outputs.");

		if (pass.get_blit_texture_inputs().size() != pass.get_blit_texture_outputs().size())
			throw std::logic_error("Size of blit inputs must match blit outputs.");

		if (pass.get_storage_texture_inputs().size() != pass.get_storage_texture_outputs().size())
			throw std::logic_error("Size of storage texture inputs must match storage texture outputs.");

		if (!pass.get_resolve_outputs().empty() && pass.get_resolve_outputs().size() != pass.get_color_outputs().size())
			throw std::logic_error("Must have one resolve output for each color output.");

		uint32_t num_inputs = pass.get_color_inputs().size();
		for (uint32_t i = 0; i < num_inputs; i++)
		{
			if (!pass.get_color_inputs()[i])
				continue;

			if (get_resource_dimensions(*pass.get_color_inputs()[i]) != get_resource_dimensions(*pass.get_color_outputs()[i]))
				pass.make_color_input_scaled(i);
		}

		if (!pass.get_storage_outputs().empty())
		{
			uint32_t num_outputs = pass.get_storage_outputs().size();
			for (uint32_t i = 0; i < num_outputs; i++)
			{
				if (!pass.get_storage_inputs()[i])
					continue;

				if (pass.get_storage_outputs()[i]->get_buffer_info() != pass.get_storage_inputs()[i]->get_buffer_info())
					throw std::logic_error("Doing RMW on a storage buffer, but usage and sizes do not match.");
			}
		}

		if (!pass.get_blit_texture_outputs().empty())
		{
			uint32_t num_outputs = pass.get_blit_texture_outputs().size();
			for (uint32_t i = 0; i < num_outputs; i++)
			{
				if (!pass.get_blit_texture_inputs()[i])
					continue;

				if (get_resource_dimensions(*pass.get_blit_texture_inputs()[i]) != get_resource_dimensions(*pass.get_blit_texture_outputs()[i]))
					throw std::logic_error("Doing RMW on a blit image, but usage and sizes do not match.");
			}
		}

		if (!pass.get_storage_texture_outputs().empty())
		{
			uint32_t num_outputs = pass.get_storage_texture_outputs().size();
			for (uint32_t i = 0; i < num_outputs; i++)
			{
				if (!pass.get_storage_texture_inputs()[i])
					continue;

				if (get_resource_dimensions(*pass.get_storage_texture_outputs()[i]) != get_resource_dimensions(*pass.get_storage_texture_inputs()[i]))
					throw std::logic_error("Doing RMW on a storage texture image, but sizes do not match.");
			}
		}

		if (pass.get_depth_stencil_input() && pass.get_depth_stencil_output())
		{
			if (get_resource_dimensions(*pass.get_depth_stencil_input()) != get_resource_dimensions(*pass.get_depth_stencil_output()))
				throw std::logic_error("Dimension mismatch.");
		}
	}
}

void vk_rgraph::VulkanRenderGraph::build_barriers()
{
	_pass_barriersList.clear();
	_pass_barriersList.reserve(_pass_stack.size());

	const auto get_access = [&](std::vector<Barrier>& barriers, uint32_t index, bool history) -> Barrier& {
		auto itr = find_if(begin(barriers), end(barriers), [index, history](const Barrier& b) {
			return index == b.resource_index && history == b.history;
			});
		if (itr != end(barriers))
			return *itr;
		else
		{
			barriers.push_back({ index, VK_IMAGE_LAYOUT_UNDEFINED, 0, 0, history });
			return barriers.back();
		}
	};

	for (auto& index : _pass_stack)
	{
		auto& pass = *_passesList[index];
		Barriers barriers;

		const auto get_invalidate_access = [&](uint32_t i, bool history) -> Barrier& {
			return get_access(barriers.invalidate, i, history);
		};

		const auto get_flush_access = [&](uint32_t i) -> Barrier& {
			return get_access(barriers.flush, i, false);
		};

		for (auto& input : pass.get_generic_buffer_inputs())
		{
			auto& barrier = get_invalidate_access(input.buffer->get_physical_index(), false);
			barrier.access |= input.access;
			barrier.stages |= input.stages;
			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = input.layout;
		}

		for (auto& input : pass.get_generic_texture_inputs())
		{
			auto& barrier = get_invalidate_access(input.texture->get_physical_index(), false);
			barrier.access |= input.access;
			barrier.stages |= input.stages;
			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = input.layout;
		}

		for (auto* input : pass.get_history_inputs())
		{
			auto& barrier = get_invalidate_access(input->get_physical_index(), true);
			barrier.access |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

			if ((pass.get_queue() & compute_queues) == 0)
				barrier.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // TODO: Pick appropriate stage.
			else
				barrier.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		for (auto* input : pass.get_attachment_inputs())
		{
			if (pass.get_queue() & compute_queues)
				throw std::logic_error("Only graphics passes can have input attachments.");

			auto& barrier = get_invalidate_access(input->get_physical_index(), false);
			barrier.access |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			barrier.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			if (vk_utils::format_has_depth_or_stencil_aspect(input->get_attachment_info().format))
			{
				// Need DEPTH_ATTACHMENT_READ here to satisfy loadOp = LOAD.
				barrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
				barrier.stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			}
			else
			{
				// Need COLOR_ATTACHMENT_READ here to satisfy loadOp = LOAD.
				barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
				barrier.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			}

			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		for (auto* input : pass.get_storage_inputs())
		{
			if (!input)
				continue;

			auto& barrier = get_invalidate_access(input->get_physical_index(), false);
			barrier.access |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
			if ((pass.get_queue() & compute_queues) == 0)
				barrier.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // TODO: Pick appropriate stage.
			else
				barrier.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
		}

		for (auto* input : pass.get_storage_texture_inputs())
		{
			if (!input)
				continue;

			auto& barrier = get_invalidate_access(input->get_physical_index(), false);
			barrier.access |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
			if ((pass.get_queue() & compute_queues) == 0)
				barrier.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // TODO: Pick appropriate stage.
			else
				barrier.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
		}

		for (auto* input : pass.get_blit_texture_inputs())
		{
			if (!input)
				continue;

			auto& barrier = get_invalidate_access(input->get_physical_index(), false);
			barrier.access |= VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.stages |= VK_PIPELINE_STAGE_2_BLIT_BIT;
			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		}

		for (auto* input : pass.get_color_inputs())
		{
			if (!input)
				continue;

			if (pass.get_queue() & compute_queues)
				throw std::logic_error("Only graphics passes can have color inputs.");

			auto& barrier = get_invalidate_access(input->get_physical_index(), false);
			barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			barrier.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			// If the attachment is also bound as an input attachment (programmable blending)
			// we need VK_IMAGE_LAYOUT_GENERAL.
			if (barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
			else if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			else
				barrier.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		for (auto* input : pass.get_color_scale_inputs())
		{
			if (!input)
				continue;

			if (pass.get_queue() & compute_queues)
				throw std::logic_error("Only graphics passes can have scaled color inputs.");

			auto& barrier = get_invalidate_access(input->get_physical_index(), false);
			barrier.access |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			barrier.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		for (auto* output : pass.get_color_outputs())
		{
			if (pass.get_queue() & compute_queues)
				throw std::logic_error("Only graphics passes can have color outputs.");

			auto& barrier = get_flush_access(output->get_physical_index());

			if ((_physical_dimensionsList[output->get_physical_index()].levels > 1) &&
				(_physical_dimensionsList[output->get_physical_index()].flags & ATTACHMENT_INFO_MIPGEN_BIT) != 0)
			{
				// access should be 0 here. generate_mipmaps will take care of invalidation needed.
				barrier.access |= VK_ACCESS_TRANSFER_READ_BIT; // Validation layers complain without this.
				barrier.stages |= VK_PIPELINE_STAGE_2_BLIT_BIT;
				if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
					throw std::logic_error("Layout mismatch.");
				barrier.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}
			else
			{
				barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				barrier.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

				// If the attachment is also bound as an input attachment (programmable blending)
				// we need VK_IMAGE_LAYOUT_GENERAL.
				if (barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
					barrier.layout == VK_IMAGE_LAYOUT_GENERAL)
				{
					barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
				}
				else if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
					throw std::logic_error("Layout mismatch.");
				else
					barrier.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
		}

		for (auto* output : pass.get_resolve_outputs())
		{
			if (pass.get_queue() & compute_queues)
				throw std::logic_error("Only graphics passes can resolve outputs.");

			auto& barrier = get_flush_access(output->get_physical_index());
			barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		for (auto* output : pass.get_blit_texture_outputs())
		{
			auto& barrier = get_invalidate_access(output->get_physical_index(), false);
			barrier.access |= VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.stages |= VK_PIPELINE_STAGE_2_BLIT_BIT;
			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		}

		for (auto* output : pass.get_storage_outputs())
		{
			auto& barrier = get_flush_access(output->get_physical_index());
			barrier.access |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

			if ((pass.get_queue() & compute_queues) == 0)
				barrier.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // TODO: Pick appropriate stage.
			else
				barrier.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
		}

		for (auto* output : pass.get_transfer_outputs())
		{
			auto& barrier = get_flush_access(output->get_physical_index());
			barrier.access |= VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.stages |= VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT;
			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
		}

		for (auto* output : pass.get_storage_texture_outputs())
		{
			auto& barrier = get_flush_access(output->get_physical_index());
			barrier.access |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

			if ((pass.get_queue() & compute_queues) == 0)
				barrier.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // TODO: Pick appropriate stage.
			else
				barrier.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
		}

		auto* output = pass.get_depth_stencil_output();
		auto* input = pass.get_depth_stencil_input();

		if ((output || input) && (pass.get_queue() & compute_queues))
			throw std::logic_error("Only graphics passes can have depth attachments.");

		if (output && input)
		{
			auto& dst_barrier = get_invalidate_access(input->get_physical_index(), false);
			auto& src_barrier = get_flush_access(output->get_physical_index());

			if (dst_barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				dst_barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
			else if (dst_barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			else
				dst_barrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			dst_barrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dst_barrier.stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

			src_barrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			src_barrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			src_barrier.stages |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		}
		else if (input)
		{
			auto& dst_barrier = get_invalidate_access(input->get_physical_index(), false);

			if (dst_barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				dst_barrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			else if (dst_barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			else
				dst_barrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

			dst_barrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			dst_barrier.stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		}
		else if (output)
		{
			auto& src_barrier = get_flush_access(output->get_physical_index());

			if (src_barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				src_barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
			else if (src_barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
				throw std::logic_error("Layout mismatch.");
			else
				src_barrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			src_barrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			src_barrier.stages |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		}

		_pass_barriersList.push_back(std::move(barriers));
	}
}

void vk_rgraph::VulkanRenderGraph::set_backbuffer_source(const std::string& name)
{
	_backbuffer_source = name;
}

vk_rgraph::ResourceDimensions vk_rgraph::VulkanRenderGraph::get_resource_dimensions(const vk_rgraph::VulkanRenderBufferResource& resource) const
{
	ResourceDimensions dim;
	auto& info = resource.get_buffer_info();
	dim.buffer_info = info;
	dim.buffer_info.usage |= resource.get_buffer_usage();
	dim.flags |= info.flags;
	dim.name = resource.get_name();
	return dim;
}

vk_rgraph::ResourceDimensions vk_rgraph::VulkanRenderGraph::get_resource_dimensions(const vk_rgraph::VulkanRenderTextureResource& resource) const
{
	ResourceDimensions dim;
	auto& info = resource.get_attachment_info();
	dim.layers = info.layers;
	dim.samples = info.samples;
	dim.format = info.format;
	dim.queues = resource.get_used_queues();
	dim.image_usage = info.aux_usage | resource.get_image_usage();
	dim.name = resource.get_name();
	dim.flags = info.flags & ~(ATTACHMENT_INFO_SUPPORTS_PREROTATE_BIT | ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT);

	if (resource.get_transient_state())
		dim.flags |= ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT;

	// Mark the resource as potentially supporting pre-rotate.
	// If this resource ends up aliasing with the swapchain, it might go through.
	if ((info.flags & ATTACHMENT_INFO_SUPPORTS_PREROTATE_BIT) != 0)
		dim.transform = _swapchain_dimensions.transform;

	switch (info.size_class)
	{
	case SizeClass::SwapchainRelative:
		dim.image_extend.width = std::max(uint32_t(std::ceil(info.size_x * _swapchain_dimensions.image_extend.width)), 1u);
		dim.image_extend.height = std::max(uint32_t(std::ceil(info.size_y * _swapchain_dimensions.image_extend.height)), 1u);
		dim.image_extend.depth = std::max(uint32_t(std::ceil(info.size_z)), 1u);
		if (vk_utils::surface_transform_swaps_xy(_swapchain_dimensions.transform))
			std::swap(dim.image_extend.width, dim.image_extend.height);
		break;

	case SizeClass::Absolute:
		dim.image_extend.width = std::max(uint32_t(info.size_x), 1u);
		dim.image_extend.height = std::max(uint32_t(info.size_y), 1u);
		dim.image_extend.depth = std::max(uint32_t(info.size_z), 1u);
		break;

	case SizeClass::InputRelative:
	{
		auto itr = _resource_to_indexMap.find(info.size_relative_name);
		if (itr == std::end(_resource_to_indexMap))
			throw std::logic_error("Resource does not exist.");
		auto& input = static_cast<VulkanRenderTextureResource&>(*_resourcesList[itr->second]);
		auto input_dim = get_resource_dimensions(input);

		dim.image_extend.width = std::max(uint32_t(std::ceil(input_dim.image_extend.width * info.size_x)), 1u);
		dim.image_extend.height = std::max(uint32_t(std::ceil(input_dim.image_extend.height * info.size_y)), 1u);
		dim.image_extend.depth = std::max(uint32_t(std::ceil(input_dim.image_extend.depth * info.size_z)), 1u);
		break;
	}
	}

	if (dim.format == VK_FORMAT_UNDEFINED)
		dim.format = _swapchain_dimensions.format;

	const auto num_levels = [](uint32_t width, uint32_t height, uint32_t depth) -> uint32_t {
		uint32_t levels = 0;
		uint32_t max_dim = std::max(std::max(width, height), depth);
		while (max_dim)
		{
			levels++;
			max_dim >>= 1;
		}
		return levels;
	};

	dim.levels = std::min(num_levels(dim.image_extend.width, dim.image_extend.height, dim.image_extend.depth), info.levels == 0 ? ~0u : info.levels);
	return dim;
}

void vk_rgraph::VulkanRenderGraph::set_backbuffer_dimensions(const ResourceDimensions& dim)
{
	_swapchain_dimensions = dim;
}

const vk_rgraph::ResourceDimensions& vk_rgraph::VulkanRenderGraph::get_backbuffer_dimensions() const
{
	return _swapchain_dimensions;
}

void vk_rgraph::VulkanRenderGraph::build_physical_passes()
{
	physical_passes.clear();
	PhysicalPass physical_pass;

	const auto find_attachment = [](const std::vector<VulkanRenderTextureResource*>& resource_list, const VulkanRenderTextureResource* resource) -> bool {
		if (!resource)
			return false;

		auto itr = std::find_if(std::begin(resource_list), std::end(resource_list), [resource](const VulkanRenderTextureResource* res) {
			return res->get_physical_index() == resource->get_physical_index();
			});
		return itr != end(resource_list);
	};

	const auto find_buffer = [](const std::vector<VulkanRenderBufferResource*>& resource_list, const VulkanRenderBufferResource* resource) -> bool {
		if (!resource)
			return false;

		auto itr = std::find_if(std::begin(resource_list), std::end(resource_list), [resource](const VulkanRenderBufferResource* res) {
			return res->get_physical_index() == resource->get_physical_index();
			});
		return itr != end(resource_list);
	};

	const auto should_merge = [&](const VulkanRenderPass& prev, const VulkanRenderPass& next) -> bool {
		// Can only merge graphics in same queue.
		if ((prev.get_queue() & compute_queues) || (next.get_queue() != prev.get_queue()))
			return false;

		for (auto* output : prev.get_color_outputs())
		{
			// Need to mip-map after this pass, so cannot merge.
			if ((_physical_dimensionsList[output->get_physical_index()].levels > 1) &&
				(_physical_dimensionsList[output->get_physical_index()].flags & ATTACHMENT_INFO_MIPGEN_BIT) != 0)
				return false;
		}

		// Need non-local dependency, cannot merge.
		for (auto& input : next.get_generic_texture_inputs())
		{
			if (find_attachment(prev.get_color_outputs(), input.texture))
				return false;
			if (find_attachment(prev.get_resolve_outputs(), input.texture))
				return false;
			if (find_attachment(prev.get_storage_texture_outputs(), input.texture))
				return false;
			if (find_attachment(prev.get_blit_texture_outputs(), input.texture))
				return false;
			if (input.texture && prev.get_depth_stencil_output() == input.texture)
				return false;
		}

		// Need non-local dependency, cannot merge.
		for (auto& input : next.get_generic_buffer_inputs())
			if (find_buffer(prev.get_storage_outputs(), input.buffer))
				return false;

		// Need non-local dependency, cannot merge.
		for (auto* input : next.get_blit_texture_inputs())
			if (find_attachment(prev.get_blit_texture_inputs(), input))
				return false;

		// Need non-local dependency, cannot merge.
		for (auto* input : next.get_storage_inputs())
			if (find_buffer(prev.get_storage_outputs(), input))
				return false;

		// Need non-local dependency, cannot merge.
		for (auto* input : next.get_storage_texture_inputs())
			if (find_attachment(prev.get_storage_texture_outputs(), input))
				return false;

		// Need non-local dependency, cannot merge.
		for (auto* input : next.get_color_scale_inputs())
		{
			if (find_attachment(prev.get_storage_texture_outputs(), input))
				return false;
			if (find_attachment(prev.get_blit_texture_outputs(), input))
				return false;
			if (find_attachment(prev.get_color_outputs(), input))
				return false;
			if (find_attachment(prev.get_resolve_outputs(), input))
				return false;
		}

		const auto different_attachment = [](const VulkanRenderResource* a, const VulkanRenderResource* b) {
			return a && b && a->get_physical_index() != b->get_physical_index();
		};

		const auto same_attachment = [](const VulkanRenderResource* a, const VulkanRenderResource* b) {
			return a && b && a->get_physical_index() == b->get_physical_index();
		};

		// Need a different depth attachment, break up the pass.
		if (different_attachment(next.get_depth_stencil_input(), prev.get_depth_stencil_input()))
			return false;
		if (different_attachment(next.get_depth_stencil_output(), prev.get_depth_stencil_input()))
			return false;
		if (different_attachment(next.get_depth_stencil_input(), prev.get_depth_stencil_output()))
			return false;
		if (different_attachment(next.get_depth_stencil_output(), prev.get_depth_stencil_output()))
			return false;

		for (auto* input : next.get_color_inputs())
		{
			if (!input)
				continue;
			if (find_attachment(prev.get_storage_texture_outputs(), input))
				return false;
			if (find_attachment(prev.get_blit_texture_outputs(), input))
				return false;
		}

		// Now, we have found all failure cases, try to see if we *should* merge.

		// Keep color on tile.
		for (auto* input : next.get_color_inputs())
		{
			if (!input)
				continue;
			if (find_attachment(prev.get_color_outputs(), input))
				return true;
			if (find_attachment(prev.get_resolve_outputs(), input))
				return true;
		}

		// Keep depth on tile.
		if (same_attachment(next.get_depth_stencil_input(), prev.get_depth_stencil_input()) ||
			same_attachment(next.get_depth_stencil_input(), prev.get_depth_stencil_output()))
		{
			return true;
		}

		// Keep depth attachment or color on-tile.
		for (auto* input : next.get_attachment_inputs())
		{
			if (find_attachment(prev.get_color_outputs(), input))
				return true;
			if (find_attachment(prev.get_resolve_outputs(), input))
				return true;
			if (input && prev.get_depth_stencil_output() == input)
				return true;
		}

		// No reason to merge, so don't.
		return false;
	};

	for (uint32_t index = 0; index < _pass_stack.size(); )
	{
		uint32_t merge_end = index + 1;
		for (; merge_end < _pass_stack.size(); merge_end++)
		{
			bool merge = true;
			for (uint32_t merge_start = index; merge_start < merge_end; merge_start++)
			{
				if (!should_merge(*_passesList[_pass_stack[merge_start]], *_passesList[_pass_stack[merge_end]]))
				{
					merge = false;
					break;
				}
			}

			if (!merge)
				break;
		}

		physical_pass.passes.insert(end(physical_pass.passes), begin(_pass_stack) + index, begin(_pass_stack) + merge_end);
		physical_passes.push_back(std::move(physical_pass));
		index = merge_end;
	}

	for (auto& phys_pass : physical_passes)
	{
		uint32_t index = uint32_t(&phys_pass - physical_passes.data());
		for (auto& pass : phys_pass.passes)
			_passesList[pass]->set_physical_pass_index(index);
	}
}

void vk_rgraph::VulkanRenderGraph::build_transients()
{
	std::vector<uint32_t> physical_pass_used(_physical_dimensionsList.size());

	for (auto& dim : _physical_dimensionsList)
	{
		// Buffers are never transient.
		// Storage images are never transient.
		if (dim.is_buffer_like())
			dim.flags &= ~ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT;
		else
			dim.flags |= ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT;

		auto index = uint32_t(&dim - _physical_dimensionsList.data());
		if (_physical_history_image_attachmentsList[index])
			dim.flags &= ~ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT;

		if (vk_utils::format_has_depth_or_stencil_aspect(dim.format))
			dim.flags &= ~ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT;
		else
			dim.flags |= ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT;
	}

	for (auto& resource : _resourcesList)
	{
		if (resource->get_type() != VulkanRenderResource::Type::Texture)
			continue;

		uint32_t physical_index = resource->get_physical_index();
		if (physical_index == std::numeric_limits<uint32_t>::max())
			continue;

		for (auto& pass : resource->get_write_passes())
		{
			uint32_t phys = _passesList[pass]->get_physical_pass_index();
			if (phys != std::numeric_limits<uint32_t>::max())
			{
				if (physical_pass_used[physical_index] != std::numeric_limits<uint32_t>::max() &&
					phys != physical_pass_used[physical_index])
				{
					_physical_dimensionsList[physical_index].flags &= ~ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT;
					break;
				}
				physical_pass_used[physical_index] = phys;
			}
		}

		for (auto& pass : resource->get_read_passes())
		{
			uint32_t phys = _passesList[pass]->get_physical_pass_index();
			if (phys != std::numeric_limits<uint32_t>::max())
			{
				if (physical_pass_used[physical_index] != std::numeric_limits<uint32_t>::max() &&
					phys != physical_pass_used[physical_index])
				{
					_physical_dimensionsList[physical_index].flags &= ~ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT;
					break;
				}
				physical_pass_used[physical_index] = phys;
			}
		}
	}
}

void vk_rgraph::VulkanRenderGraph::build_physical_resources()
{
	uint32_t phys_index = 0;

	// Find resources which can alias safely.
	for (auto& pass_index : _pass_stack)
	{
		auto& pass = *_passesList[pass_index];

		for (auto& input : pass.get_generic_texture_inputs())
		{
			if (input.texture->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*input.texture));
				input.texture->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[input.texture->get_physical_index()].queues |= input.texture->get_used_queues();
				_physical_dimensionsList[input.texture->get_physical_index()].image_usage |= input.texture->get_image_usage();
			}
		}

		for (auto& input : pass.get_generic_buffer_inputs())
		{
			if (input.buffer->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*input.buffer));
				input.buffer->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[input.buffer->get_physical_index()].queues |= input.buffer->get_used_queues();
				_physical_dimensionsList[input.buffer->get_physical_index()].image_usage |= input.buffer->get_buffer_usage();
			}
		}

		for (auto* input : pass.get_color_scale_inputs())
		{
			if (input && input->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*input));
				input->set_physical_index(phys_index++);
				_physical_dimensionsList[input->get_physical_index()].image_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
			}
			else if (input)
			{
				_physical_dimensionsList[input->get_physical_index()].queues |= input->get_used_queues();
				_physical_dimensionsList[input->get_physical_index()].image_usage |= input->get_image_usage();
				_physical_dimensionsList[input->get_physical_index()].image_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
			}
		}

		if (!pass.get_color_inputs().empty())
		{
			uint32_t size = pass.get_color_inputs().size();
			for (uint32_t i = 0; i < size; i++)
			{
				auto* input = pass.get_color_inputs()[i];
				if (input)
				{
					if (input->get_physical_index() == std::numeric_limits<uint32_t>::max())
					{
						_physical_dimensionsList.push_back(get_resource_dimensions(*input));
						input->set_physical_index(phys_index++);
					}
					else
					{
						_physical_dimensionsList[input->get_physical_index()].queues |= input->get_used_queues();
						_physical_dimensionsList[input->get_physical_index()].image_usage |= input->get_image_usage();
					}

					if (pass.get_color_outputs()[i]->get_physical_index() == std::numeric_limits<uint32_t>::max())
						pass.get_color_outputs()[i]->set_physical_index(input->get_physical_index());
					else if (pass.get_color_outputs()[i]->get_physical_index() != input->get_physical_index())
						throw std::logic_error("Cannot alias resources. Index already claimed.");
				}
			}
		}

		if (!pass.get_storage_inputs().empty())
		{
			uint32_t size = pass.get_storage_inputs().size();
			for (uint32_t i = 0; i < size; i++)
			{
				auto* input = pass.get_storage_inputs()[i];
				if (input)
				{
					if (input->get_physical_index() == std::numeric_limits<uint32_t>::max())
					{
						_physical_dimensionsList.push_back(get_resource_dimensions(*input));
						input->set_physical_index(phys_index++);
					}
					else
					{
						_physical_dimensionsList[input->get_physical_index()].queues |= input->get_used_queues();
						_physical_dimensionsList[input->get_physical_index()].buffer_info.usage |= input->get_buffer_usage();
					}

					if (pass.get_storage_outputs()[i]->get_physical_index() == std::numeric_limits<uint32_t>::max())
						pass.get_storage_outputs()[i]->set_physical_index(input->get_physical_index());
					else if (pass.get_storage_outputs()[i]->get_physical_index() != input->get_physical_index())
						throw std::logic_error("Cannot alias resources. Index already claimed.");
				}
			}
		}

		if (!pass.get_blit_texture_inputs().empty())
		{
			uint32_t size = pass.get_blit_texture_inputs().size();
			for (uint32_t i = 0; i < size; i++)
			{
				auto* input = pass.get_blit_texture_inputs()[i];
				if (input)
				{
					if (input->get_physical_index() == std::numeric_limits<uint32_t>::max())
					{
						_physical_dimensionsList.push_back(get_resource_dimensions(*input));
						input->set_physical_index(phys_index++);
					}
					else
					{
						_physical_dimensionsList[input->get_physical_index()].queues |= input->get_used_queues();
						_physical_dimensionsList[input->get_physical_index()].image_usage |= input->get_image_usage();
					}

					if (pass.get_blit_texture_outputs()[i]->get_physical_index() == std::numeric_limits<uint32_t>::max())
						pass.get_blit_texture_outputs()[i]->set_physical_index(input->get_physical_index());
					else if (pass.get_blit_texture_outputs()[i]->get_physical_index() != input->get_physical_index())
						throw std::logic_error("Cannot alias resources. Index already claimed.");
				}
			}
		}

		if (!pass.get_storage_texture_inputs().empty())
		{
			uint32_t size = pass.get_storage_texture_inputs().size();
			for (uint32_t i = 0; i < size; i++)
			{
				auto* input = pass.get_storage_texture_inputs()[i];
				if (input)
				{
					if (input->get_physical_index() == std::numeric_limits<uint32_t>::max())
					{
						_physical_dimensionsList.push_back(get_resource_dimensions(*input));
						input->set_physical_index(phys_index++);
					}
					else
					{
						_physical_dimensionsList[input->get_physical_index()].queues |= input->get_used_queues();
						_physical_dimensionsList[input->get_physical_index()].image_usage |= input->get_image_usage();
					}

					if (pass.get_storage_texture_outputs()[i]->get_physical_index() == std::numeric_limits<uint32_t>::max())
						pass.get_storage_texture_outputs()[i]->set_physical_index(input->get_physical_index());
					else if (pass.get_storage_texture_outputs()[i]->get_physical_index() != input->get_physical_index())
						throw std::logic_error("Cannot alias resources. Index already claimed.");
				}
			}
		}

		for (auto* output : pass.get_color_outputs())
		{
			if (output->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*output));
				output->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[output->get_physical_index()].queues |= output->get_used_queues();
				_physical_dimensionsList[output->get_physical_index()].image_usage |= output->get_image_usage();
			}
		}

		for (auto* output : pass.get_resolve_outputs())
		{
			if (output->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*output));
				output->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[output->get_physical_index()].queues |= output->get_used_queues();
				_physical_dimensionsList[output->get_physical_index()].image_usage |= output->get_image_usage();
			}
		}

		for (auto* output : pass.get_storage_outputs())
		{
			if (output->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*output));
				output->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[output->get_physical_index()].queues |= output->get_used_queues();
				_physical_dimensionsList[output->get_physical_index()].buffer_info.usage |= output->get_buffer_usage();
			}
		}

		for (auto* output : pass.get_transfer_outputs())
		{
			if (output->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*output));
				output->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[output->get_physical_index()].queues |= output->get_used_queues();
				_physical_dimensionsList[output->get_physical_index()].buffer_info.usage |= output->get_buffer_usage();
			}
		}

		for (auto* output : pass.get_blit_texture_outputs())
		{
			if (output->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*output));
				output->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[output->get_physical_index()].queues |= output->get_used_queues();
				_physical_dimensionsList[output->get_physical_index()].image_usage |= output->get_image_usage();
			}
		}

		for (auto* output : pass.get_storage_texture_outputs())
		{
			if (output->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*output));
				output->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[output->get_physical_index()].queues |= output->get_used_queues();
				_physical_dimensionsList[output->get_physical_index()].image_usage |= output->get_image_usage();
			}
		}

		auto* ds_output = pass.get_depth_stencil_output();
		auto* ds_input = pass.get_depth_stencil_input();
		if (ds_input)
		{
			if (ds_input->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*ds_input));
				ds_input->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[ds_input->get_physical_index()].queues |= ds_input->get_used_queues();
				_physical_dimensionsList[ds_input->get_physical_index()].image_usage |= ds_input->get_image_usage();
			}

			if (ds_output)
			{
				if (ds_output->get_physical_index() == std::numeric_limits<uint32_t>::max())
					ds_output->set_physical_index(ds_input->get_physical_index());
				else if (ds_output->get_physical_index() != ds_input->get_physical_index())
					throw std::logic_error("Cannot alias resources. Index already claimed.");

				_physical_dimensionsList[ds_output->get_physical_index()].queues |= ds_output->get_used_queues();
				_physical_dimensionsList[ds_output->get_physical_index()].image_usage |= ds_output->get_image_usage();
			}
		}
		else if (ds_output)
		{
			if (ds_output->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*ds_output));
				ds_output->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[ds_output->get_physical_index()].queues |= ds_output->get_used_queues();
				_physical_dimensionsList[ds_output->get_physical_index()].image_usage |= ds_output->get_image_usage();
			}
		}

		// Assign input attachments last so they can alias properly with existing color/depth attachments in the
		// same subpass.
		for (auto* input : pass.get_attachment_inputs())
		{
			if (input->get_physical_index() == std::numeric_limits<uint32_t>::max())
			{
				_physical_dimensionsList.push_back(get_resource_dimensions(*input));
				input->set_physical_index(phys_index++);
			}
			else
			{
				_physical_dimensionsList[input->get_physical_index()].queues |= input->get_used_queues();
				_physical_dimensionsList[input->get_physical_index()].image_usage |= input->get_image_usage();
			}
		}
	}

	// Figure out which physical resources need to have history.
	_physical_image_has_historyList.clear();
	_physical_image_has_historyList.resize(_physical_dimensionsList.size());

	for (auto& pass_index : _pass_stack)
	{
		auto& pass = *_passesList[pass_index];
		for (auto& history : pass.get_history_inputs())
		{
			uint32_t history_phys_index = history->get_physical_index();
			if (history_phys_index == std::numeric_limits<uint32_t>::max())
				throw std::logic_error("History input is used, but it was never written to.");
			_physical_image_has_historyList[history_phys_index] = true;
		}
	}
}

void vk_rgraph::VulkanRenderGraph::build_physical_barriers()
{
	auto barrier_itr = std::begin(_pass_barriersList);

	const auto flush_access_to_invalidate = [](VkAccessFlags2 flags) -> VkAccessFlags2 {
		if (flags & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
			flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		if (flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
			flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		if (flags & VK_ACCESS_SHADER_WRITE_BIT)
			flags |= VK_ACCESS_SHADER_READ_BIT;
		if (flags & VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT)
			flags |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
		return flags;
	};

	const auto flush_stage_to_invalidate = [](VkPipelineStageFlags2 flags) -> VkPipelineStageFlags2 {
		if (flags & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)
			flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		return flags;
	};

	struct ResourceState
	{
		VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkAccessFlags2 invalidated_types = 0;
		VkAccessFlags2 flushed_types = 0;

		VkPipelineStageFlags2 invalidated_stages = 0;
		VkPipelineStageFlags2 flushed_stages = 0;
	};

	// To handle state inside a physical pass.
	std::vector<ResourceState> resource_state;
	resource_state.reserve(_physical_dimensionsList.size());

	for (auto& physical_pass : physical_passes)
	{
		resource_state.clear();
		resource_state.resize(_physical_dimensionsList.size());

		// Go over all physical passes, and observe their use of barriers.
		// In multipass, only the first and last barriers need to be considered externally.
		// Compute never has multipass.
		uint32_t subpasses = physical_pass.passes.size();
		for (uint32_t i = 0; i < subpasses; i++, ++barrier_itr)
		{
			auto& barriers = *barrier_itr;
			auto& invalidates = barriers.invalidate;
			auto& flushes = barriers.flush;

			for (auto& invalidate : invalidates)
			{
				auto& res = resource_state[invalidate.resource_index];

				// Transients and swapchain images are handled implicitly.
				if ((_physical_dimensionsList[invalidate.resource_index].flags & ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT) != 0 ||
					invalidate.resource_index == _swapchain_physical_index)
				{
					continue;
				}

				if (invalidate.history)
				{
					auto itr = find_if(begin(physical_pass.invalidate), end(physical_pass.invalidate), [&](const Barrier& b) -> bool {
						return b.resource_index == invalidate.resource_index && b.history;
						});

					if (itr == end(physical_pass.invalidate))
					{
						// Storage images should just be in GENERAL all the time instead of SHADER_READ_ONLY_OPTIMAL.
						auto layout = _physical_dimensionsList[invalidate.resource_index].is_storage_image() ?
							VK_IMAGE_LAYOUT_GENERAL :
							invalidate.layout;

						// Special case history barriers. They are a bit different from other barriers.
						// We just need to ensure the layout is right and that we avoid write-after-read.
						// Even if we see these barriers in multiple render passes, they will not emit multiple barriers.
						physical_pass.invalidate.push_back(
							{ invalidate.resource_index, layout, invalidate.access, invalidate.stages, true });
						physical_pass.flush.push_back(
							{ invalidate.resource_index, layout, 0, invalidate.stages, true });
					}

					continue;
				}

				// Only the first use of a resource in a physical pass needs to be handled externally.
				if (res.initial_layout == VK_IMAGE_LAYOUT_UNDEFINED)
				{
					res.invalidated_types |= invalidate.access;
					res.invalidated_stages |= invalidate.stages;

					// Storage images should just be in GENERAL all the time instead of SHADER_READ_ONLY_OPTIMAL.
					if (_physical_dimensionsList[invalidate.resource_index].is_storage_image())
						res.initial_layout = VK_IMAGE_LAYOUT_GENERAL;
					else
						res.initial_layout = invalidate.layout;
				}

				// A read-only invalidation can change the layout.
				if (_physical_dimensionsList[invalidate.resource_index].is_storage_image())
					res.final_layout = VK_IMAGE_LAYOUT_GENERAL;
				else
					res.final_layout = invalidate.layout;

				// All pending flushes have been invalidated in the appropriate stages already.
				// This is relevant if the invalidate happens in subpass #1 and beyond.
				res.flushed_types = 0;
				res.flushed_stages = 0;
			}

			for (auto& flush : flushes)
			{
				auto& res = resource_state[flush.resource_index];

				// Transients are handled implicitly.
				if ((_physical_dimensionsList[flush.resource_index].flags & ATTACHMENT_INFO_INTERNAL_TRANSIENT_BIT) != 0 ||
					flush.resource_index == _swapchain_physical_index)
				{
					continue;
				}

				// The last use of a resource in a physical pass needs to be handled externally.
				res.flushed_types |= flush.access;
				res.flushed_stages |= flush.stages;

				// Storage images should just be in GENERAL all the time instead of SHADER_READ_ONLY_OPTIMAL.
				if (_physical_dimensionsList[flush.resource_index].is_storage_image())
					res.final_layout = VK_IMAGE_LAYOUT_GENERAL;
				else
					res.final_layout = flush.layout;

				// If we didn't have an invalidation before first flush, we must invalidate first.
				// Only first flush in a render pass needs a matching invalidation.
				if (res.initial_layout == VK_IMAGE_LAYOUT_UNDEFINED)
				{
					// If we end in TRANSFER_SRC_OPTIMAL, we actually start in COLOR_ATTACHMENT_OPTIMAL.
					if (flush.layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
					{
						res.initial_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						res.invalidated_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
						res.invalidated_types = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
					}
					else
					{
						res.initial_layout = flush.layout;
						res.invalidated_stages = flush_stage_to_invalidate(flush.stages);
						res.invalidated_types = flush_access_to_invalidate(flush.access);
					}

					// We're not reading the resource in this pass, so we might as well transition from UNDEFINED to discard the resource.
					physical_pass.discards.push_back(flush.resource_index);
				}
			}
		}

		// Now that the render pass has been studied, look at each resource individually and see how we need to deal
		// with the physical render pass as a whole.
		for (auto& resource : resource_state)
		{
			// Resource was not touched in this pass.
			if (resource.final_layout == VK_IMAGE_LAYOUT_UNDEFINED && resource.initial_layout == VK_IMAGE_LAYOUT_UNDEFINED)
				continue;

			uint32_t index = uint32_t(&resource - resource_state.data());

			physical_pass.invalidate.push_back(
				{ index, resource.initial_layout, resource.invalidated_types, resource.invalidated_stages, false });

			if (resource.flushed_types)
			{
				// Did the pass write anything in this pass which needs to be flushed?
				physical_pass.flush.push_back({ index, resource.final_layout, resource.flushed_types, resource.flushed_stages, false });
			}
			else if (resource.invalidated_types)
			{
				// Did the pass read anything in this pass which needs to be protected before it can be written?
				// Implement this as a flush with 0 access bits.
				// This is how Vulkan essentially implements a write-after-read hazard.
				// The only purpose of this flush barrier is to set the last pass which the resource was used as a stage.
				// Do not clear last_invalidate_pass, because we can still keep tacking on new access flags, etc.
				physical_pass.flush.push_back({ index, resource.final_layout, 0, resource.invalidated_stages, false });
			}

			// If we end in TRANSFER_SRC_OPTIMAL, this is a sentinel for needing mipmapping, so enqueue that up here.
			if (resource.final_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				physical_pass.mipmap_requests.push_back({ index, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
														  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
			}
		}
	}
}

void vk_rgraph::VulkanRenderGraph::build_render_pass_info()
{
	for (auto& physical_pass : physical_passes)
	{
		auto& rp = physical_pass.render_pass_info;
		physical_pass.subpasses.resize(physical_pass.passes.size());
		rp.subpasses = physical_pass.subpasses.data();
		rp.num_subpasses = physical_pass.subpasses.size();
		rp.clear_attachments = 0;
		rp.load_attachments = 0;
		rp.store_attachments = ~0u;
		physical_pass.color_clear_requests.clear();
		physical_pass.depth_clear_request = {};

		auto& colors = physical_pass.physical_color_attachments;
		colors.clear();

		const auto add_unique_color = [&](uint32_t index) -> std::pair<uint32_t, bool> {
			auto itr = std::find(std::begin(colors), std::end(colors), index);
			if (itr != std::end(colors))
				return std::make_pair(uint32_t(itr - std::begin(colors)), false);
			else
			{
				uint32_t ret = colors.size();
				colors.push_back(index);
				return std::make_pair(ret, true);
			}
		};

		const auto add_unique_input_attachment = [&](uint32_t index) -> std::pair<uint32_t, bool> {
			if (index == physical_pass.physical_depth_stencil_attachment)
				return std::make_pair(uint32_t(colors.size()), false); // The N + 1 attachment refers to depth.
			else
				return add_unique_color(index);
		};

		for (auto& subpass : physical_pass.passes)
		{
			std::vector<ScaledClearRequests> scaled_clear_requests;

			auto& pass = *_passesList[subpass];
			auto subpass_index = uint32_t(&subpass - physical_pass.passes.data());

			// Add color attachments.
			uint32_t num_color_attachments = pass.get_color_outputs().size();
			physical_pass.subpasses[subpass_index].num_color_attachments = num_color_attachments;
			for (uint32_t i = 0; i < num_color_attachments; i++)
			{
				auto res = add_unique_color(pass.get_color_outputs()[i]->get_physical_index());
				physical_pass.subpasses[subpass_index].color_attachments[i] = res.first;

				if (res.second) // This is the first time the color attachment is used, check if we need LOAD, or if we can clear it.
				{
					bool has_color_input = !pass.get_color_inputs().empty() && pass.get_color_inputs()[i];
					bool has_scaled_color_input = !pass.get_color_scale_inputs().empty() && pass.get_color_scale_inputs()[i];

					if (!has_color_input && !has_scaled_color_input)
					{
						if (pass.get_clear_color(i))
						{
							rp.clear_attachments |= 1u << res.first;
							physical_pass.color_clear_requests.push_back({ &pass, &rp.clear_color[res.first], i });
						}
					}
					else
					{
						if (has_scaled_color_input)
							scaled_clear_requests.push_back({ i, pass.get_color_scale_inputs()[i]->get_physical_index() });
						else
							rp.load_attachments |= 1u << res.first;
					}
				}
			}

			if (!pass.get_resolve_outputs().empty())
			{
				physical_pass.subpasses[subpass_index].num_resolve_attachments = num_color_attachments;
				for (uint32_t i = 0; i < num_color_attachments; i++)
				{
					auto res = add_unique_color(pass.get_resolve_outputs()[i]->get_physical_index());
					physical_pass.subpasses[subpass_index].resolve_attachments[i] = res.first;
					// Resolve attachments are don't care always.
				}
			}

			physical_pass.scaled_clear_requests.push_back(std::move(scaled_clear_requests));

			auto* ds_input = pass.get_depth_stencil_input();
			auto* ds_output = pass.get_depth_stencil_output();

			const auto add_unique_ds = [&](uint32_t index) -> std::pair<uint32_t, bool> {
				assert(physical_pass.physical_depth_stencil_attachment == std::numeric_limits<uint32_t>::max() ||
					physical_pass.physical_depth_stencil_attachment == index);

				bool new_attachment = physical_pass.physical_depth_stencil_attachment == std::numeric_limits<uint32_t>::max();
				physical_pass.physical_depth_stencil_attachment = index;
				return std::make_pair(index, new_attachment);
			};

			if (ds_output && ds_input)
			{
				auto res = add_unique_ds(ds_output->get_physical_index());
				// If this is the first subpass the attachment is used, we need to load it.
				if (res.second)
					rp.load_attachments |= 1u << res.first;

				rp.op_flags |= RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT;
				physical_pass.subpasses[subpass_index].depth_stencil_mode = RenderPassInfo::DepthStencil::ReadWrite;
			}
			else if (ds_output)
			{
				auto res = add_unique_ds(ds_output->get_physical_index());
				// If this is the first subpass the attachment is used, we need to either clear or discard.
				if (res.second && pass.get_clear_depth_stencil())
				{
					rp.op_flags |= RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT;
					physical_pass.depth_clear_request.pass = &pass;
					physical_pass.depth_clear_request.target = &rp.clear_depth_stencil;
				}

				rp.op_flags |= RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT;
				physical_pass.subpasses[subpass_index].depth_stencil_mode = RenderPassInfo::DepthStencil::ReadWrite;

				assert(physical_pass.physical_depth_stencil_attachment == std::numeric_limits<uint32_t>::max() ||
					physical_pass.physical_depth_stencil_attachment == ds_output->get_physical_index());
				physical_pass.physical_depth_stencil_attachment = ds_output->get_physical_index();
			}
			else if (ds_input)
			{
				auto res = add_unique_ds(ds_input->get_physical_index());

				// If this is the first subpass the attachment is used, we need to load.
				if (res.second)
				{
					rp.op_flags |=RENDER_PASS_OP_DEPTH_STENCIL_READ_ONLY_BIT |
						RENDER_PASS_OP_LOAD_DEPTH_STENCIL_BIT;

					auto current_physical_pass = uint32_t(&physical_pass - physical_passes.data());

					const auto check_preserve = [this, current_physical_pass](const VulkanRenderResource& tex) -> bool {
						for (auto& read_pass : tex.get_read_passes())
							if (_passesList[read_pass]->get_physical_pass_index() > current_physical_pass)
								return true;
						return false;
					};

					bool preserve_depth = check_preserve(*ds_input);
					//if (!preserve_depth)
					//{
					//	for (auto& logical_pass : _passesList)
					//	{
					//		for (auto& alias : logical_pass->get_fake_resource_aliases())
					//		{
					//			if (alias.first == ds_input && check_preserve(*alias.second))
					//			{
					//				preserve_depth = true;
					//				break;
					//			}
					//		}
					//	}
					//}

					if (preserve_depth)
					{
						// Have to store here, or the attachment becomes undefined in future passes.
						rp.op_flags |= RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT;
					}
				}

				physical_pass.subpasses[subpass_index].depth_stencil_mode = RenderPassInfo::DepthStencil::ReadOnly;
			}
			else
			{
				physical_pass.subpasses[subpass_index].depth_stencil_mode = RenderPassInfo::DepthStencil::None;
			}
		}

		for (auto& subpass : physical_pass.passes)
		{
			auto& pass = *_passesList[subpass];
			uint32_t subpass_index = uint32_t(&subpass - physical_pass.passes.data());

			// Add input attachments.
			// Have to do these in a separate loop so we can pick up depth stencil input attachments properly.
			uint32_t num_input_attachments = pass.get_attachment_inputs().size();
			physical_pass.subpasses[subpass_index].num_input_attachments = num_input_attachments;
			for (uint32_t i = 0; i < num_input_attachments; i++)
			{
				auto res = add_unique_input_attachment(pass.get_attachment_inputs()[i]->get_physical_index());
				physical_pass.subpasses[subpass_index].input_attachments[i] = res.first;

				// If this is the first subpass the attachment is used, we need to load it.
				if (res.second)
					rp.load_attachments |= 1u << res.first;
			}
		}

		physical_pass.render_pass_info.num_color_attachments = physical_pass.physical_color_attachments.size();
	}
}

void vk_rgraph::VulkanRenderGraph::depend_passes_recursive(const vk_rgraph::VulkanRenderPass& self, const std::unordered_set<uint32_t>& written_passes, uint32_t stack_count, bool no_check, bool ignore_self, bool merge_dependency)
{
	if (!no_check && written_passes.empty())
		throw std::logic_error("No pass exists which writes to resource.");

	if (stack_count > _passesList.size())
		throw std::logic_error("Cycle detected.");

	for (auto& pass : written_passes)
		if (pass != self.get_index())
			_pass_dependenciesList[self.get_index()].insert(pass);

	if (merge_dependency)
		for (auto& pass : written_passes)
			if (pass != self.get_index())
				_pass_merge_dependenciesList[self.get_index()].insert(pass);

	stack_count++;

	for (auto& pushed_pass : written_passes)
	{
		if (ignore_self && pushed_pass == self.get_index())
			continue;
		else if (pushed_pass == self.get_index())
			throw std::logic_error("Pass depends on itself.");

		_pass_stack.push_back(pushed_pass);
		auto& pass = *_passesList[pushed_pass];
		traverse_dependencies(pass, stack_count);
	}
}

void vk_rgraph::VulkanRenderGraph::traverse_dependencies(const vk_rgraph::VulkanRenderPass& pass, uint32_t stack_count)
{
	// For these kinds of resources,
	// make sure that we pull in the dependency right away so we can merge render passes if possible.
	if (pass.get_depth_stencil_input())
	{
		depend_passes_recursive(pass, pass.get_depth_stencil_input()->get_write_passes(),
			stack_count, false, false, true);
	}

	for (auto* input : pass.get_attachment_inputs())
	{
		bool self_dependency = pass.get_depth_stencil_output() == input;
		if (std::find(std::begin(pass.get_color_outputs()), std::end(pass.get_color_outputs()), input) != std::end(pass.get_color_outputs()))
			self_dependency = true;

		if (!self_dependency)
			depend_passes_recursive(pass, input->get_write_passes(), stack_count, false, false, true);
	}

	for (auto* input : pass.get_color_inputs())
	{
		if (input)
			depend_passes_recursive(pass, input->get_write_passes(), stack_count, false, false, true);
	}

	for (auto* input : pass.get_color_scale_inputs())
	{
		if (input)
			depend_passes_recursive(pass, input->get_write_passes(), stack_count, false, false, false);
	}

	for (auto* input : pass.get_blit_texture_inputs())
	{
		if (input)
			depend_passes_recursive(pass, input->get_write_passes(), stack_count, false, false, false);
	}

	for (auto& input : pass.get_generic_texture_inputs())
		depend_passes_recursive(pass, input.texture->get_write_passes(), stack_count, false, false, false);


	for (auto* input : pass.get_storage_inputs())
	{
		if (input)
		{
			// There might be no writers of this resource if it's used in a feedback fashion.
			depend_passes_recursive(pass, input->get_write_passes(), stack_count, true, false, false);
			// Deal with write-after-read hazards if a storage buffer is read in other passes
			// (feedback) before being updated.
			depend_passes_recursive(pass, input->get_read_passes(), stack_count, true, true, false);
		}
	}

	for (auto* input : pass.get_storage_texture_inputs())
	{
		if (input)
			depend_passes_recursive(pass, input->get_write_passes(), stack_count, false, false, false);
	}

	for (auto& input : pass.get_generic_buffer_inputs())
	{
		// There might be no writers of this resource if it's used in a feedback fashion.
		depend_passes_recursive(pass, input.buffer->get_write_passes(), stack_count, true, false, false);
	}
}

bool vk_rgraph::VulkanRenderGraph::depends_on_pass(uint32_t dst_pass, uint32_t src_pass)
{
	if (dst_pass == src_pass)
		return true;

	for (auto& dep : _pass_dependenciesList[dst_pass])
	{
		if (depends_on_pass(dep, src_pass))
			return true;
	}

	return false;
}

void vk_rgraph::VulkanRenderGraph::reorder_passes(std::vector<uint32_t>& flattened_passes)
{
	// If a pass depends on an earlier pass via merge dependencies,
	// copy over dependencies to the dependees to avoid cases which can break subpass merging.
	// This is a "soft" dependency. If we ignore it, it's not a real problem.
	for (auto& pass_merge_deps : _pass_merge_dependenciesList)
	{
		auto pass_index = uint32_t(&pass_merge_deps - _pass_merge_dependenciesList.data());
		auto& pass_deps = _pass_dependenciesList[pass_index];

		for (auto& merge_dep : pass_merge_deps)
		{
			for (auto& dependee : pass_deps)
			{
				// Avoid cycles.
				if (depends_on_pass(dependee, merge_dep))
					continue;

				if (merge_dep != dependee)
					_pass_dependenciesList[merge_dep].insert(dependee);
			}
		}
	}

	// TODO: This is very inefficient, but should work okay for a reasonable amount of passes ...
	// But, reasonable amounts are always one more than what you'd think ...
	// Clarity in the algorithm is pretty important, because these things tend to be very annoying to debug.

	if (flattened_passes.size() <= 2)
		return;

	std::vector<uint32_t> unscheduled_passes;
	unscheduled_passes.reserve(_passesList.size());
	std::swap(flattened_passes, unscheduled_passes);

	const auto schedule = [&](uint32_t index) {
		// Need to preserve the order of remaining elements.
		flattened_passes.push_back(unscheduled_passes[index]);
		std::move(unscheduled_passes.begin() + index + 1,
			unscheduled_passes.end(),
			unscheduled_passes.begin() + index);
		unscheduled_passes.pop_back();
	};

	schedule(0);
	while (!unscheduled_passes.empty())
	{
		// Find the next pass to schedule.
		// We can pick any pass N, if the pass does not depend on anything left in unscheduled_passes.
		// unscheduled_passes[0] is always okay as a fallback, so unless we find something better,
		// we will at least pick that.

		// Ideally, we pick a pass which does not introduce any hard barrier.
		// A "hard barrier" here is where a pass depends directly on the pass before it forcing something ala vkCmdPipelineBarrier,
		// we would like to avoid this if possible.

		// Find the pass which has the optimal overlap factor which means the number of passes can be scheduled in-between
		// the depender, and the dependee.

		uint32_t best_candidate = 0;
		uint32_t best_overlap_factor = 0;

		for (uint32_t i = 0; i < unscheduled_passes.size(); i++)
		{
			uint32_t overlap_factor = 0;

			// Always try to merge passes if possible on tilers.
			// This might not make sense on desktop however,
			// so we can conditionally enable this path depending on our GPU.
			if (_pass_merge_dependenciesList[unscheduled_passes[i]].count(flattened_passes.back()))
			{
				overlap_factor = ~0u;
			}
			else
			{
				for (auto itr = flattened_passes.rbegin(); itr != flattened_passes.rend(); ++itr)
				{
					if (depends_on_pass(unscheduled_passes[i], *itr))
						break;
					overlap_factor++;
				}
			}

			if (overlap_factor <= best_overlap_factor)
				continue;

			bool possible_candidate = true;
			for (uint32_t j = 0; j < i; j++)
			{
				if (depends_on_pass(unscheduled_passes[i], unscheduled_passes[j]))
				{
					possible_candidate = false;
					break;
				}
			}

			if (!possible_candidate)
				continue;

			best_candidate = i;
			best_overlap_factor = overlap_factor;
		}

		schedule(best_candidate);
	}
}

void vk_rgraph::VulkanRenderGraph::setup_physical_buffer(uint32_t attachment)
{
	const auto& bufferInfo = _physical_dimensionsList[attachment].buffer_info;

	bool need_buffer = true;
	if (_physical_buffersList[attachment]._buffer != VK_NULL_HANDLE)
	{
		need_buffer = false;
	}

	if (need_buffer)
	{
		_physical_buffersList[attachment] = _engine->create_buffer(bufferInfo.size, bufferInfo.usage, bufferInfo.vma_alloc_flags);
	}
}

void vk_rgraph::VulkanRenderGraph::setup_physical_image(uint32_t attachment)
{
	const auto& att = _physical_dimensionsList[attachment];

	bool need_image = true;
	VkImageUsageFlags usage = att.image_usage;
	VkImageCreateFlags flags = 0;

	if (att.is_storage_image())
		flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

	if (_physical_image_attachmentsList[attachment].imageView != VK_NULL_HANDLE)
	{
		if ((att.flags & ATTACHMENT_INFO_PERSISTENT_BIT) != 0)
		{
			need_image = false;
		}
	}

	if (need_image)
	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_physical_image_attachmentsList[attachment] = texBuilder.start()
			.make_img_info(att.format, att.image_usage, att.image_extend)
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(att.format, att.aspectFlags)
			.create_texture();
	}

	_physical_attachmentsList[attachment] = _physical_image_attachmentsList[attachment].imageView;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderGraph::get_texture_resource(const std::string& name)
{
	auto itr = _resource_to_indexMap.find(name);
	if (itr != std::end(_resource_to_indexMap))
	{
		assert(_resourcesList[itr->second]->get_type() == vk_rgraph::VulkanRenderResource::Type::Texture);
		return static_cast<vk_rgraph::VulkanRenderTextureResource&>(*_resourcesList[itr->second]);
	}
	else
	{
		uint32_t index = _resourcesList.size();
		_resourcesList.emplace_back(std::make_unique<vk_rgraph::VulkanRenderTextureResource>(index, name));
		_resource_to_indexMap[name] = index;
		return static_cast<vk_rgraph::VulkanRenderTextureResource&>(*_resourcesList.back());
	}
}

vk_rgraph::VulkanRenderBufferResource& vk_rgraph::VulkanRenderGraph::get_buffer_resource(const std::string& name)
{
	auto itr = _resource_to_indexMap.find(name);
	if (itr != std::end(_resource_to_indexMap))
	{
		assert(_resourcesList[itr->second]->get_type() == vk_rgraph::VulkanRenderResource::Type::Buffer);
		return static_cast<vk_rgraph::VulkanRenderBufferResource&>(*_resourcesList[itr->second]);
	}
	else
	{
		uint32_t index = _resourcesList.size();
		_resourcesList.emplace_back(std::make_unique<vk_rgraph::VulkanRenderBufferResource>(index, name));
		_resource_to_indexMap[name] = index;
		return static_cast<vk_rgraph::VulkanRenderBufferResource&>(*_resourcesList.back());
	}
}

vk_rgraph::VulkanRenderPass::VulkanRenderPass(VulkanRenderGraph* graph_, uint32_t index_, RenderGraphQueueFlagBits queue_, const std::string& name_)
	: _graph{graph_}
	, _indexInGraph{index_}
	, _queue{queue_}
	, _name{name_}
{
}

vk_rgraph::VulkanRenderResource::Type vk_rgraph::VulkanRenderResource::get_type() const
{
	return _resource_type;
}

void vk_rgraph::VulkanRenderResource::written_in_pass(uint32_t index_)
{
	_written_in_passes.insert(index_);
}

void vk_rgraph::VulkanRenderResource::read_in_pass(uint32_t index_)
{
	_read_in_passes.insert(index_);
}

const std::unordered_set<uint32_t>& vk_rgraph::VulkanRenderResource::get_read_passes() const
{
	return _read_in_passes;
}

const std::unordered_set<uint32_t>& vk_rgraph::VulkanRenderResource::get_write_passes() const
{
	return _written_in_passes;
}

std::unordered_set<uint32_t>& vk_rgraph::VulkanRenderResource::get_read_passes()
{
	return _read_in_passes;
}

std::unordered_set<uint32_t>& vk_rgraph::VulkanRenderResource::get_write_passes()
{
	return _written_in_passes;
}

uint32_t vk_rgraph::VulkanRenderResource::get_render_graph_index() const
{
	return _index;
}

void vk_rgraph::VulkanRenderResource::set_physical_index(uint32_t index_)
{
	_physical_index = index_;
}

uint32_t vk_rgraph::VulkanRenderResource::get_physical_index() const
{
	assert(_physical_index.has_value());
	return _physical_index.value_or(std::numeric_limits<uint32_t>::max());
}

const std::string& vk_rgraph::VulkanRenderResource::get_name() const
{
	return _name;
}

void vk_rgraph::VulkanRenderResource::add_queue(vk_rgraph::RenderGraphQueueFlagBits queue)
{
	_used_queues |= queue;
}

vk_rgraph::RenderGraphQueueFlags vk_rgraph::VulkanRenderResource::get_used_queues() const
{
	return _used_queues;
}

void vk_rgraph::VulkanRenderBufferResource::set_buffer_info(const vk_rgraph::BufferInfo& info_)
{
	_buffer_info = info_;
}

const vk_rgraph::BufferInfo& vk_rgraph::VulkanRenderBufferResource::get_buffer_info() const
{
	return _buffer_info;
}

void vk_rgraph::VulkanRenderBufferResource::add_buffer_usage(VkBufferUsageFlags flags)
{
	_buffer_usage |= flags;
}

VkBufferUsageFlags vk_rgraph::VulkanRenderBufferResource::get_buffer_usage() const
{
	return _buffer_usage;
}

void vk_rgraph::VulkanRenderTextureResource::set_attachment_info(const AttachmentInfo& info_)
{
	_attach_info = info_;
}

const vk_rgraph::AttachmentInfo& vk_rgraph::VulkanRenderTextureResource::get_attachment_info() const
{
	return _attach_info;
}

vk_rgraph::AttachmentInfo& vk_rgraph::VulkanRenderTextureResource::get_attachment_info()
{
	return _attach_info;
}

void vk_rgraph::VulkanRenderTextureResource::set_transient_state(bool enable)
{
	_transient = enable;
}

bool vk_rgraph::VulkanRenderTextureResource::get_transient_state() const
{
	return _transient;
}

void vk_rgraph::VulkanRenderTextureResource::add_image_usage(VkImageUsageFlags flags)
{
	_image_usage |= flags;
}

VkImageUsageFlags vk_rgraph::VulkanRenderTextureResource::get_image_usage() const
{
	return _image_usage;
}

vk_rgraph::VulkanRenderBufferResource& vk_rgraph::VulkanRenderPass::add_generic_buffer_input(const std::string& name, VkPipelineStageFlags2 stages,
	VkAccessFlags2 access, VkBufferUsageFlags usage)
{
	auto& res = _graph->get_buffer_resource(name);
	res.add_queue(_queue);
	res.read_in_pass(_indexInGraph);
	res.add_buffer_usage(usage);

	AccessedBufferResource acc;
	acc.buffer = &res;
	acc.layout = VK_IMAGE_LAYOUT_GENERAL;
	acc.access = access;
	acc.stages = stages;
	_generic_bufferList.push_back(acc);
	return res;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderPass::set_depth_stencil_input(const std::string& name)
{
	auto& res = _graph->get_texture_resource(name);
	res.add_queue(_queue);
	res.read_in_pass(_indexInGraph);
	res.add_image_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	_depth_stencil_input = &res;
	return res;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderPass::set_depth_stencil_output(const std::string& name, const AttachmentInfo& info)
{
	auto& res = _graph->get_texture_resource(name);
	res.add_queue(_queue);
	res.written_in_pass(_indexInGraph);
	res.set_attachment_info(info);
	res.add_image_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	_depth_stencil_output = &res;
	return res;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderPass::add_color_output(const std::string& name, const AttachmentInfo& info, const std::string& input)
{
	auto& res = _graph->get_texture_resource(name);
	res.add_queue(_queue);
	res.written_in_pass(_indexInGraph);
	res.set_attachment_info(info);
	res.add_image_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	if (info.levels != 1)
		res.add_image_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	_color_outputsList.push_back(&res);

	if (!input.empty())
	{
		auto& input_res = _graph->get_texture_resource(input);
		input_res.read_in_pass(_indexInGraph);
		input_res.add_image_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
		_color_inputsList.push_back(&input_res);
		_color_scale_inputsList.push_back(nullptr);
	}
	else
	{
		_color_inputsList.push_back(nullptr);
		_color_scale_inputsList.push_back(nullptr);
	}

	return res;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderPass::add_resolve_output(const std::string& name, const AttachmentInfo& info)
{
	auto& res = _graph->get_texture_resource(name);
	res.add_queue(_queue);
	res.written_in_pass(_indexInGraph);
	res.set_attachment_info(info);
	res.add_image_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	_resolve_outputsList.push_back(&res);
	return res;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderPass::add_attachment_input(const std::string& name)
{
	auto& res = _graph->get_texture_resource(name);
	res.add_queue(_queue);
	res.read_in_pass(_indexInGraph);
	res.add_image_usage(VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
	_attachments_inputsList.push_back(&res);
	return res;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderPass::add_history_input(const std::string& name)
{
	auto& res = _graph->get_texture_resource(name);
	res.add_queue(_queue);
	res.add_image_usage(VK_IMAGE_USAGE_SAMPLED_BIT);
	// History inputs are not used in any particular pass, but next frame.
	_history_inputsList.push_back(&res);
	return res;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderPass::add_texture_input(const std::string& name, VkPipelineStageFlags2 stages)
{
	auto& res = _graph->get_texture_resource(name);
	res.add_queue(_queue);
	res.read_in_pass(_indexInGraph);
	res.add_image_usage(VK_IMAGE_USAGE_SAMPLED_BIT);

	// Support duplicate add_texture_inputs.
	auto itr = std::find_if(std::begin(_generic_textureList), std::end(_generic_textureList), [&](const AccessedTextureResource& acc) {
		return acc.texture == &res;
		});

	if (itr != std::end(_generic_textureList))
		return *itr->texture;

	AccessedTextureResource acc;
	acc.texture = &res;
	acc.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	acc.access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

	if (stages != 0)
		acc.stages = stages;
	else if ((_queue & compute_queues) != 0)
		acc.stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	else
		acc.stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	_generic_textureList.push_back(acc);
	return res;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderPass::add_blit_texture_read_only_input(const std::string& name)
{
	auto& res = _graph->get_texture_resource(name);
	res.add_queue(_queue);
	res.read_in_pass(_indexInGraph);
	res.add_image_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	AccessedTextureResource acc;
	acc.texture = &res;
	acc.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	acc.access = VK_ACCESS_TRANSFER_READ_BIT;
	acc.stages = VK_PIPELINE_STAGE_2_BLIT_BIT;

	_generic_textureList.push_back(acc);
	return res;
}

vk_rgraph::VulkanRenderBufferResource& vk_rgraph::VulkanRenderPass::add_uniform_input(const std::string& name, VkPipelineStageFlags2 stages)
{
	if (stages == 0)
	{
		if ((_queue & compute_queues) != 0)
			stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		else
			stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	return add_generic_buffer_input(name, stages, VK_ACCESS_UNIFORM_READ_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
}

vk_rgraph::VulkanRenderBufferResource& vk_rgraph::VulkanRenderPass::add_storage_read_only_input(const std::string& name, VkPipelineStageFlags2 stages)
{
	if (stages == 0)
	{
		if ((_queue & compute_queues) != 0)
			stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		else
			stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	return add_generic_buffer_input(name, stages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

vk_rgraph::VulkanRenderBufferResource& vk_rgraph::VulkanRenderPass::add_storage_output(const std::string& name, const BufferInfo& info, const std::string& input)
{
	auto& res = _graph->get_buffer_resource(name);
	res.add_queue(_queue);
	res.set_buffer_info(info);
	res.written_in_pass(_indexInGraph);
	res.add_buffer_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	_storage_inputsList.push_back(&res);

	if (!input.empty())
	{
		auto& input_res = _graph->get_buffer_resource(input);
		input_res.read_in_pass(_indexInGraph);
		input_res.add_buffer_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		_storage_inputsList.push_back(&input_res);
	}
	else
		_storage_inputsList.push_back(nullptr);

	return res;
}

vk_rgraph::VulkanRenderBufferResource& vk_rgraph::VulkanRenderPass::add_transfer_output(const std::string& name, const BufferInfo& info)
{
	auto& res = _graph->get_buffer_resource(name);
	res.add_queue(_queue);
	res.set_buffer_info(info);
	res.written_in_pass(_indexInGraph);
	res.add_buffer_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	_transfer_outputsList.push_back(&res);
	return res;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderPass::add_storage_texture_output(const std::string& name, const AttachmentInfo& info,
	const std::string& input)
{
	auto& res = _graph->get_texture_resource(name);
	res.add_queue(_queue);
	res.written_in_pass(_indexInGraph);
	res.set_attachment_info(info);
	res.add_image_usage(VK_IMAGE_USAGE_STORAGE_BIT);
	_storage_texture_outputsList.push_back(&res);

	if (!input.empty())
	{
		auto& input_res = _graph->get_texture_resource(input);
		input_res.read_in_pass(_indexInGraph);
		input_res.add_image_usage(VK_IMAGE_USAGE_STORAGE_BIT);
		_storage_texture_outputsList.push_back(&input_res);
	}
	else
		_storage_texture_outputsList.push_back(nullptr);

	return res;
}

vk_rgraph::VulkanRenderTextureResource& vk_rgraph::VulkanRenderPass::add_blit_texture_output(const std::string& name, const AttachmentInfo& info,
	const std::string& input)
{
	auto& res = _graph->get_texture_resource(name);
	res.add_queue(_queue);
	res.written_in_pass(_indexInGraph);
	res.set_attachment_info(info);
	res.add_image_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	_blit_texture_outputsList.push_back(&res);

	if (!input.empty())
	{
		auto& input_res = _graph->get_texture_resource(input);
		input_res.read_in_pass(_indexInGraph);
		input_res.add_image_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		_blit_texture_outputsList.push_back(&input_res);
	}
	else
		_blit_texture_outputsList.push_back(nullptr);

	return res;
}

vk_rgraph::VulkanRenderBufferResource& vk_rgraph::VulkanRenderPass::add_vertex_buffer_input(const std::string& name)
{
	return add_generic_buffer_input(name,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

vk_rgraph::VulkanRenderBufferResource& vk_rgraph::VulkanRenderPass::add_index_buffer_input(const std::string& name)
{
	return add_generic_buffer_input(name,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		VK_ACCESS_INDEX_READ_BIT,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

vk_rgraph::VulkanRenderBufferResource& vk_rgraph::VulkanRenderPass::add_indirect_buffer_input(const std::string& name)
{
	return add_generic_buffer_input(name,
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
}

void vk_rgraph::VulkanRenderPass::make_color_input_scaled(uint32_t index_)
{
	std::swap(_color_scale_inputsList[index_], _color_inputsList[index_]);
}

const std::vector<vk_rgraph::VulkanRenderTextureResource*>& vk_rgraph::VulkanRenderPass::get_color_outputs() const
{
	return _color_outputsList;
}

const std::vector<vk_rgraph::VulkanRenderTextureResource*>& vk_rgraph::VulkanRenderPass::get_resolve_outputs() const
{
	return _resolve_outputsList;
}

const std::vector<vk_rgraph::VulkanRenderTextureResource*>& vk_rgraph::VulkanRenderPass::get_color_inputs() const
{
	return _color_inputsList;
}

const std::vector<vk_rgraph::VulkanRenderTextureResource*>& vk_rgraph::VulkanRenderPass::get_color_scale_inputs() const
{
	return _color_scale_inputsList;
}

const std::vector<vk_rgraph::VulkanRenderTextureResource*>& vk_rgraph::VulkanRenderPass::get_storage_texture_outputs() const
{
	return _storage_texture_outputsList;
}

const std::vector<vk_rgraph::VulkanRenderTextureResource*>& vk_rgraph::VulkanRenderPass::get_storage_texture_inputs() const
{
	return _storage_texture_inputsList;
}

const std::vector<vk_rgraph::VulkanRenderTextureResource*>& vk_rgraph::VulkanRenderPass::get_blit_texture_inputs() const
{
	return _blit_texture_inputsList;
}

const std::vector<vk_rgraph::VulkanRenderTextureResource*>& vk_rgraph::VulkanRenderPass::get_blit_texture_outputs() const
{
	return _blit_texture_outputsList;
}

const std::vector<vk_rgraph::VulkanRenderTextureResource*>& vk_rgraph::VulkanRenderPass::get_attachment_inputs() const
{
	return _attachments_inputsList;
}

const std::vector<vk_rgraph::VulkanRenderTextureResource*>& vk_rgraph::VulkanRenderPass::get_history_inputs() const
{
	return _history_inputsList;
}

const std::vector<vk_rgraph::VulkanRenderBufferResource*>& vk_rgraph::VulkanRenderPass::get_storage_inputs() const
{
	return _storage_inputsList;
}

const std::vector<vk_rgraph::VulkanRenderBufferResource*>& vk_rgraph::VulkanRenderPass::get_storage_outputs() const
{
	return _storage_outputsList;
}

const std::vector<vk_rgraph::VulkanRenderBufferResource*>& vk_rgraph::VulkanRenderPass::get_transfer_outputs() const
{
	return _transfer_outputsList;
}

const std::vector<vk_rgraph::VulkanRenderPass::AccessedTextureResource>& vk_rgraph::VulkanRenderPass::get_generic_texture_inputs() const
{
	return _generic_textureList;
}

const std::vector<vk_rgraph::VulkanRenderPass::AccessedBufferResource>& vk_rgraph::VulkanRenderPass::get_generic_buffer_inputs() const
{
	return _generic_bufferList;
}

vk_rgraph::VulkanRenderTextureResource* vk_rgraph::VulkanRenderPass::get_depth_stencil_input() const
{
	return _depth_stencil_input;
}

vk_rgraph::VulkanRenderTextureResource* vk_rgraph::VulkanRenderPass::get_depth_stencil_output() const
{
	return _depth_stencil_output;
}

uint32_t vk_rgraph::VulkanRenderPass::get_physical_pass_index() const
{
	return _physical_passIndex.value_or(std::numeric_limits<uint32_t>::max());
}

void vk_rgraph::VulkanRenderPass::set_physical_pass_index(uint32_t index_)
{
	_physical_passIndex = index_;
}

bool vk_rgraph::VulkanRenderPass::get_clear_color(uint32_t index_, VkClearColorValue* value/* = nullptr*/) const
{
	if (get_clear_color_cb)
		return get_clear_color_cb(index_, value);
	else
		return false;
}

bool vk_rgraph::VulkanRenderPass::get_clear_depth_stencil(VkClearDepthStencilValue* value/* = nullptr*/) const
{
	if (get_clear_depth_stencil_cb)
		return get_clear_depth_stencil_cb(value);
	else
		return false;
}


void vk_rgraph::VulkanRenderPass::build_render_pass(VkCommandBuffer& cmd)
{
	if (build_render_pass_cb)
		build_render_pass_cb(cmd);
}

void vk_rgraph::VulkanRenderPass::set_build_render_pass(std::function<void(VkCommandBuffer&)> func)
{
	build_render_pass_cb = std::move(func);
}

void vk_rgraph::VulkanRenderPass::set_get_clear_depth_stencil(std::function<bool(VkClearDepthStencilValue*)> func)
{
	get_clear_depth_stencil_cb = std::move(func);
}

void vk_rgraph::VulkanRenderPass::set_get_clear_color(std::function<bool(uint32_t, VkClearColorValue*)> func)
{
	get_clear_color_cb = std::move(func);
}

const std::string& vk_rgraph::VulkanRenderPass::get_name() const
{
	return _name;
}

bool vk_rgraph::ResourceDimensions::operator==(const ResourceDimensions& other) const
{
	return format == other.format &&
		image_extend.width == other.image_extend.width &&
		image_extend.height == other.image_extend.height &&
		image_extend.depth == other.image_extend.depth &&
		layers == other.layers &&
		levels == other.levels &&
		buffer_info == other.buffer_info &&
		flags == other.flags &&
		transform == other.transform;
	// image_usage is deliberately not part of this test.
	// queues is deliberately not part of this test.
}

bool vk_rgraph::ResourceDimensions::operator!=(const ResourceDimensions & other) const
{
	return !(*this == other);
}

bool vk_rgraph::ResourceDimensions::uses_semaphore() const
{
	if ((flags & ATTACHMENT_INFO_INTERNAL_PROXY_BIT) != 0)
		return true;

	// If more than one queue is used for a resource, we need to use semaphores.
	auto physical_queues = queues;

	// Regular compute uses regular graphics queue.
	if (physical_queues & RENDER_GRAPH_QUEUE_COMPUTE_BIT)
		physical_queues |= RENDER_GRAPH_QUEUE_GRAPHICS_BIT;
	physical_queues &= ~RENDER_GRAPH_QUEUE_COMPUTE_BIT;
	return (physical_queues & (physical_queues - 1)) != 0;
}

bool vk_rgraph::ResourceDimensions::is_storage_image() const
{
	return (image_usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
}

bool vk_rgraph::ResourceDimensions::is_buffer_like() const
{
	return is_storage_image() || (buffer_info.size != 0) || (flags & ATTACHMENT_INFO_INTERNAL_PROXY_BIT) != 0;
}


