#ifndef VOCEKIT_HOTKEY_DEFINITIONS_H
#define VOCEKIT_HOTKEY_DEFINITIONS_H

#include "../domain/app_legacy_types.h"

#include <QVector>

// 内置快捷键定义：集中保存默认快捷键、标题和提示文案。
// 旧设置中心、设置页和全局快捷键注册都应从这里读取，避免多处硬编码。
const QVector<HotkeyDef> &hotkeyDefs();

// 核心功能快捷键：只包含听写、翻译和问答，不包含主界面和加入词库。
const QVector<HotkeyDef> &coreFunctionDefs();

// 截图输入使用独立快捷键时的默认值。
QString screenshotShortcutFromFunctionShortcut(const QString &shortcut);
QString defaultScreenshotShortcutForFunction(const QString &id);

#endif // VOCEKIT_HOTKEY_DEFINITIONS_H
