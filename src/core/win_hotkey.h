#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

namespace impress {

/**
 * @brief CapsLock 长按语音输入快捷键管理器（Windows）
 *
 * 使用 Windows RegisterHotKey API 实现全局快捷键。
 * 工作流程：
 * 1. 用户长按 CapsLock 1 秒后触发录音
 * 2. 长按期间持续录音
 * 3. 松开 CapsLock 后停止录音并触发转写
 * 4. 短按（< 1s）直接传递 CapsLock 事件（切换大小写锁定）
 */
class CapsLockVoiceHotkey : public QObject {
    Q_OBJECT
public:
    explicit CapsLockVoiceHotkey(QObject* parent = nullptr);
    ~CapsLockVoiceHotkey() override;

    /** @brief 初始化并注册快捷键 */
    bool start();

    /** @brief 停止并注销快捷键 */
    void stop();

    /** @brief 是否已激活 */
    bool isActive() const { return active_; }

    /** @brief 当前是否正在录音（CapsLock 长按超过 1s 后） */
    bool isRecording() const { return recording_; }

signals:
    /** @brief 开始录音（长按超过 1 秒后） */
    void recordingStarted();

    /** @brief 停止录音（松开快捷键后） */
    void recordingStopped();

    /** @brief 快捷键已注册 */
    void ready();

    /** @brief 初始化失败 */
    void error(const QString& message);

    /** @brief 处理 WM_HOTKEY 事件（由原生事件过滤器调用） */
    void onHotkeyEvent(int hotkeyId);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool active_ = false;
    bool recording_ = false;
};

} // namespace impress
