#include "voice_run_planner.h"

namespace {

QString text(const char *value)
{
    return QString::fromUtf8(value);
}

QString screenshotTranslateInstruction()
{
    return text(
        "这是截图 OCR 结果。请逐行翻译，严格保持原文非空行数量和顺序；"
        "每个原文文字块对应一行译文，不要添加编号、解释或额外标题。"
    );
}

VoiceRunOperation operationForMode(const QString &modeId)
{
    if (modeId == QStringLiteral("dictate")) {
        return VoiceRunOperation::Dictate;
    }
    if (modeId == QStringLiteral("translate")) {
        return VoiceRunOperation::Translate;
    }
    if (modeId == QStringLiteral("ask")) {
        return VoiceRunOperation::Ask;
    }
    return VoiceRunOperation::Custom;
}

QString mergeInstruction(const QString &base, const QString &extra)
{
    const QString trimmedBase = base.trimmed();
    const QString trimmedExtra = extra.trimmed();
    if (trimmedBase.isEmpty()) {
        return trimmedExtra;
    }
    if (trimmedExtra.isEmpty()) {
        return trimmedBase;
    }
    return trimmedBase + text("\n\n") + trimmedExtra;
}

} // namespace

VoiceRunModelPlan VoiceRunPlanner::plan(
    const VoiceRunContext &context,
    const QString &extraInstruction
)
{
    VoiceRunModelPlan result;
    result.modeId = context.modeId;
    result.operation = operationForMode(context.modeId);
    result.hasVoiceInput =
        !context.textOnly && !context.voiceText.trimmed().isEmpty();

    QString baseInstruction;
    if (context.screenshotInput
        && context.modeId == QStringLiteral("translate")
        && !context.screenshotBlocks.isEmpty()) {
        baseInstruction = screenshotTranslateInstruction();
    }
    result.extraInstruction =
        mergeInstruction(baseInstruction, extraInstruction);

    switch (result.operation) {
    case VoiceRunOperation::Dictate:
        result.primaryText = context.textOnly
            ? context.textOnlyInput
            : context.voiceText;
        break;
    case VoiceRunOperation::Translate:
        if (context.textOnly) {
            result.primaryText = context.textOnlyInput;
        } else if (context.selectedText.trimmed().isEmpty()) {
            result.primaryText = context.voiceText;
        } else {
            result.primaryText = context.selectedText;
            result.voiceInstruction = context.voiceText;
        }
        break;
    case VoiceRunOperation::Ask:
        result.question = context.textOnly ? QString() : context.voiceText;
        break;
    case VoiceRunOperation::Custom:
        result.customVoiceText =
            context.textOnly ? QString() : context.voiceText;
        break;
    }

    return result;
}
