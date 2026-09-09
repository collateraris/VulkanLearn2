#include "vk_engine.h"
#include <rhi/vulkan_resources.h>
#include <sys_config/ConfigManager.h>
#include <sys_config/vk_strings.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
float halton(uint32_t index, uint32_t base)
{
    float value = 0.f, fraction = 1.f;
    while (index) { fraction /= float(base); value += fraction * float(index % base); index /= base; }
    return value;
}

// Streamline uses row vectors and row-major matrices. GLM's column-major
// storage already contains the transposed matrix needed for that convention.
std::array<float, 16> streamline_matrix(const glm::mat4& matrix)
{
    std::array<float, 16> result{};
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            result[row * 4 + column] = matrix[row][column];
    return result;
}
std::array<float, 3> vector3(glm::vec3 v) { return {v.x, v.y, v.z}; }
}

void VulkanEngine::init_upscaling()
{
    if (!_dlssActive) return;
    _dlssPrepare.init(this);
    const auto image = _rhi.resources().create_image({_windowExtent.width, _windowExtent.height,
        rhi::Format::Rgba16Float, rhi::ImageUsage::Storage | rhi::ImageUsage::Sampled |
        rhi::ImageUsage::TransferSource | rhi::ImageUsage::TransferDestination});
    _dlssOutput = _rhi.vulkan_resources().texture(image);
    _mainDeletionQueue.push_function([this, image]() { _rhi.resources().destroy(image); });
    _dlssReset = true;
    _dlssLastAccumulation = _frameAccumulationEnabled;
    _dlssLastDenoiser = _denoiserEnabled;
}

void VulkanEngine::update_render_jitter()
{
    if (!_dlssActive) { _camera.update_jitter(_renderExtent.width, _renderExtent.height); return; }
    const float ratio = float(_windowExtent.width) / float(_renderExtent.width);
    const uint32_t phases = std::max(8u, uint32_t(std::ceil(8.f * ratio * ratio)));
    const uint32_t phase = uint32_t(_frameNumber) % phases + 1u;
    _camera.set_jitter((halton(phase, 2) - 0.5f) / float(_renderExtent.width),
        (halton(phase, 3) - 0.5f) / float(_renderExtent.height));
}

