#ifndef VOCEKIT_CLIPBOARD_WRITER_H
#define VOCEKIT_CLIPBOARD_WRITER_H

#include <QString>

using ClipboardWindowHandle = void *;

class ClipboardWriter
{
public:
    static void pasteText(const QString &text);
    static void pasteTextToWindow(
        const QString &text,
        ClipboardWindowHandle window,
        bool replaceSelection = true,
        bool hasSelection = false
    );
};

#endif
