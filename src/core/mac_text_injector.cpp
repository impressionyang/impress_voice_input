#include "mac_text_injector.h"
#include "utils/logger.h"

#ifdef Q_OS_MACOS
#include <ApplicationServices/ApplicationServices.h>
#endif

#include <QThread>

static const char* const kTag = "WaylandTextInjector";

namespace impress {

WaylandTextInjector::WaylandTextInjector(QObject* parent)
    : QObject(parent)
{}

WaylandTextInjector::~WaylandTextInjector() = default;

bool WaylandTextInjector::initialize() {
    if (initialized_) return true;

#ifdef Q_OS_MACOS
    // macOS: 需要辅助功能权限
    initialized_ = true;
    LOG_INFO(kTag, "macOS 文本注入器已初始化（需要辅助功能权限）");
    return true;
#else
    LOG_ERROR(kTag, "WaylandTextInjector 仅支持 macOS 平台");
    return false;
#endif
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
        QThread::usleep(10000);
    }

    LOG_DEBUG(kTag, "文本注入完成");
    return true;
}

bool WaylandTextInjector::injectChar(QChar ch) {
#ifdef Q_OS_MACOS
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!source) return false;

    UniChar code = ch.unicode();

    // 按键按下
    CGEventRef keyDown = CGEventCreateKeyboardEvent(source, 0, true);
    CGEventKeyboardSetUnicodeString(keyDown, 1, &code);
    CGEventPost(kCGHIDEventTap, keyDown);
    CFRelease(keyDown);

    // 按键释放
    CGEventRef keyUp = CGEventCreateKeyboardEvent(source, 0, false);
    CGEventKeyboardSetUnicodeString(keyUp, 1, &code);
    CGEventPost(kCGHIDEventTap, keyUp);
    CFRelease(keyUp);

    CFRelease(source);
    return true;
#else
    (void)ch;
    return false;
#endif
}

bool WaylandTextInjector::simulateKeycode(unsigned int keycode) {
#ifdef Q_OS_MACOS
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!source) return false;

    CGEventRef keyDown = CGEventCreateKeyboardEvent(source, (CGKeyCode)keycode, true);
    CGEventPost(kCGHIDEventTap, keyDown);
    CFRelease(keyDown);

    CGEventRef keyUp = CGEventCreateKeyboardEvent(source, (CGKeyCode)keycode, false);
    CGEventPost(kCGHIDEventTap, keyUp);
    CFRelease(keyUp);

    CFRelease(source);
    return true;
#else
    (void)keycode;
    return false;
#endif
}

} // namespace impress
