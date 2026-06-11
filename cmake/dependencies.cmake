include(FetchContent)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# ============================================================================
# ONNX Runtime
# ============================================================================
if(WIN32)
    # Windows 版本：onnxruntime.dll
    set(ONNXRUNTIME_ROOT "${THIRD_PARTY_DIR}/onnxruntime-win-x64")
    if(NOT EXISTS "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll")
        # 回退到旧目录名
        set(ONNXRUNTIME_ROOT "${THIRD_PARTY_DIR}/onnxruntime")
    endif()
    # 直接用 DLL 路径（MinGW 可直接链接 DLL）
    if(EXISTS "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll")
        set(ONNXRUNTIME_LIB "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll")
        set(ONNXRUNTIME_DLL "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll")
        set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include")
    endif()
    if(NOT ONNXRUNTIME_INCLUDE_DIR)
        set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include")
    endif()
else()
    # Linux 版本：libonnxruntime.so
    set(ONNXRUNTIME_ROOT "${THIRD_PARTY_DIR}/onnxruntime")
    find_library(ONNXRUNTIME_LIB
        NAMES onnxruntime
        PATHS "${ONNXRUNTIME_ROOT}/lib"
        NO_DEFAULT_PATH
    )
    find_path(ONNXRUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATHS "${ONNXRUNTIME_ROOT}/include"
        NO_DEFAULT_PATH
    )
endif()

if(ONNXRUNTIME_LIB AND ONNXRUNTIME_INCLUDE_DIR)
    set(ONNXRUNTIME_LIBRARIES ${ONNXRUNTIME_LIB})
    set(ONNXRUNTIME_INCLUDE_DIRS ${ONNXRUNTIME_INCLUDE_DIR})
    message(STATUS "找到 ONNX Runtime: ${ONNXRUNTIME_LIB}")
    add_compile_definitions(HAVE_ONNXRUNTIME)
else()
    message(WARNING "未找到 ONNX Runtime，推理功能将使用占位实现")
endif()

# ============================================================================
# PortAudio
# ============================================================================
set(PORTAUDIO_ROOT "${THIRD_PARTY_DIR}/portaudio")

if(WIN32)
    # Windows 版本：libportaudio.dll 在 bin/ 目录
    if(EXISTS "${PORTAUDIO_ROOT}/bin/libportaudio.dll")
        set(PORTAUDIO_LIB "${PORTAUDIO_ROOT}/bin/libportaudio.dll")
        set(PORTAUDIO_DLL "${PORTAUDIO_ROOT}/bin/libportaudio.dll")
    endif()
    if(EXISTS "${PORTAUDIO_ROOT}/include/portaudio.h")
        set(PORTAUDIO_INCLUDE_DIR "${PORTAUDIO_ROOT}/include")
    endif()
else()
    # Linux 版本：优先使用构建好的本地库
    find_library(PORTAUDIO_LIB
        NAMES portaudio libportaudio
        PATHS "${PORTAUDIO_ROOT}/lib" /usr/lib64 /usr/lib /usr/local/lib
        NO_DEFAULT_PATH
    )
    find_path(PORTAUDIO_INCLUDE_DIR
        NAMES portaudio.h
        PATHS "${PORTAUDIO_ROOT}/include" /usr/include /usr/include/portaudio /usr/local/include
        NO_DEFAULT_PATH
    )

    # 回退：通过 pkg-config 查找
    if(NOT PORTAUDIO_LIB OR NOT PORTAUDIO_INCLUDE_DIR)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(PORTAUDIO_PC portaudio-2.0 QUIET)
            if(PORTAUDIO_PC_FOUND)
                set(PORTAUDIO_LIBRARIES ${PORTAUDIO_PC_LIBRARIES})
                set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_PC_INCLUDE_DIRS})
            endif()
        endif()
    endif()
endif()

if(PORTAUDIO_LIB AND PORTAUDIO_INCLUDE_DIR)
    set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIB})
    set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIR})
    message(STATUS "找到 PortAudio: ${PORTAUDIO_LIB}")
    add_compile_definitions(HAVE_PORTAUDIO)
else()
    message(WARNING "未找到 PortAudio，音频采集功能将使用占位实现")
endif()

# ============================================================================
# dr_libs (header-only)
# ============================================================================
set(DR_LIBS_INCLUDE_DIR "${THIRD_PARTY_DIR}/dr_libs")
if(EXISTS "${DR_LIBS_INCLUDE_DIR}/dr_wav.h")
    message(STATUS "找到 dr_libs: ${DR_LIBS_INCLUDE_DIR}")
    add_compile_definitions(HAVE_DR_LIBS)
else()
    message(WARNING "未找到 dr_libs 头文件")
endif()

# ============================================================================
# nlohmann/json (header-only)
# ============================================================================
set(NLOHMANN_JSON_INCLUDE_DIR "${THIRD_PARTY_DIR}/nlohmann_json")
