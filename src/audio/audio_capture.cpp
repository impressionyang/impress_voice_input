#include "audio_capture.h"
#include "utils/logger.h"

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#include <cmath>
#endif

static const char* const kTag = "AudioCapture";

namespace impress {

// 预分配缓冲区，避免在实时回调中分配内存
static constexpr int kMaxBufferSize = 8192;

#ifdef HAVE_PORTAUDIO
// 全局 PortAudio 初始化状态
static bool gPaInitialized = false;

/** 安全初始化 PortAudio（多次调用不报错） */
static bool ensurePaInitialized() {
    if (gPaInitialized) return true;
    if (Pa_Initialize() == paNoError) {
        gPaInitialized = true;
        return true;
    }
    return false;
}

/** 安全终止 PortAudio */
static void safePaTerminate() {
    if (gPaInitialized) {
        Pa_Terminate();
        gPaInitialized = false;
    }
}
#endif

// 回调上下文：独立于 Impl 的 POD 结构，供静态回调使用
struct CallbackContext {
    AudioCapture* owner = nullptr;
#ifdef HAVE_PORTAUDIO
    PaStream* stream = nullptr;
    float buffer[kMaxBufferSize];
#endif
    int sampleRate = 16000;
    // 音频电平诊断
    float peakLevel = 0.0f;
    double sumSquares = 0.0;
    int sampleCount = 0;
};

struct AudioCapture::Impl {
    CallbackContext ctx;
};

#ifdef HAVE_PORTAUDIO
static int paCallback(const void* input, void* /*output*/,
                      unsigned long frameCount,
                      const PaStreamCallbackTimeInfo* /*timeInfo*/,
                      PaStreamCallbackFlags /*statusFlags*/,
                      void* userData)
{
    auto* ctx = static_cast<CallbackContext*>(userData);

    const float* samples = static_cast<const float*>(input);

    // 使用预分配缓冲区，避免实时线程中分配内存
    unsigned long count = frameCount;
    if (count > kMaxBufferSize) count = kMaxBufferSize;

    // 拷贝到预分配缓冲区 + 计算音频电平
    float peak = 0.0f;
    double ss = 0.0;
    for (unsigned long i = 0; i < count; i++) {
        float s = samples[i];
        ctx->buffer[i] = s;
        float absS = std::fabs(s);
        if (absS > peak) peak = absS;
        ss += s * s;
    }

    // 更新诊断数据
    if (peak > ctx->peakLevel) ctx->peakLevel = peak;
    ctx->sumSquares += ss;
    ctx->sampleCount += count;

    // 发射信号（Qt 使用 QueuedConnection，线程安全）
    std::vector<float> data(ctx->buffer, ctx->buffer + count);
    emit ctx->owner->audioDataReady(data, ctx->sampleRate);

    return paContinue;
}
#else
// 占位回调（无 PortAudio 时不使用）
static int paCallbackStub(const void*, void*, unsigned long,
                          const int*, int, void*)
{
    return 0;
}
#endif

AudioCapture::AudioCapture(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{
    impl_->ctx.owner = this;
}

AudioCapture::~AudioCapture() {
    stop();
}

QStringList AudioCapture::getDeviceList() {
    QStringList devices;
#ifdef HAVE_PORTAUDIO
    devices << "默认设备";
    if (!ensurePaInitialized()) {
        LOG_ERROR(kTag, "PortAudio 初始化失败");
        return devices;
    }
    int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info && info->maxInputChannels > 0) {
            const PaHostApiInfo* hostApi = Pa_GetHostApiInfo(info->hostApi);
            QString hostApiName = hostApi ? hostApi->name : "未知";
            devices << QString("[%1] %2 (CH:%3, SR:%4, %5)")
                .arg(i).arg(info->name).arg(info->maxInputChannels)
                .arg(info->defaultSampleRate).arg(hostApiName);
        }
    }
#else
    devices << "PortAudio 未启用（占位设备）";
    LOG_WARNING(kTag, "PortAudio 未编译启用，设备列表为占位");
#endif
    return devices;
}

int AudioCapture::getDefaultDeviceIndex() {
#ifdef HAVE_PORTAUDIO
    if (!ensurePaInitialized()) return -1;
    return Pa_GetDefaultInputDevice();
#else
    return -1;
#endif
}

