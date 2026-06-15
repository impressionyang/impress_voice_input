#!/bin/bash
# ============================================================================
# Impress Voice Input — Linux 发布包打包脚本
# 用法: ./package-linux.sh [--clean]
# ============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_linux"
DIST_DIR="${SCRIPT_DIR}/dist"
PKG_NAME="impress_voice_input_linux"
PKG_DIR="${DIST_DIR}/${PKG_NAME}"

for arg in "$@"; do
    case "$arg" in
        --clean)
            echo "清理发布包目录..."
            rm -rf "${PKG_DIR}" "${DIST_DIR}/${PKG_NAME}.tar.gz"
            ;;
    esac
done

# 1. 确保已编译
if [ ! -f "${BUILD_DIR}/impress_voice_input" ]; then
    echo "错误：未找到可执行文件，请先运行 ./build-linux.sh"
    exit 1
fi

# 2. 创建发布目录结构
echo "[1/6] 创建发布目录..."
rm -rf "${PKG_DIR}"
mkdir -p "${PKG_DIR}/lib"
mkdir -p "${PKG_DIR}/platforms"
mkdir -p "${PKG_DIR}/styles"
mkdir -p "${PKG_DIR}/gnome-extension/io.impress.voice-input-hotkey@impress"

# 3. 复制核心文件
echo "[2/6] 复制可执行文件和资源..."
cp "${BUILD_DIR}/impress_voice_input" "${PKG_DIR}/"
cp "${SCRIPT_DIR}/configs/default_config.json" "${PKG_DIR}/default_config.json"
cp "${SCRIPT_DIR}/src/ui/resources/styles/main.qss" "${PKG_DIR}/styles/" 2>/dev/null || true
cp "${SCRIPT_DIR}/src/ui/resources/styles/main_dark.qss" "${PKG_DIR}/styles/" 2>/dev/null || true

# 4. 复制依赖库
echo "[3/6] 复制依赖库..."
cp "${SCRIPT_DIR}/third_party/onnxruntime/lib/libonnxruntime.so"* "${PKG_DIR}/lib/"
cp "${SCRIPT_DIR}/third_party/portaudio/lib/libportaudio.so"* "${PKG_DIR}/lib/" 2>/dev/null || true

for lib in libQt6Core.so.6 libQt6Widgets.so.6 libQt6Gui.so.6 libQt6Concurrent.so.6 libQt6Network.so.6; do
    src=$(find /home/alvin/Qt/6.11.1/gcc_64/lib -name "${lib}" -o -name "${lib}.*" 2>/dev/null | head -1)
    [ -n "$src" ] && cp -L "$src" "${PKG_DIR}/lib/" || true
done

for lib in libicui18n.so.77 libicuuc.so.77 libicudata.so.77; do
    src=$(find /home/alvin/Qt/6.11.1/gcc_64/lib -name "${lib}" -o -name "${lib}.*" 2>/dev/null | head -1)
    [ -n "$src" ] && cp -L "$src" "${PKG_DIR}/lib/" || true
done

for lib in libpcre2-16.so.0 libfontconfig.so.1 libfreetype.so.6; do
    src=$(find /usr/lib64 /usr/lib -name "${lib}" -o -name "${lib}.*" 2>/dev/null | head -1)
    [ -n "$src" ] && cp -L "$src" "${PKG_DIR}/lib/" || true
done

for lib in libei.so.1 libeis.so.1; do
    src=$(find /usr/lib64 /usr/lib -name "${lib}" -o -name "${lib}.*" 2>/dev/null | head -1)
    [ -n "$src" ] && cp -L "$src" "${PKG_DIR}/lib/" || true
done

QT_PLUGIN_DIR=$(find /home/alvin/Qt -path "*/plugins/platforms" -type d 2>/dev/null | head -1)
if [ -n "$QT_PLUGIN_DIR" ]; then
    cp "${QT_PLUGIN_DIR}"/libq*.so "${PKG_DIR}/platforms/" 2>/dev/null || true
fi

# 5. 复制 GNOME 扩展
echo "[4/6] 复制 GNOME Shell 扩展..."
cp "${SCRIPT_DIR}/gnome-extension/io.impress.voice-input-hotkey@impress/extension.js" \
   "${PKG_DIR}/gnome-extension/io.impress.voice-input-hotkey@impress/"
cp "${SCRIPT_DIR}/gnome-extension/io.impress.voice-input-hotkey@impress/metadata.json" \
   "${PKG_DIR}/gnome-extension/io.impress.voice-input-hotkey@impress/"

# 6. 生成脚本和桌面文件
echo "[5/6] 生成安装脚本和启动脚本..."

# --- install.sh ---
cat > "${PKG_DIR}/install.sh" << 'INSTALLSH'
#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_NAME="impress_voice_input"
GNOME_EXT_UUID="io.impress.voice-input-hotkey@impress"
INSTALL_MODE="user"
for arg in "$@"; do case "$arg" in --system) INSTALL_MODE="system" ;; --user) INSTALL_MODE="user" ;; esac; done

echo "============================================"
echo "  Impress Voice Input — 安装"
echo "  模式: ${INSTALL_MODE}"
echo "============================================"

