#ifndef VOCEKIT_SELECTED_TEXT_DIAGNOSTIC_TASK_H
#define VOCEKIT_SELECTED_TEXT_DIAGNOSTIC_TASK_H

#include <QString>

struct SelectedTextDiagnosticRequest
{
    QString selectedText;
    bool strongMode = false;
};

struct SelectedTextDiagnosticResult
{
    QString displayText;
    bool success = false;
    int characterCount = 0;
};

SelectedTextDiagnosticResult runSelectedTextDiagnosticTask(
    const SelectedTextDiagnosticRequest &request
);

#endif // VOCEKIT_SELECTED_TEXT_DIAGNOSTIC_TASK_H
