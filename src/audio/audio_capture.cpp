#include "audio_capture.h"
#include "utils/logger.h"

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif

static const char* const kTag = "AudioCapture";

namespace impress {

// 预分配缓冲区，避免在实时回调中分配内存
static constexpr int kMaxBufferSize = 8192;

// 回调上下文：独立于 Impl 的 POD 结构，供静态回调使用
struct CallbackContext {
    AudioCapture* owner = nullptr;
#ifdef HAVE_PORTAUDIO
    PaStream* stream = nullptr;
    float buffer[kMaxBufferSize];
#endif
    int sampleRate = 16000;
};

struct AudioCapture::Impl {
    CallbackContext ctx;
};

static int paCallback(const void* input, void* /*output*/,
                      unsigned long frameCount,
                      const PaStreamCallbackTimeInfo* /*timeInfo*/,
                      PaStreamCallbackFlags /*statusFlags*/,
                      void* userData)
{
#ifdef HAVE_PORTAUDIO
    auto* ctx = static_cast<CallbackContext*>(userData);

    const float* samples = static_cast<const float*>(input);

    // 使用预分配缓冲区，避免实时线程中分配内存
    unsigned long count = frameCount;
    if (count > kMaxBufferSize) count = kMaxBufferSize;

    // 拷贝到预分配缓冲区
    for (unsigned long i = 0; i < count; i++) {
        ctx->buffer[i] = samples[i];
    }

    // 发射信号（Qt 使用 QueuedConnection，线程安全）
    std::vector<float> data(ctx->buffer, ctx->buffer + count);
    emit ctx->owner->audioDataReady(data, ctx->sampleRate);

    return paContinue;
#else
    (void)input; (void)frameCount; (void)userData;
    return 0;
#endif
}

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
    devices << "默认设备";
#ifdef HAVE_PORTAUDIO
    Pa_Terminate(); // 确保未初始化
    if (Pa_Initialize() == paNoError) {
        int count = Pa_GetDeviceCount();
        for (int i = 0; i < count; ++i) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (info && info->maxInputChannels > 0) {
                devices << QString("%1 (CH:%2, SR:%3)").arg(
                    info->name).arg(info->maxInputChannels).arg(info->defaultSampleRate);
            }
        }
        Pa_Terminate();
    }
#endif
    return devices;
}

bool AudioCapture::start(int deviceIndex, int sampleRate, int bufferSizeMs) {
    if (running_) {
        LOG_WARNING(kTag, "已在运行中");
        return false;
    }

#ifdef HAVE_PORTAUDIO
    if (Pa_Initialize() != paNoError) {
        LOG_ERROR(kTag, "PortAudio 初始化失败");
        return false;
    }

    int devIdx = deviceIndex < 0 ? Pa_GetDefaultInputDevice() : deviceIndex;
    if (devIdx < 0 || devIdx >= Pa_GetDeviceCount()) {
        LOG_ERROR(kTag, QString("无效的音频设备索引: %1").arg(deviceIndex));
        Pa_Terminate();
        return false;
    }

    const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(devIdx);
    if (!devInfo || devInfo->maxInputChannels <= 0) {
        LOG_ERROR(kTag, "所选设备不是输入设备");
        Pa_Terminate();
        return false;
    }

    PaStreamParameters inputParams{};
    inputParams.device = devIdx;
    inputParams.channelCount = 1;
    inputParams.sampleFormat = paFloat32 | paNonInterleaved;
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
        Pa_Terminate();
        return false;
    }

    err = Pa_StartStream(impl_->ctx.stream);
    if (err != paNoError) {
        LOG_ERROR(kTag, QString("启动音频流失败: %1").arg(Pa_GetErrorText(err)));
        Pa_CloseStream(impl_->ctx.stream);
        impl_->ctx.stream = nullptr;
        Pa_Terminate();
        return false;
    }

    impl_->ctx.sampleRate = sampleRate;
    running_ = true;
    emit runningChanged(true);
    LOG_INFO(kTag, QString("音频采集已启动 (设备: %1, 采样率: %2, 缓冲区: %3ms)")
        .arg(deviceIndex).arg(sampleRate).arg(bufferSizeMs));
    return true;
#else
    LOG_ERROR(kTag, "PortAudio 未编译启用");
    emit error("PortAudio 未编译启用");
    return false;
#endif
}

void AudioCapture::stop() {
    if (!running_) return;

#ifdef HAVE_PORTAUDIO
    if (impl_->ctx.stream) {
        Pa_StopStream(impl_->ctx.stream);
        Pa_CloseStream(impl_->ctx.stream);
        impl_->ctx.stream = nullptr;
    }
    Pa_Terminate();
#endif

    running_ = false;
    emit runningChanged(false);
    LOG_INFO(kTag, "音频采集已停止");
}

} // namespace impress
