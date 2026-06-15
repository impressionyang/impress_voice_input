#include "caps_lock_evdev.h"
#include "utils/logger.h"

#include <QSocketNotifier>
#include <QTimer>
#include <QDir>
#include <QFile>

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <linux/uinput.h>

static const char* const kTag = "EvdevCapsLockBackend";

namespace impress {

// Qt::Key → Linux evdev keycode 映射
// 只映射常用键（字母、数字、功能键等）
static int qtKeyToEvdevCode(int key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return KEY_A + (key - Qt::Key_A);
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return KEY_0 + (key - Qt::Key_0);
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return KEY_F1 + (key - Qt::Key_F1);
    }
    if (key >= Qt::Key_F13 && key <= Qt::Key_F24) {
        return KEY_F13 + (key - Qt::Key_F13);
    }
    switch (key) {
    case Qt::Key_Space:     return KEY_SPACE;
    case Qt::Key_Return:
    case Qt::Key_Enter:     return KEY_ENTER;
    case Qt::Key_Escape:    return KEY_ESC;
    case Qt::Key_Tab:       return KEY_TAB;
    case Qt::Key_Backspace: return KEY_BACKSPACE;
    case Qt::Key_Delete:    return KEY_DELETE;
    case Qt::Key_Insert:    return KEY_INSERT;
    case Qt::Key_Home:      return KEY_HOME;
    case Qt::Key_End:       return KEY_END;
    case Qt::Key_PageUp:    return KEY_PAGEUP;
    case Qt::Key_PageDown:  return KEY_PAGEDOWN;
    case Qt::Key_Up:        return KEY_UP;
    case Qt::Key_Down:      return KEY_DOWN;
    case Qt::Key_Left:      return KEY_LEFT;
    case Qt::Key_Right:     return KEY_RIGHT;
    case Qt::Key_Comma:     return KEY_COMMA;
    case Qt::Key_Period:    return KEY_DOT;
    case Qt::Key_Slash:     return KEY_SLASH;
    case Qt::Key_Semicolon: return KEY_SEMICOLON;
    case Qt::Key_BracketLeft: return KEY_LEFTBRACE;
    case Qt::Key_BracketRight: return KEY_RIGHTBRACE;
    case Qt::Key_Apostrophe: return KEY_APOSTROPHE;
    case Qt::Key_Minus:     return KEY_MINUS;
    case Qt::Key_Equal:     return KEY_EQUAL;
    case Qt::Key_Backslash: return KEY_BACKSLASH;
    default:                return 0;
    }
}

struct EvdevCapsLockBackend::Impl {
    struct KeyboardDevice {
        int fd = -1;
        QString path;
        bool grabbed = false;
    };

    int uinputFd = -1;
    QList<KeyboardDevice> keyboards;
    QTimer* retryTimer = nullptr;
    int retryCount = 0;
    static constexpr int MaxRetries = 3;

    // 当前修饰键状态（跟踪所有键盘的修饰键）
    bool ctrlPressed = false;
    bool altPressed = false;
    bool shiftPressed = false;
    bool metaPressed = false;

    // 快捷键配置
    int targetKeycode = 0;      // evdev keycode
    int targetModifiers = 0;    // Qt::KeyboardModifiers

    /** 判断修饰键 */
    static bool isModifierKey(uint16_t code) {
        return code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL ||
               code == KEY_LEFTALT || code == KEY_RIGHTALT ||
               code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT ||
               code == KEY_LEFTMETA || code == KEY_RIGHTMETA;
    }

    /** 获取当前合成修饰符状态 */
    int currentModifiers() const {
        int mods = 0;
        if (ctrlPressed) mods |= Qt::ControlModifier;
        if (altPressed) mods |= Qt::AltModifier;
        if (shiftPressed) mods |= Qt::ShiftModifier;
        if (metaPressed) mods |= Qt::MetaModifier;
        return mods;
    }

    /** 判断是否为快捷键组合 */
    bool isHotkeyCombo(uint16_t code) const {
        if (code != (uint16_t)targetKeycode) return false;
        return currentModifiers() == targetModifiers;
    }

