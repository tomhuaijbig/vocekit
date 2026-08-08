#include "clipboard_writer.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QMap>
#include <QMimeData>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QVariant>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

const char kLeaseMimeType[] =
    "application/x-vocekit-clipboard-lease";

struct ClipboardSnapshot
{
    QMap<QString, QByteArray> formats;
    QVariant imageData;
    bool hasImage = false;
};

struct ClipboardLease
{
    bool active = false;
    quint64 generation = 0;
    QByteArray token;
    ClipboardSnapshot original;
};

ClipboardLease &clipboardLease()
{
    static ClipboardLease lease;
    return lease;
}

ClipboardWriteResult writeFailure(const QString &code)
{
    ClipboardWriteResult result;
    result.errorCode = code;
    return result;
}

ClipboardSnapshot captureSnapshot(const QMimeData *source)
{
    ClipboardSnapshot snapshot;
    if (!source) {
        return snapshot;
    }
    for (const QString &format : source->formats()) {
        snapshot.formats.insert(format, source->data(format));
    }
    snapshot.hasImage = source->hasImage();
    if (snapshot.hasImage) {
        snapshot.imageData = source->imageData();
    }
    return snapshot;
}

QMimeData *mimeDataForSnapshot(const ClipboardSnapshot &snapshot)
{
    auto *data = new QMimeData;
    for (auto it = snapshot.formats.constBegin();
         it != snapshot.formats.constEnd();
         ++it) {
        data->setData(it.key(), it.value());
    }
    if (snapshot.hasImage) {
        data->setImageData(snapshot.imageData);
    }
    return data;
}

QByteArray currentLeaseToken(QClipboard *clipboard)
{
    if (!clipboard || !clipboard->mimeData()) {
        return QByteArray();
    }
    return clipboard->mimeData()->data(
        QString::fromLatin1(kLeaseMimeType)
    );
}

void abandonLease()
{
    ClipboardLease &lease = clipboardLease();
    lease.active = false;
    lease.token.clear();
    lease.original = ClipboardSnapshot();
    ++lease.generation;
}

void restoreLeaseIfOwned(quint64 generation)
{
    ClipboardLease &lease = clipboardLease();
    QClipboard *clipboard = QApplication::clipboard();
    if (!lease.active || generation != lease.generation
        || !clipboard) {
        return;
    }
    if (currentLeaseToken(clipboard) != lease.token) {
        abandonLease();
        return;
    }
    const ClipboardSnapshot original = lease.original;
    abandonLease();
    clipboard->setMimeData(mimeDataForSnapshot(original));
}

bool beginClipboardLease(const QString &text, quint64 *generation)
{
    QClipboard *clipboard = QApplication::clipboard();
    if (!clipboard || !generation) {
        return false;
    }

    ClipboardLease &lease = clipboardLease();
    if (lease.active) {
        if (currentLeaseToken(clipboard) != lease.token) {
            abandonLease();
        }
    }
    if (!lease.active) {
        lease.original = captureSnapshot(clipboard->mimeData());
        lease.active = true;
    }

    lease.token = QUuid::createUuid().toByteArray();
    ++lease.generation;
    *generation = lease.generation;

    auto *data = new QMimeData;
    data->setText(text);
    data->setData(
        QString::fromLatin1(kLeaseMimeType),
        lease.token
    );
    clipboard->setMimeData(data);
    QApplication::processEvents();
    return currentLeaseToken(clipboard) == lease.token;
}

void scheduleClipboardRestore(quint64 generation)
{
    QTimer::singleShot(
        500,
        QCoreApplication::instance(),
        [generation]() {
            if (QApplication::instance()) {
                restoreLeaseIfOwned(generation);
            }
        }
    );
}

#ifdef Q_OS_WIN
UINT sendCtrlKey(WORD key)
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
    return SendInput(4, inputs, sizeof(INPUT));
}

UINT sendSingleKey(WORD key)
{
    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = key;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = key;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(2, inputs, sizeof(INPUT));
}

