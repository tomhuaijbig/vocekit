#ifndef VOCEKIT_DIAGNOSTIC_ACTION_CARD_H
#define VOCEKIT_DIAGNOSTIC_ACTION_CARD_H

#include <QString>

#include <functional>

class QLabel;
class QPushButton;
class QWidget;

QWidget *diagnosticActionCard(
    const QString &title,
    const QString &hint,
    const QString &buttonText,
    QPushButton **buttonOut,
    QLabel **resultOut,
    const std::function<void()> &onClick
);

void showDiagnosticResult(QLabel *result, const QString &text);

#endif // VOCEKIT_DIAGNOSTIC_ACTION_CARD_H
