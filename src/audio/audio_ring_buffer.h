#pragma once

#include <QObject>
#include <QVector>
#include <memory>
#include <cstddef>

namespace impress {

/**
 * @brief 音频环形缓冲区
 *
 * 线程安全的单生产者单消费者环形缓冲区，用于麦克风采集数据缓冲。
 */
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t capacity);
    ~AudioRingBuffer();

    /** @brief 写入数据（生产端） */
    size_t write(const float* data, size_t count);

    /** @brief 读取数据（消费端） */
    size_t read(float* data, size_t count);

    /** @brief 当前可读数据量 */
    size_t available() const;

    /** @brief 清空缓冲区 */
    void clear();

private:
    float* buffer_;
    size_t capacity_;
    size_t readPos_ = 0;
    size_t writePos_ = 0;
};

} // namespace impress
