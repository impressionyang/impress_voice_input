#include "audio_decoder.h"
#include "utils/logger.h"

#ifdef HAVE_DR_LIBS
#define DR_WAV_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include <dr_wav.h>
#include <dr_mp3.h>
#include <dr_flac.h>
#endif

#include <QFileInfo>

static const char* const kTag = "AudioDecoder";

namespace impress {

AudioDecoder::AudioDecoder(QObject* parent)
    : QObject(parent)
{}

AudioDecoder::~AudioDecoder() = default;

QStringList AudioDecoder::supportedFormats() {
    return {"wav", "mp3", "flac", "ogg", "aac"};
}

bool AudioDecoder::decode(const QString& filePath) {
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();

    samples_.clear();
    sampleRate_ = 0;
    channels_ = 0;

    if (ext != "wav") {
        LOG_ERROR(kTag, QString("暂不支持格式: %1").arg(ext));
        emit error(QString("暂不支持格式: %1").arg(ext));
        return false;
    }

#ifdef HAVE_DR_LIBS
    drwav wav;
    if (!drwav_init_file(&wav, filePath.toUtf8().constData(), nullptr)) {
        LOG_ERROR(kTag, QString("无法打开 WAV 文件: %1").arg(filePath));
        emit error("无法打开音频文件");
        return false;
    }

    channels_ = wav.channels;
    sampleRate_ = wav.sampleRate;

    std::vector<short> pcm16(wav.totalPCMFrameCount * wav.channels);
    drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, pcm16.data());
    drwav_uninit(&wav);

    // 多声道混合为单声道
    if (channels_ == 1) {
        samples_ = std::vector<float>(pcm16.begin(), pcm16.end());
        // 归一化
        for (auto& s : samples_) s /= 32768.0f;
    } else {
        samples_.resize(wav.totalPCMFrameCount);
        for (size_t i = 0; i < wav.totalPCMFrameCount; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels_; ++ch) {
                sum += pcm16[static_cast<size_t>(i) * static_cast<size_t>(channels_) + static_cast<size_t>(ch)];
            }
            samples_[i] = sum / (channels_ * 32768.0f);
        }
    }

    emit progress(1.0);
    emit decoded(filePath);
    LOG_INFO(kTag, QString("文件解码完成: %1 (%2 样本, %3Hz)")
        .arg(filePath).arg(samples_.size()).arg(sampleRate_));
    return true;
#else
    LOG_ERROR(kTag, "dr_libs 未编译启用");
    emit error("音频解码库未启用");
    return false;
#endif
}

double AudioDecoder::duration() const {
    if (sampleRate_ == 0) return 0.0;
    return static_cast<double>(samples_.size()) / sampleRate_;
}

} // namespace impress
