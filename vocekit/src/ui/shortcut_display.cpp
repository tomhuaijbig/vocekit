#include "shortcut_display.h"

namespace {

QString sdTr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

QString displayShortcut(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return sdTr8("未设置");
    }
    return QString(trimmed).replace(QStringLiteral("+"), QStringLiteral(" + "));
}
