# 第三方依赖部署指南

本目录存放项目依赖库。部分库已由版本控制管理（header-only 库），部分需手动下载或编译。

---

## 目录结构

```
third_party/
├── onnxruntime/      # ONNX Runtime (需手动下载)
├── portaudio/        # PortAudio (需手动编译)
├── dr_libs/          # 音频解码 (已纳入版本控制)
│   ├── dr_wav.h
│   ├── dr_mp3.h
│   └── dr_flac.h
└── nlohmann_json/    # JSON 解析 (已纳入版本控制)
    └── json.hpp
```

---

## 1. ONNX Runtime

### 下载

前往 GitHub Releases 页面下载 Linux x64 CPU 版本：
https://github.com/microsoft/onnxruntime/releases

以 v1.18.1 为例：
```bash
# 下载预编译的 Linux x64 CPU 包
wget https://github.com/microsoft/onnxruntime/releases/download/v1.18.1/onnxruntime-linux-x64-1.18.1.tgz

# 解压并重命名
tar -xzf onnxruntime-linux-x64-1.18.1.tgz
mv onnxruntime-linux-x64-1.18.1 third_party/onnxruntime
```

### 验证

确认以下文件存在：
```
third_party/onnxruntime/
├── include/
│   ├── onnxruntime_c_api.h
│   └── onnxruntime_cxx_api.h
└── lib/
    └── libonnxruntime.so
```

### 其他平台

| 平台 | 下载地址 |
|------|----------|
| macOS (ARM64) | `onnxruntime-osx-arm64-<version>.tgz` |
| macOS (x64) | `onnxruntime-osx-x64-<version>.tgz` |
| Windows (x64) | `onnxruntime-win-x64-<version>.zip` |

---

## 2. PortAudio

### 编译安装

```bash
cd third_party

# 克隆源码
git clone https://github.com/PortAudio/portaudio.git
cd portaudio

# 创建构建目录
mkdir -p build && cd build

# CMake 配置并编译
cmake .. -DCMAKE_INSTALL_PREFIX=.. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
cmake --install .
```

### 验证

确认以下文件存在：
```
third_party/portaudio/
├── include/
│   └── portaudio.h
└── lib/
    ├── libportaudio.so
    └── libportaudio.a
```

### 替代方案（系统包）

如果使用 Linux 包管理器，可直接安装：
```bash
# Debian/Ubuntu
sudo apt install libportaudio2 libportaudiocpp0 portaudio19-dev

# Fedora/RHEL
sudo dnf install portaudio-devel
```

安装后 CMake 会自动从系统路径找到库文件。

---

## 3. dr_libs (已完成)

单头文件音频解码库，已纳入版本控制，无需手动操作。

源码地址：https://github.com/mackron/dr_libs

---

## 4. nlohmann/json (已完成)

单头文件 JSON 库，已纳入版本控制，无需手动操作。

源码地址：https://github.com/nlohmann/json

---

## 快速部署脚本 (Linux)

```bash
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# 1. ONNX Runtime v1.18.1 Linux x64 CPU
echo "=== 下载 ONNX Runtime ==="
if [ ! -d "onnxruntime" ]; then
    ONNX_VERSION="1.18.1"
    wget -q "https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/onnxruntime-linux-x64-${ONNX_VERSION}.tgz"
    tar -xzf "onnxruntime-linux-x64-${ONNX_VERSION}.tgz"
    mv "onnxruntime-linux-x64-${ONNX_VERSION}" onnxruntime
    rm "onnxruntime-linux-x64-${ONNX_VERSION}.tgz"
    echo "ONNX Runtime 已部署"
else
    echo "ONNX Runtime 已存在，跳过"
fi

# 2. PortAudio
echo "=== 编译 PortAudio ==="
if [ ! -d "portaudio/lib" ]; then
    if [ ! -d "portaudio" ]; then
        git clone https://github.com/PortAudio/portaudio.git
    fi
    cd portaudio
    mkdir -p build && cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=.. -DCMAKE_BUILD_TYPE=Release
    cmake --build . -j$(nproc)
    cmake --install .
    cd ../..
    echo "PortAudio 已部署"
else
    echo "PortAudio 已存在，跳过"
fi

echo "=== 依赖部署完成 ==="
```
