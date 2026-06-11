/**
 * @brief ONNX Runtime 轻量级 C API 包装器 - 实现
 *
 * 直接使用 C API，避免 onnxruntime_cxx_api.h 的 MinGW 兼容性问题。
 */

#ifdef HAVE_ONNXRUNTIME

#include "ort_minimal.h"
#include <cstring>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ort {

/**
 * 获取 OrtApi 指针
 *
 * 调用路径: OrtGetApiBase() -> OrtApiBase -> GetApi(version) -> OrtApi
 */
const OrtApi* getApi() {
    static const OrtApi* api = nullptr;
    if (!api) {
        const OrtApiBase* apiBase = OrtGetApiBase();
        api = apiBase->GetApi(ORT_API_VERSION);
    }
    return api;
}

// ============================================================================
// Env
// ============================================================================
Env::Env(OrtLoggingLevel logLevel, const char* logId) {
    const OrtApi* api = getApi();
    OrtStatus* status = api->CreateEnv(logLevel, logId, &env_);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "CreateEnv failed");
    }
}

Env::~Env() {
    if (env_) {
        const OrtApi* api = getApi();
        api->ReleaseEnv(env_);
    }
}

// ============================================================================
// SessionOptions
// ============================================================================
SessionOptions::SessionOptions() {
    const OrtApi* api = getApi();
    OrtStatus* status = api->CreateSessionOptions(&opts_);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "CreateSessionOptions failed");
    }
}

SessionOptions::~SessionOptions() {
    if (opts_) {
        const OrtApi* api = getApi();
        api->ReleaseSessionOptions(opts_);
    }
}

void SessionOptions::setIntraOpNumThreads(int n) {
    const OrtApi* api = getApi();
    OrtStatus* status = api->SetIntraOpNumThreads(opts_, n);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "SetIntraOpNumThreads failed");
    }
}

void SessionOptions::setGraphOptimizationLevel(GraphOptimizationLevel level) {
    const OrtApi* api = getApi();
    OrtStatus* status = api->SetSessionGraphOptimizationLevel(opts_, level);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "SetSessionGraphOptimizationLevel failed");
    }
}

// ============================================================================
// Session
// ============================================================================
Session::Session(const Env& env, const char* modelPath, const SessionOptions& opts) {
    const OrtApi* api = getApi();
#ifdef _WIN32
    // Windows: ORTCHAR_T = wchar_t, 需要将 UTF-8 转换为 UTF-16
    int len = MultiByteToWideChar(CP_UTF8, 0, modelPath, -1, nullptr, 0);
    std::vector<wchar_t> widePath(len);
    MultiByteToWideChar(CP_UTF8, 0, modelPath, -1, widePath.data(), len);
    OrtStatus* status = api->CreateSession(env.ptr(), widePath.data(), opts.ptr(), &session_);
#else
    OrtStatus* status = api->CreateSession(env.ptr(), modelPath, opts.ptr(), &session_);
#endif
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "CreateSession failed");
    }
}

#ifdef _WIN32
Session::Session(const Env& env, const wchar_t* modelPath, const SessionOptions& opts) {
    const OrtApi* api = getApi();
    OrtStatus* status = api->CreateSession(env.ptr(), modelPath, opts.ptr(), &session_);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "CreateSession failed");
    }
}
#endif

Session::~Session() {
    if (session_) {
        const OrtApi* api = getApi();
        api->ReleaseSession(session_);
    }
}

size_t Session::getInputCount() const {
    const OrtApi* api = getApi();
    size_t count = 0;
    OrtStatus* status = api->SessionGetInputCount(session_, &count);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "SessionGetInputCount failed");
    }
    return count;
}

size_t Session::getOutputCount() const {
    const OrtApi* api = getApi();
    size_t count = 0;
    OrtStatus* status = api->SessionGetOutputCount(session_, &count);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "SessionGetOutputCount failed");
    }
    return count;
}

std::string Session::getInputName(size_t index) const {
    const OrtApi* api = getApi();
    char* name = nullptr;
    OrtAllocator* allocator = nullptr;
    OrtStatus* status = api->GetAllocatorWithDefaultOptions(&allocator);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "GetAllocatorWithDefaultOptions failed");
    }
    status = api->SessionGetInputName(session_, index, allocator, &name);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "SessionGetInputName failed");
    }
    std::string result(name);
    // 不要释放 name - allocator 分配的内存由 allocator 管理
    return result;
}

std::string Session::getOutputName(size_t index) const {
    const OrtApi* api = getApi();
    char* name = nullptr;
    OrtAllocator* allocator = nullptr;
    OrtStatus* status = api->GetAllocatorWithDefaultOptions(&allocator);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "GetAllocatorWithDefaultOptions failed");
    }
    status = api->SessionGetOutputName(session_, index, allocator, &name);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "SessionGetOutputName failed");
    }
    std::string result(name);
    return result;
}

