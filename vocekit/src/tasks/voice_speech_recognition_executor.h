#ifndef VOCEKIT_VOICE_SPEECH_RECOGNITION_EXECUTOR_H
#define VOCEKIT_VOICE_SPEECH_RECOGNITION_EXECUTOR_H

#include "speech_recognition_task.h"

#include <functional>

struct VoiceSpeechRecognitionRequest
{
    int index = 0;
    QString modeId;
    QByteArray audioData;
    QString provider;
    QString networkPolicy = QStringLiteral("inherit");
    bool useSystemProxy = false;
    QString audioFormat = QStringLiteral("pcm");
    int sampleRate = 16000;
};

struct VoiceSpeechRecognitionHandlers
{
    std::function<SpeechRecognitionTaskResult(
        const SpeechRecognitionProviderTaskRequest &request
    )> recognizeProvider;
};

struct VoiceSpeechRecognitionResult
{
    int index = 0;
    bool ok = false;
    QString text;
    QString error;
    qint64 elapsedMs = -1;
    QString logCategory;
    QString logAction;
    QString logDetail;
};

class VoiceSpeechRecognitionExecutor
{
public:
    static VoiceSpeechRecognitionResult run(
        const VoiceSpeechRecognitionRequest &request,
        const VoiceSpeechRecognitionHandlers &handlers
    );
};

#endif // VOCEKIT_VOICE_SPEECH_RECOGNITION_EXECUTOR_H
