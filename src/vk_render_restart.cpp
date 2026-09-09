#include "vk_engine.h"
#include <SDL.h>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace {
constexpr wchar_t restartVariable[] = L"RESTIR_RENDERER_RESTART_STATE";
}

bool VulkanEngine::launch_renderer_restart()
{
#if defined(_WIN32)
    const auto resume = capture_resume_state();
    std::ostringstream state;
    state.imbue(std::locale::classic());
    state << std::setprecision(std::numeric_limits<float>::max_digits10)
        << 2 << ' ' << _pendingRenderSettings.outputWidth << ' ' << _pendingRenderSettings.outputHeight << ' '
        << _pendingRenderSettings.dlssMode << ' ' << resume.position.x << ' ' << resume.position.y << ' '
        << resume.position.z << ' ' << resume.pitch << ' ' << resume.yaw << ' ' << resume.fov << ' '
        << resume.activeCamera << ' ' << resume.accumulation << ' ' << resume.denoiser << ' ' << resume.hasSun << ' '
        << resume.sunDirection.x << ' ' << resume.sunDirection.y << ' ' << resume.sunDirection.z << ' '
        << resume.sunColor.x << ' ' << resume.sunColor.y << ' ' << resume.sunColor.z << ' ' << resume.numRays << ' '
        << resume.hasGeneratedLightSeed << ' ' << resume.generatedLightSeed;
    const std::string text = state.str();
    const std::wstring inherited(text.begin(), text.end());
    wchar_t executable[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768 || !SetEnvironmentVariableW(restartVariable, inherited.c_str()))
    {
        _renderSettingsError = "Could not prepare renderer restart.";
        _reloadRequested = false;
        return false;
    }
    // Start a fresh process so driver-owned DLSS state cannot leak between
    // devices. Only this child inherits the temporary camera/settings handoff.
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring command = L"\"" + std::wstring(executable, length) + L"\"";
    const BOOL started = CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE,
        CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &startup, &process);
    const DWORD error = started ? ERROR_SUCCESS : GetLastError();
    SetEnvironmentVariableW(restartVariable, nullptr);
    if (!started)
    {
        _renderSettingsError = "Could not restart renderer (Windows error " + std::to_string(error) + ").";
        _reloadRequested = false;
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    SDL_HideWindow(_window);
    return true;
#else
    _renderSettingsError = "Renderer restart currently requires Windows.";
    _reloadRequested = false;
    return false;
#endif
}

bool VulkanEngine::read_restart_state(RenderSettings& settings, ResumeState& resume)
{
#if defined(_WIN32)
    wchar_t inherited[4096]{};
    const DWORD length = GetEnvironmentVariableW(restartVariable, inherited, 4096);
    SetEnvironmentVariableW(restartVariable, nullptr);
    if (!length || length >= 4096) return false;
    const std::wstring wide(inherited, length);
    std::istringstream input(std::string(wide.begin(), wide.end()));
    input.imbue(std::locale::classic());
    int version = 0;
    int64_t generatedLightSeed = 0;
    RenderSettings candidate;
    ResumeState restored;
    if (!(input >> version >> candidate.outputWidth >> candidate.outputHeight >> candidate.dlssMode
        >> restored.position.x >> restored.position.y >> restored.position.z >> restored.pitch >> restored.yaw
        >> restored.fov >> restored.activeCamera >> restored.accumulation >> restored.denoiser >> restored.hasSun
        >> restored.sunDirection.x >> restored.sunDirection.y >> restored.sunDirection.z
        >> restored.sunColor.x >> restored.sunColor.y >> restored.sunColor.z >> restored.numRays
        >> restored.hasGeneratedLightSeed >> generatedLightSeed)) return false;
    input >> std::ws;
    if (!input.eof() || version != 2 || candidate.outputWidth < 320 || candidate.outputWidth > 7680 ||
        candidate.outputHeight < 200 || candidate.outputHeight > 4320 || candidate.dlssMode < 0 || candidate.dlssMode > 5 ||
        restored.fov <= 0 || restored.fov >= 180 || restored.numRays < 0 || restored.numRays > 32 ||
        generatedLightSeed < 0 || generatedLightSeed > std::numeric_limits<uint32_t>::max()) return false;
    for (const float value : {restored.position.x, restored.position.y, restored.position.z, restored.pitch, restored.yaw,
        restored.fov, restored.sunDirection.x, restored.sunDirection.y, restored.sunDirection.z,
        restored.sunColor.x, restored.sunColor.y, restored.sunColor.z})
        if (!std::isfinite(value)) return false;
    if (restored.hasSun && (glm::dot(restored.sunDirection, restored.sunDirection) <= 1e-10f ||
        restored.sunColor.x < 0 || restored.sunColor.y < 0 || restored.sunColor.z < 0)) return false;
    restored.valid = true;
    restored.generatedLightSeed = static_cast<uint32_t>(generatedLightSeed);
    settings = candidate;
    resume = restored;
    return true;
#else
    (void)settings; (void)resume;
    return false;
#endif
}
