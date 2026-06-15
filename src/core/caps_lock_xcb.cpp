#include "caps_lock_xcb.h"
#include "utils/logger.h"

#include <QGuiApplication>
#include <QAbstractEventDispatcher>
#include <QtGui/qguiapplication_platform.h>

#include <xcb/xcb.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

static const char* const kTag = "XcbCapsLockBackend";

namespace impress {

XcbCapsLockBackend::XcbCapsLockBackend(QObject* parent)
    : CapsLockBackend(parent)
{}

XcbCapsLockBackend::~XcbCapsLockBackend() {
    stop();
}

void XcbCapsLockBackend::setHotkeyCombo(int modifiers, int key) {
    CapsLockBackend::setHotkeyCombo(modifiers, key);

    // 如果已经在运行，需要重新注册
    if (m_active) {
        stop();
        start();
    }
}

bool XcbCapsLockBackend::start() {
    if (m_active) return true;

    // 获取 X11 连接（需要 QGuiApplication 类型的指针来访问 nativeInterface）
    auto* guiApp = dynamic_cast<QGuiApplication*>(QCoreApplication::instance());
    if (!guiApp) return false;

    auto* x11App = guiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11App) {
        LOG_INFO(kTag, "非 X11 会话，XGrabKey 不可用");
        return false;
    }

    m_conn = x11App->connection();
    if (!m_conn) {
        LOG_ERROR(kTag, "无法获取 XCB 连接");
        return false;
    }

    // 获取根窗口（通过 XCB setup 信息）
    auto* setup = xcb_get_setup(m_conn);
    if (!setup || setup->roots_len == 0) {
        LOG_ERROR(kTag, "无法获取 XCB 根窗口");
        return false;
    }
    m_rootWindow = xcb_setup_roots_iterator(setup).data->root;

    // 获取按键 keycode（XCB 没有直接的 keysym->keycode，用 Xlib 获取）
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        LOG_ERROR(kTag, "无法打开 X Display");
        return false;
    }
    m_keycode = XKeysymToKeycode(dpy, KeySym(m_key));
    XCloseDisplay(dpy);

    if (m_keycode == 0) {
        LOG_ERROR(kTag, QString("无法获取按键 keycode (key=0x%1)").arg(m_key, 0, 16));
        return false;
    }

    LOG_INFO(kTag, QString("快捷键 keycode: %1 (modifiers=0x%2)").arg(m_keycode).arg(m_modifiers, 0, 16));

    // 抓取快捷键
    if (!grabKey()) {
        return false;
    }

    // 安装 native event filter
    QAbstractEventDispatcher::instance()->installNativeEventFilter(this);

    m_active = true;
    emit ready();
    LOG_INFO(kTag, "全局快捷键已注册（X11-XGrabKey）");
    return true;
}

void XcbCapsLockBackend::stop() {
    if (!m_active) return;

    // 移除 native event filter
    if (QAbstractEventDispatcher* dispatcher = QAbstractEventDispatcher::instance()) {
        dispatcher->removeNativeEventFilter(this);
    }

    ungrabKey();
    m_active = false;
    LOG_INFO(kTag, "全局快捷键已注销（X11）");
}

