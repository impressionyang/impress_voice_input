# Impress Voice Input

基于 ONNX 的实时语音转文本输入法，C++ 跨平台实现。

## 功能特性

- **实时语音识别** — 麦克风采集，流式输出识别结果
- **音频文件转写** — 支持 WAV/MP3/FLAC，批量处理，导出 TXT/SRT
- **跨平台 GUI** — Qt 6 构建，支持 Windows / macOS / Linux
- **本地推理** — ONNX Runtime，支持 CPU/GPU 加速
- **可配置** — 模型路径、推理参数、快捷键均可自定义

## 项目结构

```
impress_voice_input/
├── CMakeLists.txt              # 构建配置
├── PRD.md                      # 产品需求文档
├── cmake/                      # CMake 模块
│   └── dependencies.cmake      # 依赖查找
├── src/
│   ├── main.cpp                # 入口
│   ├── app/                    # 应用层 (Application, ConfigManager)
│   ├── core/                   # 核心 (STTEngine, AudioProcessor, Decoder, Tokenizer)
│   ├── audio/                  # 音频 (AudioCapture, AudioDecoder, RingBuffer)
│   ├── ui/                     # GUI 页面与控件
│   │   ├── main_window.cpp     # 主窗口
│   │   ├── stt_test_page.cpp   # 实时识别页
│   │   ├── file_transcribe_page.cpp  # 文件转写页
│   │   ├── settings_page.cpp   # 配置页
│   │   └── widgets/            # 自定义控件
│   └── utils/                  # 工具类
├── configs/                    # 配置文件
├── models/                     # ONNX 模型存放目录
└── third_party/                # 第三方依赖
```

## 技术栈

| 组件 | 选型 |
|------|------|
| GUI | Qt 6 |
| 推理引擎 | ONNX Runtime (C++ API) |
| 音频采集 | PortAudio |
| 音频解码 | dr_libs (dr_wav, dr_mp3, dr_flac) |
| 构建系统 | CMake 3.20+ |
| 配置存储 | nlohmann/json |

## 编译指南

### 前置依赖

1. **CMake** >= 3.20
2. **Qt 6** >= 6.5
3. **ONNX Runtime** C++ 库
4. **PortAudio**
5. C++17 兼容编译器 (GCC 9+ / Clang 10+ / MSVC 2019+)

### 第三方库准备

```bash
# 放入 third_party/ 目录
third_party/
├── onnxruntime/
│   ├── include/
│   └── lib/
├── portaudio/
│   ├── include/
│   └── lib/
├── dr_libs/
│   ├── dr_wav.h
│   ├── dr_mp3.h
│   └── dr_flac.h
└── nlohmann_json/
    └── json.hpp
```

### 编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### 带测试编译

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build . -j$(nproc)
ctest
```

## 运行

```bash
# 默认启动
./impress_voice_input

# 指定配置文件
./impress_voice_input --config /path/to/config.json

# 指定模型
./impress_voice_input --model /path/to/model.onnx
```

## 模型准备

支持以下 ONNX 格式模型：
- **Whisper** (small/medium) — 多语言，高质量
- **Paraformer** — 中文优化
- 其他兼容 CTC 的 ASR 模型

将 `.onnx` 模型文件放入 `models/` 目录，然后在配置页面设置路径。

## 当前状态

项目处于 **核心推理实现阶段**：

- [x] 项目结构与 CMake 配置
- [x] 配置管理模块 (线程安全，自动持久化)
- [x] STT 推理引擎 (ONNX Runtime 集成，异步模型加载)
- [x] Mel 频谱图提取 (Hann 窗 + FFT + Mel 滤波器组)
- [x] Whisper Tokenizer (BPE 分词)
- [x] 音频采集/解码框架 (PortAudio/dr_libs)
- [x] 三个 GUI 页面 (实时识别 / 文件转写 / 配置)
- [x] 日志系统 (控制台 + 文件输出)
- [x] 批量文件转写 (支持 WAV/MP3/FLAC)
- [x] 结果导出 (TXT / SRT 字幕 / JSON 结构化数据)
- [x] 音频重采样 (非 16kHz 音频自动重采样)
- [x] 语音活动检测 (VAD — 短时能量 + 过零率)
- [x] 音频文件信息 (时长/采样率/声道数)
- [x] 单元测试框架 (Catch2, 39 个测试用例)
- [ ] 完整 Whisper 推理 (自回归解码 + 流式识别)
- [ ] 跨平台打包

## License

MIT
