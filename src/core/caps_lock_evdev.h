#pragma once

#include "caps_lock_backend.h"

#include <QThread>
#include <memory>

namespace impress {

/**
 * @brief Wayland 后端：通过 evdev 直接读取键盘设备
 *
 * 使用 EVIOCGRAB 抓取键盘 + uinput 重放非快捷键事件。
 * 需要 input 组权限或 udev 规则。
 *
 * 权限设置（一次性）：
 *   sudo usermod -aG input $USER
 *   或创建 /etc/udev/rules.d/99-impress-keyboard.rules:
 *     KERNEL=="event*", SUBSYSTEM=="input", MODE="0660", GROUP="input"
 *     KERNEL=="uinput", MODE="0660", GROUP="input"
 */
class EvdevCapsLockBackend : public CapsLockBackend {
    Q_OBJECT
public:
    explicit EvdevCapsLockBackend(QObject* parent = nullptr);
    ~EvdevCapsLockBackend() override;

    bool start() override;
    void stop() override;
    bool isActive() const override { return m_active; }
    const char* backendName() const override { return "evdev"; }

    void setHotkeyCombo(int modifiers, int key) override;

public slots:
    void emitPressed() { emit pressed(); }
    void emitReleased() { emit released(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_active = false;
};

} // namespace impress
