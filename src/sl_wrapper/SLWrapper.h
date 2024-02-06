#pragma once

#include <vk_types.h>

#if STREAMLINE_ON

class VulkanEngine;

// This is a wrapper around SL functionality for DLSS. It is seperated to provide focus to the calls specific to NGX for code sample purposes.
class SLWrapper
{

public:

    static SLWrapper& Get();
    SLWrapper(const SLWrapper&) = delete;
    SLWrapper(SLWrapper&&) = delete;
    SLWrapper& operator=(const SLWrapper&) = delete;
    SLWrapper& operator=(SLWrapper&&) = delete;

    bool Initialize_preDevice(VulkanEngine* engine);

private:

    SLWrapper() {}

    VulkanEngine* _engine = nullptr;

    bool m_sl_initialised = false;
   
};

#endif

