#ifndef UI_STYLE_H
#define UI_STYLE_H

#include <QFont>
#include <QString>

QFont appFont(int pointSize = 10, int weight = QFont::Normal);
QString cardStyle();
QString buttonStyle(const QString &bg, const QString &fg = QStringLiteral("#ffffff"));
QString compactButtonStyle(const QString &bg, const QString &fg = QStringLiteral("#ffffff"));

#endif
