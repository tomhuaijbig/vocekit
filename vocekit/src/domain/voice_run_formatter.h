#ifndef VOCEKIT_VOICE_RUN_FORMATTER_H
#define VOCEKIT_VOICE_RUN_FORMATTER_H

#include "voice_run_context.h"

#include <QString>

struct ResultPopupFormatRequest
{
    VoiceRunContext context;
    QString output;
    QString templateId;
    QString functionTitle;
    QString modelTitle;
    qint64 elapsedMs = -1;
};

// 统一生成历史记录和结果小框中展示的输入文本，避免各页面各自拼接。
class VoiceRunFormatter
{
public:
    static QString historyInput(const VoiceRunContext &context);
    static QString resultPopupText(const ResultPopupFormatRequest &request);
};

#endif // VOCEKIT_VOICE_RUN_FORMATTER_H
