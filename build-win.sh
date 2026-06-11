#!/bin/bash
# ============================================================================
# Impress Voice Input — Windows 交叉编译 + 打包脚本
# 用法: ./build-win.sh [--clean] [--release|--debug]
#
# 输出:
#   build_win/dist_win/              — 解压后可直接运行的目录
#   dist/impress_voice_input_windows.zip — 发布压缩包
# ============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_win"
DIST_DIR="${BUILD_DIR}/dist_win"
OUTPUT_DIR="${SCRIPT_DIR}/dist"
BUILD_TYPE="RelWithDebInfo"

# Windows 交叉编译工具链和依赖路径
MINGW_PREFIX="/usr/x86_64-w64-mingw32/sys-root/mingw"
QT6_BIN="${MINGW_PREFIX}/bin"
QT6_PLUGINS="${MINGW_PREFIX}/lib/qt6/plugins"
WINE_WINDOWS="/usr/lib64/wine-wow64/wine/x86_64-windows"

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
echo "  Impress Voice Input — Windows 构建"
echo "  构建类型: ${BUILD_TYPE}"
echo "============================================"

# ============================================================================
# 0. 强制更新编译时间戳
# ============================================================================
touch "${SCRIPT_DIR}/src/app/application.cpp"

# ============================================================================
# 1. 编译
# ============================================================================
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "[1/5] 强制更新编译时间戳..."
touch "${SCRIPT_DIR}/src/app/application.cpp"

echo "[2/5] 配置 CMake (Windows 交叉编译)..."
cmake .. \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DQt6_DIR="${MINGW_PREFIX}/lib/cmake/Qt6"

echo "[3/5] 编译..."
cmake --build . -j$(nproc)

# ============================================================================
# 2. 收集依赖
# ============================================================================
echo "[4/5] 收集 Windows 依赖 DLL..."

# 清理旧的 dist_win 目录（保留 platforms 子目录结构）
rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}/platforms"

# 可执行文件（CMake 输出到 build_win/）
cp -f "${BUILD_DIR}/impress_voice_input.exe" "${DIST_DIR}/"

# ONNX Runtime
cp -f "${SCRIPT_DIR}/third_party/onnxruntime-win-x64/lib/onnxruntime.dll" "${DIST_DIR}/"
if [ -f "${SCRIPT_DIR}/third_party/onnxruntime-win-x64/lib/onnxruntime_providers_shared.dll" ]; then
    cp -f "${SCRIPT_DIR}/third_party/onnxruntime-win-x64/lib/onnxruntime_providers_shared.dll" "${DIST_DIR}/"
fi

# PortAudio
cp -f "${SCRIPT_DIR}/third_party/portaudio/bin/libportaudio.dll" "${DIST_DIR}/"

# Qt6 核心 DLL
for dll in Qt6Core Qt6Widgets Qt6Gui Qt6Concurrent Qt6Network; do
    src="${QT6_BIN}/${dll}.dll"
    if [ -f "$src" ]; then
        cp -f "$src" "${DIST_DIR}/"
    else
        echo "警告: 未找到 $src"
    fi
done

# Qt6 平台插件
cp -f "${QT6_PLUGINS}/platforms/qwindows.dll" "${DIST_DIR}/platforms/"

# ICU 国际化库（Qt6 依赖）
for dll in icuuc77 icui18n77 icudata77; do
    src="${QT6_BIN}/${dll}.dll"
    if [ -f "$src" ]; then
        cp -f "$src" "${DIST_DIR}/"
    fi
done

# MinGW 运行时
for dll in libgcc_s_seh-1 libstdc++-6 libwinpthread-1; do
    src="${QT6_BIN}/${dll}.dll"
    if [ -f "$src" ]; then
        cp -f "$src" "${DIST_DIR}/"
    fi
done

# Qt6 字体和图像依赖
for dll in zlib1 libpng16-16 libfreetype-6 libharfbuzz-0 \
           libfontconfig-1 libpcre2-16-0 libpcre2-8-0 \
           libcrypto-3-x64 libexpat-1 libbz2-1 \
           libglib-2.0-0 libintl-8 iconv; do
    src="${QT6_BIN}/${dll}.dll"
    if [ -f "$src" ]; then
        cp -f "$src" "${DIST_DIR}/"
    fi
done

# MSVC 运行时（MinGW 交叉编译的 Qt6 需要这些）
for dll in vcruntime140 vcruntime140_1 msvcp140 msvcp140_1 msvcp140_2 concrt140; do
    src="${WINE_WINDOWS}/${dll}.dll"
    if [ -f "$src" ]; then
        cp -f "$src" "${DIST_DIR}/"
    fi
done

echo "  已收集 $(ls "${DIST_DIR}" | wc -l) 个文件到 dist_win/"

# ============================================================================
# 3. 打包
# ============================================================================
echo "[5/5] 打包..."

mkdir -p "${OUTPUT_DIR}"
cd "${BUILD_DIR}"
rm -f "${OUTPUT_DIR}/impress_voice_input_windows.zip"

# 打包 dist_win/ 目录（包含目录前缀）
zip -r "${OUTPUT_DIR}/impress_voice_input_windows.zip" dist_win/ > /dev/null 2>&1

echo ""
echo "============================================"
echo "  构建完成！"
echo "============================================"
echo ""
echo "解压后可直接运行目录: ${DIST_DIR}/"
echo "  运行: ${DIST_DIR}/impress_voice_input.exe"
echo ""
echo "发布压缩包: ${OUTPUT_DIR}/impress_voice_input_windows.zip"
echo "  解压后进入 dist_win/ 目录运行 impress_voice_input.exe"
echo ""

# 列出 dist_win 内容
echo "dist_win/ 内容:"
ls -lh "${DIST_DIR}/" | tail -n +2
echo ""
echo "dist_win/platforms/ 内容:"
ls -lh "${DIST_DIR}/platforms/"