    /** 判断设备是否为键盘 */
    static bool isKeyboard(int fd) {
        static constexpr size_t kEvdevBitArraySize = 64;
        unsigned long evbits[kEvdevBitArraySize] = {};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits) < 0)
            return false;

        static constexpr size_t kEvKeyIdx = EV_KEY / (sizeof(unsigned long) * 8);
        static constexpr int kEvKeyBit = EV_KEY % (sizeof(unsigned long) * 8);
        if (!(evbits[kEvKeyIdx] & (1ULL << kEvKeyBit)))
            return false;

        bool hasAlpha = false;
        for (int code = KEY_Q; code <= KEY_P; code++) {
            unsigned long keybits[KEY_CNT / (sizeof(unsigned long) * 8)] = {};
            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) >= 0) {
                if (keybits[code / (sizeof(unsigned long) * 8)] & (1ULL << (code % (sizeof(unsigned long) * 8)))) {
                    hasAlpha = true;
                    break;
                }
            }
        }
        return hasAlpha;
    }

    /** 查找所有键盘设备 */
    QList<KeyboardDevice> findKeyboards() {
        QList<KeyboardDevice> result;
        QDir inputDir("/dev/input");
        QStringList filters = {"event*"};
        QFileInfoList files = inputDir.entryInfoList(filters, QDir::System | QDir::Readable);

        for (const QFileInfo& fi : files) {
            QString path = fi.absoluteFilePath();
            int fd = open(qPrintable(path), O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;

            if (isKeyboard(fd)) {
                KeyboardDevice dev;
                dev.fd = fd;
                dev.path = path;
                result.append(dev);
                LOG_DEBUG(kTag, QString("发现键盘: %1").arg(path));
            } else {
                close(fd);
            }
        }
        return result;
    }

    /** 创建 uinput 虚拟设备 */
    bool createUinputDevice() {
        uinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (uinputFd < 0) {
            LOG_ERROR(kTag, "无法打开 /dev/uinput，需要 input 组或 udev 规则");
            return false;
        }

        struct uinput_setup setup = {};
        memset(&setup, 0, sizeof(setup));
        strncpy(setup.name, "Impress Voice Input Virtual Keyboard", UINPUT_MAX_NAME_SIZE - 1);
        setup.id.bustype = BUS_VIRTUAL;
        setup.id.vendor = 0x1234;
        setup.id.product = 0x5678;
        setup.id.version = 1;

        ioctl(uinputFd, UI_SET_EVBIT, EV_KEY);
        for (int code = 0; code < KEY_CNT; code++) {
            ioctl(uinputFd, UI_SET_KEYBIT, code);
        }

        if (ioctl(uinputFd, UI_DEV_SETUP, &setup) < 0) {
            LOG_ERROR(kTag, "UI_DEV_SETUP 失败");
            close(uinputFd);
            uinputFd = -1;
            return false;
        }

        if (ioctl(uinputFd, UI_DEV_CREATE) < 0) {
            LOG_ERROR(kTag, "UI_DEV_CREATE 失败");
            close(uinputFd);
            uinputFd = -1;
            return false;
        }

        LOG_INFO(kTag, "uinput 虚拟键盘已创建");
        return true;
    }

    /** 抓取键盘设备 */
    bool grabAllKeyboards() {
        for (auto& dev : keyboards) {
            struct input_event ev;
            while (read(dev.fd, &ev, sizeof(ev)) > 0) {}

            unsigned char keyState[KEY_MAX / 8 + 1];
            int attempts = 0;
            while (attempts < 50) {
                memset(keyState, 0, sizeof(keyState));
                if (ioctl(dev.fd, EVIOCGKEY(sizeof(keyState)), keyState) >= 0) {
                    bool allReleased = true;
                    for (size_t i = 0; i < sizeof(keyState); i++) {
                        if (keyState[i] != 0) {
                            allReleased = false;
                            break;
                        }
                    }
                    if (allReleased) break;
                }
                usleep(50000);
                attempts++;
            }

            if (ioctl(dev.fd, EVIOCGRAB, 1) == 0) {
                dev.grabbed = true;
                LOG_INFO(kTag, QString("已抓取: %1").arg(dev.path));
            } else {
                LOG_WARNING(kTag, QString("抓取失败: %1").arg(dev.path));
            }
        }
        return true;
    }

    /** 释放所有键盘抓取 */
    void ungrabAllKeyboards() {
        for (auto& dev : keyboards) {
            if (dev.grabbed) {
                ioctl(dev.fd, EVIOCGRAB, 0);
                dev.grabbed = false;
            }
        }
    }

    /** 重放事件到 uinput */
    void replayEvent(uint16_t type, uint16_t code, int32_t value) {
        if (uinputFd < 0) return;
        struct input_event ev;
        memset(&ev, 0, sizeof(ev));
        gettimeofday(&ev.time, nullptr);
        ev.type = type;
        ev.code = code;
        ev.value = value;
        write(uinputFd, &ev, sizeof(ev));
    }

    /** 处理单个键盘的事件 */
    void handleKeyboardEvents(KeyboardDevice& dev, QObject* parent) {
        struct input_event ev;
        while (true) {
            ssize_t n = read(dev.fd, &ev, sizeof(ev));
            if (n < 0) break;

            if (ev.type != EV_KEY) {
                // 非按键事件，重放
                replayEvent(ev.type, ev.code, ev.value);
                continue;
            }

            // value: 1=按下, 0=松开, 2=自动重复
            if (ev.value == 2) {
                replayEvent(ev.type, ev.code, ev.value);
                continue;
            }

            // 更新修饰键状态
            switch (ev.code) {
            case KEY_LEFTCTRL:
            case KEY_RIGHTCTRL:
                ctrlPressed = (ev.value == 1);
                break;
            case KEY_LEFTALT:
            case KEY_RIGHTALT:
                altPressed = (ev.value == 1);
                break;
            case KEY_LEFTSHIFT:
            case KEY_RIGHTSHIFT:
                shiftPressed = (ev.value == 1);
                break;
            case KEY_LEFTMETA:
            case KEY_RIGHTMETA:
                metaPressed = (ev.value == 1);
                break;
            }

            // 判断是否为配置的快捷键
            if (isHotkeyCombo(ev.code)) {
                // 快捷键组合 — 不重放（抑制）
                if (ev.value == 1) {
                    LOG_DEBUG(kTag, "快捷键按下 (evdev)");
                    QMetaObject::invokeMethod(parent, "emitPressed", Qt::QueuedConnection);
                } else {
                    LOG_DEBUG(kTag, "快捷键松开 (evdev)");
                    QMetaObject::invokeMethod(parent, "emitReleased", Qt::QueuedConnection);
                }
            } else if (isModifierKey(ev.code)) {
                // 单独的修饰键 — 正常重放
                replayEvent(ev.type, ev.code, ev.value);
            } else {
                // 其他按键 — 重放到虚拟设备
                replayEvent(ev.type, ev.code, ev.value);
            }
        }
    }
};

