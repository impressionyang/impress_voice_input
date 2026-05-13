#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace impress {

/**
 * @brief 文本注入器
 *
 * 通过 XTest (XWayland) 或 RemoteDesktop Portal 将文本注入到当前光标位置。
 * 使用 dlopen 动态加载 libXtst，无需编译时依赖 XTest 头文件。
 */
class WaylandTextInjector : public QObject {
    Q_OBJECT
public:
    explicit WaylandTextInjector(QObject* parent = nullptr);
    ~WaylandTextInjector() override;

    /** @brief 初始化（加载 XTest 库） */
    bool initialize();

    /** @brief 将文本注入到当前光标位置 */
    bool injectText(const QString& text);

    /** @brief 是否已初始化 */
    bool isInitialized() const { return initialized_; }

    /** @brief 模拟 X11 keycode 按下+释放（用于 CapsLock 等系统按键） */
    bool simulateKeycode(unsigned int keycode);

signals:
    void error(const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool initialized_ = false;

    bool injectChar(QChar ch);
};

} // namespace impress
