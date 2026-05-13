#pragma once

#include <QObject>
#include <QString>
#include <QFile>
#include <QDataStream>
#include <QMutex>
#include <cstdint>
#include <memory>
#include <vector>

namespace impress {

class VoiceActivityDetector;

/**
 * @brief 流式音频录制器
 *
 * 将连续音频数据写入 WAV 文件，通过 VAD 检测静音段自动切换文件。
 * 完成一个 WAV 文件后，通过 signal 输出文件路径供外部识别。
 *
 * 工作流程：
 * 1. 音频数据持续写入当前 WAV 文件
 * 2. VAD 实时检测语音活动
 * 3. 检测到 ~1s 静音后，关闭当前文件、发射 chunkCompleted 信号、打开新文件
 * 4. 外部收到信号后，在后台线程对 WAV 文件进行识别
 *
 * 音频存储路径：
 *   - debug_save_audio 开启 → 使用配置的 audio_debug_dir
 *   - debug_save_audio 关闭 → Windows: 当前目录, Linux/Mac: 系统临时目录
 */
class StreamingAudioWriter : public QObject {
    Q_OBJECT
public:
    explicit StreamingAudioWriter(QObject* parent = nullptr);
    ~StreamingAudioWriter() override;

    /**
     * @brief 开始录制（打开第一个 WAV 文件）
     * @param sampleRate 采样率 (如 16000)
     * @param debugEnabled 是否开启调试模式（保存到配置路径）
     * @param debugDir 调试目录（debugEnabled=true 时使用，为空则使用默认值）
     */
    bool start(int sampleRate, bool debugEnabled = false, const QString& debugDir = QString());

    /**
     * @brief 写入音频样本（归一化 PCM float，范围 -1.0 ~ 1.0）
     *
     * 此方法会：
     * 1. 写入当前 WAV 文件
     * 2. 通过 VAD 检测语音活动
     * 3. 检测到静音段时自动切换文件并触发 chunkCompleted 信号
     */
    void writeSamples(const std::vector<float>& samples);

    /**
     * @brief 停止录制，关闭当前文件（不触发 chunkCompleted）
     */
    void stop();

    /** @brief 是否正在录制 */
    bool isRecording() const { return recording_; }

    /** @brief 当前 WAV 文件路径 */
    QString currentFilePath() const;

    /** @brief 当前文件已写入的样本数 */
    int currentSampleCount() const { return samplesWritten_; }

    /** @brief 已录制音频总时长（毫秒） */
    int recordedDurationMs() const;

    /**
     * @brief 获取音频存储目录（根据 debug 状态自动选择）
     */
    static QString getAudioStorageDir(bool debugEnabled, const QString& debugDir = QString());

signals:
    /**
     * @brief 一个 WAV 文件录制完成（检测到静音段切换）
     * @param filePath WAV 文件的完整路径
     * @param durationMs 音频时长（毫秒）
     */
    void chunkCompleted(const QString& filePath, int durationMs);

    /** @brief 录制错误 */
    void error(const QString& message);

private:
    /** 打开新的 WAV 文件 */
    bool openNewFile();

    /** 关闭当前 WAV 文件（不更新文件头） */
    void closeCurrentFile();

    /** 更新 WAV 文件头的 data chunk 大小和 RIFF 大小 */
    void finalizeWavFile();

    // WAV 文件头结构 (44 字节)
    struct WavHeader {
        char riff[4];          // "RIFF"
        uint32_t fileSize;     // 文件总大小 - 8
        char wave[4];          // "WAVE"
        char fmt[4];           // "fmt "
        uint32_t fmtSize;      // fmt chunk 大小 (16)
        uint16_t audioFormat;  // 音频格式 (1 = PCM)
        uint16_t numChannels;  // 通道数 (1 = mono)
        uint32_t sampleRate;   // 采样率
        uint32_t byteRate;     // 字节率
        uint16_t blockAlign;   // 块对齐
        uint16_t bitsPerSample;// 位深度 (16)
        char data[4];          // "data"
        uint32_t dataSize;     // data chunk 大小
    };

    int sampleRate_ = 16000;
    bool recording_ = false;
    bool debugEnabled_ = false;
    QString debugDir_;

    // VAD
    std::unique_ptr<VoiceActivityDetector> vad_;
    bool wasSpeaking_ = false;        // 上一帧是否在说话
    int silenceFramesAfterSpeech_ = 0; // 说话后连续静音帧数
    int silenceFramesNeeded_ = 4;      // 需要多少帧静音才切换（~1s）

    // 当前文件
    QString currentFilePath_;
    QFile* currentFile_ = nullptr;
    QDataStream* currentStream_ = nullptr;
    uint32_t samplesWritten_ = 0;
    int64_t totalSamples_ = 0;

    // 能量计算帧大小（VAD 帧大小）
    int vadFrameSize_ = 480;  // 16000 * 30ms = 480

    mutable QMutex mutex_;
};

} // namespace impress
