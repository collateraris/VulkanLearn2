#include "streamline_dlss.h"
#include "rhi.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <utility>

#ifndef RESTIR_HAS_STREAMLINE
#define RESTIR_HAS_STREAMLINE 0
#endif

#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <thread>
#include <sl.h>
#include <sl_dlss.h>
#include <sl_helpers.h>
#include <sl_helpers_vk.h>
#include <sl_security.h>
#endif

namespace rhi {

struct StreamlineDlss::Implementation {
    DlssRequirements requirements;
    std::string status = "DLSS is off";
    bool initialized = false;
    bool ready = false;
#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
    HMODULE module = nullptr;
    std::wstring pluginDirectory;
    PFN_vkGetInstanceProcAddr getInstanceProc = nullptr;
    PFN_vkGetDeviceProcAddr getDeviceProc = nullptr;
    PFun_slInit* init = nullptr;
    PFun_slShutdown* shutdown = nullptr;
    PFun_slGetFeatureRequirements* getRequirements = nullptr;
    PFun_slSetVulkanInfo* setVulkanInfo = nullptr;
    PFun_slIsFeatureSupported* isSupported = nullptr;
    PFun_slGetFeatureFunction* getFeatureFunction = nullptr;
    PFun_slGetNewFrameToken* getFrameToken = nullptr;
    PFun_slSetConstants* setConstants = nullptr;
    PFun_slEvaluateFeature* evaluateFeature = nullptr;
    PFun_slDLSSGetOptimalSettings* getOptimalSettings = nullptr;
    PFun_slDLSSSetOptions* setOptions = nullptr;
    const sl::ViewportHandle viewport{0};

    bool check(sl::Result result, const char* operation) {
        if (result == sl::Result::eOk) return true;
        ready = false;
        status = std::string(operation) + ": " + sl::getResultAsStr(result);
        std::cerr << "[DLSS] " << status << '\n';
        return false;
    }

