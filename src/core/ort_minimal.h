#pragma once
/**
 * @brief ONNX Runtime 轻量级 C API 包装器
 *
 * 替代 onnxruntime_cxx_api.h（该文件与 MinGW 存在 ABI 兼容性问题）。
 * 直接使用 C API（onnxruntime_c_api.h），用异常替代 C 风格返回值。
 */

#ifdef HAVE_ONNXRUNTIME

#ifndef ORT_MINIMAL_H
#define ORT_MINIMAL_H

/* 使用 shim 头文件处理 MinGW 兼容性问题 */
#include "ort_api_shim.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace ort {

/** 异常类型 */
class Exception : public std::runtime_error {
public:
    explicit Exception(const char* msg) : std::runtime_error(msg) {}
    explicit Exception(const std::string& msg) : std::runtime_error(msg) {}
};

/** 获取 API 基础指针（内部使用） */
const OrtApi* getApi();

/** Env: ONNX Runtime 环境 */
class Env {
public:
    explicit Env(OrtLoggingLevel logLevel = ORT_LOGGING_LEVEL_WARNING,
                 const char* logId = "ort");
    ~Env();
    OrtEnv* ptr() const { return env_; }
    Env(const Env&) = delete;
    Env& operator=(const Env&) = delete;
private:
    OrtEnv* env_ = nullptr;
};

/** SessionOptions: 会话配置选项 */
class SessionOptions {
public:
    SessionOptions();
    ~SessionOptions();
    OrtSessionOptions* ptr() const { return opts_; }
    void setIntraOpNumThreads(int n);
    void setGraphOptimizationLevel(GraphOptimizationLevel level);
    SessionOptions(const SessionOptions&) = delete;
    SessionOptions& operator=(const SessionOptions&) = delete;
private:
    OrtSessionOptions* opts_ = nullptr;
};

/** Session: 推理会话 */
class Session {
public:
    Session(const Env& env, const char* modelPath, const SessionOptions& opts);
    Session(const Env& env, const wchar_t* modelPath, const SessionOptions& opts);
    ~Session();
    OrtSession* ptr() const { return session_; }
    size_t getInputCount() const;
    size_t getOutputCount() const;
    std::string getInputName(size_t index) const;
    std::string getOutputName(size_t index) const;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
private:
    OrtSession* session_ = nullptr;
};

/** MemoryInfo: 内存信息 */
class MemoryInfo {
public:
    static MemoryInfo createCpu(OrtAllocatorType type = OrtDeviceAllocator,
                                OrtMemType memType = OrtMemTypeCPU);
    ~MemoryInfo();
    const OrtMemoryInfo* ptr() const { return info_; }
private:
    explicit MemoryInfo(OrtMemoryInfo* info);
    OrtMemoryInfo* info_ = nullptr;
};

/** Value: 张量值 */
class Value {
public:
    static Value createTensor(const MemoryInfo& info, float* data, size_t elemCount,
                              const int64_t* shape, size_t shapeLen);
    static Value createTensor(const MemoryInfo& info, int32_t* data, size_t elemCount,
                              const int64_t* shape, size_t shapeLen);
    ~Value();
    OrtValue* ptr() const { return value_; }
    std::vector<int64_t> getShape() const;
    const float* getTensorData() const;
    Value(Value&& other) noexcept;
    Value& operator=(Value&& other) noexcept;
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;
private:
    explicit Value(OrtValue* value);
    OrtValue* value_ = nullptr;
public:
    /** @brief 从原始指针构造（用于接收 C API 返回的值） */
    static Value fromRaw(OrtValue* value);
};

/** RunOptions: 推理选项 */
class RunOptions {
public:
    RunOptions();
    ~RunOptions();
    OrtRunOptions* ptr() const { return opts_; }
private:
    OrtRunOptions* opts_ = nullptr;
};

/** 推理执行 */
std::vector<Value> run(Session& session,
                       const RunOptions& runOptions,
                       const char* const* inputNames,
                       Value* inputValues,
                       size_t inputCount,
                       const char* const* outputNames,
                       size_t outputCount);

} // namespace ort

#endif // ORT_MINIMAL_H
#endif // HAVE_ONNXRUNTIME
