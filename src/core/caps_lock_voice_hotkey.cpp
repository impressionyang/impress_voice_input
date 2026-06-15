#include "caps_lock_voice_hotkey.h"
#include "caps_lock_backend.h"
#include "utils/logger.h"

#include <QSocketNotifier>
#include <QProcessEnvironment>

#ifdef HAVE_LIBEI
extern "C" {
#include <libei.h>
}
#endif

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QUuid>

static const char* const kTag = "CapsLockVoiceHotkey";

// Portal 常量
static const char* const kPortalService = "org.freedesktop.portal.Desktop";
static const char* const kPortalObjectPath = "/org/freedesktop/portal/desktop";
static const char* const kGlobalShortcutsIface = "org.freedesktop.portal.GlobalShortcuts";
static const char* const kRequestIface = "org.freedesktop.portal.Request";

// GNOME 扩展 D-Bus 接口
static const char* const kGnomeHotkeyService = "io.impress.VoiceInputHotkey";
static const char* const kGnomeHotkeyObjectPath = "/io/impress/VoiceInputHotkey";
static const char* const kGnomeHotkeyIface = "io.impress.VoiceInputHotkey";

namespace impress {

struct CapsLockVoiceHotkey::Impl {
    // Portal 相关
    QString sessionPath;
    QString pendingRequestPath;

    enum State { Idle, WaitingSession, WaitingBind, Active, GnomeActive, LibeiActive };
    State state = Idle;

    // 快捷键配置
    int modifiers = 0;
    int key = 0;

    // libei 相关
#ifdef HAVE_LIBEI
    struct ei* eiCtx = nullptr;
    QSocketNotifier* socketNotifier = nullptr;
#endif

    /** 生成唯一 token */
    static QString makeToken(const QString& prefix) {
        return prefix + "_" + QUuid::createUuid().toString().mid(1, 8);
    }

    /** 构造 session path（从 sender 名和 token） */
    static QString makeSessionPath(const QString& sender, const QString& token) {
        QString safeSender = sender;
        safeSender.remove(0, 1);  // 去掉前导 ':'
        safeSender.replace('.', '_');
        return QString("/org/freedesktop/portal/desktop/session/%1/%2")
            .arg(safeSender, token);
    }

    /** 获取 session bus */
    static QDBusConnection bus() {
        return QDBusConnection::sessionBus();
    }
};

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

    // 如果原生后端已注册，更新它
    if (nativeBackend_) {
        int mods, key;
        if (parseHotkeyCombo(combo, mods, key)) {
            impl_->modifiers = mods;
            impl_->key = key;
            nativeBackend_->setHotkeyCombo(mods, key);
        }
    }
}

