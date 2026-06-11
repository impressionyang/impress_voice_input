#include "streaming_audio_writer.h"
#include "core/vad.h"
#include "utils/logger.h"

#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <cmath>
#include <algorithm>
#include <cstring>

static const char* const kTag = "StreamingAudioWriter";

namespace impress {

StreamingAudioWriter::StreamingAudioWriter(QObject* parent)
    : QObject(parent)
{
}

StreamingAudioWriter::~StreamingAudioWriter() {
    stop();
}

QString StreamingAudioWriter::getAudioStorageDir(bool debugEnabled, const QString& debugDir) {
    if (debugEnabled && !debugDir.isEmpty()) {
        return debugDir;
    }
    if (debugEnabled) {
        // 使用配置默认值：临时目录
        return QDir::tempPath() + "/impress_audio_debug";
    }

#ifdef PLATFORM_WINDOWS
    return ".";
#else
    return QDir::tempPath();
#endif
}

bool StreamingAudioWriter::start(int sampleRate, bool debugEnabled, const QString& debugDir) {
    QMutexLocker locker(&mutex_);

    if (recording_) {
        LOG_WARNING(kTag, "已在录制中");
        return false;
    }

    sampleRate_ = sampleRate;
    debugEnabled_ = debugEnabled;
    debugDir_ = debugDir;
    totalSamples_ = 0;
    samplesWritten_ = 0;
    wasSpeaking_ = false;
    silenceFramesAfterSpeech_ = 0;

    // 初始化 VAD（30ms 帧，降低能量阈值以适配低增益麦克风）
    vad_ = std::make_unique<VoiceActivityDetector>(sampleRate_, 30, 0.003f, 3);

    // VAD 帧大小
    vadFrameSize_ = sampleRate_ * 30 / 1000;
    if (vadFrameSize_ < 320) vadFrameSize_ = 320;

    // 静音切换：~1s 的连续静音帧
    silenceFramesNeeded_ = 1000 / 30;  // ~33 帧

    // 确保目录存在
    QString dir = getAudioStorageDir(debugEnabled, debugDir);
    QDir d;
    if (!d.exists(dir)) {
        if (!d.mkpath(dir)) {
            LOG_ERROR(kTag, QString("无法创建音频存储目录: %1").arg(dir));
            return false;
        }
    }

    if (!openNewFile()) {
        return false;
    }

    recording_ = true;
    LOG_INFO(kTag, QString("流式录制已启动 (采样率: %1, VAD帧: %2, 静音切换: %3帧, 存储: %4)")
        .arg(sampleRate_).arg(vadFrameSize_).arg(silenceFramesNeeded_).arg(dir));
    return true;
}

void StreamingAudioWriter::writeSamples(const std::vector<float>& samples) {
    QMutexLocker locker(&mutex_);
    if (!recording_ || !currentStream_) return;

    if (samples.empty()) return;

    // 1. 写入 WAV 文件 (float -> int16)
    for (float s : samples) {
        s = std::max(-1.0f, std::min(1.0f, s));  // clip
        int16_t val = static_cast<int16_t>(s * 32767.0f);
        *currentStream_ << val;
        samplesWritten_++;
        totalSamples_++;
    }

    // 2. 用 VAD 检测语音活动
    bool isSpeaking = vad_->process(samples);

    // 3. 静音段切换逻辑：
    //    检测到「说话 → 静音」的过渡，连续静音帧数达到阈值时切换
    if (isSpeaking) {
        silenceFramesAfterSpeech_ = 0;
        wasSpeaking_ = true;
    } else if (wasSpeaking_) {
        silenceFramesAfterSpeech_++;
        if (silenceFramesAfterSpeech_ >= silenceFramesNeeded_ && static_cast<int>(samplesWritten_) > sampleRate_ / 2) {
            // 至少有 0.5 秒音频才切换
            LOG_DEBUG(kTag, QString("检测到静音段 (连续 %1 帧, 能量: %2)，切换 WAV 文件")
                .arg(silenceFramesAfterSpeech_)
                .arg(vad_->currentEnergy(), 0, 'f', 4));

            // 保存文件信息（必须在 closeCurrentFile 之前）
            QString completedPath = currentFilePath_;
            int durationMs = static_cast<int>(samplesWritten_ * 1000 / sampleRate_);

            // 完成并关闭当前文件
            finalizeWavFile();
            closeCurrentFile();

            // 发射完成信号
            emit chunkCompleted(completedPath, durationMs);

            // 打开新文件
            samplesWritten_ = 0;
            silenceFramesAfterSpeech_ = 0;
            wasSpeaking_ = false;
            vad_ = std::make_unique<VoiceActivityDetector>(sampleRate_, 30, 0.003f, 3);

            if (!openNewFile()) {
                LOG_ERROR(kTag, "无法打开新的 WAV 文件，停止录制");
                recording_ = false;
                return;
            }
        }
    }
    // else: 还没开始说话，不计数
}

void StreamingAudioWriter::stop() {
    QMutexLocker locker(&mutex_);
    if (!recording_) return;

    if (samplesWritten_ > 0) {
        finalizeWavFile();
        // 停止时不触发 chunkCompleted，因为最后一小段可能太短
        // 如果需要处理最后一段，可以在外部调用时手动处理
    }
    closeCurrentFile();

    recording_ = false;
    LOG_INFO(kTag, QString("流式录制已停止 (总计: %1 样本, 约 %2 秒)")
        .arg(totalSamples_).arg(totalSamples_ * 1000.0 / sampleRate_ / 1000.0, 0, 'f', 1));
}

QString StreamingAudioWriter::currentFilePath() const {
    QMutexLocker locker(&mutex_);
    return currentFilePath_;
}

int StreamingAudioWriter::recordedDurationMs() const {
    QMutexLocker locker(&mutex_);
    return static_cast<int>(totalSamples_ * 1000 / sampleRate_);
}

bool StreamingAudioWriter::openNewFile() {
    // 生成文件名
    QString dir = getAudioStorageDir(debugEnabled_, debugDir_);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    currentFilePath_ = QString("%1/record_%2.wav").arg(dir).arg(timestamp);

    currentFile_ = new QFile(currentFilePath_);
    if (!currentFile_->open(QIODevice::WriteOnly)) {
        LOG_ERROR(kTag, QString("无法创建 WAV 文件: %1").arg(currentFilePath_));
        delete currentFile_;
        currentFile_ = nullptr;
        return false;
    }

    currentStream_ = new QDataStream(currentFile_);
    currentStream_->setByteOrder(QDataStream::LittleEndian);

    // 初始化并写入 WAV 头
    WavHeader header{};
    memcpy(header.riff, "RIFF", 4);
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    header.fmtSize = 16;
    header.audioFormat = 1;       // PCM
    header.numChannels = 1;       // mono
    header.sampleRate = static_cast<uint32_t>(sampleRate_);
    header.byteRate = static_cast<uint32_t>(sampleRate_) * 2;
    header.blockAlign = 2;
    header.bitsPerSample = 16;
    memcpy(header.data, "data", 4);
    header.dataSize = 0;
    header.fileSize = sizeof(WavHeader) - 8;

    // 写入头
    currentStream_->writeRawData(header.riff, 4);
    *currentStream_ << header.fileSize;
    currentStream_->writeRawData(header.wave, 4);
    currentStream_->writeRawData(header.fmt, 4);
    *currentStream_ << header.fmtSize;
    *currentStream_ << header.audioFormat;
    *currentStream_ << header.numChannels;
    *currentStream_ << header.sampleRate;
    *currentStream_ << header.byteRate;
    *currentStream_ << header.blockAlign;
    *currentStream_ << header.bitsPerSample;
    currentStream_->writeRawData(header.data, 4);
    *currentStream_ << header.dataSize;

    LOG_DEBUG(kTag, QString("新 WAV 文件已打开: %1").arg(currentFilePath_));
    return true;
}

void StreamingAudioWriter::closeCurrentFile() {
    if (currentStream_) {
        delete currentStream_;
        currentStream_ = nullptr;
    }
    if (currentFile_) {
        currentFile_->close();
        delete currentFile_;
        currentFile_ = nullptr;
    }
    currentFilePath_.clear();
}

void StreamingAudioWriter::finalizeWavFile() {
    if (!currentFile_ || !currentFile_->isOpen()) return;

    // 计算实际大小
    uint32_t dataBytes = samplesWritten_ * 2;  // 16-bit mono
    uint32_t fileSize = sizeof(WavHeader) + dataBytes - 8;

    // 回写到文件头更新大小
    currentFile_->seek(4);
    currentStream_->writeRawData(reinterpret_cast<const char*>(&fileSize), 4);
    currentFile_->seek(sizeof(WavHeader) - 4);  // dataSize 偏移
    currentStream_->writeRawData(reinterpret_cast<const char*>(&dataBytes), 4);
    currentFile_->flush();

    int durationMs = static_cast<int>(samplesWritten_ * 1000 / sampleRate_);
    LOG_DEBUG(kTag, QString("WAV 文件已保存: %1 (时长: %2ms, 样本: %3)")
        .arg(currentFilePath_).arg(durationMs).arg(samplesWritten_));
}

} // namespace impress
