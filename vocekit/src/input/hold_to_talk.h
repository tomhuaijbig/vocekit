#pragma once

#include <QMap>
#include <QString>
#include <functional>

enum HoldModifier
{
    HoldModifierNone = 0,
    HoldModifierControl = 1 << 0,
    HoldModifierAlt = 1 << 1,
    HoldModifierShift = 1 << 2,
    HoldModifierMeta = 1 << 3
};

enum class HoldShortcutTransition
{
    None,
    Pressed,
    Released
};

// 按住说话快捷键匹配器：过滤自动重复和无关按键，只生成一次按下和一次松开。
class HoldShortcutMatcher
{
public:
    bool configure(const QString &shortcut);
    void reset();

    HoldShortcutTransition process(
        int nativeKey,
        unsigned int modifiers,
        bool pressed,
        bool autoRepeat
    );

    int nativeKey() const;
    unsigned int modifiers() const;

private:
    int m_nativeKey = 0;
    unsigned int m_modifiers = HoldModifierNone;
    bool m_pressed = false;
};

// Windows 按住说话监听器：只跟踪已配置快捷键的按下和松开，不采集普通键入内容。
class HoldToTalkHook
{
public:
    ~HoldToTalkHook();

    void setCallback(
        const std::function<void(const QString &, HoldShortcutTransition)> &callback
    );
    bool configure(
        const QMap<QString, QString> &shortcuts,
        QString *error = nullptr
    );
    void uninstall();
    bool isInstalled() const;

    void processNativeKey(
        unsigned int message,
        unsigned int nativeKey
    );

private:
    QMap<QString, HoldShortcutMatcher> m_matchers;
    std::function<void(const QString &, HoldShortcutTransition)> m_callback;
    void *m_hook = nullptr;
};
