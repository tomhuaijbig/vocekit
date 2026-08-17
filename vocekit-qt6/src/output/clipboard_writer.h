#ifndef VOCEKIT_CLIPBOARD_WRITER_H
#define VOCEKIT_CLIPBOARD_WRITER_H

#include <QString>

using ClipboardWindowHandle = void *;

struct ClipboardWriteResult
{
    bool ok = false;
    QString errorCode;
};

class ClipboardWriter
{
public:
    static bool copyText(const QString &text);
    static bool isUsableExternalWindow(
        ClipboardWindowHandle window
    );
    static void pasteText(const QString &text);
    static void pasteTextToWindow(
        const QString &text,
        ClipboardWindowHandle window,
        bool replaceSelection = true,
        bool hasSelection = false
    );
    static ClipboardWriteResult pasteTextToWindowChecked(
        const QString &text,
        ClipboardWindowHandle window,
        bool replaceSelection = true,
        bool hasSelection = false
    );
};

#endif
