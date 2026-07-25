#ifndef VOCEKIT_MICROPHONE_DIAGNOSTIC_TASK_H
#define VOCEKIT_MICROPHONE_DIAGNOSTIC_TASK_H

#include <QByteArray>
#include <QString>

struct MicrophoneDiagnosticRequest
{
    QByteArray pcm;
    QString samplePath;
    bool keepSample = false;
    int sampleRate = 16000;
};

struct MicrophoneDiagnosticResult
{
    QString displayText;
    bool showWarning = false;
    QString warningTitle;
    QString warningMessage;
    int peakPercent = 0;
    int averagePercent = 0;
    bool clipped = false;
};

MicrophoneDiagnosticResult runMicrophoneDiagnosticTask(
    const MicrophoneDiagnosticRequest &request
);

#endif // VOCEKIT_MICROPHONE_DIAGNOSTIC_TASK_H
