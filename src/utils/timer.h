#pragma once

#include <chrono>

namespace impress {

/**
 * @brief 高精度计时器
 */
class Timer {
public:
    Timer() { reset(); }

    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    /** @brief 返回从 reset 至今的毫秒数 */
    double elapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

    /** @brief 返回从 reset 至今的微秒数 */
    double elapsedUs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(now - start_).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace impress