void VulkanEngine::append_upscaling_passes(int frameSlot, const Texture& source)
{
    _dlssPrepare.append_passes(_rgraph, frameSlot, source);
    rhi::TemporalUpscaleDescription description{};
    description.color = _rhi.image(_dlssPrepare.output_color());
    description.depth = _rhi.image(_dlssPrepare.output_depth());
    description.motion = _rhi.image(_dlssPrepare.output_motion());
    description.output = _rhi.image(_dlssOutput);
    description.mode = static_cast<rhi::UpscaleMode>(_renderSettings.dlssMode);
    description.frameIndex = uint32_t(_frameNumber);
    description.renderWidth = _renderExtent.width;
    description.renderHeight = _renderExtent.height;
    description.outputWidth = _windowExtent.width;
    description.outputHeight = _windowExtent.height;
    // The renderer retains its original GL projection. DLSS depth is converted
    // to [0,1] by the preparation shader, so its camera matrices must match.
    glm::mat4 depthRemap(1.f);
    depthRemap[2][2] = 0.5f;
    depthRemap[3][2] = 0.5f;
    const auto projection = depthRemap * _camera.get_projection_matrix(false);
    const auto previousProjection = depthRemap * _camera.get_prev_projection_matrix(false);
    const auto viewProjection = projection * _camera.get_view_matrix();
    const auto previousViewProjection = previousProjection * _camera.get_prev_view_matrix();
    description.viewToClip = streamline_matrix(projection);
    description.clipToView = streamline_matrix(glm::inverse(projection));
    description.clipToPrevClip = streamline_matrix(previousViewProjection * glm::inverse(viewProjection));
    description.prevClipToClip = streamline_matrix(viewProjection * glm::inverse(previousViewProjection));
    description.jitterX = _camera.bUseJitter ? _camera.jitterX * float(_renderExtent.width) : 0.f;
    description.jitterY = _camera.bUseJitter ? _camera.jitterY * float(_renderExtent.height) : 0.f;
    description.motionScaleX = 1.f / float(_renderExtent.width);
    description.motionScaleY = 1.f / float(_renderExtent.height);
    const auto cameraToWorld = glm::inverse(_camera.get_view_matrix());
    const auto forward = glm::normalize(-glm::vec3(cameraToWorld[2]));
    description.cameraPosition = vector3(_camera.position);
    description.cameraRight = vector3(glm::normalize(glm::vec3(cameraToWorld[0])));
    description.cameraUp = vector3(glm::normalize(glm::vec3(cameraToWorld[1])));
    description.cameraForward = vector3(forward);
    description.cameraNear = _camera.nearDistance;
    description.cameraFar = _camera.farDistance;
    description.cameraFov = glm::radians(_camera.FOV);
    description.cameraAspect = _camera.aspectRatio;
    // Continuous camera movement is reprojected. Only cuts invalidate history.
    const float sceneSize = glm::length(_resManager.maxCube - _resManager.minCube);
    const bool cameraCut = _frameNumber > 0 &&
        (glm::distance(_camera.position, _dlssPreviousPosition) > std::max(1.f, sceneSize * .05f) ||
         glm::dot(forward, _dlssPreviousForward) < .75f ||
         _camera.get_projection_matrix(false) != _camera.get_prev_projection_matrix(false));
    description.reset = _dlssReset || _frameNumber == 0 || cameraCut ||
        _dlssLastAccumulation != _frameAccumulationEnabled || _dlssLastDenoiser != _denoiserEnabled;
    _dlssPreviousPosition = _camera.position;
    _dlssPreviousForward = forward;
    _dlssLastAccumulation = _frameAccumulationEnabled;
    _dlssLastDenoiser = _denoiserEnabled;
    _dlssReset = false;
    using rhi::Stage;
    using rhi::Access;
    using rhi::Layout;
    const auto color = _rgraph.import_resource("DLSS.Color", description.color, false);
    const auto depth = _rgraph.import_resource("DLSS.Depth", description.depth, false);
    const auto motion = _rgraph.import_resource("DLSS.Motion", description.motion, false);
    const auto output = _rgraph.import_resource("DLSS.Output", description.output, false);
    _rgraph.add_pass("DLSS.SuperResolution", {
        {color, {Stage::Compute, Access::ShaderRead, Layout::General}},
        {depth, {Stage::Compute, Access::ShaderRead, Layout::General}},
        {motion, {Stage::Compute, Access::ShaderRead, Layout::General}},
        {output, {Stage::Compute | Stage::Transfer, Access::ShaderWrite | Access::TransferWrite, Layout::General}}
    }, [this, description](rhi::CommandList& commands) {
        _dlssLastEvaluationSucceeded = commands.evaluate_upscaler(description);
        if (description.frameIndex == 0)
            std::cout << "DLSS evaluation: " << (_dlssLastEvaluationSucceeded ? "OK" : "FAILED (spatial fallback)")
                << " | " << _dlss.status() << std::endl;
        if (!_dlssLastEvaluationSucceeded) _dlssReset = true;
    });
}

VulkanEngine::ResumeState VulkanEngine::capture_resume_state() const
{
    ResumeState result;
    result.valid = true;
    result.position = _camera.position;
    result.pitch = _camera.pitch;
    result.yaw = _camera.yaw;
    result.fov = _camera.FOV;
    result.activeCamera = _camera.bActiveCamera;
    result.accumulation = _frameAccumulationEnabled;
    result.denoiser = _denoiserEnabled;
    result.numRays = _indirectNumRays;
    result.hasGeneratedLightSeed = _lightManager.has_generated_light_seed();
    result.generatedLightSeed = _lightManager.get_generated_light_seed();
    result.hasSun = _lightManager.is_sun_active();
    if (result.hasSun)
    {
        const auto& sun = _lightManager.get_lights()[_lightManager.get_sun_index()];
        result.sunDirection = glm::vec3(sun.direction_flux);
        result.sunColor = glm::vec3(sun.color_type);
    }
    return result;
}

void VulkanEngine::request_render_settings()
{
    if (!vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).SaveRenderSettings(_pendingRenderSettings))
    {
        _renderSettingsError = "Could not save valid display settings to config.xml.";
        return;
    }
    _reloadRequested = true;
}
