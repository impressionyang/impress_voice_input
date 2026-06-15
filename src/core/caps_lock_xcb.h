#pragma once

#include "caps_lock_backend.h"

#include <QAbstractNativeEventFilter>
#include <QByteArray>

struct xcb_connection_t;

typedef unsigned int xcb_window_t;
typedef unsigned char xcb_keycode_t;

namespace impress {

/**
 * @brief X11 后端：使用 XCB XGrabKey 捕获全局快捷键
 *
 * 零权限，在 X11 下工作良好。Wayland 下不可用。
 * 支持可配置的快捷键组合（如 Ctrl+Alt+C）。
 */
class XcbCapsLockBackend : public CapsLockBackend, public QAbstractNativeEventFilter {
public:
    explicit XcbCapsLockBackend(QObject* parent = nullptr);
    ~XcbCapsLockBackend() override;

    bool start() override;
    void stop() override;
    bool isActive() const override { return m_active; }
    const char* backendName() const override { return "X11-XGrabKey"; }

    void setHotkeyCombo(int modifiers, int key) override;

    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           qintptr* result) override;

private:
    bool grabKey();
    void ungrabKey();

    xcb_connection_t* m_conn = nullptr;
    xcb_window_t m_rootWindow = 0;
    xcb_keycode_t m_keycode = 0;
    bool m_active = false;
};

} // namespace impress
