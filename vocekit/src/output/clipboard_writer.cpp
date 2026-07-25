#include "clipboard_writer.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>

static void sendCtrlKey(WORD key)
{
    INPUT inputs[4];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = key;
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = key;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
}

static void sendSingleKey(WORD key)
{
    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = key;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = key;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}
#endif

void ClipboardWriter::pasteText(const QString &text)
{
    QClipboard *clipboard = QApplication::clipboard();
    const QString previous = clipboard->text();
    clipboard->setText(text);
    QThread::msleep(60);
#ifdef Q_OS_WIN
    sendCtrlKey('V');
#endif
    QTimer::singleShot(500, QCoreApplication::instance(), [previous]() {
        if (QApplication::instance() && QApplication::clipboard()) {
            QApplication::clipboard()->setText(previous);
        }
    });
}

void ClipboardWriter::pasteTextToWindow(
    const QString &text,
    ClipboardWindowHandle window,
    bool replaceSelection,
    bool hasSelection
)
{
#ifdef Q_OS_WIN
    if (window) {
        SetForegroundWindow(static_cast<HWND>(window));
        QThread::msleep(100);
        QApplication::processEvents();
    }
    if (!replaceSelection && hasSelection) {
        sendSingleKey(VK_RIGHT);
        QThread::msleep(80);
        QApplication::processEvents();
    }
#else
    Q_UNUSED(window);
    Q_UNUSED(replaceSelection);
    Q_UNUSED(hasSelection);
#endif
    pasteText(text);
}
