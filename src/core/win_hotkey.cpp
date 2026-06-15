#include "win_hotkey.h"
#include "utils/logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QThread>
#include <QWidget>
#include <QTimer>
#endif

static const char* const kTag = "CapsLockVoiceHotkey";

namespace impress {

struct CapsLockVoiceHotkey::Impl {
#ifdef Q_OS_WIN
    int hotkeyId = 0;
    QTimer* keyUpTimer = nullptr;      // 检测松开的轮询定时器
    QWidget* hiddenWindow = nullptr;
    void* nativeEventFilter = nullptr;
    bool isHolding = false;
    bool pollThreadRunning = false;
#endif
};

#ifdef Q_OS_WIN
/** Native event filter to catch WM_HOTKEY */
class HotkeyNativeEventFilter : public QAbstractNativeEventFilter {
public:
    explicit HotkeyNativeEventFilter(CapsLockVoiceHotkey* hotkey)
        : hotkey_(hotkey) {}

    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           qintptr* /*result*/) override {
        if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
            auto* msg = static_cast<MSG*>(message);
            if (msg->message == WM_HOTKEY) {
                if (hotkey_) {
                    hotkey_->onHotkeyEvent(static_cast<int>(msg->wParam));
                }
                return true;
            }
        }
        return false;
    }

private:
    CapsLockVoiceHotkey* hotkey_ = nullptr;
};
#endif

/** Qt::KeyboardModifiers → Win32 MOD_* */
static int qtModsToWin32(int qtMods) {
    int mods = 0;
    if (qtMods & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (qtMods & Qt::AltModifier) mods |= MOD_ALT;
    if (qtMods & Qt::ShiftModifier) mods |= MOD_SHIFT;
    if (qtMods & Qt::MetaModifier) mods |= MOD_WIN;
    return mods;
}

/** Qt::Key → Win32 virtual key code */
static int qtKeyToWin32(int key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return 'A' + (key - Qt::Key_A);
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return '0' + (key - Qt::Key_0);
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return VK_F1 + (key - Qt::Key_F1);
    }
    switch (key) {
    case Qt::Key_Space:     return VK_SPACE;
    case Qt::Key_Return:
    case Qt::Key_Enter:     return VK_RETURN;
    case Qt::Key_Escape:    return VK_ESCAPE;
    case Qt::Key_Tab:       return VK_TAB;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Delete:    return VK_DELETE;
    case Qt::Key_Insert:    return VK_INSERT;
    case Qt::Key_Home:      return VK_HOME;
    case Qt::Key_End:       return VK_END;
    case Qt::Key_PageUp:    return VK_PRIOR;
    case Qt::Key_PageDown:  return VK_NEXT;
    case Qt::Key_Up:        return VK_UP;
    case Qt::Key_Down:      return VK_DOWN;
    case Qt::Key_Left:      return VK_LEFT;
    case Qt::Key_Right:     return VK_RIGHT;
    case Qt::Key_CapsLock:  return VK_CAPITAL;
    default:                return 0;
    }
}

