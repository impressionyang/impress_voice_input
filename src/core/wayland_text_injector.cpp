#include "wayland_text_injector.h"
#include "utils/logger.h"

#include <QLibrary>
#include <QThread>

static const char* const kTag = "WaylandTextInjector";

namespace impress {

// XTest 函数指针类型
typedef int (*XTestFakeKeyEventFn)(void* display, unsigned int keycode,
                                    int is_press, unsigned long delay);
typedef void* (*XOpenDisplayFn)(const char* display_name);
typedef int (*XCloseDisplayFn)(void* display);
typedef unsigned int (*XKeysymToKeycodeFn)(void* display, unsigned long keysym);
typedef unsigned long (*XStringToKeysymFn)(const char* str);
typedef int (*XSyncFn)(void* display, int discard);

struct WaylandTextInjector::Impl {
    QLibrary x11Lib;
    QLibrary xtstLib;

    XOpenDisplayFn XOpenDisplay = nullptr;
    XCloseDisplayFn XCloseDisplay = nullptr;
    XKeysymToKeycodeFn XKeysymToKeycode = nullptr;
    XStringToKeysymFn XStringToKeysym = nullptr;
    XSyncFn XSyncFnPtr = nullptr;
    XTestFakeKeyEventFn XTestFakeKeyEvent = nullptr;

    void* display = nullptr;

    bool loadLibraries() {
        // 加载 libX11
        x11Lib.setFileName("libX11.so.6");
        if (!x11Lib.load()) {
            LOG_ERROR(kTag, QString("无法加载 libX11: %1").arg(x11Lib.errorString()));
            return false;
        }

        // 加载 libXtst
        xtstLib.setFileName("libXtst.so.6");
        if (!xtstLib.load()) {
            LOG_ERROR(kTag, QString("无法加载 libXtst: %1").arg(xtstLib.errorString()));
            return false;
        }

        // 解析 X11 符号
        XOpenDisplay = reinterpret_cast<XOpenDisplayFn>(x11Lib.resolve("XOpenDisplay"));
        XCloseDisplay = reinterpret_cast<XCloseDisplayFn>(x11Lib.resolve("XCloseDisplay"));
        XKeysymToKeycode = reinterpret_cast<XKeysymToKeycodeFn>(x11Lib.resolve("XKeysymToKeycode"));
        XStringToKeysym = reinterpret_cast<XStringToKeysymFn>(x11Lib.resolve("XStringToKeysym"));
        XSyncFnPtr = reinterpret_cast<XSyncFn>(x11Lib.resolve("XSync"));

        // 解析 XTest 符号
        XTestFakeKeyEvent = reinterpret_cast<XTestFakeKeyEventFn>(
            xtstLib.resolve("XTestFakeKeyEvent"));

        if (!XOpenDisplay || !XCloseDisplay || !XKeysymToKeycode ||
            !XSyncFnPtr || !XTestFakeKeyEvent) {
            LOG_ERROR(kTag, "无法解析 X11/XTest 符号");
            return false;
        }

        // 打开 X11 显示（通过 XWayland）
        display = XOpenDisplay(nullptr);
        if (!display) {
            display = XOpenDisplay(":0");
        }
        if (!display) {
            LOG_ERROR(kTag, "无法连接 X11 显示（XWayland）");
            return false;
        }

        LOG_INFO(kTag, "XTest 文本注入器已初始化");
        return true;
    }
};

WaylandTextInjector::WaylandTextInjector(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{}

WaylandTextInjector::~WaylandTextInjector() {
    if (impl_->display && impl_->XCloseDisplay) {
        impl_->XCloseDisplay(impl_->display);
    }
}

bool WaylandTextInjector::initialize() {
    if (initialized_) return true;
    initialized_ = impl_->loadLibraries();
    return initialized_;
}

bool WaylandTextInjector::injectText(const QString& text) {
    if (!initialized_) {
        LOG_ERROR(kTag, "文本注入器未初始化");
        return false;
    }

    if (text.isEmpty()) return true;

    LOG_DEBUG(kTag, QString("注入文本 (%1 字符): %2").arg(text.length()).arg(text));

    for (int i = 0; i < text.length(); i++) {
        if (!injectChar(text[i])) {
            LOG_WARNING(kTag, QString("字符注入失败: '%1'").arg(text[i]));
        }
        // 字符间短暂延迟
        QThread::usleep(10000); // 10ms
    }

    LOG_DEBUG(kTag, "文本注入完成");
    return true;
}

bool WaylandTextInjector::injectChar(QChar ch) {
    if (!impl_->display) return false;

    // 处理常见字符映射
    unsigned long keysym;
    if (ch.isLetterOrNumber() || ch.isPunct() || ch.isSymbol()) {
        // ASCII 字符直接使用 keysym
        keysym = ch.unicode();
    } else if (ch == '\n' || ch == '\r') {
        keysym = 0xff0d; // XK_Return
    } else if (ch == '\t') {
        keysym = 0xff09; // XK_Tab
    } else if (ch == ' ') {
        keysym = 0x020; // XK_space
    } else {
        // 尝试通过 XStringToKeysym 解析
        QByteArray ba = QString(ch).toUtf8();
        keysym = impl_->XStringToKeysym(ba.constData());
        if (keysym == 0) {
            return false; // 不支持的字符
        }
    }

    unsigned int keycode = impl_->XKeysymToKeycode(impl_->display, keysym);
    if (keycode == 0) return false;

    // Shift 处理（大写字母需要按住 Shift）
    bool needShift = ch.isUpper() && ch.isLetter();
    if (needShift) {
        unsigned int shiftCode = impl_->XKeysymToKeycode(impl_->display, 0xffe1); // XK_Shift_L
        if (shiftCode) {
            impl_->XTestFakeKeyEvent(impl_->display, shiftCode, 1, 0);
        }
    }

    // 按键按下 + 释放
    impl_->XTestFakeKeyEvent(impl_->display, keycode, 1, 0);
    impl_->XTestFakeKeyEvent(impl_->display, keycode, 0, 0);

    if (needShift) {
        unsigned int shiftCode = impl_->XKeysymToKeycode(impl_->display, 0xffe1);
        if (shiftCode) {
            impl_->XTestFakeKeyEvent(impl_->display, shiftCode, 0, 0);
        }
    }

    impl_->XSyncFnPtr(impl_->display, 0);
    return true;
}

bool WaylandTextInjector::simulateKeycode(unsigned int keycode) {
    if (!impl_->display || !impl_->XTestFakeKeyEvent) return false;

    LOG_DEBUG(kTag, QString("模拟 keycode: 0x%1").arg(keycode, 0, 16));

    // 按下 + 释放
    impl_->XTestFakeKeyEvent(impl_->display, keycode, 1, 0);
    impl_->XTestFakeKeyEvent(impl_->display, keycode, 0, 0);
    impl_->XSyncFnPtr(impl_->display, 0);
    return true;
}

bool WaylandTextInjector::simulateKeysym(unsigned long keysym) {
    if (!impl_->display || !impl_->XKeysymToKeycode) return false;

    unsigned int keycode = impl_->XKeysymToKeycode(impl_->display, keysym);
    if (keycode == 0) {
        LOG_WARNING(kTag, QString("keysym 0x%1 无法转换为 keycode").arg(keysym, 0, 16));
        return false;
    }

    LOG_DEBUG(kTag, QString("模拟 keysym 0x%1 → keycode %2").arg(keysym, 0, 16).arg(keycode));
    return simulateKeycode(keycode);
}

} // namespace impress
