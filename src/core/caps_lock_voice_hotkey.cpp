#include "caps_lock_voice_hotkey.h"
#include "utils/logger.h"

#include <QSocketNotifier>

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

// Linux CapsLock keycode (scan code)
static const uint32_t kCapsLockKeycode = 58;

namespace impress {

struct CapsLockVoiceHotkey::Impl {
    // Portal 相关
    QString sessionPath;
    QString pendingRequestPath;

    enum State { Idle, WaitingSession, WaitingBind, Active };
    State state = Idle;

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
{}

CapsLockVoiceHotkey::~CapsLockVoiceHotkey() {
    stop();
}

bool CapsLockVoiceHotkey::start() {
    if (active_) return true;

#ifdef HAVE_LIBEI
    // 优先尝试 libei（GNOME 47+ 推荐方案）
    LOG_INFO(kTag, "尝试 libei 后端...");
    startLibei();
    if (active_) {
        LOG_INFO(kTag, "libei 快捷键已就绪");
        return true;
    }
    LOG_INFO(kTag, "libei 不可用，回退到 Portal...");
#endif

    // 回退到 Portal（KDE 等支持 GlobalShortcuts 的桌面）
    startPortal();
    return active_;
}

void CapsLockVoiceHotkey::stop() {
    if (!active_ && impl_->state == Impl::Idle) return;

#ifdef HAVE_LIBEI
    stopLibei();
#endif
    stopPortal();

    active_ = false;
    recording_ = false;
    impl_->state = Impl::Idle;
    impl_->sessionPath.clear();
    LOG_INFO(kTag, "CapsLock 语音快捷键已停止");
}

// ============================================================================
// Portal 后端（KDE 等支持 GlobalShortcuts 的桌面）
// ============================================================================

void CapsLockVoiceHotkey::startPortal() {
    QDBusConnection bus = Impl::bus();
    if (!bus.isConnected()) {
        LOG_ERROR(kTag, "无法连接到 D-Bus session bus");
        emit error("无法连接到 D-Bus session bus");
        return;
    }

    // 连接信号
    bus.connect(kPortalService, kPortalObjectPath,
        kGlobalShortcutsIface, "Activated",
        this, SLOT(handleActivated(QString)));

    bus.connect(kPortalService, kPortalObjectPath,
        kGlobalShortcutsIface, "Deactivated",
        this, SLOT(handleDeactivated(QString)));

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
        return;
    }

    // 保存预期 session path
    QString sender = bus.baseService();
    impl_->sessionPath = Impl::makeSessionPath(sender, sessionToken);
    impl_->state = Impl::WaitingSession;

    LOG_INFO(kTag, "CreateSession 已发送，等待用户授权...");
    LOG_DEBUG(kTag, QString("Session path: %1").arg(impl_->sessionPath));
}

void CapsLockVoiceHotkey::stopPortal() {
    QDBusConnection bus = Impl::bus();
    bus.disconnect(kPortalService, kPortalObjectPath,
        kGlobalShortcutsIface, "Activated",
        this, SLOT(handleActivated(QString)));
    bus.disconnect(kPortalService, kPortalObjectPath,
        kGlobalShortcutsIface, "Deactivated",
        this, SLOT(handleDeactivated(QString)));
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
        emit error(QString("Session 被拒绝 (response=%1)").arg(response));
        LOG_ERROR(kTag, QString("Session 被拒绝: %1").arg(response));
        impl_->state = Impl::Idle;
        return;
    }

    QString actualPath = results.value("session_handle").toString();
    if (!actualPath.isEmpty()) {
        impl_->sessionPath = actualPath;
    }
    LOG_INFO(kTag, QString("Session 已授权: %1").arg(impl_->sessionPath));

    // 发送 BindShortcuts
    impl_->state = Impl::WaitingBind;

    QDBusInterface portal(kPortalService, kPortalObjectPath,
        kGlobalShortcutsIface, Impl::bus());

    QString bindToken = Impl::makeToken("io_impress_bind");

    QVariantMap shortcutProps;
    shortcutProps["description"] = "语音输入（CapsLock）";

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
        emit error(QString("快捷键绑定被拒绝 (response=%1)").arg(response));
        LOG_ERROR(kTag, QString("Bind 被拒绝: %1").arg(response));
        impl_->state = Impl::Idle;
        return;
    }

    active_ = true;
    impl_->state = Impl::Active;
    emit ready();
    LOG_INFO(kTag, "快捷键已注册（Portal），CapsLock 语音输入已就绪");
}

void CapsLockVoiceHotkey::handleActivated(const QString& shortcutId) {
    if (!active_ || ignoreEvents_) return;
    LOG_DEBUG(kTag, QString("快捷键按下: %1").arg(shortcutId));
    recording_ = true;
    emit recordingStarted();
}

void CapsLockVoiceHotkey::handleDeactivated(const QString& shortcutId) {
    if (!active_ || ignoreEvents_) return;
    LOG_DEBUG(kTag, QString("快捷键松开: %1").arg(shortcutId));
    recording_ = false;
    emit recordingStopped();
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
    impl_->state = Impl::Active;
    emit ready();
    LOG_INFO(kTag, "CapsLock 语音快捷键已就绪（libei）");
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
            uint32_t keycode = ei_event_keyboard_get_key(ev);
            bool isPress = ei_event_keyboard_get_key_is_press(ev);

            if (keycode == kCapsLockKeycode) {
                if (ignoreEvents_) break;
                if (isPress) {
                    LOG_DEBUG(kTag, "libei: CapsLock 按下");
                    recording_ = true;
                    emit recordingStarted();
                } else {
                    LOG_DEBUG(kTag, "libei: CapsLock 松开");
                    recording_ = false;
                    emit recordingStopped();
                }
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
