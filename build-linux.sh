#!/bin/bash
# ============================================================================
# Impress Voice Input — Linux 构建脚本
# 用法: ./build-linux.sh [--clean] [--release|--debug]
# ============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_linux"
BUILD_TYPE="RelWithDebInfo"

# 解析参数
for arg in "$@"; do
    case "$arg" in
        --clean)
            echo "清理构建目录..."
            rm -rf "${BUILD_DIR}"
            ;;
        --debug)
            BUILD_TYPE="Debug"
            ;;
        --release)
            BUILD_TYPE="Release"
            ;;
    esac
done

echo "============================================"
echo "  Impress Voice Input — Linux 构建"
echo "  构建类型: ${BUILD_TYPE}"
echo "============================================"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "[1/4] 强制更新编译时间戳..."
touch "${SCRIPT_DIR}/src/app/application.cpp"

echo "[2/4] 配置 CMake..."
cmake .. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

echo "[2/4] 配置 CMake..."
cmake .. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

echo "[3/4] 编译..."
cmake --build . -j$(nproc)

echo "[4/4] 构建完成"
echo ""
echo "可执行文件: ${BUILD_DIR}/impress_voice_input"
echo ""
echo "运行方式:"
echo "  cd ${SCRIPT_DIR} && ./run.sh"
echo "  或 LD_LIBRARY_PATH=third_party/onnxruntime/lib:third_party/portaudio/lib ${BUILD_DIR}/impress_voice_input"