bool XcbCapsLockBackend::grabKey() {
    // 需要用所有 modifier 组合来抓取
    // 包括: 无修饰、NumLock(Mask2)、Lock(Mask1)、Mode_switch(Mask5) 等
    const uint16_t masks[] = {
        0,                                      // 无修饰
        XCB_MOD_MASK_2,                         // NumLock
        XCB_MOD_MASK_1,                         // Lock
        XCB_MOD_MASK_1 | XCB_MOD_MASK_2,        // Lock + NumLock
        XCB_MOD_MASK_5,                         // Mode_switch
        XCB_MOD_MASK_2 | XCB_MOD_MASK_5,        // NumLock + Mode_switch
        XCB_MOD_MASK_1 | XCB_MOD_MASK_5,        // Lock + Mode_switch
        XCB_MOD_MASK_1 | XCB_MOD_MASK_2 | XCB_MOD_MASK_5,  // 全部
    };

    // Qt modifier → XCB modifier mask
    uint16_t baseMask = 0;
    if (m_modifiers & Qt::ShiftModifier) baseMask |= XCB_MOD_MASK_SHIFT;
    if (m_modifiers & Qt::ControlModifier) baseMask |= XCB_MOD_MASK_CONTROL;
    if (m_modifiers & Qt::AltModifier) baseMask |= XCB_MOD_MASK_1;    // Alt is typically Mod1
    if (m_modifiers & Qt::MetaModifier) baseMask |= XCB_MOD_MASK_4;   // Meta is typically Mod4 (Super)

    for (uint16_t mask : masks) {
        uint16_t combined = baseMask | mask;

        xcb_void_cookie_t cookie = xcb_grab_key_checked(
            m_conn,
            1,              // owner_events = true
            m_rootWindow,   // 在根窗口上抓取
            combined,       // 修饰掩码
            m_keycode,      // 按键码
            XCB_GRAB_MODE_ASYNC,   // pointer mode
            XCB_GRAB_MODE_ASYNC    // keyboard mode
        );

        xcb_generic_error_t* error = xcb_request_check(m_conn, cookie);
        if (error) {
            // 某些 modifier 组合可能已被其他程序占用，忽略
            LOG_DEBUG(kTag, QString("grab modifier %1 失败 (code=%2)")
                .arg(combined).arg(error->error_code));
            free(error);
        }
    }

    return true;
}

void XcbCapsLockBackend::ungrabKey() {
    if (!m_conn || !m_keycode) return;

    const uint16_t masks[] = {
        0,
        XCB_MOD_MASK_2,
        XCB_MOD_MASK_1,
        XCB_MOD_MASK_1 | XCB_MOD_MASK_2,
        XCB_MOD_MASK_5,
        XCB_MOD_MASK_2 | XCB_MOD_MASK_5,
        XCB_MOD_MASK_1 | XCB_MOD_MASK_5,
        XCB_MOD_MASK_1 | XCB_MOD_MASK_2 | XCB_MOD_MASK_5,
    };

    uint16_t baseMask = 0;
    if (m_modifiers & Qt::ShiftModifier) baseMask |= XCB_MOD_MASK_SHIFT;
    if (m_modifiers & Qt::ControlModifier) baseMask |= XCB_MOD_MASK_CONTROL;
    if (m_modifiers & Qt::AltModifier) baseMask |= XCB_MOD_MASK_1;
    if (m_modifiers & Qt::MetaModifier) baseMask |= XCB_MOD_MASK_4;

    for (uint16_t mask : masks) {
        xcb_ungrab_key(m_conn, m_keycode, m_rootWindow, baseMask | mask);
    }
    xcb_flush(m_conn);
}

bool XcbCapsLockBackend::nativeEventFilter(const QByteArray& eventType,
                                            void* message, qintptr*) {
    if (!m_active || eventType != "xcb_generic_event_t")
        return false;

    auto* ev = static_cast<xcb_generic_event_t*>(message);
    uint8_t responseType = ev->response_type & ~0x80;

    if (responseType == XCB_KEY_PRESS) {
        auto* kev = reinterpret_cast<xcb_key_press_event_t*>(ev);
        if (kev->detail == m_keycode) {
            LOG_DEBUG(kTag, "快捷键按下 (X11)");
            emit pressed();
            return true;  // 阻止事件传播
        }
    } else if (responseType == XCB_KEY_RELEASE) {
        auto* kev = reinterpret_cast<xcb_key_release_event_t*>(ev);
        if (kev->detail == m_keycode) {
            LOG_DEBUG(kTag, "快捷键松开 (X11)");
            emit released();
            return true;  // 阻止事件传播
        }
    }

    return false;
}

} // namespace impress
