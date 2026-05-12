cmake_minimum_required(VERSION 3.20)

# 使用 Catch2 作为测试框架（通过 FetchContent 自动下载）
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# 测试需要的源文件
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/audio_processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/mel_spectrogram.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/whisper_tokenizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/vad.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/timer.cpp
)

# 测试可执行文件
add_executable(tests
    test_audio_processor.cpp
    test_vad.cpp
    test_mel_spectrogram.cpp
    test_whisper_tokenizer.cpp
    ${TEST_SOURCES}
)

target_include_directories(tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

target_link_libraries(tests PRIVATE
    Catch2::Catch2WithMain
    Qt6::Core
    pthread
)

target_compile_features(tests PRIVATE cxx_std_17)

# 注册测试
include(Catch)
catch_discover_tests(tests)