bool CapsLockVoiceHotkey::start() {
    if (active_ || pending_) return true;

    // 解析快捷键组合
    if (!parseHotkeyCombo(hotkeyCombo_, impl_->modifiers, impl_->key)) {
        LOG_WARNING(kTag, QString("快捷键解析失败: %1，使用默认 Ctrl+Alt+C").arg(hotkeyCombo_));
        parseHotkeyCombo("Ctrl+Alt+C", impl_->modifiers, impl_->key);
    }

    LOG_INFO(kTag, QString("快捷键配置: %1 (modifiers=0x%2, key=0x%3)")
        .arg(hotkeyCombo_).arg(impl_->modifiers, 0, 16).arg(impl_->key, 0, 16));

    // 打印环境信息
    const char* sessionType = getenv("XDG_SESSION_TYPE");
    LOG_INFO(kTag, QString("当前会话类型: %1").arg(sessionType ? sessionType : "(未知)"));

    // 1. 尝试纯 C++ 原生后端（X11 XGrabKey / evdev）
    nativeBackend_ = std::unique_ptr<CapsLockBackend>(createCapsLockBackend(this));
    if (nativeBackend_) {
        LOG_INFO(kTag, QString("尝试原生后端: %1").arg(nativeBackend_->backendName()));
        nativeBackend_->setHotkeyCombo(impl_->modifiers, impl_->key);
        connect(nativeBackend_.get(), &CapsLockBackend::pressed,
                this, &CapsLockVoiceHotkey::onNativePressed);
        connect(nativeBackend_.get(), &CapsLockBackend::ready,
                this, &CapsLockVoiceHotkey::ready);
        connect(nativeBackend_.get(), &CapsLockBackend::error,
                this, &CapsLockVoiceHotkey::error);

        if (nativeBackend_->start()) {
            active_ = true;
            LOG_INFO(kTag, QString("✓ 快捷键已就绪（%1）")
                .arg(nativeBackend_->backendName()));
            return true;
        }
        nativeBackend_.reset();
    } else {
        LOG_INFO(kTag, "原生后端不可用: X11 下需 XCB 支持；Wayland 下需配置 evdev 权限");
        LOG_INFO(kTag, "  Wayland 权限配置: sudo usermod -aG input $USER");
        LOG_INFO(kTag, "  或创建 udev 规则: /etc/udev/rules.d/99-impress-keyboard.rules");
    }

    // 2. 尝试 GNOME 扩展后端
    LOG_INFO(kTag, "尝试 GNOME 扩展后端...");
    LOG_INFO(kTag, "  需安装: gnome-extension/io.impress.voice-input-hotkey@impress/");
    LOG_INFO(kTag, "  安装到 ~/.local/share/gnome-shell/extensions/ 并重新登录");
    if (startGnomeExtension()) {
        LOG_INFO(kTag, "✓ GNOME 扩展快捷键已就绪");
        return true;
    }
    LOG_INFO(kTag, "GNOME 扩展不可用: 扩展未安装或 GNOME Shell 未重启");

#ifdef HAVE_LIBEI
    // 3. 尝试 libei（GNOME 47+/KDE）
    LOG_INFO(kTag, "尝试 libei 后端...");
    LOG_INFO(kTag, "  需 EIS socket 可用（KDE 原生支持，GNOME 不暴露 EIS socket）");
    startLibei();
    if (active_) {
        LOG_INFO(kTag, "✓ libei 快捷键已就绪");
        return true;
    }
    LOG_INFO(kTag, "libei 不可用: EIS socket 不可访问");
#endif

    // 4. 回退到 Portal（KDE 等支持 GlobalShortcuts 的桌面）
    LOG_INFO(kTag, "尝试 Portal GlobalShortcuts 后端...");
    LOG_INFO(kTag, "  需设置 XDG_DESKTOP_PORTAL_APP_ID 环境变量");
    return startPortal();
}

void CapsLockVoiceHotkey::stop() {
    if (!active_ && !pending_ && impl_->state == Impl::Idle) return;

    if (nativeBackend_) {
        nativeBackend_->stop();
        nativeBackend_.reset();
    }
    stopGnomeExtension();
#ifdef HAVE_LIBEI
    stopLibei();
#endif
    stopPortal();

    active_ = false;
    pending_ = false;
    recording_ = false;
    impl_->state = Impl::Idle;
    impl_->sessionPath.clear();
    LOG_INFO(kTag, "语音快捷键已停止");
}

/**
 * @brief 切换模式：按下快捷键时在录音/停止之间切换
 */
void CapsLockVoiceHotkey::toggleRecording() {
    if (!active_ || ignoreEvents_) return;

    if (recording_) {
        // 正在录音 → 停止
        recording_ = false;
        emit recordingStopped();
    } else {
        // 空闲 → 开始录音
        recording_ = true;
        emit recordingStarted();
    }
}

// ============================================================================
// GNOME 扩展后端（通过 D-Bus 信号监听快捷键）
// ============================================================================

bool CapsLockVoiceHotkey::startGnomeExtension() {
    QDBusConnection bus = Impl::bus();
    if (!bus.isConnected()) return false;

    // 探测 GNOME 扩展的 D-Bus 服务是否在线
    QDBusInterface probe(kGnomeHotkeyService, kGnomeHotkeyObjectPath,
                         kGnomeHotkeyIface, bus);
    if (!probe.isValid()) {
        LOG_DEBUG(kTag, "GNOME 扩展 D-Bus 服务不可用");
        return false;
    }

    // 连接快捷键按下信号（GNOME 扩展目前只支持 CapsLock 检测）
    bool okPressed = bus.connect(kGnomeHotkeyService, kGnomeHotkeyObjectPath,
        kGnomeHotkeyIface, "CapsLockPressed",
        this, SLOT(onGnomeCapsLockPressed()));

    if (!okPressed) {
        LOG_ERROR(kTag, "GNOME 扩展 D-Bus 信号连接失败");
        return false;
    }

    active_ = true;
    impl_->state = Impl::GnomeActive;
    emit ready();
    LOG_INFO(kTag, "语音快捷键已就绪（GNOME 扩展）");
    return true;
}

void CapsLockVoiceHotkey::stopGnomeExtension() {
    QDBusConnection bus = Impl::bus();
    bus.disconnect(kGnomeHotkeyService, kGnomeHotkeyObjectPath,
        kGnomeHotkeyIface, "CapsLockPressed",
        this, SLOT(onGnomeCapsLockPressed()));

    if (impl_->state == Impl::GnomeActive) {
        impl_->state = Impl::Idle;
    }
}

