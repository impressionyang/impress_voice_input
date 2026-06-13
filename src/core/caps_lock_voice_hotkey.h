#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace impress {

/**
 * @brief CapsLock 长按语音输入快捷键管理器
 *
 * 双后端实现：
 * - Portal (KDE/默认)：freedesktop GlobalShortcuts D-Bus Portal
 * - libei (GNOME 47+)：libinput-emulator，直接监听键盘事件
 *
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

    /** @brief 临时忽略信号（XTest 模拟按键期间） */
    void setIgnoreEvents(bool ignore) { ignoreEvents_ = ignore; }

signals:
    void recordingStarted();
    void recordingStopped();
    void ready();
    void error(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool active_ = false;
    bool recording_ = false;
    bool ignoreEvents_ = false;

    // Portal 后端方法
    void startPortal();
    void stopPortal();
    void handleSessionResponse(uint response, const QVariantMap& results);
    void handleBindResponse(uint response, const QVariantMap& results);
    void handleActivated(const QString& shortcutId);
    void handleDeactivated(const QString& shortcutId);
    void onPortalResponse(uint response, const QVariantMap& results);

    // libei 后端方法
    void startLibei();
    void stopLibei();
    void onLibeiEvent();
};

} // namespace impress