// ============================================================================
// MemoryInfo
// ============================================================================
MemoryInfo::MemoryInfo(OrtMemoryInfo* info) : info_(info) {}

MemoryInfo MemoryInfo::createCpu(OrtAllocatorType type, OrtMemType memType) {
    const OrtApi* api = getApi();
    OrtMemoryInfo* info = nullptr;
    OrtStatus* status = api->CreateCpuMemoryInfo(type, memType, &info);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "CreateCpuMemoryInfo failed");
    }
    return MemoryInfo(info);
}

MemoryInfo::~MemoryInfo() {
    if (info_) {
        const OrtApi* api = getApi();
        api->ReleaseMemoryInfo(info_);
    }
}

// ============================================================================
// Value
// ============================================================================
Value::Value(OrtValue* value) : value_(value) {}

Value Value::fromRaw(OrtValue* value) {
    return Value(value);
}

Value Value::createTensor(const MemoryInfo& info, float* data, size_t elemCount,
                          const int64_t* shape, size_t shapeLen) {
    const OrtApi* api = getApi();
    OrtValue* value = nullptr;
    OrtStatus* status = api->CreateTensorWithDataAsOrtValue(
        info.ptr(), data, elemCount * sizeof(float),
        shape, shapeLen, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &value);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "CreateTensorWithDataAsOrtValue failed");
    }
    return Value(value);
}

Value Value::createTensor(const MemoryInfo& info, int32_t* data, size_t elemCount,
                          const int64_t* shape, size_t shapeLen) {
    const OrtApi* api = getApi();
    OrtValue* value = nullptr;
    OrtStatus* status = api->CreateTensorWithDataAsOrtValue(
        info.ptr(), data, elemCount * sizeof(int32_t),
        shape, shapeLen, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32, &value);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "CreateTensorWithDataAsOrtValue failed");
    }
    return Value(value);
}

Value::~Value() {
    if (value_) {
        const OrtApi* api = getApi();
        api->ReleaseValue(value_);
    }
}

std::vector<int64_t> Value::getShape() const {
    const OrtApi* api = getApi();
    OrtTensorTypeAndShapeInfo* info = nullptr;
    OrtStatus* status = api->GetTensorTypeAndShape(value_, &info);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "GetTensorTypeAndShape failed");
    }
    size_t dimCount = 0;
    status = api->GetDimensionsCount(info, &dimCount);
    std::vector<int64_t> shape(dimCount);
    if (dimCount > 0) {
        status = api->GetDimensions(info, shape.data(), dimCount);
    }
    api->ReleaseTensorTypeAndShapeInfo(info);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "GetDimensions failed");
    }
    return shape;
}

const float* Value::getTensorData() const {
    const OrtApi* api = getApi();
    void* data = nullptr;
    OrtStatus* status = api->GetTensorMutableData(value_, &data);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "GetTensorMutableData failed");
    }
    return static_cast<const float*>(data);
}

Value::Value(Value&& other) noexcept : value_(other.value_) {
    other.value_ = nullptr;
}

Value& Value::operator=(Value&& other) noexcept {
    if (this != &other) {
        if (value_) {
            const OrtApi* api = getApi();
            api->ReleaseValue(value_);
        }
        value_ = other.value_;
        other.value_ = nullptr;
    }
    return *this;
}

// ============================================================================
// RunOptions
// ============================================================================
RunOptions::RunOptions() {
    const OrtApi* api = getApi();
    OrtStatus* status = api->CreateRunOptions(&opts_);
    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "CreateRunOptions failed");
    }
}

RunOptions::~RunOptions() {
    if (opts_) {
        const OrtApi* api = getApi();
        api->ReleaseRunOptions(opts_);
    }
}

// ============================================================================
// run
// ============================================================================
std::vector<Value> run(Session& session,
                       const RunOptions& runOptions,
                       const char* const* inputNames,
                       Value* inputValues,
                       size_t inputCount,
                       const char* const* outputNames,
                       size_t outputCount)
{
    const OrtApi* api = getApi();

    // 准备输入输出 OrtValue 指针数组
    std::vector<OrtValue*> inputPtrs(inputCount);
    for (size_t i = 0; i < inputCount; i++) {
        inputPtrs[i] = inputValues[i].ptr();
    }

    std::vector<OrtValue*> outputPtrs(outputCount, nullptr);

    OrtStatus* status = api->Run(
        session.ptr(),
        runOptions.ptr(),
        inputNames, inputPtrs.data(), static_cast<int>(inputCount),
        outputNames, static_cast<int>(outputCount),
        outputPtrs.data());

    if (status) {
        const char* msg = api->GetErrorMessage(status);
        throw Exception(msg ? msg : "Run failed");
    }

    std::vector<Value> results;
    results.reserve(outputCount);
    for (size_t i = 0; i < outputCount; i++) {
        results.emplace_back(Value::fromRaw(outputPtrs[i]));
    }
    return results;
}

} // namespace ort

#endif // HAVE_ONNXRUNTIME
