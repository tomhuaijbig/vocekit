#ifndef VOCEKIT_SPEECH_RECOGNITION_TASK_H
#define VOCEKIT_SPEECH_RECOGNITION_TASK_H

#include "cancellation_token.h"
#include "../providers/provider_types.h"

#include <QByteArray>
#include <QString>

#include <functional>

using SpeechRecognitionCallback = std::function<QString(QString *error)>;

struct SpeechRecognitionTaskRequest
{
    int index = 0;
    QByteArray pcm;
    QString provider;
    QString networkPolicy;
    bool useSystemProxy = false;
    SpeechRecognitionCallback recognize;
};

struct SpeechRecognitionTaskResult
{
    int index = 0;
    QString text;
    QString error;
    QString errorCode;
    qint64 elapsedMs = -1;
};

struct SpeechRecognitionProviderTaskRequest
{
    int index = 0;
    QByteArray audioData;
    QString audioFormat = QStringLiteral("pcm");
    int sampleRate = 16000;
    QString provider;
    QString language = QStringLiteral("follow-windows");
    QString networkPolicy = QStringLiteral("inherit");
    bool useSystemProxy = false;
    CancellationToken cancellation;
};

// 后台语音识别任务：集中处理接口调用、错误和耗时统计。
SpeechRecognitionTaskResult runSpeechRecognitionTask(
    const SpeechRecognitionTaskRequest &request
);

SpeechRecognitionTaskResult runSpeechRecognitionProviderTask(
    const SpeechRecognitionProviderTaskRequest &request
);

QString speechRecognitionProviderConfigurationError(
    const QString &provider
);

inline SpeechRecognitionTaskResult
speechRecognitionTaskResultFromProviderResult(
    int index,
    const SpeechRecognitionResult &providerResult,
    qint64 fallbackElapsedMs)
{
    SpeechRecognitionTaskResult result;
    result.index = index;
    result.text = providerResult.text;
    result.error = providerResult.error.message;
    result.errorCode = providerResult.error.code;
    result.elapsedMs = providerResult.durationMs >= 0
        ? providerResult.durationMs
        : fallbackElapsedMs;
    return result;
}

#endif // VOCEKIT_SPEECH_RECOGNITION_TASK_H
