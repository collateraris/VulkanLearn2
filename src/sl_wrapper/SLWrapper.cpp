#include "SLWrapper.h"

#if STREAMLINE_ON

#include <vk_engine.h>
#include <sys_config/vk_strings.h>

// Streamline Core
#include <sl.h>
#include <sl_consts.h>
#include <sl_hooks.h>
#include <sl_version.h>

// Streamline Features
#include <sl_dlss.h>
#include <sl_reflex.h>
#include <sl_nis.h>
#ifdef DLSSG_ALLOWED // NDA ONLY DLSS-G DLSS_G Release
#include <sl_dlss_g.h>
#endif // DLSSG_ALLOWED END NDA ONLY DLSS-G DLSS_G Release

#include <filesystem>
#include <sl_security.h>

#define PATH_MAX MAX_PATH

static constexpr int APP_ID = 231313132;

void logFunctionCallback(sl::LogType type, const char* msg) {

    if (type == sl::LogType::eError) {
        // Add a breakpoint here to break on errors
        PLOG_ERROR << std::string(msg) + "\n";
    }
    if (type == sl::LogType::eWarn) {
        // Add a breakpoint here to break on warnings
        PLOG_WARNING << std::string(msg) + "\n";
    }
    else {
        PLOG_INFO << std::string(msg) + "\n";
    }
}

bool successCheck(sl::Result result, const char* location) {

    if (result == sl::Result::eOk)
        return true;

    const std::map< const sl::Result, const std::string> errors = {
            {sl::Result::eErrorIO,"eErrorIO"},
            {sl::Result::eErrorDriverOutOfDate,"eErrorDriverOutOfDate"},
            {sl::Result::eErrorOSOutOfDate,"eErrorOSOutOfDate"},
            {sl::Result::eErrorOSDisabledHWS,"eErrorOSDisabledHWS"},
            {sl::Result::eErrorDeviceNotCreated,"eErrorDeviceNotCreated"},
            {sl::Result::eErrorAdapterNotSupported,"eErrorAdapterNotSupported"},
            {sl::Result::eErrorNoPlugins,"eErrorNoPlugins"},
            {sl::Result::eErrorVulkanAPI,"eErrorVulkanAPI"},
            {sl::Result::eErrorDXGIAPI,"eErrorDXGIAPI"},
            {sl::Result::eErrorD3DAPI,"eErrorD3DAPI"},
            {sl::Result::eErrorNRDAPI,"eErrorNRDAPI"},
            {sl::Result::eErrorNVAPI,"eErrorNVAPI"},
            {sl::Result::eErrorReflexAPI,"eErrorReflexAPI"},
            {sl::Result::eErrorNGXFailed,"eErrorNGXFailed"},
            {sl::Result::eErrorJSONParsing,"eErrorJSONParsing"},
            {sl::Result::eErrorMissingProxy,"eErrorMissingProxy"},
            {sl::Result::eErrorMissingResourceState,"eErrorMissingResourceState"},
            {sl::Result::eErrorInvalidIntegration,"eErrorInvalidIntegration"},
            {sl::Result::eErrorMissingInputParameter,"eErrorMissingInputParameter"},
            {sl::Result::eErrorNotInitialized,"eErrorNotInitialized"},
            {sl::Result::eErrorComputeFailed,"eErrorComputeFailed"},
            {sl::Result::eErrorInitNotCalled,"eErrorInitNotCalled"},
            {sl::Result::eErrorExceptionHandler,"eErrorExceptionHandler"},
            {sl::Result::eErrorInvalidParameter,"eErrorInvalidParameter"},
            {sl::Result::eErrorMissingConstants,"eErrorMissingConstants"},
            {sl::Result::eErrorDuplicatedConstants,"eErrorDuplicatedConstants"},
            {sl::Result::eErrorMissingOrInvalidAPI,"eErrorMissingOrInvalidAPI"},
            {sl::Result::eErrorCommonConstantsMissing,"eErrorCommonConstantsMissing"},
            {sl::Result::eErrorUnsupportedInterface,"eErrorUnsupportedInterface"},
            {sl::Result::eErrorFeatureMissing,"eErrorFeatureMissing"},
            {sl::Result::eErrorFeatureNotSupported,"eErrorFeatureNotSupported"},
            {sl::Result::eErrorFeatureMissingHooks,"eErrorFeatureMissingHooks"},
            {sl::Result::eErrorFeatureFailedToLoad,"eErrorFeatureFailedToLoad"},
            {sl::Result::eErrorFeatureWrongPriority,"eErrorFeatureWrongPriority"},
            {sl::Result::eErrorFeatureMissingDependency,"eErrorFeatureMissingDependency"},
            {sl::Result::eErrorFeatureManagerInvalidState,"eErrorFeatureManagerInvalidState"},
            {sl::Result::eErrorInvalidState,"eErrorInvalidState"},
            {sl::Result::eWarnOutOfVRAM,"eWarnOutOfVRAM"} };

    auto a = errors.find(result);
    if (a != errors.end())
        logFunctionCallback(sl::LogType::eError, ("Error: " + a->second + (location == nullptr ? "" : (" encountered in " + std::string(location)))).c_str());
    else
        logFunctionCallback(sl::LogType::eError, ("Unknown error " + static_cast<int>(result) + (location == nullptr ? "" : (" encountered in " + std::string(location)))).c_str());

    return false;

}

std::wstring GetSlInterposerDllLocation() {
    static std::string path = vk_utils::STREAMLINE_DLL + "/sl.interposer.dll";
    return std::wstring(path.begin(), path.end());
}

SLWrapper& SLWrapper::Get() {
    static SLWrapper instance;
    return instance;
}

bool SLWrapper::Initialize_preDevice(VulkanEngine* engine)
{
    _engine = engine;

    if (m_sl_initialised) {
        _engine->_logger.debug_log("SLWrapper is already initialised.");
        return true;
    }

    sl::Preferences pref;

    pref.applicationId = APP_ID;

#if _DEBUG
    pref.showConsole = true;
    pref.logMessageCallback = &logFunctionCallback;
    pref.logLevel = sl::LogLevel::eDefault;
#else
    pref.logLevel = sl::LogLevel::eOff;
#endif

    sl::Feature featuresToLoad[] = {
        sl::kFeatureDLSS,
        sl::kFeatureNRD,
    };
    pref.featuresToLoad = featuresToLoad;
    pref.numFeaturesToLoad = _countof(featuresToLoad);
    pref.renderAPI = sl::RenderAPI::eVulkan;

    pref.flags |= sl::PreferenceFlags::eUseManualHooking;

    auto pathDll = GetSlInterposerDllLocation();

    HMODULE interposer = {};
    if (sl::security::verifyEmbeddedSignature(pathDll.c_str())) {
        interposer = LoadLibraryW(pathDll.c_str());
    }
    
    if (!interposer)
    {
        PLOG_ERROR << "Unable to load Streamline Interposer\n";
        return false;
    }

    m_sl_initialised = successCheck(slInit(pref, sl::kSDKVersion), "slInit");
    if (!m_sl_initialised) {
        PLOG_ERROR << "Failed to initialse SL\n";
        return false;
    }

    return true;
};

#endif

