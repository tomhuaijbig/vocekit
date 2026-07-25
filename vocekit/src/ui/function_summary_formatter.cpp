#include "function_summary_formatter.h"

#include <QStringList>

namespace {

QString text8(const char *text)
{
    return QString::fromUtf8(text);
}

QString secondsText(int seconds, const char *zeroText)
{
    return seconds == 0
        ? text8(zeroText)
        : QString::number(seconds) + text8(" 秒");
}

} // namespace

QString functionInputModeSummary(const FunctionSummaryViewData &data)
{
    QStringList inputs;
    if (data.useSelection) {
        inputs.append(text8("选中文字"));
    }
    if (data.useVoice) {
        inputs.append(text8("语音"));
    }
    if (data.useScreenshot) {
        inputs.append(text8("截图"));
    }
    return inputs.isEmpty() ? text8("未启用") : inputs.join(text8(" + "));
}

QString functionSummaryText(const FunctionSummaryViewData &data)
{
    return data.shortcut
        + text8(" · 模型：") + data.modelTitle
        + text8(" · 输入：") + functionInputModeSummary(data)
        + text8(" · 展现：") + data.outputModeTitle
        + text8(" · 模板：") + data.resultTemplateTitle
        + text8(" · 浮动条：") + secondsText(data.floatingBarSeconds, "不调用")
        + text8(" · 结果小框：") + secondsText(data.resultPopupSeconds, "手动关闭")
        + text8(" · 倒计时：") + secondsText(data.countdownSeconds, "不调用")
        + text8(" · 提示音：") + (data.recordingBeepEnabled ? text8("开启") : text8("关闭"))
        + text8(" · 录音方式：")
        + (data.recordingTriggerMode == QStringLiteral("hold")
            ? text8("按住说话")
            : text8("切换"))
        + text8(" · 长录音：") + (data.longRecordingEnabled ? text8("开启") : text8("关闭"))
        + text8(" · 提示词：") + data.promptTitle;
}
