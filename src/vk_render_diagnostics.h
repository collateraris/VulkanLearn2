#pragma once

// Opt-in, deterministic-frame HDR readback for rendering regression checks.
// Include from vk_engine.cpp; no diagnostics resources exist unless enabled.
#include <vk_engine.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class VulkanRenderDiagnostics
{
public:
    // Frame numbers count completed submissions, starting at one.
    // RESTIR_DIAGNOSTICS_FRAMES=1,32,128,512,2048
    // RESTIR_DIAGNOSTICS_MAX_FRAMES=2048 (defaults to last capture frame)
    // RESTIR_DIAGNOSTICS_OUTPUT=absolute/output/directory
    // RESTIR_DIAGNOSTICS_RAW=1 captures history instead of denoised display HDR.
    // RESTIR_DIAGNOSTICS_DENOISER_TOGGLE_FRAMES=16,32 exercises the UI state.
    static std::unique_ptr<VulkanRenderDiagnostics> from_environment()
    {
        const char* frames = std::getenv("RESTIR_DIAGNOSTICS_FRAMES");
        const char* limit = std::getenv("RESTIR_DIAGNOSTICS_MAX_FRAMES");
        if ((!frames || !*frames) && (!limit || !*limit))
            return nullptr;
        return std::unique_ptr<VulkanRenderDiagnostics>(new VulkanRenderDiagnostics(frames, limit));
    }

    bool capture_raw_output() const { return _captureRawOutput; }

    void before_frame(VulkanEngine& engine)
    {
        const uint32_t nextFrame = static_cast<uint32_t>(engine._frameNumber) + 1u;
        if (std::binary_search(_denoiserToggleFrames.begin(), _denoiserToggleFrames.end(), nextFrame))
            engine._denoiserEnabled = !engine._denoiserEnabled;
        if (_cameraMotion)
        {
            auto& camera = engine._camera;
            if (nextFrame == 1)
            {
                _basePosition = camera.position;
                _baseYaw = camera.yaw;
            }
            // Deterministic pan/translation, then stop; optional abrupt cut.
            const float phase = std::clamp((float(nextFrame) - 64.0f) / 64.0f, 0.0f, 1.0f);
            camera.position = _basePosition + glm::vec3(0.12f * phase, 0.0f, 0.0f);
            camera.yaw = _baseYaw + 0.10f * phase;
            if (_cameraCut && nextFrame >= 192)
                camera.yaw += 0.7f;
            camera.calculate_view_matrix();
        }
    }

    void record_gpu_time(uint32_t frame, double totalMs, double denoiserMs)
    {
        _gpuTimes << frame << ',' << totalMs << ',' << denoiserMs << '\n';
    }

    // Call immediately after engine.draw(): _frameNumber has already advanced.
    // The supplied image must be linear RGBA32F radiance in shader-read
    // layout, with TRANSFER_SRC usage. Returns true at the requested frame limit.
    bool after_frame(VulkanEngine& engine, const Texture& accumulated)
    {
        const uint32_t frame = static_cast<uint32_t>(engine._frameNumber);
        if (std::binary_search(_captureFrames.begin(), _captureFrames.end(), frame))
        {
            const auto& submitted = engine._frames[(frame - 1u) % FRAME_OVERLAP];
            check(vkWaitForFences(engine._device, 1, &submitted._renderFence, VK_TRUE, UINT64_MAX));
            capture(engine, accumulated, frame);
        }
        if (frame >= _maxFrames)
        {
            // Also make a limit-only run safe for the engine's cleanup path.
            check(vkDeviceWaitIdle(engine._device));
            return true;
        }
        return false;
    }

private:
    static void check(VkResult result)
    {
        if (result != VK_SUCCESS)
            throw std::runtime_error("Vulkan diagnostics failed with VkResult " + std::to_string(result));
    }

    static uint32_t parse_frame(const std::string& value)
    {
        size_t consumed = 0;
        const auto result = std::stoul(value, &consumed);
        if (value.find_first_not_of(" \t\r\n", consumed) != std::string::npos || result == 0 || result > 10000000u)
            throw std::runtime_error("Diagnostic frame numbers must be integers in [1, 10000000]");
        return static_cast<uint32_t>(result);
    }

    VulkanRenderDiagnostics(const char* frames, const char* limit)
    {
        const char* raw = std::getenv("RESTIR_DIAGNOSTICS_RAW");
        _captureRawOutput = raw && std::string(raw) == "1";
        const char* motion = std::getenv("RESTIR_DIAGNOSTICS_CAMERA_MOTION");
        _cameraMotion = motion && std::string(motion) == "1";
        const char* cut = std::getenv("RESTIR_DIAGNOSTICS_CAMERA_CUT");
        _cameraCut = cut && std::string(cut) == "1";
        if (const char* toggles = std::getenv("RESTIR_DIAGNOSTICS_DENOISER_TOGGLE_FRAMES"))
        {
            std::istringstream input(toggles);
            std::string token;
            while (std::getline(input, token, ','))
                _denoiserToggleFrames.push_back(parse_frame(token));
            std::sort(_denoiserToggleFrames.begin(), _denoiserToggleFrames.end());
        }
        if (frames && *frames)
        {
            std::istringstream input(frames);
            std::string token;
            while (std::getline(input, token, ','))
                _captureFrames.push_back(parse_frame(token));
            std::sort(_captureFrames.begin(), _captureFrames.end());
            _captureFrames.erase(std::unique(_captureFrames.begin(), _captureFrames.end()), _captureFrames.end());
        }
        _maxFrames = limit && *limit ? parse_frame(limit) : _captureFrames.back();
        const char* output = std::getenv("RESTIR_DIAGNOSTICS_OUTPUT");
        _directory = output && *output ? std::filesystem::path(output) : std::filesystem::path("win64/runtime/diagnostics");
        std::filesystem::create_directories(_directory);
        _statistics.open(_directory / "frames.csv", std::ios::out | std::ios::trunc);
        if (!_statistics)
            throw std::runtime_error("Cannot open diagnostics frames.csv");
        _statistics << "frame,seconds,width,height,finite_pixels,nonfinite_pixels,black_pixels,mean_r,mean_g,mean_b,mean_luminance,min_luminance,max_luminance,stddev_luminance\n";
        _statistics << std::setprecision(12);
        _gpuTimes.open(_directory / "gpu-times.csv", std::ios::out | std::ios::trunc);
        if (!_gpuTimes) throw std::runtime_error("Cannot open diagnostics gpu-times.csv");
        _gpuTimes << "frame,total_ms,denoiser_ms\n" << std::setprecision(9);
        _start = std::chrono::steady_clock::now();
    }

    void capture(VulkanEngine& engine, const Texture& image, uint32_t frame)
    {
        if (image.createInfo.format != VK_FORMAT_R32G32B32A32_SFLOAT ||
            !(image.createInfo.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
            throw std::runtime_error("Diagnostics require an RGBA32F accumulated image with TRANSFER_SRC usage");
        const size_t pixelCount = size_t(image.extend.width) * image.extend.height;
        const VkDeviceSize byteSize = VkDeviceSize(pixelCount) * 4 * sizeof(float);
        auto staging = engine.create_buffer(size_t(byteSize), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        void* mapped = nullptr;
        try
        {
            engine.immediate_submit([&](VkCommandBuffer command) {
                VkImageMemoryBarrier toCopy{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                toCopy.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                toCopy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                toCopy.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                toCopy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                toCopy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toCopy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toCopy.image = image.image._image;
                toCopy.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &toCopy);

                VkBufferImageCopy copy{};
                copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
                copy.imageExtent = image.extend;
                vkCmdCopyImageToBuffer(command, image.image._image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    staging._buffer, 1, &copy);

                VkBufferMemoryBarrier toHost{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                toHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                toHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                toHost.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toHost.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toHost.buffer = staging._buffer;
                toHost.size = VK_WHOLE_SIZE;
                VkImageMemoryBarrier restore = toCopy;
                restore.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                restore.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                restore.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                restore.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, 0, nullptr, 1, &toHost, 1, &restore);
            });
            // immediate_submit waits its fence before returning. Invalidate
            // non-coherent host memory before inspecting the GPU's writes.
            check(vmaMapMemory(engine._allocator, staging._allocation, &mapped));
            check(vmaInvalidateAllocation(engine._allocator, staging._allocation, 0, byteSize));
            save(static_cast<const float*>(mapped), image.extend.width, image.extend.height, frame);
            vmaUnmapMemory(engine._allocator, staging._allocation);
            mapped = nullptr;
        }
        catch (...)
        {
            if (mapped)
                vmaUnmapMemory(engine._allocator, staging._allocation);
            engine.destroy_buffer(engine._allocator, staging);
            throw;
        }
        engine.destroy_buffer(engine._allocator, staging);
    }

    void save(const float* pixels, uint32_t width, uint32_t height, uint32_t frame)
    {
        std::ostringstream filename;
        filename << "frame_" << std::setfill('0') << std::setw(6) << frame << ".pfm";
        std::ofstream pfm(_directory / filename.str(), std::ios::binary);
        if (!pfm)
            throw std::runtime_error("Cannot open diagnostics PFM output");
        pfm << "PF\n" << width << ' ' << height << "\n"
            << (std::endian::native == std::endian::little ? "-1.0\n" : "1.0\n");
        // PFM stores rows bottom-to-top and RGB only. Preserve NaNs/Infs in the
        // artifact so invalid paths remain observable rather than masked.
        std::vector<float> row(size_t(width) * 3);
        for (uint32_t y = height; y > 0; --y)
        {
            const float* source = pixels + size_t(y - 1) * width * 4;
            for (uint32_t x = 0; x < width; ++x)
                std::copy_n(source + size_t(x) * 4, 3, row.data() + size_t(x) * 3);
            pfm.write(reinterpret_cast<const char*>(row.data()), std::streamsize(row.size() * sizeof(float)));
        }
        if (!pfm)
            throw std::runtime_error("Writing diagnostics PFM failed");

        std::array<double, 3> sum{};
        double luminanceSum = 0.0, luminanceSquared = 0.0;
        double minimum = std::numeric_limits<double>::infinity(), maximum = -minimum;
        size_t finite = 0, nonfinite = 0, black = 0;
        for (size_t i = 0; i < size_t(width) * height; ++i)
        {
            const float* rgb = pixels + i * 4;
            if (!std::isfinite(rgb[0]) || !std::isfinite(rgb[1]) || !std::isfinite(rgb[2]))
            {
                ++nonfinite;
                continue;
            }
            ++finite;
            if (rgb[0] == 0.f && rgb[1] == 0.f && rgb[2] == 0.f) ++black;
            for (size_t c = 0; c < 3; ++c) sum[c] += rgb[c];
            const double luminance = 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2];
            luminanceSum += luminance;
            luminanceSquared += luminance * luminance;
            minimum = std::min(minimum, luminance);
            maximum = std::max(maximum, luminance);
        }
        const double denominator = double(std::max(size_t(1), finite));
        const double mean = luminanceSum / denominator;
        const double deviation = std::sqrt(std::max(0.0, luminanceSquared / denominator - mean * mean));
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - _start).count();
        _statistics << frame << ',' << seconds << ',' << width << ',' << height << ','
            << finite << ',' << nonfinite << ',' << black << ',' << sum[0] / denominator << ','
            << sum[1] / denominator << ',' << sum[2] / denominator << ',' << mean << ','
            << minimum << ',' << maximum << ',' << deviation << '\n';
        _statistics.flush();
    }

    std::vector<uint32_t> _captureFrames;
    std::vector<uint32_t> _denoiserToggleFrames;
    bool _captureRawOutput = false;
    bool _cameraMotion = false;
    bool _cameraCut = false;
    glm::vec3 _basePosition{};
    float _baseYaw = 0.0f;
    uint32_t _maxFrames = 0;
    std::filesystem::path _directory;
    std::ofstream _statistics;
    std::ofstream _gpuTimes;
    std::chrono::steady_clock::time_point _start;
};
