include(FetchContent)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# ----------------------------------------------------------------------------
# ONNX Runtime
# ----------------------------------------------------------------------------
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

if(ONNXRUNTIME_LIB AND ONNXRUNTIME_INCLUDE_DIR)
    set(ONNXRUNTIME_LIBRARIES ${ONNXRUNTIME_LIB})
    set(ONNXRUNTIME_INCLUDE_DIRS ${ONNXRUNTIME_INCLUDE_DIR})
    message(STATUS "找到 ONNX Runtime: ${ONNXRUNTIME_LIB}")
    add_compile_definitions(HAVE_ONNXRUNTIME)
else()
    message(WARNING "未找到 ONNX Runtime，推理功能将使用占位实现")
endif()

# ----------------------------------------------------------------------------
# PortAudio
# ----------------------------------------------------------------------------
set(PORTAUDIO_ROOT "${THIRD_PARTY_DIR}/portaudio")

find_library(PORTAUDIO_LIB
    NAMES portaudio libportaudio
    PATHS "${PORTAUDIO_ROOT}/lib"
    NO_DEFAULT_PATH
)
find_path(PORTAUDIO_INCLUDE_DIR
    NAMES portaudio.h
    PATHS "${PORTAUDIO_ROOT}/include"
    NO_DEFAULT_PATH
)

if(PORTAUDIO_LIB AND PORTAUDIO_INCLUDE_DIR)
    set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIB})
    set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIR})
    message(STATUS "找到 PortAudio: ${PORTAUDIO_LIB}")
    add_compile_definitions(HAVE_PORTAUDIO)
else()
    message(WARNING "未找到 PortAudio，音频采集功能将使用占位实现")
endif()

# ----------------------------------------------------------------------------
# dr_libs (header-only)
# ----------------------------------------------------------------------------
set(DR_LIBS_INCLUDE_DIR "${THIRD_PARTY_DIR}/dr_libs")
if(EXISTS "${DR_LIBS_INCLUDE_DIR}/dr_wav.h")
    message(STATUS "找到 dr_libs: ${DR_LIBS_INCLUDE_DIR}")
    add_compile_definitions(HAVE_DR_LIBS)
else()
    message(WARNING "未找到 dr_libs 头文件")
endif()

# ----------------------------------------------------------------------------
# nlohmann/json (header-only)
# ----------------------------------------------------------------------------
set(NLOHMANN_JSON_INCLUDE_DIR "${THIRD_PARTY_DIR}/nlohmann_json")
