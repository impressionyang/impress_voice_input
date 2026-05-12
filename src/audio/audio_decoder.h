#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>
#include <memory>
#include <cstdint>

namespace impress {

/**
 * @brief 音频文件解码模块
 *
 * 支持 WAV, MP3, FLAC, OGG, AAC 格式。
 * 基于 dr_libs (dr_wav, dr_mp3, dr_flac)。
 */
class AudioDecoder : public QObject {
    Q_OBJECT
public:
    explicit AudioDecoder(QObject* parent = nullptr);
    ~AudioDecoder() override;

    /** @brief 支持的格式 (WAV, MP3, FLAC) */
    static QStringList supportedFormats();

    /** @brief 解码音频文件 */
    bool decode(const QString& filePath);

    /** @brief 获取解码后的 PCM 数据 */
    const std::vector<float>& samples() const { return samples_; }

    /** @brief 采样率 */
    int sampleRate() const { return sampleRate_; }

    /** @brief 声道数 */
    int channels() const { return channels_; }

    /** @brief 时长（秒） */
    double duration() const;

signals:
    /** @brief 解码进度 (0.0 - 1.0) */
    void progress(double progress);

    /** @brief 解码完成 */
    void decoded(const QString& filePath);

    /** @brief 解码错误 */
    void error(const QString& message);

private:
#ifdef HAVE_DR_LIBS
    bool decodeWav(const QString& filePath);
    bool decodeMp3(const QString& filePath);
    bool decodeFlac(const QString& filePath);
    void convertToMono(const std::vector<short>& pcm16, uint64_t frameCount);
#endif

    std::vector<float> samples_;
    int sampleRate_ = 0;
    int channels_ = 0;
};

} // namespace impress
