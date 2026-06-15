#pragma once

#include <QObject>
#include <QString>

namespace impress {

/**
 * @brief 解析快捷键组合字符串
 *
 * 支持格式: "Ctrl+Alt+C", "Ctrl+Shift+F1", "Alt+Space" 等
 *
 * @param combo 如 "Ctrl+Alt+C"
 * @param modifiers 输出 Qt::KeyboardModifiers
 * @param key 输出 Qt::Key（主键）
 * @return 解析是否成功
 */
bool parseHotkeyCombo(const QString& combo, int& modifiers, int& key);

/**
 * @brief 全局快捷键抽象接口
 *
 * 不同后端实现：
 * - X11: XCB XGrabKey（零权限）
 * - Wayland: evdev grab + uinput replay（需 input 组 + /dev/uinput）
 */
class CapsLockBackend : public QObject {
    Q_OBJECT
public:
    explicit CapsLockBackend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~CapsLockBackend() = default;

    /** @brief 注册全局快捷键 */
    virtual bool start() = 0;

    /** @brief 注销快捷键 */
    virtual void stop() = 0;

    /** @brief 是否正在工作 */
    virtual bool isActive() const = 0;

    /** @brief 后端名称（用于日志） */
    virtual const char* backendName() const = 0;

    /** @brief 设置快捷键组合（在 start() 前调用） */
    virtual void setHotkeyCombo(int modifiers, int key) {
        m_modifiers = modifiers;
        m_key = key;
    }

    /** @brief 获取当前设置的修饰符 */
    int modifiers() const { return m_modifiers; }

    /** @brief 获取当前设置的主键 */
    int key() const { return m_key; }

signals:
    void pressed();
    void released();
    void ready();
    void error(const QString& message);

protected:
    int m_modifiers = 0;
    int m_key = 0;
};

/**
 * @brief 工厂：根据当前会话类型自动选择后端
 */
CapsLockBackend* createCapsLockBackend(QObject* parent = nullptr);

} // namespace impress
