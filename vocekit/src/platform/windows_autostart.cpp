#include "windows_autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace {

QString autoStartRegistryValueName()
{
    return QStringLiteral("vocekit");
}

QString autoStartLaunchArgument()
{
    return QStringLiteral("--autostart");
}

QString executableLaunchCommand()
{
    return QStringLiteral("\"") + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + QStringLiteral("\"");
}

QString autoStartCommand()
{
    return executableLaunchCommand() + QStringLiteral(" ") + autoStartLaunchArgument();
}

} // namespace

bool isAutoStartLaunch()
{
    return QCoreApplication::arguments().contains(autoStartLaunchArgument());
}

bool setWindowsAutoStartEnabled(bool enabled, QString *error)
{
#ifdef Q_OS_WIN
    QSettings runKey(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat
    );
    if (enabled) {
        runKey.setValue(autoStartRegistryValueName(), autoStartCommand());
    } else {
        runKey.remove(autoStartRegistryValueName());
    }
    runKey.sync();
    if (runKey.status() != QSettings::NoError) {
        if (error) {
            *error = enabled
                ? QString::fromUtf8("无法写入 Windows 当前用户启动项。请检查安全软件是否拦截注册表写入。")
                : QString::fromUtf8("无法删除 Windows 当前用户启动项。请检查安全软件是否拦截注册表修改。");
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(enabled)
    if (error) {
        *error = QString::fromUtf8("当前版本只支持 Windows 开机自启动。");
    }
    return false;
#endif
}

bool windowsAutoStartEnabled()
{
#ifdef Q_OS_WIN
    QSettings runKey(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat
    );
    const QString command = runKey.value(autoStartRegistryValueName()).toString().trimmed();
    return !command.isEmpty()
        && (command.compare(autoStartCommand(), Qt::CaseInsensitive) == 0
            || command.compare(executableLaunchCommand(), Qt::CaseInsensitive) == 0);
#else
    return false;
#endif
}
