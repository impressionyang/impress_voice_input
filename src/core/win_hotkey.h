#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace impress {

/**
 * @brief 全局语音输入快捷键管理器（Windows）
 *
 * 使用 Windows RegisterHotKey API 实现全局快捷键。
 * 切换模式：按一次开始录音，再按一次停止录音并转写。
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

    /** @brief 当前是否正在录音 */
    bool isRecording() const { return recording_; }

    /** @brief 临时忽略信号 */
    void setIgnoreEvents(bool ignore) { ignoreEvents_ = ignore; }

    /** @brief 设置快捷键组合（如 "Ctrl+Alt+C"） */
    void setHotkeyCombo(const QString& combo);

signals:
    void recordingStarted();
    void recordingStopped();
    void ready();
    void error(const QString& message);

    /** @brief 处理 WM_HOTKEY 事件（由原生事件过滤器调用） */
public:
    void onHotkeyEvent(int hotkeyId);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool active_ = false;
    bool recording_ = false;
    bool ignoreEvents_ = false;
    QString hotkeyCombo_;
    int modifiers_ = 0x0002 | 0x0001;  // MOD_CONTROL | MOD_ALT
    int vkKey_ = 'C';

    void registerHotkey();
    void unregisterHotkey();
    void toggleRecording();
};

} // namespace impress
