#ifndef VOCEKIT_VOICE_RUN_PLANNER_H
#define VOCEKIT_VOICE_RUN_PLANNER_H

#include "voice_run_context.h"

#include <QString>

enum class VoiceRunOperation
{
    Dictate,
    Translate,
    Ask,
    Custom
};

struct VoiceRunModelPlan
{
    VoiceRunOperation operation = VoiceRunOperation::Custom;
    QString modeId;
    QString primaryText;
    QString voiceInstruction;
    QString question;
    QString customVoiceText;
    QString extraInstruction;
    bool hasVoiceInput = false;
};

// VoiceRunPlanner 只根据上下文决定“该把哪些文本交给哪类功能”。
// 它不调用网络、不读配置，也不写 UI，方便后续把执行管线继续拆出 VoiceController。
class VoiceRunPlanner
{
public:
    static VoiceRunModelPlan plan(
        const VoiceRunContext &context,
        const QString &extraInstruction = QString()
    );
};

#endif // VOCEKIT_VOICE_RUN_PLANNER_H
