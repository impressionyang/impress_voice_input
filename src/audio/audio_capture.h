#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <vector>
#include <memory>

namespace impress {

/**
 * @brief 麦克风音频采集模块
 *
 * 基于 PortAudio 实现跨平台音频采集。
 * 在独立线程中运行，通过信号输出音频数据。
 */
class AudioCapture : public QObject {
    Q_OBJECT
public:
    explicit AudioCapture(QObject* parent = nullptr);
    ~AudioCapture() override;

    /** @brief 获取可用输入设备列表 */
    static QStringList getDeviceList();

    /** @brief 获取默认输入设备索引 (-1 表示无默认设备) */
    static int getDefaultDeviceIndex();

    /** @brief 开始采集 */
    bool start(int deviceIndex = -1,
               int sampleRate = 16000,
               int bufferSizeMs = 20);

    /** @brief 停止采集 */
    void stop();

    /** @brief 是否正在采集 */
    bool isRunning() const { return running_; }

signals:
    /** @brief 输出音频数据（归一化 PCM float） */
    void audioDataReady(const std::vector<float>& samples, int sampleRate);

    /** @brief 采集错误 */
    void error(const QString& message);

    /** @brief 采集状态变化 */
    void runningChanged(bool running);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool running_ = false;
};

} // namespace impress
