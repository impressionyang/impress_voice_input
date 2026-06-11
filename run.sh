#!/bin/bash
# Impress Voice Input 启动脚本
# 设置 ONNX Runtime / PortAudio 库路径并启动应用

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
ONNXRUNTIME_LIB_DIR="${SCRIPT_DIR}/third_party/onnxruntime/lib"
PORTAUDIO_LIB_DIR="${SCRIPT_DIR}/third_party/portaudio/lib"

# 检查可执行文件
if [ ! -f "${BUILD_DIR}/impress_voice_input" ]; then
    echo "错误：未找到可执行文件，请先编译："
    echo "  mkdir -p build && cd build"
    echo "  cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo"
    echo "  cmake --build . -j\$(nproc)"
    exit 1
fi

# 检查 ONNX Runtime
if [ ! -f "${ONNXRUNTIME_LIB_DIR}/libonnxruntime.so" ]; then
    echo "警告：ONNX Runtime 未部署，推理功能将不可用"
    echo "  请按照 third_party/README.md 部署 ONNX Runtime"
fi

# 设置库路径（ONNX Runtime 优先，PortAudio 回退到系统）
export LD_LIBRARY_PATH="${ONNXRUNTIME_LIB_DIR}:${PORTAUDIO_LIB_DIR}:${LD_LIBRARY_PATH}"

# 启动应用
exec "${BUILD_DIR}/impress_voice_input" "$@"
