#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace impress {

class CapsLockVoiceHotkey : public QObject {
    Q_OBJECT
public:
    explicit CapsLockVoiceHotkey(QObject* parent = nullptr);
    ~CapsLockVoiceHotkey() override;

    bool start();
    void stop();
    bool isActive() const { return active_; }
    bool isRecording() const { return recording_; }

signals:
    void recordingStarted();
    void recordingStopped();
    void ready();
    void error(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool active_ = false;
    bool recording_ = false;
};

} // namespace impress
