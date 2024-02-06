#include "SLWrapper.h"

#if STREAMLINE_ON

#include <vk_engine.h>

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


std::wstring GetSlInterposerDllLocation() {

    return std::wstring(L"\\sl.interposer.dll");
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

    return true;
};

#endif

