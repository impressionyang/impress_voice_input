#include "caps_lock_backend.h"
#include "utils/logger.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <cstring>

#ifdef Q_OS_LINUX
#include <unistd.h>
#include "caps_lock_xcb.h"
#include "caps_lock_evdev.h"
#endif

static const char* const kTag = "HotkeyBackend";

namespace impress {

bool parseHotkeyCombo(const QString& combo, int& modifiers, int& key) {
    if (combo.isEmpty() || combo == "未设置" || combo == "CapsLock") {
        return false;
    }

    modifiers = 0;
    key = 0;

    QStringList parts = combo.split('+', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;

    for (int i = 0; i < parts.size() - 1; i++) {
        QString part = parts[i].trimmed().toLower();
        if (part == "ctrl" || part == "control") modifiers |= Qt::ControlModifier;
        else if (part == "alt") modifiers |= Qt::AltModifier;
        else if (part == "shift") modifiers |= Qt::ShiftModifier;
        else if (part == "meta" || part == "super" || part == "win") modifiers |= Qt::MetaModifier;
        else {
            LOG_WARNING(kTag, QString("未知的修饰键: %1").arg(part));
            return false;
        }
    }

    // 最后一个部分是主键
    QString mainKey = parts.last().trimmed();
    if (mainKey.length() == 1) {
        // 单字母/数字
        QChar ch = mainKey[0];
        if (ch.isLetter()) {
            key = Qt::Key_A + (ch.toUpper().unicode() - 'A');
        } else if (ch.isDigit()) {
            key = Qt::Key_0 + (ch.unicode() - '0');
        }
    } else if (mainKey.startsWith('F') && mainKey.size() <= 3) {
        bool ok;
        int num = mainKey.mid(1).toInt(&ok);
        if (ok && num >= 1 && num <= 35) {
            key = Qt::Key_F1 + (num - 1);
        }
    } else if (mainKey.compare("space", Qt::CaseInsensitive) == 0) {
        key = Qt::Key_Space;
    } else if (mainKey.compare("enter", Qt::CaseInsensitive) == 0) {
        key = Qt::Key_Return;
    } else if (mainKey.compare("return", Qt::CaseInsensitive) == 0) {
        key = Qt::Key_Return;
    } else if (mainKey.compare("escape", Qt::CaseInsensitive) == 0) {
        key = Qt::Key_Escape;
    } else if (mainKey.compare("tab", Qt::CaseInsensitive) == 0) {
        key = Qt::Key_Tab;
    } else if (mainKey.compare("backspace", Qt::CaseInsensitive) == 0) {
        key = Qt::Key_Backspace;
    } else if (mainKey.compare("delete", Qt::CaseInsensitive) == 0) {
        key = Qt::Key_Delete;
    } else {
        LOG_WARNING(kTag, QString("未知的主键: %1").arg(mainKey));
        return false;
    }

    return key != 0;
}

/**
 * @brief 检测当前是否在 X11 下运行
 */
static bool isX11Session() {
    const char* sessionType = getenv("XDG_SESSION_TYPE");
    if (sessionType && std::strcmp(sessionType, "x11") == 0)
        return true;

    // 回退：检查 DISPLAY 环境变量
    return getenv("DISPLAY") != nullptr;
}

CapsLockBackend* createCapsLockBackend(QObject* parent) {
    // 1. X11 会话优先使用 XGrabKey（零权限）
    if (isX11Session()) {
#ifdef Q_OS_LINUX
        return new XcbCapsLockBackend(parent);
#else
        Q_UNUSED(parent);
        return nullptr;
#endif
    }

    // 2. Wayland 会话尝试 evdev（需权限）
#ifdef Q_OS_LINUX
    // 先快速检查权限
    if (access("/dev/uinput", W_OK) == 0) {
        return new EvdevCapsLockBackend(parent);
    }
#endif

    // 3. 没有可用的纯 C++ 后端
    return nullptr;
}

} // namespace impress