void CapsLockVoiceHotkey::onGnomeCapsLockPressed() {
    LOG_DEBUG(kTag, "GNOME 扩展: 快捷键触发");
    toggleRecording();
}

// 原生后端（X11 / evdev）事件处理
void CapsLockVoiceHotkey::onNativePressed() {
    LOG_DEBUG(kTag, "原生后端: 快捷键触发");
    toggleRecording();
}

// ============================================================================
// Portal 后端（KDE 等支持 GlobalShortcuts 的桌面）
// ============================================================================

bool CapsLockVoiceHotkey::startPortal() {
    QDBusConnection bus = Impl::bus();
    if (!bus.isConnected()) {
        LOG_ERROR(kTag, "无法连接到 D-Bus session bus");
        emit error("无法连接到 D-Bus session bus");
        return false;
    }

    // 连接信号
    bus.connect(kPortalService, kPortalObjectPath,
        kGlobalShortcutsIface, "Activated",
        this, SLOT(handleActivated(QString)));

    // 连接 Response 信号
    bus.connect(kPortalService, QString(),
        kRequestIface, "Response",
        this, SLOT(onPortalResponse(uint, QVariantMap)));

    // 发送 CreateSession
    QDBusInterface portal(kPortalService, kPortalObjectPath,
        kGlobalShortcutsIface, bus);

    QString sessionToken = Impl::makeToken("io_impress_sess");
    QString requestToken = Impl::makeToken("io_impress_req");

    QVariantMap options;
    options["handle_token"] = requestToken;
    options["session_handle_token"] = sessionToken;
    options["app_id"] = "io.impress.voice-input";

    QDBusMessage reply = portal.call("CreateSession", options);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        emit error(QString("CreateSession 失败: %1").arg(reply.errorMessage()));
        LOG_ERROR(kTag, reply.errorMessage());
        impl_->state = Impl::Idle;
        return false;
    }

    // CreateSession 已接受，进入等待授权状态
    QString sender = bus.baseService();
    impl_->sessionPath = Impl::makeSessionPath(sender, sessionToken);
    impl_->state = Impl::WaitingSession;
    pending_ = true;

    LOG_INFO(kTag, "CreateSession 已发送，等待 Portal 响应...");
    LOG_DEBUG(kTag, QString("Session path: %1").arg(impl_->sessionPath));
    return true;
}

void CapsLockVoiceHotkey::stopPortal() {
    QDBusConnection bus = Impl::bus();
    bus.disconnect(kPortalService, kPortalObjectPath,
        kGlobalShortcutsIface, "Activated",
        this, SLOT(handleActivated(QString)));
    bus.disconnect(kPortalService, QString(),
        kRequestIface, "Response",
        this, SLOT(onPortalResponse(uint, QVariantMap)));
}

void CapsLockVoiceHotkey::onPortalResponse(uint response, const QVariantMap& results) {
    if (impl_->state == Impl::WaitingSession) {
        handleSessionResponse(response, results);
    } else if (impl_->state == Impl::WaitingBind) {
        handleBindResponse(response, results);
    }
}

void CapsLockVoiceHotkey::handleSessionResponse(uint response, const QVariantMap& results) {
    if (impl_->state != Impl::WaitingSession) return;

    if (response != 0) {
        pending_ = false;
        emit error(QString("Session 被拒绝 (response=%1)").arg(response));
        LOG_ERROR(kTag, QString("Session 被拒绝: %1").arg(response));
        impl_->state = Impl::Idle;
        return;
    }

    QString actualPath = results.value("session_handle").toString();
    if (!actualPath.isEmpty()) {
        impl_->sessionPath = actualPath;
    }
    LOG_INFO(kTag, QString("Portal Session 已授权: %1").arg(impl_->sessionPath));

    // 发送 BindShortcuts
    impl_->state = Impl::WaitingBind;

    QDBusInterface portal(kPortalService, kPortalObjectPath,
        kGlobalShortcutsIface, Impl::bus());

    QString bindToken = Impl::makeToken("io_impress_bind");

    QVariantMap shortcutProps;
    shortcutProps["description"] = QString("语音输入（%1）").arg(hotkeyCombo_);

    QList<QVariant> shortcuts;
    QVariantMap shortcutEntry;
    shortcutEntry["id"] = "voice_input";
    shortcutEntry["properties"] = shortcutProps;
    shortcuts.append(shortcutEntry);

    QVariantMap bindOptions;
    bindOptions["handle_token"] = bindToken;

    QDBusMessage reply = portal.call("BindShortcuts",
        QDBusObjectPath(impl_->sessionPath),
        shortcuts,
        QString(),  // parent_window (空 = Wayland 模式)
        bindOptions);

    if (reply.type() == QDBusMessage::ErrorMessage) {
        emit error(QString("BindShortcuts 失败: %1").arg(reply.errorMessage()));
        LOG_ERROR(kTag, reply.errorMessage());
        impl_->state = Impl::Idle;
        return;
    }

    impl_->pendingRequestPath = reply.arguments().isEmpty() ?
        QString() : reply.arguments()[0].toString();
    LOG_INFO(kTag, "BindShortcuts 已发送，等待用户设置快捷键...");
}

