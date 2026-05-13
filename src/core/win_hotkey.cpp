#include "win_hotkey.h"
#include "utils/logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#endif

static const char* const kTag = "CapsLockVoiceHotkey";

namespace impress {

struct CapsLockVoiceHotkey::Impl {
#ifdef Q_OS_WIN
    int hotkeyId = 0;
    QTimer* longPressTimer = nullptr;
    QTimer* keyUpDebounce = nullptr;
    bool isHolding = false;
    bool longPressFired = false;
    bool pollThreadRunning = false;
    void* nativeEventFilter = nullptr;
    static constexpr int kLongPressMs = 1000;
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

CapsLockVoiceHotkey::CapsLockVoiceHotkey(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{}

CapsLockVoiceHotkey::~CapsLockVoiceHotkey() {
    stop();
}

bool CapsLockVoiceHotkey::start() {
    if (active_) return true;

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(QGuiApplication::instance()->winId());
    if (!hwnd) {
        // Try to get the top-level widget's window handle
        hwnd = GetForegroundWindow();
    }

    if (!hwnd) {
        emit error("无法获取窗口句柄");
        return false;
    }

    // 注册 CapsLock (VK_CAPITAL = 0x14) 全局快捷键
    // MOD_NOREPEAT 防止按住时重复触发
    const int vkCapsLock = 0x14;
    impl_->hotkeyId = GlobalAddAtom(L"ImpressVoiceHotkey");

    BOOL ok = RegisterHotKey(hwnd, impl_->hotkeyId, MOD_NOREPEAT, vkCapsLock);
    if (!ok) {
        DWORD err = GetLastError();
        emit error(QString("注册 CapsLock 快捷键失败 (错误码: %1)").arg(err));
        LOG_ERROR(kTag, QString("RegisterHotKey failed: %1").arg(err));
        return false;
    }

    // 安装原生事件过滤器
    auto* filter = new HotkeyNativeEventFilter(this);
    QGuiApplication::instance()->installNativeEventFilter(filter);
    impl_->nativeEventFilter = filter;

    // 长按定时器
    impl_->longPressTimer = new QTimer(this);
    impl_->longPressTimer->setSingleShot(true);
    impl_->longPressTimer->setInterval(Impl::kLongPressMs);
    connect(impl_->longPressTimer, &QTimer::timeout, this, [this]() {
        if (impl_->isHolding && !impl_->longPressFired) {
            impl_->longPressFired = true;
            recording_ = true;
            emit recordingStarted();
            LOG_DEBUG(kTag, "长按触发，开始录音");
        }
    });

    // 松开后延迟重置，避免 CapsLock 状态闪烁
    impl_->keyUpDebounce = new QTimer(this);
    impl_->keyUpDebounce->setSingleShot(true);
    impl_->keyUpDebounce->setInterval(200);
    connect(impl_->keyUpDebounce, &QTimer::timeout, this, [this]() {
        impl_->isHolding = false;
        impl_->longPressFired = false;
    });

    active_ = true;
    emit ready();
    LOG_INFO(kTag, "CapsLock 快捷键已注册");
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

    if (impl_->longPressTimer) {
        impl_->longPressTimer->stop();
    }
    if (impl_->keyUpDebounce) {
        impl_->keyUpDebounce->stop();
    }

    // 移除原生事件过滤器
    if (impl_->nativeEventFilter) {
        auto* filter = static_cast<HotkeyNativeEventFilter*>(impl_->nativeEventFilter);
        QGuiApplication::instance()->removeNativeEventFilter(filter);
        delete filter;
        impl_->nativeEventFilter = nullptr;
    }

    // 注销快捷键
    HWND hwnd = reinterpret_cast<HWND>(QGuiApplication::instance()->winId());
    if (hwnd && impl_->hotkeyId) {
        UnregisterHotKey(hwnd, impl_->hotkeyId);
        GlobalDeleteAtom(impl_->hotkeyId);
        impl_->hotkeyId = 0;
    }

    active_ = false;
    recording_ = false;
    impl_->isHolding = false;
    impl_->longPressFired = false;
    LOG_INFO(kTag, "CapsLock 快捷键已停止");
#endif
}

#ifdef Q_OS_WIN
void CapsLockVoiceHotkey::onHotkeyEvent(int /*hotkeyId*/) {
    if (!active_) return;

    // Windows 只在按键按下时触发 WM_HOTKEY
    // 我们通过 GetAsyncKeyState 轮询检测松开
    impl_->isHolding = true;
    impl_->longPressFired = false;

    // 启动长按定时器
    if (impl_->longPressTimer) {
        impl_->longPressTimer->start();
    }

    // 启动轮询线程检测松开
    if (impl_->pollThreadRunning) return;
    impl_->pollThreadRunning = true;
    QThread::create([this]() {
        const int vkCapsLock = 0x14;
        while (impl_->pollThreadRunning && impl_->isHolding) {
            SHORT state = GetAsyncKeyState(vkCapsLock);
            if (!(state & 0x8000)) {
                // 按键松开
                impl_->isHolding = false;
                if (impl_->longPressTimer) {
                    impl_->longPressTimer->stop();
                }
                QMetaObject::invokeMethod(this, [this]() {
                    if (impl_->longPressFired) {
                        // 长按 → 停止录音
                        recording_ = false;
                        emit recordingStopped();
                        LOG_DEBUG(kTag, "长按结束，停止录音");
                    } else {
                        // 短按 → 不处理（让系统处理 CapsLock）
                        LOG_DEBUG(kTag, "短按，不拦截 CapsLock");
                    }
                    impl_->longPressFired = false;
                    if (impl_->keyUpDebounce) {
                        impl_->keyUpDebounce->start();
                    }
                }, Qt::QueuedConnection);
                break;
            }
            QThread::msleep(50);
        }
        impl_->pollThreadRunning = false;
    }).start();
}
#endif

} // namespace impress