bool AudioCapture::start(int deviceIndex, int sampleRate, int bufferSizeMs) {
    if (running_) {
        LOG_WARNING(kTag, "已在运行中");
        return false;
    }

#ifdef HAVE_PORTAUDIO
    if (!ensurePaInitialized()) {
        LOG_ERROR(kTag, "PortAudio 初始化失败");
        return false;
    }

    // 枚举所有 Host API 用于诊断
    LOG_DEBUG(kTag, QString("Host API 数量: %1").arg(Pa_GetHostApiCount()));
    for (int i = 0; i < Pa_GetHostApiCount(); i++) {
        const PaHostApiInfo* api = Pa_GetHostApiInfo(i);
        if (api) {
            LOG_DEBUG(kTag, QString("  Host API #%1: %2 (设备数: %3)")
                .arg(i).arg(api->name).arg(api->deviceCount));
        }
    }

    // 选择设备
    int devIdx = deviceIndex < 0 ? Pa_GetDefaultInputDevice() : deviceIndex;
    if (devIdx < 0 || devIdx >= Pa_GetDeviceCount()) {
        LOG_ERROR(kTag, QString("无效的音频设备索引: %1 (默认设备: %2)")
            .arg(deviceIndex).arg(Pa_GetDefaultInputDevice()));
        return false;
    }

    const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(devIdx);
    if (!devInfo || devInfo->maxInputChannels <= 0) {
        LOG_ERROR(kTag, "所选设备不是输入设备");
        return false;
    }

    const PaHostApiInfo* hostApi = Pa_GetHostApiInfo(devInfo->hostApi);
    LOG_INFO(kTag, QString("=== 音频设备诊断 ==="));
    LOG_INFO(kTag, QString("  设备 #%1: %2").arg(devIdx).arg(devInfo->name));
    LOG_INFO(kTag, QString("  Host API: %1").arg(hostApi ? hostApi->name : "未知"));
    LOG_INFO(kTag, QString("  最大输入通道: %1").arg(devInfo->maxInputChannels));
    LOG_INFO(kTag, QString("  设备默认采样率: %1 Hz").arg(devInfo->defaultSampleRate, 0, 'f', 0));
    LOG_INFO(kTag, QString("  请求采样率: %1 Hz").arg(sampleRate));
    LOG_INFO(kTag, QString("  采样格式: paFloat32 | paNonInterleaved"));
    LOG_INFO(kTag, QString("  请求通道数: 1 (mono)"));
    LOG_INFO(kTag, QString("  缓冲区: %1ms (%2 帧)").arg(bufferSizeMs)
        .arg(sampleRate * bufferSizeMs / 1000));

    // 检查是否可能选错设备（名称包含 monitor 的通常是回环设备）
    QString devName = QString(devInfo->name).toLower();
    if (devName.contains("monitor") || devName.contains("output")) {
        LOG_WARNING(kTag, "⚠️ 当前设备名称包含 'monitor' 或 'output'，"
            "这可能是扬声器回环设备而非麦克风！如果录制的是噪音，请在设置中选择正确的麦克风设备。");
    }

    PaStreamParameters inputParams{};
    inputParams.device = devIdx;
    inputParams.channelCount = 1;
    inputParams.sampleFormat = paFloat32;
    // 不使用 paNonInterleaved：input 指针直接是 float* 数组（interleaved mono），
    // 回调中可以安全地 static_cast<const float*>(input)
    // 使用高延迟以避免回调过快
    inputParams.suggestedLatency = devInfo->defaultHighInputLatency;

    int framesPerBuffer = sampleRate * bufferSizeMs / 1000;
    if (framesPerBuffer < 256) framesPerBuffer = 256;

    PaError err = Pa_OpenStream(
        &impl_->ctx.stream, &inputParams, nullptr, sampleRate,
        static_cast<unsigned long>(framesPerBuffer),
        paClipOff, paCallback, &impl_->ctx);

    if (err != paNoError || !impl_->ctx.stream) {
        LOG_ERROR(kTag, QString("打开音频流失败: %1").arg(Pa_GetErrorText(err)));
        return false;
    }

    err = Pa_StartStream(impl_->ctx.stream);
    if (err != paNoError) {
        LOG_ERROR(kTag, QString("启动音频流失败: %1").arg(Pa_GetErrorText(err)));
        Pa_CloseStream(impl_->ctx.stream);
        impl_->ctx.stream = nullptr;
        return false;
    }

    // 重置诊断计数器
    impl_->ctx.peakLevel = 0.0f;
    impl_->ctx.sumSquares = 0.0;
    impl_->ctx.sampleCount = 0;

    impl_->ctx.sampleRate = sampleRate;
    running_ = true;
    emit runningChanged(true);
    LOG_INFO(kTag, QString("音频采集已启动 (设备: %1, 采样率: %2, 缓冲区: %3ms)")
        .arg(deviceIndex).arg(sampleRate).arg(bufferSizeMs));
    return true;
#else
    (void)deviceIndex; (void)sampleRate; (void)bufferSizeMs;
    LOG_ERROR(kTag, "PortAudio 未编译启用，无法启动采集");
    emit error("PortAudio 未编译启用，请在 third_party/portaudio/ 中部署后重新编译");
    return false;
#endif
}

void AudioCapture::stop() {
    if (!running_) return;

#ifdef HAVE_PORTAUDIO
    // 输出音频电平诊断信息
    if (impl_->ctx.sampleCount > 0) {
        double rms = std::sqrt(impl_->ctx.sumSquares / impl_->ctx.sampleCount);
        LOG_INFO(kTag, QString("=== 音频电平诊断 ==="));
        LOG_INFO(kTag, QString("  总样本数: %1").arg(impl_->ctx.sampleCount));
        LOG_INFO(kTag, QString("  RMS 电平: %1").arg(rms, 0, 'f', 6));
        LOG_INFO(kTag, QString("  峰值: %1").arg(impl_->ctx.peakLevel, 0, 'f', 4));
        if (rms < 0.001) {
            LOG_WARNING(kTag, "⚠️ 音频信号过弱，可能是静音或设备未正确采集");
        } else if (rms > 0.5) {
            LOG_WARNING(kTag, "⚠️ 音频信号过强，可能存在削波");
        } else if (impl_->ctx.peakLevel > 0.9f) {
            LOG_INFO(kTag, "  信号幅度正常");
        } else {
            LOG_WARNING(kTag, "⚠️ 信号幅度偏低，请检查设备选择");
        }
    }

    if (impl_->ctx.stream) {
        Pa_StopStream(impl_->ctx.stream);
        Pa_CloseStream(impl_->ctx.stream);
        impl_->ctx.stream = nullptr;
    }
    safePaTerminate();
#endif

    running_ = false;
    emit runningChanged(false);
    LOG_INFO(kTag, "音频采集已停止");
}

} // namespace impress