EvdevCapsLockBackend::EvdevCapsLockBackend(QObject* parent)
    : CapsLockBackend(parent)
    , m_impl(std::make_unique<Impl>())
{}

EvdevCapsLockBackend::~EvdevCapsLockBackend() {
    stop();
}

void EvdevCapsLockBackend::setHotkeyCombo(int modifiers, int key) {
    CapsLockBackend::setHotkeyCombo(modifiers, key);

    int evdevCode = qtKeyToEvdevCode(key);
    m_impl->targetKeycode = evdevCode;
    m_impl->targetModifiers = modifiers;

    if (m_active) {
        stop();
        start();
    }
}

bool EvdevCapsLockBackend::start() {
    if (m_active) return true;

    // 检查快捷键配置
    int evdevCode = qtKeyToEvdevCode(m_key);
    if (evdevCode == 0) {
        LOG_ERROR(kTag, QString("不支持的按键 (key=0x%1)").arg(m_key, 0, 16));
        emit error("不支持的按键，请使用字母、数字或功能键");
        return false;
    }
    m_impl->targetKeycode = evdevCode;
    m_impl->targetModifiers = m_modifiers;

    LOG_INFO(kTag, QString("快捷键配置: keycode=%1, modifiers=0x%2").arg(evdevCode).arg(m_modifiers, 0, 16));

    // 检查 /dev/uinput 权限
    if (access("/dev/uinput", W_OK) != 0) {
        LOG_ERROR(kTag, "/dev/uinput 不可写。请执行以下操作之一:\n"
            "  1. sudo usermod -aG input $USER（然后重新登录）\n"
            "  2. 创建 /etc/udev/rules.d/99-impress-keyboard.rules:\n"
            "     KERNEL==\"uinput\", MODE=\"0660\", GROUP=\"input\"\n"
            "     KERNEL==\"event*\", SUBSYSTEM==\"input\", MODE=\"0660\", GROUP=\"input\"");
        emit error("/dev/uinput 权限不足，请配置 evdev 访问权限");
        return false;
    }

    // 查找键盘
    m_impl->keyboards = m_impl->findKeyboards();
    if (m_impl->keyboards.isEmpty()) {
        LOG_ERROR(kTag, "未找到键盘设备");
        return false;
    }

    LOG_INFO(kTag, QString("找到 %1 个键盘设备").arg(m_impl->keyboards.size()));

    // 创建 uinput 虚拟设备
    if (!m_impl->createUinputDevice()) {
        return false;
    }

    // 抓取所有键盘
    m_impl->grabAllKeyboards();

    // 为每个键盘创建 QSocketNotifier
    for (auto& dev : m_impl->keyboards) {
        if (!dev.grabbed) continue;
        auto* notifier = new QSocketNotifier(dev.fd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, [this, &dev]() {
            m_impl->handleKeyboardEvents(dev, this);
        });
    }

    m_active = true;
    emit ready();
    LOG_INFO(kTag, "全局快捷键已注册（evdev）");
    return true;
}

void EvdevCapsLockBackend::stop() {
    if (!m_active) return;

    m_impl->ungrabAllKeyboards();

    if (m_impl->uinputFd >= 0) {
        ioctl(m_impl->uinputFd, UI_DEV_DESTROY);
        close(m_impl->uinputFd);
        m_impl->uinputFd = -1;
    }

    for (auto& dev : m_impl->keyboards) {
        if (dev.fd >= 0) {
            close(dev.fd);
            dev.fd = -1;
        }
    }
    m_impl->keyboards.clear();
    m_impl->ctrlPressed = false;
    m_impl->altPressed = false;
    m_impl->shiftPressed = false;
    m_impl->metaPressed = false;

    m_active = false;
    LOG_INFO(kTag, "全局快捷键已注销（evdev）");
}

} // namespace impress
