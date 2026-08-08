#ifndef VOCEKIT_CUSTOM_MODEL_DIALOG_SUPPORT_H
#define VOCEKIT_CUSTOM_MODEL_DIALOG_SUPPORT_H

#include <QString>

class QFontMetrics;
class QPushButton;

int customModelDialogButtonMinimumHeight(const QFontMetrics &fontMetrics);
void applyCustomModelDialogButtonSizing(
    QPushButton *button,
    const QString &background = QStringLiteral("#111827"),
    const QString &foreground = QStringLiteral("#ffffff")
);
QString customModelFinalEndpointPreview(const QString &urlText);

#endif // VOCEKIT_CUSTOM_MODEL_DIALOG_SUPPORT_H
