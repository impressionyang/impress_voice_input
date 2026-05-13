#include "mac_hotkey.h"
#include "utils/logger.h"

static const char* const kTag = "CapsLockVoiceHotkey";

namespace impress {

struct CapsLockVoiceHotkey::Impl {};

CapsLockVoiceHotkey::CapsLockVoiceHotkey(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{}

CapsLockVoiceHotkey::~CapsLockVoiceHotkey() {
    stop();
}

bool CapsLockVoiceHotkey::start() {
    if (active_) return true;
    // macOS: 使用 CGEventTap 或 Carbon EventManager 实现
    LOG_WARNING(kTag, "macOS 全局快捷键尚未实现");
    emit error("macOS 全局快捷键尚未实现（待完善 CGEventTap）");
    return false;
}

void CapsLockVoiceHotkey::stop() {
    if (!active_) return;
    active_ = false;
    recording_ = false;
    LOG_INFO(kTag, "CapsLock 快捷键已停止");
}

} // namespace impress
