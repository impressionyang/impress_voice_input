cmake_minimum_required(VERSION 3.20)
project(impress_voice_input
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "基于 ONNX 的实时语音转文本输入法"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

    # Core
    src/core/stt_engine.cpp
    src/core/mel_spectrogram.cpp
    src/core/whisper_tokenizer.cpp
    src/core/audio_processor.cpp
    src/core/decoder.cpp
    src/core/tokenizer.cpp

    # Audio
    src/audio/audio_capture.cpp
    src/audio/audio_decoder.cpp
    src/audio/audio_ring_buffer.cpp

    # UI
    src/ui/main_window.cpp
    src/ui/stt_test_page.cpp
    src/ui/file_transcribe_page.cpp
    src/ui/settings_page.cpp
    src/ui/widgets/audio_waveform.cpp
    src/ui/widgets/text_output.cpp
    src/ui/widgets/progress_panel.cpp

    # Utils
    src/utils/logger.cpp
    src/utils/timer.cpp
    src/utils/string_utils.cpp
)

set(HEADERS
    src/app/application.h
    src/app/config_manager.h

    src/core/stt_engine.h
    src/core/mel_spectrogram.h
    src/core/whisper_tokenizer.h
    src/core/audio_processor.h
    src/core/decoder.h
    src/core/tokenizer.h

    src/audio/audio_capture.h
    src/audio/audio_decoder.h
    src/audio/audio_ring_buffer.h

    src/ui/main_window.h
    src/ui/stt_test_page.h
    src/ui/file_transcribe_page.h
    src/ui/settings_page.h
    src/ui/widgets/audio_waveform.h
    src/ui/widgets/text_output.h
    src/ui/widgets/progress_panel.h

    src/utils/logger.h
    src/utils/timer.h
    src/utils/string_utils.h
)

# ============================================================================
# 可执行文件
# ============================================================================
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

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
    ${ONNXRUNTIME_LIBRARIES}
    ${PORTAUDIO_LIBRARIES}
    pthread
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ============================================================================
# 资源文件
# ============================================================================
# 样式表
qt_add_resources(${PROJECT_NAME} "styles"
    PREFIX "/"
    FILES
        src/ui/resources/styles/main.qss
)

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
