#include "ui_style.h"

QFont appFont(int pointSize, int weight)
{
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(pointSize);
    font.setWeight(weight);
    return font;
}

QString cardStyle()
{
    return QStringLiteral(
        "QFrame#card {"
        "  background: #ffffff;"
        "  border: 1px solid #e4e7ec;"
        "  border-radius: 8px;"
        "}"
    );
}

QString buttonStyle(const QString &bg, const QString &fg)
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 12px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background: %3;"
        "}"
    ).arg(bg, fg, bg == QStringLiteral("#ffffff") ? QStringLiteral("#f3f4f6") : QStringLiteral("#263244"));
}

QString compactButtonStyle(const QString &bg, const QString &fg)
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: %3;"
        "  border-radius: 6px;"
        "  padding: 0 12px;"
        "  margin: 0;"
        "  font-weight: 600;"
        "  min-height: 40px;"
        "}"
        "QPushButton:hover {"
        "  background: %4;"
        "}"
    ).arg(
        bg,
        fg,
        bg == QStringLiteral("#ffffff") ? QStringLiteral("1px solid #e4e7ec") : QStringLiteral("none"),
        bg == QStringLiteral("#ffffff") ? QStringLiteral("#f3f4f6") : QStringLiteral("#263244")
    );
}
