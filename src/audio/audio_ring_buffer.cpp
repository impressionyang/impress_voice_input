#include "audio_ring_buffer.h"
#include <algorithm>
#include <cstring>

namespace impress {

AudioRingBuffer::AudioRingBuffer(size_t capacity)
    : buffer_(new float[capacity]())
    , capacity_(capacity)
{}

AudioRingBuffer::~AudioRingBuffer() {
    delete[] buffer_;
}

size_t AudioRingBuffer::write(const float* data, size_t count) {
    size_t avail = capacity_ - this->available();
    size_t toWrite = std::min(count, avail);

    for (size_t i = 0; i < toWrite; ++i) {
        buffer_[writePos_] = data[i];
        writePos_ = (writePos_ + 1) % capacity_;
    }

    return toWrite;
}

size_t AudioRingBuffer::read(float* data, size_t count) {
    size_t available = this->available();
    size_t toRead = std::min(count, available);

    for (size_t i = 0; i < toRead; ++i) {
        data[i] = buffer_[readPos_];
        readPos_ = (readPos_ + 1) % capacity_;
    }

    return toRead;
}

size_t AudioRingBuffer::available() const {
    if (writePos_ >= readPos_) {
        return writePos_ - readPos_;
    }
    return capacity_ - (readPos_ - writePos_);
}

void AudioRingBuffer::clear() {
    readPos_ = 0;
    writePos_ = 0;
}

} // namespace impress
