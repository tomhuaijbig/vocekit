#include "custom_model_dialog_support.h"

#include "../api/api_client_utils.h"

#include <QFontMetrics>
#include <QPushButton>
#include <QUrl>
#include <QVariant>

#include <algorithm>

int customModelDialogButtonMinimumHeight(const QFontMetrics &fontMetrics)
{
    return std::max(40, fontMetrics.height() + 16);
}

void applyCustomModelDialogButtonSizing(
    QPushButton *button,
    const QString &background,
    const QString &foreground)
{
    if (!button) {
        return;
    }

    const QString hoverBackground = background == QStringLiteral("#ffffff")
        ? QStringLiteral("#f3f4f6")
        : QStringLiteral("#263244");
    button->setProperty(
        "customModelDialogActionButton",
        QVariant(true)
    );
    button->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 0px 12px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background: %3;"
        "}"
    ).arg(background, foreground, hoverBackground));
    button->ensurePolished();
    button->setMinimumHeight(
        customModelDialogButtonMinimumHeight(button->fontMetrics())
    );
}

QString customModelFinalEndpointPreview(const QString &urlText)
{
    if (urlText.trimmed().isEmpty()) {
        return QString::fromUtf8("填写后显示最终请求地址");
    }

    const QUrl url = openAiCompatibleChatUrl(urlText);
    if (url.isEmpty()) {
        return QString::fromUtf8("地址无效");
    }
    return url.toString(QUrl::FullyEncoded);
}
