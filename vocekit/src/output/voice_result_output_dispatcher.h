#ifndef VOCEKIT_VOICE_RESULT_OUTPUT_DISPATCHER_H
#define VOCEKIT_VOICE_RESULT_OUTPUT_DISPATCHER_H

#include "result_output_router.h"

#include <QString>

struct VoiceResultOutputRequest
{
    QString modeId;
    QString outputMode;
    QString finalOutput;
    bool screenshotInput = false;
    bool hasSelectedText = false;
};

struct VoiceResultOutputDispatch
{
    ResultOutputPlan routePlan;
    QString completionLogDetail;
    QString autoWriteLogDetail;
};

// 语音结果输出调度：集中决定结果写入、弹窗或截图面板的输出规则。
class VoiceResultOutputDispatcher
{
public:
    static VoiceResultOutputDispatch plan(
        const VoiceResultOutputRequest &request
    );
};

#endif // VOCEKIT_VOICE_RESULT_OUTPUT_DISPATCHER_H
