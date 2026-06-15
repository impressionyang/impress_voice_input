#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace impress {

class CapsLockBackend;

/**
 * @brief 全局语音输入快捷键管理器
 *
 * 支持可配置的组合键（默认 Ctrl+Alt+C），切换模式：
 *   按一次 → 开始录音
 *   再按一次 → 停止录音并转写
 *
 * 五后端实现（按优先级）：
 * 1. X11 XGrabKey（X11 会话，零权限）
 * 2. evdev grab + uinput replay（Wayland，需 input 组）
 * 3. GNOME 扩展（GNOME 46+，通过 D-Bus 信号）
 * 4. libei（GNOME 47+/KDE，libinput-emulator）
 * 5. Portal（KDE/默认，GlobalShortcuts D-Bus Portal）
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

    /** @brief 是否已激活或正在等待 Portal 授权 */
    bool isActive() const { return active_ || pending_; }

    /** @brief 当前是否正在录音 */
    bool isRecording() const { return recording_; }

    /** @brief 临时忽略信号（XTest 模拟按键期间） */
    void setIgnoreEvents(bool ignore) { ignoreEvents_ = ignore; }

    /** @brief 设置快捷键组合（如 "Ctrl+Alt+C"） */
    void setHotkeyCombo(const QString& combo);

signals:
    void recordingStarted();
    void recordingStopped();
    void ready();
    void error(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool active_ = false;
    bool pending_ = false;  // Portal 授权进行中
    bool recording_ = false;
    bool ignoreEvents_ = false;
    QString hotkeyCombo_;

    // 纯 C++ 后端
    std::unique_ptr<CapsLockBackend> nativeBackend_;

    /** @brief 切换模式：按下快捷键时在录音/停止之间切换 */
    void toggleRecording();

    void onNativePressed();
    void onNativeReleased();

    // GNOME 扩展后端
    bool startGnomeExtension();
    void stopGnomeExtension();
    void onGnomeCapsLockPressed();
    void onGnomeCapsLockReleased();

    // Portal 后端
    bool startPortal();
    void stopPortal();
    void handleSessionResponse(uint response, const QVariantMap& results);
    void handleBindResponse(uint response, const QVariantMap& results);
    void handleActivated(const QString& shortcutId);
    void handleDeactivated(const QString& shortcutId);
    void onPortalResponse(uint response, const QVariantMap& results);

    // libei 后端
    void startLibei();
    void stopLibei();
    void onLibeiEvent();
};

} // namespace impress