CapsLockVoiceHotkey::CapsLockVoiceHotkey(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{
    hotkeyCombo_ = "Ctrl+Alt+C";
}

CapsLockVoiceHotkey::~CapsLockVoiceHotkey() {
    stop();
}

void CapsLockVoiceHotkey::setHotkeyCombo(const QString& combo) {
    hotkeyCombo_ = combo;

    int mods = 0, key = 0;
    QStringList parts = combo.split('+', Qt::SkipEmptyParts);

    // 解析修饰键（除了最后一个部分）
    for (int i = 0; i < parts.size() - 1; i++) {
        QString part = parts[i].trimmed().toLower();
        if (part == "ctrl" || part == "control") mods |= Qt::ControlModifier;
        else if (part == "alt") mods |= Qt::AltModifier;
        else if (part == "shift") mods |= Qt::ShiftModifier;
        else if (part == "meta" || part == "super" || part == "win") mods |= Qt::MetaModifier;
    }

    // 解析主键（最后一个部分）
    QString mainKey = parts.last().trimmed();
    if (mainKey.length() == 1 && mainKey[0].isLetter()) {
        key = Qt::Key_A + (mainKey[0].toUpper().unicode() - 'A');
    } else if (mainKey.length() == 1 && mainKey[0].isDigit()) {
        key = Qt::Key_0 + (mainKey[0].unicode() - '0');
    } else {
        // 特殊按键映射
        QString mk = mainKey.toLower();
        if (mk == "capslock") key = Qt::Key_CapsLock;
        else if (mk == "space") key = Qt::Key_Space;
        else if (mk == "enter" || mk == "return") key = Qt::Key_Return;
        else if (mk == "esc") key = Qt::Key_Escape;
        else if (mk == "tab") key = Qt::Key_Tab;
        else if (mk == "backspace") key = Qt::Key_Backspace;
        else if (mk == "delete") key = Qt::Key_Delete;
        else if (mk == "insert") key = Qt::Key_Insert;
        else if (mk == "home") key = Qt::Key_Home;
        else if (mk == "end") key = Qt::Key_End;
        else if (mk == "pageup") key = Qt::Key_PageUp;
        else if (mk == "pagedown") key = Qt::Key_PageDown;
        else if (mk == "up") key = Qt::Key_Up;
        else if (mk == "down") key = Qt::Key_Down;
        else if (mk == "left") key = Qt::Key_Left;
        else if (mk == "right") key = Qt::Key_Right;
        else if (mk.startsWith('f') && mk.length() >= 2) {
            int num = mk.mid(1).toInt();
            if (num >= 1 && num <= 12) key = Qt::Key_F1 + (num - 1);
        }
    }

    if (key == 0) {
        LOG_ERROR(kTag, QString("无法解析快捷键组合: %1").arg(combo));
        emit error(QString("无效的快捷键: %1").arg(combo));
        return;
    }

    modifiers_ = qtModsToWin32(mods);
    vkKey_ = qtKeyToWin32(key);

    LOG_INFO(kTag, QString("快捷键已更新: %1 (mods=%2, vk=0x%3)").arg(combo).arg(modifiers_).arg(vkKey_, 0, 16));

    if (active_) {
        unregisterHotkey();
        registerHotkey();
    }
}

bool CapsLockVoiceHotkey::start() {
    if (active_) return true;

#ifdef Q_OS_WIN
    registerHotkey();

    active_ = true;
    emit ready();
    LOG_INFO(kTag, QString("快捷键已注册（%1）").arg(hotkeyCombo_));
    return true;
#else
    emit error("CapsLockVoiceHotkey 仅支持 Windows 平台");
    return false;
#endif
}

void CapsLockVoiceHotkey::stop() {
    if (!active_) return;

#ifdef Q_OS_WIN
    impl_->pollThreadRunning = false;
    unregisterHotkey();

    active_ = false;
    recording_ = false;
    impl_->isHolding = false;
    LOG_INFO(kTag, "快捷键已停止");
#endif
}

#ifdef Q_OS_WIN
void CapsLockVoiceHotkey::registerHotkey() {
    if (!impl_->hiddenWindow) {
        impl_->hiddenWindow = new QWidget();
        impl_->hiddenWindow->setObjectName("HotkeyReceiver");
        impl_->hiddenWindow->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
        impl_->hiddenWindow->resize(1, 1);
        impl_->hiddenWindow->move(-10000, -10000);  // 移到屏幕外
    }

    // 通过 winId() 创建原生窗口（不显示）
    WId wid = impl_->hiddenWindow->winId();
    HWND hwnd = reinterpret_cast<HWND>(wid);
    if (!hwnd) {
        emit error("无法创建窗口句柄");
        return;
    }

    impl_->hotkeyId = GlobalAddAtom(L"ImpressVoiceHotkey");

    // MOD_NOREPEAT 防止按住时重复触发 WM_HOTKEY
    BOOL ok = RegisterHotKey(hwnd, impl_->hotkeyId, modifiers_ | MOD_NOREPEAT, vkKey_);
    if (!ok) {
        DWORD err = GetLastError();
        emit error(QString("注册快捷键 %1 失败 (错误码: %2)").arg(hotkeyCombo_).arg(err));
        LOG_ERROR(kTag, QString("RegisterHotKey failed: %1").arg(err));
        return;
    }

    LOG_DEBUG(kTag, QString("热键窗口已注册 (HWND=%1)").arg((qulonglong)hwnd));

    auto* filter = new HotkeyNativeEventFilter(this);
    QGuiApplication::instance()->installNativeEventFilter(filter);
    impl_->nativeEventFilter = filter;

    // keyUpTimer 用于检测松开
    impl_->keyUpTimer = new QTimer(this);
    impl_->keyUpTimer->setSingleShot(true);
    impl_->keyUpTimer->setInterval(500);  // 最长等待 500ms
}

void CapsLockVoiceHotkey::unregisterHotkey() {
    if (impl_->keyUpTimer) {
        impl_->keyUpTimer->stop();
    }

    if (impl_->nativeEventFilter) {
        auto* filter = static_cast<HotkeyNativeEventFilter*>(impl_->nativeEventFilter);
        QGuiApplication::instance()->removeNativeEventFilter(filter);
        delete filter;
        impl_->nativeEventFilter = nullptr;
    }

    if (impl_->hiddenWindow) {
        WId wid = impl_->hiddenWindow->winId();
        HWND hwnd = reinterpret_cast<HWND>(wid);
        if (hwnd && impl_->hotkeyId) {
            UnregisterHotKey(hwnd, impl_->hotkeyId);
            GlobalDeleteAtom(impl_->hotkeyId);
            impl_->hotkeyId = 0;
        }
        impl_->hiddenWindow->hide();
    }
}
#endif

void CapsLockVoiceHotkey::toggleRecording() {
    if (!active_ || ignoreEvents_) return;

    if (recording_) {
        recording_ = false;
        emit recordingStopped();
    } else {
        recording_ = true;
        emit recordingStarted();
    }
}

#ifdef Q_OS_WIN
void CapsLockVoiceHotkey::onHotkeyEvent(int /*hotkeyId*/) {
    if (!active_ || impl_->isHolding) return;

    LOG_DEBUG(kTag, "快捷键按下，开始录音");
    recording_ = true;
    impl_->isHolding = true;
    emit recordingStarted();

    // 启动轮询线程检测松开（任何修饰键或主键松开即停止）
    if (impl_->pollThreadRunning) return;
    impl_->pollThreadRunning = true;

    const int vk = vkKey_;
    const int mods = modifiers_;

    QThread::create([this, vk, mods]() {
        // 等待所有相关键松开
        while (impl_->pollThreadRunning && impl_->isHolding) {
            // 检查主键
            SHORT mainState = GetAsyncKeyState(vk);
            bool mainHeld = (mainState & 0x8000) != 0;

            // 检查修饰键
            bool ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool winHeld = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
                           (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

            bool modsHeld = true;
            if (mods & MOD_CONTROL && !ctrlHeld) modsHeld = false;
            if (mods & MOD_ALT && !altHeld) modsHeld = false;
            if (mods & MOD_SHIFT && !shiftHeld) modsHeld = false;
            if (mods & MOD_WIN && !winHeld) modsHeld = false;

            if (!mainHeld || !modsHeld) {
                // 任一组合键松开
                impl_->isHolding = false;

                QMetaObject::invokeMethod(this, [this]() {
                    if (recording_) {
                        LOG_DEBUG(kTag, "快捷键松开，停止录音");
                        recording_ = false;
                        emit recordingStopped();
                    }
                    impl_->isHolding = false;
                }, Qt::QueuedConnection);
                break;
            }
            QThread::msleep(30);
        }
        impl_->pollThreadRunning = false;
    })->start();
}
#endif

} // namespace impress
