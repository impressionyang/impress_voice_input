#include "audio_decoder.h"
#include "utils/logger.h"

#include <cstdint>

#ifdef HAVE_DR_LIBS
#define DR_WAV_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include "dr_wav.h"
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
    return {"wav", "mp3", "flac"};
}

bool AudioDecoder::decode(const QString& filePath) {
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();

    samples_.clear();
    sampleRate_ = 0;
    channels_ = 0;

#ifdef HAVE_DR_LIBS
    bool success = false;

    if (ext == "wav") {
        success = decodeWav(filePath);
    } else if (ext == "mp3") {
        success = decodeMp3(filePath);
    } else if (ext == "flac") {
        success = decodeFlac(filePath);
    } else {
        LOG_ERROR(kTag, QString("暂不支持格式: %1").arg(ext));
        emit error(QString("暂不支持格式: %1").arg(ext));
        return false;
    }

    if (success) {
        emit progress(1.0);
        emit decoded(filePath);
        LOG_INFO(kTag, QString("文件解码完成: %1 (%2 样本, %3Hz, %4声道)")
            .arg(filePath).arg(samples_.size()).arg(sampleRate_).arg(channels_));
    }
    return success;
#else
    LOG_ERROR(kTag, "dr_libs 未编译启用");
    emit error("音频解码库未启用");
    return false;
#endif
}

#ifdef HAVE_DR_LIBS
bool AudioDecoder::decodeWav(const QString& filePath) {
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

    convertToMono(pcm16, wav.totalPCMFrameCount);
    return true;
}

bool AudioDecoder::decodeMp3(const QString& filePath) {
    drmp3 mp3;
    if (!drmp3_init_file(&mp3, filePath.toUtf8().constData(), nullptr)) {
        LOG_ERROR(kTag, QString("无法打开 MP3 文件: %1").arg(filePath));
        emit error("无法打开音频文件");
        return false;
    }

    channels_ = mp3.channels;
    sampleRate_ = mp3.sampleRate;

    drmp3_uint64 totalFrames = drmp3_get_pcm_frame_count(&mp3);
    std::vector<float> rawPcm(totalFrames * channels_);
    drmp3_read_pcm_frames_f32(&mp3, totalFrames, rawPcm.data());
    drmp3_uninit(&mp3);

    // MP3 解码器直接输出归一化浮点数据
    if (channels_ == 1) {
        samples_ = std::move(rawPcm);
    } else {
        samples_.resize(totalFrames);
        for (drmp3_uint64 i = 0; i < totalFrames; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels_; ++ch) {
                sum += rawPcm[i * channels_ + ch];
            }
            samples_[i] = sum / channels_;
        }
    }
    return true;
}

bool AudioDecoder::decodeFlac(const QString& filePath) {
    drflac* flac = drflac_open_file(filePath.toUtf8().constData(), nullptr);
    if (!flac) {
        LOG_ERROR(kTag, QString("无法打开 FLAC 文件: %1").arg(filePath));
        emit error("无法打开音频文件");
        return false;
    }

    channels_ = flac->channels;
    sampleRate_ = flac->sampleRate;

    drflac_uint64 totalFrames = flac->totalPCMFrameCount;
    std::vector<short> pcm16(totalFrames * channels_);
    drflac_read_pcm_frames_s16(flac, totalFrames, pcm16.data());
    drflac_close(flac);

    convertToMono(pcm16, totalFrames);
    return true;
}

void AudioDecoder::convertToMono(const std::vector<short>& pcm16, uint64_t frameCount) {
    if (channels_ == 1) {
        samples_.resize(frameCount);
        for (uint64_t i = 0; i < frameCount; ++i) {
            samples_[i] = pcm16[i] / 32768.0f;
        }
    } else {
        samples_.resize(frameCount);
        for (uint64_t i = 0; i < frameCount; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels_; ++ch) {
                sum += pcm16[i * channels_ + ch];
            }
            samples_[i] = sum / (channels_ * 32768.0f);
        }
    }
}
#endif

double AudioDecoder::duration() const {
    if (sampleRate_ == 0) return 0.0;
    return static_cast<double>(samples_.size()) / sampleRate_;
}

} // namespace impress
