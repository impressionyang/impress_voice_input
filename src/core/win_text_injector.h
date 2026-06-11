#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace impress {

/**
 * @brief 文本注入器（Windows）
 *
 * 通过 Windows SendInput API 将文本注入到当前光标位置。
 */
class WaylandTextInjector : public QObject {
    Q_OBJECT
public:
    explicit WaylandTextInjector(QObject* parent = nullptr);
    ~WaylandTextInjector() override;

    /** @brief 初始化 */
    bool initialize();

    /** @brief 将文本注入到当前光标位置 */
    bool injectText(const QString& text);

    /** @brief 是否已初始化 */
    bool isInitialized() const { return initialized_; }

    /** @brief 模拟 keycode 按下+释放（Windows 使用虚拟键码） */
    bool simulateKeycode(unsigned int keycode);

    /** @brief 模拟 keysym 按下+释放（X11 keysym，自动映射为 Windows VK 键码） */
    bool simulateKeysym(unsigned long keysym);

signals:
    void error(const QString& message);

private:
    bool initialized_ = false;

    bool injectChar(QChar ch);
};

} // namespace impress
