#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace impress {

class WaylandTextInjector : public QObject {
    Q_OBJECT
public:
    explicit WaylandTextInjector(QObject* parent = nullptr);
    ~WaylandTextInjector() override;

    bool initialize();
    bool injectText(const QString& text);
    bool isInitialized() const { return initialized_; }
    bool simulateKeycode(unsigned int keycode);

signals:
    void error(const QString& message);

private:
    bool initialized_ = false;

    bool injectChar(QChar ch);
};

} // namespace impress