bool usableExternalWindow(HWND window)
{
    if (!window || !IsWindow(window)) {
        return false;
    }
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    return processId != 0
        && processId != GetCurrentProcessId();
}

ClipboardWriteResult writeToTarget(
    const QString &text,
    HWND window,
    bool replaceSelection,
    bool hasSelection,
    bool strictTarget)
{
    if (strictTarget && !usableExternalWindow(window)) {
        return writeFailure(
            QStringLiteral("flow_target_window_unavailable")
        );
    }
    if (window && GetForegroundWindow() != window) {
        if (!SetForegroundWindow(window)) {
            return writeFailure(
                QStringLiteral("flow_target_window_activation_failed")
            );
        }
        QThread::msleep(100);
        QApplication::processEvents();
    }
    if (strictTarget && GetForegroundWindow() != window) {
        return writeFailure(
            QStringLiteral("flow_target_window_activation_failed")
        );
    }

    if (!replaceSelection && hasSelection) {
        if (sendSingleKey(VK_RIGHT) != 2) {
            return writeFailure(
                QStringLiteral("flow_input_injection_failed")
            );
        }
        QThread::msleep(80);
        QApplication::processEvents();
        if (strictTarget && GetForegroundWindow() != window) {
            return writeFailure(
                QStringLiteral("flow_target_window_unavailable")
            );
        }
    }

    quint64 generation = 0;
    if (!beginClipboardLease(text, &generation)) {
        return writeFailure(
            QStringLiteral("flow_clipboard_unavailable")
        );
    }
    QThread::msleep(60);
    if (sendCtrlKey('V') != 4) {
        restoreLeaseIfOwned(generation);
        return writeFailure(
            QStringLiteral("flow_input_injection_failed")
        );
    }
    scheduleClipboardRestore(generation);

    ClipboardWriteResult result;
    result.ok = true;
    return result;
}
#endif

} // namespace

bool ClipboardWriter::isUsableExternalWindow(
    ClipboardWindowHandle window)
{
#ifdef Q_OS_WIN
    return usableExternalWindow(static_cast<HWND>(window));
#else
    Q_UNUSED(window);
    return false;
#endif
}

void ClipboardWriter::pasteText(const QString &text)
{
    if (!QApplication::instance()
        || QThread::currentThread()
            != QApplication::instance()->thread()) {
        return;
    }
#ifdef Q_OS_WIN
    writeToTarget(text, nullptr, true, false, false);
#else
    quint64 generation = 0;
    if (beginClipboardLease(text, &generation)) {
        scheduleClipboardRestore(generation);
    }
#endif
}

void ClipboardWriter::pasteTextToWindow(
    const QString &text,
    ClipboardWindowHandle window,
    bool replaceSelection,
    bool hasSelection)
{
    if (!QApplication::instance()
        || QThread::currentThread()
            != QApplication::instance()->thread()) {
        return;
    }
#ifdef Q_OS_WIN
    writeToTarget(
        text,
        static_cast<HWND>(window),
        replaceSelection,
        hasSelection,
        false
    );
#else
    Q_UNUSED(window);
    Q_UNUSED(replaceSelection);
    Q_UNUSED(hasSelection);
    pasteText(text);
#endif
}

ClipboardWriteResult ClipboardWriter::pasteTextToWindowChecked(
    const QString &text,
    ClipboardWindowHandle window,
    bool replaceSelection,
    bool hasSelection)
{
    if (!QApplication::instance()
        || QThread::currentThread()
            != QApplication::instance()->thread()) {
        return writeFailure(
            QStringLiteral("flow_clipboard_wrong_thread")
        );
    }
#ifdef Q_OS_WIN
    return writeToTarget(
        text,
        static_cast<HWND>(window),
        replaceSelection,
        hasSelection,
        true
    );
#else
    Q_UNUSED(text);
    Q_UNUSED(window);
    Q_UNUSED(replaceSelection);
    Q_UNUSED(hasSelection);
    return writeFailure(
        QStringLiteral("flow_target_window_unavailable")
    );
#endif
}
