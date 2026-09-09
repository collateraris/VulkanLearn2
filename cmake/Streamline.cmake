# Optional NVIDIA Streamline 2.14.1 Super Resolution runtime.
# SDK binaries are intentionally kept outside the source tree. Acquire the
# public signed SDK with -DRESTIR_FETCH_STREAMLINE=ON, or set the SDK directory.
include_guard(GLOBAL)
option(RESTIR_ENABLE_STREAMLINE "Build the optional Streamline DLSS SR backend" ON)
option(RESTIR_FETCH_STREAMLINE "Download the pinned official Streamline SDK if missing" OFF)
set(RESTIR_STREAMLINE_SDK_DIR "${PROJECT_SOURCE_DIR}/win64/streamline-sdk-2.14.1"
    CACHE PATH "Streamline SDK directory containing include/sl.h and bin/x64")
set(RESTIR_STREAMLINE_VERSION "2.14.1")
set(RESTIR_STREAMLINE_SHA256 "92c4d954631a1710da86ca3fa8d5034f2b9503838c95fc4ae977ae149319781b")

function(restir_configure_streamline target)
    if(WIN32 AND RESTIR_ENABLE_STREAMLINE AND RESTIR_FETCH_STREAMLINE
       AND NOT EXISTS "${RESTIR_STREAMLINE_SDK_DIR}/include/sl.h")
        set(archive "${CMAKE_BINARY_DIR}/downloads/streamline-sdk-v${RESTIR_STREAMLINE_VERSION}.zip")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/downloads")
        file(DOWNLOAD
            "https://github.com/NVIDIA-RTX/Streamline/releases/download/v${RESTIR_STREAMLINE_VERSION}/streamline-sdk-v${RESTIR_STREAMLINE_VERSION}.zip"
            "${archive}" SHOW_PROGRESS TLS_VERIFY ON
            EXPECTED_HASH "SHA256=${RESTIR_STREAMLINE_SHA256}")
        file(MAKE_DIRECTORY "${RESTIR_STREAMLINE_SDK_DIR}")
        file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${RESTIR_STREAMLINE_SDK_DIR}")
    endif()

    # sl.common initializes its Vulkan low-latency helper even for SR alone.
    set(runtime_files sl.interposer.dll sl.common.dll sl.dlss.dll nvngx_dlss.dll NvLowLatencyVk.dll)
    set(available TRUE)
    if(NOT WIN32 OR NOT RESTIR_ENABLE_STREAMLINE OR
       NOT EXISTS "${RESTIR_STREAMLINE_SDK_DIR}/include/sl.h")
        set(available FALSE)
    endif()
    foreach(runtime IN LISTS runtime_files)
        if(NOT EXISTS "${RESTIR_STREAMLINE_SDK_DIR}/bin/x64/${runtime}")
            set(available FALSE)
        endif()
    endforeach()
    if(available)
        foreach(component IN ITEMS MAJOR MINOR PATCH)
            file(STRINGS "${RESTIR_STREAMLINE_SDK_DIR}/include/sl_version.h" version_line
                 REGEX "^#define SL_VERSION_${component} [0-9]+")
            string(REGEX REPLACE ".*SL_VERSION_${component} ([0-9]+).*" "\\1"
                   sdk_${component} "${version_line}")
        endforeach()
        set(sdk_version "${sdk_MAJOR}.${sdk_MINOR}.${sdk_PATCH}")
        if(sdk_version VERSION_LESS "2.14.1")
            message(WARNING "Streamline ${sdk_version} is too old; select the signed 2.14.1 SDK or newer")
            set(available FALSE)
        endif()
    endif()
    if(NOT available)
        target_compile_definitions(${target} PRIVATE RESTIR_HAS_STREAMLINE=0)
        message(STATUS "Streamline DLSS unavailable: native rendering remains enabled")
        return()
    endif()

    target_compile_definitions(${target} PRIVATE RESTIR_HAS_STREAMLINE=1)
    target_include_directories(${target} BEFORE PRIVATE "${RESTIR_STREAMLINE_SDK_DIR}/include")
    # The loader resolves all SL entry points dynamically. Never link the
    # interposer import library: a missing runtime must not prevent app startup.
    set(runtime_paths)
    foreach(runtime IN LISTS runtime_files)
        list(APPEND runtime_paths "${RESTIR_STREAMLINE_SDK_DIR}/bin/x64/${runtime}")
    endforeach()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${target}>/streamline"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${runtime_paths}
            "$<TARGET_FILE_DIR:${target}>/streamline"
        COMMENT "Deploying signed Streamline DLSS Super Resolution runtime"
        VERBATIM)
    foreach(license IN ITEMS license.txt bin/x64/nvngx_dlss.license.txt bin/x64/reflex.license.txt)
        if(EXISTS "${RESTIR_STREAMLINE_SDK_DIR}/${license}")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${RESTIR_STREAMLINE_SDK_DIR}/${license}"
                    "$<TARGET_FILE_DIR:${target}>/streamline"
                VERBATIM)
        endif()
    endforeach()
    message(STATUS "Streamline DLSS SDK: ${RESTIR_STREAMLINE_SDK_DIR} (dynamic, SR only)")
endfunction()