echo ""
echo "[1/3] GNOME Shell 扩展（GNOME 用户可选）..."
if [ -d "${SCRIPT_DIR}/gnome-extension/${GNOME_EXT_UUID}" ]; then
    if [ "$INSTALL_MODE" = "system" ]; then
        EXT_DST="/usr/share/gnome-shell/extensions/${GNOME_EXT_UUID}"
        echo "  系统安装: ${EXT_DST}"
        sudo mkdir -p "$(dirname "${EXT_DST}")"
        sudo rm -rf "${EXT_DST}"
        sudo cp -r "${SCRIPT_DIR}/gnome-extension/${GNOME_EXT_UUID}" "${EXT_DST}"
        sudo chmod -R o+r "${EXT_DST}"
    else
        EXT_DST="${HOME}/.local/share/gnome-shell/extensions/${GNOME_EXT_UUID}"
        echo "  用户安装: ${EXT_DST}"
        mkdir -p "$(dirname "${EXT_DST}")"
        rm -rf "${EXT_DST}"
        cp -r "${SCRIPT_DIR}/gnome-extension/${GNOME_EXT_UUID}" "${EXT_DST}"
    fi
    echo "  ✓ GNOME 扩展已安装"

    ENABLED=$(gsettings get org.gnome.shell enabled-extensions 2>/dev/null || echo "[]")
    if echo "${ENABLED}" | grep -q "${GNOME_EXT_UUID}"; then
        echo "  ✓ 扩展已在启用列表中"
    else
        echo "  添加到 enabled-extensions..."
        NEW_LIST=$(echo "${ENABLED}" | sed 's/\]$/,"'"${GNOME_EXT_UUID}"'"]/')
        gsettings set org.gnome.shell enabled-extensions "${NEW_LIST}" 2>/dev/null || {
            echo "  ⚠ 无法自动启用，请在「扩展」应用中手动启用"
        }
    fi
else
    echo "  ⚠ 未找到扩展目录，跳过"
fi

echo ""
echo "[2/3] Wayland evdev 权限（Wayland 用户可选）..."
SESSION_TYPE="${XDG_SESSION_TYPE:-}"
if [ "$SESSION_TYPE" = "wayland" ] && [ ! -w /dev/uinput ]; then
    echo "  当前为 Wayland 会话，evdev 快捷键需要 input 组权限"
    echo "  执行以下命令并重新登录："
    echo "    sudo usermod -aG input $USER"
    echo "  或创建 udev 规则："
    echo '    KERNEL=="uinput", MODE="0660", GROUP="input"'
    echo '    KERNEL=="event*", SUBSYSTEM=="input", MODE="0660", GROUP="input"'
else
    echo "  ✓ evdev 权限正常（X11 或已有 input 组访问）"
fi

echo ""
echo "[3/3] Desktop Entry..."
if [ -f "${SCRIPT_DIR}/${APP_NAME}.desktop" ]; then
    if [ "$INSTALL_MODE" = "system" ]; then
        DST="/usr/local/share/applications/${APP_NAME}.desktop"
        sudo mkdir -p "$(dirname "${DST}")"
        sudo cp "${SCRIPT_DIR}/${APP_NAME}.desktop" "${DST}"
        sudo sed -i "s|^Exec=.*|Exec=${SCRIPT_DIR}/run.sh|" "${DST}"
        echo "  ✓ 已安装到 ${DST}"
    else
        DST="${HOME}/.local/share/applications/${APP_NAME}.desktop"
        mkdir -p "$(dirname "${DST}")"
        cp "${SCRIPT_DIR}/${APP_NAME}.desktop" "${DST}"
        sed -i "s|^Exec=.*|Exec=${SCRIPT_DIR}/run.sh|" "${DST}"
        echo "  ✓ 已安装到 ${DST}"
    fi
else
    echo "  ⚠ 未找到 .desktop 文件"
fi

echo ""
echo "============================================"
echo "  安装完成"
echo "============================================"
echo ""
echo "启动: ${SCRIPT_DIR}/run.sh 或从应用菜单搜索"
echo ""
echo "快捷键方案（自动检测，默认 Ctrl+Alt+C 可自定义）："
echo "  1. X11: XGrabKey（零权限）"
echo "  2. Wayland: evdev（需 input 组）"
echo "  3. GNOME 扩展: D-Bus 信号（备用）"
if echo "${ENABLED:-}" | grep -q "${GNOME_EXT_UUID}" 2>/dev/null; then
    echo "  4. GNOME 扩展已启用"
fi
INSTALLSH
chmod +x "${PKG_DIR}/install.sh"

# --- run.sh ---
cat > "${PKG_DIR}/run.sh" << 'RUNSH'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="${SCRIPT_DIR}/platforms"
export XDG_DESKTOP_PORTAL_APP_ID="io.impress.voice-input"

if [ -d "${SCRIPT_DIR}/gnome-extension" ] && \
   [ ! -d "${HOME}/.local/share/gnome-shell/extensions/io.impress.voice-input-hotkey@impress" ]; then
    echo "GNOME 用户推荐安装 CapsLock 快捷键扩展（一次即可）："
    echo "  ./install.sh"
    echo ""
fi

exec "${SCRIPT_DIR}/impress_voice_input" "$@"
RUNSH
chmod +x "${PKG_DIR}/run.sh"

# desktop entry
cp "${DIST_DIR}/${PKG_NAME}/io.impress.voice-input.desktop" "${PKG_DIR}/" 2>/dev/null || true

# 7. 打包
echo "[6/6] 打包..."
cd "${DIST_DIR}"
tar czf "${PKG_NAME}.tar.gz" "${PKG_NAME}/"

echo ""
echo "发布包已生成: ${DIST_DIR}/${PKG_NAME}.tar.gz"
echo "大小: $(du -h "${DIST_DIR}/${PKG_NAME}.tar.gz" | cut -f1)"
echo ""
echo "使用方式："
echo "  tar xzf impress_voice_input_linux.tar.gz"
echo "  cd impress_voice_input_linux"
echo "  ./install.sh          # 安装 GNOME 扩展 + 桌面入口"
echo "  ./run.sh              # 启动应用"
