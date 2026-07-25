#ifndef WINDOWS_AUTOSTART_H
#define WINDOWS_AUTOSTART_H

#include <QString>

bool isAutoStartLaunch();
bool setWindowsAutoStartEnabled(bool enabled, QString *error = nullptr);
bool windowsAutoStartEnabled();

#endif
