#ifndef VOCEKIT_CLASSIC_AUTO_WRITE_EXECUTOR_H
#define VOCEKIT_CLASSIC_AUTO_WRITE_EXECUTOR_H

#include "clipboard_writer.h"

#include <QString>

#include <functional>

struct ClassicAutoWriteRequest
{
    QString text;
    bool replaceSelection = true;
    bool hasSelection = false;
    bool popupFallbackEnabled = true;
};

struct ClassicAutoWriteAccess
{
    std::function<ClipboardWriteResult(
        const QString &,
        bool,
        bool
    )> checkedWrite;
    std::function<void(const QString &, const QString &)> setStatus;
    std::function<void(const QString &)> showFallbackPopup;
    std::function<void(const QString &, const QString &)> log;
};

class ClassicAutoWriteExecutor
{
public:
    static ClipboardWriteResult execute(
        const ClassicAutoWriteRequest &request,
        const ClassicAutoWriteAccess &access
    );
};

#endif // VOCEKIT_CLASSIC_AUTO_WRITE_EXECUTOR_H
