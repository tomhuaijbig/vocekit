#ifndef VOCEKIT_GLOBAL_HOTKEYS_H
#define VOCEKIT_GLOBAL_HOTKEYS_H

#include "hold_to_talk.h"

#include <QAbstractNativeEventFilter>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

struct GlobalHotkeyFunction
{
    QString id;
    QString title;
    QString shortcut;
    QString recordingTriggerMode;
    bool useVoice = false;
    bool useScreenshot = false;
    QString screenshotTriggerMode;
    QString screenshotShortcut;
};

struct GlobalHotkeySettingsSnapshot
{
    QVector<GlobalHotkeyFunction> functions;
};

// 全局快捷键注册器：只处理系统快捷键和按住说话监听，不直接读取完整设置对象。
class GlobalHotkeys : public QAbstractNativeEventFilter
{
public:
    ~GlobalHotkeys();

    void setCallback(const std::function<void(const QString &)> &callback);
    void setHoldCallback(
        const std::function<void(const QString &, HoldShortcutTransition)> &callback
    );

    QSet<QString> activeHoldFunctions() const;
    QStringList registerFromSnapshot(
        const GlobalHotkeySettingsSnapshot &snapshot
    );

    bool nativeEventFilter(
        const QByteArray &eventType,
        void *message,
        long *result
    ) override;

private:
    bool registerHotkey(
        int nativeId,
        const QString &logicalId,
        const QString &shortcut,
        QString *reason
    );
    void unregisterAll();

    QMap<int, QString> m_ids;
    std::function<void(const QString &)> m_callback;
    HoldToTalkHook m_holdHook;
    QSet<QString> m_activeHoldFunctions;
};

#endif // VOCEKIT_GLOBAL_HOTKEYS_H
