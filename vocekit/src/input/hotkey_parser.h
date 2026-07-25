#ifndef VOCEKIT_HOTKEY_PARSER_H
#define VOCEKIT_HOTKEY_PARSER_H

#include <QString>

// Windows RegisterHotKey 需要的修饰键和虚拟键码。
// 这里不直接包含 windows.h，方便单元测试和后续迁移。
struct NativeHotkey
{
    uint modifiers = 0;
    uint key = 0;

    bool isValid() const
    {
        return modifiers != 0 && key != 0;
    }
};

bool parseNativeHotkey(const QString &shortcut, NativeHotkey *hotkey);

#endif // VOCEKIT_HOTKEY_PARSER_H
