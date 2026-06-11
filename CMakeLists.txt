cmake_minimum_required(VERSION 3.20)
project(impress_voice_input
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "基于 ONNX 的实时语音转文本输入法"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 默认使用 Release 带调试信息
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# 依赖查找
# ============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Qt 6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent Network)

# Windows 不需要 D-Bus，Linux/macOS 按需加载
if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Gui)
else()
    find_package(Qt6 REQUIRED COMPONENTS DBus)
endif()

qt_standard_project_setup()

# ONNX Runtime
include(dependencies)

# ============================================================================
# 源文件
# ============================================================================
set(SOURCES
    src/main.cpp

    # App
    src/app/application.cpp
    src/app/config_manager.cpp

    # Core (平台无关)
    src/core/stt_engine.cpp
    src/core/sense_voice_engine.cpp
    src/core/ort_minimal.cpp
    src/core/sense_voice_features.cpp
    src/core/sense_voice_tokenizer.cpp
    src/core/mel_spectrogram.cpp
    src/core/whisper_tokenizer.cpp
    src/core/audio_processor.cpp
    src/core/vad.cpp
    src/core/voice_input_service.cpp

    # Audio
    src/audio/audio_capture.cpp
    src/audio/audio_decoder.cpp
    src/audio/audio_ring_buffer.cpp
    src/audio/streaming_audio_writer.cpp

    # UI
    src/ui/main_window.cpp
    src/ui/stt_test_page.cpp
    src/ui/file_transcribe_page.cpp
    src/ui/settings_page.cpp
    src/ui/widgets/audio_waveform.cpp
    src/ui/widgets/text_output.cpp
    src/ui/widgets/progress_panel.cpp
    src/ui/widgets/hotkey_recorder.cpp

    # Utils
    src/utils/logger.cpp
    src/utils/timer.cpp
    src/utils/string_utils.cpp
)

set(HEADERS
    src/app/application.h
    src/app/config_manager.h

    src/core/stt_engine.h
    src/core/sense_voice_engine.h
    src/core/sense_voice_features.h
    src/core/sense_voice_tokenizer.h
    src/core/sense_voice_cmvn.h
    src/core/mel_spectrogram.h
    src/core/whisper_tokenizer.h
    src/core/audio_processor.h
    src/core/vad.h
    src/core/voice_input_service.h

    src/audio/audio_capture.h
    src/audio/audio_decoder.h
    src/audio/audio_ring_buffer.h
    src/audio/streaming_audio_writer.h

    src/ui/main_window.h
    src/ui/stt_test_page.h
    src/ui/file_transcribe_page.h
    src/ui/settings_page.h
    src/ui/widgets/audio_waveform.h
    src/ui/widgets/text_output.h
    src/ui/widgets/progress_panel.h
    src/ui/widgets/hotkey_recorder.h

    src/utils/logger.h
    src/utils/timer.h
    src/utils/string_utils.h
)

# ============================================================================
# 平台特定源文件
# ============================================================================
if(WIN32)
    # Windows 实现（RegisterHotKey + SendInput）
    list(APPEND SOURCES src/core/win_hotkey.cpp src/core/win_text_injector.cpp)
    list(APPEND HEADERS src/core/win_hotkey.h src/core/win_text_injector.h)
    add_compile_definitions(PLATFORM_WINDOWS)
elseif(APPLE)
    # macOS 实现（待完善：Carbon EventManager / CGEvent）
    list(APPEND SOURCES src/core/mac_hotkey.cpp src/core/mac_text_injector.cpp)
    list(APPEND HEADERS src/core/mac_hotkey.h src/core/mac_text_injector.h)
    add_compile_definitions(PLATFORM_MACOS)
else()
    # Linux 实现（D-Bus Portal + XTest）
    list(APPEND SOURCES src/core/caps_lock_voice_hotkey.cpp src/core/wayland_text_injector.cpp)
    list(APPEND HEADERS src/core/caps_lock_voice_hotkey.h src/core/wayland_text_injector.h)
    add_compile_definitions(PLATFORM_LINUX)
endif()
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS}
    src/ui/resources/styles/styles.qrc
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${ONNXRUNTIME_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${DR_LIBS_INCLUDE_DIR}
    ${NLOHMANN_JSON_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
    $<$<BOOL:${WIN32}>:Qt6::Gui>
    $<$<NOT:$<BOOL:${WIN32}>>:Qt6::DBus>
    ${ONNXRUNTIME_LIBRARIES}
    ${PORTAUDIO_LIBRARIES}
    $<$<NOT:$<BOOL:${WIN32}>>:pthread>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ============================================================================
# Windows DLL 复制（交叉编译时自动拷贝到输出目录）
# ============================================================================
if(WIN32)
    if(ONNXRUNTIME_DLL)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNXRUNTIME_DLL}"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "拷贝 onnxruntime.dll 到输出目录"
        )
    endif()
    if(ONNXRUNTIME_PROVIDERS_DLL)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ONNXRUNTIME_PROVIDERS_DLL}"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "拷贝 onnxruntime_providers_shared.dll 到输出目录"
        )
    endif()
    if(PORTAUDIO_DLL)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PORTAUDIO_DLL}"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "拷贝 libportaudio.dll 到输出目录"
        )
    endif()
endif()


# ============================================================================
# 安装
# ============================================================================
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

# ============================================================================
# 测试
# ============================================================================
option(BUILD_TESTS "构建单元测试" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
