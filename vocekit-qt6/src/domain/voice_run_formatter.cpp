#include "voice_run_formatter.h"

#include "../config/app_settings_defaults.h"

#include <QStringList>
#include <QtGlobal>

namespace {

QString uiText(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

QString VoiceRunFormatter::historyInput(const VoiceRunContext &context)
{
    if (context.hasScreenshotText()) {
        QString text = uiText("截图识别文字：\n")
            + context.screenshotRecognizedText.trimmed();
        if (context.hasVoiceText()) {
            text += uiText("\n\n语音补充要求：\n")
                + context.voiceText.trimmed();
        }
        return text;
    }
    if (context.textOnly) {
        return context.textOnlyInput;
    }
    if (!context.hasSelectedText()) {
        return context.voiceText;
    }
    return uiText("选中文字：\n") + context.selectedText
        + uiText("\n\n语音输入：\n") + context.voiceText;
}

QString VoiceRunFormatter::resultPopupText(
    const ResultPopupFormatRequest &request
)
{
    const QString cleanedOutput = request.output.trimmed();
    const QString normalizedTemplate =
        normalizeResultTemplate(request.templateId);
    if (normalizedTemplate == resultTemplateOutputOnly()) {
        return cleanedOutput;
    }

    const QString inputText =
        VoiceRunFormatter::historyInput(request.context).trimmed();
    if (normalizedTemplate == resultTemplateCompare()) {
        return uiText("原文 / 输入\n")
            + (inputText.isEmpty() ? uiText("无") : inputText)
            + uiText("\n\n输出结果\n")
            + cleanedOutput;
    }

    if (normalizedTemplate == resultTemplateDetail()) {
        QStringList lines;
        lines << uiText("功能：") + request.functionTitle;
        lines << uiText("模型：") + request.modelTitle;
        lines << uiText("输入方式：")
            + (request.context.textOnly
                ? uiText("选中文字")
                : (!request.context.hasVoiceText()
                    ? uiText("选中文字")
                    : uiText("语音 / 选中文字")));
        lines << uiText("耗时：")
            + QString::number(qMax<qint64>(0, request.elapsedMs))
            + uiText(" ms");
        lines << QString();
        lines << uiText("输入内容：");
        lines << (inputText.isEmpty() ? uiText("无") : inputText);
        lines << QString();
        lines << uiText("输出结果：");
        lines << cleanedOutput;
        return lines.join(QStringLiteral("\n"));
    }

    return cleanedOutput;
}
