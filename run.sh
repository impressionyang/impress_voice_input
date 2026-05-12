#!/bin/bash
# Impress Voice Input 启动脚本

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# 设置库路径
export LD_LIBRARY_PATH="${SCRIPT_DIR}/third_party/onnxruntime/lib:${SCRIPT_DIR}/third_party/portaudio/lib:${LD_LIBRARY_PATH}"

# 运行
exec "${BUILD_DIR}/impress_voice_input" "$@"
