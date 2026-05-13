#include "win_text_injector.h"
#include "utils/logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
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

#ifdef Q_OS_WIN
    // SendInput 不需要额外初始化
    initialized_ = true;
    LOG_INFO(kTag, "Windows 文本注入器已初始化 (SendInput)");
    return true;
#else
    LOG_ERROR(kTag, "WaylandTextInjector 仅支持 Windows 平台");
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
        // 字符间短暂延迟
        QThread::usleep(10000); // 10ms
    }

    LOG_DEBUG(kTag, "文本注入完成");
    return true;
}

bool WaylandTextInjector::injectChar(QChar ch) {
#ifdef Q_OS_WIN
    INPUT inputs[4] = {};
    int count = 0;

    // 尝试使用 Unicode 输入法（支持所有 Unicode 字符）
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = ch.unicode();
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
    count++;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wScan = ch.unicode();
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    count++;

    UINT sent = SendInput(count, inputs, sizeof(INPUT));
    if (sent == 0) {
        DWORD err = GetLastError();
        LOG_ERROR(kTag, QString("SendInput 失败: %1").arg(err));
        return false;
    }
    return true;
#else
    (void)ch;
    return false;
#endif
}

bool WaylandTextInjector::simulateKeycode(unsigned int keycode) {
#ifdef Q_OS_WIN
    INPUT inputs[2] = {};

    // 按下
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = static_cast<WORD>(keycode);
    inputs[0].ki.dwFlags = 0;

    // 释放
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = static_cast<WORD>(keycode);
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    UINT sent = SendInput(2, inputs, sizeof(INPUT));
    if (sent == 0) {
        DWORD err = GetLastError();
        LOG_ERROR(kTag, QString("SendInput keycode 注入失败: %1").arg(err));
        return false;
    }

    LOG_DEBUG(kTag, QString("模拟 keycode: 0x%1").arg(keycode, 0, 16));
    return true;
#else
    (void)keycode;
    return false;
#endif
}

} // namespace impress
