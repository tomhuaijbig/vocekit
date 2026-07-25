#ifndef VOCEKIT_SPEECH_RECOGNITION_TASK_H
#define VOCEKIT_SPEECH_RECOGNITION_TASK_H

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
    qint64 elapsedMs = -1;
};

struct SpeechRecognitionProviderTaskRequest
{
    int index = 0;
    QByteArray audioData;
    QString audioFormat = QStringLiteral("pcm");
    int sampleRate = 16000;
    QString provider;
    QString networkPolicy = QStringLiteral("inherit");
    bool useSystemProxy = false;
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

#endif // VOCEKIT_SPEECH_RECOGNITION_TASK_H
