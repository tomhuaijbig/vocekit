#ifndef VOCEKIT_SPEECH_PROVIDER_H
#define VOCEKIT_SPEECH_PROVIDER_H

#include "provider_types.h"

#include "../tasks/cancellation_token.h"

// 语音适配器不得直接弹窗，只返回结果或结构化错误。
class ISpeechProvider
{
public:
    virtual ~ISpeechProvider()
    {
    }

    virtual QString id() const = 0;
    virtual ProviderCheckResult checkConfiguration(
        const CancellationToken &cancellation = CancellationToken()
    ) const = 0;
    virtual SpeechRecognitionResult recognize(
        const SpeechRecognitionRequest &request,
        const CancellationToken &cancellation
    ) = 0;
    virtual void refreshConfiguration()
    {
    }
};

#endif // VOCEKIT_SPEECH_PROVIDER_H
