#ifndef VOCEKIT_SHORTCUT_DISPLAY_H
#define VOCEKIT_SHORTCUT_DISPLAY_H

#include <QString>

// 快捷键展示格式：把配置里的 Alt+X 转成界面上更容易阅读的 Alt + X。
QString displayShortcut(const QString &value);

#endif // VOCEKIT_SHORTCUT_DISPLAY_H
