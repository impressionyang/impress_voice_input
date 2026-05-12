#include "audio_capture.h"
#include "utils/logger.h"

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif

static const char* const kTag = "AudioCapture";

namespace impress {

struct AudioCapture::Impl {
#ifdef HAVE_PORTAUDIO
    PaStream* stream = nullptr;
#endif
    AudioCapture* owner = nullptr;
    int sampleRate = 16000;
};

static int paCallback(const void* input, void* /*output*/,
                      unsigned long frameCount,
                      const PaStreamCallbackTimeInfo* /*timeInfo*/,
                      PaStreamCallbackFlags /*statusFlags*/,
                      void* userData)
{
#ifdef HAVE_PORTAUDIO
    auto* capture = static_cast<AudioCapture*>(userData);
    const float* samples = static_cast<const float*>(input);
    std::vector<float> data(samples, samples + frameCount);
    emit capture->audioDataReady(data, 16000);
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
    impl_->owner = this;
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

    PaStreamParameters inputParams{};
    inputParams.device = deviceIndex < 0 ? Pa_GetDefaultInputDevice() : deviceIndex;
    inputParams.channelCount = 1;
    inputParams.sampleFormat = paFloat32 | paNonInterleaved;
    inputParams.suggestedLatency =
        Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;

    PaError err = Pa_OpenStream(
        &impl_->stream, &inputParams, nullptr, sampleRate,
        static_cast<unsigned long>(sampleRate * bufferSizeMs / 1000),
        paClipOff, paCallback, this);

    if (err != paNoError || !impl_->stream) {
        LOG_ERROR(kTag, QString("打开音频流失败: %1").arg(Pa_GetErrorText(err)));
        Pa_Terminate();
        return false;
    }

    err = Pa_StartStream(impl_->stream);
    if (err != paNoError) {
        LOG_ERROR(kTag, QString("启动音频流失败: %1").arg(Pa_GetErrorText(err)));
        Pa_CloseStream(impl_->stream);
        impl_->stream = nullptr;
        Pa_Terminate();
        return false;
    }

    impl_->sampleRate = sampleRate;
    running_ = true;
    emit runningChanged(true);
    LOG_INFO(kTag, QString("音频采集已启动 (设备: %1, 采样率: %2)").arg(deviceIndex).arg(sampleRate));
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
    if (impl_->stream) {
        Pa_StopStream(impl_->stream);
        Pa_CloseStream(impl_->stream);
        impl_->stream = nullptr;
    }
    Pa_Terminate();
#endif

    running_ = false;
    emit runningChanged(false);
    LOG_INFO(kTag, "音频采集已停止");
}

} // namespace impress