void CapsLockVoiceHotkey::handleBindResponse(uint response, const QVariantMap&) {
    if (impl_->state != Impl::WaitingBind) return;

    if (response != 0) {
        pending_ = false;
        emit error(QString("快捷键绑定被拒绝 (response=%1)").arg(response));
        LOG_ERROR(kTag, QString("Bind 被拒绝: %1").arg(response));
        impl_->state = Impl::Idle;
        return;
    }

    active_ = true;
    pending_ = false;
    impl_->state = Impl::Active;
    emit ready();
    LOG_INFO(kTag, "快捷键已注册（Portal），语音输入已就绪");
}

void CapsLockVoiceHotkey::handleActivated(const QString& shortcutId) {
    LOG_DEBUG(kTag, QString("快捷键触发: %1").arg(shortcutId));
    toggleRecording();
}

// ============================================================================
// libei 后端（GNOME 47+）
// ============================================================================

#ifdef HAVE_LIBEI

void CapsLockVoiceHotkey::startLibei() {
    impl_->eiCtx = ei_new(this);
    if (!impl_->eiCtx) {
        LOG_ERROR(kTag, "ei_new 失败");
        return;
    }

    ei_configure_name(impl_->eiCtx, "io.impress.voice-input");

    int fd = ei_setup_backend_socket(impl_->eiCtx, nullptr);
    if (fd < 0) {
        LOG_ERROR(kTag, "ei_setup_backend_socket 失败，libei 不可用");
        ei_unref(impl_->eiCtx);
        impl_->eiCtx = nullptr;
        return;
    }

    // 使用 QSocketNotifier 集成到 Qt 事件循环
    impl_->socketNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(impl_->socketNotifier, &QSocketNotifier::activated,
            this, &CapsLockVoiceHotkey::onLibeiEvent);

    LOG_INFO(kTag, "libei 已连接，等待键盘事件...");

    active_ = true;
    impl_->state = Impl::LibeiActive;
    emit ready();
    LOG_INFO(kTag, "语音快捷键已就绪（libei）");
}

void CapsLockVoiceHotkey::stopLibei() {
    if (impl_->socketNotifier) {
        impl_->socketNotifier->setEnabled(false);
        delete impl_->socketNotifier;
        impl_->socketNotifier = nullptr;
    }

    if (impl_->eiCtx) {
        ei_disconnect(impl_->eiCtx);
        ei_unref(impl_->eiCtx);
        impl_->eiCtx = nullptr;
    }
}

void CapsLockVoiceHotkey::onLibeiEvent() {
    if (!impl_->eiCtx) return;

    struct ei_event* ev;
    while ((ev = ei_get_event(impl_->eiCtx)) != nullptr) {
        enum ei_event_type type = ei_event_get_type(ev);

        switch (type) {
        case EI_EVENT_KEYBOARD_KEY: {
            // TODO: 根据 impl_->key 过滤配置的按键
            bool isPress = ei_event_keyboard_get_key_is_press(ev);
            if (isPress) {
                LOG_DEBUG(kTag, "libei: 快捷键触发");
                toggleRecording();
            }
            break;
        }

        case EI_EVENT_DISCONNECT:
            LOG_WARNING(kTag, "libei 连接已断开");
            active_ = false;
            break;

        default:
            break;
        }

        ei_event_unref(ev);
    }
}

#else // HAVE_LIBEI

void CapsLockVoiceHotkey::startLibei() {
    LOG_WARNING(kTag, "libei 未编译启用");
}

void CapsLockVoiceHotkey::stopLibei() {
    // nothing
}

void CapsLockVoiceHotkey::onLibeiEvent() {
    // nothing
}

#endif // HAVE_LIBEI

} // namespace impress
