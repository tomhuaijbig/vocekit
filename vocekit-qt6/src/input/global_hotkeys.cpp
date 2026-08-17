#include "global_hotkeys.h"

#include "../capture/screenshot_types.h"
#include "hotkey_parser.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString textUtf8(const char *text)
{
    return QString::fromUtf8(text);
}

bool shouldUseHoldToTalk(const GlobalHotkeyFunction &function)
{
    return function.useHoldToTalk;
}

bool shouldRegisterScreenshotHotkey(const GlobalHotkeyFunction &function)
{
    return function.registerScreenshotHotkey;
}

} // namespace

GlobalHotkeys::~GlobalHotkeys()
{
    unregisterAll();
}

void GlobalHotkeys::setCallback(
    const std::function<void(const QString &)> &callback)
{
    m_callback = callback;
}

void GlobalHotkeys::setHoldCallback(
    const std::function<void(const QString &, HoldShortcutTransition)> &callback)
{
    m_holdHook.setCallback(callback);
}

QSet<QString> GlobalHotkeys::activeHoldFunctions() const
{
    return m_activeHoldFunctions;
}

bool GlobalHotkeys::hasPressedHold() const
{
    return m_holdHook.hasPressedHold();
}

QStringList GlobalHotkeys::registerFromSnapshot(
    const GlobalHotkeySettingsSnapshot &snapshot)
{
    unregisterAll();

    QStringList failures;
    QMap<QString, QString> holdShortcuts;
    int nativeId = 100;

    for (const GlobalHotkeyFunction &function : snapshot.functions) {
        QString reason;
        if (!registerHotkey(
                nativeId++,
                function.id,
                function.shortcut,
                &reason)) {
            failures << function.title
                + textUtf8("（")
                + function.shortcut
                + textUtf8("）：")
                + reason;
        }

        if (shouldUseHoldToTalk(function)) {
            holdShortcuts.insert(function.id, function.shortcut);
        }

        if (shouldRegisterScreenshotHotkey(function)) {
            if (!registerHotkey(
                    nativeId++,
                    screenshotHotkeyLogicalId(function.id),
                    function.screenshotShortcut,
                    &reason)) {
                failures << function.title
                    + textUtf8("截图（")
                    + function.screenshotShortcut
                    + textUtf8("）：")
                    + reason;
            }
        }
    }

    QString holdError;
    if (!m_holdHook.configure(holdShortcuts, &holdError)) {
        failures << textUtf8("按住说话监听：")
            + holdError
            + textUtf8("。本次运行会回退为按一次开始、再按一次结束。");
        m_activeHoldFunctions.clear();
    } else {
        m_activeHoldFunctions.clear();
        for (auto it = holdShortcuts.constBegin();
             it != holdShortcuts.constEnd();
             ++it) {
            m_activeHoldFunctions.insert(it.key());
        }
    }

    return failures;
}

bool GlobalHotkeys::nativeEventFilter(
    const QByteArray &eventType,
    void *message,
    qintptr *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    const MSG *msg = static_cast<const MSG *>(message);
    if (msg && msg->message == WM_HOTKEY) {
        const int nativeId = static_cast<int>(msg->wParam);
        if (m_ids.contains(nativeId) && m_callback) {
            const QString logicalId = m_ids.value(nativeId);
            const std::function<void(const QString &)> callback = m_callback;
            QTimer::singleShot(
                0,
                QCoreApplication::instance(),
                [callback, logicalId]() {
                    if (callback) {
                        callback(logicalId);
                    }
                }
            );
        }
        return true;
    }
#else
    Q_UNUSED(message);
#endif
    return false;
}

bool GlobalHotkeys::registerHotkey(
    int nativeId,
    const QString &logicalId,
    const QString &shortcut,
    QString *reason)
{
#ifdef Q_OS_WIN
    NativeHotkey hotkey;
    if (!parseNativeHotkey(shortcut, &hotkey)) {
        if (reason) {
            *reason = textUtf8("快捷键格式无效");
        }
        return false;
    }

    if (RegisterHotKey(nullptr, nativeId, hotkey.modifiers, hotkey.key)) {
        m_ids.insert(nativeId, logicalId);
        return true;
    }

    const DWORD errorCode = GetLastError();
    if (reason) {
        *reason = errorCode == ERROR_HOTKEY_ALREADY_REGISTERED
            ? textUtf8("已被其它软件或系统占用")
            : textUtf8("Windows 注册失败，错误码 ")
                + QString::number(errorCode);
    }
#else
    Q_UNUSED(nativeId);
    Q_UNUSED(logicalId);
    Q_UNUSED(shortcut);
    if (reason) {
        *reason = textUtf8("当前系统不支持全局快捷键");
    }
#endif
    return false;
}

void GlobalHotkeys::unregisterAll()
{
    m_holdHook.uninstall();
    m_activeHoldFunctions.clear();

#ifdef Q_OS_WIN
    for (auto it = m_ids.constBegin(); it != m_ids.constEnd(); ++it) {
        UnregisterHotKey(nullptr, it.key());
    }
#endif
    m_ids.clear();
}
