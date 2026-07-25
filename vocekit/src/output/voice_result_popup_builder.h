#ifndef VOCEKIT_VOICE_RESULT_POPUP_BUILDER_H
#define VOCEKIT_VOICE_RESULT_POPUP_BUILDER_H

#include "../domain/voice_run_context.h"

#include <QString>

struct VoiceResultPopupBuildRequest
{
    VoiceRunContext context;
    QString output;
    QString functionTitle;
    QString modelId;
    QString modelTitle;
    QString templateId;
    qint64 elapsedMs = -1;
    int timeoutMs = 0;
};

struct VoiceResultPopupPresentation
{
    QString title;
    QString text;
    QString currentModel;
    bool hasSelectedText = false;
    int timeoutMs = 0;
};

class VoiceResultPopupBuilder
{
public:
    static VoiceResultPopupPresentation build(
        const VoiceResultPopupBuildRequest &request
    );
};

#endif // VOCEKIT_VOICE_RESULT_POPUP_BUILDER_H