    template<class T> bool load(T*& function, const char* name) {
        function = reinterpret_cast<T*>(GetProcAddress(module, name));
        if (function) return true;
        status = std::string("Streamline entry point missing: ") + name;
        return false;
    }
#endif
};

StreamlineDlss::StreamlineDlss() : _implementation(std::make_unique<Implementation>()) {}

StreamlineDlss::~StreamlineDlss() {
    shutdown();
#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
    // The engine must destroy its Vulkan objects before this object. In
    // particular, shutdown() must not invalidate its presentation dispatch.
    if (_implementation->module) FreeLibrary(_implementation->module);
#endif
}

const DlssRequirements& StreamlineDlss::requirements() const noexcept {
    return _implementation->requirements;
}
bool StreamlineDlss::supported() const noexcept { return _implementation->ready; }
const std::string& StreamlineDlss::status() const noexcept { return _implementation->status; }
void StreamlineDlss::disable(std::string reason) {
    _implementation->ready = false;
    _implementation->status = std::move(reason);
    std::cerr << "[DLSS] " << _implementation->status << '\n';
}

#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
namespace {
void write_shutdown_diagnostic(const char* message, DWORD length) noexcept {
    // A hung driver can hold CRT/logging locks. Use the process's existing
    // stderr handle directly; diagnostics are redirected there by the runner.
    const HANDLE output = GetStdHandle(STD_ERROR_HANDLE);
    if (output && output != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(output, message, length, &written, nullptr);
    }
}

sl::Result shutdown_with_watchdog(PFun_slShutdown* shutdown) {
    // NVIDIA's driver can block in NGX -> UninitializeTelemetry waiting for
    // its telemetry pipe. Keep the SDK call on this thread and its Vulkan
    // device alive. On timeout the process is the only safe cleanup boundary;
    // never abandon/kill the SDK thread and then unload its DLL or device.
    std::mutex mutex;
    std::condition_variable finished;
    bool complete = false;
    std::thread watchdog;
    try {
        watchdog = std::thread([&] {
            std::unique_lock<std::mutex> lock(mutex);
            if (finished.wait_for(lock, std::chrono::seconds(5), [&] { return complete; })) return;
            constexpr char message[] =
                "DLSS driver shutdown timed out; terminating the renderer process.\r\n";
            write_shutdown_diagnostic(message, sizeof(message) - 1);
            TerminateProcess(GetCurrentProcess(), 70);
        });
    } catch (const std::system_error&) {
        // Preserve the SDK's required synchronous cleanup if the OS cannot
        // create a watchdog thread. Do not destroy resources while it runs.
        constexpr char message[] =
            "DLSS shutdown watchdog could not start; waiting for synchronous driver cleanup.\r\n";
        write_shutdown_diagnostic(message, sizeof(message) - 1);
        return shutdown();
    }

    const sl::Result result = shutdown();
    {
        std::lock_guard<std::mutex> lock(mutex);
        complete = true;
    }
    finished.notify_one();
    watchdog.join();
    return result;
}

sl::DLSSMode dlss_mode(int mode) {
    switch (mode) {
    case 1: return sl::DLSSMode::eMaxQuality;
    case 2: return sl::DLSSMode::eBalanced;
    case 3: return sl::DLSSMode::eMaxPerformance;
    case 4: return sl::DLSSMode::eUltraPerformance;
    case 5: return sl::DLSSMode::eDLAA;
    default: return sl::DLSSMode::eOff;
    }
}

sl::DLSSOptions dlss_options(int mode, VkExtent2D output, float preExposure = 1.0f) {
    sl::DLSSOptions options;
    options.mode = dlss_mode(mode);
    options.outputWidth = output.width;
    options.outputHeight = output.height;
    options.colorBuffersHDR = sl::Boolean::eTrue;
    options.useAutoExposure = sl::Boolean::eTrue;
    options.preExposure = preExposure;
    options.exposureScale = 1.0f;
    // Select the supported K transformer for the lower-resolution modes as
    // well. The default Performance model on this SDK/driver reports initial
    // layout errors for private NGX images; keep that model path out of use.
    options.performancePreset = sl::DLSSPreset::ePresetK;
    options.ultraPerformancePreset = sl::DLSSPreset::ePresetK;
    return options;
}

void log_streamline(sl::LogType type, const char* message) {
    if (type == sl::LogType::eError || type == sl::LogType::eWarn)
        std::cerr << "[Streamline] " << message << '\n';
}

sl::Resource native_resource(const DlssNativeTexture& texture) {
    sl::Resource resource;
    resource.type = sl::ResourceType::eTex2d;
    resource.native = reinterpret_cast<void*>(texture.image);
    resource.view = reinterpret_cast<void*>(texture.view);
    resource.state = static_cast<uint32_t>(texture.layout);
    resource.width = texture.extent.width;
    resource.height = texture.extent.height;
    resource.nativeFormat = static_cast<uint32_t>(texture.format);
    resource.mipLevels = 1;
    resource.arrayLayers = 1;
    resource.flags = 0;
    resource.usage = texture.usage;
    return resource;
}

bool valid_texture(const DlssNativeTexture& texture, uint32_t width, uint32_t height) {
    return texture.image && texture.view && texture.format != VK_FORMAT_UNDEFINED &&
           texture.layout != VK_IMAGE_LAYOUT_UNDEFINED && width && height &&
           texture.extent.width == width && texture.extent.height == height;
}
} // namespace
#endif

bool StreamlineDlss::initialize_before_device() {
    auto& impl = *_implementation;
#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
    if (impl.initialized) return true;
    impl.ready = false;
    impl.requirements = {};
    std::error_code error;
    wchar_t executable[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (length == 0 || length >= 32768) {
        disable("Could not resolve the executable directory for Streamline");
        return false;
    }
    std::filesystem::path directory = std::filesystem::path(executable).parent_path() / L"streamline";
    wchar_t configured[32768]{};
    const DWORD configuredLength = GetEnvironmentVariableW(L"RESTIR_STREAMLINE_PATH", configured, 32768);
    if (configuredLength && configuredLength < 32768) directory = configured;
    if (!directory.is_absolute()) {
        disable("RESTIR_STREAMLINE_PATH must be an absolute SDK runtime directory");
        return false;
    }
    directory = std::filesystem::weakly_canonical(directory, error);
    const auto interposer = directory / L"sl.interposer.dll";
    if (error || !std::filesystem::is_regular_file(interposer, error)) {
        disable("Streamline runtime is unavailable; using native rendering");
        return false;
    }
    if (!impl.module) {
        // Use NVIDIA's SDK validation, including its secondary publisher
        // signature, before executing code from the optional DLL.
        if (!sl::security::verifyEmbeddedSignature(interposer.c_str())) {
            disable("Streamline interposer signature verification failed");
            return false;
        }
        impl.module = LoadLibraryExW(interposer.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!impl.module) {
            disable("Loading Streamline failed (Windows error " + std::to_string(GetLastError()) + ")");
            return false;
        }
    }
    if (!impl.load(impl.init, "slInit") || !impl.load(impl.shutdown, "slShutdown") ||
        !impl.load(impl.getRequirements, "slGetFeatureRequirements") ||
        !impl.load(impl.setVulkanInfo, "slSetVulkanInfo") ||
        !impl.load(impl.isSupported, "slIsFeatureSupported") ||
        !impl.load(impl.getFeatureFunction, "slGetFeatureFunction") ||
        !impl.load(impl.getFrameToken, "slGetNewFrameToken") ||
        !impl.load(impl.setConstants, "slSetConstants") ||
        !impl.load(impl.evaluateFeature, "slEvaluateFeature") ||
        !impl.load(impl.getInstanceProc, "vkGetInstanceProcAddr") ||
        !impl.load(impl.getDeviceProc, "vkGetDeviceProcAddr")) return false;

    impl.pluginDirectory = directory.wstring();
    const wchar_t* paths[] = {impl.pluginDirectory.c_str()};
    const sl::Feature features[] = {sl::kFeatureDLSS};
    sl::Preferences preferences;
    preferences.pathsToPlugins = paths;
    preferences.numPathsToPlugins = 1;
    preferences.featuresToLoad = features;
    preferences.numFeaturesToLoad = 1;
    preferences.renderAPI = sl::RenderAPI::eVulkan;
    preferences.flags = sl::PreferenceFlags::eDisableCLStateTracking |
                        sl::PreferenceFlags::eUseManualHooking |
                        sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    preferences.logMessageCallback = log_streamline;
    // This is the project's own identity, not an NVIDIA-assigned application ID.
    // Streamline accepts engine/version/project identity for development.
    preferences.applicationId = 0;
    preferences.engine = sl::EngineType::eCustom;
    preferences.engineVersion = "VulkanLearn2 1.0";
    preferences.projectId = "4db23db2-bbb2-4db2-893d-7d4bb9ea4a71";
    if (!impl.check(impl.init(preferences, sl::kSDKVersion), "slInit")) return false;
    impl.initialized = true;

    sl::FeatureRequirements requirements;
    if (!impl.check(impl.getRequirements(sl::kFeatureDLSS, requirements), "DLSS requirements")) return false;
    if (!(requirements.flags & sl::FeatureRequirementFlags::eVulkanSupported)) {
        disable("The installed DLSS plugin does not support Vulkan");
        return false;
    }
    for (uint32_t i = 0; i < requirements.vkNumInstanceExtensions; ++i)
        impl.requirements.instanceExtensions.emplace_back(requirements.vkInstanceExtensions[i]);
    for (uint32_t i = 0; i < requirements.vkNumDeviceExtensions; ++i)
        impl.requirements.deviceExtensions.emplace_back(requirements.vkDeviceExtensions[i]);
    impl.requirements.features12 = sl::getVkPhysicalDeviceVulkan12Features(
        requirements.vkNumFeatures12, requirements.vkFeatures12);
    impl.requirements.features13 = sl::getVkPhysicalDeviceVulkan13Features(
        requirements.vkNumFeatures13, requirements.vkFeatures13);
    // Streamline 2.14.1's Vulkan common backend creates a private-data slot
    // during device initialization, although SR's reported feature list omits
    // this requirement. Request it explicitly before native device creation.
    impl.requirements.features13.privateData = VK_TRUE;
    impl.requirements.graphicsQueues = requirements.vkNumGraphicsQueuesRequired;
    impl.requirements.computeQueues = requirements.vkNumComputeQueuesRequired;
    impl.status = "Streamline initialized; waiting for Vulkan device";
    std::cout << "[DLSS] Streamline SDK " << SL_VERSION_MAJOR << '.' << SL_VERSION_MINOR
              << '.' << SL_VERSION_PATCH << ", graphics queues " << impl.requirements.graphicsQueues
              << ", compute queues " << impl.requirements.computeQueues << '\n';
    return true;
#else
    disable("DLSS support was not built; using native rendering");
    return false;
#endif
}

bool StreamlineDlss::initialize_device(VkInstance instance, VkPhysicalDevice physicalDevice,
    VkDevice device, uint32_t graphicsFamily, uint32_t queueIndex,
    uint32_t computeFamily, uint32_t computeQueueIndex) {
    auto& impl = *_implementation;
#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
    if (!impl.initialized || !instance || !physicalDevice || !device) return false;
    sl::VulkanInfo info;
    info.instance = instance;
    info.physicalDevice = physicalDevice;
    info.device = device;
    info.graphicsQueueFamily = graphicsFamily;
    info.graphicsQueueIndex = queueIndex;
    info.computeQueueFamily = computeFamily == VK_QUEUE_FAMILY_IGNORED ? graphicsFamily : computeFamily;
    info.computeQueueIndex = computeQueueIndex;
    if (!impl.check(impl.setVulkanInfo(info), "slSetVulkanInfo")) return false;
    sl::AdapterInfo adapter;
    adapter.vkPhysicalDevice = physicalDevice;
    if (!impl.check(impl.isSupported(sl::kFeatureDLSS, adapter), "DLSS adapter support")) return false;
    void* function = nullptr;
    if (!impl.check(impl.getFeatureFunction(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", function),
                    "DLSS optimal-settings interface")) return false;
    impl.getOptimalSettings = reinterpret_cast<PFun_slDLSSGetOptimalSettings*>(function);
    function = nullptr;
    if (!impl.check(impl.getFeatureFunction(sl::kFeatureDLSS, "slDLSSSetOptions", function),
                    "DLSS options interface")) return false;
    impl.setOptions = reinterpret_cast<PFun_slDLSSSetOptions*>(function);
    if (!impl.getOptimalSettings || !impl.setOptions) {
        disable("DLSS plugin returned an empty feature interface");
        return false;
    }
    impl.ready = true;
    impl.status = "DLSS Super Resolution is available";
    std::cout << "[DLSS] " << impl.status << '\n';
    return true;
#else
    (void)instance; (void)physicalDevice; (void)device; (void)graphicsFamily;
    (void)queueIndex; (void)computeFamily; (void)computeQueueIndex;
    return false;
#endif
}

VkExtent2D StreamlineDlss::optimal_extent(int mode, VkExtent2D output) {
#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
    auto& impl = *_implementation;
    if (!impl.ready || dlss_mode(mode) == sl::DLSSMode::eOff || !output.width || !output.height) return output;
    const auto options = dlss_options(mode, output);
    sl::DLSSOptimalSettings settings;
    if (!impl.check(impl.getOptimalSettings(options, settings), "DLSS optimal render extent")) return output;
    if (!settings.optimalRenderWidth || !settings.optimalRenderHeight ||
        settings.optimalRenderWidth > output.width || settings.optimalRenderHeight > output.height) {
        disable("DLSS returned an invalid render extent");
        return output;
    }
    return {settings.optimalRenderWidth, settings.optimalRenderHeight};
#else
    (void)mode;
    return output;
#endif
}

bool StreamlineDlss::evaluate(VkCommandBuffer command, const DlssNativeTexture& color,
    const DlssNativeTexture& depth, const DlssNativeTexture& motion, const DlssNativeTexture& output,
    const TemporalUpscaleDescription& description) {
#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
    auto& impl = *_implementation;
    if (!impl.ready || !command || description.mode == UpscaleMode::Off) return false;
    if (!valid_texture(color, description.renderWidth, description.renderHeight) ||
        !valid_texture(depth, description.renderWidth, description.renderHeight) ||
        !valid_texture(motion, description.renderWidth, description.renderHeight) ||
        !valid_texture(output, description.outputWidth, description.outputHeight) ||
        !(output.usage & VK_IMAGE_USAGE_STORAGE_BIT) ||
        !std::isfinite(description.preExposure) || description.preExposure <= 0) {
        disable("DLSS received invalid input resources or exposure");
        return false;
    }
    const auto options = dlss_options(static_cast<int>(description.mode), output.extent, description.preExposure);
    if (!impl.check(impl.setOptions(impl.viewport, options), "DLSS options")) return false;
    sl::FrameToken* frame = nullptr;
    if (!impl.check(impl.getFrameToken(frame, &description.frameIndex), "DLSS frame token") || !frame) return false;
    sl::Constants constants;
    // The RHI provides row-major matrices for row-vector multiplication.
    // A GLM column-vector matrix has the same bytes as this transposed form.
    static_assert(sizeof(sl::float4x4) == 16 * sizeof(float));
    std::memcpy(&constants.cameraViewToClip, description.viewToClip.data(), sizeof(sl::float4x4));
    std::memcpy(&constants.clipToCameraView, description.clipToView.data(), sizeof(sl::float4x4));
    std::memcpy(&constants.clipToPrevClip, description.clipToPrevClip.data(), sizeof(sl::float4x4));
    std::memcpy(&constants.prevClipToClip, description.prevClipToClip.data(), sizeof(sl::float4x4));
    constants.clipToLensClip = {{sl::float4(1, 0, 0, 0), sl::float4(0, 1, 0, 0),
                               sl::float4(0, 0, 1, 0), sl::float4(0, 0, 0, 1)}};
    constants.jitterOffset = {description.jitterX, description.jitterY};
    constants.mvecScale = {description.motionScaleX, description.motionScaleY};
    constants.cameraPinholeOffset = {0, 0};
    constants.cameraPos = {description.cameraPosition[0], description.cameraPosition[1], description.cameraPosition[2]};
    constants.cameraUp = {description.cameraUp[0], description.cameraUp[1], description.cameraUp[2]};
    constants.cameraRight = {description.cameraRight[0], description.cameraRight[1], description.cameraRight[2]};
    constants.cameraFwd = {description.cameraForward[0], description.cameraForward[1], description.cameraForward[2]};
    constants.cameraNear = description.cameraNear;
    constants.cameraFar = description.cameraFar;
    constants.cameraFOV = description.cameraFov;
    constants.cameraAspectRatio = description.cameraAspect;
    constants.depthInverted = description.depthInverted ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    constants.cameraMotionIncluded = sl::Boolean::eTrue;
    constants.motionVectors3D = sl::Boolean::eFalse;
    constants.motionVectorsJittered = sl::Boolean::eFalse;
    constants.reset = description.reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    if (!impl.check(impl.setConstants(constants, *frame, impl.viewport), "DLSS frame constants")) return false;

    auto colorResource = native_resource(color);
    auto depthResource = native_resource(depth);
    auto motionResource = native_resource(motion);
    auto outputResource = native_resource(output);
    const sl::Extent inputExtent{0, 0, description.renderWidth, description.renderHeight};
    const sl::Extent outputExtent{0, 0, description.outputWidth, description.outputHeight};
    sl::ResourceTag colorTag(&colorResource, sl::kBufferTypeScalingInputColor, sl::eValidUntilEvaluate, &inputExtent);
    sl::ResourceTag depthTag(&depthResource, sl::kBufferTypeDepth, sl::eValidUntilEvaluate, &inputExtent);
    sl::ResourceTag motionTag(&motionResource, sl::kBufferTypeMotionVectors, sl::eValidUntilEvaluate, &inputExtent);
    sl::ResourceTag outputTag(&outputResource, sl::kBufferTypeScalingOutputColor, sl::eValidUntilEvaluate, &outputExtent);
    const sl::BaseStructure* inputs[] = {&impl.viewport, &colorTag, &depthTag, &motionTag, &outputTag};
    // Tags are consumed for this frame by EvaluateFeature; there is no shared
    // mutable tag set and no copy needed for later frame-generation evaluation.
    // NGX keeps opaque scratch resources across evaluations. They cannot be
    // imported into our graph, so synchronize their compute and transfer work
    // here, including transfer writes from vkCmdFillBuffer in earlier frames.
    VkMemoryBarrier workspaceBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    workspaceBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    workspaceBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    constexpr VkPipelineStageFlags workspaceStages =
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    vkCmdPipelineBarrier(command, workspaceStages, workspaceStages, 0,
        1, &workspaceBarrier, 0, nullptr, 0, nullptr);
    return impl.check(impl.evaluateFeature(sl::kFeatureDLSS, *frame, inputs, 5,
        reinterpret_cast<sl::CommandBuffer*>(command)), "DLSS evaluation");
#else
    (void)command; (void)color; (void)depth; (void)motion; (void)output; (void)description;
    return false;
#endif
}

PFN_vkGetInstanceProcAddr StreamlineDlss::instance_proc_addr() const noexcept {
#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
    return _implementation->initialized ? _implementation->getInstanceProc : nullptr;
#else
    return nullptr;
#endif
}
PFN_vkGetDeviceProcAddr StreamlineDlss::device_proc_addr() const noexcept {
#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
    return _implementation->initialized ? _implementation->getDeviceProc : nullptr;
#else
    return nullptr;
#endif
}

void StreamlineDlss::shutdown() {
    auto& impl = *_implementation;
    impl.ready = false;
#if RESTIR_HAS_STREAMLINE && defined(_WIN32)
    if (impl.initialized && impl.shutdown) {
        impl.check(shutdown_with_watchdog(impl.shutdown), "slShutdown");
        impl.initialized = false;
        impl.getOptimalSettings = nullptr;
        impl.setOptions = nullptr;
    }
#endif
}

} // namespace rhi
