#include "hotkey_parser.h"

#include <QKeySequence>

namespace {

const uint kModAlt = 0x0001;
const uint kModControl = 0x0002;
const uint kModShift = 0x0004;
const uint kModWin = 0x0008;

const uint kVkF1 = 0x70;
const uint kVkSpace = 0x20;
const uint kVkReturn = 0x0d;
const uint kVkEscape = 0x1b;
const uint kVkTab = 0x09;

uint virtualKeyForQtKey(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return static_cast<uint>('A' + key - Qt::Key_A);
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return static_cast<uint>('0' + key - Qt::Key_0);
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return kVkF1 + static_cast<uint>(key - Qt::Key_F1);
    }
    if (key == Qt::Key_Space) {
        return kVkSpace;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        return kVkReturn;
    }
    if (key == Qt::Key_Escape) {
        return kVkEscape;
    }
    if (key == Qt::Key_Tab) {
        return kVkTab;
    }
    return 0;
}

} // namespace

bool parseNativeHotkey(const QString &shortcut, NativeHotkey *hotkey)
{
    if (hotkey) {
        *hotkey = NativeHotkey();
    }

    const QKeySequence sequence(shortcut);
    if (sequence.isEmpty()) {
        return false;
    }

    const int value = sequence[0];
    uint modifiers = 0;
    if (value & Qt::CTRL) {
        modifiers |= kModControl;
    }
    if (value & Qt::ALT) {
        modifiers |= kModAlt;
    }
    if (value & Qt::SHIFT) {
        modifiers |= kModShift;
    }
    if (value & Qt::META) {
        modifiers |= kModWin;
    }

    const int modifierMask = static_cast<int>(Qt::CTRL)
        | static_cast<int>(Qt::ALT)
        | static_cast<int>(Qt::SHIFT)
        | static_cast<int>(Qt::META);
    const uint key = virtualKeyForQtKey(value & ~modifierMask);

    if (modifiers == 0 || key == 0) {
        return false;
    }
    if (hotkey) {
        hotkey->modifiers = modifiers;
        hotkey->key = key;
    }
    return true;
}
